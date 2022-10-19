/*!
 * \file render_engine_gl3_backend.cpp
 * \brief file render_engine_gl3_backend.cpp
 *
 * Copyright 2019 by InvisionApp.
 *
 * Contact: kevinrogovin@invisionapp.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 */

#include <astral/util/ostream_utility.hpp>
#include <astral/util/gl/gl_get.hpp>
#include "render_engine_gl3_backend.hpp"
#include "render_engine_gl_util.hpp"
#include "render_engine_gl3_packing.hpp"
#include "render_engine_gl3_static_data.hpp"
#include "render_engine_gl3_image.hpp"
#include "render_engine_gl3_colorstop.hpp"
#include "render_engine_gl3_shadow_map.hpp"
#include "render_engine_gl3_vertex.hpp"
#include "render_engine_gl3_shader_builder.hpp"

/* Code Overview
 *   I. RenderEngineGL3::Implement::Backend has a DataStach object for each
 *      enumeration of RenderEngineGL3::buffer_t;
 *     A. each DataStash tracks how many objects have been written
 *        to it, once that number reaches Config::m_max_per_draw_call[]
 *        then the stash "is full" and all of the DataStash objects
 *        get their data pushed to a GL buffer object managed by
 *        the BufferPool m_ubo_item_data_buffer_pool
 *     B. The pushing of that data to m_ubo_item_data_buffer_pool places all
 *        of the data from each of the DataStash objects into a single
 *        BO; if they all do not fit, a new BO is started; this is
 *        implemented in Backend::emit_item_stashes().
 *
 * II. PackedValue<T> represents the object behind a RenderValue<T>;
 *     the second template argument just refers to what DataStach of
 *     RenderEngineGL3::Implement::Backend::m_data_stashes to write to.
 *   A. A PackedValue<T> stores the value T, the location in the buffer
 *      it is at (or InvalidRenderValue if it is not yet written) and
 *      the value of RenderEngineGL3::Implement::Backend::m_current_item_stash when it
 *      was written. If the current value of Backend::m_current_item_stash
 *      is not the same, then the PackedValue knows it needs to be written
 *      to the buffer again; the value of m_current_item_stash is incremented
 *      at each call to end_item_stashes().
 *   B. PackedValue<T> for Brush has a specialization for writing it
 *      to a buffer along with if it will fit because a Brush has as
 *      part of its value a number of RenderValue<S> for several classes S.
 *   C. RenderEngineGL3::Implement::Backend::get(RenderValue<T> v) is essentially just
 *      returning a *reference* to m_foo[v.cookie()] where m_foo is the
 *      std::vector<PackedValue<T>> of RenderEngineGL3::Implement::Backend that stores
 *      its PackedValue<T> values.
 *   D. RenderEngineGL3::Implement::Backend::PackedItemData is almost the same
 *      as PackedValue<T> but it is for ItemData only; it is NOT a
 *      template specialization because ItemData is a variable sized
 *      object, so the copy of its value is instead realized in a shared
 *      std::vector<gvec4>, RenderEngineGL3::Implement::Backend::m_item_data_backing;
 *      an additional std::vector<uint32_t>,
 *      RenderEngineGL3::Implement::Backend::m_item_data_image_id_backing
 *      is used to store the backing for implementing
 *      image_id_of_item_data().
 *
 *  III. RenderEngineGL3::Implement::Backend::StagingBuffer is where draw commands
 *       are sent. It has four jobs
 *   A. Build a list of DrawCall values
 *   B. Build the "vertex-list" that is a common list for all DrawCall
 *      values, that list is represented by StagingBuffer::m_vertex_blit_entries
 *   C. Before rendering anything, each StagingBuffer needs to convert the
 *      list StagingBuffer:m_vertex_blit_entries into something the GPU can
 *      consume in drawing; this is accomplished by realizing each element
 *      of the list as 1-pixel high rectangle to an RG_32UI render target
 *      where the .r() channel is what vertex to use the VertexDataBacking
 *      and the .g() channel is what Header to use for the vertex. The surface
 *      realization is done in StagingBuffer:pre_emit(). There is also the
 *      additional complication that a single entry in m_vertex_blit_entries
 *      can expand to several blit-rects to make sure it fits within the width
 *      of the render target.
 *   D. The last job is to emit the GL draw calls of each DrawCall. The vertex
 *      shader is attribute-less and relies solely on gl_VertexID. That value
 *      is used to get teh correct texel from the RG_32UI surface in step C.
 */

template<typename T>
class astral::gl::RenderEngineGL3::Implement::Backend::ValueMirror
{
public:
  typedef const T &value_type;

  class type
  {
  public:
    type(const T &v):
      m_value(v)
    {}

    value_type
    value(void) const
    {
      return m_value;
    }

    value_type
    packable_value(void) const
    {
      return value();
    }

  private:
    T m_value;
  };
};

template<>
class astral::gl::RenderEngineGL3::Implement::Backend::ValueMirror<const astral::ShadowMap&>
{
public:
  typedef const ShadowMap &value_type;

  class type
  {
  public:
    type(const ShadowMap &v):
      m_v(&v)
    {}

    value_type
    value(void) const
    {
      return *m_v;
    }

    value_type
    packable_value(void) const
    {
      return value();
    }

  private:
    reference_counted_ptr<const ShadowMap> m_v;
  };
};

template<>
class astral::gl::RenderEngineGL3::Implement::Backend::ValueMirror<astral::ImageSampler>
{
public:
  typedef const ImageSampler &value_type;

  class type
  {
  public:
    type(const ImageSampler &v, ImageAtlas &atlas):
      m_value(v)
    {
      m_packable_value.init(m_value, atlas);
    }

    value_type
    value(void) const
    {
      return m_value;
    }

    const Packing::ProcessedImageSampler&
    packable_value(void) const
    {
      return m_packable_value;
    }

  private:
    ImageSampler m_value;
    Packing::ProcessedImageSampler m_packable_value;
  };
};

template<>
class astral::gl::RenderEngineGL3::Implement::Backend::ValueMirror<const astral::RenderClipElement*>
{
public:
  typedef const astral::RenderClipElement *value_type;

  class type
  {
  public:
    type(const astral::RenderClipElement *v)
    {
      ASTRALassert(v);
      m_packable_value.init(*v);
    }

    const Packing::ProcessedRenderClipElement&
    packable_value(void) const
    {
      return m_packable_value;
    }

  private:
    Packing::ProcessedRenderClipElement m_packable_value;
  };
};


template<typename T, enum astral::gl::RenderEngineGL3::data_t tp>
class astral::gl::RenderEngineGL3::Implement::Backend::PackedValue
{
public:
  template<typename ...Args>
  PackedValue(Args&&... args):
    m_value(std::forward<Args>(args)...),
    m_item_stash(InvalidRenderValue),
    m_location(InvalidRenderValue)
  {}

  bool
  on_buffer(Backend &backend) const
  {
    return (m_item_stash == backend.m_current_item_stash);
  }

  void
  data_freespace_requirement(Backend &backend,
                             vecN<unsigned int, number_data_types> &out_incr)
  {
    if (!on_buffer(backend))
      {
        ++out_incr[tp];
      }
  }

  uint32_t
  pack_data(Backend &backend)
  {
    if (!on_buffer(backend))
      {
        c_array<generic_data> dst;

        ++backend.m_stats[number_items_bufferX + tp];
        dst = backend.m_data_stashes[tp].write_location(&m_location);
        Packing::pack(dst, m_value.packable_value());
        m_item_stash = backend.m_current_item_stash;
      }
    else
      {
        ++backend.m_stats[number_reuses_bufferX + tp];
      }

    return m_location;
  }

  typename ValueMirror<T>::value_type
  value(void) const
  {
    return m_value.value();
  }

  const typename ValueMirror<T>::type&
  raw_value(void) const
  {
    return m_value;
  }

private:
  typename ValueMirror<T>::type m_value;
  uint32_t m_item_stash, m_location;
};

template<>
void
astral::gl::RenderEngineGL3::Implement::Backend::PackedValue<astral::EmulateFramebufferFetch, astral::gl::RenderEngineGL3::data_item_transformation>::
data_freespace_requirement(Backend &backend,
                           vecN<unsigned int, number_data_types> &out_incr)
{
  if (on_buffer(backend))
    {
      return;
    }

  ASTRALassert(m_value.value().m_image.valid());

  backend.get(m_value.value().m_image).data_freespace_requirement(backend, out_incr);
  ++out_incr[RenderEngineGL3::data_item_transformation];
}

template<>
uint32_t
astral::gl::RenderEngineGL3::Implement::Backend::PackedValue<astral::EmulateFramebufferFetch, astral::gl::RenderEngineGL3::data_item_transformation>::
pack_data(Backend &backend)
{
  if (!on_buffer(backend))
    {
      c_array<generic_data> dst;
      Packing::PackableEmulateFramebufferFetch framebuffer_pixels;

      ASTRALassert(m_value.value().m_image.valid());

      ++backend.m_stats[number_items_bufferX + data_item_transformation];

      framebuffer_pixels.m_image = backend.get(m_value.value().m_image).pack_data(backend);
      framebuffer_pixels.m_transformation = m_value.value().m_image_transformation_pixels;

      dst = backend.m_data_stashes[data_item_transformation].write_location(&m_location);
      Packing::pack(dst, framebuffer_pixels);
      m_item_stash = backend.m_current_item_stash;
    }
  else
    {
      ++backend.m_stats[number_reuses_bufferX + data_item_transformation];
    }

  return m_location;
}

template<>
void
astral::gl::RenderEngineGL3::Implement::Backend::PackedValue<astral::Brush, astral::gl::RenderEngineGL3::data_brush>::
data_freespace_requirement(Backend &backend,
                           vecN<unsigned int, number_data_types> &out_incr)
{
  if (on_buffer(backend))
    {
      return;
    }

  ++out_incr[RenderEngineGL3::data_brush];

  /* The brush is not on the buffer, BUT the BufferUBO still has
   * some room; we need to check if each of the fields of the
   * brush requires a new buffer.
   */
  if (m_value.value().m_image.valid())
    {
      backend.get(m_value.value().m_image).data_freespace_requirement(backend, out_incr);
      if (m_value.value().m_image_transformation.valid())
        {
          backend.get(m_value.value().m_image_transformation).data_freespace_requirement(backend, out_incr);
        }
    }

  if (m_value.value().m_gradient.valid())
    {
      backend.get(m_value.value().m_gradient).data_freespace_requirement(backend, out_incr);
      if (m_value.value().m_gradient_transformation.valid())
        {
          backend.get(m_value.value().m_gradient_transformation).data_freespace_requirement(backend, out_incr);
        }
    }
}

template<>
uint32_t
astral::gl::RenderEngineGL3::Implement::Backend::PackedValue<astral::Brush, astral::gl::RenderEngineGL3::data_brush>::
pack_data(Backend &backend)
{
  if (!on_buffer(backend))
    {
      Packing::PackableBrush br;
      c_array<generic_data> dst;

      ++backend.m_stats[number_items_bufferX + data_brush];
      br.m_colorspace = m_value.value().m_colorspace;

      /* PackableBrush requires the color to be with alpha pre-multiplied */
      br.m_base_color.x() = m_value.value().m_base_color.w() * m_value.value().m_base_color.x();
      br.m_base_color.y() = m_value.value().m_base_color.w() * m_value.value().m_base_color.y();
      br.m_base_color.z() = m_value.value().m_base_color.w() * m_value.value().m_base_color.z();
      br.m_base_color.w() = m_value.value().m_base_color.w();

      if (m_value.value().m_image.valid())
        {
          br.m_image = backend.get(m_value.value().m_image).pack_data(backend);
          if (m_value.value().m_image_transformation.valid())
            {
              br.m_image_transformation = backend.get(m_value.value().m_image_transformation).pack_data(backend);
            }
        }

      if (m_value.value().m_gradient.valid())
        {
          br.m_gradient = backend.get(m_value.value().m_gradient).pack_data(backend);
          if (m_value.value().m_gradient_transformation.valid())
            {
              br.m_gradient_transformation = backend.get(m_value.value().m_gradient_transformation).pack_data(backend);
            }
        }

      dst = backend.m_data_stashes[RenderEngineGL3::data_brush].write_location(&m_location);

      Packing::pack(dst, br);
      m_item_stash = backend.m_current_item_stash;
    }
  else
    {
      ++backend.m_stats[number_reuses_bufferX + data_brush];
    }

  return m_location;
}

class astral::gl::RenderEngineGL3::Implement::Backend::PackedItemData
{
public:
  PackedItemData(range_type<unsigned int> item_data,
                 range_type<unsigned int> interpretation_data,
                 range_type<unsigned int> image_id_data,
                 range_type<unsigned int> shadow_map_id_data):
    m_backing_data(item_data),
    m_interpretation_data(interpretation_data),
    m_image_id_data(image_id_data),
    m_shadow_map_id_data(shadow_map_id_data),
    m_size(item_data.m_end - item_data.m_begin),
    m_item_stash(InvalidRenderValue),
    m_location(InvalidRenderValue)
  {
    ASTRALassert(m_backing_data.m_end >= m_backing_data.m_begin);
    ASTRALassert(m_interpretation_data.m_end >= m_interpretation_data.m_begin);
  }

  bool
  on_buffer(Backend &backend) const
  {
    return (m_item_stash == backend.m_current_item_stash);
  }

  void
  data_freespace_requirement(Backend &backend,
                             vecN<unsigned int, number_data_types> &out_incr);

  uint32_t
  pack_data(Backend &backend);

  c_array<const ImageID>
  image_id(Backend &backend)
  {
    c_array<const ImageID> tmp;

    tmp = make_c_array(backend.m_item_data_image_id_backing);
    return tmp.sub_array(m_image_id_data);
  }

  c_array<const ShadowMapID>
  shadow_map_id(Backend &backend)
  {
    c_array<const ShadowMapID> tmp;

    tmp = make_c_array(backend.m_item_data_shadow_map_id_backing);
    return tmp.sub_array(m_shadow_map_id_data);
  }

  c_array<const gvec4>
  data(Backend &backend)
  {
    c_array<const gvec4> tmp;

    tmp = make_c_array(backend.m_item_data_backing);
    return tmp.sub_array(m_backing_data);
  }

private:
  /* range into Backend::m_item_data_backing */
  range_type<unsigned int> m_backing_data;
  range_type<unsigned int> m_interpretation_data;
  range_type<unsigned int> m_image_id_data;
  range_type<unsigned int> m_shadow_map_id_data;
  unsigned int m_size;
  uint32_t m_item_stash, m_location;
};

class astral::gl::RenderEngineGL3::Implement::Backend::BufferPool:
  public reference_counted<BufferPool>::non_concurrent
{
public:
  enum texture_constants_t:uint32_t
    {
      texture_log2_width = 11u,
      texture_width = 1u << texture_log2_width,

      texture_log2_values_per_texel = 2u,
      texture_values_per_texel = 1u << texture_log2_values_per_texel,
      texture_values_per_texel_mask = texture_values_per_texel - 1u,

      texture_scalar_alignment = 4u * 2048u,
      texture_scalar_alignemnt_mask = 4u * 2048u - 1u,
    };

  explicit
  BufferPool(enum data_streaming_t tp, unsigned int size_generic_data, bool as_texture);

  ~BufferPool(void);

  c_array<generic_data>
  begin_write(void);

  /* End the write to the current BO, returning the BO written to.
   * \param range_generic_data range into BO, in units of generic_data written to
   * \param size_needed the required size of the BO in order to be used by GL;
   *                    this has effect only when the streaming by orphaning where
   *                    a new BO is made on each end_write().
   */
  astral_GLuint
  end_write(range_type<unsigned int> range_generic_data, unsigned int size_needed);

  /* Same as end_write(range_generic_data), begin_write().size()) */
  astral_GLuint
  end_write(range_type<unsigned int> range_generic_data)
  {
    return end_write(range_generic_data, m_size);
  }

  /* same as end_write(range_type<unsigned int>(0, begin_write().size())) */
  astral_GLuint
  end_write(void)
  {
    return end_write(range_type<unsigned int>(0, m_size));
  }

  astral_GLuint
  current_bo(void) const
  {
    ASTRALassert(m_current != 0u);
    return m_current;
  }

  /* The GL backing for the buffer is to be a texture, instead
   * of a buffer object, i.e. the return values end_write()
   * and current_bo() are GL ID's for texture objects instead
   * of GL buffer objects.
   */
  bool
  as_texture(void) const
  {
    return m_texture_size != uvec2(0, 0);
  }

  void
  reset_pool(void);

  c_array<generic_data>
  current_ptr(void)
  {
    ASTRALassert(m_current_ptr.size() > 0u);
    return m_current_ptr;
  }

private:
  enum data_streaming_t m_tp;
  uvec2 m_texture_size;
  unsigned int m_size;
  astral_GLuint m_current;
  std::vector<astral_GLuint> m_free_bos, m_used_bos;
  std::vector<generic_data> m_cpu_buffer;
  c_array<generic_data> m_current_ptr;
};

class astral::gl::RenderEngineGL3::Implement::Backend::VertexSurfacePool:
  public reference_counted<VertexSurfacePool>::non_concurrent
{
public:
  VertexSurfacePool(unsigned int w, unsigned int h):
    m_dims(w, h)
  {
  }

  ~VertexSurfacePool()
  {
    for (auto R : m_available)
      {
        delete_surface(R);
      }
    for (auto R : m_used)
      {
        delete_surface(R);
      }
  }

  VertexSurface
  allocate_surface(void)
  {
    VertexSurface return_value;

    if (m_available.empty())
      {
        m_available.push_back(make_surface());
      }

    return_value = m_available.back();
    m_used.push_back(return_value);
    m_available.pop_back();

    return return_value;
  }

  void
  reset_pool(void)
  {
    unsigned int sz(m_available.size());

    m_available.resize(sz + m_used.size());
    std::copy(m_used.begin(), m_used.end(), m_available.begin() + sz);
    m_used.clear();
  }

  const uvec2&
  dims(void) const
  {
    return m_dims;
  }

  unsigned int
  max_vertices(void) const
  {
    return m_dims.x() * m_dims.y();
  }

private:
  VertexSurface
  make_surface(void)
  {
    VertexSurface R;

    astral_glGenTextures(1, &R.m_texture);
    ASTRALassert(R.m_texture != 0u);

    astral_glBindTexture(ASTRAL_GL_TEXTURE_2D, R.m_texture);
    astral_glTexStorage2D(ASTRAL_GL_TEXTURE_2D, 1, ASTRAL_GL_RG32UI, m_dims.x(), m_dims.y());
    astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D, ASTRAL_GL_TEXTURE_MIN_FILTER, ASTRAL_GL_NEAREST);
    astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D, ASTRAL_GL_TEXTURE_MAG_FILTER, ASTRAL_GL_NEAREST);

    astral_glGenFramebuffers(1, &R.m_fbo);
    ASTRALassert(R.m_fbo != 0);

    astral_glBindFramebuffer(ASTRAL_GL_READ_FRAMEBUFFER, R.m_fbo);
    astral_glFramebufferTexture2D(ASTRAL_GL_READ_FRAMEBUFFER, ASTRAL_GL_COLOR_ATTACHMENT0, ASTRAL_GL_TEXTURE_2D, R.m_texture, 0);

    R.m_dims = m_dims;

    return R;
  }

  void
  delete_surface(VertexSurface R)
  {
    astral_glDeleteFramebuffers(1, &R.m_fbo);
    astral_glDeleteTextures(1, &R.m_texture);
  }

  uvec2 m_dims;
  std::vector<VertexSurface> m_available;
  std::vector<VertexSurface> m_used;
};

class astral::gl::RenderEngineGL3::Implement::Backend::StagingBuffer:
  public reference_counted<StagingBuffer>::non_concurrent
{
public:
  StagingBuffer(const Backend &backend);

  ~StagingBuffer();

  void
  begin(const StagingBuffer *prev_buffer);

  /*
   * Returns and index into R to one past the
   * the range processed.
   */
  unsigned int
  on_draw_render_data(Backend &backend,
                      c_array<const pointer<const ItemShader>> shaders,
                      unsigned int z,
                      const RenderValues &st,
                      UberShadingKey::Cookie uber_shader_key,
                      RenderValue<ScaleTranslate> tr,
                      ClipWindowValue cl,
                      bool permute_xy,
                      c_array<const std::pair<unsigned int, range_type<int>>> R);

  void
  end(Backend &backend, bool issue_begin_buffers);

  void
  pre_emit(Backend &backend, astral_GLint recip_half_viewport_uniform_location);

  void
  emit_draws(Backend &backend);

  void
  color_write_mask(bvec4 b)
  {
    if (b != m_state.m_color_writes_enabled)
      {
        m_state.m_color_writes_enabled = b;
        m_state.m_dirty |= gpu_dirty_state::color_mask;
      }
  }

  void
  depth_buffer_mode(enum depth_buffer_mode_t b)
  {
    if (b != m_state.m_depth_buffer_mode)
      {
        m_state.m_depth_buffer_mode = b;
        m_state.m_dirty |= gpu_dirty_state::depth;
      }
  }

  void
  set_stencil_state(const StencilState &st)
  {
    if (st != m_state.m_stencil_state)
      {
        m_state.m_stencil_state = st;
        m_state.m_dirty |= gpu_dirty_state::stencil;
      }
  }

private:
  class DrawCall;

  class GPUState
  {
  public:
    gpu_dirty_state m_dirty;
    enum depth_buffer_mode_t m_depth_buffer_mode;
    bvec4 m_color_writes_enabled;
    StencilState m_stencil_state;
    reference_counted_ptr<Program> m_program;
    BackendBlendMode m_blend_mode;
  };

  class VertexSurfaceBlitEntry
  {
  public:
    range_type<unsigned int> m_vertex_src;
    range_type<unsigned int> m_vertex_dst;
    unsigned int m_header_location;
  };

  class VertexSurfaceBlitRect
  {
  public:
    unsigned int m_src_start;
    uvec2 m_dst_start;
    unsigned int m_header_ID;
    unsigned int m_length;
  };

  void
  add_vertex_surface_blit(Backend &backend, const VertexSurfaceBlitEntry &V);

  void
  add_vertex_surface_blit_rect(Backend &backend, const VertexSurfaceBlitRect &R);

  void
  ready_vertex_blit_index_buffer(void);

  /* current GPUState */
  GPUState m_state;

  /* ability to reuse Header values, used and saved only when
   * a single shader is passed to on_draw_render_data()
   */
  uint32_t m_prev_header_idx;
  Packing::Header m_prev_header;

  /* when multiple shaders are used in a draw,
   * this is the array of header locations for
   * each shader along with what GLProgram used
   * with that header
   */
  std::vector<std::pair<uint32_t, Program*>> m_header_shader_idxs;

  /* maximum number of vertices that this StagingBuffer can handle */
  unsigned int m_max_number_vertices;

  /* set of draw calls queued up for drawing */
  unsigned int m_draw_to_set_data_bos;
  std::vector<DrawCall> m_draws;

  /* the list of "vertex-blit" commands to onto the vertex surface. */
  std::vector<VertexSurfaceBlitEntry> m_vertex_blit_entries;

  /* surface that vertices are blitted to */
  VertexSurface m_vertex_surface;

  /* BO's and VAO for vertex blitting */
  astral_GLuint m_vertex_blit_vbo, m_vertex_blit_ibo, m_vertex_blit_vao;
  std::vector<gvec4> m_vertex_blit_cpu_vertex_buffer;
  std::vector<astral_GLuint> m_vertex_blit_cpu_index_buffer;

  /* VAO for rendering which is empty because rendering
   * is attributeless rendering
   */
  astral_GLuint m_render_vao;

  /* current vertex in StagingBuffer, i.e. where the next
   * set of vertices added will start.
   */
  unsigned int m_current_vert;
};

class astral::gl::RenderEngineGL3::Implement::Backend::StagingBuffer::DrawCall
{
public:
  explicit
  DrawCall(GPUState state, StagingBuffer &staging);

  void
  emit_draw(Backend &backend);

  void
  update_draw_call_range_end(unsigned int v);

  /* Sets to set the data BO's */
  void
  set_data_bos(Backend &backend, bool issue_begin_buffers);

private:
  GPUState m_set_state;

  /* backng GL buffer object of the data, m_data_ranges give the ranges into
   * the BO where the data is located. A zero value indicates that UBO's
   * should not be set by this draw call.
   */
  astral_GLuint m_data_ubo;
  vecN<BufferRange, number_data_types> m_data_ranges;

  /* if data is backed by a texture instead, m_data_texture_offsets gives the offsets
   * into the texture (when viewed as a linear buffer) where the data is located.
   * A zero value indicates that the texture should not be set by this draw call.
   */
  astral_GLuint m_data_texture;
  vecN<generic_data, number_data_types> m_data_texture_offsets;

  range_type<unsigned int> m_draw_call_range;
};

////////////////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::Backend::PackedItemData methods
void
astral::gl::RenderEngineGL3::Implement::Backend::PackedItemData::
data_freespace_requirement(Backend &backend,
                           vecN<unsigned int, number_data_types> &out_incr)
{
  if (!on_buffer(backend))
    {
      c_array<const gvec4> src;
      c_array<const ItemDataValueMapping::entry> item_data_value_map;

      out_incr[data_item_data] += m_size;
      item_data_value_map = make_c_array(backend.m_item_data_interpretation_backing).sub_array(m_interpretation_data);
      src = make_c_array(backend.m_item_data_backing).sub_array(m_backing_data);

      for (const auto &entry : item_data_value_map)
        {
          enum data_t b;

          b = data_t_value(entry.m_type);
          backend.data_freespace_requirement(b, src[entry.m_component][entry.m_channel].u, out_incr);
        }
    }
}

uint32_t
astral::gl::RenderEngineGL3::Implement::Backend::PackedItemData::
pack_data(Backend &backend)
{
  if (!on_buffer(backend))
    {
      backend.m_stats[number_items_bufferX + data_item_data] += m_size;
      if (m_size > 0)
        {
          c_array<generic_data> dst;
          c_array<const gvec4> src;
          c_array<const ItemDataValueMapping::entry> item_data_value_map;

          dst = backend.m_data_stashes[data_item_data].write_location(m_size, &m_location);
          src = make_c_array(backend.m_item_data_backing).sub_array(m_backing_data);
          item_data_value_map = make_c_array(backend.m_item_data_interpretation_backing).sub_array(m_interpretation_data);

          Packing::pack_item_data(dst, src);
          for (const auto &entry : item_data_value_map)
            {
              enum data_t b;
              unsigned int idx;

              idx = 4u * entry.m_component + entry.m_channel;
              b = data_t_value(entry.m_type);
              dst[idx].u = backend.pack_data(b, dst[idx].u);
            }
        }
      else
        {
          m_location = 0u;
        }
      m_item_stash = backend.m_current_item_stash;
    }

  return m_location;
}

//////////////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::Backend::BufferPool methods
astral::gl::RenderEngineGL3::Implement::Backend::BufferPool::
BufferPool(enum data_streaming_t tp, unsigned int size_generic_data, bool as_texture):
  m_tp(tp),
  m_texture_size(0, 0),
  m_size(size_generic_data),
  m_current(0u)
{
  if (as_texture)
    {
       m_texture_size.x() = texture_width;
       m_texture_size.y() = size_generic_data >> texture_log2_width;

       if (texture_values_per_texel * m_texture_size.x() * m_texture_size.y() < size_generic_data)
         {
           ++m_texture_size.y();
         }
       m_size = texture_values_per_texel * m_texture_size.x() * m_texture_size.y();

       if (m_tp == data_streaming_bo_mapping)
         {
           m_tp = data_streaming_bo_subdata;
         }
    }

  if (m_tp != data_streaming_bo_mapping || as_texture)
    {
      m_cpu_buffer.resize(m_size);
    }
}

astral::gl::RenderEngineGL3::Implement::Backend::BufferPool::
~BufferPool(void)
{
  ASTRALassert(m_current == 0u);
  if (!m_free_bos.empty())
    {
      if (as_texture())
        {
          astral_glDeleteTextures(m_free_bos.size(), &m_free_bos[0]);
        }
      else
        {
          astral_glDeleteBuffers(m_free_bos.size(), &m_free_bos[0]);
        }
    }

  if (!m_used_bos.empty())
    {
      if (as_texture())
        {
          astral_glDeleteTextures(m_used_bos.size(), &m_used_bos[0]);
        }
      else
        {
          astral_glDeleteBuffers(m_used_bos.size(), &m_used_bos[0]);
        }
    }
}

astral::c_array<astral::generic_data>
astral::gl::RenderEngineGL3::Implement::Backend::BufferPool::
begin_write(void)
{
  ASTRALassert(m_current == 0u);
  if (m_free_bos.empty())
    {
      astral_GLuint bo(0u);

      if (!as_texture())
        {
          astral_glGenBuffers(1, &bo);
          ASTRALassert(bo != 0u);

          astral_glBindBuffer(ASTRAL_GL_UNIFORM_BUFFER, bo);
          if (m_tp != data_streaming_bo_orphaning)
            {
              astral_glBufferData(ASTRAL_GL_UNIFORM_BUFFER, sizeof(generic_data) * m_size, nullptr, ASTRAL_GL_STREAM_DRAW);
            }
        }
      else
        {
          astral_glGenTextures(1, &bo);
          astral_glActiveTexture(data_buffer_texture_binding_point_index + ASTRAL_GL_TEXTURE0);
          astral_glBindTexture(ASTRAL_GL_TEXTURE_2D, bo);
          astral_glTexStorage2D(ASTRAL_GL_TEXTURE_2D, 1, ASTRAL_GL_RGBA32UI, m_texture_size.x(), m_texture_size.y());
          astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D, ASTRAL_GL_TEXTURE_MIN_FILTER, ASTRAL_GL_NEAREST);
          astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D, ASTRAL_GL_TEXTURE_MAG_FILTER, ASTRAL_GL_NEAREST);
        }

      m_free_bos.push_back(bo);
    }

  m_current = m_free_bos.back();
  m_free_bos.pop_back();
  m_used_bos.push_back(m_current);

  #ifndef __EMSCRIPTEN__
  if (!as_texture() && m_tp == data_streaming_bo_mapping)
    {
      void *p;
      generic_data *q;

      astral_glBindBuffer(ASTRAL_GL_UNIFORM_BUFFER, m_current);
      p = astral_glMapBufferRange(ASTRAL_GL_UNIFORM_BUFFER, 0, sizeof(generic_data) * m_size,
                                  ASTRAL_GL_MAP_WRITE_BIT | ASTRAL_GL_MAP_INVALIDATE_BUFFER_BIT | ASTRAL_GL_MAP_FLUSH_EXPLICIT_BIT);
      astral_glBindBuffer(ASTRAL_GL_UNIFORM_BUFFER, 0u);

      q = static_cast<generic_data*>(p);
      m_current_ptr = c_array<generic_data>(q, m_size);
    }
  else
  #endif
    {
      m_current_ptr = make_c_array(m_cpu_buffer);
    }

  return m_current_ptr;
}

astral_GLuint
astral::gl::RenderEngineGL3::Implement::Backend::BufferPool::
end_write(range_type<unsigned int> range_generic_data, unsigned int size_needed)
{
  astral_GLuint return_value(m_current);
  unsigned int cnt(range_generic_data.m_end - range_generic_data.m_begin);

  ASTRALassert(return_value != 0u);
  ASTRALassert(range_generic_data.m_end >= range_generic_data.m_begin);
  ASTRALassert(range_generic_data.m_end <= m_size);

  m_current_ptr = c_array<generic_data>();
  m_current = 0u;

  if (!as_texture())
    {
      if (cnt == 0)
        {
          #ifndef __EMSCRIPTEN__
            {
              if (m_tp == data_streaming_bo_mapping)
                {
                  astral_glBindBuffer(ASTRAL_GL_UNIFORM_BUFFER, return_value);
                  astral_glUnmapBuffer(ASTRAL_GL_UNIFORM_BUFFER);
                  astral_glBindBuffer(ASTRAL_GL_UNIFORM_BUFFER, 0u);
                }
            }
          #endif
          return return_value;
        }

      astral_glBindBuffer(ASTRAL_GL_UNIFORM_BUFFER, return_value);

      switch (m_tp)
        {
        #ifndef __EMSCRIPTEN__
        case data_streaming_bo_mapping:
          astral_glFlushMappedBufferRange(ASTRAL_GL_UNIFORM_BUFFER,
                                          range_generic_data.m_begin * sizeof(generic_data),
                                          cnt * sizeof(generic_data));
          astral_glUnmapBuffer(ASTRAL_GL_UNIFORM_BUFFER);
          break;
        #endif

        case data_streaming_bo_orphaning:
          {
            ASTRALassert(size_needed >= range_generic_data.m_end);
            /* Question: would it be better to collapse the glBufferSubData and
             *           glBufferData calls together?
             */
            astral_glBufferData(ASTRAL_GL_UNIFORM_BUFFER, size_needed * sizeof(generic_data), nullptr, ASTRAL_GL_STREAM_DRAW);
            astral_glBufferSubData(ASTRAL_GL_UNIFORM_BUFFER, range_generic_data.m_begin * sizeof(generic_data),
                                   cnt * sizeof(generic_data),
                                   &m_cpu_buffer[range_generic_data.m_begin]);
          }
          break;

        case data_streaming_bo_subdata:
          astral_glBufferSubData(ASTRAL_GL_UNIFORM_BUFFER, range_generic_data.m_begin * sizeof(generic_data),
                                 cnt * sizeof(generic_data),
                                 &m_cpu_buffer[range_generic_data.m_begin]);
          break;

        default:
          ASTRALassert("Invalid buffer streaming type\n");
        }
      astral_glBindBuffer(ASTRAL_GL_UNIFORM_BUFFER, 0u);
    }
  else
    {
      if (cnt == 0)
        {
          return return_value;
        }

      /* beginning of upload must be aligned */
      ASTRALassert(m_texture_size.x() == texture_width);
      range_generic_data.m_begin &= ~texture_scalar_alignemnt_mask;

      /* ending must also be aligned */
      if (range_generic_data.m_end & texture_scalar_alignemnt_mask)
        {
          range_generic_data.m_end &= ~texture_scalar_alignemnt_mask;
          range_generic_data.m_end += texture_scalar_alignment;
        }

      cnt = range_generic_data.m_end - range_generic_data.m_begin;

      uvec2 upload_dims, upload_loc;

      upload_loc.x() = 0;
      upload_loc.y() = range_generic_data.m_begin >> (texture_log2_values_per_texel + texture_log2_width);

      upload_dims.x() = texture_width;
      upload_dims.y() = cnt >> (texture_log2_values_per_texel + texture_log2_width);

      astral_glActiveTexture(data_buffer_texture_binding_point_index + ASTRAL_GL_TEXTURE0);
      astral_glBindTexture(ASTRAL_GL_TEXTURE_2D, return_value);
      astral_glTexSubImage2D(ASTRAL_GL_TEXTURE_2D, 0,
                             upload_loc.x(), upload_loc.y(),
                             upload_dims.x(), upload_dims.y(),
                             ASTRAL_GL_RGBA_INTEGER, ASTRAL_GL_UNSIGNED_INT,
                             &m_cpu_buffer[range_generic_data.m_begin]);
      astral_glBindTexture(ASTRAL_GL_TEXTURE_2D, 0u);
    }

  return return_value;
}

void
astral::gl::RenderEngineGL3::Implement::Backend::BufferPool::
reset_pool(void)
{
  ASTRALassert(m_current == 0u);

  /* make it so that the bo's that are still free are
   * favored to be used first in the next run.
   */
  unsigned int sz(m_used_bos.size());

  m_used_bos.resize(sz + m_free_bos.size());
  std::copy(m_free_bos.begin(), m_free_bos.end(), m_used_bos.begin() + sz);
  m_free_bos.clear();
  m_free_bos.swap(m_used_bos);
}

////////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::Backend::StagingBuffer methods
astral::gl::RenderEngineGL3::Implement::Backend::StagingBuffer::
StagingBuffer(const Backend &backend):
  m_max_number_vertices(0u),
  m_vertex_blit_vbo(0u),
  m_vertex_blit_ibo(0u),
  m_vertex_blit_vao(0u),
  m_render_vao(0u)
{
  m_max_number_vertices = backend.m_max_vertices_per_surface;

  /* Rendering is attributeless, so the VAO has nothing */
  astral_glGenVertexArrays(1, &m_render_vao);
  astral_glBindVertexArray(m_render_vao);
  astral_glBindVertexArray(0);
  ASTRALassert(m_render_vao != 0u);

  /* generate BO and VAO for rendering to VertexSurface */
  astral_glGenVertexArrays(1, &m_vertex_blit_vao);
  astral_glGenBuffers(1, &m_vertex_blit_vbo);

  ASTRALassert(m_vertex_blit_vao != 0u);
  ASTRALassert(m_vertex_blit_vbo != 0u);

  astral_glBindVertexArray(m_vertex_blit_vao);
  astral_glBindBuffer(ASTRAL_GL_ARRAY_BUFFER, m_vertex_blit_vbo);
  VertexAttribIPointer(0, gl_vertex_attrib_value<uvec4>(sizeof(uvec4), 0));

  if (backend.m_config.m_use_indices)
    {
      astral_glGenBuffers(1, &m_vertex_blit_ibo);
      ASTRALassert(m_vertex_blit_ibo != 0);
      astral_glBindBuffer(ASTRAL_GL_ELEMENT_ARRAY_BUFFER, m_vertex_blit_ibo);
    }

  astral_glBindVertexArray(0);
  astral_glBindBuffer(ASTRAL_GL_ARRAY_BUFFER, 0u);

  /* TODO: add code to support BufferSubData and mapping for
   * streaming vertex blitting vertex buffer (m_vertex_blit_bo).
   */
}

astral::gl::RenderEngineGL3::Implement::Backend::StagingBuffer::
~StagingBuffer()
{
  astral_glDeleteVertexArrays(1, &m_render_vao);
  astral_glDeleteBuffers(1, &m_vertex_blit_vbo);
  if (m_vertex_blit_ibo != 0)
    {
      astral_glDeleteBuffers(1, &m_vertex_blit_ibo);
    }
}

void
astral::gl::RenderEngineGL3::Implement::Backend::StagingBuffer::
begin(const StagingBuffer *prev_buffer)
{
  ASTRALassert(m_vertex_blit_entries.empty());

  if (prev_buffer)
    {
      m_state = prev_buffer->m_state;
    }
  else
    {
      m_state.m_dirty = gpu_dirty_state::depth
        | gpu_dirty_state::stencil
        | gpu_dirty_state::color_mask;

      m_state.m_depth_buffer_mode = depth_buffer_occlude;
      m_state.m_color_writes_enabled = bvec4(true, true, true, true);
      m_state.m_stencil_state = StencilState();
      m_state.m_program = nullptr;
      m_state.m_blend_mode = BackendBlendMode();
    }

  m_current_vert = 0u;
  m_draw_to_set_data_bos = 0;
  m_prev_header_idx = InvalidRenderValue;
}

unsigned int
astral::gl::RenderEngineGL3::Implement::Backend::StagingBuffer::
on_draw_render_data(Backend &backend,
                    c_array<const pointer<const ItemShader>> shaders,
                    unsigned int z,
                    const RenderValues &st,
                    UberShadingKey::Cookie uber_shader_key,
                    RenderValue<ScaleTranslate> tr,
                    ClipWindowValue cl,
                    bool permute_xy,
                    c_array<const std::pair<unsigned int, range_type<int>>> Rs)
{
  /* Step 1: see how much we can fit, if any */
  unsigned int return_value, num_vertices;

  for (num_vertices = 0u, return_value = 0; return_value < Rs.size(); ++return_value)
    {
      unsigned int sz;

      ASTRALassert(Rs[return_value].second.m_begin <= Rs[return_value].second.m_end);

      sz = Rs[return_value].second.difference();
      ASTRALassert(sz <= backend.m_max_vertices_per_surface);

      if (num_vertices + m_current_vert + sz > m_max_number_vertices)
        {
          break;
        }
      num_vertices += sz;
    }

  if (return_value == 0)
    {
      return 0u;
    }

  /* Early out if there are no vertices to process; there are cases where
   * the a draw has no vertices. If we process that draw, we will dirty
   * the state unnecessarily.
   */
  if (num_vertices == 0)
    {
      return return_value;
    }

  /* Step 2: decide if we need a new draw */
  bool need_new_draw(false);

  ShaderBuilder &builder(*backend.m_engine->m_shader_builder);
  const Material &material(st.m_material);
  const MaterialShader *material_shader;
  enum clip_window_value_type_t clip_window_value_type(cl.clip_window_value_type());

  ASTRALassert(!shaders.empty());
  ASTRALassert(shaders.front());
  enum ItemShader::type_t shader_type(shaders.front()->type());

  if (shader_type == ItemShader::color_item_shader)
    {
      material_shader = (material.material_shader()) ?
        material.material_shader() :
        backend.m_engine->m_default_shaders.m_brush_shader.get();
    }
  else
    {
      material_shader = nullptr;
    }

  if (!m_state.m_blend_mode.valid() || backend.requires_emit_gl_blend_state(m_state.m_blend_mode, st.m_blend_mode))
    {
      m_state.m_dirty |= gpu_dirty_state::blend_mode;
      need_new_draw = true;
    }
  m_state.m_blend_mode = st.m_blend_mode;

  if (m_draws.empty() || m_state.m_dirty)
    {
      need_new_draw = true;
    }

  if (backend.requires_new_item_stash(shaders, st, tr, cl.m_clip_window))
    {
      ASTRALassert(m_draw_to_set_data_bos < m_draws.size());
      m_draws[m_draw_to_set_data_bos].set_data_bos(backend, true);

      m_draw_to_set_data_bos = m_draws.size();
      need_new_draw = true;

      /* starting new buffers means the previous header is on a different buffer */
      m_prev_header_idx = InvalidRenderValue;
    }

  Program *uber_pr(nullptr);
  if (uber_shader_key.valid())
    {
      ASTRALassert(material_shader);
      ASTRALassert(builder.uber_has_shaders(uber_shader_key,
                                            shaders, *material_shader,
                                            st.m_blend_mode));
      ASTRALassert(builder.uber_shader_clipping(uber_shader_key) == clip_window_value_type);

      uber_pr = builder.uber_program(uber_shader_key);

      /* if the caller asks for the uber-of-all, skip any callback code */
      if (uber_pr
          && !builder.uber_shader_cookie_is_all_uber_shader(uber_shader_key)
          && backend.m_config.m_uber_shader_fallback != uber_shader_fallback_none
          && !uber_pr->program_linked())
        {
          Program *fallback(nullptr);

          /* only set fallback_uber to the uber-of-all if that is what the config
           * asks for; leaving it as null means the same as-if no uber is to be
           * used. Note that if the uber-of-all is not yet read we also fallback
           * to the per-item uber.
           */
          if (backend.m_config.m_uber_shader_fallback == uber_shader_fallback_uber_all)
            {
              fallback = builder.uber_of_all_program(clip_window_value_type);
              ASTRALassert(fallback);

              if (fallback->program_linked())
                {
                  ++backend.m_stats[number_times_super_uber_used];
                }
              else
                {
                  fallback = nullptr;
                }
            }

          uber_pr = fallback;
        }
    }

  if (!uber_pr && uber_shader_key.valid())
    {
      ++backend.m_stats[number_times_separate_used];
    }

  /* step 3: pack the state */
  Packing::Header header;

  header.m_transformation = backend.pack_data(st.m_transformation);
  header.m_translate = backend.pack_data(tr);
  header.m_item_data = backend.pack_data(st.m_item_data);
  header.m_material_transformation = backend.pack_data(st.m_material_transformation);
  header.m_framebuffer_copy = backend.pack_data(st.m_framebuffer_copy);
  header.m_clip_mask = backend.pack_data(st.m_clip_mask);

  if (st.m_clip_mask.valid())
    {
      const PackedValue<const RenderClipElement*, data_clip_mask> &PV(backend.get(st.m_clip_mask));
      const Packing::ProcessedRenderClipElement &PRCE(PV.raw_value().packable_value());

      /* ick, also tack on the bits for filtering and complementing */
      header.m_clip_mask_bits = PRCE.m_bits
        | Packing::ProcessedRenderClipElement::additional_bits(st.m_clip_mask_filter, st.m_clip_out);
    }
  else
    {
      header.m_clip_mask_bits = 0u;
    }

  if (material_shader)
    {
      header.m_material_shader = material_shader->ID();
      header.m_material_brush = backend.pack_data(material.brush());
      header.m_material_data = backend.pack_data(material.shader_data());
    }
  else
    {
      header.m_material_brush = InvalidRenderValue;
      header.m_material_data = InvalidRenderValue;
      header.m_material_shader = 0u;
    }

  /* Note that RenderBackend::depth_buffer_value_clear is 0
   * and that the depth test is ASTRAL_GL_LEQUAL, thus we can pass the
   * z-value along directly. Packer::pack() and the shader together
   * will handle the special case of depth_buffer_value_occlude.
   */
  header.m_z = z;
  header.m_clip_window = backend.pack_data(cl.m_clip_window);

  if (shader_type == ItemShader::color_item_shader)
    {
      /* Only those ItemShader::color_item_shader() have
       * a blending epilogue
       */
      header.m_blend_mode_shader_epilogue = backend.m_engine->m_shader_builder->blend_mode_shader_epilogue(st.m_blend_mode);
    }
  else
    {
      /* for other shader types, Packing::Header::m_blend_mode_shader_epilogue
       * holds the value to RenderValues::m_mask_shader_clip_mode.
       */
      header.m_blend_mode_shader_epilogue = (st.m_clip_mask.valid()) ?
        st.m_mask_shader_clip_mode :
        mask_item_shader_clip_cutoff;
    }

  /* Pack the headers */
  m_header_shader_idxs.resize(shaders.size());
  for (unsigned int i = 0; i < shaders.size(); ++i)
    {
      ASTRALassert(shaders[i]);
      ASTRALassert(shaders[i]->type() == shader_type);

      header.m_item_shader = shaders[i]->shaderID();
      if (uber_pr)
        {
          ASTRALassert(uber_shader_key.valid());
          m_header_shader_idxs[i].second = uber_pr;

          ASTRALassert(uber_pr->link_success());
        }
      else
        {
          Program *p;

          p = builder.gl_program(*shaders[i], material_shader, st.m_blend_mode, clip_window_value_type);
          if (backend.m_config.m_uber_shader_fallback == uber_shader_fallback_uber_all
              && shaders[i]->type() == ItemShader::color_item_shader
              && !p->program_linked())
            {
              Program *q;

              q = builder.uber_of_all_program(clip_window_value_type);
              if (q->program_linked())
                {
                  p = q;
                  ++backend.m_stats[number_times_super_uber_used];
                }
            }
          ASTRALassert(p && p->link_success());
          m_header_shader_idxs[i].second = p;
        }

      /* only re-use and save headers on the first shader */
      if (i == 0u)
        {
          if (m_prev_header_idx == InvalidRenderValue
              || header != m_prev_header)
            {
              m_prev_header = header;
              m_prev_header_idx = backend.pack_data(header);
            }
          else
            {
              ++backend.m_stats[number_reuses_bufferX + data_header];
            }

          m_header_shader_idxs[i].first = m_prev_header_idx;
        }
      else
        {
          m_header_shader_idxs[i].first = backend.pack_data(header);
        }
    }

  /* add the command to copy the named vertices to the surface */
  for (unsigned int n = 0; n < return_value; ++n)
    {
      range_type<int> R(Rs[n].second);
      unsigned int shader_choice(Rs[n].first);

      ASTRALassert(shader_choice < shaders.size());

      if (R.m_begin < R.m_end)
        {
          unsigned int sz(R.difference());
          VertexSurfaceBlitEntry V;

          if (m_header_shader_idxs[shader_choice].second != m_state.m_program)
            {
              m_state.m_program = m_header_shader_idxs[shader_choice].second;
              m_state.m_dirty |= gpu_dirty_state::shader;
            }

          /* make a new draw if we need one */
          if (need_new_draw || m_state.m_dirty)
            {
              if (!m_draws.empty())
                {
                  m_draws.back().update_draw_call_range_end(m_current_vert);
                }
              m_draws.push_back(DrawCall(m_state, *this));
              m_state.m_dirty = 0u;
              need_new_draw = false;
            }

          V.m_vertex_src.m_begin = R.m_begin;
          V.m_vertex_src.m_end = R.m_end;
          V.m_header_location = pack_bits(ShaderBuilder::header_location_id_bit0,
                                          ShaderBuilder::header_location_id_num_bits,
                                          m_header_shader_idxs[shader_choice].first)
            | pack_bits(ShaderBuilder::header_location_color_space_bit0,
                        ShaderBuilder::header_location_color_space_num_bits,
                        backend.m_fragment_shader_emit_encoding)
            | pack_bits(ShaderBuilder::header_location_permute_xy_bit, 1u,
                        permute_xy ? 1u : 0u);

          if (!m_vertex_blit_entries.empty()
              && m_vertex_blit_entries.back().m_vertex_src.m_end == V.m_vertex_src.m_begin
              && m_vertex_blit_entries.back().m_header_location == V.m_header_location)
            {
              m_vertex_blit_entries.back().m_vertex_src.m_end += sz;
              m_vertex_blit_entries.back().m_vertex_dst.m_end += sz;
            }
          else
            {
              V.m_vertex_dst.m_begin = m_current_vert;
              V.m_vertex_dst.m_end = m_current_vert + sz;
              m_vertex_blit_entries.push_back(V);
            }

          m_current_vert += sz;
        }
    }

  return return_value;
}

void
astral::gl::RenderEngineGL3::Implement::Backend::StagingBuffer::
end(Backend &backend, bool issue_begin_buffers)
{
  if (!m_draws.empty())
    {
      ASTRALassert(m_draw_to_set_data_bos < m_draws.size());
      m_draws.back().update_draw_call_range_end(m_current_vert);
      m_draws[m_draw_to_set_data_bos].set_data_bos(backend, issue_begin_buffers);
    }
  else
    {
      /* we still need to close the buffer pools */
      astral_GLuint tmp;

      if (backend.m_config.m_use_texture_for_uniform_buffer)
        {
          vecN<generic_data, number_data_types> tmp2;
          backend.end_item_stashes_texture(&tmp, &tmp2, issue_begin_buffers);
        }
      else
        {
          vecN<BufferRange, number_data_types> tmp2;
          backend.end_item_stashes_ubo(&tmp, &tmp2, issue_begin_buffers);
        }
    }
}

void
astral::gl::RenderEngineGL3::Implement::Backend::StagingBuffer::
add_vertex_surface_blit_rect(Backend &backend, const VertexSurfaceBlitRect &R)
{
  /* pack data
   *  - .x = pixel location, packed into 16-bit pair.
   *  - .y = source start
   *  - .z = offset from source start
   *  - .w = header index/location (uint)
   */
  gvec4 minx_miny, minx_maxy, maxx_miny, maxx_maxy;

  ASTRALassert(R.m_dst_start.x() + R.m_length <= m_vertex_surface.m_dims.x());
  ASTRALassert(R.m_dst_start.y() < m_vertex_surface.m_dims.y());
  ASTRALassert(R.m_dst_start.x() + R.m_length <= 0xFFFF);
  ASTRALassert(R.m_dst_start.y() + 1u <= 0xFFFF);

  minx_miny.x().u = (R.m_dst_start.x()) | ((R.m_dst_start.y()) << 16u);
  minx_miny.y().u = R.m_src_start;
  minx_miny.z().u = 0;
  minx_miny.w().u = R.m_header_ID;

  minx_maxy = minx_miny;
  minx_maxy.x().u = (R.m_dst_start.x()) | ((R.m_dst_start.y() + 1u) << 16u);

  maxx_miny = minx_miny;
  maxx_miny.x().u = (R.m_dst_start.x() + R.m_length) | ((R.m_dst_start.y()) << 16u);
  maxx_miny.z().u = R.m_length;

  maxx_maxy = maxx_miny;
  maxx_maxy.x().u = (R.m_dst_start.x() + R.m_length) | ((R.m_dst_start.y() + 1u) << 16u);

  if (backend.m_config.m_use_indices)
    {
      static const astral_GLuint idxs[6] =
        {
          0, 1, 2,
          2, 1, 3
        };

      for (unsigned int i = 0, s = m_vertex_blit_cpu_vertex_buffer.size(); i < 6; ++i)
        {
          m_vertex_blit_cpu_index_buffer.push_back(idxs[i] + s);
        }

      m_vertex_blit_cpu_vertex_buffer.push_back(minx_miny);
      m_vertex_blit_cpu_vertex_buffer.push_back(minx_maxy);
      m_vertex_blit_cpu_vertex_buffer.push_back(maxx_miny);
      m_vertex_blit_cpu_vertex_buffer.push_back(maxx_maxy);
    }
  else
    {
      m_vertex_blit_cpu_vertex_buffer.push_back(minx_miny);
      m_vertex_blit_cpu_vertex_buffer.push_back(minx_maxy);
      m_vertex_blit_cpu_vertex_buffer.push_back(maxx_miny);

      m_vertex_blit_cpu_vertex_buffer.push_back(maxx_miny);
      m_vertex_blit_cpu_vertex_buffer.push_back(minx_maxy);
      m_vertex_blit_cpu_vertex_buffer.push_back(maxx_maxy);
    }
}

void
astral::gl::RenderEngineGL3::Implement::Backend::StagingBuffer::
add_vertex_surface_blit(Backend &backend, const VertexSurfaceBlitEntry &B)
{
  unsigned int src, dst;

  src = B.m_vertex_src.m_begin;
  dst = B.m_vertex_dst.m_begin;
  while (src < B.m_vertex_src.m_end)
    {
      VertexSurfaceBlitRect R;

      R.m_src_start = src;
      R.m_dst_start.x() = dst & (backend.m_vertex_surface_width - 1u);
      R.m_dst_start.y() = dst >> backend.m_config.m_log2_gpu_stream_surface_width;
      R.m_header_ID = B.m_header_location;
      R.m_length = t_min(backend.m_vertex_surface_width - R.m_dst_start.x(),
                         B.m_vertex_src.m_end - src);

      ASTRALassert(R.m_length > 0u);
      ASTRALassert(R.m_dst_start.x() + R.m_length <= m_vertex_surface.m_dims.x());
      ASTRALassert(R.m_dst_start.y() < m_vertex_surface.m_dims.y());

      add_vertex_surface_blit_rect(backend, R);

      src += R.m_length;
      dst += R.m_length;
    }
  ASTRALassert(src == B.m_vertex_src.m_end);
  ASTRALassert(dst == B.m_vertex_dst.m_end);
}

void
astral::gl::RenderEngineGL3::Implement::Backend::StagingBuffer::
pre_emit(Backend &backend, astral_GLint recip_half_viewport_uniform_location)
{
  ASTRALassert(recip_half_viewport_uniform_location != -1);
  if (!m_vertex_blit_entries.empty())
    {
      vec2 recip_half_vwp;

      ASTRALassert(m_vertex_blit_cpu_vertex_buffer.empty());
      ASTRALassert(m_vertex_blit_cpu_index_buffer.empty());
      ASTRALassert(m_vertex_blit_vao != 0);

      /* realize each of entries in m_vertex_blit_entries into the vertex surface */
      m_vertex_surface = backend.allocate_vertex_surface(m_current_vert);

      for (const auto &B : m_vertex_blit_entries)
        {
          add_vertex_surface_blit(backend, B);
        }

      astral_glBindVertexArray(m_vertex_blit_vao);

      /* TODO: obey backend.m_config.m_data_streaming instead
       *       of always orphaning the buffer object.
       *
       * TODO: look into storing the blits into a UBO and
       *       issueing a glDrawInstancedArrays() instead.
       *       The potential benefit is that the amount of
       *       data is streamed is cutdown by that the UBO
       *       storage would not repeat any value whereas
       *       vertex storage does. We'd also be able to
       *       make an indexed draw call easily too.
       */
      ASTRALassert(m_vertex_blit_vao != 0);
      astral_glBindBuffer(ASTRAL_GL_ARRAY_BUFFER, m_vertex_blit_vbo);
      astral_glBufferData(ASTRAL_GL_ARRAY_BUFFER,
                          m_vertex_blit_cpu_vertex_buffer.size() * sizeof(gvec4),
                          &m_vertex_blit_cpu_vertex_buffer[0],
                          ASTRAL_GL_STREAM_DRAW);

      if (backend.m_config.m_use_indices)
        {
          /* TODO: We do not need to rebuild the index buffer every frame,
           *       we can instead recycle it since its indices values
           *       do not change. However, on WebGL2 this may NOT be optimal
           *       because some implementations of WebGL2 will perform bounds
           *       checking on the index buffer against the size of the
           *       vertex buffer; if the vertex buffer changes, it may trigger
           *       such a WebGL2 implementation to upload a new index buffer
           *       anyways. Moreover, the purpose of using an index buffer
           *       here is to help compatibility with WebGL2
           */
          ASTRALassert(m_vertex_blit_ibo != 0u);
          astral_glBindBuffer(ASTRAL_GL_ELEMENT_ARRAY_BUFFER, m_vertex_blit_ibo);
          astral_glBufferData(ASTRAL_GL_ELEMENT_ARRAY_BUFFER,
                              m_vertex_blit_cpu_index_buffer.size() * sizeof(astral_GLuint),
                              &m_vertex_blit_cpu_index_buffer[0],
                              ASTRAL_GL_STREAM_DRAW);
        }

      astral_glBindFramebuffer(ASTRAL_GL_FRAMEBUFFER, m_vertex_surface.m_fbo);

      /* TODO: some platforms, namely tiled based renderers would save
       *       bandwidth if we issue a glClear(ASTRAL_GL_COLOR_BUFFER_BIT)
       */

      /* assume that:
       *  - m_engine->m_shader_builder->gpu_streaming_blitter() is active
       *  - clip planes off
       *  - depth test off
       *  - stencil test off
       *  - blending off
       *  - scissor off
       *
       * but we need to set:
       *  - glViewport
       *  - set uniform of shader
       */
      astral_glViewport(0, 0, m_vertex_surface.m_dims.x(), m_vertex_surface.m_dims.y());

      recip_half_vwp = vec2(2.0f) / vec2(m_vertex_surface.m_dims);
      astral_glUniform2f(recip_half_viewport_uniform_location,
                         recip_half_vwp.x(), recip_half_vwp.y());

      if (backend.m_config.m_use_indices)
        {
          astral_glDrawElements(ASTRAL_GL_TRIANGLES, m_vertex_blit_cpu_index_buffer.size(), ASTRAL_GL_UNSIGNED_INT, nullptr);
        }
      else
        {
          astral_glDrawArrays(ASTRAL_GL_TRIANGLES, 0, m_vertex_blit_cpu_vertex_buffer.size());
        }

      ++backend.m_stats[number_draws];
      backend.m_stats[number_blit_entries] += m_vertex_blit_entries.size();
      backend.m_stats[number_blit_rect_vertices] += m_vertex_blit_cpu_vertex_buffer.size();

      m_vertex_blit_cpu_vertex_buffer.clear();
      m_vertex_blit_cpu_index_buffer.clear();
      m_vertex_blit_entries.clear();
    }
}

void
astral::gl::RenderEngineGL3::Implement::Backend::StagingBuffer::
emit_draws(Backend &backend)
{
  astral_glActiveTexture(ASTRAL_GL_TEXTURE0 + vertex_surface_texture_binding_point_index);
  astral_glBindTexture(ASTRAL_GL_TEXTURE_2D, m_vertex_surface.m_texture);

  astral_glActiveTexture(ASTRAL_GL_TEXTURE0 + vertex_backing_texture_binding_point_index);
  astral_glBindTexture(backend.m_engine->m_vertex_backing->binding_point(),
                       backend.m_engine->m_vertex_backing->texture());

  if (backend.m_config.m_use_attributes || backend.m_config.m_use_indices)
    {
      backend.ready_vertex_id_vao(m_current_vert);
      astral_glBindVertexArray(backend.m_vertex_id_vao);
    }
  else
    {
      astral_glBindVertexArray(m_render_vao);
    }

  for (auto &D : m_draws)
    {
      D.emit_draw(backend);
    }

  m_current_vert = 0;
  m_draws.clear();
}

//////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::Backend::StagingBuffer::DrawCall methods
astral::gl::RenderEngineGL3::Implement::Backend::StagingBuffer::DrawCall::
DrawCall(GPUState state, StagingBuffer &staging_buffer):
  m_set_state(state),
  m_data_ubo(0u),
  m_data_texture(0u)
{
  m_draw_call_range.m_begin = m_draw_call_range.m_end = staging_buffer.m_current_vert;
}

void
astral::gl::RenderEngineGL3::Implement::Backend::StagingBuffer::DrawCall::
set_data_bos(Backend &backend, bool issue_begin_buffers)
{
  if (backend.m_config.m_use_texture_for_uniform_buffer)
    {
      backend.end_item_stashes_texture(&m_data_texture, &m_data_texture_offsets, issue_begin_buffers);
    }
  else
    {
      backend.end_item_stashes_ubo(&m_data_ubo, &m_data_ranges, issue_begin_buffers);
    }
}

void
astral::gl::RenderEngineGL3::Implement::Backend::StagingBuffer::DrawCall::
update_draw_call_range_end(unsigned int v)
{
  ASTRALassert(v >= m_draw_call_range.m_end);
  m_draw_call_range.m_end = v;
}

void
astral::gl::RenderEngineGL3::Implement::Backend::StagingBuffer::DrawCall::
emit_draw(Backend &bk)
{
  /* On paper, we could have implementation to allow for
   * RenderEngineGL3 to NOT tell Renderer it needs to
   * framebuffer pixels for various blend modes and
   * have here code that issues glTextureBarrier or
   * glCopyTexSubImage2D() to get the pixels. The latter
   * means that Renderer needs to emit a bounding box
   * of the texels needed.
   */

  if (m_set_state.m_dirty & gpu_dirty_state::shader)
    {
      ++bk.m_stats[number_program_binds];
      m_set_state.m_program->use_program();
    }

  if (m_set_state.m_dirty & gpu_dirty_state::depth)
    {
      emit_gl_depth_buffer_mode(m_set_state.m_depth_buffer_mode);
    }

  if (m_set_state.m_dirty & gpu_dirty_state::stencil)
    {
      emit_gl_set_stencil_state(m_set_state.m_stencil_state, ASTRAL_GL_CW);
    }

  if (m_set_state.m_dirty & gpu_dirty_state::color_mask)
    {
      emit_gl_color_write_mask(m_set_state.m_color_writes_enabled);
    }

  if (m_set_state.m_dirty & gpu_dirty_state::blend_mode)
    {
      ++bk.m_stats[number_blend_state_changes];
      bk.emit_gl_blend_state(m_set_state.m_blend_mode);
    }

  if (m_data_ubo != 0u)
    {
      ASTRALassert(m_data_texture == 0u);
      for (unsigned int i = 0; i < number_data_types; ++i)
        {
          enum data_t tp;
          tp = static_cast<enum data_t>(i);

          astral_glBindBufferRange(ASTRAL_GL_UNIFORM_BUFFER,
                                   data_binding_point_index(tp),
                                   m_data_ubo,
                                   m_data_ranges[i].m_offset,
                                   m_data_ranges[i].m_size);
        }
    }

  if (m_data_texture != 0u)
    {
      ASTRALassert(m_data_ubo == 0u);
      astral_glActiveTexture(ASTRAL_GL_TEXTURE0 + data_buffer_texture_binding_point_index);
      astral_glBindTexture(ASTRAL_GL_TEXTURE_2D, m_data_texture);

      c_array<generic_data> dst;

      /* we also need to specify the offsets into the texture where the data elements are located */
      dst = bk.m_data_texture_offset_buffer_pool->begin_write();
      std::copy(m_data_texture_offsets.begin(), m_data_texture_offsets.end(), dst.begin());

      astral_glBindBufferBase(ASTRAL_GL_UNIFORM_BUFFER,
                              data_texture_offset_ubo_binding_point_index(),
                              bk.m_data_texture_offset_buffer_pool->end_write());
    }

  /* just issue a glDrawArrays, the meaning of m_draw_call_range
   * is just the range into the StagingBuffer's vertex buffer
   */
  ++bk.m_stats[number_draws];
  if (bk.m_config.m_use_indices)
    {
      astral_glDrawElements(ASTRAL_GL_TRIANGLES, m_draw_call_range.difference(),
                            ASTRAL_GL_UNSIGNED_INT, offset_as_pointer<astral_GLuint>(m_draw_call_range.m_begin));
    }
  else
    {
      astral_glDrawArrays(ASTRAL_GL_TRIANGLES, m_draw_call_range.m_begin, m_draw_call_range.difference());
    }
}

/////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::Backend methods
astral::gl::RenderEngineGL3::Implement::Backend::
Backend(Implement &engine):
  RenderBackend(engine),
  m_config(engine.m_config),
  m_engine(&engine),
  m_number_gl_clip_planes(m_engine->m_number_gl_clip_planes),
  m_vertex_id_vao(0u),
  m_vertex_id_buffer(0u),
  m_index_buffer(0u),
  m_vertex_id_buffer_size(0u),
  m_on_end_called_count_since_reset_pools(0)
{
  for (unsigned int i = 0; i < number_data_types; ++i)
    {
      enum data_t tp;
      unsigned int sz;

      tp = static_cast<enum data_t>(i);
      sz = Packing::element_size(tp);

      m_data_stashes[i].init(m_config.m_max_per_draw_call[i], sz);
    }

  m_misc_buffer_pool = ASTRALnew BufferPool(m_config.m_data_streaming,
                                            Packing::misc_buffer_size(),
                                            false);

  m_ubo_item_data_buffer_pool = ASTRALnew BufferPool(m_config.m_data_streaming, m_config.m_uniform_buffer_size,
                                                     m_config.m_use_texture_for_uniform_buffer);

  if (m_config.m_use_texture_for_uniform_buffer)
    {
      m_data_texture_offset_buffer_pool = ASTRALnew BufferPool(m_config.m_data_streaming,
                                                               ASTRAL_ROUND_UP_MULTIPLE_OF4(number_data_types),
                                                               false);
    }

  unsigned int max_texture_size, max_pool_index, w, max_h;

  max_texture_size = context_get<astral_GLint>(ASTRAL_GL_MAX_TEXTURE_SIZE);

  /* not only must the texture be in height no more than
   * context_get<astral_GLint>(ASTRAL_GL_MAX_TEXTURE_SIZE), but the
   * height value must also fit within 16-bits (because
   * the rect blitted to the bottom will have the bottom
   * corner y-coodinate as the height of the texture).
   */
  max_texture_size = t_min(max_texture_size, 0xFFFFu);
  max_pool_index = uint32_log2_floor(max_texture_size);

  w = 1u << m_config.m_log2_gpu_stream_surface_width;
  max_h = 1u << max_pool_index;
  m_vertex_surface_width = w;

  ASTRALassert(w <= max_texture_size);
  ASTRALassert(max_h <= max_texture_size);

  m_max_vertices_per_surface = w * max_h;
  m_vertex_surface_pools.resize(max_pool_index + 1);
  for (unsigned int i = 0; i <= max_pool_index; ++i)
    {
      unsigned int h;

      h = (1u << i);
      m_vertex_surface_pools[i] = ASTRALnew VertexSurfacePool(w, h);
    }
}

astral::gl::RenderEngineGL3::Implement::Backend::
~Backend()
{
  if (m_vertex_id_vao != 0u)
    {
      astral_glDeleteVertexArrays(1, &m_vertex_id_vao);
    }

  if (m_vertex_id_buffer != 0u)
    {
      astral_glDeleteBuffers(1, &m_vertex_id_buffer);
    }

  if (m_index_buffer != 0u)
    {
      astral_glDeleteBuffers(1, &m_index_buffer);
    }
}

uint32_t
astral::gl::RenderEngineGL3::Implement::Backend::
texture_dims_for_uniform_buffer(void)
{
  return BufferPool::texture_width;
}

uint32_t
astral::gl::RenderEngineGL3::Implement::Backend::
log2_texture_dims_for_uniform_buffer(void)
{
  return BufferPool::texture_log2_width;
}

void
astral::gl::RenderEngineGL3::Implement::Backend::
ready_vertex_id_vao(unsigned int sz)
{
  if (sz > m_vertex_id_buffer_size || m_vertex_id_vao == 0u)
    {
      ASTRALassert(m_config.m_use_attributes || m_config.m_use_indices);

      if (m_vertex_id_vao == 0u)
        {
          astral_glGenVertexArrays(1, &m_vertex_id_vao);
          ASTRALassert(m_vertex_id_vao != 0u);
        }

      if (m_vertex_id_buffer == 0 && m_config.m_use_attributes)
        {
          astral_glGenBuffers(1, &m_vertex_id_buffer);
          ASTRALassert(m_vertex_id_buffer != 0u);
        }

      if (m_index_buffer == 0u && m_config.m_use_indices)
        {
          astral_glGenBuffers(1, &m_index_buffer);
          ASTRALassert(m_index_buffer != 0u);
        }

      std::vector<astral_GLuint> values(sz);
      for (unsigned int i = 0; i < sz; ++i)
        {
          values[i] = i;
        }
      astral_glBindVertexArray(m_vertex_id_vao);

      if (m_config.m_use_attributes)
        {
          astral_glBindBuffer(ASTRAL_GL_ARRAY_BUFFER, m_vertex_id_buffer);
          BufferData(ASTRAL_GL_ARRAY_BUFFER, make_c_array(values), ASTRAL_GL_STATIC_DRAW);
          VertexAttribIPointer(0, gl_vertex_attrib_value<astral_GLuint>());
        }

      if (m_config.m_use_indices)
        {
          astral_glBindBuffer(ASTRAL_GL_ELEMENT_ARRAY_BUFFER, m_index_buffer);
          BufferData(ASTRAL_GL_ELEMENT_ARRAY_BUFFER, make_c_array(values), ASTRAL_GL_STATIC_DRAW);
        }

      astral_glBindVertexArray(0);
      m_vertex_id_buffer_size = sz;
    }
}

astral::gl::RenderEngineGL3::Implement::Backend::VertexSurface
astral::gl::RenderEngineGL3::Implement::Backend::
allocate_vertex_surface(unsigned int number_vertices)
{
  unsigned int height_needed, K;

  /* the height needed is given by number_vertices / surface_width,
   * and surface_width = 2^m_config.m_log2_gpu_stream_surface_width,
   * so the height needed is number_vertices >> m_config.m_log2_gpu_stream_surface_width
   */
  height_needed = number_vertices >> m_config.m_log2_gpu_stream_surface_width;

  /* if number_vertices is not a multiple of m_vertex_surface_width
   * we need one more row.
   */
  if ((number_vertices % m_vertex_surface_width) != 0u)
    {
      ++height_needed;
    }

  /* The surfaces from m_vertex_surface_pools[K] have
   * height 2^K.
   */
  K = uint32_log2_ceiling(height_needed);

  ASTRALassert(K < m_vertex_surface_pools.size());
  ASTRALassert(number_vertices <= m_vertex_surface_pools[K]->max_vertices());
  ASTRALassert(K == 0 || number_vertices > m_vertex_surface_pools[K - 1u]->max_vertices());

  m_stats[number_vertex_surface_pixels] += m_vertex_surface_pools[K]->max_vertices();
  return m_vertex_surface_pools[K]->allocate_surface();
}

enum astral::gl::RenderEngineGL3::data_t
astral::gl::RenderEngineGL3::Implement::Backend::
data_t_value(enum ItemDataValueMapping::type_t v)
{
  static const enum data_t values[ItemDataValueMapping::render_value_type_count] =
    {
      [ItemDataValueMapping::render_value_transformation] = data_item_transformation,
      [ItemDataValueMapping::render_value_scale_translate] = data_item_scale_translate,
      [ItemDataValueMapping::render_value_brush] = data_brush,
      [ItemDataValueMapping::render_value_image] = data_image,
      [ItemDataValueMapping::render_value_gradient] = data_gradient,
      [ItemDataValueMapping::render_value_image_transformation] = data_gradient_transformation,
      [ItemDataValueMapping::render_value_clip] = data_clip_window,
      [ItemDataValueMapping::render_value_item_data] = data_item_data,
      [ItemDataValueMapping::render_value_shadow_map] = data_shadow_map,
    };

  ASTRALassert(v < ItemDataValueMapping::render_value_type_count);
  return values[v];
}

void
astral::gl::RenderEngineGL3::Implement::Backend::
data_freespace_requirement(enum data_t tp, uint32_t cookie,
                           vecN<unsigned int, number_data_types> &out_incr)
{
  if (cookie == InvalidRenderValue)
    {
      return;
    }

  switch (tp)
    {
    case data_item_transformation:
      ASTRALassert(cookie < m_packed_transformation.size());
      m_packed_transformation[cookie].data_freespace_requirement(*this, out_incr);
      break;

    case data_item_scale_translate:
      ASTRALassert(cookie < m_packed_translate.size());
      m_packed_translate[cookie].data_freespace_requirement(*this, out_incr);
      break;

    case data_clip_window:
      ASTRALassert(cookie < m_packed_clip_window.size());
      m_packed_clip_window[cookie].data_freespace_requirement(*this, out_incr);
      break;

    case data_brush:
      ASTRALassert(cookie < m_packed_render_brush.size());
      m_packed_render_brush[cookie].data_freespace_requirement(*this, out_incr);
      break;

    case data_image:
      ASTRALassert(cookie < m_packed_image_sampler.size());
      m_packed_image_sampler[cookie].data_freespace_requirement(*this, out_incr);
      break;

    case data_gradient:
      ASTRALassert(cookie < m_packed_gradient.size());
      m_packed_gradient[cookie].data_freespace_requirement(*this, out_incr);
      break;

    case data_gradient_transformation:
      ASTRALassert(cookie < m_packed_gradient_transformation.size());
      m_packed_gradient_transformation[cookie].data_freespace_requirement(*this, out_incr);
      break;

    case data_item_data:
      ASTRALassert(cookie < m_packed_item_data.size());
      m_packed_item_data[cookie].data_freespace_requirement(*this, out_incr);
      break;

    case data_shadow_map:
      ASTRALassert(cookie < m_packed_shadow_maps.size());
      m_packed_shadow_maps[cookie].data_freespace_requirement(*this, out_incr);
      break;

    case data_clip_mask:
      ASTRALassert(cookie < m_packed_clip_masks.size());
      m_packed_clip_masks[cookie].data_freespace_requirement(*this, out_incr);
      break;

    default:
      ASTRALassert(!"Invalid data_t for data_freespace_requirement()");
      break;
    }
}

uint32_t
astral::gl::RenderEngineGL3::Implement::Backend::
pack_data(enum data_t tp, uint32_t cookie)
{
  if (cookie == InvalidRenderValue)
    {
      return Packing::invalid_render_index;
    }

  switch (tp)
    {
    case data_item_transformation:
      ASTRALassert(cookie < m_packed_transformation.size());
      return m_packed_transformation[cookie].pack_data(*this);
      break;

    case data_item_scale_translate:
      ASTRALassert(cookie < m_packed_translate.size());
      return m_packed_translate[cookie].pack_data(*this);
      break;

    case data_clip_window:
      ASTRALassert(cookie < m_packed_clip_window.size());
      return m_packed_clip_window[cookie].pack_data(*this);
      break;

    case data_brush:
      ASTRALassert(cookie < m_packed_render_brush.size());
      return m_packed_render_brush[cookie].pack_data(*this);
      break;

    case data_image:
      ASTRALassert(cookie < m_packed_image_sampler.size());
      return m_packed_image_sampler[cookie].pack_data(*this);
      break;

    case data_gradient:
      ASTRALassert(cookie < m_packed_gradient.size());
      return m_packed_gradient[cookie].pack_data(*this);
      break;

    case data_gradient_transformation:
      ASTRALassert(cookie < m_packed_gradient_transformation.size());
      return m_packed_gradient_transformation[cookie].pack_data(*this);
      break;

    case data_item_data:
      ASTRALassert(cookie < m_packed_item_data.size());
      return m_packed_item_data[cookie].pack_data(*this);
      break;

    case data_shadow_map:
      ASTRALassert(cookie < m_packed_shadow_maps.size());
      return m_packed_shadow_maps[cookie].pack_data(*this);
      break;

    case data_clip_mask:
      ASTRALassert(cookie < m_packed_clip_masks.size());
      return m_packed_clip_masks[cookie].pack_data(*this);
      break;

    default:
      break;
    }

  ASTRALfailure("Invalid data_t for data_freespace_requirement()");
  return InvalidRenderValue;
}

template<typename T, enum astral::gl::RenderEngineGL3::data_t tp>
astral::gl::RenderEngineGL3::Implement::Backend::PackedValue<T, tp>&
astral::gl::RenderEngineGL3::Implement::Backend::
get(RenderValue<T> v, std::vector<PackedValue<T, tp>> &values)
{
  ASTRALassert(v.valid());
  ASTRALassert(v.cookie() < values.size());
  return values[v.cookie()];
}

astral::gl::RenderEngineGL3::Implement::Backend::PackedItemData&
astral::gl::RenderEngineGL3::Implement::Backend::
get(ItemData v)
{
  ASTRALassert(v.valid());
  ASTRALassert(v.cookie() < m_packed_item_data.size());
  return m_packed_item_data[v.cookie()];
}

template<typename T>
void
astral::gl::RenderEngineGL3::Implement::Backend::
data_freespace_requirement(RenderValue<T> v,
                           vecN<unsigned int, RenderEngineGL3::number_data_types> &out_incr)
{
  if (v.valid())
    {
      get(v).data_freespace_requirement(*this, out_incr);
    }
}

template<typename T>
uint32_t
astral::gl::RenderEngineGL3::Implement::Backend::
pack_data(RenderValue<T> v)
{
  if (v.valid())
    {
      return get(v).pack_data(*this);
    }
  else
    {
      return InvalidRenderValue;
    }
}

void
astral::gl::RenderEngineGL3::Implement::Backend::
data_freespace_requirement(ItemData v,
                           vecN<unsigned int, RenderEngineGL3::number_data_types> &out_incr)
{
  if (v.valid())
    {
      get(v).data_freespace_requirement(*this, out_incr);
    }
}

uint32_t
astral::gl::RenderEngineGL3::Implement::Backend::
pack_data(ItemData v)
{
  if (v.valid())
    {
      return get(v).pack_data(*this);
    }
  else
    {
      return InvalidRenderValue;
    }
}

uint32_t
astral::gl::RenderEngineGL3::Implement::Backend::
pack_data(const Packing::Header &header)
{
  c_array<generic_data> dst;
  unsigned int return_value;

  ++m_stats[number_items_bufferX + data_header];
  dst = m_data_stashes[data_header].write_location(&return_value);
  Packing::pack(dst, header);

  return return_value;
}

bool
astral::gl::RenderEngineGL3::Implement::Backend::
requires_new_item_stash(c_array<const pointer<const ItemShader>> shaders,
                        const RenderValues &st,
                        RenderValue<ScaleTranslate> tr,
                        RenderValue<ClipWindow> cl)
{
  vecN<unsigned int, RenderEngineGL3::number_data_types> needed_data_room(0);
  bool return_value(false), need_material(false);

  needed_data_room[data_header] += shaders.size();
  data_freespace_requirement(st.m_transformation, needed_data_room);
  data_freespace_requirement(st.m_material_transformation, needed_data_room);
  data_freespace_requirement(tr, needed_data_room);
  data_freespace_requirement(cl, needed_data_room);
  data_freespace_requirement(st.m_item_data, needed_data_room);
  data_freespace_requirement(st.m_framebuffer_copy, needed_data_room);
  data_freespace_requirement(st.m_clip_mask, needed_data_room);

  /* only ColorItemShader's use a material */
  for (unsigned int i = 0; i < shaders.size() && !need_material; ++i)
    {
      need_material = need_material || (shaders[i]->type() == ItemShader::color_item_shader);
    }

  if (need_material)
    {
      data_freespace_requirement(st.m_material.brush(), needed_data_room);
      data_freespace_requirement(st.m_material.shader_data(), needed_data_room);
    }

  for (unsigned int i = 0; i < RenderEngineGL3::number_data_types; ++i)
    {
      if (m_data_stashes[i].freespace() < needed_data_room[i])
        {
          ++m_stats[number_times_bufferX_full + i];
          return_value = true;
        }
    }

  return return_value;
}

void
astral::gl::RenderEngineGL3::Implement::Backend::
on_draw_render_data(unsigned int z,
                    c_array<const pointer<const ItemShader>> shaders,
                    const RenderValues &st,
                    UberShadingKey::Cookie uber_shader_key,
                    RenderValue<ScaleTranslate> tr,
                    ClipWindowValue cl,
                    bool permute_xy,
                    c_array<const std::pair<unsigned int, range_type<int>>> R)
{
  unsigned int N;

  N = m_current_staging_buffer->on_draw_render_data(*this, shaders, z, st, uber_shader_key, tr, cl, permute_xy, R);
  R = R.sub_array(N);
  while (!R.empty())
    {
      new_staging_buffer();
      N = m_current_staging_buffer->on_draw_render_data(*this, shaders, z, st, uber_shader_key, tr, cl, permute_xy, R);
      ASTRALassert(N > 0u);
      R = R.sub_array(N);
    }
}

void
astral::gl::RenderEngineGL3::Implement::Backend::
color_write_mask(bvec4 b)
{
  m_current_staging_buffer->color_write_mask(b);
}

void
astral::gl::RenderEngineGL3::Implement::Backend::
depth_buffer_mode(enum depth_buffer_mode_t b)
{
  m_current_staging_buffer->depth_buffer_mode(b);
}

void
astral::gl::RenderEngineGL3::Implement::Backend::
set_stencil_state(const StencilState &st)
{
  m_current_staging_buffer->set_stencil_state(st);
}

void
astral::gl::RenderEngineGL3::Implement::Backend::
set_fragment_shader_emit(enum colorspace_t encoding)
{
  m_fragment_shader_emit_encoding = encoding;
}

uint32_t
astral::gl::RenderEngineGL3::Implement::Backend::
allocate_transformation(const Transformation &value)
{
  uint32_t return_value(m_packed_transformation.size());

  m_packed_transformation.push_back(PackedValue<Transformation, data_item_transformation>(value));
  return return_value;
}

const astral::Transformation&
astral::gl::RenderEngineGL3::Implement::Backend::
fetch_transformation(uint32_t cookie)
{
  ASTRALassert(cookie < m_packed_transformation.size());
  return m_packed_transformation[cookie].value();
}

uint32_t
astral::gl::RenderEngineGL3::Implement::Backend::
allocate_translate(const ScaleTranslate &value)
{
  uint32_t return_value(m_packed_translate.size());

  m_packed_translate.push_back(PackedValue<ScaleTranslate, data_item_scale_translate>(value));
  return return_value;
}

const astral::ScaleTranslate&
astral::gl::RenderEngineGL3::Implement::Backend::
fetch_translate(uint32_t cookie)
{
  ASTRALassert(cookie < m_packed_translate.size());
  return m_packed_translate[cookie].value();
}

uint32_t
astral::gl::RenderEngineGL3::Implement::Backend::
allocate_clip_window(const ClipWindow &value)
{
  uint32_t return_value(m_packed_clip_window.size());

  m_packed_clip_window.push_back(PackedValue<ClipWindow, data_clip_window>(value));
  return return_value;
}

const astral::ClipWindow&
astral::gl::RenderEngineGL3::Implement::Backend::
fetch_clip_window(uint32_t cookie)
{
  ASTRALassert(cookie < m_packed_clip_window.size());
  return m_packed_clip_window[cookie].value();
}

uint32_t
astral::gl::RenderEngineGL3::Implement::Backend::
allocate_render_brush(const Brush &value)
{
  uint32_t return_value(m_packed_render_brush.size());

  m_packed_render_brush.push_back(PackedValue<Brush, data_brush>(value));
  return return_value;
}

const astral::Brush&
astral::gl::RenderEngineGL3::Implement::Backend::
fetch_render_brush(uint32_t cookie)
{
  ASTRALassert(cookie < m_packed_render_brush.size());
  return m_packed_render_brush[cookie].value();
}

uint32_t
astral::gl::RenderEngineGL3::Implement::Backend::
allocate_image_sampler(const ImageSampler &value)
{
  uint32_t return_value(m_packed_image_sampler.size());
  PackedValue<ImageSampler, data_image> P(value, m_engine->image_atlas());

  m_packed_image_sampler.push_back(P);
  return return_value;
}

const astral::ImageSampler&
astral::gl::RenderEngineGL3::Implement::Backend::
fetch_image_sampler(uint32_t cookie)
{
  ASTRALassert(cookie < m_packed_image_sampler.size());
  return m_packed_image_sampler[cookie].value();
}

uint32_t
astral::gl::RenderEngineGL3::Implement::Backend::
allocate_gradient(const Gradient &value)
{
  uint32_t return_value(m_packed_gradient.size());

  m_packed_gradient.push_back(PackedValue<Gradient, data_gradient>(value));
  return return_value;
}

const astral::Gradient&
astral::gl::RenderEngineGL3::Implement::Backend::
fetch_gradient(uint32_t cookie)
{
  ASTRALassert(cookie < m_packed_gradient.size());
  return m_packed_gradient[cookie].value();
}

uint32_t
astral::gl::RenderEngineGL3::Implement::Backend::
allocate_image_transformation(const GradientTransformation &value)
{
  uint32_t return_value(m_packed_gradient_transformation.size());

  m_packed_gradient_transformation.push_back(PackedValue<GradientTransformation, data_gradient_transformation>(value));
  return return_value;
}

const astral::GradientTransformation&
astral::gl::RenderEngineGL3::Implement::Backend::
fetch_image_transformation(uint32_t cookie)
{
  ASTRALassert(cookie < m_packed_gradient_transformation.size());
  return m_packed_gradient_transformation[cookie].value();
}

uint32_t
astral::gl::RenderEngineGL3::Implement::Backend::
allocate_shadow_map(const ShadowMap &value)
{
  uint32_t return_value(m_packed_shadow_maps.size());

  m_packed_shadow_maps.push_back(PackedValue<const ShadowMap&, data_shadow_map>(value));
  return return_value;
}

const astral::ShadowMap&
astral::gl::RenderEngineGL3::Implement::Backend::
fetch_shadow_map(uint32_t cookie)
{
  ASTRALassert(cookie < m_packed_shadow_maps.size());
  return m_packed_shadow_maps[cookie].value();
}

uint32_t
astral::gl::RenderEngineGL3::Implement::Backend::
allocate_framebuffer_pixels(const EmulateFramebufferFetch &value)
{
  uint32_t return_value(m_packed_framebuffer_pixels.size());

  m_packed_framebuffer_pixels.push_back(PackedValue<EmulateFramebufferFetch, data_item_transformation>(value));
  return return_value;
}

const astral::EmulateFramebufferFetch&
astral::gl::RenderEngineGL3::Implement::Backend::
fetch_framebuffer_pixels(uint32_t cookie)
{
  ASTRALassert(cookie < m_packed_framebuffer_pixels.size());
  return m_packed_framebuffer_pixels[cookie].value();
}

uint32_t
astral::gl::RenderEngineGL3::Implement::Backend::
allocate_render_clip_element(const RenderClipElement *value)
{
  uint32_t return_value(m_packed_clip_masks.size());

  m_packed_clip_masks.push_back(PackedValue<const RenderClipElement*, data_clip_mask>(value));
  return return_value;
}

uint32_t
astral::gl::RenderEngineGL3::Implement::Backend::
allocate_item_data(c_array<const gvec4> value,
                   c_array<const ItemDataValueMapping::entry> item_data_value_map,
                   const ItemDataDependencies &dependencies)
{
  uint32_t return_value(m_packed_item_data.size());
  range_type<unsigned int> R(m_item_data_backing.size(),
                             m_item_data_backing.size() + value.size());
  range_type<unsigned int> M(m_item_data_interpretation_backing.size(),
                             m_item_data_interpretation_backing.size() + item_data_value_map.size());
  range_type<unsigned int> TD(m_item_data_image_id_backing.size(),
                              m_item_data_image_id_backing.size());
  range_type<unsigned int> SM(m_item_data_shadow_map_id_backing.size(),
                              m_item_data_shadow_map_id_backing.size());

  m_item_data_backing.resize(R.m_end);
  std::copy(value.begin(), value.end(), m_item_data_backing.begin() + R.m_begin);

  for (const auto &id : dependencies.m_images)
    {
      m_item_data_image_id_backing.push_back(id);
    }

  for (const auto &id : dependencies.m_shadow_maps)
    {
      m_item_data_shadow_map_id_backing.push_back(id);
    }

  if (!item_data_value_map.empty())
    {
      ItemDataValueMapping::entry E(ItemDataValueMapping::render_value_type_count,
                                    ItemDataValueMapping::x_channel,
                                    ~0u);

      m_item_data_interpretation_backing.resize(M.m_end, E);
      std::copy(item_data_value_map.begin(), item_data_value_map.end(),
                m_item_data_interpretation_backing.begin() + M.m_begin);

      for (const auto &e : item_data_value_map)
        {
          enum data_t b;
          uint32_t cookie;
          ImageID tid;

          cookie = value[e.m_component][e.m_channel].u;
          if (cookie == InvalidRenderValue)
            continue;

          b = data_t_value(e.m_type);
          switch (b)
            {
            case data_image:
              tid = fetch_image_sampler(cookie).image_id();
              break;

            case data_brush:
              tid = image_id(fetch_render_brush(cookie).m_image);
              break;

            case data_item_data:
              {
                /* copy-paste the values of the source ItemData */
                c_array<const ImageID> tids;

                tids = image_id_of_item_data(cookie);
                if (!tids.empty())
                  {
                    unsigned int prev_size(m_item_data_image_id_backing.size());

                    m_item_data_image_id_backing.resize(prev_size + tids.size());
                    std::copy(tids.begin(), tids.end(),
                              m_item_data_image_id_backing.begin() + prev_size);
                  }

                c_array<const ShadowMapID> smids;
                smids = shadow_map_id_of_item_data(cookie);
                if (!smids.empty())
                  {
                    unsigned int prev_size(m_item_data_shadow_map_id_backing.size());

                    m_item_data_shadow_map_id_backing.resize(prev_size + smids.size());
                    std::copy(smids.begin(), smids.end(),
                              m_item_data_shadow_map_id_backing.begin() + prev_size);
                  }
              }
              break;

            case data_shadow_map:
              {
                ShadowMapID smid;

                smid = fetch_shadow_map(cookie).ID();
                if (smid.valid())
                  {
                    m_item_data_shadow_map_id_backing.push_back(smid);
                  }
              }
              break;

            default:
              break;
            }

          if (tid.valid())
            {
              m_item_data_image_id_backing.push_back(tid);
            }
        }
    }

  TD.m_end = m_item_data_image_id_backing.size();
  SM.m_end = m_item_data_shadow_map_id_backing.size();

  m_packed_item_data.push_back(PackedItemData(R, M, TD, SM));
  return return_value;
}

astral::c_array<const astral::gvec4>
astral::gl::RenderEngineGL3::Implement::Backend::
fetch_item_data(uint32_t cookie)
{
  ASTRALassert(cookie < m_packed_item_data.size());
  return m_packed_item_data[cookie].data(*this);
}

astral::c_array<const astral::ImageID>
astral::gl::RenderEngineGL3::Implement::Backend::
image_id_of_item_data(uint32_t cookie)
{
  ASTRALassert(cookie < m_packed_item_data.size());
  return m_packed_item_data[cookie].image_id(*this);
}

astral::c_array<const astral::ShadowMapID>
astral::gl::RenderEngineGL3::Implement::Backend::
shadow_map_id_of_item_data(uint32_t cookie)
{
  ASTRALassert(cookie < m_packed_item_data.size());
  return m_packed_item_data[cookie].shadow_map_id(*this);
}

void
astral::gl::RenderEngineGL3::Implement::Backend::
on_begin_render_target(const ClearParams &clear_params, RenderTarget &rt)
{
  ASTRALassert(dynamic_cast<RenderTargetGL*>(&rt));

  m_clear_current_rt_params = clear_params;
  m_current_rt_is_shadowmap_backing = (&rt == m_engine->shadow_map_atlas().render_target());

  /* default to sRGB encoding */
  m_fragment_shader_emit_encoding = colorspace_srgb;

  ASTRALassert(m_active_staging_buffers.empty());
  ASTRALassert(!m_current_staging_buffer);

  for (unsigned int i = 0; i < number_data_types; ++i)
    {
      m_data_stashes[i].begin_write();
    }

  m_ubo_item_data_data_ptr = m_ubo_item_data_buffer_pool->begin_write();
  m_ubo_item_data_location = 0u;
  m_ubo_item_data_last_size_needed = 0u;
  new_staging_buffer();

  /* on each render target, we refresh our checking for if
   * a gl::Program is ready to be used.
   */
  Program::increment_global_query_counter();
}

void
astral::gl::RenderEngineGL3::Implement::Backend::
new_staging_buffer(void)
{
  const StagingBuffer *prev_buffer(nullptr);

  if (m_current_staging_buffer)
    {
      prev_buffer = m_current_staging_buffer.get();
      m_current_staging_buffer->end(*this, true);
      m_active_staging_buffers.push_back(m_current_staging_buffer);
    }

  if (m_staging_buffer_pool.empty())
    {
      m_staging_buffer_pool.push_back(ASTRALnew StagingBuffer(*this));
    }

  m_current_staging_buffer = m_staging_buffer_pool.back();
  m_staging_buffer_pool.pop_back();

  ++m_stats[number_staging_buffers];
  m_current_staging_buffer->begin(prev_buffer);
}

void
astral::gl::RenderEngineGL3::Implement::Backend::
on_end_render_target(RenderTarget &rt)
{
  ASTRALassert(m_current_staging_buffer);
  m_current_staging_buffer->end(*this, false);
  m_active_staging_buffers.push_back(m_current_staging_buffer);

  for (unsigned int i = 0; i < m_number_gl_clip_planes; ++i)
    {
      astral_glDisable(ASTRAL_GL_CLIP_DISTANCE0 + i);
    }

  astral_GLint uniform_location;

  /* Issue the vertex-blits for each StagingBuffer, note that because
   * the underlying color buffers for pre_emit() are not sRGB buffers
   */
  astral_glDisable(ASTRAL_GL_SCISSOR_TEST);
  astral_glDisable(ASTRAL_GL_DEPTH_TEST);
  astral_glDisable(ASTRAL_GL_STENCIL_TEST);
  astral_glDisable(ASTRAL_GL_BLEND);
  astral_glColorMask(ASTRAL_GL_TRUE, ASTRAL_GL_TRUE, ASTRAL_GL_TRUE, ASTRAL_GL_TRUE);
  astral_glFrontFace(ASTRAL_GL_CW);

  m_engine->m_shader_builder->gpu_streaming_blitter(&uniform_location)->use_program();
  for (const auto &b : m_active_staging_buffers)
    {
      b->pre_emit(*this, uniform_location);
    }

  /* now emit the rendering */
  emit_gl_begin_render_target(m_clear_current_rt_params, rt);
  if (m_config.m_use_hw_clip_window)
    {
      for (unsigned int i = 0; i < 4u; ++i)
        {
          astral_glEnable(ASTRAL_GL_CLIP_DISTANCE0 + i);
        }
    }

  Packing::pack_misc_buffer(m_misc_buffer_pool->begin_write(), *m_engine, rt);
  astral_glBindBufferBase(ASTRAL_GL_UNIFORM_BUFFER,
                          misc_data_binding_point_index(),
                          m_misc_buffer_pool->end_write());

  astral_glActiveTexture(ASTRAL_GL_TEXTURE0 + colorstop_atlas_binding_point_index);
  astral_glBindSampler(colorstop_atlas_binding_point_index, 0);
  astral_glBindTexture(ASTRAL_GL_TEXTURE_2D, m_engine->m_colorstop_atlas->texture());

  astral_glActiveTexture(ASTRAL_GL_TEXTURE0 + static_data32_texture_binding_point_index);
  astral_glBindSampler(static_data32_texture_binding_point_index, 0);
  astral_glBindTexture(m_engine->m_static_data_atlas->binding_point(), m_engine->m_static_data_atlas->texture());

  astral_glActiveTexture(ASTRAL_GL_TEXTURE0 + static_data16_texture_binding_point_index);
  astral_glBindSampler(static_data16_texture_binding_point_index, 0);
  astral_glBindTexture(m_engine->m_static_data_atlas->binding_point(), m_engine->m_static_data_fp16_atlas->texture());

  astral_glActiveTexture(ASTRAL_GL_TEXTURE0 + color_tile_image_atlas_binding_point_index);
  astral_glBindSampler(color_tile_image_atlas_binding_point_index, 0);
  astral_glBindTexture(ASTRAL_GL_TEXTURE_2D_ARRAY, m_engine->m_image_color_backing->texture());

  astral_glActiveTexture(ASTRAL_GL_TEXTURE0 + index_tile_image_atlas_binding_point_index);
  astral_glBindSampler(index_tile_image_atlas_binding_point_index, 0);
  astral_glBindTexture(ASTRAL_GL_TEXTURE_2D_ARRAY, m_engine->m_image_index_backing->texture());

  astral_glActiveTexture(ASTRAL_GL_TEXTURE0 + shadow_map_atlas_binding_point_index);
  astral_glBindSampler(shadow_map_atlas_binding_point_index, 0);

  #ifdef __EMSCRIPTEN__
    {
      /* Test on Chrome's WebGL2 gives that the distance lookup does not work
       * correctly if the depth texture has linear filtering. The upshot is
       * that the "distance" value from a ShadowMap lookup needs to be unfiltered
       * under WebGL2.
       */
      astral_glBindSampler(shadow_map_atlas_binding_point_index, 0u);
    }
  #else
    {
      astral_glBindSampler(shadow_map_atlas_binding_point_index, m_engine->m_shadow_map_backing->linear_sampler());
    }
  #endif

  if (m_current_rt_is_shadowmap_backing)
    {
      astral_glActiveTexture(ASTRAL_GL_TEXTURE0 + shadow_map_atlas_binding_point_index);
      astral_glBindTexture(ASTRAL_GL_TEXTURE_2D, 0u);
    }
  else
    {
      astral_glActiveTexture(ASTRAL_GL_TEXTURE0 + shadow_map_atlas_binding_point_index);
      astral_glBindTexture(ASTRAL_GL_TEXTURE_2D, m_engine->m_shadow_map_backing->texture());
    }

  astral_glHint(ASTRAL_GL_FRAGMENT_SHADER_DERIVATIVE_HINT, ASTRAL_GL_NICEST);

  for (const auto &b : m_active_staging_buffers)
    {
      b->emit_draws(*this);
    }

  /* reset staging buffers for next render target */
  unsigned int sz(m_staging_buffer_pool.size());
  m_staging_buffer_pool.resize(sz + m_active_staging_buffers.size());
  std::copy(m_active_staging_buffers.begin(),
            m_active_staging_buffers.end(),
            m_staging_buffer_pool.begin() + sz);
  m_active_staging_buffers.clear();
  m_current_staging_buffer = nullptr;
}

void
astral::gl::RenderEngineGL3::Implement::Backend::
reset_pools(void)
{
  m_misc_buffer_pool->reset_pool();
  m_ubo_item_data_buffer_pool->reset_pool();

  for (const auto &p : m_vertex_surface_pools)
    {
      p->reset_pool();
    }

  if (m_data_texture_offset_buffer_pool)
    {
      m_data_texture_offset_buffer_pool->reset_pool();
    }
}

void
astral::gl::RenderEngineGL3::Implement::Backend::
on_begin(void)
{
  ASTRALassert(m_packed_transformation.empty());
  ASTRALassert(m_packed_translate.empty());
  ASTRALassert(m_packed_clip_window.empty());
  ASTRALassert(m_packed_render_brush.empty());
  ASTRALassert(m_packed_image_sampler.empty());
  ASTRALassert(m_packed_gradient.empty());
  ASTRALassert(m_packed_gradient_transformation.empty());
  ASTRALassert(m_packed_shadow_maps.empty());
  ASTRALassert(m_packed_framebuffer_pixels.empty());
  ASTRALassert(m_packed_clip_masks.empty());
  ASTRALassert(m_packed_item_data.empty());
  ASTRALassert(m_item_data_backing.empty());
  ASTRALassert(m_item_data_image_id_backing.empty());
  ASTRALassert(m_item_data_shadow_map_id_backing.empty());
  ASTRALassert(m_item_data_interpretation_backing.empty());

  m_current_item_stash = 0;
  std::fill(m_stats.begin(), m_stats.end(), 0);

  Implement::init_gl_state();
}

void
astral::gl::RenderEngineGL3::Implement::Backend::
on_end(c_array<unsigned int> stats)
{
  m_packed_transformation.clear();
  m_packed_translate.clear();
  m_packed_clip_window.clear();
  m_packed_render_brush.clear();
  m_packed_image_sampler.clear();
  m_packed_gradient.clear();
  m_packed_gradient_transformation.clear();
  m_packed_shadow_maps.clear();
  m_packed_framebuffer_pixels.clear();
  m_packed_clip_masks.clear();
  m_packed_item_data.clear();
  m_item_data_backing.clear();
  m_item_data_image_id_backing.clear();
  m_item_data_shadow_map_id_backing.clear();
  m_item_data_interpretation_backing.clear();

  ASTRALassert(m_active_staging_buffers.empty());
  ASTRALassert(!m_current_staging_buffer);

  /* QUESTION: when is a good time to reset the pools?
   *           Delaying all the way to on_end() means
   *           potentially quite a bit of buffers.
   */
  ++m_on_end_called_count_since_reset_pools;
  if (m_on_end_called_count_since_reset_pools >= m_config.m_buffer_reuse_period)
    {
      reset_pools();
      m_on_end_called_count_since_reset_pools = 0;
    }

  /* do not let our GL objects leak to the caller */
  Implement::unbind_objects();

  unsigned int sz;
  sz = m_stats[written_ubo_bytes] + m_stats[unwritten_ubo_bytes];
  if (sz != 0)
    {
      m_stats[percentage_ubo_written] = (100 * m_stats[written_ubo_bytes]) / sz;
    }
  else
    {
      m_stats[percentage_ubo_written] = 0;
    }

  std::copy(m_stats.begin(), m_stats.end(), stats.begin());
}

void
astral::gl::RenderEngineGL3::Implement::Backend::
end_item_stashes_texture(astral_GLuint *dst_texture,
                         vecN<generic_data, number_data_types> *pdst_offsets,
                         bool issue_begin_stashes)
{
  vecN<generic_data, number_data_types> &dst_offsets(*pdst_offsets);
  vecN<c_array<const generic_data>, number_data_types> datas;
  unsigned int size_needed = 0;

  ++m_stats[number_item_groups];
  for (unsigned int i = 0; i < number_data_types; ++i)
    {
      datas[i] = m_data_stashes[i].end_write();
      size_needed += datas[i].size();
    }

  if (m_ubo_item_data_location + size_needed > m_ubo_item_data_data_ptr.size())
    {
      m_stats[written_ubo_bytes] += sizeof(generic_data) * m_ubo_item_data_location;
      ++m_stats[number_item_buffers];

      m_ubo_item_data_buffer_pool->end_write(range_type<unsigned int>(0, m_ubo_item_data_location), m_ubo_item_data_location);
      m_ubo_item_data_data_ptr = m_ubo_item_data_buffer_pool->begin_write();
      m_ubo_item_data_location = 0u;
    }

  /* write the data to the texture acting as a virtual buffer and record the offset to where */
  for (unsigned int i = 0; i < number_data_types; ++i)
    {
      ASTRALassert((m_ubo_item_data_location & BufferPool::texture_values_per_texel_mask) == 0u);

      dst_offsets[i].u = m_ubo_item_data_location >> BufferPool::texture_log2_values_per_texel;
      std::copy(datas[i].begin(), datas[i].end(), m_ubo_item_data_data_ptr.begin() + m_ubo_item_data_location);
      m_ubo_item_data_location += datas[i].size();
    }

  /* tell the backing texture ID */
  *dst_texture = m_ubo_item_data_buffer_pool->current_bo();

  if (issue_begin_stashes)
    {
      for (unsigned int i = 0; i < number_data_types; ++i)
        {
          m_data_stashes[i].begin_write();
        }
    }
  else
    {
      m_stats[written_ubo_bytes] += sizeof(generic_data) * m_ubo_item_data_location;
      ++m_stats[number_item_buffers];

      m_ubo_item_data_buffer_pool->end_write(range_type<unsigned int>(0, m_ubo_item_data_location), m_ubo_item_data_location);
      m_ubo_item_data_location = 0u;
    }

  ++m_current_item_stash;
}

void
astral::gl::RenderEngineGL3::Implement::Backend::
end_item_stashes_ubo(astral_GLuint *dst_ubo,
                     vecN<BufferRange, number_data_types> *pdst_ranges,
                     bool issue_begin_stashes)
{
  vecN<BufferRange, number_data_types> &dst_ranges(*pdst_ranges);
  vecN<c_array<const generic_data>, number_data_types> datas;
  vecN<unsigned int, number_data_types> advance;
  unsigned int size_needed, total_advance;

  ++m_stats[number_item_groups];
  for (unsigned int i = 0; i < number_data_types; ++i)
    {
      datas[i] = m_data_stashes[i].end_write();
    }

  /* see if the current UBO buffer can fit all of the
   * data and the necessary after padding; along the
   * way, compute the offsets relative to the current
   * UBO write location.
   */
  size_needed = 0u;
  total_advance = 0u;
  for (unsigned int i = 0; i < number_data_types; ++i)
    {
      unsigned int padding, sz;

      /* record the offset/size for glBindBufferRange */
      dst_ranges[i].m_size = m_data_stashes[i].size_bytes();
      dst_ranges[i].m_offset = total_advance * sizeof(generic_data);
      ASTRALassert((dst_ranges[i].m_offset & 0xFF) == 0u);

      /* the actual size needed is the offset + the size in bytes */
      sz = dst_ranges[i].m_offset + dst_ranges[i].m_size;
      size_needed = t_max(size_needed, sz);

      /* glBindBufferBase requires that the offset is a multiple of
       * context_get<astral_GLint>(ASTRAL_GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT) which
       * at worst is 256 (bytes). Rather than doing the trickiness
       * of adapting to that alignment, we just always pad to
       * 256 byte boundaries, which is 64 generic_data boundary.
       */
      if ((datas[i].size() & 63u) != 0u)
        {
          padding = 64u - (datas[i].size() & 63u);
        }
      else
        {
          padding = 0u;
        }
      m_stats[padded_ubo_bytes] += sizeof(generic_data) * padding;

      advance[i] = datas[i].size() + padding;
      ASTRALassert((advance[i] & 63u) == 0u);

      total_advance += advance[i];
    }

  /* NOTE: size_needed is made large enough so that all
   * of the UBO's are fully backed, even if only a much
   * smaller number would be accessed by the shader.
   * This is because a number of platforms exhibit poor
   * performance if a UBO in a shader is not fully backed.
   * One platform is MacOS. In addition, WebGL2 requires
   * that a UBO is fully backed. None of this would be
   * necesary if we used TBO's; in that case all of the
   * data would be a single flat buffer and reading outside
   * of it is well defined to return 0. Sadly, TBO's are
   * not present in WebGL2.
   */
  if (m_ubo_item_data_location + size_needed > m_ubo_item_data_data_ptr.size())
    {
      /* start a new buffer to fit the new data */
      m_stats[unwritten_ubo_bytes] += sizeof(generic_data) * (m_ubo_item_data_data_ptr.size() - m_ubo_item_data_location);
      m_stats[written_ubo_bytes] += sizeof(generic_data) * m_ubo_item_data_location;
      ++m_stats[number_item_buffers];

      m_ubo_item_data_buffer_pool->end_write(range_type<unsigned int>(0, m_ubo_item_data_location), m_ubo_item_data_last_size_needed);
      m_ubo_item_data_data_ptr = m_ubo_item_data_buffer_pool->begin_write();
      m_ubo_item_data_location = 0u;
      m_ubo_item_data_last_size_needed = 0u;
    }

  /* modify dst_ranges[i].m_offset by m_ubo_item_data_location and write the data to the UBO */
  for (unsigned int i = 0, loc = m_ubo_item_data_location; i < number_data_types; ++i)
    {
      dst_ranges[i].m_offset += m_ubo_item_data_location * sizeof(generic_data);
      std::copy(datas[i].begin(), datas[i].end(), m_ubo_item_data_data_ptr.begin() + loc);
      loc += advance[i];
    }

  /* save the size needed to fully back all UBO's for the next call to end_write() */
  m_ubo_item_data_last_size_needed = m_ubo_item_data_location + size_needed;

  /* increment m_ubo_item_data_location by the total advance */
  m_ubo_item_data_location += total_advance;

  // ubo_location should be 64-generic_data boundary aligned
  ASTRALassert((m_ubo_item_data_location & 63u) == 0u);

  /* tell the backing BO */
  *dst_ubo = m_ubo_item_data_buffer_pool->current_bo();

  if (issue_begin_stashes)
    {
      for (unsigned int i = 0; i < number_data_types; ++i)
        {
          m_data_stashes[i].begin_write();
        }
    }
  else
    {
      m_stats[unwritten_ubo_bytes] += sizeof(generic_data) * (m_ubo_item_data_data_ptr.size() - m_ubo_item_data_location);
      m_stats[written_ubo_bytes] += sizeof(generic_data) * m_ubo_item_data_location;
      ++m_stats[number_item_buffers];

      m_ubo_item_data_buffer_pool->end_write(range_type<unsigned int>(0, m_ubo_item_data_location), m_ubo_item_data_last_size_needed);
      m_ubo_item_data_location = 0u;
      m_ubo_item_data_last_size_needed = 0u;
    }

  ++m_current_item_stash;
}

bool
astral::gl::RenderEngineGL3::Implement::Backend::
requires_emit_gl_blend_state(BackendBlendMode modeA, BackendBlendMode modeB)
{
  const BlendBuilder &blend_builder(m_engine->m_shader_builder->blend_builder());
  const BlendBuilder::PerBlendMode &A(blend_builder.info(modeA));
  const BlendBuilder::PerBlendMode &B(blend_builder.info(modeB));

  return A.requires_emit_gl_blend_state(B);
}

void
astral::gl::RenderEngineGL3::Implement::Backend::
emit_gl_blend_state(BackendBlendMode mode)
{
  const BlendBuilder &blend_builder(m_engine->m_shader_builder->blend_builder());
  const BlendBuilder::PerBlendMode &blend_info(blend_builder.info(mode));

  blend_info.emit_gl_blend_state();
}

astral::c_string
astral::gl::RenderEngineGL3::Implement::Backend::
render_stats_label_derived(unsigned int idx) const
{
  static const c_string labels[number_total_stats] =
    {
      [number_program_binds] = "gl3_number_program_binds",
      [number_blend_state_changes] = "gl3_number_blend_state_changes",
      [number_item_groups] = "gl3_number_item_groups",
      [number_item_buffers] = "gl3_number_item_buffers",
      [unwritten_ubo_bytes] = "gl3_unwritten_ubo_bytes",
      [written_ubo_bytes] = "gl3_written_ubo_bytes  ",
      [percentage_ubo_written] = "gl3_percentage_ubo_written",
      [padded_ubo_bytes] = "gl3_padded_ubo_bytes",
      [number_draws] = "gl3_number_draws",
      [number_staging_buffers] = "gl3_number_staging_buffers",
      [number_blit_entries] = "gl3_number_blit_entries",
      [number_blit_rect_vertices] = "gl3_number_blit_rect_vertices",
      [number_vertex_surface_pixels] = "gl3_number_vertex_surface_pixels",
      [number_times_super_uber_used] = "gl3_number_times_super_uber_used",
      [number_times_separate_used] = "gl3_number_times_separate_used",

      [number_items_bufferX + data_header] = "gl3_number_items_data_header",
      [number_items_bufferX + data_item_transformation] = "gl3_number_items_data_item_transformation",
      [number_items_bufferX + data_item_scale_translate] = "gl3_number_items_data_item_scale_translate",
      [number_items_bufferX + data_clip_window] = "gl3_number_items_data_clip_window",
      [number_items_bufferX + data_brush] = "gl3_number_items_data_brush",
      [number_items_bufferX + data_gradient] = "gl3_number_items_data_gradient",
      [number_items_bufferX + data_gradient_transformation] = "gl3_number_items_data_gradient_transformation",
      [number_items_bufferX + data_item_data] = "gl3_number_items_data_item_data",
      [number_items_bufferX + data_image] = "gl3_number_items_data_image",
      [number_items_bufferX + data_shadow_map] = "gl3_number_items_data_shadow_map",
      [number_items_bufferX + data_clip_mask] = "gl3_number_items_data_clip_mask",

      [number_reuses_bufferX + data_header] = "gl3_number_reuses_data_header",
      [number_reuses_bufferX + data_item_transformation] = "gl3_number_reuses_data_item_transformation",
      [number_reuses_bufferX + data_item_scale_translate] = "gl3_number_reuses_data_item_scale_translate",
      [number_reuses_bufferX + data_clip_window] = "gl3_number_reuses_data_clip_window",
      [number_reuses_bufferX + data_brush] = "gl3_number_reuses_data_brush",
      [number_reuses_bufferX + data_gradient] = "gl3_number_reuses_data_gradient",
      [number_reuses_bufferX + data_gradient_transformation] = "gl3_number_reuses_data_gradient_transformation",
      [number_reuses_bufferX + data_item_data] = "gl3_number_reuses_data_item_data",
      [number_reuses_bufferX + data_image] = "gl3_number_reuses_data_image",
      [number_reuses_bufferX + data_shadow_map] = "gl3_number_reuses_data_shadow_map",
      [number_reuses_bufferX + data_clip_mask] = "gl3_number_reuses_data_clip_mask",

      [number_times_bufferX_full + data_header] = "gl3_number_times_data_header_full",
      [number_times_bufferX_full + data_item_transformation] = "gl3_number_times_data_item_transformation_full",
      [number_times_bufferX_full + data_item_scale_translate] = "gl3_number_times_data_item_scale_translate_full",
      [number_times_bufferX_full + data_clip_window] = "gl3_number_times_data_clip_window_full",
      [number_times_bufferX_full + data_brush] = "gl3_number_times_data_brush_full",
      [number_times_bufferX_full + data_gradient] = "gl3_number_times_data_gradient_full",
      [number_times_bufferX_full + data_gradient_transformation] = "gl3_number_times_data_gradient_transformation_full",
      [number_times_bufferX_full + data_item_data] = "gl3_number_times_data_item_data_full",
      [number_times_bufferX_full + data_image] = "gl3_number_tiles_data_image_full",
      [number_times_bufferX_full + data_shadow_map] = "gl3_number_data_shadow_map_full",
      [number_times_bufferX_full + data_clip_mask] = "gl3_number_data_clip_mask_full",
    };

  ASTRALassert(idx < number_total_stats);
  ASTRALassert(labels[idx]);

  return labels[idx];
}

astral::reference_counted_ptr<astral::RenderBackend::UberShadingKey>
astral::gl::RenderEngineGL3::Implement::Backend::
create_uber_shading_key(void)
{
  const MaterialShader *default_brush;

  default_brush = m_engine->m_default_shaders.m_brush_shader.get();
  return m_engine->m_shader_builder->create_uber_shading_key(default_brush);
}

/*!
 * \file render_engine_gl3_backend.hpp
 * \brief render_engine_gl3_backend.hpp
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

#ifndef ASTRAL_RENDER_ENGINE_GL3_BACKEND_HPP
#define ASTRAL_RENDER_ENGINE_GL3_BACKEND_HPP

#include <astral/renderer/renderer.hpp>
#include <astral/renderer/gl3/render_target_gl3.hpp>
#include <astral/renderer/gl3/render_engine_gl3.hpp>
#include <astral/util/gl/astral_gl.hpp>
#include <astral/util/gl/gl_vertex_attrib.hpp>
#include "render_engine_gl3_implement.hpp"
#include "render_engine_gl3_packing.hpp"

class astral::gl::RenderEngineGL3::Implement::Backend:public RenderBackend
{
public:
  explicit
  Backend(Implement &engine);

  ~Backend(void);

  /* If the Config::m_use_texture_for_uniform_buffer is
   * true, returns the width which is also the maximum
   * height for the the texture that will store the data.
   *
   * This value is also gauranteed to be a power of 2.
   */
  static
  uint32_t
  texture_dims_for_uniform_buffer(void);

  /* Gives the log2 of texture_dims_for_uniform_buffer() */
  static
  uint32_t
  log2_texture_dims_for_uniform_buffer(void);

  virtual
  void
  color_write_mask(bvec4 b) override final;

  virtual
  void
  depth_buffer_mode(enum depth_buffer_mode_t b) override final;

  virtual
  void
  set_stencil_state(const StencilState &st) override final;

  virtual
  void
  set_fragment_shader_emit(enum colorspace_t encoding) override final;

  virtual
  reference_counted_ptr<UberShadingKey>
  create_uber_shading_key(void) override final;

protected:
  virtual
  void
  on_draw_render_data(unsigned int z,
                      c_array<const pointer<const ItemShader>> shaders,
                      const RenderValues &st,
                      UberShadingKey::Cookie uber_shader_cookie,
                      RenderValue<ScaleTranslate> tr,
                      ClipWindowValue cl,
                      bool permute_xy,
                      c_array<const std::pair<unsigned int, range_type<int>>> R) override final;

  virtual
  uint32_t
  allocate_transformation(const Transformation &value) override final;

  virtual
  const Transformation&
  fetch_transformation(uint32_t cookie) override final;

  virtual
  uint32_t
  allocate_translate(const ScaleTranslate &value) override final;

  virtual
  const ScaleTranslate&
  fetch_translate(uint32_t cookie) override final;

  virtual
  uint32_t
  allocate_clip_window(const ClipWindow &value) override final;

  virtual
  const ClipWindow&
  fetch_clip_window(uint32_t cookie) override final;

  virtual
  uint32_t
  allocate_render_brush(const Brush &value) override final;

  virtual
  const Brush&
  fetch_render_brush(uint32_t cookie) override final;

  virtual
  uint32_t
  allocate_image_sampler(const ImageSampler &value) override final;

  virtual
  const ImageSampler&
  fetch_image_sampler(uint32_t cookie) override final;

  virtual
  uint32_t
  allocate_gradient(const Gradient &value) override final;

  virtual
  const Gradient&
  fetch_gradient(uint32_t cookie) override final;

  virtual
  uint32_t
  allocate_image_transformation(const GradientTransformation &value) override final;

  virtual
  const GradientTransformation&
  fetch_image_transformation(uint32_t cookie) override final;

  virtual
  uint32_t
  allocate_shadow_map(const ShadowMap &value) override final;

  virtual
  const ShadowMap&
  fetch_shadow_map(uint32_t cookie) override final;

  virtual
  uint32_t
  allocate_framebuffer_pixels(const EmulateFramebufferFetch &value) override final;

  const EmulateFramebufferFetch&
  fetch_framebuffer_pixels(uint32_t cookie) override final;

  virtual
  uint32_t
  allocate_render_clip_element(const RenderClipElement *value) override final;

  virtual
  uint32_t
  allocate_item_data(c_array<const gvec4> value,
                     c_array<const ItemDataValueMapping::entry> item_data_value_map,
                     const ItemDataDependencies &dependencies) override final;

  virtual
  c_array<const gvec4>
  fetch_item_data(uint32_t cookie) override final;

  virtual
  c_array<const ImageID>
  image_id_of_item_data(uint32_t cookie) override final;

  virtual
  c_array<const ShadowMapID>
  shadow_map_id_of_item_data(uint32_t cookie) override final;

  virtual
  void
  on_begin_render_target(const ClearParams &clear_params, RenderTarget &rt) override final;

  virtual
  void
  on_end_render_target(RenderTarget &rt) override final;

  virtual
  void
  on_begin(void) override final;

  virtual
  void
  on_end(c_array<unsigned int> stats) override final;

  virtual
  unsigned int
  render_stats_size_derived(void) const override final
  {
    return number_total_stats;
  }

  virtual
  c_string
  render_stats_label_derived(unsigned int) const override final;

private:
  /* A PackedValue holds the original value that generated
   * along with if it already in a buffer and if so where
   * in a buffer. The cookie returned in each of the
   * allocate_foo() methods is an index into the
   * corresponding m_packed_foo array.
   */
  template<typename T, enum data_t> class PackedValue;

  /* class helper for PackedValue */
  template<typename T> class ValueMirror;

  /* Special class for handling item data */
  class PackedItemData;

  /* class to hold a set of draw calls that source
   * from a common vertex buffer
   */
  class StagingBuffer;

  /* class to hold a pool of GL buffer objects of the same size */
  class BufferPool;

  /* simple class to hold a texture and fbo used to render to it */
  class VertexSurface
  {
  public:
    VertexSurface(void):
      m_texture(0u),
      m_fbo(0u),
      m_dims(0, 0)
    {}

    astral_GLuint m_texture, m_fbo;
    uvec2 m_dims;
  };

  /* class to hold a pool of surfaces to which we render
   * attribute and header-ids. The dimension of each surface
   * is the same and of the form of 2^N x 2^k where N is
   * so that 2^N <= MaxTextureWidth < 2^(N + 1). The format
   * of the surface is ASTRAL_GL_RG_32UI where .x holds the offset
   * into the VertexBacking of teh vertex to read and .y
   * holds the token ID.
   */
  class VertexSurfacePool;

  class BufferRange
  {
  public:
    astral_GLintptr m_offset;
    astral_GLsizeiptr m_size;
  };

  class DataStash:astral::noncopyable
  {
  public:
    DataStash(void):
      m_active(false),
      m_num_elements(0),
      m_size_of_element(0)
    {}

    void
    init(unsigned int num_elements,
         unsigned int datas_per_element)
    {
      m_cpu_buffer.resize(num_elements * datas_per_element);
      m_data = make_c_array(m_cpu_buffer);
      m_num_elements = num_elements;
      m_size_of_element = datas_per_element;
    }

    c_array<generic_data>
    write_location(uint32_t *element)
    {
      return write_location(1, element);
    }

    c_array<generic_data>
    write_location(unsigned int count, uint32_t *element)
    {
      unsigned int start(m_current_element * m_size_of_element);

      *element = m_current_element;
      m_current_element += count;
      ASTRALassert(m_active);
      ASTRALassert(m_current_element <= m_num_elements);

      return m_data.sub_array(start, count * m_size_of_element);
    }

    unsigned int
    freespace(void)
    {
      ASTRALassert(m_active);
      ASTRALassert(m_current_element <= m_num_elements);
      return m_num_elements - m_current_element;
    }

    void
    begin_write(void)
    {
      ASTRALassert(!m_active);
      m_active = true;
      m_current_element = 0;
    }

    c_array<const generic_data>
    end_write(void)
    {
      ASTRALassert(m_active);
      m_active = false;
      return m_data.sub_array(0, m_current_element * m_size_of_element);
    }

    unsigned int //returns the size in bytes needed if the buffer is full.
    size_bytes(void) const
    {
      return sizeof(generic_data) * m_cpu_buffer.size();
    }

  private:
    bool m_active;
    c_array<generic_data> m_data;
    unsigned int m_current_element, m_num_elements, m_size_of_element;
    std::vector<generic_data> m_cpu_buffer;
  };

  template<typename T, enum data_t tp>
  static
  PackedValue<T, tp>&
  get(RenderValue<T> v, std::vector<PackedValue<T, tp>> &values);

  template<typename T>
  void
  data_freespace_requirement(RenderValue<T>,
                             vecN<unsigned int, number_data_types> &out_incr);

  void
  data_freespace_requirement(ItemData,
                             vecN<unsigned int, number_data_types> &out_incr);

  template<typename T>
  uint32_t
  pack_data(RenderValue<T>);

  uint32_t
  pack_data(ItemData);

  uint32_t
  pack_data(const Packing::Header &header);

  PackedValue<Transformation, data_item_transformation>&
  get(RenderValue<Transformation> v)
  {
    return get(v, m_packed_transformation);
  }

  PackedValue<ScaleTranslate, data_item_scale_translate>&
  get(RenderValue<ScaleTranslate> v)
  {
    return get(v, m_packed_translate);
  }

  PackedValue<ClipWindow, data_clip_window>&
  get(RenderValue<ClipWindow> v)
  {
    return get(v, m_packed_clip_window);
  }

  PackedValue<Brush, data_brush>&
  get(RenderValue<Brush> v)
  {
    return get(v, m_packed_render_brush);
  }

  PackedValue<ImageSampler, data_image>&
  get(RenderValue<ImageSampler> v)
  {
    return get(v, m_packed_image_sampler);
  }

  PackedValue<Gradient, data_gradient>&
  get(RenderValue<Gradient> v)
  {
    return get(v, m_packed_gradient);
  }

  PackedValue<GradientTransformation, data_gradient_transformation>&
  get(RenderValue<GradientTransformation> v)
  {
    return get(v, m_packed_gradient_transformation);
  }

  PackedValue<const ShadowMap&, data_shadow_map>&
  get(RenderValue<const ShadowMap&> v)
  {
    return get(v, m_packed_shadow_maps);
  }

  PackedValue<EmulateFramebufferFetch, data_item_transformation>&
  get(RenderValue<EmulateFramebufferFetch> v)
  {
    return get(v, m_packed_framebuffer_pixels);
  }

  PackedValue<const RenderClipElement*, data_clip_mask>&
  get(RenderValue<const RenderClipElement*> v)
  {
    return get(v, m_packed_clip_masks);
  }

  PackedItemData&
  get(ItemData v);

  static
  enum data_t
  data_t_value(enum ItemDataValueMapping::type_t v);

  void
  data_freespace_requirement(enum data_t tp, uint32_t cookie,
                             vecN<unsigned int, number_data_types> &out_incr);

  bool
  requires_new_item_stash(c_array<const pointer<const ItemShader>> shaders,
                          const RenderValues &st,
                          RenderValue<ScaleTranslate> tr,
                          RenderValue<ClipWindow> cl);

  /*
   * \param dst_ubo UBO that holds all the data stashed into each DataStash
   * \param dst_ranges for each data_t enumeration, the range to feed
   *                   glBindBufferRange
   * \param begin_stashes if true, call for each DataStash, begin_write().
   */
  void
  end_item_stashes_ubo(astral_GLuint *dst_ubo,
                       vecN<BufferRange, number_data_types> *dst_ranges,
                       bool begin_stashes);

  /*
   * \param dst_ubo texture that holds all the data stashed into each DataStash
   * \param dst_offset for each data_t enumeration, the linear offset into
   *                   the texture where the data is located
   * \param begin_stashes if true, call for each DataStash, begin_write().
   */
  void
  end_item_stashes_texture(astral_GLuint *dst_texture,
                           vecN<generic_data, number_data_types> *dst_offset,
                           bool begin_stashes);

  void
  new_staging_buffer(void);

  bool
  requires_emit_gl_blend_state(BackendBlendMode modeA, BackendBlendMode modeB);

  void
  emit_gl_blend_state(BackendBlendMode mode);

  uint32_t
  pack_data(enum data_t tp, uint32_t cookie);

  VertexSurface //returns the surface
  allocate_vertex_surface(unsigned int number_vertices);

  void
  reset_pools(void);

  void
  ready_vertex_id_vao(unsigned int sz);

  /* configuration */
  ExtraConfig m_config;
  reference_counted_ptr<Implement> m_engine;
  unsigned int m_number_gl_clip_planes;

  /* how and if to clear the current render target */
  ClearParams m_clear_current_rt_params;

  /* data that backs RenderValue<T> */
  std::vector<PackedValue<Transformation, data_item_transformation>> m_packed_transformation;
  std::vector<PackedValue<ScaleTranslate, data_item_scale_translate>> m_packed_translate;
  std::vector<PackedValue<ClipWindow, data_clip_window>> m_packed_clip_window;
  std::vector<PackedValue<Brush, data_brush>> m_packed_render_brush;
  std::vector<PackedValue<Gradient, data_gradient>> m_packed_gradient;
  std::vector<PackedValue<GradientTransformation, data_gradient_transformation>> m_packed_gradient_transformation;
  std::vector<PackedValue<ImageSampler, data_image>> m_packed_image_sampler;
  std::vector<PackedValue<const ShadowMap&, data_shadow_map>> m_packed_shadow_maps;
  std::vector<PackedValue<EmulateFramebufferFetch, data_item_transformation>> m_packed_framebuffer_pixels;
  std::vector<PackedValue<const RenderClipElement*, data_clip_mask>> m_packed_clip_masks;

  /* ItemData is handled differently because of its variable size. */
  std::vector<PackedItemData> m_packed_item_data;

  /* Holds the backing store for the values of render item data
   * that are sent to GL
   */
  std::vector<gvec4> m_item_data_backing;

  /* Holds the backing for the arrays returned by
   * \ref image_id_of_item_data()
   */
  std::vector<ImageID> m_item_data_image_id_backing;

  /* Holds the backing for the arrays returned by
   * \ref shadow_map_id_of_item_data()
   */
  std::vector<ShadowMapID> m_item_data_shadow_map_id_backing;

  /* Holds the backing store for the ItemDataValueMapping
   * of render item data
   */
  std::vector<ItemDataValueMapping::entry> m_item_data_interpretation_backing;

  /* current state */
  unsigned int m_current_item_stash;
  vecN<unsigned int, number_total_stats> m_stats;

  /* Buffers used to feed the shaders */
  vecN<DataStash, number_data_types> m_data_stashes;
  reference_counted_ptr<BufferPool> m_misc_buffer_pool;
  reference_counted_ptr<BufferPool> m_ubo_item_data_buffer_pool;
  reference_counted_ptr<BufferPool> m_data_texture_offset_buffer_pool;
  c_array<generic_data> m_ubo_item_data_data_ptr;
  unsigned int m_ubo_item_data_location;
  unsigned int m_ubo_item_data_last_size_needed;

  /* All surfaces from m_vertex_surface_pools[k] are size 2^N x 2^k
   * where N = m_config.m_log2_gpu_stream_surface_width
   * and m_log2_max_texture_mask is (2^N - 1).
   */
  unsigned int m_vertex_surface_width, m_max_vertices_per_surface;
  std::vector<reference_counted_ptr<VertexSurfacePool>> m_vertex_surface_pools;

  /* Staging buffer, each staging buffer represents using a
   * single VertexSurface
   */
  reference_counted_ptr<StagingBuffer> m_current_staging_buffer;
  std::vector<reference_counted_ptr<StagingBuffer>> m_active_staging_buffers;
  std::vector<reference_counted_ptr<StagingBuffer>> m_staging_buffer_pool;

  /* if attribute-less rendering is bugged on a driver or WebGL2
   * implementation, we have a GL buffer that is just integers
   * increasing in order to emulate gl_VertexID.
   */
  astral_GLuint m_vertex_id_vao;
  astral_GLuint m_vertex_id_buffer, m_index_buffer;
  unsigned int m_vertex_id_buffer_size;

  /* if we are rendering to the RenderTarget of the shadowmap
   * backing, then the shader cannot source from the shadowmap
   * backing (and the Renderer makes sure it does not). However,
   * to make the GL API happy, we must make sure that the shader
   * execution does not even reference it, i.e. we do NOT bind
   * the shadowmap backing in this case
   */
  bool m_current_rt_is_shadowmap_backing;

  /* the number of times on_end() had been called since reset_pools() has been called */
  unsigned int m_on_end_called_count_since_reset_pools;

  /* current color encoding that fragment shader is to emit */
  enum colorspace_t m_fragment_shader_emit_encoding;
};


#endif

/*!
 * \file render_encoder.cpp
 * \brief file render_encoder.cpp
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

#include <numeric>
#include <functional>
#include <astral/util/ostream_utility.hpp>
#include <astral/renderer/vertex_streamer.hpp>
#include <astral/renderer/static_data_streamer.hpp>
#include <astral/renderer/renderer.hpp>
#include <astral/renderer/effect/effect.hpp>

#include "renderer_shared_util.hpp"
#include "renderer_draw_command.hpp"
#include "renderer_cached_transformation.hpp"
#include "renderer_streamer.hpp"
#include "renderer_virtual_buffer.hpp"
#include "renderer_storage.hpp"
#include "renderer_workroom.hpp"
#include "renderer_filler.hpp"
#include "renderer_mask_drawer.hpp"
#include "render_encoder_shadowmap_util.hpp"
#include "render_clip_node.hpp"

class astral::RenderEncoderBase::Details:public astral::RenderEncoderBase
{
public:
  Details(RenderEncoderBase b):
    RenderEncoderBase(b)
  {}

  template<typename T>
  void
  direct_stroke_pathT(bool skip_joins_caps,
                      const DirectStrokeShader &shader,
                      const CombinedPath &P,
                      const StrokeParameters &stroke_params,
                      const StrokeShader::ItemDataPackerBase &packer,
                      const ItemMaterial &material,
                      enum blend_mode_t blend_mode) const;

  reference_counted_ptr<Image>
  snapshot_logical_implement(RenderEncoderBase src_encoder,
                             const RelativeBoundingBox &logical_bb,
                             RenderScaleFactor scale_rendering,
                             Transformation *out_image_transformation_logical,
                             unsigned int pixel_slack,
                             unsigned int lod_requirement) const;

  void
  draw_mask_implement(const SubImageT<float> &mask,
                      const Transformation &mask_transformation_logical,
                      enum filter_t filter,
                      enum mask_post_sampling_mode_t post_sampling_mode,
                      enum mask_type_t mask_type,
                      enum mask_channel_t mask_channel,
                      const ItemMaterial &material,
                      enum blend_mode_t blend_mode) const;

  RenderValue<EmulateFramebufferFetch>
  framebuffer_fetch_surface_logical(const BoundingBox<float> &logical_rect) const;

  RenderValue<EmulateFramebufferFetch>
  draw_custom_common(const RectRegion &region,
                     bool shader_emits_partially_covered_fragments,
                     const ItemMaterial &material,
                     enum blend_mode_t blend_mode) const;

  void
  draw_image_helper(const Rect &image_rect,
                    const ImageMipElement &mip,
                    const ItemMaterial &material,
                    enum ImageMipElement::element_type_t e,
                    enum blend_mode_t blend_mode,
                    bool with_aa) const;

private:
  class ShaderIndices
  {
  public:
    ShaderIndices(bool skip_joins_caps, const StrokeParameters &params, const DirectStrokeShader::ItemShaderSet &shaders):
      m_shaders(nullptr),
      m_next(0),
      m_line_segment_shader(params.m_draw_edges, m_next, shaders.m_line_segment_shader, m_shaders),
      m_biarc_curve_shader(params.m_draw_edges, m_next, shaders.m_biarc_curve_shader, m_shaders),
      m_inner_glue_shader(params.m_draw_edges && !skip_joins_caps, m_next, shaders.m_inner_glue_shader, m_shaders),
      m_outer_glue_shader(params.m_draw_edges && !skip_joins_caps, params.m_glue_join, m_next, shaders.m_join_shaders, m_shaders),
      m_outer_glue_cusp_shader(params.m_draw_edges && !skip_joins_caps, params.m_glue_cusp_join, m_next, shaders.m_join_shaders, m_shaders),
      m_join_shader(!skip_joins_caps, params.m_join, m_next, shaders.m_join_shaders, m_shaders),
      m_line_capper_shaders(params.m_draw_edges && !skip_joins_caps, m_next, shaders.m_line_capper_shaders, m_shaders),
      m_quadratic_capper_shaders(params.m_draw_edges && !skip_joins_caps, m_next, shaders.m_quadratic_capper_shaders, m_shaders),
      m_cap_shader(!skip_joins_caps, m_next, shaders.m_cap_shader, m_shaders)
    {
    }

    c_array<const pointer<const ColorItemShader>>
    shaders(void) const
    {
      c_array<const pointer<const ColorItemShader>> R(m_shaders);

      R = R.sub_array(0, m_next);
      return R;
    }

    int
    line_segment_shader(void) const
    {
      return m_line_segment_shader.m_value;
    }

    int
    biarc_curve_shader(void) const
    {
      return m_biarc_curve_shader.m_value;
    }

    int
    join_shader(void) const
    {
      return m_join_shader.m_value;
    }

    int
    inner_glue_shader(void) const
    {
      return m_inner_glue_shader.m_value;
    }

    int
    outer_glue_shader(void) const
    {
      return m_outer_glue_shader.m_value;
    }

    int
    outer_glue_cusp_shader(void) const
    {
      return m_outer_glue_cusp_shader.m_value;
    }

    int
    cap_shader(void) const
    {
      return m_cap_shader.m_value;
    }

    int
    line_capper_shaders(unsigned int v) const
    {
      return m_line_capper_shaders.m_values[v];
    }

    int
    quadratic_capper_shaders(unsigned int v) const
    {
      return m_quadratic_capper_shaders.m_values[v];
    }

  private:
    class Idx
    {
    public:
      Idx(bool drawit, int &next, const reference_counted_ptr<const ColorItemShader> &shader, c_array<const ColorItemShader*> dst)
      {
        if (drawit && shader)
          {
            dst[next] = shader.get();
            m_value = next;
            ++next;
          }
        else
          {
            m_value = -1;
          }
      }

      Idx(bool drawit, enum join_t join, int &next,
          const vecN<reference_counted_ptr<const ColorItemShader>, number_join_t> &join_shaders,
          c_array<const ColorItemShader*> dst)
      {
        if (drawit && join < number_join_t && join_shaders[join])
          {
            dst[next] = join_shaders[join].get();
            m_value = next;
            ++next;
          }
        else
          {
            m_value = -1;
          }
      }

      /* Meanings:
       *  -1 shader not present
       *  >=0 index into ColorItem::m_shaders
       */
      int m_value;
    };

    template<size_t N>
    class VecNIdx
    {
    public:
      VecNIdx(bool drawit, int &next,
              const vecN<reference_counted_ptr<const ColorItemShader>, N> &shaders,
              c_array<const ColorItemShader*> dst)
      {
        for (unsigned int i = 0; i < N; ++i)
          {
            if (drawit && shaders[i])
              {
                dst[next] = shaders[i].get();
                m_values[i] = next;
                ++next;
              }
            else
              {
                m_values[i] = -1;
              }
          }
      }

      vecN<int, N> m_values;
    };

    vecN<const ColorItemShader*, 7 + 2 * StrokeShader::number_capper_shader> m_shaders;
    int m_next;

    Idx m_line_segment_shader;
    Idx m_biarc_curve_shader;
    Idx m_inner_glue_shader;
    Idx m_outer_glue_shader;
    Idx m_outer_glue_cusp_shader;
    Idx m_join_shader;
    VecNIdx<StrokeShader::number_capper_shader> m_line_capper_shaders;
    VecNIdx<StrokeShader::number_capper_shader> m_quadratic_capper_shaders;
    Idx m_cap_shader;
  };

  template<typename T>
  void
  direct_stroke_path_implement(const ShaderIndices &shader,
                               const T &path, float t,
                               const vec2 *translate, const float2x2 *matrix,
                               const StrokeParameters &stroke_params,
                               const StrokeShader::ItemDataPackerBase &packer,
                               const ItemMaterial &material,
                               enum blend_mode_t blend_mode) const;

  void
  handle_cooked_data(const ShaderIndices &shader,
                     const StrokeShader::SimpleCookedData &cooked_data,
                     std::vector<const VertexData*> *dst_vertex_datas,
                     std::vector<ColorItem::SubItem> *dst_sub_items) const;

  void
  handle_cooked_data_helper(int shader, //value from ShaderIndices
                            const StrokeShader::SimpleCookedData &cooked_data,
                            enum StrokeShader::primitive_type_t P,
                            std::vector<const VertexData*> *dst_vertex_datas,
                            std::vector<ColorItem::SubItem> *dst_sub_items) const;
};

/////////////////////////////////////////
// astral::RenderSupportTypes::Proxy methods
astral::BoundingBox<float>
astral::RenderSupportTypes::Proxy::
pixel_bounding_box(void) const
{
  BoundingBox<float> return_value;

  if (m_data)
    {
      return_value = m_data->m_clip_geometry.bounding_geometry().pixel_rect();
    }
  return return_value;
}

astral::ScaleTranslate
astral::RenderSupportTypes::Proxy::
image_transformation_pixel(void) const
{
  ScaleTranslate return_value;

  if (m_data)
    {
      return_value = m_data->m_clip_geometry.bounding_geometry().image_transformation_pixel();
    }
  return return_value;
}

astral::ivec2
astral::RenderSupportTypes::Proxy::
image_size(void) const
{
  ivec2 return_value(0, 0);

  if (m_data)
    {
      return_value = m_data->m_clip_geometry.bounding_geometry().image_size();
    }
  return return_value;
}

////////////////////////////////////////////////
// astral::RenderSupportTypes::ColorItem methods
bool
astral::RenderSupportTypes::ColorItem::
emits_partially_covered_fragments(void) const
{
  for (unsigned int i = 0; i < m_shaders.size(); ++i)
    {
      ASTRALassert(m_shaders[i]);
      if (m_shaders[i]->properties().m_emits_partially_covered_fragments)
        {
          return true;
        }
    }
  return true;
}

bool
astral::RenderSupportTypes::ColorItem::
emits_transparent_fragments(void) const
{
  for (unsigned int i = 0; i < m_shaders.size(); ++i)
    {
      ASTRALassert(m_shaders[i]);
      if (m_shaders[i]->properties().m_emits_transparent_fragments)
        {
          return true;
        }
    }
  return true;
}

/////////////////////////////////////////
// astral::RenderEncoderBase methods
bool
astral::RenderEncoderBase::
valid(void) const
{
  return m_virtual_buffer
    && m_virtual_buffer->m_renderer.m_begin_cnt == m_virtual_buffer->m_renderer_begin_cnt;
}

astral::Renderer::Implement::VirtualBuffer&
astral::RenderEncoderBase::
virtual_buffer(void) const
{
  ASTRALassert(valid());
  return *m_virtual_buffer;
}

bool
astral::RenderEncoderBase::
degenerate(void) const
{
  return virtual_buffer().type() == VirtualBuffer::degenerate_buffer;
}

astral::Renderer&
astral::RenderEncoderBase::
renderer(void) const
{
  return virtual_buffer().m_renderer;
}

astral::Renderer::Implement&
astral::RenderEncoderBase::
renderer_implement(void) const
{
  return virtual_buffer().m_renderer;
}

astral::RenderEngine&
astral::RenderEncoderBase::
render_engine(void) const
{
  return *renderer_implement().m_engine;
}

float
astral::RenderEncoderBase::
render_scale_factor(void) const
{
  return virtual_buffer().scale_factor();
}

enum astral::colorspace_t
astral::RenderEncoderBase::
colorspace(void) const
{
  return virtual_buffer().colorspace();
}

bool
astral::RenderEncoderBase::
rendering_to_image(void) const
{
  return virtual_buffer().type() == Renderer::VirtualBuffer::image_buffer;
}

bool
astral::RenderEncoderBase::
finished(void) const
{
  return virtual_buffer().finish_issued();
}

float
astral::RenderEncoderBase::
compute_tolerance(void) const
{
  return virtual_buffer().logical_rendering_accuracy();
}

float
astral::RenderEncoderBase::
compute_tolerance(const float2x2 *matrix) const
{
  return virtual_buffer().compute_tol(matrix);
}

const astral::Transformation&
astral::RenderEncoderBase::
transformation(void) const
{
  return virtual_buffer().m_transformation_stack.back().transformation();
}

astral::RenderValue<astral::Transformation>
astral::RenderEncoderBase::
transformation_value(void) const
{
  return virtual_buffer().render_value_transformation();
}

astral::vec2
astral::RenderEncoderBase::
singular_values(void) const
{
  return virtual_buffer().m_transformation_stack.back().singular_values();
}

float
astral::RenderEncoderBase::
surface_pixel_size_in_logical_coordinates(void) const
{
  return virtual_buffer().m_transformation_stack.back().surface_pixel_size_in_logical_coordinates(render_scale_factor());
}

const astral::Transformation&
astral::RenderEncoderBase::
inverse_transformation(void) const
{
  return virtual_buffer().m_transformation_stack.back().inverse();
}

void
astral::RenderEncoderBase::
transformation(const Transformation &v) const
{
  virtual_buffer().m_transformation_stack.back().transformation(v);
}

void
astral::RenderEncoderBase::
transformation(RenderValue<Transformation> v) const
{
  virtual_buffer().m_transformation_stack.back().transformation(v);
}

void
astral::RenderEncoderBase::
transformation_translate(float x, float y) const
{
  virtual_buffer().m_transformation_stack.back().transformation_translate(x, y);
}

void
astral::RenderEncoderBase::
transformation_matrix(const float2x2 &v) const
{
  virtual_buffer().m_transformation_stack.back().transformation_matrix(v);
}

void
astral::RenderEncoderBase::
concat(const Transformation &v) const
{
  virtual_buffer().m_transformation_stack.back().concat(v);
}

void
astral::RenderEncoderBase::
concat(const float2x2 &v) const
{
  virtual_buffer().m_transformation_stack.back().concat(v);
}

void
astral::RenderEncoderBase::
translate(float x, float y) const
{
  virtual_buffer().m_transformation_stack.back().translate(x, y);
}

void
astral::RenderEncoderBase::
scale(float sx, float sy) const
{
  virtual_buffer().m_transformation_stack.back().scale(sx, sy);
}

void
astral::RenderEncoderBase::
rotate(float radians) const
{
  virtual_buffer().m_transformation_stack.back().rotate(radians);
}

void
astral::RenderEncoderBase::
save_transformation(void) const
{
  std::vector<Renderer::Implement::CachedTransformation> &st(virtual_buffer().m_transformation_stack);
  st.push_back(st.back());
}

unsigned int
astral::RenderEncoderBase::
save_transformation_count(void) const
{
  std::vector<Renderer::Implement::CachedTransformation> &st(virtual_buffer().m_transformation_stack);
  ASTRALassert(st.size() >= 1u);

  return st.size() - 1u;
}

void
astral::RenderEncoderBase::
restore_transformation(void) const
{
  std::vector<Renderer::Implement::CachedTransformation> &st(virtual_buffer().m_transformation_stack);
  ASTRALassert(st.size() >= 2u);

  st.pop_back();
}

void
astral::RenderEncoderBase::
restore_transformation(unsigned int cnt) const
{
  std::vector<Renderer::Implement::CachedTransformation> &st(virtual_buffer().m_transformation_stack);
  cnt = t_min(cnt, save_transformation_count());
  st.resize(cnt + 1u);
}

const astral::ShaderSet&
astral::RenderEncoderBase::
default_shaders(void) const
{
  return renderer_implement().m_default_shaders;
}

const astral::EffectSet&
astral::RenderEncoderBase::
default_effects(void) const
{
  return renderer_implement().m_default_effects;
}

void
astral::RenderEncoderBase::
render_accuracy(float v) const
{
  const float min_accuracy(0.01f);
  virtual_buffer().m_render_accuracy = t_max(v, min_accuracy);
}

float
astral::RenderEncoderBase::
render_accuracy(void) const
{
  return virtual_buffer().m_render_accuracy;
}

bool
astral::RenderEncoderBase::
use_sub_ubers(void) const
{
  return virtual_buffer().m_use_sub_ubers;
}

void
astral::RenderEncoderBase::
use_sub_ubers(bool v) const
{
  virtual_buffer().m_use_sub_ubers = v;
}

void
astral::RenderEncoderBase::
draw_generic_private(RenderValue<Transformation> transformation,
                     const Item<MaskItemShader> &item,
                     const ItemMask &clip,
                     enum mask_item_shader_clip_mode_t clip_mode) const
{
  ASTRALassert(!finished());
  virtual_buffer().draw_generic(transformation, item, clip, clip_mode);
}

void
astral::RenderEncoderBase::
draw_generic_private(RenderValue<Transformation> transformation, const Item<ShadowMapItemShader> &item) const
{
  ASTRALassert(!finished());
  virtual_buffer().draw_generic(transformation, item);
}

void
astral::RenderEncoderBase::
draw_rect(const ColorItemShader &shader, const Rect &rect,
          const ItemMaterial &material,
          enum blend_mode_t blend_mode) const
{
  vecN<gvec4, DynamicRectShader::item_data_size> rect_data;

  ASTRALassert(valid());

  DynamicRectShader::pack_item_data(rect, rect_data);
  RectItem item(shader, create_item_data(rect_data, no_item_data_value_mapping));
  draw_custom_rect(rect, item, material, blend_mode);
}

void
astral::RenderEncoderBase::
draw_rect(const Rect &rect, bool with_aa,
          const ItemMaterial &material,
          enum blend_mode_t blend_mode) const
{
  const ColorItemShader *shader;

  /* no point doing anti-aliasing if rectangle
   * is screen aligned.
   */
  with_aa = with_aa
    && virtual_buffer().m_transformation_stack.back().matrix_type() == matrix_generic;

  shader = (with_aa) ?
    renderer_implement().m_default_shaders.m_dynamic_rect_aa_shader.get() :
    renderer_implement().m_default_shaders.m_dynamic_rect_shader.get();

  draw_rect(*shader, rect, material, blend_mode);
}

void
astral::RenderEncoderBase::
draw_custom_rect(const RectRegion &region, const RectItem &rect_item,
                 const ItemMaterial &material, enum blend_mode_t blend_mode) const
{
  Item<ColorItemShader> item(rect_item.m_shader, rect_item.m_item_data,
                             *renderer_implement().m_dynamic_rect);
  draw_custom(region, item, material, blend_mode);
}

astral::RenderValue<astral::EmulateFramebufferFetch>
astral::RenderEncoderBase::Details::
framebuffer_fetch_surface_logical(const BoundingBox<float> &rect) const
{
  reference_counted_ptr<Image> im;
  Transformation image_transformation_logical;
  RenderValue<EmulateFramebufferFetch> framebuffer_copy;
  unsigned int pixel_padding(2u);
  unsigned int lod_requirement(0u);
  RelativeBoundingBox bb(rect);

  /* we need a surface to hold the region */
  ASTRALassert(!finished());

  im = snapshot_logical_implement(*this, bb,
                                  RenderScaleFactor(), &image_transformation_logical,
                                  pixel_padding, lod_requirement);

  if (im)
    {
      EmulateFramebufferFetch fbp;
      ImageSampler im_sampler(*im, filter_nearest, mipmap_none);

      im_sampler
        .x_tile_mode(tile_mode_clamp)
        .y_tile_mode(tile_mode_clamp);

      fbp.m_image_transformation_pixels = image_transformation_logical * this->transformation().inverse();
      fbp.m_image = create_value(im_sampler);
      framebuffer_copy = renderer_implement().m_backend->create_value(fbp);

      ++renderer_implement().m_stats[Renderer::Implement::number_emulate_framebuffer_fetches];
    }

  return framebuffer_copy;
}

void
astral::RenderEncoderBase::
begin_pause_snapshot(void) const
{
  ASTRALassert(!finished());
  virtual_buffer().begin_pause_snapshot();
}

void
astral::RenderEncoderBase::
end_pause_snapshot(void) const
{
  ASTRALassert(!finished());
  virtual_buffer().end_pause_snapshot();
}

int
astral::RenderEncoderBase::
pause_snapshot_depth(void) const
{
  ASTRALassert(!finished());
  return virtual_buffer().pause_snapshot_counter();
}

void
astral::RenderEncoderBase::
pause_snapshot_depth(int v) const
{
  ASTRALassert(!finished());
  return virtual_buffer().pause_snapshot_counter(v);
}

void
astral::RenderEncoderBase::
add_dependency(const Image &image) const
{
  virtual_buffer().add_dependency(image);
}

astral::RenderValue<astral::EmulateFramebufferFetch>
astral::RenderEncoderBase::Details::
draw_custom_common(const RectRegion &region,
                   bool shader_emits_partially_covered_fragments,
                   const ItemMaterial &material,
                   enum blend_mode_t blend_mode) const
{
  RenderValue<EmulateFramebufferFetch> framebuffer_copy;
  bool material_reduces_coverage(material.emits_partial_coverage());
  bool partial_coverage(shader_emits_partially_covered_fragments || material_reduces_coverage);
  BackendBlendMode bb(blend_mode, partial_coverage);
  const BlendModeInformation &info(renderer_implement().m_engine->properties().m_blend_mode_information);
  enum BlendModeInformation::requires_framebuffer_pixels_t pp(info.requires_framebuffer_pixels(bb));

  ASTRALassert(!finished());
  if (pp == BlendModeInformation::requires_framebuffer_pixels_opaque_draw)
    {
      /* if the blend mode declares that the draw is opaque, its draw will
       * obscure pixels, so there is no point to have the surface blitted
       * since the draw will obscure pixels anyways. To prevent the surface
       * blit, we increment the pause snapshot counter.
       */
      begin_pause_snapshot();
    }

  /* IDEA: remove the enumeration requires_framebuffer_pixels_blend_draw
   *       from BlendModeInformation since it is not used anywhere and
   *       in addition, if the material needs the snapshot pixels we
   *       pass down to the backend the information that here are the
   *       snapshot pixels and it can do all blending in shader with
   *       those pixels and draw the element as opaque infront of all
   *       other previous pixels to obscure them allowing us to kick
   *       in early-z to prevent pixels getting drawn twice.
   */

  if (pp != BlendModeInformation::does_not_need_framebuffer_pixels
      || material.uses_framebuffer_pixels())
    {
      framebuffer_copy = framebuffer_fetch_surface_logical(region.m_rect);
    }

  if (pp == BlendModeInformation::requires_framebuffer_pixels_opaque_draw)
    {
      end_pause_snapshot();
    }

  return framebuffer_copy;
}

void
astral::RenderEncoderBase::
draw_custom(const RectRegion &region,
            const Item<ColorItemShader> &item,
            const ItemMaterial &material,
            enum blend_mode_t blend_mode) const
{
  RenderValue<EmulateFramebufferFetch> framebuffer_copy;

  framebuffer_copy = Details(*this).draw_custom_common(region,
                                                       item.m_shader.properties().m_emits_partially_covered_fragments,
                                                       material, blend_mode);

  virtual_buffer().draw_generic(transformation_value(), &region, item, material, blend_mode, framebuffer_copy);
}

void
astral::RenderEncoderBase::
draw_custom(const RectRegion &region,
            const ColorItem &item,
            const ItemMaterial &material,
            enum blend_mode_t blend_mode) const
{
  RenderValue<EmulateFramebufferFetch> framebuffer_copy;

  framebuffer_copy = Details(*this).draw_custom_common(region, item.emits_partially_covered_fragments(),
                                                       material, blend_mode);

  virtual_buffer().draw_generic(transformation_value(), &region, item, material, blend_mode, framebuffer_copy);
}

void
astral::RenderEncoderBase::
draw_mask(const MaskDetails &mask,
          const Transformation &mask_transformation_logical,
          enum filter_t filter,
          const ItemMaterial &material,
          enum blend_mode_t blend_mode) const
{
  if (!mask.m_mask)
    {
      return;
    }

  SubImageT<float> im(*mask.m_mask, mask.m_min_corner, mask.m_size);
  draw_mask(im, mask_transformation_logical, filter,
            mask_post_sampling_mode_direct,
            mask.m_mask_type,
            mask.m_mask_channel,
            material, blend_mode);
}

void
astral::RenderEncoderBase::
draw_mask(const ImageSampler &mask,
          const Transformation &mask_transformation_logical,
          const ItemMaterial &material,
          enum blend_mode_t blend_mode) const
{
  const Image *im;

  im = render_engine().image_atlas().fetch_image(mask.m_image_id);
  if (!im)
    {
      /* TODO: if the ImageSampler indicates to invert the sampling,
       *       should we draw a rect for the material?
       */
      return;
    }

  SubImage sub_im(*im, mask.m_min_corner, mask.m_size);
  draw_mask(sub_im, mask_transformation_logical,
            mask.filter(), mask.mask_post_sampling_mode(),
            mask.mask_type(), mask.mask_channel(),
            material, blend_mode);
}

void
astral::RenderEncoderBase::
draw_mask(const SubImageT<float> &mask,
          const Transformation &mask_transformation_logical,
          enum filter_t filter,
          enum mask_post_sampling_mode_t post_sampling_mode,
          enum mask_type_t mask_type,
          enum mask_channel_t mask_channel,
          const ItemMaterial &material,
          enum blend_mode_t blend_mode) const
{
  Renderer::Implement::MaskDrawerImage drawer;

  drawer.draw_mask(*this, mask, mask_transformation_logical,
                   filter, post_sampling_mode,
                   mask_type, mask_channel,
                   material, blend_mode);
}

void
astral::RenderEncoderBase::Details::
draw_image_helper(const Rect &image_rect,
                  const ImageMipElement &mip,
                  const ItemMaterial &material,
                  enum ImageMipElement::element_type_t e,
                  enum blend_mode_t blend_mode,
                  bool with_aa) const
{
  const ShaderSet &shaders(default_shaders());
  for (unsigned int tile = 0, end_tile = mip.number_elements(e); tile < end_tile; ++tile)
    {
      Rect tile_rect, intersect_rect;
      EnumFlags<enum RectEnums::side_t, 4> bd_flags;

      tile_rect.m_min_point = vec2(mip.element_location(e, tile));
      tile_rect.m_max_point = tile_rect.m_min_point + vec2(mip.element_size(e, tile));
      if (Rect::compute_intersection(tile_rect, image_rect, &intersect_rect))
        {
          if (with_aa)
            {
              bd_flags
                .value(RectEnums::minx_side, tile_rect.m_min_point.x() <= image_rect.m_min_point.x())
                .value(RectEnums::maxx_side, tile_rect.m_max_point.x() >= image_rect.m_max_point.x())
                .value(RectEnums::miny_side, tile_rect.m_min_point.y() <= image_rect.m_min_point.y())
                .value(RectEnums::maxy_side, tile_rect.m_max_point.y() >= image_rect.m_max_point.y());
            }
          draw_rect(*shaders.dynamic_rect_shader(bd_flags), tile_rect, material, blend_mode);
        }
    }
}

void
astral::RenderEncoderBase::
draw_image(const SubImage &in_image, MipmapLevel in_mipmap_level,
           const ImageDraw &draw, enum blend_mode_t blend_mode)
{
  /* bound the value to the maximim level allowed */
  in_mipmap_level.m_value = t_min(in_mipmap_level.m_value, in_image.number_mipmap_levels() - 1u);

  /* Compute what portion of the mip-tail from in_image to take. Recall that
   * each ImageMipElement holds ImageMipElement::maximum_number_of_mipmaps
   * mipmap levels.
   */
  unsigned int mip_tail;

  ASTRALassert(ImageMipElement::maximum_number_of_mipmaps == 2u);
  mip_tail = in_mipmap_level.m_value >> 1u;

  SubImage image(in_image.mip_tail(mip_tail));
  MipmapLevel mipmap_level(in_mipmap_level.m_value & 1u);
  ImageSampler sampler(image, mipmap_level, draw.m_filter,
                       draw.m_post_sampling_mode,
                       tile_mode_clamp, tile_mode_clamp);
  Brush br;
  RenderValue<Brush> im;
  const ImageMipElement &mip(*image.mip_chain().front());
  AutoRestore restore(*this);

  br.m_gradient = draw.m_gradient;
  br.m_gradient_transformation = draw.m_gradient_transformation;
  br.m_base_color = draw.m_base_color;
  br.m_colorspace = draw.m_colorspace;
  br.m_image = create_value(sampler);

  /* we will draw with image directly which may be a different size than
   * the input image, thus we may need to scale up.
   */
  if (image.m_mip_range.m_begin != in_image.m_mip_range.m_begin)
    {
      vec2 scaling_factor;

      scaling_factor = vec2(in_image.m_size) / vec2(image.m_size);
      scale(scaling_factor);
    }

  im = create_value(br);
  if (mip.has_white_or_empty_elements())
    {
      /* So this is a little nightmare of coordinate transformations.
       * We want to work in the coordinates of the ImageMipElement mip.
       * The material coordinates are the same as logical coordinate
       * which is coordinates same as the coordinates of the argument
       * image. The releation between logical (L) coordinates and mip
       * coordinates (M) is
       *
       *   L = M - image.m_min_corner
       *
       * We want to work in (M) coordinates which means all input
       * values need to be subtracted by image.m_min_corner
       */
      translate(-vec2(image.m_min_corner));

      /* We then need to insert a material transformation
       * that also does the above.
       */
      ItemMaterial mapped_image(im, draw.m_clip);
      mapped_image.m_material_transformation_logical = create_value(Transformation(-vec2(image.m_min_corner)));

      Brush br_white;
      br_white.m_colorspace = br.m_colorspace;
      br_white.m_base_color = br.m_base_color;
      ItemMaterial white_material(create_value(br_white), draw.m_clip);

      /* Now figure out what portion of the image.m_image is used */
      Rect image_rect;
      image_rect.m_min_point = vec2(image.m_min_corner);
      image_rect.m_max_point = vec2(image.m_min_corner + image.m_size);

      begin_pause_snapshot();

      Details details(*this);

      /* draw the color tiles */
      details.draw_image_helper(image_rect, mip, mapped_image, ImageMipElement::color_element, blend_mode, draw.m_with_aa);

      /* draw the white tiles */
      details.draw_image_helper(image_rect, mip, white_material, ImageMipElement::white_element, blend_mode, draw.m_with_aa);

      /* if necessary, draw the black tiles */
      enum blend_impact_t impact = blend_impact_with_clear_black(blend_mode);
      if (impact != blend_impact_none)
        {
          ItemMaterial black_material;

          switch (impact)
            {
            case blend_impact_clear_black:
              black_material = ItemMaterial(create_value(Brush().base_color(vec4(0.0f, 0.0f, 0.0f, 0.0f))), draw.m_clip);
              break;

            case blend_impact_intertacts:
              black_material = mapped_image;
              break;

            default:
              ASTRALfailure("Invalid impact value");
            }

          details.draw_image_helper(image_rect, mip, black_material, ImageMipElement::empty_element, blend_mode, draw.m_with_aa);
        }

      end_pause_snapshot();
    }
  else
    {
      draw_rect(Rect().min_point(0.0f, 0.0f).max_point(image.m_size.x(), image.m_size.y()),
                draw.m_with_aa, ItemMaterial(im, draw.m_clip),
                blend_mode);
    }
}

void
astral::RenderEncoderBase::
draw_item_path(const ColorItemPathShader &shader,
               c_array<const ItemPath::Layer> layers,
               const ItemMaterial &material,
               enum blend_mode_t blend_mode) const
{
  ASTRALassert(!finished());
  if (layers.empty())
    {
      return;
    }

  unsigned int sz;
  c_array<gvec4> data;
  BoundingBox<float> bb;

  sz = ColorItemPathShader::item_data_size(layers.size());
  renderer_implement().m_workroom->m_item_data_workroom.resize(sz);
  data = make_c_array(renderer_implement().m_workroom->m_item_data_workroom);

  bb = ColorItemPathShader::pack_item_data(*renderer_implement().m_engine, layers, data);

  if (!bb.empty())
    {
      RectItem item(*shader.get(), create_item_data(data, no_item_data_value_mapping));
      draw_custom_rect(bb.as_rect(), item, material, blend_mode);
    }
}

void
astral::RenderEncoderBase::
stroke_paths(const MaskStrokeShader &shader,
             const CombinedPath &paths,
             const StrokeParameters &params,
             const StrokeShader::ItemDataPackerBase &packer,
             const ItemMaterial &material,
             enum blend_mode_t blend_mode,
             MaskUsage mask_usage,
             const StrokeMaskProperties &mask_properties,
             MaskDetails *out_data) const
{
  MaskDetails data;
  Renderer::Implement::DrawCommandList *p;

  ASTRALassert(!finished());
  if (!out_data)
    {
      out_data = &data;
    }

  generate_mask(shader, paths, params, packer, mask_properties, mask_usage.m_mask_type, out_data);
  if (!out_data->m_mask)
    {
      return;
    }

  p = virtual_buffer().command_list();
  if (!p)
    {
      return;
    }

  /* Draw the logical rect with the named mask applied. */
  draw_mask(*out_data, mask_usage.m_filter, material, blend_mode);
}

void
astral::RenderEncoderBase::Details::
handle_cooked_data_helper(int shader, //value from ShaderIndices
                          const StrokeShader::SimpleCookedData &cooked_data,
                          enum StrokeShader::primitive_type_t P,
                          std::vector<const VertexData*> *dst_vertex_datas,
                          std::vector<ColorItem::SubItem> *dst_sub_items) const
{
  if (shader == -1)
    {
      return;
    }

  ColorItem::SubItem sub_item;

  sub_item.m_shader = shader;
  sub_item.m_vertex_data = dst_vertex_datas->size();
  sub_item.m_vertices.m_begin = 0u;
  sub_item.m_vertices.m_end = cooked_data.vertex_data(P).number_vertices();

  dst_vertex_datas->push_back(&cooked_data.vertex_data(P));
  dst_sub_items->push_back(sub_item);
}

void
astral::RenderEncoderBase::Details::
handle_cooked_data(const ShaderIndices &shader,
                   const StrokeShader::SimpleCookedData &cooked_data,
                   std::vector<const VertexData*> *dst_vertex_datas,
                   std::vector<ColorItem::SubItem> *dst_sub_items) const
{
  handle_cooked_data_helper(shader.line_segment_shader(),
                            cooked_data,
                            StrokeShader::line_segments_primitive,
                            dst_vertex_datas, dst_sub_items);

  handle_cooked_data_helper(shader.biarc_curve_shader(),
                            cooked_data,
                            StrokeShader::biarc_curves_primitive,
                            dst_vertex_datas, dst_sub_items);

  handle_cooked_data_helper(shader.line_capper_shaders(StrokeShader::capper_shader_start),
                            cooked_data,
                            StrokeShader::segments_cappers_primitive,
                            dst_vertex_datas, dst_sub_items);

  handle_cooked_data_helper(shader.line_capper_shaders(StrokeShader::capper_shader_end),
                            cooked_data,
                            StrokeShader::segments_cappers_primitive,
                            dst_vertex_datas, dst_sub_items);

  handle_cooked_data_helper(shader.quadratic_capper_shaders(StrokeShader::capper_shader_start),
                            cooked_data,
                            StrokeShader::biarc_curves_cappers_primitive,
                            dst_vertex_datas, dst_sub_items);

  handle_cooked_data_helper(shader.quadratic_capper_shaders(StrokeShader::capper_shader_end),
                            cooked_data,
                            StrokeShader::biarc_curves_cappers_primitive,
                            dst_vertex_datas, dst_sub_items);

  handle_cooked_data_helper(shader.inner_glue_shader(),
                            cooked_data,
                            StrokeShader::glue_primitive,
                            dst_vertex_datas, dst_sub_items);

  handle_cooked_data_helper(shader.outer_glue_shader(),
                            cooked_data,
                            StrokeShader::glue_primitive,
                            dst_vertex_datas, dst_sub_items);

  handle_cooked_data_helper(shader.outer_glue_cusp_shader(),
                            cooked_data,
                            StrokeShader::glue_cusp_primitive,
                            dst_vertex_datas, dst_sub_items);

  handle_cooked_data_helper(shader.inner_glue_shader(),
                            cooked_data,
                            StrokeShader::inner_glue_primitive,
                            dst_vertex_datas, dst_sub_items);

  handle_cooked_data_helper(shader.join_shader(),
                            cooked_data,
                            StrokeShader::joins_primitive,
                            dst_vertex_datas, dst_sub_items);

  handle_cooked_data_helper(shader.cap_shader(),
                            cooked_data,
                            StrokeShader::caps_primitive,
                            dst_vertex_datas, dst_sub_items);
}


template<typename T>
void
astral::RenderEncoderBase::Details::
direct_stroke_path_implement(const ShaderIndices &shader,
                             const T &path, float t,
                             const vec2 *translate, const float2x2 *matrix,
                             const StrokeParameters &stroke_params,
                             const StrokeShader::ItemDataPackerBase &packer,
                             const ItemMaterial &material,
                             enum blend_mode_t blend_mode) const
{
  if (path.number_contours() == 0)
    {
      return;
    }

  Renderer::Implement::WorkRoom &workroom(*renderer_implement().m_workroom);
  Renderer::Implement::WorkRoom::ColorItem &tmp(workroom.m_color_item);
  ColorItem color_item;
  RenderValue<Transformation> logical_transformation_path;
  float tol;

  if (translate || matrix)
    {
      Transformation tmp;

      if (translate)
        {
          tmp.m_translate = *translate;
        }

      if (matrix)
        {
          tmp.m_matrix = *matrix;
        }

      logical_transformation_path = create_value(tmp);
    }

  RectRegion region;
  BoundingBox<float> edge_bb, join_bb, cap_bb;
  float edge_expanse_factor, join_expanse_factor, r;

  edge_expanse_factor = packer.edge_stroke_inflate_factor(stroke_params.m_join,
                                                          stroke_params.m_cap);

  join_expanse_factor = packer.join_stroke_inflate_factor_with_miter(stroke_params.m_miter_limit,
                                                                     stroke_params.m_join,
                                                                     stroke_params.m_cap);

  r = t_max(0.0f, 0.5f * stroke_params.m_width);
  edge_bb = path.bounding_box(t);
  join_bb = path.join_bounding_box(t);
  cap_bb = path.open_contour_endpoint_bounding_box(t);

  if (logical_transformation_path.valid())
    {
      edge_bb = logical_transformation_path.value().apply_to_bb(edge_bb);
      join_bb = logical_transformation_path.value().apply_to_bb(join_bb);
      cap_bb = logical_transformation_path.value().apply_to_bb(cap_bb);
    }

  if (!edge_bb.empty() && stroke_params.m_draw_edges)
    {
      edge_bb.enlarge(vec2(edge_expanse_factor * r));
      region.m_rect.union_box(edge_bb);
    }

  if (!join_bb.empty() && stroke_params.m_join != join_none && r > 0.0f)
    {
      join_bb.enlarge(vec2(join_expanse_factor * r));
      region.m_rect.union_box(join_bb);
    }

  if (!cap_bb.empty() && stroke_params.m_cap != cap_flat && r > 0.0f)
    {
      float f;

      f = (stroke_params.m_cap == cap_square) ? 1.0f : ASTRAL_SQRT2;
      cap_bb.enlarge(vec2(f * r));
      region.m_rect.union_box(cap_bb);
    }

  TransformedBoundingBox pixel_coords_region(region.m_rect, transformation());

  /* skip the rendering if region does not intersect the clip-rect */
  if (!pixel_coords_region.intersects(virtual_buffer().pixel_rect()))
    {
      return;
    }

  /* build the ColorItem */
  tmp.clear();
  tol = virtual_buffer().compute_tol(matrix);

  /* create the ItemData value */
  workroom.m_item_data_workroom.resize(packer.item_data_size(stroke_params));
  packer.pack_item_data(logical_transformation_path, stroke_params, t,
                        make_c_array(workroom.m_item_data_workroom));
  color_item.m_item_data = create_item_data(make_c_array(workroom.m_item_data_workroom),
                                            packer.intrepreted_value_map());

  for (unsigned int c = 0, endc = path.number_contours(); c < endc; ++c)
    {
      /* TODO: instead of doing the intersection test against path,
       *       should we instead run it against each contour seperately?
       *       Or perhaps in addition?
       */

      const StrokeShader::SimpleCookedData *cooked_data;

      cooked_data = &path.contour(c).simple_stroke_render_data(tol, *renderer_implement().m_engine);
      ASTRALassert(cooked_data->path_shader() == T::stroke_shader_enum());

      handle_cooked_data(shader, *cooked_data, &tmp.m_vertex_datas, &tmp.m_sub_items);
    }

  /* fill the fields of the RenderSupportTypes::ColorItem */
  color_item.m_shaders = shader.shaders();
  color_item.m_vertex_datas = make_c_array(tmp.m_vertex_datas);
  color_item.m_sub_items = make_c_array(tmp.m_sub_items);

  /* issue the draw */
  draw_custom(region, color_item, material, blend_mode);
}

template<typename T>
void
astral::RenderEncoderBase::Details::
direct_stroke_pathT(bool skip_joins_caps,
                    const DirectStrokeShader &shader,
                    const CombinedPath &P,
                    const StrokeParameters &stroke_params,
                    const StrokeShader::ItemDataPackerBase &packer,
                    const ItemMaterial &material,
                    enum blend_mode_t blend_mode) const
{
  c_array<const pointer<const T>> paths;

  paths = P.paths<T>();
  if (paths.empty())
    {
      return;
    }

  enum StrokeShader::path_shader_t path_shader(T::stroke_shader_enum());
  const DirectStrokeShader::ItemShaderSet &shaders(shader.shader_set(stroke_params.m_cap).m_subset[path_shader]);
  ShaderIndices shader_indices(skip_joins_caps, stroke_params, shaders);

  for (unsigned int i = 0; i < paths.size(); ++i)
    {
      const vec2 *tr;
      const float2x2 *mat;
      float t;

      t = P.get_t<T>(i);
      tr = P.get_translate<T>(i);
      mat = P.get_matrix<T>(i);

      direct_stroke_path_implement<T>(shader_indices, *paths[i], t, tr, mat,
                                      stroke_params, packer, material, blend_mode);
    }
}

void
astral::RenderEncoderBase::
direct_stroke_paths(const DirectStrokeShader &shader,
                    const CombinedPath &paths,
                    const StrokeParameters &stroke_params,
                    const StrokeShader::ItemDataPackerBase &packer,
                    const ItemMaterial &material,
                    enum blend_mode_t blend_mode) const
{
  Renderer::VirtualBuffer &vb(virtual_buffer());
  Renderer::Implement::DrawCommandList *cmd_list(vb.command_list());

  if (!cmd_list)
    {
      return;
    }

  Renderer::Implement::DrawCommandList::SubListMarker begin(*cmd_list);
  Details worker(*this);
  bool skip_joins_caps;

  skip_joins_caps = packer.caps_joins_collapse(transformation().m_matrix, render_scale_factor(), stroke_params);
  worker.direct_stroke_pathT<Path>(skip_joins_caps, shader, paths, stroke_params, packer, material, blend_mode);
  worker.direct_stroke_pathT<AnimatedPath>(skip_joins_caps, shader, paths, stroke_params, packer, material, blend_mode);

  if (!vb.m_use_sub_ubers || vb.m_renderer.m_properties.m_overridable_properties.m_uber_shader_method != uber_shader_none)
    {
      return;
    }

  Renderer::Implement::DrawCommandList::SubListMarker end(*cmd_list);

  /* we only want to override the non-opaque draws */
  Renderer::Implement &implement(vb.m_renderer);
  Renderer::Implement::UberShadingKeyCollection &UK(implement.m_workroom->m_sub_ubers);
  BackendBlendMode backend_blend_mode(true, blend_mode);

  c_array<Renderer::Implement::DrawCommand> cmds;
  vecN<RenderBackend::UberShadingKey::Cookie, clip_window_value_type_count> K;

  cmds = cmd_list->sublist(Renderer::Implement::DrawCommandList::typical_command_list, begin, end);
  for (unsigned int i = 0; i < clip_window_value_type_count; ++i)
    {
      enum clip_window_value_type_t e;

      e = static_cast<enum clip_window_value_type_t>(i);
      K[e] = UK.stroke_uber(implement, e, shader, material.m_material.material_shader(), stroke_params.m_cap, backend_blend_mode);
    }

  for (Renderer::Implement::DrawCommand &cmd : cmds)
    {
      cmd.m_sub_uber_shader_key = K;
    }
}

void
astral::RenderEncoderBase::
fill_paths(const CombinedPath &paths,
           const FillParameters &fill_params,
           const ItemMaterial &material,
           enum blend_mode_t blend_mode,
           MaskUsage mask_usage,
           const FillMaskProperties &mask_properties,
           MaskDetails *out_data,
           reference_counted_ptr<const RenderClipElement> *out_clip_element) const
{
  MaskDetails data;
  Renderer::Implement::DrawCommandList *p;

  ASTRALassert(!finished());
  if (!out_data)
    {
      out_data = &data;
    }

  generate_mask(paths, fill_params, mask_properties, mask_usage.m_mask_type, out_data, out_clip_element);
  if (!out_data->m_mask)
    {
      return;
    }

  p = virtual_buffer().command_list();
  if (!p)
    {
      return;
    }

  ASTRALassert(out_data->m_mask);
  draw_mask(*out_data, mask_usage.m_filter, material, blend_mode);
}

int
astral::RenderEncoderBase::
draw_text(const GlyphShader &shader,
          const TextItem &text,
          const GlyphShader::ItemDataPackerBase &packer,
          const ItemMaterial &material,
          enum blend_mode_t blend_mode) const
{
  const ColorItemShader *p;
  Transformation tr;
  Rect pixel_rect;
  unsigned int sz;
  ItemData item_data;
  RectRegion R;
  int return_value;

  ASTRALassert(!finished());

  R.m_rect = packer.bounding_box(text);

  /* Early out if R does not hit the rendering bounding box;
   * The early out's main purpose is to prevent the TextItem
   * from realizing image glyphs if they are not drawn.
   */
  if (!TransformedBoundingBox(R.m_rect, transformation()).intersects(pixel_bounding_box()))
    {
      return -1;
    }

  if (text.font().typeface().is_scalable())
    {
      p = shader.m_scalable_shader.get();
    }
  else
    {
      p = shader.m_image_shader.get();
    }

  sz = packer.item_data_size();
  if (sz != 0u)
    {
      Renderer::Implement::WorkRoom &workroom(*renderer_implement().m_workroom);
      c_array<gvec4> workroom_ptr;

      workroom.m_item_data_workroom.resize(sz);
      workroom_ptr = make_c_array(workroom.m_item_data_workroom);

      packer.pack_item_data(workroom_ptr);
      item_data = create_item_data(workroom_ptr, packer.intrepreted_value_map());
    }

  ASTRALassert(p);
  /* TODO: we should take into account the rendering scale factor as well
   *       when computing the zoom_factor (for example if the rendering
   *       scale factor is 0.5 and singular_values().x() is 2.0, then the
   *       zoom factor passed should be 1.0).
   */
  float zoom_factor(singular_values().x());
  const RenderData &render_data(text.render_data(zoom_factor, render_engine(), &return_value));
  Item<ColorItemShader> item(*p, item_data, *render_data.m_vertex_data);

  draw_custom(R, item, material, blend_mode);

  return return_value;
}

enum astral::return_code
astral::RenderEncoderBase::
draw_text_as_path(const TextItem &text,
                  const ItemMaterial &material,
                  enum blend_mode_t blend_mode,
                  MaskUsage mask_usage,
                  const FillMaskProperties &fill_props) const
{
  vecN<CombinedPath, number_fill_rule> paths;
  c_array<const unsigned int> color_glyph_ids;
  float scale_factor;

  if (!text.combined_paths(&paths, &color_glyph_ids, &scale_factor))
    {
      return routine_fail;
    }

  save_transformation();
  scale(scale_factor, scale_factor);
  for (unsigned int i = 0; i < number_fill_rule; ++i)
    {
      if (!paths[i].empty())
        {
          fill_paths(paths[i],
                     FillParameters().fill_rule(static_cast<enum fill_rule_t>(i)),
                     material, blend_mode,
                     mask_usage, fill_props);
        }
    }
  restore_transformation();

  for (unsigned int g : color_glyph_ids)
    {
      Glyph glyph;
      GlyphPaletteID palette;
      vec2 position;

      text.glyph(g, &glyph, &position, &palette);
      ASTRALassert(glyph.valid() && glyph.is_colored());

      const GlyphColors *glyph_colors(glyph.colors());
      ASTRALassert(glyph_colors);

      RenderEncoderLayer encoder_layer;
      RenderEncoderBase encoder(*this);
      float alpha = 1.0f;

      /* For now, we are only going to respect the alpha coming from a Brush
       * with a constant color.
       *
       * TODO: respect material.m_material when it has a shader that emits
       *       alpha smaller than one or a brush where the gradient or image
       *       emits an alpha smaller than 1.0. One way to do this would be
       *       to have a ColorItemShader that takes a rect and image and then
       *       to use that with the material shader. We will leave this on the
       *       back burner until such time that colored scalable glyphs with
       *       a custom material become important.
       */
      if (material.m_material.brush().valid())
        {
          alpha = material.m_material.brush().value().m_base_color.w();
        }

      if (blend_mode != blend_porter_duff_src_over || alpha < 1.0f)
        {
          BoundingBox<float> bb;
          ScaleTranslate sc;

          sc.m_translate = position;
          sc.m_scale = vec2(scale_factor, scale_factor);
          bb = sc.apply_to_bb(glyph.scalable_metrics().m_bb);

          encoder_layer = begin_layer(bb, alpha, blend_mode, material.m_clip);
          encoder = encoder_layer.encoder();
        }

      for (unsigned int layer = 0, endlayer = glyph_colors->number_layers(); layer < endlayer; ++layer)
        {
          enum fill_rule_t fill_rule;
          const Path *path;

          path = glyph.path(layer, &fill_rule);
          if (!path || path->number_contours() == 0)
            {
              continue;
            }

          RenderValue<Brush> br;
          vec4 c = glyph_colors->color(palette, layer);

          br = create_value(Brush().base_color(c));
          encoder.fill_paths(CombinedPath(*path, position, vec2(scale_factor)),
                             FillParameters().fill_rule(fill_rule),
                             ItemMaterial(br, material.m_clip),
                             blend_porter_duff_src_over,
                             mask_usage, fill_props);
        }

      if (encoder_layer.valid())
        {
          end_layer(encoder_layer);
        }
    }

  return routine_success;
}

astral::BoundingBox<float>
astral::RenderEncoderBase::
pixel_bounding_box(const BoundingBox<float> &logical_bb) const
{
  const Transformation &tr(transformation());
  BoundingBox<float> return_value;

  return_value.union_point(tr.apply_to_point(logical_bb.as_rect().point(RectEnums::minx_miny_corner)));
  return_value.union_point(tr.apply_to_point(logical_bb.as_rect().point(RectEnums::minx_maxy_corner)));
  return_value.union_point(tr.apply_to_point(logical_bb.as_rect().point(RectEnums::maxx_miny_corner)));
  return_value.union_point(tr.apply_to_point(logical_bb.as_rect().point(RectEnums::maxx_maxy_corner)));

  /* question: should we apply clipping? */
  return_value.intersect_against(BoundingBox<float>(virtual_buffer().pixel_rect()));
  return return_value;
}

astral::BoundingBox<float>
astral::RenderEncoderBase::
pixel_bounding_box(void) const
{
  return virtual_buffer().pixel_rect();
}

astral::RenderClipNode
astral::RenderEncoderBase::
begin_clip_node_pixel(enum blend_mode_t blend_mode,
                      enum clip_node_flags_t flags,
                      const MaskDetails &mask,
                      const BoundingBox<float> &pclip_in_bbox,
                      const BoundingBox<float> &pclip_out_bbox,
                      enum filter_t mask_filter,
                      const ItemMask &clip) const
{
  RenderClipNode::Backing::Begin encoder(*this, mask);

  return encoder.begin_clip_node_pixel_implement(blend_mode, flags,
                                                 pclip_in_bbox, pclip_out_bbox,
                                                 mask_filter, clip);
}

astral::RenderClipNode
astral::RenderEncoderBase::
begin_clip_node_pixel(enum blend_mode_t blend_mode,
                      enum clip_node_flags_t flags,
                      const RenderClipCombineResult &mask_buffer,
                      const BoundingBox<float> &clip_in_bbox,
                      const BoundingBox<float> &clip_out_bbox,
                      enum filter_t mask_filter,
                      const ItemMask &clip)
{
  const Renderer::Implement::ClipCombineResult *p;

  p = static_cast<const Renderer::Implement::ClipCombineResult*>(&mask_buffer);
  RenderClipNode::Backing::Begin encoder(*this, *p);

  return encoder.begin_clip_node_pixel_implement(blend_mode, flags,
                                                 clip_in_bbox, clip_out_bbox,
                                                 mask_filter, clip);
}

astral::RenderClipNode
astral::RenderEncoderBase::
begin_clip_node_logical(enum blend_mode_t blend_mode,
                        enum clip_node_flags_t flags,
                        const CombinedPath &paths,
                        const FillParameters &params,
                        const FillMaskProperties &mask_properties,
                        MaskUsage mask_usage,
                        MaskDetails *out_data,
                        const ItemMask &clip) const
{
  RenderClipNode return_value;
  BoundingBox<float> bb;
  MaskDetails datav;

  ASTRALassert(!finished());

  /* if all are null, then no rendering and a null out_data
   * means to not return the the Image, thus no point in
   * running anything.
   */
  if (flags == clip_node_none && !out_data)
    {
      return return_value;
    }

  if (!out_data)
    {
      out_data = &datav;
    }

  /* generate the mask */
  generate_mask(paths, params, mask_properties, mask_usage.m_mask_type, out_data);

  /* TODO: perhaps have something where the caller can
   *       specify the clip-out rect ?
   */
  BoundingBox<float> pixel_rect(out_data->pixel_rect());
  return_value = begin_clip_node_pixel(blend_mode, flags,
                                       *out_data,
                                       pixel_rect,
                                       pixel_rect,
                                       mask_usage.m_filter,
                                       clip);

  ASTRALassert((flags & clip_node_clip_in) == 0 || return_value.clip_in().valid());
  ASTRALassert((flags & clip_node_clip_out) == 0 || return_value.clip_out().valid());

  return return_value;
}

void
astral::RenderEncoderBase::
end_clip_node(RenderClipNode clip_node) const
{
  if (!clip_node.m_backing)
    {
      return;
    }

  RenderClipNode::Backing::End encoder(*this, *clip_node.m_backing);
  encoder.end_clip_node_implement();
}

astral::RenderEncoderMask
astral::RenderEncoderBase::
encoder_mask(ivec2 size) const
{
  RenderEncoderBase return_value;

  return_value = renderer_implement().m_storage->create_virtual_buffer(VB_TAG, size, number_fill_rule,
                                                                       Renderer::VirtualBuffer::ImageCreationSpec());
  return_value.render_accuracy(render_accuracy());
  return_value.use_sub_ubers(use_sub_ubers());

  return return_value;
}

astral::RenderEncoderImage
astral::RenderEncoderBase::
encoder_image(ivec2 size, enum colorspace_t colorspace) const
{
  RenderEncoderBase return_value;

  return_value = renderer_implement().m_storage->create_virtual_buffer(VB_TAG, size, colorspace,
                                                                       Renderer::VirtualBuffer::ImageCreationSpec());
  return_value.render_accuracy(render_accuracy());
  return_value.use_sub_ubers(use_sub_ubers());

  return return_value;
}

astral::RenderEncoderLayer
astral::RenderEncoderBase::
begin_layer(const BoundingBox<float> &bb, RenderScaleFactor scale_factor,
            enum colorspace_t colorspace, const vec4 &color,
            enum blend_mode_t blend_mode, enum filter_t filter_mode,
            const ItemMask &clip) const
{
  ASTRALassert(!finished());

  RenderEncoderLayer::Backing *return_value;

  return_value = renderer_implement().m_storage->create_render_encoder_layer(*this, bb, scale_factor,
                                                                             colorspace, color, blend_mode,
                                                                             filter_mode, clip);

  return RenderEncoderLayer(return_value);
}

astral::RenderEncoderLayer
astral::RenderEncoderBase::
begin_layer(const Effect &effect,
            const EffectParameters &effect_parameters,
            const BoundingBox<float> &in_logical_rect,
            enum colorspace_t colorspace,
            enum blend_mode_t blend_mode,
            const ItemMask &clip) const
{
  ASTRALassert(!finished());

  RenderEncoderLayer::Backing *return_value;

  return_value = renderer_implement().m_storage->create_render_encoder_layer(*this, *renderer_implement().m_storage,
                                                                             effect, effect_parameters, in_logical_rect,
                                                                             colorspace, blend_mode, clip);

  return RenderEncoderLayer(return_value);
}

astral::RenderEncoderLayer
astral::RenderEncoderBase::
begin_layer(const EffectCollectionBase &effects,
            const BoundingBox<float> &in_logical_rect,
            enum colorspace_t colorspace,
            const ItemMask &clip) const
{
  ASTRALassert(!finished());

  RenderEncoderLayer::Backing *return_value;

  return_value = renderer_implement().m_storage->create_render_encoder_layer(*this, *renderer_implement().m_storage,
                                                                             renderer_implement().m_workroom->m_render_encoder_layer,
                                                                             effects, in_logical_rect,
                                                                             colorspace, clip);

  return RenderEncoderLayer(return_value);
}

void
astral::RenderEncoderBase::
end_layer(RenderEncoderLayer layer) const
{
  if (!layer.m_backing)
    {
      return;
    }

  ASTRALassert(!layer.m_backing->end_layer_called());
  ASTRALassert(layer.m_backing->parent_encoder().m_virtual_buffer == m_virtual_buffer);

  if (!layer.m_backing->encoder().valid()
      || layer.m_backing->end_layer_called())
    {
      return;
    }

  layer.m_backing->end_layer(*renderer_implement().m_storage);
}

astral::RenderEncoderMask
astral::RenderEncoderBase::
encoder_mask_relative(const RelativeBoundingBox &bb,
                      RenderScaleFactor scale_factor,
                      unsigned int pixel_slack) const
{
  return virtual_buffer().generate_child_buffer(bb, number_fill_rule, pixel_slack, scale_factor,
                                                Renderer::VirtualBuffer::ImageCreationSpec());
}

astral::RenderEncoderImage
astral::RenderEncoderBase::
encoder_image_relative(const RelativeBoundingBox &bb,
                       RenderScaleFactor scale_factor,
                       enum colorspace_t colorspace,
                       unsigned int pixel_slack) const
{
  return virtual_buffer().generate_child_buffer(bb, colorspace, pixel_slack, scale_factor,
                                                Renderer::VirtualBuffer::ImageCreationSpec());
}

astral::RenderSupportTypes::Proxy
astral::RenderEncoderBase::
proxy_relative(const RelativeBoundingBox &bb,
               RenderScaleFactor scale_factor,
               unsigned int pixel_slack) const
{
  return virtual_buffer().generate_child_proxy(bb, pixel_slack, scale_factor);
}

astral::RenderEncoderMask
astral::RenderEncoderBase::
encoder_mask(Proxy proxy) const
{
  return virtual_buffer().generate_buffer_from_proxy(proxy, number_fill_rule,
                                                     Renderer::VirtualBuffer::ImageCreationSpec());
}

astral::RenderEncoderImage
astral::RenderEncoderBase::
encoder_image(Proxy proxy, enum colorspace_t colorspace) const
{
  return virtual_buffer().generate_buffer_from_proxy(proxy, colorspace,
                                                     Renderer::VirtualBuffer::ImageCreationSpec());
}

bool
astral::RenderEncoderBase::
clips_box(BoundingBox<float> box,
          const astral::Transformation &pixel_transformation_box,
          const RenderClipElement *clip) const
{
  box = pixel_transformation_box.apply_to_bb(box);
  if (!pixel_bounding_box().contains(box))
    {
      return true;
    }

  if (!clip)
    {
      return false;
    }

  const MaskDetails *mask_details;

  mask_details = clip->mask_details();
  if (!mask_details || !mask_details->m_mask || mask_details->m_mask->mip_chain().empty())
    {
      /* no mask, means the clip will clip everything */
      return true;
    }

  if (!mask_details->pixel_rect().contains(box))
    {
      return true;
    }

  /* ick, walk the color and empty tiles of mask_details->m_image
   * and if any of them intersect box, then clipping hits the box.
   */
  box = mask_details->m_mask_transformation_pixel.apply_to_bb(box);

  const ImageMipElement *mip;
  mip = mask_details->m_mask->mip_chain().front().get();

  if (!mip)
    {
      return true;
    }

  for (unsigned int i = 0, endi = mip->number_elements(ImageMipElement::color_element); i < endi; ++i)
    {
      uvec2 sz, loc;

      sz = mip->element_size(ImageMipElement::color_element, i);
      loc = mip->element_location(ImageMipElement::color_element, i);
      if (box.intersects(BoundingBox<float>(vec2(loc), vec2(sz + loc))))
        {
          return true;
        }
    }

  for (unsigned int i = 0, endi = mip->number_elements(ImageMipElement::empty_element); i < endi; ++i)
    {
      uvec2 sz, loc;

      sz = mip->element_size(ImageMipElement::empty_element, i);
      loc = mip->element_location(ImageMipElement::empty_element, i);
      if (box.intersects(BoundingBox<float>(vec2(loc), vec2(sz + loc))))
        {
          return true;
        }
    }

  return false;
}

void
astral::RenderEncoderBase::
generate_mask(const CombinedPath &paths,
              const FillParameters &params,
              const FillMaskProperties &mask_properties,
              enum mask_type_t mask_type,
              MaskDetails *out_data,
              reference_counted_ptr<const RenderClipElement> *out_clip_element) const
{
  BoundingBox<float> bb;

  ASTRALassert(valid());
  ASTRALassert(out_data);

  if (mask_properties.m_apply_clip_equations_clipping && virtual_buffer().type() == VirtualBuffer::degenerate_buffer)
    {
      out_data->m_mask = nullptr;
      out_data->m_min_corner = astral::vec2(0.0f, 0.0f);
      out_data->m_size = astral::vec2(0.0f, 0.0f);
      out_data->m_mask_channel = MaskUsage::mask_channel(mask_type);
      out_data->m_mask_type = mask_type;
      out_data->m_mask_transformation_pixel = ScaleTranslate();

      if (out_clip_element)
        {
          Renderer::Implement::ClipElement *p;
          vecN<enum mask_channel_t, number_mask_type> mask_channels;

          mask_channels[mask_type_coverage] = MaskUsage::mask_channel(mask_type_coverage);
          mask_channels[mask_type_distance_field] = MaskUsage::mask_channel(mask_type_distance_field);

          p = renderer_implement().m_storage->create_clip_element(Renderer::Implement::ClipGeometrySimple(),
                                                                  Renderer::Implement::ClipGeometryGroup::Token(),
                                                                  out_data->m_mask, mask_channels, mask_type);
          *out_clip_element = p;
        }

      return;
    }

  if (mask_properties.m_complement_bbox && fill_rule_is_complement_rule(params.m_fill_rule))
    {
      bb = *mask_properties.m_complement_bbox;
    }
  else
    {
      bb = paths.compute_bounding_box();
    }

  /* The pixel slack must be ImageAtlas::tile_padding to interact
   * correctly with sparse masks and non-sparse masks especially
   * when params.m_render_scale_factor is small.
   */
  unsigned int pixel_slack(ImageAtlas::tile_padding);
  Renderer::Implement::ClipGeometryGroup clip_geometry;
  Transformation mask_transformation_logical;
  RelativeBoundingBox relative_bounding_box(bb, mask_properties.m_restrict_bb);

  relative_bounding_box.m_inherit_clipping_of_parent = mask_properties.m_apply_clip_equations_clipping;
  clip_geometry = virtual_buffer().child_clip_geometry(mask_properties.m_render_scale_factor,
                                                       relative_bounding_box, pixel_slack);


  mask_transformation_logical = clip_geometry.bounding_geometry().image_transformation_logical(transformation());
  if (paths.paths<AnimatedPath>().empty() && mask_properties.use_mask_shader(clip_geometry.bounding_geometry().image_size()))
    {
      Renderer::Implement::Filler::create_mask_via_item_path_shader(renderer_implement(), mask_properties.m_path_shader,
                                                                    virtual_buffer().logical_rendering_accuracy(),
                                                                    params.m_fill_rule, paths, clip_geometry.bounding_geometry(),
                                                                    mask_transformation_logical, out_data);
    }
  else
    {
      renderer_implement().m_filler[mask_properties.m_sparse_mask]->create_mask(virtual_buffer().logical_rendering_accuracy(),
                                                                                params.m_fill_rule, params.m_aa_mode, paths,
                                                                                clip_geometry.bounding_geometry(),
                                                                                clip_geometry.sub_rects(*renderer_implement().m_storage),
                                                                                mask_transformation_logical,
                                                                                out_data);
    }

  out_data->m_mask_type = mask_type;
  out_data->m_mask_channel = MaskUsage::mask_channel(mask_type);

  if (mask_properties.m_restrict_bb)
    {
      out_data->instersect_against_pixel_rect(*mask_properties.m_restrict_bb);
    }

  if (out_clip_element)
    {
      Renderer::Implement::ClipElement *p;
      vecN<enum mask_channel_t, number_mask_type> mask_channels;

      mask_channels[mask_type_coverage] = mask_channel_red;
      mask_channels[mask_type_distance_field] = mask_channel_green;

      p = renderer_implement().m_storage->create_clip_element(clip_geometry.bounding_geometry(),
                                                              clip_geometry.token(),
                                                              out_data->m_mask, mask_channels, mask_type);

      *out_clip_element = p;
    }
}

astral::reference_counted_ptr<const astral::RenderClipCombineResult>
astral::RenderEncoderBase::
combine_clipping(const RenderClipElement &clip_element,
                 const CombinedPath &path,
                 const RenderClipCombineParams &params) const
{
  RenderClipCombineResult *p;

  p = renderer_implement().m_storage->create_clip_combine_result(virtual_buffer().logical_rendering_accuracy(),
                                                                 transformation(), clip_element, path, params,
                                                                 Renderer::Implement::Filler::clip_combine_both);
  return p;
}

astral::reference_counted_ptr<const astral::RenderClipElement>
astral::RenderEncoderBase::
intersect_clipping(const RenderClipElement &clip_element,
                   const CombinedPath &path,
                   const RenderClipCombineParams &params) const
{
  reference_counted_ptr<const RenderClipCombineResult> p;

  if (!clip_element.mask_details())
    {
      return renderer_implement().m_storage->create_empty_clip_element(clip_element.preferred_mask_type());
    }

  BoundingBox<float> bb;
  bb = path.compute_bounding_box();
  if (bb.empty())
    {
      return renderer_implement().m_storage->create_empty_clip_element(clip_element.preferred_mask_type());
    }

  /* should we intersect clip_element against the bounding box of
   * path first? OR should we assume the caller already did this?
   */
  reference_counted_ptr<const RenderClipElement> q;

  bb = transformation().apply_to_bb(bb);
  q = clip_element.intersect(bb);

  if (!q->mask_details())
    {
      return q;
    }

  p = renderer_implement().m_storage->create_clip_combine_result(virtual_buffer().logical_rendering_accuracy(),
                                                                 transformation(), *q, path, params,
                                                                 Renderer::Implement::Filler::clip_combine_intersect_only);

  return p->clip_in();
}

void
astral::RenderEncoderBase::
generate_mask(const MaskStrokeShader &shader,
              const CombinedPath &paths,
              const StrokeParameters &stroke_params,
              const StrokeShader::ItemDataPackerBase &packer,
              const StrokeMaskProperties &mask_properties,
              enum mask_type_t mask_type,
              MaskDetails *out_data) const
{
  ASTRALassert(valid());
  ASTRALassert(out_data);

  if (virtual_buffer().type() == VirtualBuffer::degenerate_buffer)
    {
      out_data->m_mask = nullptr;
      out_data->m_min_corner = astral::vec2(0.0f, 0.0f);
      out_data->m_size = astral::vec2(0.0f, 0.0f);
      out_data->m_mask_channel = MaskUsage::mask_channel(mask_type);
      out_data->m_mask_type = mask_type;
      out_data->m_mask_transformation_pixel = ScaleTranslate();

      return;
    }

  RenderEncoderStrokeMask generator;
  float current_t(0.0f);

  generator = encoder_stroke(mask_properties);
  generator.set_shader(shader);
  generator.set_item_packer(&packer);
  generator.set_stroke_params(stroke_params, current_t);
  generator.add_path(paths);
  generator.finish();

  *out_data = generator.mask_details(mask_type);

  if (mask_properties.m_restrict_bb)
    {
      out_data->instersect_against_pixel_rect(*mask_properties.m_restrict_bb);
    }
}

astral::RenderEncoderShadowMap
astral::RenderEncoderBase::
encoder_shadow_map(int dimensions, vec2 light_p) const
{
  RenderEncoderBase return_value;
  reference_counted_ptr<ShadowMap> dst;

  dst = render_engine().shadow_map_atlas().create(dimensions, light_p);
  return_value = renderer_implement().m_storage->create_virtual_buffer(VB_TAG, dst, light_p);
  return_value.render_accuracy(render_accuracy());
  return_value.use_sub_ubers(use_sub_ubers());
  return RenderEncoderShadowMap(return_value);
}

astral::RenderEncoderShadowMap
astral::RenderEncoderBase::
encoder_shadow_map_relative(int dimensions, vec2 light_p) const
{
  light_p = transformation().apply_to_point(light_p);

  RenderEncoderShadowMap M(encoder_shadow_map(dimensions, light_p));
  M.transformation(transformation());
  return M;
}

astral::RenderEncoderStrokeMask
astral::RenderEncoderBase::
encoder_stroke(const StrokeMaskProperties &mask_properties) const
{
  RenderEncoderStrokeMask return_value;

  return_value = renderer_implement().m_storage->create_stroke_builder(virtual_buffer().clip_geometry(),
                                                                       mask_properties,
                                                                       transformation(),
                                                                       render_accuracy());

  return return_value;
}

astral::reference_counted_ptr<astral::Image>
astral::RenderEncoderBase::
snapshot_logical(RenderEncoderBase src_encoder,
                     const RelativeBoundingBox &logical_bb,
                     RenderScaleFactor scale_rendering,
                     Transformation *out_image_transformation_logical,
                     unsigned int pixel_slack,
                     unsigned int lod_requirement) const
{
  return Details(*this).snapshot_logical_implement(src_encoder,
                                                   logical_bb, scale_rendering,
                                                   out_image_transformation_logical,
                                                   pixel_slack, lod_requirement);
}

astral::reference_counted_ptr<astral::Image>
astral::RenderEncoderBase::Details::
snapshot_logical_implement(RenderEncoderBase src_encoder,
                           const RelativeBoundingBox &logical_bb,
                           RenderScaleFactor scale_rendering,
                           Transformation *out_image_transformation_logical,
                           unsigned int pixel_slack,
                           unsigned int lod_requirement) const
{
  reference_counted_ptr<Image> return_value;
  RenderEncoderImage encoder;
  Transformation image_transformation_logical;

  /* TODO: if src_encoder is finished() and renders to an astral::Image,
   *       instead of re-rendering, just take that image
   */

  if (!out_image_transformation_logical)
    {
      out_image_transformation_logical = &image_transformation_logical;
    }

  /* MAYBE: Add occluders to encoder for the region outside of logical_bb
   *        to reduce pixel shader load. Can be a big deal if logical_bb
   *        is far from square and the current transformation has rotation.
   */

  encoder = encoder_image_relative(logical_bb, scale_rendering, pixel_slack);

  if (encoder.virtual_buffer().type() != Renderer::VirtualBuffer::degenerate_buffer)
    {
      BoundingBox<float> logical_bb_with_padding(logical_bb.bb_with_padding());
      float pixel_slack_in_logical;
      bool blit_rect, delete_commands;

      *out_image_transformation_logical = Transformation(encoder.image_transformation_pixel()) * transformation();

      /* We only blit the rect if all of these conditions are true:
       *  - src_encoder is not finished
       *  - all commands are taken, including unfinished commands
       *  - src_encoder is not in a pause snapshot session. This is because
       *    the pixels in return_value only reflect the commands before the
       *    snapshot session started and the blit below is opaque and would
       *    go infront of the commands that have already been added to the
       *    current snapshot session.
       *  - src_encoder's rendering scale factor is the same as the created data
       *  - logical_bb.m_pixel_bb is null
       *
       * TODO: we could still blit even if unfinished are to be skipped
       *       IF there no unfinished commands to skip. This requires
       *       plumbing in VirtualBuffer and DrawCommandList to enable.
       *
       * TODO: we can still blit a rect if logical_bb.m_pixel_bb is non-null,
       *       but instead of blitting the rect, we should draw the polygon
       *       of the logical_bb.m_pixel_bb intersected against the pixel rect
       *       of encoder; relying on ItemMask clipping would make the draw
       *       non-opaque which would defeat the purpose of having the draw.
       */
      blit_rect = !src_encoder.snapshot_paused()
        && !src_encoder.finished()
        && logical_bb.m_pixel_bb == nullptr
        && src_encoder.render_scale_factor() == encoder.render_scale_factor();

      /* We can only delete the commands from the src_encoder if in addition
       * to blitting the rect logical_bb_with_padding is not clipped
       * by the pixel rect of encoder.virtual_buffer(). Note that we map the
       * corners of logical_bb_with_padding_rect back to pixel coordinates
       * by applying the inverse of transformation().
       */
      const BoundingBox<float> &encoder_pixel_rect(encoder.virtual_buffer().pixel_rect());
      const Transformation &inv_tr(inverse_transformation());
      const Rect &logical_bb_with_padding_rect(logical_bb_with_padding.as_rect());

      /* TODO:
       *  (1) change delete_commands to mean delete command only if not intersecting commands were skipped
       *  (2) pass logical_bb.m_pixel_bb to specify that commands also need to intersect that
       *      rect if it is non-null.
       */
      delete_commands = blit_rect
        && encoder_pixel_rect.contains(inv_tr.apply_to_point(logical_bb_with_padding_rect.point(Rect::minx_miny_corner)))
        && encoder_pixel_rect.contains(inv_tr.apply_to_point(logical_bb_with_padding_rect.point(Rect::minx_maxy_corner)))
        && encoder_pixel_rect.contains(inv_tr.apply_to_point(logical_bb_with_padding_rect.point(Rect::maxx_miny_corner)))
        && encoder_pixel_rect.contains(inv_tr.apply_to_point(logical_bb_with_padding_rect.point(Rect::maxx_maxy_corner)));

      pixel_slack_in_logical = static_cast<float>(pixel_slack) * surface_pixel_size_in_logical_coordinates();
      encoder.virtual_buffer().copy_commands(src_encoder.virtual_buffer(),
                                             transformation_value(),
                                             logical_bb_with_padding,
                                             pixel_slack_in_logical,
                                             delete_commands);
      encoder.finish();
      return_value = encoder.image_with_mips(lod_requirement);

      if (blit_rect)
        {
          /* draw a rect blitting the contents of return_value that will
           * go infront of all content; this way those pixels do not get
           * rendered twice; by drawing a rect without anti-aliasing and
           * with the blend_porter_duff_src, it is just an opaque blit.
           */
          ImageSampler im(*encoder.image(), filter_nearest, mipmap_none);
          Transformation tr(*out_image_transformation_logical);
          Rect prect(logical_bb_with_padding.as_rect());
          Brush brush;

          im
            .x_tile_mode(tile_mode_clamp)
            .y_tile_mode(tile_mode_clamp);

          brush
            .image(create_value(im))
            .image_transformation(create_value(tr));

          if (src_encoder != *this)
            {
              src_encoder.save_transformation();
              src_encoder.transformation(transformation_value());
            }

          /* TODO: once copy_commands() takes logical_bb.m_pixel_bb, instead of
           *       drawing a rect, draw the convex polygon of logical_bb.m_pixel_bb
           *       intersected against prect.
           */
          src_encoder.draw_rect(prect, false, create_value(brush), blend_porter_duff_src);

          if (src_encoder != *this)
            {
              src_encoder.restore_transformation();
            }
        }

    }

  return return_value;
}

void
astral::RenderEncoderBase::
snapshot_effect(RenderEncoderBase src_encoder,
                const Effect &effect,
                c_array<const generic_data> custom_parameters,
                const RelativeBoundingBox &logical_rect,
                EffectMaterial *out_material) const
{
  /* this is not totally correct, if the encoder is finished
   * and is an image, we can return the image it resolves
   * to; the main issue is that out_pixel_rect will need
   * to be the entire image which is an unpleasant surprise
   * for the caller (though assert'ing is an even worse
   * surprise). Worse, the effect will need to be told
   * somehow to restrict its rendering to a sub-image as
   * well.
   */
  ASTRALassert(!finished());

  AutoRestore auto_restore(*this);
  Effect::BufferParameters buffer_parameters;
  Effect::OverridableBufferProperties overridable_properties;
  Effect::BufferProperties buffer_properties;
  Effect::BlitParameters blit_params;
  Renderer::Implement::Storage::EffectDataHolder effect_data(*renderer_implement().m_storage);
  astral::reference_counted_ptr<astral::Image> image;

  buffer_parameters.m_custom_data = custom_parameters;
  buffer_parameters.m_pixel_transformation_logical = transformation();
  buffer_parameters.m_singular_values = singular_values();
  buffer_parameters.m_logical_rect = logical_rect.bb_with_padding().as_rect();
  buffer_parameters.m_render_scale_factor = render_scale_factor();

  effect.compute_overridable_buffer_properties(buffer_parameters, &overridable_properties);
  effect.compute_buffer_properties(overridable_properties, buffer_parameters,
                                   &effect_data.processed_params(), &buffer_properties);

  blit_params.m_logical_rect = logical_rect;
  blit_params.m_logical_rect.m_padding += overridable_properties.m_logical_slack;

  ASTRALassert(overridable_properties.m_render_scale_factor > 0.0f);
  image = snapshot_logical(src_encoder, blit_params.m_logical_rect,
                           RenderScaleFactor(overridable_properties.m_render_scale_factor, false),
                           &blit_params.m_content_transformation_logical,
                           buffer_properties.m_pixel_slack,
                           buffer_properties.m_required_lod);

  if (image)
    {
      /* generate the material via the effect */
      effect.material_effect(renderer_implement(),
                             make_c_array(effect_data.processed_params()),
                             effect_data.workroom(),
                             *image,
                             blit_params, out_material);
    }
  else
    {
      out_material->m_material = Material();
      out_material->m_material_transformation_rect = Transformation();
      out_material->m_rect.min_point(0.0f, 0.0f).max_point(0.0f, 0.0f);
    }
}

void
astral::RenderEncoderBase::
finish(void) const
{
  virtual_buffer().issue_finish();
}

//////////////////////////////////////
// astral::RenderEncoderImage methods
bool
astral::RenderEncoderImage::
use_pixel_rect_tile_culling(void) const
{
  return virtual_buffer().m_use_pixel_rect_tile_culling;
}

void
astral::RenderEncoderImage::
use_pixel_rect_tile_culling(bool b) const
{
  virtual_buffer().m_use_pixel_rect_tile_culling = b;
}

astral::reference_counted_ptr<astral::Image>
astral::RenderEncoderImage::
image(void) const
{
  if (virtual_buffer().finish_issued())
    {
      return virtual_buffer().fetch_image();
    }
  else
    {
      return nullptr;
    }
}

astral::reference_counted_ptr<astral::Image>
astral::RenderEncoderImage::
image_with_mips(unsigned int max_lod) const
{
  if (virtual_buffer().finish_issued())
    {
      return virtual_buffer().image_with_mips(max_lod);
    }
  else
    {
      return nullptr;
    }
}

astral::reference_counted_ptr<astral::Image>
astral::RenderEncoderImage::
image_last_mip_only(unsigned int LOD, unsigned int *actualLOD) const
{
  if (virtual_buffer().finish_issued())
    {
      return virtual_buffer().image_last_mip_only(LOD, actualLOD);
    }
  else
    {
      return nullptr;
    }
}

const astral::ScaleTranslate&
astral::RenderEncoderImage::
image_transformation_pixel(void) const
{
  return virtual_buffer().image_transformation_pixel();
}

astral::reference_counted_ptr<const astral::RenderClipElement>
astral::RenderEncoderImage::
clip_element(enum mask_type_t mask_type,
             enum mask_channel_t mask_channel) const
{
  if (virtual_buffer().finish_issued())
    {
      return virtual_buffer().clip_element(mask_type, mask_channel);
    }
  else
    {
      return nullptr;
    }
}

//////////////////////////////////////////
// astral::RenderEncoderMask methods
void
astral::RenderEncoderMask::
add_path_strokes(const MaskStrokeShader &shader,
                 const CombinedPath &paths,
                 const StrokeParameters &params,
                 const StrokeShader::ItemDataPackerBase &packer,
                 const ItemMask &clip,
                 enum mask_item_shader_clip_mode_t clip_mode) const
{
  /* TODO. Likely best to add a static method to Renderer::Implement::StokeBuilder */
  ASTRALunused(shader);
  ASTRALunused(paths);
  ASTRALunused(params);
  ASTRALunused(packer);
  ASTRALunused(clip);
  ASTRALunused(clip_mode);
}

///////////////////////////////////////////////////
// astral::RenderEncoderShadowMap methods
astral::reference_counted_ptr<astral::ShadowMap>
astral::RenderEncoderShadowMap::
finish(void) const
{
  virtual_buffer().issue_finish();
  return virtual_buffer().shadow_map();
}

void
astral::RenderEncoderShadowMap::
add_path(const CombinedPath &paths, bool include_implicit_closing_edge) const
{
  detail::add_shadowmap_path_implement<Path>(*this, paths, include_implicit_closing_edge);
  detail::add_shadowmap_path_implement<AnimatedPath>(*this, paths, include_implicit_closing_edge);
}

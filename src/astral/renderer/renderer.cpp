/*!
 * \file renderer.cpp
 * \brief file renderer.cpp
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
#include "renderer_filler_curve_clipping.hpp"
#include "renderer_filler_line_clipping.hpp"
#include "renderer_filler_non_sparse.hpp"


/* Renderer Overview
 * -----------------
 *
 * I. DrawCommand
 *   Because of the need to delay drawing until end, we do not send
 *   vertex and index data directly to the RenderBackend when
 *   a draw command is encountered. Instead we save the command,
 *   which consists of mostly handles, that specifies the draw command
 *   for later processing.
 *
 * II. Offscreen Rendering
 *   The key optimization for Renderer is atlas'ed offscreen rendering
 *   where multiple virtual offscreen buffers are drawn to a single
 *   3D API render target. A single virtual buffer is embodied by
 *   an instance of VirtualBuffer. To inherit clipping and to reduce
 *   the pixel rect of a virtual buffer, the class ClipGeometry is
 *   employed. It also computes the transformation from pixel coordinates
 *   of the virtual buffer region to the image coordindates of the Image
 *   that backs the virtual buffer for reading. When RenderEncoderBase::finish()
 *   is called, it will issue VirtualBuffer::issue_finish() which will mark
 *   that the VirtualBuffer is "done" and will no longer accept commands.
 *
 * III. Offscreen Rendering dependency
 *   Each VirtualBuffer needs to track on what VirtualBuffer content
 *   needs to be ready before it can render. The tracking is as follows:
 *   each VirtualBuffer has a list of those VirtualBuffers that DEPEND
 *   on it. This list can have duplicates. In addition each VirtualBuffer
 *   has a counter giving the number of VirtualBuffers on which it depends,
 *   dependencies are counted with duplication. When a VirtualBuffer is
 *   rendered, it decrements the counter on each VirtualBuffer in its list,
 *   with repetition. Thus, a VirtualBuffer has all of its dependencies
 *   ready if its counter is zero.
 *
 * IV. Just-in-time ImageAltas color tile allocation and release
 *   In VirtualBuffer, a special ctor is used to construct the Image objects
 *   that marks them as contents being generarted by Renderer. This marking
 *   also signals to Image that the color tiles on ImageAtlas::color_backing()
 *   are NOT allocated until Image::mark_as_usual_image() is called. In addition,
 *   VirtualBuffer::m_image is set to nullptr as soon as all the VirtualBuffer's
 *   that use it are rendered. If those were the only references to m_image, then
 *   the color tiles on ImageAtlas::color_backing() are released and can be reused
 *   for other offscreen renders within the frame. For some extreme scenes this
 *   can result in a MASSIVE reduction on the needed size of the color backing
 *   of ImageAtlas. The main catch is that through the entire stack of Renderer
 *   and RenderBacked reference to Image and ImageMipElement objects are not saved
 *   (otherwise the color tiles are not released for resuse) and that the location
 *   of the color tiles is NOT needed to be known on CPU either (except by Image's
 *   implementation of blitting the contents of the scratch RenderTarget to the
 *   tiles of the Image). For Image objects coming from the caller, VirtualBufer
 *   calls Image::mark_in_use() which prevents the GPU backing of Image objects
 *   from being released even if the caller's last reference goes out of scope.
 *
 * V. Preventing malloc/free noise.
 *   Renderer has a large number of std::vector<>'s where the various
 *   data values are held. Recall that on std::vector::clear(),
 *   typically memory is not free'd, only dtors are called. So we
 *   make sure that the dtor's of T for those std::vector<T> do
 *   not free any memory (and likewise the ctor's do not allocate).
 *   This is solved by the classes Backing and Storage.
 */

class astral::Renderer::Implement::ScratchRenderTarget
{
public:
  void
  init(ivec2 dims, RenderEngine &engine)
  {
    m_render_target = engine.create_render_target(dims, &m_color_buffer, &m_ds_buffer);
  }

  const reference_counted_ptr<RenderTarget>&
  render_target(void) const
  {
    return m_render_target;
  }

  ColorBuffer&
  color_buffer(void) const
  {
    return *m_color_buffer;
  }

  DepthStencilBuffer&
  depth_stencil_buffer(void) const
  {
    return *m_ds_buffer;
  }

private:
  reference_counted_ptr<RenderTarget> m_render_target;
  reference_counted_ptr<ColorBuffer> m_color_buffer;
  reference_counted_ptr<DepthStencilBuffer> m_ds_buffer;
};

//////////////////////////////////
// astral::Renderer::OffscreenBufferAllocInfo methods
astral::ivec2
astral::Renderer::OffscreenBufferAllocInfo::
session_smallest_size(void)
{
  return ivec2(VirtualBuffer::render_scratch_buffer_size, 1);
}

astral::ivec2
astral::Renderer::OffscreenBufferAllocInfo::
session_largest_size(void)
{
  return ivec2(VirtualBuffer::render_scratch_buffer_size, VirtualBuffer::render_scratch_buffer_size);
}

/////////////////////////////////
// astral::Renderer methods
astral::reference_counted_ptr<astral::Renderer>
astral::Renderer::
create(RenderEngine &engine)
{
  return ASTRALnew Renderer::Implement(engine);
}

astral::Renderer::Implement&
astral::Renderer::
implement(void)
{
  return *static_cast<Implement*>(this);
}

const astral::Renderer::Implement&
astral::Renderer::
implement(void) const
{
  return *static_cast<const Implement*>(this);
}

astral::RenderBackend&
astral::Renderer::
backend(void)
{
  return *implement().m_backend;
}

astral::RenderEngine&
astral::Renderer::
render_engine(void) const
{
  return *implement().m_engine;
}

astral::c_array<const unsigned int>
astral::Renderer::
last_stats(void) const
{
  return make_c_array(implement().m_stats);
}

astral::c_array<const astral::c_string>
astral::Renderer::
stats_labels(void) const
{
  return make_c_array(implement().m_stat_labels);
}

void
astral::Renderer::
overridable_properties(const RenderEngine::OverridableProperties &props)
{
  ASTRALassert(!implement().m_backend->rendering());
  implement().m_properties.m_overridable_properties = props;
}

const astral::RenderEngine::OverridableProperties&
astral::Renderer::
overridable_properties(void) const
{
  return implement().m_properties.m_overridable_properties;
}

bool
astral::Renderer::
custom_draw_can_overdraw_itself(bool emits_partially_covered_fragments, enum blend_mode_t blend_mode) const
{
  BackendBlendMode bb(emits_partially_covered_fragments, blend_mode);
  return implement().m_properties.m_blend_mode_information.requires_framebuffer_pixels(bb) == BlendModeInformation::does_not_need_framebuffer_pixels;
}

unsigned int
astral::Renderer::
stat_index(enum renderer_stats_t st) const
{
  return st;
}

unsigned int
astral::Renderer::
stat_index(enum RenderBackend::render_backend_stats_t st) const
{
  return number_renderer_stats + implement().m_backend->stat_index(st);
}

unsigned int
astral::Renderer::
stat_index(RenderBackend::DerivedStat st) const
{
  return number_renderer_stats + implement().m_backend->stat_index(st);
}

void
astral::Renderer::
default_render_accuracy(float v)
{
  const float min_accuracy(1.0f / (1024.0f * 1024.0f * 1024.0f));
  implement().m_default_render_accuracy = t_max(v, min_accuracy);
}

float
astral::Renderer::
default_render_accuracy(void) const
{
  return implement().m_default_render_accuracy;
}

void
astral::Renderer::
default_use_pixel_rect_tile_culling(bool b)
{
  implement().m_default_use_pixel_rect_tile_culling = b;
}

bool
astral::Renderer::
default_use_pixel_rect_tile_culling(void) const
{
  return implement().m_default_use_pixel_rect_tile_culling;
}

void
astral::Renderer::
begin(enum colorspace_t c)
{
  implement().begin_implement(c);
}

astral::c_array<const unsigned int>
astral::Renderer::
end(OffscreenBufferAllocInfo *out_alloc_info)
{
  return implement().end_implement(out_alloc_info);
}

astral::c_array<const unsigned int>
astral::Renderer::
end_abort(void)
{
  return implement().end_abort_implement();
}

astral::RenderEncoderSurface
astral::Renderer::
encoder_surface(RenderTarget &rt, enum colorspace_t colorspace, u8vec4 clear_color)
{
  RenderValue<Brush> clear_brush;
  RenderEncoderBase return_value;

  ASTRALassert(implement().m_backend->rendering());
  ASTRALassert(rt.has_color_buffer());
  if (clear_color != u8vec4(0, 0, 0, 0))
    {
      Brush brush;

      brush.base_color(vec4(clear_color) / 255.0f, colorspace);
      clear_brush = create_value(brush);
    }
  else
    {
      clear_brush = RenderValue<Brush>();
    }

  rt.active_status(detail::RenderTargetRendererStatus(this));
  return_value = implement().m_storage->create_virtual_buffer(VB_TAG, rt, clear_color, colorspace, clear_brush);

  implement().m_virtual_buffer_to_render_target.push_back(RenderEncoderSurface(return_value.m_virtual_buffer));
  return implement().m_virtual_buffer_to_render_target.back();
}

void
astral::Renderer::
encoders_surface(RenderTarget &rt,
                 c_array<const SubViewport> in_regions,
                 c_array<RenderEncoderSurface> out_encoders,
                 enum colorspace_t colorspace,
                 u8vec4 clear_color)
{
  RenderValue<Brush> clear_brush;

  ASTRALassert(in_regions.size() == out_encoders.size());
  ASTRALassert(implement().m_backend->rendering());
  ASTRALassert(rt.has_color_buffer());

  if (in_regions.empty())
    {
      return;
    }

  if (clear_color != u8vec4(0, 0, 0, 0))
    {
      Brush brush;

      brush.base_color(vec4(clear_color) / 255.0f, colorspace);
      clear_brush = create_value(brush);
    }
  else
    {
      clear_brush = RenderValue<Brush>();
    }

  range_type<unsigned int> same_surface_range;

  same_surface_range.m_begin = implement().m_virtual_buffer_to_render_target_subregion.size();
  rt.active_status(detail::RenderTargetRendererStatus(this));
  for (unsigned int i = 0; i < in_regions.size(); ++i)
    {
      RenderEncoderBase R;

      R = implement().m_storage->create_virtual_buffer(VB_TAG, rt, clear_color, colorspace, clear_brush, &in_regions[i]);
      out_encoders[i] = RenderEncoderSurface(R.m_virtual_buffer);
      implement().m_virtual_buffer_to_render_target_subregion.push_back(R.m_virtual_buffer->render_index());
    }
  same_surface_range.m_end = implement().m_virtual_buffer_to_render_target_subregion.size();
  implement().m_virtual_buffer_to_render_target_subregion_same_surface.push_back(same_surface_range);
}

astral::RenderEncoderMask
astral::Renderer::
encoder_mask(ivec2 size)
{
  ASTRALassert(implement().m_backend->rendering());
  return implement().m_storage->create_virtual_buffer(VB_TAG, size, number_fill_rule,
                                                      Renderer::VirtualBuffer::ImageCreationSpec());
}

astral::RenderEncoderImage
astral::Renderer::
encoder_image(ivec2 size, enum colorspace_t colorspace)
{
  ASTRALassert(implement().m_backend->rendering());
  return implement().m_storage->create_virtual_buffer(VB_TAG, size, colorspace,
                                                      Renderer::VirtualBuffer::ImageCreationSpec());
}

astral::RenderEncoderImage
astral::Renderer::
encoder_image(ivec2 size)
{
  enum colorspace_t colorspace;

  ASTRALassert(implement().m_backend->rendering());
  colorspace = implement().m_default_encoder_image_colorspace;
  return implement().m_storage->create_virtual_buffer(VB_TAG, size, colorspace,
                                                      Renderer::VirtualBuffer::ImageCreationSpec());
}

////////////////////////////////////////
// astral::Renderer::Implement methods
astral::Renderer::Implement::
Implement(RenderEngine &engine):
  m_default_render_accuracy(0.5f),
  m_default_use_pixel_rect_tile_culling(false),
  m_engine(&engine),
  m_properties(engine.properties()),
  m_begin_cnt(0u),
  m_default_encoder_image_colorspace(colorspace_srgb)
{
  ASTRALassert(m_engine);
  m_default_shaders = m_engine->default_shaders();
  m_default_effects = m_engine->default_effects();
  m_backend = m_engine->create_backend();
  m_storage = ASTRALnew Storage(*this);
  m_workroom = ASTRALnew WorkRoom(*m_backend);
  m_filler[fill_method_no_sparse] = ASTRALnew Filler::NonSparse(*this);
  m_filler[fill_method_sparse_line_clipping] = ASTRALnew Filler::LineClipper(*this);
  m_filler[fill_method_sparse_curve_clipping] = ASTRALnew Filler::CurveClipper(*this);

  m_num_backend_stats = m_backend->render_stats_size();
  m_stats.resize(m_num_backend_stats + number_renderer_stats, 0);
  m_stat_labels.resize(m_num_backend_stats + number_renderer_stats);
  for (unsigned int i = 0; i < m_num_backend_stats; ++i)
    {
      m_stat_labels[i + number_renderer_stats] = m_backend->render_stats_label(i);
    }
  m_stat_labels[number_virtual_buffers] = "renderer_number_virtual_buffers";
  m_stat_labels[number_non_degenerate_virtual_buffers] = "renderer_number_non_degenerate_virtual_buffers";
  m_stat_labels[number_non_degenerate_color_virtual_buffers] = "renderer_number_non_degenerate_color_virtual_buffers";
  m_stat_labels[number_non_degenerate_mask_virtual_buffers] = "renderer_number_non_degenerate_mask_virtual_buffers";
  m_stat_labels[number_non_degenerate_shadowmap_virtual_buffers] = "renderer_number_non_degenerate_shadowmap_virtual_buffers";
  m_stat_labels[number_emulate_framebuffer_fetches] = "renderer_number_emulate_framebuffer_fetches";
  m_stat_labels[number_color_virtual_buffer_pixels] = "renderer_number_color_virtual_buffer_pixels";
  m_stat_labels[number_skipped_color_buffer_pixels] = "renderer_number_skipped_color_buffer_pixels";
  m_stat_labels[number_mask_virtual_buffer_pixels] = "renderer_number_mask_virtual_buffer_pixels";
  m_stat_labels[number_virtual_buffer_pixels] = "renderer_number_virtual_buffer_pixels";
  m_stat_labels[number_virtual_buffer_backing_allocation_failed] = "renderer_number_virtual_buffer_backing_allocation_failed";
  m_stat_labels[number_tiles_skipped_from_sparse_filling] = "renderer_number_tiles_skipped_from_sparse_filling";
  m_stat_labels[number_pixels_blitted] = "renderer_number_pixels_blitted";
  m_stat_labels[number_offscreen_render_targets] = "renderer_number_offscreen_render_targets";
  m_stat_labels[number_vertices_streamed] = "renderer_number_vertices_streamed";
  m_stat_labels[number_static_u32vec4_streamed] = "renderer_number_static_u32vec4_streamed";
  m_stat_labels[number_static_u16vec4_streamed] = "renderer_number_static_u16vec4_streamed";
  m_stat_labels[number_commands_copied] = "renderer_number_commands_copied";
  m_stat_labels[number_sparse_fill_curves_mapped] = "renderer_sparse_fill_number_curves_mapped";
  m_stat_labels[number_sparse_fill_contours_mapped] = "renderer_sparse_fill_number_contours_mapped";
  m_stat_labels[number_sparse_fill_curves_clipped] = "renderer_sparse_fill_number_curves_clipped";
  m_stat_labels[number_sparse_fill_contours_clipped] = "renderer_sparse_fill_number_contours_clipped";
  m_stat_labels[number_sparse_fill_late_culled_contours] = "renderer_sparse_fill_number_late_culled_contours";
  m_stat_labels[number_sparse_fill_subrects_clipping] = "renderer_sparse_fill_number_subrects_clipping";
  m_stat_labels[number_sparse_fill_subrect_skip_clipping] = "renderer_sparse_fill_number_subrect_skip_clipping";
  m_stat_labels[number_sparse_fill_contour_skip_clipping] = "renderer_sparse_fill_number_contour_skip_clipping";
  m_stat_labels[number_sparse_fill_awkward_fully_clipped_or_unclipped] = "renderer_sparse_fill_number_awkward_fully_clipped_or_unclipped";
}

bool
astral::Renderer::Implement::
pre_process_command(bool render_to_color_buffer, DrawCommand &cmd)
{
  if (!render_to_color_buffer)
    {
      /* when rendering to a mask or shadow map, there is
       * no opaque draws or occluders; draws are sorted
       * purely by shader
       */
      ASTRALassert(cmd.m_render_values.m_blend_mode.item_shader_type() != ItemShader::color_item_shader);
      ASTRALassert(cmd.m_render_values.m_blend_mode.blend_mode() == number_blend_modes);
      return false;
    }

  ASTRALassert(cmd.m_render_values.m_blend_mode.item_shader_type() == ItemShader::color_item_shader);
  ASTRALassert(cmd.m_render_values.m_blend_mode.blend_mode() < number_blend_modes);

  bool emits_partial_coverage, is_opaque;

  /* set if the command emits partially covered pixels */
  emits_partial_coverage = cmd.m_render_values.m_material.emits_partial_coverage()
    || cmd.an_item_shader_emits_partially_covered_fragments()
    || cmd.m_render_values.m_clip_mask.valid();

  cmd.m_render_values.m_blend_mode = BackendBlendMode(cmd.m_render_values.m_blend_mode.blend_mode(), emits_partial_coverage);

  /* if a command's blend use framebuffer emulation and does
   * not use GPU fixed function blending, the commands can
   * be drawn as an occluder.
   */
  const BlendModeInformation &info(m_properties.m_blend_mode_information);
  enum BlendModeInformation::requires_framebuffer_pixels_t pp;

  pp = info.requires_framebuffer_pixels(cmd.m_render_values.m_blend_mode);
  if (pp == BlendModeInformation::requires_framebuffer_pixels_opaque_draw)
    {
      return true;
    }

  is_opaque = !cmd.m_render_values.m_blend_mode.emits_partial_coverage()
    && (cmd.m_render_values.m_blend_mode.blend_mode() == blend_porter_duff_src_over
        || cmd.m_render_values.m_blend_mode.blend_mode() == blend_porter_duff_src);

  if (cmd.m_render_values.m_blend_mode.blend_mode() == blend_porter_duff_src_over)
    {
      is_opaque = is_opaque
        && !cmd.an_item_shader_emits_transparent_fragments()
        && !cmd.m_render_values.m_material.emits_transparent_fragments();
    }

  if (is_opaque && cmd.m_render_values.m_blend_mode.blend_mode() == blend_porter_duff_src_over)
    {
      /* if it is opaque, then we can draw with blend_porter_duff_src
       *
       * NOTE: this assumes that the backend's implementation of blending
       *       does not need to emulate famebuffer fetch on the blend
       *       mode blend_porter_duff_src when fragments are fully
       *       covered.
       */
      cmd.m_render_values.m_blend_mode = BackendBlendMode(blend_porter_duff_src, false);
    }

  return is_opaque;
}

astral::RenderBackend::ClipWindowValue
astral::Renderer::Implement::
create_clip_window(vec2 min_corner, vec2 size)
{
  RenderBackend::ClipWindowValue return_value;
  if (m_properties.m_overridable_properties.m_clip_window_strategy != clip_window_strategy_depth_occlude)
    {
      ClipWindow eq;

      eq.m_values
        .min_point(min_corner)
        .max_point(min_corner + size);

      return_value.m_clip_window = m_backend->create_value(eq);
      return_value.m_enforce = (m_properties.m_overridable_properties.m_clip_window_strategy == clip_window_strategy_shader);
    }

  ASTRALassert(return_value.clip_window_value_type() == compute_shader_clipping());
  return return_value;
}

enum astral::clip_window_value_type_t
astral::Renderer::Implement::
compute_shader_clipping(void)
{
  switch (m_properties.m_overridable_properties.m_clip_window_strategy)
    {
    case clip_window_strategy_shader:
      return clip_window_present_enforce;

    case clip_window_strategy_depth_occlude:
      return clip_window_not_present;

    case clip_window_strategy_depth_occlude_hinted:
      return clip_window_present_optional;
    }
}

void
astral::Renderer::Implement::
begin_implement(enum colorspace_t c)
{
  ASTRALassert(m_storage->number_virtual_buffers() == 0u);

  m_default_encoder_image_colorspace = c;

  std::fill(m_stats.begin(), m_stats.end(), 0u);
  m_engine->image_atlas().lock_resources();
  m_engine->colorstop_sequence_atlas().lock_resources();
  m_engine->vertex_data_allocator().lock_resources();
  m_engine->static_data_allocator32().lock_resources();
  m_engine->static_data_allocator16().lock_resources();

  /* send message to backend that now rendering begins, we need
   * to do this so that m_backend can create valid RenderValue
   */
  m_backend->begin();

  /* We delay creating m_dynamic_rect until the first
   * begin()/end() pair to make sure that the 3D API
   * state is correct for vertex upload.
   */
  if (!m_dynamic_rect)
    {
      m_dynamic_rect = DynamicRectShader::create_rect(*m_engine);

      /* We also delay creating the streamers until the first
       * time as well
       */
      ASTRALassert(!m_vertex_streamer);
      ASTRALassert(!m_static_streamer);
      ASTRALassert(!m_static_streamer_fp16);

      /* the VertexData objects for streaming must have their size as a multiple of
       * three, guessing that 16184 * 3 is a nice size
       */
      unsigned int vertex_data_streamer_size(16184u * 3u);
      m_vertex_streamer = ASTRALnew VertexStreamer(*m_engine, vertex_data_streamer_size);

      /* this value is just a guess as to what is a good idea to use */
      unsigned int gvec4_data_streamer_size(16184u);
      m_static_streamer = ASTRALnew StaticStreamer32(*m_engine, gvec4_data_streamer_size);
      m_static_streamer_fp16 = ASTRALnew StaticStreamer16(*m_engine, gvec4_data_streamer_size);
    }

  /* create some common values needed during a render-frame */
  m_identity = create_value(Transformation());
  Brush black, white;

  black.m_base_color = vec4(0.0f, 0.0f, 0.0f, 0.0f);
  white.m_base_color = vec4(1.0f, 1.0f, 1.0f, 1.0f);

  m_black_brush = create_value(black);
  m_white_brush = create_value(white);

  m_vertex_streamer->begin();
  m_static_streamer->begin();
  m_static_streamer_fp16->begin();
}

void
astral::Renderer::Implement::
render_stc_virtual_buffers_cover_pass(c_array<const unsigned int> buffers)
{
  const MaskItemShader &cover_shader(*m_default_shaders.m_stc_shader.m_cover_shader);
  astral::RenderValues R;
  vecN<gvec4, DynamicRectShader::item_data_size> rect_data;

  R.m_transformation = m_identity;
  R.m_blend_mode = BackendBlendMode(BackendBlendMode::mask_mode_rendering);
  for (unsigned int b : buffers)
    {
      const VirtualBuffer &buffer(m_storage->virtual_buffer(b));
      DynamicRectShader::pack_item_data(buffer.pixel_rect().as_rect(), rect_data);

      R.m_item_data = create_item_data(rect_data, no_item_data_value_mapping);
      m_backend->draw_render_data(buffer.start_z(), cover_shader, R,
                                  RenderBackend::UberShadingKey::Cookie(),
                                  buffer.render_scale_translate(),
                                  buffer.clip_window(),
                                  buffer.permute_xy_when_rendering(),
                                  m_dynamic_rect->vertex_range());
    }
}

void
astral::Renderer::Implement::
render_stc_virtual_buffers_pass(c_array<const unsigned int>::iterator begin,
                                c_array<const unsigned int>::iterator end,
                                enum FillSTCShader::pass_t pass, const ItemShader &shader)
{
  astral::RenderValues R;

  R.m_blend_mode = BackendBlendMode(BackendBlendMode::mask_mode_rendering);
  for (c_array<const unsigned int>::iterator iter = begin; iter != end; ++iter)
    {
      unsigned int b(*iter);
      const VirtualBuffer &buffer(m_storage->virtual_buffer(b));
      c_array<const STCData> stc_data(buffer.stc_data_values(pass));

      for (const STCData &stc : stc_data)
        {
          R.m_item_data = stc.item_data();
          R.m_transformation = stc.transformation();
          m_backend->draw_render_data(buffer.start_z(), shader, R,
                                      RenderBackend::UberShadingKey::Cookie(),
                                      buffer.render_scale_translate(),
                                      buffer.clip_window(),
                                      buffer.permute_xy_when_rendering(),
                                      stc.ranges());
        }
    }
}

void
astral::Renderer::Implement::
render_stc_prepare_pass(c_array<const unsigned int>::iterator begin,
                        c_array<const unsigned int>::iterator end)
{
  /* stencil prepare pass; color writes are off and
   * stencil is set to increment with clockwise
   * triangles and decrement with counter-clockwise
   * triangles. Once the stencil prepare pass is
   * completed, the value if the stencil buffer at
   * each pixel is the winding number at each
   * pixel.
   */
  m_backend->color_write_mask(bvec4(false));
  m_backend->set_stencil_state(StencilState()
                               .enabled(true)
                               .func(StencilState::test_always)
                               .reference(0u)
                               .reference_mask(~0u)
                               .write_mask(~0u)
                               .stencil_fail_op(StencilState::op_keep)
                               .stencil_pass_depth_fail_op(StencilState::op_keep)
                               .stencil_pass_depth_pass_op(StencilState::op_incr_wrap, StencilState::face_cw)
                               .stencil_pass_depth_pass_op(StencilState::op_decr_wrap, StencilState::face_ccw));

  render_stc_virtual_buffers_pass(begin, end,
                                  FillSTCShader::pass_contour_stencil,
                                  *m_default_shaders.m_stc_shader.m_shaders[FillSTCShader::pass_contour_stencil]);

  render_stc_virtual_buffers_pass(begin, end,
                                  FillSTCShader::pass_conic_triangles_stencil,
                                  *m_default_shaders.m_stc_shader.m_shaders[FillSTCShader::pass_conic_triangles_stencil]);
}

void
astral::Renderer::Implement::
render_stc_cover_pass(void)
{
  // enable color writes to .r channel only when doing STC cover pass
  m_backend->color_write_mask(bvec4(true, false, false, false));

  // cover pass for non-zero
  if (!m_workroom->m_by_fill_rule[nonzero_fill_rule].empty())
    {
      m_backend->set_stencil_state(StencilState()
                                   .enabled(true)
                                   .func(StencilState::test_not_equal)
                                   .reference(0u)
                                   .reference_mask(~0u)
                                   .write_mask(~0u)
                                   .stencil_fail_op(StencilState::op_keep)
                                   .stencil_pass_depth_fail_op(StencilState::op_keep)
                                   .stencil_pass_depth_pass_op(StencilState::op_keep));
      render_stc_virtual_buffers_cover_pass(make_c_array(m_workroom->m_by_fill_rule[nonzero_fill_rule]));
    }

  // cover pass for complement-non-zero
  if (!m_workroom->m_by_fill_rule[complement_nonzero_fill_rule].empty())
    {
      m_backend->set_stencil_state(StencilState()
                                   .enabled(true)
                                   .func(StencilState::test_equal)
                                   .reference(0u)
                                   .reference_mask(~0u)
                                   .write_mask(~0u)
                                   .stencil_fail_op(StencilState::op_keep)
                                   .stencil_pass_depth_fail_op(StencilState::op_keep)
                                   .stencil_pass_depth_pass_op(StencilState::op_keep));
      render_stc_virtual_buffers_cover_pass(make_c_array(m_workroom->m_by_fill_rule[complement_nonzero_fill_rule]));
    }

  // cover pass for odd-even
  if (!m_workroom->m_by_fill_rule[odd_even_fill_rule].empty())
    {
      m_backend->set_stencil_state(StencilState()
                                   .enabled(true)
                                   .func(StencilState::test_not_equal)
                                   .reference(0u)
                                   .reference_mask(1u)
                                   .write_mask(~0u)
                                   .stencil_fail_op(StencilState::op_keep)
                                   .stencil_pass_depth_fail_op(StencilState::op_keep)
                                   .stencil_pass_depth_pass_op(StencilState::op_keep));
      render_stc_virtual_buffers_cover_pass(make_c_array(m_workroom->m_by_fill_rule[odd_even_fill_rule]));
    }

  // cover pass for complement odd-even
  if (!m_workroom->m_by_fill_rule[complement_odd_even_fill_rule].empty())
    {
      m_backend->set_stencil_state(StencilState()
                                   .enabled(true)
                                   .func(StencilState::test_equal)
                                   .reference(0u)
                                   .reference_mask(1u)
                                   .write_mask(~0u)
                                   .stencil_fail_op(StencilState::op_keep)
                                   .stencil_pass_depth_fail_op(StencilState::op_keep)
                                   .stencil_pass_depth_pass_op(StencilState::op_keep));
      render_stc_virtual_buffers_cover_pass(make_c_array(m_workroom->m_by_fill_rule[complement_odd_even_fill_rule]));
    }
}

void
astral::Renderer::Implement::
render_stc_aa_virtual_buffers(c_array<const unsigned int>::iterator begin,
                              c_array<const unsigned int>::iterator end)
{
  /* Draw the anti-alias fuzz, this is to be drawn with color
   * write on and stencil test off.
   */
  render_stc_virtual_buffers_pass(begin, end,
                                  FillSTCShader::pass_conic_triangle_fuzz,
                                  *m_default_shaders.m_stc_shader.m_shaders[FillSTCShader::pass_conic_triangle_fuzz]);

  render_stc_virtual_buffers_pass(begin, end,
                                  FillSTCShader::pass_contour_fuzz,
                                  *m_default_shaders.m_stc_shader.m_shaders[FillSTCShader::pass_contour_fuzz]);
}

void
astral::Renderer::Implement::
render_stc_virtual_buffers(c_array<const unsigned int>::iterator begin,
                           c_array<const unsigned int>::iterator end)
{
  /* prepare arrays to quickly walk through the buffers that
   * have STC applied to them.
   */
  ASTRALassert(begin != end);
  ASTRALassert(m_workroom->m_by_fill_rule[odd_even_fill_rule].empty());
  ASTRALassert(m_workroom->m_by_fill_rule[nonzero_fill_rule].empty());
  ASTRALassert(m_workroom->m_by_fill_rule[complement_odd_even_fill_rule].empty());
  ASTRALassert(m_workroom->m_by_fill_rule[complement_nonzero_fill_rule].empty());

  bool added(false);
  for (c_array<const unsigned int>::iterator i = begin; i != end; ++i)
    {
      unsigned int b(*i);
      const VirtualBuffer &buffer(m_storage->virtual_buffer(b));

      ASTRALassert(buffer.command_list() && buffer.command_list()->renders_to_mask_buffer());
      if (buffer.stc_fill_rule() != number_fill_rule)
        {
          m_workroom->m_by_fill_rule[buffer.stc_fill_rule()].push_back(b);
          added = true;
        }
      ++m_stats[number_non_degenerate_mask_virtual_buffers];
    }

  if (!added)
    {
      ASTRALassert(m_workroom->m_by_fill_rule[odd_even_fill_rule].empty());
      ASTRALassert(m_workroom->m_by_fill_rule[nonzero_fill_rule].empty());
      ASTRALassert(m_workroom->m_by_fill_rule[complement_odd_even_fill_rule].empty());
      ASTRALassert(m_workroom->m_by_fill_rule[complement_nonzero_fill_rule].empty());

      return;
    }

  /* step 1: setup the stencil buffer for the cover pass */
  render_stc_prepare_pass(begin, end);

  /* step 2: perform the cover pass */
  render_stc_cover_pass();

  m_workroom->m_by_fill_rule[odd_even_fill_rule].clear();
  m_workroom->m_by_fill_rule[nonzero_fill_rule].clear();
  m_workroom->m_by_fill_rule[complement_odd_even_fill_rule].clear();
  m_workroom->m_by_fill_rule[complement_nonzero_fill_rule].clear();
}

void
astral::Renderer::Implement::
render_virtual_buffers(OffscreenBufferAllocInfo *tracker,
                       c_array<const unsigned int> in_image_buffers,
                       c_array<const unsigned int> in_shadow_map_buffers,
                       enum render_virtual_buffer_mode_t mode)
{
  ASTRALhard_assert(!in_image_buffers.empty() || !in_shadow_map_buffers.empty());

  /* when rendering directly, only color buffer rendering is supported */
  ASTRALassert(mode == render_virtual_buffers_blit_atlas || in_shadow_map_buffers.empty());

  /* the backend should have a render target bound if and only if rendering directly */
  ASTRALassert((mode == render_virtual_buffers_directly) == (m_backend->current_render_target().get() != nullptr));

  /* Signal to the image buffers that they are about to be rendered
   *
   * TODO: instead of having it here in render_virtual_buffers(), we
   *       really should have it in ImageBufferList::choose_ready_buffers()
   *       and ShadowMapBufferList::choose_ready_buffers() so that if the
   *       backing to an Image (or ShadowMap) cannot be realized, then the
   *       rendering area is not allocated. The basic strategy would be the
   *       following:
   *
   *   Add to LayeredRectAtlas to allocate space in two steps: first
   *   query if possible then allocate with the restriction that only
   *   previous query can be allocated. Then have ImageBufferList do
   *   the following:
   *     1) check if the region needed can be allocated
   *     2) if so, call about_to_render_content() on the VirtualBuffer
   *     3) only allocate if about_to_render_content() returns success.
   *   The work needed on LayeredRectAtlas is actually pretty simple,
   *   since it gets a BucketEntry that can allocate the space and then
   *   allocates it; so the return value of (1) would be that BucketEntry
   *   and the restriction that only the previous query can be allocated
   *   will be fine.
   */
  m_workroom->m_renderable_image_buffers.clear();
  for (unsigned int b : in_image_buffers)
    {
      VirtualBuffer &vb(m_storage->virtual_buffer(b));
      if (routine_success == vb.about_to_render_content())
        {
          m_workroom->m_renderable_image_buffers.push_back(b);
        }
    }
  c_array<unsigned int> image_buffers;
  image_buffers = make_c_array(m_workroom->m_renderable_image_buffers);

  m_workroom->m_renderable_shadowmap_buffers.clear();
  for (unsigned int b : in_shadow_map_buffers)
    {
      VirtualBuffer &vb(m_storage->virtual_buffer(b));
      if (routine_success == vb.about_to_render_content())
        {
          m_workroom->m_renderable_shadowmap_buffers.push_back(b);
        }
    }
  c_array<unsigned int> shadow_map_buffers;
  shadow_map_buffers = make_c_array(m_workroom->m_renderable_shadowmap_buffers);

  const ScratchRenderTarget *scratch_rt = nullptr;
  if (mode == render_virtual_buffers_blit_atlas)
    {
      /* (1) instead of having a single render target, we have an array
       *     of render targets where the widths of each render target
       *     is VirtualBuffer::render_scratch_buffer_size and the height
       *     of the i'th target is VirtualBuffer::render_scratch_buffer_size / 2^i
       * (2) compute the bounding box of all the rects of the buffers
       *     to be rendered and pick the smallest render target that
       *     contains them all.
       */
      BoundingBox<int> scratch_area;

      for (unsigned int b : image_buffers)
        {
          m_storage->virtual_buffer(b).add_scratch_area(&scratch_area);
        }

      for (unsigned int b : shadow_map_buffers)
        {
          m_storage->virtual_buffer(b).add_scratch_area(&scratch_area);
        }

      /* It is not the size of the scratch area, but the max_point()
       * that matters since we have already decided the region to
       * place each VirtualBuffer and made the ScaleTranslate value
       * ready.
       */
      uint32_t which_buffer, buffer_height;

      ASTRALassert(scratch_area.as_rect().m_min_point.x() >= 0);
      ASTRALassert(scratch_area.as_rect().m_min_point.y() >= 0);
      ASTRALassert(scratch_area.as_rect().m_max_point.x() <= VirtualBuffer::render_scratch_buffer_size);
      ASTRALassert(scratch_area.as_rect().m_max_point.y() <= VirtualBuffer::render_scratch_buffer_size);

      buffer_height = scratch_area.as_rect().m_max_point.y();
      which_buffer = uint32_log2_ceiling(buffer_height);

      if (m_scratch_render_targets.size() <= which_buffer)
        {
          m_scratch_render_targets.resize(which_buffer + 1u);
        }

      if (!m_scratch_render_targets[which_buffer].render_target())
        {
          ivec2 dims;

          dims.x() = VirtualBuffer::render_scratch_buffer_size;
          dims.y() = 1u << which_buffer;

          ASTRALassert(dims.y() <= VirtualBuffer::render_scratch_buffer_size);
          m_scratch_render_targets[which_buffer].init(dims, *m_engine);
        }

      if (tracker)
        {
          tracker->begin_offscreen_session(m_scratch_render_targets[which_buffer].render_target()->size());
          for (unsigned int b : image_buffers)
            {
              VirtualBuffer &buffer(m_storage->virtual_buffer(b));
              RectT<int> R;

              R.m_min_point = buffer.location_in_color_buffer().m_location;
              R.m_max_point = R.m_min_point + buffer.offscreen_render_size();
              if (buffer.permute_xy_when_rendering())
                {
                  std::swap(R.m_min_point.x(), R.m_min_point.y());
                  std::swap(R.m_max_point.x(), R.m_max_point.y());
                }
              tracker->add_rect(R);
            }
        }

      scratch_rt = &m_scratch_render_targets[which_buffer];

      /* begin rendering to render target */
      m_backend->begin_render_target(RenderBackend::ClearParams()
                                     .clear_color(vec4(0.0f, 0.0f, 0.0f, 0.0f))
                                     .clear_depth(RenderBackend::depth_buffer_value_clear)
                                     .clear_stencil(0),
                                     *m_scratch_render_targets[which_buffer].render_target());
    }

  /* sort the buffers by VirtualBuffer so that color rendered buffers
   * come first and mask rendering buffers come last.
   */
  VirtualBuffer::FormatSorter format_sorter(*this);
  std::stable_sort(image_buffers.begin(), image_buffers.end(), format_sorter);

  /* find the first mask buffer */
  VirtualBuffer::IsMaskFormat is_mask_format(*this);
  c_array<unsigned int>::iterator iter;
  iter = std::find_if(image_buffers.begin(), image_buffers.end(), is_mask_format);

  /* when rendering directly, only color buffer rendering is supported */
  ASTRALassert(mode == render_virtual_buffers_blit_atlas || iter == image_buffers.end());

  /* if we do not use uber-shading, we sort the image buffers by
   * the first shader they used in the hopes of reducing shader
   * changes; the use case is for those offscreen renders that
   * are for downsampling or Effect renders that only draw with
   * a single shader
   */
  if (m_properties.m_overridable_properties.m_uber_shader_method == uber_shader_none)
    {
      VirtualBuffer::FirstShaderUsedSorter shader_sorter(*this);
      std::stable_sort(image_buffers.begin(), iter, shader_sorter);
    }

  /* Initialize drawing state:
   *   - stencil test off
   *   - depth test occlude (i.e. test that the depth emitted
   *     is greater than or equal to what is in the depth buffer
   *     and write to the depth buffer).
   *   - color writes off
   */
  m_backend->set_stencil_state(StencilState().enabled(false));
  m_backend->color_write_mask(bvec4(false));
  m_backend->depth_buffer_mode(RenderBackend::depth_buffer_occlude);

  /* Overview of how clip_window_strategy_depth_occlude works
   * - depth_occlude on mask buffers means that the depth test is equals and
   *   before any drawing each depth buffer has a depth-rect drawn to it so that
   *   the depth-rect is in front of ALL image buffers and is a unique value.
   *
   * - depth_occlude on color buffers means that a depth-rect is drawn on each
   *   color buffer before any drawing where the depth value of the rect increases
   *   making sure that content of buffers drawn before them do not affect pixels.
   *   Then just after finishing color rendering on a buffer, a depth-rect is drawn
   *   that covers the buffer with a depth value that is always in front.
   */

  RenderBackend::UberShadingKey &uber_key(*m_workroom->m_uber_shading_key[m_properties.m_overridable_properties.m_uber_shader_method]);
  RenderBackend::UberShadingKey::Cookie uber_key_cookie;
  unsigned int current_z = 0u;
  enum clip_window_value_type_t shader_clipping(compute_shader_clipping());
  bool depth_occlusion_performs_clipping(m_properties.m_overridable_properties.m_clip_window_strategy != clip_window_strategy_shader);

  /* set the z-values now for all virtual buffers if we are using depth occlusion */
  if (depth_occlusion_performs_clipping)
    {
      for (c_array<unsigned int>::iterator i = image_buffers.begin(); i != iter; ++i)
        {
          unsigned int b(*i);
          VirtualBuffer &buffer(m_storage->virtual_buffer(b));

          /* draw a depth rect for each color buffer with spacing between the color
           * buffes z-values enough for the color buffers' occluders and opaque
           * items.
           */
          if (i != image_buffers.begin() && depth_occlusion_performs_clipping)
            {
              /* initialize the depth buffer on each color buffer to occlude
               * all content of all color buffers that come before it.
               * Note that the value is current_z - 1; this is because
               * the occluder is to come "just before" the content of
               * the image buffer
               */
              buffer.draw_depth_rect(RenderBackend::UberShadingKey::Cookie(), current_z - 1);
            }

          /* We add 1 to the number_z() needed, because the
           * occluder above takes a slot as well.
           */
          ASTRALassert(buffer.command_list() && buffer.command_list()->renders_to_color_buffer());
          buffer.start_z(current_z);
          current_z += buffer.command_list()->number_z() + 1;
        }

      /* the mask buffers come after the color buffers, they each only
       * needs a single z-slot; when shader clipping is not used, they
       * use equality depth test.
       */
      for (c_array<unsigned int>::iterator i = iter; i != image_buffers.end(); ++i)
	{
	  unsigned int b(*i);
	  VirtualBuffer &buffer(m_storage->virtual_buffer(b));

	  ASTRALassert(buffer.command_list() && buffer.command_list()->renders_to_mask_buffer());
	  ++current_z;
	  buffer.draw_depth_rect(RenderBackend::UberShadingKey::Cookie(), current_z);
	  buffer.start_z(current_z);
	}
    }

  if (iter != image_buffers.begin())
    {
      /* Subsequence code assumes that color writes are on */
      m_backend->color_write_mask(bvec4(true));

      /* accumulate, if necessary, the uber-shader key */
      if (m_properties.m_overridable_properties.m_uber_shader_method == uber_shader_all)
        {
          uber_key.uber_shader_of_all(shader_clipping);
          uber_key_cookie = uber_key.cookie();
        }
      else if (m_properties.m_overridable_properties.m_uber_shader_method != uber_shader_none)
        {
          uber_key.begin_accumulate(shader_clipping, m_properties.m_overridable_properties.m_uber_shader_method);

          /* add the shader used for drawing a depth rect */
          VirtualBuffer::add_depth_rect_shader_to_uber(*this, uber_key);

          for (c_array<unsigned int>::iterator i = image_buffers.begin(); i != iter; ++i)
            {
              unsigned int b(*i);
              VirtualBuffer &buffer(m_storage->virtual_buffer(b));

              buffer.command_list()->accumulate_opaques_shaders(*m_storage, uber_key);
              buffer.command_list()->accumulate_typical_shaders(*m_storage, uber_key);
            }
          uber_key.end_accumulate();
          uber_key_cookie = uber_key.cookie();
        }
      else
        {
          uber_key_cookie = RenderBackend::UberShadingKey::Cookie();
        }

      /* Walk each of the image buffers. If clipping is via depth-buffer, then
       * we also need to sent the occluders and opaque draws before sending
       * the non-opaque draws.
       */
      for (c_array<unsigned int>::iterator i = image_buffers.begin(); i != iter; ++i)
        {
          unsigned int b(*i);
          VirtualBuffer &buffer(m_storage->virtual_buffer(b));

          ASTRALassert(buffer.command_list() && buffer.command_list()->renders_to_color_buffer());

          m_backend->set_fragment_shader_emit(buffer.colorspace());

          buffer.command_list()->send_occluders_to_backend(*this, uber_key_cookie, buffer.render_scale_translate(),
                                                           buffer.clip_window(), buffer.start_z(), buffer.permute_xy_when_rendering());

          buffer.command_list()->send_opaque_commands_to_backend(*this, uber_key_cookie, buffer.render_scale_translate(),
                                                                 buffer.clip_window(), buffer.start_z(), buffer.permute_xy_when_rendering());

          buffer.command_list()->send_commands_to_backend(*this, uber_key_cookie, buffer.render_scale_translate(),
                                                          buffer.clip_window(), buffer.start_z(), buffer.permute_xy_when_rendering());

          if (depth_occlusion_performs_clipping)
            {
              /* The depth buffer caps drawn at the beginning make sure that the VirtualBuffer
               * buffer does not draw to the buffers that come after it, but it does not
               * prevent later buffers from drawing of the content just rendered. To prevent
               * that, draw another depth rect with a depth value that occludes everything.
               */
              buffer.draw_depth_rect(RenderBackend::UberShadingKey::Cookie(), RenderBackend::depth_buffer_value_occlude);
            }
          ++m_stats[number_non_degenerate_color_virtual_buffers];
        }
    }

  /* If we are rendering to the scrach and then blitting to the atlases,
   * then render the masks, shadows and then blit to the atlas.
   */
  if (mode == render_virtual_buffers_blit_atlas)
    {
      ASTRALassert(scratch_rt != nullptr);

      /* any rendering after color rendering is in linear space;
       * this includes both occluders and all STC rendering
       */
      m_backend->set_fragment_shader_emit(colorspace_linear);

      if (iter != image_buffers.end())
        {
          m_backend->color_write_mask(bvec4(true));

          if (depth_occlusion_performs_clipping)
            {
              /* Clipping via depth buffer for masks is done with the
               * depth buffer equal test because mask draws have the
               * blending as max-blending which is order independent
               */
              m_backend->depth_buffer_mode(RenderBackend::depth_buffer_equal);
            }
          else
            {
              m_backend->depth_buffer_mode(RenderBackend::depth_buffer_off);
            }

          /* render the anti-alias fuzz of the fills. The rendering
           * order between the fuzz and STC does not matter because
           * the fuzz hits a different color channel than the STC.
           * It is the post-processing blit that combines the fuzz
           * with the actual coverage.
           */
          render_stc_aa_virtual_buffers(iter, image_buffers.end());

          /* render the mask buffers taking advantage that their render order does
           * not matter and thus rendering can be done completely ordered by shader.
           */
          DrawCommandList::send_commands_sorted_by_shader_to_backend(*this, iter, image_buffers.end());

          /* We delay the stencil-then-cover rendering until the
           * end because it invokes discard which can disable
           * GPU's early-z; in addition, write to the stencil
           * buffer with stencil tests which also might disable
           * early-z.
           */
          render_stc_virtual_buffers(iter, image_buffers.end());
        }

      /* render shadow maps */
      if (!shadow_map_buffers.empty())
        {
          m_backend->set_fragment_shader_emit(colorspace_linear);
          render_shadow_maps(shadow_map_buffers);
          m_stats[number_non_degenerate_shadowmap_virtual_buffers] += shadow_map_buffers.size();
        }

      /* indicate to backend that rendering to render target is done */
      m_backend->end_render_target();

      if (!image_buffers.empty())
        {
          /* Blit the contents of the rendering of scratch_rt to the Image objects */
          for (unsigned int b : image_buffers)
            {
              VirtualBuffer &virtual_buffer(m_storage->virtual_buffer(b));
              virtual_buffer.render_performed(&scratch_rt->color_buffer());
            }
          m_engine->image_atlas().flush();
        }

      if (!shadow_map_buffers.empty())
        {
          for (unsigned int b : shadow_map_buffers)
            {
              VirtualBuffer &virtual_buffer(m_storage->virtual_buffer(b));
              virtual_buffer.render_performed_shadow_map(&scratch_rt->depth_stencil_buffer());
            }
          m_engine->shadow_map_atlas().backing().flush_gpu();
        }
    }
  else
    {
      ASTRALassert(shadow_map_buffers.empty());
      ASTRALassert(scratch_rt == nullptr);

      for (unsigned int b : image_buffers)
        {
          VirtualBuffer &virtual_buffer(m_storage->virtual_buffer(b));
          virtual_buffer.render_performed(nullptr);
        }
    }
}

void
astral::Renderer::Implement::
render_non_render_target_virtual_buffers(OffscreenBufferAllocInfo *p)
{
  if (p)
    {
      p->clear();
    }

  m_workroom->m_image_buffer_list.clear();
  m_workroom->m_shadowmap_buffer_list.clear();

  for (unsigned int i = 0, endi = m_storage->number_virtual_buffers(); i < endi; ++i)
    {
      VirtualBuffer &buffer(m_storage->virtual_buffer(i));

      switch (buffer.type())
        {
        case VirtualBuffer::image_buffer:
        case VirtualBuffer::sub_image_buffer:
          {
            ASTRALassert(buffer.fetch_image());
            if (buffer.area() != 0)
              {
                ASTRALassert(buffer.area() > 0);
                ASTRALassert(buffer.command_list());
                m_workroom->m_image_buffer_list.add_buffer(i);
              }
          }
          break;

        case VirtualBuffer::shadowmap_buffer:
          if (buffer.uses_shadow_map() || buffer.remaining_dependencies() > 0)
            {
              m_workroom->m_shadowmap_buffer_list.add_buffer(i);
            }
          break;

        default:
          break;
        }
    }

  while (m_workroom->m_image_buffer_list.buffers_remaining()
         || m_workroom->m_shadowmap_buffer_list.buffers_remaining())
    {
      c_array<unsigned int> image_buffers, shadow_map_buffers;

      image_buffers = m_workroom->m_image_buffer_list.choose_ready_buffers(*this);
      shadow_map_buffers = m_workroom->m_shadowmap_buffer_list.choose_ready_buffers(*this);

      if (image_buffers.empty() && shadow_map_buffers.empty())
        {
          ASTRALfailure("Unable to make forward progress on virtual buffers");
          break;
        }

      render_virtual_buffers(p, image_buffers, shadow_map_buffers, render_virtual_buffers_blit_atlas);
      ++m_stats[number_offscreen_render_targets];
    }
}

void
astral::Renderer::Implement::
render_direct_shadow_maps(void)
{
  m_workroom->m_direct_shadowmap_buffers.clear();
  for (unsigned int i = 1, endi = m_storage->number_virtual_buffers(); i < endi; ++i)
    {
      VirtualBuffer &buffer(m_storage->virtual_buffer(i));
      if (buffer.type() == VirtualBuffer::shadowmap_buffer
          && !buffer.uses_shadow_map()
          && buffer.remaining_dependencies() == 0
          && buffer.about_to_render_content() == routine_success)
        {
          if (!buffer.finish_issued())
            {
              buffer.issue_finish();
            }
          m_workroom->m_direct_shadowmap_buffers.push_back(i);
          buffer.location_in_depth_buffer(buffer.shadow_map()->atlas_location());
        }
    }

  if (m_workroom->m_direct_shadowmap_buffers.empty())
    {
      return;
    }
  c_array<const unsigned int> shadowmap_buffers(make_c_array(m_workroom->m_direct_shadowmap_buffers));

  /* make the render target active, but do not clear anything */
  m_backend->begin_render_target(RenderBackend::ClearParams(),
                                 *m_engine->shadow_map_atlas().render_target());

  m_backend->set_fragment_shader_emit(colorspace_linear);
  render_shadow_maps(make_c_array(m_workroom->m_direct_shadowmap_buffers));

  m_backend->end_render_target();

  for (unsigned int b : shadowmap_buffers)
    {
      VirtualBuffer &buffer(m_storage->virtual_buffer(b));
      buffer.render_performed_shadow_map(nullptr);
    }
}

void
astral::Renderer::Implement::
render_shadow_maps(c_array<const unsigned int> shadowmap_buffers)
{
  /* no color writes when generating a shadow map, also no uber-shading either */
  m_backend->set_stencil_state(StencilState().enabled(false));
  m_backend->color_write_mask(bvec4(false));
  m_backend->depth_buffer_mode(RenderBackend::depth_buffer_always);

  /* step 1: issue clears */

  /* The four virtual 1D textures are together as a
   * single rect of D pixels wide, 4 pixels high at
   * loc. ShadowmapGenerator shaders operate in
   * coordinate space local to the shadowmap to
   * which they render which is [-1, 1]x[0, 4].
   */
  Rect clear_rect;
  vecN<gvec4, DynamicRectShader::item_data_size> clear_rect_data;
  ItemData clear_rect_item_data;
  const ItemShader &clear_shader(*m_default_shaders.m_shadow_map_generator_shader.m_clear_shader);

  clear_rect.m_min_point = vec2(-1.0f, 0.0f);
  clear_rect.m_max_point = vec2(+1.0f, 4.0f);
  DynamicRectShader::pack_item_data(clear_rect, clear_rect_data);
  clear_rect_item_data = create_item_data(clear_rect_data, no_item_data_value_mapping);

  for (unsigned int b : shadowmap_buffers)
    {
      VirtualBuffer &buffer(m_storage->virtual_buffer(b));
      RenderValues st;
      unsigned int z_ignored(0);

      ASTRALassert(buffer.render_scale_translate().valid());

      st.m_item_data = clear_rect_item_data;
      st.m_blend_mode = BackendBlendMode(BackendBlendMode::shadowmap_mode_rendering);

      m_backend->draw_render_data(z_ignored, clear_shader, st,
                                  RenderBackend::UberShadingKey::Cookie(),
                                  buffer.render_scale_translate(),
                                  RenderBackend::ClipWindowValue(),
                                  false,
                                  m_dynamic_rect->vertex_range());
    }

  /* step 2: issue draws. We do not need to worry about draw order
   *         because the blend mode is always blend_mode_min.
   */
  m_backend->depth_buffer_mode(RenderBackend::depth_buffer_shadow_map);
  DrawCommandList::send_commands_sorted_by_shader_to_backend(*this,
                                                             shadowmap_buffers.begin(),
                                                             shadowmap_buffers.end());
}

astral::c_array<const unsigned int>
astral::Renderer::Implement::
end_abort_implement(void)
{
  m_vertex_streamer->end_abort();
  m_static_streamer->end_abort();
  m_static_streamer_fp16->end_abort();

  for (unsigned int i = 0, endi = m_storage->number_virtual_buffers(); i < endi; ++i)
    {
      m_storage->virtual_buffer(i).on_renderer_end_abort();
    }

  /* flush the image atlas anyways */
  m_engine->image_atlas().flush();

  /* for renders to RenderTarget(s), mark the surfaces as changeable again */
  for (RenderEncoderSurface encoder : m_virtual_buffer_to_render_target)
    {
      VirtualBuffer &render_target_buffer(*encoder.m_virtual_buffer);

      ASTRALassert(render_target_buffer.type() == VirtualBuffer::render_target_buffer);
      ASTRALassert(render_target_buffer.render_target()->active_status(detail::RenderTargetRendererStatusQuery()) == this);
      render_target_buffer.render_target()->active_status(detail::RenderTargetRendererStatus(nullptr));
    }

  for (range_type<unsigned int> R : m_virtual_buffer_to_render_target_subregion_same_surface)
    {
      unsigned int render_index;

      ASTRALassert(R.m_begin < R.m_end);
      ASTRALassert(R.m_end <= m_virtual_buffer_to_render_target_subregion.size());

      render_index = m_virtual_buffer_to_render_target_subregion[R.m_begin];

      VirtualBuffer &render_target_buffer(m_storage->virtual_buffer(render_index));

      ASTRALassert(render_target_buffer.type() == VirtualBuffer::render_target_buffer);
      ASTRALassert(render_target_buffer.render_target()->active_status(detail::RenderTargetRendererStatusQuery()) == this);
      render_target_buffer.render_target()->active_status(detail::RenderTargetRendererStatus(nullptr));
    }

  m_storage->clear();
  ASTRALassert(m_storage->number_virtual_buffers() == 0u);

  /* Let the backend know we are done with the current session */
  m_backend->end(make_c_array(m_stats).sub_array(number_renderer_stats));

  m_engine->image_atlas().unlock_resources();
  m_engine->colorstop_sequence_atlas().unlock_resources();
  m_engine->vertex_data_allocator().unlock_resources();
  m_engine->static_data_allocator32().unlock_resources();
  m_engine->static_data_allocator16().unlock_resources();

  m_virtual_buffer_to_render_target.clear();
  m_virtual_buffer_to_render_target_subregion.clear();
  m_virtual_buffer_to_render_target_subregion_same_surface.clear();

  /* increment m_begin_cnt to make all the RenderEncoderBase
   * derived object invalid.
   */
  ++m_begin_cnt;

  return make_c_array(m_stats);
}

astral::c_array<const unsigned int>
astral::Renderer::Implement::
end_implement(OffscreenBufferAllocInfo *p)
{
  /* Inform the virtual buffers that the frame has come to an end
   * for them to do any work needed before submitting to the backend.
   * Note that on_renderer_end() may add additional VirtualBuffer
   * objects. Those VirtualBuffer objects do NOT have on_renderer_end()
   * called. In addition, on_renderer_end() will call issue_finish()
   * on each VirtualBuffer which can trigger the creation of new
   * astral::Image objects; thus this must be done before the
   * image atlas is flushed, via m_engine->image_atlas().flush()
   */
  for (unsigned int i = 0, endi = m_storage->number_virtual_buffers(); i < endi; ++i)
    {
      m_storage->virtual_buffer(i).on_renderer_end();
    }

  /* Flush the streamed data and any texture data from the CPU.
   * Note that we need to flush the ImageAtlas here as well because
   * any image uploads need to be on GPU before any rendering.
   * Note that this is ok in terms of resize, because all the
   * Image allocations are completed for frame on end(), thus
   * this flush will also induce the only possible color and
   * index backing resize of the frame.
   */
  m_stats[number_vertices_streamed] = m_vertex_streamer->end();
  m_stats[number_static_u32vec4_streamed] = m_static_streamer->end();
  m_stats[number_static_u16vec4_streamed] = m_static_streamer_fp16->end();
  m_engine->image_atlas().flush();

  /* render shadow maps and the virtual buffers */
  render_direct_shadow_maps();
  render_non_render_target_virtual_buffers(p);

  /* now we can render the virtual buffers that render to a render target*/
  for (RenderEncoderSurface encoder : m_virtual_buffer_to_render_target)
    {
      VirtualBuffer &render_target_buffer(*encoder.m_virtual_buffer);
      enum return_code R;

      R = render_target_buffer.about_to_render_content();

      ASTRALassert(render_target_buffer.type() == VirtualBuffer::render_target_buffer);
      ASTRALassert(R == routine_success);
      ASTRALunused(R);

      RenderBackend::ClearParams clear_params;
      vec4 clear_color;
      RenderBackend::UberShadingKey::Cookie uber_key_cookie;

      /* the clear color sent to the backend needs to be
       * pre-multiplied by alpha because we are rendering
       * pre-multiplied by alpha color values.
       */
      clear_color = vec4(render_target_buffer.render_target_clear_color()) / 255.0f;
      clear_color.x() *= clear_color.w();
      clear_color.y() *= clear_color.w();
      clear_color.z() *= clear_color.w();

      clear_params
        .clear_stencil(0)
        .clear_color(clear_color)
        .clear_depth(RenderBackend::depth_buffer_value_clear);

      m_backend->begin_render_target(clear_params, *render_target_buffer.render_target());
      m_backend->depth_buffer_mode(RenderBackend::depth_buffer_occlude);
      m_backend->color_write_mask(bvec4(true));
      m_backend->set_stencil_state(StencilState().enabled(false));

      m_backend->set_fragment_shader_emit(render_target_buffer.colorspace());

      ASTRALassert(render_target_buffer.remaining_dependencies() == 0u);
      if (m_properties.m_overridable_properties.m_uber_shader_method == uber_shader_all)
        {
          RenderBackend::UberShadingKey &uber_key(*m_workroom->m_uber_shading_key[m_properties.m_overridable_properties.m_uber_shader_method]);
          uber_key.uber_shader_of_all(clip_window_not_present); /* no shader clipping when rendering to a surface */
          uber_key_cookie = uber_key.cookie();
        }
      else if (m_properties.m_overridable_properties.m_uber_shader_method != uber_shader_none)
        {
          RenderBackend::UberShadingKey &uber_key(*m_workroom->m_uber_shading_key[m_properties.m_overridable_properties.m_uber_shader_method]);
          uber_key.begin_accumulate(clip_window_not_present, /* no shader clipping when rendering to a surface */
                                    m_properties.m_overridable_properties.m_uber_shader_method);
          render_target_buffer.command_list()->accumulate_opaques_shaders(*m_storage, uber_key);
          render_target_buffer.command_list()->accumulate_typical_shaders(*m_storage, uber_key);
          uber_key.end_accumulate();
          uber_key_cookie = uber_key.cookie();
        }

      m_backend->color_write_mask(bvec4(true));
      render_target_buffer.command_list()->send_opaque_commands_to_backend(*this, uber_key_cookie,
                                                                           RenderValue<ScaleTranslate>(),
                                                                           RenderBackend::ClipWindowValue(),
                                                                           render_target_buffer.start_z(), false);
      render_target_buffer.command_list()->send_commands_to_backend(*this, uber_key_cookie,
                                                                    RenderValue<ScaleTranslate>(),
                                                                    RenderBackend::ClipWindowValue(),
                                                                    render_target_buffer.start_z(), false);
      m_backend->end_render_target();

      ASTRALassert(render_target_buffer.render_target()->active_status(detail::RenderTargetRendererStatusQuery()) == this);
      render_target_buffer.render_target()->active_status(detail::RenderTargetRendererStatus(nullptr));

      render_target_buffer.render_performed(nullptr);
    }

  for (range_type<unsigned int> R : m_virtual_buffer_to_render_target_subregion_same_surface)
    {
      c_array<unsigned int> image_buffers, shadow_map_buffers;

      ASTRALassert(R.m_begin < R.m_end);
      ASTRALassert(R.m_end <= m_virtual_buffer_to_render_target_subregion.size());
      image_buffers = make_c_array(m_virtual_buffer_to_render_target_subregion).sub_array(R);

      VirtualBuffer &first_buffer(m_storage->virtual_buffer(image_buffers.front()));
      RenderBackend::ClearParams clear_params;
      vec4 clear_color;

      /* the clear color sent to the backend needs to be
       * pre-multiplied by alpha because we are rendering
       * pre-multiplied by alpha color values.
       */
      clear_color = vec4(first_buffer.render_target_clear_color()) / 255.0f;
      clear_color.x() *= clear_color.w();
      clear_color.y() *= clear_color.w();
      clear_color.z() *= clear_color.w();

      clear_params
        .clear_stencil(0)
        .clear_color(clear_color)
        .clear_depth(RenderBackend::depth_buffer_value_clear);

      /* start the render target */
      m_backend->begin_render_target(clear_params, *first_buffer.render_target());
      m_backend->depth_buffer_mode(RenderBackend::depth_buffer_occlude);
      m_backend->color_write_mask(bvec4(true));
      m_backend->set_stencil_state(StencilState().enabled(false));
      m_backend->set_fragment_shader_emit(first_buffer.colorspace());

      render_virtual_buffers(p, image_buffers, shadow_map_buffers, render_virtual_buffers_directly);

      /* end the render target and mark it as not-active */
      m_backend->end_render_target();
      ASTRALassert(first_buffer.render_target()->active_status(detail::RenderTargetRendererStatusQuery()) == this);
      first_buffer.render_target()->active_status(detail::RenderTargetRendererStatus(nullptr));
    }

  m_virtual_buffer_to_render_target.clear();
  m_virtual_buffer_to_render_target_subregion.clear();
  m_virtual_buffer_to_render_target_subregion_same_surface.clear();

  /* Let the backend know we are done drawing */
  m_backend->end(make_c_array(m_stats).sub_array(number_renderer_stats));

  ASTRALassert(m_storage->number_virtual_buffers() > 0u);
  m_stats[number_virtual_buffers] = m_storage->number_virtual_buffers() - 1u;

  /* Step 3: Now that rendering is done, clear all the tmp storage arrays,
   *         this does not deallocate memory but marks the arrays as empty
   */
  m_storage->clear();
  ASTRALassert(m_storage->number_virtual_buffers() == 0u);

  m_engine->image_atlas().unlock_resources();
  m_engine->colorstop_sequence_atlas().unlock_resources();
  m_engine->vertex_data_allocator().unlock_resources();
  m_engine->static_data_allocator32().unlock_resources();
  m_engine->static_data_allocator16().unlock_resources();

  /* increment m_begin_cnt to make all the RenderEncoderBase
   * derived object invalid.
   */
  ++m_begin_cnt;

  return make_c_array(m_stats);
}

astral::reference_counted_ptr<astral::Image>
astral::Renderer::Implement::
create_image(ivec2 sz)
{
  reference_counted_ptr<Image> return_value;

  if (sz.x() > 0 && sz.y() > 0)
    {
      unsigned int num_mip_levels(1);
      return_value = m_engine->image_atlas().create_image(num_mip_levels, uvec2(sz));
    }

  return return_value;
}

void
astral::Renderer::
set_clip_error_callback(reference_counted_ptr<ClippingErrorCallback> callback)
{
  implement().m_clipping_error_callback = callback;
}

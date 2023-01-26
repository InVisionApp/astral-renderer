/*!
 * \file renderer_implement.hpp
 * \brief file renderer_implement.hpp
 *
 * Copyright 2022 by InvisionApp.
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

#ifndef ASTRAL_RENDERER_IMPLEMENT_HPP
#define ASTRAL_RENDERER_IMPLEMENT_HPP

#include <astral/renderer/renderer.hpp>

class astral::Renderer::Implement:public astral::Renderer
{
public:
  class DrawCommandVerticesShaders;
  class DrawCommandDetailed;
  class DrawCommand;
  class DrawCommandList;
  class CachedTransformation;
  class ClipGeometry;
  class ClipGeometryGroup;
  class ClipGeometrySimple;
  class Storage;
  class STCData;
  class Streamer;
  class VertexStreamer;
  class StaticStreamer32;
  class StaticStreamer16;
  class Filler;
  class WorkRoom;
  class CachedCombinedPath;
  class ClipElement;
  class ClipCombineResult;
  class ScratchRenderTarget;
  class UberShadingKeyCollection;
  class TileHitDetection;

  class MaskDrawerImage;

  enum render_virtual_buffer_mode_t
    {
      /* indicates rendering to scratch render target and then blitting
       * contents to atlases
       */
      render_virtual_buffers_blit_atlas,

      /* indicates rendering virtual buffers to the currently bound
       * render taget in the backend
       */
      render_virtual_buffers_directly,
    };

  explicit
  Implement(RenderEngine &engine);

  void
  begin_implement(enum colorspace_t);

  c_array<const unsigned int>
  end_implement(OffscreenBufferAllocInfo *out_alloc_info);

  c_array<const unsigned int>
  end_abort_implement(void);

  bool // returns true if the command should be viewed as opaque
  pre_process_command(bool, DrawCommand &cmd);

  void
  render_direct_shadow_maps(void);

  void
  render_shadow_maps(c_array<const unsigned int> shadowmap_buffers);

  void //renders all virtual buffers that do NOT render to a RenderTarget
  render_non_render_target_virtual_buffers(OffscreenBufferAllocInfo *p);

  void //renders a sub-set of buffers that have their dependencies met
  render_virtual_buffers(OffscreenBufferAllocInfo *p,
                         c_array<const unsigned int> image_buffers,
                         c_array<const unsigned int> shadow_map_buffers,
                         enum render_virtual_buffer_mode_t mode);

  void //performs the STC algorithm on the passed buffers.
  render_stc_virtual_buffers(c_array<const unsigned int>::iterator begin,
                             c_array<const unsigned int>::iterator end);

  void //render-to-stencil pass of STC
  render_stc_prepare_pass(c_array<const unsigned int>::iterator begin,
                          c_array<const unsigned int>::iterator end);

  void //performs the cover pass of the STC
  render_stc_cover_pass(void);

  void //performs the anti-alias passes of STC algorithm
  render_stc_aa_virtual_buffers(c_array<const unsigned int>::iterator begin,
                                c_array<const unsigned int>::iterator end);

  void //worker for render_stc_prepare_pass() and render_stc_aa_virtual_buffers()
  render_stc_virtual_buffers_pass(c_array<const unsigned int>::iterator begin,
                                  c_array<const unsigned int>::iterator end,
                                  enum FillSTCShader::pass_t pass, const ItemShader &shader);

  void //worker for render_stc_cover_pass()
  render_stc_virtual_buffers_cover_pass(c_array<const unsigned int> buffers);

  /* Creates a set of clip-equations for clipping against
   * a rectangle
   */
  RenderBackend::ClipWindowValue
  create_clip_window(vec2 min_corner, vec2 size);

  enum clip_window_value_type_t
  compute_shader_clipping(void);

  reference_counted_ptr<Image>
  create_image(ivec2 sz);

  float m_default_render_accuracy;
  bool m_default_use_pixel_rect_tile_culling;

  /* the storage storage for all commands within a begin()/end() pair */
  reference_counted_ptr<Storage> m_storage;

  /* the scratch render targets where virtual buffers are rendered.
   * Each successive buffer is twice the height of the previous.
   */
  std::vector<ScratchRenderTarget> m_scratch_render_targets;

  /* The RenderEngine to use. */
  reference_counted_ptr<RenderEngine> m_engine;

  /* local copy of Properties */
  RenderEngine::Properties m_properties;

  /* The RenderBackend object that will perform the rendering */
  reference_counted_ptr<RenderBackend> m_backend;

  /* cached copy of the default shaders to use */
  ShaderSet m_default_shaders;

  /* cached copy of default effects to use */
  EffectSet m_default_effects;

  unsigned int m_num_backend_stats;
  std::vector<unsigned int> m_stats;
  std::vector<c_string> m_stat_labels;

  /* the number of times begin() has been called */
  unsigned int m_begin_cnt;

  /* default color space for encoder_image() */
  enum colorspace_t m_default_encoder_image_colorspace;

  /* values that get reused alot */
  reference_counted_ptr<const VertexData> m_dynamic_rect;
  RenderValue<Brush> m_black_brush, m_white_brush;
  RenderValue<Transformation> m_identity;

  /* list of VirtualBuffers that render to a RenderTarget's entire viewport */
  std::vector<RenderEncoderSurface> m_virtual_buffer_to_render_target;

  /* list of VirtualBuffers that render to a Subviewport of a RenderTarget;
   * the encoders are placed in an order so that the encoders made from
   * the same encoders_surface() call are together. Note that the
   * values are integers, this is so that render_virtual_buffers() can
   * be reused to render their content.
   */
  std::vector<unsigned int> m_virtual_buffer_to_render_target_subregion;

  /* m_virtual_buffer_to_render_target_subregion_same_surface[i]
   * gives a range into m_virtual_buffer_to_render_target_subregion where
   * the same surface is used.
   */
  std::vector<range_type<unsigned int>> m_virtual_buffer_to_render_target_subregion_same_surface;

  reference_counted_ptr<SparseFillingErrorCallBack> m_clipping_error_callback;
  vecN<reference_counted_ptr<Filler>, number_fill_method_t> m_filler;
  reference_counted_ptr<WorkRoom> m_workroom;
  reference_counted_ptr<VertexStreamer> m_vertex_streamer;
  reference_counted_ptr<StaticStreamer32> m_static_streamer;
  reference_counted_ptr<StaticStreamer16> m_static_streamer_fp16;
};

#endif

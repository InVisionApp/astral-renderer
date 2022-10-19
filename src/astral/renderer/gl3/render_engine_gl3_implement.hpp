/*!
 * \file render_engine_gl3_implement.hpp
 * \brief render_engine_gl3_implement.hpp
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

#include <astral/util/gl/wasm_missing_gl_enums.hpp>
#include <astral/renderer/gl3/render_engine_gl3.hpp>

#ifndef ASTRAL_RENDER_ENGINE_GL3_IMPLEMENT_HPP
#define ASTRAL_RENDER_ENGINE_GL3_IMPLEMENT_HPP

class astral::gl::RenderEngineGL3::Implement:public astral::gl::RenderEngineGL3
{
public:
  class Backend;
  class Packing;
  class ShaderBuilder;
  class BlendBuilder;
  class AtlasBlitter;
  class ColorStopSequenceBacking;
  class StaticDataBackingBase;
  class StaticDataBackingTexture;
  class StaticDataBackingBufferObject;
  class VertexBacking;
  class ImageBacking;
  class ImageColorBacking;
  class ImageIndexBacking;
  class ShadowMapBacking;
  class FBOBlitter;

  /* this class exists for easier extendability to stash
   * configuration options that are not public in Config
   * that are usually a function of the presence of
   * GL/GLES extensions.
   */
  class ExtraConfig:public Config
  {
  public:
    explicit
    ExtraConfig(const Config &config);
  };

  enum
    {
      /* texture binding point for colorstop atlas backing */
      colorstop_atlas_binding_point_index = 0,

      /* texture binding point for shared data backing */
      static_data32_texture_binding_point_index,

      /* texture binding point for shared fp16 data backing */
      static_data16_texture_binding_point_index,

      /* texture binding point for the color data of tiled images */
      color_tile_image_atlas_binding_point_index,

      /* texture binding point for the index data of tiled images */
      index_tile_image_atlas_binding_point_index,

      /* texture binding point for the rect shadow map atlas */
      shadow_map_atlas_binding_point_index,

      /* texture binding point for when data buffers are backed by a texture */
      data_buffer_texture_binding_point_index,

      number_data_texture_binding_points,

      /* the texture binding point for the VertexBacking as a TEXTURE_BUFFER or
       * TEXTURE_2D
       */
      vertex_backing_texture_binding_point_index = number_data_texture_binding_points,

      /* the texture binding point for the surface texture that from gl_VertexID
       * gives the index into the VertexBacking and the header index
       */
      vertex_surface_texture_binding_point_index,

      total_number_texture_binding_points,
    };

  explicit
  Implement(const reference_counted_ptr<AtlasBlitter> &atlas_blitter,
            const reference_counted_ptr<FBOBlitter> &fbo_blitter,
            const reference_counted_ptr<ColorStopSequenceBacking> &cs,
            const reference_counted_ptr<VertexBacking> &iv,
            const reference_counted_ptr<StaticDataBackingBase> &sd,
            const reference_counted_ptr<StaticDataBackingBase> &sd16,
            const reference_counted_ptr<ImageColorBacking> &tic,
            const reference_counted_ptr<ImageIndexBacking> &tii,
            const reference_counted_ptr<ShadowMapBacking> &sm,
            const ExtraConfig &config, const Properties &properties,
            const BlendBuilder &blender, unsigned int num_clip_planes);

  virtual
  reference_counted_ptr<RenderBackend>
  create_backend(void) override;

  virtual
  reference_counted_ptr<RenderTarget>
  create_render_target(ivec2 dims,
                       reference_counted_ptr<ColorBuffer> *out_color_buffer,
                       reference_counted_ptr<DepthStencilBuffer> *out_ds_buffer) override;

  virtual
  reference_counted_ptr<const StaticData>
  pack_image_sampler_as_static_data(const ImageSampler &image) override;

  virtual
  const ShaderSet&
  default_shaders(void) override
  {
    return m_default_shaders;
  }

  virtual
  const EffectShaderSet&
  default_effect_shaders(void) override
  {
    return m_default_effect_shaders;
  }

  virtual
  const EffectSet&
  default_effects(void) override
  {
    return m_default_effects;
  }

  /*!
   * Gives the buffer binding point index for the named buffer.
   */
  static
  unsigned int
  data_binding_point_index(enum data_t tp)
  {
    return tp;
  }

  /*!
   * Returns the buffer binding point index to
   * use for the misc data UBO.
   */
  static
  unsigned int
  misc_data_binding_point_index(void)
  {
    return number_data_types;
  }

  /*!
   * Returns the buffer binding point index to use for
   * the offsets into the texture that backs the data
   * when m_config.m_use_texture_for_uniform_buffer
   * is true.
   */
  static
  unsigned int
  data_texture_offset_ubo_binding_point_index(void)
  {
    return 0;
  }

  static
  reference_counted_ptr<ColorStopSequenceBacking>
  create_color_stop_backing(const ExtraConfig &config, FBOBlitter &fbo_blitter);

  static
  reference_counted_ptr<VertexBacking>
  create_vertex_index_backing(const ExtraConfig &config, FBOBlitter &fbo_blitter);

  static
  reference_counted_ptr<StaticDataBackingBase>
  create_data_backing(enum StaticDataBacking::type_t tp,
                      const ExtraConfig &config, FBOBlitter &fbo_blitter);

  static
  reference_counted_ptr<ImageColorBacking>
  create_image_color_backing(const ExtraConfig &config, AtlasBlitter &blitter);

  static
  reference_counted_ptr<ImageIndexBacking>
  create_image_index_backing(const ExtraConfig &config, AtlasBlitter &blitter);

  static
  reference_counted_ptr<ShadowMapBacking>
  create_shadow_map_atlas(const ExtraConfig &config, FBOBlitter &fbo_blitter, AtlasBlitter &atlas_blitter);

  /* Binds 0 (i.e. unbinds) from all binding points used by RenderEngineGL3 */
  static
  void
  unbind_objects(void);

  /* sets GL state to be clean to correctly work with RenderEngineGL3 */
  static
  void
  init_gl_state(void);

  ExtraConfig m_config;
  unsigned int m_number_gl_clip_planes;
  reference_counted_ptr<ShaderBuilder> m_shader_builder;
  reference_counted_ptr<AtlasBlitter> m_atlas_blitter;
  reference_counted_ptr<FBOBlitter> m_fbo_blitter;
  const ColorStopSequenceBacking *m_colorstop_atlas;
  const StaticDataBackingBase *m_static_data_atlas;
  const StaticDataBackingBase *m_static_data_fp16_atlas;
  const VertexBacking *m_vertex_backing;
  const ImageColorBacking *m_image_color_backing;
  const ImageIndexBacking *m_image_index_backing;
  const ShadowMapBacking *m_shadow_map_backing;

  ShaderSet m_default_shaders;
  EffectShaderSet m_default_effect_shaders;
  EffectSet m_default_effects;
  ShaderSetGL3 m_gl3_shaders;
};

#endif

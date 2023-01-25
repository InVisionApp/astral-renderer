/*!
 * \file render_engine_gl3.cpp
 * \brief render_engine_gl3.cpp
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

#include <cstring>
#include <sstream>
#include <astral/renderer/gl3/render_engine_gl3.hpp>
#include <astral/renderer/gl3/render_target_gl3.hpp>
#include <astral/util/gl/gl_context_properties.hpp>
#include <astral/util/gl/astral_gl.hpp>
#include <astral/util/gl/gl_get.hpp>
#include <astral/util/gl/wasm_missing_gl_enums.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include "render_engine_gl3_implement.hpp"
#include "render_engine_gl3_backend.hpp"
#include "render_engine_gl3_packing.hpp"
#include "render_engine_gl3_shader_builder.hpp"
#include "render_engine_gl3_static_data.hpp"
#include "render_engine_gl3_colorstop.hpp"
#include "render_engine_gl3_vertex.hpp"
#include "render_engine_gl3_image.hpp"
#include "render_engine_gl3_shadow_map.hpp"
#include "render_engine_gl3_atlas_blitter.hpp"
#include "render_engine_gl3_fbo_blitter.hpp"

///////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::ExtraConfig methods
astral::gl::RenderEngineGL3::Implement::ExtraConfig::
ExtraConfig(const Config &config):
  Config(config)
{
}

//////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement methods
astral::gl::RenderEngineGL3::Implement::
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
          const BlendBuilder &blender, unsigned int num_clip_planes):
  RenderEngineGL3(properties, cs, iv, sd, sd16, tii, tic, sm),
  m_config(config),
  m_number_gl_clip_planes(num_clip_planes),
  m_atlas_blitter(atlas_blitter),
  m_fbo_blitter(fbo_blitter)
{
  m_colorstop_atlas = cs.get();
  m_static_data_atlas = sd.get();
  m_static_data_fp16_atlas = sd16.get();
  m_vertex_backing = iv.get();
  m_image_color_backing = tic.get();
  m_image_index_backing = tii.get();
  m_shadow_map_backing = sm.get();

  m_shader_builder = ASTRALnew ShaderBuilder(*this, blender, m_config);
  m_shader_builder->create_shaders(&m_default_shaders,
                                   &m_default_effect_shaders,
                                   &m_gl3_shaders);
  m_default_effects.m_gaussian_blur =
    m_default_effect_shaders.m_gaussian_blur_shader.create_effect();
}

astral::reference_counted_ptr<astral::gl::RenderEngineGL3::Implement::ColorStopSequenceBacking>
astral::gl::RenderEngineGL3::Implement::
create_color_stop_backing(const ExtraConfig &config, FBOBlitter &fbo_blitter)
{
  reference_counted_ptr<ColorStopSequenceBacking> b;
  b = ASTRALnew ColorStopSequenceBacking(fbo_blitter,
                                         config.m_log2_dims_colorstop_atlas,
                                         config.m_initial_num_colorstop_atlas_layers);
  return b;
}

astral::reference_counted_ptr<astral::gl::RenderEngineGL3::Implement::VertexBacking>
astral::gl::RenderEngineGL3::Implement::
create_vertex_index_backing(const ExtraConfig &config, FBOBlitter &fbo_blitter)
{
  reference_counted_ptr<StaticDataBackingBase> store;

  #ifndef __EMSCRIPTEN__
  if (config.m_vertex_buffer_layout == linear_array)
    {
      store = ASTRALnew StaticDataBackingBufferObject(StaticDataBacking::type32,
                                                      config.m_vertex_buffer_size);
    }
  else
  #endif
    {
      store = ASTRALnew StaticDataBackingTexture(StaticDataBacking::type32,
                                                 fbo_blitter,
                                                 config.m_vertex_buffer_log2_width,
                                                 config.m_vertex_buffer_log2_height,
                                                 config.m_vertex_buffer_size);
    }

  return ASTRALnew VertexBacking(store);
}

astral::reference_counted_ptr<astral::gl::RenderEngineGL3::Implement::ImageColorBacking>
astral::gl::RenderEngineGL3::Implement::
create_image_color_backing(const ExtraConfig &config, AtlasBlitter &blitter)
{
  return ASTRALnew ImageColorBacking(blitter,
                                     config.m_image_color_atlas_width_height,
                                     config.m_image_color_atlas_number_layers,
                                     config.m_max_number_color_backing_layers);
}

astral::reference_counted_ptr<astral::gl::RenderEngineGL3::Implement::ImageIndexBacking>
astral::gl::RenderEngineGL3::Implement::
create_image_index_backing(const ExtraConfig &config, AtlasBlitter &blitter)
{
  return ASTRALnew ImageIndexBacking(blitter,
                                     config.m_image_index_atlas_width_height,
                                     config.m_image_index_atlas_number_layers,
                                     config.m_max_number_index_backing_layers);
}

astral::reference_counted_ptr<astral::gl::RenderEngineGL3::Implement::StaticDataBackingBase>
astral::gl::RenderEngineGL3::Implement::
create_data_backing(enum StaticDataBacking::type_t tp,
                    const ExtraConfig &config,
                    FBOBlitter &fbo_blitter)
{
  #ifndef __EMSCRIPTEN__
  if (config.m_static_data_layout == linear_array)
    {
      return ASTRALnew StaticDataBackingBufferObject(tp, config.m_initial_static_data_size);
    }
  else
  #endif
    {
      return ASTRALnew StaticDataBackingTexture(tp, fbo_blitter,
                                                config.m_static_data_log2_width,
                                                config.m_static_data_log2_height,
                                                config.m_initial_static_data_size);
    }
}

astral::reference_counted_ptr<astral::gl::RenderEngineGL3::Implement::ShadowMapBacking>
astral::gl::RenderEngineGL3::Implement::
create_shadow_map_atlas(const ExtraConfig &config, FBOBlitter &fbo_blitter,
                        AtlasBlitter &atlas_blitter)
{
  return ASTRALnew ShadowMapBacking(config.m_shadow_map_atlas_width,
                                    config.m_shadow_map_atlas_initial_height,
                                    fbo_blitter, atlas_blitter);
}

astral::reference_counted_ptr<const astral::StaticData>
astral::gl::RenderEngineGL3::Implement::
pack_image_sampler_as_static_data(const ImageSampler &image)
{
  vecN<generic_data, Packing::packed_data_image_size> data_backing;
  c_array<generic_data> data(data_backing);
  Packing::ProcessedImageSampler processed;

  processed.init(image, image_atlas());
  Packing::pack(data, processed);

  return static_data_allocator32().create(data.reinterpret_pointer<const gvec4>());
}

astral::reference_counted_ptr<astral::RenderBackend>
astral::gl::RenderEngineGL3::Implement::
create_backend(void)
{
  return ASTRALnew Backend(*this);
}

astral::reference_counted_ptr<astral::RenderTarget>
astral::gl::RenderEngineGL3::Implement::
create_render_target(ivec2 dims,
                     reference_counted_ptr<ColorBuffer> *out_cb,
                     reference_counted_ptr<DepthStencilBuffer> *out_ds)
{
  reference_counted_ptr<ColorBufferGL> cb;
  reference_counted_ptr<DepthStencilBufferGL> ds;

  cb = ColorBufferGL::create(dims);
  ds = DepthStencilBufferGL::create(dims);

  if (out_cb)
    {
      *out_cb = cb;
    }

  if (out_ds)
    {
      *out_ds = ds;
    }

  return RenderTargetGL_Texture::create(cb, ds);
}

void
astral::gl::RenderEngineGL3::Implement::
unbind_objects(void)
{
  astral_glBindFramebuffer(ASTRAL_GL_DRAW_FRAMEBUFFER, 0u);
  astral_glBindFramebuffer(ASTRAL_GL_READ_FRAMEBUFFER, 0u);

  /* don't let any VAO leak */
  astral_glBindVertexArray(0);

  /* don't let a GL program leak */
  astral_glUseProgram(0);

  /* don't let any texture leak */
  for (unsigned int i = 0; i < total_number_texture_binding_points; ++i)
    {
      astral_glActiveTexture(i + ASTRAL_GL_TEXTURE0);
      astral_glBindTexture(ASTRAL_GL_TEXTURE_2D_ARRAY, 0u);
      astral_glBindTexture(ASTRAL_GL_TEXTURE_2D, 0u);
      astral_glBindSampler(i, 0u);
    }

  /* don't let any buffers leak either, note that we have no VAO bound either
   * so this only affects actual GL state
   */
  astral_glBindBuffer(ASTRAL_GL_ARRAY_BUFFER, 0);
  astral_glBindBuffer(ASTRAL_GL_ELEMENT_ARRAY_BUFFER, 0);
  astral_glBindBuffer(ASTRAL_GL_UNIFORM_BUFFER, 0);
}

void
astral::gl::RenderEngineGL3::Implement::
init_gl_state(void)
{
  /* make sure rasterization is not discarded */
  astral_glDisable(ASTRAL_GL_RASTERIZER_DISCARD);

  /* various other rasterization options */
  astral_glDisable(ASTRAL_GL_DITHER);
  #ifndef __EMSCRIPTEN__
    {
      if (!ContextProperties::is_es())
        {
          astral_glDisable(ASTRAL_GL_POLYGON_SMOOTH);
          astral_glDisable(ASTRAL_GL_COLOR_LOGIC_OP);
        }
    }
  #endif

  /* do not allow depth values to be changed,
   * note that we only care about drawing triangles
   * so that is why it is only GL_POLYGON_OFFSET_FILL
   * we disable
   */
  astral_glDisable(ASTRAL_GL_POLYGON_OFFSET_FILL);

  /* primitive restart */
  if (ContextProperties::is_es() || ContextProperties::version() >= ivec2(4, 3))
    {
      astral_glDisable(ASTRAL_GL_PRIMITIVE_RESTART_FIXED_INDEX);
    }

  #ifndef __EMSCRIPTEN__
    {
      if (!ContextProperties::is_es())
        {
          astral_glDisable(ASTRAL_GL_PRIMITIVE_RESTART);
        }
    }
  #endif

  /* Restore the GL defaults for values that affect
   * pixel upload to a texture
   */
  astral_glPixelStorei(ASTRAL_GL_UNPACK_ROW_LENGTH, 0);
  astral_glPixelStorei(ASTRAL_GL_UNPACK_IMAGE_HEIGHT, 0);
  astral_glPixelStorei(ASTRAL_GL_UNPACK_SKIP_ROWS, 0);
  astral_glPixelStorei(ASTRAL_GL_UNPACK_SKIP_PIXELS, 0);
  astral_glPixelStorei(ASTRAL_GL_UNPACK_SKIP_IMAGES, 0);
  astral_glPixelStorei(ASTRAL_GL_UNPACK_ALIGNMENT, 4);
  astral_glBindBuffer(ASTRAL_GL_PIXEL_UNPACK_BUFFER, 0);

  if (!ContextProperties::is_es())
    {
      /* These are only present in desktop GL */
      astral_glPixelStorei(ASTRAL_GL_UNPACK_SWAP_BYTES, ASTRAL_GL_FALSE);
      astral_glPixelStorei(ASTRAL_GL_UNPACK_LSB_FIRST, ASTRAL_GL_FALSE);
    }

  #ifdef __EMSCRIPTEN__
    {
      #ifndef GL_UNPACK_FLIP_Y_WEBGL
      #define GL_UNPACK_FLIP_Y_WEBGL 0x9240
      #endif

      #ifndef GL_UNPACK_PREMULTIPLY_ALPHA_WEBGL
      #define GL_UNPACK_PREMULTIPLY_ALPHA_WEBGL 0x9241
      #endif

      #ifndef GL_UNPACK_COLORSPACE_CONVERSION_WEBGL
      #define GL_UNPACK_COLORSPACE_CONVERSION_WEBGL 0x9243
      #endif

      /* these are only present in WebGL */
      astral_glPixelStorei(GL_UNPACK_FLIP_Y_WEBGL, ASTRAL_GL_FALSE);
      astral_glPixelStorei(GL_UNPACK_PREMULTIPLY_ALPHA_WEBGL, ASTRAL_GL_FALSE);
      astral_glPixelStorei(GL_UNPACK_COLORSPACE_CONVERSION_WEBGL, ASTRAL_GL_FALSE);
    }
  #endif
}

////////////////////////////////////////////
// astral::gl::RenderEngineGL3 methods
astral::reference_counted_ptr<astral::gl::RenderEngineGL3>
astral::gl::RenderEngineGL3::
create(const Config &in_config)
{
  Config config(in_config);
  unsigned int max_buffer_size;
  bool has_hw_clip_planes;
  int num_clip_planes;
  reference_counted_ptr<Implement::AtlasBlitter> atlas_blitter;
  reference_counted_ptr<Implement::FBOBlitter> fbo_blitter;

  #ifdef __EMSCRIPTEN__
    {
      auto hnd = emscripten_webgl_get_current_context();

      /* make sure all reported extensions are enabled; Emscripten
       * reports all GL extensions but does not enable them.
       */
      for (const std::string &ext : ContextProperties::extension_set())
        {
          std::string tmp;
          tmp = "GL_" + ext;

          emscripten_webgl_enable_extension(hnd, ext.c_str());
          emscripten_webgl_enable_extension(hnd, tmp.c_str());
        }

      if (config.m_data_streaming == astral::gl::RenderEngineGL3::data_streaming_bo_mapping)
        {
          config.m_data_streaming = astral::gl::RenderEngineGL3::data_streaming_bo_subdata;
        }
    }
  #endif

  if (ContextProperties::is_es())
    {
      has_hw_clip_planes = ContextProperties::has_extension("GL_EXT_clip_cull_distance")
        || ContextProperties::has_extension("GL_APPLE_clip_distance")
        || ContextProperties::has_extension("GL_ANGLE_clip_cull_distance")
        || ContextProperties::has_extension("WEBGL_clip_cull_distance");

      /* if one day we support native mobile, check
       * for TBO support either by extensions and/or
       * GLES version.
       */
      config.m_static_data_layout = texture_2d_array;
      config.m_vertex_buffer_layout = texture_2d_array;
    }
  else
    {
      has_hw_clip_planes = true;
      config.m_use_glsl_unpack_fp16 = config.m_use_glsl_unpack_fp16
        && (ContextProperties::version() >= ivec2(4, 2) || ContextProperties::has_extension("GL_ARB_shading_language_packing"));
    }

  if (has_hw_clip_planes)
    {
      num_clip_planes = context_get<astral_GLint>(ASTRAL_GL_MAX_CLIP_DISTANCES);
    }
  else
    {
      num_clip_planes = 0;
    }

  if (config.m_use_texture_for_uniform_buffer)
    {
      unsigned int total = 0;

      /* the maximum size of the texture allowed */
      max_buffer_size = Implement::Backend::texture_dims_for_uniform_buffer() * Implement::Backend::texture_dims_for_uniform_buffer();

      /* the values of Packing::element_size_blocks(tp) gives how many
       * uvec4's an element of the named type occupies.
       */
      for (unsigned int i = 0; i < number_data_types; ++i)
        {
          enum data_t tp;
          tp = static_cast<enum data_t>(i);

          /* indices are 16-bit values with 0xFFFF representing "null", so no more than one less */
          config.m_max_per_draw_call[tp] = t_min(config.m_max_per_draw_call[tp], 0xFFFFu - 1u);

          total += Implement::Packing::element_size_blocks(tp) * config.m_max_per_draw_call[tp];
        }

      ASTRALhard_assert(total <= max_buffer_size);
    }
  else
    {
      /* Clamp the buffer-sizes to what we can support. */
      max_buffer_size = context_get<astral_GLint>(ASTRAL_GL_MAX_UNIFORM_BLOCK_SIZE);
      for (unsigned int i = 0; i < number_data_types; ++i)
        {
          enum data_t tp;
          unsigned int sz_per_element;

          tp = static_cast<enum data_t>(i);
          sz_per_element = sizeof(uint32_t) * Implement::Packing::element_size(tp);
          ASTRALassert(sz_per_element > 0u);

          /* The shaders store the 16-bit indices into the buffer-object
           * backed arrays. In addition, the shaders use the value 0xFFFF
           * to indicate a "null" reference. Thus we can have no more than
           * 0xFFFF - 1 elements in an array.
           */
          config.m_max_per_draw_call[i] = t_min(config.m_max_per_draw_call[i], 0xFFFFu - 1u);

          /* Bound the size of the array by how big a uniform buffer can be */
          config.m_max_per_draw_call[i] = t_min(config.m_max_per_draw_call[i], max_buffer_size / sz_per_element);
        }
    }

  config.m_uniform_buffer_size = t_max(config.m_uniform_buffer_size,
                                       compute_min_ubo_size(config.m_max_per_draw_call));

  /* adjust for max texture size */
  unsigned int max_texture_size, max_texture_layer, log2_max_texture_size;

  max_texture_size = context_get<astral_GLint>(ASTRAL_GL_MAX_TEXTURE_SIZE);
  max_texture_layer = context_get<astral_GLint>(ASTRAL_GL_MAX_ARRAY_TEXTURE_LAYERS);
  log2_max_texture_size = uint32_log2_floor(max_texture_size);

  config.m_log2_dims_colorstop_atlas = t_min(log2_max_texture_size,
                                             config.m_log2_dims_colorstop_atlas);
  config.m_static_data_log2_width = t_min(log2_max_texture_size,
                                          config.m_static_data_log2_width);
  config.m_static_data_log2_height = t_min(log2_max_texture_size,
                                          config.m_static_data_log2_height);
  config.m_vertex_buffer_log2_width = t_min(log2_max_texture_size,
                                            config.m_vertex_buffer_log2_width);
  config.m_vertex_buffer_log2_height = t_min(log2_max_texture_size,
                                             config.m_vertex_buffer_log2_height);

  config.m_shadow_map_atlas_width = t_min(max_texture_size,
                                               config.m_shadow_map_atlas_width);

  /* not only must the height be no more than max_texture_size, it must also be
   * a multiple of 4.
   */
  config.m_shadow_map_atlas_initial_height = t_min(max_texture_size,
                                                   t_max(4u, config.m_shadow_map_atlas_initial_height));

  config.m_shadow_map_atlas_initial_height &= ~3u;

  /* the vertex surface can be noe more than 16-bits wide because
   * the shader assumes that the x and y coordinate can fit within
   * 16-bits.
   */
  config.m_log2_gpu_stream_surface_width = t_min(t_min(16u, log2_max_texture_size),
                                                 config.m_log2_gpu_stream_surface_width);

  config.m_initial_num_colorstop_atlas_layers = t_min(max_texture_layer,
                                                      config.m_initial_num_colorstop_atlas_layers);

  config.m_use_hw_clip_window = config.m_use_hw_clip_window && (num_clip_planes >= 4);

  /* both m_image_color_atlas_width_height and
   * m_image_index_atlas_width_height must be
   * a multiple of ImageAtlas::color_tile_size
   * and no more than 2048. Since color_tile_size is
   * a power of two, masking by the complement of one
   * less than it gives us the even multiple.
   */
  const unsigned int mask(ImageAtlas::tile_size - 1u);
  unsigned int max_color_layers(t_min((unsigned int)Implement::ImageBacking::max_layers_color_texture, max_texture_layer));
  unsigned int max_index_layers(t_min((unsigned int)Implement::ImageBacking::max_layers_index_texture, max_texture_layer));
  unsigned int max_wh(t_min((unsigned int)Implement::ImageBacking::max_width_height, max_texture_size));

  config.m_max_number_color_backing_layers = t_min(config.m_max_number_color_backing_layers, max_color_layers);
  config.m_image_color_atlas_number_layers = t_min(config.m_image_color_atlas_number_layers, max_color_layers);
  config.m_image_color_atlas_width_height = t_min(config.m_image_color_atlas_width_height, max_wh);
  config.m_image_color_atlas_width_height &= ~mask;

  config.m_max_number_index_backing_layers = t_min(config.m_max_number_index_backing_layers, max_index_layers);
  config.m_image_index_atlas_number_layers = t_min(config.m_image_index_atlas_number_layers, max_index_layers);
  config.m_image_index_atlas_width_height = t_min(config.m_image_index_atlas_width_height, max_wh);
  config.m_image_index_atlas_width_height &= ~mask;

  /* The GL driver on M1 Mac's will emit a warning message if any texture
   * is empty; avoid the warning message by forcing any backing stores
   * to be non-empty.
   */
  config.m_initial_num_colorstop_atlas_layers = t_max(config.m_initial_num_colorstop_atlas_layers, 1u);
  config.m_vertex_buffer_size = t_max(config.m_vertex_buffer_size, 1u);
  config.m_initial_static_data_size = t_max(config.m_initial_static_data_size, 1u);
  config.m_image_color_atlas_number_layers = t_max(config.m_image_color_atlas_number_layers, 1u);
  config.m_image_index_atlas_number_layers = t_max(config.m_image_index_atlas_number_layers, 1u);

  /* We only support ultra uber-shader fall back if gl::Program::program_linked()
   * works, i.e when GL_KHR_parallel_shader_compile is present
   */
  if (!ContextProperties::has_extension("GL_KHR_parallel_shader_compile"))
    {
      config.m_uber_shader_fallback = uber_shader_fallback_none;
    }

  Properties properties;
  Implement::ExtraConfig extra_config(config);
  Implement::BlendBuilder blender(extra_config);

  /* make sure that the active texture unit it ASTRAL_GL_TEXTURE0, we do this
   * so that unbind_objects() will catch any texture objects bound
   * while making the backing for the resources.
   */
  astral_glActiveTexture(ASTRAL_GL_TEXTURE0);

  properties.m_overridable_properties.m_clip_window_strategy = (config.m_use_hw_clip_window) ?
    clip_window_strategy_shader:
    clip_window_strategy_depth_occlude_hinted;

  blender.set_blend_mode_information(&properties.m_blend_mode_information);

  Implement::init_gl_state();

  atlas_blitter = ASTRALnew Implement::AtlasBlitter(num_clip_planes);
  fbo_blitter = ASTRALnew Implement::FBOBlitter(num_clip_planes);

  reference_counted_ptr<RenderEngineGL3> return_value;
  return_value = ASTRALnew Implement(atlas_blitter, fbo_blitter,
                                     Implement::create_color_stop_backing(extra_config, *fbo_blitter),
                                     Implement::create_vertex_index_backing(extra_config, *fbo_blitter),
                                     Implement::create_data_backing(StaticDataBacking::type32, extra_config, *fbo_blitter),
                                     Implement::create_data_backing(StaticDataBacking::type16, extra_config, *fbo_blitter),
                                     Implement::create_image_color_backing(extra_config, *atlas_blitter),
                                     Implement::create_image_index_backing(extra_config, *atlas_blitter),
                                     Implement::create_shadow_map_atlas(extra_config, *fbo_blitter, *atlas_blitter),
                                     extra_config, properties, blender, num_clip_planes);

  Implement::unbind_objects();

  return return_value;
}

unsigned int
astral::gl::RenderEngineGL3::
compute_min_ubo_size(const vecN<unsigned int, number_data_types> &max_per_draw_call)
{
  unsigned int return_value(0u);

  for (unsigned int i = 0; i < number_data_types; ++i)
    {
      enum data_t tp;
      unsigned int sz, v;

      tp = static_cast<enum data_t>(i);
      sz = Implement::Packing::element_size(tp);
      v = max_per_draw_call[tp] * sz;

      /* glBindBufferBase requires that the offset is a multiple of
       * context_get<astral_GLint>(ASTRAL_GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT) which
       * at worst is 256 (bytes). Thus we make v a multiple of 64
       * because sizeof(generic_data) is 4.
       */
      if (v & 63u)
        {
          v += 64u - (v & 63u);
        }
      return_value += v;
    }
  return return_value * sizeof(generic_data);
}

unsigned int
astral::gl::RenderEngineGL3::
allocate_item_shader_index(detail::ShaderIndexArgument, const ItemShaderBackendGL3 *pshader, enum ItemShader::type_t type)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  return p->m_shader_builder->allocate_item_shader_index(pshader, type);
}

unsigned int
astral::gl::RenderEngineGL3::
allocate_material_shader_index(detail::ShaderIndexArgument, const MaterialShaderGL3 *pshader)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  return p->m_shader_builder->allocate_material_shader_index(pshader);
}

astral::gl::Program*
astral::gl::RenderEngineGL3::
gl_program(const ItemShader &shader,
           const MaterialShader *material,
           BackendBlendMode mode,
           enum clip_window_value_type_t shader_clipping)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  return p->m_shader_builder->gl_program(shader, material, mode, shader_clipping);
}

astral::gl::Program*
astral::gl::RenderEngineGL3::
gl_program(const RenderBackend::UberShadingKey &key)
{
  Implement *p;
  RenderBackend::UberShadingKey::Cookie cookie;

  p = static_cast<Implement*>(this);
  cookie = key.cookie();

  return p->m_shader_builder->uber_program(cookie);
}

void
astral::gl::RenderEngineGL3::
force_uber_shader_program_link(void)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  return p->m_shader_builder->force_uber_shader_program_link();
}

const astral::gl::RenderEngineGL3::Config&
astral::gl::RenderEngineGL3::
config(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_config;
}

const astral::gl::ShaderSetGL3&
astral::gl::RenderEngineGL3::
gl3_shaders(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_gl3_shaders;
}

astral::c_string
astral::
label(enum gl::RenderEngineGL3::data_t b)
{
  static const c_string labels[gl::RenderEngineGL3::number_data_types] =
    {
      [gl::RenderEngineGL3::data_header] = "data_header",
      [gl::RenderEngineGL3::data_item_transformation] = "data_item_transformation",
      [gl::RenderEngineGL3::data_item_scale_translate] = "data_item_scale_translate",
      [gl::RenderEngineGL3::data_clip_window] = "data_clip_window",
      [gl::RenderEngineGL3::data_brush] = "data_brush",
      [gl::RenderEngineGL3::data_gradient] = "data_gradient",
      [gl::RenderEngineGL3::data_gradient_transformation] = "data_gradient_transformation",
      [gl::RenderEngineGL3::data_item_data] = "data_item_data",
      [gl::RenderEngineGL3::data_image] = "data_image",
      [gl::RenderEngineGL3::data_shadow_map] = "data_shadow_map",
      [gl::RenderEngineGL3::data_clip_mask] = "data_clip_mask",
    };

  ASTRALassert(b < gl::RenderEngineGL3::number_data_types);
  ASTRALassert(labels[b]);

  return labels[b];
}

astral::c_string
astral::
label(enum gl::RenderEngineGL3::data_streaming_t b)
{
  static const c_string labels[gl::RenderEngineGL3::data_streaming_bo_subdata + 1] =
    {
      [gl::RenderEngineGL3::data_streaming_bo_orphaning] = "data_streaming_bo_orphaning",
      [gl::RenderEngineGL3::data_streaming_bo_mapping] = "data_streaming_bo_mapping",
      [gl::RenderEngineGL3::data_streaming_bo_subdata] = "data_streaming_bo_subdata",
    };

  ASTRALassert(b <= gl::RenderEngineGL3::data_streaming_bo_subdata);
  ASTRALassert(labels[b]);

  return labels[b];
}

astral::c_string
astral::
label(enum gl::RenderEngineGL3::uber_shader_fallback_t b)
{
  static const c_string labels[gl::RenderEngineGL3::uber_shader_fallback_last + 1] =
    {
      [gl::RenderEngineGL3::uber_shader_fallback_separate] = "uber_shader_fallback_separate",
      [gl::RenderEngineGL3::uber_shader_fallback_uber_all] = "uber_shader_fallback_uber_all",
      [gl::RenderEngineGL3::uber_shader_fallback_none] = "uber_shader_fallback_none",
    };

  ASTRALassert(b <= gl::RenderEngineGL3::uber_shader_fallback_last);
  ASTRALassert(labels[b]);

  return labels[b];
}



astral::c_string
astral::
label(enum gl::RenderEngineGL3::layout_t b)
{
  static const c_string labels[gl::RenderEngineGL3::layout_last + 1] =
    {
      [gl::RenderEngineGL3::linear_array] = "linear_array",
      [gl::RenderEngineGL3::texture_2d_array] = "texture_2d_array",
    };

  ASTRALassert(b <= gl::RenderEngineGL3::layout_last);
  ASTRALassert(labels[b]);

  return labels[b];
}

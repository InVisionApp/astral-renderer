/*!
 * \file render_engine_gl3_shadow_map.hpp
 * \brief file render_engine_gl3_shadow_map.hpp
 *
 * Copyright 2020 by InvisionApp.
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

#ifndef ASTRAL_RENDER_ENGINE_GL3_SHADOW_MAP_HPP
#define ASTRAL_RENDER_ENGINE_GL3_SHADOW_MAP_HPP

#include <astral/renderer/gl3/render_target_gl3.hpp>
#include <astral/renderer/gl3/render_engine_gl3.hpp>

#include "render_engine_gl3_implement.hpp"
#include "render_engine_gl3_fbo_blitter.hpp"
#include "render_engine_gl3_atlas_blitter.hpp"

/*!
 * ShadowMapBacking represents an implementation of
 * astral::ShadowMapAtlasBacking for the GL3 backend
 *
 * WebGL2 and depth textures has some incredibly bad
 * behaviours:
 *   (1) If a depth-texture has its sampling set to linear,
 *       the light_test demo hangs in Chrome.
 *   (2) However, if the depth-texture has its sampling set
 *       to nearest, but a sampler is bound with the filtering
 *       set to linear then instead of hanging, it returns 0
 *       always regardless of the actual contents.
 */
class astral::gl::RenderEngineGL3::Implement::ShadowMapBacking:
  public astral::ShadowMapAtlasBacking
{
public:
  ShadowMapBacking(unsigned int width, unsigned int initial_height,
                   FBOBlitter &fbo_blitter, AtlasBlitter &atlas_blitter);
  ~ShadowMapBacking();

  /*!
   * The GL texture. NOTE! this value changes if the
   * ShadowMapBacking is resized. The filtering for
   * the returned texture is GL_NEAREST for both
   * magnification and minification.
   */
  astral_GLuint
  texture(void) const
  {
    return m_texture;
  }

  /* Returns the GL Sampler object suitable for a sampler2DShadow
   * in the GLSL code. Has the filters set to linear and the the
   * comparison mode set to ASTRAL_GL_COMPARE_REF_TO_TEXTURE with the
   * function as ASTRAL_GL_LESS
   */
  astral_GLuint
  shadow_sampler(void) const
  {
    return m_shadow_sampler;
  }

  /* Returns a sampler which does not have the comparison modes
   * set, but has the filtering set to LINEAR for both magnificaiton
   * and minification.
   */
  astral_GLuint
  linear_sampler(void) const
  {
    return m_linear_sampler;
  }

  virtual
  void
  flush_gpu(void) override;

  virtual
  void
  copy_pixels(uvec2 dst_location, uvec2 size,
              DepthStencilBuffer &src, uvec2 src_location) override;

  virtual
  reference_counted_ptr<RenderTarget>
  render_target(void) const override
  {
    return m_render_target;
  }

protected:
  virtual
  unsigned int
  on_resize(unsigned int new_height) override;

private:
  void
  create_storage(unsigned int width, unsigned int height);

  unsigned int m_max_texture_size;
  reference_counted_ptr<RenderTargetGL_Texture> m_render_target;
  reference_counted_ptr<FBOBlitter> m_fbo_blitter;
  reference_counted_ptr<AtlasBlitter> m_atlas_blitter;
  astral_GLuint m_texture, m_shadow_sampler, m_linear_sampler;

  AtlasBlitter::Texture m_gpu_texture;
  std::vector<AtlasBlitter::BlitRect> m_src_rects, m_dst_rects;
};

#endif

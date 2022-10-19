/*!
 * \file render_engine_gl3_fbo_blitter.hpp
 * \brief file render_engine_gl3_fbo_blitter.hpp
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

#ifndef ASTRAL_RENDER_ENGINE_GL3_FBO_BLITTER_HPP
#define ASTRAL_RENDER_ENGINE_GL3_FBO_BLITTER_HPP

#include <astral/util/gl/astral_gl.hpp>
#include <astral/renderer/gl3/render_engine_gl3.hpp>

#include "render_engine_gl3_implement.hpp"

/* Class to blit content from one texture to another via
 * FBO blits. Includes implementation details to workaround
 * various GL driver bugs encountered in the wild.
 */
class astral::gl::RenderEngineGL3::Implement::FBOBlitter:public reference_counted<FBOBlitter>::non_concurrent
{
public:
  explicit
  FBOBlitter(int number_clip_planes);

  ~FBOBlitter();

  /* Copy the contents from one texture to another.
   * \param buffer must be ASTRAL_GL_COLOR_BUFFER_BIT or ASTRAL_GL_DEPTH_BUFFER_BIT
   * \param src_texture texture from which to blit
   * \param dst_texture texture to which to blit
   * \param region width and height (starting at (0, 0)) to blit
   * \param number_layers if non-negative then both src_texture and
   *                      dst_texture are ASTRAL_GL_TEXTURE_2D_ARRAY textures
   *                      and blit the layers [0, number_layers). If
   *                      negative then both src_texture and dst_texture
   *                      are ASTRAL_GL_TEXTURE_2D textures.
   */
  void
  blit(astral_GLbitfield buffer,
       astral_GLuint src_texture, astral_GLuint dst_texture,
       ivec2 region, int number_layers = -1);

private:
  int m_number_clip_planes;
  astral_GLuint m_draw_fbo, m_read_fbo;
};

#endif

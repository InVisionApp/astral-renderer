/*!
 * \file render_engine_gl3_fbo_blitter.cpp
 * \brief file render_engine_gl3_fbo_blitter.cpp
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

#include "render_engine_gl3_fbo_blitter.hpp"

astral::gl::RenderEngineGL3::Implement::FBOBlitter::
FBOBlitter(int number_clip_planes):
  m_number_clip_planes(number_clip_planes),
  m_draw_fbo(0u),
  m_read_fbo(0u)
{
  astral_glGenFramebuffers(1, &m_read_fbo);
  astral_glGenFramebuffers(1, &m_draw_fbo);

  ASTRALassert(m_read_fbo != 0u);
  ASTRALassert(m_draw_fbo != 0u);
}

astral::gl::RenderEngineGL3::Implement::FBOBlitter::
~FBOBlitter()
{
  astral_glDeleteFramebuffers(1, &m_read_fbo);
  astral_glDeleteFramebuffers(1, &m_draw_fbo);
}

void
astral::gl::RenderEngineGL3::Implement::FBOBlitter::
blit(astral_GLbitfield buffer,
     astral_GLuint src_texture, astral_GLuint dst_texture,
     ivec2 region, int number_layers)
{
  astral_GLenum attach_pt;

  if (number_layers == 0 || region.x() <= 0 || region.y() <= 0)
    {
      return;
    }

  /* The GL specification states that the only pixel tests
   * that matter are the scissor test and pixel ownership
   * test for glBlitFramebuffer (for render-to-texture
   * the pixel owner ship test always passes). However,
   * some GL implementations bork glBlitFramebuffer. For
   * example, the blits fail on Apple MacBook Pro if
   * the hardware clip planes are active. We force all
   * HW clip planes to be off along with any enable's
   * that would bork a blit via a shader as well.
   */
  if (buffer == ASTRAL_GL_COLOR_BUFFER_BIT)
    {
      attach_pt = ASTRAL_GL_COLOR_ATTACHMENT0;
      astral_glColorMask(ASTRAL_GL_TRUE, ASTRAL_GL_TRUE, ASTRAL_GL_TRUE, ASTRAL_GL_TRUE);
    }
  else
    {
      ASTRALassert(buffer == ASTRAL_GL_DEPTH_BUFFER_BIT);
      attach_pt = ASTRAL_GL_DEPTH_STENCIL_ATTACHMENT;
      astral_glDepthMask(ASTRAL_GL_TRUE);
    }
  astral_glDisable(ASTRAL_GL_SCISSOR_TEST);
  astral_glDisable(ASTRAL_GL_DEPTH_TEST);
  astral_glDisable(ASTRAL_GL_STENCIL_TEST);
  astral_glDisable(ASTRAL_GL_BLEND);
  for (int i = 0; i < m_number_clip_planes; ++i)
    {
      astral_glDisable(ASTRAL_GL_CLIP_DISTANCE0 + i);
    }

  astral_glBindFramebuffer(ASTRAL_GL_DRAW_FRAMEBUFFER, m_draw_fbo);
  astral_glBindFramebuffer(ASTRAL_GL_READ_FRAMEBUFFER, m_read_fbo);
  if (number_layers < 0)
    {
      astral_glFramebufferTexture2D(ASTRAL_GL_READ_FRAMEBUFFER, attach_pt, ASTRAL_GL_TEXTURE_2D, src_texture, 0);
      astral_glFramebufferTexture2D(ASTRAL_GL_DRAW_FRAMEBUFFER, attach_pt, ASTRAL_GL_TEXTURE_2D, dst_texture, 0);
      astral_glBlitFramebuffer(0, 0, region.x(), region.y(),
                               0, 0, region.x(), region.y(),
                               buffer,
                               ASTRAL_GL_NEAREST);

      /* make sure we detatch to allow for the texture memory
       * to be released by the GL driver if the texture is
       * deleted.
       */
      astral_glFramebufferTexture2D(ASTRAL_GL_READ_FRAMEBUFFER, attach_pt, ASTRAL_GL_TEXTURE_2D, 0, 0);
      astral_glFramebufferTexture2D(ASTRAL_GL_DRAW_FRAMEBUFFER, attach_pt, ASTRAL_GL_TEXTURE_2D, 0, 0);
    }
  else
    {
      for (int d = 0; d < number_layers; ++d)
        {
          astral_glFramebufferTextureLayer(ASTRAL_GL_READ_FRAMEBUFFER,
                                           attach_pt,
                                           src_texture, 0, d);
          astral_glFramebufferTextureLayer(ASTRAL_GL_DRAW_FRAMEBUFFER,
                                           attach_pt,
                                           dst_texture, 0, d);
          astral_glBlitFramebuffer(0, 0, region.x(), region.y(),
                                   0, 0, region.x(), region.y(),
                                   buffer,
                                   ASTRAL_GL_NEAREST);
        }

      /* make sure we detatch to allow for the texture memory
       * to be released by the GL driver if the texture is
       * deleted.
       */
      astral_glFramebufferTextureLayer(ASTRAL_GL_READ_FRAMEBUFFER, attach_pt, 0, 0, 0);
      astral_glFramebufferTextureLayer(ASTRAL_GL_DRAW_FRAMEBUFFER, attach_pt, 0, 0, 0);
    }
}

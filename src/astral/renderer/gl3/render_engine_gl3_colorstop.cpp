/*!
 * \file render_engine_gl3_colorstop.cpp
 * \brief file render_engine_gl3_colorstop.cpp
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

#include <astral/util/gl/gl_get.hpp>
#include "render_engine_gl3_colorstop.hpp"

///////////////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::ColorStopSequenceBacking methods
astral::gl::RenderEngineGL3::Implement::ColorStopSequenceBacking::
ColorStopSequenceBacking(FBOBlitter &blitter,
                         unsigned int log2_per_layer_width, unsigned int initial_number_layers):
  ColorStopSequenceAtlasBacking(initial_number_layers, (1u << log2_per_layer_width)),
  m_texture(0),
  m_blitter(&blitter)
{
  if (initial_number_layers > 0)
    {
      astral_glGenTextures(1, &m_texture);
      ASTRALassert(m_texture != 0u);
      create_storage(layer_dimensions(), number_layers());
    }
}

astral::gl::RenderEngineGL3::Implement::ColorStopSequenceBacking::
~ColorStopSequenceBacking()
{
  astral_glDeleteTextures(1, &m_texture);
}

void
astral::gl::RenderEngineGL3::Implement::ColorStopSequenceBacking::
create_storage(unsigned int width, unsigned int height)
{
  astral_glBindTexture(ASTRAL_GL_TEXTURE_2D, m_texture);
  astral_glTexStorage2D(ASTRAL_GL_TEXTURE_2D, 1u, ASTRAL_GL_RGBA8, width, height);
  astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D, ASTRAL_GL_TEXTURE_MIN_FILTER, ASTRAL_GL_LINEAR);
  astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D, ASTRAL_GL_TEXTURE_MAG_FILTER, ASTRAL_GL_LINEAR);
  astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D, ASTRAL_GL_TEXTURE_WRAP_S, ASTRAL_GL_CLAMP_TO_EDGE);
  astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D, ASTRAL_GL_TEXTURE_WRAP_T, ASTRAL_GL_CLAMP_TO_EDGE);
}

void
astral::gl::RenderEngineGL3::Implement::ColorStopSequenceBacking::
load_pixels(int layer, int start, c_array<const u8vec4> pixels)
{
  astral_glBindTexture(ASTRAL_GL_TEXTURE_2D, m_texture);
  astral_glTexSubImage2D(ASTRAL_GL_TEXTURE_2D, 0,
                         start, layer,
                         pixels.size(), 1,
                         ASTRAL_GL_RGBA, ASTRAL_GL_UNSIGNED_BYTE,
                         pixels.c_ptr());
}

unsigned int
astral::gl::RenderEngineGL3::Implement::ColorStopSequenceBacking::
on_resize(unsigned int L)
{
  astral_GLuint src(m_texture);
  int return_value(L);

  m_texture = 0u;
  astral_glGenTextures(1, &m_texture);
  ASTRALassert(m_texture != 0u);
  create_storage(layer_dimensions(), return_value);

  m_blitter->blit(ASTRAL_GL_COLOR_BUFFER_BIT,
                  src, m_texture,
                  ivec2(layer_dimensions(), number_layers()));

  astral_glDeleteTextures(1, &src);
  return return_value;
}

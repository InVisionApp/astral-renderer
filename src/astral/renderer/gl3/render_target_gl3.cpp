/*!
 * \file render_target_gl3.cpp
 * \brief file render_target_gl3.cpp
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
#include <astral/util/gl/gl_get.hpp>
#include <astral/util/gl/gl_context_properties.hpp>
#include <astral/util/gl/wasm_missing_gl_enums.hpp>
#include <astral/renderer/gl3/render_target_gl3.hpp>

namespace
{
  class FakeColorBuffer:public astral::ColorBuffer
  {
  public:
    explicit
    FakeColorBuffer(astral::ivec2 sz):
      astral::ColorBuffer(sz)
    {}
  };

  class FakeDepthStencilBuffer:public astral::DepthStencilBuffer
  {
  public:
    explicit
    FakeDepthStencilBuffer(astral::ivec2 sz):
      astral::DepthStencilBuffer(sz)
    {}
  };
}

//////////////////////////////////////
// astral::gl::TextureHolder methods
astral::gl::TextureHolder::
~TextureHolder()
{
  if (m_dtor_behaviour == delete_texture_on_dtor)
    {
      astral_glDeleteTextures(1, &m_texture);
    }
}

astral::gl::TextureHolder::
TextureHolder(astral_GLenum internal_format,
              ivec2 sz,
              astral_GLenum min_filter,
              astral_GLenum mag_filter,
              unsigned int number_lod,
              enum dtor_behaviour_t dtor_behaviour):
  m_texture(0u),
  m_dtor_behaviour(dtor_behaviour)
{
  astral_glGenTextures(1, &m_texture);
  ASTRALassert(m_texture != 0);

  astral_glBindTexture(ASTRAL_GL_TEXTURE_2D, m_texture);
  astral_glTexStorage2D(ASTRAL_GL_TEXTURE_2D, number_lod, internal_format, sz.x(), sz.y());
  astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D, ASTRAL_GL_TEXTURE_MIN_FILTER, min_filter);
  astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D, ASTRAL_GL_TEXTURE_MAG_FILTER, mag_filter);
  astral_glBindTexture(ASTRAL_GL_TEXTURE_2D, 0);
}

astral::gl::TextureHolder::
TextureHolder(astral_GLenum internal_format,
              ivec3 sz,
              astral_GLenum min_filter,
              astral_GLenum mag_filter,
              unsigned int number_lod,
              enum dtor_behaviour_t dtor_behaviour):
  m_texture(0u),
  m_dtor_behaviour(dtor_behaviour)
{
  astral_glGenTextures(1, &m_texture);
  ASTRALassert(m_texture != 0);

  astral_glBindTexture(ASTRAL_GL_TEXTURE_2D_ARRAY, m_texture);
  astral_glTexStorage3D(ASTRAL_GL_TEXTURE_2D_ARRAY, number_lod, internal_format, sz.x(), sz.y(), sz.z());
  astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D_ARRAY, ASTRAL_GL_TEXTURE_MIN_FILTER, min_filter);
  astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D_ARRAY, ASTRAL_GL_TEXTURE_MAG_FILTER, mag_filter);
  astral_glBindTexture(ASTRAL_GL_TEXTURE_2D_ARRAY, 0);
}

/////////////////////////////////////////
// astral::gl::ColorBufferGL methods
astral::reference_counted_ptr<astral::gl::ColorBufferGL>
astral::gl::ColorBufferGL::
create(ivec2 sz, astral_GLenum min_filter, astral_GLenum mag_filter)
{
  ASTRALassert(min_filter == ASTRAL_GL_LINEAR || min_filter == ASTRAL_GL_NEAREST);
  ASTRALassert(mag_filter == ASTRAL_GL_LINEAR || mag_filter == ASTRAL_GL_NEAREST);

  reference_counted_ptr<TextureHolder> ptexture;

  ptexture = TextureHolder::create(ASTRAL_GL_RGBA8, sz, min_filter, mag_filter, 1);
  return ASTRALnew ColorBufferGL(sz, ptexture, -1);
}

/////////////////////////////////////////
// astral::gl::DepthStencilBufferGL methods
astral::reference_counted_ptr<astral::gl::DepthStencilBufferGL>
astral::gl::DepthStencilBufferGL::
create(ivec2 sz, astral_GLenum min_filter, astral_GLenum mag_filter)
{
  ASTRALassert(min_filter == ASTRAL_GL_LINEAR || min_filter == ASTRAL_GL_NEAREST);
  ASTRALassert(mag_filter == ASTRAL_GL_LINEAR || mag_filter == ASTRAL_GL_NEAREST);

  reference_counted_ptr<TextureHolder> ptexture;

  ptexture = TextureHolder::create(ASTRAL_GL_DEPTH24_STENCIL8, sz, min_filter, mag_filter, 1);
  return ASTRALnew DepthStencilBufferGL(sz, ptexture, -1);
}

/////////////////////////////////////////////
// astral::gl::RenderTargetGL methods
void
astral::gl::RenderTargetGL::
read_color_buffer_implement(ivec2 read_location, ivec2 read_size, c_array<u8vec4> dst) const
{
  /* flip the y-coordinate if necessary */
  if (m_y_coordinate_convention == pixel_y_zero_is_bottom)
    {
      int size_y(size().y());

      /* The request is to read the pixels where
       *  top = read_location.y()
       *  bottom =  read_location.y() + read_size.y() - 1
       *
       * in Astral coordinates. In GL coordinates that becomes
       *
       *  top =  size_y - 1 - top
       *  bottom = size_y - 1 - (read_location.y() + read_size.y() - 1)
       *
       * where bottom is the value to pass to glReadPixels(). Simplifying,
       * we get
       *
       *  bottom = size_y - (read_location.y() + read_size.y())
       */
      read_location.y() = size_y - (read_location.y() + read_size.y());
    }
  astral_glBindFramebuffer(ASTRAL_GL_READ_FRAMEBUFFER, fbo());
  astral_glBindBuffer(ASTRAL_GL_PIXEL_PACK_BUFFER, 0u);
  astral_glPixelStorei(ASTRAL_GL_PACK_ROW_LENGTH, 0);
  astral_glPixelStorei(ASTRAL_GL_PACK_SKIP_PIXELS, 0);
  astral_glPixelStorei(ASTRAL_GL_PACK_SKIP_ROWS, 0);
  astral_glPixelStorei(ASTRAL_GL_PACK_ALIGNMENT, 4);

  if (!ContextProperties::is_es())
    {
      /* these are present in desktop GL only */
      astral_glPixelStorei(ASTRAL_GL_PACK_IMAGE_HEIGHT, 0);
      astral_glPixelStorei(ASTRAL_GL_PACK_SKIP_IMAGES, 0);
      astral_glPixelStorei(ASTRAL_GL_PACK_LSB_FIRST, ASTRAL_GL_FALSE);
      astral_glPixelStorei(ASTRAL_GL_PACK_SWAP_BYTES, ASTRAL_GL_FALSE);
    }

  astral_glReadPixels(read_location.x(), read_location.y(), read_size.x(), read_size.y(), ASTRAL_GL_RGBA, ASTRAL_GL_UNSIGNED_BYTE, &dst[0]);

  /* flip it if necessary */
  if (m_y_coordinate_convention == pixel_y_zero_is_bottom)
    {
      int a, b;
      std::vector<u8vec4> tmp(read_size.x());

      for (a = 0, b = read_size.y() - 1; a < b; ++a, --b)
        {
          // copy the pixels at y = a into tmp
          std::memcpy(&tmp[0], &dst[a * read_size.x()], sizeof(u8vec4) * read_size.x());

          // copy the pixels at y = b to the pixels at y = a
          std::memcpy(&dst[a * read_size.x()], &dst[b * read_size.x()], sizeof(u8vec4) * read_size.x());

          // copy the pixels from tmp to y = b
          std::memcpy(&dst[b * read_size.x()], &tmp[0], sizeof(u8vec4) * read_size.x());
        }
    }
}

//////////////////////////////////////////////
// astral::gl::RenderTargetGL_Texture methods
astral::gl::RenderTargetGL_Texture::
RenderTargetGL_Texture(astral_GLuint fbo,
                       const reference_counted_ptr<ColorBufferGL> &cb,
                       const reference_counted_ptr<DepthStencilBufferGL> &ds):
  RenderTargetGL(cb, ds),
  m_fbo(fbo)
{
  if (cb)
    {
      m_color_texture = cb->texture();
    }

  if (ds)
    {
      m_depth_texture = ds->texture();
    }
}

astral::gl::RenderTargetGL_Texture::
~RenderTargetGL_Texture()
{
  astral_glDeleteFramebuffers(1, &m_fbo);
}

astral::reference_counted_ptr<astral::gl::RenderTargetGL_Texture>
astral::gl::RenderTargetGL_Texture::
create(const reference_counted_ptr<ColorBufferGL> &cb,
       const reference_counted_ptr<DepthStencilBufferGL> &ds)
{
  astral_GLuint tmp, fbo;

  /* we take the highest LOD, i.e. mipmap level 0. */
  astral_GLint level(0);

  /* We use ASTRAL_GL_READ_FRAMEBUFFER_BINDING at the FBO target
   * so that we do not effect to what FBO is begin drawn.
   * This sillyness could be avoided if we were using GL 4.5
   * or the extension GL_ARB_direct_state_access.
   */
  tmp = context_get<astral_GLint>(ASTRAL_GL_READ_FRAMEBUFFER_BINDING);

  astral_glGenFramebuffers(1, &fbo);
  astral_glBindFramebuffer(ASTRAL_GL_READ_FRAMEBUFFER, fbo);
  if (cb)
    {
      if (cb->bind_target() == ASTRAL_GL_TEXTURE_2D)
        {
          astral_glFramebufferTexture2D(ASTRAL_GL_READ_FRAMEBUFFER, ASTRAL_GL_COLOR_ATTACHMENT0,
                                        ASTRAL_GL_TEXTURE_2D, cb->texture()->texture(), level);
        }
      else
        {
          astral_glFramebufferTextureLayer(ASTRAL_GL_READ_FRAMEBUFFER, ASTRAL_GL_COLOR_ATTACHMENT0,
                                           cb->texture()->texture(), level, cb->layer());
        }
    }
  else
    {
      astral_glFramebufferTexture2D(ASTRAL_GL_READ_FRAMEBUFFER, ASTRAL_GL_COLOR_ATTACHMENT0,
                                    ASTRAL_GL_TEXTURE_2D, 0, level);
    }

  if (ds)
    {
      if (ds->bind_target() == ASTRAL_GL_TEXTURE_2D)
        {
          astral_glFramebufferTexture2D(ASTRAL_GL_READ_FRAMEBUFFER, ASTRAL_GL_DEPTH_STENCIL_ATTACHMENT,
                                        ASTRAL_GL_TEXTURE_2D, ds->texture()->texture(), level);
        }
      else
        {
          astral_glFramebufferTextureLayer(ASTRAL_GL_READ_FRAMEBUFFER, ASTRAL_GL_DEPTH_STENCIL_ATTACHMENT,
                                           ds->texture()->texture(), level, ds->layer());
        }
    }
  else
    {
      astral_glFramebufferTexture2D(ASTRAL_GL_READ_FRAMEBUFFER, ASTRAL_GL_DEPTH_STENCIL_ATTACHMENT,
                                    ASTRAL_GL_TEXTURE_2D, 0, level);
    }

  astral_glBindFramebuffer(ASTRAL_GL_READ_FRAMEBUFFER, tmp);

  return ASTRALnew RenderTargetGL_Texture(fbo, cb, ds);
}

/////////////////////////////////////////////////
// astral::gl::RenderTargetGL_DefaultFBO methods
astral::reference_counted_ptr<astral::gl::RenderTargetGL_DefaultFBO>
astral::gl::RenderTargetGL_DefaultFBO::
create(ivec2 sz)
{
  return ASTRALnew RenderTargetGL_DefaultFBO(ASTRALnew FakeColorBuffer(sz),
                                             ASTRALnew FakeDepthStencilBuffer(sz));
}

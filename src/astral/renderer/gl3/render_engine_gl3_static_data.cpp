/*!
 * \file render_engine_gl3_static_data.cpp
 * \brief file render_engine_gl3_static_data.cpp
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
#include "render_engine_gl3_static_data.hpp"

#ifndef __EMSCRIPTEN__

///////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::StaticDataBackingBufferObject methods
astral::gl::RenderEngineGL3::Implement::StaticDataBackingBufferObject::
StaticDataBackingBufferObject(enum type_t tp, unsigned int initial_size):
  StaticDataBackingBase(tp, initial_size, ASTRAL_GL_TEXTURE_BUFFER),
  m_buffer(0u)
{
  astral_glGenBuffers(1, &m_buffer);
  ASTRALassert(m_buffer != 0u);

  if (tp == type32)
    {
      m_unit_size = sizeof(gvec4);
      m_internal_format = ASTRAL_GL_RGBA32UI;
    }
  else
    {
      m_unit_size = sizeof(u16vec4);
      m_internal_format = ASTRAL_GL_RG32UI;
    }

  astral_glBindBuffer(ASTRAL_GL_COPY_WRITE_BUFFER, m_buffer);
  astral_glBufferData(ASTRAL_GL_COPY_WRITE_BUFFER, m_unit_size * initial_size, nullptr, ASTRAL_GL_STATIC_DRAW);
  astral_glBindBuffer(ASTRAL_GL_COPY_WRITE_BUFFER, 0u);
  create_texture_buffer();
}

astral::gl::RenderEngineGL3::Implement::StaticDataBackingBufferObject::
~StaticDataBackingBufferObject()
{
  ASTRALassert(m_texture != 0u);
  ASTRALassert(m_buffer != 0u);
  astral_glDeleteTextures(1, &m_texture);
  astral_glDeleteBuffers(1, &m_buffer);
}

unsigned int
astral::gl::RenderEngineGL3::Implement::StaticDataBackingBufferObject::
enlarge_implement(unsigned int new_size)
{
  unsigned int old_size(size());
  astral_GLuint old_buffer(m_buffer);

  m_buffer = 0;
  astral_glGenBuffers(1, &m_buffer);
  ASTRALassert(m_buffer != 0u);

  astral_glBindBuffer(ASTRAL_GL_COPY_WRITE_BUFFER, m_buffer);
  astral_glBufferData(ASTRAL_GL_COPY_WRITE_BUFFER, m_unit_size * new_size, nullptr, ASTRAL_GL_STATIC_DRAW);
  astral_glBindBuffer(ASTRAL_GL_COPY_READ_BUFFER, old_buffer);
  astral_glCopyBufferSubData(ASTRAL_GL_COPY_READ_BUFFER, ASTRAL_GL_COPY_WRITE_BUFFER, 0, 0, m_unit_size * old_size);

  /* unbind buffers */
  astral_glBindBuffer(ASTRAL_GL_COPY_WRITE_BUFFER, 0u);
  astral_glBindBuffer(ASTRAL_GL_COPY_READ_BUFFER, 0u);

  /* delete the old TBO and its backing BO */
  ASTRALassert(m_texture != 0u);
  astral_glDeleteTextures(1, &m_texture);
  astral_glDeleteBuffers(1, &old_buffer);

  m_texture = 0u;
  create_texture_buffer();

  return new_size;
}

void
astral::gl::RenderEngineGL3::Implement::StaticDataBackingBufferObject::
set_data_implement(unsigned int offset, const void *data, unsigned int count)
{
  if (count > 0)
    {
      astral_glBindBuffer(ASTRAL_GL_COPY_WRITE_BUFFER, m_buffer);
      astral_glBufferSubData(ASTRAL_GL_COPY_WRITE_BUFFER, m_unit_size * offset, m_unit_size * count, data);
    }
}

void
astral::gl::RenderEngineGL3::Implement::StaticDataBackingBufferObject::
create_texture_buffer(void)
{
  ASTRALassert(m_texture == 0u);

  astral_glGenTextures(1, &m_texture);
  ASTRALassert(m_texture != 0u);

  astral_glBindTexture(ASTRAL_GL_TEXTURE_BUFFER, m_texture);
  astral_glTexBuffer(ASTRAL_GL_TEXTURE_BUFFER, m_internal_format, m_buffer);
}

#endif

/////////////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::StaticDataBackingTexture methods
astral::gl::RenderEngineGL3::Implement::StaticDataBackingTexture::
StaticDataBackingTexture(enum type_t tp, FBOBlitter &fbo_blitter,
                         unsigned int log2_width,
                         unsigned int log2_max_height,
                         unsigned int init_size):
  StaticDataBackingBase(tp, compute_size(log2_width, log2_max_height, init_size), ASTRAL_GL_TEXTURE_2D_ARRAY),
  m_log2_width(log2_width),
  m_log2_max_height(log2_max_height),
  m_width(1u << log2_width),
  m_max_height(1u << log2_max_height),
  m_fbo_blitter(&fbo_blitter)
{
  if (tp == type32)
    {
      /* It might be tempting to use ASTRAL_GL_RGBA32F, but there are
       * two issues with it:
       *  - on WebGL2, support for rendering to a floating point
       *    buffer is an extension, so it may not be available
       *  - glBlitFramebuffer does odd things with values outside
       *    of the range [0, 1] with fp32 render surfaces on
       *    atleast one tested platform.
       */
      m_internal_format = ASTRAL_GL_RGBA32UI;
      m_external_format = ASTRAL_GL_RGBA_INTEGER;
      m_external_type = ASTRAL_GL_UNSIGNED_INT;
    }
  else
    {
      /* Given the above, using an fp16 surface
       * has the same issues; so use a uint format
       */
      m_internal_format = ASTRAL_GL_RG32UI;
      m_external_format = ASTRAL_GL_RG_INTEGER;
      m_external_type = ASTRAL_GL_UNSIGNED_INT;
    }

  compute_dimensions(size(), &m_height, &m_depth);
  ASTRALassert(size() == m_height * m_width * m_depth);

  create_storage(m_height, m_depth);
}

astral::gl::RenderEngineGL3::Implement::StaticDataBackingTexture::
~StaticDataBackingTexture()
{
  astral_glDeleteTextures(1, &m_texture);
}

unsigned int
astral::gl::RenderEngineGL3::Implement::StaticDataBackingTexture::
compute_size(unsigned int log2_width,
             unsigned int max_log2_height,
             unsigned int init_size)
{
  unsigned int w, h, max_h;

  w = 1u << log2_width;
  max_h = 1u << max_log2_height;

  h = init_size >> log2_width;
  if (w * h < init_size)
    {
      ++h;
    }

  if (h > max_h)
    {
      unsigned int d;

      d = h >> max_log2_height;
      if (d * max_h < h)
        {
          ++d;
        }

      ASTRALassert(h <= d * max_h);
      h = d * max_h;
    }

  ASTRALassert(w * h >= init_size);
  return w * h;
}

void
astral::gl::RenderEngineGL3::Implement::StaticDataBackingTexture::
compute_dimensions(unsigned int in_size,
                   unsigned int *out_height,
                   unsigned int *out_depth)
{
  unsigned int h, d;

  h = in_size >> m_log2_width;
  if (m_width * h < in_size)
    {
      ++h;
    }

  if (h > m_max_height)
    {
      d = h >> m_log2_max_height;
      if (d * m_max_height < h)
        {
          ++d;
        }
      ASTRALassert(d * m_max_height >= h);
      h = m_max_height;
    }
  else
    {
      d = 1u;
    }

  ASTRALassert(d * h * m_width >= in_size);

  *out_height = h;
  *out_depth = d;
}

void
astral::gl::RenderEngineGL3::Implement::StaticDataBackingTexture::
create_storage(unsigned int height, unsigned int depth)
{
  ASTRALassert(m_texture == 0u);

  astral_glGenTextures(1, &m_texture);
  ASTRALassert(m_texture != 0u);

  ASTRALassert(height <= m_max_height);
  ASTRALassert(depth == 1u || (depth > 1u && height == m_max_height));

  astral_glBindTexture(ASTRAL_GL_TEXTURE_2D_ARRAY, m_texture);
  astral_glTexStorage3D(ASTRAL_GL_TEXTURE_2D_ARRAY, 1, m_internal_format,
                        m_width, height, depth);
  astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D_ARRAY, ASTRAL_GL_TEXTURE_MIN_FILTER, ASTRAL_GL_NEAREST);
  astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D_ARRAY, ASTRAL_GL_TEXTURE_MAG_FILTER, ASTRAL_GL_NEAREST);
  astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D_ARRAY, ASTRAL_GL_TEXTURE_WRAP_S, ASTRAL_GL_CLAMP_TO_EDGE);
  astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D_ARRAY, ASTRAL_GL_TEXTURE_WRAP_T, ASTRAL_GL_CLAMP_TO_EDGE);
}

unsigned int
astral::gl::RenderEngineGL3::Implement::StaticDataBackingTexture::
enlarge_implement(unsigned int new_size)
{
  unsigned int old_depth(m_depth), old_height(m_height);
  astral_GLuint old_texture(m_texture);

  compute_dimensions(new_size, &m_height, &m_depth);

  m_texture = 0;
  create_storage(m_height, m_depth);

  m_fbo_blitter->blit(ASTRAL_GL_COLOR_BUFFER_BIT,
                      old_texture, m_texture,
                      ivec2(m_width, old_height),
                      old_depth);

  // delete the old texture
  astral_glDeleteTextures(1, &old_texture);

  return m_depth * m_width * m_height;
}

template<typename T>
void
astral::gl::RenderEngineGL3::Implement::StaticDataBackingTexture::
set_data_implement(unsigned int offset, c_array<const T> data)
{
  uvec3 xyz;

  /* TODO: instead of uploading directly, create a staging
   *       buffer which is uploaded to a staging texture
   *       which is then blitted with a single draw call
   *       holding many rects.
   */

  astral_glBindTexture(ASTRAL_GL_TEXTURE_2D_ARRAY, m_texture);
  xyz = coordinate_from_offset(offset);

  /* Break the upload into 3 chunks:
   *  - the start scan-line if it is partial
   *  - all of the full scan lines together
   *  - the last scan-line if it is partial
   */
  if (xyz.x() != 0u)
    {
      unsigned int num;

      num = data.size();
      num = t_min(m_width - xyz.x(), num);

      ASTRALassert(xyz.x() + num <= m_width);
      ASTRALassert(xyz.y() < m_height);
      ASTRALassert(xyz.z() < m_depth);

      astral_glTexSubImage3D(ASTRAL_GL_TEXTURE_2D_ARRAY, 0,
                             xyz.x(), xyz.y(), xyz.z(),
                             num, 1, 1,
                             m_external_format, m_external_type,
                             data.c_ptr());

      xyz.x() = 0;
      ++xyz.y();
      if (xyz.y() == m_height)
        {
          xyz.y() = 0;
          ++xyz.z();
        }
      data = data.sub_array(num);
    }

  while (data.size() >= m_width)
    {
      unsigned int num, h;

      num = data.size();

      h = num >> m_log2_width;
      h = t_min(h, m_height - xyz.y());

      num = m_width * h;

      ASTRALassert(xyz.x() == 0);
      ASTRALassert(xyz.y() + h <= m_height);
      ASTRALassert(xyz.z() < m_depth);
      ASTRALassert(num <= data.size());
      ASTRALassert(num % m_width == 0u);
      ASTRALassert(m_width * h == num);

      astral_glTexSubImage3D(ASTRAL_GL_TEXTURE_2D_ARRAY, 0,
                             xyz.x(), xyz.y(), xyz.z(),
                             m_width, h, 1,
                             m_external_format, m_external_type,
                             data.c_ptr());

      xyz.y() += h;
      if (xyz.y() == m_height)
        {
          xyz.y() = 0u;
          ++xyz.z();
        }

      data = data.sub_array(num);
    }

  ASTRALassert(data.size() < m_width);
  if (!data.empty())
    {
      ASTRALassert(xyz.x() == 0);
      ASTRALassert(xyz.y() < m_height);
      ASTRALassert(xyz.z() < m_depth);

      astral_glTexSubImage3D(ASTRAL_GL_TEXTURE_2D_ARRAY, 0,
                             xyz.x(), xyz.y(), xyz.z(),
                             data.size(), 1, 1,
                             m_external_format, m_external_type,
                             data.c_ptr());
    }
}

void
astral::gl::RenderEngineGL3::Implement::StaticDataBackingTexture::
set_data_implement(unsigned int offset, const void *pdata, unsigned int count)
{
  if (count == 0)
    {
      return;
    }

  if (type() == type32)
    {
      c_array<const uvec4> data;

      data = c_array<const uvec4>(static_cast<const uvec4*>(pdata), count);
      set_data_implement<uvec4>(offset, data);
    }
  else
    {
      c_array<const u16vec4> data;

      data = c_array<const u16vec4>(static_cast<const u16vec4*>(pdata), count);
      set_data_implement<u16vec4>(offset, data);
    }
}

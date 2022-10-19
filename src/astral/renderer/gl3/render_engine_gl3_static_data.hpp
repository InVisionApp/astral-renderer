/*!
 * \file render_engine_gl3_static_data.hpp
 * \brief file render_engine_gl3_static_data.hpp
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

#ifndef ASTRAL_RENDER_ENGINE_GL3_SHARED_DATA_HPP
#define ASTRAL_RENDER_ENGINE_GL3_SHARED_DATA_HPP

#include <astral/util/gl/astral_gl.hpp>
#include <astral/renderer/static_data.hpp>
#include <astral/renderer/gl3/render_engine_gl3.hpp>
#include "render_engine_gl3_fbo_blitter.hpp"

/*!
 * \brief
 * Common base class for the GL3 backend's different implementations
 * of astral::StaticDataBacking
 */
class astral::gl::RenderEngineGL3::Implement::StaticDataBackingBase:
  public astral::StaticDataBacking
{
public:
  /*!
   * Ctor. Indicates that backing is realized as a linear buffer.
   * \param initial_size initial size (in units of 4-tuples of type tp) of the buffer,
   *                     i.e. if tp is StaticDataBacking::type_generic, then in
   *                     uints of 16-bytes (a four tuple of 32-bit values) and if tp is
   *                     StaticDataBacking::type_fp16, then in units of 8-bytes (a four
   *                     tuple of 16-bit values).
   */
  StaticDataBackingBase(enum type_t tp, unsigned int initial_size, astral_GLenum pbinding_point):
    astral::StaticDataBacking(tp, initial_size),
    m_texture(0u),
    m_binding_point(pbinding_point)
  {}

  /*!
   * Gives the binding point for texture().
   */
  astral_GLenum
  binding_point(void) const
  {
    return m_binding_point;
  }

  /*!
   * Returns the GL name of the backing GL texture
   */
  astral_GLuint
  texture(void) const
  {
    return m_texture;
  }

protected:
  /* Derived class needs to manage the GL texture;
   * this includes both creation and destruction.
   */
  astral_GLuint m_texture;

private:
  astral_GLenum m_binding_point;
};

#ifndef __EMSCRIPTEN__

/*!
 * \brief
 * An astral::gl::RenderEngineGL3::Implement::StaticDataBackingBufferObject
 * implements astral::StaticDataBacking as a GL buffer object.
 * This requires that the GL/GLES used supports texture buffer objects.
 */
class astral::gl::RenderEngineGL3::Implement::StaticDataBackingBufferObject:
  public astral::gl::RenderEngineGL3::Implement::StaticDataBackingBase
{
public:
  /*!
   * Ctor
   * \param tp type of data to back
   * \param initial_size initial size of the linear array
   */
  explicit
  StaticDataBackingBufferObject(enum type_t tp, unsigned int initial_size);

  ~StaticDataBackingBufferObject();

private:
  virtual
  unsigned int
  enlarge_implement(unsigned int new_size) override;

  virtual
  void
  set_data_implement(unsigned int offset, const void *data, unsigned int count) override;

  void
  create_texture_buffer(void);

  unsigned int m_unit_size;
  astral_GLenum m_internal_format;
  astral_GLuint m_buffer;
};

#endif

/*!
 * \brief
 * An astral::gl::RenderEngineGL3::Implement::StaticDataBackingTexture
 * implements astral::StaticDataBacking as a GL texture
 */
class astral::gl::RenderEngineGL3::Implement::StaticDataBackingTexture:
  public astral::gl::RenderEngineGL3::Implement::StaticDataBackingBase
{
public:
  /*!
   * Ctor.
   * \param tp type of data to back
   * \param fbo_blitter fbo-blitter used on resize
   * \param log2_width the log2 of the width of the texture, the
   *                   width is constant for the lifetime of the
   *                   constructed object
   * \param max_log2_height the log2 of the max-height of the texture
   * \param init_size initial size of the buffer
   */
  StaticDataBackingTexture(enum type_t tp, FBOBlitter &fbo_blitter,
                           unsigned int log2_width,
                           unsigned int max_log2_height,
                           unsigned int init_size);

  ~StaticDataBackingTexture();

private:
  /*!
   * Returns the texel coordinate of an offset
   * \param L linear offset
   */
  uvec3
  coordinate_from_offset(unsigned int L) const
  {
    uvec3 return_value;

    return_value.x() = L & (m_width - 1u);
    return_value.y() = (L >> m_log2_width) & (m_max_height - 1u);
    return_value.z() = L >> (m_log2_width + m_log2_max_height);

    return return_value;
  }

  virtual
  unsigned int
  enlarge_implement(unsigned int new_height) override final;

  virtual
  void
  set_data_implement(unsigned int offset, const void *data, unsigned int count) override final;

  template<typename T>
  void
  set_data_implement(unsigned int offset, c_array<const T> data);

  void
  create_storage(unsigned int height, unsigned int depth);

  static
  unsigned int
  compute_size(unsigned int log2_width,
               unsigned int max_log2_height,
               unsigned int init_size);

  void
  compute_dimensions(unsigned int in_size,
                     unsigned int *out_height,
                     unsigned int *out_depth);

  unsigned int m_log2_width, m_log2_max_height;
  unsigned int m_width, m_max_height, m_height, m_depth;
  astral_GLenum m_internal_format, m_external_format, m_external_type;
  reference_counted_ptr<FBOBlitter> m_fbo_blitter;
};

#endif

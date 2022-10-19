/*!
 * \file gl_vertex_attrib.hpp
 * \brief file gl_vertex_attrib.hpp
 *
 * Adapted from: opengl_trait.hpp and opengl_trait_implement of WRATH:
 *
 * Copyright 2013 by Nomovok Ltd.
 * Contact: info@nomovok.com
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#ifndef ASTRAL_GL_VERTEX_ATTRIB_HPP
#define ASTRAL_GL_VERTEX_ATTRIB_HPP

#include <astral/util/vecN.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/gl/astral_gl.hpp>

namespace astral {
namespace gl {

/*!\addtogroup gl_util
 * @{
 */

/*!
 * Converts an offset given in bytes to a const void* value
 * as expected by GL functions (for example glVertexAttribPointer)
 * \param offset offset in bytes
 */
template<typename T>
inline
const T*
offset_as_pointer(unsigned int offset)
{
  const T *return_value(nullptr);
  return_value += offset;
  return return_value;
}

/*!
 * \brief
 * Type trait struct that provides type information to feed GL commands.
 * \tparam T type from which to extract values. The struct is specicialized for
 *           each of the GL types: GLubyte, GLbyte, astral_GLuint, astral_GLint, GLushort,
 *           GLshort, astral_GLfloat and recursively for astral::vecN of those types
 */
template<typename T>
struct opengl_trait
{
  /*!
   * Typedef to template parameter
   */
  typedef T data_type;

  /*!
   * For an array type, such as vecN,
   * element type of the array, otherwise
   * is same as data_type. Note,
   * for vecN types of vecN's reports
   * the element type of inner array type,
   * for example
   * \code vecN<vecN<S, N>, M> \endcode
   * gives S for basic_type
   */
  typedef T basic_type;

  enum
    {
      /*!
       * GL type label, for example if basic_type
       * is astral_GLuint, then type is ASTRAL_GL_UNSIGNED_INT
       */
      type = ASTRAL_GL_INVALID_ENUM
    };

  enum
    {
      /*!
       * The number of basic_type
       * elements packed into one data_type
       */
      count = 1
    };

  enum
    {
      /*!
       * The space between adjacent
       * data_type elements in an array
       */
      stride = sizeof(T)
    };
};

///@cond
template<>
struct opengl_trait<astral_GLbyte>
{
  enum { type = ASTRAL_GL_BYTE };
  enum { count = 1 };
  enum { stride = sizeof(astral_GLbyte) };
  typedef astral_GLbyte basic_type;
  typedef basic_type data_type;
};

template<>
struct opengl_trait<astral_GLubyte>
{
  enum { type = ASTRAL_GL_UNSIGNED_BYTE };
  enum { count = 1 };
  enum { stride = sizeof(astral_GLubyte) };
  typedef astral_GLubyte basic_type;
  typedef basic_type data_type;
};

template<>
struct opengl_trait<astral_GLshort>
{
  enum { type = ASTRAL_GL_SHORT };
  enum { count = 1 };
  enum { stride = sizeof(astral_GLshort) };
  typedef astral_GLshort basic_type;
  typedef basic_type data_type;
};

template<>
struct opengl_trait<astral_GLushort>
{
  enum { type = ASTRAL_GL_UNSIGNED_SHORT };
  enum { count = 1 };
  enum { stride = sizeof(astral_GLushort) };
  typedef astral_GLushort basic_type;
  typedef basic_type data_type;
};

template<>
struct opengl_trait<astral_GLint>
{
  enum { type = ASTRAL_GL_INT };
  enum { count = 1 };
  enum { stride = sizeof(astral_GLint) };
  typedef astral_GLint basic_type;
  typedef basic_type data_type;
};

template<>
struct opengl_trait<astral_GLuint>
{
  enum { type = ASTRAL_GL_UNSIGNED_INT };
  enum { count = 1 };
  enum { stride = sizeof(astral_GLuint) };
  typedef astral_GLuint basic_type;
  typedef basic_type data_type;
};

template<>
struct opengl_trait<astral_GLfloat>
{
  enum { type = ASTRAL_GL_FLOAT };
  enum { count = 1 };
  enum { stride = sizeof(astral_GLfloat) };
  typedef astral_GLfloat basic_type;
  typedef basic_type data_type;
};

#ifdef ASTRAL_GL_DOUBLE
template<>
struct opengl_trait<astral_GLdouble>
{
  enum { type = ASTRAL_GL_DOUBLE };
  enum { count = 1 };
  enum { stride = sizeof(astral_GLdouble) };
  typedef astral_GLdouble basic_type;
  typedef basic_type data_type;
};
#endif

template<typename T, size_t N>
struct opengl_trait< vecN<T,N>>
{
  enum { type = opengl_trait<T>::type };
  enum { count = N * opengl_trait<T>::count };
  enum { stride = sizeof(vecN<T,N>) };

  typedef typename opengl_trait<T>::basic_type basic_type;
  typedef vecN<T,N> data_type;
};
///@endcond


/*!
 * \brief
 * Class that bundles up count, size and type parameters
 * for the GL API function glVertexAttribPointer
 */
class gl_vertex_attrib
{
public:
  /*!
   * The number of elements, \sa opengl_trait::count
   */
  astral_GLint m_count;

  /*!
   * The element type, \sa opengl_train::type
   */
  astral_GLenum m_type;

  /*!
   * The stride to the next element in the buffer
   * from which to source the vertex data
   */
  astral_GLsizei m_stride;

  /*!
   * The <i>offset</i> of the location of the
   * vertex data in the buffer from which to
   * source the vertex data
   */
  const void *m_offset;
};

/*!
 * Template function that initializes the members of
 * gl_vertex_attrib_base class from the enums of a
 * astral::opengl_trait.
 */
template<typename T>
gl_vertex_attrib
gl_vertex_attrib_value(void)
{
  gl_vertex_attrib R;
  R.m_type = opengl_trait<T>::type;
  R.m_count = opengl_trait<T>::count;
  R.m_stride = opengl_trait<T>::stride;
  R.m_offset = nullptr;
  return R;
}

/*!
 * Template function that initializes the members of
 * astral::gl_vertex_attrib class from the enums of a
 * astral::opengl_trait.
 * \param stride stride argument, override the value
 *               of opengl_trait::stride
 * \param offset offset in bytes
 */
template<typename T>
gl_vertex_attrib
gl_vertex_attrib_value(astral_GLsizei stride, astral_GLsizei offset)
{
  gl_vertex_attrib R;
  R.m_type = opengl_trait<T>::type;
  R.m_count = opengl_trait<T>::count;
  R.m_stride = stride;
  R.m_offset = offset_as_pointer<uint8_t>(offset);
  return R;
}

/*!
 * Template function that initializes the members of
 * astral::gl_vertex_attrib class from the enums of a
 * astral::opengl_trait, equivalent to
 * \code
 * gl_vertex_attrib_value<T>(sizeof(C), offset)
 * \endcode
 */
template<typename C, typename T>
gl_vertex_attrib
gl_vertex_attrib_value(astral_GLsizei offset)
{
  return gl_vertex_attrib_value<T>(sizeof(C), offset);
}

/*!
 * Provided as a conveniance, equivalent to
 * \code
 * glEnableVertexAttribArray(index);
 * glVertexAttribPointer(index, v.m_count, v.m_type, normalized, v.m_stride, v.m_offset);
 * \endcode
 * \param index which vertex
 * \param v traits for vertex
 * \param normalized if vertex data should be normalized
 */
inline
void
VertexAttribPointer(astral_GLint index, const gl_vertex_attrib &v, astral_GLboolean normalized = ASTRAL_GL_FALSE)
{
  astral_glEnableVertexAttribArray(index);
  astral_glVertexAttribPointer(index, v.m_count, v.m_type, normalized, v.m_stride, v.m_offset);
}

/*!
 * Provided as a conveniance, equivalent to
 * \code
 * glEnableVertexAttribArray(index);
 * glVertexAttribIPointer(index, v.m_count, v.m_type, v.m_stride, v.m_offset);
 * \endcode
 * \param index which vertex
 * \param v traits for vertex
 */
inline
void
VertexAttribIPointer(astral_GLint index, const gl_vertex_attrib &v)
{
  astral_glEnableVertexAttribArray(index);
  astral_glVertexAttribIPointer(index, v.m_count, v.m_type, v.m_stride, v.m_offset);
}

/*!
 * Provided as a conveniance, equivalent to
 * \code
 * glBufferData(binding_point, sizeof(T) * data.size(), data.c_ptr(), hint);
 * \endcode
 * \param binding_point GL binding point
 * \param data data with which to initialize the GL buffer object
 * \param hint GL storage hint enumeration for the GL buffer object
 */
template<typename T>
void
BufferData(astral_GLenum binding_point, c_array<const T> data, astral_GLenum hint)
{
  astral_glBufferData(binding_point, sizeof(T) * data.size(), data.c_ptr(), hint);
}

/*!
 * Provided as a conveniance, equivalent to
 * \code
 * glBufferData(binding_point, sizeof(T) * data.size(), data.c_ptr(), hint);
 * \endcode
 * \param binding_point GL binding point
 * \param data data with which to initialize the GL buffer object
 * \param hint GL storage hint enumeration for the GL buffer object
 */
template<typename T>
void
BufferData(astral_GLenum binding_point, c_array<T> data, astral_GLenum hint)
{
  astral_glBufferData(binding_point, sizeof(T) * data.size(), data.c_ptr(), hint);
}

/*!
 * Provided as a conveniance, equivalent to
 * \code
 * glBufferSubData(binding_point, sizeof(T) * offset, sizeof(T) * data.size(), data.c_ptr());
 * \endcode
 * \param binding_point GL binding point
 * \param offset offset in units of T (not machine units) from which
 *               to start the copy
 * \param data data frop which to copy to the GL buffer object
 */
template<typename T>
void
BufferSubData(astral_GLenum binding_point, uint32_t offset, c_array<const T> data)
{
  astral_glBufferSubData(binding_point, sizeof(T) * offset, sizeof(T) * data.size(), data.c_ptr());
}

/*!
 * Provided as a conveniance, equivalent to
 * \code
 * glBufferSubData(binding_point, sizeof(T) * offset, sizeof(T) * data.size(), data.c_ptr());
 * \endcode
 * \param binding_point GL binding point
 * \param offset offset in units of T (not machine units) from which
 *               to start the copy
 * \param data data frop which to copy to the GL buffer object
 */
template<typename T>
void
BufferSubData(astral_GLenum binding_point, uint32_t offset, c_array<T> data)
{
  astral_glBufferSubData(binding_point, sizeof(T) * offset, sizeof(T) * data.size(), data.c_ptr());
}
/*! @} */

} //namespace gl
} //namespace astral

#endif

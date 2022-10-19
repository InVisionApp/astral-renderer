/*!
 * \file gl_uniform_implement.cpp
 * \brief file gl_uniform_implement.cpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#include <astral/util/gl/astral_gl.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/matrix.hpp>
#include <astral/util/c_array.hpp>

namespace astral
{
namespace gl
{

  /* Note on the transposiness of matrices. Matrices are indexed in Astral
   * as matrix(a, b) whose element is at (a + b * N).
   * Astral defines matrix multiple out = M * in as
   *   out[a] = sum_{b} M(a, b) * in[b]
   *          = sum_{b} data[a + b * N] * in[b]
   *
   * GLSL defines matrix multiple matN M, out = M * in as
   *   out.x = M[0].x * in.x + M[1].x * in.y + M[2].x * in.z
   *   out.y = M[0].y * in.y + M[1].x * in.y + M[2].y * in.z
   *   out.z = M[0].z * in.y + M[1].z * in.y + M[2].z * in.z
   * i.e.
   *   out = M[0] * in.x + M[1] * in.y + M[2] * in.z;
   *
   * where M[col].row is the notation with the expectation
   * that M[col].row is stored at [row + col * N].
   *
   * Thus, how Astral stores matrices what GLSL expects,
   * but it names in the opposite order as GLSL. In particular
   * a GLSL matAxB is an astral::matrix<B, A>.
   */

  inline
  astral_GLenum
  transposed_value(bool t)
  {
    return (t) ? ASTRAL_GL_TRUE : ASTRAL_GL_FALSE;
  }

#ifndef __EMSCRIPTEN__

#define MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, COUNT)          \
  void Uniform##COUNT##v(int location, astral_GLsizei count, const TYPE *v)    \
  {                                                                     \
    astral_glUniform##COUNT##GLFN##v(location, count, v);           \
  }                                                                     \
  void ProgramUniform##COUNT##v(astral_GLuint program, int location, astral_GLsizei count, const TYPE *v) \
  {                                                                     \
    astral_glProgramUniform##COUNT##GLFN##v(program, location, count, v); \
  }

#define MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL(GLFN, TYPE)                     \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 1)                    \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 2)                    \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 3)                    \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 4)                    \
  void Uniform(int location, TYPE v)                                    \
  {                                                                     \
    astral_glUniform1##GLFN(location, v);                           \
  }                                                                     \
  void ProgramUniform(astral_GLuint program, int location, TYPE v)             \
  {                                                                     \
    astral_glProgramUniform1##GLFN(program, location, v);           \
  }

#define MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, A) \
  void Uniform(int location, astral_GLsizei count, const matrix<A, A, TYPE> *matrices, bool transposed) \
  {                                                                     \
    astral_glUniformMatrix##A##GLFN##v(location, count, transposed_value(transposed), reinterpret_cast<const TYPE*>(matrices)); \
  }                                                                     \
  void ProgramUniform(astral_GLuint program, int location, astral_GLsizei count, const matrix<A,A,TYPE> *matrices, bool transposed) \
  {                                                                     \
    astral_glProgramUniformMatrix##A##GLFN##v(program, location, count, transposed_value(transposed), reinterpret_cast<const TYPE*>(matrices)); \
  }

#define MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, A, B) \
  void Uniform(int location, astral_GLsizei count, const matrix<B, A, TYPE> *matrices, bool transposed) \
  {                                                                     \
    astral_glUniformMatrix##A##x##B##GLFN##v(location, count, transposed_value(transposed), reinterpret_cast<const TYPE*>(matrices)); \
  }                                                                     \
  void ProgramUniform(astral_GLuint program, int location, astral_GLsizei count, const matrix<B, A, TYPE> *matrices, bool transposed) \
  {                                                                     \
    astral_glProgramUniformMatrix##A##x##B##GLFN##v(program, location, count, transposed_value(transposed), reinterpret_cast<const TYPE*>(matrices)); \
  }

#else

#define MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, COUNT)          \
  void Uniform##COUNT##v(int location, GLsizei count, const TYPE *v)    \
  {                                                                     \
    astral_glUniform##COUNT##GLFN##v(location, count, v);           \
  }                                                                     \

#define MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL(GLFN, TYPE)                     \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 1)                    \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 2)                    \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 3)                    \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 4)                    \
  void Uniform(int location, TYPE v)                                    \
  {                                                                     \
    astral_glUniform1##GLFN(location, v);                           \
  }                                                                     \

#define MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, A) \
  void Uniform(int location, astral_GLsizei count, const matrix<A, A, TYPE> *matrices, bool transposed) \
  {                                                                     \
    astral_glUniformMatrix##A##GLFN##v(location, count, transposed_value(transposed), reinterpret_cast<const TYPE*>(matrices)); \
  }                                                                     \

#define MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, A, B) \
  void Uniform(int location, astral_GLsizei count, const matrix<B, A, TYPE> *matrices, bool transposed) \
  {                                                                     \
    astral_glUniformMatrix##A##x##B##GLFN##v(location, count, transposed_value(transposed), reinterpret_cast<const TYPE*>(matrices)); \
  }                                                                     \

#endif

#define MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_SQUARE_MATRIX_IMPL(GLFN, TYPE)  \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, 2) \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, 3) \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, 4)

#define MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL(GLFN, TYPE)   \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE, 2, 3)  \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE, 2, 4)  \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE, 3, 2)  \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE, 3, 4)  \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE, 4, 2)  \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE, 4, 3)


#define MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_MATRIX_IMPL(GLFN, TYPE)        \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_SQUARE_MATRIX_IMPL(GLFN, TYPE)       \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL(GLFN, TYPE)


MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL(f, astral_GLfloat)
MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL(i, astral_GLint)
MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL(ui, astral_GLuint)
MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_MATRIX_IMPL(f, astral_GLfloat)

#ifdef astral_glUniform1d
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL(d, astral_GLdouble)
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_MATRIX_IMPL(d, astral_GLdouble)
#endif

}
}

// -*- C++ -*-

/*!
 * \file gl_uniform_implement.hpp
 * \brief file gl_uniform_implement.hpp
 *
 * Adapted from: WRATHgl_uniform_implement.tcc of WRATH:
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


#ifndef ASTRAL_GLUNIFORM_IMPLEMENT_HPP
#define ASTRAL_GLUNIFORM_IMPLEMENT_HPP

#include <astral/util/gl/astral_gl.hpp>

namespace astral
{
namespace gl
{

///@cond

#ifndef __EMSCRIPTEN__
/*
 * Implement Uniform{1,2,3,4}v overloads to correct
 * glUniform{1,2,3,4}{d,f,i,ui}v calls and provide overloads
 * for vecN types too.
 *
 * GLFN one of {d,f,i,ui}
 * TYPE GL type, such as astral_GLfloat
 * COUNT one of {1,2,3,4}
 */
#define MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, COUNT)          \
  void Uniform##COUNT##v(int location, astral_GLsizei count, const TYPE *v);   \
  inline void Uniform(int location, const vecN<TYPE, COUNT> &v)         \
  {                                                                     \
    Uniform##COUNT##v(location, 1, &v[0]);                              \
  }                                                                     \
  inline void Uniform(int location, astral_GLsizei count, const vecN<TYPE, COUNT> *v) \
  {                                                                     \
    Uniform##COUNT##v(location, count, reinterpret_cast<const TYPE*>(v)); \
  }                                                                     \
  void ProgramUniform##COUNT##v(astral_GLuint program, int location, astral_GLsizei count, const TYPE *v); \
  inline void ProgramUniform(astral_GLuint program, int location, const vecN<TYPE, COUNT> &v) \
  {                                                                     \
    ProgramUniform##COUNT##v(program, location, 1, &v[0]);              \
  }                                                                     \
  inline void ProgramUniform(astral_GLuint program, int location, astral_GLsizei count, const vecN<TYPE, COUNT> *v) \
  {                                                                     \
    ProgramUniform##COUNT##v(program, location, count, reinterpret_cast<const TYPE*>(v)); \
  }

/*
 * Use MACRO_IMPEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT to implement
 * all for a given type. In addition array Uniform
 * without vecN.
 *
 * GLFN one of {d,f,i,ui}
 * TYPE GL type, such as astral_GLfloat
 */
#define MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL(GLFN, TYPE)                     \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 1)                    \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 2)                    \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 3)                    \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 4)                    \
  void Uniform(int location, TYPE v);                                   \
  inline void Uniform(int location, astral_GLsizei count, const TYPE *v)       \
  {                                                                     \
    Uniform1v(location, count, v);                                      \
  }                                                                     \
  void ProgramUniform(astral_GLuint program, int location, TYPE v);            \
  inline void ProgramUniform(astral_GLuint program, int location, astral_GLsizei count, const TYPE *v) \
  {                                                                     \
    ProgramUniform1v(program, location, count, v);                      \
  }

/*
 * Implement square matrices uniform setting
 * A: dimension of matrix, one of  {2,3,4}
 * GLFN: one of {f,d}
 * TYPE: one of {astral_GLfloat, GLdouble}
 */
#define MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, A) \
  void Uniform(int location, astral_GLsizei count, const matrix<A,A,TYPE> *matrices, bool transposed=false); \
  inline void Uniform(int location, const matrix<A,A,TYPE> &matrix, bool transposed=false) \
  {                                                                     \
    Uniform(location, 1, &matrix, transposed);                          \
  }                                                                     \
  void ProgramUniform(astral_GLuint program, int location, astral_GLsizei count, const matrix<A,A,TYPE> *matrices, bool transposed=false); \
  inline void ProgramUniform(astral_GLuint program, int location, const matrix<A,A,TYPE> &matrix, bool transposed=false) \
  {                                                                     \
    ProgramUniform(program, location, 1, &matrix, transposed);          \
  }

/*
 * Implement non-square matrices uniform setting
 * A: height of matrix, one of  {2,3,4}
 * B: width of matrix, one of  {2,3,4}
 * GLFN: one of {f,d}
 * TYPE: one of {astral_GLfloat, GLdouble}
 */
#define MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, A, B) \
  void Uniform(int location, astral_GLsizei count, const matrix<A,B,TYPE> *matrices, bool transposed=false); \
  inline void Uniform(int location, const matrix<A,B,TYPE> &matrix, bool transposed=false) \
  {                                                                     \
    Uniform(location, 1, &matrix, transposed);                          \
  }                                                                     \
  void ProgramUniform(astral_GLuint program, int location, astral_GLsizei count, const matrix<A,B,TYPE> *matrices, bool transposed=false); \
  inline void ProgramUniform(astral_GLuint program, int location, const matrix<A,B,TYPE> &matrix, bool transposed=false) \
  {                                                                     \
    ProgramUniform(program, location, 1, &matrix, transposed);          \
  }

#else

#define MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, COUNT)          \
  void Uniform##COUNT##v(int location, astral_GLsizei count, const TYPE *v);   \
  inline void Uniform(int location, const vecN<TYPE, COUNT> &v)         \
  {                                                                     \
    Uniform##COUNT##v(location, 1, &v[0]);                              \
  }                                                                     \
  inline void Uniform(int location, astral_GLsizei count, const vecN<TYPE, COUNT> *v) \
  {                                                                     \
    Uniform##COUNT##v(location, count, reinterpret_cast<const TYPE*>(v)); \
  }

#define MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL(GLFN, TYPE)                     \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 1)                    \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 2)                    \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 3)                    \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT(GLFN, TYPE, 4)                    \
  void Uniform(int location, TYPE v);                                   \
  inline void Uniform(int location, astral_GLsizei count, const TYPE *v)       \
  {                                                                     \
    Uniform1v(location, count, v);                                      \
  }

#define MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, A) \
  void Uniform(int location, astral_GLsizei count, const matrix<A,A,TYPE> *matrices, bool transposed=false); \
  inline void Uniform(int location, const matrix<A,A,TYPE> &matrix, bool transposed=false) \
  {                                                                     \
    Uniform(location, 1, &matrix, transposed);                          \
  }

#define MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, A, B) \
  void Uniform(int location, astral_GLsizei count, const matrix<A,B,TYPE> *matrices, bool transposed=false); \
  inline void Uniform(int location, const matrix<A,B,TYPE> &matrix, bool transposed=false) \
  {                                                                     \
    Uniform(location, 1, &matrix, transposed);                          \
  }
#endif

/*
 * Implement square matrices uniform setting
 * GLFN: one of {f,d}
 * TYPE: one of {astral_GLfloat, GLdouble}
 */
#define MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_SQUARE_MATRIX_IMPL(GLFN, TYPE)  \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, 2) \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, 3) \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_SQUARE_MATRIX_IMPL_DIM(GLFN, TYPE, 4)


/*
 * Implement non-square matrices uniform setting
 * GLFN: one of {f,d}
 * TYPE: one of {astral_GLfloat, GLdouble}
 */
#define MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL(GLFN, TYPE)   \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE,2,3)  \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE,2,4)  \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE,3,2)  \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE,3,4)  \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE,4,2)  \
  MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM(GLFN,TYPE,4,3)


/*
 * Implement all matrix uniform setting
 * GLFN: one of {f,d}
 * TYPE: one of {astral_GLfloat, GLdouble}
 */
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

#undef MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL
#undef MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_IMPL_CNT
#undef MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_MATRIX_IMPL
#undef MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL
#undef MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_NON_SQUARE_MATRIX_IMPL_DIM
#undef MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_SQUARE_MATRIX_IMPL
#undef MACRO_IMPLEMENT_ASTRAL_GL_UNIFORM_SQUARE_MATRIX_IMPL_DIM

///@endcond

}
}

#endif

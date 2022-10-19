/*!
 * \file gl_get.cpp
 * \brief file gl_get.cpp
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
#include <astral/util/gl/gl_get.hpp>

namespace astral {
namespace gl {

void
context_get(astral_GLenum v, astral_GLint *ptr)
{
  astral_glGetIntegerv(v, ptr);
}

  void
context_get(astral_GLenum v, astral_GLuint *ptr)
{
  astral_glGetIntegerv(v, (astral_GLint*)ptr);
}

void
context_get(astral_GLenum v, astral_GLboolean *ptr)
{
  astral_glGetBooleanv(v, ptr);
}

void
context_get(astral_GLenum v, bool *ptr)
{
  astral_GLboolean bptr(*ptr ? ASTRAL_GL_TRUE : ASTRAL_GL_FALSE);
  astral_glGetBooleanv(v, &bptr);
  *ptr = (bptr == ASTRAL_GL_FALSE)?
    false:
    true;
}

void
context_get(astral_GLenum v, astral_GLfloat *ptr)
{
  astral_glGetFloatv(v, ptr);
}


} //namespace gl
} //namespace astral

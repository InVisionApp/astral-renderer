/*!
 * \file gl_context_properties.cpp
 * \brief file gl_context_properties.cpp
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

#include <iostream>
#include <string>
#include <astral/util/gl/astral_gl.hpp>
#include <astral/util/gl/gl_context_properties.hpp>
#include <astral/util/gl/gl_get.hpp>

namespace
{
  class ContextPropertiesImplement
  {
  public:
    static
    const ContextPropertiesImplement&
    value(void)
    {
      static ContextPropertiesImplement V;
      return V;
    }

    bool
    has_extension(const std::string &ext) const
    {
      return m_extensions.find(std::string(ext)) != m_extensions.end();
    }

    bool m_is_es;
    astral::vecN<int, 2> m_version;
    std::set<std::string> m_extensions;

  private:
    ContextPropertiesImplement(void);

    void
    make_version_ready(void);

    void
    make_extensions_ready(void);
  };
}

//////////////////////////////////////
// ContextPropertiesImplemment methods
ContextPropertiesImplement::
ContextPropertiesImplement(void)
{
  #ifdef __EMSCRIPTEN__
    {
      m_is_es = true;
    }
  #else
    {
      std::string version;
      version = reinterpret_cast<const char*>(astral_glGetString(ASTRAL_GL_VERSION));

      /* a GLES compliant implementation will start its version string with "OpenGL ES" */
      std::string opengl_es = "OpenGL ES";

      if (version.size() >= opengl_es.size())
        {
          m_is_es = (version.substr(0, opengl_es.size()) == opengl_es);
        }
      else
        {
          m_is_es = false;
        }
    }
  #endif

  make_version_ready();
  make_extensions_ready();
}

void
ContextPropertiesImplement::
make_version_ready(void)
{
  m_version[0] = astral::gl::context_get<astral_GLint>(ASTRAL_GL_MAJOR_VERSION);
  m_version[1] = astral::gl::context_get<astral_GLint>(ASTRAL_GL_MINOR_VERSION);
}

void
ContextPropertiesImplement::
make_extensions_ready(void)
{
  int cnt;
  cnt = astral::gl::context_get<astral_GLint>(ASTRAL_GL_NUM_EXTENSIONS);
  for(int i = 0; i < cnt; ++i)
    {
      astral::c_string ext;
      ext = reinterpret_cast<astral::c_string>(astral_glGetStringi(ASTRAL_GL_EXTENSIONS, i));
      m_extensions.insert(ext);
    }
}

////////////////////////////////////
// ContextProperties methods
astral::vecN<int, 2>
astral::gl::ContextProperties::
version(void)
{
  return ContextPropertiesImplement::value().m_version;
}

bool
astral::gl::ContextProperties::
is_es(void)
{
  return ContextPropertiesImplement::value().m_is_es;
}

bool
astral::gl::ContextProperties::
has_extension(astral::c_string ext)
{
  return ext && ContextPropertiesImplement::value().has_extension(ext);
}

const std::set<std::string>&
astral::gl::ContextProperties::
extension_set(void)
{
  return ContextPropertiesImplement::value().m_extensions;
}

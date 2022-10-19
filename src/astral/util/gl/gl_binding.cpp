/*!
 * \file gl_binding.cpp
 * \brief file gl_binding.cpp
 *
 * Adapted from: ngl_backend.cpp of WRATH:
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

#include <iostream>
#include <sstream>
#include <list>
#include <mutex>
#include <astral/util/util.hpp>
#include <astral/util/api_callback.hpp>
#include <astral/util/gl/astral_gl.hpp>
#include <astral/util/gl/gl_binding.hpp>

namespace
{
  std::string
  gl_error_check(void)
  {
    auto p = ASTRALglfunctionPointer(glGetError);
    astral_GLenum error_code;
    std::ostringstream str;

    for(error_code = p(); error_code != ASTRAL_GL_NO_ERROR; error_code = p())
      {
        switch(error_code)
          {
          case ASTRAL_GL_INVALID_ENUM:
            str << "ASTRAL_GL_INVALID_ENUM ";
            break;
          case ASTRAL_GL_INVALID_VALUE:
            str << "ASTRAL_GL_INVALID_VALUE ";
            break;
          case ASTRAL_GL_INVALID_OPERATION:
            str << "ASTRAL_GL_INVALID_OPERATION ";
            break;
          case ASTRAL_GL_OUT_OF_MEMORY:
            str << "ASTRAL_GL_OUT_OF_MEMORY ";
            break;

          default:
            str << "0x" << std::hex << error_code;
          }
      }
    return str.str();
  }

  class NGL:public astral::APICallbackSet
  {
  public:
    explicit
    NGL(astral::c_string label):
      astral::APICallbackSet(label),
      m_glStringMarkerGREMEDY(nullptr),
      m_emit_gl_string_marker_at_every_call(false)
    {
    }

    void
    emit_text(astral::c_string text)
    {
      if (!m_glStringMarkerGREMEDY)
        {
          /* TODO: the correct thing to do really
           *       is to check for the extenion and
           *       from that decide if the extension
           *       is present.
           */
          m_glStringMarkerGREMEDY = (glStringMarkerGREMEDY_fptr)get_proc("glStringMarkerGREMEDY");
          if (m_glStringMarkerGREMEDY == nullptr)
            {
              m_glStringMarkerGREMEDY = &do_nothing;
            }
        }
      m_glStringMarkerGREMEDY(0, text);
    }

    void
    enable_gl_string_marker(bool v)
    {
      m_emit_gl_string_marker_at_every_call = v;
    }

    bool
    enable_gl_string_marker(void)
    {
      return m_emit_gl_string_marker_at_every_call;
    }

  private:
    typedef void (*glStringMarkerGREMEDY_fptr)(astral_GLsizei len, const astral_GLvoid *string);

    static
    void
    do_nothing(astral_GLsizei, const astral_GLvoid *)
    {}

    glStringMarkerGREMEDY_fptr m_glStringMarkerGREMEDY;
    bool m_emit_gl_string_marker_at_every_call;
  };

  NGL&
  ngl(void)
  {
    static NGL R("ngl");
    return R;
  }
}

/////////////////////////////////
// astral::gl_binding::CallbackGL methods
astral::gl_binding::CallbackGL::
CallbackGL(void):
  APICallbackSet::CallBack(&ngl())
{}

///////////////////////////////
// gl_binding methods
namespace astral
{
  namespace gl_binding
  {
    void on_load_function_error(const char *fname);
    void call_unloadable_function(const char *fname);
    void post_call(const char *call, const char *src, const char *function_name, void* fptr, const char *fileName, int line);
    void pre_call(const char *call, const char *src, const char *function_name, void* fptr, const char *fileName, int line);
    void load_all_functions(bool);
  }
}

void
astral::gl_binding::
on_load_function_error(c_string fname)
{
  std::cerr << ngl().label() << ": Unable to load function: \"" << fname << "\"\n";
}

void
astral::gl_binding::
call_unloadable_function(c_string fname)
{
  /* Should we just assume the loggers will
   * emit?
   */
  std::cerr << ngl().label() << ": Call to unloadable function: \"" << fname << "\"\n";
  ngl().call_unloadable_function(fname);
}

void
astral::gl_binding::
pre_call(c_string call_string_values,
         c_string call_string_src,
         c_string function_name,
         void *function_ptr,
         c_string src_file, int src_line)
{
  ngl().pre_call(call_string_values, call_string_src,
                 function_name, function_ptr,
                 src_file, src_line);

  if (ngl().enable_gl_string_marker())
    {
      emit_string(function_name, src_file, src_line);
    }
}

void
astral::gl_binding::
post_call(c_string call_string_values,
          c_string call_string_src,
          c_string function_name,
          void *function_ptr,
          c_string src_file, int src_line)
{
  std::string error;
  error = gl_error_check();

  /* Should we just assume the loggers will
   * emit?
   */
  if (!error.empty())
    {
      std::cerr << "[" << src_file << "," << std::dec
                << src_line << "] "
                << call_string_values << "{"
                << error << "}\n";
    }

  ngl().post_call(call_string_values, call_string_src,
                  function_name, error.c_str(),
                  function_ptr, src_file, src_line);
}

void
astral::gl_binding::
get_proc_function(void* (*get_proc)(c_string), bool load_functions)
{
  #ifdef __EMSCRIPTEN__
    {
      /* Do nothing, functions are not fetched at runtime
       * by funciton pointers
       */
      ASTRALunused(get_proc);
      ASTRALunused(load_functions);
    }
  #else
    {
      ngl().get_proc_function(get_proc);
      if (load_functions && get_proc != nullptr)
        {
          load_all_functions(false);
        }
    }
  #endif
}

void
astral::gl_binding::
get_proc_function(void *data,
                  void* (*get_proc)(void *, c_string),
                  bool load_functions)
{
  #ifdef __EMSCRIPTEN__
    {
      /* Do nothing, functions are not fetched at runtime
       * by funciton pointers
       */
      ASTRALunused(data);
      ASTRALunused(get_proc);
      ASTRALunused(load_functions);
    }
  #else
    {
      ngl().get_proc_function(data, get_proc);
      if (load_functions && get_proc != nullptr)
        {
          load_all_functions(false);
        }
    }
  #endif
}

void*
astral::gl_binding::
get_proc(c_string function_name)
{
  #ifdef __EMSCRIPTEN__
    {
      /* this should never be called */
      ASTRALassert(!"Calling gl_binding::get_proc() is an error in EMSCRIPTEN builds");
      return nullptr;
    }
  #else
    {
      return ngl().get_proc(function_name);
    }
  #endif
}

void
astral::gl_binding::
message(c_string message, c_string src_file, int src_line)
{
  ngl().message(message, src_file, src_line);
}

void
astral::gl_binding::
enable_gl_string_marker(bool b)
{
  ngl().enable_gl_string_marker(b);
}

void
astral::gl_binding::
emit_string(c_string label, c_string file, int line)
{
  std::ostringstream str;

  str << "[" << file << ", " << line << "] " << label;
  ngl().emit_text(str.str().c_str());
}

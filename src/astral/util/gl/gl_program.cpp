/*!
 * \file gl_program.cpp
 * \brief file gl_program.cpp
 *
 * Adapted from: WRATHGLProgram.cpp of WRATH:
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
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <map>
#include <vector>
#include <cstdlib>

#if __cplusplus > 201402L
#include <filesystem>
#else
#include <unistd.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <astral/util/static_resource.hpp>
#include <astral/util/gl/astral_gl.hpp>
#include <astral/util/gl/gl_get.hpp>
#include <astral/util/gl/gl_context_properties.hpp>
#include <astral/util/gl/wasm_missing_gl_enums.hpp>
#include <astral/util/gl/gl_program.hpp>



/* Both gl::Shader and gl::Program have three stages to their assembly.
 *
 * Stage 1: issue glCompileShader/glLinkProgram, but do not query ANYTHNG.
 *          By not querying ANYTHING, this allows for a GL implementation
 *          to spawn the work to another thread for the sake of
 *          GL_KHR_parallel_shader_compile.
 *
 * Stage 2: issue glGetShadiv(ASTRAL_GL_COMPILE_STATUS)/glGetProgramiv(ASTRAL_GL_LINK_STATUS)
 *          but do not query anthing else, especially the compile or link
 *          logs. Tests on Chrome on MacOS have shown that by not asking for
 *          the logs hitches from compiling shaders when RenderEngineGL3 uses
 *          Shader::shader_compiled()/Program::program_linked(), the hitching
 *          times on rendering when a new uber-shader is used are massively
 *          reduced.
 *
 * Stage 3: issue the glGetShaderInfoLog()/glGetProgramInfoLog() on demand
 *          if a caller wants the log.
 *
 */

namespace
{
  std::string
  fullpath_from_filename(const std::string &name)
  {
    #if __cplusplus > 201402L
      {
        std::filesystem::path path(name);
        return std::filesystem::absolute(path).string();
      }
    #else
      {
        char cwd_backing[2048];
        char *cwd;
        std::string return_value;

        cwd = getcwd(cwd_backing, 2048);
        if (cwd)
          {
            return_value = std::string(cwd) + "/" + name;
          }
        else
          {
            return_value = name;
          }

        return return_value;
      }
    #endif
  }


  class ExtensionsSupportedValues
  {
  public:
    static
    const ExtensionsSupportedValues&
    value(void)
    {
      static ExtensionsSupportedValues R;
      return R;
    }

    bool
    translated_shader_source_supported(void) const
    {
      return m_translated_shader_source_supported;
    }

    bool
    khr_parallel_shader_compile_supported(void) const
    {
      return m_khr_parallel_shader_compile_supported;
    }

  private:
    ExtensionsSupportedValues(void)
    {
      m_translated_shader_source_supported = astral::gl::ContextProperties::has_extension("GL_ANGLE_translated_shader_source");
      m_khr_parallel_shader_compile_supported = astral::gl::ContextProperties::has_extension("GL_KHR_parallel_shader_compile");
    }

    bool m_translated_shader_source_supported;
    bool m_khr_parallel_shader_compile_supported;
  };
}

#ifdef __EMSCRIPTEN__
EM_JS(char*, get_translated_shader_source, (int32_t nm), {
    let ext = GLctx.getExtension('WEBGL_debug_shaders');
    if (ext) {
      let jsString = ext.getTranslatedShaderSource(GL.shaders[nm]);
      let lengthBytes = lengthBytesUTF8(jsString) + 1;
      let stringOnWasmHeap = _malloc(lengthBytes);
      stringToUTF8(jsString, stringOnWasmHeap, lengthBytes);
      return stringOnWasmHeap;
    } else {
      return 0;
    }
  });

EM_JS(void, popup_dump_file, (const char *label, const char *dump), {
    let jsDump = UTF8ToString(dump);
    let data = new Blob([jsDump], {type: 'text/plain'});
    let file = window.URL.createObjectURL(data);
    let jsLabel = UTF8ToString(label);

    console.log("Dump to file " + jsLabel);

    let link = document.createElement("a");
    link.href = file;
    link.download = jsLabel;
    link.click();
  });

#endif

////////////////////////////////////////////////
// astral::gl::Shader and Shader::Implement methods
static unsigned int gl_shader_unique_id = 0;
static bool gl_shader_emit_on_compile_error = true;

class astral::gl::Shader::Implement:public astral::gl::Shader
{
public:
  Implement(const gl::ShaderSource &src, astral_GLenum pshader_type);
  ~Implement();

  void
  post_compile(void);

  void
  generate_log(void);

  void
  stream_bad_shader_contents(std::ostream &str);

  bool m_post_compile_called, m_log_generated;
  astral_GLuint m_name;
  astral_GLenum m_shader_type;

  std::string m_source_code;
  std::string m_compile_log;
  std::string m_translated_code;
  bool m_compile_success;

  unsigned int m_unique_id;
};

astral::reference_counted_ptr<astral::gl::Shader>
astral::gl::Shader::
create(const gl::ShaderSource &src, astral_GLenum pshader_type)
{
  return ASTRALnew Implement(src, pshader_type);
}

astral::gl::Shader::Implement::
Implement(const gl::ShaderSource &src, astral_GLenum pshader_type):
  m_post_compile_called(false),
  m_log_generated(false),
  m_name(0),
  m_shader_type(pshader_type),
  m_compile_success(false),
  m_unique_id(++gl_shader_unique_id)
{
  m_source_code = std::string(src.assembled_code());

  //now do the GL work, create a name and compile the source code:
  ASTRALassert(m_name == 0);

  m_name = astral_glCreateShader(m_shader_type);

  astral::c_string sourceString[1];
  sourceString[0] = m_source_code.c_str();

  astral_glShaderSource(m_name, //shader handle
                        1, //number strings
                        sourceString, //array of strings
                        nullptr); //lengths of each string or nullptr implies each is 0-terminated

  astral_glCompileShader(m_name);

  if (!ExtensionsSupportedValues::value().khr_parallel_shader_compile_supported())
    {
      post_compile();
      generate_log();
    }
}

astral::gl::Shader::Implement::
~Implement()
{
  if (m_name)
    {
      astral_glDeleteShader(m_name);
    }
}

void
astral::gl::Shader::Implement::
post_compile(void)
{
  if (m_post_compile_called)
    {
      return;
    }

  astral_GLint shaderOK;

  astral_glGetShaderiv(m_name, ASTRAL_GL_COMPILE_STATUS, &shaderOK);
  m_compile_success = (shaderOK == ASTRAL_GL_TRUE);
  m_post_compile_called = true;

  /* If the shader did not compile successfully, we
   * want the shader log to get generated so that it
   * will trigger writing to files of the bad shader.
   */
  if (!m_compile_success)
    {
      generate_log();
    }
}

void
astral::gl::Shader::Implement::
stream_bad_shader_contents(std::ostream &eek)
{
  eek << m_source_code
      << "\n------------------------- end original source -----------------------\n"
      << "\n\ncompile log:\n" << m_compile_log
      << "\n";
}

void
astral::gl::Shader::Implement::
generate_log(void)
{
  ASTRALassert(m_post_compile_called);
  if (m_log_generated)
    {
      return;
    }

  m_log_generated = true;

  std::vector<char> raw_log;
  astral_GLint log_size(0);

  astral_glGetShaderiv(m_name, ASTRAL_GL_INFO_LOG_LENGTH, &log_size);

  //retrieve the compile log string, eh gross.
  raw_log.resize(log_size + 2, '\0');
  astral_glGetShaderInfoLog(m_name, //shader handle
                            log_size + 1, //maximum size of string
                            nullptr, //astral_GLint* return length of string
                            &raw_log[0]); //char* to write log to.

  m_compile_log = &raw_log[0];

  /* generate the translader code */
  #ifdef __EMSCRIPTEN__
    {
      char *str;

      str = get_translated_shader_source(m_name);
      if (str)
        {
          m_translated_code = str;
          std::free(str);
        }
    }
  #else
    {
      if (ExtensionsSupportedValues::value().translated_shader_source_supported())
        {
          astral_glGetShaderiv(m_name, ASTRAL_GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE, &log_size);
          raw_log.resize(log_size + 2, '\0');
          astral_glGetTranslatedShaderSourceANGLE(m_name, log_size + 2, nullptr, &raw_log[0]);
          m_translated_code = &raw_log[0];
        }
    }
  #endif

  if (!m_compile_success && gl_shader_emit_on_compile_error)
    {
      std::ostringstream oo;

      oo << "bad_shader_" << m_unique_id << "(name = " << m_name << ").glsl";

      #ifndef __EMSCRIPTEN__
        {
          std::string filename;
          filename = fullpath_from_filename(oo.str());

          std::ofstream eek(filename.c_str());
          stream_bad_shader_contents(eek);
          std::cout << "Wrote to file \"" << filename << "\"\n";
        }
      #else
        {
          std::string filename;
          filename = oo.str();

          std::ostringstream eek;
          stream_bad_shader_contents(eek);

          std::string eek_contents;
          eek_contents = eek.str();
          popup_dump_file(filename.c_str(), eek_contents.c_str());
          std::cout << "Wrote to file \"" << filename << "\"\n";
        }
      #endif
    }
  else if (0)
    {
      std::ostringstream oo;
      std::string filename;

      oo << "good_shader_" << m_name << ".glsl";
      filename = fullpath_from_filename(oo.str());

      std::ofstream eek(filename.c_str());
      eek << m_source_code
          << "\n\n"
          << m_compile_log;

      std::cout << "Wrote to file \"" << filename << "\"\n";
    }
}

bool
astral::gl::Shader::
compile_success(void)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  p->post_compile();
  return p->m_compile_success;
}

astral::c_string
astral::gl::Shader::
compile_log(void)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  p->post_compile();
  p->generate_log();
  return p->m_compile_log.c_str();
}

astral::c_string
astral::gl::Shader::
translated_code(void)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  p->post_compile();
  p->generate_log();
  return p->m_translated_code.c_str();
}

astral_GLuint
astral::gl::Shader::
name(void)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  return p->m_name;
}

bool
astral::gl::Shader::
shader_compiled(void)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  if (p->m_post_compile_called)
    {
      return true;
    }

  astral_GLint v(ASTRAL_GL_TRUE);
  astral_glGetShaderiv(p->m_name, ASTRAL_GL_COMPLETION_STATUS_KHR, &v);

  return v == ASTRAL_GL_TRUE;
}

astral::c_string
astral::gl::Shader::
source_code(void)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  return p->m_source_code.c_str();
}

astral_GLenum
astral::gl::Shader::
shader_type(void)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  return p->m_shader_type;
}

astral::c_string
astral::gl::Shader::
gl_shader_type_label(astral_GLenum shader_type)
{
  #define CASE(X) case X: return #X

  switch(shader_type)
    {
    default:
      return "UNKNOWN_SHADER_STAGE";

      CASE(ASTRAL_GL_FRAGMENT_SHADER);
      CASE(ASTRAL_GL_VERTEX_SHADER);
#ifndef __EMSCRIPTEN__
      CASE(ASTRAL_GL_GEOMETRY_SHADER);
      CASE(ASTRAL_GL_TESS_EVALUATION_SHADER);
      CASE(ASTRAL_GL_TESS_CONTROL_SHADER);
      CASE(ASTRAL_GL_COMPUTE_SHADER);
#endif
    }

  #undef CASE
}

astral::c_string
astral::gl::Shader::
default_shader_version(void)
{
  if (ContextProperties::is_es())
    {
      return "300 es";
    }
  else
    {
      return "330 core";
    }
}

bool
astral::gl::Shader::
emit_file_on_compile_error(void)
{
  return gl_shader_emit_on_compile_error;
}

void
astral::gl::Shader::
emit_file_on_compile_error(bool v)
{
  gl_shader_emit_on_compile_error = v;
}

////////////////////////////////////////
// astral::gl::BindAttribute methods
void
astral::gl::BindAttribute::
action(astral_GLuint glsl_program) const
{
  astral_glBindAttribLocation(glsl_program, m_location, m_label.c_str());
}


////////////////////////////////////
// ProgramSeparable methods
void
astral::gl::ProgramSeparable::
action(astral_GLuint glsl_program) const
{
  #ifdef __EMSCRIPTEN__
    {
      ASTRALassert(!"ASTRAL_GL_PROGRAM_SEPARABLE not supported in EMCRIPTEN");
    }
  #else
    {
      astral_glProgramParameteri(glsl_program, ASTRAL_GL_PROGRAM_SEPARABLE, ASTRAL_GL_TRUE);
    }
  #endif
}

///////////////////////////////////////////////////
// astral::gl::TransformFeedbackVarying methods
void
astral::gl::TransformFeedbackVarying::
action(astral_GLuint glsl_program) const
{
  if (m_transform_feedback_varyings.size() > 0)
    {
      std::vector<astral::c_string> tmp(m_transform_feedback_varyings.size());
      for (unsigned int i = 0; i < tmp.size(); ++i)
        {
          tmp[i] = m_transform_feedback_varyings[i].c_str();
        }

      astral_glTransformFeedbackVaryings(glsl_program, tmp.size(), &tmp[0], m_buffer_mode);
    }
}

////////////////////////////////////////////
// astral::gl::PreLinkActionArray methods
void
astral::gl::PreLinkActionArray::
execute_actions(astral_GLuint pr) const
{
  for(const auto &p : m_values)
    {
      if (p)
        {
          p->action(pr);
        }
    }
}

/////////////////////////////////////////////////////////
//astral::gl::Program and astral::gl::Program::Implement methods
static unsigned int gl_program_total_programs_linked = 0u;
static unsigned int gl_program_global_query_counter = 1;
static unsigned int gl_program_unique_id = 0;
static bool gl_program_emit_file_on_link_error = true;

class astral::gl::Program::Implement:public astral::gl::Program
{
public:
  class ShaderData;

  Implement(c_array<const reference_counted_ptr<Shader>> pshaders,
            const PreLinkActionArray &action,
            const ProgramInitializerArray &initers);

  Implement(reference_counted_ptr<Shader> vert_shader,
            reference_counted_ptr<Shader> frag_shader,
            const PreLinkActionArray &action,
            const ProgramInitializerArray &initers);

  Implement(const gl::ShaderSource &vert_shader,
            const gl::ShaderSource &frag_shader,
            const PreLinkActionArray &action,
            const ProgramInitializerArray &initers);

  Implement(astral_GLuint pname, bool take_ownership);

  ~Implement();

  void
  create_program_and_link(void);

  void
  post_link(void);

  void
  clear_shaders_and_save_shader_data(void);

  void
  generate_link_log(void);

  void
  stream_bad_program_contents(std::ostream &str);

  std::vector<astral::reference_counted_ptr<astral::gl::Shader>> m_shaders;
  std::vector<ShaderData> m_shader_data;
  std::map<astral_GLenum, std::vector<int>> m_shader_data_sorted_by_type;

  astral_GLuint m_name;
  bool m_delete_program;
  bool m_link_success, m_post_link_actions_called;
  bool m_logs_generated, m_link_counted;
  std::string m_link_log;
  std::string m_log;
  unsigned int m_query_counter;

  astral::gl::ProgramInitializerArray m_initializers;
  astral::gl::PreLinkActionArray m_pre_link_actions;

  unsigned int m_unique_id;
};

class astral::gl::Program::Implement::ShaderData
{
public:
  std::string m_source_code;
  std::string m_translated_code;
  astral_GLuint m_name;
  astral_GLenum m_shader_type;
  std::string m_compile_log;
  bool m_compile_success;
};

astral::gl::Program::Implement::
Implement(astral_GLuint pname, bool take_ownership):
  m_name(pname),
  m_delete_program(take_ownership),
  m_post_link_actions_called(false),
  m_logs_generated(false),
  m_query_counter(0),
  m_unique_id(++gl_program_unique_id)
{
}

astral::gl::Program::Implement::
Implement(reference_counted_ptr<Shader> vert_shader,
          reference_counted_ptr<Shader> frag_shader,
          const PreLinkActionArray &action,
          const ProgramInitializerArray &initers):
  m_name(0),
  m_delete_program(true),
  m_post_link_actions_called(false),
  m_logs_generated(false),
  m_link_counted(false),
  m_query_counter(0),
  m_initializers(initers),
  m_pre_link_actions(action),
  m_unique_id(++gl_program_unique_id)
{
  ASTRALassert(vert_shader && vert_shader->shader_type() == ASTRAL_GL_VERTEX_SHADER);
  ASTRALassert(frag_shader && frag_shader->shader_type() == ASTRAL_GL_FRAGMENT_SHADER);
  m_shaders.push_back(vert_shader);
  m_shaders.push_back(frag_shader);

  create_program_and_link();
}

astral::gl::Program::Implement::
Implement(const ShaderSource &vert_shader,
          const ShaderSource &frag_shader,
          const PreLinkActionArray &action,
          const ProgramInitializerArray &initers):
  m_name(0),
  m_delete_program(true),
  m_post_link_actions_called(false),
  m_logs_generated(false),
  m_link_counted(false),
  m_query_counter(0),
  m_initializers(initers),
  m_pre_link_actions(action),
  m_unique_id(++gl_program_unique_id)
{
  m_shaders.push_back(Shader::create(vert_shader, ASTRAL_GL_VERTEX_SHADER));
  m_shaders.push_back(Shader::create(frag_shader, ASTRAL_GL_FRAGMENT_SHADER));

  create_program_and_link();
}

astral::gl::Program::Implement::
Implement(c_array<const reference_counted_ptr<Shader>> pshaders,
          const PreLinkActionArray &action,
          const ProgramInitializerArray &initers):
  m_name(0),
  m_delete_program(true),
  m_post_link_actions_called(false),
  m_logs_generated(false),
  m_link_counted(false),
  m_query_counter(0),
  m_initializers(initers),
  m_pre_link_actions(action),
  m_unique_id(++gl_program_unique_id)
{
  m_shaders.resize(pshaders.size());
  std::copy(pshaders.begin(), pshaders.end(), m_shaders.begin());

  create_program_and_link();
}

astral::gl::Program::Implement::
~Implement()
{
  if (m_name && m_delete_program)
    {
      astral_glDeleteProgram(m_name);
    }
}

astral::reference_counted_ptr<astral::gl::Program>
astral::gl::Program::
create(c_array<const reference_counted_ptr<Shader>> pshaders,
       const PreLinkActionArray &action,
       const ProgramInitializerArray &initers)
{
  return ASTRALnew Implement(pshaders, action, initers);
}

astral::reference_counted_ptr<astral::gl::Program>
astral::gl::Program::
create(reference_counted_ptr<Shader> vert_shader,
       reference_counted_ptr<Shader> frag_shader,
       const PreLinkActionArray &action,
       const ProgramInitializerArray &initers)
{
  return ASTRALnew Implement(vert_shader, frag_shader, action, initers);
}

astral::reference_counted_ptr<astral::gl::Program>
astral::gl::Program::
create(const gl::ShaderSource &vert_shader,
       const gl::ShaderSource &frag_shader,
       const PreLinkActionArray &action,
       const ProgramInitializerArray &initers)
{
  return ASTRALnew Implement(vert_shader, frag_shader, action, initers);
}

astral::reference_counted_ptr<astral::gl::Program>
astral::gl::Program::
create(astral_GLuint pname, bool take_ownership)
{
  return ASTRALnew Implement(pname, take_ownership);
}

void
astral::gl::Program::
increment_global_query_counter(void)
{
  ++gl_program_global_query_counter;
}

bool
astral::gl::Program::
program_linked(void)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  if (p->m_post_link_actions_called)
    {
      return true;
    }

  if (p->m_query_counter >= gl_program_global_query_counter)
    {
      return false;
    }

  /* If the GL driver does not support ASTRAL_GL_COMPLETION_STATUS_KHR,
   * then glGetProgramiv() will not change the value of v. In
   * that case, by initializing it as true, the caller will
   * then not use a fall back program. If we left it as ASTRAL_GL_FALSE,
   * the caller would be forever stuck using the fallback
   */
  astral_GLint v(ASTRAL_GL_TRUE);
  astral_glGetProgramiv(p->m_name, ASTRAL_GL_COMPLETION_STATUS_KHR, &v);
  p->m_query_counter = gl_program_global_query_counter;

  if (v == ASTRAL_GL_TRUE)
    {
      p->post_link();
    }

  return v == ASTRAL_GL_TRUE;
}

void
astral::gl::Program::Implement::
create_program_and_link(void)
{
  ASTRALassert(m_name == 0);
  m_name = astral_glCreateProgram();

  /* attatch the shaders, we do NOT check the compile status
   * because we want to avoid forcing the GL driver to do anything
   * now, and allow for the driver to delay the compile and linking
   * work as long as possible.
   */
  for(const auto &sh : m_shaders)
    {
      astral_glAttachShader(m_name, sh->name());
    }

  //perform any pre-link actions and then clear them
  m_pre_link_actions.execute_actions(m_name);
  m_pre_link_actions = astral::gl::PreLinkActionArray();

  //now finally link
  astral_glLinkProgram(m_name);

  if (!ExtensionsSupportedValues::value().khr_parallel_shader_compile_supported())
    {
      post_link();
      generate_link_log();
    }
}

void
astral::gl::Program::Implement::
post_link(void)
{
  if (m_post_link_actions_called)
    {
      return;
    }

  astral_GLint linkOK;

  astral_glGetProgramiv(m_name, ASTRAL_GL_LINK_STATUS, &linkOK);
  m_link_success = (linkOK == ASTRAL_GL_TRUE);
  m_post_link_actions_called = true;

  /* If the program did not link successfully, we
   * want the link log to get generated so that it
   * will trigger writing to files of the bad shader.
   */
  if (!m_link_success)
    {
      generate_link_log();
    }

  if (!m_link_counted)
    {
      m_link_counted = true;
      ++gl_program_total_programs_linked;
    }
}

void
astral::gl::Program::Implement::
stream_bad_program_contents(std::ostream &eek)
{
  for(const auto &d : m_shader_data)
    {
      eek << "\n\nshader: " << d.m_name
          << "[" << astral::gl::Shader::gl_shader_type_label(d.m_shader_type) << "]"
          << "\n------------------------- begin translated source -----------------------\n\n"
          << d.m_translated_code
          << "\n\n------------------------- end translated source -----------------------\n"
          << "\n------------------------- begin original source -----------------------\n"
          << "\nshader_source:\n" << d.m_source_code
          << "\n------------------------- end original source -----------------------\n"
          << "\ncompile log:\n" << d.m_compile_log;
    }
  eek << "\n\nLink Log: " << m_link_log;
}

void
astral::gl::Program::Implement::
generate_link_log(void)
{
  ASTRALassert(m_post_link_actions_called);
  if (m_logs_generated)
    {
      return;
    }

  /* Fetch the compile logs before getting the link logs;
   * browsers may fail out on a hard shader in such a way that
   * fetching the translated source fails.
   */
  clear_shaders_and_save_shader_data();

  std::vector<char> raw_log;
  astral_GLint log_size;
  std::ostringstream ostr;

  m_logs_generated = true;

  // fetch the link-log
  astral_glGetProgramiv(m_name, ASTRAL_GL_INFO_LOG_LENGTH, &log_size);
  raw_log.resize(log_size + 2, '\0');
  astral_glGetProgramInfoLog(m_name, log_size + 1, nullptr , &raw_log[0]);
  m_link_log = std::string(&raw_log[0]);

  // generate m_log
  ostr << "gl::Program [GLname: " << m_name << "]\n";
  if (!m_shader_data.empty())
    {
      ostr << "Shader Source Codes:";

      for(const auto &d : m_shader_data)
        {
          ostr << "\n\nGLSL name = " << d.m_name
               << ", type = " << astral::gl::Shader::gl_shader_type_label(d.m_shader_type)
               << "\nSource:\n" << d.m_source_code
               << "\nTranslatedSource:\n" << d.m_translated_code
               << "\n\n";
        }
    }

  if (!m_shader_data.empty())
    {
      ostr << "\nShader Logs:";
      for(const auto &d : m_shader_data)
        {
          ostr << "\n\nGLSL name = " << d.m_name
               << ", type = " << astral::gl::Shader::gl_shader_type_label(d.m_shader_type)
               << "\nCompile Log:\n" << d.m_compile_log
               << "\n\n";
        }
    }

  ostr << "\nLink Log:\n" << m_link_log << "\n";
  m_log = ostr.str();

  if (!m_link_success && gl_program_emit_file_on_link_error)
    {
      std::ostringstream oo;

      oo << "bad_program_" << m_unique_id << "(name = " << m_name << ").glsl";

      #ifndef __EMSCRIPTEN__
        {
          std::string filename;
          filename = fullpath_from_filename(oo.str());

          std::ofstream eek(filename.c_str());
          stream_bad_program_contents(eek);
          std::cout << "Wrote to file \"" << filename << "\"\n";
        }
      #else
        {
          std::string filename;
          filename = oo.str();

          std::ostringstream eek;
          stream_bad_program_contents(eek);

          std::string eek_contents;
          eek_contents = eek.str();

          popup_dump_file(filename.c_str(), eek_contents.c_str());
          std::cout << "Wrote to file \"" << filename << "\"\n";
        }
      #endif
    }
}

unsigned int
astral::gl::Program::
total_programs_linked(void)
{
  return gl_program_total_programs_linked;
}

void
astral::gl::Program::Implement::
clear_shaders_and_save_shader_data(void)
{
  m_shader_data.resize(m_shaders.size());

  /* first fetch the translated code before requesting
   * the compile_log(), we do this so that if a browser
   * loses the GL context on trying to complete compiling
   * a shader we can still get the translated code
   */
  for(unsigned int i = 0, endi = m_shaders.size(); i<endi; ++i)
    {
      m_shader_data[i].m_source_code = m_shaders[i]->source_code();
      m_shader_data[i].m_translated_code = m_shaders[i]->translated_code();
      m_shader_data[i].m_name = m_shaders[i]->name();
      m_shader_data[i].m_shader_type = m_shaders[i]->shader_type();
    }

  for(unsigned int i = 0, endi = m_shaders.size(); i<endi; ++i)
    {
      m_shader_data[i].m_compile_log = m_shaders[i]->compile_log();
      m_shader_data[i].m_compile_success = m_shaders[i]->compile_success();
      m_shader_data_sorted_by_type[m_shader_data[i].m_shader_type].push_back(i);
      astral_glDetachShader(m_name, m_shaders[i]->name());
    }
  m_shaders.clear();
}

void
astral::gl::Program::
use_program(void)
{
  Implement *p;

  p = static_cast<Implement*>(this);

  /* NOTE: in checking if the program can be used,
   *       we assert on link_success() which may
   *       trigger a call to post_link(); since
   *       ASTRALassert is only active in debug
   *       build, we avoid the readback from GL
   *       which allos for the extension
   *       GL_KHR_parallel_shader_compile to
   *       function.
   */
  ASTRALassert(p->m_name != 0);
  ASTRALassert(p->link_success());

  astral_glUseProgram(p->m_name);

  if (!p->m_initializers.empty())
    {
      p->m_initializers.perform_initializations(this);
      p->m_initializers.clear();
    }

  if (!p->m_link_counted)
    {
      p->m_link_counted = true;
      ++gl_program_total_programs_linked;
    }
}

astral_GLuint
astral::gl::Program::
name(void)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  return p->m_name;
}

astral::c_string
astral::gl::Program::
link_log(void)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  p->post_link();
  p->generate_link_log();
  return p->m_link_log.c_str();
}

bool
astral::gl::Program::
link_success(void)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  p->post_link();
  return p->m_link_success;
}

astral::c_string
astral::gl::Program::
log(void)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  p->post_link();
  p->generate_link_log();
  return p->m_log.c_str();
}

void
astral::gl::Program::
generate_logs(void)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  p->post_link();
  p->generate_link_log();
}

unsigned int
astral::gl::Program::
num_shaders(astral_GLenum tp)
{
  std::map<astral_GLenum, std::vector<int>>::const_iterator iter;
  Implement *p;

  p = static_cast<Implement*>(this);
  p->post_link();
  p->generate_link_log();

  iter = p->m_shader_data_sorted_by_type.find(tp);
  return (iter != p->m_shader_data_sorted_by_type.end()) ?
    iter->second.size() : 0;
}

bool
astral::gl::Program::
shader_compile_success(astral_GLenum tp, unsigned int i)
{
  std::map<astral_GLenum, std::vector<int>>::const_iterator iter;
  Implement *p;

  p = static_cast<Implement*>(this);
  p->post_link();
  p->generate_link_log();

  iter = p->m_shader_data_sorted_by_type.find(tp);
  if (iter != p->m_shader_data_sorted_by_type.end() && i < iter->second.size())
    {
      return p->m_shader_data[iter->second[i]].m_compile_success;
    }
  else
    {
      return false;
    }
}

astral::c_string
astral::gl::Program::
shader_src_code(astral_GLenum tp, unsigned int i)
{
  std::map<astral_GLenum, std::vector<int>>::const_iterator iter;
  Implement *p;

  p = static_cast<Implement*>(this);
  p->post_link();
  p->generate_link_log();

  iter = p->m_shader_data_sorted_by_type.find(tp);
  if (iter != p->m_shader_data_sorted_by_type.end() && i < iter->second.size())
    {
      return p->m_shader_data[iter->second[i]].m_source_code.c_str();
    }
  else
    {
      return "";
    }
}

astral::c_string
astral::gl::Program::
shader_compile_log(astral_GLenum tp, unsigned int i)
{
  std::map<astral_GLenum, std::vector<int>>::const_iterator iter;
  Implement *p;

  p = static_cast<Implement*>(this);
  p->post_link();
  p->generate_link_log();

  iter = p->m_shader_data_sorted_by_type.find(tp);
  if (iter != p->m_shader_data_sorted_by_type.end() && i < iter->second.size())
    {
      return p->m_shader_data[iter->second[i]].m_compile_log.c_str();
    }
  else
    {
      return "";
    }
}

astral_GLint
astral::gl::Program::
uniform_location(const std::string &uniform_name)
{
  return astral_glGetUniformLocation(name(), uniform_name.c_str());
}

bool
astral::gl::Program::
emit_file_on_link_error(void)
{
  return gl_program_emit_file_on_link_error;
}

void
astral::gl::Program::
emit_file_on_link_error(bool v)
{
  gl_program_emit_file_on_link_error = v;
}

//////////////////////////////////////
// astral::gl::ProgramInitializerArray methods
void
astral::gl::ProgramInitializerArray::
perform_initializations(Program *pr) const
{
  for(const auto &v : m_values)
    {
      ASTRALassert(v);
      v->perform_initialization(pr);
    }
}

void
astral::gl::ProgramInitializerArray::
clear(void)
{
  m_values.clear();
}

//////////////////////////////////////////////////
// astral::gl::UniformBlockInitializer methods
void
astral::gl::UniformBlockInitializer::
perform_initialization(Program *pr) const
{
  int loc;
  loc = astral_glGetUniformBlockIndex(pr->name(), m_block_name.c_str());
  if (loc != -1)
    {
      astral_glUniformBlockBinding(pr->name(), loc, m_binding_point);
    }
  else
    {
      #ifdef ASTRAL_DEBUG
        {
          std::cerr << "Failed to find uniform block \""
                    << m_block_name
                    << "\" in program " << pr->name()
                    << " for initialization\n";
        }
      #endif
    }
}

/////////////////////////////////////////////////
// astral::gl::UniformInitalizerBase methods
void
astral::gl::UniformInitalizerBase::
perform_initialization(Program *pr) const
{
  int location;

  location = astral_glGetUniformLocation(pr->name(), m_uniform_name.c_str());
  if (location != - 1)
    {
      init_uniform(pr->name(), location);
    }
  else
    {
      #ifdef ASTRAL_DEBUG
        {
          std::cerr << "Failed to find uniform \"" << m_uniform_name
                    << "\" in program " << pr->name()
                    << " for initialization\n";
        }
      #endif
    }
}

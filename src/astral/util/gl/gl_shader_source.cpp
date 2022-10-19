/*!
 * \file gl_shader_source.cpp
 * \brief file gl_shader_source.cpp
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
#include <cstring>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <list>
#include <algorithm>
#include <sstream>
#include <stdint.h>

#include <astral/util/util.hpp>
#include <astral/util/astral_memory.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/static_resource.hpp>
#include <astral/util/gl/gl_shader_source.hpp>

namespace
{
  std::string
  macro_value_as_string(uint32_t v)
  {
    std::ostringstream str;
    str << v << "u";
    return str.str();
  }

  std::string
  macro_value_as_string(int32_t v)
  {
    std::ostringstream str;
    str << v;
    return str.str();
  }

  std::string
  macro_value_as_string(float v)
  {
    std::ostringstream str;
    str << "float(" << v << ")";
    return str.str();
  }

  std::string
  stripped_macro_name(const std::string &macro_name)
  {
    std::string::size_type pos;
    pos = macro_name.find('(');
    return macro_name.substr(0, pos);
  }
}

class astral::gl::ShaderSource::CodeAssembler
{
public:
  static
  void
  add_source_entry(const source_code_t &v, std::ostream &output_stream);

  static
  astral::c_string
  string_from_extension_t(extension_enable_t tp);

  static
  void
  assemble_glsl_lib_code(const std::vector<reference_counted_ptr<const ShaderLibrary>> &libs,
                         std::string *out_glsl_code,
                         std::unordered_map<std::string, extension_enable_t> *out_extensions,
                         std::string *out_glsl_version)
  {
    std::ostringstream str;
    std::unordered_set<const ShaderLibrary*> included_libs;

    assemble_glsl_lib_code(libs, str, out_extensions, out_glsl_version, &included_libs);
    *out_glsl_code = str.str();
  }

private:
  static
  std::string
  strip_leading_white_spaces(const std::string &S);

  static
  std::string
  replace_double_colon(std::string S);

  static
  void
  emit_source_line(std::ostream &output_stream,
                   const std::string &source,
                   int line_number, const std::string &label);

  static
  void
  add_source_code_from_stream(const std::string &label,
                              std::istream &istr,
                              std::ostream &output_stream);

  static
  void
  assemble_glsl_lib_code(const std::vector<astral::reference_counted_ptr<const astral::gl::ShaderLibrary>> &libs,
                         std::ostream &output_stream,
                         std::unordered_map<std::string, extension_enable_t> *in_out_extensions,
                         std::string *in_out_glsl_version,
                         std::unordered_set<const ShaderLibrary*> *included_libs);

  static
  void
  add_glsl_lib(const ShaderLibrary &lib, std::ostream &output_stream,
               std::unordered_map<std::string, extension_enable_t> *in_out_extensions,
               std::string *in_out_glsl_version,
               std::unordered_set<const ShaderLibrary*> *included_libs);
};

//////////////////////////////////////////////////
// astral::gl::ShaderSource::CodeAssembler methods
void
astral::gl::ShaderSource::CodeAssembler::
assemble_glsl_lib_code(const std::vector<astral::reference_counted_ptr<const ShaderLibrary>> &libs,
                       std::ostream &output_stream,
                       std::unordered_map<std::string, extension_enable_t> *in_out_extensions,
                       std::string *in_out_glsl_version,
                       std::unordered_set<const ShaderLibrary*> *included_libs)
{
  for (const auto &p : libs)
    {
      add_glsl_lib(*p, output_stream, in_out_extensions, in_out_glsl_version, included_libs);
    }
}

void
astral::gl::ShaderSource::CodeAssembler::
add_glsl_lib(const ShaderLibrary &lib, std::ostream &output_stream,
             std::unordered_map<std::string, extension_enable_t> *in_out_extensions,
             std::string *in_out_glsl_version,
             std::unordered_set<const ShaderLibrary*> *included_libs)
{
  if (included_libs->find(&lib) != included_libs->end())
    {
      return;
    }

  included_libs->insert(&lib);

  const ShaderSource &src(lib.content());
  if (src.m_version != "")
    {
      /* This is a little delicate since it relies on the
       * string to do the job; this is find as long as
       * compatibility, core and es sub-variants are
       * not mixing.
       */
      *in_out_glsl_version = t_max(src.m_version, *in_out_glsl_version);
    }

  for (const auto &v : src.m_extensions)
    {
      auto iter(in_out_extensions->find(v.first));
      if (iter != in_out_extensions->end())
        {
          /* The ordering of the extension_t values have that
           * code that works with larger values will also
           * woth with code with smaller values because
           * the ordering is
           *  0. require_extension
           *  1. enable_extension
           *  2. warn_extension
           *  3. disable_extension
           */
          iter->second = static_cast<enum extension_enable_t>(t_min(iter->second, v.second));
        }
      else
        {
          (*in_out_extensions)[v.first] = v.second;
        }
    }

  output_stream << "// Insert library @" << &lib << "\n";

  /* first add all dependencies */
  assemble_glsl_lib_code(src.m_libs, output_stream, in_out_extensions,
                         in_out_glsl_version, included_libs);

  /* now add the shader code */
  for(const source_code_t &v : src.m_values)
    {
      add_source_entry(v, output_stream);
    }
}

astral::c_string
astral::gl::ShaderSource::CodeAssembler::
string_from_extension_t(extension_enable_t tp)
{
  using namespace astral;
  using namespace astral::gl;
  switch(tp)
    {
    case ShaderSource::enable_extension:
      return "enable";
      break;

    case ShaderSource::require_extension:
      return "require";
      break;

    case ShaderSource::warn_extension:
      return "warn";
      break;

    case ShaderSource::disable_extension:
      return "disable";
      break;

    default:
      ASTRALassert(!"Unknown value for extension_enable_t");
      return "";
    }
}

std::string
astral::gl::ShaderSource::CodeAssembler::
replace_double_colon(std::string S)
{
  char *prev_char(nullptr);

  for (auto iter = S.begin(), end = S.end(); iter != end; ++iter)
    {
      if (prev_char && *prev_char == ':' && *iter == ':')
        {
          *prev_char = 'D';
          *iter= 'D';
        }
      prev_char= &(*iter);
    }
  return S;
}

std::string
astral::gl::ShaderSource::CodeAssembler::
strip_leading_white_spaces(const std::string &S)
{
  std::string::const_iterator iter, end;

  for(iter = S.begin(), end = S.end(); iter != end && isspace(*iter); ++iter)
    {}

  return (iter != end && *iter == '#')?
    std::string(iter, end):
    S;
}

void
astral::gl::ShaderSource::CodeAssembler::
emit_source_line(std::ostream &output_stream,
                 const std::string &source,
                 int line_number, const std::string &label)
{
  std::string S;
  S = strip_leading_white_spaces(source);
  S = replace_double_colon(S);
  output_stream << S;

  #ifndef NDEBUG
    {
      if (!label.empty() && (S.empty() || (S.back() != '\\' && S.front() != '#')))
        {
          output_stream << std::setw(80-S.length()) << "  //["
                        << std::setw(3) << line_number
                        << ", " << label
                        << "]";
        }
    }
  #else
    {
      ASTRALunused(label);
      ASTRALunused(line_number);
    }
  #endif
}

void
astral::gl::ShaderSource::CodeAssembler::
add_source_code_from_stream(const std::string &label,
                            std::istream &istr,
                            std::ostream &output_stream)
{
  std::string S;
  int line_number(0);

  while (getline(istr, S))
    {
      ++line_number;

      /* combine source lines that end with \ */
      if (!S.empty() && S.back() == '\\')
        {
          std::vector<std::string> strings;

          strings.push_back(S);
          while(!strings.back().empty() && strings.back().back() == '\\')
            {
              getline(istr, S);
              strings.push_back(S);
              ++line_number;
            }
          getline(istr, S);
          strings.push_back(S);

          /* now remove the '\\' and put on S. */
          S.clear();
          for(std::string &str : strings)
            {
              if (!str.empty() && str.back() == '\\')
                {
                  str.pop_back();
                }
              S += str;
            }
        }

      emit_source_line(output_stream, S, line_number, label);
      if (!istr.eof())
        {
          output_stream << "\n";
        }
      S.clear();
    }
}

void
astral::gl::ShaderSource::CodeAssembler::
add_source_entry(const source_code_t &v, std::ostream &output_stream)
{
  using namespace astral;
  using namespace astral::gl;

  /* NOTES:
   *   - if source is a file or resource, we add a \n
   *   - if just a string, we do NOT add a \n
   */

  if (v.second == ShaderSource::from_file)
    {
      std::ifstream file(v.first.c_str());

      if (file)
        {
          add_source_code_from_stream(v.first, file, output_stream);
          output_stream << "\n";
        }
      else
        {
          output_stream << "\n//WARNING: Could not open file \""
                        << v.first << "\"\n";
        }
    }
  else
    {
      if (v.second == ShaderSource::from_string)
        {
          std::istringstream istr;
          istr.str(v.first);
          add_source_code_from_stream("", istr, output_stream);
        }
      else
        {
          c_array<const uint8_t> resource_string;

          resource_string = fetch_static_resource(v.first.c_str());
          if (!resource_string.empty() && resource_string.back() == 0)
            {
              std::istringstream istr;
              astral::c_string s;

              s = reinterpret_cast<astral::c_string>(resource_string.c_ptr());
              istr.str(std::string(s));

              add_source_code_from_stream(v.first, istr, output_stream);
              output_stream << "\n";
            }
          else
            {
              output_stream << "\n//WARNING: Unable to fetch string resource \"" << v.first
                            << "\"\n";
              return;
            }
        }
    }
}

///////////////////////////////////////////////////
// astral::gl::ShaderSource::MacroSet::Entry methods
astral::gl::ShaderSource::MacroSet::Entry::
Entry(astral::c_string name,
      uint32_t value):
  m_name(name),
  m_value(macro_value_as_string(value))
{}

astral::gl::ShaderSource::MacroSet::Entry::
Entry(astral::c_string name,
      int32_t value):
  m_name(name),
  m_value(macro_value_as_string(value))
{}

astral::gl::ShaderSource::MacroSet::Entry::
Entry(astral::c_string name,
      float value):
  m_name(name),
  m_value(macro_value_as_string(value))
{}

/////////////////////////////////////////
// astral::gl::ShaderSource methods
astral::gl::ShaderSource&
astral::gl::ShaderSource::
specify_version(c_string v)
{
  m_version = v ? std::string(v) : "";
  m_dirty = true;
  return *this;
}

astral::c_string
astral::gl::ShaderSource::
version(void) const
{
  return m_version.c_str();
}

astral::gl::ShaderSource&
astral::gl::ShaderSource::
add_source(c_string str, enum source_t tp, enum add_location_t loc)
{
  ASTRALassert(str);
  source_code_t v(str, tp);

  if (loc == push_front)
    {
      m_values.push_front(v);
    }
  else
    {
      m_values.push_back(v);
    }
  m_dirty = true;
  return *this;
}

astral::gl::ShaderSource&
astral::gl::ShaderSource::
add_source(const ShaderSource &obj)
{
  std::copy(obj.m_values.begin(), obj.m_values.end(),
            std::inserter(m_values, m_values.end()));
  std::copy(obj.m_libs.begin(), obj.m_libs.end(),
            std::inserter(m_libs, m_libs.end()));
  m_dirty = true;
  return *this;
}

astral::gl::ShaderSource&
astral::gl::ShaderSource::
add_macro(c_string macro_name, c_string macro_value,
          enum add_location_t loc)
{
  std::ostringstream ostr;

  ostr << "#define " << macro_name << " " << macro_value << "\n";
  return add_source(ostr.str().c_str(), from_string, loc);
}

astral::gl::ShaderSource&
astral::gl::ShaderSource::
add_macro(c_string macro_name, uint32_t macro_value,
          enum add_location_t loc)
{
  std::string v(macro_value_as_string(macro_value));
  return add_macro(macro_name, v.c_str(), loc);
}

astral::gl::ShaderSource&
astral::gl::ShaderSource::
add_macro(c_string macro_name, int32_t macro_value,
          enum add_location_t loc)
{
  std::string v(macro_value_as_string(macro_value));
  return add_macro(macro_name, v.c_str(), loc);
}

astral::gl::ShaderSource&
astral::gl::ShaderSource::
add_macro(c_string macro_name, float macro_value,
          enum add_location_t loc)
{
  std::string v(macro_value_as_string(macro_value));
  return add_macro(macro_name, v.c_str(), loc);
}

astral::gl::ShaderSource&
astral::gl::ShaderSource::
add_macros(const MacroSet &macros, enum add_location_t loc)
{
  for (const MacroSet::Entry &entry : macros.m_entries)
    {
      add_macro(entry.m_name.c_str(), entry.m_value.c_str(), loc);
    }
  return *this;
}

astral::gl::ShaderSource&
astral::gl::ShaderSource::
remove_macro(c_string macro_name,
             enum add_location_t loc)
{
  std::ostringstream ostr;
  ostr << "#undef " << macro_name << "\n";
  return add_source(ostr.str().c_str(), from_string, loc);
}

astral::gl::ShaderSource&
astral::gl::ShaderSource::
remove_macros(const MacroSet &macros, enum add_location_t loc)
{
  for (const MacroSet::Entry &entry : macros.m_entries)
    {
      std::string stripped(stripped_macro_name(entry.m_name));
      remove_macro(stripped.c_str(), loc);
    }
  return *this;
}

astral::gl::ShaderSource&
astral::gl::ShaderSource::
specify_extension(c_string ext_name, enum extension_enable_t tp)
{
  m_extensions[std::string(ext_name)] = tp;
  m_dirty = true;
  return *this;
}

astral::gl::ShaderSource&
astral::gl::ShaderSource::
specify_extensions(const ShaderSource &obj)
{
  for(const auto &ext : obj.m_extensions)
    {
      m_extensions[ext.first] = ext.second;
    }
  return *this;
}

astral::c_string
astral::gl::ShaderSource::
assembled_code(bool code_only) const
{
  if (m_dirty)
    {
      std::unordered_map<std::string, extension_enable_t> extensions(m_extensions);
      std::string version(m_version), glsl_lib_code;
      std::ostringstream output_glsl_source_code;
      std::ostringstream output_glsl_source_code_base;
      static c_string begin_library_code_tag = "\n//////////////////////////////////\n// Begin LibraryCode\n\n";
      static c_string end_library_code_tag = "\n// EndLibraryCode\n///////////////////////////////////////\n\n";

      CodeAssembler::assemble_glsl_lib_code(m_libs, &glsl_lib_code, &extensions, &version);
      if (!version.empty())
        {
          output_glsl_source_code << "#version " << version << "\n";
        }

      for(const auto &ext : extensions)
        {
          output_glsl_source_code << "#extension " << ext.first << ": "
                                  << CodeAssembler::string_from_extension_t(ext.second)
                                  << "\n";
        }

      switch (m_shader_type)
        {
        case ASTRAL_GL_FRAGMENT_SHADER:
          output_glsl_source_code << "#define ASTRAL_FRAGMENT_SHADER\n";
          break;

        case ASTRAL_GL_VERTEX_SHADER:
          output_glsl_source_code << "#define ASTRAL_VERTEX_SHADER\n";
          break;

#ifndef __EMSCRIPTEN__
        case ASTRAL_GL_GEOMETRY_SHADER:
          output_glsl_source_code << "#define ASTRAL_GEOMETRY_SHADER\n";
          break;

        case ASTRAL_GL_TESS_EVALUATION_SHADER:
          output_glsl_source_code << "#define ASTRAL_TESS_EVALUATION_SHADER\n";
          break;

        case ASTRAL_GL_COMPUTE_SHADER:
          output_glsl_source_code << "#define ASTRAL_COMPUTE_SHADER\n";
          break;
#endif
        }

      #ifdef ASTRAL_DEBUG
        {
          output_glsl_source_code << "#define ASTRAL_DEBUG\n";
        }
      #endif

      #ifdef __EMSCRIPTEN__
        {
          output_glsl_source_code << "#define ASTRAL_EMSCRIPTEN";
        }
      #endif

      output_glsl_source_code << begin_library_code_tag << glsl_lib_code << end_library_code_tag;
      output_glsl_source_code_base << begin_library_code_tag << glsl_lib_code << end_library_code_tag;
      for(const source_code_t &src : m_values)
        {
          CodeAssembler::add_source_entry(src, output_glsl_source_code);
          CodeAssembler::add_source_entry(src, output_glsl_source_code_base);
        }

      /*
       * some GLSL pre-processors do not like to end on a
       * comment or other certain tokens, to make them
       * less grouchy, we emit a few extra \n's
       */
      output_glsl_source_code << "\n\n\n";
      output_glsl_source_code_base << "\n\n\n";

      m_assembled_code = output_glsl_source_code.str();
      m_assembled_code_base = output_glsl_source_code_base.str();

      #ifdef __EMSCRIPTEN__
        {
          /* GL and GLES2/3 accept shader source with ", but
           * WebGL1/2 report error if a " is present.
           */
          std::replace(m_assembled_code.begin(), m_assembled_code.end(), '"', ' ');
          std::replace(m_assembled_code_base.begin(), m_assembled_code_base.end(), '"', ' ');
        }
      #endif
      m_dirty = false;
    }

  return code_only ?
    m_assembled_code_base.c_str():
    m_assembled_code.c_str();
}

/*!
 * \file gl_shader_source.hpp
 * \brief file gl_shader_source.hpp
 *
 * Adapted from: WRATHGLProgram.hpp of WRATH:
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

#ifndef ASTRAL_GL_SHADER_SOURCE_HPP
#define ASTRAL_GL_SHADER_SOURCE_HPP

#include <sstream>
#include <vector>
#include <list>
#include <unordered_map>
#include <string>
#include <astral/util/util.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/reference_counted.hpp>
#include <astral/util/gl/astral_gl.hpp>

namespace astral {
namespace gl {

/*!\addtogroup gl_util
 * @{
 */

class ShaderLibrary;

/*!
 * \brief
 * A ShaderSource represents the source code
 * to a GLSL shader, specifying blocks of source
 * code and macros to use.
 */
class ShaderSource
{
public:
  /*!
   * \brief
   * Enumeration to indiciate the source for a shader
   * source code.
   */
  enum source_t:uint32_t
    {
      /*!
       * Shader source code is taken from the file whose
       * name is the passed string.
       */
      from_file,

      /*!
       * The passed string is the shader source code.
       */
      from_string,

      /*!
       * The passed string is label for a string of text
       * fetched with astral::fetch_static_resource().
       * The resource will be IGNORED if the last byte of
       * resource is not 0 (which indicates end-of-string).
       */
      from_resource,
    };

  /*!
   * \brief
   * Enumeration to determine if a block of
   * code is added to the fron or back
   */
  enum add_location_t:uint32_t
    {
      /*!
       * add the source code or macro
       * to the back.
       */
      push_back,

      /*!
       * add the source code or macro
       * to the front.
       */
      push_front
    };

  /*!
   * \brief
   * Enumeration to indicate extension
   * enable flags.
   */
  enum extension_enable_t:uint32_t
    {
      /*!
       * Requires the named GLSL extension,
       * i.e. will add <B>"#extension extension_name: require"</B>
       * to GLSL source code.
       */
      require_extension,

      /*!
       * Enables the named GLSL extension,
       * i.e. will add <B>"#extension extension_name: enable"</B>
       * to GLSL source code.
       */
      enable_extension,

      /*!
       * Enables the named GLSL extension,
       * but request that the GLSL compiler
       * issues warning when the extension
       * is used, i.e. will add
       * <B>"#extension extension_name: warn"</B>
       * to GLSL source code.
       */
      warn_extension,

      /*!
       * Disables the named GLSL extension,
       * i.e. will add <B>"#extension extension_name: disable"</B>
       * to GLSL source code.
       */
      disable_extension
    };

  /*!
   * \brief
   * A MacroSet represents a set of macros.
   */
  class MacroSet
  {
  public:
    /*!
     * Swap operation
     * \param obj object with which to swap
     */
    void
    swap(MacroSet &obj)
    {
      obj.m_entries.swap(m_entries);
    }

    /*!
     * Add a macro to this MacroSet.
     * \param macro_name name of macro
     * \param macro_value value to which macro is given
     */
    MacroSet&
    add_macro(c_string macro_name, c_string macro_value = "")
    {
      m_entries.push_back(Entry(macro_name, macro_value));
      return *this;
    }

    /*!
     * Add a macro to this MacroSet.
     * \param macro_name name of macro
     * \param macro_value value to which macro is given
     */
    MacroSet&
    add_macro(c_string macro_name, uint32_t macro_value)
    {
      m_entries.push_back(Entry(macro_name, macro_value));
      return *this;
    }

    /*!
     * Add a macro to this MacroSet.
     * \param macro_name name of macro
     * \param macro_value value to which macro is given
     */
    MacroSet&
    add_macro(c_string macro_name, int32_t macro_value)
    {
      m_entries.push_back(Entry(macro_name, macro_value));
      return *this;
    }

    /*!
     * Add a macro to this MacroSet.
     * \param macro_name name of macro
     * \param macro_value value to which macro is given
     */
    MacroSet&
    add_macro(c_string macro_name, float macro_value)
    {
      m_entries.push_back(Entry(macro_name, macro_value));
      return *this;
    }

    /*!
     * Add a macro to this MacroSet for casted to uint32_t
     * \param macro_name name of macro
     * \param macro_value value to which macro is given
     */
    template<typename T>
    MacroSet&
    add_macro_u32(c_string macro_name, T macro_value)
    {
      uint32_t v(macro_value);
      return add_macro(macro_name, v);
    }

    /*!
     * Add a macro to this MacroSet for casted to int32_t
     * \param macro_name name of macro
     * \param macro_value value to which macro is given
     */
    template<typename T>
    MacroSet&
    add_macro_i32(c_string macro_name, T macro_value)
    {
      int32_t v(macro_value);
      return add_macro(macro_name, v);
    }

    /*!
     * Add a macro to this MacroSet for casted to float
     * \param macro_name name of macro
     * \param macro_value value to which macro is given
     */
    template<typename T>
    MacroSet&
    add_macro_float(c_string macro_name, T macro_value)
    {
      float v(macro_value);
      return add_macro(macro_name, v);
    }

  private:
    class Entry
    {
    public:
      Entry(astral::c_string name, astral::c_string value):
        m_name(name), m_value(value)
      {}

      Entry(astral::c_string name, uint32_t value);
      Entry(astral::c_string name, int32_t value);
      Entry(astral::c_string name, float value);

      std::string m_name, m_value;
    };

    friend class ShaderSource;
    std::vector<Entry> m_entries;
  };

  /*!
   * Ctor.
   */
  ShaderSource(void):
    m_dirty(false),
    m_shader_type(ASTRAL_GL_INVALID_ENUM)
  {}

  /*!
   * Specifies the version of GLSL to which to
   * declare the shader. An empty string indicates
   * to not have a "#version" directive in the shader.
   * String is -copied-.
   */
  ShaderSource&
  specify_version(c_string v);

  /*!
   * Returns the value set by specify_version().
   * Returned pointer is only valid until the
   * next time that specify_version() is called.
   */
  c_string
  version(void) const;

  /*!
   * Add shader source code to this ShaderSource.
   * \param str string that is a filename, GLSL source or a resource name
   * \param tp interpretation of str, i.e. determines if
   *           str is a filename, raw GLSL source or a resource
   * \param loc location to add source
   */
  ShaderSource&
  add_source(c_string str, enum source_t tp = from_file,
             enum add_location_t loc = push_back);

  /*!
   * Add the sources from another ShaderSource object; does
   * NOT absorb extension or version values.
   * \param obj ShaderSource object from which to absorb
   */
  ShaderSource&
  add_source(const ShaderSource &obj);

  /*!
   * Add a macro to this ShaderSource.
   * Functionally, will insert \#define macro_name macro_value
   * in the GLSL source code.
   * \param macro_name name of macro
   * \param macro_value value to which macro is given
   * \param loc location to add macro within code
   */
  ShaderSource&
  add_macro(c_string macro_name, c_string macro_value = "",
            enum add_location_t loc = push_back);

  /*!
   * Add a macro to this ShaderSource.
   * Functionally, will insert \#define macro_name macro_value
   * in the GLSL source code.
   * \param macro_name name of macro
   * \param macro_value value to which macro is given
   * \param loc location to add macro within code
   */
  ShaderSource&
  add_macro(c_string macro_name, uint32_t macro_value,
            enum add_location_t loc = push_back);

  /*!
   * Add a macro to this ShaderSource.
   * Functionally, will insert \#define macro_name macro_value
   * in the GLSL source code.
   * \param macro_name name of macro
   * \param macro_value value to which macro is given
   * \param loc location to add macro within code
   */
  ShaderSource&
  add_macro(c_string macro_name, int32_t macro_value,
            enum add_location_t loc = push_back);

  /*!
   * Add a macro to this ShaderSource.
   * Functionally, will insert \#define macro_name macro_value
   * in the GLSL source code.
   * \param macro_name name of macro
   * \param macro_value value to which macro is given
   * \param loc location to add macro within code
   */
  ShaderSource&
  add_macro(c_string macro_name, float macro_value,
            enum add_location_t loc = push_back);

  /*!
   * Add a macro to this MacroSet for casted to uint32_t
   * \param macro_name name of macro
   * \param macro_value value to which macro is given
   * \param loc location to add macro within code
   */
  template<typename T>
  ShaderSource&
  add_macro_u32(c_string macro_name, T macro_value,
                enum add_location_t loc = push_back)
  {
    uint32_t v(macro_value);
    return add_macro(macro_name, v, loc);
  }

  /*!
   * Add a macro to this MacroSet for casted to int32_t
   * \param macro_name name of macro
   * \param macro_value value to which macro is given
   * \param loc location to add macro within code
   */
  template<typename T>
  ShaderSource&
  add_macro_i32(c_string macro_name, T macro_value,
                enum add_location_t loc = push_back)
  {
    int32_t v(macro_value);
    return add_macro(macro_name, v, loc);
  }

  /*!
   * Add a macro to this MacroSet for casted to float
   * \param macro_name name of macro
   * \param macro_value value to which macro is given
   * \param loc location to add macro within code
   */
  template<typename T>
  ShaderSource&
  add_macro_float(c_string macro_name, T macro_value,
                  enum add_location_t loc = push_back)
  {
    float v(macro_value);
    return add_macro(macro_name, v, loc);
  }

  /*!
   * Add macros of a MacroSet to this ShaderSource.
   * Functionally, will insert \#define macro_name macro_value
   * in the GLSL source code for each macro in the
   * \ref MacroSet.
   * \param macros set of macros to add
   * \param loc location to add macro within code
   */
  ShaderSource&
  add_macros(const MacroSet &macros,
             enum add_location_t loc = push_back);

  /*!
   * Functionally, will insert \#undef macro_name
   * in the GLSL source code.
   * \param macro_name name of macro
   * \param loc location to add macro within code
   */
  ShaderSource&
  remove_macro(c_string macro_name,
               enum add_location_t loc = push_back);

  /*!
   * Remove macros of a MacroSet to this ShaderSource.
   * Functionally, will insert \#undef macro_name
   * in the GLSL source code for each macro in the
   * \ref MacroSet.
   * \param macros set of macros to remove
   * \param loc location to add \#undef's
   */
  ShaderSource&
  remove_macros(const MacroSet &macros,
                enum add_location_t loc = push_back);

  /*!
   * Specifiy an extension and usage.
   * \param ext_name name of GL extension
   * \param tp usage of extension
   */
  ShaderSource&
  specify_extension(c_string ext_name,
                    enum extension_enable_t tp = enable_extension);

  /*!
   * Add all the extension specifacation from another
   * ShaderSource object to this ShaderSource objects.
   * Extension already set in this ShaderSource that
   * are specified in obj are overwritten to the values
   * specified in obj.
   * \param obj ShaderSource object from which to take
   */
  ShaderSource&
  specify_extensions(const ShaderSource &obj);

  /*!
   * Add the a library; the GLSL code of a library
   * is placed before any code added with add_source()
   * or add_macro(). In addition, all libraries added
   * indirectly through add_source(const ShaderSource&)
   * are also added before any other source code. Lastly,
   * any library's GLSL will only be added ONCE even if
   * it is added several times directly or indirectly.
   * \param lib library to add
   */
  ShaderSource&
  add_library(const reference_counted_ptr<const ShaderLibrary> &lib)
  {
    m_libs.push_back(lib);
    return *this;
  }

  /*!
   * Specify the shader stage; when specified as a recogonized
   * shader stage, then a macro is defined which is added to the
   * start of the assembled source code (after version and extension,
   * but before any libs or added sources) as:
   * - ASTRAL_GL_VERTEX_SHADER --> ASTRAL_VERTEX_SHADER
   * - ASTRAL_GL_FRAGMENT_SHADER --> ASTRAL_FRAGMENT_SHADER
   * - ASTRAL_GL_GEOMETRY_SHADER --> ASTRAL_GEOMETRY_SHADER
   * - ASTRAL_GL_TESS_EVALUATION_SHADER --> ASTRAL_TESS_EVALUATION_SHADER
   * - ASTRAL_GL_TESS_CONTROL_SHADER --> ASTRALTESS_CONTROL__SHADER
   * - ASTRAL_GL_COMPUTE_SHADER --> ASTRAL_COMPUTE_SHADER
   */
  ShaderSource&
  shader_type(astral_GLenum v)
  {
    m_dirty = m_dirty || (v != m_shader_type);
    m_shader_type = v;
    return *this;
  }

  /*!
   * Returns the GLSL code assembled. The returned string is only
   * gauranteed to be valid up until the ShaderSource object
   * is modified.
   * \param code_only if true, only return the GLSL code without
   *                  the additions of version, extension and
   *                  Astral convenience functions and macros.
   */
  c_string
  assembled_code(bool code_only = false) const;

private:
  class CodeAssembler;
  typedef std::pair<std::string, source_t> source_code_t;

  mutable bool m_dirty;
  std::list<source_code_t> m_values;
  std::unordered_map<std::string, enum extension_enable_t> m_extensions;
  std::string m_version;
  std::vector<reference_counted_ptr<const ShaderLibrary>> m_libs;
  astral_GLenum m_shader_type;

  mutable std::string m_assembled_code;
  mutable std::string m_assembled_code_base;
};

/*!
 * A ShaderLibrary represents a set of GLSL code (typically
 * functions and types) that are to be shared across shaders.
 * When assembling shaders, the GLSL code of a ShaderLibrary
 * is added only once even if it is included multiple times.
 */
class ShaderLibrary:public reference_counted<ShaderLibrary>::non_concurrent
{
public:
  /*!
   * Ctor
   * \param src contents of the ShaderLibrary
   */
  static
  reference_counted_ptr<const ShaderLibrary>
  create(const ShaderSource &src)
  {
    return ASTRALnew ShaderLibrary(src);
  }

  ~ShaderLibrary()
  {}

  /*!
   * Returns the content of the library.
   */
  const ShaderSource&
  content(void) const
  {
    return m_content;
  }

private:
  ShaderLibrary(const ShaderSource &src):
    m_content(src)
  {}

  ShaderSource m_content;
};
/*! @} */

} //namespace gl
} //namespace astral


/*!\addtogroup gl_util
 * @{
 */

/*!
 * Conveniance overload of operator<<. A wrapper over
 * \code
 * std::ostringstream tmp;
 * tmp << obj;
 * src.add_source(tmp.str().c_str(), astral::gl::ShaderSource::from_string);
 * \endcode
 * i.e. maps to operator<<(std::ostream&, const T&) to get the
 * string to add
 * \param src object to which to add GLSL source
 * \param obj generic object convertible to a string with operator<<(std::ostream&, const T&)
 */
template<typename T>
astral::gl::ShaderSource&
operator<<(astral::gl::ShaderSource &src, const T &obj)
{
  std::ostringstream tmp;

  tmp << obj;
  return src.add_source(tmp.str().c_str(), astral::gl::ShaderSource::from_string);
}

/*! @} */

#endif

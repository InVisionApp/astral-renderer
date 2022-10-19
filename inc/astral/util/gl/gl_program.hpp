/*!
 * \file gl_program.hpp
 * \brief file gl_program.hpp
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


#ifndef ASTRAL_GL_PROGRAM_HPP
#define ASTRAL_GL_PROGRAM_HPP

#include <map>

#include <astral/util/util.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/reference_counted.hpp>
#include <astral/util/gl/gl_shader_source.hpp>
#include <astral/util/gl/astral_gl.hpp>
#include <astral/util/gl/gl_uniform.hpp>

namespace astral {
namespace gl {

/*!\addtogroup gl_util
 * @{
 */

/*!
 * \brief
 * Simple Shader utility class providing a simple interface to build
 * GL shader objects using a gl::ShaderSouce as its source code.
 * A GL context must be current both at ctor and dtor of a gl::Shader.
 */
class Shader:public reference_counted<Shader>::non_concurrent
{
public:
  /*!
   * Ctor. Construct a Shader, the GL context must be current.
   * \param src GLSL source code of the shader
   * \param pshader_type type of shader, i.e. ASTRAL_GL_VERTEX_SHADER
   *                     for a vertex shader, ASTRAL_GL_FRAGMENT_SHADER
   *                     for a fragment shader, etc.
   */
  static
  reference_counted_ptr<Shader>
  create(const gl::ShaderSource &src, astral_GLenum pshader_type);

  virtual
  ~Shader()
  {}

  /*!
   * Queries the GLSL shaders's value of
   * ASTRAL_GL_COMPLETION_STATUS_KHR; this requires
   * the extension GL_KHR_parallel_shader_compile.
   */
  bool
  shader_compiled(void);

  /*!
   * Returns the GLSL source string fed to GL
   * to create the GLSL shader.
   */
  c_string
  source_code(void);

  /*!
   * Returns the GLSL compile log of
   * the GLSL source code.
   * If the shader source has not yet
   * been sent to GL for compiling, will
   * trigger those commands. Hence, should
   * only be called from the GL rendering
   * thread or if shader_ready() returns true.
   */
  c_string
  compile_log(void);

  /*!
   * For WebGL and GL via ANGLE, an implementation will
   * translate the code as returned by source_code() into
   * another shading language for the native platform.
   * This returns the translated code. If the code is not
   * translated returns an empty string.
   */
  c_string
  translated_code(void);

  /*!
   * Returns true if and only if GL
   * successfully compiled the shader.
   * If the shader source has not yet
   * been sent to GL for compiling, will
   * trigger those commands. Hence, should
   * only be called from the GL rendering
   * thread or if shader_ready() returns true.
   */
  bool
  compile_success(void);

  /*!
   * Returns the GL name (i.e. ID assigned by GL)
   * of this Shader.
   * If the shader source has not yet
   * been sent to GL for compiling, will
   * trigger those commands. Hence, should
   * only be called from the GL rendering
   * thread or if shader_ready() returns true.
   */
  astral_GLuint
  name(void);

  /*!
   * Returns the shader type of this
   * Shader as set by it's constructor.
   */
  astral_GLenum
  shader_type(void);

  /*!
   * Provided as a conveniance to return a string
   * from a GL enumeration naming a shader type.
   * For example <B>ASTRAL_GL_VERTEX_SHADER</B> will
   * return the string "ASTRAL_GL_VERTEX_SHADER".
   * Unreconized shader types will return the
   * label "UNKNOWN_SHADER_STAGE".
   */
  static
  c_string
  gl_shader_type_label(astral_GLenum ptype);

  /*!
   * Returns the default shader version to feed to
   * \ref gl::ShaderSource::specify_version() to
   * match with the GL API. If GL backend, then
   * gives "330". If GLES backend, then gives "300 es".
   */
  static
  c_string
  default_shader_version(void);

  /*!
   * When a shader fails to compile, Astral will
   * dump a file containing the GLSL fed to the
   * driver of the offending shader; Emscripten
   * builds will spawn a file download dialogue
   * box. Default value is true.
   */
  static
  bool
  emit_file_on_compile_error(void);

  /*!
   * Set the value returned by emit_file_on_compile_error().
   */
  static
  void
  emit_file_on_compile_error(bool);

private:
  class Implement;

  Shader(void)
  {}
};

/*!
 * \brief
 * A PreLinkAction is an action to apply to a \ref Program
 * after attaching shaders but before linking.
 */
class PreLinkAction:
  public reference_counted<PreLinkAction>::non_concurrent
{
public:
  virtual
  ~PreLinkAction()
  {}

  /*!
   * To be implemented by a derived class to perform an action _before_ the
   * GLSL program is linked. Default implementation does nothing.
   * \param glsl_program GL name of GLSL program on which to perform the action.
   */
  virtual
  void
  action(astral_GLuint glsl_program) const = 0;
};


/*!
 * \brief
 * A BindAttribute inherits from \ref PreLinkAction,
 * it's purpose is to bind named vertices to named
 * locations, i.e. it calls glBindAttributeLocation().
 */
class BindAttribute:public PreLinkAction
{
public:
  /*!
   * Ctor.
   * \param pname name of vertex in GLSL code
   * \param plocation location to which to place the vertex
   */
  static
  reference_counted_ptr<BindAttribute>
  create(const std::string& pname, int plocation)
  {
    return ASTRALnew BindAttribute(pname, plocation);
  }

  virtual
  void
  action(astral_GLuint glsl_program) const override final;

private:
  BindAttribute(const std::string& pname, int plocation):
    m_label(pname),
    m_location(plocation)
  {}

  std::string m_label;
  int m_location;
};

/*!
 * \brief
 * A ProgramSeparable inherits from \ref PreLinkAction,
 * its purpose is to set a GLSL program as separable,
 * so that it can be used by a GLSL pipeline.
 * Using a ProgramSeparable requires:
 * - for GLES: GLES3.0 or higher
 * - for GL: either GL version 4.1 or the extension GL_ARB_separate_shader_objects
 */
class ProgramSeparable:public PreLinkAction
{
public:
  /*!
   * Ctor.
   */
  static
  reference_counted_ptr<ProgramSeparable>
  create(void)
  {
    return ASTRALnew ProgramSeparable();
  }

  virtual
  void
  action(astral_GLuint glsl_program) const override final;

private:
  ProgramSeparable(void)
  {}
};

/*!
 * \brief
 * A TransformFeedbackVarying encapsulates a call to
 * glTransformFeedbackVarying. Note that if there are
 * multiple \ref TransformFeedbackVarying objects on a
 * single \ref PreLinkActionArray, then only the last
 * one added has effect.
 */
class TransformFeedbackVarying:public PreLinkAction
{
public:
  /*!
   * Ctor.
   * \param buffer_mode the buffer mode to use on
   *        glTransformFeedbackVarying.
   */
  static
  reference_counted_ptr<TransformFeedbackVarying>
  create(astral_GLenum buffer_mode = ASTRAL_GL_INTERLEAVED_ATTRIBS)
  {
    return ASTRALnew TransformFeedbackVarying(buffer_mode);
  }

  /*!
   * Return a reference to an array holding the names
   * ofthe varyings to capture in transform feedback in
   * the order they will be captured; modify this object
   * to change what is captured in transform feedback.
   */
  std::vector<std::string>&
  transform_feedback_varyings(void)
  {
    return m_transform_feedback_varyings;
  }

  /*!
   * Returns the array holding the names of the varyings to
   * capture in transform feedback in the order they will
   * be captured.
   */
  const std::vector<std::string>&
  transform_feedback_varyings(void) const
  {
    return m_transform_feedback_varyings;
  }

  virtual
  void
  action(astral_GLuint glsl_program) const override final;

private:
  explicit
  TransformFeedbackVarying(astral_GLenum buffer_mode = ASTRAL_GL_INTERLEAVED_ATTRIBS):
    m_buffer_mode(buffer_mode)
  {}

  astral_GLenum m_buffer_mode;
  std::vector<std::string> m_transform_feedback_varyings;
};

/*!
 * \brief
 * A PreLinkActionArray is a conveniance class
 * wrapper over an array of \ref PreLinkAction handles.
 */
class PreLinkActionArray
{
public:
  /*!
   * Swap operation
   * \param obj object with which to swap
   */
  void
  swap(PreLinkActionArray &obj)
  {
    m_values.swap(obj.m_values);
  }

  /*!
   * Add a prelink action to execute.
   * \param h handle to action to add
   */
  PreLinkActionArray&
  add(reference_counted_ptr<PreLinkAction> h)
  {
    ASTRALassert(h);
    m_values.push_back(h);
    return *this;
  }

  /*!
   * Provided as a conveniance, equivalent to
   * \code
   * add(ASTRALnew BindAttribute(pname, plocation))
   * \endcode
   * \param pname name of the vertex
   * \param plocation location to which to bind the vertex.
   */
  PreLinkActionArray&
  add_binding(const std::string& pname, int plocation)
  {
    reference_counted_ptr<PreLinkAction> h;
    h = BindAttribute::create(pname, plocation);
    return add(h);
  }

  /*!
   * Provided as a conveniance, equivalent to
   * \code
   * reference_counted_ptr<TransformFeedbackVarying> h;
   * h = TransformFeedbackVarying(buffer_mode);
   * h->transform_feedback_varyings() = varyings;
   * add(h)
   * \endcode
   * \param varyings list of varyings to capture for transform feedback
   * \param buffer_mode buffer mode (i.e. interleaved or not) for transform feedback
   */
  PreLinkActionArray&
  set_transform_feedback(const std::vector<std::string> &varyings,
                         astral_GLenum buffer_mode = ASTRAL_GL_INTERLEAVED_ATTRIBS)
  {
    reference_counted_ptr<TransformFeedbackVarying> h;
    h = TransformFeedbackVarying::create(buffer_mode);
    h->transform_feedback_varyings() = varyings;
    return add(h);
  }

  /*!
   * Executes PreLinkAction::action() for each of those
   * actions added via add().
   */
  void
  execute_actions(astral_GLuint glsl_program) const;

private:
  std::vector<reference_counted_ptr<PreLinkAction>> m_values;
};

class Program;

/*!
 * \brief
 * A ProgramInitializer is a functor object called the first time
 * a Program is bound (i.e. the first
 * time Program::use_program() is called).
 * It's main purpose is to facilitate initializing
 * uniform values.
 */
class ProgramInitializer:
  public reference_counted<ProgramInitializer>::non_concurrent
{
public:
  virtual
  ~ProgramInitializer()
  {}

  /*!
   * To be implemented by a derived class to perform additional
   * one-time actions. Function is called the first time the program
   * is used and the program is bound.
   * \param pr Program to initialize
   */
  virtual
  void
  perform_initialization(Program *pr) const = 0;
};

/*!
 * \brief
 * Conveniance class to hold an array of handles
 * of ProgramInitializer objects
 */
class ProgramInitializerArray
{
public:
  /*!
   * Swap operation
   * \param obj object with which to swap
   */
  void
  swap(ProgramInitializerArray &obj)
  {
    m_values.swap(obj.m_values);
  }

  /*!
   * Add an initializer
   * \param h handle to initializer to add
   */
  ProgramInitializerArray&
  add(reference_counted_ptr<ProgramInitializer> h)
  {
    m_values.push_back(h);
    return *this;
  }

  /*!
   * Provided as a conveniance, creates a UniformInitializer
   * object and adds that via add().
   * \param uniform_name name of uniform in GLSL to initialize
   * \param value value with which to set the uniform
   */
  template<typename T>
  ProgramInitializerArray&
  add_uniform_initializer(const std::string& uniform_name, const T &value);

  /*!
   * Provided as a conveniance, creates a SamplerInitializer
   * object and adds that via add().
   * \param uniform_name name of uniform in GLSL to initialize
   * \param value value with which to set the uniform, in this
   *              case specifies the texture unit as follows:
   *              a value of n means to use ASTRAL_GL_TEXTUREn texture
   *              unit.
   */
  ProgramInitializerArray&
  add_sampler_initializer(const std::string& uniform_name, int value);

  /*!
   * Provided as a conveniance, creates a UniformBlockInitializer
   * object and adds that via add().
   * \param uniform_name name of uniform in GLSL to initialize
   * \param value value with which to set the uniform, in this
   *              case specifies the binding point index to
   *              pass to glBindBufferBase or glBindBufferRange.
   */
  ProgramInitializerArray&
  add_uniform_block_binding(const std::string& uniform_name, int value);

  /*!
   * For each objected added via add(), call
   * ProgramInitializer::perform_initialization().
   * \param pr Program to pass along
   */
  void
  perform_initializations(Program *pr) const;

  /*!
   * Clear all elements that have been added via add().
   */
  void
  clear(void);

  /*!
   * Returns true if this ProgramInitializerArray is empty
   */
  bool
  empty(void)
  {
    return m_values.empty();
  }

private:
  std::vector<reference_counted_ptr<ProgramInitializer>> m_values;
};

/*!
 * \brief
 * Class for creating and using GLSL programs. The GL context
 * must be current at construction and destruction.
 */
class Program:public reference_counted<Program>::non_concurrent
{
public:
  /*!
   * Ctor. The GL context must be current.
   * \param pshaders shaders used to create the Program
   * \param action specifies actions to perform before linking of the Program
   * \param initers one-time initialization actions to perform at GLSL
   *                program creation
   */
  static
  reference_counted_ptr<Program>
  create(c_array<const reference_counted_ptr<Shader>> pshaders,
         const PreLinkActionArray &action = PreLinkActionArray(),
         const ProgramInitializerArray &initers = ProgramInitializerArray());

  /*!
   * Ctor. The GL context must be current.
   * \param vert_shader pointer to vertex shader to use for the Program
   * \param frag_shader pointer to fragment shader to use for the Program
   * \param action specifies actions to perform before and
   *               after linking of the Program.
   * \param initers one-time initialization actions to perform at GLSL
   *                program creation
   */
  static
  reference_counted_ptr<Program>
  create(reference_counted_ptr<Shader> vert_shader,
         reference_counted_ptr<Shader> frag_shader,
         const PreLinkActionArray &action = PreLinkActionArray(),
         const ProgramInitializerArray &initers = ProgramInitializerArray());

  /*!
   * Ctor. The GL context must be current.
   * \param vert_shader pointer to vertex shader to use for the Program
   * \param frag_shader pointer to fragment shader to use for the Program
   * \param action specifies actions to perform before and
   *               after linking of the Program.
   * \param initers one-time initialization actions to perform at GLSL
   *                program creation
   */
  static
  reference_counted_ptr<Program>
  create(const gl::ShaderSource &vert_shader,
         const gl::ShaderSource &frag_shader,
         const PreLinkActionArray &action = PreLinkActionArray(),
         const ProgramInitializerArray &initers = ProgramInitializerArray());

  /*!
   * Ctor. Create a \ref Program from a previously linked GL shader.
   * \param pname GL ID of previously linked shader
   * \param take_ownership if true when dtor is called glDeleteProgram
   *                       is called as well
   */
  static
  reference_counted_ptr<Program>
  create(astral_GLuint pname, bool take_ownership);

  virtual
  ~Program(void)
  {}

  /*!
   * Call to set GL to use the GLSLProgram
   * of this Program. The GL context
   * must be current.
   */
  void
  use_program(void);

  /*!
   * Returns the GL name (i.e. ID assigned by GL,
   * for use in glUseProgram) of this Program.
   */
  astral_GLuint
  name(void);

  /*!
   * Queries the GLSL program's value of
   * ASTRAL_GL_COMPLETION_STATUS_KHR; this requires
   * the extension GL_KHR_parallel_shader_compile.
   * The motivation is that a caller can instead
   * use a fallback gl::Program to avoid waiting
   * for the link to complete in the GL driver.
   */
  bool
  program_linked(void);

  /*!
   * Returns the link log of this Program,
   * essentially the value returned by
   * glGetProgramInfoLog. The GL context must
   * be current on the first call.
   */
  c_string
  link_log(void);

  /*!
   * Returns true if and only if this Program
   * successfully linked. The GL context must
   * be current on the first call.
   */
  bool
  link_success(void);

  /*!
   * Returns the full log (including shader source
   * code and link_log()) of this Program. A GL
   * context must be current on the first call.
   */
  c_string
  log(void);

  /*!
   * Returns the location of a uniform; just a wrapper over
   * glGetUniformLocation. The GL context must be current.
   */
  astral_GLint
  uniform_location(const std::string& name);

  /*!
   * Returns the number of shaders of a given type attached to
   * the Program. The GL context must be current.
   * \param tp GL enumeration of the shader type, see Shader::shader_type()
   */
  unsigned int
  num_shaders(astral_GLenum tp);

  /*!
   * Returns true if the source code string for a shader
   * attached to the Program compiled successfully. The GL
   * context must be current.
   * \param tp GL enumeration of the shader type, see Shader::shader_type()
   * \param i which shader with 0 <= i < num_shaders(tp)
   */
  bool
  shader_compile_success(astral_GLenum tp, unsigned int i);

  /*!
   * Returns the source code string for a shader attached to
   * the Program. The GL context must be current.
   * \param tp GL enumeration of the shader type, see Shader::shader_type()
   * \param i which shader with 0 <= i < num_shaders(tp)
   */
  c_string
  shader_src_code(astral_GLenum tp, unsigned int i);

  /*!
   * Returns the compile log for a shader attached to
   * the Program. The GL context must be current.
   * \param tp GL enumeration of the shader type, see Shader::shader_type()
   * \param i which shader with 0 <= i < num_shaders(tp)
   */
  c_string
  shader_compile_log(astral_GLenum tp, unsigned int i);

  /*!
   * This function forces the generation of logs and the querying
   * of the compile and link status of the Program.
   */
  void
  generate_logs(void);

  /*!
   * Returns the total number of Program objects that have been linked.
   */
  static
  unsigned int
  total_programs_linked(void);

  /*!
   * Each gl::Program maintains an internal query marker value
   * which is an integer. In addition, there is a global query
   * counter value as well. When program_linked() is called when
   * it requires to query the GL API, if the internal query
   * marker is greater than or equal to the global query marker,
   * then the function early outs as false. Otherwise, it performs
   * the query and sets the internal query counter value to the
   * value of the global counter. This function increments the
   * global counter. The purpose of such logic is to prevent
   * querying the GL API excessively on the same gl::Program.
   *
   * This function increments the global query counter.
   */
  static
  void
  increment_global_query_counter(void);

  /*!
   * When a program fails to link, Astral will
   * dump a file containing the GLSL fed to the
   * driver of the offending program; Emscripten
   * builds will spawn a file download dialogue
   * box. Default value is true.
   */
  static
  bool
  emit_file_on_link_error(void);

  /*!
   * Set the value returned by emit_file_on_link_error().
   */
  static
  void
  emit_file_on_link_error(bool);

private:
  class Implement;

  Program(void)
  {}
};

/*!
 * \brief
 * A UniformInitalizerBase is a base class for
 * initializing a uniform, the actual GL call to
 * set the uniform value is to be implemented by
 * a derived class in init_uniform().
 */
class UniformInitalizerBase:public ProgramInitializer
{
public:
  /*!
   * Ctor.
   * \param uniform_name name of uniform to initialize
   */
  UniformInitalizerBase(const std::string& uniform_name):
    m_uniform_name(uniform_name)
  {}

  virtual
  ~UniformInitalizerBase()
  {}

  virtual
  void
  perform_initialization(Program *pr) const override final;

protected:

  /*!
   * To be implemented by a derived class to make the GL call
   * to initialize a uniform in a GLSL shader. Caller gaurantees
   * that the program is bound (via glUseProgram()).
   * \param program GL program
   * \param location location of uniform
   */
  virtual
  void
  init_uniform(astral_GLuint program, int location) const = 0;

private:
  std::string m_uniform_name;
};

/*!
 * \brief
 * Initialize a uniform via the templated
 * overloaded function astral::gl::Uniform().
 */
template<typename T>
class UniformInitializer:public UniformInitalizerBase
{
public:
  /*!
   * Ctor.
   * \param uniform_name name of uniform in GLSL to initialize
   * \param value value with which to set the uniform
   */
  static
  reference_counted_ptr<UniformInitializer>
  create(const std::string& uniform_name, const T &value)
  {
    return ASTRALnew UniformInitializer(uniform_name, value);
  }

protected:

  virtual
  void
  init_uniform(astral_GLuint program, int location) const override final
  {
    ASTRALunused(program);
    Uniform(location, m_value);
  }

private:
  UniformInitializer(const std::string& uniform_name, const T &value):
    UniformInitalizerBase(uniform_name),
    m_value(value)
  {}

  T m_value;
};

/*!
 * \brief
 * Specialization for type c_array<const T> for \ref UniformInitializer
 * so that data behind the c_array is copied
 */
template<typename T>
class UniformInitializer<c_array<const T>>:public UniformInitalizerBase
{
public:
  /*!
   * Ctor.
   * \param uniform_name name of uniform in GLSL to initialize
   * \param value value with which to set the uniform
   */
  static
  reference_counted_ptr<UniformInitializer>
  create(const std::string& uniform_name, const c_array<const T> &value)
  {
    return ASTRALnew UniformInitializer(uniform_name, value);
  }

  ~UniformInitializer()
  {
    if (m_data != nullptr)
      {
        for (unsigned int i = 0; i < m_value.size(); ++i)
          {
            m_data[i].~T();
          }
        ASTRALfree(m_data);
      }
  }

protected:

  virtual
  void
  init_uniform(astral_GLuint program, int location) const override final
  {
    ASTRALunused(program);
    Uniform(location, m_value);
  }

private:
  UniformInitializer(const std::string& uniform_name, const c_array<const T> &value):
    UniformInitalizerBase(uniform_name),
    m_data(nullptr)
  {
    if (!value.empty())
      {
        m_data = ASTRALmalloc(sizeof(T) * value.size());
        for (unsigned int i = 0; i < value.size(); ++i)
          {
            new (&m_data[i]) T(value[i]);
          }
        m_value = c_array<const T>(m_data, value.size());
      }
  }

  T *m_data;
  c_array<const T> m_value;
};

/*!
 * \brief
 * Class to intialize the binding points of samplers.
 * If the uniform is an array, the first element will
 * be given the specified binding point and successive
 * elements in the array will be given successive
 * binding points.
 */
typedef UniformInitializer<int> SamplerInitializer;

/*!
 * \brief
 * A UniformBlockInitializer is used to initalize the binding point
 * used by a bindable uniform (aka Uniform buffer object, see the
 * GL spec on glGetUniformBlockIndex and glUniformBlockBinding.
 */
class UniformBlockInitializer:public ProgramInitializer
{
public:
  /*!
   * Ctor.
   * \param name name of uniform block in GLSL to initialize
   * \param binding_point_index value with which to set the uniform
   */
  static
  reference_counted_ptr<UniformBlockInitializer>
  create(const std::string& name, int binding_point_index)
  {
    return ASTRALnew UniformBlockInitializer(name, binding_point_index);
  }

  virtual
  void
  perform_initialization(Program *pr) const override final;

private:
  UniformBlockInitializer(const std::string& name, int binding_point_index):
    m_block_name(name),
    m_binding_point(binding_point_index)
  {}

  std::string m_block_name;
  int m_binding_point;
};

//////////////////////////////////////////////////
// inlined methods that need above classes defined
template<typename T>
ProgramInitializerArray&
ProgramInitializerArray::
add_uniform_initializer(const std::string& uniform_name, const T &value)
{
  return add(UniformInitializer<T>::create(uniform_name, value));
}

inline
ProgramInitializerArray&
ProgramInitializerArray::
add_sampler_initializer(const std::string& uniform_name, int value)
{
  return add(SamplerInitializer::create(uniform_name, value));
}

inline
ProgramInitializerArray&
ProgramInitializerArray::
add_uniform_block_binding(const std::string& uniform_name, int value)
{
  return add(UniformBlockInitializer::create(uniform_name, value));
}

/*! @} */

} //namespace gl
} //namespace astral

#endif

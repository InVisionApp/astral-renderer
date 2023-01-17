/*!
 * \file gl_binding.hpp
 * \brief file gl_binding.hpp
 *
 * Adapted from: ngl_backend.hpp of WRATH:
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

#ifndef ASTRAL_GL_BINDING_HPP
#define ASTRAL_GL_BINDING_HPP

#include <astral/util/util.hpp>
#include <astral/util/api_callback.hpp>

namespace astral {

/*!\addtogroup gl_util
 * @{
 */

/*!
 * \brief
 * Provides interface for application to use GL where function pointers
 * are auto-resolved transparently and under debug provides error checking.
 * The header defines for each GL/GLES function, glFoo, the macro astral_glFoo.
 *
 * Short version:
 *  - Native application MUST call astral::gl_binding::get_proc_function()
 *    to set the function which will be used to fetch GL function pointers;
 *    WASM applications do not call it as all GL functions are resolved at
 *    build time instead.
 *  - If an application wishes, one can include <astral/util/gl/astral_gl.hpp>.
 *    The header will add the GL function-macros and an application can
 *    issue GL calls without needing to fetch the GL functions via
 *    astral_glFoo where glFoo is the GL funcion to all. Under release,
 *    the macros are defined to function pointers that automatically set
 *    themselves up correcty. For debug, the macros preceed and postceed each
 *    GL function call with error checking call backs so an application writer
 *    can quickly know what line/file triggered an GL error.
 *  - In addition to definining the macro function astral_glFoo() for each
 *    GL and GLES function glFoo(), the header <astral/util/gl/astral_gl.hpp>
 *    also defines the macro-enum value ASTRAL_GL_FOO for each GL_FOO value of
 *    the GL core and GLES3.x API's. In addition, for each type GLtype it also
 *    defines the type astral_GLtype. Thus, one can rely on <astral/util/gl/astral_gl.hpp>
 *    to interact with both the GL and GLES API's and it does NOT collide with
 *    the system GL or GLES headers.
 *  - One caveat is that for native it includes all enumerations and functions
 *    suitable from GL/glcorearb.h, GLES3/gl32.h and GLES2/gl2ext.h where as the
 *    emscripten build only includes enumerations and functions from GLES3/gl3.h.
 *
 * Long Version:
 *
 * The namespace gl_binding provides an interface for an application
 * to specify how to fetch GL function pointers (see
 * astral::gl_binding::get_proc_function()) and additional
 * functionality of where to write/store GL error messages. An application
 * can also use this functionality by including <astral/util/gl/astral_gl.hpp>.
 * The header will create a macro astral_glFoo for each GL function
 * glFoo. If ASTRAL_GL_DEBUG is defined, each GL call will be preceded
 * by a callback and postceeded by another call back. The preceed callback
 * to the GL call will call the implementation of CallbackGL::pre_call() of
 * each active CallbackGL object. The post-process callback will repeatedly
 * call glGetError (until it returns no error) to build an error-string. If
 * the error string is non-empty, it is printed to stderr. In addition,
 * regardless if the error-string is non-empty, CallbackGL::post_call() of
 * each active CallbackGL is called.
 *
 * The gl_binding system requires that an application provides
 * a function which the binding system uses to fetch function
 * pointers for the GL API, this is set via
 * gl_binding::get_proc_function().
 *
 * Lastly, before using a GL (or GLES) function, an application should
 * check the GL implementation supports the named function by examining
 * the GL version and/or extensions itself before using the function;
 * the functions in the namespace astral::gl::ContextProperties provide
 * a more friendly interface to check for API version, if API is ES or
 * not along with what extensions are present.
 */
namespace gl_binding {

/*!
 * \brief
 * A CallbackGL defines the interface (via its base class)
 * for callbacks before and after each GL call.
 */
class CallbackGL:public APICallbackSet::CallBack
{
public:
  CallbackGL(void);
};

/*!
 * Sets the function that the system uses
 * to fetch the function pointers for GL or GLES.
 * \param get_proc value to use, default is nullptr.
 * \param fetch_functions if true, fetch all GL functions
 *                        immediately instead of fetching on
 *                        first call.
 */
void
get_proc_function(void* (*get_proc)(c_string),
                  bool fetch_functions = true);
/*!
 * Sets the function that the system uses
 * to fetch the function pointers for GL or GLES.
 * \param datum data pointer passed to get_proc when invoked
 * \param get_proc value to use, default is nullptr.
 * \param fetch_functions if true, fetch all GL functions
 *                        immediately instead of fetching on
 *                        first call.
 */
void
get_proc_function(void *datum,
                  void* (*get_proc)(void*, c_string),
                  bool fetch_functions = true);

/*!
 * Fetches a GL function using the function fetcher
 * passed to get_proc_function().
 * \param function name of function to fetch
 */
void*
get_proc(c_string function);

/*!
 * Function that implements \ref ASTRAL_GL_MESSAGE
 */
void
message(c_string message, c_string src_file, int src_line);

/*!
 * Under -debug- builds, will emit a call to glStringMarkerGREMEDY()
 * which many GL tracing applications and debuggers record; this
 * allows one to see the file and line number of a GL call within
 * the GL call trace.
 */
void
enable_gl_string_marker(bool b);

///@cond
void
emit_string(c_string label, c_string file, int line);
///@endcond

}

/*!\def ASTRAL_GL_MESSAGE
 * Use this macro to emit a string to each of the
 * gl_binding::CallbackGL objects that are active;
 * only has effect in debug builds.
 */
#define ASTRAL_GL_MESSAGE(X) \
  gl_binding::message(X, __FILE__, __LINE__)

/*!
 * \def ASTRAL_GL_EMIT_STRING
 * Macro that emits astral_glStringMarkerGREMEDY() command (if the GL
 * implementation supports it). The main use case is to emit this
 * markers so that when viewing a GL call trace, one can get some
 * commentary from where the call was emitted. Active in both debug
 * and release builds.
 */
#define ASTRAL_GL_EMIT_STRING(X) astral::gl::emit_string(X, __FILE__, __LINE__)

/*! @} */

}

#endif

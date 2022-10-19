// MACHINE GENERATED FILE - DO NOT EDIT
//
// OpenGL Hybrid Header
// (C)2022 InvisonApp

#ifndef ASTRAL_NGL_HPP

#include <cstdint>

#include <astral/util/gl/astral_gl_platform.h>

#ifdef __EMSCRIPTEN__

#include <GLES3/gl3.h>

#define ASTRALglfunctionExists(name) true
#define ASTRALglfunctionPointer(name) name

#else

#define ASTRALglfunctionExists(name) astral::gl_binding::exists_function_##name()
#define ASTRALglfunctionPointer(name) astral::gl_binding::get_function_ptr_##name()

#endif

namespace astral
{
namespace gl_binding
{
// External declarations.
void* get_proc(const char* name);
void load_all_functions(bool emit_load_warning);
}
}

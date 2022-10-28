#include <cstdint>
#include <astral/util/gl/astral_gl_platform.h>
#include <astral/util/gl/astral_gl.hpp>

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#endif

namespace astral
{
namespace gl_binding
{
void* get_proc(const char* name);
void load_all_functions(bool emit_load_warning);
void on_load_function_error(const char* name);
void call_unloadable_function(const char* name);
void post_call(const char* call, const char* src, const char* name, void* ptr, const char* file, int line);
void pre_call(const char* call, const char* src, const char* name, void* ptr, const char* file, int line);

template<typename T> T empty_return_value(void) { return T(); }
template<typename T> class type_tag {};

template<typename T, typename F, typename ...Args>
T debug_invoke(type_tag<T>, const std::string& call_string, const char* file, int line, const char* call, const char* fname, F f, Args&&... args)
{
    pre_call(call_string.c_str(), call, fname, (void*)f, file, line);
    T result = f(std::forward<Args>(args)...);
    post_call(call_string.c_str(), call, fname, (void*)f, file, line);
    return result;
}

template<typename F, typename ...Args>
void debug_invoke(type_tag<void>, const std::string& call_string, const char* file, int line, const char* call, const char* fname, F f, Args&&... args)
{
    pre_call(call_string.c_str(), call, fname, (void*)f, file, line);
    f(std::forward<Args>(args)...);
    post_call(call_string.c_str(), call, fname, (void*)f, file, line);
}

template<typename T>
static
T
printable(T v)
{
  return v;
}

static
int
printable(signed char v)
{
  return v;
}

static
unsigned int
printable(unsigned char v)
{
  return v;
}

/* -*- mode: c++; c-file-style: "gnu"; -*- */

/*!
 * \file sdl_demo.cpp
 * \brief file sdl_demo.cpp
 *
 * Adapted from: sdl_demo.cpp of WRATH:
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

#include <typeinfo>
#include <fstream>
#include <iomanip>
#include <iostream>

#include <SDL_syswm.h>
#include <SDL_video.h>
#include <SDL_render.h>
#include <SDL_surface.h>

#include <astral/util/vecN.hpp>
#include <astral/util/ostream_utility.hpp>
#include <astral/util/astral_memory.hpp>
#include <astral/util/memory_pool.hpp>
#include <astral/util/gl/gl_binding.hpp>
#include <astral/util/gl/gl_get.hpp>
#include <astral/util/gl/gl_context_properties.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "stream_holder.hpp"
#include "ImageSaver.hpp"
#include "ImageLoader.hpp"
#include "ImageCompare.hpp"
#include "sdl_demo.hpp"

////////////////////////////////////////////////////////////
// Some values have different default values for Emscripten
#ifndef __EMSCRIPTEN__
#define DEFAULT_WIDTH 3000
#define DEFAULT_HEIGHT 2000

#else
#define DEFAULT_WIDTH 1920
#define DEFAULT_HEIGHT 1080
#endif


namespace
{
  int
  GetSDLGLValue(SDL_GLattr arg)
  {
    int R(0);
    SDL_GL_GetAttribute(arg, &R);
    return R;
  }

  void
  print_gl_extensions(std::ostream &dst)
  {
    int cnt;

    cnt = astral::gl::context_get<astral_GLint>(ASTRAL_GL_NUM_EXTENSIONS);
    dst << "\nGL_EXTENSIONS(" << cnt << "):";
    for(int i = 0; i < cnt; ++i)
      {
        dst << "\n\t" << astral_glGetStringi(ASTRAL_GL_EXTENSIONS, i);
      }
  }

  void
  multiply_mouse_position_of_sdl_event(astral::vec2 f, SDL_Event &ev)
  {
    switch(ev.type)
      {
      case SDL_MOUSEBUTTONUP:
      case SDL_MOUSEBUTTONDOWN:
        ev.button.x = f.x() * static_cast<float>(ev.button.x);
        ev.button.y = f.y() * static_cast<float>(ev.button.y);
        break;

      case SDL_MOUSEMOTION:
        ev.motion.x = f.x() * static_cast<float>(ev.motion.x);
        ev.motion.y = f.y() * static_cast<float>(ev.motion.y);
        ev.motion.xrel = f.x() * static_cast<float>(ev.motion.xrel);
        ev.motion.yrel = f.y() * static_cast<float>(ev.motion.yrel);
        break;
      }
  }

  void*
  get_proc(astral::c_string proc_name)
  {
    return SDL_GL_GetProcAddress(proc_name);
  }

  class OstreamLogger:public astral::gl_binding::CallbackGL
  {
  public:
    explicit
    OstreamLogger(const astral::reference_counted_ptr<StreamHolder> &str):
      m_str(str)
    {}

    virtual
    void
    pre_call(astral::c_string call_string_values,
             astral::c_string call_string_src,
             astral::c_string function_name,
             void *function_ptr,
             astral::c_string src_file, int src_line);

    virtual
    void
    post_call(astral::c_string call_string_values,
              astral::c_string call_string_src,
              astral::c_string function_name,
              astral::c_string error_string,
              void *function_ptr,
              astral::c_string src_file, int src_line);

    virtual
    void
    message(astral::c_string message,
            astral::c_string src_file, int src_line);

    std::ostream&
    stream(void)
    {
      return m_str->stream();
    }

  private:
    astral::reference_counted_ptr<StreamHolder> m_str;
  };
}

//////////////////////////
// OstreamLogger methods
void
OstreamLogger::
pre_call(astral::c_string call_string_values,
         astral::c_string call_string_src,
         astral::c_string function_name,
         void *function_ptr,
         astral::c_string src_file, int src_line)
{
  ASTRALunused(call_string_src);
  ASTRALunused(function_name);
  ASTRALunused(function_ptr);
  m_str->stream() << "Pre: [" << src_file << "," << src_line << "] "
                  << call_string_values << "\n";
}

void
OstreamLogger::
post_call(astral::c_string call_string_values,
          astral::c_string call_string_src,
          astral::c_string function_name,
          astral::c_string error_string,
          void *function_ptr,
          astral::c_string src_file, int src_line)
{
  ASTRALunused(call_string_src);
  ASTRALunused(function_name);
  ASTRALunused(function_ptr);
  ASTRALunused(error_string);
  m_str->stream() << "Post: [" << src_file << "," << src_line << "] "
                  << call_string_values;

  if (error_string && *error_string)
    {
      m_str->stream() << "{" << error_string << "}";
    }
  m_str->stream() << "\n";
}

void
OstreamLogger::
message(astral::c_string message,
        astral::c_string src_file,
        int src_line)
{
  m_str->stream() << "Message: [" << src_file << "," << src_line << "] "
                  << message << "\n";
}

////////////////////////////
// sdl_demo methods
sdl_demo::
sdl_demo(const std::string &about_text, bool dimensions_must_match_default_value):
  m_handle_events(true),
  m_current_frame(0),
  m_about(command_line_argument::tabs_to_spaces(command_line_argument::format_description_string("", about_text))),
  m_common_label("Screen and Context Option", *this),
  m_red_bits(8, "red_bits",
             "Bpp of red channel, non-positive values mean use SDL defaults",
             *this),
  m_green_bits(8, "green_bits",
               "Bpp of green channel, non-positive values mean use SDL defaults",
               *this),
  m_blue_bits(8, "blue_bits",
              "Bpp of blue channel, non-positive values mean use SDL defaults",
              *this),
  m_alpha_bits(8, "alpha_bits",
               "Bpp of alpha channel, non-positive values mean use SDL defaults",
               *this),
  m_depth_bits(24, "depth_bits",
               "Bpp of depth buffer, non-positive values mean use SDL defaults",
               *this),
  m_stencil_bits(8, "stencil_bits",
                 "Bpp of stencil buffer, non-positive values mean use SDL defaults",
                 *this),
  m_srgb_capable(true, "srgb_capable",
                 "Set to true to request via SDL an SRGB backing surface for the window "
                 "set to false to request via SDL for a non-SRGB backing surface, leave unset "
                 "to have SDL's defaults decide",
                 *this),
  m_fullscreen(false, "fullscreen", "fullscreen mode", *this),
  m_hide_cursor(false, "hide_cursor", "If true, hide the mouse cursor with a SDL call", *this),
  m_use_msaa(false, "enable_msaa", "If true enables MSAA", *this),
  m_msaa(4, "msaa_samples",
         "If greater than 0, specifies the number of samples "
         "to request for MSAA. If not, SDL will choose the "
         "sample count as the highest available value",
         *this),
  m_width(DEFAULT_WIDTH, "width", "window width", *this),
  m_height(DEFAULT_HEIGHT, "height", "window height", *this),
  m_dimensions_must_match(dimensions_must_match_default_value, "dimensions_must_match",
                          "If true, then will abort if the created window dimensions do not "
                          "match precisely the width and height parameters", *this),
  m_bpp(32, "bpp", "bits per pixel", *this),
  m_log_gl_commands("", "log_gl", "if non-empty, GL commands are logged to the named file. "
                    "If value is stderr then logged to stderr, if value is stdout logged to stdout", *this),
  m_emit_gl_string_markers(false, "emit_gl_string_markers",
                           "If true emit GL string marker calls after each GL command, "
                           "this helps enable using GL debugger/trace tools when "
                           "examining the GL API trace",
                           *this),
  m_print_gl_info(false, "print_gl_info", "If true print to stdout GL information", *this),

#ifndef __EMSCRIPTEN__
  m_swap_interval(-1, "swap_interval",
                  "If set, pass the specified value to SDL_GL_SetSwapInterval, "
                  "a value of 0 means no vsync, a value of 1 means vsync and "
                  "a value of -1, if the platform supports, late swap tearing "
                  "as found in extensions GLX_EXT_swap_control_tear and "
                  "WGL_EXT_swap_control_tear. STRONG REMINDER: the value is "
                  "only passed to SDL_GL_SetSwapInterval if the value is set "
                  "at command line", *this),
  m_gl_major(3, "gl_major", "GL major version", *this),
  m_gl_minor(3, "gl_minor", "GL minor version", *this),
  m_gles_major(3, "gles_major", "GLES major version", *this),
  m_gles_minor(0, "gles_minor", "GLES minor version", *this),

  m_gl_forward_compatible_context(false, "foward_context", "if true request forward compatible context", *this),
  m_gl_debug_context(false, "debug_context", "if true request a context with debug", *this),
  m_gl_core_profile(true, "core_context", "if true request a context which is core profile", *this),
  m_try_to_get_latest_gl_version(false, "try_to_get_latest_gl_version",
                                 "If true, first create a GL context the old fashioned way "
                                 "and query its context version and then max that value with "
                                 "the requested version before making the context used by the application",
                                 *this),
  m_use_gles(false, "use_gles", "If true, create and use a GLES context", *this),
  m_save_screenshot("", "save_screenshot",
                    "If non-empty, render a single frame, take a screenshot and save a PNG of the image to this filename",
                    *this),
  m_reference_image("", "reference_screenshot",
                    "If non-empty, render a single frame, take a screenshot and compare that screenshot to the "
                    "PNG referred to by this argument and have the demo return the number of pixels different",
                    *this),
  m_compare_image_diff("", "compare_image_diff",
                       "If reference_screenshot is non-empty, filename to which to "
                       "save a PNG of the image difference",
                       *this),
#else
  m_emscripten_fps(0, "emscripten_fps",
                   "Value to pass as fps to emscripten_set_main_loop_arg() "
                   "A value <= 0  indicates to use the JS interface "
                   "requestionAnimationFrame, a value > 0 indicates an "
                   "FPS to -try- for", *this),
#endif

  m_frames(0, "frames", "Number of frames to render before exiting. Runs indefinitely with default of 0.", *this),
  m_num_warm_up_frames(10, "num_warm_up_frames", "Number of warm-up frames to skip when elapsed and average time. Ignored if greater than frames.", *this),
  m_show_framerate(false, "show_framerate", "if true show the cumulative framerate at end", *this),
  m_show_render_ms(false, "show_render_ms",
                   "If true, at each frame show the number of milliseconds to render the frame",
                   *this),
  m_show_memory_pool_allocs(false, "show_memory_pool_allocs",
                            "If true show whenever MemoryPool allocates memory",
                            *this),
  m_use_high_dpi_flag(true, "use_high_dpi_flag", "If true, add SDL_WINDOW_ALLOW_HIGHDPI to window creation flags", *this),
  m_use_gl_drawable_size(true, "use_gl_drawable_size", "If true, use SDL_GL_GetDrawableSize, otherwise use SDL_GetWindowSize", *this),
  m_window(nullptr),
  m_ctx(nullptr)
{
}

sdl_demo::
~sdl_demo()
{
  if (m_window)
    {
      if (m_ctx)
        {
          SDL_GL_MakeCurrent(m_window, nullptr);
          SDL_GL_DeleteContext(m_ctx);
        }

      SDL_ShowCursor(SDL_ENABLE);
      SDL_SetWindowGrab(m_window, SDL_FALSE);

      SDL_DestroyWindow(m_window);
      SDL_Quit();
    }
}

void
sdl_demo::
set_sdl_gl_context_attribs(void)
{
  #ifdef __EMSCRIPTEN__
    {
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    }
  #else
    {
      if (m_use_gles.value())
        {
          SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, m_gles_major.value());
          SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, m_gles_minor.value());
          SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        }
      else
        {
          if (m_gl_major.value() >= 3)
            {
              int context_flags(0);
              int profile_mask(0);

              SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, m_gl_major.value());
              SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, m_gl_minor.value());

              if (m_gl_forward_compatible_context.value())
                {
                  context_flags |= SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
                }

              if (m_gl_debug_context.value())
                {
                  context_flags |= SDL_GL_CONTEXT_DEBUG_FLAG;
                }

              if (m_gl_core_profile.value())
                {
                  profile_mask = SDL_GL_CONTEXT_PROFILE_CORE;
                }
              else
                {
                  profile_mask = SDL_GL_CONTEXT_PROFILE_COMPATIBILITY;
                }
              SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, context_flags);
              SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile_mask);
            }
        }
    }
  #endif
}

void
sdl_demo::
create_sdl_gl_context(void)
{
  #ifdef __EMSCRIPTEN__
    {
      set_sdl_gl_context_attribs();
      m_ctx = SDL_GL_CreateContext(m_window);
    }
  #else
    {
      if (m_use_gles.value())
        {
          set_sdl_gl_context_attribs();
          m_ctx = SDL_GL_CreateContext(m_window);
        }
      else
        {
          if (!m_try_to_get_latest_gl_version.value())
            {
              set_sdl_gl_context_attribs();
              m_ctx = SDL_GL_CreateContext(m_window);
              return;
            }

          /* Some WGL/GLX implementations will only give the exact GL
           * version requested, but for our purposes we really want the
           * latest version we can get. Very often, by having SDL create
           * a context the old-fashioned way, we can get a context of
           * the greatest version for compatibility profiles. We get SDL to
           * make a GL context the old-fashioned way by NOT setting any
           * of the SDL-GL attribs related to context versions/profiles
           */
          m_ctx = SDL_GL_CreateContext(m_window);
          if (m_ctx == nullptr)
            {
              std::cerr << "Unable to create vanilla GL context: " << SDL_GetError() << "\n";
              return;
            }
          SDL_GL_MakeCurrent(m_window, m_ctx);

          /* Query the version of a context made the old-way and max
           * that value with the requested version; note that we
           * CANNOT use ngl_ system because (1) the get_proc function
           * is not yet assigned and (2) some GL implementation on
           * MS-Windows have that the functions returned by
           * wglGetProcAddress are only good for the context that made
           * them (shudders).
           */
          ASTRAL_PFNGLGETINTEGERVPROC get_integer;
          get_integer = (ASTRAL_PFNGLGETINTEGERVPROC)get_proc("glGetIntegerv");
          if (get_integer)
            {
              astral::ivec2 ver(0, 0);
              astral::ivec2 req(m_gl_major.value(), m_gl_minor.value());

              get_integer(ASTRAL_GL_MAJOR_VERSION, &ver.x());
              get_integer(ASTRAL_GL_MINOR_VERSION, &ver.y());
              req = astral::t_max(ver, req);
              m_gl_major.value() = req.x();
              m_gl_minor.value() = req.y();
            }

          SDL_GL_MakeCurrent(m_window, nullptr);
          SDL_GL_DeleteContext(m_ctx);
          set_sdl_gl_context_attribs();
          m_ctx = SDL_GL_CreateContext(m_window);
        }
    }
  #endif
}

enum astral::return_code
sdl_demo::
init_sdl(void)
{
  #ifdef _WIN32
    {
      SetProcessDPIAware();
    }
  #endif

  Uint32 init_flags;

  #ifdef __EMSCRIPTEN__
    {
      /* doing SDL_INIT_EVERYTHING makes it not work */
      init_flags = SDL_INIT_VIDEO;
    }
  #else
    {
      init_flags = SDL_INIT_EVERYTHING;
    }
  #endif

  if (SDL_Init(init_flags) < 0)
    {
      std::cerr << "\nFailed on SDL_Init: " << SDL_GetError() << "\n";
      return astral::routine_fail;
    }

  int video_flags;
  video_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

  if (m_use_high_dpi_flag.value())
    {
      video_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
    }

  if (m_fullscreen.value())
    {
      video_flags = video_flags | SDL_WINDOW_FULLSCREEN;
    }

  /* To get libANGLE to work with SDL2, apparently some of the
   * SDL_GL_SetAttribute needs to be called before setting the
   * video mode.
   */
  #ifndef __EMSCRIPTEN__
    {
      if (m_use_gles.value())
        {
          SDL_SetHint("SDL_HINT_OPENGL_ES_DRIVER", "1");
          SDL_GL_SetAttribute(SDL_GL_CONTEXT_EGL, 1);
        }
    }
  #endif

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, m_stencil_bits.value());
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, m_depth_bits.value());
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, m_red_bits.value());
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, m_green_bits.value());
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, m_blue_bits.value());
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, m_alpha_bits.value());

  if (m_srgb_capable.set_by_command_line())
    {
      SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, m_srgb_capable.value() ? 1 : 0);
    }

  if (m_use_msaa.value())
    {
      SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
      SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, m_msaa.value());
    }

  m_window = SDL_CreateWindow("",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              m_width.value(),
                              m_height.value(),
                              video_flags);

  if (m_window == nullptr)
    {
      std::cerr << "\nFailed on SDL_SetVideoMode: " << SDL_GetError() << "\n";
      return astral::routine_fail;
    }

  astral::reference_counted_ptr<StreamHolder> str;
  if (!m_log_gl_commands.value().empty())
    {
      str = ASTRALnew StreamHolder(m_log_gl_commands.value());
    }
  astral::gl_binding::enable_gl_string_marker(m_emit_gl_string_markers.value());

  create_sdl_gl_context();
  if (m_ctx == nullptr)
    {
      std::cerr << "Unable to create GL context: " << SDL_GetError() << "\n";
      return astral::routine_fail;
    }
  SDL_GL_MakeCurrent(m_window, m_ctx);
  ready_gl_draw_size_to_window_size();

  if (m_dimensions_must_match.value())
    {
      astral::ivec2 dims;
      bool is_fullscreen;

      is_fullscreen = (SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN) != 0;
      dims = dimensions();
      if (dims.x() != m_width.value() || dims.y() != m_height.value() || is_fullscreen != m_fullscreen.value())
        {
          std::cerr << "\nDimensions did not match and required to match\n";
          return astral::routine_fail;
        }
    }
  
  #ifndef __EMSCRIPTEN__
    {
      /* Emscripten build of NGL does NOT use function pointers,
       * instead it just uses macros to all the functions declared
       * in GLES3/gl3.h
       */
      astral::gl_binding::get_proc_function(get_proc);

      if (m_swap_interval.set_by_command_line())
        {
          if (SDL_GL_SetSwapInterval(m_swap_interval.value()) != 0)
            {
              std::cerr << "Warning unable to set swap interval: "
                        << SDL_GetError() << "\n";
            }
        }
    }
  #endif

  if (m_hide_cursor.value())
    {
      SDL_ShowCursor(SDL_DISABLE);
    }

  if (str)
    {
      m_gl_logger = ASTRALnew OstreamLogger(str);
    }

  if (m_print_gl_info.value())
    {
      std::cout << "\nSwapInterval: " << SDL_GL_GetSwapInterval()
                << "\ndepth bits: " << GetSDLGLValue(SDL_GL_DEPTH_SIZE)
                << "\nstencil bits: " << GetSDLGLValue(SDL_GL_STENCIL_SIZE)
                << "\nred bits: " << GetSDLGLValue(SDL_GL_RED_SIZE)
                << "\ngreen bits: " << GetSDLGLValue(SDL_GL_GREEN_SIZE)
                << "\nblue bits: " << GetSDLGLValue(SDL_GL_BLUE_SIZE)
                << "\nalpha bits: " << GetSDLGLValue(SDL_GL_ALPHA_SIZE)
                << "\ndouble buffered: " << GetSDLGLValue(SDL_GL_DOUBLEBUFFER)
                << "\nSRGB enabled: " << GetSDLGLValue(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE)
                << "\nGL_MAJOR_VERSION: " << astral::gl::context_get<astral_GLint>(ASTRAL_GL_MAJOR_VERSION)
                << "\nGL_MINOR_VERSION: " << astral::gl::context_get<astral_GLint>(ASTRAL_GL_MINOR_VERSION)
                << "\nGL_VERSION string:" << astral_glGetString(ASTRAL_GL_VERSION)
                << "\nGL_VENDOR:" << astral_glGetString(ASTRAL_GL_VENDOR)
                << "\nGL_RENDERER:" << astral_glGetString(ASTRAL_GL_RENDERER)
                << "\nGL_SHADING_LANGUAGE_VERSION:" << astral_glGetString(ASTRAL_GL_SHADING_LANGUAGE_VERSION)
                << "\nGL_MAX_VARYING_COMPONENTS:" << astral::gl::context_get<astral_GLint>(ASTRAL_GL_MAX_VARYING_COMPONENTS)
                << "\nGL_MAX_VERTEX_ATTRIBS:" << astral::gl::context_get<astral_GLint>(ASTRAL_GL_MAX_VERTEX_ATTRIBS)
                << "\nGL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:" << astral::gl::context_get<astral_GLint>(ASTRAL_GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS)
                << "\nGL_MAX_VERTEX_UNIFORM_BLOCKS:" << astral::gl::context_get<astral_GLint>(ASTRAL_GL_MAX_VERTEX_UNIFORM_BLOCKS)
                << "\nGL_MAX_FRAGMENT_UNIFORM_BLOCKS:" << astral::gl::context_get<astral_GLint>(ASTRAL_GL_MAX_FRAGMENT_UNIFORM_BLOCKS)
                << "\nGL_MAX_COMBINED_UNIFORM_BLOCKS:" << astral::gl::context_get<astral_GLint>(ASTRAL_GL_MAX_COMBINED_UNIFORM_BLOCKS)
                << "\nGL_MAX_UNIFORM_BLOCK_SIZE:" << astral::gl::context_get<astral_GLint>(ASTRAL_GL_MAX_UNIFORM_BLOCK_SIZE)
                << "\nGL_MAX_TEXTURE_SIZE: " << astral::gl::context_get<astral_GLint>(ASTRAL_GL_MAX_TEXTURE_SIZE)
        #ifndef __EMSCRIPTEN__
                << "\nGL_MAX_TEXTURE_BUFFER_SIZE: " << astral::gl::context_get<astral_GLint>(ASTRAL_GL_MAX_TEXTURE_BUFFER_SIZE)
        #endif
                << "\nGL_MAX_ARRAY_TEXTURE_LAYERS: " << astral::gl::context_get<astral_GLint>(ASTRAL_GL_MAX_ARRAY_TEXTURE_LAYERS);

      #ifndef __EMSCRIPTEN__
        {
          if (!m_use_gles.value())
            {
              std::cout << "\nGL_MAX_GEOMETRY_UNIFORM_BLOCKS:" << astral::gl::context_get<astral_GLint>(ASTRAL_GL_MAX_GEOMETRY_UNIFORM_BLOCKS)
                        << "\nGL_MAX_CLIP_DISTANCES:" << astral::gl::context_get<astral_GLint>(ASTRAL_GL_MAX_CLIP_DISTANCES);
            }
        }
      #endif

      print_gl_extensions(std::cout);
      std::cout << "\n";
    }

  return astral::routine_success;
}

#ifdef __EMSCRIPTEN__
void
sdl_demo::
render_emscripten_call_back(void *args)
{
  sdl_demo *p;
  SDL_Event ev;

  p = static_cast<sdl_demo*>(args);

  if (p->m_total_frames && p->m_current_frame == p->m_total_frames)
    {
      p->end_demo(0);
    }

  while (p->m_run_demo && SDL_PollEvent(&ev))
    {
      p->handle_event(ev);
    }

  if (p->m_run_demo)
    {
      p->pre_draw_frame();
      p->draw_frame();
      p->post_draw_frame();

      ++p->m_current_frame;
    }
  else
    {
      SDL_GL_MakeCurrent(p->m_window, nullptr);
      SDL_GL_DeleteContext(p->m_ctx);
      SDL_DestroyWindow(p->m_window);
      p->m_window = nullptr;
      p->m_ctx = nullptr;
      SDL_Quit();
      emscripten_cancel_main_loop();
    }
}
#endif

void
sdl_demo::
handle_event(const SDL_Event &ev)
{
  if (ev.type == SDL_QUIT || (ev.type == SDL_KEYUP && ev.key.keysym.sym == SDLK_ESCAPE))
    {
      end_demo(0);
    }
  else if (m_use_gl_drawable_size.value()
           && ev.type == SDL_WINDOWEVENT
           && ev.window.event == SDL_WINDOWEVENT_RESIZED)
    {
      ready_gl_draw_size_to_window_size();
    }
}

void
sdl_demo::
post_demo(void)
{
}

void
sdl_demo::
ready_gl_draw_size_to_window_size(void)
{
  astral::ivec2 w, gl;

  SDL_GL_GetDrawableSize(m_window, &gl.x(), &gl.y());
  SDL_GetWindowSize(m_window, &w.x(), &w.y());

  if (gl.x() != 0 && gl.y() != 0)
    {
      m_gl_draw_size_to_window_size = astral::vec2(gl) / astral::vec2(w);
    }
  else
    {
      m_gl_draw_size_to_window_size = astral::vec2(1.0f);
    }
}

int
sdl_demo::
main(int argc, char **argv)
{
  std::vector<float> warm_up_frame_times;

  if (argc == 2 && is_help_request(argv[1]))
    {
      std::cout << m_about << "\n\nUsage: " << argv[0];
      print_help(std::cout);
      print_detailed_help(std::cout);
      return 0;
    }

  std::cout << "\n\nRunning: \"";
  for(int i = 0; i < argc; ++i)
    {
      std::cout << argv[i];
      if (i != argc - 1)
        {
          std::cout << " ";
        }
    }
  std::cout << '"';

  parse_command_line(argc, argv);
  std::cout << "\n\n" << std::flush;
  m_total_frames = m_frames.value();

  astral::track_memory_pool_memory_allocations(m_show_memory_pool_allocs.value());

  astral::ivec2 wh;
  enum astral::return_code R;
  R = init_sdl();

  if (R == astral::routine_fail)
    {
      return -1;
    }

  m_run_demo = true;
  wh = dimensions();
  init_gl(wh.x(), wh.y());

  #ifndef __EMSCRIPTEN__
    {
      simple_time frame_time;

      while (m_run_demo)
        {
          if (m_total_frames && m_current_frame == m_total_frames)
            {
              end_demo(0);
              break;
            }

          if (m_current_frame == warmup_frames())
            {
              m_render_time.restart();
            }

          pre_draw_frame();
          draw_frame();
          post_draw_frame();

          if (pixel_testing())
            {
              /* read the pixels from the current back buffer,
               * which is the frame just rendered and then
               * immediately exit.
               */
              int demo_return_value(0);
              astral::ivec2 wh(dimensions());
              std::vector<astral::u8vec4> pixels(wh.x() * wh.y());

              astral_glBindFramebuffer(ASTRAL_GL_READ_FRAMEBUFFER, 0);
              astral_glBindBuffer(ASTRAL_GL_PIXEL_PACK_BUFFER, 0u);
              astral_glPixelStorei(ASTRAL_GL_PACK_ROW_LENGTH, 0);
              astral_glPixelStorei(ASTRAL_GL_PACK_SKIP_PIXELS, 0);
              astral_glPixelStorei(ASTRAL_GL_PACK_SKIP_ROWS, 0);
              astral_glPixelStorei(ASTRAL_GL_PACK_ALIGNMENT, 4);

              if (!astral::gl::ContextProperties::is_es())
                {
                  /* these are present in desktop GL only */
                  astral_glPixelStorei(ASTRAL_GL_PACK_IMAGE_HEIGHT, 0);
                  astral_glPixelStorei(ASTRAL_GL_PACK_SKIP_IMAGES, 0);
                  astral_glPixelStorei(ASTRAL_GL_PACK_LSB_FIRST, ASTRAL_GL_FALSE);
                  astral_glPixelStorei(ASTRAL_GL_PACK_SWAP_BYTES, ASTRAL_GL_FALSE);
                }

              astral_glReadPixels(0, 0, wh.x(), wh.y(), ASTRAL_GL_RGBA, ASTRAL_GL_UNSIGNED_BYTE, &pixels[0]);

              if (!m_save_screenshot.value().empty())
                {
                  save_png(true, wh, astral::make_c_array(pixels), m_save_screenshot.value(), true);
                }

              if (!m_reference_image.value().empty())
                {
                  astral::ivec2 ref_image_size;
                  std::vector<astral::u8vec4> ref_image;

                  ref_image_size = load_image_to_array(m_reference_image.value(), ref_image, true);
                  ImageCompare image_compare(ImageCompare::Options(),
                                             wh, astral::make_c_array(pixels),
                                             ref_image_size, astral::make_c_array(ref_image));

                  if (!m_compare_image_diff.value().empty())
                    {
                      save_png(true, image_compare.m_size, astral::make_c_array(image_compare.m_diff_image), m_compare_image_diff.value(), true);
                    }

                  demo_return_value = image_compare.m_difference_count;
                  std::cout << demo_return_value << " pixels different of "
                            << wh.x() * wh.y() << " pixels ("
                            << 100.0f * static_cast<float>(demo_return_value) / static_cast<float>(wh.x() * wh.y())
                            << "%)\n";
                }

              end_demo(demo_return_value);
            }

          if (m_gl_logger)
            {
              OstreamLogger *p;

              p = static_cast<OstreamLogger*>(m_gl_logger.get());
              p->stream() << "\n------ Swap Buffers(frame = " << m_current_frame << ") ---------\n\n";
            }
          SDL_GL_SwapWindow(m_window);
          ++m_current_frame;

          uint64_t us;
          float dt;

          us = frame_time.restart_us();
          dt = static_cast<float>(us) * 0.001f;

          if (m_show_render_ms.value())
            {
              std::cout << "Frame ms = " << dt << "\n";
            }

          if (m_current_frame < warmup_frames())
            {
              warm_up_frame_times.push_back(dt);
            }

          if (m_run_demo && m_handle_events)
            {
              SDL_Event ev;
              while(m_run_demo && m_handle_events && SDL_PollEvent(&ev))
                {
                  if (m_use_gl_drawable_size.value())
                    {
                      multiply_mouse_position_of_sdl_event(m_gl_draw_size_to_window_size, ev);
                    }
                  handle_event(ev);
                }
            }
        }

      if (m_show_framerate.value() && m_current_frame > warmup_frames())
        {
          int32_t ms;
          float msf, numf;

          ms = m_render_time.elapsed();
          numf = static_cast<float>(std::max(1u, benchmarked_frames()));
          msf = static_cast<float>(std::max(1, ms));
          std::cout << "Warm up frame times (in ms): "
                    << astral::make_c_array(warm_up_frame_times)
                    << "\nRendered " << benchmarked_frames() << " in " << ms << " ms.\n"
                    << "ms/frame = " << msf / numf  << "\n"
                    << "FPS = " << 1000.0f * numf / msf << "\n";
        }
    }
  #else
    {
      int loop_forever, fps;

      /* fps : target FPS, if <= 0 then use JS interface requestionAnimationFrame
       * loop_forever : if non-zero then loop forever until emscripten_cancel_main_loop()
       *
       * NOTE: for newer versions of EMSDK,requesting a higher FPS than a demo can draw
       *       at will caused the web browser and demo to take a massive performance hit
       *       (but still report a high framerate). Makng a guess, it appears that EMSDK
       *       just fires some function to draw at teh named timing and it fires it anyways
       *       even if the last "draw" has not yet completed.
       */
      loop_forever = 1;
      fps = m_emscripten_fps.value();
      emscripten_set_main_loop_arg(sdl_demo::render_emscripten_call_back, this, fps, loop_forever);
    }
  #endif

  post_demo();

  return m_return_value;
}

astral::ivec2
sdl_demo::
dimensions(void)
{
  astral::ivec2 return_value;

  ASTRALassert(m_window);
  if (m_use_gl_drawable_size.value())
    {
      SDL_GL_GetDrawableSize(m_window, &return_value.x(), &return_value.y());
      if (return_value.x() == 0 || return_value.y() == 0)
        {
          SDL_GetWindowSize(m_window, &return_value.x(), &return_value.y());
        }
    }
  else
    {
      SDL_GetWindowSize(m_window, &return_value.x(), &return_value.y());
    }

  return return_value;
}

Uint32
sdl_demo::
GetMouseState(int *x, int *y)
{
  Uint32 return_value;

  return_value = SDL_GetMouseState(x, y);

  if (x && m_use_gl_drawable_size.value())
    {
      *x = static_cast<float>(*x) * m_gl_draw_size_to_window_size.x();
    }

  if (y && m_use_gl_drawable_size.value())
    {
      *y = static_cast<float>(*y) * m_gl_draw_size_to_window_size.y();
    }

  return return_value;
}

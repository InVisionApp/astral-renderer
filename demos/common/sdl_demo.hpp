/* -*- mode: c++; c-file-style: "gnu"; -*- */

/*!
 * \file sdl_demo.hpp
 * \brief file sdl_demo.hpp
 *
 * Adapted from: sdl_demo.hpp of WRATH:
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

#ifndef ASTRAL_DEMO_SDL_DEMO_HPP
#define ASTRAL_DEMO_SDL_DEMO_HPP

#include <chrono>

#include <SDL.h>

#include <astral/util/util.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/math.hpp>
#include <astral/util/ostream_utility.hpp>
#include <astral/util/gl/astral_gl.hpp>
#include <astral/util/gl/gl_binding.hpp>

#include "generic_command_line.hpp"
#include "simple_time.hpp"

/* TODO: select a real default font reliably from the system
 *       instead of taking from demo_data/fonts.
 */

#if 1
  #define DEFAULT_FONT "demo_data/fonts/DejaVuSans.ttf"
#else
  #if defined(__EMSCRIPTEN__)
    #define DEFAULT_FONT "demo_data/fonts/DejaVuSans.ttf"
  #elif defined(_WIN32)
    #define DEFAULT_FONT "C:/Windows/Fonts/arial.ttf"
  #elif defined(__APPLE__)
    #define DEFAULT_FONT "/Library/Fonts/Arial.ttf"
  #else
    #define DEFAULT_FONT "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
  #endif
#endif

/*
  Notes:
    ~sdl_demo() destroy the window and GL context,
    thus it is safe to use GL functions in dtor
    of a derived class

    sdl_demo() DOES NOT create the window or GL
    context, so one cannot call GL functions
    from there; rather call GL init functions
    in init_gl().

 */
class sdl_demo:public command_line_register
{
public:
  explicit
  sdl_demo(const std::string &about_text=std::string(),
           bool dimensions_must_match_default_value = false);

  virtual
  ~sdl_demo();

  /*
    call this as your main, at main exit, demo is over.
   */
  int
  main(int argc, char **argv);

  unsigned int
  benchmarked_frames() const
  {
    return m_current_frame < warmup_frames() ? 0U : m_current_frame - warmup_frames();
  }

protected:

  virtual
  void
  init_gl(int w, int h)
  {
    (void)w;
    (void)h;
  }

  virtual
  void
  pre_draw_frame(void)
  {}

  virtual
  void
  draw_frame(void)
  {}

  virtual
  void
  post_draw_frame(void)
  {}

  virtual
  void
  handle_event(const SDL_Event&);

  virtual
  void
  post_demo(void);

  void
  reverse_event_y(bool v);

  void
  end_demo(int return_value)
  {
    m_run_demo = false;
    m_return_value = return_value;
  }

  bool
  demo_over(void)
  {
    return !m_run_demo;
  }

  void
  set_window_title(astral::c_string title)
  {
    SDL_SetWindowTitle(m_window, title);
  }

  astral::ivec2
  dimensions(void);

  /* wrapper over SDL_GetMouseState that potentially multiplies the mouse position */
  Uint32
  GetMouseState(int *x, int *y);

  unsigned int
  total_frames(void) const
  {
    return m_total_frames;
  }

  unsigned int
  current_frame(void) const
  {
    return m_current_frame;
  }

  unsigned int
  warmup_frames(void) const
  {
    return m_num_warm_up_frames.value() < total_frames() ? m_num_warm_up_frames.value() : 0U;
  }

  int64_t
  elapsed_frame(void) const
  {
      return m_render_time.elapsed_us();
  }

  /* Returns true if teh demo is set to performing pixel testing,
   * which means it is saving a screenshot of the first frame
   * rendered and then exiting.
   */
  bool
  pixel_testing(void) const
  {
    #ifndef __EMSCRIPTEN__
      {
        return !m_save_screenshot.value().empty() || !m_reference_image.value().empty();
      }
    #else
      {
        return false;
      }
    #endif
  }

protected:
  void
  total_frames(unsigned int n)
  {
    m_total_frames = n;
  }

  bool m_handle_events;

private:
  unsigned int m_total_frames;
  unsigned int m_current_frame;

  enum astral::return_code
  init_sdl(void);

  void
  set_sdl_gl_context_attribs(void);

  void
  create_sdl_gl_context(void);

  void
  ready_gl_draw_size_to_window_size(void);

#ifdef __EMSCRIPTEN__
  static
  void
  render_emscripten_call_back(void *args);
#endif

  std::string m_about;
  command_separator m_common_label;
  command_line_argument_value<int> m_red_bits;
  command_line_argument_value<int> m_green_bits;
  command_line_argument_value<int> m_blue_bits;
  command_line_argument_value<int> m_alpha_bits;
  command_line_argument_value<int> m_depth_bits;
  command_line_argument_value<int> m_stencil_bits;
  command_line_argument_value<bool> m_srgb_capable;
  command_line_argument_value<bool> m_fullscreen;
  command_line_argument_value<bool> m_hide_cursor;
  command_line_argument_value<bool> m_use_msaa;
  command_line_argument_value<int> m_msaa;
  command_line_argument_value<int> m_width;
  command_line_argument_value<int> m_height;
  command_line_argument_value<bool> m_dimensions_must_match;
  command_line_argument_value<int> m_bpp;
  command_line_argument_value<std::string> m_log_gl_commands;
  command_line_argument_value<bool> m_emit_gl_string_markers;
  command_line_argument_value<bool> m_print_gl_info;

#ifndef __EMSCRIPTEN__
  command_line_argument_value<int> m_swap_interval;
  command_line_argument_value<int> m_gl_major, m_gl_minor;
  command_line_argument_value<int> m_gles_major, m_gles_minor;
  command_line_argument_value<bool> m_gl_forward_compatible_context;
  command_line_argument_value<bool> m_gl_debug_context;
  command_line_argument_value<bool> m_gl_core_profile;
  command_line_argument_value<bool> m_try_to_get_latest_gl_version;
  command_line_argument_value<bool> m_use_gles;
  command_line_argument_value<std::string> m_save_screenshot;
  command_line_argument_value<std::string> m_reference_image;
  command_line_argument_value<std::string> m_compare_image_diff;
#else
  command_line_argument_value<int> m_emscripten_fps;
#endif

  command_line_argument_value<unsigned int> m_frames;
  command_line_argument_value<unsigned int> m_num_warm_up_frames;
  command_line_argument_value<bool> m_show_framerate;
  command_line_argument_value<bool> m_show_render_ms;
  command_line_argument_value<bool> m_show_memory_pool_allocs;
  command_line_argument_value<bool> m_use_high_dpi_flag;
  command_line_argument_value<bool> m_use_gl_drawable_size;

  astral::reference_counted_ptr<astral::gl_binding::CallbackGL> m_gl_logger;

  bool m_run_demo;
  int m_return_value;

  astral::vec2 m_gl_draw_size_to_window_size;
  simple_time m_render_time;

  SDL_Window *m_window;
  SDL_GLContext m_ctx;
};

#endif

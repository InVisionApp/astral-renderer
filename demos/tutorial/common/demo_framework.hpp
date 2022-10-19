/*!
 * \file demo_framework.hpp
 * \brief file demo_framework.hpp
 *
 * Copyright 2019 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 */

#ifndef ASTRAL_DEMO_DEMO_FRAMEWORK_HPP
#define ASTRAL_DEMO_DEMO_FRAMEWORK_HPP

//! [ExampleFramework]

#include <SDL.h>
#include <SDL_video.h>

#include <astral/util/vecN.hpp>
#include <astral/util/astral_memory.hpp>

class Demo;
class DemoRunner;

/* Demo class which is so that at ctor and dtor it is gauranteed
 * that a GL (or GLES) context will be current.
 */
class Demo:astral::noncopyable
{
public:
  Demo(DemoRunner *runner, int argc, char **argv);

  virtual
  ~Demo()
  {}

  /* Using SDL, fetch the dimensions of the window */
  astral::ivec2
  window_dimensions(void);

  /* End the event loop of the demo */
  void
  end_demo(int return_code = 0);

  /* To be implemented by a derived class to render the
   * contents of the current frame.
   */
  virtual
  void
  draw_frame(void)
  {}

  /* To be implemented by a derived class to handle an event.
   * Default implementation does nothing
   */
  virtual
  void
  handle_event(const SDL_Event &ev)
  {
    ASTRALunused(ev);
  }

private:
  friend class DemoRunner;
  DemoRunner *m_demo_runner;
};

class DemoRunner:astral::noncopyable
{
public:
  DemoRunner(void);
  ~DemoRunner();

  template<typename T>
  int
  main(int argc, char **argv)
  {
    enum astral::return_code R;

    R = init_sdl();
    if (R == astral::routine_fail)
      {
        return -1;
      }

    Demo *p;
    p = ASTRALnew T(this, argc, argv);

    ASTRALassert(m_demo == p);
    ASTRALunused(p);

    event_loop();
    return m_return_code;
  }

private:
  friend class Demo;

  enum astral::return_code
  init_sdl(void);

  void
  event_loop(void);

  void
  handle_event(const SDL_Event &ev);

  void
  end_demo(int return_code = 0)
  {
    m_return_code = return_code;
    m_run_demo = false;
  }

  #ifdef __EMSCRIPTEN__
  static
  void
  emscripten_call_back(void *args);
  #endif

  bool m_run_demo;
  int m_return_code;
  SDL_Window *m_window;
  SDL_GLContext m_ctx;
  Demo *m_demo;
};

//! [ExampleFramework]

#endif

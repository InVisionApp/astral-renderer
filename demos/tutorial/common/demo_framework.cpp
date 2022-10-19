/*!
 * \file demo_framework.cpp
 * \brief file demo_framework.cpp
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

//! [ExampleFramework]

#include <iostream>
#include <SDL_syswm.h>
#include <SDL_video.h>
#include <SDL_render.h>
#include <SDL_surface.h>

#include "demo_framework.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

////////////////////////////
//DemoRunner methods
DemoRunner::
DemoRunner(void):
  m_run_demo(true),
  m_return_code(0),
  m_window(nullptr),
  m_ctx(nullptr),
  m_demo(nullptr)
{
}

DemoRunner::
~DemoRunner()
{
  if (m_window)
    {
      if (m_demo)
        {
          ASTRALdelete(m_demo);
        }

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

enum astral::return_code
DemoRunner::
init_sdl(void)
{
  Uint32 init_flags;

  #ifdef _WIN32
    {
      SetProcessDPIAware();
    }
  #endif

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

  /* With SDL:
   *   1) Create a window
   *   2) Create a GL context
   *   3) make the GL context current
   */
  if (SDL_Init(init_flags) < 0)
    {
      std::cerr << "\nFailed on SDL_Init\n";
      return astral::routine_fail;
    }

  int window_width(800), window_height(600);

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

  /* The Astral library works with both OpenGL and OpenGL ES.
   * The only requirements from the library are the version
   * requirements: GL requires atleast version 3.3 and GLES
   * requires atleast version 3.0.
   */
  bool use_gles = false;

  #ifdef __EMSCRIPTEN__
    {
      /* Emscripten offers WebGL1 and WebGL2 which are
       * essentially wrappers around GLES2.0 and GLES3.0,
       * thus for Emscripten builds, use_gles must be true.
       */
      use_gles = true;
    }
  #endif

  if (use_gles)
    {
      /* To get libANGLE to work with SDL2, apparently some of the
       * SDL_GL_SetAttribute needs to be called before setting the
       * video mode.
       */
      SDL_SetHint("SDL_HINT_OPENGL_ES_DRIVER", "1");
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_EGL, 1);

      /* Tell SDL to that we want a GLES3.0 (or higher) context */
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    }
  else
    {
      /* Some GL implementations will only deliver a GL3.0
       * or worse GL2.1 context unless a core profile is
       * requested. In addition, Astral uses attributeless
       * rendering which is not available in GL compatibility
       * profiles for any GL version. So we require a core
       * profile of atleast 3.3. A more sophisticated method
       * is to  try to create a GL context starting at 4.6
       * and decrement down to 3.3 in an attempt to get the
       * latest GL context a platform offers (tests in the
       * wild indicate this is only necessary on Mesa backed
       * by non-Gallium drivers, such as Intel's i965 which
       * is used for IvyBridge and before).
       */
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    }

  m_window = SDL_CreateWindow("",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              window_width,
                              window_height,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);

  if (m_window == nullptr)
    {
      std::cerr << "\nFailed on SDL_SetVideoMode\n";
      return astral::routine_fail;
    }

  m_ctx = SDL_GL_CreateContext(m_window);
  if (m_ctx == nullptr)
    {
      std::cerr << "Unable to create GL context: " << SDL_GetError() << "\n";
      return astral::routine_fail;
    }
  SDL_GL_MakeCurrent(m_window, m_ctx);

  return astral::routine_success;
}

void
DemoRunner::
handle_event(const SDL_Event &ev)
{
  switch (ev.type)
    {
    case SDL_QUIT:
        end_demo();
        break;
    case SDL_KEYUP:
      switch (ev.key.keysym.sym)
        {
        case SDLK_ESCAPE:
          end_demo();
          break;
        }
      break;
    }

  m_demo->handle_event(ev);
}

void
DemoRunner::
event_loop(void)
{
  ASTRALassert(m_demo != nullptr);
  #ifndef __EMSCRIPTEN__
    {
      while (m_run_demo)
        {
          m_demo->draw_frame();
          SDL_GL_SwapWindow(m_window);

          if (m_run_demo)
            {
              SDL_Event ev;
              while(m_run_demo && SDL_PollEvent(&ev))
                {
                  handle_event(ev);
                }
            }
        }
    }
  #else
    {
      int loop_forever, fps;

      /* fps : target FPS, if <= 0 then use JS interface requestionAnimationFrame
       * loop_forever : if non-zero then loop forever until emscripten_cancel_main_loop()
       */
      loop_forever = 1;
      fps = 0;
      emscripten_set_main_loop_arg(DemoRunner::emscripten_call_back, this, fps, loop_forever);
    }
  #endif
}

#ifdef __EMSCRIPTEN__
void
DemoRunner::
emscripten_call_back(void *args)
{
  DemoRunner *p;
  SDL_Event ev;
  static int v = 0;

  p = static_cast<DemoRunner*>(args);
  while (p->m_run_demo && SDL_PollEvent(&ev))
    {
      p->handle_event(ev);
    }

  if (p->m_run_demo)
    {
      p->m_demo->draw_frame();
      SDL_GL_SwapWindow(p->m_window);
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

////////////////////////////////////
// Demo methods
Demo::
Demo(DemoRunner *runner, int argc, char **argv):
  m_demo_runner(runner)
{
  ASTRALunused(argc);
  ASTRALunused(argv);
  m_demo_runner->m_demo = this;
}

astral::ivec2
Demo::
window_dimensions(void)
{
  astral::ivec2 return_value;

  ASTRALassert(m_demo_runner);
  ASTRALassert(m_demo_runner->m_demo == this);
  ASTRALassert(m_demo_runner->m_window);
  SDL_GetWindowSize(m_demo_runner->m_window, &return_value.x(), &return_value.y());
  return return_value;
}

void
Demo::
end_demo(int return_code)
{
  ASTRALassert(m_demo_runner);
  ASTRALassert(m_demo_runner->m_demo == this);
  m_demo_runner->end_demo(return_code);
}

//! [ExampleFramework]

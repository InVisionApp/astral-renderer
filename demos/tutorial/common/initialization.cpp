/*!
 * \file initialization.cpp
 * \brief file initialization.cpp
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

//! [ExampleInitialization]

#include <astral/util/gl/gl_binding.hpp>
#include "initialization.hpp"

static
void*
get_proc(astral::c_string proc_name)
{
  return SDL_GL_GetProcAddress(proc_name);
}

Initialization::
Initialization(DemoRunner *runner, int argc, char **argv):
  Demo(runner, argc, argv)
{
  /* The GL (or GLES) engine need a way to fetch the GL (or GLES)
   * function pointers. It is the application's responsibility to
   * provide to Astral a function to fetch the GL (or GLES)
   * function pointers. We wrap the SDL function SDL_GL_GetProcAddress()
   * in get_proc() static function to make sure that the function
   * signatures match precisely. DANGER: on MS-Windows, the function
   * to fetch GL function pointers requires that the GL context
   * with which they are used is current (this is not the case on
   * Unix). An additional danger with MS-Windows is that the function
   * pointers fetched may or may not be compatible with a different
   * GL context.
   *
   * NOTE: Emscipten build do not need to do the load functions
   *       heartache as startup, and do not need to do this step.
   */
  #ifndef __EMSCRIPTEN__
    {
      astral::gl_binding::get_proc_function(get_proc);
    }
  #endif

  /* Now that Astral can fetch the GL (or GLES) function pointers
   * we can now create our Astral objects.
   *
   * The first object to create is the astral::gl::RenderEngineGL3
   * which embodies how Astral uses the GL (or GLES) to draw, this
   * includes shaders, atlases and other API specific entities.
   * The member class astral::gl::RenderEngineGL3::Config embodies
   * all the knobs and switches to control how the RenderEngine
   * runs. A GL context must curent when creating a RenderEngineGL3
   * object
   */
  astral::gl::RenderEngineGL3::Config config;
  m_engine = astral::gl::RenderEngineGL3::create(config);

  /* Create the Renderer from the RenderEngine. A typical
   * application should really only have one Renderer as
   * well. A Renderer is a heavy object because it contains
   * a (large) buffer it uses for offscreen rendering in
   * addition to a large number of dynamic arrays for work
   * room and intra-frame caching.
   *
   * Astral makes heavy use of reference counting to make memory
   * managment easier. In addition, Astral provides the macros
   * ASTRALnew and ASTRALdelete which under debug builds
   * record object creation and deletion so that if an object is
   * created and not deleted an error message is printed and to
   * also print an error message if an object is deleted with
   * ASTRALdelete that was not created with ASTRALnew. The
   * main consequence of the system is that the creation of any
   * Astral objects must be done with ASTRALnew.
   */
  m_renderer = astral::Renderer::create(*m_engine);
}

astral::gl::RenderTargetGL_DefaultFBO&
Initialization::
render_target(void)
{
  /* create a RenderTarget to where rendering is directed.
   * The below RenderTarget is to render to the application
   * window. There is also a RenderTarget type, \ref
   * RenderTargetGL_Texture for rendering to a GL texture.
   *
   * A RenderTargetGL_DefaultFBO needs to know the size
   * of framebuffer to operate correctly; so we recreate
   * one whenever the window_dimensions() do not match the
   * current one.
   */
  astral::ivec2 sz(window_dimensions());
  if (!m_render_target || m_render_target->size() != sz)
    {
      m_render_target = astral::gl::RenderTargetGL_DefaultFBO::create(sz);
    }

  return *m_render_target;
}

Initialization::
~Initialization()
{
  /* Recall that demo_framework does not destroy the window
   * or GL context untils its dtor. Hence, the GL context is
   * current at our dtor. When the reference counted pointers
   * have their dtors' called, they will decrement the reference
   * count which when it reaches zero will delete the object.
   * Of critical importance is that the last reference to the
   * astral::RenderEngine goes away with a GL context
   * current so that the dtor of astral::gl::RenderEngineGL3
   * will be able to call GL (or GLES) functions to free GL
   * resources. Since the dtor's of the reference counters
   * are called by C++ once this function exists, we have nothing
   * to do actually for Astral cleanup.
   */
}

//! [ExampleInitialization]

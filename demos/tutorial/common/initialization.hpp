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

#include <astral/renderer/renderer.hpp>
#include <astral/renderer/gl3/render_target_gl3.hpp>
#include <astral/renderer/gl3/render_engine_gl3.hpp>

#include "demo_framework.hpp"

class Initialization:public Demo
{
public:
  Initialization(DemoRunner *runner, int argc, char **argv);
  ~Initialization();

public:
  astral::gl::RenderEngineGL3&
  engine(void)
  {
    return *m_engine;
  }

  astral::Renderer&
  renderer(void)
  {
    return *m_renderer;
  }

  astral::gl::RenderTargetGL_DefaultFBO&
  render_target(void);

private:
  /* A RenderEngine embodies the atlases and shaders used for rendering. */
  astral::reference_counted_ptr<astral::gl::RenderEngineGL3> m_engine;

  /* A Renderer is the entry point interface with which to render 2D content with Astral */
  astral::reference_counted_ptr<astral::Renderer> m_renderer;

  /* A RenderTarget represents a surface to which a Renderer will render */
  astral::reference_counted_ptr<astral::gl::RenderTargetGL_DefaultFBO> m_render_target;
};

//! [ExampleInitialization]

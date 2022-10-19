/*!
 * \file render_engine_gl3_demo.hpp
 * \brief render_engine_gl3_demo.hpp
 *
 * Copyright 2020 by InvisionApp.
 *
 * Contact kevinrogovin@invisionapp.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 */
#ifndef ASTRAL_DEMO_RENDER_ENGINE_GL3_DEMO_HPP
#define ASTRAL_DEMO_RENDER_ENGINE_GL3_DEMO_HPP

#include <astral/util/rect.hpp>
#include <astral/util/gl/gl_get.hpp>
#include <astral/text/freetype_face.hpp>
#include <astral/renderer/renderer.hpp>
#include <astral/renderer/gl3/render_target_gl3.hpp>

#include "sdl_demo.hpp"
#include "render_engine_gl3_options.hpp"

class render_engine_gl3_demo:public sdl_demo
{
public:
  render_engine_gl3_demo(void);

  /* Only call this when a GL context is current, makes the RenderEngine on demand */
  astral::gl::RenderEngineGL3&
  engine(void);

  /* Only call this when a GL context is current, makes the Renderer on demand */
  astral::Renderer&
  renderer(void);

  /* Only call this when a GL context is current, makes the RenderTarget on demand;
   * the return value is made to match the size of the window in pixels.
   */
  astral::gl::RenderTargetGL_DefaultFBO&
  render_target(void);

  const astral::gl::RenderEngineGL3::Config&
  requested_config(void)
  {
    return m_engine_options.config();
  }

  astral::reference_counted_ptr<astral::Typeface>
  create_typeface_from_file(int face_index, const char *filename);

  const astral::reference_counted_ptr<astral::FreetypeLib>&
  freetype_lib(void)
  {
    return m_freetype_lib;
  }

  astral::reference_counted_ptr<astral::Typeface>
  create_typeface_from_file(const char *filename)
  {
    return create_typeface_from_file(0, filename);
  }

  astral::Typeface&
  default_typeface(void);

  astral::Typeface&
  tofu_typeface(void)
  {
    if (!m_tofu_typeface)
      {
        m_tofu_typeface = astral::Typeface::create(astral::GlyphGenerator::tofu_generator());
      }
    return *m_tofu_typeface;
  }

  unsigned int
  typeface_threads(void) const
  {
    return m_typeface_threads.value();
  }

  void
  draw_offscreen_alloc(astral::RenderEncoderBase render_encoder,
                       const astral::Renderer::OffscreenBufferAllocInfo &info);

  astral::vec2
  draw_offscreen_alloc_size(const astral::Renderer::OffscreenBufferAllocInfo &info);

  void
  draw_offscreen_alloc_hud(astral::vec2 render_target_size,
                           astral::RenderEncoderBase render_encoder,
                           const astral::Renderer::OffscreenBufferAllocInfo &info);

protected:
  void
  handle_event(const SDL_Event&) override;

private:
  render_engine_gl3_options m_engine_options;
  command_line_argument_value<bool> m_print_actual_config;
  command_line_argument_value<int> m_typeface_threads;
  command_line_argument_value<std::string> m_screenshot_label;

  astral::reference_counted_ptr<astral::gl::RenderEngineGL3> m_engine;
  astral::reference_counted_ptr<astral::Renderer> m_renderer;
  astral::reference_counted_ptr<astral::gl::RenderTargetGL_DefaultFBO> m_render_target;

  astral::reference_counted_ptr<astral::Typeface> m_default_typeface, m_tofu_typeface;
  astral::reference_counted_ptr<astral::FreetypeLib> m_freetype_lib;

  unsigned int m_num_screenshots;
};

#endif

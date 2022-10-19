/*!
 * \file render_engine_gl3_demo.cpp
 * \brief render_engine_gl3_demo.cpp
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

#include <SDL_surface.h>
#include "render_engine_gl3_demo.hpp"
#include "cycle_value.hpp"
#include "ImageSaver.hpp"

render_engine_gl3_demo::
render_engine_gl3_demo(void):
  sdl_demo(),
  m_engine_options(*this),
  m_print_actual_config(false, "print_config", "If true, print the actual engine config", *this),
  m_typeface_threads(4, "typeface_threads",
                     "Number of threads to use for GlyphCache::fetch_glyphs_parallel()",
                     *this),
  m_screenshot_label("screenshot", "screenshot_label", "name prefix for screenshots saved with print-screen key", *this),
  m_freetype_lib(astral::FreetypeLib::create()),
  m_num_screenshots(0)
{
  std::cout << "Common Key Commands:\n"
            << "\tprint-screen: save screenshot\n"
            << "\tleft-shift-escape: change clip window strategy\n"
            << "\tleft-alt-escape: cycle uber-shader strategies for color rendering\n";
}

astral::gl::RenderEngineGL3&
render_engine_gl3_demo::
engine(void)
{
  if (!m_engine)
    {
      m_engine = astral::gl::RenderEngineGL3::create(m_engine_options.config());
      astral::Typeface::default_item_path_params(m_engine_options.item_path_params());
      if (m_print_actual_config.value())
        {
          std::cout << "Actual RenderEngineGL3::Config values:\n"
                    << "\tinitial_num_colorstop_atlas_layers = " << m_engine->config().m_initial_num_colorstop_atlas_layers << "\n"
                    << "\tlog2_dims_colorstop_atlas = " << m_engine->config().m_log2_dims_colorstop_atlas << "\n"
                    << "\tvertex_buffer_size = " << m_engine->config().m_vertex_buffer_size << "\n"
                    << "\tuse_hw_clip_window = " << m_engine->config().m_use_hw_clip_window << "\n"
                    << "\tdata_streaming = " << astral::label(m_engine->config().m_data_streaming) << "\n";
          for (unsigned int i = 0; i < astral::gl::RenderEngineGL3::number_data_types; ++i)
            {
              enum astral::gl::RenderEngineGL3::data_t e;

              e = static_cast<enum astral::gl::RenderEngineGL3::data_t>(i);
              std::cout << "\tMaxPerDrawCall(" << astral::label(e)
                        << ") = " << m_engine->config().m_max_per_draw_call[e]
                        << "\n";
            }
          std::cout << "\tGL_MAX_UNIFORM_BLOCK_SIZE = " << astral::gl::context_get<astral_GLint>(ASTRAL_GL_MAX_UNIFORM_BLOCK_SIZE) << "\n";
        }
    }
  return *m_engine;
}

astral::Renderer&
render_engine_gl3_demo::
renderer(void)
{
  if (!m_renderer)
    {
      m_renderer = astral::Renderer::create(engine());

      astral::RenderEngine::OverridableProperties props(m_renderer->overridable_properties());

      if (m_engine_options.clip_window_strategy().set_by_command_line())
        {
          props.m_clip_window_strategy = m_engine_options.clip_window_strategy().value();
        }

      if (m_engine_options.uber_shader_method().set_by_command_line())
        {
          props.m_uber_shader_method = m_engine_options.uber_shader_method().value();
        }


      m_renderer->overridable_properties(props);
    }

  return *m_renderer;
}

astral::gl::RenderTargetGL_DefaultFBO&
render_engine_gl3_demo::
render_target(void)
{
  astral::ivec2 sz(dimensions());

  if (!m_render_target || sz != m_render_target->size())
    {
      m_render_target = astral::gl::RenderTargetGL_DefaultFBO::create(sz);
    }

  return *m_render_target;
}

astral::reference_counted_ptr<astral::Typeface>
render_engine_gl3_demo::
create_typeface_from_file(int face_index, const char *filename)
{
  astral::reference_counted_ptr<astral::FreetypeFace::GeneratorBase> face_generator;
  astral::reference_counted_ptr<const astral::GlyphGenerator> glyph_generator;

  /* Emscripten note: Using GeneratorMemory fails on Emscripten although the
   * code is quite simple. Printf debugging did indicate that it gets the
   * correct size and likely reads the same values (produces the same hash
   * value as in native), but the loading from Freetype does not produce
   * reliable values. However, using a GeneratorResource works fine as well.
   */
  face_generator = astral::FreetypeFace::GeneratorFile::create(filename, face_index);
  if (face_generator->check_creation(m_freetype_lib) == astral::routine_fail)
    {
      std::cout << "WARNING: unable to create typeface from file " << filename
                << ", index = " << face_index << ": returning a tofu-only typeface\n";
      return &tofu_typeface();
    }
  glyph_generator = face_generator->create_glyph_generator(m_typeface_threads.value(), m_freetype_lib);

  return astral::Typeface::create(glyph_generator, m_engine_options.item_path_params());
}

astral::Typeface&
render_engine_gl3_demo::
default_typeface(void)
{
  if (!m_default_typeface)
    {
      m_default_typeface = create_typeface_from_file(DEFAULT_FONT);
    }
  return *m_default_typeface;
}

void
render_engine_gl3_demo::
handle_event(const SDL_Event &ev)
{
  if (ev.type == SDL_KEYUP
      && ev.key.keysym.sym == SDLK_ESCAPE
      && (ev.key.keysym.mod & (KMOD_LSHIFT | KMOD_LALT | KMOD_LCTRL)) != 0u)
    {
      astral::RenderEngine::OverridableProperties props(renderer().overridable_properties());
      if (ev.key.keysym.mod & KMOD_LSHIFT)
        {
          cycle_value(props.m_clip_window_strategy, false,
                      astral::number_clip_window_strategy);
          std::cout << "clip_window_strategy set to "
                    << astral::label(props.m_clip_window_strategy) << "\n";
        }

      if (ev.key.keysym.mod & KMOD_LALT)
        {
          cycle_value(props.m_uber_shader_method, false,
                      astral::number_uber_shader_method);
          std::cout << "use_uber_shading set to " << std::boolalpha
                    << astral::label(props.m_uber_shader_method) << "\n";
        }

      renderer().overridable_properties(props);
    }
  else if (ev.type == SDL_KEYUP && ev.key.keysym.sym == SDLK_PRINTSCREEN && m_render_target)
    {
        std::ostringstream oss;
        oss << m_screenshot_label.value() << "_" << ++m_num_screenshots << ".png";
        std::string filename = oss.str();

        if (save_png(true, *m_render_target, filename) == astral::routine_success)
          {
            std::cout << "Saved: " << filename << "\n";
          }
        else
          {
            std::cout << "ERROR: Failed Writing " << filename << "\n";
          }
    }
  else
    {
      sdl_demo::handle_event(ev);
    }
}

#define DRAW_OFFSCEEN_ALLOC_PADDING_FACTOR 1.1f

astral::vec2
render_engine_gl3_demo::
draw_offscreen_alloc_size(const astral::Renderer::OffscreenBufferAllocInfo &info)
{
  float width = 0.0;
  for (unsigned int s = 0, ends = info.number_offscreen_sessions(); s < ends; ++s)
    {
      /* add padding between the rects */
      width += float(info.session_size(s).x()) * DRAW_OFFSCEEN_ALLOC_PADDING_FACTOR;
    }

  return astral::vec2(width, astral::Renderer::OffscreenBufferAllocInfo::session_largest_size().y());
}

void
render_engine_gl3_demo::
draw_offscreen_alloc(astral::RenderEncoderBase render_encoder,
                     const astral::Renderer::OffscreenBufferAllocInfo &info)
{
  astral::RenderEncoderBase::AutoRestore auto_restore(render_encoder);
  astral::RenderValue<astral::Brush> brush, free_brush;

  free_brush = render_encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 1.0f, 0.5f)));
  brush = render_encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 0.0f, 1.0f, 0.5f)));

  /* just draw the sessions one after another in the size they are */
  for (unsigned int s = 0, ends = info.number_offscreen_sessions(); s < ends; ++s)
    {
      /* draw the rect representing the entire rect in white */
      astral::vec2 session_size;
      astral::c_array<const astral::RectT<int>> rects;

      session_size = astral::vec2(info.session_size(s));
      rects = info.sessions_rects(s);

      render_encoder.draw_rect(astral::Rect()
                               .min_point(0.0f, 0.0f)
                               .max_point(session_size.x(), session_size.y()),
                               free_brush);

      for (const auto &rect : rects)
        {
          render_encoder.draw_rect(astral::Rect(rect), brush);
        }
      render_encoder.translate(session_size.x() * DRAW_OFFSCEEN_ALLOC_PADDING_FACTOR, 0.0f);
    }
}

void
render_engine_gl3_demo::
draw_offscreen_alloc_hud(astral::vec2 fdims,
                         astral::RenderEncoderBase render_encoder,
                         const astral::Renderer::OffscreenBufferAllocInfo &info)
{
  astral::RenderEncoderBase::AutoRestore auto_restore(render_encoder);
  astral::vec2 sz;
  float scale;

  sz = draw_offscreen_alloc_size(info);
  if (sz.x() > 0.0f)
    {
      /* have it occupy the middle 2/3 */
      scale = 2.0f * fdims.x() / (sz.x() * 3.0f);

      /* but no more than 1 / 10 the screen */
      scale = astral::t_min(scale, fdims.y() / (sz.y() * 10.0f));

      render_encoder.transformation(astral::Transformation());
      render_encoder.translate(fdims.x() / 6.0f, fdims.y() - 1.1f * scale * sz.y());
      render_encoder.scale(scale);
      draw_offscreen_alloc(render_encoder, info);
    }
}

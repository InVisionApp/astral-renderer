/*!
 * \file main.cpp
 * \brief main.cpp
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
#include <string>

#include <astral/util/ostream_utility.hpp>
#include <astral/path.hpp>
#include <astral/animated_path.hpp>
#include <astral/renderer/gl3/render_target_gl3.hpp>
#include <astral/renderer/renderer.hpp>

#include "sdl_demo.hpp"
#include "render_engine_gl3_demo.hpp"
#include "PanZoomTracker.hpp"
#include "simple_time.hpp"
#include "UniformScaleTranslate.hpp"
#include "cycle_value.hpp"

typedef astral::reference_counted<astral::Path>::NonConcurrent PerPath;

class PerAnimatedPath:public astral::reference_counted<PerAnimatedPath>::non_concurrent
{
public:
  PerAnimatedPath(const astral::Path &start_path,
                  const astral::Path &end_path)
  {
    m_path.set(start_path, end_path,
               astral::AnimatedPath::LengthContourSorter());
  }

  astral::AnimatedPath m_path;
};

class TestAnimatedPath:public render_engine_gl3_demo
{
public:
  TestAnimatedPath(void);

protected:
  virtual
  void
  init_gl(int, int) override final;

  virtual
  void
  draw_frame(void) override final;

  virtual
  void
  handle_event(const SDL_Event &ev) override final;

private:
  enum
    {
      animated_path = 0,
      start_path,
      end_path
    };

  void
  draw_frame_with_renderer(const astral::Transformation &base_tr,
                           float t, enum astral::blend_mode_t bl,
                           bool draw_repeat);

  void
  load_path(int glyph_code, astral::Path *dst);

  void
  reset_zoom_transformation(int w, int h);

  const astral::Path&
  get_path(unsigned int glyph);

  const astral::Path&
  get_next_path(unsigned int glyph);

  const PerAnimatedPath&
  get_animated_path(unsigned int glyph);

  void
  update_smooth_values(void);

  command_separator m_demo_options;
  command_line_argument_value<std::string> m_font_file;
  command_line_argument_value<int> m_animation_time;
  command_line_argument_value<int> m_start_glyph;

  astral::reference_counted_ptr<astral::Typeface> m_typeface;

  simple_time m_glyph_start_time;
  int m_current_glyph;
  enum astral::anti_alias_t m_aa_mode;

  std::vector<astral::reference_counted_ptr<PerPath>> m_paths;
  std::vector<astral::reference_counted_ptr<PerAnimatedPath>> m_animated_paths;

  astral::reference_counted_ptr<const astral::VertexData> m_rect;

  PanZoomTrackerSDLEvent m_zoom;
  astral::vec2 m_scale_pre_rotate, m_scale_post_rotate;
  float m_rotate_angle;
  simple_time m_draw_timer;
};

//////////////////////////////////////////
// TestAnimatedPath methods
TestAnimatedPath::
TestAnimatedPath(void):
  m_demo_options("Demo Options", *this),
  m_font_file(DEFAULT_FONT, "font_file", "TTF File from which to extract glyph(s)", *this),
  m_animation_time(3000, "animation_time", "Animation time between glyphs in ms", *this),
  m_start_glyph(0, "start_glyph", "Glyph code of first glyph shown", *this),
  m_aa_mode(astral::with_anti_aliasing),
  m_scale_pre_rotate(1.0f, 1.0f),
  m_scale_post_rotate(1.0f, 1.0f),
  m_rotate_angle(0.0f)
{
  std::cout << "Controls:"
            << "\n\tleft/right arrow key: change what glyph by one glyph code"
            << "\n\tup/down arrow key: change what glyph by ten glyph codes"
            << "\n\ta: toggle drawing anti-alias fuzz"
            << "\n\tp: pause animation"
            << "\n\tq: reset transformation applied to rect"
            << "\n\t6: increase horizontal pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\tctrl-6: decrease horizontal pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\t7: increase vertical pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\tctrl-7: decrease vertical pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + 6: increase horizontal post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + ctrl-6: decrease horizontal post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + 7: increase vertical post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + ctrl-7: decrease vertical post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\t9/0 increase/decrease angle of rotation"
            << "\n\tq: reset view"
            << "\n\tz: increase rendering accuracy\n"
            << "\n\tctrl-z: decrease rendering accuracy\n"
            << "\n\tLeft Mouse Drag: pan"
            << "\n\tHold Left Mouse, then drag up/down: zoom out/in"
            << "\n";
}

void
TestAnimatedPath::
reset_zoom_transformation(int w, int h)
{
  const astral::TypefaceMetricsScalable &metrics(m_typeface->scalable_metrics());

  /* Initialize zoom location so that bottom left corner is (metrics.m_units_per_EM, 0)
   * and scale to that 3 * metrics.m_units_per_EM fits across the window
   */
  UniformScaleTranslate<float> tmp;
  astral::vec2 dims(w, h);
  astral::vec2 scw;
  float sc;

  scw.x() = dims.x() / (5.0f * metrics.m_units_per_EM);
  scw.y() = dims.y() / (metrics.m_units_per_EM);
  sc = astral::t_min(scw.x(), scw.y());

  tmp = UniformScaleTranslate<float>(astral::vec2(0.0f, h))
    * UniformScaleTranslate<float>(sc)
    * UniformScaleTranslate<float>(astral::vec2(2.0f * metrics.m_units_per_EM, 0.0));
  m_zoom.transformation(tmp);
}

void
TestAnimatedPath::
init_gl(int w, int h)
{
  m_typeface = create_typeface_from_file(0, m_font_file.value().c_str());
  ASTRALassert(m_typeface);

  reset_zoom_transformation(w, h);

  m_current_glyph = m_start_glyph.value();
  m_glyph_start_time.restart();
  m_rect = astral::DynamicRectShader::create_rect(engine());

  ASTRALunused(w);
}

void
TestAnimatedPath::
draw_frame(void)
{
  float t;
  int32_t ms;
  astral::Transformation tr;
  astral::vecN<astral::vec2, 3> translates;
  astral::RenderEncoderSurface encoder;
  astral::FillParameters fill_params;
  const astral::TypefaceMetricsScalable &metrics(m_typeface->scalable_metrics());

  update_smooth_values();
  tr = m_zoom.transformation().astral_transformation();

  tr.scale(m_scale_pre_rotate);
  tr.rotate(m_rotate_angle);
  tr.scale(m_scale_post_rotate);

  ms = m_glyph_start_time.elapsed();
  t = static_cast<float>(ms) * 0.001f;
  t = astral::t_min(1.0f, astral::t_max(0.0f, t));

  translates[animated_path] = astral::vec2(0.0f, 0.0f);
  translates[start_path] = astral::vec2(-2.0f * metrics.m_units_per_EM, 0.0f);
  translates[end_path] = astral::vec2(2.0f * metrics.m_units_per_EM, 0.0f);

  encoder = renderer().begin(render_target());

  encoder.transformation(tr);

  fill_params
    .fill_rule(astral::nonzero_fill_rule)
    .aa_mode(m_aa_mode);

  for (int i = 0; i < 3; ++i)
    {
      encoder.save_transformation();
      encoder.translate(translates[i].x(), translates[i].y());

      switch (i)
        {
        case animated_path:
          encoder.fill_paths(astral::CombinedPath(t, get_animated_path(m_current_glyph).m_path),
                             fill_params);
          break;

        case start_path:
          encoder.fill_paths(get_path(m_current_glyph),
                             fill_params);
          break;

        case end_path:
          encoder.fill_paths(get_next_path(m_current_glyph),
                             fill_params);
          break;
        }

      encoder.restore_transformation();
    }

  renderer().end();
}

void
TestAnimatedPath::
update_smooth_values(void)
{
  const Uint8 *keyboard_state = SDL_GetKeyboardState(nullptr);
  float delta, scale_delta, angle_delta;
  astral::vec2 *scale_ptr(nullptr);
  astral::c_string scale_txt(nullptr);

  ASTRALassert(keyboard_state != nullptr);
  delta = static_cast<float>(m_draw_timer.restart_us()) * 0.001f;

  if (keyboard_state[SDL_SCANCODE_LSHIFT])
    {
      delta *= 0.1f;
    }
  if (keyboard_state[SDL_SCANCODE_RSHIFT])
    {
      delta *= 10.0f;
    }

  scale_delta = 0.01f * delta;
  angle_delta = 0.0025f * delta;
  if (keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
    {
      scale_delta = -scale_delta;
    }

  if (keyboard_state[SDL_SCANCODE_RETURN])
    {
      scale_ptr = &m_scale_post_rotate;
      scale_txt = "post-rotate-scale";
    }
  else
    {
      scale_ptr = &m_scale_pre_rotate;
      scale_txt = "pre-rotate-scale";
    }

  if (keyboard_state[SDL_SCANCODE_6])
    {
      scale_ptr->x() += scale_delta;
      std::cout << scale_txt << " set to: " << *scale_ptr << "\n";
    }
  if (keyboard_state[SDL_SCANCODE_7])
    {
      scale_ptr->y() += scale_delta;
      std::cout << scale_txt << " set to: " << *scale_ptr << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_9])
    {
      m_rotate_angle += angle_delta;
      if (angle_delta > 2.0f * ASTRAL_PI)
        {
          m_rotate_angle -= 2.0f * ASTRAL_PI;
        }
      std::cout << "Angle set to: " << m_rotate_angle * (180.0f / ASTRAL_PI) << " degrees\n";
    }
  if (keyboard_state[SDL_SCANCODE_0])
    {
      m_rotate_angle -= angle_delta;
      if (angle_delta < 0.0f)
        {
          m_rotate_angle += 2.0f * ASTRAL_PI;
        }
      std::cout << "Angle set to: " << m_rotate_angle * (180.0f / ASTRAL_PI) << " degrees\n";
    }
}

void
TestAnimatedPath::
handle_event(const SDL_Event &ev)
{
  m_zoom.handle_event(ev);
  if (ev.type == SDL_KEYDOWN)
    {
      switch(ev.key.keysym.sym)
        {
        case SDLK_z:
          {
            float v;

            v = (ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT)) ? 2.0f : 0.5f;
            renderer().default_render_accuracy(v * renderer().default_render_accuracy());
            std::cout << "Render Accuracy set to " << renderer().default_render_accuracy() << "\n";
          }
          break;

        case SDLK_LEFT:
          m_current_glyph = astral::t_max(0, m_current_glyph - 1);
          m_glyph_start_time.restart();
          break;

        case SDLK_RIGHT:
          m_current_glyph = astral::t_min((int)m_typeface->number_glyphs() - 1, m_current_glyph + 1);
          m_glyph_start_time.restart();
          break;

        case SDLK_DOWN:
          m_current_glyph = astral::t_max(0, m_current_glyph - 10);
          m_glyph_start_time.restart();
          break;

        case SDLK_UP:
          m_current_glyph = astral::t_min((int)m_typeface->number_glyphs() - 1, m_current_glyph + 10);
          m_glyph_start_time.restart();
          break;

        case SDLK_p:
          m_glyph_start_time.pause(!m_glyph_start_time.paused());
          break;

        case SDLK_a:
          cycle_value(m_aa_mode,
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      astral::number_anti_alias_modes);
          std::cout << "m_draw_with_aa set to " << astral::label(m_aa_mode) << "\n";
          break;

        case SDLK_q:
          {
            astral::ivec2 dims(dimensions());
            reset_zoom_transformation(dims.x(), dims.y());
            m_scale_pre_rotate = m_scale_post_rotate = astral::vec2(1.0f, 1.0f);
            m_rotate_angle = 0.0f;
          }
          break;
        }
    }
  render_engine_gl3_demo::handle_event(ev);
}

void
TestAnimatedPath::
load_path(int glyph_code, astral::Path *dst)
{
  const astral::Path *path;
  enum astral::fill_rule_t fill_rule;
  astral::Glyph G;

  G = m_typeface->fetch_glyph(astral::GlyphIndex(glyph_code));
  path = G.path(0, &fill_rule);

  *dst = *path;
}

const astral::Path&
TestAnimatedPath::
get_path(unsigned int glyph)
{
  if (glyph >= m_paths.size())
    {
      m_paths.resize(glyph + 1);
    }

  if (!m_paths[glyph])
    {
      m_paths[glyph] = PerPath::create();
      load_path(glyph, m_paths[glyph].get());
    }

  return *m_paths[glyph];
}

const astral::Path&
TestAnimatedPath::
get_next_path(unsigned int glyph)
{
  unsigned int next_glyph(glyph + 1u);

  if (next_glyph >= m_typeface->number_glyphs())
    {
      next_glyph = 0;
    }

  return get_path(next_glyph);
}

const PerAnimatedPath&
TestAnimatedPath::
get_animated_path(unsigned int glyph)
{
  if (glyph >= m_animated_paths.size())
    {
      m_animated_paths.resize(glyph + 1);
    }

  if (!m_animated_paths[glyph])
    {
      unsigned int next_glyph(glyph + 1u);

      if (next_glyph >= m_typeface->number_glyphs())
        {
          next_glyph = 0;
        }

      m_animated_paths[glyph] = ASTRALnew PerAnimatedPath(get_path(glyph), get_path(next_glyph));
    }

  return *m_animated_paths[glyph];
}

int
main(int argc, char **argv)
{
  TestAnimatedPath M;
  return M.main(argc, argv);
}

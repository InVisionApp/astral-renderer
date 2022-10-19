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
#include <cctype>
#include <string>
#include <fstream>
#include <sstream>

#include <astral/util/ostream_utility.hpp>
#include <astral/path.hpp>
#include <astral/animated_path.hpp>
#include <astral/renderer/gl3/render_target_gl3.hpp>
#include <astral/renderer/renderer.hpp>
#include <astral/renderer/item_path.hpp>

#include "sdl_demo.hpp"
#include "read_path.hpp"
#include "render_engine_gl3_demo.hpp"
#include "PanZoomTracker.hpp"
#include "simple_time.hpp"
#include "UniformScaleTranslate.hpp"
#include "cycle_value.hpp"
#include "ImageLoader.hpp"
#include "command_line_list.hpp"
#include "text_helper.hpp"
#include "demo_macros.hpp"

class ItemPathTest:public render_engine_gl3_demo
{
public:
  ItemPathTest(void);

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
      basic_hud,
      detail_level1_hud,
      detail_level2_hud,
      detail_level3_hud,

      number_hud_modes,
    };

  static
  astral::c_string
  return_not_on_false(bool b)
  {
    return (b) ? "" : "not ";
  }

  float
  update_smooth_values(void);

  void
  draw_hud(astral::RenderEncoderSurface encoder, float frame_ms);

  command_separator m_demo_options;
  command_line_argument_value<std::string> m_path_file;
  command_line_argument_value<float> m_rotate_angle;
  enumerated_command_line_argument_value<enum astral::fill_rule_t> m_fill_rule;
  command_line_argument_value<UniformScaleTranslate<float>> m_initial_camera;

  astral::reference_counted_ptr<astral::TextItem> m_text_item;

  simple_time m_draw_timer;
  average_timer m_frame_time_average;
  PanZoomTrackerSDLEvent m_zoom;

  astral::vec2 m_center;
  astral::reference_counted_ptr<astral::ItemPath> m_item_path;

  unsigned int m_hud_mode;
  std::vector<unsigned int> m_prev_stats;
};

//////////////////////////////////////////
//  methods
ItemPathTest::
ItemPathTest(void):
  m_demo_options("Demo Options", *this),
  m_path_file("", "path", "File from which to read the path", *this),
  m_rotate_angle(0.0f, "rotate_angle", "angle in degrees by which to rotate the path around its center", *this),
  m_fill_rule(astral::nonzero_fill_rule,
              enumerated_string_type<enum astral::fill_rule_t>(&astral::label, astral::number_fill_rule),
              "fill_rule",
              "fill rule to apply to path",
              *this),
  m_initial_camera(UniformScaleTranslate<float>(), "initial_camera",
                   "if set, initial position of camera. If not set, intial camera position is "
                   "so that the path is centered in the window" , *this),
  m_frame_time_average(1000u),
  m_hud_mode(basic_hud)
{
  std::cout << "Controls:"
            << "\n\tLeft Mouse Drag: pan"
            << "\n\tHold Left Mouse, then drag up/down: zoom out/in"
            << "\n\tRight Mouse: move path"
            << "\n\tMiddle Mouse: move clip-path"
            << "\n";
}

void
ItemPathTest::
init_gl(int w, int h)
{
  astral::vec2 wh(w, h);

  m_prev_stats.resize(renderer().stats_labels().size(), 0);

  astral::Path path;
  astral::BoundingBox<float> bb;
  astral::ItemPath::Geometry geometry;
  std::ifstream path_file(m_path_file.value().c_str());

  if (path_file)
    {
      read_path(&path, path_file);
    }
  else
    {
      const char *default_path =
        "[ ( 100, -524 ) ( 100, -692 ) ( 596, -692 ) ( 596, -524 ) ]\n"
        "R[ ( 233, -600) arc 33 (233 -670) arc -60 (400, -670) arc 40 (400 -600) arc -45 ]\n"
        "[ (453 -1274) (453 -1274) ]\n";

      read_path(&path, default_path);
    }

  for (unsigned int i = 0, endi = path.number_contours(); i < endi; ++i)
    {
      geometry.add(path.contour(i), astral::RelativeThreshhold(1e-2));
    }


  m_item_path = astral::ItemPath::create(geometry);

  if (m_initial_camera.set_by_command_line())
    {
      m_zoom.transformation(m_initial_camera.value());
    }

  bb = path.bounding_box();
  if (!bb.empty())
    {
      astral::vec2 screen_pt(w / 2, h / 2), path_pt;
      UniformScaleTranslate<float> tr;

      path_pt = (bb.min_point() + bb.max_point()) * 0.5f;
      tr.m_translation = screen_pt - path_pt;

      if (!m_initial_camera.set_by_command_line())
        {
          m_zoom.transformation(tr);
        }

      m_center = bb.as_rect().center_point();
    }
  else
    {
      m_center = astral::vec2(0.0f, 0.0f);
    }

  float pixel_size(32.0f);
  astral::Font font(default_typeface(), pixel_size);
  m_text_item = astral::TextItem::create(font);
}

void
ItemPathTest::
draw_hud(astral::RenderEncoderSurface encoder, float frame_ms)
{
  static const enum astral::Renderer::renderer_stats_t vs[] =
    {
      astral::Renderer::number_offscreen_render_targets,
      astral::Renderer::number_non_degenerate_virtual_buffers,
      astral::Renderer::number_virtual_buffers,
    };

  static const enum astral::RenderBackend::render_backend_stats_t bvs[] =
    {
      astral::RenderBackend::stats_number_draws,
      astral::RenderBackend::stats_vertices,
      astral::RenderBackend::stats_render_targets,
    };

  static const unsigned int gvs[] =
    {
      astral::gl::RenderEngineGL3::number_draws,
      astral::gl::RenderEngineGL3::number_program_binds,
      astral::gl::RenderEngineGL3::number_staging_buffers,
    };

  astral::c_array<const enum astral::Renderer::renderer_stats_t> vs_p;
  astral::c_array<const enum astral::RenderBackend::render_backend_stats_t> bvs_p;
  astral::c_array<const unsigned int> gvs_p;
  std::ostringstream ostr;

  if (m_hud_mode >= detail_level1_hud)
    {
      bvs_p = MAKE_C_ARRAY(bvs, const astral::RenderBackend::render_backend_stats_t);
    }

  if (m_hud_mode >= detail_level2_hud)
    {
      gvs_p = MAKE_C_ARRAY(gvs, const unsigned int);
    }

  if (m_hud_mode >= detail_level3_hud)
    {
      vs_p = MAKE_C_ARRAY(vs, const astral::Renderer::renderer_stats_t);
    }

  astral::ivec2 mouse_pos;
  GetMouseState(&mouse_pos.x(), &mouse_pos.y());

  ostr << "Resolution = " << dimensions() << "\n"
       << "Zoom = " << m_zoom.transformation().m_scale
       << ", Translation = " << m_zoom.transformation().m_translation << "\n"
       << "Mouse at " << mouse_pos << "\n"
       << "\nAverage over " << m_frame_time_average.interval_ms() << " ms: "
       << m_frame_time_average.average_elapsed_ms()
       << m_frame_time_average.parity_string() << "\n";

  /* draw the HUD in fixed location */
  encoder.transformation(astral::Transformation());
  set_and_draw_hud(encoder, frame_ms,
                   astral::make_c_array(m_prev_stats),
                   *m_text_item, ostr.str(),
                   vs_p, bvs_p, gvs_p);
}

void
ItemPathTest::
draw_frame(void)
{
  float frame_ms;
  astral::RenderEncoderSurface render_encoder;
  astral::c_array<const unsigned int> stats;

  m_frame_time_average.increment_counter();
  frame_ms = update_smooth_values();

  render_encoder = renderer().begin(render_target());
    {
      render_encoder.transformation(m_zoom.transformation().astral_transformation());
      render_encoder.translate(m_center);
      render_encoder.rotate(m_rotate_angle.value() * (ASTRAL_PI / 180.0f));
      render_encoder.translate(-m_center);
      render_encoder.draw_item_path(astral::ItemPath::Layer(*m_item_path)
                                    .fill_rule(m_fill_rule.value())
                                    .color(astral::vec4(1.0f, 1.0f, 1.0f, 1.0f)));
      if (!pixel_testing())
        {
          draw_hud(render_encoder, frame_ms);
        }
    }
  stats = renderer().end();

  ASTRALassert(m_prev_stats.size() == stats.size());
  std::copy(stats.begin(), stats.end(), m_prev_stats.begin());
}

float
ItemPathTest::
update_smooth_values(void)
{
  const Uint8 *keyboard_state = SDL_GetKeyboardState(nullptr);
  float return_value, delta, angle_delta;

  ASTRALassert(keyboard_state != nullptr);
  return_value = delta = static_cast<float>(m_draw_timer.restart_us()) * 0.001f;

  if (keyboard_state[SDL_SCANCODE_LSHIFT])
    {
      delta *= 0.1f;
    }

  if (keyboard_state[SDL_SCANCODE_RSHIFT])
    {
      delta *= 10.0f;
    }

  angle_delta = 0.0025f * delta * 180.0f / ASTRAL_PI;

  if ((keyboard_state[SDL_SCANCODE_0] || keyboard_state[SDL_SCANCODE_9]))
    {
      if (keyboard_state[SDL_SCANCODE_0])
        {
          m_rotate_angle.value() -= angle_delta;
          if (m_rotate_angle.value() < 0.0f)
            {
              m_rotate_angle.value() += 360.0f;
            }
        }
      else
        {
          m_rotate_angle.value() += angle_delta;
          if (m_rotate_angle.value() > 360.0f)
            {
              m_rotate_angle.value() -= 360.0f;
            }
        }
      std::cout << "Angle set to: " << m_rotate_angle.value() << " degrees\n";
    }

  return return_value;
}

void
ItemPathTest::
handle_event(const SDL_Event &ev)
{
  m_zoom.handle_event(ev, SDL_BUTTON_LEFT);
  if (ev.type == SDL_KEYDOWN)
    {
      switch(ev.key.keysym.sym)
        {
        case SDLK_q:
          m_zoom.transformation(UniformScaleTranslate<float>());
          m_rotate_angle.value() = 45.0f;
          break;

        case SDLK_r:
          {
            cycle_value(m_fill_rule.value(),
                        ev.key.keysym.mod & (KMOD_CTRL|KMOD_ALT),
                        astral::number_fill_rule);
            std::cout << "Fill rule set to "
                      << astral::label(m_fill_rule.value()) << "\n";
          }
          break;

        case SDLK_SPACE:
          cycle_value(m_hud_mode, false, number_hud_modes);
          break;
        }
    }
  render_engine_gl3_demo::handle_event(ev);
}

int
main(int argc, char **argv)
{
  ItemPathTest M;
  return M.main(argc, argv);
}

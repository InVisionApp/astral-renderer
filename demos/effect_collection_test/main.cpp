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

class EffectCollectionTest:public render_engine_gl3_demo
{
public:
  EffectCollectionTest(void);

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

  enum hud_mode_t
    {
      basic_hud,
      detail_level1_hud,
      detail_level2_hud,
      detail_level3_hud,

      number_hud_modes,
    };

  enum shadow_count_t:uint32_t
    {
      shadow_count = 8
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
  reset_zoom_transformation(PanZoomTrackerSDLEvent *p);

  void
  draw_hud(astral::RenderEncoderSurface encoder, float frame_ms);

  void
  load_path(void);

  astral::StrokeParameters m_stroke_params;

  command_separator m_demo_options;
  command_line_argument_value<std::string> m_path_file;
  command_line_argument_value<float> m_shadow_offset;
  command_line_argument_value<UniformScaleTranslate<float>> m_initial_camera;
  command_line_argument_value<bool> m_use_effect_collection;
  command_line_argument_value<float> m_blur_radius;
  command_line_argument_value<int> m_max_sample_radius;
  command_line_argument_value<float> m_blur_min_scale_factor;

  astral::Path m_path;
  astral::reference_counted_ptr<astral::TextItem> m_text_item;

  astral::vecN<astral::vec2, shadow_count> m_shadow_offset_dir;
  astral::vecN<astral::vec4, shadow_count> m_shadow_colors;
  astral::vecN<float, shadow_count> m_shadow_blur_factors;

  simple_time m_draw_timer;
  average_timer m_frame_time_average;
  PanZoomTrackerSDLEvent m_zoom;

  enum hud_mode_t m_hud_mode;
  std::vector<unsigned int> m_prev_stats;
};

//////////////////////////////////////////
// EffectCollectionTest  methods
EffectCollectionTest::
EffectCollectionTest(void):
  m_demo_options("Demo Options", *this),
  m_path_file("", "path", "File from which to read the path", *this),
  m_shadow_offset(-2.0f, "shadow_offset",
                  "A negative value indicates that the shadow offset value is "
                  "multiplied by the largest of the witdh and height; a positive "
                  "value means teh shadow offset value is absolute",
                  *this),
  m_initial_camera(UniformScaleTranslate<float>(), "initial_camera",
                   "if set, initial position of camera otherwise camera initialize to see center on path with no zoom applied", *this),
  m_use_effect_collection(true, "use_effect_collection", "if true, use EffectCollection instead of a single effect per shadow", *this),
  m_blur_radius(30.0f, "blur_radius", "Blur radius to apply to each shadow", *this),
  m_max_sample_radius(4, "max_blur_sample_radius", "", *this),
  m_blur_min_scale_factor(0.0f, "blur_min_scale_factor",
                          "sets the minimum rendering scale when drawing blurred",
                          *this),
  m_shadow_offset_dir(astral::vec2(1.0f, 0.0f),
                      astral::vec2(-1.0f, 0.0f),
                      astral::vec2(0.0f, 1.0f),
                      astral::vec2(0.0f, -1.0f),
                      astral::vec2(1.0f, 1.0f),
                      astral::vec2(-1.0f, 1.0f),
                      astral::vec2(1.0f, -1.0f),
                      astral::vec2(-1.0f, -1.0f)),
  m_shadow_colors(astral::vec4(1.0f, 0.0f, 0.0f, 1.0f),
                  astral::vec4(0.0f, 1.0f, 0.0f, 1.0f),
                  astral::vec4(0.0f, 0.0f, 1.0f, 1.0f),
                  astral::vec4(1.0f, 1.0f, 0.0f, 1.0f),
                  astral::vec4(0.0f, 1.0f, 1.0f, 1.0f),
                  astral::vec4(1.0f, 0.0f, 1.0f, 1.0f),
                  astral::vec4(0.5f, 0.3f, 1.0f, 1.0f),
                  astral::vec4(1.0f, 0.5f, 0.3f, 1.0f)),
  m_shadow_blur_factors(0.25f, 0.5f, 0.75f, 1.0f, 0.25f, 0.5f, 0.75f, 1.0f),
  m_frame_time_average(1000u),
  m_hud_mode(basic_hud)
{
  std::cout << "Controls:"
            << "\n";
}

void
EffectCollectionTest::
reset_zoom_transformation(PanZoomTrackerSDLEvent *p)
{
  /* Initialize zoom location to be identity */
  p->transformation(UniformScaleTranslate<float>());
}

void
EffectCollectionTest::
load_path(void)
{
  std::ifstream path_file(m_path_file.value().c_str());

  if (path_file)
    {
      read_path(&m_path, path_file);
    }
  else
    {
      const char *default_path =
        "[ (50.0, 35.0) [[(60.0, 50.0) ]] (70.0, 35.0)\n"
        "arc 180 (70.0, -100.0)\n"
        "[[ (60.0, -150.0) (30.0, -50.0) ]]\n"
        "(0.0, -100.0) arc 90 ]\n"
        "{ (200, 200) (400, 200) (400, 400) (200, 400) }\n"
        "[ (-50, 100) (0, 200) (100, 300) (150, 325) (150, 100) ]\n"
        "{ (300 300) }\n";

      read_path(&m_path, default_path);
    }
}

void
EffectCollectionTest::
init_gl(int w, int h)
{
  astral::vec2 wh(w, h);
  astral::BoundingBox<float> path_bb;

  load_path();
  path_bb = m_path.bounding_box();

  if (m_shadow_offset.value() < 0.0f)
    {
      astral::vec2 sz(path_bb.size());
      m_shadow_offset.value() = astral::t_abs(m_shadow_offset.value()) * astral::t_max(sz.x(), sz.y());
    }

  if (m_initial_camera.set_by_command_line())
    {
      m_zoom.transformation(m_initial_camera.value());
    }
  else
    {
      float s(m_shadow_offset.value());
      astral::BoundingBox<float> bb(path_bb);

      for (unsigned int i = 0; i < shadow_count; ++i)
        {
          astral::vec2 v;

          v = m_shadow_offset_dir[i] * s;
          bb.union_point(v + path_bb.min_point());
          bb.union_point(v + path_bb.max_point());
        }

      astral::vec2 bb_size(bb.size());
      astral::vec2 screen_pt(w / 2, h / 2), path_pt;
      UniformScaleTranslate<float> tr;

      path_pt = (bb.min_point() + bb.max_point()) * 0.5f;
      tr.m_scale = astral::t_min(w / bb_size.x(), h / bb_size.y());
      tr.m_translation = screen_pt - path_pt * tr.m_scale;
      m_zoom.transformation(tr);
    }

  float pixel_size(32.0f);
  astral::Font font(default_typeface(), pixel_size);
  m_text_item = astral::TextItem::create(font);

  m_prev_stats.resize(renderer().stats_labels().size(), 0);
}

void
EffectCollectionTest::
draw_hud(astral::RenderEncoderSurface encoder, float frame_ms)
{
  static const enum astral::Renderer::renderer_stats_t vs[] =
    {
      /*
      astral::Renderer::number_sparse_fill_subrects_clipping,
      astral::Renderer::number_sparse_fill_subrect_skip_clipping,
      astral::Renderer::number_sparse_fill_contour_skip_clipping,
      astral::Renderer::number_sparse_fill_curves_clipped,
      astral::Renderer::number_sparse_fill_curves_mapped,
      astral::Renderer::number_sparse_fill_contours_clipped,
      astral::Renderer::number_sparse_fill_contours_mapped,
      astral::Renderer::number_sparse_fill_late_culled_contours,

      astral::Renderer::number_emulate_framebuffer_fetches,
      astral::Renderer::number_vertices_streamed,
      astral::Renderer::number_static_u32vec4_streamed,
      */
      astral::Renderer::number_skipped_color_buffer_pixels,
      astral::Renderer::number_virtual_buffer_pixels,
      astral::Renderer::number_non_degenerate_virtual_buffers,
      astral::Renderer::number_non_degenerate_color_virtual_buffers,
      astral::Renderer::number_non_degenerate_mask_virtual_buffers,
      astral::Renderer::number_non_degenerate_shadowmap_virtual_buffers,
      astral::Renderer::number_offscreen_render_targets,
      astral::Renderer::number_virtual_buffers,
    };

  static const enum astral::RenderBackend::render_backend_stats_t bvs[] =
    {
      astral::RenderBackend::stats_number_draws,
      astral::RenderBackend::stats_vertices,
    };

  static const unsigned int gvs[] =
    {
      astral::gl::RenderEngineGL3::number_draws,
      astral::gl::RenderEngineGL3::number_program_binds,
      astral::gl::RenderEngineGL3::number_blend_state_changes,
      astral::gl::RenderEngineGL3::number_staging_buffers,
    };

  astral::c_array<const enum astral::Renderer::renderer_stats_t> vs_p;
  astral::c_array<const enum astral::RenderBackend::render_backend_stats_t> bvs_p;
  astral::c_array<const unsigned int> gvs_p;
  std::ostringstream hud_text;

  hud_text << "Resolution = " << dimensions() << "\n"
           << "Zoom = " << m_zoom.transformation().m_scale
           << ", Translation = " << m_zoom.transformation().m_translation
           << "\n[e] EffectCollection used: " << std::boolalpha << m_use_effect_collection.value()
           << "\n[up/down] Blur radius: " << m_blur_radius.value()
           << "\n[left/right] Max blur sample radius: " << m_max_sample_radius.value()
           << "\n[v/ctrl-v] Blur min-scale factor: " << m_blur_min_scale_factor.value()
           << "\n[space] Hud Level: " << m_hud_mode << " [space]\n"
           << "Average over " << m_frame_time_average.interval_ms() << " ms: "
           << m_frame_time_average.average_elapsed_ms()
           << m_frame_time_average.parity_string() << "\n"
           << "Number commands copied: " << m_prev_stats[astral::Renderer::number_commands_copied]
           << "\n";

  if (m_hud_mode >= detail_level2_hud)
    {
      bvs_p = MAKE_C_ARRAY(bvs, const astral::RenderBackend::render_backend_stats_t);
    }

  if (m_hud_mode >= detail_level3_hud)
    {
      gvs_p = MAKE_C_ARRAY(gvs, const unsigned int);
    }

  if (m_hud_mode >= detail_level1_hud)
    {
      vs_p = MAKE_C_ARRAY(vs, const astral::Renderer::renderer_stats_t);
    }

  /* draw the HUD in fixed location */
  encoder.transformation(astral::Transformation());
  set_and_draw_hud(encoder, frame_ms,
                   astral::make_c_array(m_prev_stats),
                   *m_text_item, hud_text.str(),
                   vs_p, bvs_p, gvs_p);
}

void
EffectCollectionTest::
draw_frame(void)
{
  float frame_ms;
  astral::RenderEncoderSurface render_encoder;
  astral::c_array<const unsigned int> stats;

  m_frame_time_average.increment_counter();
  frame_ms = update_smooth_values();

  render_encoder = renderer().begin(render_target(), astral::colorspace_srgb, astral::u8vec4(0, 0, 0, 0));
    {
      render_encoder.transformation(m_zoom.transformation().astral_transformation());

      astral::vecN<astral::GaussianBlurParameters, shadow_count + 1> blur_params;
      astral::vecN<astral::EffectParameters, shadow_count + 1> effect_params;

      for (unsigned int i = 0; i < shadow_count; ++i)
        {
          blur_params[i]
            .color_modulation(m_shadow_colors[i].x(), m_shadow_colors[i].y(), m_shadow_colors[i].z(), m_shadow_colors[i].w())
            .include_halo(true)
            .min_render_scale(m_blur_min_scale_factor.value())
            .max_sample_radius(m_max_sample_radius.value())
            .radius(m_shadow_blur_factors[i] * m_blur_radius.value());

          effect_params[i] = astral::EffectParameters(blur_params[i].effect_parameters(),
                                                      m_shadow_offset.value() * m_shadow_offset_dir[i]);
        }

      blur_params[shadow_count]
        .color_modulation(1.0f, 1.0f, 1.0f, 1.0f)
        .include_halo(false)
        .min_render_scale(m_blur_min_scale_factor.value())
        .max_sample_radius(m_max_sample_radius.value())
        .radius(0.0f);

      effect_params[shadow_count] = astral::EffectParameters(blur_params[shadow_count].effect_parameters());

      const astral::Effect *blur;
      astral::BoundingBox<float> bb(m_path.bounding_box());

      blur = render_encoder.default_effects().m_gaussian_blur.get();
      bb.enlarge(astral::vec2(m_stroke_params.m_width * 0.5f));

      if (m_use_effect_collection.value())
        {
          astral::RenderEncoderLayer layer;

          layer = render_encoder.begin_layer(astral::EffectCollection(*blur, effect_params), bb);
          layer.encoder().stroke_paths(m_path, m_stroke_params);
          render_encoder.end_layer(layer);
        }
      else
        {
          for (unsigned int i = 0; i < effect_params.size(); ++i)
            {
              astral::RenderEncoderLayer layer;

              layer = render_encoder.begin_layer(*blur, effect_params[i], bb);
              layer.encoder().stroke_paths(m_path, m_stroke_params);
              render_encoder.end_layer(layer);
            }
        }

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
EffectCollectionTest::
update_smooth_values(void)
{
  const Uint8 *keyboard_state = SDL_GetKeyboardState(nullptr);
  float return_value, delta, blur_delta, scale_delta;
  bool blur_radius_changed(false);

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

  blur_delta = 0.01f * delta;
  scale_delta = 0.01f * delta;

  if (keyboard_state[SDL_SCANCODE_UP])
    {
      blur_radius_changed = true;
      m_blur_radius.value() += blur_delta;
    }

  if (keyboard_state[SDL_SCANCODE_DOWN])
    {
      blur_radius_changed = true;
      m_blur_radius.value() -= blur_delta;
    }

  if (keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
    {
      scale_delta = -scale_delta;
    }

  if (keyboard_state[SDL_SCANCODE_V])
    {
      m_blur_min_scale_factor.value() += scale_delta * 0.1f;
      m_blur_min_scale_factor.value() = astral::t_clamp(m_blur_min_scale_factor.value(), 0.0f, 1.0f);
      std::cout << "Blur min-scale factor set to: " << m_blur_min_scale_factor.value() << "\n";
    }

  if (blur_radius_changed)
    {
      m_blur_radius.value() = astral::t_max(m_blur_radius.value(), 0.0f);

      std::cout << "Blur radius set to " << m_blur_radius.value() << "\n";
    }

  return return_value;
}

void
EffectCollectionTest::
handle_event(const SDL_Event &ev)
{
  m_zoom.handle_event(ev, SDL_BUTTON_LEFT);
  if (ev.type == SDL_KEYDOWN)
    {
      switch(ev.key.keysym.sym)
        {
        case SDLK_SPACE:
          cycle_value(m_hud_mode, false, number_hud_modes);
          break;

        case SDLK_p:
          {
            const UniformScaleTranslate<float> &tr(m_zoom.transformation());
            std::cout << "initial_camera " << tr.m_translation.x() << ":"
                      << tr.m_translation.y() << ":" << tr.m_scale
                      << " shadow_offset " << m_shadow_offset.value()
                      << " use_effect_collection " << std::boolalpha
                      << m_use_effect_collection.value()
                      << " blur_radius " << m_blur_radius.value()
                      << " max_blur_sample_radius " << m_max_sample_radius.value()
                      << " blur_min_scale_factor " << m_blur_min_scale_factor.value()
                      << "\n";
              }
          break;

        case SDLK_e:
          m_use_effect_collection.value() = !m_use_effect_collection.value();
          std::cout << "Use EffectCollection set to "
                    << std::boolalpha << m_use_effect_collection.value()
                    << "\n";
          break;

        case SDLK_RIGHT:
          ++m_max_sample_radius.value();
          std::cout << "Max blur pixel radius set to: " << m_max_sample_radius.value() << "\n";
          break;

        case SDLK_LEFT:
          m_max_sample_radius.value() = astral::t_max(m_max_sample_radius.value() - 1, 1);
          std::cout << "Max blur pixel radius set to: " << m_max_sample_radius.value() << "\n";
          break;
        }
    }
  render_engine_gl3_demo::handle_event(ev);
}

int
main(int argc, char **argv)
{
  EffectCollectionTest M;
  return M.main(argc, argv);
}

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
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <random>
#include <dirent.h>
#include <stdlib.h>

#include <astral/util/ostream_utility.hpp>
#include <astral/renderer/renderer.hpp>
#include <astral/renderer/gl3/render_target_gl3.hpp>

#include "command_line_list.hpp"
#include "sdl_demo.hpp"
#include "render_engine_gl3_demo.hpp"
#include "ImageLoader.hpp"
#include "PanZoomTracker.hpp"
#include "read_colorstops.hpp"
#include "cycle_value.hpp"
#include "text_helper.hpp"

class SnapshotTest:public render_engine_gl3_demo
{
public:
  SnapshotTest(void);

protected:
  virtual
  void
  init_gl(int, int) override;

  virtual
  void
  draw_frame(void) override;

  virtual
  void
  handle_event(const SDL_Event &ev) override;

private:
  class AnimatedValue
  {
  public:
    void
    advance_time(float ms)
    {
      float v;

      v = m_value + ms * m_derivative;
      if (v < m_min)
        {
          m_derivative = astral::t_abs(m_derivative);
          m_value = m_min + ms * m_derivative;
        }
      else if (v > m_max)
        {
          m_derivative = -astral::t_abs(m_derivative);
          v = m_value + ms * m_derivative;
        }
      m_value = v;
    }

    float m_value;
    float m_derivative;
    float m_min;
    float m_max;
  };

  template<size_t N>
  static
  void
  advance_time(float ms, astral::vecN<AnimatedValue, N> &v)
  {
    for (unsigned int i = 0; i < N; ++i)
      {
        v[i].advance_time(ms);
      }
  }

  static
  void
  advance_time(float ms, AnimatedValue &v)
  {
    v.advance_time(ms);
  }

  class GradientRectBase
  {
  public:
    astral::vec2 m_size, m_position;
    astral::reference_counted_ptr<const astral::ColorStopSequence> m_colorstop_sequence;
    enum astral::tile_mode_t m_tile_mode;
    AnimatedValue m_angle;
    astral::vecN<AnimatedValue, 2> m_p0, m_p1;

    virtual
    ~GradientRectBase()
    {}

    void
    init(float px, float py, astral::vec2 sz,
         astral::reference_counted_ptr<const astral::ColorStopSequence> C,
         std::uniform_real_distribution<float> &dist, std::mt19937 &e)
    {
      m_size = sz;
      m_position.x() = px;
      m_position.y() = py;
      m_colorstop_sequence = C;
      m_tile_mode = astral::tile_mode_mirror_repeat;

      m_p0.x().m_min = -sz.x();
      m_p0.y().m_min = -sz.y();
      m_p0.x().m_max = 2.0f * sz.x();
      m_p0.y().m_max = 2.0f * sz.y();

      m_p1 = m_p0;

      m_p0.x().m_value = sz.x() * dist(e);
      m_p0.y().m_value = sz.y() * dist(e);
      m_p1.x().m_value = sz.x() * dist(e);
      m_p1.y().m_value = sz.y() * dist(e);

      m_p0.x().m_derivative = 0.0001f * sz.x() * dist(e);
      m_p0.y().m_derivative = 0.0001f * sz.y() * dist(e);

      m_p1.x().m_derivative = 0.0001f * sz.x() * dist(e);
      m_p1.y().m_derivative = 0.0001f * sz.y() * dist(e);

      m_angle.m_min = -ASTRAL_PI * 12.0f;
      m_angle.m_max = ASTRAL_PI * 12.0f;
      m_angle.m_value = 0.0f;
      m_angle.m_derivative = 0.002f * dist(e) * ASTRAL_PI;

      derived_init(dist, e);
    }

    void
    draw_rect(astral::RenderEncoderBase r, SnapshotTest*)
    {
      r.save_transformation();
      r.translate(m_position.x() + m_size.x() * 0.5f, m_position.y() + m_size.y() * 0.5f);
      r.rotate(m_angle.m_value);
      r.translate(-m_size.x() * 0.5f, -m_size.y() * 0.5f);
      r.draw_rect(astral::Rect()
                  .min_point(0.0f, 0.0f)
                  .max_point(m_size.x(), m_size.y()),
                  r.create_value(astral::Brush().gradient(r.create_value(generate_gradient()))));
      r.restore_transformation();
    }

    void
    time_passes(float ms)
    {
      advance_time(ms, m_angle);
      advance_time(ms, m_p0);
      advance_time(ms, m_p1);
      derived_time_passes(ms);
    }

    virtual
    void
    derived_time_passes(float ms) = 0;

    virtual
    astral::Gradient
    generate_gradient(void) const = 0;

    virtual
    void
    derived_init(std::uniform_real_distribution<float> &dist, std::mt19937 &e) = 0;
  };

  class LinearGradientRect:public GradientRectBase
  {
  public:
    virtual
    void
    derived_time_passes(float) override
    {}

    virtual
    void
    derived_init(std::uniform_real_distribution<float>&, std::mt19937&) override
    {}

    virtual
    astral::Gradient
    generate_gradient(void) const override
    {
      return astral::Gradient(m_colorstop_sequence,
                              astral::vec2(m_p0.x().m_value, m_p0.y().m_value),
                              astral::vec2(m_p1.x().m_value, m_p1.y().m_value),
                              m_tile_mode);
    }
  };

  class RadialGradientRect:public GradientRectBase
  {
  public:
    AnimatedValue m_r0, m_r1;

    virtual
    void
    derived_time_passes(float ms) override
    {
      advance_time(ms, m_r0);
      advance_time(ms, m_r1);
    }

    virtual
    void
    derived_init(std::uniform_real_distribution<float> &dist, std::mt19937 &e) override
    {
      m_r0.m_min = m_r1.m_min = 0.0f;
      m_r0.m_max = m_r1.m_max = m_size.x() + m_size.y();

      m_r0.m_value = m_r0.m_max * dist(e);
      m_r1.m_value = m_r1.m_max * dist(e);
      m_r0.m_derivative = 0.0001f * m_r0.m_max * dist(e);
      m_r1.m_derivative = 0.0001f * m_r1.m_max * dist(e);
    }

    virtual
    astral::Gradient
    generate_gradient(void) const override
    {
     return astral::Gradient(m_colorstop_sequence,
                              astral::vec2(m_p0.x().m_value, m_p0.y().m_value), m_r0.m_value,
                              astral::vec2(m_p1.x().m_value, m_p1.y().m_value), m_r1.m_value,
                              m_tile_mode);
    }
  };

  class SweepGradientRect:public GradientRectBase
  {
  public:
    AnimatedValue m_sweep_multiplier;

    virtual
    void
    derived_time_passes(float ms) override
    {
      m_sweep_multiplier.advance_time(ms);
    }

    virtual
    void
    derived_init(std::uniform_real_distribution<float> &dist, std::mt19937 &e) override
    {
      m_sweep_multiplier.m_min = 1.0f;
      m_sweep_multiplier.m_max = 51.0f;
      m_sweep_multiplier.m_value = 1.0f + 50.0f * dist(e);
      m_sweep_multiplier.m_derivative = 0.001f + 0.005f * dist(e);
    }

    virtual
    astral::Gradient
    generate_gradient(void) const override
    {
      astral::vec2 v;
      float theta;

      v.x() = m_p1.x().m_value - m_p0.x().m_value;
      v.y() = m_p1.y().m_value - m_p0.y().m_value;
      theta = astral::t_atan2(v.y(), v.x());
      return astral::Gradient(m_colorstop_sequence,
                              astral::vec2(m_p0.x().m_value, m_p0.y().m_value),
                              theta, m_sweep_multiplier.m_value,
                              m_tile_mode);
    }
  };

  class BgRect
  {
  public:
    BgRect(void):
      m_wiggle_time(0.0f)
    {}

    astral::vec2 m_size, m_position;
    AnimatedValue m_angle;
    float m_wiggle_time;

    void
    time_passes(float ms)
    {
      advance_time(ms, m_angle);
      m_wiggle_time += ms;
    }

    void
    draw_rect(astral::RenderEncoderBase r, SnapshotTest *p)
    {
      astral::vecN<astral::gvec4, 1> custom_data;
      astral::ItemData custom_data_value;
      float phase, omega, amplitude, tf;
      uint32_t t, ticks;

      ticks = static_cast<uint32_t>(m_wiggle_time);
      t = ticks % 4000u;
      tf = 2.0f * ASTRAL_PI * static_cast<float>(t) / 4000.0f ;
      phase = 1.0f - astral::t_abs(static_cast<float>(t) - 2000.0f) / 2000.0f;
      omega = 8.0f * ASTRAL_PI / m_size.y();
      amplitude = 30.0f * astral::t_cos(tf) * p->m_zoom.transformation().m_scale;

      custom_data[0].x().f = phase;
      custom_data[0].y().f = omega;
      custom_data[0].z().f = amplitude;
      custom_data_value = r.create_item_data(custom_data, astral::no_item_data_value_mapping);

      r.save_transformation();
      r.translate(m_position + 0.5f * m_size);
      if (p->m_rotate_overlay_rects.value())
        {
          r.rotate(m_angle.m_value);
        }
      r.translate(-0.5f * m_size);
      r.draw_rect(astral::Rect()
                  .min_point(0.0f, 0.0f)
                  .max_point(m_size.x(), m_size.y()),
                  astral::Material(*p->m_material_shader, custom_data_value));
      r.restore_transformation();
    }
  };

  template<typename T>
  class RectCollection
  {
  public:
    void
    time_passes(float ms)
    {
      for (T &r : m_rects)
        {
          r.time_passes(ms);
        }
    }

    void
    add_rect(const T &rect)
    {
      m_rects.push_back(rect);
    }

    void
    draw_rects(astral::RenderEncoderBase encoder, SnapshotTest *p)
    {
      for (T &r : m_rects)
        {
          r.draw_rect(encoder, p);
        }
    }

  private:
    std::vector<T> m_rects;
  };

  float
  update_smooth_values(void);

  void
  create_material_shader(void);

  command_separator m_demo_options;
  command_line_list_colorstops<astral::colorspace_srgb> m_loaded_colorstop_sequences;
  command_line_argument_value<unsigned int> m_num_grid_y;
  command_line_argument_value<unsigned int> m_num_grid_linear_gradient_rects;
  command_line_argument_value<unsigned int> m_num_grid_radial_gradient_rects;
  command_line_argument_value<unsigned int> m_num_grid_sweep_gradient_rects;
  command_line_argument_value<unsigned int> m_num_bg_rects_x;
  command_line_argument_value<unsigned int> m_num_bg_rects_y;
  command_line_argument_value<float> m_bg_rect_spacing;
  command_line_argument_value<bool> m_rotate_overlay_rects;
  command_line_argument_value<bool> m_allow_overlays_to_interact;
  command_line_argument_value<float> m_fixed_draw_time;
  command_line_argument_value<UniformScaleTranslate<float>> m_initial_camera;

  astral::reference_counted_ptr<const astral::MaterialShader> m_material_shader;
  astral::reference_counted_ptr<astral::TextItem> m_text_item;
  std::vector<unsigned int> m_prev_stats;
  simple_time m_draw_timer;
  RectCollection<LinearGradientRect> m_linear_rects;
  RectCollection<RadialGradientRect> m_radial_rects;
  RectCollection<SweepGradientRect> m_sweep_rects;
  RectCollection<BgRect> m_bg_rects;
  std::vector<astral::reference_counted_ptr<astral::ColorStopSequence>> m_colorstop_sequences;
  unsigned int m_frame_id;

  PanZoomTrackerSDLEvent m_zoom;
};

//////////////////////////////////
// SnapshotTest methods
SnapshotTest::
SnapshotTest(void):
  m_demo_options("Demo Options", *this),
  m_loaded_colorstop_sequences("add_colorstop", "Add a colorstop to use", *this),
  m_num_grid_y(30, "num_grid_y", "", *this),
  m_num_grid_linear_gradient_rects(10, "num_grid_linear_gradient_rects", "Number of rects with linear gradient", *this),
  m_num_grid_radial_gradient_rects(10, "num_grid_radial_gradient_rects", "Number of rects with radial gradient", *this),
  m_num_grid_sweep_gradient_rects(10, "num_grid_sweep_gradient_rects", "Number of rects with sweep gradient", *this),
  m_num_bg_rects_x(10, "num_bg_rects_x", "", *this),
  m_num_bg_rects_y(10, "num_bg_rects_y", "", *this),
  m_bg_rect_spacing(0.25f, "bg_rect_spacing", "", *this),
  m_rotate_overlay_rects(false, "rotate_overlay_rects", "", *this),
  m_allow_overlays_to_interact(false, "allow_overlays_to_interact", "", *this),
  m_fixed_draw_time(0.0f, "fixed_draw_time", "If set, freeze the animation at the specified time given in ms", *this),
  m_initial_camera(UniformScaleTranslate<float>(), "initial_camera", "Position of initial camera set as translate-x:translate-y:zoom", *this),
  m_frame_id(0)
{
}


void
SnapshotTest::
create_material_shader(void)
{
  astral::c_string vertex_shader =
    "void astral_material_pre_vert_shader(in uint sub_shader, in uint shader_data,\n"
    "                                     in uint brush_idx, in vec2 item_p,"
    "                                     in AstralTransformation pixel_transformation_material)\n"
    "{\n"
    "}\n"
    "void astral_material_vert_shader(in uint sub_shader, in uint shader_data,\n"
    "                                 in uint brush_idx, in vec2 item_p,"
    "                                 in AstralTransformation pixel_transformation_material)\n"
    "{\n"
    "   vec3 raw_data;\n"
    "   raw_data = astral_read_item_dataf(shader_data + 0u).xyz;\n"
    "\n"
    "   wobbly_phase = raw_data.x;\n"
    "   wobbly_omega = raw_data.y;\n"
    "   wobbly_amplitude = raw_data.z;\n"
    "   wobbly_y = item_p.y;\n"
    "}\n";

  astral::c_string fragment_shader =
    "void astral_material_pre_frag_shader(in uint sub_shader, in uint color_space)\n"
    "{\n"
    "}\n"
    "void astral_material_frag_shader(in uint sub_shader, in uint color_space, inout vec4 color, inout float coverage)\n"
    "{\n"
    "   float dx;\n"
    "\n"
    "   dx = wobbly_amplitude * cos(wobbly_omega * wobbly_y + wobbly_phase);\n"
    "   color = astral_framebuffer_fetch(vec2(dx, 0.0)).grba;\n"
    "}\n";

  m_material_shader
    = astral::gl::MaterialShaderGL3::create(engine(),
                                            astral::gl::ShaderSource().add_source(vertex_shader, astral::gl::ShaderSource::from_string),
                                            astral::gl::ShaderSource().add_source(fragment_shader, astral::gl::ShaderSource::from_string),
                                            astral::gl::ShaderSymbolList()
                                            .add_varying("wobbly_phase", astral::gl::ShaderVaryings::interpolator_flat)
                                            .add_varying("wobbly_omega", astral::gl::ShaderVaryings::interpolator_flat)
                                            .add_varying("wobbly_amplitude", astral::gl::ShaderVaryings::interpolator_flat)
                                            .add_varying("wobbly_y", astral::gl::ShaderVaryings::interpolator_smooth),
                                            astral::MaterialShader::Properties().uses_framebuffer_pixels(true).emits_transparent_fragments(true),
                                            astral::gl::MaterialShaderGL3::DependencyList());
}

void
SnapshotTest::
init_gl(int w, int h)
{
  float pixel_size(32.0f);
  astral::Font font(default_typeface(), pixel_size);
  m_text_item = astral::TextItem::create(font);

  ASTRALunused(w);
  ASTRALunused(h);

  for (const auto &e : m_loaded_colorstop_sequences.elements())
    {
      astral::reference_counted_ptr<astral::ColorStopSequence> C;

      C = engine().colorstop_sequence_atlas().create(astral::make_c_array(e.m_loaded_value));
      m_colorstop_sequences.push_back(C);
    }

  if (m_colorstop_sequences.empty())
    {
      std::vector<astral::ColorStop<astral::FixedPointColor_sRGB>> colorstops;

      colorstops.push_back(astral::ColorStop<astral::FixedPointColor_sRGB>()
                           .color(astral::FixedPointColor_sRGB(255u, 255u, 255u, 255u))
                           .t(0.0f));

      colorstops.push_back(astral::ColorStop<astral::FixedPointColor_sRGB>()
                           .color(astral::FixedPointColor_sRGB(0u, 255u, 0u, 255u))
                           .t(0.25f));

      colorstops.push_back(astral::ColorStop<astral::FixedPointColor_sRGB>()
                           .color(astral::FixedPointColor_sRGB(0u, 0u, 255u, 255u))
                           .t(0.5f));

      colorstops.push_back(astral::ColorStop<astral::FixedPointColor_sRGB>()
                           .color(astral::FixedPointColor_sRGB(255u, 0u, 0u, 255u))
                           .t(0.5f));

      colorstops.push_back(astral::ColorStop<astral::FixedPointColor_sRGB>()
                           .color(astral::FixedPointColor_sRGB(0u, 255u, 0u, 255u))
                           .t(0.75f));

      colorstops.push_back(astral::ColorStop<astral::FixedPointColor_sRGB>()
                           .color(astral::FixedPointColor_sRGB(255u, 255u, 0u, 255u))
                           .t(1.0f));

      m_colorstop_sequences.push_back(engine().colorstop_sequence_atlas().create(astral::make_c_array(colorstops)));
    }

  std::mt19937 random_engine;
  std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
  int grid_x(m_num_grid_linear_gradient_rects.value() + m_num_grid_radial_gradient_rects.value() + m_num_grid_sweep_gradient_rects.value());
  astral::vec2 rect_size(1.0f / astral::vec2(grid_x, m_num_grid_y.value()));
  unsigned int x, y, c;
  float px, py;

  for (c = 0, y = 0, py = 0.0f; y < m_num_grid_y.value(); ++y, py += rect_size.y())
    {
      for (x = 0, px = 0.0f; x < m_num_grid_linear_gradient_rects.value(); ++x, px += rect_size.x(), ++c)
        {
          LinearGradientRect R;

          R.init(px, py, rect_size, m_colorstop_sequences[c % m_colorstop_sequences.size()],
                 distribution, random_engine);
          m_linear_rects.add_rect(R);
        }

      for (x = 0; x < m_num_grid_radial_gradient_rects.value(); ++x, px += rect_size.x(), ++c)
        {
          RadialGradientRect R;

          R.init(px, py, rect_size, m_colorstop_sequences[c % m_colorstop_sequences.size()],
                 distribution, random_engine);
          m_radial_rects.add_rect(R);
        }

      for (x = 0; x < m_num_grid_sweep_gradient_rects.value(); ++x, px += rect_size.x(), ++c)
        {
          SweepGradientRect R;

          R.init(px, py, rect_size, m_colorstop_sequences[c % m_colorstop_sequences.size()],
                 distribution, random_engine);
          m_sweep_rects.add_rect(R);
        }
    }

  float bg_x, bg_y;
  astral::vec2 rect_size_with_padding, padding;

  bg_x = static_cast<float>(m_num_bg_rects_x.value()) + m_bg_rect_spacing.value();
  bg_y = static_cast<float>(m_num_bg_rects_y.value()) + m_bg_rect_spacing.value();

  rect_size_with_padding = 1.0f / astral::vec2(bg_x, bg_y);
  rect_size = rect_size_with_padding / (1.0f + m_bg_rect_spacing.value());

  for (y = 0, py = 0.0f; y < m_num_bg_rects_y.value(); ++y, py += rect_size_with_padding.y())
    {
      for (x = 0, px = 0.0f; x < m_num_bg_rects_x.value(); ++x, px += rect_size_with_padding.x())
        {
          BgRect R;

          R.m_size = rect_size;
          R.m_position = astral::vec2(px, py);

          R.m_angle.m_min = -ASTRAL_PI * 12.0f;
          R.m_angle.m_max = ASTRAL_PI * 12.0f;
          R.m_angle.m_value = 0.0f;
          R.m_angle.m_derivative = 0.002f * distribution(random_engine) * ASTRAL_PI;

          m_bg_rects.add_rect(R);
        }
    }

  m_prev_stats.resize(renderer().stats_labels().size(), 0);
  create_material_shader();

  m_zoom.transformation(m_initial_camera.value());
}

float
SnapshotTest::
update_smooth_values(void)
{
  const Uint8 *keyboard_state = SDL_GetKeyboardState(nullptr);
  float frame_ms;

  frame_ms = static_cast<float>(m_draw_timer.restart_us()) * 0.001f;
  ASTRALunused(keyboard_state);

  return frame_ms;
}

void
SnapshotTest::
handle_event(const SDL_Event &ev)
{
  m_zoom.handle_event(ev);
  if (ev.type == SDL_KEYDOWN)
    {
      switch(ev.key.keysym.sym)
        {
        case SDLK_r:
          m_rotate_overlay_rects.value() = !m_rotate_overlay_rects.value();
          std::cout << std::boolalpha << "Overlay rect rotate set to "
                    << m_rotate_overlay_rects.value() << "\n";
          break;

        case SDLK_o:
          m_allow_overlays_to_interact.value() = !m_allow_overlays_to_interact.value();
          std::cout << std::boolalpha << "Overlay rects interact set to "
                    << m_allow_overlays_to_interact.value() << "\n";
          break;
        }
    }

  render_engine_gl3_demo::handle_event(ev);
}

void
SnapshotTest::
draw_frame(void)
{
  astral::ivec2 dims(dimensions());
  astral::vec2 fdims(dims);
  float frame_ms;
  astral::c_array<const unsigned int> stats;
  astral::Transformation tr;
  astral::RenderEncoderSurface render_encoder;

  frame_ms = update_smooth_values();

  if (m_fixed_draw_time.set_by_command_line())
    {
      if (m_frame_id == 0)
        {
          m_linear_rects.time_passes(m_fixed_draw_time.value());
          m_radial_rects.time_passes(m_fixed_draw_time.value());
          m_sweep_rects.time_passes(m_fixed_draw_time.value());
          m_bg_rects.time_passes(m_fixed_draw_time.value());
        }
    }
  else
    {
      m_linear_rects.time_passes(frame_ms);
      m_radial_rects.time_passes(frame_ms);
      m_sweep_rects.time_passes(frame_ms);
      m_bg_rects.time_passes(frame_ms);
    }

  render_encoder = renderer().begin(render_target());

  /* scale so that size is [0, 1] */
  render_encoder.scale(fdims);
  m_linear_rects.draw_rects(render_encoder, this);
  m_radial_rects.draw_rects(render_encoder, this);
  m_sweep_rects.draw_rects(render_encoder, this);

  tr = m_zoom.transformation().astral_transformation();
  render_encoder.transformation(tr);
  render_encoder.scale(fdims);

  if (!m_allow_overlays_to_interact.value())
    {
      render_encoder.begin_pause_snapshot();
    }

  m_bg_rects.draw_rects(render_encoder, this);

  if (!m_allow_overlays_to_interact.value())
    {
      render_encoder.end_pause_snapshot();
    }

  render_encoder.transformation(astral::Transformation());

  static const enum astral::Renderer::renderer_stats_t vs[] =
    {
      astral::Renderer::number_commands_copied,
      astral::Renderer::number_non_degenerate_virtual_buffers,
      astral::Renderer::number_offscreen_render_targets,
      astral::Renderer::number_virtual_buffer_pixels,
    };
  astral::c_array<const enum astral::Renderer::renderer_stats_t> vss(vs, 4);

  if (!pixel_testing())
    {
      set_and_draw_hud(render_encoder, frame_ms, astral::make_c_array(m_prev_stats),
                       *m_text_item, "", vss);
    }

  stats = renderer().end();

  ASTRALassert(m_prev_stats.size() == stats.size());
  std::copy(stats.begin(), stats.end(), m_prev_stats.begin());

  ++m_frame_id;
}

int
main(int argc, char **argv)
{
  SnapshotTest M;
  return M.main(argc, argv);
}

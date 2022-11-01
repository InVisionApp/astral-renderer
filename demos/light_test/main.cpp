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
#include "animated_path_reflect.hpp"

class LightTest:public render_engine_gl3_demo
{
public:
  LightTest(void);

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

  enum animation_generation_t:uint32_t
    {
      by_length_animation_generation,
      by_area_animation_generation,
      by_order_animation_generation,
      manual_animation_generation,
    };

  enum path_mode_t:uint32_t
    {
      t0_path,
      animated_path_at_0,
      t1_path,
      animated_path_at_1,
      animated_path,

      number_path_modes,
    };

  enum shadow_aa_t:uint32_t
    {
      shadow_aa_none,
      shadow_aa4,
      shadow_aa8,
      shadow_aa16,

      number_shadow_aa
    };

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
  label(enum path_mode_t v)
  {
    static const astral::c_string labels[number_path_modes] =
      {
        [t0_path] = "t0_path",
        [animated_path_at_0] = "animated_path_at_0",
        [t1_path] = "t1_path",
        [animated_path_at_1] = "animated_path_at_1",
        [animated_path] = "animated_path",
      };

    ASTRALassert(v < number_path_modes);
    return labels[v];
  }

  static
  astral::c_string
  label(enum shadow_aa_t v)
  {
    static const astral::c_string labels[number_shadow_aa] =
      {
        [shadow_aa_none] = "shadow_aa_none",
        [shadow_aa4] = "shadow_aa4",
        [shadow_aa8] = "shadow_aa8",
        [shadow_aa16] = "shadow_aa16",
      };

    ASTRALassert(v < number_shadow_aa);
    return labels[v];
  }

  float
  update_smooth_values(void);

  void
  reset_zoom_transformation(void);

  float
  compute_animation_interpolate(int32_t &ms);

  static
  bool
  load_path(const std::string &filename, astral::Path *dst, PathCommand *dst_cmd);

  void
  add_animatations(bool closed_contours,
                   const std::vector<PerContourCommand> &st,
                   const std::vector<PerContourCommand> &ed);

  void
  draw_hud(int32_t raw_ms, float anim_t,
           astral::RenderEncoderSurface encoder, float frame_ms);

  void
  draw_ui_rect(astral::RenderEncoderSurface render_encoder,
               astral::RenderValue<astral::Brush> outer,
               astral::RenderValue<astral::Brush> inner,
               astral::vec2 p);

  void
  draw_lighting(astral::CombinedPath drawn_path,
                const astral::BoundingBox<float> &bb,
                astral::RenderEncoderBase render_encoder);

  astral::FillParameters m_fill_params;
  astral::FillMaskProperties m_mask_fill_params;
  astral::MaskUsage m_mask_fill_usage_params;

  command_separator m_demo_options;
  command_line_argument_value<std::string> m_path_file;
  command_line_argument_value<std::string> m_end_path_file;
  command_line_argument_value<bool> m_show_render_stats;
  enumerated_command_line_argument_value<enum animation_generation_t> m_animation_style;
  command_line_argument_value<unsigned int> m_animation_time;
  command_line_argument_value<float> m_reflect_direction_x;
  command_line_argument_value<float> m_reflect_direction_y;
  command_line_argument_value<float> m_reflect_pt_x;
  command_line_argument_value<float> m_reflect_pt_y;
  command_line_argument_value<UniformScaleTranslate<float>> m_initial_camera;
  command_line_argument_value<astral::vec2> m_light_p, m_light_to_p;
  command_line_argument_value<bool> m_light_directional;
  command_line_argument_value<float> m_light_angle;
  command_line_argument_value<float> m_light_z;
  command_line_argument_value<float> m_light_shadow_fall_off;
  command_line_argument_value<float> m_light_shadow_fall_off_length;
  command_line_argument_value<float> m_soft_shadow_penumbra;
  enumerated_command_line_argument_value<enum shadow_aa_t> m_anti_alias_shadow;
  command_line_argument_value<bool> m_include_implicit_closing_edges;
  command_line_argument_value<astral::vec2> m_scale_pre_rotate, m_scale_post_rotate;
  command_line_argument_value<float> m_rotate_angle;
  command_line_argument_value<bool> m_stroke;
  command_line_argument_value<float> m_scale_factor;
  enumerated_command_line_argument_value<enum path_mode_t> m_mode;
  command_line_argument_value<simple_time> m_path_time;
  enumerated_command_line_argument_value<enum astral::anti_alias_t&> m_fill_params_aa_mode;
  enumerated_command_line_argument_value<enum astral::fill_rule_t&> m_fill_params_fill_rule;
  enumerated_command_line_argument_value<enum astral::fill_method_t&> m_mask_fill_params_sprase_mask;
  enumerated_command_line_argument_value<enum astral::mask_type_t&> m_mask_fill_usage_params_mask_type;
  enumerated_command_line_argument_value<enum astral::filter_t&> m_mask_filter;

  astral::reference_counted_ptr<astral::TextItem> m_text_item;

  astral::Path m_path;
  astral::Path m_end_path;
  astral::AnimatedPath m_animated_path;
  astral::Rect m_ui_inner_rect, m_ui_outer_rect;

  astral::StrokeMaskProperties m_stroke_mask_params;
  astral::MaskUsage m_stroke_mask_usage_params;
  astral::StrokeParameters m_stroke_params;

  simple_time m_draw_timer;
  average_timer m_frame_time_average;
  PanZoomTrackerSDLEvent m_zoom;

  unsigned int m_hud_mode;
  std::vector<unsigned int> m_prev_stats;
};

//////////////////////////////////////////
//  methods
LightTest::
LightTest(void):
  m_mask_fill_usage_params(astral::mask_type_distance_field, astral::filter_linear),
  m_demo_options("Demo Options", *this),
  m_path_file("", "path", "File from which to read the path", *this),
  m_end_path_file("", "end_path",
                  "File from which to read the end path, if no file "
                  "is given then the ending path is path reflected ",
                  *this),
  m_show_render_stats(false, "show_render_stats",
                      "If true, at each frame print stdout stats of rendering",
                      *this),
  m_animation_style(by_length_animation_generation,
                    enumerated_string_type<enum animation_generation_t>()
                    .add_entry("by_length", by_length_animation_generation, "")
                    .add_entry("by_area", by_area_animation_generation, "")
                    .add_entry("by_order", by_order_animation_generation, "")
                    .add_entry("manual", manual_animation_generation, ""),
                    "animation_style",
                    "Specifies how the animated path is constructed",
                    *this),
  m_animation_time(3000, "animation_time", "Time to animate path in ms", *this),
  m_reflect_direction_x(0.0f, "reflect_direction_x", "x-coordinate of reflection axis direciton if end path is reflection", *this),
  m_reflect_direction_y(1.0f, "reflect_direction_y", "y-coordinate of reflection axis direciton if end path is reflection", *this),
  m_reflect_pt_x(0.0f, "reflect_pt_x", "x-coordinate of reflection axis position if end path is reflection", *this),
  m_reflect_pt_y(0.0f, "reflect_pt_y", "y-coordinate of reflection axis position if end path is reflection", *this),
  m_initial_camera(UniformScaleTranslate<float>(), "initial_camera",
                   "if set, initial position of camera otherwise camera initialize to see center on path with no zoom applied", *this),
  m_light_p(astral::vec2(0.0f, 0.0f), "light_p", "initial position of light", *this),
  m_light_to_p(astral::vec2(0.0f, 0.0f), "light_to_p",
               "position to which the light points; if not set will be initialized "
               "to the bottom right of the window", *this),
  m_light_directional(false, "light_directional", "if true the light is directional, see also light_to_p", *this),
  m_light_angle(30.0f, "light_angle", "if the light is directional, the angle of the cone of the light", *this),
  m_light_z(-1.0f, "light_z", "if positive, gives the light a z-position and is used to also give attenuation from the light direction", *this),
  m_light_shadow_fall_off(-1.0f, "light_shadow_fall_off", "if positive, gives the distance at which the shadow attenuation starts to fall off", *this),
  m_light_shadow_fall_off_length(0.0f, "light_shadow_fall_off_length", "if positive, gives the length of the shadow fall off", *this),
  m_soft_shadow_penumbra(-1.0f, "soft_shadow_penumbra", "if positive, make the shadow soft with the specified penumbra size", *this),
  m_anti_alias_shadow(shadow_aa8,
                      enumerated_string_type<enum shadow_aa_t>(&label, number_shadow_aa),
                      "anti_alias_shadow",
                      "specifies anti-aliasing applied to the light shadow",
                      *this),
  m_include_implicit_closing_edges(true, "include_implicit_closing_edges",
                                   "include in the light casting the geoemtry of the implicit closing "
                                   "edges of the open contours", *this),
  m_scale_pre_rotate(astral::vec2(1.0f, 1.0f), "scale_pre_rotate",
                     "scaling transformation to apply to path before rotation, formatted as ScaleX:SaleY", *this),
  m_scale_post_rotate(astral::vec2(1.0f, 1.0f), "scale_post_rotate",
                      "scaling transformation to apply to path after rotation, formatted as ScaleX:SaleY", *this),
  m_rotate_angle(0.0f, "rotate_angle", "rotation of path in degrees to apply to path", *this),
  m_stroke(true, "stroke_path", "if true draw a hairline stroke of the path", *this),
  m_scale_factor(1.0f, "scale_factor", "when generating the mask for the fill, amount by which "
                 "to scale the mask generation; a value less than 1.0 means to generate the mask "
                 "as a lower resolution than that which it is used", *this),
  m_mode(t0_path,
         enumerated_string_type<enum path_mode_t>(&label, number_path_modes),
         "path_mode",
         "Initial path mode to select to draw animated path or static path.",
         *this),
  m_path_time(simple_time(),
              "path_time",
              "If set, pauses the timer for path aimation and specifies the intial time value in ms",
              *this),
  m_fill_params_aa_mode(m_fill_params.m_aa_mode,
                        enumerated_string_type<enum astral::anti_alias_t>(&astral::label, astral::number_anti_alias_modes),
                        "fill_aa",
                        "anti-aliasing mode to apply to path fill",
                        *this),
  m_fill_params_fill_rule(m_fill_params.m_fill_rule,
                          enumerated_string_type<enum astral::fill_rule_t>(&astral::label, astral::number_fill_rule)
                          .add_entry("no_fill", astral::number_fill_rule, ""),
                          "fill_rule",
                          "initial fill rule to apply to path",
                          *this),
  m_mask_fill_params_sprase_mask(m_mask_fill_params.m_sparse_mask,
                                 enumerated_string_type<enum astral::fill_method_t>(&astral::label, astral::number_fill_method_t),
                                 "fill_method",
                                 "method for generating fill mask",
                                 *this),
  m_mask_fill_usage_params_mask_type(m_mask_fill_usage_params.m_mask_type,
                                     enumerated_string_type<enum astral::mask_type_t>(&astral::label, astral::number_mask_type),
                                     "fill_mask_type",
                                     "specifies the kind of mask to use when filling the path",
                                     *this),
  m_mask_filter(m_mask_fill_usage_params.m_filter,
                enumerated_string_type<enum astral::filter_t>(&astral::label, astral::number_filter_modes),
                "fill_mask_filter",
                "filter to apply to mask of the fill",
                *this),
  m_stroke_mask_usage_params(astral::mask_type_coverage),
  m_frame_time_average(1000u),
  m_hud_mode(basic_hud)
{
  std::cout << "Controls:"
            << "\n\tspace: cycle through HUD modes"
            << "\n\tshift-space: toggle showing frame rate to console"
            << "\n\tq: reset transformation applied to the path"
            << "\n\tp: pause animation and print current magnification"
            << "\n\tr: cycle through different fill rules"
            << "\n\tctrl-r: cycle through different filling implementations"
            << "\n\tctrl-a: toggle fill anti-aliasing"
            << "\n\td: cycle through drawing mode: draw start path, draw end path, draw animated path, etc"
            << "\n\tg: cycle through how to sample from coverage mask"
            << "\n\tk: cycle through filter mode when sampling from the mask"
            << "\n\tl: toggle light directional"
            << "\n\tc: toggle hacing implicit closing edges of open contours casting a shadow"
            << "\n\tup/down: increase/decrease light angle"
            << "\n\ta: toggle shadow anti-aliasing"
            << "\n\tleft-alt + up/down: increase/decrease light-z"
            << "\n\tleft/right: decrease/inscrease shadow fall off"
            << "\n\tctrl + left/right: decrease/inscrease shadow fall off length"
            << "\n\t:down/up: decrease/increase shadow fall off length"
            << "\n\talt + left/right: decrease/increase shadow penumbra"
            << "\n\tleft-crtl + up/down: increase/decrease render fill scale factor"
            << "\n\talt + 1,2, ... 9: set render fill scale factor to 10%, 20%, ..., 90% repsectively"
            << "\n\talt + 0: set render fill scale factor to 100%"
            << "\n\t6: increase horizontal pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\tctrl-6: decrease horizontal pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\t7: increase vertical pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\tctrl-7: decrease vertical pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + 6: increase horizontal post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + ctrl-6: decrease horizontal post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + 7: increase vertical post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + ctrl-7: decrease vertical post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\t9/0 increase/decrease angle of rotation"
            << "\n\t[/] : decrease/incrase stroking width"
            << "\n\tw: change dash pattern adjust mode"
            << "\n\tctrl-w: toggle adjust dash pattern's draw lengths"
            << "\n\tshift-w: toggle adjust dash pattern's skip lengths"
            << "\n\tLeft Mouse Drag: pan"
            << "\n\tHold Left Mouse, then drag up/down: zoom out/in"
            << "\n";
}

void
LightTest::
reset_zoom_transformation(void)
{
  /* Initialize zoom location to be identity */
  m_zoom.transformation(UniformScaleTranslate<float>());
}

bool
LightTest::
load_path(const std::string &filename, astral::Path *dst, PathCommand *dst_cmd)
{
  std::ifstream path_file(filename.c_str());
  if (path_file)
    {
      read_path(dst, path_file, dst_cmd);
      return true;
    }

  return false;
}

void
LightTest::
add_animatations(bool closed_contours,
                 const std::vector<PerContourCommand> &st,
                 const std::vector<PerContourCommand> &ed)
{
  unsigned int sz;
  std::vector<astral::AnimatedPath::CompoundCurve> s, e;
  std::vector<astral::ContourCurve> sc, ec;
  astral::c_string tp;

  tp = (closed_contours) ? "closed" : "open";
  sz = astral::t_min(st.size(), ed.size());
  for (unsigned int i = 0; i < sz; ++i)
    {
      if (st[i].m_curve_commands.size() == ed[i].m_curve_commands.size())
        {
          st[i].generate_compound_curve_contour(&s);
          ed[i].generate_compound_curve_contour(&e);
          m_animated_path.add_animated_contour(closed_contours,
                                               astral::make_c_array(s),
                                               astral::make_c_array(e));
        }
      else
        {
          astral::vec2 st_pt, ed_pt;

          st_pt = st[i].m_src->start();
          ed_pt = ed[i].m_src->start();

          std::cout << "Warning: the " << i << "'th "<< tp << " contours from "
                    << "the start path (#" << st[i].m_ID << " and from "
                    << "the end path (#" << ed[i].m_ID << " are not the "
                    << "same number of compound curves\n";
          m_animated_path.add_animated_contour(closed_contours,
                                               st[i].m_src->curves(), st_pt,
                                               ed[i].m_src->curves(), ed_pt);
        }
    }

  if (sz < st.size())
    {
      std::cout << "Warning: Starting path has more " << tp << " contours than end\n";
    }

  for (unsigned int i = sz; i < st.size(); ++i)
    {
      astral::vec2 p;

      p = st[i].m_src->bounding_box().as_rect().center_point();
      if (st[i].m_src->curves().empty())
        {
          m_animated_path.add_animated_contour_raw(p, p);
        }
      else
        {
          m_animated_path.add_animated_contour_raw(closed_contours, st[i].m_src->curves(), p);
        }
    }

  if (sz < ed.size())
    {
      std::cout << "Warning: Ending path has more " << tp << " contours than start\n";
    }

  for (unsigned int i = sz; i < ed.size(); ++i)
    {
      astral::vec2 p;

      p = ed[i].m_src->bounding_box().as_rect().center_point();
      if (ed[i].m_src->curves().empty())
        {
          m_animated_path.add_animated_contour_raw(p, p);
        }
      else
        {
          m_animated_path.add_animated_contour_raw(closed_contours, p, ed[i].m_src->curves());
        }
    }
}

void
LightTest::
init_gl(int w, int h)
{
  PathCommand st, ed;

  m_prev_stats.resize(renderer().stats_labels().size(), 0);

  float pixel_size(32.0f);
  astral::Font font(default_typeface(), pixel_size);
  m_text_item = astral::TextItem::create(font);

  if (!load_path(m_path_file.value(), &m_path, &st))
    {
      const char *default_path =
        "[ (50.0, 35.0) [[(60.0, 50.0) ]] (70.0, 35.0)\n"
        "arc 180 (70.0, -100.0)\n"
        "[[ (60.0, -150.0) (30.0, -50.0) ]]\n"
        "(0.0, -100.0) arc 90 ]\n"
        //"{ (200, 200) (400, 200) (400, 400) (200, 400) }\n"
        //"[ (-50, 100) (0, 200) (100, 300) (150, 325) (150, 100) ]\n"
        //"{ (300 300) }\n"
        ;

      read_path(&m_path, default_path, &st);
    }

  if (load_path(m_end_path_file.value(), &m_end_path, &ed))
    {
      switch (m_animation_style.value())
        {
        case by_length_animation_generation:
          m_animated_path.set(m_path, m_end_path, astral::AnimatedPath::LengthContourSorter());
          break;

        case by_area_animation_generation:
          m_animated_path.set(m_path, m_end_path, astral::AnimatedPath::AreaContourSorter());
          break;

        case by_order_animation_generation:
          m_animated_path.set(m_path, m_end_path, astral::AnimatedPath::SimpleContourSorter());
          break;

        case manual_animation_generation:
          {
            add_animatations(false, st.m_open_contours, ed.m_open_contours);
            add_animatations(true, st.m_closed_contours, ed.m_closed_contours);
          }
          break;
        };
    }
  else
    {
      Line reflection;

      reflection.m_v.x() = m_reflect_direction_x.value();
      reflection.m_v.y() = m_reflect_direction_y.value();
      reflection.m_v.normalize();
      if (m_reflect_pt_x.set_by_command_line() || m_reflect_pt_y.set_by_command_line())
        {
          reflection.m_p.x() = m_reflect_pt_x.value();
          reflection.m_p.y() = m_reflect_pt_y.value();
        }
      else
        {
          reflection.m_p = m_path.bounding_box().as_rect().center_point();
        }

      create_animated_reflection(&m_animated_path, m_path, reflection, &m_end_path);
    }

  m_stroke_params.width(0.0f);
  if (m_light_to_p.set_by_command_line())
    {
      m_light_to_p.value() = astral::vec2(w, h);
    }

  float inner_size(15.0f);
  float outer_size(30.0f);

  m_ui_inner_rect
    .min_point(-0.5f * inner_size, -0.5f * inner_size)
    .max_point(+0.5f * inner_size, +0.5f * inner_size);

  m_ui_outer_rect
    .min_point(-0.5f * outer_size, -0.5f * outer_size)
    .max_point(+0.5f * outer_size, +0.5f * outer_size);

  if (m_initial_camera.set_by_command_line())
    {
      m_zoom.transformation(m_initial_camera.value());
    }
  else
    {
      /* set the camera so that the middle of the path's
       * bounding box is in the middle of the window
       */
        astral::BoundingBox<float> bb;

        bb = m_path.bounding_box();
        if (!bb.empty())
          {
            astral::vec2 screen_pt(w / 2, h / 2), path_pt;
            UniformScaleTranslate<float> tr;

            path_pt = (bb.min_point() + bb.max_point()) * 0.5f;
            tr.m_translation = screen_pt - path_pt;
            m_zoom.transformation(tr);
          }
    }
}

float
LightTest::
compute_animation_interpolate(int32_t &ms)
{
  float t;

  ms = m_path_time.value().elapsed() % (2 * m_animation_time.value());
  t = static_cast<float>(ms) / m_animation_time.value();
  t = astral::t_min(2.0f, astral::t_max(0.0f, t));
  t = (t > 1.0f) ? 2.0f - t : t;
  t = astral::t_min(1.0f, astral::t_max(0.0f, t));

  return t;
}

void
LightTest::
draw_hud(int32_t raw_ms, float anim_t,
         astral::RenderEncoderSurface encoder, float frame_ms)
{
  static const enum astral::Renderer::renderer_stats_t vs[] =
    {
      astral::Renderer::number_sparse_fill_subrects_clipping,
      astral::Renderer::number_sparse_fill_subrect_skip_clipping,
      astral::Renderer::number_sparse_fill_contour_skip_clipping,
      astral::Renderer::number_sparse_fill_curves_clipped,
      astral::Renderer::number_sparse_fill_curves_mapped,
      astral::Renderer::number_sparse_fill_contours_clipped,
      astral::Renderer::number_sparse_fill_contours_mapped,
      astral::Renderer::number_sparse_fill_late_culled_contours,
      astral::Renderer::number_virtual_buffer_pixels,
      astral::Renderer::number_non_degenerate_virtual_buffers,
      astral::Renderer::number_offscreen_render_targets,
      astral::Renderer::number_vertices_streamed,
      astral::Renderer::number_static_u32vec4_streamed,
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
       << "Light@ " << m_light_p.value() << "\n"
       << "Raw MS= " << raw_ms << "\n"
       << "interpolate_t = " << anim_t << "\n"
       << "Mouse at " << mouse_pos << "\n";

  if (m_fill_params.m_fill_rule != astral::number_fill_rule)
    {
      ostr << "Rendering: " << astral::label(m_mask_fill_params.m_sparse_mask) << "\n";
    }

  ostr << "Average over " << m_frame_time_average.interval_ms() << " ms: "
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
LightTest::
draw_ui_rect(astral::RenderEncoderSurface render_encoder,
             astral::RenderValue<astral::Brush> outer,
             astral::RenderValue<astral::Brush> inner,
             astral::vec2 p)
{
  render_encoder.save_transformation();
  render_encoder.transformation(astral::Transformation().translate(p));
  render_encoder.draw_rect(m_ui_outer_rect, false, outer);
  render_encoder.draw_rect(m_ui_inner_rect, false, inner);
  render_encoder.restore_transformation();
}

void
LightTest::
draw_lighting(astral::CombinedPath drawn_path,
              const astral::BoundingBox<float> &bb,
              astral::RenderEncoderBase render_encoder)
{
  /* generate the ShadowMap in the same coordinate system as we
   * drew the stroke or fill. Doing so gaurantees that the same
   * tessellation for the path is used for generating the shadow
   * map.
   */
  astral::ivec2 dims(dimensions());

  astral::reference_counted_ptr<astral::ShadowMap> shadow_map;
  astral::RenderEncoderShadowMap shadow_map_generator;
  unsigned int shadow_map_size;
  const astral::ShaderSet &shader_set(render_encoder.default_shaders());
  const astral::MaterialShader *material_shader;

  switch (m_anti_alias_shadow.value())
    {
    default:
    case shadow_aa_none:
      material_shader = shader_set.m_light_material_shader.get();
      break;

    case shadow_aa4:
      material_shader = shader_set.m_light_material_shader_aa4_shadow.get();
      break;

    case shadow_aa8:
      material_shader = shader_set.m_light_material_shader_aa8_shadow.get();
      break;

    case shadow_aa16:
      material_shader = shader_set.m_light_material_shader_aa16_shadow.get();
      break;
    }

  shadow_map_size = astral::t_max(dims.x(), dims.y());
  shadow_map_size = astral::t_min(shadow_map_size, engine().shadow_map_atlas().backing().width());

  shadow_map_generator = render_encoder.encoder_shadow_map_relative(shadow_map_size, m_light_p.value());
  shadow_map_generator.add_path(drawn_path, m_include_implicit_closing_edges.value());
  shadow_map = shadow_map_generator.finish();

  astral::RenderValue<const astral::ShadowMap&> render_shadow_map;
  astral::LightMaterialShader::LightProperties light_properties;

  render_shadow_map = render_encoder.create_value(*shadow_map);
  light_properties
    .light_z(m_light_z.value())
    .shadow_fall_off(m_light_shadow_fall_off.value())
    .shadow_fall_off_length(m_light_shadow_fall_off_length.value())
    .shadow_map(render_shadow_map)
    .light_direction((m_light_to_p.value() - m_light_p.value()).unit_vector())
    .color(astral::u8vec4(255u, 0u, 0u, 127u))
    .shadow_color(astral::u8vec4(0u, 0u, 255u, 77u));

  if (m_light_directional.value())
    {
      light_properties.directional_angle_degrees(m_light_angle.value());
    }

  /* When doing the ShadowMap look up, we need the transformation from
   * item coordinates to ShadowMap coordinates; ShadowMap coordinates are
   * the same as pixel coordiante and that transformation is the current
   * transformation on encoder.
   */
  astral::vecN<astral::gvec4, astral::LightMaterialShader::item_data_size> item_data;
  astral::LightMaterialShader::pack_item_data(render_encoder.transformation_value(), light_properties, item_data);
  astral::ItemData item_data_value;

  item_data_value = render_encoder.create_item_data(item_data, astral::LightMaterialShader::intrepreted_value_map());

  astral::Material light_material(*material_shader, item_data_value);
  render_encoder.draw_rect(bb.as_rect(), false, light_material);
}

void
LightTest::
draw_frame(void)
{
  float frame_ms, t;
  int32_t ms;
  astral::Transformation tr;
  astral::RenderEncoderSurface render_encoder;
  astral::Transformation out_mask_transformation_logical;
  astral::RectT<int> out_pixel_rect;
  astral::BoundingBox<float> out_logical_rect;
  astral::c_array<const unsigned int> stats;
  astral::CombinedPath drawn_path;

  m_frame_time_average.increment_counter();
  frame_ms = update_smooth_values();
  tr = m_zoom.transformation().astral_transformation();

  tr.scale(m_scale_pre_rotate.value());
  tr.rotate(m_rotate_angle.value() * (ASTRAL_PI / 180.0f));
  tr.scale(m_scale_post_rotate.value());

  render_encoder = renderer().begin( render_target());

  render_encoder.transformation(tr);

  t = compute_animation_interpolate(ms);

  switch (m_mode.value())
    {
    default:
      break;

    case animated_path:
      t = astral::t_min(1.0f, astral::t_max(0.0f, t));
      break;

    case animated_path_at_0:
      t = 0.0f;
      break;

    case animated_path_at_1:
      t = 1.0f;
      break;
    }

  switch (m_mode.value())
    {
    case animated_path:
    case animated_path_at_0:
    case animated_path_at_1:
      drawn_path = astral::CombinedPath(t, m_animated_path);
      break;

    case t0_path:
      drawn_path = astral::CombinedPath(m_path);
      break;

    case t1_path:
      drawn_path = astral::CombinedPath(m_end_path);
      break;

    default:
      ASTRALfailure("Invalid path mode");
      drawn_path = astral::CombinedPath(m_path);
    }

  m_mask_fill_params.render_scale_factor(m_scale_factor.value());

  /* or fill path via fill_paths() and pass m_fill_params */
  if (m_fill_params.m_fill_rule != astral::number_fill_rule)
    {
      render_encoder.fill_paths(drawn_path, m_fill_params,
                                render_encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 0.0f, 1.0f))),
                                astral::blend_porter_duff_src_over,
                                m_mask_fill_usage_params, m_mask_fill_params);
    }

  if (m_stroke.value())
    {
      render_encoder.stroke_paths(drawn_path, m_stroke_params,
                                  astral::ItemMaterial(),
                                  astral::blend_porter_duff_src_over,
                                  m_stroke_mask_usage_params,
                                  m_stroke_mask_params);
    }

  astral::BoundingBox<float> bb(drawn_path.compute_bounding_box());
  bb.enlarge(bb.size());

  if (m_soft_shadow_penumbra.value() > 0.0f)
    {
      astral::RenderEncoderLayer layer;
      const astral::Effect *blur_effect;
      astral::GaussianBlurParameters blur_params;

      blur_effect = render_encoder.default_effects().m_gaussian_blur.get();
      blur_params
        .radius(m_soft_shadow_penumbra.value())
        .min_render_scale(1.0f)
        .include_halo(false);

      layer = render_encoder.begin_layer(*blur_effect, blur_params.effect_parameters(), bb);
      draw_lighting(drawn_path, bb, layer.encoder());
      render_encoder.end_layer(layer);
    }
  else
    {
      draw_lighting(drawn_path, bb, render_encoder);
    }

  render_encoder.save_transformation();
    {
      astral::vec2 p0, p1;
      astral::RenderValue<astral::Brush> black, white;

      p0 = render_encoder.transformation().apply_to_point(m_light_p.value());
      p1 = render_encoder.transformation().apply_to_point(m_light_to_p.value());
      render_encoder.transformation(astral::Transformation());

      black = render_encoder.create_value(astral::Brush().base_color(astral::vec4(0.0f, 0.0f, 0.0f, 1.0f)));
      white = render_encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 1.0f, 1.0f)));
      draw_ui_rect(render_encoder, black, white, p0);

      if (m_light_directional.value())
        {
          draw_ui_rect(render_encoder, white, black, p1);
        }
    }
  render_encoder.restore_transformation();


  if (!pixel_testing())
    {
      draw_hud(ms, t, render_encoder, frame_ms);
    }

  stats = renderer().end();
  ASTRALassert(m_prev_stats.size() == stats.size());
  std::copy(stats.begin(), stats.end(), m_prev_stats.begin());
}

float
LightTest::
update_smooth_values(void)
{
  const Uint8 *keyboard_state = SDL_GetKeyboardState(nullptr);
  float return_value, delta, scale_delta, angle_delta;
  astral::vec2 *scale_ptr(nullptr);
  astral::c_string scale_txt(nullptr);
  bool scale_factor_changed(false), light_angle_changed(false);
  const float scale_rate(0.0001f);
  const float light_angle_rate(0.5f);
  float scale_factor_delta(0.0f);
  bool alt_held, ctrl_held;
  unsigned int animation_time_delta = 10;

  ASTRALassert(keyboard_state != nullptr);
  return_value = delta = static_cast<float>(m_draw_timer.restart_us()) * 0.001f;

  alt_held = keyboard_state[SDL_SCANCODE_LALT] || keyboard_state[SDL_SCANCODE_RALT];
  ctrl_held = keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL];

  if (keyboard_state[SDL_SCANCODE_LSHIFT])
    {
      delta *= 0.1f;
      animation_time_delta = 1;
    }
  if (keyboard_state[SDL_SCANCODE_RSHIFT])
    {
      delta *= 10.0f;
      animation_time_delta = 100;
    }

  if (keyboard_state[SDL_SCANCODE_LEFT])
    {
      if (alt_held)
        {
          m_soft_shadow_penumbra.value() -= delta;
          std::cout << "Soft shadow penumbra set to " << m_soft_shadow_penumbra.value() << "\n";
        }
      else if (ctrl_held)
        {
          m_light_shadow_fall_off_length.value() -= delta;
          std::cout << "Shadow fall off length set to " << m_light_shadow_fall_off_length.value() << "\n";
        }
      else
        {
          m_light_shadow_fall_off.value() -= delta;
          std::cout << "Shadow fall off set to " << m_light_shadow_fall_off.value() << "\n";
        }
    }

  if (keyboard_state[SDL_SCANCODE_RIGHT])
    {
      if (alt_held)
        {
          m_soft_shadow_penumbra.value() += delta;
          std::cout << "Soft shadow penumbra set to " << m_soft_shadow_penumbra.value() << "\n";
        }
      else if (ctrl_held)
        {
          m_light_shadow_fall_off_length.value() += delta;
          std::cout << "Shadow fall off length set to " << m_light_shadow_fall_off_length.value() << "\n";
        }
      else
        {
          m_light_shadow_fall_off.value() += delta;
          std::cout << "Shadow fall off set to " << m_light_shadow_fall_off.value() << "\n";
        }
    }

  if (keyboard_state[SDL_SCANCODE_UP])
    {
      if (keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
        {
          scale_factor_delta += delta * scale_rate;
          scale_factor_changed = true;
        }
      else if (keyboard_state[SDL_SCANCODE_LALT])
        {
          m_light_z.value() += delta;
          std::cout << "Light-z set to " << m_light_z.value() << "\n";
        }
      else if (keyboard_state[SDL_SCANCODE_RALT])
        {
          m_animation_time.value() += animation_time_delta;
        }
      else
        {
          m_light_angle.value() = astral::t_min(m_light_angle.value() + light_angle_rate * delta, 360.0f);
          light_angle_changed = true;
        }
    }

  if (keyboard_state[SDL_SCANCODE_DOWN])
    {
      if (keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
        {
          scale_factor_delta -= delta * scale_rate;
          scale_factor_changed = true;
        }
      else if (keyboard_state[SDL_SCANCODE_LALT])
        {
          m_light_z.value() -= delta;
          std::cout << "Light-z set to " << m_light_z.value() << "\n";
        }
      else if (keyboard_state[SDL_SCANCODE_RALT])
        {
          if (m_animation_time.value() > animation_time_delta)
            {
              m_animation_time.value() -= animation_time_delta;
            }
          else
            {
              m_animation_time.value() = 1;
            }
        }
      else
        {
          m_light_angle.value() = astral::t_max(m_light_angle.value() - light_angle_rate * delta, 0.0f);
          light_angle_changed = true;
        }
    }

  if (light_angle_changed)
    {
      std::cout << "Light angle set to " << m_light_angle.value() << " degrees\n";
    }

  if (scale_factor_changed)
    {
      m_scale_factor.value() += scale_factor_delta;
      m_scale_factor.value() = astral::t_max(0.0f, m_scale_factor.value());
      std::cout << "Fill path scale factor set to " << m_scale_factor.value()
                << "\n" << std::flush;
    }

  scale_delta = 0.01f * delta;
  angle_delta = 0.0025f * delta * (180.0f / ASTRAL_PI);
  if (keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
    {
      scale_delta = -scale_delta;
    }

  if (keyboard_state[SDL_SCANCODE_RETURN])
    {
      scale_ptr = &m_scale_post_rotate.value();
      scale_txt = "post-rotate-scale";
    }
  else
    {
      scale_ptr = &m_scale_pre_rotate.value();
      scale_txt = "pre-rotate-scale";
    }

  if (keyboard_state[SDL_SCANCODE_6] && !alt_held)
    {
      scale_ptr->x() += scale_delta;
      std::cout << scale_txt << " set to: " << *scale_ptr << "\n";
    }
  if (keyboard_state[SDL_SCANCODE_7] && !alt_held)
    {
      scale_ptr->y() += scale_delta;
      std::cout << scale_txt << " set to: " << *scale_ptr << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_9] && !alt_held)
    {
      m_rotate_angle.value() += angle_delta;
      if (angle_delta > 360.0f)
        {
          m_rotate_angle.value() -= 360.0f;
        }
      std::cout << "Angle set to: " << m_rotate_angle.value() << " degrees\n";
    }
  if (keyboard_state[SDL_SCANCODE_0] && !alt_held)
    {
      m_rotate_angle.value() -= angle_delta;
      if (angle_delta < 0.0f)
        {
          m_rotate_angle.value() += 360.0f;
        }
      std::cout << "Angle set to: " << m_rotate_angle.value() << " degrees\n";
    }

  return return_value;
}

void
LightTest::
handle_event(const SDL_Event &ev)
{
  m_zoom.handle_event(ev);
  if (ev.type == SDL_MOUSEMOTION)
    {
      astral::vec2 p, c(ev.motion.x + ev.motion.xrel,
                        ev.motion.y + ev.motion.yrel);
      astral::Transformation tr;

      /* light is in item coordinates */
      tr = m_zoom.transformation().astral_transformation();
      tr.scale(m_scale_pre_rotate.value());
      tr.rotate(m_rotate_angle.value() * (ASTRAL_PI / 180.0f));
      tr.scale(m_scale_post_rotate.value());
      tr = tr.inverse();
      p = tr.apply_to_point(c);

      if (ev.motion.state & SDL_BUTTON(SDL_BUTTON_RIGHT))
        {
          m_light_p.value() = p;
        }

      if (ev.motion.state & SDL_BUTTON(SDL_BUTTON_MIDDLE))
        {
          m_light_to_p.value() = p;
        }
    }
  else if (ev.type == SDL_KEYDOWN)
    {
      switch(ev.key.keysym.sym)
        {
        case SDLK_z:
          if (ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT))
            {
              renderer().default_render_accuracy(2.0f * renderer().default_render_accuracy());
            }
          else
            {
              renderer().default_render_accuracy(0.5f * renderer().default_render_accuracy());
            }
          std::cout << "Render accuracy set to "
                    << renderer().default_render_accuracy() << "\n";
          break;

        case SDLK_c:
          m_include_implicit_closing_edges.value() = !m_include_implicit_closing_edges.value();
          std::cout << "Have implicit closing edges of open contours cast shadows set to "
                    << std::boolalpha << m_include_implicit_closing_edges.value() << "\n";
          break;

        case SDLK_f:
          {
            int64_t fake = 1;
            if (ev.key.keysym.mod & KMOD_SHIFT)
              {
                fake *= 10;
              }

            if (ev.key.keysym.mod & KMOD_CTRL)
              {
                fake *= 100;
              }

            if (ev.key.keysym.mod & KMOD_ALT)
              {
                fake *= 1000;
              }
            m_path_time.value().decrement_time(fake);
          }
          break;

        case SDLK_g:
          {
            int64_t fake = 1;
            if (ev.key.keysym.mod & KMOD_SHIFT)
              {
                fake *= 10;
              }

            if (ev.key.keysym.mod & KMOD_CTRL)
              {
                fake *= 100;
              }

            if (ev.key.keysym.mod & KMOD_ALT)
              {
                fake *= 1000;
              }
            m_path_time.value().increment_time(fake);
          }
          break;

        case SDLK_SPACE:
          if (ev.key.keysym.mod & KMOD_SHIFT)
            {
              m_show_render_stats.value() = !m_show_render_stats.value();
            }
          else
            {
              cycle_value(m_hud_mode, false, number_hud_modes);
            }
          break;

        case SDLK_q:
          reset_zoom_transformation();
          m_scale_pre_rotate.value() = m_scale_post_rotate.value() = astral::vec2(1.0f, 1.0f);
          m_rotate_angle.value() = 0.0f;
          break;

        case SDLK_a:
          if (ev.key.keysym.mod & KMOD_CTRL)
            {
              cycle_value(m_fill_params.m_aa_mode,
                          ev.key.keysym.mod & (KMOD_SHIFT|KMOD_ALT),
                          astral::number_anti_alias_modes);
              std::cout << "Fill anti-aliasing set to "
                        << astral::label(m_fill_params.m_aa_mode) << "\n";
            }
          else
            {
              cycle_value(m_anti_alias_shadow.value(),
                          ev.key.keysym.mod & (KMOD_SHIFT|KMOD_ALT),
                          number_shadow_aa);
              std::cout << "Anti-alias shadow set to " << std::boolalpha
                        << label(m_anti_alias_shadow.value()) << "\n";
            }
          break;

        case SDLK_h:
          cycle_value(m_mask_fill_usage_params.m_mask_type,
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      astral::number_mask_type);
          std::cout << "Fill maks mode set to "
                    << astral::label(m_mask_fill_usage_params.m_mask_type) << "\n";
          break;

        case SDLK_r:
          if (ev.key.keysym.mod & KMOD_SHIFT)
            {
              cycle_value(m_mask_fill_params.m_sparse_mask,
                          ev.key.keysym.mod & (KMOD_CTRL|KMOD_ALT),
                          astral::number_fill_method_t);
              std::cout << "Filling with sparse mask set to: "
                        << astral::label(m_mask_fill_params.m_sparse_mask)
                        << "\n";
            }
          else
            {
              cycle_value(m_fill_params.m_fill_rule,
                          ev.key.keysym.mod & (KMOD_CTRL|KMOD_ALT),
                          astral::number_fill_rule + 1);

              if (m_fill_params.m_fill_rule != astral::number_fill_rule)
                {
                  std::cout << "Fill rule set to "
                            << astral::label(m_fill_params.m_fill_rule) << "\n";
                }
              else
                {
                  std::cout << "Filling off\n";
                }
            }
          break;

        case SDLK_s:
          m_stroke.value() = !m_stroke.value();
          break;

        case SDLK_p:
          m_path_time.value().pause(!m_path_time.value().paused());
          if (m_path_time.value().paused())
            {
              int32_t ms;
              float t;

              t = compute_animation_interpolate(ms);
              std::cout << "Animation paused at " << t  << "(raw = " << ms << ")\n";
            }
          std::cout << "Current Zoom = " << m_zoom.transformation().m_scale
                    << "\n";
          break;

        case SDLK_d:
          {
            cycle_value(m_mode.value(),
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        number_path_modes);
            std::cout << "Draw mode set to " << label(m_mode.value()) << "\n";
          }
          break;

        case SDLK_k:
          cycle_value(m_mask_fill_usage_params.m_filter,
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      astral::number_filter_modes);
          std::cout << "Mask filter mode mode set to "
                    << astral::label(m_mask_fill_usage_params.m_filter) << "\n";
          break;

        case SDLK_l:
          m_light_directional.value() = !m_light_directional.value();
          std::cout << "Direction Light set to: " << std::boolalpha << m_light_directional.value() << "\n";
          break;

        case SDLK_0:
          if (ev.key.keysym.mod & KMOD_ALT)
            {
              m_scale_factor.value() = 1.0f;
              std::cout << "Fill path render scale factor set to " << m_scale_factor.value()
                        << "\n" << std::flush;
            }
          break;

        case SDLK_1:
        case SDLK_2:
        case SDLK_3:
        case SDLK_4:
        case SDLK_5:
        case SDLK_6:
        case SDLK_7:
        case SDLK_8:
        case SDLK_9:
          if (ev.key.keysym.mod & KMOD_ALT)
            {
              int v;
              float fv;

              v = (ev.key.keysym.sym - SDLK_1) + 1;
              fv = static_cast<float>(v) * 0.1f;
              m_scale_factor.value() = fv;
              std::cout << "Fill path render scale factor set to " << m_scale_factor.value()
                        << "\n" << std::flush;
            }
          break;

        }
    }
  render_engine_gl3_demo::handle_event(ev);
}

int
main(int argc, char **argv)
{
  LightTest M;
  return M.main(argc, argv);
}

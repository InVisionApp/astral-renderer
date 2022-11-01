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

#include "wavy_stroke.hpp"
#include "graph_stroke.hpp"
#include "wavy_graph_stroke.hpp"


std::ostream&
operator<<(std::ostream &str, const astral::StrokeShader::DashPattern &pattern)
{
  for (const float v : pattern.source_intervals())
    {
      if (v >= 0.0f)
        {
          str << "Draw: " << v;
        }
      else
        {
          str << "Skip: " << -v;
        }
      str << " ";
    }
  return str;
}

class PrintFloatBits
{
public:
  explicit
  PrintFloatBits(float f)
  {
    m_v.f = f;
  }

  astral::generic_data m_v;
};

std::ostream&
operator<<(std::ostream &ostr, PrintFloatBits f)
{
  ostr << f.m_v.f << "(0x" << std::hex << f.m_v.u << "u)";
  return ostr;
}


class PathTest:public render_engine_gl3_demo
{
public:
  PathTest(void);

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
  enum display_fill_method_t
    {
      display_fill_directly,
      display_fill_with_item_mask,
      display_fill_with_render_clip,
      display_fill_with_render_clip_complement,
      display_fill_with_clip,
      display_fill_test_clip_in_clip_out,
      display_fill_item_path,

      number_display_fill_methods
    };

  enum animation_generation_t
    {
      by_length_animation_generation,
      by_area_animation_generation,
      by_order_animation_generation,
      manual_animation_generation,
    };

  enum path_mode_t
    {
      t0_path,
      animated_path_at_0,
      t1_path,
      animated_path_at_1,
      animated_path,

      number_path_modes,
    };

  enum hud_mode_t
    {
      basic_hud,
      basic_hud_with_commands,
      detail_level1_hud,
      detail_level2_hud,
      detail_level3_hud,

      number_hud_modes,
    };

  enum stroke_mode_t
    {
      stroke_vanilla,
      stroke_direct,
      stroke_direct_with_mask,
      stroke_wavy,
      stroke_graph,
      stroke_wavy_graph,
      stroke_clip_in_cutoff,
      stroke_clip_in_combine,
      stroke_clip_out_cutoff,
      stroke_clip_out_combine,
      stroke_clip_union_cutoff,
      stroke_clip_union_combine,
      number_stroke_modes,

      stroke_none = number_stroke_modes
    };

  static
  bool
  stroke_include_clip_in_content(enum stroke_mode_t m)
  {
    return m == stroke_clip_in_cutoff
      || m == stroke_clip_union_cutoff
      || m == stroke_clip_in_combine
      || m == stroke_clip_union_combine;
  }

  static
  bool
  stroke_include_clip_out_content(enum stroke_mode_t m)
  {
    return m == stroke_clip_out_cutoff
      || m == stroke_clip_union_cutoff
      || m == stroke_clip_out_combine
      || m == stroke_clip_union_combine;
  }

  enum astral::mask_item_shader_clip_mode_t
  mask_item_shader_clip_mode(enum stroke_mode_t m)
  {
    bool b;

    b = (m == stroke_clip_in_cutoff
         || m == stroke_clip_out_cutoff
         || m == stroke_clip_union_cutoff);

    return (b) ?
      astral::mask_item_shader_clip_cutoff :
      astral::mask_item_shader_clip_combine;
  }

  enum background_blur_mode
    {
       no_backgroud_blur,
       background_blur_radius_relative_to_pixels,
       background_blur_radius_relative_to_path,

       number_background_blur_modes,
    };

  static
  astral::c_string
  label(enum stroke_mode_t v)
  {
    static astral::c_string labels[number_stroke_modes + 1] =
      {
        [stroke_vanilla] = "stroke_vanilla",
        [stroke_direct] = "stroke_direct",
        [stroke_direct_with_mask] = "stroke_direct_with_mask",
        [stroke_wavy] = "stroke_wavy",
        [stroke_graph] = "stroke_graph",
        [stroke_wavy_graph] = "stroke_wavy_graph",
        [stroke_clip_in_cutoff] = "stroke_clip_in_cutoff",
        [stroke_clip_in_combine] = "stroke_clip_in_combine",
        [stroke_clip_out_cutoff] = "stroke_clip_out_cutoff",
        [stroke_clip_out_combine] = "stroke_clip_out_combine",
        [stroke_clip_union_cutoff] = "stroke_clip_union_cutoff",
        [stroke_clip_union_combine] = "stroke_clip_union_combine",
        [number_stroke_modes] = "stroke_none",
      };

    ASTRALassert(v <= number_stroke_modes);
    return labels[v];
  }

  static
  astral::c_string
  label(enum path_mode_t v)
  {
    static astral::c_string labels[number_path_modes] =
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
  label(enum display_fill_method_t v)
  {
    static astral::c_string labels[number_display_fill_methods] =
      {
        [display_fill_directly] = "display_fill_directly",
        [display_fill_with_item_mask] = "display_fill_with_item_mask",
        [display_fill_with_render_clip] = "display_fill_with_render_clip",
        [display_fill_with_render_clip_complement] = "display_fill_with_render_clip_complement",
        [display_fill_with_clip] = "display_fill_with_clip",
        [display_fill_test_clip_in_clip_out] = "display_fill_test_clip_in_clip_out",
        [display_fill_item_path] = "display_fill_item_path",
      };

    ASTRALassert(v < number_display_fill_methods);
    return labels[v];
  }

  static
  astral::c_string
  label(enum background_blur_mode v)
  {
    static astral::c_string labels[number_background_blur_modes] =
      {
        [no_backgroud_blur] = "no_backgroud_blur",
        [background_blur_radius_relative_to_pixels] = "background_blur_radius_relative_to_pixels",
        [background_blur_radius_relative_to_path] = "background_blur_radius_relative_to_path",
      };

    ASTRALassert(v < number_background_blur_modes);
    return labels[v];
  }

  float
  update_smooth_values(void);

  void
  reset_zoom_transformation(void);

  float
  compute_animation_interpolate(void);

  bool
  load_path(const std::string &filename, astral::Path *dst, PathCommand *dst_cmd);

  void
  add_animatations(bool closed_contours,
                   const std::vector<PerContourCommand> &st,
                   const std::vector<PerContourCommand> &ed);

  void
  draw_hud(astral::RenderEncoderSurface encoder, float frame_ms);

  astral::Gradient
  generate_gradient(void);

  void
  generate_stroke_shaders(void);

  void
  compute_wavy_value(float distance_scale_factor, WavyPattern &wavy)
  {
    uint32_t ms_tt;
    float fs, fc, fs2, fc2;

    ms_tt = m_path_time.value().elapsed() % 4000u;

    wavy.m_phase = static_cast<float>(ms_tt) / 2000.0f * ASTRAL_PI;
    wavy.m_domain_coeff = 8.0f * ASTRAL_PI / (m_stroke_params.m_width * 10.0f * distance_scale_factor);

    fc = astral::t_cos(wavy.m_phase);
    fs = astral::t_sin(wavy.m_phase);
    fc2 = astral::t_cos(2.0f * wavy.m_phase);
    fs2 = astral::t_sin(2.0f * wavy.m_phase);
    wavy.m_cos_coeffs = astral::vec4(+8.0f * fc, -4.0f * fs, +2.0f * fs2, -1.0f * fc2);
    wavy.m_sin_coeffs = astral::vec4(+1.3f * fs, -2.0f * fc, +4.0f * fs2, -8.0f * fc2);
  }

  void
  compute_graph_value(GraphPattern &G)
  {
    G.m_thickness = m_stroke_params.m_width * m_graph_stroke_thickness.value();
    G.m_spacing = m_stroke_params.m_width * m_graph_stroke_spacing.value();
    G.m_count = std::round(0.5f / m_graph_stroke_spacing.value());
  }

  astral::FillParameters m_fill_params;
  astral::FillMaskProperties m_mask_fill_params;
  astral::MaskUsage m_mask_fill_usage_params;

  astral::StrokeParameters m_stroke_params;
  astral::StrokeMaskProperties m_mask_stroke_params;
  astral::MaskUsage m_mask_stroke_usage_params;

  command_separator m_demo_options;
  command_line_argument_value<std::string> m_path_file;
  command_line_argument_value<std::string> m_end_path_file;
  command_line_argument_value<bool> m_show_render_stats;
  command_line_argument_value<std::string> m_image_file;
  enumerated_command_line_argument_value<enum animation_generation_t> m_animation_style;
  command_line_argument_value<unsigned int> m_animation_time;
  command_line_list_dash_pattern m_dash_pattern_files;
  command_line_argument_value<float> m_reflect_direction_x;
  command_line_argument_value<float> m_reflect_direction_y;
  command_line_argument_value<float> m_reflect_pt_x;
  command_line_argument_value<float> m_reflect_pt_y;
  command_line_argument_value<UniformScaleTranslate<float>> m_initial_camera;
  command_line_argument_value<float> m_scale_factor;
  enumerated_command_line_argument_value<enum astral::filter_t> m_mask_filter;
  command_line_argument_value<astral::vec2> m_scale_pre_rotate, m_scale_post_rotate;
  command_line_argument_value<float> m_rotate_angle;
  enumerated_command_line_argument_value<enum path_mode_t> m_path_mode;
  command_line_argument_value<simple_time> m_path_time;
  enumerated_command_line_argument_value<enum astral::fill_rule_t&> m_fill_params_fill_rule;
  enumerated_command_line_argument_value<enum astral::anti_alias_t&> m_fill_params_aa;
  enumerated_command_line_argument_value<enum astral::fill_method_t&> m_fill_params_fill_method;
  enumerated_command_line_argument_value<enum astral::mask_type_t&> m_fill_mask_type;
  command_line_argument_value<bool&> m_stroke_params_sparse_mask;
  enumerated_command_line_argument_value<enum astral::mask_type_t&> m_stroke_mask_type;
  command_line_argument_value<float&> m_stroke_params_width;
  enumerated_command_line_argument_value<enum astral::join_t&> m_stroke_params_join;
  enumerated_command_line_argument_value<enum astral::cap_t&> m_stroke_params_cap;
  enumerated_command_line_argument_value<enum astral::join_t&> m_stroke_params_glue_join;
  enumerated_command_line_argument_value<enum astral::join_t&> m_stroke_params_glue_cusp_join;
  command_line_argument_value<float&> m_stroke_params_miter_limit;
  command_line_argument_value<bool&> m_stroke_params_draw_edges;
  command_line_argument_value<bool> m_graceful_thin_stroking;
  command_line_argument_value<bool> m_stroke_width_pixels;
  command_line_argument_value<bool> m_scale_dash_pattern_on_pixel_width_stroking;
  enumerated_command_line_argument_value<enum stroke_mode_t> m_stroke_mode;
  command_line_argument_value<unsigned int> m_dash_pattern_choice;
  command_line_argument_value<float> m_dash_pattern_start_offset;
  command_line_argument_value<bool> m_dash_pattern_draw_lengths_adjusted;
  command_line_argument_value<bool> m_dash_pattern_skip_lengths_adjusted;
  enumerated_command_line_argument_value<enum astral::StrokeShader::DashPattern::adjust_t> m_dash_pattern_adjust_mode;
  command_line_argument_value<float> m_dash_pattern_corner_radius;
  command_line_argument_value<bool> m_dash_pattern_per_edge;
  command_line_argument_value<float> m_graph_stroke_thickness, m_graph_stroke_spacing;
  enumerated_command_line_argument_value<enum display_fill_method_t> m_display_fill_method;
  enumerated_command_line_argument_value<enum astral::blend_mode_t> m_blend_mode;
  enumerated_command_line_argument_value<enum astral::tile_mode_t> m_gradient_tile_mode;
  enumerated_command_line_argument_value<enum astral::Gradient::type_t> m_gradient_type;
  command_line_argument_value<astral::vec2> m_gradient_p0, m_gradient_p1;
  command_line_argument_value<float> m_gradient_r0, m_gradient_r1, m_gradient_sweep_factor;
  command_line_argument_value<bool> m_swap_fill_and_stroke_brush;
  command_line_argument_value<bool> m_use_sub_ubers;
  enumerated_command_line_argument_value<enum background_blur_mode> m_background_blur_mode;
  command_line_argument_value<float> m_blur_min_scale_factor;
  command_line_argument_value<float> m_background_blur_radius;
  command_line_argument_value<float> m_blur_max_sample_radius;
  command_line_argument_value<bool> m_add_some_background_text;
  command_line_argument_value<float> m_alpha;
  command_line_argument_value<bool> m_render_to_layer;

  astral::reference_counted_ptr<astral::Image> m_image;
  astral::reference_counted_ptr<astral::TextItem> m_text_item;
  astral::reference_counted_ptr<astral::TextItem> m_bg_text_item;

  astral::vecN<astral::reference_counted_ptr<const astral::MaskStrokeShader>, number_stroke_modes> m_stroke_shaders;
  astral::vecN<astral::reference_counted_ptr<const astral::MaskStrokeShader>, number_stroke_modes> m_dashed_stroke_shaders;

  astral::Path m_path;
  astral::Path m_end_path;
  astral::AnimatedPath m_animated_path;
  std::vector<astral::StrokeShader::DashPattern> m_dash_patterns;

  simple_time m_draw_timer;
  average_timer m_frame_time_average;
  PanZoomTrackerSDLEvent m_zoom;

  astral::reference_counted_ptr<astral::ColorStopSequence> m_colorstop_sequence;

  bool m_print_stats, m_print_item_path_text;
  enum hud_mode_t m_hud_mode;
  std::vector<unsigned int> m_prev_stats;
  astral::Renderer::OffscreenBufferAllocInfo m_offscreen_alloc_info;
  bool m_show_offscreen_alloc_info;
};

//////////////////////////////////////////
//PathTest methods
PathTest::
PathTest(void):
  m_mask_fill_usage_params(astral::mask_type_distance_field),
  m_mask_stroke_usage_params(astral::mask_type_coverage),
  m_demo_options("Demo Options", *this),
  m_path_file("", "path", "File from which to read the path", *this),
  m_end_path_file("", "end_path",
                  "File from which to read the end path, if no file "
                  "is given then the ending path is path reflected ",
                  *this),
  m_show_render_stats(false, "show_render_stats",
                      "If true, at each frame print stdout stats of rendering",
                      *this),
  m_image_file("", "image", "name of file for image background", *this),
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
  m_dash_pattern_files("add_dash_pattern", "Add a dash pattern to use", *this),
  m_reflect_direction_x(0.0f, "reflect_direction_x", "x-coordinate of reflection axis direciton if end path is reflection", *this),
  m_reflect_direction_y(1.0f, "reflect_direction_y", "y-coordinate of reflection axis direciton if end path is reflection", *this),
  m_reflect_pt_x(0.0f, "reflect_pt_x", "x-coordinate of reflection axis position if end path is reflection", *this),
  m_reflect_pt_y(0.0f, "reflect_pt_y", "y-coordinate of reflection axis position if end path is reflection", *this),
  m_initial_camera(UniformScaleTranslate<float>(), "initial_camera",
                   "if set, initial position of camera otherwise camera initialize to see center on path with no zoom applied", *this),
  m_scale_factor(1.0f, "mask_scale_factor",
                 "Scale factor at which to generate stroke and fill masks "
                 "a value of less than 1.0 indicates that the mask is at a lower resolution "
                 "than its display", *this),
  m_mask_filter(astral::filter_linear,
                enumerated_string_type<enum astral::filter_t>(&astral::label, astral::number_filter_modes),
                "mask_filter",
                "filter to apply to masks generated for stroking and filling",
                *this),
  m_scale_pre_rotate(astral::vec2(1.0f, 1.0f), "scale_pre_rotate", "scaling transformation to apply to path before rotation, formatted as ScaleX:SaleY", *this),
  m_scale_post_rotate(astral::vec2(1.0f, 1.0f), "scale_post_rotate", "scaling transformation to apply to path after rotation, formatted as ScaleX:SaleY", *this),
  m_rotate_angle(0.0f, "rotate_angle", "rotation of path in degrees to apply to path", *this),
  m_path_mode(t0_path,
              enumerated_string_type<enum path_mode_t>(&label, number_path_modes),
              "path_mode",
              "Initial path mode to select to draw animated path or static path.",
              *this),
  m_path_time(simple_time(),
              "path_time",
              "If set, pauses the timer for path aimation and specifies the intial time value in ms",
              *this),
  m_fill_params_fill_rule(m_fill_params.m_fill_rule,
                          enumerated_string_type<enum astral::fill_rule_t>(&astral::label, astral::number_fill_rule)
                          .add_entry("no_fill", astral::number_fill_rule, ""),
                          "fill_rule",
                          "initial fill rule to apply to path",
                          *this),
  m_fill_params_aa(m_fill_params.m_aa_mode,
                   enumerated_string_type<enum astral::anti_alias_t>(&astral::label, astral::number_anti_alias_modes),
                   "fill_aa",
                   "anti-aliasing mode to apply to path fill",
                   *this),
  m_fill_params_fill_method(m_mask_fill_params.m_sparse_mask,
                            enumerated_string_type<enum astral::fill_method_t>(&astral::label, astral::number_fill_method_t),
                            "fill_method",
                            "method for generating fill mask",
                            *this),
  m_fill_mask_type(m_mask_fill_usage_params.m_mask_type,
                   enumerated_string_type<enum astral::mask_type_t>(&astral::label, astral::number_mask_type),
                   "fill_mask_type",
                   "specifies the kind of mask to use when filling the path",
                   *this),
  m_stroke_params_sparse_mask(m_mask_stroke_params.m_sparse_mask, "sparse_stroke",
                              "if true, generate a sparse mask for stroking",
                              *this),
  m_stroke_mask_type(m_mask_stroke_usage_params.m_mask_type,
                     enumerated_string_type<enum astral::mask_type_t>(&astral::label, astral::number_mask_type),
                     "stroke_mask_type",
                     "specifies the kind of mask to use when stroking the path",
                     *this),
  m_stroke_params_width(m_stroke_params.m_width, "stroke_width", "stroking width", *this),
  m_stroke_params_join(m_stroke_params.m_join,
                       enumerated_string_type<enum astral::join_t>(&astral::label, astral::number_join_t),
                       "stroke_join",
                       "stroking join style",
                       *this),
  m_stroke_params_cap(m_stroke_params.m_cap,
                      enumerated_string_type<enum astral::cap_t>(&astral::label, astral::number_cap_t),
                      "stroke_cap",
                      "stroking cap style",
                      *this),
  m_stroke_params_glue_join(m_stroke_params.m_glue_join,
                            enumerated_string_type<enum astral::join_t>(&astral::label, astral::number_join_t),
                            "stroke_glue_join",
                            "how to draw glue joins when stroking",
                            *this),
  m_stroke_params_glue_cusp_join(m_stroke_params.m_glue_cusp_join,
                                 enumerated_string_type<enum astral::join_t>(&astral::label, astral::number_join_t),
                                 "stroke_glue_cusp_join",
                                 "how to draw glue joins at cusps when stroking",
                                 *this),
  m_stroke_params_miter_limit(m_stroke_params.m_miter_limit, "stroke_miter_limit", "Miter limit when stroking", *this),
  m_stroke_params_draw_edges(m_stroke_params.m_draw_edges, "stroke_draw_edges", "specifies if to draw edges when stroking", *this),
  m_graceful_thin_stroking(true, "graceful_thin_stroking",
                           "If true, draw very thin strokes as transparent", *this),
  m_stroke_width_pixels(false, "stroke_width_pixels", "if true, the stroking width is in pixels", *this),
  m_scale_dash_pattern_on_pixel_width_stroking(false, "scale_dash_pattern_on_pixel_width_stroking",
                                               "if true, when performing dashed stroking and stroking width "
                                               "is in pixels, scale the dash pattern as well", *this),
  m_stroke_mode(stroke_vanilla,
                enumerated_string_type<enum stroke_mode_t>(&label, number_stroke_modes)
                .add_entry("no_stroke", stroke_none, ""),
                "stroke_mode",
                "Specifies stroking mode",
                *this),
  m_dash_pattern_choice(0u, "dash_pattern_choice",
                        "Select which dash pattern to apply to stroking with 0 meaning no dash pattern "
                        "and N meaning the N'th dash pattern loaded",
                        *this),
  m_dash_pattern_start_offset(0.0f, "dash_pattern_start_offset", "if set, set the dash pattern start offset for all dash patterns to this value", *this),
  m_dash_pattern_draw_lengths_adjusted(false, "dash_pattern_draw_lengths_adjusted", "if set, set the dash pattern if to adjust draw lenghts for all dash patterns to this value", *this),
  m_dash_pattern_skip_lengths_adjusted(false, "dash_pattern_skip_lengths_adjusted", "if set, set the dash pattern if to adjust skip lenghts for all dash patterns to this value", *this),
  m_dash_pattern_adjust_mode(astral::StrokeShader::DashPattern::adjust_none,
                             enumerated_string_type<enum astral::StrokeShader::DashPattern::adjust_t>(&astral::label, astral::StrokeShader::DashPattern::number_adjust),
                             "dash_pattern_adjust_mode",
                             "if set, set the dash pattern how to adjust for all dash patterns to this value", *this),
  m_dash_pattern_corner_radius(0.0f, "dash_pattern_corner_radius", "if set, set the dash pattern corner radius for all dash patterns to this value", *this),
  m_dash_pattern_per_edge(false, "dash_pattern_per_edge", "if set, set the dash pattern if to apply to each edge seperately for all dash patterns to this value", *this),
  m_graph_stroke_thickness(0.05f, "graph_stroke_thickness", "specifies relative thickess of graph stroking lines", *this),
  m_graph_stroke_spacing(0.25f, "graph_stroke_spacing", "specifies relative thickess of space between graph stroking lines", *this),
  m_display_fill_method(display_fill_directly,
                        enumerated_string_type<enum display_fill_method_t>(&label, number_display_fill_methods),
                        "display_fill_method",
                        "method with which to display/generate the fill",
                        *this),
  m_blend_mode(astral::blend_porter_duff_src_over,
               enumerated_string_type<enum astral::blend_mode_t>(&astral::label, astral::number_blend_modes),
               "blend_mode",
               "blend mode with which to draw the path",
               *this),
  m_gradient_tile_mode(astral::tile_mode_mirror_repeat,
                       enumerated_string_type<enum astral::tile_mode_t>(&astral::label, astral::tile_mode_number_modes),
                       "gradient_tile_mode",
                       "tile mode to apply to gradient pattern for interpolate outside of [0, 1]",
                       *this),
  m_gradient_type(astral::Gradient::number_types,
                  enumerated_string_type<enum astral::Gradient::type_t>(&astral::label, astral::Gradient::number_types)
                  .add_entry("no_gradient", astral::Gradient::number_types, ""),
                  "gradient_type",
                  "specify the kind of gradient to have",
                  *this),
  m_gradient_p0(astral::vec2(0.0f, 0.0f), "gradient_p0", "position for start point of gradient (linear and radial) or position of gradent center (for sweep gradients) ", *this),
  m_gradient_p1(astral::vec2(0.0f, 0.0f), "gradient_p1",
                "if set position for end point of gradient (linear and radial) or position of point to determine start axis "
                "(for sweep gradients), if not set value will be the dimensions of the window", *this),
  m_gradient_r0(0.0f, "gradient_r0", "if set, start radius for radial gradient, if not set value is maximum of the width and height of the window", *this),
  m_gradient_r1(0.0f, "gradient_r1", "if set, end radius for radial gradient, if not set value is maximum of the width and height of the window", *this),
  m_gradient_sweep_factor(3.0f, "gradient_sweep_factor", "gradient sweep factor for sweep gradient (i.e. how many times it repeats)", *this),
  m_swap_fill_and_stroke_brush(false, "swap_fill_and_stroke_brush",
                               "If false, fill gets the brush and stroke is white, if true "
                               "stroke gets the brush and fill is white",
                               *this),
  m_use_sub_ubers(true, "use_sub_ubers", "if true, instruct astral::Renderer to use sub-uber shaders", *this),
  m_background_blur_mode(no_backgroud_blur,
                         enumerated_string_type<enum background_blur_mode>(&label, number_background_blur_modes),
                         "background_blur_mode",
                         "option to instead of filling the path with a brush, to instead fill with a background blur",
                         *this),
  m_blur_min_scale_factor(0.0f, "blur_min_scale_factor",
                          "sets the minimum rendering scale for content that is blurred from backgroun blur",
                          *this),
  m_background_blur_radius(4.0f, "background_blur_radius", "blur rarius of background blur", *this),
  m_blur_max_sample_radius(8.0f, "blur_max_sample_radius", "maximum number of samples blur effect will use before using lower resolution images to achieve blur", *this),
  m_add_some_background_text(true, "add_some_background_text", "if true add a text blox background behind the path drawing", *this),
  m_alpha(1.0f, "alpha", "alpha value to apply to path drawing", *this),
  m_render_to_layer(false, "render_to_layer", "if true, first render path to a layer and then blit the layer", *this),

  m_frame_time_average(1000u),
  m_print_stats(false),
  m_print_item_path_text(false),
  m_hud_mode(basic_hud),
  m_show_offscreen_alloc_info(false)
{
  std::cout << "Controls:"
            << "\n\tspace: cycle through HUD modes"
            << "\n\tshift-space: toggle showing frame rate to console"
            << "\n\tq: reset transformation applied to the path"
            << "\n\te: toggle stroking edges when stroking"
            << "\n\tp: pause animation and print current magnification"
            << "\n\tshift-p: toggle stroking width pixels"
            << "\n\tctrl-p: toggle graceful thin stroking"
            << "\n\tr: cycle through different fill rules"
            << "\n\tctrl-r: cycle through different filling implementations"
            << "\n\ta: toggle filling with our without anti-aliasing"
            << "\n\to: toggle stroking with our without anti-aliasing"
            << "\n\td: cycle through drawing mode: draw start path, draw end path, draw animated path, etc"
            << "\n\tf: cycle through filling the path directly, drawing as clip-in only, or drawing both clip-in and clip-out"
            << "\n\tb: cycle through the backgroun blur modes"
            << "\n\tctrl-b: cycle through blend modes"
            << "\n\talt-b: toggle background text"
            << "\n\tl: toggle rendering to layer"
            << "\n\tk: cycle through filter mode when sampling from the mask or layer "
            << "\n\tj: cycle through different join styles"
            << "\n\talt-j: cycle through different glue join styles"
            << "\n\tctrl-j: cycle through different glue cusp join styles"
            << "\n\tctrl-m: toggle miter style"
            << "\n\tc: cycle through different cap styles"
            << "\n\ts: cycle through stroking modes"
            << "\n\tctrl-s: swap fill and stroke brushes"
            << "\n\talt-s: toggle use sub-uber shaders"
            << "\n\tx: cycle though dash patterns when stroking"
            << "\n\tshift-x: toggle dash pattern scaling with zoom under pixel width stroking"
            << "\n\tv: toggle between stroking via arcs or quadratic curves"
            << "\n\tn/m: decrease/increate miter limit"
            << "\n\talt-n: toggle using mipmaps for blur generation"
            << "\n\tg: cycle through different ways to use the offscreen mask for filling"
            << "\n\tt: cycle through different ways to use the offscreen mask for stroking"
            << "\n\treturn + up/down: increase/decrease render fill scale factor"
            << "\n\talt + 1,2, ... 9: set render fill scale factor to 10%, 20%, ..., 90% repsectively"
            << "\n\talt + 0: set render fill scale factor to 100%"
            << "\n\tup/down arrow: increase/decrease opacity"
            << "\n\t6: increase horizontal pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\tctrl-6: decrease horizontal pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\t7: increase vertical pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\tctrl-7: decrease vertical pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + 6: increase horizontal post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + ctrl-6: decrease horizontal post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + 7: increase vertical post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + ctrl-7: decrease vertical post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\t9/0 increase/decrease angle of rotation"
            << "\n\tctrl-9/ctrl-0 increase/decrease graph stroke width"
            << "\n\tctrl-1/ctrl-2 increase/decrease graph stroke spacing"
            << "\n\t[/] : decrease/incrase stroking width"
            << "\n\tleft/right : decrease/increase blur radius"
            << "\n\talt-left/alt-right: decreate/increase max sample window for blur"
            << "\n\tctrl-[/ctrl-] : decrease/incrase stroking dash corner radius"
            << "\n\tw: change dash pattern adjust mode"
            << "\n\tctrl-w: toggle adjust dash pattern's draw lengths"
            << "\n\tshift-w: toggle adjust dash pattern's skip lengths"
            << "\n\tLeft Mouse Drag: pan"
            << "\n\tHold Left Mouse, then drag up/down: zoom out/in"
            << "\n";
}

void
PathTest::
reset_zoom_transformation(void)
{
  /* set the camera so that the middle of the path's
   * bounding box is in the middle of the window
   */
  astral::BoundingBox<float> bb;

  bb = m_path.bounding_box();
  if (!bb.empty())
    {
      astral::ivec2 wh(dimensions());
      astral::vec2 screen_pt(wh.x() / 2, wh.y() / 2), path_pt;
      UniformScaleTranslate<float> tr;

      path_pt = (bb.min_point() + bb.max_point()) * 0.5f;
      tr.m_translation = screen_pt - path_pt;
      m_zoom.transformation(tr);
    }
  else
    {
      m_zoom.transformation(UniformScaleTranslate<float>());
    }
}

bool
PathTest::
load_path(const std::string &filename, astral::Path *dst, PathCommand *dst_cmd)
{
  std::ifstream path_file(filename.c_str());
  if (path_file)
    {
      read_path(dst, path_file, dst_cmd);

      /* Only santize if animation style is not manual. This
       * is because with manual animation style, the caller
       * is expecting that contours in the file are completely
       * preserved.
       */
      if (m_animation_style.value() != manual_animation_generation)
        {
          dst->sanitize();
        }
      return true;
    }

  return false;
}

void
PathTest::
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
PathTest::
init_gl(int w, int h)
{
  PathCommand st, ed;

  reset_zoom_transformation();
  m_prev_stats.resize(renderer().stats_labels().size(), 0);

  float pixel_size(32.0f);
  astral::Font font(default_typeface(), pixel_size);
  m_text_item = astral::TextItem::create(font);
  m_bg_text_item = astral::TextItem::create(font);

  std::ostringstream bg_text;
  bg_text << "Some wonderful background text";
  add_text(0.0f, bg_text.str(), *m_bg_text_item);

  if (!load_path(m_path_file.value(), &m_path, &st))
    {
      const char *default_path =
        "[ (50.0, 35.0) [[(60.0, 50.0) ]] (70.0, 35.0)\n"
        "arc 180 (70.0, -100.0)\n"
        "[[ (60.0, -150.0) (30.0, -50.0) ]]\n"
        "(0.0, -100.0) arc 90 ]\n"
        "{ (200, 200) (400, 200) (400, 400) (200, 400) }\n"
        "[ (-50, 100) (0, 200) (100, 300) (150, 325) (150, 100) ]\n"
        "{ (300 300) }\n";

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

  if (!m_image_file.value().empty())
    {
      astral::reference_counted_ptr<ImageLoader> pixels = ImageLoader::create(m_image_file.value());
      astral::uvec2 image_dims;

      image_dims = pixels->dimensions();
      if (pixels->non_empty())
        {
          std::cout << "Loaded image from file \""
                    << m_image_file.value() << "\"\n";
          m_image = engine().image_atlas().create_image(image_dims);
          for (unsigned int mip = 0, w = image_dims.x(), h = image_dims.y();
               w > 0u && h > 0u && mip < m_image->number_mipmap_levels();
               w >>= 1u, h >>= 1u, ++mip)
            {
              m_image->set_pixels(mip,
                                  astral::ivec2(0, 0),
                                  astral::ivec2(w, h),
                                  w,
                                  pixels->mipmap_pixels(mip));
            }
        }
    }

  for (const auto &e : m_dash_pattern_files.elements())
    {
      m_dash_patterns.push_back(astral::StrokeShader::DashPattern());
      for (const auto &v : e.m_loaded_value)
        {
          m_dash_patterns.back().add(v);
        }
    }

  if (m_dash_patterns.empty())
    {
      m_dash_patterns.push_back(astral::StrokeShader::DashPattern());
      m_dash_patterns.back()
        .add(astral::StrokeShader::DashPatternElement(55.0f, 20.0f))
        .add(astral::StrokeShader::DashPatternElement(0.0f, 20.0f))
        .add(astral::StrokeShader::DashPatternElement(0.0f, 20.0f))
        ;

      m_dash_patterns.push_back(astral::StrokeShader::DashPattern());
      m_dash_patterns.back()
        .add(astral::StrokeShader::DashPatternElement(55.0f, 20.0f))
        .add(astral::StrokeShader::DashPatternElement(5.0f, 20.0f))
        .add(astral::StrokeShader::DashPatternElement(15.0f, 20.0f))
        ;
    }

  for (auto &v : m_dash_patterns)
    {
      if (m_dash_pattern_start_offset.set_by_command_line())
        {
          v.dash_start_offset(m_dash_pattern_start_offset.value());
        }

      if (m_dash_pattern_draw_lengths_adjusted.set_by_command_line())
        {
          v.draw_lengths_adjusted(m_dash_pattern_draw_lengths_adjusted.value());
        }

      if (m_dash_pattern_skip_lengths_adjusted.set_by_command_line())
        {
          v.skip_lengths_adjusted(m_dash_pattern_skip_lengths_adjusted.value());
        }

      if (m_dash_pattern_adjust_mode.set_by_command_line())
        {
          v.adjust_mode(m_dash_pattern_adjust_mode.value());
        }

      if (m_dash_pattern_corner_radius.set_by_command_line())
        {
          v.dash_corner_radius(m_dash_pattern_corner_radius.value());
        }

      if (m_dash_pattern_per_edge.set_by_command_line())
        {
          v.dash_pattern_per_edge(m_dash_pattern_per_edge.value());
        }
    }

  if (!m_gradient_p1.set_by_command_line())
    {
      m_gradient_p1.value() = astral::vec2(w, h);
    }

  if (!m_gradient_r0.set_by_command_line())
    {
      m_gradient_r0.value() = astral::t_max(w, h);
    }

  if (!m_gradient_r1.set_by_command_line())
    {
      m_gradient_r1.value() = astral::t_max(w, h);
    }

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

  astral::c_array<const astral::ColorStop<astral::FixedPointColor_sRGB>> colorstops_carray(make_c_array(colorstops));
  m_colorstop_sequence = engine().colorstop_sequence_atlas().create(colorstops_carray);

  generate_stroke_shaders();

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

  /* produces a render crack on M1 with demo_data/paths/arc1.txt
   * when generating a sparse mask. The crack is on different
   * sides of the primitive depending on if hw clip planes are
   * on or or not.
   */
  if (0)
    {
      UniformScaleTranslate<float> tr;
      astral::generic_data V;

      V.u = 0x3f800000u;
      tr.m_scale = V.f;

      V.u = 0x44960000u;
      tr.m_translation.x() = V.f;

      V.u = 0x44120000u;
      tr.m_translation.y() = V.f;

      V.u = 0x43839eacu;
      m_stroke_params.m_width = V.f;

      m_zoom.transformation(tr);
    }
}

void
PathTest::
generate_stroke_shaders(void)
{
  const auto &gl3_stroke_shaders(engine().gl3_shaders().m_mask_stroke_shaders);

  WavyStrokeShaderGenerator wavy_generator(engine());
  wavy_generator.generate_stroke_shader(gl3_stroke_shaders[astral::gl::ShaderSetGL3::distance_stroking], m_stroke_shaders[stroke_wavy]);
  wavy_generator.generate_stroke_shader(gl3_stroke_shaders[astral::gl::ShaderSetGL3::dashed_stroking], m_dashed_stroke_shaders[stroke_wavy]);

  GraphStrokeShaderGenerator graph_generator(engine());
  graph_generator.generate_stroke_shader(gl3_stroke_shaders[astral::gl::ShaderSetGL3::distance_stroking], m_stroke_shaders[stroke_graph]);
  graph_generator.generate_stroke_shader(gl3_stroke_shaders[astral::gl::ShaderSetGL3::dashed_stroking], m_dashed_stroke_shaders[stroke_graph]);

  WavyGraphStrokeShaderGenerator wavy_graph_generator(engine());
  wavy_graph_generator.generate_stroke_shader(gl3_stroke_shaders[astral::gl::ShaderSetGL3::distance_stroking], m_stroke_shaders[stroke_wavy_graph]);
  wavy_graph_generator.generate_stroke_shader(gl3_stroke_shaders[astral::gl::ShaderSetGL3::dashed_stroking], m_dashed_stroke_shaders[stroke_wavy_graph]);

  m_stroke_shaders[stroke_vanilla] = engine().default_shaders().m_mask_stroke_shader;
  m_dashed_stroke_shaders[stroke_vanilla] = engine().default_shaders().m_mask_dashed_stroke_shader;
}

float
PathTest::
compute_animation_interpolate(void)
{
  int32_t ms;
  float t;

  ms = m_path_time.value().elapsed() % (2 * m_animation_time.value());
  t = static_cast<float>(ms) / m_animation_time.value();
  t = astral::t_min(2.0f, astral::t_max(0.0f, t));
  t = (t > 1.0f) ? 2.0f - t : t;
  t = astral::t_min(1.0f, astral::t_max(0.0f, t));

  return t;
}

void
PathTest::
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
      */
      astral::Renderer::number_emulate_framebuffer_fetches,
      astral::Renderer::number_virtual_buffer_pixels,
      astral::Renderer::number_non_degenerate_virtual_buffers,
      astral::Renderer::number_non_degenerate_color_virtual_buffers,
      astral::Renderer::number_non_degenerate_mask_virtual_buffers,
      astral::Renderer::number_non_degenerate_shadowmap_virtual_buffers,
      astral::Renderer::number_offscreen_render_targets,
      astral::Renderer::number_vertices_streamed,
      astral::Renderer::number_static_u32vec4_streamed,
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

  hud_text << "Resolution = " << dimensions() << "\n"
           << "Zoom = " << m_zoom.transformation().m_scale
           << ", Translation = " << m_zoom.transformation().m_translation
           << "\nHud Level: " << m_hud_mode << " [space]\n"
           << "Average over " << m_frame_time_average.interval_ms() << " ms: "
           << m_frame_time_average.average_elapsed_ms()
           << m_frame_time_average.parity_string() << "\n"
           << "Number commands copied: " << m_prev_stats[astral::Renderer::number_commands_copied]
           << "\n";

  if (m_hud_mode >= basic_hud_with_commands)
    {
      hud_text << "Render Accuracy: " << renderer().default_render_accuracy() << " [z]\n"
               << "Draw mode:" << label(m_path_mode.value()) << " [d]\n"
               << "ShowOffscreenAllocation: " << std::boolalpha
               << m_show_offscreen_alloc_info << " [alt-space]\n"
               << "Animation paused: " << m_path_time.value().paused() << " [p]\n"
               << "DrawBackgroundText: " << m_add_some_background_text.value() << " [alt-b]\n"
               << "StrokeMode: "<< label(m_stroke_mode.value()) << " [s]\n";

      if (m_stroke_mode.value() != stroke_none)
        {
          hud_text << "\tSparse: " << std::boolalpha
                   << m_mask_stroke_params.m_sparse_mask << " [o]\n"
                   << "\tStrokingWidth: " << m_stroke_params.m_width
                   << "{change with [ and ]}\n"
                   << "\tStrokingWidthInPixels: " << std::boolalpha
                   << m_stroke_width_pixels.value() << "[shift-p]\n"
                   << "\tGracefulThinStroking: " << std::boolalpha
                   << m_graceful_thin_stroking.value() << "[ctrl-p]\n"
                   << "\tMaskType: " << astral::label(m_mask_stroke_usage_params.m_mask_type) << " [t]\n"
                   << "\tDraw Edges: " << std::boolalpha << m_stroke_params.m_draw_edges << " [e]\n"
                   << "\tCap Style: " << astral::label(m_stroke_params.m_cap) << " [c]\n"
                   << "\tJoin Style: " << astral::label(m_stroke_params.m_join) << " [j]\n"
                   << "\tGlueJoin: " << astral::label(m_stroke_params.m_glue_join) << " [alt-j]\n"
                   << "\tGlueCuspJoin: " << astral::label(m_stroke_params.m_glue_cusp_join) << " [ctrl-j]\n"
                   << "\tMitit Style: " << ((m_stroke_params.m_miter_clip) ? "miter-clip" : "miter-cull") << " [ctrl-m]\n";

          if (m_stroke_params.m_join == astral::join_miter)
            {
              hud_text << "\t\tMiterLimit: " << m_stroke_params.m_miter_limit
                       << " [n/m]\n";
            }

          if (m_dash_pattern_choice.value() == 0)
            {
              hud_text << "\tNo DashPattern applied [x]\n";
            }
          else
            {
              const auto &v(m_dash_patterns[m_dash_pattern_choice.value() - 1u]);

              hud_text << "\tDashPattern #" << m_dash_pattern_choice.value() << " [x]\n"
                       << "\tAdjust mode: " << astral::label(v.adjust_mode()) << " [w]\n"
                       << "\t\tdash-offset: " << v.dash_start_offset() << " [alt-[]]\n"
                       << "\t\tdash-corner_radius = " << v.dash_corner_radius() << " [ctrl-[]]\n"
                       << "\t\tdash-per-edge = " << v.dash_pattern_per_edge() << " [ctrl-shift-w]\n"
                       << "\t\tadjust_draw_lengths = " << v.draw_lengths_adjusted() << " [ctrl-w]\n"
                       << "\t\tadjust_skip_lengths = " << v.skip_lengths_adjusted() << " [shift-w]\n";
            }
        }

      if (m_fill_params.m_fill_rule == astral::number_fill_rule)
        {
          hud_text << "Filling Off [r]\n";
        }
      else
        {
          hud_text << "Filling: " << astral::label(m_fill_params.m_fill_rule) << " [r]\n"
                   << "\tSparse: " << astral::label(m_mask_fill_params.m_sparse_mask) << " [shift-r]\n"
                   << "\tDisplayMethod: " << label(m_display_fill_method.value()) << " [f]\n"
                   << "\tAnti-alias: " << astral::label(m_fill_params.m_aa_mode) << " [a]\n"
                   << "\tMaskType: " << astral::label(m_mask_fill_usage_params.m_mask_type) << " [g]\n"
                   << "\tBlurMode: " << label(m_background_blur_mode.value()) << " [b]\n";

          if (m_background_blur_mode.value() != no_backgroud_blur)
            {
              hud_text << "\t\tBlur Radius: "<< m_background_blur_radius.value() << " [left/right arrow]\n"
                       << "\t\tBlur SampleRadius: " << m_blur_max_sample_radius.value() << " [alt-left/alt-right arrow]\n"
                       << "\t\tBlur BlurMinScaleFactor: "<< m_blur_min_scale_factor.value() << " [v]\n";
            }
        }

      if (m_fill_params.m_fill_rule != astral::number_fill_rule
          || m_stroke_mode.value() != stroke_none)
        {
          hud_text << "Opacity: " << m_alpha.value() << " [up/down]\n"
                   << "Blend mode: " << astral::label(m_blend_mode.value()) << " [ctrl-b]\n"
                   << "MaskScaleFactor:" << m_scale_factor.value() << " [alt-0, alt-1, ..., alt-9], [return + up/down]\n"
                   << "MaskFilter: " << astral::label(m_mask_filter.value()) << " [k]\n";

          if (m_gradient_type.value() == astral::Gradient::number_types)
            {
              hud_text << "No Gradient [ctrl-g]\n";
            }
          else
            {
              hud_text << "Gradient: "<< astral::label(m_gradient_type.value())
                       << " [ctrol-g]\n";
            }
        }
    }

  astral::ivec2 mouse_pos;
  astral::vec2 mouse_logical_pos;
  const astral::Path *path;

  GetMouseState(&mouse_pos.x(), &mouse_pos.y());
  mouse_logical_pos = encoder.transformation().inverse().apply_to_point(astral::vec2(mouse_pos));

  hud_text << "Mouse at " << mouse_pos << mouse_logical_pos << "\n";

  switch (m_path_mode.value())
    {
    case animated_path_at_0:
    case t0_path:
      path = &m_path;
      break;

    case animated_path_at_1:
    case t1_path:
      path = &m_path;
      break;

    default:
      path = nullptr;
    }

  if (path)
    {
      float tol;
      astral::Path::PointQueryResult Q;

      tol = encoder.compute_tolerance();
      Q = path->distance_to_path(tol, mouse_logical_pos);

      if (Q.m_closest_contour >= 0)
        {
          astral::vec2 pt;

          hud_text << "Winding = " << Q.m_winding_impact << "\n"
                   << "Distance to Path = " << Q.m_distance << "\n"
                   << "Contour #" << Q.m_closest_contour << ", "
                   << "Curve #" << Q.m_closest_curve << ":"
                   << path->contour(Q.m_closest_contour).curve(Q.m_closest_curve)
                   << "\n";

          pt = path->contour(Q.m_closest_contour).curve(Q.m_closest_curve).eval_at(Q.m_closest_point_t);
          hud_text << "@" << Q.m_closest_point_t << " --> " << pt << "\n";
        }
    }

  /* draw the HUD in fixed location */
  encoder.transformation(astral::Transformation());
  set_and_draw_hud(encoder, frame_ms,
                   astral::make_c_array(m_prev_stats),
                   *m_text_item, hud_text.str(),
                   vs_p, bvs_p, gvs_p);
}

astral::Gradient
PathTest::
generate_gradient(void)
{
  switch (m_gradient_type.value())
    {
    default:
      ASTRALassert(!"Bad gradient type enumeration");
      // fall through

    case astral::Gradient::linear:
      return astral::Gradient(m_colorstop_sequence,
                              m_gradient_p0.value(), m_gradient_p1.value(),
                              m_gradient_tile_mode.value());

    case astral::Gradient::radial_unextended_opaque:
    case astral::Gradient::radial_unextended_clear:
    case astral::Gradient::radial_extended:
      return astral::Gradient(m_colorstop_sequence,
                              m_gradient_p0.value(), m_gradient_r0.value(),
                              m_gradient_p1.value(), m_gradient_r1.value(),
                              m_gradient_tile_mode.value(),
                              astral::Gradient::gradient_extension_type(m_gradient_type.value()));

    case astral::Gradient::sweep:
      {
        float angle;
        astral::vec2 v;

        v = m_gradient_p1.value() - m_gradient_p0.value();
        angle = astral::t_atan2(v.y(), v.x());
        return astral::Gradient(m_colorstop_sequence,
                                m_gradient_p0.value(), angle,
                                m_gradient_sweep_factor.value(),
                                m_gradient_tile_mode.value());
      }
    }
}

void
PathTest::
draw_frame(void)
{
  float frame_ms;
  astral::Transformation tr;
  astral::ivec2 dims(dimensions());
  astral::RenderEncoderSurface render_encoder;
  astral::Transformation out_mask_transformation_logical;
  astral::RectT<int> out_pixel_rect;
  astral::BoundingBox<float> out_logical_rect;
  astral::c_array<const unsigned int> stats;
  astral::c_array<const astral::c_string> stats_labels(renderer().stats_labels());

  m_frame_time_average.increment_counter();
  frame_ms = update_smooth_values();
  tr = m_zoom.transformation().astral_transformation();

  tr.scale(m_scale_pre_rotate.value());
  tr.rotate(m_rotate_angle.value() * (ASTRAL_PI / 180.0f));
  tr.scale(m_scale_post_rotate.value());

  render_encoder = renderer().begin(render_target(),
                                    astral::FixedPointColor<astral::colorspace_srgb>(0u, 0u, 0u, 255u));

  render_encoder.use_sub_ubers(m_use_sub_ubers.value());

  if (m_image)
    {
      astral::Brush brush;
      astral::RenderValue<astral::ImageSampler> im;
      astral::RenderValue<astral::Brush> br;
      astral::vec2 target_sz(dims);
      astral::vec2 src_sz(m_image->size());
      astral::ImageSampler image(*m_image, astral::filter_nearest, astral::mipmap_none);

      im = render_encoder.create_value(image);

      brush.image(im);
      br = render_encoder.create_value(brush);

      render_encoder.save_transformation();
      render_encoder.scale(target_sz / src_sz);
      render_encoder.draw_rect(astral::Rect().size(src_sz), br);
      render_encoder.restore_transformation();
    }

  if (m_add_some_background_text.value())
    {
      const astral::BoundingBox<float> &bb(m_bg_text_item->bounding_box());
      astral::vec2 bb_sz(bb.size());
      astral::RenderEncoderBase::AutoRestore restore(render_encoder);
      astral::RenderValue<astral::Brush> black, white;

      white = render_encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 1.0f, 0.5f)));
      black = render_encoder.create_value(astral::Brush().base_color(astral::vec4(0.0f, 0.0f, 0.0f, 1.0f)));

      /* we want the middle of the bb to be at the middle of the screen */
      render_encoder.translate(dims.x() * 0.5f - 0.5f * bb_sz.x(), dims.y() * 0.5f + bb_sz.y());

      astral::BoundingBox<float> bigger_bb(bb);
      float sz(m_bg_text_item->font().pixel_size());
      bigger_bb.enlarge(astral::vec2(sz, sz));
      render_encoder.draw_rect(bigger_bb.as_rect(), false, white);

      render_encoder.draw_text(*m_bg_text_item, black);
    }

  render_encoder.transformation(tr);

  astral::RenderEncoderBase *r;
  astral::RenderEncoderLayer render_encoder_layer;
  astral::RenderEncoderBase render_encoder_image;
  float alpha, t;
  enum astral::blend_mode_t blend_mode;
  enum astral::filter_t mask_filter;
  astral::CombinedPath drawn_path;
  astral::RenderValue<astral::Brush> fill_brush, stroke_brush;

  t = compute_animation_interpolate();

  switch (m_path_mode.value())
    {
    default:
      t = 0.0f;
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

  switch (m_path_mode.value())
    {
    case number_path_modes:
      ASTRALfailure("Invalid DrawMode");
      // fall through

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
    }

  /* This must come first because if blur is involved it wants
   * the "pixels" at this current point in time. If we asked
   * for the pixels after encoder_layer() was called, then the
   * pixels would include the contents of the layer which would
   * be a feedback loop; astral::Renderer prvents such feedback
   * loops because it would astral::RenderEncoderBase::end()
   * the obejct returned by encoder_layer() which then means attempting
   * to draw to it would assert.
   */
  astral::Brush brush;
  astral::vec4 brush_color(1.0f, 1.0f, 1.0f, 1.0f);

  if (m_gradient_type.value() != astral::Gradient::number_types)
    {
      brush
        .gradient(render_encoder.create_value(generate_gradient()));
    }
  else if (m_gradient_type.value() == astral::Gradient::number_types)
    {
      brush_color = astral::vec4(0.0f, 0.5f, 1.0f, 1.0f);
    }

  if (m_render_to_layer.value())
    {
      render_encoder_layer = render_encoder.begin_layer(drawn_path.compute_bounding_box(),
                                                        astral::vec2(m_scale_factor.value()),
                                                        astral::vec4(1.0f, 1.0f, 1.0f, m_alpha.value()),
                                                        m_blend_mode.value(),
                                                        m_mask_filter.value());

      render_encoder_image = render_encoder_layer.encoder();
      r = &render_encoder_image;
      alpha = 1.0f;
      blend_mode = astral::blend_porter_duff_src_over;
      mask_filter = astral::filter_nearest;
      m_mask_fill_params.render_scale_factor(1.0f);
      m_mask_stroke_params.render_scale_factor(1.0f);
    }
  else
    {
      r = &render_encoder;
      alpha = m_alpha.value();
      blend_mode = m_blend_mode.value();
      m_mask_fill_params.render_scale_factor(m_scale_factor.value());
      m_mask_stroke_params.render_scale_factor(m_scale_factor.value());
      mask_filter = m_mask_filter.value();
    }

  m_mask_fill_usage_params.m_filter = mask_filter;
  m_mask_stroke_usage_params.m_filter = mask_filter;

  brush_color.w() = alpha;
  brush.m_base_color = brush_color;
  fill_brush = render_encoder.create_value(brush);
  stroke_brush = r->create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 1.0f, alpha)));

  if (m_swap_fill_and_stroke_brush.value())
    {
      std::swap(fill_brush, stroke_brush);
    }

  if (m_fill_params.m_fill_rule != astral::number_fill_rule)
    {
      astral::ItemMaterial fill_material(fill_brush);

      if (m_background_blur_mode.value() != no_backgroud_blur)
        {
          const astral::Effect *effect;
          astral::GaussianBlurParameters effect_params;
          astral::EffectMaterial effect_material;

          effect_params.radius(m_background_blur_radius.value());
          effect_params.blur_radius_in_local_coordinates(m_background_blur_mode.value() == background_blur_radius_relative_to_path);

          effect_params.min_render_scale(m_blur_min_scale_factor.value());
          effect_params.max_sample_radius(m_blur_max_sample_radius.value());
          effect_params.post_sampling_mode(astral::color_post_sampling_mode_rgb_direct_alpha_one);
          effect_params.color_modulation_alpha(alpha);
          effect = render_encoder.default_effects().m_gaussian_blur.get();

          /* we want the pixels from the starting encoder surface */
          render_encoder.snapshot_effect(*effect, effect_params.effect_parameters(),
                                         drawn_path.compute_bounding_box(),
                                         &effect_material);

          fill_material.m_material = effect_material.m_material;
          fill_material.m_material_transformation_logical = render_encoder.create_value(effect_material.m_material_transformation_rect);
        }

      switch (m_display_fill_method.value())
        {
        default:
        case display_fill_directly:
          {
            r->fill_paths(drawn_path, m_fill_params,
                          fill_material, blend_mode,
                          m_mask_fill_usage_params,
                          m_mask_fill_params);
          }
          break;

        case display_fill_with_item_mask:
          {
            astral::MaskDetails path_data;
            astral::reference_counted_ptr<const astral::RenderClipElement> clip_element;

            r->generate_mask(drawn_path,
                             m_fill_params,
                             m_mask_fill_params,
                             m_mask_fill_usage_params.m_mask_type,
                             &path_data, &clip_element);
            if (clip_element->mask_details())
              {
                r->draw_mask(path_data, m_mask_filter.value(), fill_material, blend_mode);
              }
          }
          break;

        case display_fill_with_render_clip:
          {
            astral::MaskDetails path_data;
            astral::reference_counted_ptr<const astral::RenderClipElement> clip_element;

            r->generate_mask(drawn_path,
                             m_fill_params, m_mask_fill_params,
                             m_mask_fill_usage_params.m_mask_type,
                             &path_data, &clip_element);
            if (clip_element->mask_details())
              {
                astral::BoundingBox<float> bb(drawn_path.compute_bounding_box());
                astral::ItemMask mask(clip_element, m_mask_filter.value(), false);
                astral::ItemMaterial material(fill_material, mask);

                r->draw_rect(bb.as_rect(), false, material, blend_mode);
              }
          }
          break;

        case display_fill_with_render_clip_complement:
          {
            astral::MaskDetails path_data;
            astral::BoundingBox<float> bb(drawn_path.compute_bounding_box());
            astral::reference_counted_ptr<const astral::RenderClipElement> clip_element;

            r->generate_mask(drawn_path,
                             m_fill_params, m_mask_fill_params,
                             m_mask_fill_usage_params.m_mask_type,
                             &path_data, &clip_element);
            if (clip_element->mask_details())
              {
                astral::ItemMask mask(clip_element, m_mask_filter.value(), true);
                astral::ItemMaterial material(fill_material, mask);

                r->draw_rect(bb.as_rect(), false, material, blend_mode);
              }
            else
              {
                r->draw_rect(bb.as_rect(), false, fill_material);
              }
          }
          break;

        case display_fill_with_clip:
          {
            astral::RenderClipNode clip_encoders;
            astral::BoundingBox<float> bb(drawn_path.compute_bounding_box());

            clip_encoders = r->begin_clip_node_logical(astral::RenderEncoderBase::clip_node_clip_in,
                                                       drawn_path,
                                                       m_fill_params,
                                                       m_mask_fill_params,
                                                 m_mask_fill_usage_params);
            clip_encoders.clip_in().draw_rect(bb.as_rect(), false, fill_material, blend_mode);
            r->end_clip_node(clip_encoders);
          }
          break;

        case display_fill_test_clip_in_clip_out:
          {
            astral::RenderClipNode clip_encoders;
            astral::BoundingBox<float> bb(drawn_path.compute_bounding_box());

            clip_encoders = r->begin_clip_node_logical(astral::RenderEncoderBase::clip_node_both,
                                                       drawn_path,
                                                       m_fill_params,
                                                       m_mask_fill_params,
                                                       m_mask_fill_usage_params);
            clip_encoders.clip_in().draw_rect(bb.as_rect(), false, fill_material, blend_mode);
            clip_encoders.clip_out().draw_rect(bb.as_rect(), false,
                                               r->create_value(astral::Brush().base_color(astral::vec4(1.0, 0.5, 1.0, alpha))),
                                               blend_mode);
            r->end_clip_node(clip_encoders);
          }
          break;

        case display_fill_item_path:
          {
            float tol;
            const astral::ItemPath *item_path;

            tol = astral::RelativeThreshhold(1e-3).absolute_threshhold(m_path.bounding_box());
            item_path = &m_path.item_path(tol);

            r->draw_item_path(astral::ItemPath::Layer(*item_path)
                                .fill_rule(m_fill_params.m_fill_rule)
                                .color(astral::vec4(1.0f, 0.0f, 1.0f, 1.0f)));
            if (m_print_item_path_text)
              {
                const auto &props(item_path->properties());

                m_print_item_path_text = false;
                std::cout << "ItemPath stats:\n"
                          << "\tRender costs = " << props.m_average_render_cost << "\n"
                          << "\tNumber bands = " << props.m_number_bands << "\n"
                          << "\tFP16 data size = " << sizeof(astral::u16vec4) * props.m_fp16_data_size << " Bytes\n"
                          << "\tG32 data size = " << sizeof(astral::gvec4) * props.m_generic_data_size << " Bytes\n";
              }
          }
          break;
        }
    }

  r->save_transformation();
    {
      float distance_scale_factor(1.0f);
      astral::MaskUsage use_mask(m_mask_stroke_usage_params);
      astral::StrokeParameters use_params(m_stroke_params);

      if (m_stroke_width_pixels.value())
        {
          astral::Transformation current_tr(r->transformation());

          if (!m_scale_dash_pattern_on_pixel_width_stroking.value())
            {
              float det;

              det = astral::compute_determinant(current_tr.m_matrix);
              distance_scale_factor = astral::t_sqrt(astral::t_abs(det));
            }

          r->transformation(astral::Transformation());
          switch (m_path_mode.value())
            {
            case number_path_modes:
              ASTRALfailure("Invalid DrawMode");
              // fall through

            case animated_path:
            case animated_path_at_0:
            case animated_path_at_1:
              drawn_path = astral::CombinedPath(t, m_animated_path, current_tr.m_translate, current_tr.m_matrix);
              break;

            case t0_path:
              drawn_path = astral::CombinedPath(m_path, current_tr.m_translate, current_tr.m_matrix);
              break;

            case t1_path:
              drawn_path = astral::CombinedPath(m_end_path, current_tr.m_translate, current_tr.m_matrix);
              break;
            }
        }

      if (m_graceful_thin_stroking.value())
        {
          use_params.graceful_thin_stroking(true);
          if (use_params.m_width <= 0.0f)
            {
              use_mask.m_mask_type = astral::mask_type_coverage;
            }
        }

      switch (m_stroke_mode.value())
        {
        case stroke_wavy:
          {
            WavyPattern wavy;

            compute_wavy_value(distance_scale_factor, wavy);
            if (m_dash_pattern_choice.value() == 0u)
              {
                astral::StrokeShader::ItemDataPacker base;
                r->stroke_paths(*m_stroke_shaders[m_stroke_mode.value()],
                                drawn_path, use_params,
                                WavyStrokeItemDataPacker(wavy, base),
                                stroke_brush, blend_mode,
                                use_mask,
                                m_mask_stroke_params);
              }
            else
              {
                m_dash_patterns[m_dash_pattern_choice.value() - 1u].scale_factor(distance_scale_factor);
                r->stroke_paths(*m_dashed_stroke_shaders[m_stroke_mode.value()],
                                drawn_path, use_params,
                                WavyStrokeItemDataPacker(wavy, m_dash_patterns[m_dash_pattern_choice.value() - 1u]),
                                stroke_brush, blend_mode, use_mask,
                                m_mask_stroke_params);
              }
          }
          break;

        case stroke_graph:
          {
            GraphPattern G;

            compute_graph_value(G);
            if (m_dash_pattern_choice.value() == 0u)
              {
                astral::StrokeShader::ItemDataPacker base;
                r->stroke_paths(*m_stroke_shaders[m_stroke_mode.value()],
                                drawn_path, use_params,
                                GraphStrokeItemDataPacker(G, base),
                                stroke_brush, blend_mode,
                                use_mask,
                                m_mask_stroke_params);
              }
            else
              {
                m_dash_patterns[m_dash_pattern_choice.value() - 1u].scale_factor(distance_scale_factor);
                r->stroke_paths(*m_dashed_stroke_shaders[m_stroke_mode.value()],
                                drawn_path, use_params,
                                GraphStrokeItemDataPacker(G, m_dash_patterns[m_dash_pattern_choice.value() - 1u]),
                                stroke_brush, blend_mode, use_mask,
                                m_mask_stroke_params);
              }
          }
          break;

        case stroke_wavy_graph:
          {
            astral::StrokeShader::ItemDataPacker base;
            const astral::StrokeShader::ItemDataPacker *p;

            if (m_dash_pattern_choice.value() == 0u)
              {
                p = &base;
              }
            else
              {
                m_dash_patterns[m_dash_pattern_choice.value() - 1u].scale_factor(distance_scale_factor);
                p = &m_dash_patterns[m_dash_pattern_choice.value() - 1u];
              }

            GraphPattern G;
            WavyPattern wavy;

            compute_wavy_value(distance_scale_factor, wavy);
            compute_graph_value(G);

            GraphStrokeItemDataPacker graph_packer(G, *p);
            WavyGraphStrokeItemDataPacker wavy_graph_packer(wavy, graph_packer);

            if (m_dash_pattern_choice.value() == 0u)
              {
                r->stroke_paths(*m_stroke_shaders[m_stroke_mode.value()],
                                drawn_path, use_params,
                                wavy_graph_packer,
                                stroke_brush, blend_mode,
                                use_mask,
                                m_mask_stroke_params);
              }
            else
              {
                r->stroke_paths(*m_dashed_stroke_shaders[m_stroke_mode.value()],
                                drawn_path, use_params,
                                wavy_graph_packer,
                                stroke_brush, blend_mode,
                                use_mask,
                                m_mask_stroke_params);
              }

          }
          break;

        case stroke_vanilla:
          {
            if (m_dash_pattern_choice.value() == 0u)
              {
                r->stroke_paths(drawn_path, use_params,
                                stroke_brush, blend_mode,
                                use_mask,
                                m_mask_stroke_params);
              }
            else
              {
                m_dash_patterns[m_dash_pattern_choice.value() - 1u].scale_factor(distance_scale_factor);
                r->stroke_paths(drawn_path, use_params,
                                m_dash_patterns[m_dash_pattern_choice.value() - 1u],
                                stroke_brush, blend_mode,
                                use_mask,
                                m_mask_stroke_params);
              }
          }
          break;

        case stroke_direct:
          {
            if (m_dash_pattern_choice.value() == 0u)
              {
                r->direct_stroke_paths(drawn_path, use_params,
                                       stroke_brush, blend_mode);
              }
            else
              {
                m_dash_patterns[m_dash_pattern_choice.value() - 1u].scale_factor(distance_scale_factor);
                r->direct_stroke_paths(drawn_path, use_params,
                                       m_dash_patterns[m_dash_pattern_choice.value() - 1u],
                                       stroke_brush, blend_mode);
              }
          }
          break;

        case stroke_direct_with_mask:
          {
            astral::RenderEncoderStrokeMask stroke_mask_generator;

            stroke_mask_generator = r->encoder_stroke(m_mask_stroke_params);
            stroke_mask_generator.set_stroke_params(use_params, 0.0f);
            stroke_mask_generator.add_path(drawn_path);
            stroke_mask_generator.finish();

            astral::ItemMask item_mask(stroke_mask_generator.clip_element(use_mask.m_mask_type),
                                       use_mask.m_filter);
            astral::ItemMaterial material(stroke_brush, item_mask);

            if (m_dash_pattern_choice.value() == 0u)
              {
                r->direct_stroke_paths(drawn_path, use_params,
                                       material, blend_mode);
              }
            else
              {
                m_dash_patterns[m_dash_pattern_choice.value() - 1u].scale_factor(distance_scale_factor);
                r->direct_stroke_paths(drawn_path, use_params,
                                       m_dash_patterns[m_dash_pattern_choice.value() - 1u],
                                       material, blend_mode);
              }
          }
          break;

        case stroke_clip_in_cutoff:
        case stroke_clip_out_cutoff:
        case stroke_clip_union_cutoff:
        case stroke_clip_in_combine:
        case stroke_clip_out_combine:
        case stroke_clip_union_combine:
          if (m_fill_params.m_fill_rule != astral::number_fill_rule)
            {
              astral::reference_counted_ptr<const astral::RenderClipElement> fill_clip;
              astral::MaskDetails fill_mask;
              enum astral::mask_item_shader_clip_mode_t clip_mode;

              /* generate the fill-mask */
              r->generate_mask(drawn_path,
                               m_fill_params,
                               m_mask_fill_params,
                               m_mask_fill_usage_params.m_mask_type,
                               &fill_mask, &fill_clip);

              fill_clip = fill_clip->as_mask_type(astral::mask_type_coverage);

              /* generate the stroking mask but have its generation clipped against fill_clip */
              astral::RenderEncoderStrokeMask stroke_mask_generator;
              astral::MaskDetails stroke_mask;
              astral::ItemMask clip_in(fill_clip, astral::filter_linear, false);
              astral::ItemMask clip_out(fill_clip, astral::filter_linear, true);

              stroke_mask_generator = r->encoder_stroke(m_mask_stroke_params);
              stroke_mask_generator.set_stroke_params(use_params, 0.0f);

              clip_mode = mask_item_shader_clip_mode(m_stroke_mode.value());

              if (stroke_include_clip_out_content(m_stroke_mode.value()))
                {
                  stroke_mask_generator.set_item_clip(clip_in, clip_mode);
                  stroke_mask_generator.add_path(drawn_path);
                }

              if (stroke_include_clip_in_content(m_stroke_mode.value()))
                {
                  stroke_mask_generator.set_item_clip(clip_out, clip_mode);
                  stroke_mask_generator.add_path(drawn_path);
                }
              stroke_mask_generator.finish();

              stroke_mask = stroke_mask_generator.mask_details(use_mask.m_mask_type);

              /* now draw the mask */
              r->draw_mask(stroke_mask, use_mask.m_filter, stroke_brush, blend_mode);
            }
          break;

        default:
          break;
        }
    }
  r->restore_transformation();

  if (m_render_to_layer.value())
    {
      render_encoder.end_layer(render_encoder_layer);
    }

  if (m_show_offscreen_alloc_info)
    {
      draw_offscreen_alloc_hud(astral::vec2(dimensions()),
                               render_encoder,
                               m_offscreen_alloc_info);
    }

  if (!pixel_testing())
    {
      draw_hud(render_encoder, frame_ms);
    }

  stats = renderer().end(&m_offscreen_alloc_info);
  ASTRALassert(m_prev_stats.size() == stats.size());
  std::copy(stats.begin(), stats.end(), m_prev_stats.begin());

  if (m_show_render_stats.value() || m_print_stats)
    {
      m_print_stats = false;
      std::cout << "frame ms = " << frame_ms << "\n"
                << "average over " << m_frame_time_average.interval_ms() << " ms: "
                << m_frame_time_average.average_elapsed_ms()
                << m_frame_time_average.parity_string()
                << "\n"
                << "sparse stroking = " << std::boolalpha << m_mask_stroke_params.m_sparse_mask << "\n";
      for (unsigned int i = 0; i < stats.size(); ++i)
        {
          std::cout << "\t" << stats_labels[i] << " = " << stats[i] << "\n";
        }
    }
}

float
PathTest::
update_smooth_values(void)
{
  const Uint8 *keyboard_state = SDL_GetKeyboardState(nullptr);
  float return_value, delta, scale_delta, angle_delta;
  astral::vec2 *scale_ptr(nullptr);
  astral::c_string scale_txt(nullptr);
  bool alpha_changed(false), scale_factor_changed(false);
  const float alpha_rate(0.001f);
  const float scale_rate(0.0001f);
  const float stroke_rate(0.1f / m_zoom.transformation().m_scale);
  const float graph_stroke_rate(0.0005f);
  const float miter_rate(0.02f);
  float scale_factor_delta(0.0f);
  bool alt_held, ctrl_held;

  ASTRALassert(keyboard_state != nullptr);
  return_value = delta = static_cast<float>(m_draw_timer.restart_us()) * 0.001f;

  alt_held = keyboard_state[SDL_SCANCODE_LALT] || keyboard_state[SDL_SCANCODE_RALT];
  ctrl_held = keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL];

  if (keyboard_state[SDL_SCANCODE_LSHIFT])
    {
      delta *= 0.1f;
    }
  if (keyboard_state[SDL_SCANCODE_RSHIFT])
    {
      delta *= 10.0f;
    }

  if (keyboard_state[SDL_SCANCODE_RIGHTBRACKET])
    {
      if (keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
        {
          if (m_dash_pattern_choice.value() != 0)
            {
              astral::generic_data r;

              r.f = m_dash_patterns[m_dash_pattern_choice.value() - 1u].dash_corner_radius();
              r.f += stroke_rate * delta;
              m_dash_patterns[m_dash_pattern_choice.value() - 1u].dash_corner_radius(r.f);
              std::cout << "DashCornerRadius set to: " << r.f
                        << "(0x" << std::hex << r.u << std::dec << ")\n";
            }
        }
      else if (alt_held)
        {
          if (m_dash_pattern_choice.value() != 0)
            {
              float c;

              c = m_dash_patterns[m_dash_pattern_choice.value() - 1u].dash_start_offset();
              c += stroke_rate * delta;
              m_dash_patterns[m_dash_pattern_choice.value() - 1u].dash_start_offset(c);
              std::cout << "DashStart set to: " << c << "\n";
            }
        }
      else
        {
          m_stroke_params.m_width += stroke_rate * delta;
          std::cout << "Stroke width set to: "
                    << PrintFloatBits(m_stroke_params.m_width)
                    << ", zoom = " << PrintFloatBits(m_zoom.transformation().m_scale)
                    << ", translate = (" << PrintFloatBits(m_zoom.transformation().m_translation.x())
                    << ", " << PrintFloatBits(m_zoom.transformation().m_translation.y()) << ")"
                    << "\n";
        }
    }

  if (keyboard_state[SDL_SCANCODE_LEFTBRACKET] && m_stroke_params.m_width > 0.0f)
    {
      if (keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
        {
          if (m_dash_pattern_choice.value() != 0)
            {
              astral::generic_data r;

              r.f = m_dash_patterns[m_dash_pattern_choice.value() - 1u].dash_corner_radius();
              r.f -= stroke_rate * delta;
              m_dash_patterns[m_dash_pattern_choice.value() - 1u].dash_corner_radius(r.f);
              std::cout << "DashCornerRadius set to: " << r.f
                        << "(0x" << std::hex << r.u << std::dec << ")\n";
            }
        }
      else if (alt_held)
        {
          if (m_dash_pattern_choice.value() != 0)
            {
              float c;

              c = m_dash_patterns[m_dash_pattern_choice.value() - 1u].dash_start_offset();
              c -= stroke_rate * delta;
              m_dash_patterns[m_dash_pattern_choice.value() - 1u].dash_start_offset(c);
              std::cout << "DashStart set to: " << c << "\n";
            }
        }
      else
        {
          m_stroke_params.m_width -= stroke_rate * delta;
          m_stroke_params.m_width = astral::t_max(m_stroke_params.m_width, 0.0f);
          std::cout << "Stroke width set to: "
                    << PrintFloatBits(m_stroke_params.m_width)
                    << ", zoom = " << PrintFloatBits(m_zoom.transformation().m_scale)
                    << ", translate = (" << PrintFloatBits(m_zoom.transformation().m_translation.x())
                    << ", " << PrintFloatBits(m_zoom.transformation().m_translation.y()) << ")"
                    << "\n";
        }
    }

  if (keyboard_state[SDL_SCANCODE_UP])
    {
      if (keyboard_state[SDL_SCANCODE_RETURN])
        {
          scale_factor_delta += delta * scale_rate;
          scale_factor_changed = true;
        }
      else
        {
          m_alpha.value() += delta * alpha_rate;
          alpha_changed = true;
        }
    }

  if (keyboard_state[SDL_SCANCODE_RIGHT])
    {
      if (alt_held)
        {
          m_blur_max_sample_radius.value() += 0.01f * delta;
          std::cout << "Blur Sample Radius set to " << m_blur_max_sample_radius.value()  << "\n";
        }
      else
        {
          m_background_blur_radius.value() += 0.01f * delta;
          std::cout << "Blur Radius set to " << m_background_blur_radius.value() << "\n";
        }
    }

  if (keyboard_state[SDL_SCANCODE_LEFT])
    {
      if (alt_held)
        {
          m_blur_max_sample_radius.value() -= 0.01f * delta;
          m_blur_max_sample_radius.value() = astral::t_max(0.0f, m_blur_max_sample_radius.value());
          std::cout << "Blur Sample Radius set to " << m_blur_max_sample_radius.value()  << "\n";
        }
      else
        {
          m_background_blur_radius.value() -= 0.01f * delta;
          m_background_blur_radius.value() = astral::t_max(0.0f, m_background_blur_radius.value());
          std::cout << "Blur Radius set to " << m_background_blur_radius.value() << "\n";
        }
    }


  if (keyboard_state[SDL_SCANCODE_DOWN])
    {
      if (keyboard_state[SDL_SCANCODE_RETURN])
        {
          scale_factor_delta -= delta * scale_rate;
          scale_factor_changed = true;
        }
      else
        {
          m_alpha.value() -= delta * alpha_rate;
          alpha_changed = true;
        }
    }

  if (alpha_changed)
    {
      m_alpha.value() = astral::t_max(0.0f, astral::t_min(1.0f, m_alpha.value()));
      std::cout << "alpha set to " << m_alpha.value()
                << "(" << int(m_alpha.value() * 255.0f) << ")\n"
                << std::flush;
    }

  if (scale_factor_changed)
    {
      m_scale_factor.value() += scale_factor_delta;
      m_scale_factor.value() = astral::t_max(0.0f, m_scale_factor.value());
      std::cout << "Fill path scale factor set to " << m_scale_factor.value()
                << "\n" << std::flush;
    }

  scale_delta = 0.01f * delta;
  angle_delta = 0.0025f * delta * 180.0f / ASTRAL_PI;
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

  if (keyboard_state[SDL_SCANCODE_N])
    {
      m_stroke_params.m_miter_limit
        = astral::t_max(0.0f, m_stroke_params.m_miter_limit - delta * miter_rate);
      std::cout << "Miter limit set to: " << m_stroke_params.m_miter_limit << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_M])
    {
      if (!keyboard_state[SDL_SCANCODE_LCTRL] && !keyboard_state[SDL_SCANCODE_RCTRL])
        {
          m_stroke_params.m_miter_limit += delta * miter_rate;
          std::cout << "Miter limit set to: " << m_stroke_params.m_miter_limit << "\n";
        }
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
      if (!ctrl_held)
        {
          m_rotate_angle.value() += angle_delta;
          if (angle_delta > 360.0f)
            {
              m_rotate_angle.value() -= 360.0f;
            }
          std::cout << "Angle set to: " << m_rotate_angle.value() << " degrees\n";
        }
      else
        {
          m_graph_stroke_thickness.value() = astral::t_max(0.0f, m_graph_stroke_thickness.value() - graph_stroke_rate * delta);
          std::cout << "GraphStrokeThickness = " << m_graph_stroke_thickness.value() << "\n";
        }
    }

  if (keyboard_state[SDL_SCANCODE_0] && !alt_held)
    {
      if (!ctrl_held)
        {
          m_rotate_angle.value() -= angle_delta;
          if (angle_delta < 0.0f)
            {
              m_rotate_angle.value() += 360.0f;
            }
          std::cout << "Angle set to: " << m_rotate_angle.value() << " degrees\n";
        }
      else
        {
          m_graph_stroke_thickness.value() = astral::t_min(1.0f, graph_stroke_rate * delta + m_graph_stroke_thickness.value());
          std::cout << "GraphStrokeThickness = " << m_graph_stroke_thickness.value() << "\n";
        }
    }

  if (keyboard_state[SDL_SCANCODE_1] && !alt_held && ctrl_held)
    {
      m_graph_stroke_spacing.value() = astral::t_max(0.0f, m_graph_stroke_spacing.value() - graph_stroke_rate * delta);
      std::cout << "GraphStrokeSpacing = " << m_graph_stroke_spacing.value() << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_2] && !alt_held && ctrl_held)
    {
      m_graph_stroke_spacing.value() = astral::t_min(1.0f, m_graph_stroke_spacing.value() + graph_stroke_rate * delta);
      std::cout << "GraphStrokeSpacing = " << m_graph_stroke_spacing.value() << "\n";
    }

  return return_value;
}

void
PathTest::
handle_event(const SDL_Event &ev)
{
  m_zoom.handle_event(ev);
  if (ev.type == SDL_MOUSEMOTION)
    {
      astral::vec2 p, c(ev.motion.x + ev.motion.xrel,
                        ev.motion.y + ev.motion.yrel);
      astral::Transformation tr;

      /* brush is in item coordinates */
      tr = m_zoom.transformation().astral_transformation();
      tr.scale(m_scale_pre_rotate.value());
      tr.rotate(m_rotate_angle.value() * ASTRAL_PI / 180.0f);
      tr.scale(m_scale_post_rotate.value());
      tr = tr.inverse();
      p = tr.apply_to_point(c);

      if (ev.motion.state & SDL_BUTTON(SDL_BUTTON_MIDDLE))
        {
          m_gradient_p0.value() = p;
        }

      if (ev.motion.state & SDL_BUTTON(SDL_BUTTON_RIGHT))
        {
          m_gradient_p1.value() = p;
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

        case SDLK_SPACE:
          if (ev.key.keysym.mod & KMOD_ALT)
            {
              m_show_offscreen_alloc_info = !m_show_offscreen_alloc_info;
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

        case SDLK_e:
          m_stroke_params.m_draw_edges = !m_stroke_params.m_draw_edges;
          if (m_stroke_params.m_draw_edges)
            {
              std::cout << "Stroke with edges\n";
            }
          else
            {
              std::cout << "Stroke without edges\n";
            }
          m_print_stats = true;
          break;

        case SDLK_s:
          if (ev.key.keysym.mod & KMOD_CTRL)
            {
              m_swap_fill_and_stroke_brush.value() = !m_swap_fill_and_stroke_brush.value();
              if (m_swap_fill_and_stroke_brush.value())
                {
                  std::cout << "Swap stroke and fill brushes: ON\n";
                }
              else
                {
                  std::cout << "Swap stroke and fill brushes: OFF\n";
                }
            }
          else if (ev.key.keysym.mod & KMOD_ALT)
            {
              m_use_sub_ubers.value() = !m_use_sub_ubers.value();
              std::cout << "UseSubUbers sets to " << std::boolalpha << m_use_sub_ubers.value() << "\n";
            }
          else
            {
              cycle_value(m_stroke_mode.value(),
                          ev.key.keysym.mod & (KMOD_SHIFT|KMOD_ALT),
                          number_stroke_modes + 1);
              std::cout << "Stroking mode set to " << label(m_stroke_mode.value()) << "\n";
            }
          break;

        case SDLK_w:
          if (m_stroke_mode.value() != stroke_none && m_dash_pattern_choice.value() != 0)
            {
              if ((ev.key.keysym.mod & KMOD_SHIFT) && (ev.key.keysym.mod & KMOD_CTRL))
                {
                  bool b;

                  b = m_dash_patterns[m_dash_pattern_choice.value() - 1u].dash_pattern_per_edge();
                  m_dash_patterns[m_dash_pattern_choice.value() - 1u].dash_pattern_per_edge(!b);
                  if (b)
                    {
                      std::cout << "DashPattern applied per contour\n";
                    }
                  else
                    {
                      std::cout << "DashPattern applied per edge\n";
                    }
                }
              else if (ev.key.keysym.mod & KMOD_SHIFT)
                {
                  bool b;

                  b = m_dash_patterns[m_dash_pattern_choice.value() - 1u].skip_lengths_adjusted();
                  m_dash_patterns[m_dash_pattern_choice.value() - 1u].skip_lengths_adjusted(!b);
                  if (b)
                    {
                      std::cout << "DashPattern does not adjust skip lengths\n";
                    }
                  else
                    {
                      std::cout << "DashPattern adjusts skip lengths\n";
                    }
                }
              else if (ev.key.keysym.mod & KMOD_CTRL)
                {
                  bool b;

                  b = m_dash_patterns[m_dash_pattern_choice.value() - 1u].draw_lengths_adjusted();
                  m_dash_patterns[m_dash_pattern_choice.value() - 1u].draw_lengths_adjusted(!b);
                  if (b)
                    {
                      std::cout << "DashPattern does not adjust draw lengths\n";
                    }
                  else
                    {
                      std::cout << "DashPattern adjusts draw lengths\n";
                    }
                }
              else
                {
                  enum astral::StrokeShader::DashPattern::adjust_t a;

                  a = m_dash_patterns[m_dash_pattern_choice.value() - 1u].adjust_mode();
                  cycle_value(a, false, astral::StrokeShader::DashPattern::number_adjust);
                  m_dash_patterns[m_dash_pattern_choice.value() - 1u].adjust_mode(a);

                  std::cout << "Dash pattern modified to " << astral::label(a) << "\n";
                }
            }
          break;

        case SDLK_x:
          if (m_stroke_mode.value() != stroke_none)
            {
              if (ev.key.keysym.mod & KMOD_CTRL)
                {
                  m_scale_dash_pattern_on_pixel_width_stroking.value() = !m_scale_dash_pattern_on_pixel_width_stroking.value();
                  std::cout << "Dash patterns scales with zoom under pixel width stroking set to: "
                            << std::boolalpha << m_scale_dash_pattern_on_pixel_width_stroking.value()
                            << "\n";
                }
              else
                {
                  cycle_value(m_dash_pattern_choice.value(),
                              ev.key.keysym.mod & (KMOD_SHIFT|KMOD_ALT),
                              m_dash_patterns.size() + 1u);
                  if (m_dash_pattern_choice.value() == 0u)
                    {
                      std::cout << "Stroke without dashing\n";
                    }
                  else
                    {
                      std::cout << "Stroke with dash pattern: "
                                << m_dash_patterns[m_dash_pattern_choice.value() - 1u]
                                << "\n";
                    }
                }
            }
          break;

        case SDLK_o:
          m_mask_stroke_params.m_sparse_mask = !m_mask_stroke_params.m_sparse_mask;
          std::cout << "Stroking with sparse mask set to: "
                    << std::boolalpha << m_mask_stroke_params.m_sparse_mask
                    << "\n";
          break;

        case SDLK_a:
          cycle_value(m_fill_params.m_aa_mode,
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      astral::number_anti_alias_modes);
          std::cout << "Fill anti-aliasing set to "
                    << astral::label(m_fill_params.m_aa_mode) << "\n";
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

        case SDLK_j:
          {
            astral::c_string jlabel(nullptr);
            enum astral::join_t *j(nullptr);

            if (ev.key.keysym.mod & KMOD_ALT)
              {
                j = &m_stroke_params.m_glue_join;
                jlabel = "GlueJoin";
              }
            else if (ev.key.keysym.mod & KMOD_CTRL)
              {
                j = &m_stroke_params.m_glue_cusp_join;
                jlabel = "GlueCuspJoin";
              }
            else
              {
                j = &m_stroke_params.m_join;
                jlabel = "Join style";
              }

            cycle_value(*j, ev.key.keysym.mod & KMOD_SHIFT, astral::number_join_t + 1);
            std::cout << jlabel << " set to " << astral::label(*j) << "\n";
          }
          break;

        case SDLK_m:
          {
            if (ev.key.keysym.mod & KMOD_CTRL)
              {
                astral::c_string label;

                m_stroke_params.m_miter_clip = !m_stroke_params.m_miter_clip;
                label = (m_stroke_params.m_miter_clip) ?
                  "miter-clip":
                  "miter-cull";
                std::cout << "Miter style set to " << label << "\n";
              }
            }
          break;

        case SDLK_c:
          if (m_stroke_mode.value() != stroke_none)
            {
              cycle_value(m_stroke_params.m_cap,
                          ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                          astral::number_cap_t);
              std::cout << "Cap style set to "
                        << astral::label(m_stroke_params.m_cap) << "\n";
            }
          break;

        case SDLK_g:
          if (ev.key.keysym.mod & KMOD_CTRL)
            {
              astral::c_string label;

              cycle_value(m_gradient_type.value(),
                          ev.key.keysym.mod & (KMOD_SHIFT|KMOD_ALT),
                          astral::Gradient::number_types + 1);

              label = (m_gradient_type.value() == astral::Gradient::number_types) ?
                "no-gradient" :
                astral::label(static_cast<astral::Gradient::type_t>(m_gradient_type.value()));

              std::cout << "Gradient type set to " << label << "\n";
            }
          else
            {
              cycle_value(m_mask_fill_usage_params.m_mask_type,
                          ev.key.keysym.mod & (KMOD_SHIFT|KMOD_ALT),
                          astral::number_mask_type);
              std::cout << "FillMask mode set to: "
                        << astral::label(m_mask_fill_usage_params.m_mask_type) << "\n";
            }
          break;

        case SDLK_t:
          cycle_value(m_mask_stroke_usage_params.m_mask_type,
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      astral::number_mask_type);
          std::cout << "StrokeMask mode set to: " << astral::label(m_mask_stroke_usage_params.m_mask_type) << "\n";
          break;

        case SDLK_p:
          if (ev.key.keysym.mod & KMOD_SHIFT)
            {
              m_stroke_width_pixels.value() = !m_stroke_width_pixels.value();
              std::cout << "Stroking width in pixesl set to " << std::boolalpha
                        << m_stroke_width_pixels.value() << "\n";
            }
          else if (ev.key.keysym.mod & KMOD_CTRL)
            {
              m_graceful_thin_stroking.value() = !m_graceful_thin_stroking.value();
              std::cout << "Graceful thin stroking set to " << std::boolalpha
                        << m_graceful_thin_stroking.value() << "\n";
            }
          else
            {
              m_path_time.value().pause(!m_path_time.value().paused());
              if (m_path_time.value().paused())
                {
                  std::cout << "Animation paused at " << compute_animation_interpolate() << "\n";
                }
              std::cout << "Current Zoom = " << m_zoom.transformation().m_scale
                        << "\n";
              m_print_stats = true;
            }
          break;

        case SDLK_d:
          {
            cycle_value(m_path_mode.value(),
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        number_path_modes);
            std::cout << "Draw mode set to " << label(m_path_mode.value()) << "\n";
          }
          break;

        case SDLK_f:
          {
            cycle_value(m_display_fill_method.value(),
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        number_display_fill_methods);
            std::cout << "Fill method set to " << label(m_display_fill_method.value()) << "\n";
            m_print_item_path_text = (m_display_fill_method.value() == display_fill_item_path);
          }
          break;

        case SDLK_b:
          if ((ev.key.keysym.mod & KMOD_CTRL) != 0u)
            {
              cycle_value(m_blend_mode.value(),
                          ev.key.keysym.mod & (KMOD_SHIFT|KMOD_ALT),
                          astral::number_blend_modes);
              std::cout << "Blend mode set to " << astral::label(m_blend_mode.value()) << "\n";
            }
          else if ((ev.key.keysym.mod & KMOD_ALT) != 0u)
            {
              m_add_some_background_text.value() = !m_add_some_background_text.value();
              std::cout << "Draw background text set to " << std::boolalpha
                        << m_add_some_background_text.value() << "\n";
            }
          else
            {
              cycle_value(m_background_blur_mode.value(),
                          ev.key.keysym.mod & KMOD_SHIFT,
                          number_background_blur_modes);
              std::cout << "Backgound blur mode set to " << label(m_background_blur_mode.value()) << "\n";
            }
          break;

        case SDLK_l:
          m_render_to_layer.value() = !m_render_to_layer.value();
          std::cout << "Render to layer set to: " << m_render_to_layer.value() << "\n";
          break;

        case SDLK_k:
          cycle_value(m_mask_filter.value(),
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      astral::number_filter_modes);
          std::cout << "Mask filter mode mode set to " << astral::label(m_mask_filter.value()) << "\n";
          break;

        case SDLK_0:
          if (ev.key.keysym.mod & KMOD_ALT)
            {
              m_scale_factor.value() = 1.0f;
              std::cout << "Stroke and fill mask render scale factor set to " << m_scale_factor.value()
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
  PathTest M;
  return M.main(argc, argv);
}

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
#include "load_svg.hpp"
#include "print_bytes.hpp"
#include "text_helper.hpp"
#include "demo_macros.hpp"
#include "animated_path_reflect.hpp"

class SVGExample:public render_engine_gl3_demo
{
public:
  SVGExample(void);

  ~SVGExample();

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
  enum fill_mode_t
    {
      fill_with_path,
      fill_with_path_as_layer,
      fill_with_item_path,
      fill_stroke_instead,
      fill_none,

      number_fill_mode
    };

  enum stroke_mode_t
    {
      stroke_svg_width,
      stroke_hairline,
      stroke_none,

      number_stroke_mode
    };

  enum color_mode_t
    {
      color_mode_normal,
      color_mode_override,
      color_mode_show_overdraw,

      number_color_mode
    };

  enum hud_mode_t:uint32_t
    {
      basic_hud,
      detail_level1_hud,
      detail_level2_hud,
      detail_level3_hud,

      number_hud_modes,
    };

  enum draw_mode_t:uint32_t
    {
      draw_t0,
      draw_animated_at_0,
      draw_t1,
      draw_animated_at_1,
      draw_animated,

      number_draw_modes,
    };

  class ClipErrorLogger:public astral::Renderer::SparseFillingErrorCallBack
  {
  public:
    explicit
    ClipErrorLogger(const SVGExample *p):
      m_current_path_id(-1),
      m_current_t(0.0f),
      m_src(p)
    {}

    void
    report_error(const astral::Contour&, const std::string &message) override
    {
      report_implement(message);
    }

    void
    report_error(const astral::AnimatedContour&, float, const std::string &message) override
    {
      report_implement(message);
    }

    void
    report_implement(const std::string &message);

    unsigned int m_current_path_id;
    float m_current_t;

  private:
    const SVGExample *m_src;
  };

  static
  astral::c_string
  label(enum draw_mode_t m)
  {
    static const astral::c_string labels[number_draw_modes] =
      {
        [draw_t0] = "draw_t0",
        [draw_animated_at_0] = "draw_animated_at_0",
        [draw_t1] = "draw_t1",
        [draw_animated_at_1] = "draw_animated_at_1",
        [draw_animated] = "draw_animated",
      };

    return labels[m];
  }

  static
  astral::c_string
  label(enum fill_mode_t m)
  {
    static const astral::c_string labels[number_fill_mode] =
      {
        [fill_with_path] = "fill_with_path",
        [fill_with_path_as_layer] = "fill_with_path_as_layer",
        [fill_with_item_path] = "fill_with_item_path",
        [fill_stroke_instead] = "fill_stroke_instead",
        [fill_none] = "fill_none",
      };

    return labels[m];
  }

  static
  astral::c_string
  label(enum stroke_mode_t m)
  {
    static const astral::c_string labels[number_stroke_mode] =
      {
        [stroke_svg_width] = "stroke_svg_width",
        [stroke_hairline] = "stroke_hairline",
        [stroke_none] = "stroke_none",
      };

    return labels[m];
  }

  static
  astral::c_string
  label(enum color_mode_t m)
  {
    static const astral::c_string labels[number_color_mode] =
      {
        [color_mode_normal] = "color_mode_normal",
        [color_mode_override] = "color_mode_override",
        [color_mode_show_overdraw] = "color_mode_show_overdraw",
      };

    return labels[m];
  }

  float
  update_smooth_values(void);

  void
  reset_zoom_transformation(void);

  void
  generate_brush(astral::RenderEncoderBase render_encoder, unsigned int idx,
                 const SVGBrush &in_brush, float opacity, astral::Brush *out_brush);

  void
  render_svg_element(astral::RenderEncoderBase render_encoder,
                     unsigned int idx, const SVGElement &element);

  void
  draw_hud(astral::RenderEncoderSurface encoder, float frame_ms);

  float
  compute_animation_interpolate(void);

  astral::CombinedPath
  fetch_path(unsigned int idx);

  command_separator m_demo_options;
  command_line_argument_value<std::string> m_svg_file;
  command_line_argument_value<std::string> m_svg_units;
  command_line_argument_value<float> m_svg_dpi;
  command_line_argument_value<std::string> m_image_file;
  command_line_argument_value<bool> m_show_render_stats;
  command_line_argument_value<bool> m_init_stretched_to_window;
  command_line_argument_value<unsigned int> m_animation_time;
  command_line_argument_value<float> m_reflect_direction_x;
  command_line_argument_value<float> m_reflect_direction_y;
  command_line_argument_value<float> m_reflect_pt_x;
  command_line_argument_value<float> m_reflect_pt_y;
  command_line_argument_value<float> m_fill_scale_factor;
  command_line_argument_value<float> m_stroke_scale_factor;
  command_line_argument_value<float> m_layer_scale_factor;
  command_line_argument_value<int> m_clear_red, m_clear_green, m_clear_blue, m_clear_alpha;
  command_line_argument_value<bool> m_log_clipping_errors;

  enumerated_command_line_argument_value<enum stroke_mode_t> m_stroke_mode;
  enumerated_command_line_argument_value<enum astral::anti_alias_t> m_fill_aa_mode;
  command_line_argument_value<bool> m_sparse_stroke, m_use_direct_stroking;
  enumerated_command_line_argument_value<enum astral::fill_method_t> m_sparse_fill;
  enumerated_command_line_argument_value<enum astral::filter_t> m_layer_filter, m_stroke_filter, m_fill_filter;
  command_line_argument_value<astral::vec2> m_scale_pre_rotate, m_scale_post_rotate;
  command_line_argument_value<float> m_rotate_angle;
  enumerated_command_line_argument_value<enum fill_mode_t> m_fill_mode;
  enumerated_command_line_argument_value<enum color_mode_t> m_color_mode;
  enumerated_command_line_argument_value<enum draw_mode_t> m_mode;
  command_line_argument_value<simple_time> m_path_time;
  command_line_argument_value<UniformScaleTranslate<float>> m_initial_camera;

  astral::reference_counted_ptr<astral::Image> m_image;
  astral::reference_counted_ptr<astral::TextItem> m_text_item;
  astral::reference_counted_ptr<ClipErrorLogger> m_clip_error_log;

  std::vector<astral::vec4> m_color_list;

  SVG m_svg;
  std::vector<astral::Path> m_reflect_svg;
  std::vector<astral::AnimatedPath> m_reflect_animated_svg;
  astral::Transformation m_svg_transform;

  PanZoomTrackerSDLEvent m_zoom;

  simple_time m_draw_timer;
  average_timer m_frame_time_average;
  enum hud_mode_t m_hud_mode;
  std::vector<unsigned int> m_prev_stats;
};

//////////////////////////////////////////
// SVGExample::ClipErrorLogger methods
void
SVGExample::ClipErrorLogger::
report_implement(const std::string &message)
{
  std::cout << "Clipping error:\n\n" << message << "\n\n"
            << "\tencountered on path #" << m_current_path_id
            << "\tt = " << print_float_and_bits(m_current_t) << "\n"
            << "\tZ = " << print_float_and_bits(m_src->m_zoom.transformation().m_scale) << "\n"
            << "\tTR = " << print_float_and_bits(m_src->m_zoom.transformation().m_translation) << "\n"
            << "\tpre-rotate = " << print_float_and_bits(m_src->m_scale_pre_rotate.value()) << "\n"
            << "\trotate = " << print_float_and_bits(m_src->m_rotate_angle.value()) << "\n"
            << "\tpost-rotate = " << print_float_and_bits(m_src->m_scale_post_rotate.value()) << "\n\n\n";
}

//////////////////////////////////////////
// SVGExample methods
SVGExample::
SVGExample(void):
  m_demo_options("Demo Options", *this),
  m_svg_file("demo_data/svg/Ghostscript_tiger_(original_background).svg", "file", "SVG File from which to load", *this),
  m_svg_units("px", "svg_units", "", *this),
  m_svg_dpi(96.0f, "dpi", "", *this),
  m_image_file("", "image", "name of file for image background", *this),
  m_show_render_stats(false, "show_render_stats",
                      "If true, at each frame print stdout stats of rendering",
                      *this),
  m_init_stretched_to_window(false, "init_stretched_to_window",
                             "If true, initialize display transformation to "
                             "stretch SVG file across the window",
                             *this),
  m_animation_time(3000, "animation_time", "Time to animate SVG reflection in ms", *this),
  m_reflect_direction_x(0.0f, "reflect_direction_x", "x-coordinate of reflection axis direciton", *this),
  m_reflect_direction_y(1.0f, "reflect_direction_y", "y-coordinate of reflection axis direciton", *this),
  m_reflect_pt_x(0.0f, "reflect_pt_x", "x-coordinate of reflection axis position", *this),
  m_reflect_pt_y(0.0f, "reflect_pt_y", "y-coordinate of reflection axis position", *this),
  m_fill_scale_factor(0.5f, "fill_scale_factor", "Resolution scale at which to compute fill masks", *this),
  m_stroke_scale_factor(1.0f, "stroke_scale_factor", "Resolution scale at which to compute stroke masks", *this),
  m_layer_scale_factor(1.0f, "layer_scale_factor", "Resolution scale at which to render transparent layers", *this),
  m_clear_red(0, "clear_red", "value (integer) for red channel for clear color in range [0, 255]", *this),
  m_clear_green(0, "clear_green", "value (integer) for green channel for clear color in range [0, 255]", *this),
  m_clear_blue(0, "clear_blue", "value (integer) for blue channel for clear color in range [0, 255]", *this),
  m_clear_alpha(0, "clear_alpha", "value (integer) for alpha channel for clear color in range [0, 255]", *this),
  m_log_clipping_errors(false, "log_clipping_errors",
                        "if true, log clipping errors to console. "
                        "Note that clipping errors are recoverable and nearly always "
                        "the result of numerical round off", *this),
  m_stroke_mode(stroke_svg_width,
                enumerated_string_type<enum stroke_mode_t>()
                .add_entry("stroke_svg_width", stroke_svg_width, "Strokes are with width from SVG file")
                .add_entry("stroke_hairline", stroke_hairline, "Strokes are with hairline strokes ")
                .add_entry("stroke_none", stroke_none, "Strokes are skipped"),
                "stroke_mode",
                "Specifies initial mode for stroking",
                *this),
  m_fill_aa_mode(astral::with_anti_aliasing,
                 enumerated_string_type<enum astral::anti_alias_t>(&astral::label, astral::number_anti_alias_modes),
                 "fill_aa_mode",
                 "specifies aa-mode to apply to fills of the SVG",
                 *this),
  m_sparse_stroke(true, "sparse_stroke", "if true, stroke the strokes of the SVG sparsely", *this),
  m_use_direct_stroking(true, "use_direct_stroking", "if true, on opaque strokes, use direct stroking", *this),
  m_sparse_fill(astral::fill_method_sparse_curve_clipping,
                enumerated_string_type<enum astral::fill_method_t>(&astral::label, astral::number_fill_method_t),
                "sparse_fill",
                "specifies sparse filling method to use on fills of the SVG",
                *this),
  m_layer_filter(astral::filter_linear,
                 enumerated_string_type<enum astral::filter_t>(&astral::label, astral::number_filter_modes),
                 "layer_filter",
                 "filter to apply if drawing SVG's layers to offscreen images",
                 *this),
  m_stroke_filter(astral::filter_linear,
                  enumerated_string_type<enum astral::filter_t>(&astral::label, astral::number_filter_modes),
                  "stroke_filter",
                  "filter to apply to masks generated for strokes",
                  *this),
  m_fill_filter(astral::filter_linear,
                enumerated_string_type<enum astral::filter_t>(&astral::label, astral::number_filter_modes),
                "fill_filter",
                "filter to apply to masks generate for fills",
                *this),
  m_scale_pre_rotate(astral::vec2(1.0f, 1.0f), "scale_pre_rotate", "Scaling factor to perform on SVG before rotation, formatted as ScaleX:ScaleY", *this),
  m_scale_post_rotate(astral::vec2(1.0f, 1.0f), "scale_post_rotate", "Scaling factor to perform on SVG after rotation, formatted as ScaleX:ScaleY", *this),
  m_rotate_angle(0.0f, "rotate", "ampunt by which to rotate the SVG in degrees", *this),
  m_fill_mode(fill_with_path,
              enumerated_string_type<enum fill_mode_t>(&label, number_fill_mode),
              "fill_mode",
              "if and how to substitute the fills on the SVG",
              *this),
  m_color_mode(color_mode_normal,
               enumerated_string_type<enum color_mode_t>(&label, number_color_mode),
               "color_mode",
               "if and how to override the colors of the SVG",
               *this),
  m_mode(draw_t0,
         enumerated_string_type<enum draw_mode_t>(&label, number_draw_modes),
         "draw_mode",
         "if and how to animate the paths of the SVG",
         *this),
  m_path_time(simple_time(), "path_time",
              "If set, pauses the timer for path aimation and specifies the intial time value in ms",
              *this),
  m_initial_camera(UniformScaleTranslate<float>(), "initial_camera", "Initial position of camera give as translate-x:translate-y:zoom", *this),
  m_frame_time_average(1000u),
  m_hud_mode(basic_hud)
{
  std::cout << "Controls:"
            << "\n\tspace: cycle through HUD modes"
            << "\n\td: cycle through drawing mode: draw start, draw end, draw animated, etc"
            << "\n\tshift-space: toggle showing rendering stats to console"
            << "\n\tp: pause animation"
            << "\n\tctrl-z: decrease rendering accuracy"
            << "\n\tz: increase rendering accuracy"
            << "\n\tq: reset transformation applied to the path"
            << "\n\ts: cycle through stroking: as in SVG file, hairline, skip stroking"
            << "\n\tc: cycle through filling color mode: as in SVG, override, show overdraw"
            << "\n\tf: cycle through filling: fill with path, fill with shader path, stroke instead of filling"
            << "\n\tk: cycle through filter applied to fill mask"
            << "\n\tshift-k: cycle through filter applied to stroke mask"
            << "\n\tctrl-k: cycle through filter applied to transparent layer fill mask"
            << "\n\ta: toggle filling anti-aliasing"
            << "\n\tshift-a: toggle sparse filling"
            << "\n\to: toggle stroking anti-aliasing"
            << "\n\tshift-o: toggle sparse stroking"
            << "\n\tALT-1 -- ALT-9: set fill-mask scale factor"
            << "\n\tSHIFT-ALT-1 -- SHIFT-ALT-9: set stroke-mask scale factor"
            << "\n\tCTR-ALT-1 -- CTRL-ALT-9: set transparanet fill layer scale factor"
            << "\n\t6: increase horizontal pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\tctrl-6: decrease horizontal pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\t7: increase vertical pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\tctrl-7: decrease vertical pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + 6: increase horizontal post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + ctrl-6: decrease horizontal post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + 7: increase vertical post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + ctrl-7: decrease vertical post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\t9/0 increase/decrease angle of rotation"
            << "\n\t[/] : decrease/incrase strokign width"
            << "\n\tLeft Mouse Drag: pan"
            << "\n\tHold Left Mouse, then drag up/down: zoom out/in"
            << "\n";
}

float
SVGExample::
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
SVGExample::
reset_zoom_transformation(void)
{
  /* Initialize zoom location to be identity */
  m_zoom.transformation(UniformScaleTranslate<float>());
}

void
SVGExample::
init_gl(int w, int h)
{
  PathCommand st, ed;

  m_zoom.transformation(m_initial_camera.value());
  m_prev_stats.resize(renderer().stats_labels().size(), 0);

  float pixel_size(32.0f);
  astral::Font font(default_typeface(), pixel_size);
  m_text_item = astral::TextItem::create(font);

  if (!m_image_file.value().empty())
    {
      astral::reference_counted_ptr<ImageLoader> pixels = ImageLoader::create(m_image_file.value());
      astral::uvec2 image_dims;

      image_dims = pixels->dimensions();

      if (pixels->non_empty())
        {
          std::cout << "Loaded image from file \""
                    << m_image_file.value() << "\"\n";
          m_image = engine().image_atlas().create_image(astral::uvec2(image_dims));
          for (unsigned int mip = 0, w = image_dims.x(), h = image_dims.y();
               w > 0u && h > 0u; w >>= 1u, h >>= 1u, ++mip)
            {
              m_image->set_pixels(mip,
                                  astral::ivec2(0, 0),
                                  astral::ivec2(w, h),
                                  w,
                                  pixels->mipmap_pixels(mip));
            }
        }
    }

  m_color_list.push_back(astral::vec4(1.0f, 1.0f, 1.0f, 1.0f));
  m_color_list.push_back(astral::vec4(0.0f, 0.0f, 0.0f, 1.0f));
  m_color_list.push_back(astral::vec4(1.0f, 0.0f, 0.0f, 1.0f));
  m_color_list.push_back(astral::vec4(0.0f, 1.0f, 0.0f, 1.0f));
  m_color_list.push_back(astral::vec4(0.0f, 0.0f, 1.0f, 1.0f));
  m_color_list.push_back(astral::vec4(1.0f, 1.0f, 0.0f, 1.0f));
  m_color_list.push_back(astral::vec4(1.0f, 0.0f, 1.0f, 1.0f));
  m_color_list.push_back(astral::vec4(0.0f, 1.0f, 1.0f, 1.0f));

  m_color_list.push_back(astral::vec4(0.5f, 0.5f, 0.5f, 1.0f));
  m_color_list.push_back(astral::vec4(1.0f, 0.5f, 0.5f, 1.0f));
  m_color_list.push_back(astral::vec4(0.5f, 1.0f, 0.5f, 1.0f));
  m_color_list.push_back(astral::vec4(0.5f, 0.5f, 1.0f, 1.0f));
  m_color_list.push_back(astral::vec4(1.0f, 1.0f, 0.5f, 1.0f));
  m_color_list.push_back(astral::vec4(1.0f, 0.5f, 1.0f, 1.0f));
  m_color_list.push_back(astral::vec4(0.5f, 1.0f, 1.0f, 1.0f));

  m_svg.load(engine(), m_svg_file.value().c_str(), m_svg_units.value().c_str(), m_svg_dpi.value());

  Line reflection;
  astral::c_array<const SVGElement> svg_elements(m_svg.elements());

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
      reflection.m_p = m_svg.bbox().as_rect().center_point();
    }
  m_reflect_animated_svg.resize(m_svg.elements().size());
  m_reflect_svg.resize(m_svg.elements().size());
  for (unsigned int i = 0; i < svg_elements.size(); ++i)
    {
      create_animated_reflection(&m_reflect_animated_svg[i],
                                 svg_elements[i].m_path,
                                 reflection,
                                 &m_reflect_svg[i]);
    }

  if (m_init_stretched_to_window.value())
    {
      const astral::Rect &bb(m_svg.bbox().as_rect());
      astral::vec2 sz(bb.size());
      if (sz.x() > 0.0f && sz.y() > 0.0f)
        {
          m_svg_transform.scale(static_cast<float>(w) / sz.x(),
                                static_cast<float>(h) / sz.y());
          m_svg_transform.translate(-bb.min_x(), -bb.min_y());
        }
    }

  if (m_log_clipping_errors.value())
    {
      m_clip_error_log = ASTRALnew ClipErrorLogger(this);
      renderer().set_clip_error_callback(m_clip_error_log);
    }
}

SVGExample::
~SVGExample()
{
  if (m_clip_error_log)
    {
      renderer().set_clip_error_callback(nullptr);
    }
}

void
SVGExample::
generate_brush(astral::RenderEncoderBase render_encoder, unsigned int idx,
               const SVGBrush &in_brush, float opacity, astral::Brush *out_brush)
{
  switch (m_color_mode.value())
    {
    default:
    case color_mode_normal:
      {
        /* colors from SVG are always in sRGB colorspace */
        if (in_brush.m_gradient.m_colorstops)
          {
            out_brush->colorspace(astral::colorspace_srgb);
            out_brush->gradient(render_encoder.create_value(in_brush.m_gradient));
          }
        else
          {
            out_brush->base_color(in_brush.m_color);
          }
        out_brush->m_base_color.w() *= opacity;
      }
      break;

    case color_mode_override:
      out_brush->base_color(m_color_list[idx % m_color_list.size()]);
      break;

    case color_mode_show_overdraw:
      out_brush->base_color(astral::vec4(1.0f, 1.0f, 1.0f, 0.05f));
      break;
    }
}

astral::CombinedPath
SVGExample::
fetch_path(unsigned int idx)
{
  float t;
  astral::CombinedPath return_value;

  t = compute_animation_interpolate();

  switch (m_mode.value())
    {
    default:
      break;

    case draw_animated:
      t = astral::t_min(1.0f, astral::t_max(0.0f, t));
      break;

    case draw_animated_at_0:
      t = 0.0f;
      break;

    case draw_animated_at_1:
      t = 1.0f;
      break;
    }

  switch (m_mode.value())
    {
    case draw_animated:
    case draw_animated_at_0:
    case draw_animated_at_1:
      return_value = astral::CombinedPath(t, m_reflect_animated_svg[idx]);
      break;

    case draw_t0:
      return_value = astral::CombinedPath(m_svg.elements()[idx].m_path);
      break;

    case draw_t1:
      return_value = astral::CombinedPath(m_reflect_svg[idx]);
      break;

    default:
      ASTRALfailure("Invalid draw_mode");
    }

  if (m_log_clipping_errors.value())
    {
      m_clip_error_log->m_current_path_id = idx;
      m_clip_error_log->m_current_t = t;
    }

  return return_value;
}

void
SVGExample::
render_svg_element(astral::RenderEncoderBase render_encoder,
                   unsigned int idx, const SVGElement &element)
{
  enum astral::blend_mode_t blend_mode;
  astral::CombinedPath path(fetch_path(idx));

  blend_mode = (m_color_mode.value() == color_mode_show_overdraw) ?
    astral::blend_porter_duff_plus:
    astral::blend_porter_duff_src_over;

  if (element.m_fill_brush.m_active)
    {
      astral::Brush brush;

      generate_brush(render_encoder, idx, element.m_fill_brush, element.m_opacity, &brush);
      switch (m_fill_mode.value())
        {
        case fill_stroke_instead:
          render_encoder.stroke_paths(path,
                                      astral::StrokeParameters()
                                      .width(0.0f),
                                      render_encoder.create_value(brush),
                                      blend_mode,
                                      astral::MaskUsage()
                                      .mask_type(astral::mask_type_distance_field)
                                      .filter(m_stroke_filter.value()),
                                      astral::StrokeMaskProperties()
                                      .sparse_mask(m_sparse_stroke.value())
                                      .render_scale_factor(m_fill_scale_factor.value()));
          break;

        case fill_with_path:
          render_encoder.fill_paths(path,
                                    astral::FillParameters()
                                    .aa_mode(m_fill_aa_mode.value())
                                    .fill_rule(element.m_fill_rule),
                                    render_encoder.create_value(brush),
                                    blend_mode,
                                    astral::MaskUsage()
                                    .mask_type(astral::mask_type_distance_field)
                                    .filter(m_fill_filter.value()),
                                    astral::FillMaskProperties()
                                    .sparse_mask(m_sparse_fill.value())
                                    .render_scale_factor(m_fill_scale_factor.value()));
          break;

        case fill_with_path_as_layer:
          {
            astral::RenderEncoderLayer layer_encoder;

            layer_encoder = render_encoder.begin_layer(path.compute_bounding_box(),
                                                       m_layer_scale_factor.value(),
                                                       astral::vec4(1.0f, 1.0f, 1.0f, 0.75f),
                                                       blend_mode,
                                                       m_layer_filter.value());
            layer_encoder.encoder().fill_paths(path,
                                               astral::FillParameters()
                                               .aa_mode(m_fill_aa_mode.value())
                                               .fill_rule(element.m_fill_rule),
                                               render_encoder.create_value(brush),
                                               blend_mode,
                                               astral::MaskUsage()
                                               .filter(m_fill_filter.value()),
                                               astral::FillMaskProperties()
                                               .sparse_mask(m_sparse_fill.value())
                                               .render_scale_factor(m_fill_scale_factor.value()));

            render_encoder.end_layer(layer_encoder);
          }
          break;

        case fill_with_item_path:
          {
            float tol;
            const astral::ItemPath *item_path;

            tol = render_encoder.compute_tolerance();
            item_path = &element.m_path.item_path(tol);
            render_encoder.draw_item_path(astral::ItemPath::Layer(*item_path)
                                            .fill_rule(element.m_fill_rule)
                                            .color(astral::vec4(1.0f, 1.0f, 1.0f, 1.0f)),
                                            render_encoder.create_value(brush),
                                            blend_mode);
          }
          break;

        default:
          break;
        }
    }

  if (element.m_stroke_brush.m_active && m_stroke_mode.value() != stroke_none)
    {
      astral::Brush brush;
      astral::RenderValue<astral::Brush> render_brush;
      astral::BoundingBox<float> bb;
      astral::StrokeParameters stroke_params(element.m_stroke_params);

      generate_brush(render_encoder, idx, element.m_stroke_brush, element.m_opacity, &brush);
      render_brush = render_encoder.create_value(brush);

      if (m_stroke_mode.value() == stroke_hairline)
        {
          stroke_params.width(0.0f);
        }
      else
        {
          stroke_params.graceful_thin_stroking(true);
        }

      if (m_use_direct_stroking.value() && render_brush.value().m_opaque)
        {
          if (element.m_dash_pattern.empty())
            {
              render_encoder.direct_stroke_paths(path,
                                                 stroke_params,
                                                 render_brush,
                                                 astral::blend_porter_duff_src_over);
            }
          else
            {
              render_encoder.direct_stroke_paths(path,
                                                 stroke_params,
                                                 element.m_dash_pattern,
                                                 render_brush,
                                                 astral::blend_porter_duff_src_over);
            }
        }
      else
        {
          astral::StrokeMaskProperties mask_stroke_params;

          mask_stroke_params
            .render_scale_factor(m_stroke_scale_factor.value())
            .sparse_mask(m_sparse_stroke.value());

          if (stroke_params.m_width <= 0.0f)
            {
              mask_stroke_params.render_scale_factor(1.0f);
            }

          if (element.m_dash_pattern.empty())
            {
              render_encoder.stroke_paths(path,
                                          stroke_params,
                                          render_brush,
                                          astral::blend_porter_duff_src_over,
                                          astral::MaskUsage(),
                                          mask_stroke_params);
            }
          else
            {
              render_encoder.stroke_paths(path,
                                          stroke_params,
                                          element.m_dash_pattern,
                                          render_brush,
                                          astral::blend_porter_duff_src_over,
                                          astral::MaskUsage(),
                                          mask_stroke_params);
            }
        }
    }
}

void
SVGExample::
draw_hud(astral::RenderEncoderSurface encoder, float frame_ms)
{
  static const enum astral::Renderer::renderer_stats_t vs[] =
    {
      astral::Renderer::number_sparse_fill_awkward_fully_clipped_or_unclipped,
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

  /* set the text */
  hud_text << "Resolution = " << dimensions() << "\n"
           << "Zoom = " << m_zoom.transformation().m_scale
           << ", Translation = " << m_zoom.transformation().m_translation << "\n\n"
           << "Rendering: " << astral::label(m_sparse_fill.value()) << "\n"
           << "Average over " << m_frame_time_average.interval_ms() << " ms: "
           << m_frame_time_average.average_elapsed_ms()
           << m_frame_time_average.parity_string() << "\n";

  /* draw the HUD in fixed location with a background */
  encoder.transformation(astral::Transformation());
  set_and_draw_hud(encoder, frame_ms,
                   astral::make_c_array(m_prev_stats),
                   *m_text_item, hud_text.str(),
                   vs_p, bvs_p, gvs_p);
}

void
SVGExample::
draw_frame(void)
{
  float frame_ms;
  astral::Transformation tr;
  astral::ivec2 dims(dimensions());
  astral::RenderEncoderSurface render_encoder;
  astral::c_array<const unsigned int> stats;
  astral::c_array<const astral::c_string> stats_labels(renderer().stats_labels());
  astral::u8vec4 clear_color(0, 0, 0, 0);

  m_frame_time_average.increment_counter();
  frame_ms = update_smooth_values();
  tr = m_zoom.transformation().astral_transformation();

  tr.scale(m_scale_pre_rotate.value());
  tr.rotate(m_rotate_angle.value() * (ASTRAL_PI / 180.0f));
  tr.scale(m_scale_post_rotate.value());

  if (color_mode_show_overdraw != m_color_mode.value())
    {
      clear_color.x() = astral::t_clamp(m_clear_red.value(), 0, 255);
      clear_color.y() = astral::t_clamp(m_clear_green.value(), 0, 255);
      clear_color.z() = astral::t_clamp(m_clear_blue.value(), 0, 255);
      clear_color.w() = astral::t_clamp(m_clear_alpha.value(), 0, 255);
    }

  render_encoder = renderer().begin(render_target(), astral::colorspace_srgb, clear_color);

  if (m_image)
    {
      astral::ImageSampler image(*m_image, astral::filter_cubic, astral::mipmap_ceiling);
      astral::Brush brush;
      astral::RenderValue<astral::ImageSampler> im;
      astral::RenderValue<astral::Brush> br;
      astral::vec2 target_sz(dims);
      astral::vec2 src_sz(m_image->size());

      im = render_encoder.create_value(image);

      brush.image(im);
      br = render_encoder.create_value(brush);

      render_encoder.scale(target_sz / src_sz);
      render_encoder.draw_rect(astral::Rect().size(src_sz), br);
    }

  render_encoder.transformation(tr);
  render_encoder.concat(m_svg_transform);
  astral::c_array<const SVGElement> elements(m_svg.elements());
  for (unsigned int i = 0; i < elements.size(); ++i)
    {
      render_svg_element(render_encoder, i, elements[i]);
    }

  if (!pixel_testing())
    {
      draw_hud(render_encoder, frame_ms);
    }

  stats = renderer().end();
  ASTRALassert(m_prev_stats.size() == stats.size());
  std::copy(stats.begin(), stats.end(), m_prev_stats.begin());

  if (m_show_render_stats.value())
    {
      std::cout << "frame ms = " << frame_ms << "\n"
                << "average over " << m_frame_time_average.interval_ms() << " ms: "
                << m_frame_time_average.average_elapsed_ms()
                << m_frame_time_average.parity_string()
                << "\n"
                << "\tSparseStroking = " << std::boolalpha << m_sparse_stroke.value() << "\n"
                << "\tTranslate = " << m_zoom.transformation().m_translation << "\n"
                << "\tScale = " << m_zoom.transformation().m_scale << "\n";
      for (unsigned int i = 0; i < stats.size(); ++i)
        {
          std::cout << "\t" << stats_labels[i] << " = " << stats[i] << "\n";
        }
    }

  #if 0
    {
      if (stats[renderer().stat_index(astral::Renderer::number_sparse_fill_clipping_errors)] != 0)
        {
          std::cout << "Clipping error encountered at:\n"
                    << "\tZ = " << print_float_and_bits(m_zoom.transformation().m_scale) << "\n"
                    << "\tTR = " << print_float_and_bits(m_zoom.transformation().m_translation) << "\n"
                    << "\tpre-rotate = " << print_float_and_bits(m_scale_pre_rotate.value()) << "\n"
                    << "\trotate = " << print_float_and_bits(m_rotate_angle.value()) << "\n"
                    << "\tpost-rotate = " << print_float_and_bits(m_scale_post_rotate.value()) << "\n";
        }
    }
  #endif
}

float
SVGExample::
update_smooth_values(void)
{
  const Uint8 *keyboard_state = SDL_GetKeyboardState(nullptr);
  float return_value, delta, scale_delta, angle_delta;
  astral::vec2 *scale_ptr(nullptr);
  astral::c_string scale_txt(nullptr);
  bool alt_held;

  ASTRALassert(keyboard_state != nullptr);
  return_value = delta = static_cast<float>(m_draw_timer.restart_us()) * 0.001f;

  alt_held = keyboard_state[SDL_SCANCODE_LALT] || keyboard_state[SDL_SCANCODE_RALT];

  if (keyboard_state[SDL_SCANCODE_LSHIFT])
    {
      delta *= 0.1f;
    }
  if (keyboard_state[SDL_SCANCODE_RSHIFT])
    {
      delta *= 10.0f;
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
SVGExample::
handle_event(const SDL_Event &ev)
{
  m_zoom.handle_event(ev);
  if (ev.type == SDL_KEYDOWN)
    {
      switch(ev.key.keysym.sym)
        {

        case SDLK_d:
          {
            cycle_value(m_mode.value(),
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        number_draw_modes);
            std::cout << "Draw mode set to " << label(m_mode.value()) << "\n";
          }
          break;

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

        case SDLK_q:
          reset_zoom_transformation();
          m_scale_pre_rotate.value() = m_scale_post_rotate.value() = astral::vec2(1.0f, 1.0f);
          m_rotate_angle.value() = 0.0f;
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

        case SDLK_p:
          m_path_time.value().pause(!m_path_time.value().paused());
          break;

        case SDLK_s:
          {
            cycle_value(m_stroke_mode.value(),
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        number_stroke_mode);
            std::cout << "Stroking mode set to " << label(m_stroke_mode.value()) << "\n";
          }
          break;

        case SDLK_c:
          {
            cycle_value(m_color_mode.value(),
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        number_color_mode);
            std::cout << "Color mode set to " << label(m_color_mode.value()) << "\n";
          }
          break;

        case SDLK_f:
          {
            cycle_value(m_fill_mode.value(),
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        number_fill_mode);

            std::cout << "Fill mode set to " << label(m_fill_mode.value()) << "\n";
          }
          break;

        case SDLK_k:
          {
            if (ev.key.keysym.mod & KMOD_SHIFT)
              {
                cycle_value(m_stroke_filter.value(),
                            ev.key.keysym.mod & (KMOD_ALT),
                            astral::number_filter_modes);
                std::cout << "Stroke mask filter set to " << astral::label(m_stroke_filter.value()) << "\n";
              }
            else if (ev.key.keysym.mod & KMOD_CTRL)
              {
                cycle_value(m_layer_filter.value(),
                            ev.key.keysym.mod & (KMOD_ALT),
                            astral::number_filter_modes);
                std::cout << "Layer filter set to " << astral::label(m_layer_filter.value()) << "\n";
              }
            else
              {
                cycle_value(m_fill_filter.value(),
                            ev.key.keysym.mod & (KMOD_ALT),
                            astral::number_filter_modes);
                std::cout << "Filter mask filter set to " << astral::label(m_fill_filter.value()) << "\n";
              }
          }
          break;

        case SDLK_r:
          cycle_value(m_sparse_fill.value(),
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      astral::number_fill_method_t);
          std::cout << "Filling with sparse mask set to: "
                    << astral::label(m_sparse_fill.value())
                    << "\n";
          break;

        case SDLK_a:
          cycle_value(m_fill_aa_mode.value(),
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      astral::number_anti_alias_modes);
          std::cout << "Fill anti-aliasing set to "
                    << astral::label(m_fill_aa_mode.value()) << "\n";
          break;

        case SDLK_o:
          if (ev.key.keysym.mod & KMOD_SHIFT)
            {
              m_use_direct_stroking.value() = !m_use_direct_stroking.value();
              std::cout << "Use direct stroking set to: "
                        << std::boolalpha << m_use_direct_stroking.value()
                        << "\n";
            }
          else
            {
              m_sparse_stroke.value() = !m_sparse_stroke.value();
              std::cout << "Stroking with sparse mask set to: "
                        << std::boolalpha << m_sparse_stroke.value()
                        << "\n";
            }
          break;

        case SDLK_0:
          if (ev.key.keysym.mod & KMOD_ALT)
            {
              if (ev.key.keysym.mod & KMOD_SHIFT)
                {
                  m_stroke_scale_factor.value() = 1.0f;
                  std::cout << "Stroke render scale factor set to " << m_stroke_scale_factor.value()
                            << "\n" << std::flush;
                }
              else if (ev.key.keysym.mod & KMOD_CTRL)
                {
                  m_layer_scale_factor.value() = 1.0f;
                  std::cout << "Layer render scale factor set to " << m_layer_scale_factor.value()
                            << "\n" << std::flush;
                }
              else
                {
                  m_fill_scale_factor.value() = 1.0f;
                  std::cout << "Fill render scale factor set to " << m_fill_scale_factor.value()
                            << "\n" << std::flush;
                }
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
              if (ev.key.keysym.mod & KMOD_SHIFT)
                {
                  m_stroke_scale_factor.value() = fv;
                  std::cout << "Stroke render scale factor set to " << m_stroke_scale_factor.value()
                            << "\n" << std::flush;
                }
              else if (ev.key.keysym.mod & KMOD_CTRL)
                {
                  m_layer_scale_factor.value() = fv;
                  std::cout << "Layer render scale factor set to " << m_layer_scale_factor.value()
                            << "\n" << std::flush;
                }
              else
                {
                  m_fill_scale_factor.value() = fv;
                  std::cout << "Fill render scale factor set to " << m_fill_scale_factor.value()
                            << "\n" << std::flush;
                }
            }
          break;
        }
    }
  render_engine_gl3_demo::handle_event(ev);
}

int
main(int argc, char **argv)
{
  SVGExample M;
  return M.main(argc, argv);
}

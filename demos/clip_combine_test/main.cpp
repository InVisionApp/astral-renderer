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

class ClipCombineTest:public render_engine_gl3_demo
{
public:
  ClipCombineTest(void);

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

  class PathTransform
  {
  public:
    enum scale_t:uint32_t
      {
        pre_rotate = 0,
        post_rotate,

        number_scale,
      };

    PathTransform(const std::string &label, command_line_register &reg):
      m_scales(astral::vec2(1.0f)),
      m_rotate_angle(0.0f),
      m_scale_pre_rotate_cmd(m_scales[pre_rotate], label + "_scale_pre_rotate", "scaling transformation to apply to path before rotation, formatted as ScaleX:SaleY", reg),
      m_scale_post_rotate_cmd(m_scales[post_rotate], label + "_scale_pre_rotate", "scaling transformation to apply to path after rotation, formatted as ScaleX:SaleY", reg),
      m_rotate_angle_cmd(m_rotate_angle, label + "_rotate_angle", "rotation of path in degrees to apply to path", reg)
    {}

    void
    apply(astral::RenderEncoderBase encoder) const
    {
      encoder.scale(m_scales[pre_rotate]);
      encoder.rotate(m_rotate_angle * ASTRAL_PI / 180.0f);
      encoder.scale(m_scales[post_rotate]);
    }

    void
    apply(astral::Transformation *tr) const
    {
      tr->scale(m_scales[pre_rotate]);
      tr->rotate(m_rotate_angle * ASTRAL_PI / 180.0f);
      tr->scale(m_scales[post_rotate]);
    }

    void
    reset(void)
    {
      m_scales[0] = m_scales[1] = astral::vec2(1.0f, 1.0f);
      m_rotate_angle = 0.0f;
    }

    astral::vecN<astral::vec2, number_scale> m_scales;
    float m_rotate_angle;

  private:
    command_line_argument_value<astral::vec2&> m_scale_pre_rotate_cmd, m_scale_post_rotate_cmd;
    command_line_argument_value<float&> m_rotate_angle_cmd;
  };

  float
  update_smooth_values(void);

  void
  reset_zoom_transformation(PanZoomTrackerSDLEvent *p);

  static
  bool
  load_path(const std::string &filename, astral::Path *dst);

  void
  draw_hud(astral::RenderEncoderSurface encoder, bool combine_mask_type_ignored, float frame_ms);

  void
  draw_ui_rect(astral::RenderEncoderSurface render_encoder,
               astral::RenderValue<astral::Brush> brush,
               astral::vec2 p);

  void
  draw_ui_rects_at_corners(astral::RenderEncoderSurface render_encoder,
                           astral::RenderValue<astral::Brush> brush,
                           const astral::BoundingBox<float> &bb);

  astral::FillParameters m_fill_params;
  astral::FillMaskProperties m_mask_fill_params;
  astral::MaskUsage m_mask_fill_usage_params;
  astral::RenderClipCombineParams m_combine_params;

  command_separator m_demo_options;
  command_line_argument_value<std::string> m_path_file;
  command_line_argument_value<std::string> m_clip_path_file;
  enumerated_command_line_argument_value<enum astral::mask_type_t> m_combine_mask_type;
  command_line_argument_value<UniformScaleTranslate<float>> m_initial_camera;
  command_line_argument_value<UniformScaleTranslate<float>> m_initial_path_view;
  command_line_argument_value<UniformScaleTranslate<float>> m_initial_clip_path_view;
  enumerated_command_line_argument_value<enum astral::fill_rule_t&> m_fill_params_fill_rule;
  enumerated_command_line_argument_value<enum astral::fill_method_t&> m_fill_params_fill_method;
  enumerated_command_line_argument_value<enum astral::mask_type_t&> m_fill_mask_type;
  enumerated_command_line_argument_value<enum astral::fill_rule_t&> m_combine_params_fill_rule;
  enumerated_command_line_argument_value<enum astral::fill_method_t&> m_combine_params_fill_method;

  PathTransform m_path_transform;
  PathTransform m_clip_path_transform;

  astral::reference_counted_ptr<astral::TextItem> m_text_item;
  astral::Path m_path;
  astral::Path m_clip_path;
  astral::Rect m_ui_rect;

  simple_time m_draw_timer;
  average_timer m_frame_time_average;
  PanZoomTrackerSDLEvent m_zoom, m_path_zoom, m_clip_path_zoom;

  unsigned int m_hud_mode;
  std::vector<unsigned int> m_prev_stats;
};

//////////////////////////////////////////
//  methods
ClipCombineTest::
ClipCombineTest(void):
  m_mask_fill_usage_params(astral::mask_type_distance_field),
  m_combine_params(astral::odd_even_fill_rule),
  m_demo_options("Demo Options", *this),
  m_path_file("", "path", "File from which to read the path", *this),
  m_clip_path_file("", "clip_path",
                  "File from which to read to read the clip-path", *this),
  m_combine_mask_type(astral::number_mask_type,
                      enumerated_string_type<enum astral::mask_type_t>(&astral::label, astral::number_mask_type)
                      .add_entry("inherit_from_fill", astral::number_mask_type, ""),
                      "combine_mask_type",
                      "specifies mask type to make/use when performing clipping",
                      *this),
  m_initial_camera(UniformScaleTranslate<float>(), "initial_camera", "initial camera", *this),
  m_initial_path_view(UniformScaleTranslate<float>(), "initial_path_view", "initial transformation applied to path", *this),
  m_initial_clip_path_view(UniformScaleTranslate<float>(), "initial_clip_path_view", "initial transformation applied to clipping path", *this),
  m_fill_params_fill_rule(m_fill_params.m_fill_rule,
                          enumerated_string_type<enum astral::fill_rule_t>(&astral::label, astral::number_fill_rule),
                          "path_fill_rule",
                          "initial fill rule to apply to path",
                          *this),
  m_fill_params_fill_method(m_mask_fill_params.m_sparse_mask,
                            enumerated_string_type<enum astral::fill_method_t>(&astral::label, astral::number_fill_method_t),
                            "path_fill_method",
                            "method for generating fill mask of path",
                            *this),
  m_fill_mask_type(m_mask_fill_usage_params.m_mask_type,
                   enumerated_string_type<enum astral::mask_type_t>(&astral::label, astral::number_mask_type),
                   "path_fill_mask_type",
                   "specifies the kind of mask to use when filling the path",
                   *this),
  m_combine_params_fill_rule(m_combine_params.m_fill_rule,
                             enumerated_string_type<enum astral::fill_rule_t>(&astral::label, astral::number_fill_rule),
                             "clip_path_fill_rule",
                             "initial fill rule to apply to clip path",
                             *this),
  m_combine_params_fill_method(m_combine_params.m_sparse,
                               enumerated_string_type<enum astral::fill_method_t>(&astral::label, astral::number_fill_method_t),
                               "combine_fill_method",
                               "method for generating fill mask of clipping computation",
                               *this),
  m_path_transform("path", *this),
  m_clip_path_transform("clip_path", *this),
  m_frame_time_average(1000u),
  m_hud_mode(basic_hud)
{
  std::cout << "Controls:"
            << "\n\tspace: cycle through HUD modes"
            << "\n\tshift-space: toggle showing frame rate to console"
            << "\n\tp: print current values"
            << "\n\tq: reset transformation applied to the path"
            << "\n\tw: reset transformation applied to the clip path"
            << "\n\tr: cycle through different fill rules"
            << "\n\tg: cycle through different mask types"
            << "\n\tctrl-r: cycle through different fill rules on clip path"
            << "\n\ts: cycle through different sparseness for path"
            << "\n\tctrl-s: cycle through different sparseness for clip-path"
            << "\n\talt-r: cycle through different clip combine implementations"
            << "\n\talt + 1,2, ... 9: set render fill scale factor to 10%, 20%, ..., 90% repsectively"
            << "\n\talt + 0: set render fill scale factor to 100%"
            << "\n\t4: increase horizontal pre-rotate scale of path (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\tctrl-4: decrease horizontal pre-rotate scale of path  (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\t5: increase vertical pre-rotate scale of path  (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\tctrl-5: decrease vertical pre-rotate scale of path  (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + 5: increase horizontal post-rotate scale of path  (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + ctrl-5: decrease horizontal post-rotate scale of path  (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + 5: increase vertical post-rotate scale of path  (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + ctrl-5: decrease vertical post-rotate scale of path  (hold left-shit for slow change, hold right-shift for faster change)"

            << "\n\t6: increase horizontal pre-rotate scale of clip-path (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\tctrl-6: decrease horizontal pre-rotate scale of clip-path  (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\t7: increase vertical pre-rotate scale of clip-path  (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\tctrl-7: decrease vertical pre-rotate scale of clip-path  (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + 6: increase horizontal post-rotate scale of clip-path  (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + ctrl-6: decrease horizontal post-rotate scale of clip-path  (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + 7: increase vertical post-rotate scale of clip-path  (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + ctrl-7: decrease vertical post-rotate scale of clip-path  (hold left-shit for slow change, hold right-shift for faster change)"

            << "\n\t9/0 increase/decrease angle of rotation of path"
            << "\n\tctrl-9/0 increase/decrease angle of rotation of clip-path"
            << "\n\tLeft Mouse Drag: pan"
            << "\n\tHold Left Mouse, then drag up/down: zoom out/in"
            << "\n\tRight Mouse: move path"
            << "\n\tMiddle Mouse: move clip-path"
            << "\n";
}

void
ClipCombineTest::
reset_zoom_transformation(PanZoomTrackerSDLEvent *p)
{
  /* Initialize zoom location to be identity */
  p->transformation(UniformScaleTranslate<float>());
}

bool
ClipCombineTest::
load_path(const std::string &filename, astral::Path *dst)
{
  std::ifstream path_file(filename.c_str());
  if (path_file)
    {
      read_path(dst, path_file);
      return true;
    }

  return false;
}

void
ClipCombineTest::
init_gl(int w, int h)
{
  m_prev_stats.resize(renderer().stats_labels().size(), 0);

  float pixel_size(32.0f);
  astral::Font font(default_typeface(), pixel_size);
  m_text_item = astral::TextItem::create(font);

  if (!load_path(m_path_file.value(), &m_path))
    {
      //const char *default_path =
      //"[ (50.0, 185.0) [[(60.0, 200.0) ]] (70.0, 185.0)\n"
      //"arc 180 (70.0, 50.0)\n"
      //"[[ (60.0, 0.0) (30.0, 100.0) ]]\n"
      //"(0.0, 50.0) arc 90 ]\n"
      //;
      const char *default_path =
        "[(200 1000) (1000 1000) (200 500)]"
        ;

      read_path(&m_path, default_path);
    }

  if (!load_path(m_clip_path_file.value(), &m_clip_path))
    {
      const char *default_path =
        "[(200 1000) (1000 1000) (1000 500)]"
        ;

      read_path(&m_clip_path, default_path);
    }

  float ui_size(25.0f);
  m_ui_rect
    .min_point(-0.5f * ui_size, -0.5f * ui_size)
    .max_point(+0.5f * ui_size, +0.5f * ui_size);

  ASTRALunused(w);
  ASTRALunused(h);

  m_zoom.transformation(m_initial_camera.value());
  m_path_zoom.transformation(m_initial_path_view.value());
  m_clip_path_zoom.transformation(m_initial_clip_path_view.value());
}

void
ClipCombineTest::
draw_hud(astral::RenderEncoderSurface encoder, bool combine_mask_type_ignored, float frame_ms)
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
       << "[alt-0 .. alt-9] Render Scale Factor: " << m_mask_fill_params.m_render_scale_factor << "\n"

       << "[ctrl-r] Path FillRule: " << astral::label(m_fill_params.m_fill_rule) << "\n"
       << "[r] CombinePath FillRule: " << astral::label(m_combine_params.m_fill_rule) << "\n"

       << "[ctrl-s] Path Sparseness: " << astral::label(m_mask_fill_params.m_sparse_mask) << "\n"
       << "[s] CombinePath Sparseness: " << astral::label(m_combine_params.m_sparse) << "\n"

       << "[g] FillMaskType: " << astral::label(m_mask_fill_usage_params.m_mask_type) << "\n"
       << "[h] CombineMaskType: ";

  if (m_combine_mask_type.value() != astral::number_mask_type)
    {
      ostr << astral::label(m_combine_mask_type.value());
      if (combine_mask_type_ignored)
        {
          ostr << " unsupported by fill, using fill type instead";
        }
    }
  else
    {
      ostr << "inherit from fill\n";
    }

  ostr << "\nAverage over " << m_frame_time_average.interval_ms() << " ms: "
       << m_frame_time_average.average_elapsed_ms()
       << m_frame_time_average.parity_string() << "\n"
       << "\n\n"
       << "StartPath = white corners, ClipPath = green corners\n"
       << "Blue: Intersect(StartPath, ClipPath)\n"
       << "Red: StartPath \\ ClipPath\n";

  /* draw the HUD in fixed location */
  encoder.transformation(astral::Transformation());
  set_and_draw_hud(encoder, frame_ms,
                   astral::make_c_array(m_prev_stats),
                   *m_text_item, ostr.str(),
                   vs_p, bvs_p, gvs_p);
}

void
ClipCombineTest::
draw_ui_rect(astral::RenderEncoderSurface render_encoder,
             astral::RenderValue<astral::Brush> brush,
             astral::vec2 p)
{
  render_encoder.save_transformation();
  p = render_encoder.transformation().apply_to_point(p);
  render_encoder.transformation(astral::Transformation().translate(p));
  render_encoder.draw_rect(m_ui_rect, false, brush);
  render_encoder.restore_transformation();
}

void
ClipCombineTest::
draw_ui_rects_at_corners(astral::RenderEncoderSurface render_encoder,
                         astral::RenderValue<astral::Brush> brush,
                         const astral::BoundingBox<float> &bb)
{
  draw_ui_rect(render_encoder, brush, bb.as_rect().point(astral::Rect::minx_miny_corner));
  draw_ui_rect(render_encoder, brush, bb.as_rect().point(astral::Rect::minx_maxy_corner));
  draw_ui_rect(render_encoder, brush, bb.as_rect().point(astral::Rect::maxx_miny_corner));
  draw_ui_rect(render_encoder, brush, bb.as_rect().point(astral::Rect::maxx_maxy_corner));
}

void
ClipCombineTest::
draw_frame(void)
{
  float frame_ms;
  astral::Transformation tr;
  astral::RenderEncoderSurface render_encoder;
  astral::c_array<const unsigned int> stats;
  astral::MaskDetails path_mask;
  astral::reference_counted_ptr<const astral::RenderClipElement> path_clip_element;
  astral::reference_counted_ptr<const astral::RenderClipCombineResult> final_clip_element;
  astral::RenderValue<astral::Brush> red, green, blue, white;
  bool combine_mask_type_ignored(false);

  m_frame_time_average.increment_counter();
  frame_ms = update_smooth_values();
  tr = m_zoom.transformation().astral_transformation();

  render_encoder = renderer().begin(render_target());

  render_encoder.transformation(tr);

  red = renderer().create_value(astral::Brush().base_color(astral::vec4(1.0f, 0.0f, 0.0f, 0.5f)));
  green = renderer().create_value(astral::Brush().base_color(astral::vec4(0.0f, 1.0f, 0.0f, 0.5f)));
  blue = renderer().create_value(astral::Brush().base_color(astral::vec4(0.0f, 0.0f, 1.0f, 0.5f)));
  white = renderer().create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 1.0f, 1.0f)));

  render_encoder.save_transformation();
  render_encoder.concat(m_path_zoom.transformation().astral_transformation());
  m_path_transform.apply(render_encoder);
  draw_ui_rects_at_corners(render_encoder, white, m_path.bounding_box());
  render_encoder.generate_mask(m_path, m_fill_params,
                               m_mask_fill_params,
                               m_mask_fill_usage_params.m_mask_type,
                               &path_mask, &path_clip_element);
  render_encoder.restore_transformation();

  render_encoder.save_transformation();
  render_encoder.concat(m_clip_path_zoom.transformation().astral_transformation());
  m_clip_path_transform.apply(render_encoder);
  draw_ui_rects_at_corners(render_encoder, green, m_clip_path.bounding_box());
  final_clip_element = render_encoder.combine_clipping(*path_clip_element, m_clip_path, m_combine_params);
  render_encoder.restore_transformation();

  if (m_combine_mask_type.value() != astral::number_mask_type)
    {
      astral::reference_counted_ptr<const astral::RenderClipCombineResult> tmp;

      tmp = final_clip_element->as_mask_type(m_combine_mask_type.value());
      if (tmp)
        {
          final_clip_element = tmp;
        }
      else
        {
          combine_mask_type_ignored = true;
        }
    }

  #if 0
    {
      if (final_clip_element->clip_in() && final_clip_element->clip_in()->mask_details())
        {
          render_encoder.draw_mask(*final_clip_element->clip_in()->mask_details(), astral::filter_linear, blue);
        }

      if (final_clip_element->clip_out() && final_clip_element->clip_out()->mask_details())
        {
          render_encoder.draw_mask(*final_clip_element->clip_out()->mask_details(), astral::filter_linear, red);
        }
    }
  #elif 0
    {
      astral::RenderClipNode clip_encoders;
      astral::BoundingBox<float> bbI, bbC, bb;
      astral::Transformation pixel_tr_path, pixel_tr_clip_path;

      pixel_tr_path = (m_zoom.transformation() * m_path_zoom.transformation()).astral_transformation();
      m_path_transform.apply(&pixel_tr_path);

      pixel_tr_clip_path = (m_zoom.transformation() * m_clip_path_zoom.transformation()).astral_transformation();
      m_clip_path_transform.apply(&pixel_tr_clip_path);

      bbI = pixel_tr_path.apply_to_bb(m_path.bounding_box());
      bbC = pixel_tr_clip_path.apply_to_bb(m_clip_path.bounding_box());
      bb.union_box(bbI);
      bb.union_box(bbC);

      render_encoder.transformation(astral::Transformation());
      clip_encoders = render_encoder.begin_clip_node_pixel(astral::blend_porter_duff_src_over,
                                                           astral::RenderEncoderBase::clip_node_both,
                                                           *final_clip_element, bb, bb, astral::filter_linear);

      clip_encoders.m_clip_in.draw_rect(bb.as_rect(), blue);
      clip_encoders.m_clip_out.draw_rect(bb.as_rect(), red);

      render_encoder.end_clip_node(clip_encoders);
    }
  #elif 1
    {
      render_encoder.transformation(tr);

      render_encoder.save_transformation();
        {
          astral::ItemMaterial material(blue, final_clip_element->clip_in());

          render_encoder.concat(m_path_zoom.transformation().astral_transformation());
          m_path_transform.apply(render_encoder);
          render_encoder.draw_rect(m_path.bounding_box().as_rect(), false, material);
        }
      render_encoder.restore_transformation();

      render_encoder.save_transformation();
        {
          astral::ItemMaterial material(red, final_clip_element->clip_out());

          render_encoder.concat(m_path_zoom.transformation().astral_transformation());
          m_path_transform.apply(render_encoder);
          render_encoder.draw_rect(m_path.bounding_box().as_rect(), false, material);
        }
      render_encoder.restore_transformation();
    }
  #else
    {
      render_encoder.transformation(astral::Transformation());
      if (final_clip_element->clip_in()->mask_details())
        {
          render_encoder.draw_rect(final_clip_element->clip_in()->mask_details()->pixel_rect().as_rect(), false, material);
        }
      if (final_clip_element->clip_out()->mask_details())
        {
          render_encoder.draw_rect(final_clip_element->clip_out()->mask_details()->pixel_rect().as_rect(), false, material);
        }
    }
  #endif


  if (!pixel_testing())
    {
      draw_hud(render_encoder, combine_mask_type_ignored, frame_ms);
    }

  stats = renderer().end();
  ASTRALassert(m_prev_stats.size() == stats.size());
  std::copy(stats.begin(), stats.end(), m_prev_stats.begin());
}

float
ClipCombineTest::
update_smooth_values(void)
{
  const Uint8 *keyboard_state = SDL_GetKeyboardState(nullptr);
  float return_value, delta, scale_delta, angle_delta;
  enum PathTransform::scale_t which_scale;
  astral::c_string scale_txt(nullptr);
  bool scale_factor_changed(false);
  const float scale_rate(0.0001f);
  float scale_factor_delta(0.0f);
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

  if (keyboard_state[SDL_SCANCODE_UP])
    {
      scale_factor_delta += delta * scale_rate;
      scale_factor_changed = true;
    }

  if (keyboard_state[SDL_SCANCODE_DOWN])
    {
      scale_factor_delta -= delta * scale_rate;
      scale_factor_changed = true;
    }

  if (scale_factor_changed)
    {
      m_mask_fill_params.m_render_scale_factor.m_scale_factor += scale_factor_delta;
      m_mask_fill_params.m_render_scale_factor = astral::t_max(0.0f, m_mask_fill_params.m_render_scale_factor.m_scale_factor);
      std::cout << "Fill path scale factor set to "
                << m_mask_fill_params.m_render_scale_factor
                << "\n" << std::flush;
    }

  scale_delta = 0.01f * delta;
  angle_delta = 0.0025f * 180.0f / ASTRAL_PI * delta;
  if (keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
    {
      scale_delta = -scale_delta;
    }

  if (keyboard_state[SDL_SCANCODE_RETURN])
    {
      which_scale = PathTransform::post_rotate;
      scale_txt = "post-rotate-scale";
    }
  else
    {
      which_scale = PathTransform::pre_rotate;
      scale_txt = "pre-rotate-scale";
    }

  if (keyboard_state[SDL_SCANCODE_6] && !alt_held)
    {
      m_clip_path_transform.m_scales[which_scale].x() += scale_delta;
      std::cout << "clip-path " << scale_txt << " set to: " << m_clip_path_transform.m_scales[which_scale] << "\n";
    }
  if (keyboard_state[SDL_SCANCODE_7] && !alt_held)
    {
      m_clip_path_transform.m_scales[which_scale].y() += scale_delta;
      std::cout << "clip-path " << scale_txt << " set to: " << m_clip_path_transform.m_scales[which_scale] << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_4] && !alt_held)
    {
      m_path_transform.m_scales[which_scale].x() += scale_delta;
      std::cout << "path " << scale_txt << " set to: " << m_path_transform.m_scales[which_scale] << "\n";
    }
  if (keyboard_state[SDL_SCANCODE_5] && !alt_held)
    {
      m_path_transform.m_scales[which_scale].y() += scale_delta;
      std::cout << "path " << scale_txt << " set to: " << m_path_transform.m_scales[which_scale] << "\n";
    }

  if ((keyboard_state[SDL_SCANCODE_0] || keyboard_state[SDL_SCANCODE_9]) && !alt_held)
    {
      PathTransform *tr;
      astral::c_string tr_txt;

      if (keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
        {
          tr = &m_clip_path_transform;
          tr_txt = "Clip-path";
        }
      else
        {
          tr = &m_path_transform;
          tr_txt = "Path";
        }

      if (keyboard_state[SDL_SCANCODE_0])
        {
          tr->m_rotate_angle -= angle_delta;
          if (tr->m_rotate_angle < 0.0f)
            {
              tr->m_rotate_angle += 360.0f;
            }
        }
      else
        {
          tr->m_rotate_angle += angle_delta;
          if (tr->m_rotate_angle > 360.0f)
            {
              tr->m_rotate_angle -= 360.0f;
            }
        }
      std::cout << tr_txt << " angle set to: " << tr->m_rotate_angle << " degrees\n";
    }

  return return_value;
}

void
ClipCombineTest::
handle_event(const SDL_Event &ev)
{
  UniformScaleTranslate<float> sc;

  m_zoom.handle_event(ev, SDL_BUTTON_LEFT);

  /* have the path and clip-path zoom handlers transform the event
   * position to take into accout the camera transformation
   * from m_zoom
   */
  sc = m_zoom.transformation().inverse();
  m_path_zoom.m_scale_event = m_clip_path_zoom.m_scale_event = astral::vec2(sc.m_scale);
  m_path_zoom.m_scale_zooming = m_clip_path_zoom.m_scale_zooming = sc.m_scale;
  m_path_zoom.m_translate_event = m_clip_path_zoom.m_translate_event = sc.m_translation;

  m_path_zoom.handle_event(ev, SDL_BUTTON_RIGHT);
  m_clip_path_zoom.handle_event(ev, SDL_BUTTON_MIDDLE);

  if (ev.type == SDL_KEYDOWN)
    {
      switch(ev.key.keysym.sym)
        {
        case SDLK_p:
          std::cout << "Current values:"
                    << "\n\tcamera: " << m_zoom.transformation()
                    << "\n\tpath_view: " << m_path_zoom.transformation()
                    << "\n\tpath_transformation:"
                    << "\n\t\tscale_pre_rotate: " << m_path_transform.m_scales[PathTransform::pre_rotate]
                    << "\n\t\trotate: " << m_path_transform.m_rotate_angle
                    << "\n\t\tscale_post_rotate: " << m_path_transform.m_scales[PathTransform::post_rotate]
                    << "\n\tclip_path_view: " << m_clip_path_zoom.transformation()
                    << "\n\tclip_path_transformation:"
                    << "\n\t\tscale_pre_rotate: " << m_clip_path_transform.m_scales[PathTransform::pre_rotate]
                    << "\n\t\trotate: " << m_clip_path_transform.m_rotate_angle
                    << "\n\t\tscale_post_rotate: " << m_clip_path_transform.m_scales[PathTransform::post_rotate]
                    << "\n";
          break;

        case SDLK_q:
          m_path_transform.reset();
          reset_zoom_transformation(&m_path_zoom);
          break;

        case SDLK_w:
          m_clip_path_transform.reset();
          reset_zoom_transformation(&m_clip_path_zoom);
          break;

        case SDLK_e:
          reset_zoom_transformation(&m_zoom);
          break;

        case SDLK_r:
          {
            enum astral::fill_rule_t *p;
            astral::c_string p_txt;

            p = (ev.key.keysym.mod & KMOD_CTRL) ?
              &m_fill_params.m_fill_rule:
              &m_combine_params.m_fill_rule;

            p_txt = (ev.key.keysym.mod & KMOD_CTRL) ?
              "path" :
              "clip-path";

            cycle_value(*p, ev.key.keysym.mod & KMOD_SHIFT, astral::number_fill_rule);
            std::cout << p_txt << " fill rule set to " << astral::label(*p) << "\n";
          }
          break;

        case SDLK_s:
          {
            enum astral::fill_method_t *p;
            astral::c_string p_txt;

            p = (ev.key.keysym.mod & KMOD_CTRL) ?
              &m_mask_fill_params.m_sparse_mask:
              &m_combine_params.m_sparse;

            p_txt = (ev.key.keysym.mod & KMOD_CTRL) ?
              "path" :
              "clip-path";

            cycle_value(*p, ev.key.keysym.mod & KMOD_SHIFT, astral::number_fill_method_t);
            std::cout << p_txt << " spraseness set to " << astral::label(*p) << "\n";
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

        case SDLK_g:
          cycle_value(m_mask_fill_usage_params.m_mask_type,
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      astral::number_mask_type);
          std::cout << "Mask type set to "
                    << astral::label(m_mask_fill_usage_params.m_mask_type)
                    << "\n";
          break;

        case SDLK_h:
          cycle_value(m_combine_mask_type.value(),
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      astral::number_mask_type + 1);

          std::cout << "Combine mask type set to ";
          if (m_combine_mask_type.value() != astral::number_mask_type)
            {
              std::cout << astral::label(m_combine_mask_type.value());
            }
          else
            {
              std::cout << "inherit from fill mask\n";
            }
          std::cout << "\n";
          break;

        case SDLK_SPACE:
          cycle_value(m_hud_mode, false, number_hud_modes);
          break;

        case SDLK_0:
          if (ev.key.keysym.mod & KMOD_ALT)
            {
              m_mask_fill_params.m_render_scale_factor = 1.0f;
              std::cout << "Fill path render scale factor set to "
                        << m_mask_fill_params.m_render_scale_factor
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
              m_mask_fill_params.m_render_scale_factor = fv;
              std::cout << "Fill path render scale factor set to "
                        << m_mask_fill_params.m_render_scale_factor
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
  ClipCombineTest M;
  return M.main(argc, argv);
}

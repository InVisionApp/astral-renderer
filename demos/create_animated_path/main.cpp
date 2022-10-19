/*!
 * \file main.cpp
 * \brief main.cpp
 *
 * Copyright 2021 by InvisionApp.
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
#include "read_path.hpp"

#include "animated_path_document.hpp"

class CreateAnimtedPathDemo:public render_engine_gl3_demo
{
public:
  CreateAnimtedPathDemo(void);

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
  enum mode_t
    {
      preview_mode,
      edit_mode,

      number_modes
    };

  enum contour_classification_t
    {
      paired_contour,
      unparied_contour,
      collapsed_contour,
    };

  enum text_item_t
    {
      left_mouse_command,
      right_clip_command, /* also shows what contour and point is selected */
      d_delete_point_command, /* also shows that middle mouse drag selects point */
      p_pair_contour_command,
      u_unpair_contour_command,
      ctrl_u_collapse_contour_command,
      q_select_pair_command,
      r_reverse_contour_command,
      a_add_point_command,
      shift_d_delete_all_points_command,
      y_set_anchor_point_command,
      shift_u_clear_all_contour_pairings_command,
      s_save_command,
      m_toggle_view_command,

      text_item_number_commands
    };

  enum stroke_mode_t
    {
      stroke_logical_coordinates,
      stroke_pixel_coordinates,
      stroke_none,

      number_stroke_modes
    };

  class Panes
  {
  public:
    explicit
    Panes(astral::ivec2 dims);

    int
    which_pane(astral::ivec2 pt);

    void
    render_boundaries(astral::RenderEncoderBase encoder, int selected_pane) const;

    astral::vecN<astral::BoundingBox<float>, 2> m_path_panes;
    astral::BoundingBox<float> m_bottom_pane;
  };

  static
  astral::c_string
  label(enum mode_t mode);

  static
  astral::c_string
  label(enum AnimatedPathDocument::path_t path)
  {
    ASTRALassert(path == AnimatedPathDocument::start_path || path == AnimatedPathDocument::end_path);
    return (path == AnimatedPathDocument::start_path) ?
      "start_path" :
      "end_path";
  }

  void
  load_path(const std::string &filename, astral::Path *out_path);

  astral::return_code
  load_path_from_glyph(const std::string &filename, astral::Path *out_path);

  astral::return_code
  load_path_from_file(const std::string &filename, astral::Path *out_path);

  float
  update_preview_continuous_parameters(void);

  float
  compute_animation_interpolate(void);

  void
  handle_event_preview_mode(const SDL_Event &ev);

  void
  handle_event_edit_mode(const SDL_Event &ev);

  static
  void
  render_clipped_rect(astral::RenderEncoderBase encoder,
                      const astral::BoundingBox<float> &pane,
                      const astral::Rect &R,
                      astral::RenderValue<astral::Brush> brush);

  static
  void
  render_anchor_point(astral::RenderEncoderBase encoder,
                      const astral::BoundingBox<float> &pane,
                      const astral::vec2 &pt,
                      astral::RenderValue<astral::Brush> brush);

  static
  void
  render_point(astral::RenderEncoderBase encoder,
               const astral::BoundingBox<float> &pane,
               const astral::vec2 &pt,
               astral::RenderValue<astral::Brush> outer,
               astral::RenderValue<astral::Brush> inner);

  static
  void
  render_points(astral::RenderEncoderBase encoder,
                const astral::BoundingBox<float> &pane,
                const astral::Transformation &tr,
                astral::c_array<const AnimatedPathDocument::ContourPoint> pts);

  void
  render_path(astral::RenderEncoderBase encoder,
              const astral::BoundingBox<float> &pane,
              enum AnimatedPathDocument::path_t path);

  void
  render_hud(astral::RenderEncoderBase encoder,
             const astral::BoundingBox<float> pane);

  void
  draw_preview_hud(astral::RenderEncoderBase encoder);

  static
  astral::c_string
  label(enum stroke_mode_t m)
  {
    static astral::c_string labels[] =
      {
        [stroke_logical_coordinates] = "stroke_logical_coordinates",
        [stroke_pixel_coordinates] = "stroke_pixel_coordinates",
        [stroke_none] = "stroke_none",
      };

    return labels[m];
  }

  static const float STROKE_WIDTH;
  static const float INNER_BOX_RADIUS;
  static const float OUTER_BOX_RADIUS;
  static const float SELECTED_BOX_RADIUS;
  static const float PANE_BOUNDARY_RADIUS;
  static const float PANE_SELECTED_RADIUS;

  command_separator m_demo_options;
  command_line_argument_value<std::string> m_start_path, m_end_path;
  command_separator m_path_decriptor;
  command_line_argument_value<std::string> m_load, m_save;
  command_line_argument_value<unsigned int> m_animation_time;

  astral::reference_counted_ptr<AnimatedPathDocument> m_document;
  astral::vecN<astral::reference_counted_ptr<astral::TextItem>, text_item_number_commands> m_text_items;
  astral::reference_counted_ptr<astral::TextItem> m_preview_text;
  std::vector<enum text_item_t> m_visible_text_items;

  enum mode_t m_mode;

  /* state for edit mode */
  astral::vecN<PanZoomTrackerSDLEvent, 2> m_zooms;
  int m_current_pane;
  astral::vecN<int, 2> m_selected_contour;
  AnimatedPathDocument::PointIndex m_selected_point;

  /* state for preview mode */
  PanZoomTrackerSDLEvent m_preview_zoom;
  simple_time m_path_time, m_draw_timer;
  enum stroke_mode_t m_stroke_mode;
  bool m_show_preview_commands;
  astral::StrokeParameters m_stroke_params;
  astral::StrokeMaskProperties m_mask_stroke_params;

  astral::FillParameters m_fill_params;
  astral::FillMaskProperties m_mask_fill_params;

  astral::vecN<std::vector<const astral::Path*>, 3> m_tmp_paths;
  astral::vecN<std::vector<astral::vec2>, 3> m_tmp_translates;
  astral::vecN<std::vector<astral::float2x2>, 3> m_tmp_matrices;
  std::vector<astral::vec2> m_tmp_pts;
};

const float CreateAnimtedPathDemo::STROKE_WIDTH = 5.0f;
const float CreateAnimtedPathDemo::INNER_BOX_RADIUS = 5.0f;
const float CreateAnimtedPathDemo::OUTER_BOX_RADIUS = 10.0f;
const float CreateAnimtedPathDemo::SELECTED_BOX_RADIUS = 14.0f;
const float CreateAnimtedPathDemo::PANE_BOUNDARY_RADIUS = 5.0f;
const float CreateAnimtedPathDemo::PANE_SELECTED_RADIUS = 2.0f;

//////////////////////////////////
// CreateAnimtedPathDemo::Panes methods
CreateAnimtedPathDemo::Panes::
Panes(astral::ivec2 dims)
{
  astral::vec2 fdims(dims);

  /* UI setup, by hand.
   *
   * Bottom 25% is the "HUD". Specifies mode and shows key commands. We could also make the HUD an overlay rect maybe.
   *
   * Left 50%/Top75% is the start path
   * Right 50%/Top75% is the end path
   *
   * Clipping for the path drawing relies on pixel coordinate clipping
   * until Astral's clip-stack interface is implemented.
   */

  m_path_panes[AnimatedPathDocument::start_path].union_point(astral::vec2(0.0f, 0.0f) * fdims);
  m_path_panes[AnimatedPathDocument::start_path].union_point(astral::vec2(0.5f, 0.75f) * fdims);

  m_path_panes[AnimatedPathDocument::end_path].union_point(astral::vec2(0.5f, 0.0f) * fdims);
  m_path_panes[AnimatedPathDocument::end_path].union_point(astral::vec2(1.0f, 0.75f) * fdims);

  m_bottom_pane.union_point(astral::vec2(0.0f, 0.75f) * fdims);
  m_bottom_pane.union_point(astral::vec2(1.0f, 1.0f) * fdims);
}

void
CreateAnimtedPathDemo::Panes::
render_boundaries(astral::RenderEncoderBase encoder, int path) const
{
  astral::RenderEncoderBase::AutoRestore auto_restore(encoder);
  astral::RenderValue<astral::Brush> white;
  astral::vec2 Rx(PANE_BOUNDARY_RADIUS, 0.0f);
  astral::vec2 Ry(0.0f, PANE_BOUNDARY_RADIUS);

  encoder.transformation(astral::Transformation());
  white = encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 1.0f, 1.0f)));

  encoder.draw_rect(astral::Rect()
                    .min_point(m_bottom_pane.min_point() - Ry)
                    .max_point(m_bottom_pane.as_rect().point(astral::Rect::maxx_miny_corner) + Ry),
                    white);

  encoder.draw_rect(astral::Rect()
                    .min_point(m_path_panes[1].min_point() - Rx)
                    .max_point(m_path_panes[1].as_rect().point(astral::Rect::minx_maxy_corner) + Rx),
                    white);

  if (path != -1)
    {
      astral::BoundingBox<float> bb;
      astral::RenderValue<astral::Brush> red;

      red = encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 0.0f, 0.0f, 1.0f)));
      Rx.x() = (path == 0) ? -PANE_SELECTED_RADIUS : PANE_SELECTED_RADIUS;
      Ry.y() = -PANE_SELECTED_RADIUS;

      /* center line */
      bb.clear();
      bb.union_point(m_path_panes[1].min_point());
      bb.union_point(m_path_panes[1].min_point() + Rx);
      bb.union_point(m_path_panes[1].as_rect().point(astral::Rect::minx_maxy_corner));
      bb.union_point(m_path_panes[1].as_rect().point(astral::Rect::minx_maxy_corner) + Rx);
      encoder.draw_rect(bb.as_rect(), red);

      /* bottom line */
      bb.clear();
      bb.union_point(m_path_panes[path].as_rect().point(astral::Rect::minx_maxy_corner));
      bb.union_point(m_path_panes[path].as_rect().point(astral::Rect::minx_maxy_corner) + Ry);
      bb.union_point(m_path_panes[path].as_rect().point(astral::Rect::maxx_maxy_corner));
      bb.union_point(m_path_panes[path].as_rect().point(astral::Rect::maxx_maxy_corner) + Ry);
      encoder.draw_rect(bb.as_rect(), red);
    }
}

int
CreateAnimtedPathDemo::Panes::
which_pane(astral::ivec2 pt)
{
  astral::vec2 fpt(pt);

  for (unsigned int i = 0; i < 2; ++i)
    {
      if (m_path_panes[i].contains(fpt))
        {
          return i;
        }
    }

  return -1;
}

//////////////////////////////////
// CreateAnimtedPathDemo methods
CreateAnimtedPathDemo::
CreateAnimtedPathDemo(void):
  m_demo_options("Demo Options", *this),
  m_start_path("demo_data/paths/insane_path.txt", "start_path","Start path", *this),
  m_end_path("demo_data/paths/insane_path_curved.txt", "end_path", "End path", *this),
  m_path_decriptor("Both start_path and end_path can be a path-file as used by "
                   "all other demos (see demo_data/paths) or a glyph from a font. "
                   "For the latter, the format is one of the following:\n"
                   "\tfile_name:I:@G load glyph code G from face index I from font file_name\n"
                   "\tfile_name:@G   load glyph code G from face index 0 from font file_name\n"
                   "\tfile_name:I:C  load glyph with character -code- C where C is an integer from face index I from font file_name\n"
                   "\tfile_name:C    load glyph with character -code- C where C is an integer from face index 0 from font file_name\n"
                   "\tfile_name:I:%c load glyph with character c where c is an ascii character from face index I from font file_name\n"
                   "\tfile_name:%c   load glyph with character c where C is an ascii character from face index 0 from font file_name\n",
                   *this),
  m_load("", "load", "If set, load previously made document", *this),
  m_save("animated_path.bin", "save", "File to which to save document", *this),
  m_animation_time(3000, "animation_time", "Time to animate path in ms", *this),
  m_mode(edit_mode),
  m_current_pane(-1),
  m_selected_contour(-1),
  m_stroke_mode(stroke_none),
  m_show_preview_commands(true)
{
}

astral::return_code
CreateAnimtedPathDemo::
load_path_from_glyph(const std::string &filename, astral::Path *out_path)
{
  astral::reference_counted_ptr<astral::Typeface> typeface;
  std::string::const_iterator iter, iter_which, iter_quote;
  unsigned int face, code;
  astral::GlyphIndex glyph_index;
  astral::Glyph glyph;
  bool is_glyph_code;
  const astral::Path *glyph_path;

  /* Glyph loading syntax:
   *   file.txt      --> path file read with read_path()
   *   file.XXX:I:@G --> load glyph code G from face index I of font file file.XXX
   *   file.XXX:@G   --> load glyph code G from face index 0 of font file file.XXX
   *   file.XXX:I:C  --> load character code C from face index I of font file file.XXX
   *   file.XXX:C    --> load character code C from face index 0 of font file file.XXX
   *   file.XXX:I:%C  --> load character C from face index I of font file file.XXX
   *   file.XXX:%C    --> load character C from face index 0 of font file file.XXX
   */

  /* if the argument has a ':'' in it, then it is a font file
   * and the text before the colon gives the font file.
   */
  iter = std::find(filename.begin(), filename.end(), ':');
  if (iter == filename.end())
    {
      return astral::routine_fail;
    }

  /* figure out which face */
  iter_which = std::find(iter + 1, filename.end(), ':');
  if (iter_which == filename.end())
    {
      face = 0u;
      iter_which = iter + 1;
    }
  else
    {
      std::string face_string(iter + 1, iter_which);

      face = std::atoi(face_string.c_str());
      ++iter_which;
    }

  /* figure out if glyph code or character code */
  if (iter_which == filename.end())
    {
      return astral::routine_fail;
    }

  if (*iter_which == '@')
    {
      is_glyph_code = true;
      ++iter_which;
    }
  else
    {
      is_glyph_code = false;
    }

  if (iter_which == filename.end())
    {
      return astral::routine_fail;
    }

  iter_quote = std::find(iter_which, filename.end(), '%');
  if (iter_quote == filename.end())
    {
      code = std::atoi(std::string(iter_which, filename.end()).c_str());
    }
  else
    {
      ++iter_quote;
      if (iter_quote == filename.end())
        {
          return astral::routine_fail;
        }
      code = *iter_quote;
    }

  typeface = create_typeface_from_file(face, std::string(filename.begin(), iter).c_str());
  if (typeface == &tofu_typeface())
    {
      return astral::routine_fail;
    }

  if (!is_glyph_code)
    {
      /* convert character code to glyph code */
      glyph_index = typeface->glyph_index(code);
    }
  else
    {
      glyph_index = astral::GlyphIndex(code);
    }

  /* fetch the glyph */
  glyph = typeface->fetch_glyph(glyph_index);

  if (!glyph.valid())
    {
      return astral::routine_fail;
    }

  /* now finally fetch the path */
  unsigned int layer(0);
  enum astral::fill_rule_t fill_rule;

  glyph_path = glyph.path(layer, &fill_rule);
  if (!glyph_path)
    {
      return astral::routine_fail;
    }

  *out_path = *glyph_path;
  return astral::routine_success;
}

astral::return_code
CreateAnimtedPathDemo::
load_path_from_file(const std::string &filename, astral::Path *out_path)
{
  std::ifstream path_file(filename.c_str());

  if (!path_file)
    {
      return astral::routine_fail;
    }

  read_path(out_path, path_file);
  return astral::routine_success;
}

void
CreateAnimtedPathDemo::
load_path(const std::string &filename, astral::Path *out_path)
{
  if (astral::routine_success == load_path_from_glyph(filename, out_path))
    {
      return;
    }

  if (astral::routine_success == load_path_from_file(filename, out_path))
    {
      return;
    }

  astral::RoundedRect rect;
  std::cerr << "Unable to load path from \"" << filename
            << "\", using a rounded rect for the path.\n";
  rect
    .corner_radii(4.0f)
    .min_point(-30.0f, -30.0f)
    .max_point(30.0f, 30.0f);

  out_path->add_rounded_rect(rect, astral::contour_direction_clockwise);
}

void
CreateAnimtedPathDemo::
init_gl(int, int)
{
  if (m_load.set_by_command_line())
    {
      std::ifstream in_file(m_load.value().c_str());

      if (in_file)
        {
          m_document = AnimatedPathDocument::load_from_file(in_file);
          if (m_document)
            {
              std::cout << "Loaded animation from \"" << m_load.value() << "\"\n";
            }
        }
    }

  if (!m_document)
    {
      astral::Path start_path, end_path;

      load_path(m_start_path.value(), &start_path);
      load_path(m_end_path.value(), &end_path);
      m_document = ASTRALnew AnimatedPathDocument(start_path, end_path);
    }

  /* init m_zooms to be centered at anchor points */
  for (unsigned int i = 0; i < 2; ++i)
    {
      UniformScaleTranslate<float> sc;
      enum AnimatedPathDocument::path_t P;

      P = static_cast<enum AnimatedPathDocument::path_t>(i);
      sc.m_translation = -m_document->anchor_point(P);
      m_zooms[P].transformation(sc);
    }

  m_path_time.restart();
  if (m_mode != preview_mode)
    {
      m_path_time.pause();
    }

  float pixel_size(32.0f);
  astral::Font font(default_typeface(), pixel_size);

  m_preview_text = astral::TextItem::create(font);
  for (unsigned int i = 0; i < text_item_number_commands; ++i)
    {
      m_text_items[i] = astral::TextItem::create(font);
    }

  add_text(0.0f, "Use Left-Drag to pan and zoom", *m_text_items[left_mouse_command]);
  add_text(0.0f, "[d]: Delete point", *m_text_items[d_delete_point_command]);
  add_text(0.0f, "[p]: pair selected contours", *m_text_items[p_pair_contour_command]);
  add_text(0.0f, "[u]: unpair selected contour", *m_text_items[u_unpair_contour_command]);
  add_text(0.0f, "[ctrl-u]: unpair selected contour and set to collapse to mouse location", *m_text_items[ctrl_u_collapse_contour_command]);
  add_text(0.0f, "[q]: select paired contour in other pane", *m_text_items[q_select_pair_command]);
  add_text(0.0f, "[r]: reverse selected contour", *m_text_items[r_reverse_contour_command]);
  add_text(0.0f, "[a]: add point at mouse location", *m_text_items[a_add_point_command]);
  add_text(0.0f, "[shift-d]: clear all points on selected contour", *m_text_items[shift_d_delete_all_points_command]);
  add_text(0.0f, "[y]: set anchor point to current mouse position", *m_text_items[y_set_anchor_point_command]);
  add_text(0.0f, "[shift-u]: clear all contour pairings", *m_text_items[shift_u_clear_all_contour_pairings_command]);
  add_text(0.0f, "[m]: toggle between edit and preview mode", *m_text_items[m_toggle_view_command]);

  std::ostringstream str;

  str << "[s]: save current document to " << m_save.value();
  add_text(0.0f, str.str(), *m_text_items[s_save_command]);
}

float
CreateAnimtedPathDemo::
compute_animation_interpolate(void)
{
  int32_t ms;
  float t;

  ms = m_path_time.elapsed() % (2 * m_animation_time.value());
  t = static_cast<float>(ms) / m_animation_time.value();
  t = astral::t_min(2.0f, astral::t_max(0.0f, t));
  t = (t > 1.0f) ? 2.0f - t : t;
  t = astral::t_min(1.0f, astral::t_max(0.0f, t));

  return t;
}

void
CreateAnimtedPathDemo::
render_clipped_rect(astral::RenderEncoderBase encoder,
                    const astral::BoundingBox<float> &pane,
                    const astral::Rect &R,
                    astral::RenderValue<astral::Brush> brush)
{
  astral::Rect clippedR;

  if (astral::Rect::compute_intersection(pane.as_rect(), R, &clippedR))
    {
      encoder.draw_rect(clippedR, false, brush);
    }
}

void
CreateAnimtedPathDemo::
render_anchor_point(astral::RenderEncoderBase encoder,
                    const astral::BoundingBox<float> &pane,
                    const astral::vec2 &pt,
                    astral::RenderValue<astral::Brush> brush)
{
  astral::vec2 R(INNER_BOX_RADIUS);
  render_clipped_rect(encoder, pane, astral::Rect().min_point(pt - R).max_point(pt + R), brush);
}

void
CreateAnimtedPathDemo::
render_point(astral::RenderEncoderBase encoder,
             const astral::BoundingBox<float> &pane,
             const astral::vec2 &pt,
             astral::RenderValue<astral::Brush> outer,
             astral::RenderValue<astral::Brush> inner)
{
  astral::vec2 O(OUTER_BOX_RADIUS);
  astral::vec2 I(INNER_BOX_RADIUS);

  render_clipped_rect(encoder, pane, astral::Rect().min_point(pt - O).max_point(pt + O), outer);
  render_clipped_rect(encoder, pane, astral::Rect().min_point(pt - I).max_point(pt + I), inner);
}

void
CreateAnimtedPathDemo::
render_points(astral::RenderEncoderBase encoder,
              const astral::BoundingBox<float> &pane,
              const astral::Transformation &tr,
              astral::c_array<const AnimatedPathDocument::ContourPoint> pts)
{
  astral::RenderEncoderBase::AutoRestore auto_restore(encoder);
  astral::RenderValue<astral::Brush> white, black, red, green, blue;

  white = encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 1.0f, 1.0f)));
  black = encoder.create_value(astral::Brush().base_color(astral::vec4(0.0f, 0.0f, 0.0f, 1.0f)));
  red   = encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 0.0f, 0.0f, 1.0f)));
  green = encoder.create_value(astral::Brush().base_color(astral::vec4(0.0f, 1.0f, 0.0f, 1.0f)));
  blue = encoder.create_value(astral::Brush().base_color(astral::vec4(0.0f, 0.0f, 1.0f, 1.0f)));

  if (!pts.empty())
    {
      render_point(encoder, pane, tr.apply_to_point(pts.front().m_position), red, green);
      pts.pop_front();
    }

  if (!pts.empty())
    {
      render_point(encoder, pane, tr.apply_to_point(pts.front().m_position), green, blue);
      pts.pop_front();
    }

  for (const AnimatedPathDocument::ContourPoint &pt : pts)
    {
      render_point(encoder, pane, tr.apply_to_point(pt.m_position), white, black);
    }
}

void
CreateAnimtedPathDemo::
render_path(astral::RenderEncoderBase encoder,
            const astral::BoundingBox<float> &pane,
            enum AnimatedPathDocument::path_t path)
{
  astral::RenderEncoderBase::AutoRestore auto_restore(encoder);
  astral::Transformation tr;
  astral::StrokeParameters stroke_params;
  astral::StrokeMaskProperties stroke_mask_params;
  astral::MaskUsage stroke_mask_usage_params(astral::mask_type_coverage);

  stroke_params
    .width(STROKE_WIDTH);

  stroke_mask_params
    .restrict_bb(&pane)
    .render_scale_factor(1.0f);

  /* make the center point of the pane be (0, 0) in logical */
  encoder.translate(pane.as_rect().center_point());

  /* apply the correct pan-zoom */
  encoder.concat(m_zooms[path].transformation().astral_transformation());

  if (m_mode == preview_mode)
    {
      /* the path animation includes the anchor point, so weed to untranslate by it */
      encoder.translate(m_document->anchor_point(path));
    }

  /* extract the transformation */
  tr = encoder.transformation();

  /* we are going to pass tr along the path renders so that the stroking
   * is effecitevly given in pixel coordinates
   */
  encoder.transformation(astral::Transformation());

  ASTRALassert(m_mode != preview_mode);
  const astral::Path *selected_contour(nullptr);

  m_tmp_pts.clear();
  for (unsigned int i = 0; i < 3; ++i)
    {
      m_tmp_paths[i].clear();
      m_tmp_translates[i].clear();
      m_tmp_matrices[i].clear();
    }

  for (int i = 0, endi = m_document->number_contours(path); i < endi; ++i)
    {
      const astral::Path *as_path;
      astral::vec2 pt;
      int idx;
      enum contour_classification_t C;

      as_path = &m_document->contour_as_path(path, i);
      idx = m_document->query_pairing(path, i, &pt);
      switch (idx)
        {
        default:
          C = paired_contour;
          break;

        case -1:
          C = unparied_contour;
          break;

        case -2:
          C = collapsed_contour;
          break;
        }

      m_tmp_paths[C].push_back(as_path);
      m_tmp_translates[C].push_back(tr.m_translate);
      m_tmp_matrices[C].push_back(tr.m_matrix);
      if (C == collapsed_contour)
        {
          m_tmp_pts.push_back(pt);
        }

      if (i == m_selected_contour[path])
        {
          selected_contour = as_path;
        }
    }

  if (!m_tmp_paths[paired_contour].empty())
    {
      astral::CombinedPath drawn_path(astral::make_c_array(m_tmp_paths[paired_contour]),
                                      astral::make_c_array(m_tmp_translates[paired_contour]),
                                      astral::make_c_array(m_tmp_matrices[paired_contour]));

      encoder.stroke_paths(drawn_path, stroke_params,
                           encoder.create_value(astral::Brush().base_color(astral::vec4(0.0f, 1.0f, 0.0f, 0.5f))),
                           astral::blend_porter_duff_src_over,
                           stroke_mask_usage_params,
                           stroke_mask_params);
    }

  if (!m_tmp_paths[unparied_contour].empty())
    {
      astral::CombinedPath drawn_path(astral::make_c_array(m_tmp_paths[unparied_contour]),
                                      astral::make_c_array(m_tmp_translates[unparied_contour]),
                                      astral::make_c_array(m_tmp_matrices[unparied_contour]));

      encoder.stroke_paths(drawn_path, stroke_params,
                           encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 0.0f, 0.0f, 0.5f))),
                           astral::blend_porter_duff_src_over,
                           stroke_mask_usage_params,
                           stroke_mask_params);
    }

  if (!m_tmp_paths[collapsed_contour].empty())
    {
      astral::RenderValue<astral::Brush> brush;
      astral::CombinedPath drawn_path(astral::make_c_array(m_tmp_paths[collapsed_contour]),
                                      astral::make_c_array(m_tmp_translates[collapsed_contour]),
                                      astral::make_c_array(m_tmp_matrices[collapsed_contour]));

      brush = encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 0.0f, 1.0f, 1.0f)));
      encoder.stroke_paths(drawn_path, stroke_params, brush,
                           astral::blend_porter_duff_src_over,
                           stroke_mask_usage_params,
                           stroke_mask_params);
      for (const astral::vec2 &pt : m_tmp_pts)
        {
          render_anchor_point(encoder, pane, tr.apply_to_point(pt), brush);
        }
    }

  if (selected_contour)
    {
      astral::c_array<const AnimatedPathDocument::ContourPoint> pts;
      astral::CombinedPath drawn_path(*selected_contour, tr.m_translate, tr.m_matrix);

      /* stroke the selected contour in some different color */
      encoder.stroke_paths(drawn_path, stroke_params,
                           encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 1.0f, 0.5f))),
                           astral::blend_porter_duff_src_over,
                           stroke_mask_usage_params,
                           stroke_mask_params);

      /* draw the points of the selected contour */
      pts = m_document->sorted_points(path, m_selected_contour[path]);
      if (m_document->contour(path, m_selected_contour[path]).closed())
        {
          /* when a contour is closed, the returned point array makes the
           * first and last points match.
           */
          pts.pop_back();
        }

      /* draw a big yellow bottom rect on the selected point */
      if (m_current_pane == path && m_selected_point.valid())
        {
          astral::RenderValue<astral::Brush> yellow;
          const AnimatedPathDocument::ContourPoint &P(m_document->point_information(path, m_selected_contour[path], m_selected_point));
          astral::vec2 pt, R(SELECTED_BOX_RADIUS);

          yellow = encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 0.0f, 0.8f)));
          pt = tr.apply_to_point(P.m_position);
          render_clipped_rect(encoder, pane, astral::Rect().min_point(pt - R).max_point(pt + R), yellow);
        }

      render_points(encoder, pane, tr, pts);
    }

  render_anchor_point(encoder, pane, tr.apply_to_point(m_document->anchor_point(path)),
                      encoder.create_value(astral::Brush().base_color(astral::vec4(0.0f, 0.0f, 1.0f, 1.0f))));
}

void
CreateAnimtedPathDemo::
render_hud(astral::RenderEncoderBase encoder,
           const astral::BoundingBox<float> pane)
{
  astral::Font font(m_text_items.front()->font());
  float line_height, max_descender;
  astral::RenderValue<astral::Brush> white;
  int num_lines;
  astral::RenderEncoderBase::AutoRestore auto_restore(encoder);

  line_height = font.base_metrics().m_height;
  if (font.typeface().is_scalable())
    {
      max_descender = font.scaling_factor() * font.typeface().scalable_metrics().m_descender;
    }
  else
    {
      max_descender = 0.0f;
    }
  num_lines = static_cast<int>((pane.as_rect().height() + max_descender) / line_height);

  m_visible_text_items.clear();
  m_visible_text_items.push_back(left_mouse_command);
  if (m_current_pane != -1)
    {
      std::ostringstream right_click_str;

      m_visible_text_items.push_back(right_clip_command);

      right_click_str << "[Right Click ]";
      if (m_selected_contour[m_current_pane] != -1)
        {
          right_click_str << "Contour = #" << m_selected_contour << " selected, ";
          if (m_selected_point.valid())
            {
              right_click_str << "editing point #" << m_selected_point.m_value;
              m_visible_text_items.push_back(d_delete_point_command);
            }
          else
            {
              right_click_str << "[hold middle mouse to edit point]";
            }

          m_visible_text_items.push_back(p_pair_contour_command);
          m_visible_text_items.push_back(u_unpair_contour_command);
          m_visible_text_items.push_back(ctrl_u_collapse_contour_command);
          m_visible_text_items.push_back(q_select_pair_command);
          m_visible_text_items.push_back(r_reverse_contour_command);
          m_visible_text_items.push_back(a_add_point_command);
          m_visible_text_items.push_back(shift_d_delete_all_points_command);
        }
      else
        {
          right_click_str << "No selected Contour";
        }

      m_text_items[right_clip_command]->clear();
      add_text(0.0f, right_click_str.str(), *m_text_items[right_clip_command]);

      m_visible_text_items.push_back(y_set_anchor_point_command);
      m_visible_text_items.push_back(shift_u_clear_all_contour_pairings_command);
    }
  m_visible_text_items.push_back(s_save_command);
  m_visible_text_items.push_back(m_toggle_view_command);

  white = encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 1.0f, 1.0f)));
  encoder.transformation(astral::Transformation());
  encoder.translate(0.0f, pane.min_point().y());

  int current_line = 0;
  float x = 0.0f, widest = 0.0f, spacing = line_height;

  for (const enum text_item_t idx : m_visible_text_items)
    {
      encoder.translate(0.0f, line_height);
      encoder.draw_text(*m_text_items[idx], white);

      ++current_line;
      widest = astral::t_max(widest, m_text_items[idx]->bounding_box().as_rect().width());
      if (current_line >= num_lines)
        {
          x += widest;
          encoder.transformation(astral::Transformation());
          encoder.translate(x + spacing, pane.min_point().y());

          current_line = 0;
          widest = 0.0f;
        }
    }
}

float
CreateAnimtedPathDemo::
update_preview_continuous_parameters(void)
{
  const Uint8 *keyboard_state = SDL_GetKeyboardState(nullptr);
  const float stroke_rate(0.1f / m_preview_zoom.transformation().m_scale);
  const float scale_rate(0.0001f);
  float return_value, delta;
  const float miter_rate(0.02f);

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

  if (keyboard_state[SDL_SCANCODE_B])
    {
      m_stroke_params.m_miter_limit
        = astral::t_max(0.0f, m_stroke_params.m_miter_limit - delta * miter_rate);
      std::cout << "Miter limit set to: " << m_stroke_params.m_miter_limit << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_N])
    {
      m_stroke_params.m_miter_limit += delta * miter_rate;
      std::cout << "Miter limit set to: " << m_stroke_params.m_miter_limit << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_RIGHTBRACKET])
    {
      m_stroke_params.m_width += stroke_rate * delta;
      std::cout << "Stroke width set to: "
                << m_stroke_params.m_width
                << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_LEFTBRACKET] && m_stroke_params.m_width > 0.0f)
    {
      m_stroke_params.m_width -= stroke_rate * delta;
      m_stroke_params.m_width = astral::t_max(m_stroke_params.m_width, 0.0f);
      std::cout << "Stroke width set to: "
                << m_stroke_params.m_width
                << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_UP] && m_mask_stroke_params.m_render_scale_factor.m_scale_factor < 1.0f)
    {
      m_mask_stroke_params.m_render_scale_factor.m_scale_factor += scale_rate * delta;
      m_mask_stroke_params.m_render_scale_factor.m_scale_factor = astral::t_min(1.0f, m_mask_stroke_params.m_render_scale_factor.m_scale_factor);
      std::cout << "Stroke render-scale factor set to " << m_mask_stroke_params.m_render_scale_factor << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_DOWN] && m_mask_stroke_params.m_render_scale_factor.m_scale_factor > 0.0f)
    {
      m_mask_stroke_params.m_render_scale_factor.m_scale_factor -= scale_rate * delta;
      m_mask_stroke_params.m_render_scale_factor.m_scale_factor = astral::t_max(1e-5f, m_mask_stroke_params.m_render_scale_factor.m_scale_factor);
      std::cout << "Stroke render-scale factor set to " << m_mask_stroke_params.m_render_scale_factor << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_RIGHT] && m_mask_fill_params.m_render_scale_factor.m_scale_factor < 1.0f)
    {
      m_mask_fill_params.m_render_scale_factor.m_scale_factor += scale_rate * delta;
      m_mask_fill_params.m_render_scale_factor.m_scale_factor = astral::t_min(1.0f, m_mask_fill_params.m_render_scale_factor.m_scale_factor);
      std::cout << "Fill render-scale factor set to " << m_mask_fill_params.m_render_scale_factor << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_LEFT] && m_mask_fill_params.m_render_scale_factor.m_scale_factor > 0.0f)
    {
      m_mask_fill_params.m_render_scale_factor.m_scale_factor -= scale_rate * delta;
      m_mask_fill_params.m_render_scale_factor.m_scale_factor = astral::t_max(1e-5f, m_mask_fill_params.m_render_scale_factor.m_scale_factor);
      std::cout << "Fill render-scale factor set to " << m_mask_fill_params.m_render_scale_factor << "\n";
    }

  return return_value;
}

void
CreateAnimtedPathDemo::
draw_preview_hud(astral::RenderEncoderBase encoder)
{
  std::ostringstream str;

  str << "[h]: toggle showing commands\n"
      << "[p]: pause animation\n"
      << "[ [ and ] ]: change stroking width (" << m_stroke_params.m_width << ")\n"
      << "[s]: changing stroking (" << label(m_stroke_mode) << ")\n"
      << "[r]: change fill rule (";

  if (m_fill_params.m_fill_rule != astral::number_fill_rule)
    {
      str << astral::label(m_fill_params.m_fill_rule);
    }
  else
    {
      str << "no fill";
    }
  str << ")\n";

  str << "[Up/Down]: change stroking render scale factor (" << m_mask_stroke_params.m_render_scale_factor << ")\n"
      << "[Left/Right]: change filling render scale factor (" << m_mask_fill_params.m_render_scale_factor << ")\n"
      << "[b/n]: change miter limit (" << m_stroke_params.m_miter_limit << ")\n"
      << "[c]: change cap style (" << astral::label(m_stroke_params.m_cap) << ")\n"
      << "[j]: change join style (" << astral::label(m_stroke_params.m_join) << ")\n"
      << "[shift-j]: toggle miter-style (";
  if (m_stroke_params.m_miter_clip)
    {
      str << "miter-clip";
    }
  else
    {
      str << "miter-cull";
    }
  str << ")\n";

  encoder.transformation(astral::Transformation());
  set_and_draw_hud(encoder, *m_preview_text, str.str());
}

void
CreateAnimtedPathDemo::
draw_frame(void)
{
  astral::ivec2 dims(dimensions());
  astral::RenderEncoderSurface encoder;

  encoder = renderer().begin(render_target());

  if (m_mode == edit_mode)
    {
      Panes panes(dims);

      render_path(encoder, panes.m_path_panes[AnimatedPathDocument::start_path], AnimatedPathDocument::start_path);
      render_path(encoder, panes.m_path_panes[AnimatedPathDocument::end_path], AnimatedPathDocument::end_path);

      render_hud(encoder, panes.m_bottom_pane);
      panes.render_boundaries(encoder, m_current_pane);
    }
  else
    {
      float t;

      update_preview_continuous_parameters();
      t = compute_animation_interpolate();

      /* make the center point of the screen be (0, 0) in logical */
      encoder.translate(astral::vec2(dims) * 0.5f);

      /* apply the correct pan-zoom */
      encoder.concat(m_preview_zoom.transformation().astral_transformation());

      /* the path animation makes the anchor point as (0.0, 0.0), and we have
       * that when m_preview_zoom is the identity, then it maps to the center
       */

      if (m_fill_params.m_fill_rule != astral::number_fill_rule)
        {
          astral::CombinedPath path(t, m_document->animated_path());

          encoder.fill_paths(path, m_fill_params,
                             encoder.create_value(astral::Brush().base_color(astral::vec4(0.0f, 1.0f, 1.0f, 1.0f))),
                             astral::blend_porter_duff_src_over,
                             astral::MaskUsage(astral::mask_type_distance_field),
                             m_mask_fill_params);
        }

      if (m_stroke_mode != stroke_none)
        {
          astral::CombinedPath path;
          float width_on_screen;

          if (m_stroke_mode == stroke_pixel_coordinates)
            {
              astral::Transformation tr(encoder.transformation());

              width_on_screen = m_stroke_params.m_width;
              path = astral::CombinedPath(t, m_document->animated_path(), tr.m_translate, tr.m_matrix);
              encoder.transformation(astral::Transformation());
            }
          else
            {
              path = astral::CombinedPath(t, m_document->animated_path());
              width_on_screen = m_stroke_params.m_width * m_preview_zoom.transformation().m_scale;
            }

          astral::StrokeParameters stroke_params(m_stroke_params);
          astral::StrokeMaskProperties stroke_mask_params(m_mask_stroke_params);
          astral::MaskUsage stroke_mask_usage_params(astral::mask_type_distance_field);

          if (width_on_screen <= 2.0f)
            {
              /* override the render scale factor to 1.0 and use coverage if the
               * effective stroking width is less than 4 pixels
               */
              stroke_mask_params
                .render_scale_factor(1.0f);

              stroke_mask_usage_params
                .mask_type(astral::mask_type_coverage);
            }
          else if (0.5f * stroke_mask_params.m_render_scale_factor.m_scale_factor * width_on_screen < 1.0f)
            {
              stroke_mask_params.m_render_scale_factor.m_scale_factor = astral::t_min(1.0f, 2.0f / width_on_screen);
            }

          encoder.stroke_paths(path, stroke_params,
                               encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 1.0f, 1.0f))),
                               astral::blend_porter_duff_src_over,
                               stroke_mask_usage_params,
                               stroke_mask_params);
        }

      if (m_show_preview_commands)
        {
          draw_preview_hud(encoder);
        }
    }

  renderer().end();
}

astral::c_string
CreateAnimtedPathDemo::
label(enum mode_t mode)
{
  static astral::c_string labels[number_modes] =
    {
      [preview_mode] = "preview_mode",
      [edit_mode] = "edit_mode",
    };

  ASTRALassert(mode < number_modes);
  return labels[mode];
}

void
CreateAnimtedPathDemo::
handle_event_preview_mode(const SDL_Event &ev)
{
  astral::vec2 fdims(dimensions());

  /* recall that on preview, the center of the window is mapped to (0, 0) */
  m_preview_zoom.m_translate_event = -0.5f * fdims;
  m_preview_zoom.handle_event(ev);

  if (ev.type == SDL_KEYDOWN)
    {
      switch (ev.key.keysym.sym)
        {
        case SDLK_p:
          m_path_time.pause(!m_path_time.paused());
          break;

        case SDLK_h:
          m_show_preview_commands = !m_show_preview_commands;
          break;

        case SDLK_r:
          {
            cycle_value(m_fill_params.m_fill_rule,
                        ev.key.keysym.mod & (KMOD_CTRL | KMOD_ALT | KMOD_SHIFT),
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
            if (ev.key.keysym.mod & KMOD_CTRL)
              {
                if (m_stroke_params.m_join == astral::join_miter)
                  {
                    astral::c_string label;

                    m_stroke_params.m_miter_clip = !m_stroke_params.m_miter_clip;
                    label = (m_stroke_params.m_miter_clip) ?
                      "miter-clip":
                      "miter-cull";
                    std::cout << "MiterJoin style set to " << label << "\n";
                  }
              }
            else
              {
                cycle_value(m_stroke_params.m_join,
                            ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                            astral::number_join_t + 1);
                std::cout << "Join style set to "
                          << astral::label(m_stroke_params.m_join) << "\n";
              }
          }
          break;

        case SDLK_c:
          {
            cycle_value(m_stroke_params.m_cap,
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        astral::number_cap_t);
            std::cout << "Cap style set to "
                      << astral::label(m_stroke_params.m_cap) << "\n";
          }
          break;

        case SDLK_s:
          {
            cycle_value(m_stroke_mode,
                        ev.key.keysym.mod & (KMOD_CTRL | KMOD_ALT | KMOD_SHIFT),
                        number_stroke_modes);
            std::cout << "Stroke mode set to " << label(m_stroke_mode) << "\n";
          }
          break;
        }
    }
}

void
CreateAnimtedPathDemo::
handle_event_edit_mode(const SDL_Event &ev)
{
  Panes panes(dimensions());
  int mx, my;
  Uint32 mouse_state;

  mouse_state = SDL_GetMouseState(&mx, &my);
  if (ev.type == SDL_MOUSEBUTTONDOWN || (ev.type == SDL_KEYDOWN && mouse_state == 0u))
    {
      /* Mouse button down events or key down events with no mouse buttons
       * held down change the current pane to where the mouse cursor is located.
       */
      m_current_pane = panes.which_pane(astral::ivec2(mx, my));
      m_selected_point = AnimatedPathDocument::PointIndex();
    }

  if (ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_MIDDLE)
    {
      m_selected_point = AnimatedPathDocument::PointIndex();
    }

  if (m_current_pane != -1)
    {
      m_zooms[m_current_pane].m_translate_event = -panes.m_path_panes[m_current_pane].as_rect().center_point();
      m_zooms[m_current_pane].handle_event(ev);
    }

  if (m_current_pane != -1)
    {
      astral::vec2 p;
      float tol;
      int other_pane;
      enum AnimatedPathDocument::path_t path;

      path = static_cast<enum AnimatedPathDocument::path_t>(m_current_pane);
      other_pane = 1 - path;

      p = astral::vec2(mx, my) - panes.m_path_panes[path].as_rect().center_point();
      p = m_zooms[path].transformation().inverse().apply_to_point(p);
      tol = 0.5f / m_zooms[path].transformation().m_scale;

      /* right mouse button selects contour if in edit_mode */
      if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_RIGHT)
        {
          m_selected_contour[path] = m_document->nearest_contour(tol, path, p);

          std::cout << "Path " << label(path) << " selected contour #"
                    << m_selected_contour[path] << " from query point "
                    << p << "\n";
        }

      if (m_selected_contour[path] != -1 && ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_MIDDLE)
        {
          m_selected_point = m_document->nearest_point(path, m_selected_contour[path], p);
          if (m_selected_point.valid())
            {
              std::cout << "Selected point #" << m_selected_point.m_value << "\n";
            }
        }

      /* middle-mouse drag: move nearest point */
      if (m_selected_contour[path] != -1
          && m_selected_point.valid()
          && (mouse_state & SDL_BUTTON_MMASK) != 0
          && ev.type == SDL_MOUSEMOTION)
        {
          /* get the nearest made point */
          unsigned int contour(m_selected_contour[path]);
          astral::Contour::PointQueryResult Q;

          /* now figure out where the point moves to */
          Q = m_document->query_contour(tol, path, contour, p);
          if (Q.m_closest_curve >= 0)
            {
              AnimatedPathDocument::ContourPoint cp;

              cp = m_document->point_information(path, contour, m_selected_point);
              std::cout << "Path " << label(path) << ", Contour #" << contour << ", Point #"
                        << m_selected_point.m_value << " moved from "
                        << cp.m_position << " to " << std::flush << "\n";
              m_document->modify_point(path, contour, m_selected_point, Q.m_closest_curve, Q.m_closest_point_t);
              cp = m_document->point_information(path, contour, m_selected_point);
              std::cout << cp.m_position << "\n";
            }
        }

      /*
       *  'a' --> add point to selected contour.
       *  'd' --> delete closest point
       *  'p' --> pair selected contours of the two panes
       *  'q' --> select contour in other pane to which m_selected_contour[path] is paired (if any)
       *
       *  Special: cannot delete a point if there is only one point left or if the
       *           contour is closed and there are two points left.
       */
      if (ev.type == SDL_KEYDOWN)
        {
          switch (ev.key.keysym.sym)
            {
            case SDLK_y:
              if (m_current_pane != -1)
                {
                  m_document->anchor_point(path, p);
                  std::cout << "Anchor point set to " << p << "\n";
                }
              break;

            case SDLK_p:
              if (m_selected_contour[0] != -1 && m_selected_contour[1] != -1)
                {
                  m_document->pair_contours(m_selected_contour[0], m_selected_contour[1]);
                  std::cout << "Paired Contour #" << m_selected_contour[0] << " to contour #"
                            << m_selected_contour[1] << "\n";
                }
              break;

            case SDLK_u:
              {
                if (ev.key.keysym.mod & (KMOD_SHIFT | KMOD_ALT))
                  {
                    m_document->clear_pairing();
                    std::cout << "Remove all pairings\n";
                  }
                else if (m_current_pane != -1 && m_selected_contour[path] != -1)
                  {
                    if (ev.key.keysym.mod & KMOD_CTRL)
                      {
                        m_document->collapse_to_a_point(path, m_selected_contour[path], p);
                        std::cout << "Contour #" << m_selected_contour[path] << " collapses to " << p << "\n";
                      }
                    else
                      {
                        m_document->remove_pairing(path, m_selected_contour[path]);
                        std::cout << "Contour #" << m_selected_contour[path] << " unpaired\n";
                      }
                  }
                }
              break;

            case SDLK_q:
              if (m_current_pane != -1 && m_selected_contour[path] != -1)
                {
                  m_selected_contour[other_pane] = m_document->query_pairing(path, m_selected_contour[path]);
                  std::cout << "Select matching paired contour\n";
                }
              break;

            case SDLK_r:
              if (m_current_pane != -1 && m_selected_contour[path] != -1)
                {
                  m_document->reverse_contour(path, m_selected_contour[path]);
                  std::cout << "Path " << label(path) << " Contour #"
                            << m_selected_contour[path] << " reversed\n";
                }
              break;

            case SDLK_d:
              if (m_current_pane != -1 && m_selected_contour[path] != -1)
                {
                  unsigned int contour(m_selected_contour[path]);

                  if (ev.key.keysym.mod & (KMOD_SHIFT | KMOD_ALT | KMOD_CTRL))
                    {
                      m_document->clear_points(path, contour);
                      m_selected_point = AnimatedPathDocument::PointIndex();

                      std::cout << "Cleared points from contour #" << contour
                                << " of path " << label(path) << "\n";
                    }
                  else if (m_selected_point.valid()
                           && m_document->delete_point(path, contour, m_selected_point) == astral::routine_success)
                    {
                      m_selected_point = AnimatedPathDocument::PointIndex();

                      std::cout << "Deleted Point #" << m_selected_point.m_value
                                << " from path " << label(path) << "\n";
                    }
                  break;
                }
              break;

            case SDLK_a:
              if (m_current_pane != -1 && m_selected_contour[path] != -1)
                {
                  astral::Contour::PointQueryResult Q;
                  unsigned int contour(m_selected_contour[path]);

                  Q = m_document->query_contour(tol, path, contour, p);
                  if (Q.m_closest_curve >= 0)
                    {
                      m_document->add_point(path, contour, Q.m_closest_curve, Q.m_closest_point_t);
                    }
                }
              break;
            }
        }
    }

  if (ev.type == SDL_KEYDOWN)
    {
      switch (ev.key.keysym.sym)
        {
        case SDLK_s:
          {
            std::ofstream out_file(m_save.value().c_str());
            m_document->save_to_file(out_file);

            std::cout << "Document saved to \"" << m_save.value()
                      << "\"\n";
          }
          break;
        }
    }
}

void
CreateAnimtedPathDemo::
handle_event(const SDL_Event &ev)
{
  if (m_mode == preview_mode)
    {
      handle_event_preview_mode(ev);
    }
  else
    {
      handle_event_edit_mode(ev);
    }

  if (ev.type == SDL_KEYDOWN)
    {
      switch (ev.key.keysym.sym)
        {
        case SDLK_m:
          {
            cycle_value(m_mode, ev.key.keysym.mod & (KMOD_SHIFT | KMOD_ALT | KMOD_CTRL), number_modes);
            if (m_path_time.paused())
              {
                m_path_time.resume();
              }
            m_path_time.restart();
            m_draw_timer.restart_us();
            std::cout << "Mode set to " << label(m_mode) << "\n";
          }
          break;
        };
    }

  render_engine_gl3_demo::handle_event(ev);
}

int
main(int argc, char **argv)
{
  CreateAnimtedPathDemo M;
  return M.main(argc, argv);
}

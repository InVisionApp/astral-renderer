/*!
 * \file load_svg.cpp
 * \brief file load_svg.cpp
 *
 * Copyright 2019 by InvisionApp.
 *
 * Contact: kevinrogovin@invisionapp.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 */

#include <iostream>
#include "load_svg.hpp"
#include "nanosvg.h"

namespace
{
  bool
  collinear(astral::vec2 p,
            astral::vec2 ct,
            astral::vec2 q)
  {
    astral::vec2 v, w;
    float f;

    v = p - ct;
    w = q - ct;

    f = v.x() * w.y() - v.y() * w.x();
    const float rel_tol = 1e-4;

    return astral::t_abs(f) <= rel_tol * astral::t_max(v.L1norm(), w.L1norm());
  }

  void
  simplify_line_to(astral::vec2 start_pt,
                   astral::vec2 end_pt,
                   astral::Path *out_path)
  {
    if (start_pt != end_pt)
      {
        out_path->line_to(end_pt);
      }
  }

  void
  simplify_quadratic_to(astral::vec2 start_pt,
                        astral::vec2 ct,
                        astral::vec2 end_pt,
                        astral::Path *out_path)
  {
    if (collinear(start_pt, ct, end_pt))
      {
        simplify_line_to(start_pt, end_pt, out_path);
      }
    else
      {
        out_path->quadratic_to(ct, end_pt);
      }
  }

  void
  simplify_cubic_to(astral::vec2 start_pt,
                    astral::vec2 c1,
                    astral::vec2 c2,
                    astral::vec2 end_pt,
                    astral::Path *out_path)
  {
    if (collinear(start_pt, c1, c2) && collinear(c1, c2, end_pt))
      {
        simplify_line_to(start_pt, end_pt, out_path);
        return;
      }

    /* A quadratic [p, c, q] gets promoted to a cubic [p, c1, c2, q]
     * as
     *  c1 = p + 2/3 * (c - p)
     *  c2 = q + 2/3 * (c - q)
     */
    astral::vec2 cA, cB, diff;

    cA = start_pt + 1.5f * (c1 - start_pt);
    cB = end_pt + 1.5f * (c2 - end_pt);

    diff = cA - cB;
    const float rel_tol = 1e-4;
    if (diff.L1norm() < rel_tol * astral::t_max(cA.L1norm(), cB.L1norm()))
      {
        simplify_quadratic_to(start_pt, cA, end_pt, out_path);
      }
    else
      {
        out_path->cubic_to(c1, c2, end_pt);
      }
  }

  void
  append_contour(const NSVGpath *in_path, astral::Path *out_path, bool contours_closed)
  {
    astral::vec2 first_pt, start_pt, end_pt;

    start_pt.x() = in_path->pts[0];
    start_pt.y() = in_path->pts[1];
    first_pt = start_pt;
    out_path->move(start_pt);

    for (int i = 0; i < in_path->npts - 1; i += 3)
      {
        const float *pts;
        astral::vec2 ct1, ct2;

        pts = &in_path->pts[2 * i];

        ct1.x() = pts[2];
        ct1.y() = pts[3];
        ct2.x() = pts[4];
        ct2.y() = pts[5];
        end_pt.x() = pts[6];
        end_pt.y() = pts[7];

        simplify_cubic_to(start_pt, ct1, ct2, end_pt, out_path);
        start_pt = end_pt;
      }

    if (in_path->closed || contours_closed)
      {
        out_path->close();
      }
  }

  astral::FixedPointColor_sRGB
  convert_color(unsigned int color)
  {
    astral::FixedPointColor_sRGB return_value;

    return_value.red() = color & 0xFF;
    return_value.green() = (color >> 8) & 0xFF;
    return_value.blue() = (color >> 16) & 0xFF;
    return_value.alpha() = (color >> 24) & 0xFF;

    return return_value;
  }

  astral::reference_counted_ptr<const astral::ColorStopSequence>
  load_colorstops(astral::RenderEngine &engine, const NSVGgradient *in_gradient)
  {
    std::vector<astral::ColorStop<astral::FixedPointColor_sRGB>> colorstops;
    for (int i = 0; i < in_gradient->nstops; ++i)
      {
        astral::ColorStop<astral::FixedPointColor_sRGB> v;

        v.m_t = in_gradient->stops[i].offset;
        v.m_color = convert_color(in_gradient->stops[i].color);
        colorstops.push_back(v);
      }

    return engine.colorstop_sequence_atlas().create(astral::make_c_array(colorstops));
  }

  enum astral::tile_mode_t
  convert_spread_mode(unsigned int v)
  {
    switch (v)
      {
      case NSVG_SPREAD_PAD:
        return astral::tile_mode_clamp;

      case NSVG_SPREAD_REFLECT:
        return astral::tile_mode_mirror_repeat;

      case NSVG_SPREAD_REPEAT:
        return astral::tile_mode_repeat;
      }

    return astral::tile_mode_clamp;
  }

  astral::Gradient
  load_linear_gradient(astral::RenderEngine &engine, const NSVGgradient *in_gradient)
  {
    /* for linear gradient, NSVGpaint::xform has the following:
     *    NSVGgradient::xform[0] = y2 - y1
     *    NSVGgradient::xform[1] = x1 - x2
     *    NSVGgradient::xform[2] = x2 - x1
     *    NSVGgradient::xform[3] = y2 - y1
     *    NSVGgradient::xform[4] = x1
     *    NSVGgradient::xfrom[5] = y1
     */
    astral::vec2 p1, p2;

    p1.x() = in_gradient->xform[4];
    p1.y() = in_gradient->xform[5];

    p2.x() = p1.x() + in_gradient->xform[2];
    p2.y() = p1.y() + in_gradient->xform[3];

    return astral::Gradient(load_colorstops(engine, in_gradient),
                            p1, p2,
                            convert_spread_mode(in_gradient->spread));
  }

  astral::Gradient
  load_radial_gradient(astral::RenderEngine &engine, const NSVGgradient *in_gradient)
  {
    /* for linear gradient, NSVGpaint::xform has the following:
     *    NSVGgradient::xform[0] = r
     *    NSVGgradient::xform[1] = 0
     *    NSVGgradient::xform[2] = 0
     *    NSVGgradient::xform[3] = r
     *    NSVGgradient::xform[4] = cx
     *    NSVGgradient::xfrom[5] = cy
     *    NSVGgradient::fx = fx / r
     *    NSVGgradient::fy = fy / r
     *
     * where
     *   (cx, cy) = center of end-circle
     *   (fx, fy) = center of start circle
     *          r = radius of end circle
     *
     * NanoSVG does not seem to load/care about fr which is the start radius;
     * we jus say the start radius is 0.
     */
    astral::vec2 p1, p2;
    float r1, r2;

    r1 = 0.0f;
    r2 = in_gradient->xform[0];

    p2.x() = in_gradient->xform[4];
    p2.y() = in_gradient->xform[5];

    p1.x() = r2 * in_gradient->fx;
    p1.y() = r2 * in_gradient->fy;

    return astral::Gradient(load_colorstops(engine, in_gradient),
                            p1, r1, p2, r2,
                            convert_spread_mode(in_gradient->spread));
  }

  void
  convert_paint(astral::RenderEngine &engine, const NSVGpaint *in_paint, SVGBrush *out_paint)
  {
    out_paint->m_active = (in_paint->type != NSVG_PAINT_NONE);

    switch (in_paint->type)
      {
      case NSVG_PAINT_NONE:
        return;

      case NSVG_PAINT_COLOR:
        out_paint->m_color = convert_color(in_paint->color);
        return;

      case NSVG_PAINT_LINEAR_GRADIENT:
        out_paint->m_gradient = load_linear_gradient(engine, in_paint->gradient);
        break;

      case NSVG_PAINT_RADIAL_GRADIENT:
        out_paint->m_gradient = load_radial_gradient(engine, in_paint->gradient);
        break;
      }
  }

  void
  convert_stroke_params(const NSVGshape *in_shape, astral::StrokeParameters *out_params)
  {
    out_params->m_draw_edges = true;
    out_params->m_width = in_shape->strokeWidth;
    out_params->m_miter_limit = in_shape->miterLimit;

    switch (in_shape->strokeLineJoin)
      {
      case NSVG_JOIN_MITER:
        out_params->m_join = astral::join_miter;
        break;

      case NSVG_JOIN_ROUND:
        out_params->m_join = astral::join_rounded;
        break;

      case NSVG_JOIN_BEVEL:
        out_params->m_join = astral::join_bevel;
        break;

      default:
        out_params->m_join = astral::join_none;
      }

    switch (in_shape->strokeLineCap)
      {
      case NSVG_CAP_BUTT:
        out_params->m_cap = astral::cap_flat;
        break;

      case NSVG_CAP_ROUND:
        out_params->m_cap = astral::cap_rounded;
        break;

      case NSVG_CAP_SQUARE:
        out_params->m_cap = astral::cap_square;
        break;

      default:
        out_params->m_cap = astral::cap_flat;
        break;
      }
  }

  void
  convert_dash_pattern(const NSVGshape *in_shape,
                       astral::StrokeShader::DashPattern *out_pattern)
  {
    for (char i = 0; i + 1 < in_shape->strokeDashCount; i += 2)
      {
        astral::StrokeShader::DashPatternElement D;
        int idx(i);

        D.m_draw_length = in_shape->strokeDashArray[idx];
        D.m_skip_length = in_shape->strokeDashArray[idx + 1];
        out_pattern->add(D);
      }
    out_pattern->dash_start_offset(in_shape->strokeDashOffset);
  }
}

void
SVG::
load(astral::RenderEngine &engine, const char *filename, const char *units, float dpi)
{
  NSVGimage *image;

  image = nsvgParseFromFile(filename, units, dpi);

  if (!image)
    {
      std::cout << "Failed to load SVG from \"" << filename << "\"\n";
      return;
    }

  unsigned int shape_count = 1;
  for (NSVGshape *shape = image->shapes; shape != nullptr; shape = shape->next, ++shape_count)
    {
      unsigned int contour_count = 1;

      m_elements.push_back(SVGElement());
      convert_paint(engine, &shape->fill, &m_elements.back().m_fill_brush);
      convert_paint(engine, &shape->stroke, &m_elements.back().m_stroke_brush);
      for (NSVGpath *path = shape->paths; path != nullptr; path = path->next, ++contour_count)
        {
          append_contour(path, &m_elements.back().m_path, m_elements.back().m_fill_brush.m_active);
        }

      m_bbox.union_box(m_elements.back().m_path.bounding_box());
      m_elements.back().m_opacity = shape->opacity;

      if (m_elements.back().m_fill_brush.m_active)
        {
          m_elements.back().m_fill_rule = (shape->fillRule == NSVG_FILLRULE_NONZERO) ?
            astral::nonzero_fill_rule :
            astral::odd_even_fill_rule;
        }
      else
        {
          m_elements.back().m_fill_rule = astral::number_fill_rule;
        }

      if (m_elements.back().m_stroke_brush.m_active)
        {
          convert_stroke_params(shape, &m_elements.back().m_stroke_params);
          convert_dash_pattern(shape, &m_elements.back().m_dash_pattern);
        }
    }

  nsvgDelete(image);
}

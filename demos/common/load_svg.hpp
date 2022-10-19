/*!
 * \file load_svg.hpp
 * \brief file load_svg.hpp
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

#ifndef ASTRAL_LOAD_SVG_HPP
#define ASTRAL_LOAD_SVG_HPP

#include <vector>
#include <sstream>
#include <astral/renderer/renderer.hpp>

class SVGBrush
{
public:
  SVGBrush(void):
    m_gradient(astral::Gradient::invalid_gradient)
  {}

  /* m_gradient.m_colorstops being nullptr indicates to
   * only use m_color
   */
  astral::Gradient m_gradient;
  astral::FixedPointColor_sRGB m_color;

  /* if false means that don't paint */
  bool m_active;
};

class SVGElement
{
public:
  /* a value of astral::number_fill_rule means to not fill */
  enum astral::fill_rule_t m_fill_rule;
  SVGBrush m_fill_brush;

  astral::StrokeParameters m_stroke_params;
  astral::StrokeShader::DashPattern m_dash_pattern;
  SVGBrush m_stroke_brush;

  float m_opacity;

  /* if false, not visible */
  bool m_visible;

  /* path */
  astral::Path m_path;
};

class SVG
{
public:
  /*!
   * Load from the named file
   */
  SVG(astral::RenderEngine &engine, const char *filename, const char *units, float dpi)
  {
    load(engine, filename, units, dpi);
  }

  SVG(void)
  {}

  void
  load(astral::RenderEngine &engine, const char *filename, const char *units, float dpi);

  astral::c_array<const SVGElement>
  elements(void) const
  {
    return astral::make_c_array(m_elements);
  }

  void
  clear(void)
  {
    m_elements.clear();
  }

  const astral::BoundingBox<float>&
  bbox(void) const
  {
    return m_bbox;
  }

private:
  std::vector<SVGElement> m_elements;
  astral::BoundingBox<float> m_bbox;
};

#endif

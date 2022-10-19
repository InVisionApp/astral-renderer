/*!
 * \file animated_path_reflect.hpp
 * \brief animated_path_reflect.hpp
 *
 * Copyright 2020 by InvisionApp.
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

#ifndef ASTRAL_DEMO_ANIMATED_PATH_REFLECT_HPP
#define ASTRAL_DEMO_ANIMATED_PATH_REFLECT_HPP

#include <astral/animated_path.hpp>


/* Represets a line in the plane given by
 * { p + tv | t real }
 */
class Line
{
public:
  /* Direction of the line, vector must have norm 1 */
  astral::vec2 m_v;

  /* point of the axis */
  astral::vec2 m_p;

  /* gives the transformation that refelcts a pointer across the line */
  astral::Transformation
  reflect_transformation(void) const
  {
    /* R maps { m_p + t * m_v | t real } to the x-axis. */
    astral::Transformation R;

    R.m_matrix.row_col(0, 0) = m_v.x();
    R.m_matrix.row_col(0, 1) = m_v.y();
    R.m_matrix.row_col(1, 0) = -m_v.y();
    R.m_matrix.row_col(1, 1) = m_v.x();
    R = R * astral::Transformation(-m_p);

    /* reflect maps (x, y) to (x, -y) */
    astral::Transformation reflect;

    reflect.m_matrix.row_col(0, 0) = 1.0f;
    reflect.m_matrix.row_col(0, 1) = 0.0f;
    reflect.m_matrix.row_col(1, 0) = 0.0f;
    reflect.m_matrix.row_col(1, 1) = -1.0f;

    return R.inverse() * reflect * R;
  }
};

/* Give a contour C, construct an animated contour A so that
 *   - at time 0 A is C
 *   - at time 1 A is R(C) where R is reflection across the
 *     line H.
 */
astral::reference_counted_ptr<astral::AnimatedContour>
create_animated_reflection(const astral::Contour &C, const Line &H,
                           astral::ContourData *out_reflected = nullptr,
                           astral::ContourData *out_unreflected = nullptr);

void
create_animated_reflection(astral::AnimatedPath *dst,
                           const astral::Path &src,
                           Line &H,
                           astral::Path *out_reflected = nullptr,
                           astral::Path *out_unreflected = nullptr);

#endif

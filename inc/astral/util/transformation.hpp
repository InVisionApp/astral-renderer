/*!
 * \file transformation.hpp
 * \brief file transformation.hpp
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


#ifndef ASTRAL_TRANSFORMATION_HPP
#define ASTRAL_TRANSFORMATION_HPP

#include <astral/util/matrix.hpp>
#include <astral/util/math.hpp>
#include <astral/util/scale_translate.hpp>

namespace astral
{
/*!\addtogroup Utility
 * @{
 */

  /*!
   * \brief
   * An astral::Transformation represents the transformation
   * mapping a point \f$p\f$ to \f$ M * p + T\f$ where \f$M\f$
   * is the matrix \ref m_matrix and \f$T\f$ is the translation
   * \ref m_translate.
   */
  class Transformation
  {
  public:
    /*!
     * Ctor, intitialize as the identity.
     */
    Transformation(void):
      m_translate(0.0f, 0.0f)
    {}

    /*!
     * Ctor, initialize as a translation
     */
    explicit
    Transformation(vec2 p):
      m_translate(p)
    {}

    /*!
     * Ctor, initialize from a \ref ScaleTranslate
     */
    explicit
    Transformation(const ScaleTranslate &tr):
      m_translate(tr.m_translate)
    {
      m_matrix.row_col(0, 0) = tr.m_scale.x();
      m_matrix.row_col(1, 1) = tr.m_scale.y();
    }

    /*!
     * Apply the transformation to a point.
     */
    vec2
    apply_to_point(vec2 p) const
    {
      return m_matrix * p + m_translate;
    }

    /*!
     * Apply the transformation to a direction.
     */
    vec2
    apply_to_direction(vec2 p) const
    {
      return m_matrix * p;
    }

    /*!
     * Apply the transformation to a bounding box
     */
    BoundingBox<float>
    apply_to_bb(const BoundingBox<float> &bb) const
    {
      BoundingBox<float> return_value;
      if (!bb.empty())
        {
          return_value.union_point(apply_to_point(bb.as_rect().point(Rect::minx_miny_corner)));
          return_value.union_point(apply_to_point(bb.as_rect().point(Rect::minx_maxy_corner)));
          return_value.union_point(apply_to_point(bb.as_rect().point(Rect::maxx_miny_corner)));
          return_value.union_point(apply_to_point(bb.as_rect().point(Rect::maxx_maxy_corner)));
        }
      return return_value;
    }

    /*!
     * overload for operator* to compute the composition
     * of two \ref Transformation values
     */
    Transformation
    operator*(const Transformation &rhs) const
    {
      Transformation R;

      R.m_matrix = m_matrix * rhs.m_matrix;
      R.m_translate = m_translate + m_matrix * rhs.m_translate;

      return R;
    }

    /*!
     * Concat (on the right) with another transformation.
     */
    Transformation&
    concat(const Transformation &rhs)
    {
      m_translate += m_matrix * rhs.m_translate;
      m_matrix = m_matrix * rhs.m_matrix;
      return *this;
    }

    /*!
     * Concat (on the right) with a 2x2 matrix.
     */
    Transformation&
    concat(const float2x2 &rhs)
    {
      m_matrix = m_matrix * rhs;
      return *this;
    }

    /*!
     * Apply a translate.
     */
    Transformation&
    translate(float x, float y)
    {
      m_translate += m_matrix * vec2(x, y);
      return *this;
    }

    /*!
     * Apply a translate.
     */
    Transformation&
    translate(vec2 xy)
    {
      m_translate += m_matrix * xy;
      return *this;
    }

    /*!
     * Apply scaling in x-direction and y-direction seperately.
     */
    Transformation&
    scale(float sx, float sy)
    {
      m_matrix.row_col(0, 0) *= sx;
      m_matrix.row_col(1, 0) *= sx;
      m_matrix.row_col(0, 1) *= sy;
      m_matrix.row_col(1, 1) *= sy;
      return *this;
    }

    /*!
     * Apply scaling in x-direction and y-direction seperately.
     */
    Transformation&
    scale(vec2 xy)
    {
      return scale(xy.x(), xy.y());
    }

    /*!
     * Apply a scaling factor
     */
    Transformation&
    scale(float r)
    {
      return scale(r, r);
    }

    /*!
     * Rotate counter-clockwise (assuming y-coordinate is up)
     * by the named amount of -RADIANS-
     */
    Transformation&
    rotate(float radians)
    {
      float s, c;
      float2x2 tr;

      s = t_sin(radians);
      c = t_cos(radians);

      tr.row_col(0, 0) = c;
      tr.row_col(1, 0) = s;
      tr.row_col(0, 1) = -s;
      tr.row_col(1, 1) = c;

      m_matrix = m_matrix * tr;

      return *this;
    }

    /*!
     * Compute the inverse of this. Please cache this value
     * if you are going to use it more than once.
     */
    Transformation
    inverse(void) const
    {
      Transformation R;
      float det, inv_det;

      det = m_matrix.row_col(0, 0) * m_matrix.row_col(1, 1) - m_matrix.row_col(0, 1) * m_matrix.row_col(1, 0);
      inv_det = 1.0f / det;

      R.m_matrix.row_col(0, 0) = inv_det * m_matrix.row_col(1, 1);
      R.m_matrix.row_col(1, 1) = inv_det * m_matrix.row_col(0, 0);
      R.m_matrix.row_col(0, 1) = -inv_det * m_matrix.row_col(0, 1);
      R.m_matrix.row_col(1, 0) = -inv_det * m_matrix.row_col(1, 0);
      R.m_translate = -(R.m_matrix * m_translate);

      return R;
    }

    /*!
     * The 2x2 matrix of the transformation
     */
    float2x2 m_matrix;

    /*!
     * The translation of the transformation
     */
    vec2 m_translate;
  };

/*! @} */
}

#endif

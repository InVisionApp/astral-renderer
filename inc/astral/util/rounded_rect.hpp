/*!
 * \file rounded_rect.hpp
 * \brief file rounded_rect.hpp
 *
 * Copyright 2018 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#ifndef ASTRAL_ROUNDED_RECT_HPP
#define ASTRAL_ROUNDED_RECT_HPP

#include <astral/util/rect.hpp>

namespace astral
{
/*!\addtogroup Utility
 * @{
 */

  /*!
   * Class to specify geometry of a rounded rectangle
   * where the corner radii of all corners is the same;
   * the geometry of the base class Rect defines the
   * -bounding- rectangle of the \ref UniformRoundedRect
   */
  class UniformRoundedRect:public Rect
  {
  public:
    /*!
     * Class to specify a point of an astral::RoundedRect
     * or point of an astral::UniformRoundedRect. The points
     * that can specified correspond to the end points of
     * the arc corners.
     */
    class Point
    {
    public:
      /*!
       * Ctor
       * \param side value which which to initialize \ref m_side
       * \param max_point value which which to initialize \ref m_max_point
       */
      Point(enum side_t side = minx_side, bool max_point = true):
        m_side(side),
        m_max_point(max_point)
      {}

      /*!
       * Ctor that performs the inverse of \ref Point::point_index()
       * \param I point index with 0 <= I < 7
       */
      explicit
      Point(unsigned int I)
      {
        unsigned int m;

        m_side = side_from_point_index(I >> 1u);
        m = I & 1u;

        m_max_point = (m_side == miny_side || m_side == maxx_side) ? (m == 1u) : (m == 0u);
      }

      /*!
       * Comparison operator
       */
      bool
      operator==(Point rhs) const
      {
        return m_side == rhs.m_side
          && m_max_point == rhs.m_max_point;
      }

      /*!
       * The side on which the point resides.
       */
      enum side_t m_side;

      /*!
       * Each side has two points: one where the
       * non-fixed coordinate is smaller and one
       * where it is larger.
       */
      bool m_max_point;

      /*!
       * Return an integer value in {0, 1, 2, 3, 4, 5, 6, 7}
       * from this \ref Point value so that consecutive values
       * walk clockwise (assuming y-max is the bottom) around
       * the sides of a rounded rectangle
       */
      unsigned int
      point_index(void) const
      {
        unsigned int s, m;

        ASTRALassert(Rect::point_index(miny_side) == 0u);
        s = Rect::point_index(m_side);

        /* taking the max-point on the miny_side and maxx_side means
         * increment the point index where as taking the min-point
         * on the maxy_side and minx_side means increment.
         */
        m = (m_max_point == (m_side == miny_side || m_side == maxx_side)) ? 1u : 0u;

        return m + 2u * s;
      }

      /*!
       * Returns what arc-corner the point is a part of
       */
      enum corner_t
      corner(void) const
      {
        unsigned int x, y;

        if (m_side == miny_side || m_side == maxy_side)
          {
            x = (m_max_point) ? maxx_mask : 0u;
            y = (m_side == maxy_side) ? maxy_mask : 0u;
          }
        else
          {
            x = (m_side == maxx_side) ? maxx_mask : 0u;
            y = (m_max_point) ? maxy_mask : 0u;
          }

        return static_cast<enum corner_t>(x | y);
      }
    };

    /*!
     * Default ctor
     * \param rect value to pass to ctor of astral::Rect
     * \param corner_radius initial value for \ref m_corner_radius
     */
    UniformRoundedRect(const Rect &rect, float corner_radius):
      Rect(rect),
      m_corner_radius(corner_radius)
    {}

    /*!
     * Ctor
     */
    UniformRoundedRect(void):
      m_corner_radius(0.0f)
    {}

    /*!
     * Set the corner radii.
     */
    UniformRoundedRect&
    corner_radius(float v)
    {
      m_corner_radius = v;
      return *this;
    }

    /*!
     * Set the corner radius to 0.
     */
    UniformRoundedRect&
    make_flat(void)
    {
      return corner_radius(0.0f);
    }

    /*!
     * Returns true if the corner radius is 0.
     */
    bool
    is_flat(void) const
    {
      return m_corner_radius == 0.0f;
    }

    /*!
     * Sanitize the UniformRoundedRect as follows:
     *  - the corner radius is non-negative
     *  - the corner radius is no more than half the width
     *  - the corner radius is no more than half the height
     */
    UniformRoundedRect&
    sanitize(void)
    {
      standardize();
      m_corner_radius = t_min(m_corner_radius, 0.5f * t_min(width(), height()));
      return *this;
    }

    /*!
     * Specifies the radii at each of the corners
     */
    float m_corner_radius;
  };

  /*!
   * \brief
   * Class to specify the geometry of a rounded rectangle;
   * the geometry of the base class Rect defines the -bounding-
   * rectangle of the \ref RoundedRect
   */
  class RoundedRect:public Rect
  {
  public:
    /*!
     * Typedef to specify a point of an astral::RoundedRect
     */
    typedef UniformRoundedRect::Point Point;

    /*!
     * Default ctor, intializing all fields as zero,
     */
    RoundedRect(void):
      m_corner_radii(vec2(0.0f))
    {}

    /*!
     * Ctor from an astral::UniformRoundedRect
     */
    RoundedRect(const astral::UniformRoundedRect &R):
      Rect(R),
      m_corner_radii(vec2(R.m_corner_radius))
    {}

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * m_corner_radii[c] = v;
     * \endcode
     * \param c which radii corner to set
     * \param v value to which to set the corner radii
     */
    RoundedRect&
    corner_radius(enum corner_t c, vec2 v)
    {
      m_corner_radii[c] = v;
      return *this;
    }

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * corner_radius(c, vec2(v, v));
     * \endcode
     * \param c which radii corner to set
     * \param v value to which to set the corner radii
     */
    RoundedRect&
    corner_radius(enum corner_t c, float v)
    {
      return corner_radius(c, vec2(v, v));
    }

    /*!
     * Provided as a conveniance, sets each element of
     * \ref m_corner_radii to the named value.
     * \param v value to which to set the corner radii
     */
    RoundedRect&
    corner_radii(vec2 v)
    {
      m_corner_radii[0] = m_corner_radii[1] = v;
      m_corner_radii[2] = m_corner_radii[3] = v;
      return *this;
    }

    /*!
     * Provided as a conveniance, sets each element of
     * \ref m_corner_radii to the named value.
     * \param v value to which to set the corner radii
     */
    RoundedRect&
    corner_radii(float v)
    {
      return corner_radii(vec2(v, v));
    }

    /*!
     * Set all corner radii values to 0.
     */
    RoundedRect&
    make_flat(void)
    {
      return corner_radii(0.0f);
    }

    /*!
     * Returns true if each corner radii is 0.
     */
    bool
    is_flat(void) const
    {
      return m_corner_radii[0] == vec2(0.0f)
        && m_corner_radii[1] == vec2(0.0f)
        && m_corner_radii[2] == vec2(0.0f)
        && m_corner_radii[3] == vec2(0.0f);
    }

    /*!
     * Sanitize the RoundedRect as follows:
     *  - each of the corner radii is non-negative
     *  - each of the corner radii is no more than half the dimension
     *  - both the width and height are non-negative
     */
    RoundedRect&
    sanitize_simple(void)
    {
      float fw, fh;

      standardize();
      fw = 0.5f * width();
      fh = 0.5f * height();

      m_corner_radii[0].x() = t_min(fw, t_max(m_corner_radii[0].x(), 0.0f));
      m_corner_radii[1].x() = t_min(fw, t_max(m_corner_radii[1].x(), 0.0f));
      m_corner_radii[2].x() = t_min(fw, t_max(m_corner_radii[2].x(), 0.0f));
      m_corner_radii[3].x() = t_min(fw, t_max(m_corner_radii[3].x(), 0.0f));

      m_corner_radii[0].y() = t_min(fh, t_max(m_corner_radii[0].y(), 0.0f));
      m_corner_radii[1].y() = t_min(fh, t_max(m_corner_radii[1].y(), 0.0f));
      m_corner_radii[2].y() = t_min(fh, t_max(m_corner_radii[2].y(), 0.0f));
      m_corner_radii[3].y() = t_min(fh, t_max(m_corner_radii[3].y(), 0.0f));

      return *this;
    }

    /*!
     * Sanitize the RoundedRect as follows:
     *  - each of the corner radii is non-negative
     *  - the sum of the corner radii along any side is no more
     *    than the length of the side; if the input was larger,
     *    the values are scaled to preserve their relative ratio
     *  - both the width and height are non-negative
     */
    RoundedRect&
    sanitize_scale(void)
    {
      float w, h, s;

      standardize();
      w = width();
      h = height();
      s = 1.0f;

      // make all corner radii non-negative
      m_corner_radii[0].x() = t_max(m_corner_radii[0].x(), 0.0f);
      m_corner_radii[1].x() = t_max(m_corner_radii[1].x(), 0.0f);
      m_corner_radii[2].x() = t_max(m_corner_radii[2].x(), 0.0f);
      m_corner_radii[3].x() = t_max(m_corner_radii[3].x(), 0.0f);

      m_corner_radii[0].y() = t_max(m_corner_radii[0].y(), 0.0f);
      m_corner_radii[1].y() = t_max(m_corner_radii[1].y(), 0.0f);
      m_corner_radii[2].y() = t_max(m_corner_radii[2].y(), 0.0f);
      m_corner_radii[3].y() = t_max(m_corner_radii[3].y(), 0.0f);

      /* now compute the necessary scaling factor, after this is
       * run, it is guaranteed, up to floating point arithmatic,
       * that s * (r0 + r1) <= side_length where r0 and r1
       * are the first two arguments and side_length is the
       * third argument.
       */
      s = compute_scale(m_corner_radii[minx_miny_corner].x(), m_corner_radii[maxx_miny_corner].x(), w, s);
      s = compute_scale(m_corner_radii[minx_maxy_corner].x(), m_corner_radii[maxx_maxy_corner].x(), w, s);
      s = compute_scale(m_corner_radii[minx_miny_corner].y(), m_corner_radii[minx_maxy_corner].y(), h, s);
      s = compute_scale(m_corner_radii[maxx_miny_corner].y(), m_corner_radii[maxx_maxy_corner].y(), h, s);

      m_corner_radii[0] *= s;
      m_corner_radii[1] *= s;
      m_corner_radii[2] *= s;
      m_corner_radii[3] *= s;

      return *this;
    }

    /*!
     * Specifies the radii at each of the corners,
     * enumerated by \ref corner_t
     */
    vecN<vec2, 4> m_corner_radii;

  private:
    static
    float
    compute_scale(float radius0, float radius1, float side_length, float current_value)
    {
      if (radius0 + radius1 > side_length)
        {
          return t_min(current_value, side_length / (radius0 + radius1));
        }
      else
        {
          return current_value;
        }
    }
  };
/*! @} */
}

#endif

/*!
 * \file rect.hpp
 * \brief file rect.hpp
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

#ifndef ASTRAL_RECT_HPP
#define ASTRAL_RECT_HPP

#include <utility>
#include <astral/util/vecN.hpp>
#include <astral/util/math.hpp>
#include <astral/util/c_array.hpp>

namespace astral
{
/*!\addtogroup Utility
 * @{
 */

  /*!
   * \brief
   * Class to specify enumerations used by \ref RectT.
   */
  class RectEnums
  {
  public:
    /*!
     * Enumeration type to specify the underlying types
     * of \ref maxx_mask and \ref maxy_mask
     */
    enum max_masks:uint32_t
      {
        maxx_mask = 1, /**< bitmask on \ref corner_t to test if on max-x side */
        maxy_mask = 2  /**< bitmask on \ref corner_t to test if on max-y side */
      };

    /*!
     * \brief
     * Conveniance enumeration to name the rounded corner
     * radii of a RoundedRect.
     */
    enum corner_t:uint32_t
      {
        minx_miny_corner = 0,
        minx_maxy_corner = maxy_mask,
        maxx_miny_corner = maxx_mask,
        maxx_maxy_corner = maxx_mask | maxy_mask,
      };

    /*!
     * Returns an integer value in {0, 1, 2, 3} from a
     * \ref corner_t value so that consecitive values
     * walk clockwise (assuming y-max is the bottom)
     * around the sides of a rectangle
     */
    static
    unsigned int
    point_index(enum corner_t s)
    {
      /* we cannot just cast to an unsigned int
       * because the corner_t values come from
       * bit masks.
       *
       * The ordering we do is
       *  0. minx_miny
       *  1. maxx_miny
       *  2. maxx_maxy
       *  3. minx_maxy
       *
       * So:
       *  A. if the maxy_mask bit is up, then it is 2 or 3
       *  B. if the maxy_mask bit is up, then reverse the impact from maxx_mask
       */
      unsigned int v, x, y;

      x = (s & maxx_mask) ? 1u : 0u;
      y = (s & maxy_mask) ? 2u : 0u;
      v = (y) ? (3u - x) : x;

      return v;
    }

    /*!
     * The inverse of point_index(enum corner_t)
     */
    static
    enum corner_t
    corner_from_point_index(unsigned int s)
    {
      unsigned int x, y;

      ASTRALassert(s < 4u);

      /* x-max happens at s = 1, s = 2 */
      x = (s == 1u || s == 2u) ? maxx_mask : 0u;

      /* y-max happens at s = 2, 3 */
      y = (s >= 2u) ? maxy_mask : 0u;

      return static_cast<corner_t>(x | y);
    }

    /*!
     * \brief
     * Enumeration to name the -side- of a rect
     */
    enum side_t:uint32_t
      {
        /*!
         * The side where y is minimum (i.e. the top side
         * when the coordinate convention is that y increases
         * downwards)
         */
        miny_side = 0u,

        /*!
         * The side where x is maximum (i.e. the right side)
         */
        maxx_side,

        /*!
         * The side where y is maximum (i.e. the bottom side
         * when the coordinate convention is that y increases
         * downwards)
         */
        maxy_side,

        /*!
         * The side where x is minimum (i.e. the left side)
         */
        minx_side,
      };

    /*!
     * Returns an integer value in {0, 1, 2, 3} from a
     * \ref side_t value so that consecitive values
     * walk clockwise (assuming y-max is the bottom)
     * around the sides of a rectangle
     */
    static
    unsigned int
    point_index(enum side_t s)
    {
      return s;
    }

    /*!
     * The inverse of point_index(enum side_t)
     */
    static
    enum side_t
    side_from_point_index(unsigned int s)
    {
      ASTRALassert(s < 4);
      return static_cast<enum side_t>(s);
    }
  };

  /*!
   * \brief
   * Class to specify the geometry of a rectangle.
   */
  template<typename T>
  class RectT:public RectEnums
  {
  public:
    /*!
     * Empty ctor; intializes both \ref m_min_point and
     * \ref m_max_point to (0, 0);
     */
    RectT(void):
      m_min_point(T(0), T(0)),
      m_max_point(T(0), T(0))
    {}

    /*!
     * Copy ctor from different rect type
     */
    template<typename S>
    explicit
    RectT(const RectT<S> &rect):
      m_min_point(rect.m_min_point),
      m_max_point(rect.m_max_point)
    {}

    /*!
     * Set \ref m_min_point.
     */
    RectT&
    min_point(const vecN<T, 2> &p)
    {
      m_min_point = p;
      return *this;
    }

    /*!
     * Set \ref m_min_point.
     */
    template<typename S>
    RectT&
    min_point(const vecN<S, 2> &p)
    {
      m_min_point = vecN<T, 2>(p);
      return *this;
    }

    /*!
     * Set \ref m_min_point.
     */
    RectT&
    min_point(T x, T y)
    {
      m_min_point.x() = x;
      m_min_point.y() = y;
      return *this;
    }

    /*!
     * Set \ref m_max_point.
     */
    RectT&
    max_point(const vecN<T, 2> &p)
    {
      m_max_point = p;
      return *this;
    }

    /*!
     * Set \ref m_max_point.
     */
    template<typename S>
    RectT&
    max_point(const vecN<S, 2> &p)
    {
      m_max_point = vecN<T, 2>(p);
      return *this;
    }

    /*!
     * Set \ref m_max_point.
     */
    RectT&
    max_point(T x, T y)
    {
      m_max_point.x() = x;
      m_max_point.y() = y;
      return *this;
    }

    /*!
     * Inset the rect, i.e. increase \ref m_min_point
     * and decrease \ref m_max_point.
     * \param x amount by which to increment the x-coordinate of
     *          \ref m_min_point and decrement the x-coordinate
     *          of \ref m_max_point
     * \param y amount by which to increment the y-coordinate of
     *          \ref m_min_point and decrement the y-coordinate
     *          of \ref m_max_point
     */
    RectT&
    inset(T x, T y)
    {
      m_min_point.x() += x;
      m_min_point.y() += y;
      m_max_point.x() -= x;
      m_max_point.y() -= y;

      return *this;
    }

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * inset(v, v)
     * \endcode
     * \param v amount by which to increment \ref m_min_point
     *          and decrement \ref m_max_point
     */
    RectT&
    inset(T v)
    {
      return inset(v, v);
    }

    /*!
     * Outset the rect, i.e. decrease \ref m_min_point
     * and increase \ref m_max_point.
     * \param x amount by which to decrement the x-coordinate of
     *          \ref m_min_point and increment the x-coordinate
     *          of \ref m_max_point
     * \param y amount by which to decrement the y-coordinate of
     *          \ref m_min_point and increment the y-coordinate
     *          of \ref m_max_point
     */
    RectT&
    outset(T x, T y)
    {
      m_min_point.x() -= x;
      m_min_point.y() -= y;
      m_max_point.x() += x;
      m_max_point.y() += y;

      return *this;
    }

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * outset(v, v)
     * \endcode
     * \param v amount by which to decrement \ref m_min_point
     *          and increment \ref m_max_point
     */
    RectT&
    outset(T v)
    {
      return outset(v, v);
    }

    /*!
     * Equivalent to \code m_min_point.x() \endcode
     */
    T&
    min_x(void) { return m_min_point.x(); }

    /*!
     * Equivalent to \code m_min_point.x() \endcode
     */
    T
    min_x(void) const { return m_min_point.x(); }

    /*!
     * Equivalent to \code m_min_point.y() \endcode
     */
    T&
    min_y(void) { return m_min_point.y(); }

    /*!
     * Equivalent to \code m_min_point.y() \endcode
     */
    T
    min_y(void) const { return m_min_point.y(); }

    /*!
     * Equivalent to \code m_max_point.x() \endcode
     */
    T&
    max_x(void) { return m_max_point.x(); }

    /*!
     * Equivalent to \code m_max_point.x() \endcode
     */
    T
    max_x(void) const { return m_max_point.x(); }

    /*!
     * Equivalent to \code m_max_point.y() \endcode
     */
    T&
    max_y(void) { return m_max_point.y(); }

    /*!
     * Equivalent to \code m_max_point.y() \endcode
     */
    T
    max_y(void) const { return m_max_point.y(); }

    /*!
     * Returns a reference to the value holding
     * the named side
     */
    T&
    side(enum side_t S)
    {
      vec2 *p;
      unsigned int idx;

      p = (S == maxx_side || S == maxy_side) ? &m_max_point : &m_min_point;
      idx = (S == minx_side || S == miny_side) ? 0u : 1u;

      return p->operator[](idx);
    }

    /*!
     * Returns a reference to the value holding
     * the named side
     */
    T
    side(enum side_t S) const
    {
      const vec2 *p;
      unsigned int idx;

      p = (S == maxx_side || S == maxy_side) ? &m_max_point : &m_min_point;
      idx = (S == minx_side || S == miny_side) ? 0u : 1u;

      return p->operator[](idx);
    }

    /*!
     * Return the named point of the Rect.
     * \param c which corner of the rect.
     */
    vecN<T, 2>
    point(enum corner_t c) const
    {
      vecN<T, 2> return_value;

      return_value.x() = (c & maxx_mask) ? max_x() : min_x();
      return_value.y() = (c & maxy_mask) ? max_y() : min_y();
      return return_value;
    }

    /*!
     * Returns the center of the rectangle.
     */
    vecN<T, 2>
    center_point(void) const
    {
      return (m_min_point + m_max_point) / T(2);
    }

    /*!
     * Translate the Rect, equivalent to
     * \code
     * m_min_point += tr;
     * m_max_point += tr;
     * \endcode
     * \param tr amount by which to translate
     */
    RectT&
    translate(const vecN<T, 2> &tr)
    {
      m_min_point += tr;
      m_max_point += tr;
      return *this;
    }

    /*!
     * Translate the Rect, equivalent to
     * \code
     * translate(vecN<T, 2>(x, y))
     * \endcode
     * \param x amount by which to translate in x-coordinate
     * \param y amount by which to translate in y-coordinate
     */
    RectT&
    translate(T x, T y)
    {
      return translate(vecN<T, 2>(x,y));
    }

    /*!
     * Set \ref m_max_point from \ref m_min_point
     * and a size. Equivalent to
     * \code
     * max_point(min_point() + sz)
     * \endcode
     * \param sz size to which to set the Rect
     */
    RectT&
    size(const vecN<T, 2> &sz)
    {
      m_max_point = m_min_point + sz;
      return *this;
    }

    /*!
     * Set \ref m_max_point from \ref m_min_point
     * and a size. Equivalent to
     * \code
     * max_point(min_point() + sz)
     * \endcode
     * \param sz size to which to set the Rect
     */
    template<typename S>
    RectT&
    size(const vecN<S, 2> &sz)
    {
      m_max_point = m_min_point + vecN<T, 2>(sz);
      return *this;
    }

    /*!
     * Set \ref m_max_point from \ref m_min_point
     * and a size. Equivalent to
     * \code
     * max_point(min_point() + vecN<T, 2>(width, height))
     * \endcode
     * \param width width to which to set the rect
     * \param height height to which to set the rect
     */
    RectT&
    size(T width, T height)
    {
      m_max_point.x() = m_min_point.x() + width;
      m_max_point.y() = m_min_point.y() + height;
      return *this;
    }

    /*!
     * Returns the size of the Rect; provided as
     * a conveniance, equivalent to
     * \code
     * m_max_point - m_min_point
     * \endcode
     */
    vecN<T, 2>
    size(void) const
    {
      return m_max_point - m_min_point;
    }

    /*!
     * Set the width of the Rect, equivalent to
     * \code
     * m_max_point.x() = w + m_min_point.x();
     * \endcode
     * \param w value to make the width of the rect
     */
    RectT&
    width(T w)
    {
      m_max_point.x() = w + m_min_point.x();
      return *this;
    }

    /*!
     * Set the height of the Rect, equivalent to
     * \code
     * m_max_point.y() = h + m_min_point.y();
     * \endcode
     * \param h value to make the height of the rect
     */
    RectT&
    height(T h)
    {
      m_max_point.y() = h + m_min_point.y();
      return *this;
    }

    /*!
     * Returns the width of the Rect, equivalent to
     * \code
     * m_max_point.x() - m_min_point.x();
     * \endcode
     */
    T
    width(void) const
    {
      return m_max_point.x() - m_min_point.x();
    }

    /*!
     * Returns the width of the Rect, equivalent to
     * \code
     * m_max_point.y() - m_min_point.y();
     * \endcode
     */
    T
    height(void) const
    {
      return m_max_point.y() - m_min_point.y();
    }

    /*!
     * Standardizes the rect so that the min-coordinate
     * values are no more than the max-coordinate values.
     */
    RectT&
    standardize(void)
    {
      if (m_min_point.x() > m_max_point.x())
        {
          std::swap(m_min_point.x(), m_max_point.x());
        }

      if (m_min_point.y() > m_max_point.y())
        {
          std::swap(m_min_point.y(), m_max_point.y());
        }
      return *this;
    }

    /*!
     * Returns true if the rectange is standardized,
     * i.e. the min-corner point coordinate values are
     * no more than the max-corner point coordinate
     * values
     */
    bool
    is_standardized(void) const
    {
      return m_min_point.x() <= m_max_point.x()
        && m_min_point.y() <= m_max_point.y();
    }

    /*!
     * Compute the set different of this against another
     * rect. That set difference is guaranteed to be
     * no more than four rects. This rect must be
     * standardized.
     * \param rhs the rect that subtracts from this rect,
     *            must be standardized.
     * \param results location to place results, the passed
     *                array must have a size of at least 4.
     * \returns the number of rects in the difference
     */
    unsigned int
    compute_difference(RectT rhs, c_array<RectT> results) const
    {
      unsigned int return_value(0);

      ASTRALassert(results.size() >= 4u);
      ASTRALassert(is_standardized());
      ASTRALassert(rhs.is_standardized());

      if (rhs.m_max_point.x() < m_min_point.x()
          || rhs.m_max_point.y() < m_min_point.y()
          || m_max_point.x() < rhs.m_min_point.x()
          || m_max_point.y() < rhs.m_min_point.y())
        {
          results[0] = *this;
          return 1u;
        }

      /* first make rhs to be contained within *this */
      rhs.m_min_point.x() = t_max(rhs.m_min_point.x(), m_min_point.x());
      rhs.m_min_point.y() = t_max(rhs.m_min_point.y(), m_min_point.y());

      rhs.m_max_point.x() = t_min(rhs.m_max_point.x(), m_max_point.x());
      rhs.m_max_point.y() = t_min(rhs.m_max_point.y(), m_max_point.y());

      /* The stripe to the left of rhs */
      if (m_min_point.x() < rhs.m_min_point.x())
        {
          results[return_value].m_min_point = m_min_point;
          results[return_value].m_max_point.x() = rhs.m_min_point.x();
          results[return_value].m_max_point.y() = m_max_point.y();
          ++return_value;
        }

      /* The stripe to the right of rhs */
      if (m_max_point.x() > rhs.m_max_point.x())
        {
          results[return_value].m_min_point.x() = rhs.m_max_point.x();
          results[return_value].m_min_point.y() = m_min_point.y();
          results[return_value].m_max_point = m_max_point;
          ++return_value;
        }

      /* the stripe above rhs */
      if (m_min_point.y() < rhs.m_min_point.y())
        {
          results[return_value].m_min_point.x() = rhs.m_min_point.x();
          results[return_value].m_max_point.x() = rhs.m_max_point.x();

          results[return_value].m_min_point.y() = m_min_point.y();
          results[return_value].m_max_point.y() = rhs.m_min_point.y();
          ++return_value;
        }

      /* the stripe below rhs */
      if (m_max_point.y() > rhs.m_max_point.y())
        {
          results[return_value].m_min_point.x() = rhs.m_min_point.x();
          results[return_value].m_max_point.x() = rhs.m_max_point.x();

          results[return_value].m_min_point.y() = rhs.m_max_point.y();
          results[return_value].m_max_point.y() = m_max_point.y();
          ++return_value;
        }

      return return_value;
    }

    /*!
     * Compute the set different of this against another
     * rect. That set difference is guaranteed to be
     * no more than four rects. This rect must be
     * standardized.
     * \param rhs the rect that subtracts from this rect,
     *            must be standardized.
     * \param results location to place results, the passed
     *                array must have a size of at least 4.
     * \returns the number of rects in the difference
     */
    unsigned int
    compute_difference(const RectT &rhs, vecN<RectT, 4> *results) const
    {
      return compute_difference(rhs, c_array<RectT>(*results));
    }

    /*!
     * Compute the set different of this against another
     * rect. That set difference is guaranteed to have no
     * T-intersections. This rect must be standardized,
     * \param rhs the rect that subtracts from this rect,
     *            must be standardized.
     * \param results location to place results, the passed
     *                array must have a size of at least 8.
     * \returns the number of rects in the difference
     */
    unsigned int
    compute_difference_for_rasterization(RectT rhs, c_array<RectT> results) const
    {
      unsigned int return_value(0);

      ASTRALassert(results.size() >= 8u);
      ASTRALassert(is_standardized());
      ASTRALassert(rhs.is_standardized());

      /* return *this rect if there is no intersection */
      if (rhs.m_max_point.x() < m_min_point.x()
          || rhs.m_max_point.y() < m_min_point.y()
          || m_max_point.x() < rhs.m_min_point.x()
          || m_max_point.y() < rhs.m_min_point.y())
        {
          results[0] = *this;
          return 1u;
        }

      /* first make rhs to be contained within *this by
       * reducing rhs to its intersection with *this
       */
      compute_intersection(*this, rhs, &rhs);

      /* The rect joining at the top-left corner of rhs */
      if (m_min_point.x() < rhs.m_min_point.x() &&
          m_min_point.y() < rhs.m_min_point.y())
        {
          results[return_value].m_min_point.x() = m_min_point.x();
          results[return_value].m_min_point.y() = m_min_point.y();
          results[return_value].m_max_point.x() = rhs.m_min_point.x();
          results[return_value].m_max_point.y() = rhs.m_min_point.y();
          ++return_value;
        }

        /* The rect adjacent to the top edge of rhs */
      if (m_min_point.y() < rhs.m_min_point.y())
        {
          results[return_value].m_min_point.x() = rhs.m_min_point.x();
          results[return_value].m_min_point.y() = m_min_point.y();
          results[return_value].m_max_point.x() = rhs.m_max_point.x();
          results[return_value].m_max_point.y() = rhs.m_min_point.y();
          ++return_value;
        }

      /* The rect joining at the top-right corner of rhs */
      if (rhs.m_max_point.x() < m_max_point.x() &&
          m_min_point.y() < rhs.m_min_point.y())
        {
          results[return_value].m_min_point.x() = rhs.m_max_point.x();
          results[return_value].m_min_point.y() = m_min_point.y();
          results[return_value].m_max_point.x() = m_max_point.x();
          results[return_value].m_max_point.y() = rhs.m_min_point.y();
          ++return_value;
        }

      /* The rect adjacent to the right edge of rhs */
      if (rhs.m_max_point.x() < m_max_point.x())
        {
          results[return_value].m_min_point.x() = rhs.m_max_point.x();
          results[return_value].m_min_point.y() = rhs.m_min_point.y();
          results[return_value].m_max_point.x() = m_max_point.x();
          results[return_value].m_max_point.y() = rhs.m_max_point.y();
          ++return_value;
        }

      /* The rect joining at the bottom-right corner of rhs */
      if (rhs.m_max_point.x() < m_max_point.x() &&
          rhs.m_max_point.y() < m_max_point.y())
        {
          results[return_value].m_min_point.x() = rhs.m_max_point.x();
          results[return_value].m_min_point.y() = rhs.m_max_point.y();
          results[return_value].m_max_point.x() = m_max_point.x();
          results[return_value].m_max_point.y() = m_max_point.y();
          ++return_value;
        }

      /* The rect adjacent to the bottom edge of rhs */
      if (rhs.m_max_point.y() < m_max_point.y())
        {
          results[return_value].m_min_point.x() = rhs.m_min_point.x();
          results[return_value].m_min_point.y() = rhs.m_max_point.y();
          results[return_value].m_max_point.x() = rhs.m_max_point.x();
          results[return_value].m_max_point.y() = m_max_point.y();
          ++return_value;
        }

      /* The rect joining at the bottom-left corner of rhs */
      if (m_min_point.x() < rhs.m_min_point.x() &&
          rhs.m_max_point.y() < m_max_point.y())
        {
          results[return_value].m_min_point.x() = m_min_point.x();
          results[return_value].m_min_point.y() = rhs.m_max_point.y();
          results[return_value].m_max_point.x() = rhs.m_min_point.x();
          results[return_value].m_max_point.y() = m_max_point.y();
          ++return_value;
        }

      /* The rect adjacent to the left edge of rhs */
      if (m_min_point.x() < rhs.m_min_point.x())
        {
          results[return_value].m_min_point.x() = m_min_point.x();
          results[return_value].m_min_point.y() = rhs.m_min_point.y();
          results[return_value].m_max_point.x() = rhs.m_min_point.x();
          results[return_value].m_max_point.y() = rhs.m_max_point.y();
          ++return_value;
        }

      return return_value;
    }

    /*!
     * Compute the set different of this against another
     * rect. That set difference is guaranteed to have no
     * T-intersections. This rect must be standardized.
     * \param rhs the rect that subtracts from this rect,
     *            must be standardized.
     * \param results location to place results, the passed
     *                array must have a size of at least 8.
     * \returns the number of rects in the difference
     */
    unsigned int
    compute_difference_for_rasterization(RectT rhs, vecN<RectT, 8> *results) const
    {
      return compute_difference_for_rasterization(rhs, c_array<RectT>(*results));
    }

    /*!
     * Computes the intersection of two standardized rects
     * \param A first rect, must be standardized
     * \param B second rect, must be standardized
     * \param out_rect location to which to write the intersection;
     *                 if the output value is non-standardized, then
     *                 the intersection is empty
     * \returns true if the intersection is non-empty
     */
    static
    bool
    compute_intersection(const RectT &A, const RectT &B, RectT *out_rect)
    {
      ASTRALassert(A.is_standardized());
      ASTRALassert(B.is_standardized());

      out_rect->m_min_point.x() = t_max(A.m_min_point.x(), B.m_min_point.x());
      out_rect->m_min_point.y() = t_max(A.m_min_point.y(), B.m_min_point.y());

      out_rect->m_max_point.x() = t_min(A.m_max_point.x(), B.m_max_point.x());
      out_rect->m_max_point.y() = t_min(A.m_max_point.y(), B.m_max_point.y());

      return out_rect->is_standardized();
    }

    /*!
     * Specifies the min-corner of the rectangle
     */
    vecN<T, 2> m_min_point;

    /*!
     * Specifies the max-corner of the rectangle.
     */
    vecN<T, 2> m_max_point;
  };

  /*!
   * \brief
   * Conveniance typedef for RectT<float>
   */
  typedef RectT<float> Rect;

/*! @} */
}

#endif

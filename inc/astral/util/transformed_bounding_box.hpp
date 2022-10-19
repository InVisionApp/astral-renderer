/*!
 * \file transformed_bounding_box.hpp
 * \brief file transformed_bounding_box.hpp
 *
 * Copyright 2021 by InvisionApp.
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

#ifndef ASTRAL_TRANSFORMED_BOUNDING_BOX_HPP
#define ASTRAL_TRANSFORMED_BOUNDING_BOX_HPP

#include <astral/util/vecN.hpp>
#include <astral/util/rect.hpp>
#include <astral/util/transformation.hpp>
#include <astral/util/bounding_box.hpp>

namespace astral
{
  /*!
   * \brief
   * An astral::TransformedBoundingBox represents the exact
   * region of an astral::BoundingBox<float> transformed by
   * an astral::Transformation.
   *
   * TODO: maybe make this completely private.
   */
  class TransformedBoundingBox
  {
  public:
    /*!
     * A Normalized value has a number of values normalized
     * for the purpose of computing the axis-aligned bounding
     * box of the intersection against an axis-aligned bounding
     * box.
     */
    class Normalized;

    /*!
     * Ctor to initialize as an empty bounding box.
     */
    TransformedBoundingBox(void)
    {}

    /*!
     * Ctor from an astral::BoundingBox<float>
     * \param bb bounding box.
     */
    explicit
    TransformedBoundingBox(const BoundingBox<float> &bb);

    /*!
     * Ctor from an astral::BoundingBox<float>
     * and a transformation to apply to it
     * \param bb bounding box
     * \param tr transformation to apply to bb
     */
    TransformedBoundingBox(const BoundingBox<float> &bb,
                           const Transformation &tr);

    /*!
     * Returns true if and only if this astral::TransformedBoundingBox
     * intersects a given astral::BoundingBox<float>
     * \param bb box to which to test against
     */
    bool
    intersects(const BoundingBox<float> &bb) const;

    /*!
     * Returns true if and only if this astral::TransformedBoundingBox
     * intersects a given astral::TransformedBoundingBox
     * \param bb box to which to test against
     */
    bool
    intersects(const TransformedBoundingBox &bb) const;

    /*!
     * Returns true exactly when the passed astral::BoundingBox<float>
     * is contained by this astral::TransformedBoundingBox
     */
    bool
    contains(const BoundingBox<float> &bb) const;

    /*!
     * Returns true exactly when the passed astral::TransformedBoundingBox
     * is contained by this astral::TransformedBoundingBox
     */
    bool
    contains(const TransformedBoundingBox &bb) const;

    /*!
     * Returns true exactly when the passed astral::vec2
     * is contained by this astral::TransformedBoundingBox
     */
    bool
    contains(const vec2 &pt) const;

    /*!
     * Returns the axis aligned boundinb box that tightly
     * contains this astral::TransformedBoundingBox. If the
     * return value has empty() as true, then this box is
     * also empty.
     */
    const BoundingBox<float>&
    containing_aabb(void) const
    {
      return m_bb;
    }

    /*!
     * Returns true if and only if this astral::TransformedBoundingBox
     * is axis aligned.
     */
    bool
    is_axis_aligned(void) const
    {
      return m_is_aligned_bb;
    }

    /*!
     * The points of the rect.
     */
    const vecN<vec2, 4>&
    pts(void) const
    {
      ASTRALassert(!m_bb.empty());
      return m_pts;
    }

    /*!
     * Returns true exactly when this astral::TransformedBoundingBox
     * is empty
     */
    bool
    empty(void) const
    {
      return m_bb.empty();
    }

  private:
    class Interval
    {
    public:
      Interval(void)
      {}

      Interval(float m, float M):
        m_min(m),
        m_max(M)
      {}

      bool
      intersects(Interval obj) const
      {
        return !(obj.m_min > m_max || m_min > obj.m_max);
      }

      bool
      contains(float v) const
      {
        return m_min <= v && v <= m_max;
      }

      bool
      contains(Interval obj) const
      {
        return contains(obj.m_min) && contains(obj.m_max);
      }

      float m_min, m_max;
    };

    void
    init_values_from_aabb(void);

    bool //returns true if m_normals[axis] seperates m_pts from pts
    axis_seperates(unsigned int axis, const vecN<vec2, 4> &pts) const;

    /* The intersection test is essentially the seperating
     * axis theorem for convex polygons: Let P = {p_i} and
     * Q = {q_i} be two convex polygone and let {n_i} be
     * the normal vectors of the sides of P and {m_i} be
     * the normal veccros of the sides of Q. Give a vector
     * v, define S(v, P) = [min(<p_i, v>), max(<p_i, v>)]
     * and S(v, Q) = [min(<q_i, v>), max(<q_i, v>)]. Then
     * P and Q are disjoint if and only if there exists an
     * v in ({n_i} union {m_i}) with S(v, p) disjoint from
     * S(v, Q).
     *
     * A transformed bounding box has that its normal vectors
     * are of the form {a, -a, b, -b}. Note that
     *
     * S(-v, P) = [min(<-v, p>), max(<-v, p>)]
     *          = [-max(<v, p>, -min(<v, p>)]
     *          = -S(v, p)
     *
     * We will store:
     *  - the four points {p_i} in m_pts
     *  - the normal vectors a and b in m_normals
     *  - S(a, P) and S(b, P) in m_intervals
     *  - the axis aligned bounding box containing p as well
     */
    vecN<vec2, 4> m_pts;
    vecN<vec2, 2> m_normals;
    vecN<Interval, 2> m_intervals;
    BoundingBox<float> m_bb;
    bool m_is_aligned_bb;
  };

  /*!
   * A TransformedBoundingBox::Normalized is a TransformedBoundingBox
   * whose internal representation is slightly more expensive to compute
   * but enables the ability to more quickly evalulate compute_intersection().
   */
  class TransformedBoundingBox::Normalized:public TransformedBoundingBox
  {
  public:
    /*!
     * Constructs a TransformedBoundingBox::Normalized from an
     * astral::TransformedBoundingBox; the ctor is slightly more
     * expensive than that of an astral::TransformedBoundingBox,
     * but the extra expense provides the means to implement
     * compute_intersection().
     */
    explicit
    Normalized(const TransformedBoundingBox &bb);

    /*!
     * Ctor from an astral::BoundingBox<float>
     * \param bb bounding box.
     */
    explicit
    Normalized(const BoundingBox<float> &bb);

    /*!
     * Ctor from an astral::BoundingBox<float>
     * and a transformation to apply to it
     * \param bb bounding box
     * \param tr transformation to apply to bb
     */
    Normalized(const BoundingBox<float> &bb,
               const Transformation &tr);

    /*!
     * Compute (quickly) an axis aligned bounding box that
     * contains the intersection of this value intersected
     * against an axis aligned bounding box. The returned
     * value is not as tight as it can be, but is tighter
     * than using the passed bounding if they intersect.
     * The value is equivalent to the following:
     *  - 1) transform bb to a coordiante system where this
     *       Normalized is axis-aligned
     *  - 2) compute the intersection of thist against
     *       that bounding box
     *  - 3) transform the value of (2) back to coordinates
     *       system of the argument producing a larger bounding
     *  - 4) intersect (3) agains the argument value
     *  .
     * However, the computation is *fast* requiring much less
     * computation than the above steps.
     */
    BoundingBox<float>
    compute_intersection(const BoundingBox<float> &bb) const;

  private:
    void
    normalize_values(void);
  };

}

#endif

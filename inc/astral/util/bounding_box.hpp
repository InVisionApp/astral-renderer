/*!
 * \file bounding_box.hpp
 * \brief file bounding_box.hpp
 *
 * Copyright 2016 by Intel.
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


#ifndef ASTRAL_BOUNDING_BOX_HPP
#define ASTRAL_BOUNDING_BOX_HPP

#include <astral/util/vecN.hpp>
#include <astral/util/rect.hpp>

namespace astral
{
/*!\addtogroup Utility
 * @{
 */

  /*!
   * \brief
   * Simple bounding box class
   */
  template<typename T>
  class BoundingBox:private RectT<T>
  {
  public:
    /*!
     * conveniance typedef
     */
    typedef vecN<T, 2> pt_type;

    /*!
     * Ctor as the bounding box as empty
     */
    BoundingBox(void):
      m_empty(true)
    {
      this->m_min_point = this->m_max_point = pt_type(T(0), T(0));
    }

    /*!
     * Ctor the bounding box directly
     * \param pmin min-min corner of the bounding box
     * \param pmax max-max corner of the bounding box
     */
    BoundingBox(pt_type pmin, pt_type pmax):
      m_empty(false)
    {
      ASTRALassert(pmin.x() <= pmax.x());
      ASTRALassert(pmin.y() <= pmax.y());
      this->m_min_point = pmin;
      this->m_max_point = pmax;
    }

    /*!
     * Constructs a BoundingBox as a passed bounding box
     * enlarged or shrunk by the given amount.
     * \param bb original bb
     * \param size_delta amount by which to enlarge or shrink,
     *                   positive values indicate to enlarge
     *                   and negative values indicate to shrink
     */
    BoundingBox(const BoundingBox &bb, pt_type size_delta):
      BoundingBox(bb)
    {
      if (!m_empty)
        {
          this->m_min_point -= size_delta;
          this->m_max_point += size_delta;

          m_empty = (this->m_min_point.x() > this->m_max_point.x())
            || (this->m_min_point.y() > this->m_max_point.y());
        }
    }

    /*!
     * Ctor to construct the intersection of two bounding boxes
     */
    BoundingBox(const BoundingBox &a, const BoundingBox &b):
      BoundingBox(a)
    {
      intersect_against(b);
    }

    /*!
     * Ctor the bounding box directly
     * \param rect Rect from which to take the values
     */
    template<typename S>
    explicit
    BoundingBox(const RectT<S> &rect):
      RectT<T>(rect),
      m_empty(false)
    {
      ASTRALassert(this->m_min_point.x() <= this->m_max_point.x());
      ASTRALassert(this->m_min_point.y() <= this->m_max_point.y());
    }

    /*!
     * Return the rectangle as a convex polygon corresponding to this
     * bounding box inflated by an amount
     * \param out_data location to which to write the polygon
     * \param rad amount by which to inflate
     */
    void
    inflated_polygon(vecN<pt_type, 4> &out_data, T rad) const
    {
      ASTRALassert(!m_empty);
      out_data[0] = pt_type(this->m_min_point.x() - rad, this->m_min_point.y() - rad);
      out_data[1] = pt_type(this->m_max_point.x() + rad, this->m_min_point.y() - rad);
      out_data[2] = pt_type(this->m_max_point.x() + rad, this->m_max_point.y() + rad);
      out_data[3] = pt_type(this->m_min_point.x() - rad, this->m_max_point.y() + rad);
    }

    /*!
     * Clear the bounding box, i.e. make it empty.
     */
    void
    clear(void)
    {
      m_empty = true;
    }

    /*!
     * Enlarge the bounding box by an amount in each dimension.
     * If the box is empty, this has no effect
     * \param delta amount in each dimension by which to enlarge
     */
    void
    enlarge(pt_type delta)
    {
      if (!m_empty)
        {
          ASTRALassert(delta.x() >= T(0));
          ASTRALassert(delta.y() >= T(0));
          this->m_min_point -= delta;
          this->m_max_point += delta;

          m_empty = (this->m_min_point.x() > this->m_max_point.x())
            || (this->m_min_point.y() > this->m_max_point.y());
        }
    }

    /*!
     * Translate this bounding box
     * \param delta amount by which to translate
     */
    void
    translate(pt_type delta)
    {
      if (!m_empty)
        {
          this->m_min_point += delta;
          this->m_max_point += delta;
        }
    }

    /*!
     * Shrink the bounding box by an amount in each dimension.
     * If the box is empty, this has no effect
     * \param delta amount in each dimension by which to shrink;
     *              If the shrinkage is greater than the original
     *              box size, then the box becomes empty.
     */
    void
    shrink(pt_type delta)
    {
      if (!m_empty)
        {
          ASTRALassert(delta.x() >= T(0));
          ASTRALassert(delta.y() >= T(0));
          this->m_min_point += delta;
          this->m_max_point -= delta;

          m_empty = (this->m_min_point.x() > this->m_max_point.x())
            || (this->m_min_point.y() > this->m_max_point.y());
        }
    }

    /*!
     * Compute the bounding box enlarged by an amount in each dimension.
     * If the box is empty, the computed box is empty
     * \param delta amount in each dimension by which to enlarge
     * \param dst location to which to write the enlarged box
     */
    void
    enlarge(pt_type delta, BoundingBox *dst) const
    {
      dst->m_min_point = this->m_min_point;
      dst->m_max_point = this->m_max_point;
      dst->m_empty = m_empty;
      dst->enlarge(delta);
    }

    /*!
     * Compute the bounding box enlarged by an amount in each dimension.
     * If the box is empty, the computed box is empty
     * \param delta amount in each dimension by which to shrink
     * \param dst location to which to write the shrunk box
     */
    void
    shrink(pt_type delta, BoundingBox *dst) const
    {
      dst->m_min_point = this->m_min_point;
      dst->m_max_point = this->m_max_point;
      dst->m_empty = m_empty;
      dst->shrink(delta);
    }

    /*!
     * Enlarge the box to contain a point
     * \param pt point
     * \returns true if the large became larger from the operation
     */
    bool
    union_point(const pt_type &pt)
    {
      bool R;

      R = !contains(pt);
      if (m_empty)
        {
          m_empty = false;
          this->m_min_point = this->m_max_point = pt;
        }
      else
        {
          this->m_min_point.x() = t_min(this->m_min_point.x(), pt.x());
          this->m_min_point.y() = t_min(this->m_min_point.y(), pt.y());

          this->m_max_point.x() = t_max(this->m_max_point.x(), pt.x());
          this->m_max_point.y() = t_max(this->m_max_point.y(), pt.y());
        }
      return R;
    }

    /*!
     * Enlarge the box by a sequence of points
     * \param begin iterator to first point of the sequence
     * \param end iterator to one pas the last point of the sequence
     * \returns true if the large became larger from the operation
     */
    template<typename iterator>
    bool
    union_points(iterator begin, iterator end)
    {
      bool R(false);
      for(; begin != end; ++begin)
        {
          R = union_point(*begin) || R;
        }
      return R;
    }

    /*!
     * Enlarge the box to contain a bounding box
     * \param b bounding box to contain
     * \returns true if the large became larger from the operation
     */
    bool
    union_box(const BoundingBox &b)
    {
      if (!b.m_empty)
        {
          bool r0, r1;
          r0 = union_point(b.m_min_point);
          r1 = union_point(b.m_max_point);
          return r0 || r1;
        }
      return false;
    }

    /*!
     * Enlarge the box to contain a astral::RectT
     * \param b bounding box to contain
     * \returns true if the large became larger from the operation
     */
    bool
    union_box(const RectT<T> &b)
    {
      bool r0, r1;
      r0 = union_point(b.m_min_point);
      r1 = union_point(b.m_max_point);
      return r0 || r1;
    }

    /*!
     * Returns the dimensions of the bounding box
     */
    pt_type
    size(void) const
    {
      return m_empty ?
        pt_type(T(0), T(0)) :
        this->m_max_point - this->m_min_point;
    }

    /*!
     * Returns true if and only if the box is regared as empty
     */
    bool
    empty(void) const
    {
      return m_empty;
    }

    /*!
     * Returns the min-corner of the bounding box; undefined
     * if empty() is true.
     */
    const pt_type&
    min_point(void) const
    {
      return this->m_min_point;
    }

    /*!
     * Returns the max-corner of the bounding box; undefined
     * if empty() is true.
     */
    const pt_type&
    max_point(void) const
    {
      return this->m_max_point;
    }

    /*!
     * Split the box in the middle along the x-direction
     */
    vecN<BoundingBox<T>, 2>
    split_x(void) const
    {
      vecN<BoundingBox<T>, 2> R;

      if (empty())
        {
          return R;
        }

      pt_type center;

      center = this->center_point();
      R[0] = BoundingBox(this->m_min_point, pt_type(center.x(), this->m_max_point.y()));
      R[1] = BoundingBox(pt_type(center.x(), this->m_min_point.y()), this->m_max_point);
      return R;
    }

    /*!
     * Split the box in the middle along the y-direction
     */
    vecN<BoundingBox<T>, 2>
    split_y(void) const
    {
      vecN<BoundingBox<T>, 2> R;

      if (empty())
        {
          return R;
        }

      pt_type center;

      center = this->center_point();
      R[0] = BoundingBox(this->m_min_point, pt_type(this->m_max_point.x(), center.y()));
      R[1] = BoundingBox(pt_type(this->m_min_point.x(), center.y()), this->m_max_point);
      return R;
    }

    /*!
     * Returns true if and only if this bounding box
     * intersect another bounding box
     * \param obj bounding box to test against
     */
    bool
    intersects(const BoundingBox &obj) const
    {
      return !m_empty &&
        !(obj.m_min_point.x() > this->m_max_point.x()
          || this->m_min_point.x() > obj.m_max_point.x()
          || obj.m_min_point.y() > this->m_max_point.y()
          || this->m_min_point.y() > obj.m_max_point.y());
    }

    /*!
     * Change this bounding box to the intersection
     * with the passed bounding box
     * \param obj bounding box to intersect against
     */
    void
    intersect_against(const BoundingBox &obj)
    {
      m_empty = !intersects(obj);
      if (!m_empty)
        {
          this->m_min_point.x() = t_max(obj.m_min_point.x(), this->m_min_point.x());
          this->m_min_point.y() = t_max(obj.m_min_point.y(), this->m_min_point.y());
          this->m_max_point.x() = t_min(obj.m_max_point.x(), this->m_max_point.x());
          this->m_max_point.y() = t_min(obj.m_max_point.y(), this->m_max_point.y());
        }
    }

    /*!
     * Returns true if and only if a point p is
     * inside the bounding box
     * \param p point to test against
     */
    bool
    contains(const pt_type &p) const
    {
      return !m_empty
        && p.x() >= this->m_min_point.x()
        && p.x() <= this->m_max_point.x()
        && p.y() >= this->m_min_point.y()
        && p.y() <= this->m_max_point.y();
    }

    /*!
     * Returns true if and only if another bounding
     * box is contained within this bounding box.
     * \param box box to test against
     */
    bool
    contains(const BoundingBox &box) const
    {
      return box.empty()
        || (!empty()
            && box.m_min_point.x() >= this->m_min_point.x()
            && box.m_min_point.y() >= this->m_min_point.y()
            && box.m_max_point.x() <= this->m_max_point.x()
            && box.m_max_point.y() <= this->m_max_point.y());
    }

    /*!
     * Compute the interolation of this bounding box
     * collapsing to its center point; time 0 gives the
     * original box and time 1 gives the box collapsed
     * to the center point.
     * \param t time of interpolation
     * \param return_value location to which to write the
     *                     interpolation
     */
    void
    interpolate_to_center(T t, BoundingBox *return_value) const
    {
      pt_type p(this->center_point());
      T s(T(1) - t);

      return_value->m_empty = m_empty;
      return_value->m_max_point = s * this->m_max_point + t * p;
      return_value->m_min_point = s * this->m_min_point + t * p;
    }

    /*!
     * compute the interpolation of two bounding boxes
     * \param b0 the bounding box at time 0
     * \param b1 the bounding box at time 1
     * \param t time of interpolation
     * \param return_value location to which to write the
     *                     interpolation
     */
    static
    void
    interpolate(const BoundingBox &b0, const BoundingBox &b1, T t,
                BoundingBox *return_value)
    {
      if (b0.empty() && b1.empty())
        {
          return_value->m_empty = true;
        }
      else if (b0.empty())
        {
          b1.interpolate_to_center(t, return_value);
        }
      else if (b1.empty())
        {
          b0.interpolate_to_center(t, return_value);
        }
      else
        {
          T s(T(1) - t);

          return_value->m_empty = false;
          return_value->m_max_point = s * b0.m_max_point + t * b1.m_max_point;
          return_value->m_min_point = s * b0.m_min_point + t * b1.m_min_point;
        }
    }

    /*!
     * Computes the L1-distance between a point and the boundary.
     * It is an error if the box is empty.
     * \param pt point to query
     */
    float
    distance_to_boundary(const pt_type &pt) const
    {
      T return_value;

      ASTRALassert(!m_empty);
      if (pt.x() >= this->m_min_point.x() && pt.x() <= this->m_max_point.x())
        {
          return_value = t_min(t_abs(pt.y() - this->m_min_point.y()), t_abs(pt.y() - this->m_max_point.y()));
          if (pt.y() >= this->m_min_point.y() && pt.y() <= this->m_max_point.y())
            {
              T x;

              x = t_min(t_abs(pt.x() - this->m_min_point.x()), t_abs(pt.x() - this->m_max_point.x()));
              return_value = t_min(x, return_value);
            }
        }
      else if (pt.y() >= this->m_min_point.y() && pt.y() <= this->m_max_point.y())
        {
          return_value = t_min(t_abs(pt.x() - this->m_min_point.x()), t_abs(pt.x() - this->m_max_point.x()));
        }
      else
        {
          pt_type Q;

          Q = pt - this->m_min_point;
          return_value = Q.L1norm();

          Q = pt - this->m_max_point;
          return_value = t_min(return_value, Q.L1norm());

          Q = pt - this->point(RectEnums::minx_maxy_corner);
          return_value = t_min(return_value, Q.L1norm());

          Q = pt - this->point(RectEnums::maxx_miny_corner);
          return_value = t_min(return_value, Q.L1norm());
        }

      return return_value;
    }

    /*!
     * Returns the bounding box as an astral::RectT; undefined
     * results if empty() is true.
     */
    const RectT<T>&
    as_rect(void) const
    {
      ASTRALassert(!m_empty);
      return *this;
    }

    /*!
     * Restricts a tolerance, essentially
     * \code
     * float d;
     * vec2 sz(size());
     * d = t_max(sz.x(), sz.y());
     * return t_max(in_tol, rel_tol_threshhold * d);
     * \endcode
     * \param in_tol tolerance to restrict
     * \param rel_tol_threshhold threshhold for relative minimum tolerance
     */
    float
    restrict_tolerance(float in_tol, float rel_tol_threshhold) const
    {
      vec2 sz(size());
      float d(t_max(sz.x(), sz.y()));

      return t_max(in_tol, rel_tol_threshhold * d);
    }

  private:
    bool m_empty;
  };
/*! @} */
}

#endif

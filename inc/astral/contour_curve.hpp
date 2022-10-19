/*!
 * \file contour_curve.hpp
 * \brief file contour_curve.hpp
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

#ifndef ASTRAL_CONTOUR_CURVE_HPP
#define ASTRAL_CONTOUR_CURVE_HPP

#include <astral/util/c_array.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/bounding_box.hpp>
#include <astral/util/reference_counted.hpp>
#include <astral/util/transformation.hpp>

namespace astral
{
/*!\addtogroup Paths
 * @{
 */

  class ContourCurveSplit;

  /*!
   * \brief
   * Class to describe a curve of a contour
   */
  class ContourCurve
  {
  public:
    /*!
     * Enumeration to describe if a \ref ContourCurve
     * is a continuation of the curve that preceeds it
     * in a \ref ContourData.
     */
    enum continuation_t:uint8_t
      {
        /*!
         * Curve is not a contination of the previous curve;
         * in this case when a contour that uses the curve is
         * stroked, the join between the curve and its
         * predecessor is drawn with join style specified
         */
        not_continuation_curve,

        /*!
         * Curve is a contination of the previous curve
         * where the curves are to be drawn seemlessly
         * together. In this case when a contour that uses
         * the curve is stroked, the join between the curve
         * and its predecessor is always a rounded join.
         */
        continuation_curve,

        /*!
         * Curve is a contination of the previous curve
         * where the curves different direction is to be
         * cleary preserved. In this case when a contour
         * that uses the curve is stroked, the join between
         * the curve and its predecessor is always a bevel
         * join.
         */
        continuation_curve_cusp,
      };

    /*!
     * Enumeration to specify the curve type
     */
    enum type_t:uint8_t
      {
        /*!
         * Curve is a line segment connecting its end points
         */
        line_segment = 0,

        /*!
         * Curve is a quadratic bezier curve connecting its
         * end points
         */
        quadratic_bezier,

        /*!
         * Curve is a cubic bezier curve connecting its
         * end points
         */
        cubic_bezier,

        /*!
         * Curve is a conic curve, i.e it has a single control
         * point and a scalar weight on that control point.
         * A conic curve's parametric form is given by
         *
         *  p(t) = [StartPt, w * ControlPt, EndPt](t) / [1, w, 1](t)
         *
         * where [A, B, C](t) = A(1-t)^2 + 2Bt(1-t) + Ct^2
         *
         * Some of the properties of a conic curve are given by
         * http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.44.5740&rep=rep1&type=pdf
         * We summarize some imporant properties below:
         *  - if weight is 1, then the conic is a quadratic bezier curve (this is obvious)
         *  - if weight > 1, then the conic is a hyperbola
         *  - if weight < 1, then the conic is a part of an ellipse or circle
         *  - a conic can be used to represent -exactly- an arc with angle less
         *    than 180 degrees as follows. Let the start point be P, the radius of
         *    arc is R, the start angle is alpha and the ending angle is beta.
         *    If abs(beta - alpha) < 180 degrees. Let L be rotation about the origin
         *    by alpha and theta = beta - alpha. The the conic below gives the arc:
         *    - StartPt = P + R * L(1, 0)
         *    - ControlPt = P + R * L(1, tan(theta / 2))
         *    - EndPt = P + R * L(cos(beta), sin(beta))
         *    - weight = cos(theta / 2)
         */
        conic_curve,

        /*!
         * Curve is a conic curve that is also an arc of a circle.
         * Use the methods arc_center() and arc_angle() to compute
         * the properties of the arc.
         */
        conic_arc_curve,
      };

    /*!
     * Default ctor leaving ContourCurve with unitialized values.
     */
    ContourCurve(void)
    {}

    /*!
     * Ctor copying from another ContourCurve but with a potentially
     * different continuation type
     */
    ContourCurve(const ContourCurve &obj, enum continuation_t cont_tp):
      m_type(obj.m_type),
      m_number_control_pts(obj.m_number_control_pts),
      m_continuation(cont_tp),
      m_generation(obj.m_generation),
      m_start_pt(obj.m_start_pt),
      m_end_pt(obj.m_end_pt),
      m_control_pts(obj.m_control_pts)
    {}

    /*!
     * Ctor creating a line-segment
     * \param start start point of curve
     * \param end end point of curve
     * \param cont_tp curve continuation type; used to determine
     *                how the join between the curve and its predecessor
     *                is drawn when stroked
     */
    ContourCurve(const vec2 &start, const vec2 &end,
                 enum continuation_t cont_tp):
      m_type(line_segment),
      m_number_control_pts(0u),
      m_continuation(cont_tp),
      m_generation(0u),
      m_start_pt(start),
      m_end_pt(end)
    {
    }

    /*!
     * Ctor creating a quadratic bezier curve
     * \param start start point of curve
     * \param ct control point of curve
     * \param end end point of curve
     * \param cont_tp curve continuation type; used to determine
     *                how the join between the curve and its predecessor
     *                is drawn when stroked
     */
    ContourCurve(const vec2 &start, const vec2 &ct, const vec2 &end,
                 enum continuation_t cont_tp):
      m_type(quadratic_bezier),
      m_number_control_pts(1u),
      m_continuation(cont_tp),
      m_generation(0u),
      m_start_pt(start),
      m_end_pt(end),
      m_control_pts(ct, vec2(1.0f))
    {
    }

    /*!
     * Ctor creating a conic curve
     * \param start start point of curve
     * \param w conic weight
     * \param ct control point of curve
     * \param end end point of curve
     * \param cont_tp curve continuation type; used to determine
     *                how the join between the curve and its predecessor
     *                is drawn when stroked
     */
    ContourCurve(const vec2 &start, float w, const vec2 &ct, const vec2 &end,
                 enum continuation_t cont_tp):
      m_type(conic_curve),
      m_number_control_pts(1u),
      m_continuation(cont_tp),
      m_generation(0u),
      m_start_pt(start),
      m_end_pt(end),
      m_control_pts(ct, vec2(t_max(0.0f, w)))
    {
    }

    /*!
     * Ctor creating a conic curve that is an arc
     * \param start start point of curve
     * \param radians angle in radians for the arc. It is required that
     *                the absolute value of the angle is bounded away from
     *                PI; smaller angles are tighter fits and generally
     *                better.
     * \param end end point of curve
     * \param cont_tp curve continuation type; used to determine
     *                how the join between the curve and its predecessor
     *                is drawn when stroked
     */
    ContourCurve(const vec2 &start, float radians, const vec2 &end,
                 enum continuation_t cont_tp);

    /*!
     * Ctor creating a cubic bezier curve
     * \param start start point of curve
     * \param ct1 first control point of curve
     * \param ct2 second control point of curve
     * \param end end point of curve
     * \param cont_tp curve continuation type; used to determine
     *                how the join between the curve and its predecessor
     *                is drawn when stroked
     */
    ContourCurve(const vec2 &start, const vec2 &ct1, const vec2 &ct2, const vec2 &end,
                 enum continuation_t cont_tp):
      m_type(cubic_bezier),
      m_number_control_pts(2u),
      m_continuation(cont_tp),
      m_generation(0u),
      m_start_pt(start),
      m_end_pt(end),
      m_control_pts(ct1, ct2)
    {}

    /*!
     * Ctor creating a bezier curve
     * \param start start point of curve
     * \param ctl_pts control points of curve
     * \param end end point of curve
     * \param cont_tp curve continuation type; used to determine
     *                how the join between the curve and its predecessor
     *                is drawn when stroked
     */
    ContourCurve(const vec2 &start, c_array<const vec2> ctl_pts, const vec2 &end,
                 enum continuation_t cont_tp):
      m_type(static_cast<type_t>(ctl_pts.size())),
      m_number_control_pts(ctl_pts.size()),
      m_continuation(cont_tp),
      m_generation(0u),
      m_start_pt(start),
      m_end_pt(end)
    {
      ASTRALassert(ctl_pts.size() <= 2);
      m_control_pts[1].x() = 1.0f;
      std::copy(ctl_pts.begin(), ctl_pts.end(), m_control_pts.begin());
    }

    /*!
     * Ctor from a sequence of points
     * \param pts points that define the curve with pts.front() the
     *            starting point, pts.back() then ending points and
     *            points in between the control points.
     * \param cont_tp curve continuation type; used to determine
     *                how the join between the curve and its predecessor
     *                is drawn when stroked
     */
    ContourCurve(c_array<const vec2> pts, enum continuation_t cont_tp):
      m_continuation(cont_tp),
      m_generation(0u)
    {
      ASTRALassert(pts.size() >= 2u);
      ASTRALassert(pts.size() <= 4u);

      m_type = static_cast<type_t>(pts.size() - 2u);
      m_number_control_pts = pts.size() - 2u;
      m_start_pt = pts.front();
      m_end_pt = pts.back();
      m_control_pts[1].x() = 1.0f;
      std::copy(pts.begin() + 1, pts.end() - 1, m_control_pts.begin());
    }

    /*!
     * Ctor to create a ContourCurve of a passed curved transformed
     * by a transformation
     * \param curve from which to construct
     * \param transformation transformation to apply to curve
     */
    ContourCurve(const ContourCurve &curve,
                 const Transformation &transformation);

    /*!
     * Ctor to interpolate between two curves. The passed curves must
     * have the same number of control points.
     * \param start curve at t = 0
     * \param end curve at t = 1
     * \param t time of interpolate
     */
    ContourCurve(const ContourCurve &start, const ContourCurve &end, float t);

    /*!
     * gives the starting point of the curve
     */
    const vec2&
    start_pt(void) const
    {
      return m_start_pt;
    }

    /*!
     * Gives the ending point of the curve
     */
    const vec2&
    end_pt(void) const
    {
      return m_end_pt;
    }

    /*!
     * Mutate the starting point.
     */
    void
    start_pt(const vec2 &p)
    {
      m_start_pt = p;
    }

    /*!
     * Mutate the end point.
     */
    void
    end_pt(const vec2 &p)
    {
      m_end_pt = p;
    }

    /*!
     * Returns the number of control pts
     */
    unsigned int
    number_control_pts(void) const
    {
      return m_number_control_pts;
    }

    /*!
     * Returns the control points of the curve.
     * NOTE: the backing of the returned array
     * disappears when thie ContourCurve's dtor
     * is called. Never save this array past the
     * lifetime of the \ref ContourCurve object.
     */
    c_array<const vec2>
    control_pts(void) const
    {
      return c_array<const vec2>(m_control_pts).sub_array(0u, number_control_pts());
    }

    /*!
     * Return the named control point of the curve.
     */
    vec2
    control_pt(unsigned int I) const
    {
      ASTRALassert(I < number_control_pts());
      return m_control_pts[I];
    }

    /*!
     * Returns the conic weight of the curve if the curve's
     * type() is one of \ref conic_curve, \ref quadratic_bezier
     * or \ref conic_arc_curve
     */
    float
    conic_weight(void) const
    {
      ASTRALassert(type() == conic_curve || type() == quadratic_bezier || type() == conic_arc_curve);
      return m_control_pts[1].x();
    }

    /*!
     * Returns the arc-angle of the curve if type() is \ref conic_arc_curve
     */
    float
    arc_angle(void) const
    {
      ASTRALassert(type() == conic_arc_curve);
      return m_control_pts[1].y();
    }

    /*!
     * Returns the center of the arc if type() is \ref conic_arc_curve.
     * NOTE! this value is computed at each call, so cache the return
     * value for repeated use.
     */
    vec2
    arc_center(void) const;

    /*!
     * Returns the radius of the arc if type() is \ref conic_arc_curve.
     * NOTE! this value is computed at each call, so cache the return
     * value for repeated use.
     */
    float
    arc_radius(void) const;

    /*!
     * Returns the curve type
     */
    enum type_t
    type(void) const
    {
      return m_type;
    }

    /*!
     * Returns true if type() is \ref conic_curve
     * or \ref conic_arc_curve
     */
    bool
    is_conic(void) const
    {
      return m_type == conic_curve || m_type == conic_arc_curve;
    }

    /*!
     * Returns the curve continuation type
     */
    enum continuation_t
    continuation(void) const
    {
      return m_continuation;
    }

    /*!
     * Mutate the curve continuation type
     */
    void
    continuation(enum continuation_t v)
    {
      m_continuation = v;
    }

    /*!
     * Returns this \ref ContourCurve reversed
     * \param ct the continuation type to give to the returned curve
     */
    ContourCurve
    reverse_curve(enum continuation_t ct = not_continuation_curve) const;

    /*!
     * Returns a vector that is the same
     * direction as the tangent to the curve
     * at t = 0. The return value's length
     * is not necessarily one.
     */
    vec2
    start_tangent(void) const;

    /*!
     * Returns a vector that is the same
     * direction as the tangent to the curve
     * at t = 1. The return value's length
     * is not necessarily one.
     */
    vec2
    end_tangent(void) const;

    /*!
     * Evaluate the curve at a parameteric time t.
     */
    vec2
    eval_at(float t) const;

    /*!
     * Returns true if the curve is flat, i.e. its control
     * points (if it has any) are close to the line connecting
     * the start and end points.
     * \param rel_tol regard the curve as flat if both conrol
     *                points are no more than this distance times
     *                the length of the line segment between
     *                start_pt() and end_pt().
     */
    bool
    is_flat(float rel_tol = 1e-4) const;

    /*!
     * Returns the distance of the furthest control point from
     * the line segment that connects the start and end points.
     * If the curve is a linesegment, returns 0.0.
     */
    float
    flatness(void) const;

    /*!
     * Returns true if the curve should be regarded as degenerate,
     * which in this context means its start and end points are
     * nearly the same.
     * \param rel_tol the relative tolerance to pass to
     *                vec2::approximately_equal() when comparing
     *                start_pt() and end_pt().
     */
    bool
    is_degenerate(float rel_tol = 1e-4) const
    {
      return m_start_pt.approximately_equal(m_end_pt, rel_tol);
    }

    /*!
     * Returns true if the namd control point is essentially
     * degenerate by being nearly the same as the start of end
     * point of the curve
     * \param I which control point with 0 <= I < number_control_pts()
     * \param rel_tol the relative tolerance to pass to
     *                vec2::approximately_equal() when comparing
     *                the named control point against start_pt()
     *                and end_pt().
     */
    bool
    control_pt_is_degenerate(unsigned int I, float rel_tol = 1e-4) const
    {
      ASTRALassert(I < number_control_pts());
      return m_start_pt.approximately_equal(m_control_pts[I], rel_tol)
        || m_end_pt.approximately_equal(m_control_pts[I], rel_tol);
    }

    /*!
     * Computes and returns a tight bounding box for the curve.
     * NOTE: this value is computed each time the method is
     * called, so if a caller needs this value often, the caller
     * should cache the value.
     */
    BoundingBox<float>
    tight_bounding_box(void) const;

    /*!
     * Computes and returns a bounding box that containt the
     * start_pt(), end_pt() and and control_pt() values.
     */
    BoundingBox<float>
    control_point_bounding_box(void) const;

    /*!
     * Computes the maximum number of cirtical points
     * for the coordinate functions; compute solely from
     * type() a follows:
     *   - \ref line_segment --> 0
     *   - \ref quadratic_bezier --> 1
     *   - \ref cubic_bezier --> 2
     *   - \ref conic_curve --> 2
     *   - \ref conic_arc_curve --> 2
     */
    unsigned int
    max_number_critical_points(void) const
    {
      return t_min(2u, static_cast<unsigned int>(type()));
    }

    /*!
     * Computes the points where the derivative of the
     * x-coordinate is zero and the points where the
     * y-derivative is zero. The critical points are
     * are NOT clamped to be in the range [0, 1].
     * \param out_x_crits location to which to write where
     *                    the derivative of the x-coordinate
     *                    is zero; the size will be modified;
     *                    the input array must be atlast size
     *                    as max_number_critical_points().
     * \param out_y_crits location to which to write where
     *                    the derivative of the y-coordinate
     *                    is zero; the size will be modified;
     *                    the input array must be atlast size
     *                    as max_number_critical_points().
     */
    void
    compute_critical_points(c_array<float> *out_x_crits,
                            c_array<float> *out_y_crits) const;

    /*!
     * Computes and returns a tight bounding box for the curve.
     * In addition, computes the  points where the derivative
     * of the x-coordinate is zero and the points where the
     * y-derivative is zero. The critical points are NOT clamped
     * to be in the range [0, 1].
     * \param out_x_crits location to which to write where
     *                    the derivative of the x-coordinate
     *                    is zero; the size will be modified;
     *                    the input array must be atlast size
     *                    as max_number_critical_points().
     * \param out_y_crits location to which to write where
     *                    the derivative of the y-coordinate
     *                    is zero; the size will be modified;
     *                    the input array must be atlast size
     *                    as max_number_critical_points().
     */
    BoundingBox<float>
    tight_bounding_box(c_array<float> *out_x_crits,
                       c_array<float> *out_y_crits) const;

    /*!
     * Specifies the curve's generation. A curve made diretly
     * via one of the constructor of \ref ContourCurve has
     * generation() value 0. A curve made from splitting a \ref
     * ContourCurve via \ref ContourCurveSplit has generation()
     * one plus the value of generation() of the curve that was
     * split.
     */
    unsigned int
    generation(void) const
    {
      return m_generation;
    }

    /*!
     * Set the value returned by generation(void) const
     */
    void
    generation(unsigned int v)
    {
      m_generation = v;
    }

    /*!
     * Reset the generation of the curve to 0.
     */
    void
    reset_generation(void)
    {
      m_generation = 0;
    }

    /*!
     * Flatten this curve to a line segment.
     */
    void
    flatten(void)
    {
      m_type = line_segment;
      m_number_control_pts = 0;
    }

  private:
    friend class ContourCurveSplit;

    /* each of these is 8-bits wide */
    enum type_t m_type;
    uint8_t m_number_control_pts;
    enum continuation_t m_continuation;
    uint8_t m_generation;

    vec2 m_start_pt, m_end_pt;
    vecN<vec2, 2> m_control_pts;
  };

  /*!
   * \brief
   * A ContourCurveSplit represents the two \ref ContourCurve
   * made from splitting a \ref ContourCurve
   */
  class ContourCurveSplit
  {
  public:
    /*!
     * \brief class to specify a coordinate
     */
    class Coordinate
    {
    public:
      /*!
       * Ctor.
       * \param v value with which to initialize \ref m_v
       */
      explicit
      Coordinate(int v):
        m_v(v)
      {}

      /*!
       * The coordinate, a value of 0 means the x-coordinate
       * and a value of 1 means the y-coordinate.
       */
      int m_v;
    };

    /*!
     * Split the passed \ref ContourCurve at t = 0.5
     * \param increment_generation if true the resulting curves have that
     *                             ContourCurve::generation() is one larger
     *                             that that of input astral::ContourCurve
     * \param curve curve to split
     * \param cont continuation type to give to after_t()
     */
    ContourCurveSplit(bool increment_generation, const ContourCurve &curve,
                      enum ContourCurve::continuation_t cont = ContourCurve::continuation_curve);

    /*!
     * Split the passed \ref ContourCurve at a specified t
     * \param increment_generation if true the resulting curves have that
     *                             ContourCurve::generation() is one larger
     *                             that that of input astral::ContourCurve
     * \param curve curve to split
     * \param t time of split in the domain of curve
     * \param cont continuation type to give to after_t()
     */
    ContourCurveSplit(bool increment_generation, const ContourCurve &curve, float t,
                      enum ContourCurve::continuation_t cont = ContourCurve::continuation_curve);

    /*!
     * Force the named coordinate where the curves
     * before_t() and after_t() meet. The use case
     * is for when ContourCurveSplit is used in
     * clipping and one needs to force the value
     * at the splitting coordinate
     */
    void
    force_coordinate(Coordinate coordinate, float value);

    /*!
     * Returns the curve before the split point
     */
    const ContourCurve&
    before_t(void) const
    {
      return m_before_t;
    }

    /*!
     * Returns the curve after the split point
     */
    const ContourCurve&
    after_t(void) const
    {
      return m_after_t;
    }

  private:
    ContourCurve m_before_t, m_after_t;
  };

/*! @} */
}

#endif

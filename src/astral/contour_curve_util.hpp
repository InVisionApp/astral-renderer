/*!
 * \file contour_curve_util.hpp
 * \brief file contour_curve_util.hpp
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

#ifndef ASTRAL_CONTOUR_CURVE_UTIL_HPP
#define ASTRAL_CONTOUR_CURVE_UTIL_HPP

#include <astral/util/vecN.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/polynomial.hpp>
#include <astral/util/ostream_utility.hpp>
#include <astral/contour_curve.hpp>

namespace astral
{
  namespace detail
  {
    typedef vecN<vec2, 4> CubicBezierCurve;
    typedef vecN<vec2, 3> QuadraticBezierCurve;
    typedef vecN<vec2, 2> LinearBezierCurve;

    class ConicCurve
    {
    public:
      ContourCurve
      make_contour_curve(enum ContourCurve::continuation_t tp) const
      {
        return ContourCurve(m_pts[0], m_weight, m_pts[1], m_pts[2], tp);
      }

      QuadraticBezierCurve m_pts;
      float m_weight;
    };

    /*!
     * Compute the L1-distance between a point an curve.
     * \param pt point
     * \param curve curve; the curve cannot be a cubic bezier
     *              curve (but all other curves types are fine).
     * \param w if non-null, increment *w by the impact the
     *          curve has on the winding number at pt.
     * \param t if non-null, location to which to write the domain
     *          argument of the closest point.
     */
    float
    compute_l1_distace_to_curve(const vec2 &pt, const ContourCurve &curve, int *w, float *t);

    /*!
     * Class to encpasulate the approximation of a quadratic
     * curve by a bi-arc as described in the paper
     * "Approximation of quadratic Bezier curves by arc splines"
     * by D.J. Walton and D.S. Meek
     */
    template<typename T>
    class WaltonMeekBiArc
    {
    public:
      /*!
       * Ctor passed the input quadratic bezier curve.
       * \param theta_small if 1 - cos(theta) is less than this magnitude,
       *                    then regard the input as a line segment. Here
       *                    theta is the angle between the tangent vectors
       *                    at the start and end of the curve.
       * \param length_small if the distance between any two points
       *                     of Q is no more than this value,
       *                     then regard the input as a line segment.
       */
      WaltonMeekBiArc(const QuadraticBezierCurve &Q,
                      float theta_small, float length_small);

      /*
       * m_cL is the center of the first (i.e. left) circle of the biarc
       * m_rL is the radius of the first (i.e. left) circle of the biarc
       * m_cR is the center of the second (i.e. right) circle of the biarc
       * m_rR is the radius of the second (i.e. right) circle of the biarc
       * m_G is the meeting point of the two circles
       */
      vecN<T, 2> m_G, m_cL, m_cR;
      T m_rL, m_rR;
      T m_error;

      bool m_flat;

    private:
      void
      realize_as_flat(const vecN<T, 2> &B0,
                      const vecN<T, 2> &B1,
                      const vecN<T, 2> &B2,
                      const Polynomial<vecN<T, 2>, 2> &Q);
    };

    /*!
     * Returns the error between a given quadratic curve and the
     * bi-arc made with WaltonMeekBiArc
     * \param theta_small if 1 - cos(theta) is less than this magnitude,
     *                    then regard the input as a line segment. Here
     *                    theta is the angle between the tangent vectors
     *                    at the start and end of the curve.
     * \param rel_length_small the relative length, i.e. the argument
     *                         length_small to the ctor of WaltonMeekBiArc
     *                         is this value times the largest value of
     *                         and of the points of Q.
     */
    template<typename T>
    float
    compute_biarc_error_rel_length(const QuadraticBezierCurve &Q,
                                   float theta_small = 1e-3,
                                   float rel_length_small = 1e-5)
    {
      float M;

      M = t_max(t_abs(Q[0].x()), t_abs(Q[0].y()));
      M = t_max(M, t_max(t_abs(Q[1].x()), t_abs(Q[1].y())));
      M = t_max(M, t_max(t_abs(Q[2].x()), t_abs(Q[2].y())));

      WaltonMeekBiArc<T> W(Q, theta_small, rel_length_small * M);

      return static_cast<float>(W.m_error);
    }

    /*!
     * Computes the distance between a polynomial curve C
     * on [0, 1] and the line segment connecting C(0) to
     * a named end point
     * \param end_pt end point of line segment
     * \param curve parametric curve
     * \param curve_derivative derivative of parametric curve
     * \tparam T type at which to perform the error computation
     * \tparam D degree of the curve, must be no more than 5.
     */
    template<typename T, size_t D>
    T
    compute_distance_to_line(const vecN<T, 2> &end_pt,
                             const Polynomial<vecN<T, 2>, D> &curve,
                             const Polynomial<vecN<T, 2>, D - 1> &curve_derivative);

    /*!
     * Computes the between a conic and a quadratic bezier curve
     * which has the same start, control and end point as the
     * conic.
     */
    template<typename T>
    T
    compute_error_to_conic(const vecN<T, 2> &start_pt,
                           const vecN<T, 2> &control_pt,
                           const vecN<T, 2> &end_pt,
                           T conic_weight);

    /*!
     * Compute the approximation of a ContourCurve by a single quadratic curve
     * \param p cubic bezier curve, p.type() must not be ContourCurve::line_segment
     * \param out_quad location to which to write the approximation of p
     *                 by a quadratic bezier curve
     * \returns returns an upper bound for the error between out_quad and p
     */
    float
    compute_quadratic_appoximation(const ContourCurve &p, QuadraticBezierCurve *out_quad);

    /*!
     * Compute the approximation of a curve by the line segment
     * connecting the end points of the curve. If the start and
     * end points of the curve are exactly the same, returns the
     * distance to the furthest control point.
     * \param p curve
     */
    float
    error_to_line_appoximation(const ContourCurve &p);

    /*!
     * Realize the rational bezier curve
     *
     *     [q0, q1, q2](t)
     *    -----------------
     *     [w0, w1, w2](t)
     *
     * as a conic curve
     *
     *    [a0, w * a1, a2](t)
     *   --------------------
     *       [1, w, 1](t)
     *
     */
    ConicCurve
    create_conic(const vecN<vec2, 3> q, const vecN<float, 3> w);

    /*!
     * Split a cubic bezier curve at t = 0.5
     * \param p cubic bezier curve, p.type() must be cubic_bezier
     */
    vecN<CubicBezierCurve, 2>
    split_cubic(const ContourCurve &p);

    /*!
     * Split a cubic bezier curve at t
     * \param p cubic bezier curve, p.type() must be cubic_bezier
     */
    vecN<CubicBezierCurve, 2>
    split_cubic(const ContourCurve &p, float t);

    /*!
     * Split a quadratic bezier curve at t = 0.5
     * \param p quadratic bezier curve, p.type() must be quadratic_bezier
     */
    vecN<QuadraticBezierCurve, 2>
    split_quadratic(const ContourCurve &p);

    /*!
     * Split a quadratic bezier curve at t
     * \param p quadratic bezier curve, p.type() must be quadratic_bezier
     */
    vecN<QuadraticBezierCurve, 2>
    split_quadratic(const ContourCurve &p, float t);

    /*!
     * Split a linear bezier curve at t = 0.5
     * \param p linear bezier curve, p.type() must be line_bezier
     */
    vecN<LinearBezierCurve, 2>
    split_linear(const ContourCurve &p);

    /*!
     * Split a linear bezier curve at t
     * \param p linear bezier curve, p.type() must be line_bezier
     */
    vecN<LinearBezierCurve, 2>
    split_linear(const ContourCurve &p, float t);

    /*!
     * Split a conic curve at t = 0.5
     * \param p conic curve, p.type() must be conic_curve or conic_arc_curve
     */
    vecN<ConicCurve, 2>
    split_conic(const ContourCurve &p);

    /*!
     * Split a conic curve at t
     * \param p conic curve, p.type() must be conic_curve or conic_arc_curve
     */
    vecN<ConicCurve, 2>
    split_conic(const ContourCurve &p, float t);
  }
}

#include "contour_curve_util.tcc"

#endif

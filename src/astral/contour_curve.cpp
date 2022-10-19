/*!
 * \file contour_curve.cpp
 * \brief file contour_curve.cpp
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

#include <astral/contour_curve.hpp>
#include <astral/util/polynomial.hpp>
#include <astral/util/ostream_utility.hpp>
#include "contour_curve_util.hpp"

namespace
{
  template<size_t Degree>
  void
  compute_critical_points_impl(const astral::Polynomial<astral::vec2, Degree> &p,
                               astral::c_array<float> *out_x_crits,
                               astral::c_array<float> *out_y_crits)
  {
    astral::vecN<astral::c_array<float>*, 2> out_crits(out_x_crits, out_y_crits);
    auto dp(astral::convert(p.derivative()));

    /* TODO: this can be made more efficient */
    for (unsigned int coord = 0; coord < 2; ++coord)
      {
        unsigned int num_solutions;

        ASTRALassert(out_crits[coord]->size() >= Degree - 1u);

        num_solutions = astral::solve_polynomial(dp[coord], *out_crits[coord]);
        *out_crits[coord] = out_crits[coord]->sub_array(0, num_solutions);
      }
  }

  void
  compute_critical_points_impl(const astral::Polynomial<astral::vec2, 2> &p,
                               const astral::Polynomial<float, 2> &w,
                               astral::c_array<float> *out_x_crits,
                               astral::c_array<float> *out_y_crits)
  {
    astral::vecN<astral::c_array<float>*, 2> out_crits(out_x_crits, out_y_crits);
    auto pp(astral::convert(p));

    for (unsigned int coord = 0; coord < 2; ++coord)
      {
        unsigned int num_solutions;
        astral::Polynomial<float, 3> num;
        astral::Polynomial<float, 2> f;

        num = w * pp[coord].derivative() - w.derivative() * pp[coord];

        /* because of the nature of the above, we know that
         * the leading coefficient of num is zero in exact
         * arithmatic.
         */
        f.coeff(0) = num.coeff(0);
        f.coeff(1) = num.coeff(1);
        f.coeff(2) = num.coeff(2);

        ASTRALassert(out_crits[coord]->size() >= 2u);

        num_solutions = astral::solve_polynomial(f, *out_crits[coord]);
        *out_crits[coord] = out_crits[coord]->sub_array(0, num_solutions);
      }
  }
}

///////////////////////////////////
// astral::ContourCurve methods
astral::ContourCurve::
ContourCurve(const ContourCurve &curve,
             const Transformation &transformation):
  m_type(curve.m_type),
  m_number_control_pts(curve.m_number_control_pts),
  m_continuation(curve.m_continuation),
  m_generation(curve.m_generation),
  m_start_pt(transformation.apply_to_point(curve.m_start_pt)),
  m_end_pt(transformation.apply_to_point(curve.m_end_pt))
{
  /* conic_arc_curve becomes conic_curve if the transformation's
   * matrix is not the identity
   */
  if (m_type == conic_arc_curve && transformation.m_matrix != float2x2())
    {
      m_type = conic_curve;
    }

  if (m_number_control_pts >= 1u)
    {
      m_control_pts[0] = transformation.apply_to_point(curve.m_control_pts[0]);
      if (m_number_control_pts == 2u)
        {
          m_control_pts[1] = transformation.apply_to_point(curve.m_control_pts[1]);
        }
      else
        {
          m_control_pts[1] = curve.m_control_pts[1];
        }
    }
}

astral::ContourCurve::
ContourCurve(const ContourCurve &start, const ContourCurve &end, float t):
  m_number_control_pts(start.m_number_control_pts),
  m_continuation(start.m_continuation),
  m_generation(start.m_generation),
  m_start_pt(mix(start.m_start_pt, end.m_start_pt, t)),
  m_end_pt(mix(start.m_end_pt, end.m_end_pt, t))
{
  enum type_t start_tp, end_tp;

  ASTRALassert(start.m_number_control_pts == end.m_number_control_pts);

  /* interpolating between conics kill conic_arc_curve types */
  start_tp = (start.m_type == conic_arc_curve) ? conic_curve : start.m_type;
  end_tp = (end.m_type == conic_arc_curve) ? conic_curve : end.m_type;
  m_type = (start_tp == conic_curve || end_tp == conic_curve) ? conic_curve : start_tp;

  if (m_type != line_segment)
    {
      m_control_pts[0] = mix(start.m_control_pts[0], end.m_control_pts[0], t);

      /* even with just 1 control point, to interpolate the conic weight */
      m_control_pts[1] = mix(start.m_control_pts[1], end.m_control_pts[1], t);
    }
}

astral::ContourCurve::
ContourCurve(const vec2 &start, float angle, const vec2 &end,
             enum continuation_t cont_tp):
  m_type(conic_arc_curve),
  m_number_control_pts(1u),
  m_continuation(cont_tp),
  m_generation(0u),
  m_start_pt(start),
  m_end_pt(end)
{
  float dir_angle(angle > 0.0f ? 1.0f : -1.0f);
  float abs_angle(t_abs(angle));
  float s, c;
  vec2 m(0.5f * (m_start_pt + m_end_pt));
  vec2 v(m_end_pt - m_start_pt);
  vec2 n(-v.y(), v.x());

  ASTRALassert(abs_angle < ASTRAL_PI);

  /* Let
   *  C = center of circle of the arc
   *  S = start point
   *  E = end point
   *  m = (S + E) / 2;
   *  v = E - S
   *  n = (-v.y, v.x)
   *  Q = control point of the conic weight
   *
   * then
   *   C = m + A * n for some A
   *   Q = m + B * n for some B with B the opposite sign of A
   *
   * The triangle [S, E, C] has abs_angle / 2 at the point C.
   * The triangle [S, E, Q] has abs_angle / 2 at the points S and E
   *
   * Thus,
   *
   *   tan(abs_angle / 2) = |B| * ||n|| / ||m - E||
   *                      = |B| * ||E - S|| / (0.5 * ||E - S||)
   *                      = 2 * |B|
   */

  s = t_sin(0.5f * abs_angle);
  c = t_cos(0.5f * abs_angle);

  m_control_pts[0] = m - (dir_angle * 0.5f * s / c) * n;

  m_control_pts[1].x() = c;
  m_control_pts[1].y() = angle;
}

astral::vec2
astral::ContourCurve::
arc_center(void) const
{
  vec2 m(0.5f * (m_start_pt + m_end_pt));
  vec2 v(m_end_pt - m_start_pt);
  vec2 n(-v.y(), v.x());
  float A, B;

  ASTRALassert(type() == conic_arc_curve);
  B = dot(m_control_pts[0] - m, n) / dot(n, n);
  A = -0.25f / B;
  return m + A * n;
}

float
astral::ContourCurve::
arc_radius(void) const
{
  float lambda, omega;
  vec2 delta(m_end_pt - m_start_pt);

  lambda = t_abs(0.5f * t_tan(0.5f * arc_angle()));
  omega = (1.0f + 4.0f * lambda * lambda) / (8.0f * lambda);

  return omega * delta.magnitude();
}

astral::ContourCurve
astral::ContourCurve::
reverse_curve(enum continuation_t ct) const
{
  ContourCurve R;

  R.m_type = m_type;
  R.m_number_control_pts = m_number_control_pts;
  R.m_continuation = ct;
  R.m_start_pt = m_end_pt;
  R.m_end_pt = m_start_pt;
  if (m_type == cubic_bezier)
    {
      R.m_control_pts[0] = m_control_pts[1];
      R.m_control_pts[1] = m_control_pts[0];
    }
  else
    {
      R.m_control_pts = m_control_pts;
      R.m_control_pts[1].y() = -m_control_pts[1].y();
    }

  return R;
}

bool
astral::ContourCurve::
is_flat(float rel_tol) const
{
  switch (m_number_control_pts)
    {
    default:
    case 0:
      return true;

    case 1:
      {
        float numerator;
        vec2 E_S, S_C;

        /* The distance between a point C and the line L through
         * the points S and E is given by
         *
         *                | (E.x - S.x) * (S.y - C.y) - (E.y - S.y) * (S.x - C.x) |
         *    d(C, L) = -----------------------------------------------------------
         *                                    ||E - S||
         *
         * we will say it is flat if d(P, L) < ||E - S|| * rel_tol
         *
         * TODO: take into account the conic weight.
         */
        E_S = m_end_pt - m_start_pt;
        S_C = m_start_pt - m_control_pts[0];
        numerator = E_S.x() * S_C.y() - E_S.y() * S_C.x();

        return numerator * numerator < rel_tol * rel_tol * dot(E_S, E_S);
      }

    case 2:
      {
        vec2 E_S, S_C1, S_C2;
        float n1, n2, nn;

        E_S = m_end_pt - m_start_pt;
        S_C1 = m_start_pt - m_control_pts[0];
        S_C2 = m_start_pt - m_control_pts[1];

        n1 = E_S.x() * S_C1.y() - E_S.y() * S_C1.x();
        n2 = E_S.x() * S_C2.y() - E_S.y() * S_C2.x();

        nn = astral::t_max(n1 * n1, n2 * n2);

        return nn < rel_tol * rel_tol * dot(E_S, E_S);
      }
    }
}

float
astral::ContourCurve::
flatness(void) const
{
  if (m_number_control_pts == 0)
    {
      return 0.0f;
    }

  vec2 E_S;
  float E_S_magnitude;

  E_S = m_end_pt - m_start_pt;
  E_S_magnitude = E_S.magnitude();
  ASTRALassert(m_number_control_pts < 3u);

  if (E_S_magnitude != 0.0f)
    {
      switch (m_number_control_pts)
        {
        default:
          ASTRALfailure("Not possible to reach here");
          return 0.0f;

        case 1:
          {
            float numerator;
            vec2 S_C;

            S_C = m_start_pt - m_control_pts[0];
            numerator = E_S.x() * S_C.y() - E_S.y() * S_C.x();

            return t_abs(numerator) / E_S_magnitude;
          }
          break;

        case 2:
          {
            vec2 S_C1, S_C2;
            float n1, n2, nn;

            S_C1 = m_start_pt - m_control_pts[0];
            S_C2 = m_start_pt - m_control_pts[1];

            n1 = E_S.x() * S_C1.y() - E_S.y() * S_C1.x();
            n2 = E_S.x() * S_C2.y() - E_S.y() * S_C2.x();

            nn = astral::t_max(t_abs(n1), t_abs(n2));
            return nn / E_S_magnitude;
          }
          break;
        }
    }
  else
    {
      float return_value_sq(0.0f);
      for (unsigned int i = 0; i < m_number_control_pts; ++i)
        {
          vec2 v;

          v = m_start_pt - m_control_pts[i];
          return_value_sq = t_max(return_value_sq, dot(v, v));
        }

      return t_sqrt(return_value_sq);
    }
}

void
astral::ContourCurve::
compute_critical_points(c_array<float> *out_x_crits,
                        c_array<float> *out_y_crits) const

{
  switch (type())
    {
    case line_segment:
      *out_x_crits = c_array<float>();
      *out_y_crits = c_array<float>();
      break;

    case quadratic_bezier:
      {
        Polynomial<vec2, 2> p;

        p.coeff(0) = start_pt();
        p.coeff(1) = 2.0f * (control_pt(0) - start_pt());
        p.coeff(2) = start_pt() - 2.0f * control_pt(0) + end_pt();

        compute_critical_points_impl(p, out_x_crits, out_y_crits);
      }
      break;

    case cubic_bezier:
      {
        Polynomial<vec2, 3> p;

        p.coeff(0) = start_pt();
        p.coeff(1) = 3.0f * (control_pt(0) - start_pt());
        p.coeff(2) = 3.0f * (start_pt() - 2.0f * control_pt(0) + control_pt(1));
        p.coeff(3) = -start_pt() + end_pt() + 3.0f * (control_pt(0) - control_pt(1));

        compute_critical_points_impl(p, out_x_crits, out_y_crits);
      }
      break;

    default:
      {
        Polynomial<vec2, 2> p;
        Polynomial<float, 2> w;

        p.coeff(0) = start_pt();
        p.coeff(1) = 2.0f * (conic_weight() * control_pt(0) - start_pt());
        p.coeff(2) = start_pt() - (2.0f * conic_weight()) * control_pt(0) + end_pt();

        w.coeff(0) = 1.0f;
        w.coeff(1) = 2.0f * (conic_weight() - 1.0f);
        w.coeff(2) = 2.0f * (1.0f - conic_weight());

        compute_critical_points_impl(p, w, out_x_crits, out_y_crits);
      }
      break;
    }
}

astral::BoundingBox<float>
astral::ContourCurve::
tight_bounding_box(c_array<float> *out_x_crits,
                   c_array<float> *out_y_crits) const
{
  BoundingBox<float> return_value;

  compute_critical_points(out_x_crits, out_y_crits);
  for (float t : *out_x_crits)
    {
      t = t_min(1.0f, t_max(0.0f, t));
      return_value.union_point(eval_at(t));
    }
  for (float t : *out_y_crits)
    {
      t = t_min(1.0f, t_max(0.0f, t));
      return_value.union_point(eval_at(t));
    }

  return_value.union_point(start_pt());
  return_value.union_point(end_pt());

  return return_value;
}

astral::BoundingBox<float>
astral::ContourCurve::
tight_bounding_box(void) const
{
  vecN<float, 3> x, y;
  c_array<float> xx(x), yy(y);

  return tight_bounding_box(&xx, &yy);
}

astral::BoundingBox<float>
astral::ContourCurve::
control_point_bounding_box(void) const
{
  BoundingBox<float> R;

  R.union_point(start_pt());
  R.union_point(end_pt());
  for (int i = 0, endi = number_control_pts(); i < endi; ++i)
    {
      R.union_point(control_pt(i));
    }
  return R;
}

astral::vec2
astral::ContourCurve::
start_tangent(void) const
{
  vec2 return_value;
  if (m_number_control_pts == 0)
    {
      return_value = m_end_pt - m_start_pt;
    }
  else
    {
      const float tiny = 1e-6;
      float m;

      return_value = m_control_pts[0] - m_start_pt;

      m = t_max(m_start_pt.L1norm(), m_control_pts[0].L1norm());
      if (return_value.L1norm() < tiny * m)
        {
          if (type() == cubic_bezier)
            {
              /* A cubic Bezier curve is:
               *
               *   p(t) = A + Bt + Ct^2 + Dt^3
               *
               * where
               *
               *   A = p0
               *   B = -3p0 + 3p1
               *   C = 3p0 - 6p1 + 3p2
               *   D = -p0 + 3p1 - 3p2 + p3
               *
               * The tangent is given by p1 - p0 which is almost zero,
               * i.e. B = 0. Reparameterizing p(t) with s = t^2 we get
               *
               *   p(s) = A + Cs + Ds^(3/2)
               *
               * So from the parameterization of p via s, one gets a tangent
               * vector of C. Using that p0 = p1,  we have
               *
               *  C = -3p0 + 3p2
               *
               * Dropping a factor of 3 we are left with just p2 - p0
               */
              return_value = m_control_pts[1] - m_start_pt;
              m = t_max(m_start_pt.L1norm(), m_control_pts[1].L1norm());
              if (return_value.L1norm() < tiny * m)
                {
                  /* Oh dear, C is also zero, which means that p0 = -p2 and the
                   * curve is actually
                   *
                   *  p(t) = A + Dt^3
                   *
                   * using that p1 = p0 and p2 = -p1 gives
                   *
                   *  D = -p0 - 3p0 + 3p0 + p3
                   *    = -p0 + -p3
                   *
                   * i.e. the curve is just a funny way to parameterize a line
                   * segment
                   */
                  return_value = m_end_pt - m_start_pt;
                }
            }
          else
            {
              /* A the image of teh curves lies within the convex hull
               * of {p0, p1, p2} and we have that p0 = p1, which implies
               * that the curve is just a funny way to paramterize a line
               * segment
               */
              return_value = m_end_pt - m_start_pt;
            }
        }
    }
  return return_value;
}

astral::vec2
astral::ContourCurve::
end_tangent(void) const
{
  /* TODO: maybe push the code in start_tangent()
   * to a common routine instead of having the copy
   * overhead of creating a the curve reversed.
   */
  return -reverse_curve().start_tangent();
}

astral::vec2
astral::ContourCurve::
eval_at(float t) const
{
  float s(1.0f - t);

  switch (type())
    {
    case line_segment:
      return s * start_pt() + t * end_pt();

    case quadratic_bezier:
      {
        vec2 p01, p12, p02;

        p01 = mix(start_pt(), control_pt(0), t);
        p12 = mix(control_pt(0), end_pt(), t);
        p02 = mix(p01, p12, t);

        return p02;
      }

    case conic_curve:
    case conic_arc_curve:
      {
        vec2 p01, p12, p02;
        float w01, w12, w02;

        p01 = mix(start_pt(), conic_weight() * control_pt(0), t);
        w01 = mix(1.0f, conic_weight(), t);

        p12 = mix(conic_weight() * control_pt(0), end_pt(), t);
        w12 = mix(conic_weight(), 1.0f, t);

        p02 = mix(p01, p12, t);
        w02 = mix(w01, w12, t);

        return p02 / w02;
      }

    case cubic_bezier:
      {
        vec2 p01, p12, p23, p02, p13, p03;

        p01 = mix(start_pt(), control_pt(0), t);
        p12 = mix(control_pt(0), control_pt(1), t);
        p23 = mix(control_pt(1), end_pt(), t);

        p02 = mix(p01, p12, t);
        p13 = mix(p12, p23, t);

        p03 = mix(p02, p13, t);

        return p03;
      }
    default:
      ASTRALassert(!"Bad ContourCurve::type() value");
      return vec2(0.0f, 0.0f);
    }
}

namespace std
{
  ostream&
  operator<<(ostream &str, const astral::ContourCurve &obj)
  {
    str << "[" << obj.start_pt() << ", ";
    for (unsigned int i = 0; i < obj.number_control_pts(); ++i)
      {
        str << obj.control_pt(i) << ", ";
      }
    str << obj.end_pt();
    if (obj.is_conic())
      {
        str << ", (w = " << obj.conic_weight() << ")";
      }
    str << "]";

    return str;
  }
}

/////////////////////////////////////
// astral::ContourCurveSplit methods
astral::ContourCurveSplit::
ContourCurveSplit(bool increment_generation, const ContourCurve &curve,
                  enum ContourCurve::continuation_t cont)
{
  switch (curve.type())
    {
    case ContourCurve::line_segment:
      {
        vecN<detail::LinearBezierCurve, 2> split;
        split = detail::split_linear(curve);

        m_before_t = ContourCurve(split[0], curve.continuation());
        m_after_t = ContourCurve(split[1], cont);
      }
      break;

    case ContourCurve::quadratic_bezier:
      {
        vecN<detail::QuadraticBezierCurve, 2> split;
        split = detail::split_quadratic(curve);

        m_before_t = ContourCurve(split[0], curve.continuation());
        m_after_t = ContourCurve(split[1], cont);

        ASTRALassert(m_before_t.conic_weight() == 1.0f);
        ASTRALassert(m_after_t.conic_weight() == 1.0f);
      }
      break;

    case ContourCurve::cubic_bezier:
      {
        vecN<detail::CubicBezierCurve, 2> split;
        split = detail::split_cubic(curve);

        m_before_t = ContourCurve(split[0], curve.continuation());
        m_after_t = ContourCurve(split[1], cont);
      }
      break;

    case ContourCurve::conic_arc_curve:
    case ContourCurve::conic_curve:
      {
        vecN<detail::ConicCurve, 2> split;

        split = detail::split_conic(curve);
        m_before_t = split[0].make_contour_curve(curve.continuation());
        m_after_t = split[1].make_contour_curve(cont);

        if (curve.type() == ContourCurve::conic_arc_curve)
          {
            float angle(curve.arc_angle());

            m_before_t.m_type = ContourCurve::conic_arc_curve;
            m_before_t.m_control_pts[1].y() = 0.5f * angle;

            m_after_t.m_type = ContourCurve::conic_arc_curve;
            m_after_t.m_control_pts[1].y() = 0.5f * angle;
          }
      }
      break;

    default:
      ASTRALassert(!"Invalid Curve Type!");
    }

  unsigned int incr;

  incr = (increment_generation) ? 1u : 0u;
  m_before_t.m_generation = curve.m_generation + incr;
  m_after_t.m_generation = curve.m_generation + incr;
}

astral::ContourCurveSplit::
ContourCurveSplit(bool increment_generation, const ContourCurve &curve, float t,
                  enum ContourCurve::continuation_t cont)
{
  switch (curve.type())
    {
    case ContourCurve::line_segment:
      {
        vecN<detail::LinearBezierCurve, 2> split;
        split = detail::split_linear(curve, t);

        m_before_t = ContourCurve(split[0], curve.continuation());
        m_after_t = ContourCurve(split[1], cont);
      }
      break;

    case ContourCurve::quadratic_bezier:
      {
        vecN<detail::QuadraticBezierCurve, 2> split;
        split = detail::split_quadratic(curve, t);

        m_before_t = ContourCurve(split[0], curve.continuation());
        m_after_t = ContourCurve(split[1], cont);
      }
      break;

    case ContourCurve::cubic_bezier:
      {
        vecN<detail::CubicBezierCurve, 2> split;
        split = detail::split_cubic(curve, t);

        m_before_t = ContourCurve(split[0], curve.continuation());
        m_after_t = ContourCurve(split[1], cont);
      }
      break;

    case ContourCurve::conic_arc_curve:
    case ContourCurve::conic_curve:
      {
        vecN<detail::ConicCurve, 2> split;

        split = detail::split_conic(curve, t);
        m_before_t = split[0].make_contour_curve(curve.continuation());
        m_after_t = split[1].make_contour_curve(cont);

        if (curve.type() == ContourCurve::conic_arc_curve)
          {
            float angle(curve.arc_angle());

            m_before_t.m_type = ContourCurve::conic_arc_curve;
            m_before_t.m_control_pts[1].y() = t * angle;

            m_after_t.m_type = ContourCurve::conic_arc_curve;
            m_after_t.m_control_pts[1].y() = (1.0f - t) * angle;
          }
      }
      break;

    default:
      ASTRALassert(!"Invalid Curve Type!");
    }

  unsigned int incr;

  incr = (increment_generation) ? 1u : 0u;
  m_before_t.m_generation = curve.m_generation + incr;
  m_after_t.m_generation = curve.m_generation + incr;
}

void
astral::ContourCurveSplit::
force_coordinate(Coordinate coordinate, float value)
{
  m_before_t.m_end_pt[coordinate.m_v] = value;
  m_after_t.m_start_pt[coordinate.m_v] = value;
}

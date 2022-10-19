/*!
 * \file contour_curve_util.cpp
 * \brief file contour_curve_util.cpp
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

#include "contour_curve_util.hpp"

namespace
{
  class L1DistanceComputeWorker
  {
  public:
    L1DistanceComputeWorker(const astral::vec2 &pt, const astral::ContourCurve &curve):
      m_winding(0, 0)
    {
      ASTRALassert(curve.type() != astral::ContourCurve::cubic_bezier);
      if (curve.type() == astral::ContourCurve::line_segment)
        {
          m_ct = (curve.start_pt() + curve.end_pt()) * 0.5f;
          m_w = 1.0f;
        }
      else
        {
          m_ct = curve.control_pt(0);
          m_w = curve.conic_weight();
        }

      m_q1 = curve.start_pt() - pt;
      m_q2 = m_w * (m_ct - pt);
      m_q3 = curve.end_pt() - pt;

      m_A = m_q1 - 2.0 * m_q2 + m_q3;
      m_B = m_q1 - m_q2;
      m_C = m_q1;

      m_aw = 2.0f * (1.0f - m_w);
      m_bw = 1.0f - m_w;
      m_cw = 1.0f;

      float d0, d1;

      d0 = m_q1.L1norm();
      d1 = m_q3.L1norm();

      m_distance = astral::t_min(d0, d1);
      m_time = (d0 < d1) ? 0.0f : 1.0f;

      update_distance<0>();
      update_distance<1>();
    }

    float
    distance(void) const
    {
      return m_distance;
    }

    int
    winding(void) const
    {
      return m_winding[0];
    }

    float
    time(void) const
    {
      return m_time;
    }

  private:
    template<int coordinate>
    void
    update_distance(void)
    {
      const int other_coordinate(1 - coordinate);
      bool use_t1, use_t2;
      float max_B_C, t1, t2;
      const float epsilon = 0.0005f;

      ASTRALassert(coordinate == 0 || coordinate == 1);

      /* See astral_banded_rays.glsl.resource_string for explanation of algorithm */
      use_t1 = (m_q3[coordinate] <= 0.0f && astral::t_max(m_q1[coordinate], m_q2[coordinate]) > 0.0f)
        || (m_q1[coordinate] > 0.0f && m_q2[coordinate] < 0.0f);

      use_t2 = (m_q1[coordinate] <= 0.0f && astral::t_max(m_q2[coordinate], m_q3[coordinate]) > 0.0f)
        || (m_q3[coordinate] > 0.0f && m_q2[coordinate] < 0.0f);

      max_B_C = astral::t_max(astral::t_abs(m_B[coordinate]), astral::t_abs(m_C[coordinate]));
      if (astral::t_abs(m_A[coordinate]) > epsilon * max_B_C)
        {
          float D, rootD, recipA;

          D = m_B[coordinate] * m_B[coordinate] - m_A[coordinate] * m_C[coordinate];
          if (D < 0.0f)
            {
              return;
            }

          recipA = 1.0f / m_A[coordinate];
          rootD = astral::t_sqrt(D);
          t1 = (m_B[coordinate] - rootD) * recipA;
          t2 = (m_B[coordinate] + rootD) * recipA;
        }
      else if (m_B[coordinate] != 0.0f)
        {
          t1 = t2 = 0.5f * m_C[coordinate] / m_B[coordinate];
        }
      else
        {
          return;
        }

      if (use_t1)
        {
          float w, r;

          w = (m_aw * t1 - m_bw * 2.0f) * t1 + m_cw;
          r = (m_A[other_coordinate] * t1 - m_B[other_coordinate] * 2.0f) * t1 + m_C[other_coordinate];

          if (r > 0.0f)
            {
              ++m_winding[coordinate];
            }

          r = astral::t_abs(r) / w;
          if (r < m_distance)
            {
              m_distance = r;
              m_time = t1;
            }
        }

      if (use_t2)
        {
          float w, r;

          w = (m_aw * t2 - m_bw * 2.0f) * t2 + m_cw;
          r = (m_A[other_coordinate] * t2 - m_B[other_coordinate] * 2.0f) * t2 + m_C[other_coordinate];

          if (r > 0.0f)
            {
              --m_winding[coordinate];
            }

          r = astral::t_abs(r) / w;
          if (astral::t_abs(r) < m_distance)
            {
              m_distance = r;
              m_time = t2;
            }
        }
    }

  private:
    astral::vec2 m_A, m_B, m_C, m_q1, m_q2, m_q3, m_ct;
    float m_w, m_aw, m_bw, m_cw, m_distance, m_time;
    astral::ivec2 m_winding;
  };
}

////////////////////////////////////////
// astral::detail methods
float
astral::detail::
compute_l1_distace_to_curve(const vec2 &pt, const ContourCurve &curve, int *w, float *t)
{
  L1DistanceComputeWorker L(pt, curve);

  if (w)
    {
      *w += L.winding();
    }

  if (t)
    {
      *t = L.time();
    }

  return L.distance();
}

float
astral::detail::
compute_quadratic_appoximation(const ContourCurve &c, QuadraticBezierCurve *out_quad)
{
  float error;
  switch (c.type())
    {
    case ContourCurve::cubic_bezier:
      {
        /* Source: http://caffeineowl.com/graphics/2d/vectorial/cubic2quad01.html
         *
         * Let
         *   p(t) = (1-t)^3 p0 + 3t(1-t)^2 p1 + 3t^2(1-t) p2 + t^3 p3
         *
         * Set
         *    A = 3p1 - p0
         *    B = 3p2 - p1
         *    q0 = p0,
         *    q1 = (A + B) / 4
         *    q2 = p2
         *
         * Algebra yeilds that
         *
         *   p(t) - q(t) = (A - B) (t^3 - 1.5t^2 + 0.5t)
         *
         * which implies that the maximizing ||p(t) - q(t)|| is
         * maximizing the polynomial f(t) = t^3 - 1.5t^2 + 0.5t.
         * thankfully, that is a cubic, so the derivative is
         * quadratic. The derivative is 0 at
         *
         *   t0 = 0.5 * (1 - sqrt(3))
         *   t1 = 0.5 * (1 + sqrt(3))
         *
         * Note that f(0) = 0 and f(1) = 0 as well, so the
         * maximum of f happens at t0 or t1. After algebra,
         * f(t0) = sqrt(3) / 36 and f(t1) = -sqrt(3) / 36
         * so the error is just ||A - B|| * sqrt(3) / 36
         */
        vecN<vec2, 4> p(c.start_pt(), c.control_pt(0), c.control_pt(1), c.end_pt());

        const float sqrt3_div_36(0.04811252243f);
        vec2 A(3.0f * p[1] - p[0]);
        vec2 B(3.0f * p[2] - p[3]);
        vec2 A_B(A - B);

        out_quad->operator[](0) = p[0];
        out_quad->operator[](1) = 0.25f * (A + B);
        out_quad->operator[](2) = p[3];
        error = sqrt3_div_36 * A_B.magnitude();
      }
      break;

    case ContourCurve::conic_arc_curve:
    case ContourCurve::conic_curve:
      {
        out_quad->operator[](0) = c.start_pt();
        out_quad->operator[](1) = c.control_pt(0);
        out_quad->operator[](2) = c.end_pt();
        error = compute_error_to_conic(c.start_pt(), c.control_pt(0),
                                       c.end_pt(), c.conic_weight());
      }
      break;

    case ContourCurve::quadratic_bezier:
      {
        out_quad->operator[](0) = c.start_pt();
        out_quad->operator[](1) = c.control_pt(0);
        out_quad->operator[](2) = c.end_pt();
        error = 0.0f;
      }
      break;

    default:
      error = 0.0f;
      ASTRALassert(!"Passed non-curve type to compute_quadratic_appoximation()");
    }

  return error;
}

float
astral::detail::
error_to_line_appoximation(const ContourCurve &p)
{
  if (p.type() == ContourCurve::line_segment)
    {
      return 0.0f;
    }

  if (p.start_pt() == p.end_pt())
    {
      vec2 v;
      float return_value;

      /* just returns the distance to the furthest control
       * point, this can be tightened quite a bit though.
       */
      v = p.control_pt(0) - p.start_pt();
      return_value = dot(v, v);

      if (p.number_control_pts() > 1)
        {
          v = p.control_pt(1) - p.start_pt();
          return_value = t_max(return_value, dot(v, v));
        }

      return t_sqrt(return_value);
    }

  /* Basic idea:
   *  1. First rotate and translate the curve so that the
   *     line segment connecting the end points is mapped
   *     to the x-axis
   *  2. Maximize the y-coordinate of the resulting curve
   */

  Transformation rotate, translate;
  vec2 v(p.end_pt() - p.start_pt());

  v.normalize();
  rotate.m_matrix.row_col(0, 0) = v.x();
  rotate.m_matrix.row_col(0, 1) = v.y();
  rotate.m_matrix.row_col(1, 0) = -v.y();
  rotate.m_matrix.row_col(1, 1) = v.x();

  translate.m_translate = -p.start_pt();

  /* Q is p translated and then rotated to map start_pt()
   * to (0, 0) and end_pt() to somewhere along the x-axis.
   */
  ContourCurve Q(p, rotate * translate);

  /* We only care about how much Q.y varies, which
   * means we just need to find the y-extreme points
   * of Q which we can get just by computing the
   * tight bounding box of Q.
   */
  BoundingBox<float> BB;

  BB = Q.tight_bounding_box();
  if (BB.empty())
    {
      return 0.0f;
    }

  return t_max(t_abs(BB.as_rect().min_y()), t_abs(BB.as_rect().max_y()));
}

astral::vecN<astral::detail::CubicBezierCurve, 2>
astral::detail::
split_cubic(const ContourCurve &C)
{
  ASTRALassert(C.type() == ContourCurve::cubic_bezier);

  vecN<CubicBezierCurve, 2> return_value;
  vecN<vec2, 4> p(C.start_pt(), C.control_pt(0), C.control_pt(1), C.end_pt());
  vec2 p01, p23, pA, pB, pC;

  p01 = (p[0] + p[1]) * 0.5f;
  p23 = (p[2] + p[3]) * 0.5f;
  pA = (p[0] + 2.0f * p[1] + p[2]) * 0.25f;
  pB = (p[1] + 2.0f * p[2] + p[3]) * 0.25f;
  pC = (p[0] + 3.0f * p[1] + 3.0f * p[2] + p[3]) * 0.125f;

  return_value[0] = CubicBezierCurve(p[0], p01, pA, pC);
  return_value[1] = CubicBezierCurve(pC, pB, p23, p[3]);

  return return_value;
}

astral::vecN<astral::detail::CubicBezierCurve, 2>
astral::detail::
split_cubic(const ContourCurve &C, float t)
{
  ASTRALassert(C.type() == ContourCurve::cubic_bezier);

  vecN<CubicBezierCurve, 2> return_value;
  vec2 p01, p12, p23, p02, p13, p03;

  p01 = mix(C.start_pt(), C.control_pt(0), t);
  p12 = mix(C.control_pt(0), C.control_pt(1), t);
  p23 = mix(C.control_pt(1), C.end_pt(), t);

  p02 = mix(p01, p12, t);
  p13 = mix(p12, p23, t);

  p03 = mix(p02, p13, t);

  return_value[0] = CubicBezierCurve(C.start_pt(), p01, p02, p03);
  return_value[1] = CubicBezierCurve(p03, p13, p23, C.end_pt());

  return return_value;
}

astral::vecN<astral::detail::QuadraticBezierCurve, 2>
astral::detail::
split_quadratic(const ContourCurve &C)
{
  ASTRALassert(C.type() == ContourCurve::quadratic_bezier);

  vecN<QuadraticBezierCurve, 2> return_value;
  vec2 p01, p12, pM;
  vecN<vec2, 3> p(C.start_pt(), C.control_pt(0), C.end_pt());

  p01 = 0.5f * (p[0] + p[1]);
  p12 = 0.5f * (p[1] + p[2]);
  pM = 0.5f * (p01 + p12);

  return_value[0] = QuadraticBezierCurve(p[0], p01, pM);
  return_value[1] = QuadraticBezierCurve(pM, p12, p[2]);

  return return_value;
}

astral::vecN<astral::detail::QuadraticBezierCurve, 2>
astral::detail::
split_quadratic(const ContourCurve &C, float t)
{
  ASTRALassert(C.type() == ContourCurve::quadratic_bezier);

  vecN<QuadraticBezierCurve, 2> return_value;
  vec2 p01, p12, p02;

  p01 = mix(C.start_pt(), C.control_pt(0), t);
  p12 = mix(C.control_pt(0), C.end_pt(), t);
  p02 = mix(p01, p12, t);

  return_value[0] = QuadraticBezierCurve(C.start_pt(), p01, p02);
  return_value[1] = QuadraticBezierCurve(p02, p12, C.end_pt());

  return return_value;
}

astral::vecN<astral::detail::LinearBezierCurve, 2>
astral::detail::
split_linear(const ContourCurve &C)
{
  ASTRALassert(C.type() == ContourCurve::line_segment);

  vecN<LinearBezierCurve, 2> return_value;
  vec2 m;

  m = 0.5f * (C.start_pt() + C.end_pt());

  return_value[0] = LinearBezierCurve(C.start_pt(), m);
  return_value[1] = LinearBezierCurve(m, C.end_pt());

  return return_value;
}

astral::vecN<astral::detail::LinearBezierCurve, 2>
astral::detail::
split_linear(const ContourCurve &C, float t)
{
  ASTRALassert(C.type() == ContourCurve::line_segment);

  vecN<LinearBezierCurve, 2> return_value;
  vec2 m;

  m = mix(C.start_pt(), C.end_pt(), t);

  return_value[0] = LinearBezierCurve(C.start_pt(), m);
  return_value[1] = LinearBezierCurve(m, C.end_pt());

  return return_value;
}

astral::vecN<astral::detail::ConicCurve, 2>
astral::detail::
split_conic(const ContourCurve &p)
{
  ASTRALassert(p.type() == ContourCurve::conic_curve || p.type() == ContourCurve::conic_arc_curve);

  vecN<ConicCurve, 2> return_value;
  float w, two_w, recip_one_plus_w, new_w;
  vec2 m, A, B, wc;

  w = p.conic_weight();
  two_w = 2.0f * w;
  recip_one_plus_w = 1.0f / (1.0f + w);
  wc = w * p.control_pt(0);

  new_w = t_sqrt(0.5f + 0.5f * w);

  m = (0.5f * recip_one_plus_w) * (p.start_pt() + two_w * p.control_pt(0) + p.end_pt());
  A = recip_one_plus_w * (p.start_pt() + wc);
  B = recip_one_plus_w * (wc + p.end_pt());

  return_value[0].m_pts = QuadraticBezierCurve(p.start_pt(), A, m);
  return_value[0].m_weight = new_w;

  return_value[1].m_pts = QuadraticBezierCurve(m, B, p.end_pt());
  return_value[1].m_weight = new_w;

  return return_value;
}

astral::detail::ConicCurve
astral::detail::
create_conic(const vecN<vec2, 3> q, const vecN<float, 3> w)
{
  /*
   * Normal form of conic. Given the rational quadratic curve
   *
   *  [w0 * p0, w1 * p1, w2 * p2](t)
   *  -------------------------------    (A)
   *        [w0, w1, w2](t)
   *
   * with w0, w1, w2 > 0
   *
   * can be reparameterized to
   *
   *  [p0, w * p1, p2](s)
   *  -------------------     (B)
   *     [1, w, 1](s)
   *
   * with w = sqrt(w1 * w1 / (w0 * w2))
   * via s = t / (t + a * (1 - t)) where a = sqrt(w2 / w0);
   * doing so is just algebra to confirm, see for instance
   * http://www.redpanda.nl/TUe/GentleIntroToNURBS.pdf
   *
   * Note that we are NOT given [p0, p1, p2], instead
   * we are given [q0, q1, q2] with qI = wI * pI
   */
  ConicCurve R;

  /* Place into R.m_pts the curve [p0, p1, p2], where
   * pI = qI / wI
   */
  for (unsigned int i = 0; i < 3u; ++i)
    {
      R.m_pts[i] = q[i] / w[i];
    }

  /* we now have the form (A), now get to form (B) */
  R.m_weight = t_sqrt(w[1] * w[1] / (w[0] * w[2]));

  return R;
}

astral::vecN<astral::detail::ConicCurve, 2>
astral::detail::
split_conic(const ContourCurve &p, float T)
{
  ASTRALassert(p.type() == ContourCurve::conic_curve || p.type() == ContourCurve::conic_arc_curve);

  /*
   * Splitting conic p will first give the two rational
   * curves:
   *
   *  Pre(t) =  [p0, p01, p02](t)
   *           -------------------
   *            [1, w01, w02](t)
   *
   *
   *  Post(t) =  [p02, p12, p2](t)
   *            -------------------
   *             [w02, w12, 1](t)
   *
   * where
   *    p01 = [p0, w * p1](T)
   *    p12 = [w * p1, p2](T)
   *    p02 = [p01, p12](T)
   *    w01 = [1, w](T)
   *    w12 = [w, 1](T)
   *    w02 = [1, w, 1](T)
   *
   * then use create_conic() to realize them in normal form.
   */
  float w01, w12, w02;
  vec2 p01, p12, p02, pw;
  vecN<ConicCurve, 2> return_value;

  pw = p.conic_weight() * p.control_pt(0);
  p01 = mix(p.start_pt(), pw, T);
  p12 = mix(pw, p.end_pt(), T);
  p02 = mix(p01, p12, T);

  w01 = mix(1.0f, p.conic_weight(), T);
  w12 = mix(p.conic_weight(), 1.0f, T);
  w02 = mix(w01, w12, T);

  /* TODO:
   *   This could be made more efficient by doing the normal form
   *   directly here so that we could use that that the end-point
   *   weight values are already 1.0 and to reuse the reciprocal
   *   of w02.
   */
  return_value[0] = create_conic(vecN<vec2, 3>(p.start_pt(), p01, p02), vecN<float, 3>(1.0f, w01, w02));
  return_value[1] = create_conic(vecN<vec2, 3>(p02, p12, p.end_pt()), vecN<float, 3>(w02, w12, 1.0f));

  /* force the front and back to match with the input */
  return_value[0].m_pts.front() = p.start_pt();
  return_value[1].m_pts.back() = p.end_pt();

  return return_value;
}

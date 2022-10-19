// -*- C++ -*-
/*!
 * \file contour_curve_util.tcc
 * \brief file contour_curve_util.tcc
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

#if !defined(ASTRAL_CONTOUR_CURVE_UTIL_HPP) || defined(ASTRAL_CONTOUR_CURVE_UTIL_TCC)
#error "contour_curve_util.tcc may only be included by contour_curve_util.hpp"
#endif

#define ASTRAL_CONTOUR_CURVE_UTIL_TCC

template<typename T, size_t D>
T
astral::detail::
compute_distance_to_line(const vecN<T, 2> &end,
                         const Polynomial<vecN<T, 2>, D> &C,
                         const Polynomial<vecN<T, 2>, D - 1> &Cprime)
{
  T error(0);
  vecN<T, D - 1u> root_storage;
  vecN<std::complex<T>, D - 1u> complex_root_storage;
  c_array<T> roots(root_storage);
  c_array<std::complex<T>> complex_roots(complex_root_storage);
  vecN<T, 2> start(C.coeff(0)), ES(end - start);
  vecN<T, 2> nES(ES.unit_vector()), J(-nES.y(), nES.x());
  const Polynomial<vecN<T, 2>, 0> Jp(J);
  Polynomial<T, D - 1> fp;
  unsigned int num_roots;

  fp = poly_dot(Jp, Cprime);
  num_roots = solve_polynomial(fp, roots, complex_roots);
  for (unsigned int r = 0; r < num_roots; ++r)
    {
      T t;
      vecN<T, 2> e;

      t = t_max(T(0), t_min(T(1), roots[r]));
      e = C.eval(vecN<T, 2>(t)) - end;
      error = t_max(error, t_abs(dot(e, J)));
    }
  for (unsigned int r = num_roots, rc = 0; r < complex_roots.size(); ++r, ++rc)
    {
      T t;
      vecN<T, 2> e;

      t = t_max(T(0), t_min(T(1), complex_roots[rc].real()));
      e = C.eval(vecN<T, 2>(t)) - end;
      error = t_max(error, t_abs(dot(e, J)));
    }

  return error;
}

template<typename T>
T
astral::detail::
compute_error_to_conic(const vecN<T, 2> &start_pt,
                       const vecN<T, 2> &control_pt,
                       const vecN<T, 2> &end_pt,
                       T conic_weight)
{
  /* See "High order approximation of conic sections by quadratic splines"
   * Michael Floater, 1993 for the derivation of the error estimate
   */
  vecN<T, 2> S;
  T a, m;

  S = start_pt - T(2) * control_pt + end_pt;
  a = t_abs(T(1) - conic_weight);
  m = a / (T(8) + T(4) * a);

  return m * S.magnitude();
}


template<typename T>
void
astral::detail::WaltonMeekBiArc<T>::
realize_as_flat(const vecN<T, 2> &B0,
                const vecN<T, 2> &B1,
                const vecN<T, 2> &B2,
                const Polynomial<vecN<T, 2>, 2> &Q)
{
  /* regard the quadratic curve as flat and bail early */
  m_flat = true;
  m_G = T(0.5) * (B0 + B2);
  m_cL = B0;
  m_cR = B2;
  ASTRALunused(B1);

  m_error = compute_distance_to_line(B2, Q, Q.derivative());
}

template<typename T>
astral::detail::WaltonMeekBiArc<T>::
WaltonMeekBiArc(const QuadraticBezierCurve &pQ,
                float ftheta_small, float flength_small):
  m_flat(false)
{
  typedef T real;
  typedef vecN<T, 2> rvec2;

  real theta_small(static_cast<real>(ftheta_small));
  real length_small(static_cast<real>(flength_small));
  rvec2 B0, B1, B2, V0, V1, T0, T1, N0, N1, W, V, VV, TT, NN, G_B0, G_B2;
  real a, b, d, tmp, cos_theta, sin_theta, s, lambda, mu, alpha;
  Polynomial<real, 2> f;
  vecN<real, 2> solutions;
  Polynomial<rvec2, 2> Q;
  unsigned int num_solutions;

  /* We follow closely the notation used in the paper from 1992
   * "Approximation of quadratic Bezier curves by arc splines" by
   * D.J. Walton and D.S. Meek. However, we use the symbol TT
   * to denoted the unit vector from the start to end point and
   * the symbol NN to denoted the normal vector from TT.
   */

  B0 = rvec2(pQ[0]);
  B1 = rvec2(pQ[1]);
  B2 = rvec2(pQ[2]);

  Q.coeff(0) = B0;
  Q.coeff(1) = real(2) * (B1 - B0);
  Q.coeff(2) = B0 - real(2) * B1 + B2;

  V0 = B1 - B0;
  V1 = B2 - B1;
  VV = B2 - B0;

  a = V0.magnitude();
  b = V1.magnitude();
  d = VV.magnitude();

  if (a < length_small || b < length_small || d < length_small)
    {
      realize_as_flat(B0, B1, B2, Q);
      return;
    }

  T0 = V0 / a;
  T1 = V1 / b;

  cos_theta = dot(T0, T1);
  if (cos_theta >= real(1) - theta_small)
    {
      realize_as_flat(B0, B1, B2, Q);
      return;
    }

  TT = VV / d;

  N0 = rvec2(-T0.y(), T0.x());
  N1 = rvec2(-T1.y(), T1.x());
  NN = rvec2(-TT.y(), TT.x());

  /* Make sure that NN points away from the
   * control point, i.e. inwardly
   */
  tmp = dot(NN, B1 - B0);
  if (tmp > real(0))
    {
      NN = -NN;
    }

  /* make sure that N0 and N1 point in
   * the same direction as NN, i.e.
   * inwardly.
   */
  tmp = dot(NN, N0);
  if (tmp < real(0))
    {
      N0 = -N0;
    }

  tmp = dot(NN, N1);
  if (tmp < real(0))
    {
      N1 = -N1;
    }

  sin_theta = dot(T1, N0);

  f.coeff(2) = real(1) - cos_theta;
  f.coeff(1)
    = dot(T1, N0) * dot(TT, T0) / dot(TT, N0)
    + dot(T0, N1) * dot(TT, T1) / dot(TT, N1);

  f.coeff(0) = -real(0.5) * dot(T0, N1) * dot(T1, N0) / (dot(TT, N0) * dot(TT, N1));

  num_solutions = solve_polynomial(f, c_array<real>(solutions));
  ASTRALassert(num_solutions != 0u);
  for (unsigned int i = 0; i < num_solutions; ++i)
    {
      if (solutions[i] >= real(0.0) && solutions[i] <= real(1))
        {
          s = solutions[i];
        }
    }

  lambda = s * dot(VV, N1) / dot(T0, N1);
  mu = s * dot(VV, N0) / dot(T1, N0);

  /* TODO: I'd think that that lambda and mu should
   *       be non-negative, but some curve collapse
   *       examples have lamdb and mu negative. Until
   *       we know what is the real situation, we will
   *       let them be negative. I suspect that the
   *       issue is the face forward code for NN is
   *       not correct.
   */
  //ASTRALassert(lambda >= real(0));
  //ASTRALassert(mu >= real(0));

  V = B0 + lambda * T0;
  W = B2 - mu * T1;

  /* The paper does not give a closed formula for G,
   * but it is only a few steps to get it.
   *
   * From the contruction in the paper one has
   *
   *  G = mix(V, W, alpha) for some 0 <= alpha <= 1
   *  V = B0 + lambda * T0
   *  ||G - V|| = ||V - B0||
   *
   * Note that
   *
   *  G - V = (1 - alpha) * V + alpha * W - V
   *        = alpha * (W - V)
   *
   * which gives
   *
   *            ||G - V||
   *  alpha =  -----------
   *            ||W - V||
   *
   *            ||V - B0||
   *        =  ------------
   *            ||W - V||
   *
   *             lambda
   *        =  -----------
   *            ||W - V||
   */
  alpha = lambda / (V - W).magnitude();
  m_G = (real(1) - alpha) * V + alpha * W;

  /* the two arcs meet at G tangentially
   *
   *   cL = B0 + rL * N0
   *   rL = ||G - cL|| = ||B0 - cL||
   *
   * thus
   *
   *   rL^2 = ||G - B0 - rL * N0||^2
   *        = ||G - B0||^2 - 2rL<G - B0, N0> + rL^2
   *
   * which becomes.
   *
   *  0 = ||G - B0||^2 - 2rL<G - B0, N0>
   *
   * Note that rL can be negative which just
   * means that the center of the circle is not
   * on the inward side.
   */
  G_B0 = m_G - B0;
  m_rL = dot(G_B0, G_B0) / (real(2) * dot(G_B0, N0));
  m_cL = B0 + m_rL * N0;

  /* For rR and cR we have
   *
   *   cR = B2 + rR * N1
   *   rR = ||G - cR|| = ||B2 - cR||
   *
   * thus,
   *
   *   rR^2 = ||G - B2 - rR * N1||^2
   *        = ||G - B2||^2 - 2rR<G - B2, N1> + rR^2
   *
   * which becomes
   *
   *  0 = ||G - B2||^2 - 2rR<G - B2, N1>
   *
   * Note that rR can be negative which just
   * means that the center of the circle is not
   * on the inward side.
   */
  G_B2 = m_G - B2;
  m_rR = dot(G_B2, G_B2) / (real(2) * dot(G_B2, N1));
  m_cR = B2 + m_rR * N1;

  /* compute the error part 1: the deviation of the point G
   * from the quadratic curve, Theorem 2 of paper
   */
  real cos_phi, sin_phi, sigma(0), k;
  Polynomial<real, 2> g;

  cos_phi = t_abs(dot(TT, T0));
  sin_phi = t_sqrt(t_max(T(0), T(1) - cos_phi * cos_phi));

  g.coeff(0) = -lambda * (real(1) + cos_phi);
  g.coeff(1) = real(2) * a * cos_phi;
  g.coeff(2) = d - real(2) * a * cos_phi;

  /* the quadratic solver has additional code to
   * make the root that is not large correct
   * even when the leading coefficient is close
   * to zero.
   */
  if (g.coeff(2) != T(0))
    {
      num_solutions = solve_polynomial(g, c_array<real>(solutions));
      ASTRALassert(num_solutions != 0u);
      for (unsigned int i = 0; i < num_solutions; ++i)
        {
          if (solutions[i] >= real(0.0) && solutions[i] <= real(1))
            {
              sigma = solutions[i];
            }
        }
    }
  else
    {
      sigma = -g.coeff(0) / g.coeff(1);
    }
  k = real(2) * a * sigma * (real(1) - sigma) - lambda;
  k = t_abs(k * sin_phi);

  /* compute the error part 2: which is Theorem 3 of the paper */
  m_error = k;

  Polynomial<real, 2> zL, zR;

  zL.coeff(2) = a * a + b * b - real(2) * a * b * cos_theta;
  zL.coeff(1) = real(3) * a * (b * cos_theta - a);
  zL.coeff(0) = real(2) * a * a - m_rL * b * sin_theta;
  num_solutions = solve_polynomial(zL, c_array<real>(solutions));
  for (unsigned int i = 0; i < num_solutions; ++i)
    {
      if (solutions[i] >= real(0) && solutions[i] <= sigma)
        {
          vecN<real, 2> p;
          real e;

          p = Q.eval(rvec2(solutions[i])) - m_cL;
          e = t_abs(m_rL - p.magnitude());
          m_error = t_max(e, m_error);
        }
    }

  zR.coeff(2) = zL.coeff(2);
  zR.coeff(1) = -real(2) * a * a + b * b + a * b * cos_theta;
  zR.coeff(0) = a * a + a * b * cos_theta - m_rR * a * sin_theta;
  num_solutions = solve_polynomial(zR, c_array<real>(solutions));
  for (unsigned int i = 0; i < num_solutions; ++i)
    {
      if (solutions[i] >= sigma && solutions[i] <= real(1))
        {
          vecN<real, 2> p;
          real e;

          p = Q.eval(rvec2(solutions[i])) - m_cR;
          e = t_abs(m_rR - p.magnitude());
          m_error = t_max(e, m_error);
        }
    }
}

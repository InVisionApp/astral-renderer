/*!
 * \file matrix.cpp
 * \brief file matrix.cpp
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

#include <astral/util/matrix.hpp>

static
astral::vec2
compute_singular_values_squared(const astral::float2x2 &M)
{
  float a, b, c, d, ad_bc;
  float B, C;

  /* Given a matrix M, the singular value decomposition of
   * M is given by:
   *
   *   M = U * D * transpose(V)
   *
   * where D is a diagnol matrix, U and V are orthonormal
   * matrices. Thus, the inverse of U is transpose(U)
   * and the inverse of transpose(V) is V.
   *
   * Let
   *
   *  N = M * transpose(M)
   *
   * Then,
   *
   *  N = U * (D * D) * transpose(U)
   *
   * Thus, the eigenvalue of N are the squares of the
   * singular values of M. Since N is 2x2 we can use the
   * quadratic formula to get the eigen values of N
   * easily.
   *
   *     | a^2 + b^2        ac + bd  |
   * N = |                           |
   *     |  ac + bd        c^2 + d^2 |
   *
   *
   * the eigen values are the solutions to the quadratic polynomial
   *
   * f(t) = det(tI - N)
   *
   * which (after algebra) is given by
   *
   * f(t) = t^2 - Bt + C
   *
   * where
   *
   *  B = a^2 + b^2 + c^2 + d^2
   *  C = (ad - bc)^2
   *
   * and thus the return value is
   *
   * t0 = 0.5 * (B + sqrt(D))
   * t1 = 0.5 * (B - sqrt(D))
   *
   * where D = B^2 - 4C
   *
   * In addition, because the matrix N is positive semidefinite
   * via for any vector v,
   *
   *  <v, Nv> = <v, M(transpose(M)(v)) >
   *          = <transpose(M)(v), transpose(M)(v)>
   *          >= 0
   *
   * we then know that the Eigen values are real, though
   * possibly zero. Hence, we know that D >= 0 as well.
   */
  a = M.row_col(0, 0);
  b = M.row_col(0, 1);
  c = M.row_col(1, 0);
  d = M.row_col(1, 1);

  B = a * a + b * b + c * c + d * d;
  if (B <= 0.0f)
    {
      /* we have, up to numerical accuracy that M is the
       * zero matrix, return 0's.
       */
      return astral::vec2(0.0f, 0.0f);
    }

  ad_bc = (a * d - b * c);
  C = ad_bc * ad_bc;

  if (C > 0.0f)
    {
      float desc;

      /* in exact arithmatic B * B >= 4 * C, but we max with 0
       * to prevent numerical rounding errors from taking a
       * negative sqrt.
       */
      desc = astral::t_sqrt(astral::t_max(0.0f, B * B - 4.0f * C));

      /* We know that B >= 0, thus B + desc does not
       * have catostrophic cancellation. However,
       * B - desc does have such cancellation.
       *
       *              (B - desc) (B + desc)
       *  B - desc = ----------------------
       *                  B + desc
       *
       *
       *              B * B - (B * B - 4 * C)
       *           = ------------------------
       *                    B + desc
       *
       *                4 * C
       *           = ----------
       *              B + desc
       *
       * We have already ruled out for B to be zero
       * from the earlier early return, so it is safe
       * to divide by B + desc
       */
      float B_plus_desc;
      astral::vec2 return_value;

      B_plus_desc = B + desc;
      return_value[0] = 0.5f * B_plus_desc;
      return_value[1] = 2.0f * C / B_plus_desc;

      return return_value;
    }
  else
    {
      return astral::vec2(B, B);
    }
}

astral::vecN<float, 2>
astral::
compute_singular_values(const float2x2 &M, enum matrix_type_t tp)
{
  if (tp == matrix_diagonal)
    {
      float a(t_abs(M.row_col(0, 0)));
      float b(t_abs(M.row_col(1, 1)));

      return vec2(t_max(a, b), t_min(a, b));
    }
  else if (tp == matrix_anti_diagonal)
    {
      float a(t_abs(M.row_col(1, 0)));
      float b(t_abs(M.row_col(0, 1)));

      return vec2(t_max(a, b), t_min(a, b));
    }

  vec2 R;

  R = compute_singular_values_squared(M);
  R = vec2(t_sqrt(R.x()), t_sqrt(R.y()));

  return R;
}

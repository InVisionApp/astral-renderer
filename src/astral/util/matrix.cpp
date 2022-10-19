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
  float B, C, desc;
  astral::vec2 return_value;

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
   */
  a = M.row_col(0, 0);
  b = M.row_col(0, 1);
  c = M.row_col(1, 0);
  d = M.row_col(1, 1);

  B = a * a + b * b + c * c + d * d;
  ad_bc = (a * d - b * c);
  C = ad_bc * ad_bc;

  /* in exact arithmatic B * B >= 4 * C, but we max with 0
   * to prevent numerical rounding errors from taking a
   * negative sqrt.
   */
  desc = astral::t_sqrt(astral::t_max(0.0f, B * B - 4.0f * C));

  /* in exact arithmatic B >= desc, but we max with 0
   * to prevent numerical rounding errors from giving
   * a negative return value.
   */
  return 0.5f * astral::vec2(B + desc, astral::t_max(0.0f, B - desc));
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

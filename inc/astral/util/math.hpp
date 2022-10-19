/*!
 * \file math.hpp
 * \brief file math.hpp
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


#ifndef ASTRAL_MATH_HPP
#define ASTRAL_MATH_HPP

#include <math.h>
#include <stdlib.h>

namespace astral
{
/*!\addtogroup Utility
 * @{
 */

/*!\def ASTRAL_PI
 * Macro giving the value of pi as a float
 */
#define ASTRAL_PI 3.14159265358979323846f

/*!\def ASTRAL_SQRT2
 * Macro giving sqaure root of 2 as a float
 */
#define ASTRAL_SQRT2 1.41421356237f

/*!\def ASTRAL_HALF_SQRT2
 * Macro giving half of sqaure root of 2 as a float
 */
#define ASTRAL_HALF_SQRT2 0.707106781f

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_sin(float x) { return ::sinf(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_cos(float x) { return ::cosf(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_tan(float x) { return ::tanf(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_sqrt(float x) { return ::sqrtf(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_cbrt(float x) { return ::cbrtf(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_asin(float x) { return ::asinf(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_acos(float x) { return ::acosf(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_atan(float x) { return ::atanf(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_atan2(float y, float x) { return ::atan2f(y, x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_floor(float x) { return ::floorf(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_exp(float x) { return ::expf(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_log(float x) { return ::logf(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_fmod(float x, float y) { return ::fmodf(x, y); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_ldexp(float x, int n) { return ::ldexpf(x, n); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_sin(double x) { return ::sin(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_cos(double x) { return ::cos(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_tan(double x) { return ::tan(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_sqrt(double x) { return ::sqrt(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_cbrt(double x) { return ::cbrt(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_asin(double x) { return ::asin(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_acos(double x) { return ::acos(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_atan(double x) { return ::atan(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_floor(double x) { return ::floor(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_atan2(double y, double x) { return ::atan2(y, x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_exp(double x) { return ::exp(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_log(double x) { return ::log(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_fmod(double x, double y) { return ::fmod(x, y); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_ldexp(double x, int n) { return ::ldexp(x, n); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_sin(long double x) { return ::sinl(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_cos(long double x) { return ::cosl(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_tan(long double x) { return ::tanl(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_sqrt(long double x) { return ::sqrtl(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_cbrt(long double x) { return ::cbrtl(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_asin(long double x) { return ::asinl(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_acos(long double x) { return ::acosl(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_atan(long double x) { return ::atanl(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_atan2(long double y, long double x) { return ::atan2l(y, x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_floor(long double x) { return ::floorl(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_exp(long double x) { return ::expl(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_log(long double x) { return ::logl(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_fmod(long double x, long double y) { return ::fmodl(x, y); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_ldexp(long double x, int n) { return ::ldexpl(x, n); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  int
  t_abs(int x) { return ::abs(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  unsigned int
  t_abs(unsigned int x) { return x; }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long
  t_abs(long x) { return ::labs(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  unsigned long
  t_abs(unsigned long x) { return x; }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long long
  t_abs(long long x) { return ::llabs(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  unsigned long long
  t_abs(unsigned long long x) { return x; }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  float
  t_abs(float x) { return fabsf(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  double
  t_abs(double x) { return fabs(x); }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  inline
  long double
  t_abs(long double x) { return fabsl(x); }
/*! @} */

}

#endif

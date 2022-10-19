/*!
 * \file UniformScaleTranslate.hpp
 * \brief file UniformScaleTranslate.hpp
 *
 * Adapted from: WRATHScaleTranslate.hpp of WRATH:
 *
 * Copyright 2013 by Nomovok Ltd.
 * Contact: info@nomovok.com
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#ifndef ASTRAL_DEMO_SCALE_TRANSLATE_HPP
#define ASTRAL_DEMO_SCALE_TRANSLATE_HPP

#include <cstdlib>
#include <astral/util/util.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/math.hpp>
#include <astral/util/matrix.hpp>
#include <astral/util/transformation.hpp>
#include <astral/util/ostream_utility.hpp>

#include "generic_command_line.hpp"

/*!\class UniformScaleTranslate
  A UniformScaleTranslate represents the composition
  of a scaling and a translation, i.e.
  \f[ f(x,y) = (sx, sy) + (A, B) \f]
  where \f$ s \f$ = \ref m_scale and \f$ (A, B) \f$ = \ref m_translation.
 */
template<typename T>
class UniformScaleTranslate
{
public:
  /*!
   * Ctor. Initialize a UniformScaleTranslate from a
   * scaling factor and translation
   * \param tr translation to use
   * \param s scaling factor to apply to both x-axis and y-axis
   */
  UniformScaleTranslate(const astral::vecN<T, 2> &tr = astral::vecN<T, 2>(T(0), T(0)),
                        T s = T(1)):
    m_scale(s),
    m_translation(tr)
  {}

  /*!
   * Ctor. Initialize a UniformScaleTranslate from a
   * scaling factor
   * \param s scaling factor to apply to both x-axis and y-axis
   */
  UniformScaleTranslate(T s):
    m_scale(s),
    m_translation(T(0), T(0))
  {}

  /*!
   * Returns the inverse transformation to this.
   */
  UniformScaleTranslate
  inverse(void) const
  {
    UniformScaleTranslate r;

    r.m_scale = T(1) / m_scale;
    r.m_translation = -r.m_scale * m_translation;

    return r;
  }

  /*!
   * Returns the value of applying the transformation to a point.
   * \param pt point to apply the transformation to.
   */
  astral::vecN<T, 2>
  apply_to_point(const astral::vecN<T, 2> &pt) const
  {
    return m_scale * pt + m_translation;
  }

  /*!
   * Returns the value of applying the inverse of the
   * transformation to a point.
   * \param pt point to apply the transformation to.
   */
  astral::vecN<T, 2>
  apply_inverse_to_point(const astral::vecN<T, 2> &pt) const
  {
    return (pt - m_translation) / m_scale;
  }

  /*!
   * Returns the astral::Transformation
   * value
   */
  astral::Transformation
  astral_transformation(void) const
  {
    astral::Transformation R;

    R.m_translate = astral::vec2(m_translation);
    R.m_matrix.row_col(0, 0) = R.m_matrix.row_col(1, 1) = float(m_scale);
    R.m_matrix.row_col(1, 0) = R.m_matrix.row_col(0, 1) = 0.0f;
    return R;
  }

  /*!
   * Computes the interpolation between
   * two UniformScaleTranslate objects.
   * \param a0 begin value of interpolation
   * \param a1 end value of interpolation
   * \param t interpolate, t=0 returns a0, t=1 returns a1
   */
  static
  UniformScaleTranslate
  interpolate(const UniformScaleTranslate &a0,
              const UniformScaleTranslate &a1,
              T t)
  {
    UniformScaleTranslate R;

    R.m_translation = a0.m_translation + t * (a1.m_translation - a0.m_translation);
    R.m_scale = a0.m_scale + t * (a1.m_scale - a0.m_scale);
    return R;
  }

  /*!
   * Amount by which both x-axis and y-axis are scaled
   */
  T m_scale;

  /*!
   * Amount by which to translate AFTER applying \ref m_scale
   */
  astral::vecN<T, 2> m_translation;
};

/*!
 * Compose two UniformScaleTranslate so that:
 *   (a * b).apply_to_point(p) = a.apply_to_point(b.apply_to_point(p)).
 * \param a left hand side of composition
 * \param b right hand side of composition
 */
template<typename T>
UniformScaleTranslate<T>
operator*(const UniformScaleTranslate<T> &a, const UniformScaleTranslate<T> &b)
{
  UniformScaleTranslate<T> c;

  c.m_scale = a.m_scale * b.m_scale;
  c.m_translation = a.apply_to_point(b.m_translation);

  return c;
}

template<typename T>
void
readvalue_from_string(UniformScaleTranslate<T> &value, const std::string &value_string)
{
  std::string R(value_string);

  std::replace(R.begin(), R.end(), ':', ' ');
  std::istringstream istr(R);

  /* read x-translate */
  istr >> value.m_translation.x();
  istr >> value.m_translation.y();
  istr >> value.m_scale;
}

template<typename T>
void
writevalue_to_stream(const UniformScaleTranslate<T> &value, std::ostream &ostr)
{
  ostr << "(translate = " << value.m_translation << ", scale = " << value.m_scale << ")";
}

template<typename T>
std::string
read_format_description(astral::type_tag<UniformScaleTranslate<T>>)
{
  return " formatted as translate-x:translate-y:scale";
}

template<typename T>
std::ostream&
operator<<(std::ostream &str, const UniformScaleTranslate<T> &value)
{
  str << "(translate = " << value.m_translation << ", scale = "
      << value.m_scale << ")";
  return str;
}

/*! @} */

#endif

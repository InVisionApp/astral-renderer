/*!
 * \file scale_translate.hpp
 * \brief file scale_translate.hpp
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

#ifndef ASTRAL_SCALE_TRANSLATE_HPP
#define ASTRAL_SCALE_TRANSLATE_HPP

#include <astral/util/vecN.hpp>
#include <astral/util/bounding_box.hpp>

namespace astral
{
/*!\addtogroup Utility
 * @{
 */

  /*!
   * \brief
   * An astral::ScaleTranslateT represents the transformation
   * mapping a point \f$p\f$ to \f$ S * p + T\f$ where \f$S\f$
   * is the scaling factor in each dimension held in \ref m_scale
   * and \f$T\f$ is the translation \ref m_translate, i.e. an
   * astral::ScaleTranslate can represent a transformation that
   * translates and scales but does not rotate.
   */
  template<typename T>
  class ScaleTranslateT
  {
  public:
    /*!
     * Ctor that initializes to identity transformation
     */
    ScaleTranslateT(void):
      m_translate(T(0), T(0)),
      m_scale(T(1), T(1))
    {}

    /*!
     * Ctor
     * \param p initial value for \ref m_translate
     * \param sc initial value for \ref m_scale
     */
    explicit
    ScaleTranslateT(const vecN<T, 2> &p, const vecN<T, 2> &sc = vecN<T, 2>(T(1), T(1))):
      m_translate(p),
      m_scale(sc)
    {}

    /*!
     * Compute (and return) the transformation represented
     * by this astral::ScaleTranslate applied to a point
     */
    vecN<T, 2>
    apply_to_point(const vecN<T, 2> &p) const
    {
      return m_scale * p + m_translate;
    }

    /*!
     * Compute (and return) the transformation represented
     * by this astral::ScaleTranslate applied to a bounding
     * box
     */
    BoundingBox<T>
    apply_to_bb(const BoundingBox<T> &bb) const
    {
      BoundingBox<T> return_value;
      if (!bb.empty())
        {
          return_value.union_point(apply_to_point(bb.min_point()));
          return_value.union_point(apply_to_point(bb.max_point()));
        }
      return return_value;
    }

    /*!
     * Compute and return the inverse of this \ref
     * ScaleTranslate
     */
    ScaleTranslateT
    inverse(void) const
    {
      ScaleTranslateT return_value;

      return_value.m_scale = vecN<T, 2>(T(1)) / m_scale;
      return_value.m_translate = -return_value.m_scale * m_translate;

      return return_value;
    }

    /*!
     * overload for operator* to compute the composition
     * of two \ref ScaleTranslate values
     */
    ScaleTranslateT
    operator*(const ScaleTranslateT &rhs) const
    {
      ScaleTranslateT R;

      R.m_scale = m_scale * rhs.m_scale;
      R.m_translate = m_translate + m_scale * rhs.m_translate;

      return R;
    }

    /*!
     * Set \ref m_translate
     */
    ScaleTranslateT&
    translate(const vecN<T, 2> &v)
    {
      m_translate = v;
      return *this;
    }

    /*!
     * Set \ref m_translate
     */
    ScaleTranslateT&
    translate(const T &x, const T &y)
    {
      m_translate.x() = x;
      m_translate.y() = y;
      return *this;
    }

    /*!
     * Set \ref m_scale
     */
    ScaleTranslateT&
    scale(const vecN<T, 2> &v)
    {
      m_scale = v;
      return *this;
    }

    /*!
     * Set \ref m_scale
     */
    ScaleTranslateT&
    scale(const T &x, const T &y)
    {
      m_scale.x() = x;
      m_scale.y() = y;
      return *this;
    }

    /*!
     * The translation to apply (after the scaling)
     */
    vecN<T, 2> m_translate;

    /*!
     * The scalng to apply (before the translation)
     */
    vecN<T, 2> m_scale;
  };

  /*!
   * Typedef for astral::ScaleTranslateT for type float.
   */
  typedef ScaleTranslateT<float> ScaleTranslate;

/*! @} */
}

#endif

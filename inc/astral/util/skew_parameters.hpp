/*!
 * \file skew_parameters.hpp
 * \brief file skew_parameters.hpp
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

#ifndef ASTRAL_SKEW_PARAMETERS_HPP
#define ASTRAL_SKEW_PARAMETERS_HPP

#include <astral/util/transformation.hpp>

namespace astral
{
/*!\addtogroup Utility
 * @{
 */

  /*!
   * \brief
   * An astral::SkewParameters represents how to skew
   * (typically text).
   */
  class SkewParameters
  {
  public:
    /*!
     * Ctor.
     */
    SkewParameters(void):
      m_scale_x(1.0f),
      m_skew_x(0.0f)
    {}

    /*!
     * Set \ref m_scale_x
     * \param v value to use
     */
    SkewParameters&
    scale_x(float v)
    {
      m_scale_x = v;
      return *this;
    }

    /*!
     * Set \ref m_skew_x
     * \param v value to use
     */
    SkewParameters&
    skew_x(float v)
    {
      m_skew_x = v;
      return *this;
    }

    /*!
     * Returns the SkewParameters as a transformation
     */
    Transformation
    as_transformation(void) const
    {
      Transformation R;

      R.m_matrix.row_col(0, 0) = m_scale_x;
      R.m_matrix.row_col(0, 1) = -m_skew_x;
      R.m_matrix.row_col(1, 0) = 0.0f;
      R.m_matrix.row_col(1, 1) = 1.0f;

      return R;
    }

    /*!
     * The amount by which to scale the x-coordinates.
     * Default value is 1.0.
     */
    float m_scale_x;

    /*!
     * The amount of skew to apply. The x-coordinate
     * is shifted by this value times the y-coordinate.
     * Default value is 0.0.
     */
    float m_skew_x;
  };

/*! @} */
}

#endif

/*!
 * \file relative_threshhold.hpp
 * \brief file relative_threshhold.hpp
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

#ifndef ASTRAL_RELATIVE_THRESHHOLD_HPP
#define ASTRAL_RELATIVE_THRESHHOLD_HPP

#include <astral/util/bounding_box.hpp>

namespace astral
{
/*!\addtogroup Utility
 * @{
 */
  /*!
   * \brief
   * Simple class to specify a relative threshhold
   */
  class RelativeThreshhold
  {
  public:
    /*!
     * Ctor.
     * \param v initial value for \ref m_value
     */
    explicit
    RelativeThreshhold(float v):
      m_value(v)
    {}

    /*!
     * Compute the absoulte threshhold of \ref
     * m_value acating on an astral::BoundingBox.
     */
    float
    absolute_threshhold(const BoundingBox<float> &bb)
    {
      vec2 sz(bb.size());
      return m_value * t_min(sz.x(), sz.y());
    }

    /*!
     * The value of the relative threshhold
     */
    float m_value;
  };

/*! @} */
}

#endif

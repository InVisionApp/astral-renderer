/*!
 * \file colorstop.hpp
 * \brief file colorstop.hpp
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

#ifndef ASTRAL_COLOR_STOP_HPP
#define ASTRAL_COLOR_STOP_HPP

#include <astral/util/vecN.hpp>
#include <astral/util/color.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * \brief
   * A ColorStop represents a single (time, color) pair value.
   * \tparam the type to hold the color value, T can be one of
   *         astral::vec4 or astral::FixedPointColor
   */
  template<typename T>
  class ColorStop
  {
  public:
    /*!
     * Ctor that initializes \ref m_t as 0.0 and
     * \ref m_color as FixedPointColor()
     */
    ColorStop(void):
      m_t(0.0f)
    {}

    /*!
     * Ctor
     * \param pt initail value for \ref m_t
     * \param pc initail value for \ref m_color
     */
    ColorStop(float pt, const T &pc):
      m_t(pt),
      m_color(pc)
    {}

    /*!
     * Set \ref m_t
     */
    ColorStop&
    t(float v)
    {
      m_t = v;
      return *this;
    }

    /*!
     * Set \ref m_color
     */
    ColorStop&
    color(const T &c)
    {
      m_color = c;
      return *this;
    }

    /*!
     * Comparison operator that compares \ref m_t
     */
    bool
    operator<(ColorStop rhs) const
    {
      return m_t < rhs.m_t;
    }

    /*!
     * The time of the color-stop
     */
    float m_t;

    /*!
     * the color value of the colorstop. The color
     * value is *WITHOUT* alpha being pre-multiplied.
     */
    T m_color;
  };

/*! @} */
}

#endif

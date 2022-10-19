/*!
 * \file mipmap_level.hpp
 * \brief file mipmap_level.hpp
 *
 * Copyright 2022 by InvisionApp.
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

#ifndef ASTRAL_MIPMAP_LEVEL_HPP
#define ASTRAL_MIPMAP_LEVEL_HPP

#include <stdint.h>
#include <astral/util/matrix.hpp>
#include <astral/renderer/render_enums.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * Class to specify a specific mipmap level.
   */
  class MipmapLevel
  {
  public:
    /*!
     * Ctor.
     * \param v initial value for \ref m_value
     */
    explicit
    MipmapLevel(uint32_t v = 0):
      m_value(v)
    {}

    /*!
     * Ctor computes the appropriate mipmap level
     * from a matrix and mipmap_t value
     */
    MipmapLevel(enum mipmap_t mip, const float2x2 &matrix);

    /*!
     * What mipmap LOD to use
     */
    uint32_t m_value;
  };
}

#endif

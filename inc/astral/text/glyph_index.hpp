/*!
 * \file glyph_index.hpp
 * \brief file glyph_index.hpp
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

#ifndef ASTRAL_GLYPH_INDEX_HPP
#define ASTRAL_GLYPH_INDEX_HPP

#include <stdint.h>

namespace astral
{

/*!\addtogroup Text
 * @{
 */

  /*!
   * \brief
   * Class wrapper over uint32_t to strongly type an
   * index of a glyph.
   */
  class GlyphIndex
  {
  public:
    /*!
     * Ctor.
     * \param v initial value for \ref m_value
     */
    explicit
    GlyphIndex(uint32_t v = 0u):
      m_value(v)
    {}

    /*!
     * The actual value of the glyph index
     */
    uint32_t m_value;
  };

/*! @} */
}

#endif

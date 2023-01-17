/*!
 * \file character_mapping.hpp
 * \brief file character_mapping.hpp
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

#ifndef ASTRAL_CHARACTER_MAPPING_HPP
#define ASTRAL_CHARACTER_MAPPING_HPP

#include <utility>
#include <unordered_map>

#include <astral/text/glyph_index.hpp>

namespace astral
{
/*!\addtogroup Text
 * @{
 */

  /*!
   * \brief
   * A CharacterMapping gives the mapping from character values
   * to astra::GlyphIndex values.
   */
  class CharacterMapping
  {
  public:
    /*!
     * The unordered map mapping from character codes
     * to astral::GlyphIndex values.
     */
    std::unordered_map<uint32_t, GlyphIndex> m_values;
  };

/*! @} */
}

#endif

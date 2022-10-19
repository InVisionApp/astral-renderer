/*!
 * \file shadow_map_id.hpp
 * \brief file shadow_map_id.hpp
 *
 * Copyright 2020 by InvisionApp.
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

#ifndef ASTRAL_SHADOW_MAP_ID_HPP
#define ASTRAL_SHADOW_MAP_ID_HPP

namespace astral
{
  class ShadowMap;
  class ShadowMapAtlas;

  /*!
   * \brief
   * An ShadowMapID is used to uniquely identify an astral::ShadowMap.
   */
  class ShadowMapID
  {
  public:
    ShadowMapID(void):
      m_slot(~0u)
    {}

    /*!
     * Returns true if and only if the ShadowMapID
     * came from astral::ShadowMap::shadow_map_id().
     * NOTE: if the astral::ShadowMap that made this
     * ID goes out of scope, valid() will still returns
     * true, but ShadowMapAtlas::fetch_shadow_map()
     * will return nullptr.
     */
    bool
    valid(void) const
    {
      return m_slot != ~0u;
    }

  private:
    friend class ShadowMap;
    friend class ShadowMapAtlas;

    unsigned int m_slot;
    unsigned int m_uniqueness;
  };
}

#endif

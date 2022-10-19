/*!
 * \file image_id.hpp
 * \brief file image_id.hpp
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

#ifndef ASTRAL_TILED_IMAGE_ID_HPP
#define ASTRAL_TILED_IMAGE_ID_HPP

namespace astral
{
  class ImageAtlas;
  class Image;

  /*!
   * \brief
   * An ImageID is used to uniquely identify an astral::Image.
   */
  class ImageID
  {
  public:
    ImageID(void):
      m_slot(~0u)
    {}

    /*!
     * Returns true if and only if the ImageID
     * value came from astral::Image::image_id().
     * NOTE: if the astral::Image that made this
     * ID goes out of scope, valid() will still
     * return true, but ImageAtlas::fetch_image()
     * will return nullptr.
     */
    bool
    valid(void) const
    {
      return m_slot != ~0u;
    }

  private:
    friend class Image;
    friend class ImageAtlas;

    unsigned int m_slot;
    unsigned int m_uniqueness;
  };
}

#endif

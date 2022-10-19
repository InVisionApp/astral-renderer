/*!
 * \file mask_details.cpp
 * \brief mask_details.cpp
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

#include <astral/renderer/mask_details.hpp>

///////////////////////////////////
// astral::MaskDetails methods
astral::BoundingBox<float>
astral::MaskDetails::
pixel_rect(void) const
{
  BoundingBox<float> bb(astral::vec2(0.0f), m_size);

  return m_mask_transformation_pixel.inverse().apply_to_bb(bb);
}

void
astral::MaskDetails::
instersect_against_pixel_rect(const BoundingBox<float> &pixel_rect)
{
  /* transform pixel_rect to mask coordinates and intersect rgn against it.
   * Note that rgn's min-corner is at (0, 0) this is because the transformations
   * for mapping to the sub-rect R whose min corner is given by m_min_corner and
   * size is given by m_size.
   */
  astral::BoundingBox<float> mapped, rgn(astral::vec2(0.0f), m_size);

  mapped = m_mask_transformation_pixel.apply_to_bb(pixel_rect);
  rgn.intersect_against(mapped);

  m_size = rgn.size();
  if (!rgn.empty())
    {
      m_mask_transformation_pixel.m_translate -= rgn.min_point();
      m_min_corner += rgn.min_point();
    }
}

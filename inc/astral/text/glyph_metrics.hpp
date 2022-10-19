/*!
 * \file glyph_metrics.hpp
 * \brief file glyph_metrics.hpp
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

#ifndef ASTRAL_GLYPH_METRICS_HPP
#define ASTRAL_GLYPH_METRICS_HPP

#include <astral/util/util.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/bounding_box.hpp>

namespace astral
{
/*!\addtogroup Text
 * @{
 */

  /*!
   * Class to hold metrics about a glyph.
   */
  class GlyphMetrics
  {
  public:
    /*!
     * Default ctor leaving all fields uninitialized.
     */
    GlyphMetrics(void)
    {}

    /*!
     * The offset (in font coordinates) from the pen
     * at which to display the glyph when performing
     * horizontal text layout.
     */
    vec2 m_horizontal_layout_offset;

    /*!
     * The offset (in font coordinates) from the pen
     * at which to display the glyph when performing
     * horizontal text layout.
     */
    vec2 m_vertical_layout_offset;

    /*!
     * Size (in font coordinates) reported by font (or font loading library)
     * that gives the size of the bounding box of the glyph.
     */
    vec2 m_size;

    /*!
     * The bounding box of the glyph as reported by the font loader
     */
    BoundingBox<float> m_bb;

    /*!
     * How much (in font coordinates) to advance the pen
     * after drawing the glyph. The x-coordinate holds the
     * advance when performing layout horizontally and
     * the y-coordinate when performing layout vertically.
     */
    vec2 m_advance;
  };

/*! @} */
}

#endif

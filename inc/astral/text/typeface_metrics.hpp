/*!
 * \file typeface_metrics.hpp
 * \brief file typeface_metrics.hpp
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

#ifndef ASTRAL_TYPEFACE_METRICS_HPP
#define ASTRAL_TYPEFACE_METRICS_HPP


namespace astral
{

/*!\addtogroup Text
 * @{
 */

  /*!
   * \brief
   * Metrics that occur in both scalable and non-scalable typefaces
   */
  class TypefaceMetricsBase
  {
  public:
    /*!
     * The recommended distance between lines of text.
     */
    float m_height;
  };

  /*!
   * \brief
   * Metrics for scalable typefaces. The field
   * TypefaceMetricsBase::m_height is in font EM units.
   */
  class TypefaceMetricsScalable:public TypefaceMetricsBase
  {
  public:
    /*!
     * The typographical ascender value for the font in
     * font units; this is essentially the maximum across
     * all glyphs of the distance from the baseline to top
     * of the glyph.
     */
    float m_ascender;

    /*!
     * The typographical descender value for the font in
     * font units; this is essentially the minumum across
     * all glyphs of the signed distance from the baseline
     * to bottom of the glyph. A negative value indicates
     * below the baseline and a positive value above the
     * baseline.
     */
    float m_descender;

    /*!
     * The strike-through vertical position.
     */
    float m_strikeout_position;

    /*!
     * The underline vertical position.
     */
    float m_underline_position;

    /*!
     * The strike-through thickness.
     */
    float m_strikeout_thickness;

    /*!
     * The underline thickness.
     */
    float m_underline_thickness;

    /*!
     * The number of font units per EM for the glyphs.
     * The conversion from font coordinates to pixel
     * coordiantes is given by:
     * \code
     * PixelCoordinates = FontCoordinates * PixelSize / m_units_per_EM
     * \endcode
     */
    float m_units_per_EM;
  };

  /*!
   * \brief
   * Metrics for asupported pixel size of a non-scalable typeface.
   * The field TypefaceMetricsBase::m_height is in pixel units.
   */
  class TypefaceMetricsFixedSize:public TypefaceMetricsBase
  {
  public:
    /*!
     * The size in pixels for the metrics
     */
    float m_pixel_size;
  };

/*! @} */
}

#endif

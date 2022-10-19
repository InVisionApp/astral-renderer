/*!
 * \file font.hpp
 * \brief file font.hpp
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

#ifndef ASTRAL_FONT_HPP
#define ASTRAL_FONT_HPP

#include <astral/util/skew_parameters.hpp>
#include <astral/text/typeface.hpp>
#include <astral/text/glyph.hpp>

namespace astral
{
/*!\addtogroup Text
 * @{
 */

  /*!
   * \brief
   * An astral::Font represents how to draw glyphs from
   * an astral::Typeface. The properties of an astral::Font
   * are immutable.
   */
  class Font
  {
  public:
    /*!
     * Contruct a Font sourcing from the named typeface
     * using the specified pixel size
     * \param typeface astral::Typeface that generates the glyphs
     * \param pixel_size pixel size of the font.
     */
    Font(Typeface &typeface, float pixel_size);

    /*!
     * Conversion factor from coordinates of the
     * typeface (or typeface size) to coordinates
     * specified implicitely by pixel_size().
     */
    float
    scaling_factor(void) const
    {
      return m_scaling_factor;
    }

    /*!
     * Returns the pixel size as specified at teh ctor.
     */
    float
    pixel_size(void) const
    {
      return m_pixel_size;
    }

    /*!
     * Returns the astral::Typeface of the font.
     */
    Typeface&
    typeface(void) const
    {
      return *m_typeface;
    }

    /*!
     * Returns the base metrics of the underlying typeface
     * scaled to the size of the font.
     */
    const TypefaceMetricsBase&
    base_metrics(void) const
    {
      return m_metrics;
    }

    /*!
     * If Typeface::is_scalable() returns false, returns the
     * index into Typeface::fixed_metrics() used by the font.
     * Otherwise, returns -1.
     */
    int
    fixed_size_index(void) const
    {
      return m_fixed_size_index;
    }

    /*!
     * Returns the unscaled astral::GlyphMetrics value to use
     * from astral::Glyph::metrics().
     * \param glyph astral::Glyph to query. It is an error for
     *              astral::Glyph::typeface() to be different
     *              than typeface()
     * \param scale_metrics if non-null, location to which to write
     *                      the astral::GlyphMetrics scaled for this
     *                      astral::Font.
     */
    const GlyphMetrics&
    glyph_metrics(Glyph glyph, GlyphMetrics *scale_metrics = nullptr) const
    {
      return glyph_metrics(glyph, SkewParameters(), scale_metrics);
    }

    /*!
     * Returns the unscaled astral::GlyphMetrics value to use
     * from astral::Glyph::metrics().
     * \param glyph astral::Glyph to query. It is an error for
     *              astral::Glyph::typeface() to be different
     *              than typeface()
     * \param skew astral::SkewParameters to apply to scale_metrics
     * \param scale_metrics if non-null, location to which to write
     *                      the astral::GlyphMetrics scaled for this
     *                      astral::Font.
     */
    const GlyphMetrics&
    glyph_metrics(Glyph glyph, SkewParameters skew,
                  GlyphMetrics *scale_metrics = nullptr) const;

    /*!
     * Wrapper over Glyph::image_render_data() to select the correct
     * strike index.
     */
    reference_counted_ptr<const StaticData>
    image_render_data(RenderEngine &engine, Glyph glyph,
                      reference_counted_ptr<const Image> *out_image = nullptr) const;

  private:
    /* The Typeface of the font */
    reference_counted_ptr<Typeface> m_typeface;

    /* pixel size at which to render glyphs */
    float m_pixel_size;

    /* scaling factor to apply to go from glyph coordinates
     * to pixel coordinates:
     *   pixel_coordinate = glyph_coordinate * scaling_factor
     * If the Typeface::is_scalable() is true the glyph
     * coordianates are in the EM square otherwise they
     * are the coordinates at the size chosen by
     * m_fixed_size_index
     */
    float m_scaling_factor;

    /* if m_typeface is not scalable glyphs, then index into
     * Typeface::fixed_sizes() which size to use. A value of
     * -1 is fast check saying that the Typeface is scalable.
     */
    int m_fixed_size_index;

    TypefaceMetricsBase m_metrics;
  };

/*! @} */
}

#endif

/*!
 * \file glyph.hpp
 * \brief file glyph.hpp
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

#ifndef ASTRAL_GLYPH_HPP
#define ASTRAL_GLYPH_HPP

#include <astral/util/util.hpp>
#include <astral/util/reference_counted.hpp>
#include <astral/util/scale_translate.hpp>
#include <astral/text/glyph_index.hpp>
#include <astral/text/glyph_palette_id.hpp>
#include <astral/text/glyph_colors.hpp>
#include <astral/text/glyph_metrics.hpp>
#include <astral/renderer/render_enums.hpp>

namespace astral
{
  ///@cond
  class Path;
  class RenderEngine;
  class Image;
  class StaticData;
  class ItemPath;
  class Typeface;
  ///@endcond

/*!\addtogroup Text
 * @{
 */

  /*!
   * \brief
   * A Glyph represents a HANDLE to the data for rendering a glyph.
   */
  class Glyph
  {
  public:
    /*!
     * Default ctor initializes the glyph as invalid.
     */
    Glyph(void):
      m_private(nullptr)
    {}

    /*!
     * Copy ctor.
     */
    Glyph(const Glyph &glyph);

    /*!
     * Move ctor.
     */
    Glyph(Glyph &&glyph):
      m_private(glyph.m_private)
    {
      glyph.m_private = nullptr;
    }

    /*!
     * Assignment operator
     */
    Glyph&
    operator=(const Glyph &glyph)
    {
      Glyph g(glyph);
      swap(g);
      return *this;
    }

    /*!
     * Move assignment operator
     */
    Glyph&
    operator=(Glyph &&glyph);

    ~Glyph();

    /*!
     * Swap operator
     */
    void
    swap(Glyph &rhs)
    {
      Private *p(rhs.m_private);
      rhs.m_private = m_private;
      m_private = p;
    }

    /*!
     * Indicates the astral::Glyph refers to data.
     * If returns false, then the astral::Glyph is a
     * "null" glyph
     */
    bool
    valid(void) const
    {
      return m_private != nullptr;
    }

    /*!
     * Returns the astral::Typeface of the astral::Glyph
     */
    Typeface&
    typeface(void) const;

    /*!
     * Returns the astral::GlyphIndex of the astral::Glyph
     */
    GlyphIndex
    glyph_index(void) const;

    /*!
     * Returns true if and only if the glyph is a scalable glyph
     */
    bool
    is_scalable(void) const;

    /*!
     * Returns the metrics of the glyph when the glyph
     * is scalable. The GlyphMetrics values are in EM
     * units.
     */
    const GlyphMetrics&
    scalable_metrics(void) const;

    /*!
     * Returns the metrics of the glyph when the glyph
     * is not scalable for a particular strike_index
     * where strike_index in an index into
     * astral::Typeface::fixed_metrics().
     */
    const GlyphMetrics&
    fixed_metrics(unsigned int strike_index) const;

    /*!
     * Returns true if and only if the glyph is a colored glyph
     */
    bool
    is_colored(void) const;

    /*!
     * If the glyph is scalable and colored, returns the
     * colors or each layer under each paletted of the
     * glyph. If the glyph is not scalable or not colored,
     * returns nullptr.
     */
    const GlyphColors*
    colors(void) const;

    /*!
     * The transformation that maps [0, 1]x[0, 1] to the
     * coordinate system of the paths of the glyph when
     * the glyph is scalable. If the glyph is not scalable,
     * returns the identity transformation.
     */
    const ScaleTranslate&
    scale_translate(void) const;

    /*!
     * The astral::Path for the named layer of a glyph.
     * Only applicable if the glyph is a scalable glyph
     * Returns nullptr if the glyph is not scalable.
     * NOTE: the return value becomes invalid if the
     * glyph is de-cached.
     * \param layer which layer with 0 <= layers < colors()->number_layers()
     *              for colored glyphs. For non-colored glyphs, the value
     *              is ignored
     * \param out_fill_rule location to which to write the
     *                      astral::fill_rule_t applied to the path.
     * \param out_item_path if non-null, location to which to write
     *                        a reference to the astral::ItemPath
     *                        representing the layer.
     */
    const Path*
    path(unsigned int layer, enum fill_rule_t *out_fill_rule,
         reference_counted_ptr<const ItemPath> *out_item_path = nullptr) const;

    /*!
     * Returns the render data as packed by ItemPath::pack_data()
     * to render the glyph with the named palette. The data is so
     * that the fragment shader using the render data should be
     * fed [0, 1]x[0, 1]. If the glyph is not scalable, always
     * returns nullptr; if the glyph is non-colored but scalable the
     * astral::GlyphPaletteID argument is ignored.
     */
    reference_counted_ptr<const StaticData>
    render_data(RenderEngine &engine, GlyphPaletteID palette) const;

    /*!
     * If an astral::Glyph is not scalable, returns the
     * astral::StaticData to use for the specified strike-index
     * and optionally the underlying image as well.
     * \param engine astral::RenderEngine to use to realize the data
     * \param strike_index the strike-index to fetch where strike_index
     *                     is an index into astral::Typeface::fixed_metrics().
     * \param out_image if non-null, location to which to write
     *                  a reference to the image; the image is two
     *                  pixels larger in each dimension so that it
     *                  has one pixel of clear-black padding around it.
     *                  Shaders that use this image data should set the
     *                  image coordinates to start at (1, 1) and end at
     *                  astral::Image::size() - (1, 1), but use the entire
     *                  image to take advantage of the padding so that
     *                  image filtering at the boundary is fine.
     */
    reference_counted_ptr<const StaticData>
    image_render_data(RenderEngine &engine, unsigned int strike_index,
                      reference_counted_ptr<const Image> *out_image = nullptr) const;

  private:
    friend class Typeface;
    class Private;

    explicit
    Glyph(Private *p);

    Private *m_private;
  };

/*! @} */
}

#endif

/*!
 * \file text_item.hpp
 * \brief file text_item.hpp
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

#ifndef ASTRAL_TEXT_ITEM_HPP
#define ASTRAL_TEXT_ITEM_HPP

#include <astral/util/skew_parameters.hpp>
#include <astral/text/font.hpp>
#include <astral/renderer/render_data.hpp>
#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/combined_path.hpp>
#include <astral/renderer/shader/glyph_shader.hpp>

namespace astral
{
  ///@cond
  class RenderEngine;
  ///@endcond

/*!\addtogroup Text
 * @{
 */

  /*!
   * \brief
   * An astral::TextItem repreents a collection of astral::Glyph
   * values and positions at which to draw them.
   */
  class TextItem:public reference_counted<TextItem>::non_concurrent
  {
  public:
    /*!
     * Enumeration to decide how to handle scaling of glyphs
     * that come from non-scalable typefaces
     */
    enum image_glyph_handing_t
      {
        /*!
         * Use the image of the strike specified by the
         * astral::Font(). This means that on zoom the
         * image can be quite blurry even though the
         * typeface may support higher resolution images.
         */
        use_strike_as_indicated_by_font,

        /*!
         * Use the strike whose image size closest and no more
         * than that which appears on the screen  This means that
         * on zoom, the image used will be higher resolution if
         * the underlying typeface has higher resolution strikes.
         */
        use_nearest_strike,
      };

    /*!
     * Ctor.
     * \param font what astral::Font to use
     * \param handling only has impact if the typeface of font is
     *                 not scalable; determines how the TextItem
     *                 will choose what strike of the typeface
     *                 to use when drawn.
     */
    static
    reference_counted_ptr<TextItem>
    create(const Font &font, enum image_glyph_handing_t handling = use_nearest_strike)
    {
      return ASTRALnew TextItem(font, handling);
    }

    /*!
     * Ctor.
     * \param font what astral::Font to use
     * \param max_bitmap_pixel_size only has impact if the typeface of
     *                              font is not scalable; gives the max
     *                              value of pixel_size that the TextItem
     *                              will use for bitmap glyphs. Note that
     *                              this ctor implicitely implies that
     *                              \ref use_nearest_strike is used for
     *                              non-scalable glyphs.
     */
    static
    reference_counted_ptr<TextItem>
    create(const Font &font, float max_bitmap_pixel_size)
    {
      return ASTRALnew TextItem(font, max_bitmap_pixel_size);
    }

    ~TextItem();

    /*!
     * Clear the TextItem of all glyphs
     */
    void
    clear(void);

    /*!
     * Clear the TextItem of all glyphs and change the font
     */
    void
    clear(const Font &font);

    /*!
     * Add a single glyph.
     * \param glyph_index astral::GlyphIndex that picks the glyph
     * \param position the pen position in logical coordinate to place the glyph
     * \param P what palette of the glyph to use if the glyph is colored
     */
    void
    add_glyph(GlyphIndex glyph_index, vec2 position,
              GlyphPaletteID P = GlyphPaletteID())
    {
      add_glyphs(c_array<const GlyphIndex>(&glyph_index, 1),
                 c_array<const vec2>(&position, 1),
                 P);
    }

    /*!
     * Add a set of glyphs
     * \param glyph_indices astral::GlyphIndex values that pick the glyphs
     * \param glyph_positions the pen positions in logical coordinate to place the glyphs
     * \param P what palette of the glyphs to use if the glyphs are colored
     */
    void
    add_glyphs(c_array<const GlyphIndex> glyph_indices,
               c_array<const vec2> glyph_positions,
               GlyphPaletteID P = GlyphPaletteID());

    /*!
     * Add a set of glyphs
     * \param glyph_indices astral::GlyphIndex values that pick the glyphs
     * \param glyph_positions the pen positions in logical coordinate to place the glyphs
     * \param P what palette of the glyphs to use if the glyphs are colored
     */
    void
    add_glyphs(c_array<const GlyphIndex> glyph_indices,
               c_array<const float> glyph_positions,
               GlyphPaletteID P = GlyphPaletteID());

    /*!
     * Given a zoom factor, returns the index into Typeface::fixed_metrics()
     * used in the astral::RenderData returned by render_data(). If the
     * typeface of font() is not scalable, returns -1.
     */
    int
    strike_index(float zoom_factor) const;

    /*!
     * Create on demand the rendering data to render the text item
     * \param zoom_factor zooming factor applied to the drawing of the text
     *                    item. This value is used to select what strike from
     *                    a non-scalable typeface to use.
     * \param engine astral::RenderEngine from which to allocate vertices and indices
     * \param out_strike_index if non-null returns the strike used for non-scalable
     *                         glyphs. For scalable glyphs, will write the value -1.
     */
    const RenderData&
    render_data(float zoom_factor, RenderEngine &engine, int *out_strike_index = nullptr) const;

    /*!
     * Get the the named astral::Glyph and its pen position
     * that was added to this astral::TextItem
     * \param idx index of glyph added with 0 <= idx < number_glyphs()
     * \param out_glyph if non-null, location to which to write the astral::Glyph value
     * \param out_position if non-null, location to which to write the pen position
     * \param out_palette if non-null, location to which to write palette
     */
    void
    glyph(unsigned int idx, Glyph *out_glyph,
          vec2 *out_position, GlyphPaletteID *out_palette = nullptr) const;

    /*!
     * Returns the number of glyph added.
     */
    unsigned int
    number_glyphs(void) const;

    /*!
     * Returns the astral::Font of the astral::TextItem.
     */
    const Font&
    font(void) const
    {
      return m_font;
    }

    /*!
     * Returns the bounding box of the text item.
     */
    const BoundingBox<float>&
    bounding_box(void) const
    {
      return m_bb;
    }

    /*!
     * Fetch the paths of the glyphs of this astral::TextItem as an
     * array of astral::CombinedPath values keyed by astral::fill_rule_t.
     * \param out_paths location to which to write the astral::CombinedPath
     *                  values with the array keyed by astral::fill_rule_t.
     *                  Paths from color glyphs are NOT present. The written
     *                  value is invalid to use if clear(), add_glyph() or
     *                  add_glyphs() is called.
     * \param out_color_glyph_indices location to which to write an array of
     *                                indices to pass to glyph() to fetch the
     *                                color glyphs. The written value is invalid
     *                                if clear(), add_glyph() or add_glyphs() is
     *                                called.
     * \param out_scale_factor the scaling factor to apply to ALL values returned
     *                         in order to render the glyphs at the pixel size
     *                         specified by font().
     */
    enum return_code
    combined_paths(vecN<CombinedPath, number_fill_rule> *out_paths,
                   c_array<const unsigned int> *out_color_glyph_indices,
                   float *out_scale_factor) const;

  private:
    class PerGlyph;
    class GlyphElements;
    class ScalableGlyphElements;
    class ImageGlyphElements;
    class Helper;
    class PerRenderSize;

    TextItem(const Font &font, enum image_glyph_handing_t handling);
    TextItem(const Font &font, float max_bitmap_pixel_size);

    int
    compute_render_size_index(float zoom_factor) const;

    Font m_font;
    std::vector<PerGlyph> m_glyphs;
    BoundingBox<float> m_bb;
    vecN<std::vector<const Path*>, number_fill_rule> m_combined_path_backings;
    vecN<std::vector<vec2>, number_fill_rule> m_combined_path_translate_backings;
    std::vector<unsigned int> m_color_glyphs;

    /* Scalable typefaces have only one element in this
     * array where as non-scaleable typefaces have one
     * element per strike.
     */
    mutable std::vector<PerRenderSize> m_per_render_size;
  };

/*! @} */
}

#endif

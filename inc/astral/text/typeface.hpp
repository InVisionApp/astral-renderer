/*!
 * \file typeface.hpp
 * \brief file typeface.hpp
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

#ifndef ASTRAL_TYPEFACE_HPP
#define ASTRAL_TYPEFACE_HPP

#include <astral/util/util.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/reference_counted.hpp>
#include <astral/text/glyph.hpp>
#include <astral/text/glyph_generator.hpp>
#include <astral/text/typeface_metrics.hpp>
#include <astral/text/character_mapping.hpp>
#include <astral/renderer/image.hpp>
#include <astral/renderer/item_path.hpp>

namespace astral
{
/*!\addtogroup Text
 * @{
 */

  /*!
   * \brief
   * An astral::Typeface represents the typeface and style of
   * a font, essentially a face.
   */
  class Typeface:public reference_counted<Typeface>::non_concurrent
  {
  public:
    virtual
    ~Typeface()
    {}

    /*!
     * Create a Typeface object
     * \param generator object that generates the glyph data
     * \param params specifies how scalable glyphs are realized
     *               as astral::ItemPath values
     */
    static
    reference_counted_ptr<Typeface>
    create(reference_counted_ptr<const GlyphGenerator> generator,
           const ItemPath::GenerationParams &params);

    /*!
     * Create a Typeface object using default_item_path_params()
     * to specify how scalable glyphs are realized.
     * \param generator object that generates the glyph data
     */
    static
    reference_counted_ptr<Typeface>
    create(reference_counted_ptr<const GlyphGenerator> generator)
    {
      return create(generator, default_item_path_params());
    }

    /*!
     * Returns true if and only if the Typeface is
     * a scalable typeface
     */
    bool
    is_scalable(void) const;

    /*!
     * Returns the mertics of this astral::Typeface IF
     * the typeface is scalable. If is_scalable() returns
     * false, it is an error to call scalable_metrics().
     */
    const TypefaceMetricsScalable&
    scalable_metrics(void) const;

    /*!
     * May only be called if is_scalable() returns false.
     */
    c_array<const TypefaceMetricsFixedSize>
    fixed_metrics(void) const;

    /*!
     * The number of glyphs of the typeface
     */
    unsigned int
    number_glyphs(void) const;

    /*!
     * Fetch the astral::GlyphIndex value to use for
     * a character code
     * \param character_code character code
     */
    GlyphIndex
    glyph_index(uint32_t character_code) const;

    /*!
     * Fetch the astral::GlyphIndex value to use for
     * a set of character codes
     * \param character_codes array of character codes
     * \param glyph_indices location to which to write
     *                      the astral::GlyphIndex values
     * \tparam T type automatically convertible to uint32_t
     */
    template<typename T>
    void
    glyph_indices(c_array<const T> character_codes,
                  c_array<GlyphIndex> glyph_indices) const
    {
      ASTRALassert(character_codes.size() == glyph_indices.size());
      for (unsigned int i = 0; i < character_codes.size(); ++i)
        {
          glyph_indices[i] = glyph_index(character_codes[i]);
        }
    }

    /*!
     * Fetch an astral::Glyph from this astral::Typeface.
     * The returned handle is gauranteed to be valid as long
     * as this astral::Typeface is alive.
     */
    Glyph
    fetch_glyph(GlyphIndex glyph_code);

    /*!
     * Fetch a sequece of astral::Glyph values from this
     * astral::Typeface.
     */
    void
    fetch_glyphs(c_array<const GlyphIndex> glyph_indices,
                 c_array<Glyph> out_glyphs);

    /*!
     * Fetch a sequence of astral::Glyph values using
     * multiple threads. The use case is to spread the
     * CPU load of generating the glyph data across
     * many threads. This function can be called mutiple
     * times simutansously from -DIFFERENT- astral::Typeface
     * objects.
     */
    void
    fetch_glyphs_parallel(unsigned int number_threads,
                          c_array<const GlyphIndex> glyph_indices,
                          c_array<Glyph> out_glyphs);

    /*!
     * Specifies how scalable glyphs are realized as
     * astral::ItemPath values.
     */
    const ItemPath::GenerationParams&
    item_path_params(void) const;

    /*!
     * If a ItemPath::GenerationParams is not specified at
     * create(), then this value is used to determine how
     * scalable glyphs are realized as astral::ItemPath
     * values.
     */
    static
    const ItemPath::GenerationParams&
    default_item_path_params(void);

    /*!
     * Set the value returned by default_item_path_params(void)
     */
    static
    void
    default_item_path_params(const ItemPath::GenerationParams&);

  private:
    class Implement;
    friend class Glyph;

    Typeface(void)
    {}
  };

/*! @} */
}

#endif

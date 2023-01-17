/*!
 * \file glyph_generator.hpp
 * \brief file glyph_generator.hpp
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

#ifndef ASTRAL_GLYPH_GENERATOR_HPP
#define ASTRAL_GLYPH_GENERATOR_HPP

#include <astral/util/c_array.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/reference_counted.hpp>
#include <astral/util/color.hpp>
#include <astral/text/glyph_index.hpp>
#include <astral/text/glyph_colors.hpp>
#include <astral/text/glyph_palette_id.hpp>
#include <astral/text/glyph_metrics.hpp>
#include <astral/text/character_mapping.hpp>
#include <astral/text/typeface_metrics.hpp>
#include <astral/renderer/render_enums.hpp>

namespace astral
{
  ///@cond
  class Path;
  ///@endcond

/*!\addtogroup Text
 * @{
 */

  /*!
   * \brief
   * An astral::GlyphGenerator generates the data to be
   * used for glyph rendering
   */
  class GlyphGenerator:public reference_counted<GlyphGenerator>::non_concurrent
  {
  public:
    virtual
    ~GlyphGenerator()
    {}

    /*!
     * To be implemented by a derived class to return the
     * value N where there is a glyph for each I with
     * 0 <= I < N.
     */
    virtual
    uint32_t
    number_glyphs(void) const = 0;

    /*!
     * To be implemented by a derived class to fill
     * the mapping from character codes to glyph indices.
     * \param thread_slot which thread "slot", the value must
     *                    be with 0 <= thread_slot < number_threads().
     *                    For each S, it is guaranteed that at
     *                    any time T at most one thread will
     *                    have a call to scalable_glyph_info(),
     *                    fill_character_mapping() or fixed_glyph_info()
     *                    active with thread_slot equal to S
     * \param mapping CharacterMapping to which to add the values
     */
    virtual
    void
    fill_character_mapping(unsigned int thread_slot,
                           CharacterMapping &mapping) const = 0;

    /*!
     * To be implemented by a derived class to specify
     * the maximum number of concurrent threads the
     * generator can handle
     */
    virtual
    unsigned int
    number_threads(void) const = 0;

    /*!
     * To be implemented by a derived class to return
     * a pointer to an astral::TypefaceMetricsScalable
     * value. If the generator cannot generate scalable
     * glyphs, then returns nullptr.
     */
    virtual
    const TypefaceMetricsScalable*
    scalable_metrics(void) const = 0;

    /*!
     * To be implemented by a derived class to return
     * an array of astral::TypefaceMetricsFixedSize
     * values giving the sizes supported for generating
     * image data with image_size() and image_pixels().
     * If the generator does not fetch bitmaps, then
     * should return an empty array.
     */
    virtual
    c_array<const TypefaceMetricsFixedSize>
    fixed_metrics(void) const = 0;

    /*!
     * To be implemented by a derived class to return
     * the information about a glyph that is scalable
     * \param thread_slot which thread "slot", the value must
     *                    be with 0 <= thread_slot < number_threads().
     *                    For each S, it is guaranteed that at
     *                    any time T at most one thread will
     *                    have a call to scalable_glyph_info(),
     *                    fill_character_mapping() or fixed_glyph_info()
     *                    active with thread_slot equal to S
     * \param glyph_index with 0 <= glyph_index < number_glyphs()
     * \param out_metrics location to which to write the glyph
     *                    metrics in EM units
     * \param out_layer_colors location to which to write the
     *                         the color values for each layer
     *                         of each palette; colors are NOT
     *                         pre-multiplied by alpha. Making
     *                         this empty indicates that the glyph
     *                         is NOT colored.
     * \param out_paths location to which to write the astral::Path
     *                  values for each layer. If the glyph is not
     *                  colored, then a single path should be written
     *                  holding that single path
     * \param out_fill_rules location to which to write the fill rule
     *                       to apply to each path
     */
    virtual
    void
    scalable_glyph_info(unsigned int thread_slot,
                        GlyphIndex glyph_index,
                        GlyphMetrics *out_metrics,
                        GlyphColors *out_layer_colors,
                        std::vector<Path> *out_paths,
                        std::vector<enum fill_rule_t> *out_fill_rules) const = 0;

    /*!
     * To be implemented by a derived class to return
     * the information about a glyph that is not scalable
     * \param thread_slot which thread "slot", the value must
     *                    be with 0 <= thread_slot < number_threads().
     *                    For each S, it is guaranteed that at
     *                    any time T at most one thread will
     *                    have a call to scalable_glyph_info(),
     *                    fill_character_mapping() or fixed_glyph_info()
     *                    active with thread_slot equal to S
     * \param glyph_index with 0 <= glyph_index < number_glyphs()
     * \param size_idx index into metrics() to specify what size
     * \param out_metrics location to which to write the glyph metrics in
     *                    pixel size
     * \param out_image_size location to which to write the image size
     * \param out_pixels location to which to write the pixel values;
     *                   the values are to be in sRGB color space with
     *                   alpha pre-multiplied. Note that this is the same
     *                   as the CBDT of Truetype that provides pixels
     *                   in sRGB space with alpha pre-multiplied.
     * \returns true if and only if the glyph is a colored glyph.
     */
    virtual
    bool
    fixed_glyph_info(unsigned int thread_slot,
                     GlyphIndex glyph_index,
                     unsigned int size_idx,
                     GlyphMetrics *out_metrics,
                     ivec2 *out_image_size,
                     std::vector<FixedPointColor_sRGB> *out_pixels) const = 0;

    /*!
     * Returns (and creates on demand) a astral::GlyphGenerator
     * consisting of a single tofu-glyph.
     */
    static
    reference_counted_ptr<GlyphGenerator>
    tofu_generator(void);
  };

/*! @} */
}

#endif

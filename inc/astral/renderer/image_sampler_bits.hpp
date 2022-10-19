/*!
 * \file image_sampler_bits.hpp
 * \brief file image_sampler_bits.hpp
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

#ifndef ASTRAL_IMAGE_SAMPLER_BITS_HPP
#define ASTRAL_IMAGE_SAMPLER_BITS_HPP

#include <astral/renderer/render_enums.hpp>
#include <astral/util/util.hpp>
#include <astral/util/color.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * \brief
   * Namespace to encapsulate bit values that
   * appear in astral::ImageSampler.
   */
  namespace ImageSamplerBits
  {
    /*!
     * \brief
     * enumeration sequence giving what bits are used for what for both mask and color sampling
     */
    enum common_bit_values:uint32_t
      {
        /*!
         * Number of bits needed to encode astral::filter_t
         */
        filter_num_bits = 2,

        /*!
         * Number of bits needed to encode astral::mipmap_t
         */
        mipmap_num_bits = 2,

        /*!
         * Number of bits needed to encode the maximum LOD allowed.
         * Note that if the mipmap mode is astral::mipmap_choose,
         * the the chosen level mipmap is encoded instead of the
         * maximum LOD.
         */
        maximum_lod_num_bits = 4,

        /*!
         * Number of bits needed to encode an astral::tile_mode_t
         */
        tile_mode_num_bits = 3,

        /*!
         * Number of bits to encode number of padding texels to use
         * beyond the image boundaries when sampling.
         */
        numbers_texels_pre_padding_num_bits = 2,

        /*!
         * What bit that the encoding of astral::filter_t
         * starts at.
         */
        filter_bit0 = 0,

        /*!
         * What bit that the encoding of astral::mipmap_t
         * starts at.
         */
        mipmap_bit0 = filter_bit0 + filter_num_bits,

        /*!
         * What bit that the encoding of the maximum LOD level starts at.
         * Note that if the mipmap mode is astral::mipmap_choose,
         * the the chosen level mipmap is encoded instead of the
         * maximum LOD.
         */
        maximum_lod_bit0 = mipmap_bit0 + mipmap_num_bits,

        /*!
         * Bit0 to store the number of texels before the start of an
         * astral::TiledSubImage that can be used when sampling. The
         * simplest use case is for when the it is a proper sub-image
         * and one wants the texel outside of the sub-image to contribute
         * to filtering. The second use case if for when the sub-image
         * starts at the start of the astral::Image and one wants
         * the texels of its pre-padding to affect filtering.
         */
        numbers_texels_pre_padding_bit0 = maximum_lod_bit0 + maximum_lod_num_bits,

        /*!
         * What bit that the encoding of astral::tile_mode_t
         * in the x-direction starts at
         */
        x_tile_mode_bit0 = numbers_texels_pre_padding_bit0 + numbers_texels_pre_padding_num_bits,

        /*!
         * What bit that the encoding of astral::tile_mode_t
         * in the y-direction starts at
         */
        y_tile_mode_bit0 = x_tile_mode_bit0 + tile_mode_num_bits,

        number_common_bits = y_tile_mode_bit0 + tile_mode_num_bits
      };

    /*!
     * Bit enumerations for only color image sampling
     */
    enum color_bit_values:uint32_t
      {
        /*!
         * Number of bits needed to encode astral::colorspace_t
         */
        colorspace_num_bits = 1,

        /*!
         * Number of bits needed to encode astral::color_post_sampling_mode_t
         */
        color_post_sampling_mode_num_bits = 4,

        /*!
         * Bit0 to encode the astral::color_post_sampling_mode_t
         */
        color_post_sampling_mode_bit0 = number_common_bits,

        /*!
         * bit0 to encoder astral::colorspace_t of the color image
         */
        colorspace_bit0 = color_post_sampling_mode_bit0 + color_post_sampling_mode_num_bits,

        number_color_bits = colorspace_bit0 + colorspace_num_bits
      };

    /*!
     * Bit enumerations for only mask image sampling
     */
    enum mask_bit_values:uint32_t
      {
        /*!
         * Numer of bits needed to encode astral::mask_type_t
         */
        mask_type_num_bits = 1,

        /*!
         * Number of bits needed to encode astral::mask_channel_t
         */
        mask_channel_num_bits = 2,

        /*!
         * Numer of bits needed to encode astral::mask_post_sampling_mode_t
         */
        mask_post_sampling_mode_num_bits = 1,

        /*!
         * Bit0 to encode astral::mask_post_sampling_mode_t
         *
         * Note: we make the post-sampling mode bit0 the same for
         *       both color and mask so that empty ctor of \ref
         *       ImageSampler makes sense for both color and mask
         *       sampling.
         */
        mask_post_sampling_mode_bit0 = number_common_bits,

        /*!
         * Bit0 to encode astral::mask_type_t
         */
        mask_type_bit0 = mask_post_sampling_mode_bit0 + mask_post_sampling_mode_num_bits,

        /*!
         * Bit0 to encode astral::mask_channel_t
         */
        mask_channel_bit0 = mask_type_bit0 + mask_type_num_bits,

        /*!
         * Total number of bits needed
         */
        number_mask_bits = mask_channel_bit0 + mask_channel_num_bits
      };

    /*!
     * Enumeration to hold \ref number_bits
     */
    enum number_bits_t:uint32_t
      {
        /*!
         * The maximum number of bits needed to encode mask or color sampling
         */
        number_bits = (uint32_t(number_color_bits) > uint32_t(number_mask_bits)) ?
            uint32_t(number_color_bits) :
            uint32_t(number_mask_bits)
      };

    ///@cond
    ASTRALstatic_assert(uint32_t(number_bits) >= uint32_t(number_color_bits));
    ASTRALstatic_assert(uint32_t(number_bits) >= uint32_t(number_mask_bits));
    ///@endcond

    /*!
     * \brief
     * Enumeration to hold the bit-masks generated by ImageSamplerBits::common_bit_values,
     * ImageSamplerBits::color_bit_values and ImageSamplerBits::mask_bit_values
     */
    enum bit_mask_values:uint32_t
      {
        /*!
         * bit mask made from \ref filter_bit0 and \ref filter_num_bits.
         */
        filter_mask = ASTRAL_MASK(filter_bit0, filter_num_bits),

        /*!
         * bit mask made from \ref mipmap_bit0 and \ref mipmap_num_bits.
         */
        mipmap_mask = ASTRAL_MASK(mipmap_bit0, mipmap_num_bits),

        /*!
         * bit mask made from \ref maximum_lod_bit0 and \ref maximum_lod_num_bits.
         */
        maximum_lod_mask = ASTRAL_MASK(maximum_lod_bit0, maximum_lod_num_bits),

        /*!
         * bit mask made from \ref numbers_texels_pre_padding_bit0 and \ref numbers_texels_pre_padding_num_bits
         */
        numbers_texels_pre_padding_mask = ASTRAL_MASK(numbers_texels_pre_padding_bit0, numbers_texels_pre_padding_num_bits),

        /*!
         * bit mask made from \ref x_tile_mode_bit0 and \ref tile_mode_num_bits
         */
        x_tile_mode_mask = ASTRAL_MASK(x_tile_mode_bit0, tile_mode_num_bits),

        /*!
         * bit mask made from \ref y_tile_mode_bit0 and \ref tile_mode_num_bits
         */
        y_tile_mode_mask = ASTRAL_MASK(y_tile_mode_bit0, tile_mode_num_bits),

        /*!
         * bit mask made from \ref colorspace_bit0 and \ref colorspace_num_bits.
         */
        colorspace_mask = ASTRAL_MASK(colorspace_bit0, colorspace_num_bits),

        /*!
         * bit mask made from \ref color_post_sampling_mode_bit0 and \ref color_post_sampling_mode_num_bits
         */
        color_post_sampling_mode_mask = ASTRAL_MASK(color_post_sampling_mode_bit0, color_post_sampling_mode_num_bits),

        /*!
         * bit mask made from \ref mask_type_bit0 and \ref mask_type_num_bits
         */
        mask_type_mask = ASTRAL_MASK(mask_type_bit0, mask_type_num_bits),

        /*!
         * bit mask made from \ref mask_channel_bit0 and \ref mask_channel_num_bits.
         */
        mask_channel_mask = ASTRAL_MASK(mask_channel_bit0, mask_channel_num_bits),

        /*!
         * bit mask made from \ref mask_post_sampling_mode_bit0 and \ref mask_post_sampling_mode_num_bits
         */
        mask_post_sampling_mode_mask = ASTRAL_MASK(mask_post_sampling_mode_bit0, mask_post_sampling_mode_num_bits),
      };

    /*!
     * Return a value encoding the specified enums for sampling color data
     */
    inline
    uint32_t
    value(enum filter_t filter = filter_linear,
          enum mipmap_t mip = mipmap_ceiling,
          unsigned int max_lod = 0,
          enum colorspace_t encoding = colorspace_linear,
          enum color_post_sampling_mode_t post_sample_mode = color_post_sampling_mode_direct,
          unsigned int numbers_texels_pre_padding = 0u,
          enum tile_mode_t x_tile_mode = tile_mode_clamp,
          enum tile_mode_t y_tile_mode = tile_mode_clamp)
    {
      return pack_bits(colorspace_bit0, colorspace_num_bits, encoding)
        | pack_bits(filter_bit0, filter_num_bits, filter)
        | pack_bits(mipmap_bit0, mipmap_num_bits, mip)
        | pack_bits(maximum_lod_bit0, maximum_lod_num_bits, max_lod)
        | pack_bits(color_post_sampling_mode_bit0, color_post_sampling_mode_num_bits, post_sample_mode)
        | pack_bits(numbers_texels_pre_padding_bit0, numbers_texels_pre_padding_num_bits, numbers_texels_pre_padding)
        | pack_bits(x_tile_mode_bit0, tile_mode_num_bits, x_tile_mode)
        | pack_bits(y_tile_mode_bit0, tile_mode_num_bits, y_tile_mode);
    }

    /*!
     * Return a value encoding the specified enums for sampling mask data
     */
    inline
    uint32_t
    value(enum mask_channel_t channel,
          enum mask_type_t mask_type,
          enum filter_t filter = filter_linear,
          enum mipmap_t mip = mipmap_ceiling,
          unsigned int max_lod = 0,
          enum mask_post_sampling_mode_t post_sample_mode = mask_post_sampling_mode_direct,
          unsigned int numbers_texels_pre_padding = 0u,
          enum tile_mode_t x_tile_mode = tile_mode_decal,
          enum tile_mode_t y_tile_mode = tile_mode_decal)
    {
      return pack_bits(mask_channel_bit0, mask_channel_num_bits, channel)
        | pack_bits(mask_type_bit0, mask_type_num_bits, mask_type)
        | pack_bits(filter_bit0, filter_num_bits, filter)
        | pack_bits(mipmap_bit0, mipmap_num_bits, mip)
        | pack_bits(maximum_lod_bit0, maximum_lod_num_bits, max_lod)
        | pack_bits(mask_post_sampling_mode_bit0, mask_post_sampling_mode_num_bits, post_sample_mode)
        | pack_bits(numbers_texels_pre_padding_bit0, numbers_texels_pre_padding_num_bits, numbers_texels_pre_padding)
        | pack_bits(x_tile_mode_bit0, tile_mode_num_bits, x_tile_mode)
        | pack_bits(y_tile_mode_bit0, tile_mode_num_bits, y_tile_mode);
    }

    /*!
     * Return a value encoding the specified enums for sampling mask data
     */
    inline
    uint32_t
    value(enum mask_type_t mask_type,
          enum mask_channel_t mask_channel,
          enum filter_t filter = filter_linear,
          enum mipmap_t mip = mipmap_ceiling,
          unsigned int max_lod = 0,
          enum mask_post_sampling_mode_t post_sample_mode = mask_post_sampling_mode_direct,
          unsigned int numbers_texels_pre_padding = 0u,
          enum tile_mode_t x_tile_mode = tile_mode_decal,
          enum tile_mode_t y_tile_mode = tile_mode_decal)
    {
      return pack_bits(mask_channel_bit0, mask_channel_num_bits, mask_channel)
        | pack_bits(mask_type_bit0, mask_type_num_bits, mask_type)
        | pack_bits(filter_bit0, filter_num_bits, filter)
        | pack_bits(mipmap_bit0, mipmap_num_bits, mip)
        | pack_bits(maximum_lod_bit0, maximum_lod_num_bits, max_lod)
        | pack_bits(mask_post_sampling_mode_bit0, mask_post_sampling_mode_num_bits, post_sample_mode)
        | pack_bits(numbers_texels_pre_padding_bit0, numbers_texels_pre_padding_num_bits, numbers_texels_pre_padding)
        | pack_bits(x_tile_mode_bit0, tile_mode_num_bits, x_tile_mode)
        | pack_bits(y_tile_mode_bit0, tile_mode_num_bits, y_tile_mode);
    }

    /*!
     * Return a 32-bit packed value with the specified
     * srgb encoding value packed with other
     * fields in the bits unaffected.
     */
    inline
    uint32_t
    colorspace(uint32_t bits, enum colorspace_t v)
    {
      uint32_t f;

      f = pack_bits(colorspace_bit0, colorspace_num_bits, v);
      return (bits & ~colorspace_mask) | f;
    }

    /*!
     * Return the astral::colorspace_t value packed into a 32-bit value
     */
    inline
    enum colorspace_t
    colorspace(uint32_t bits)
    {
      uint32_t f;

      f = unpack_bits(colorspace_bit0, colorspace_num_bits, bits);
      return static_cast<enum colorspace_t>(f);
    }

    /*!
     * Return a 32-bit packed value with the specified
     * astral::mask_post_sampling_mode_t value packed with
     * other fields in the bits unaffected.
     */
    inline
    uint32_t
    color_post_sampling_mode(uint32_t bits, enum color_post_sampling_mode_t v)
    {
      uint32_t f;

      f = pack_bits(color_post_sampling_mode_bit0, color_post_sampling_mode_num_bits, v);
      return (bits & ~color_post_sampling_mode_mask) | f;
    }

    /*!
     * Return the astral::mask_post_sampling_mode_t value packed into
     * a 32-bit value
     */
    inline
    enum color_post_sampling_mode_t
    color_post_sampling_mode(uint32_t bits)
    {
      uint32_t f;

      f = unpack_bits(color_post_sampling_mode_bit0, color_post_sampling_mode_num_bits, bits);
      return static_cast<enum color_post_sampling_mode_t>(f);
    }

    /*!
     * Return a 32-bit packed value with the specified
     * astral::mask_channel_t value packed with other
     * fields in the bits unaffected.
     */
    inline
    uint32_t
    mask_channel(uint32_t bits, enum mask_channel_t v)
    {
      uint32_t f;

      f = pack_bits(mask_channel_bit0, mask_channel_num_bits, v);
      return (bits & ~mask_channel_mask) | f;
    }

    /*!
     * Return the astral::mask_channel_t value packed into a 32-bit value
     */
    inline
    enum mask_channel_t
    mask_channel(uint32_t bits)
    {
      uint32_t f;

      f = unpack_bits(mask_channel_bit0, mask_channel_num_bits, bits);
      return static_cast<enum mask_channel_t>(f);
    }

    /*!
     * Return a 32-bit packed value with the specified
     * astral::mask_post_sampling_mode_t value packed with
     * other fields in the bits unaffected.
     */
    inline
    uint32_t
    mask_post_sampling_mode(uint32_t bits, enum mask_post_sampling_mode_t v)
    {
      uint32_t f;

      f = pack_bits(mask_post_sampling_mode_bit0, mask_post_sampling_mode_num_bits, v);
      return (bits & ~mask_post_sampling_mode_mask) | f;
    }

    /*!
     * Return the astral::mask_post_sampling_mode_t value packed into
     * a 32-bit value
     */
    inline
    enum mask_post_sampling_mode_t
    mask_post_sampling_mode(uint32_t bits)
    {
      uint32_t f;

      f = unpack_bits(mask_post_sampling_mode_bit0, mask_post_sampling_mode_num_bits, bits);
      return static_cast<enum mask_post_sampling_mode_t>(f);
    }

    /*!
     * Return a 32-bit packed value with the specified
     * astral::mask_type_t value packed with other
     * fields in the bits unaffected.
     */
    inline
    uint32_t
    mask_type(uint32_t bits, enum mask_type_t v)
    {
      uint32_t f;

      f = pack_bits(mask_type_bit0, mask_type_num_bits, v);
      return (bits & ~mask_type_mask) | f;
    }

    /*!
     * Return the astral::mask_type_t value packed into a 32-bit value
     */
    inline
    enum mask_type_t
    mask_type(uint32_t bits)
    {
      uint32_t f;

      f = unpack_bits(mask_type_bit0, mask_type_num_bits, bits);
      return static_cast<enum mask_type_t>(f);
    }

    /*!
     * Return a 32-bit packed value with the specified
     * astral::filter_t value packed with other fields in
     * the bits unaffected.
     */
    inline
    uint32_t
    filter(uint32_t bits, enum filter_t v)
    {
      uint32_t f;

      f = pack_bits(filter_bit0, filter_num_bits, v);
      return (bits & ~filter_mask) | f;
    }

    /*!
     * Return the astral::filter_t value packed into a 32-bit value
     */
    inline
    enum filter_t
    filter(uint32_t bits)
    {
      uint32_t f;

      f = unpack_bits(filter_bit0, filter_num_bits, bits);
      return static_cast<enum filter_t>(f);
    }

    /*!
     * Return a 32-bit packed value with the specified
     * astral::mipmap_t value packed with other fields in
     * the bits unaffected.
     */
    inline
    uint32_t
    mipmap(uint32_t bits, enum mipmap_t v)
    {
      uint32_t f;

      f = pack_bits(mipmap_bit0, mipmap_num_bits, v);
      return (bits & ~mipmap_mask) | f;
    }

    /*!
     * Return the astral::mipmap_t value packed into a 32-bit value
     */
    inline
    enum mipmap_t
    mipmap(uint32_t bits)
    {
      uint32_t f;

      f = unpack_bits(mipmap_bit0, mipmap_num_bits, bits);
      return static_cast<enum mipmap_t>(f);
    }

    /*!
     * Return a 32-bit packed value with the specified
     * maximum LOD value packed with other fields in
     * the bits unaffected. Note that if mipmap(uint32_t)
     * returns astral::mipmap_chosen, then this sets what
     * mipmap level.
     */
    inline
    uint32_t
    maximum_lod(uint32_t bits, unsigned int v)
    {
      uint32_t f;

      f = pack_bits(maximum_lod_bit0, maximum_lod_num_bits, v);
      return (bits & ~maximum_lod_mask) | f;
    }

    /*!
     * Return the maximum LOD level value packed into a 32-bit value.
     * Note that if mipmap(uint32_t) returns astral::mipmap_chosen,
     * then this returns the chosen mipmap level.
     */
    inline
    unsigned int
    maximum_lod(uint32_t bits)
    {
      uint32_t f;

      f = unpack_bits(maximum_lod_bit0, maximum_lod_num_bits, bits);
      return f;
    }

    /*!
     * Return a 32-bit packed value with the specified
     * specified LOD value packed with other fields in
     * the bits unaffected.
     */
    inline
    uint32_t
    specified_lod(uint32_t bits, unsigned int v)
    {
      bits = mipmap(bits, astral::mipmap_chosen);
      return maximum_lod(bits, v);
    }

    /*!
     * Return a 32-bit packed value with the specified
     * number of pre-pafding texels to use with other
     * fields in the bits unaffected.
     */
    inline
    uint32_t
    numbers_texels_pre_padding(uint32_t bits, unsigned int v)
    {
      uint32_t f;
      f = pack_bits(numbers_texels_pre_padding_bit0, numbers_texels_pre_padding_num_bits, v);
      return (bits & ~numbers_texels_pre_padding_mask) | f;
    }

    /*!
     * Returns the number of pre-padding texels value
     * packed into a 32-bit value
     */
    inline
    unsigned int
    numbers_texels_pre_padding(uint32_t bits)
    {
      uint32_t f;

      f = unpack_bits(numbers_texels_pre_padding_bit0, numbers_texels_pre_padding_num_bits, bits);
      return f;
    }

    /*!
     * Return a 32-bit packed value with the specified
     * tile mode in the x-direction with other fields
     * in the bits unaffected.
     */
    inline
    uint32_t
    x_tile_mode(uint32_t bits, enum tile_mode_t v)
    {
      uint32_t f;
      f = pack_bits(x_tile_mode_bit0, tile_mode_num_bits, v);
      return (bits & ~x_tile_mode_mask) | f;
    }

    /*!
     * Returns tile mode in the x-direction value
     * packed into a 32-bit value
     */
    inline
    enum tile_mode_t
    x_tile_mode(uint32_t bits)
    {
      uint32_t f;

      f = unpack_bits(x_tile_mode_bit0, tile_mode_num_bits, bits);
      return static_cast<enum tile_mode_t>(f);
    }

    /*!
     * Return a 32-bit packed value with the specified
     * tile mode in the x-direction with other fields
     * in the bits unaffected.
     */
    inline
    uint32_t
    y_tile_mode(uint32_t bits, enum tile_mode_t v)
    {
      uint32_t f;
      f = pack_bits(y_tile_mode_bit0, tile_mode_num_bits, v);
      return (bits & ~y_tile_mode_mask) | f;
    }

    /*!
     * Returns tile mode in the x-direction value
     * packed into a 32-bit value
     */
    inline
    enum tile_mode_t
    y_tile_mode(uint32_t bits)
    {
      uint32_t f;

      f = unpack_bits(y_tile_mode_bit0, tile_mode_num_bits, bits);
      return static_cast<enum tile_mode_t>(f);
    }
  }

/*! @} */
}

#endif

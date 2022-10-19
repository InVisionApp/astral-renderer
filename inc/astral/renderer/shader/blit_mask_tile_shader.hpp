/*!
 * \file blit_mask_tile_shader.hpp
 * \brief file blit_mask_tile_shader.hpp
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

#ifndef ASTRAL_BLIT_MASK_TILE_SHADER_HPP
#define ASTRAL_BLIT_MASK_TILE_SHADER_HPP

#include <astral/renderer/image_sampler_bits.hpp>
#include <astral/renderer/shader/material_shader.hpp>
#include <astral/renderer/shader/item_data.hpp>
#include <astral/renderer/render_value.hpp>
#include <astral/renderer/image_sampler.hpp>
#include <astral/util/transformation.hpp>

namespace astral
{
  class RenderEngine;

  /*!
   * \brief
   * An astral::BlitMaskTileShader represents a
   * material shader to perform blitting a clip-in/clip-out
   * of two different render images against a single
   * tile of a single mask buffer. The shader assumes
   * that the brush coordinate fed to it are the coordinates
   * of the tile within the mask.
   *
   * The item data is packed as follows:
   *  - [0].x().u --> ImageMipElement::tile_location().x()
   *  - [0].y().u --> ImageMipElement::tile_location().y()
   *  - [0].z().u --> ImageMipElement::tile_index_atlas_location().xy packed with astral::pack_pair()
   *  - [0].w().u --> bit packing of filter, mask channels, mask type, ImageMipElement::tile_padding()
   *                  and ImageMipElement::tile_index_atlas_location().z. See
   *                  \ref BlitMaskTileShader::bit_packing_t.
   *  - [1]       --> astral::PackedImageMipElement of clip_in image data
   *  - [2]       --> astral::PackedImageMipElement of clip_out image data
   *  - [3].x().u --> RenderValue<ScaleTranslate> mapping from mask coordinates to clip-in src
   *  - [3].y().u --> RenderValue<ScaleTranslate> mapping from mask coordinates to clip-out src
   * .
   */
  class BlitMaskTileShader
  {
  public:
    /*!
     * \brief
     * Enumeration giving the offsets for how the item data is packed
     */
    enum item_data_packing:uint32_t
      {
        /*!
         * Number of item datas consumed.
         */
        item_data_size = 4,
      };

    /*!
     * Bit enumerations.
     */
    enum bit_packing_t:uint32_t
      {
        /*!
         * Number of bits to encode ImageMipElement::tile_index_atlas_location().z
         */
        tile_layer_num_bits = 8u,

        /*!
         * Number of bits to encode ImageMipElement::tile_padding()
         */
        tile_padding_num_bits = 2u,

        /*!
         * bit0 of ImageMipElement::tile_index_atlas_location().z
         */
        tile_layer_bit0 = 0u,

        /*!
         * bit0 of ImageMipElement::tile_padding()
         */
        tile_padding_bit0 = tile_layer_bit0 + tile_layer_num_bits,

        /*!
         * bit0 of the mask value type
         */
        mask_type_bit0 = tile_padding_bit0 + tile_padding_num_bits,

        /*
         * bit0 of the clip-in mask channel
         */
        mask_channel_bit0 = mask_type_bit0 + 1u,

        /*!
         * bit0 for the clip-out mask channel; this value
         * is only to be used for the \ref clip_combine_variant
         */
        optional_mask_channel_bit0 = mask_channel_bit0 + ImageSamplerBits::mask_channel_num_bits,

        /*!
         * bit0 of the filter applied to the mask
         */
        filter_bit0 = optional_mask_channel_bit0 + ImageSamplerBits::mask_channel_num_bits,
      };

    ///@cond
    ASTRALstatic_assert(filter_bit0 + ImageSamplerBits::filter_num_bits <= 32u);
    ///@endcond

    /*!
     * Enumeration to specify the variants of shaders within
     * an astral::BlitMaskTileShader
     */
    enum variant_t:uint32_t
      {
        /*!
         * The clip-out coverage value is the complement
         * of the clip-in value. This occurs when performing
         * RenderEncoderBase::begin_clip_node_pixel() against an
         * astral::MaskDetails object.
         */
        mask_details_variant = 0,

        /*!
         * The clip-out and clip-in coverage values are
         * read from seperate channels. It is possible that
         * their sum is less than one and thus the shader
         * must emit a partial coverage value.
         */
        clip_combine_variant,
      };

    /*!
     * Empty ctor, leaving the object wihout any shaders
     */
    BlitMaskTileShader(void)
    {}

    /*!
     * Ctor.
     * \param shaders shaders keyed by \ref variant_t
     */
    BlitMaskTileShader(const vecN<reference_counted_ptr<const MaterialShader>, 2> &shaders):
      m_shaders(shaders)
    {}

    /*!
     * Fetch the shader for a specified variant
     * \param v what variant
     */
    const MaterialShader&
    shader(enum variant_t v) const
    {
      return *m_shaders[v];
    }

    /*!
     * Pack item data the astral::MaterialShader of an
     * astral::BlitMaskTileShader accepts. It is assumed that
     * logical and mask coordinates are the same when an
     * astral::BlitMaskTileShader is used. In this variant,
     * the coverage of clip_out is the inverse of the coverage
     * of clip-in
     *
     * \param clip_in_transformation_mask transformation from mask coordinates to
     *                                    image coordinate of clipped-in content
     * \param clip_in_image iamge data of clipped-in content
     * \param clip_out_transformation_mask transformation from mask coordinates to
     *                                     image coordinate of clipped-out content
     * \param clip_out_image iamge data of clipped-out content
     * \param mask mask buffer
     * \param tile which tile from the mask
     * \param mask_type mask type (coverage or distance field)
     * \param mask_channel which channel from the mask to sample
     * \param filter filter to apply to mask texel fetches
     * \param dst location to which to write the packed data for the shader to consume
     */
    static
    void
    pack_item_data(RenderValue<ScaleTranslate> clip_in_transformation_mask,
                   const PackedImageMipElement &clip_in_image,
                   RenderValue<ScaleTranslate> clip_out_transformation_mask,
                   const PackedImageMipElement &clip_out_image,
                   const ImageMipElement &mask, uvec2 tile,
                   enum mask_type_t mask_type,
                   enum mask_channel_t mask_channel,
                   enum filter_t filter, c_array<gvec4> dst)
    {
      pack_item_data(clip_in_transformation_mask, clip_in_image,
                     clip_out_transformation_mask, clip_out_image,
                     mask, tile, mask_type,
                     mask_channel, mask_channel,
                     filter, dst);
    }

    /*!
     * Pack item data the astral::MaterialShader of an
     * astral::BlitMaskTileShader accepts. It is assumed that
     * logical and mask coordinates are the same when an
     * astral::BlitMaskTileShader is used. In this variant,
     * the coverage of clip_out and clip_in are on seperate
     * channels of the mask
     *
     * \param clip_in_transformation_mask transformation from mask coordinates to
     *                                    image coordinate of clipped-in content
     * \param clip_in_image iamge data of clipped-in content
     * \param clip_out_transformation_mask transformation from mask coordinates to
     *                                     image coordinate of clipped-out content
     * \param clip_out_image iamge data of clipped-out content
     * \param mask mask buffer
     * \param tile which tile from the mask
     * \param mask_type mask type (coverage or distance field)
     * \param clip_in_mask_channel channel to use for clip-in masking values
     * \param clip_out_mask_channel channel to use for clip-out masking values
     * \param filter filter to apply to mask texel fetches
     * \param dst location to which to write the packed data for the shader to consume
     */
    static
    void
    pack_item_data(RenderValue<ScaleTranslate> clip_in_transformation_mask,
                   const PackedImageMipElement &clip_in_image,
                   RenderValue<ScaleTranslate> clip_out_transformation_mask,
                   const PackedImageMipElement &clip_out_image,
                   const ImageMipElement &mask, uvec2 tile,
                   enum mask_type_t mask_type,
                   enum mask_channel_t clip_in_mask_channel,
                   enum mask_channel_t clip_out_mask_channel,
                   enum filter_t filter, c_array<gvec4> dst);

    /*!
     * Returns a astral::ItemDataValueMapping for the item data
     * of a astral::BlitMaskTileShader
     */
    static
    const ItemDataValueMapping&
    intrepreted_value_map(void);

  private:
    vecN<reference_counted_ptr<const MaterialShader>, 2> m_shaders;
  };
}

#endif

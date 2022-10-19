/*!
 * \file masked_rect_shader.hpp
 * \brief file masked_rect_shader.hpp
 *
 * Copyright 2020 by InvisionApp.
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

#ifndef ASTRAL_MASKED_RECT_SHADER_HPP
#define ASTRAL_MASKED_RECT_SHADER_HPP

#include <astral/util/rect.hpp>
#include <astral/util/transformation.hpp>
#include <astral/renderer/shader/item_shader.hpp>
#include <astral/renderer/shader/item_data.hpp>
#include <astral/renderer/render_value.hpp>
#include <astral/renderer/vertex_index.hpp>
#include <astral/renderer/vertex_data.hpp>
#include <astral/renderer/image.hpp>

namespace astral
{
  class RenderEngine;
  class RenderEncoderBase;

  /*!
   * \brief
   * An astral::MaskedRectShader is for drawing masked
   * mapped rectangles, i.e. rectangles that are masked and
   * not parallel to the current logical x-axis and y-axis.
   * The masking is NOT by an astral::Image, but instead
   * by a single tile of an astral::ImageMipElement.
   *
   * The packing of vertices is as follows:
   *  - Vertex::m_data[0].f --> x-relative position, value is 0 or 1
   *  - Vertex::m_data[1].f --> y-relative position, value is 0 or 1
   *  .
   * i.e. the same attribute data as astral::DynamicRectShader
   *
   * The ItemData is packed as:
   *  - [0].x().f --> min-x corner
   *  - [0].y().f --> min-y corner
   *  - [0].z().f --> max-x corner
   *  - [0].w().f --> max-y corner
   *  - [1].x().u --> ImageMipElement::tile_location().x()
   *  - [1].y().u --> ImageMipElement::tile_location().y()
   *  - [1].z().u --> ImageMipElement::tile_index_atlas_location().xy() packed with pack_pair()
   *  - [1].w().u --> packed value of T, B and Z where Z = ImageMipElement::tile_index_atlas_location().z()
   *                  of the tile and B is bits as found in astral::ImageSamplerBits and T is the value
   *                  of ImageMipElement::tile_padding(0).
   */
  class MaskedRectShader
  {
  public:
    /*!
     * Specified the bit-packing for packing the sampling, padding
     * and z-layer of the mask's tile.
     */
    enum bit_packing_t:uint32_t
      {
        /*!
         * Number of bits that \ref ImageSamplerBits value occupies
         */
        sampling_bits_num_bits = ImageSamplerBits::number_bits,

        /*!
         * Number of bits use to store ImageMipElement::tile_padding()
         */
        tile_padding_num_bits = 2,

        /*!
         * Number of bits that ImageMipElement::tile_index_atlas_location().z()
         * value occupies
         */
        tile_z_num_bits = 8,

        /*!
         * First bit that \ref ImageSamplerBits value occupies
         */
        sampling_bits_bit0 = 0,

        /*!
         * First bit that ImageMipElement::tile_padding() value occupies
         */
        tile_padding_bit0 = sampling_bits_bit0 + sampling_bits_num_bits,

        /*!
         * First bit that ImageMipElement::tile_index_atlas_location().z()
         */
        tile_z_bit0 = tile_padding_bit0 + tile_padding_num_bits,
      };

    /*!
     * \brief
     * Enumeration giving the offsets for how the item data is packed
     */
    enum item_data_packing:uint32_t
      {
        /*!
         * Number of item datas consumed.
         */
        item_data_size = 2,
      };

    /*!
     * Empty ctor, leaving the object wihout a shader
     */
    MaskedRectShader(void)
    {}

    /*!
     * Ctor giving it a shader.
     * \param sh shader of the created object
     */
    MaskedRectShader(const reference_counted_ptr<const ColorItemShader> &sh):
      m_shader(sh)
    {}

    /*!
     * Ctor giving it a shader.
     * \param sh shader of the created object
     */
    MaskedRectShader(const ColorItemShader *sh):
      m_shader(sh)
    {}

    /*!
     * Cast operator
     */
    operator const reference_counted_ptr<const ColorItemShader>&() const { return m_shader; }

    /*!
     * Cast operator
     */
    operator reference_counted_ptr<const ColorItemShader>&() { return m_shader; }

    /*!
     * overload operator*
     */
    const ColorItemShader& operator*(void) const { return *m_shader; }

    /*!
     * overload operator->
     */
    const ColorItemShader* operator->(void) const { ASTRALassert(m_shader); return m_shader.get(); }

    /*!
     * Return the pointer to the underlying shader
     */
    const ColorItemShader* get(void) const { return m_shader.get(); }

    /*!
     * Pack the item data that the \ref ColorItemShader of a \ref
     * MaskedRectShader accepts
     * \param mask the source of the tile by which to mask
     * \param tile which tile of the astral::ImageMipElement
     * \param bounds bounds within the mask to render.
     * \param post_sampling_mode post process, if any, to apply
     * \param mask_type how to interpret the value sampled
     * \param mask_channel what channel to sample
     * \param filter the filter to apply to the mask texels
     * \param dst location to which to write the item data
     */
    static
    Rect
    pack_item_data(const ImageMipElement &mask,
                   uvec2 tile, const Rect &bounds,
                   enum mask_post_sampling_mode_t post_sampling_mode,
                   enum mask_type_t mask_type,
                   enum mask_channel_t mask_channel,
                   enum filter_t filter, c_array<gvec4> dst);

    /*!
     * Conveniance overload that converts an astral::RectT to
     * an astral::Rect.
     * \param mask the source of the tile by which to mask
     * \param tile which tile of the astral::ImageMipElement
     * \param bounds bounds within the mask to render.
     * \param post_sampling_mode post process, if any, to apply
     * \param mask_type how to interpret the value sampled
     * \param mask_channel what channel to sample
     * \param filter the filter to apply to the mask texels
     * \param dst location to which to write the item data
     */
    template<typename T>
    static
    Rect
    pack_item_data(const ImageMipElement &mask,
                   uvec2 tile, const RectT<T> &bounds,
                   enum mask_post_sampling_mode_t post_sampling_mode,
                   enum mask_type_t mask_type,
                   enum mask_channel_t mask_channel,
                   enum filter_t filter, c_array<gvec4> dst)
    {
      return pack_item_data(mask, tile, Rect(bounds), post_sampling_mode, mask_type, mask_channel, filter, dst);
    }

  private:
    reference_counted_ptr<const ColorItemShader> m_shader;
  };
}

#endif

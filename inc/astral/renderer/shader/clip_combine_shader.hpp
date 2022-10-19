/*!
 * \file clip_combine_shader.hpp
 * \brief file clip_combine_shader.hpp
 *
 * Copyright 2021 by InvisionApp.
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

#ifndef ASTRAL_CLIP_COMBINE_SHADER_HPP
#define ASTRAL_CLIP_COMBINE_SHADER_HPP

#include <astral/util/util.hpp>
#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/shader/item_data.hpp>
#include <astral/renderer/shader/material_shader.hpp>

namespace astral
{
  class RenderEngine;

  /*!
   * \brief
   * An astral::ClipCombineShader is used to combine a mask M
   * with the creation of a mask F. It is to write to the
   * .b and .a channels the values of M as according to the
   * enumerations astral::image_blit_stc_mask_processing and
   * astral::image_blit_direct_mask_processing.
   *
   * The item is packed as follows:
   *   - [0].x().u x-coordinate of the texel in ImageAtlas::index_backing()
   *               that gives the min-min corner (and layer) of the tile
   *   - [0].y().u y-coordinate of the texel in ImageAtlas::index_backing()
   *               that gives the min-min corner (and layer) of the tile
   *   - [0].z().u tile size packed with astral::pack_pair()
   *   - [0].w().u layer and channel data, packed into a single
   *               integer as described by ClipCombineShader::tile_bits
   */
  class ClipCombineShader
  {
  public:
    /*!
     * Enumeration specifing how values are packed
     */
    enum item_data_packing:uint32_t
      {
        /*!
         * Number of item datas consumed.
         */
        item_data_size = 1,
      };

    /*!
     * Enumeration to describe the blitting mode
     */
    enum mode_t:uint32_t
      {
        /*!
         * Emit the complement of the coverage and distance
         * field values to the blue and alpha channels.
         */
        emit_complement_values_to_blue_alpha = 0,

        /*!
         * Emit the coverage and distance field values
         * to the blue and alpha channels.
         */
        emit_direct_values_to_blue_alpha,

        /*!
         * Emit the coverage and distance field values
         * to the red and green channels.
         */
        emit_direct_values_to_red_green,
      };

    /*!
     * Enumeration describing the bit packing to store the
     * channel sources and layer of the mask M.
     */
    enum tile_bits:uint32_t
      {
        /*!
         * Number of bits needed to specify a channel
         * via the enumeration astral::mask_channel_t
         */
        channel_num_bits = 2u,

        /*!
         * Number of bits needed to specify the mode
         * an enumerated by \ref mode_t
         */
        mode_num_bits = 2u,

        /*!
         * Number of bits to specify what layer in the
         * index atlas the index texel resides on.
         */
        tile_layer_num_bits = 8u,

        /*!
         * Bit0 where the layer in the index atlas the index
         * texel resides on.
         */
        tile_layer_bit0 = 0u,

        /*!
         * Bit0 where the channel holding the coverage
         * value is located
         */
        coverage_channel_bit0 = tile_layer_bit0 + tile_layer_num_bits,

        /*!
         * Bit0 where the channel holding the distance
         * value is located
         */
        distance_channel_bit0 = coverage_channel_bit0 + channel_num_bits,

        /*!
         * Bit0 where the mode is located
         */
        mode_bit0 = distance_channel_bit0 + channel_num_bits,

        /*!
         * If this bit is up, add ImageAtlas::tile_padding to the tile
         * location's .x and .y value read from the index texel
         */
        add_padding_bit = mode_bit0 + mode_num_bits,
      };

    /*!
     * Empty ctor, leaving the object wihout a shader
     */
    ClipCombineShader(void)
    {}

    /*!
     * Ctor giving it a shader.
     * \param sh shader of the created object
     */
    ClipCombineShader(const reference_counted_ptr<const MaskItemShader> &sh):
      m_shader(sh)
    {}

    /*!
     * Ctor giving it a shader.
     * \param sh shader of the created object
     */
    ClipCombineShader(const MaskItemShader *sh):
      m_shader(sh)
    {}

    /*!
     * Cast operator
     */
    operator const reference_counted_ptr<const MaskItemShader>&() const { return m_shader; }

    /*!
     * Cast operator
     */
    operator reference_counted_ptr<const MaskItemShader>&() { return m_shader; }

    /*!
     * overload operator*
     */
    const MaskItemShader& operator*(void) const { return *m_shader; }

    /*!
     * overload operator->
     */
    const MaskItemShader* operator->(void) const { ASTRALassert(m_shader); return m_shader.get(); }

    /*!
     * Return the pointer to the underlying shader
     */
    const MaskItemShader* get(void) const { return m_shader.get(); }

    /*!
     * Pack item data the astral::MaskItemShader of an
     * astral::ClipCombineShader accepts.
     * \param index_location texel coordinate in ImageAtlas::index_backing() that
     *                       gives the texel coordinate in ImageAtlas::color_backing()
     *                       of the min-min corner of the tile.
     * \param offset_by_tile_padding if true, offset the tile coordinate read by
     *                               (ImageAtlas::tile_padding, ImageAtlas::tile_padding).
     * \param tile_size size of the tile
     * \param mask_channels array enumerated by astral::mask_type_t
     *                      giving which channel for each astral::mask_type_t.
     *                      A value of astral::number_mask_channel indicates that
     *                      the source tile does not support that mask value type.
     * \param mode mode of the shader to run in
     * \param dst location to which to pack the data
     */
    static
    void
    pack_item_data(uvec3 index_location, bool offset_by_tile_padding, uvec2 tile_size,
                   vecN<enum mask_channel_t, number_mask_type> mask_channels,
                   enum mode_t mode, c_array<gvec4> dst)
    {
      uint32_t coverage_channel, distance_channel;

      /* we need to make sure the value does not exceed 3, an invalid value
       * indicates that the combine on that channel type does not matter,
       * so any vlaue is fine since the result will not be used.
       */
      coverage_channel = t_min(3u, static_cast<uint32_t>(mask_channels[mask_type_coverage]));
      distance_channel = t_min(3u, static_cast<uint32_t>(mask_channels[mask_type_distance_field]));

      ASTRALassert(dst.size() == item_data_size);
      dst[0].x().u = index_location.x();
      dst[0].y().u = index_location.y();
      dst[0].z().u = pack_pair(tile_size);
      dst[0].w().u = pack_bits(tile_layer_bit0, tile_layer_num_bits, index_location.z())
        | pack_bits(mode_bit0, mode_num_bits, mode)
        | pack_bits(coverage_channel_bit0, channel_num_bits, coverage_channel)
        | pack_bits(distance_channel_bit0, channel_num_bits, distance_channel)
        | pack_bits(add_padding_bit, 1u, offset_by_tile_padding ? 1u : 0u);
    }

  private:
    reference_counted_ptr<const MaskItemShader> m_shader;
  };
}

#endif

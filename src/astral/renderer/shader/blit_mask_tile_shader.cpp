/*!
 * \file blit_mask_tile_shader.cpp
 * \brief file blit_mask_tile_shader.cpp
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

#include <astral/renderer/shader/blit_mask_tile_shader.hpp>
#include <astral/renderer/render_engine.hpp>

namespace
{
  class InitedItemDataValueMapping
  {
  public:
    InitedItemDataValueMapping(void)
    {
      m_value
        .add(astral::ItemDataValueMapping::render_value_scale_translate,
             astral::ItemDataValueMapping::x_channel, 3)
        .add(astral::ItemDataValueMapping::render_value_scale_translate,
             astral::ItemDataValueMapping::y_channel, 3);
    }

    astral::ItemDataValueMapping m_value;
  };
}

const astral::ItemDataValueMapping&
astral::BlitMaskTileShader::
intrepreted_value_map(void)
{
  static const InitedItemDataValueMapping V;
  return V.m_value;
}

void
astral::BlitMaskTileShader::
pack_item_data(RenderValue<ScaleTranslate> clip_in_transformation_mask,
               const PackedImageMipElement &clip_in_image,
               RenderValue<ScaleTranslate> clip_out_transformation_mask,
               const PackedImageMipElement &clip_out_image,
               const ImageMipElement &mask, uvec2 tile,
               enum mask_type_t mask_type,
               enum mask_channel_t clip_in_mask_channel,
               enum mask_channel_t clip_out_mask_channel,
               enum filter_t filter, c_array<gvec4> dst)
{
  uvec2 tile_location;
  uvec3 tile_index_atlas_location;

  ASTRALassert(dst.size() == item_data_size);

  tile_location = mask.tile_location(tile);
  tile_index_atlas_location = mask.tile_index_atlas_location(tile);

  dst[0].x().u = tile_location.x();
  dst[0].y().u = tile_location.y();
  dst[0].z().u = pack_pair(tile_index_atlas_location.x(), tile_index_atlas_location.y());
  dst[0].w().u = pack_bits(tile_layer_bit0, tile_layer_num_bits, tile_index_atlas_location.z())
    | pack_bits(tile_padding_bit0, tile_padding_num_bits, mask.tile_padding(0))
    | pack_bits(mask_type_bit0, 1u, mask_type)
    | pack_bits(mask_channel_bit0, ImageSamplerBits::mask_channel_num_bits, clip_in_mask_channel)
    | pack_bits(optional_mask_channel_bit0, ImageSamplerBits::mask_channel_num_bits, clip_out_mask_channel)
    | pack_bits(filter_bit0, ImageSamplerBits::filter_num_bits, filter);

  clip_in_image.pack_item_data(&dst[1]);
  clip_out_image.pack_item_data(&dst[2]);

  dst[3].x().u = clip_in_transformation_mask.cookie();
  dst[3].y().u = clip_out_transformation_mask.cookie();
}

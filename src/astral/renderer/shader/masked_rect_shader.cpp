/*!
 * \file masked_rect_shader.cpp
 * \brief file masked_rect_shader.cpp
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

#include <astral/renderer/shader/masked_rect_shader.hpp>
#include <astral/renderer/render_engine.hpp>
#include <astral/renderer/renderer.hpp>

astral::Rect
astral::MaskedRectShader::
pack_item_data(const ImageMipElement &mask,
               uvec2 tile, const Rect &bounds,
               enum mask_post_sampling_mode_t post_sampling_mode,
               enum mask_type_t mask_type,
               enum mask_channel_t mask_channel,
               enum filter_t filter, c_array<gvec4> dst)
{
  uvec2 tile_location, tile_size, tile_max_corner;
  uvec3 atlas_location;
  uint32_t bits;

  tile_location = mask.tile_location(tile);
  tile_size = mask.tile_size(tile);
  atlas_location = mask.tile_index_atlas_location(tile);

  /* intersect the rect at tile_location with size tile_size
   * against the rect bounds
   */
  Rect R;
  Rect::compute_intersection(Rect()
                             .min_point(tile_location.x(), tile_location.y())
                             .size(tile_size.x(), tile_size.y()),
                             bounds, &R);

  dst[0].x().f = R.m_min_point.x();
  dst[0].y().f = R.m_min_point.y();
  dst[0].z().f = R.m_max_point.x();
  dst[0].w().f = R.m_max_point.y();

  dst[1].x().u = tile_location.x();
  dst[1].y().u = tile_location.y();
  dst[1].z().u = pack_pair(atlas_location.x(), atlas_location.y());

  bits = ImageSamplerBits::value(mask_channel, mask_type, filter, mipmap_none, 0, post_sampling_mode);
  dst[1].w().u = pack_bits(sampling_bits_bit0, sampling_bits_num_bits, bits)
    | pack_bits(tile_z_bit0, tile_z_num_bits, atlas_location.z())
    | pack_bits(tile_padding_bit0, tile_padding_num_bits, mask.tile_padding(0));


  return R;
}

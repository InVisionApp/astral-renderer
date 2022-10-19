/*!
 * \file render_engine_gl3_packing.cpp
 * \brief file render_engine_gl3_packing.cpp
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

#include <sstream>
#include <cstring>
#include <astral/util/gl/unpack_source_generator.hpp>
#include <astral/util/ostream_utility.hpp>
#include <astral/renderer/render_clip.hpp>
#include <astral/renderer/gl3/render_target_gl3.hpp>
#include "render_engine_gl3_shader_builder.hpp"
#include "render_engine_gl3_image.hpp"
#include "render_engine_gl3_backend.hpp"
#include "render_engine_gl3_packing.hpp"

////////////////////////////////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::Packing::ProcessedRenderClipElement methods
void
astral::gl::RenderEngineGL3::Implement::Packing::ProcessedRenderClipElement::
init(const astral::RenderClipElement &value)
{
  const MaskDetails *mask;
  const Image *image;
  const ImageMipElement *mip;
  unsigned int color_texels_per_root_texel;
  float root_texels_per_color_texel;
  uvec3 root_tile;
  ScaleTranslate atlas_transformation_mask;

  mask = value.mask_details();
  ASTRALassert(mask);

  image = mask->m_mask.get();
  ASTRALassert(image);
  ASTRALassert(!image->mip_chain().empty());

  mip = image->mip_chain().front().get();
  ASTRALassert(mip);

  root_tile = mip->root_tile_location();
  color_texels_per_root_texel = mip->ratio();
  root_texels_per_color_texel = 1.0f / static_cast<float>(color_texels_per_root_texel);

  m_bits = pack_bits(layer_bit0, layer_num_bits, root_tile.z())
    | pack_bits(num_index_levels_bit0, num_index_levels_bits, mip->number_index_levels())
    | pack_bits(mask_channel_bit0, mask_channel_num_bits, mask->m_mask_channel)
    | pack_bits(mask_type_bit0, mask_type_num_bits, mask->m_mask_type);

  /* First scale the translate to the location of the root tile */
  atlas_transformation_mask = ScaleTranslate(vec2(root_tile.x(), root_tile.y()))
    * ScaleTranslate(vec2(0.0f), vec2(root_texels_per_color_texel));

  m_atlas_transformation_pixel =
    atlas_transformation_mask // tranform to atlas coordinates
    * ScaleTranslate(mask->m_min_corner) // transform to coordinates of the entire mask
    * mask->m_mask_transformation_pixel; // transform from pixel coordiante to sub-rect of mask

  /* we want the bounding box in pixel coordinates */
  m_region = mask->pixel_rect().as_rect();
}

/////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::Packing::ProcessedImageSampler methods
void
astral::gl::RenderEngineGL3::Implement::Packing::ProcessedImageSampler::
init(const ImageSampler &value, ImageAtlas &atlas)
{
  c_array<generic_data> dst(m_packed_data);
  c_array<const reference_counted_ptr<const ImageMipElement>> mips(value.mip_chain(atlas));
  uint32_t bits, max_lod_plus_one;

  ASTRALassert(dst.size() == element_size(data_image));

  bits = value.bits();

  /* remap mipmap_none to mipmap_chosen with LOD = 0 */
  if (ImageSamplerBits::mipmap(bits) == mipmap_none)
    {
      bits = ImageSamplerBits::specified_lod(bits, 0u);
    }

  /* backend only supports up to image_max_mipmap_chain_length */
  if (mips.size() > image_max_mipmap_chain_length)
    {
      mips = mips.sub_array(0, image_max_mipmap_chain_length);
    }

  ASTRALassert(mips.size() <= image_max_mipmap_chain_length);
  max_lod_plus_one = ImageMipElement::maximum_number_of_mipmaps * mips.size();

  if (!mips.empty() && mips.back()->number_mipmap_levels() != ImageMipElement::maximum_number_of_mipmaps)
    {
      max_lod_plus_one -= ImageMipElement::maximum_number_of_mipmaps;
      max_lod_plus_one += mips.back()->number_mipmap_levels();
    }

  if (ImageSamplerBits::maximum_lod(bits) >= max_lod_plus_one)
    {
      bits = ImageSamplerBits::maximum_lod(bits, t_max(max_lod_plus_one, 1u) - 1u);
    }

  dst[image_min_corner_offset].u = pack_pair(value.min_corner());
  dst[image_size_offset].u = pack_pair(value.size());
  dst[image_bits_offset].u = bits;
  dst[image_mips_x_low_bits_offset].u = 0u;
  dst[image_mips_y_low_bits_offset].u = 0u;
  dst[image_mips_layers_offset].u = 0u;
  dst[image_mips_xy_high_and_num_index_levels_offset].u = 0u;

  if (!mips.empty())
    {
      uvec3 root_tile(mips.front()->root_tile_location());

      dst[image_root_tile_offset].u =
        pack_bits(ImageBacking::root_index_tile_number_levels_bit0, ImageBacking::root_index_tile_number_levels_num_bits, mips.front()->number_index_levels())
        | pack_bits(ImageBacking::x_bit0, ImageBacking::coord_num_bits, root_tile.x())
        | pack_bits(ImageBacking::y_bit0, ImageBacking::coord_num_bits, root_tile.y())
        | pack_bits(ImageBacking::layer_bit0, ImageBacking::root_index_tile_layer_num_bits, root_tile.z());

      mips = mips.sub_array(1);
      for (unsigned int m = 0, bit = 0u; m < mips.size() && m < 4u; ++m, bit += 8u)
        {
          uint8_t tmp;

          root_tile = mips[m]->root_tile_location();
          dst[image_mips_x_low_bits_offset].u |= ((root_tile.x() & 0xFF) << bit);
          dst[image_mips_y_low_bits_offset].u |= ((root_tile.y() & 0xFF) << bit);
          dst[image_mips_layers_offset].u |= ((root_tile.z() & 0xFF) << bit);

          tmp = pack_bits(image_root_num_index_levels_bit0, image_root_num_index_levels_bits, mips[m]->number_index_levels())
            | pack_bits(image_root_high_x_bit0, image_root_num_high_bits, root_tile.x() >> 8u)
            | pack_bits(image_root_high_y_bit0, image_root_num_high_bits, root_tile.y() >> 8u);

          dst[image_mips_xy_high_and_num_index_levels_offset].u |= (tmp << bit);
        }
    }
  else
    {
      dst[image_root_tile_offset].u = 0u;
    }
}

/////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::Packing methods
unsigned int
astral::gl::RenderEngineGL3::Implement::Packing::
element_size(enum RenderEngineGL3::data_t tp)
{
  static const unsigned int per_element_size[number_data_types] =
    {
      [data_header] = packed_data_header_size,
      [data_item_transformation] = packed_data_item_transformation_size,
      [data_item_scale_translate] = packed_data_item_scale_translate_size,
      [data_clip_window] = packed_data_clip_window_size,
      [data_brush] = packed_data_brush_size,
      [data_gradient] = packed_data_gradient_size,
      [data_gradient_transformation] = packed_data_gradient_transformation_size,
      [data_item_data] = 4u,
      [data_image] = packed_data_image_size,
      [data_shadow_map] = packed_shadow_map_size,
      [data_clip_mask] = packed_clip_mask_size,
    };

  ASTRALassert(tp < number_data_types);
  return per_element_size[tp];
}

unsigned int
astral::gl::RenderEngineGL3::Implement::Packing::
misc_buffer_size(void)
{
  return 8u;
}

void
astral::gl::RenderEngineGL3::Implement::Packing::
emit_unpack_code_texture(ShaderSource &dst)
{
  c_string offset_names[number_data_types] =
    {
      [data_header] = "astral_data_header_start",
      [data_item_transformation] = "astral_data_item_transformation_start",
      [data_item_scale_translate] = "astral_data_item_scale_translate_start",
      [data_clip_window] = "astral_data_clip_window_start",
      [data_brush] = "astral_data_brush_start",
      [data_gradient] = "astral_data_gradient_start",
      [data_gradient_transformation] = "astral_data_gradient_transformation_start",
      [data_item_data] = "astral_data_item_data_start",
      [data_image] = "astral_data_image_start",
      [data_shadow_map] = "astral_data_shadow_map_start",
      [data_clip_mask] = "astral_data_clip_mask_start",
    };

  uint32_t texture_width, texture_log2_width;

  texture_width = Backend::texture_dims_for_uniform_buffer();
  texture_log2_width = Backend::log2_texture_dims_for_uniform_buffer();

  /* make the UBO that backs the offsets */
  dst << "layout(std140) uniform AstralDataTextureOffsetUBO\n"
      << "{\n";
  for (unsigned int i = 0; i < number_data_types; ++i)
    {
      dst << "\tuint " << offset_names[i] << ";\n";
    }
  for (unsigned int i = number_data_types, endi = ASTRAL_ROUND_UP_MULTIPLE_OF4(number_data_types); i < endi; ++i)
    {
      dst << "\tuint padding" << i << ";\n";
    }

  dst << "};\n";

  /* declare the texture */
  dst << "uniform usampler2D astral_data_texture;\n";

  /* stream the generic read function */
  dst << "uvec4 astral_read_from_data_texture(uint location)\n"
      << "{\n"
      << "\tuvec2 loc;\n"
      << "\tloc.x = location & uint(" << texture_width - 1u << ");\n"
      << "\tloc.y = location >> uint(" << texture_log2_width << ");\n"
      << "\treturn texelFetch(astral_data_texture, ivec2(loc.x, loc.y), 0);\n"
      << "}\n\n";

  /* add the unpack code */
  for (unsigned int i = 0; i < number_data_types; ++i)
    {
      enum data_t tp;

      tp = static_cast<enum data_t>(i);
      if (tp != data_item_data)
        {
          emit_unpack_code(dst, true, offset_names[i], tp);
        }
    }
  emit_unpack_code_framebuffer_pixels(dst, true, offset_names[data_item_transformation]);

  /* we also need to provide the function astral_read_item_dataf()
   * and astral_read_item_datau().
   */
  dst << "uvec4\n"
      << "astral_read_item_datau(uint location)\n"
      << "{\n"
      << "\treturn astral_read_from_data_texture(location + " << offset_names[data_item_data] << ");\n"
      << "}\n";

  dst << "vec4\n"
      << "astral_read_item_dataf(uint location)\n"
      << "{\n"
      << "\treturn uintBitsToFloat(astral_read_item_datau(location));\n"
      << "}\n";
}

void
astral::gl::RenderEngineGL3::Implement::Packing::
emit_unpack_code_ubo(ShaderSource &dst)
{
  dst.add_source("astral_uniforms_ubo_typeless.glsl.resource_string", ShaderSource::from_resource);

  emit_unpack_code(dst, false, "astral_ubo_packed_headers", data_header);
  emit_unpack_code(dst, false, "astral_ubo_transformations", data_item_transformation);
  emit_unpack_code(dst, false, "astral_ubo_scale_translates", data_item_scale_translate);
  emit_unpack_code(dst, false, "astral_ubo_clip_window", data_clip_window);
  emit_unpack_code(dst, false, "astral_ubo_packed_brushes", data_brush);
  emit_unpack_code(dst, false, "astral_ubo_packed_gradients", data_gradient);
  emit_unpack_code(dst, false, "astral_ubo_gradient_transformations", data_gradient_transformation);
  emit_unpack_code(dst, false, "astral_ubo_packed_images", data_image);
  emit_unpack_code(dst, false, "astral_ubo_shadow_maps", data_shadow_map);
  emit_unpack_code(dst, false, "astral_ubo_clip_elements", data_clip_mask);
  emit_unpack_code_framebuffer_pixels(dst, false, "astral_ubo_transformations");
}

void
astral::gl::RenderEngineGL3::Implement::Packing::
emit_unpack_code_framebuffer_pixels(ShaderSource &dst, bool use_texture, c_string array_src)
{
  const enum data_t tp = data_item_transformation;

  UnpackSourceGenerator unpacker("AstralEmulateFramebufferFetch", element_size_blocks(tp));
  unpacker
    .set_float(framebuffer_pixels_matrix_packing_offset + 0u, ".m_matrix.m_value.x")
    .set_float(framebuffer_pixels_matrix_packing_offset + 1u, ".m_matrix.m_value.y")
    .set_float(framebuffer_pixels_matrix_packing_offset + 2u, ".m_matrix.m_value.z")
    .set_float(framebuffer_pixels_matrix_packing_offset + 3u, ".m_matrix.m_value.w")
    .set_float(framebuffer_pixels_translate_packing_offset + 0u, ".m_translation.x")
    .set_float(framebuffer_pixels_translate_packing_offset + 1u, ".m_translation.y")
    .set_uint(framebuffer_pixels_image_offset, ".m_image");

  std::ostringstream macro;
  if (use_texture)
    {
      macro << "astral_read_from_data_texture(uint(X) + uint(" << array_src << "))";
    }
  else
    {
      macro << array_src << "[X]";
    }

  dst.add_macro("astral_read(X)", macro.str().c_str());
  unpacker.stream_unpack_function(dst, "astral_load", "astral_read");
  dst.remove_macro("astral_read");
}

void
astral::gl::RenderEngineGL3::Implement::Packing::
emit_unpack_code(ShaderSource &dst, bool use_texture, c_string array_src, enum data_t tp)
{
  c_string glsl_typenames[number_data_types] =
    {
      [data_header] = "AstralPackedHeader",
      [data_item_transformation] = "AstralTransformation",
      [data_item_scale_translate] = "AstralScaleTranslate",
      [data_clip_window] = "AstralClipWindow",
      [data_brush] = "AstralPackedBrush",
      [data_gradient] = "AstralPackedGradient",
      [data_gradient_transformation] = "AstralGradientTransformation",
      [data_item_data] = nullptr,
      [data_image] = "AstralPackedImage",
      [data_shadow_map] = "AstralShadowMap",
      [data_clip_mask] = "AstralClipElement",
    };

  UnpackSourceGenerator unpacker(glsl_typenames[tp], element_size_blocks(tp));

  switch (tp)
    {
    case data_header:
      unpacker
        .set_uint(header_transformation_translate_packing_offset, ".m_transformation_and_scale_translate")
        .set_uint(header_material_shader_offset, ".m_material_shader")
        .set_uint(header_material_brush_and_data_offset, ".m_material_brush_and_data")
        .set_uint(header_item_data_material_transformation_offset, ".m_item_data_and_material_transformation")
        .set_uint(header_clip_mask_offset, ".m_clip_mask")
        .set_uint(header_item_shader_offset, ".m_item_shader")
        .set_uint(header_z_and_blend_shader_id_offset, ".m_bits")
        .set_uint(header_clip_window_and_framebuffer_copy, ".m_clip_window_and_framebuffer_copy");
      break;

    case data_item_transformation:
      unpacker
        .set_float(transformation_matrix_packing_offset + 0u, ".m_matrix.m_value.x")
        .set_float(transformation_matrix_packing_offset + 1u, ".m_matrix.m_value.y")
        .set_float(transformation_matrix_packing_offset + 2u, ".m_matrix.m_value.z")
        .set_float(transformation_matrix_packing_offset + 3u, ".m_matrix.m_value.w")
        .set_float(transformation_translate_packing_offset + 0u, ".m_translation.x")
        .set_float(transformation_translate_packing_offset + 1u, ".m_translation.y");
      break;

    case data_item_scale_translate:
      unpacker
        .set_float(translate_packing_offset + 0u, ".m_translate.x")
        .set_float(translate_packing_offset + 1u, ".m_translate.y")
        .set_float(scale_packing_offset + 0u, ".m_scale.x")
        .set_float(scale_packing_offset + 1u, ".m_scale.y");
      break;

    case data_clip_window:
      unpacker
        .set_float(clip_window_x_min_packing_offset, ".m_min_x")
        .set_float(clip_window_y_min_packing_offset, ".m_min_y")
        .set_float(clip_window_x_max_packing_offset, ".m_max_x")
        .set_float(clip_window_y_max_packing_offset, ".m_max_y");
      break;

    case data_gradient:
      unpacker
        .set_float(gradient_data_x_packing_offset, ".m_data.x")
        .set_float(gradient_data_y_packing_offset, ".m_data.y")
        .set_float(gradient_data_z_packing_offset, ".m_data.z")
        .set_float(gradient_data_w_packing_offset, ".m_data.w")
        .set_float(gradient_r0_packing_offset, ".m_start_radius")
        .set_float(gradient_r1_packing_offset, ".m_end_radius")
        .set_uint(gradient_colorstop_location_packing_offset, ".m_packed_bits.x")
        .set_uint(gradient_bits_layer_packing_offset, ".m_packed_bits.y");
      break;

      case data_gradient_transformation:
      unpacker
        .set_float(image_transformation_matrix_packing_offset + 0u, ".m_matrix.m_value.x")
        .set_float(image_transformation_matrix_packing_offset + 1u, ".m_matrix.m_value.y")
        .set_float(image_transformation_matrix_packing_offset + 2u, ".m_matrix.m_value.z")
        .set_float(image_transformation_matrix_packing_offset + 3u, ".m_matrix.m_value.w")
        .set_float(image_transformation_translate_packing_offset + 0u, ".m_translation.x")
        .set_float(image_transformation_translate_packing_offset + 1u, ".m_translation.y")
        .set_float(image_transformation_x_tile_begin_packing_offset, ".m_x_tile_begin")
        .set_float(image_transformation_x_tile_end_packing_offset, ".m_x_tile_end")
        .set_float(image_transformation_y_tile_begin_packing_offset, ".m_y_tile_begin")
        .set_float(image_transformation_y_tile_end_packing_offset, ".m_y_tile_end")
        .set_uint(image_transformation_x_tile_mode_packing_offset, ".m_x_tile_mode")
        .set_uint(image_transformation_y_tile_mode_packing_offset, ".m_y_tile_mode");
      break;

    case data_brush:
      unpacker
        .set_uint(brush_image_packing_offset, ".m_image_and_image_transformation")
        .set_uint(brush_gradient_packing_offset, ".m_gradient_and_gradient_transformation")
        .set_uint(brush_color_rg_packing_offset, ".m_color_rg")
        .set_uint(brush_color_ba_packing_offset, ".m_color_ba");
      break;

    case data_image:
      unpacker
        .set_uint(image_root_tile_offset, ".m_base.m_root_tile")
        .set_uint(image_min_corner_offset, ".m_base.m_min_corner")
        .set_uint(image_size_offset, ".m_base.m_size")
        .set_uint(image_bits_offset, ".m_base.m_sampler.m_bits")
        .set_uint(image_mips_x_low_bits_offset, ".m_mips_x_low_bits")
        .set_uint(image_mips_y_low_bits_offset, ".m_mips_y_low_bits")
        .set_uint(image_mips_layers_offset, ".m_mips_layers")
        .set_uint(image_mips_xy_high_and_num_index_levels_offset, ".m_mips_xy_high_and_num_index_levels");
      break;

    case data_shadow_map:
      unpacker
        .set_float(shadow_map_atlas_location_x_offset, ".m_atlas_location.x")
        .set_float(shadow_map_atlas_location_y_offset, ".m_atlas_location.y")
        .set_float(shadow_map_dimensions_offset, ".m_dimensions");
      break;

    case data_clip_mask:
      unpacker
        .set_float(processed_render_clip_element_translate_x, ".m_image_atlas_transformation_pixel.m_translate.x")
        .set_float(processed_render_clip_element_translate_y, ".m_image_atlas_transformation_pixel.m_translate.y")
        .set_float(processed_render_clip_element_scale_x, ".m_image_atlas_transformation_pixel.m_scale.x")
        .set_float(processed_render_clip_element_scale_y, ".m_image_atlas_transformation_pixel.m_scale.y")
        .set_float(processed_render_clip_element_region_min_x, ".m_pixel_clip_window.m_min_x")
        .set_float(processed_render_clip_element_region_min_y, ".m_pixel_clip_window.m_min_y")
        .set_float(processed_render_clip_element_region_max_x, ".m_pixel_clip_window.m_max_x")
        .set_float(processed_render_clip_element_region_max_y, ".m_pixel_clip_window.m_max_y");
      break;

    default:
      ASTRALassert(!"Bad value");
      return;
    }

  std::ostringstream macro;
  if (use_texture)
    {
      macro << "astral_read_from_data_texture(uint(X) + uint(" << array_src << "))";
    }
  else
    {
      macro << array_src << "[X]";
    }

  dst.add_macro("astral_read(X)", macro.str().c_str());
  unpacker.stream_unpack_function(dst, "astral_load", "astral_read");
  dst.remove_macro("astral_read");
}

void
astral::gl::RenderEngineGL3::Implement::Packing::
pack(c_array<generic_data> dst, const Header &value)
{
  ASTRALassert(dst.size() == element_size(data_header));

  dst[header_transformation_translate_packing_offset].u = pack_index_pair(value.m_transformation, value.m_translate);
  dst[header_material_brush_and_data_offset].u = pack_index_pair(value.m_material_brush, value.m_material_data);

  dst[header_item_data_material_transformation_offset].u = pack_pair(filter_location(value.m_item_data), filter_location(value.m_material_transformation));
  dst[header_clip_window_and_framebuffer_copy].u = pack_index_pair(value.m_clip_window, value.m_framebuffer_copy);

  dst[header_clip_mask_offset].u = pack_pair(filter_location(value.m_clip_mask), value.m_clip_mask_bits);

  dst[header_material_shader_offset].u = value.m_material_shader;
  dst[header_item_shader_offset].u = value.m_item_shader;

  dst[header_z_and_blend_shader_id_offset].u = pack_bits(header_z_bit0, header_z_num_bits,
                                                         t_min(value.m_z, (1u << header_z_num_bits) - 1u))
    | pack_bits(header_blend_shader_id_bit0, header_blend_shader_id_num_bits, value.m_blend_mode_shader_epilogue);
}

void
astral::gl::RenderEngineGL3::Implement::Packing::
pack(c_array<generic_data> dst, const Transformation &value)
{
  ASTRALassert(dst.size() == element_size(data_item_transformation));

  pack_matrix(dst.sub_array(transformation_matrix_packing_offset, 4), value.m_matrix);
  pack_vecN(dst.sub_array(transformation_translate_packing_offset, 2), value.m_translate);
}

void
astral::gl::RenderEngineGL3::Implement::Packing::
pack(c_array<generic_data> dst, const PackableEmulateFramebufferFetch &value)
{
  ASTRALassert(dst.size() == element_size(data_item_transformation));
  pack_matrix(dst.sub_array(framebuffer_pixels_matrix_packing_offset, 4), value.m_transformation.m_matrix);
  pack_vecN(dst.sub_array(framebuffer_pixels_translate_packing_offset, 2), value.m_transformation.m_translate);
  dst[framebuffer_pixels_image_offset].u = filter_location(value.m_image);
}

void
astral::gl::RenderEngineGL3::Implement::Packing::
pack(c_array<generic_data> dst, const ScaleTranslate &value)
{
  ASTRALassert(dst.size() == element_size(data_item_scale_translate));
  pack_vecN(dst.sub_array(translate_packing_offset, 2), value.m_translate);
  pack_vecN(dst.sub_array(scale_packing_offset, 2), value.m_scale);
}

void
astral::gl::RenderEngineGL3::Implement::Packing::
pack(c_array<generic_data> dst, const ClipWindow &value)
{
  ASTRALassert(dst.size() == element_size(data_clip_window));

  dst[clip_window_x_min_packing_offset].f = value.m_values.m_min_point.x();
  dst[clip_window_y_min_packing_offset].f = value.m_values.m_min_point.y();
  dst[clip_window_x_max_packing_offset].f = value.m_values.m_max_point.x();
  dst[clip_window_y_max_packing_offset].f = value.m_values.m_max_point.y();
}

void
astral::gl::RenderEngineGL3::Implement::Packing::
pack(c_array<generic_data> dst, const Gradient &value)
{
  ASTRALassert(dst.size() == element_size(data_gradient));

  dst[gradient_data_x_packing_offset].f = value.m_data.x();
  dst[gradient_data_y_packing_offset].f = value.m_data.y();
  dst[gradient_data_z_packing_offset].f = value.m_data.z();
  dst[gradient_data_w_packing_offset].f = value.m_data.w();
  dst[gradient_r0_packing_offset].f = value.m_r0;
  dst[gradient_r1_packing_offset].f = value.m_r1;

  ASTRALassert(value.m_colorstops);
  range_type<int> location(value.m_colorstops->location());

  /* pack start and size into dst[6u] */
  dst[gradient_colorstop_location_packing_offset].u = pack_pair(location.m_begin, location.m_end - location.m_begin);

  uint32_t gradient_bits;
  gradient_bits = pack_bits(gradient_type_bit0, gradient_type_num_bits, value.m_type)
    | pack_bits(gradient_interpolate_tile_mode_bit0, gradient_interpolate_tile_mode_num_bits, value.m_interpolate_tile_mode)
    | pack_bits(gradient_colorspace_bit0, gradient_colorspace_num_bits, value.m_colorstops->colorspace());

  dst[gradient_bits_layer_packing_offset].u = pack_pair(gradient_bits, value.m_colorstops->layer());
}

void
astral::gl::RenderEngineGL3::Implement::Packing::
pack(c_array<generic_data> dst, const GradientTransformation &value)
{
  ASTRALassert(dst.size() == element_size(data_gradient_transformation));

  pack_matrix(dst.sub_array(image_transformation_matrix_packing_offset, 4), value.m_transformation.m_matrix);
  pack_vecN(dst.sub_array(image_transformation_translate_packing_offset, 2), value.m_transformation.m_translate);

  /* x-tile range */
  dst[image_transformation_x_tile_begin_packing_offset].f = value.m_x_tile.m_begin;
  dst[image_transformation_x_tile_end_packing_offset].f = value.m_x_tile.m_end;

  /* y-tile range */
  dst[image_transformation_y_tile_begin_packing_offset].f = value.m_y_tile.m_begin;
  dst[image_transformation_y_tile_end_packing_offset].f = value.m_y_tile.m_end;

  /* x-tile range */
  dst[image_transformation_x_tile_mode_packing_offset].u = value.m_x_tile.m_mode;

  /* y-tile range */
  dst[image_transformation_y_tile_mode_packing_offset].u = value.m_y_tile.m_mode;
}

void
astral::gl::RenderEngineGL3::Implement::Packing::
pack(c_array<generic_data> dst, const PackableBrush &value)
{
  ASTRALassert(dst.size() == element_size(data_brush));

  dst[brush_image_packing_offset].u = pack_index_pair(value.m_image, value.m_image_transformation);
  dst[brush_gradient_packing_offset].u = pack_index_pair(value.m_gradient, value.m_gradient_transformation);

  /* We are going to use the sign bits to encode value.m_colorspace.
   * We can use that sign bit because the base color should be non-negative,
   * i.e. the sign bit should be down.
   */
  dst[brush_color_rg_packing_offset].u = pack_as_fp16(t_max(value.m_base_color.x(), 0.0f), t_max(value.m_base_color.y(), 0.0f));
  dst[brush_color_ba_packing_offset].u = pack_as_fp16(t_max(value.m_base_color.z(), 0.0f), t_max(value.m_base_color.w(), 0.0f));

  ASTRALassert((dst[brush_color_rg_packing_offset].u & ASTRAL_BIT_MASK(31u)) == 0u);
  ASTRALassert((dst[brush_color_rg_packing_offset].u & ASTRAL_BIT_MASK(15u)) == 0u);
  ASTRALassert((dst[brush_color_ba_packing_offset].u & ASTRAL_BIT_MASK(31u)) == 0u);
  ASTRALassert((dst[brush_color_ba_packing_offset].u & ASTRAL_BIT_MASK(15u)) == 0u);

  if (value.m_colorspace.first)
    {
      dst[brush_color_rg_packing_offset].u |= ASTRAL_BIT_MASK(brush_colorspace_specified_bit);
      dst[brush_color_rg_packing_offset].u |= pack_bits(brush_colorspace_bit, 1u, value.m_colorspace.second);
    }
}

void
astral::gl::RenderEngineGL3::Implement::Packing::
pack(c_array<generic_data> dst, const ProcessedImageSampler &value)
{
  ASTRALassert(dst.size() == element_size(data_image));
  std::copy(value.packed_data().begin(), value.packed_data().end(), dst.begin());
}

void
astral::gl::RenderEngineGL3::Implement::Packing::
pack(c_array<generic_data> dst, const ShadowMap &value)
{
  ASTRALassert(dst.size() == element_size(data_shadow_map));
  dst[shadow_map_atlas_location_x_offset].f = static_cast<float>(value.atlas_location().x());
  dst[shadow_map_atlas_location_y_offset].f = static_cast<float>(value.atlas_location().y());
  dst[shadow_map_dimensions_offset].f = static_cast<float>(value.dimensions());
}

void
astral::gl::RenderEngineGL3::Implement::Packing::
pack(c_array<generic_data> dst, const ProcessedRenderClipElement &p)
{
  ASTRALassert(dst.size() == element_size(data_clip_mask));

  dst[processed_render_clip_element_region_min_x].f = p.m_region.m_min_point.x();
  dst[processed_render_clip_element_region_min_y].f = p.m_region.m_min_point.y();
  dst[processed_render_clip_element_region_max_x].f = p.m_region.m_max_point.x();
  dst[processed_render_clip_element_region_max_y].f = p.m_region.m_max_point.y();
  dst[processed_render_clip_element_translate_x].f = p.m_atlas_transformation_pixel.m_translate.x();
  dst[processed_render_clip_element_translate_y].f = p.m_atlas_transformation_pixel.m_translate.y();
  dst[processed_render_clip_element_scale_x].f = p.m_atlas_transformation_pixel.m_scale.x();
  dst[processed_render_clip_element_scale_y].f = p.m_atlas_transformation_pixel.m_scale.y();
}

void
astral::gl::RenderEngineGL3::Implement::Packing::
pack_misc_buffer(c_array<generic_data> dst,
                 RenderEngineGL3 &engine,
                 RenderTarget &render_target)
{
  ASTRALassert(dst.size() == misc_buffer_size());
  vec2 dims(render_target.viewport_size());
  int number_layers;

  dst[misc_recip_half_viewport_width_packing_offset].f = 2.0f / dims.x();
  dst[misc_recip_half_viewport_height_packing_offset].f = 2.0f / dims.y();
  dst[misc_viewport_width_packing_offset].f = dims.x();
  dst[misc_viewport_height_packing_offset].f = dims.y();

  number_layers = engine.shadow_map_atlas().backing().height();
  dst[misc_shadow_map_recip_height_packing_offset].f = (number_layers != 0) ? 1.0f / float(number_layers) : 0.0f;

  number_layers = engine.colorstop_sequence_atlas().backing().number_layers();
  dst[misc_colorstop_recip_height_packing_offset].f = (number_layers != 0) ? 1.0f / float(number_layers) : 0.0f;

  RenderTargetGL *rt_gl;

  ASTRALassert(dynamic_cast<RenderTargetGL*>(&render_target));
  rt_gl = static_cast<RenderTargetGL*>(&render_target);

  dst[misc_clip_y_coeff_offset].f = (rt_gl->m_y_coordinate_convention == RenderTargetGL::pixel_y_zero_is_bottom) ? -1.0f : 1.0f;
}

void
astral::gl::RenderEngineGL3::Implement::Packing::
pack_item_data(c_array<generic_data> dst, c_array<const gvec4> data)
{
  c_array<const generic_data> src;
  src = data.flatten_array();

  ASTRALassert(src.size() == dst.size());
  ASTRALassert(dst.size() == 4u * data.size());
  std::memcpy(dst.c_ptr(), src.c_ptr(), sizeof(generic_data) * src.size());
}

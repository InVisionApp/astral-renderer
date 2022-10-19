/*!
 * \file render_engine_gl3_packing.hpp
 * \brief file render_engine_gl3_packing.hpp
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

#ifndef ASTRAL_RENDER_ENGINE_GL3_PACKING_HPP
#define ASTRAL_RENDER_ENGINE_GL3_PACKING_HPP

#include <cstring>
#include <astral/renderer/gl3/render_engine_gl3.hpp>
#include "render_engine_gl3_shader_builder.hpp"
#include "render_engine_gl3_implement.hpp"

class astral::gl::RenderEngineGL3::Implement::Packing
{
public:
  /*!
   * Enumeration to describe the packing of
   * a \ref Gradient type, tile mode and
   * color space. We have 16 bits of room.
   */
  enum gradient_bits_t:uint32_t
    {
      gradient_type_num_bits = 3,
      gradient_interpolate_tile_mode_num_bits = 3,
      gradient_colorspace_num_bits = ImageSamplerBits::colorspace_num_bits,

      gradient_type_bit0 = 0,
      gradient_interpolate_tile_mode_bit0 = gradient_type_bit0 + gradient_type_num_bits,
      gradient_colorspace_bit0 = gradient_interpolate_tile_mode_bit0 + gradient_interpolate_tile_mode_num_bits
    };

  enum invalid_render_index_t:uint32_t
    {
      /*!
       * indices into the data store buffers are only 16-bits
       * wide; the value of all bits up is sued indicate an
       * invalid index value.
       */
      invalid_render_index = 0xFFFFu
    };

  /*!
   * The values to fill an entry in the HeadersUBO,
   */
  class Header
  {
  public:
    /*!
     * Equality operator to enable packed header reuse
     */
    bool
    operator==(const Header &h) const
    {
      /* place the m_z test earliest as that is the one that
       * is most often different.
       */
      return m_z == h.m_z
        && m_transformation == h.m_transformation
        && m_translate == h.m_translate
        && m_material_brush == h.m_material_brush
        && m_material_shader == h.m_material_shader
        && m_material_data == h.m_material_data
        && m_material_transformation == h.m_material_transformation
        && m_item_data == h.m_item_data
        && m_clip_mask == h.m_clip_mask
        && m_clip_mask_bits == h.m_clip_mask_bits
        && m_item_shader == h.m_item_shader
        && m_clip_window == h.m_clip_window
        && m_framebuffer_copy == h.m_framebuffer_copy
        && m_blend_mode_shader_epilogue == h.m_blend_mode_shader_epilogue;
    }

    /*!
     * Inequality operator to enable packed header reuse
     */
    bool
    operator!=(const Header &h) const
    {
      return !operator==(h);
    }

    /*!
     * what transformation to apply
     */
    uint32_t m_transformation;

    /*!
     * what post-transformation translate to apply
     */
    uint32_t m_translate;

    /*!
     * the material shader
     */
    uint32_t m_material_shader;

    /*!
     * the brush of the material to apply
     */
    uint32_t m_material_brush;

    /*!
     * the shader-data to feed the material shader
     */
    uint32_t m_material_data;

    /*!
     * the transformation to apply to the material
     */
    uint32_t m_material_transformation;

    /*!
     * what shader-data item shader comsumes
     */
    uint32_t m_item_data;

    /*!
     * What clipping mask, if any, to apply
     */
    uint32_t m_clip_mask;

    /*!
     * additional bits for the clip mask
     */
    uint32_t m_clip_mask_bits;

    /*!
     * what item shader to use to draw
     */
    uint32_t m_item_shader;

    /*
     * The z-base value
     */
    uint32_t m_z;

    /*!
     * what (if any clip) equations to apply
     */
    uint32_t m_clip_window;

    /*!
     * transformation from pixel coordinates to framebuffer image pixels;
     * the index for the ImageSampler to use is held in the padding
     * of the transformation.
     */
    uint32_t m_framebuffer_copy;

    /*!
     * if the shader type is ItemShader::color_item_shader(),
     * ID for blend mode shader epilogue. Otherwise a value
     * from \ref mask_item_shader_clip_combine_t
     */
    uint32_t m_blend_mode_shader_epilogue;
  };

  /*!
   * A \ref Brush has within \ref RenderValue values,
   * which cannot be packed directly because their indices into
   * the corresponing UBO's are what is needed to pack. This struct
   * stores those indices. An index value of InvalidRenderValue
   * indicates that the source \ref RenderValue has that \ref
   * RenderValue::valid() is false.
   */
  class PackableBrush
  {
  public:
    PackableBrush(void):
      m_image(InvalidRenderValue),
      m_image_transformation(InvalidRenderValue),
      m_gradient(InvalidRenderValue),
      m_gradient_transformation(InvalidRenderValue),
      m_colorspace(false, colorspace_srgb)
    {}

    /*!
     * RenderValue<ImageSampler>::cookie()
     */
    uint32_t m_image;

    /*!
     * RenderValue<GradientTransformation>::cookie()
     */
    uint32_t m_image_transformation;

    /*!
     * RenderValue<Gradient>::cookie()
     */
    uint32_t m_gradient;

    /*!
     * RenderValue<GradientTransformation>::cookie()
     */
    uint32_t m_gradient_transformation;

    /* Brush::m_base_color with alpha pre-multiplied*/
    vec4 m_base_color;

    /* Brush::m_colorspace */
    std::pair<bool, enum colorspace_t> m_colorspace;
  };

  enum packing_sizes_t:uint32_t
    {
      packed_data_header_size = 8,
      packed_data_item_transformation_size = 8,
      packed_data_item_scale_translate_size = 4,
      packed_data_clip_window_size = 4,
      packed_data_brush_size = 4,
      packed_data_gradient_size = 8,
      packed_data_gradient_transformation_size = 12,
      packed_data_image_size = 8,
      packed_shadow_map_size = 4,
      packed_clip_mask_size = 8,
    };

  /*!
   * Gives the size in units of *\ref generic_data* for an element in the named buffer type
   */
  static
  unsigned int
  element_size(enum data_t tp);

  /*!
   * Gives the size in units of *\ref gvec4* for an element in the named buffer type
   */
  static
  unsigned int
  element_size_blocks(enum data_t tp)
  {
    unsigned int e;

    e = element_size(tp);
    ASTRALassert((e & 0x3u) == 0u);
    return e >> 2u;
  }

  /*!
   * Gives the size in units of *\ref generic_data* for the size of the MiscUBO
   */
  static
  unsigned int
  misc_buffer_size(void);

  enum header_packing_t:uint32_t
    {
      /*!
       * Offset at which to pack as a uint16-pair
       *  - [0]: Header::m_transformation
       *  - [1]: Header::m_translate
       */
      header_transformation_translate_packing_offset = 0u,

      /*!
       * Offset at which to pack as a uint16-pair
       *  - [0]: Header::m_material_brush
       *  - [1]: Header::m_material_data
       */
      header_material_brush_and_data_offset,

      /*!
       * Offset at which to pack as a uint16-pair
       *  - [0]: Header::m_item_data
       *  - [1]: Header::m_material_transformation
       */
      header_item_data_material_transformation_offset,

      /*!
       * Offset at which to pack as a uint16-pair
       *  - [0]: Header::m_clip_mask
       *  - [1]: Header::m_clip_mask_bits
       */
      header_clip_mask_offset,

      /*!
       * Offset at which to  as a uint16-pair
       *  - [0]: Header::m_clip_window
       *  - [1]: Header::m_framebuffer_copy
       */
      header_clip_window_and_framebuffer_copy,

      /*!
       * Offset at which to pack Header::m_material_shader
       */
      header_material_shader_offset,

      /*!
       * Offset at which to pack Header::m_item_shader
       */
      header_item_shader_offset,

      /*!
       * Offset at which to pack Header::m_z and
       * Header::m_blend_mode_shader_epilogue; bit
       * backing of the values is according to
       * header_packing_bits
       */
      header_z_and_blend_shader_id_offset,
    };

  /* Packing of z and blend-shader ID */
  enum header_packing_bits:uint32_t
    {
      header_z_num_bits = 24,
      header_blend_shader_id_num_bits = 8,

      header_z_bit0 = 0,
      header_blend_shader_id_bit0 = header_z_num_bits,
    };

  /*!
   * Pack a \ref to a destination buffer. The size of
   * dst must be the same as element_size(\ref data_header).
   */
  static
  void
  pack(c_array<generic_data> dst, const Header &value);

  enum transformation_packing_t:uint32_t
    {
      /*!
       * Offset at which to pack Transformation::m_matrix column-major
       */
      transformation_matrix_packing_offset = 0u,

      /*!
       * Offset at which to pack Transformation::m_translate
       */
      transformation_translate_packing_offset = transformation_matrix_packing_offset + 4u,
    };

  /*!
   * Pack a \ref Transformation to a destination buffer. The size of
   * dst must be the same as element_size(\ref data_item_transformation).
   */
  static
  void
  pack(c_array<generic_data> dst, const Transformation &value);

  class PackableEmulateFramebufferFetch
  {
  public:
    /*!
     * corresponds to the value behind EmulateFramebufferFetch::m_transformation
     */
    Transformation m_transformation;

    /*!
     * Corresponds to the index into the buffer backing holding the value
     * behind EmulateFramebufferFetch::m_image.
     */
    uint32_t m_image;
  };

  enum framebufffer_pixels_packing_t:uint32_t
    {
      /*!
       * Offset at which to pack PackableEmulateFramebufferFetch::m_transformation.m_matrix column-major
       */
      framebuffer_pixels_matrix_packing_offset = transformation_matrix_packing_offset,

      /*!
       * Offset at which to pack PackableEmulateFramebufferFetch::m_transformation.m_translate
       */
      framebuffer_pixels_translate_packing_offset = transformation_translate_packing_offset,

      /*!
       * Offset at which to pack PackableEmulateFramebufferFetch::m_image_sampler
       */
      framebuffer_pixels_image_offset = transformation_translate_packing_offset + 2u,
    };

  /*!
   * Pack a \ref PackableEmulateFramebufferFetch to a destination buffer. The size of
   * dst must be the same as element_size(\ref data_item_transformation).
   */
  static
  void
  pack(c_array<generic_data> dst, const PackableEmulateFramebufferFetch &value);

  enum scale_translate_packing_t:uint32_t
    {
      /*!
       * Offset at which to pack ScaleTranslate::m_translate
       */
      translate_packing_offset = 0u,

      /*!
       * Offset at which to pack ScaleTranslate::m_scale
       */
      scale_packing_offset = 2u,
    };

  /*!
   * Pack a \ref to a destination buffer. The size of
   * dst must be the same as element_size(\ref data_item_scale_translate).
   */
  static
  void
  pack(c_array<generic_data> dst, const ScaleTranslate &value);

  enum clip_window_packing_t:uint32_t
    {
      /*!
       * Packing offset at which to pack ClipWindow::m_values.m_min_point.x()
       */
      clip_window_x_min_packing_offset = 0u,

      /*!
       * Packing offset at which to pack ClipWindow::m_values.m_min_point.y()
       */
      clip_window_y_min_packing_offset,

      /*!
       * Packing offset at which to pack ClipWindow::m_values.m_max_point.x()
       */
      clip_window_x_max_packing_offset,

      /*!
       * Packing offset at which to pack ClipWindow::m_values.m_max_point.y()
       */
      clip_window_y_max_packing_offset,
    };

  /*!
   * Pack a \ref to a destination buffer. The size of
   * dst must be the same as element_size(\ref data_clip_window).
   */
  static
  void
  pack(c_array<generic_data> dst, const ClipWindow &value);

  enum gradient_packing_t:uint32_t
    {
      /*!
       * Packing offset at which to pack Gradient::m_data.x
       */
      gradient_data_x_packing_offset = 0u,

      /*!
       * Packing offset at which to pack Gradient::m_data.y
       */
      gradient_data_y_packing_offset,

      /*!
       * Packing offset at which to pack Gradient::m_data.z
       */
      gradient_data_z_packing_offset,

      /*!
       * Packing offset at which to pack Gradient::m_data.w
       */
      gradient_data_w_packing_offset,

      /*!
       * Packing offset at which to pack Gradient::m_r0
       */
      gradient_r0_packing_offset,

      /*!
       * Packing offset at which to pack Gradient::m_r1
       */
      gradient_r1_packing_offset,

      /*!
       * Packing offset at which to pack as a uint16-pair
       * - [0]: ColorStopSequence::location().m_begin
       * - [1]: ColorStopSequence::location().m_end - ColorStopSequence::location().m_begin
       */
      gradient_colorstop_location_packing_offset,

      /*!
       * Packing offset at which to pack as a uint16-pair
       * - [0]: Gradient::m_type, Gradient::m_interpolate_tile_mode and
       *        ColorStopSequence::colorspace() as according to \ref gradient_bits_t
       * - [1]: ColorStopSequence::layer()
       */
      gradient_bits_layer_packing_offset,
    };

  /*!
   * Pack a \ref to a destination buffer. The size of
   * dst must be the same as element_size(\ref data_gradient).
   */
  static
  void
  pack(c_array<generic_data> dst, const Gradient &value);

  enum image_transformation_packing_t:uint32_t
    {
      /*!
       * Packing offset at which to pack GradientTransformation::m_transformation.m_matrix
       */
      image_transformation_matrix_packing_offset = 0u,

      /*!
       * Packing offset at which to pack GradientTransformation::m_transformation.m_translate
       */
      image_transformation_translate_packing_offset = image_transformation_matrix_packing_offset + 4u,

      /*!
       * Packing offset at which to pack GradientTransformation::m_x_tile.m_begin
       */
      image_transformation_x_tile_begin_packing_offset = image_transformation_translate_packing_offset + 2u,

      /*!
       * Packing offset at which to pack GradientTransformation::GradientTransformation::m_x_tile.m_end
       */
      image_transformation_x_tile_end_packing_offset,

      /*!
       * Packing offset at which to pack GradientTransformation::m_y_tile.m_begin
       */
      image_transformation_y_tile_begin_packing_offset,

      /*!
       * Packing offset at which to pack GradientTransformation::GradientTransformation::m_y_tile.m_end
       */
      image_transformation_y_tile_end_packing_offset,

      /*!
       * Packing offset at which to pack GradientTransformation::GradientTransformation::m_x_tile.m_mode
       */
      image_transformation_x_tile_mode_packing_offset,

      /*!
       * Packing offset at which to pack GradientTransformation::GradientTransformation::m_y_tile.m_mode
       */
      image_transformation_y_tile_mode_packing_offset,
    };

  /*!
   * Pack a \ref to a destination buffer. The size of
   * dst must be the same as element_size(\ref data_gradient_transformation).
   */
  static
  void
  pack(c_array<generic_data> dst, const GradientTransformation &value);

  enum brush_packing_t:uint32_t
    {
      /*!
       * Packing offset at which to pack as a uint16-pair
       *  - [0]: PackableBrush::m_image
       *  - [1]: PackableBrush::m_image_transformation
       */
      brush_image_packing_offset = 0u,

      /*!
       * Packing offset at which to pack as a uint16-pair
       *  - [0]: PackableBrush::m_gradient
       *  - [1]: PackableBrush::m_gradient_transformation
       */
      brush_gradient_packing_offset,

      /*!
       * Packing offset at which to pack PackableBrush::m_base_color
       * red and green channels as an fp16x2 value
       */
      brush_color_rg_packing_offset,

      /*!
       * Packing offset at which to pack PackableBrush::m_base_color
       * blue and alpha channels as an fp16x2 value
       */
      brush_color_ba_packing_offset,
    };

  enum brush_bits_t:uint32_t
    {
      /*!
       * bit in the 32-bit value at brush_color_rg_packing_offset
       * that stores the value of PackableBrush::m_colorspace.first
       */
      brush_colorspace_specified_bit = 31u,

      /*!
       * bit in the 32-bit value at brush_color_rg_packing_offset
       * that stores the value of PackableBrush::m_colorspace.second
       */
      brush_colorspace_bit = 15u,
    };

  /*!
   * Pack a \ref to a destination buffer. The size of
   * dst must be the same as element_size(\ref data_brush).
   */
  static
  void
  pack(c_array<generic_data> dst, const PackableBrush &value);

  enum image_packing_t:uint32_t
    {
      /*!
       * Offset where the root tile, ImageSampler::root_tile() and
       * number of index levels, ImageSampler::number_index_levels(),
       * are packed as according to ImageBacking
       */
      image_root_tile_offset = 0u,

      /*!
       * Offset where the min-corner of the sub-image is packed
       * as a uint16-pair
       * - [0] : ImageSampler::min_corner().x()
       * - [1] : ImageSampler::min_corner().y()
       */
      image_min_corner_offset,

      /*!
       * Packing offset at which to pack ImageSampler::m_size
       * as a uint16-pair
       * - [0] : ImageSampler::size().x()
       * - [1] : ImageSampler::size().y()
       */
      image_size_offset,

      /*!
       * Packing offset at which to pack ImageSampler::m_bits
       */
      image_bits_offset,

      /*!
       * Packing offset for bits [0, 8) for mip[I] for 1 <= I <= 4
       * where mip[I] is ImageSampler::mip_chain()[I].
       *
       * The packing of bits is that the low x-bits for mip[I]
       * are packed in bits [8 * I, 8 + 8 * I).
       */
      image_mips_x_low_bits_offset,

      /*!
       * Packing offset for bits [0, 8) for mip[I] for 1 <= I <= 4
       * where mip[I] is ImageSampler::mip_chain()[I].
       *
       * The packing of bits is that the low y-bits for mip[I]
       * are packed in bits [8 * I, 8 + 8 * I).
       */
      image_mips_y_low_bits_offset,

      /*!
       * Packing offset for the layers for mip[I] for 1 <= I <= 4
       * where mip[I] is ImageSampler::mip_chain()[I].
       *
       * The packing of layer is that the layer for mip[I]
       * are packed in bits [8 * I, 8 + 8 * I).
       */
      image_mips_layers_offset,

      /*!
       * This is ickyier. The 32-bit integer is broken into 4 bytes as follows
       *  - bits [8 * I, 8 + 8 * I) holds values for mips[I] as follows
       *    - bits [0 + 8 * I, 2 + 8 * I) stores the number of index level
       *    - bits [2 + 8 * I, 5 + 8 * I) stores the high bits for x-root tile location
       *    - bits [5 + 8 * I, 8 + 8 * I) stores the high bits for y-root tile location
       */
      image_mips_xy_high_and_num_index_levels_offset,

      /* Enumerations to enable packing the bits at
       * image_mips_x_low_bits_offset and
       * image_mips_y_low_bits_offset
       */
      image_root_num_low_bits = 8u,

      /* Enumerations to enable packing the bits at
       * image_mips_xy_high_and_num_index_levels_offset
       */
      image_root_num_high_bits = 3u,
      image_root_num_index_levels_bits = 2u,
      image_root_num_index_levels_bit0 = 0,
      image_root_high_x_bit0 = image_root_num_index_levels_bit0 + image_root_num_index_levels_bits,
      image_root_high_y_bit0 = image_root_high_x_bit0 + image_root_num_high_bits,

      /* GL3 backend supports a mipmap chain up to 5 in length coming
       * from the base level plus 4 mip levels.
       */
      image_max_mipmap_chain_length = 5,
    };

  /* We need to generate the packing values for ImageSampler
   * as soon as it comes in because the underlying Image
   * object might be released before we need to pack the data.
   */
  class ProcessedImageSampler
  {
  public:
    void
    init(const ImageSampler &sampler, ImageAtlas &atlas);

    const vecN<generic_data, packed_data_image_size>&
    packed_data(void) const
    {
      return m_packed_data;
    }

  private:
    vecN<generic_data, packed_data_image_size> m_packed_data;
  };

  /*!
   * Pack a \ref ImageSampler to a destination buffer. The size of
   * dst must be the same as element_size(\ref data_image).
   */
  static
  void
  pack(c_array<generic_data> dst, const ProcessedImageSampler &value);

  enum shadow_map_packing_t:uint32_t
    {
      /*!
       * offset where ShadowMap::atlas_location().x() is packed.
       */
      shadow_map_atlas_location_x_offset,

      /*!
       * offset where ShadowMap::atlas_location().y() is packed.
       */
      shadow_map_atlas_location_y_offset,

      /*!
       * offset where ShadowMap::dimensions() is packed.
       */
      shadow_map_dimensions_offset,
    };

  /*!
   * Pack a \ref ShadowMap to a destination buffer. The size of
   * dst must be the same as element_size(\ref data_shadow_map).
   */
  static
  void
  pack(c_array<generic_data> dst, const ShadowMap &value);

  /*!
   * Class that represents a RenderClipElement heavily processed
   */
  class ProcessedRenderClipElement
  {
  public:
    enum bits_t:uint32_t
      {
        layer_num_bits = 8u,
        num_index_levels_bits = 2u,
        mask_channel_num_bits = 2u,
        mask_type_num_bits = 1u,
        filter_num_bits = 2u,

        layer_bit0 = 0u,
        num_index_levels_bit0 = layer_bit0 + layer_num_bits,
        mask_channel_bit0 = num_index_levels_bit0 + num_index_levels_bits,
        mask_type_bit0 = mask_channel_bit0 + mask_channel_num_bits,
        filter_bit0 = mask_type_bit0 + mask_type_num_bits,
        clip_out_bit = filter_bit0 + filter_num_bits
      };

    void
    init(const astral::RenderClipElement &value);

    static
    uint32_t
    additional_bits(enum filter_t filter, bool clip_out)
    {
      uint32_t return_value;

      return_value = (clip_out) ?
        (1u << clip_out_bit) :
        0u;

      return_value |= pack_bits(filter_bit0, filter_num_bits, filter);

      return return_value;
    }

    /* region in PIXEL coordinates that is within the sub-image
     * specified by RenderClipElement::mask_details().
     */
    Rect m_region;

    /* transformation from pixel coordinates to image atlas
     * xy-coordinatess
     */
    ScaleTranslate m_atlas_transformation_pixel;

    /* Bits to hold
     * - layer of root index tile (8-bits)
     * - number index levels (2-bits)
     * - mask channel (2-bits)
     * - mask type (1-bit)
     * - filter (2-bits)
     * - clip-in/clip-out (1-bit)
     * .
     * These values are NOT packed in the UBO holding the
     * PackableRenderClipElement; instead these bits are
     * in the header. Gotta save that 16-byte read (!)
     */
    uint32_t m_bits;
  };

  enum processed_render_clip_element_packing_t:uint32_t
    {
      processed_render_clip_element_region_min_x = 0,
      processed_render_clip_element_region_min_y,
      processed_render_clip_element_region_max_x,
      processed_render_clip_element_region_max_y,
      processed_render_clip_element_translate_x,
      processed_render_clip_element_translate_y,
      processed_render_clip_element_scale_x,
      processed_render_clip_element_scale_y,
    };

  /*!
   * pack the data of a \ref RenderClipElement
   */
  static
  void
  pack(c_array<generic_data> dst, const ProcessedRenderClipElement &p);

  enum misc_buffer_packing_t:uint32_t
    {
      /*!
       * Packing offset where 2.0 / viewport_width
       * is stored.
       */
      misc_recip_half_viewport_width_packing_offset = 0u,

      /*!
       * Packing offset where 2.0 / viewport_height
       * is stored.
       */
      misc_recip_half_viewport_height_packing_offset,

      /*!
       * Packing offset where viewport_width
       * is stored; packed as a float.
       */
      misc_viewport_width_packing_offset,

      /*!
       * Packing offset where viewport_height
       * is stored; packed as a float.
       */
      misc_viewport_height_packing_offset,

      /*!
       * Packing offset where the height of the
       * ShadowMapAtlas is stored; packed
       * as a float.
       */
      misc_shadow_map_recip_height_packing_offset,

      /*!
       * Packing offset where the reciprocal of the
       * width of the ColorStopSequenceAtlas is stored;
       * packed as a float.
       */
      misc_colorstop_recip_height_packing_offset,

      /*!
       * Packing offset that specifies a coefficient to multiply
       * the y-coordinate of gl_Position by. The value is as
       * follows
       *  -1 <---> y_convention == RenderTargetGL::pixel_y_zero_is_bottom
       *  +1 <---> y_convention == RenderTargetGL::pixel_y_zero_is_top
       */
      misc_clip_y_coeff_offset
    };

  /*!
   * Pack the misc-data derived from a \ref RenderTarget (and possibly)
   * other values.
   */
  static
  void
  pack_misc_buffer(c_array<generic_data> dst, RenderEngineGL3 &engine, RenderTarget &render_target);

  /*!
   * Pack a item data to a destination buffer. The size of
   * dst must be 4u * data.size()
   */
  static
  void
  pack_item_data(c_array<generic_data> dst, c_array<const gvec4> data);

  static
  void
  emit_unpack_code_ubo(ShaderSource &dst);

  static
  void
  emit_unpack_code_texture(ShaderSource &dst);

private:
  static
  inline
  uint32_t
  filter_location(uint32_t value)
  {
    return (value != InvalidRenderValue) ? value : invalid_render_index;
  }

  static
  inline
  uint32_t
  pack_index_pair(uint32_t v0, uint32_t v1)
  {
    v0 = filter_location(v0);
    v1 = filter_location(v1);

    ASTRALassert(v0 <= 0xFFFFu);
    ASTRALassert(v1 <= 0xFFFFu);

    return v0 | (v1 << 16u);
  }

  static
  inline
  void
  unpack_index_pair(uint32_t v, uint32_t *v0, uint32_t *v1)
  {
    *v0 = v & 0xFFFFu;
    *v1 = v >> 16u;

    if (*v0 == invalid_render_index)
      {
        *v0 = InvalidRenderValue;
      }

    if (*v1 == invalid_render_index)
      {
        *v1 = InvalidRenderValue;
      }
  }

  template<size_t N>
  static
  inline
  void
  pack_vecN(c_array<generic_data> dst, const vecN<float, N> &value)
  {
    ASTRALassert(dst.size() == N);
    std::memcpy(dst.c_ptr(), value.c_ptr(), sizeof(float) * N);
  }

  template<size_t N>
  static
  inline
  vecN<float, N>
  unpack_vecN(c_array<const generic_data> src)
  {
    vecN<float, N> return_value;

    ASTRALassert(src.size() == N);
    std::memcpy(return_value.c_ptr(), src.c_ptr(), sizeof(float) * N);
    return return_value;
  }

  template<size_t N, size_t M>
  static
  inline
  void
  pack_matrix(c_array<generic_data> dst, const matrix<N, M> &value)
  {
    ASTRALassert(dst.size() == N * M);
    pack_vecN(dst, value.raw_data());
  }

  template<size_t N, size_t M>
  static
  inline
  matrix<N, M>
  unpack_matrix(c_array<const generic_data> src)
  {
    matrix<N, M> R;
    R.raw_data() = unpack_vecN<N * M>(src);
    return R;
  }

  static
  void
  emit_unpack_code_framebuffer_pixels(ShaderSource &dst, bool use_texture, c_string array_src);

  static
  void
  emit_unpack_code(ShaderSource &dst, bool use_texture, c_string array_src, enum data_t);
};

#endif

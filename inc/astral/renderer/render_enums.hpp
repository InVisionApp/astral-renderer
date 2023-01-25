/*!
 * \file render_enums.hpp
 * \brief file render_enums.hpp
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

#ifndef ASTRAL_RENDER_ENUMS_HPP
#define ASTRAL_RENDER_ENUMS_HPP

#include <astral/util/util.hpp>
#include <astral/util/vecN.hpp>
#include <astral/renderer/backend/render_backend_enums.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * \brief
   * Enumeration to describe if and how caps are drawn when stroking.
   */
  enum cap_t:uint32_t
    {
      /*!
       * Indicates flat caps which essentially means no caps.
       */
      cap_flat,

      /*!
       * Indicates rounded caps.
       */
      cap_rounded,

      /*!
       * Indicates square caps which are rectangles added
       * to the start and end of each open contour of
       * length half the stroking width.
       */
      cap_square,

      number_cap_t,
    };

  /*!
   * \brief
   * Enumeration to describe if and how joins are drawn when stroking.
   */
  enum join_t:uint32_t
    {
      /*!
       * Indicates to draw rounded joins
       */
      join_rounded,

      /*!
       * Indicates to draw bevel joins
       */
      join_bevel,

      /*!
       * Indicates to draw miter joins
       */
      join_miter,

      /*!
       * Indicates to not draw any joins
       */
      join_none,

      /*!
       */
      number_join_t = join_none
    };

  /*!
   * \brief
   * Enumerations specifying common fill rules.
   */
  enum fill_rule_t:uint32_t
    {
      odd_even_fill_rule = 0, /*!< indicates to use odd-even fill rule */
      nonzero_fill_rule, /*!< indicates to use the non-zero fill rule */
      complement_odd_even_fill_rule, /*!< indicates to give the complement of the odd-even fill rule */
      complement_nonzero_fill_rule, /*!< indicates to give the complement of the non-zero fill rule */

      number_fill_rule /*!< count of enums */
    };

  /*!
   * Returns true if the fill rule is either
   * astral::complement_odd_even_fill_rule or
   * astral::complement_nonzero_fill_rule
   */
  inline
  bool
  fill_rule_is_complement_rule(enum fill_rule_t t)
  {
    return t >= complement_odd_even_fill_rule;
  }

  /*!
   * Returns the base of a fill_rule stripping away
   * the complement nature of it.
   */
  inline
  enum fill_rule_t
  base_fill_rule(enum fill_rule_t t)
  {
    return fill_rule_is_complement_rule(t) ?
      static_cast<enum fill_rule_t>(t - complement_odd_even_fill_rule):
      t;
  }

  /*!
   * Inverts a fill rule
   */
  inline
  enum fill_rule_t
  invert_fill_rule(enum fill_rule_t t)
  {
    return fill_rule_is_complement_rule(t) ?
      static_cast<enum fill_rule_t>(t - complement_odd_even_fill_rule):
      static_cast<enum fill_rule_t>(t + complement_odd_even_fill_rule);
  }

  /*!
   * Returns true if the fill rule applied to a
   * winding number indicates to fill.
   * \param f fill rule
   * \param w winding number
   */
  inline
  bool
  apply_fill_rule(enum fill_rule_t f, int w)
  {
    enum fill_rule_t bf;
    bool complement, return_value;

    bf = base_fill_rule(f);
    complement = fill_rule_is_complement_rule(f);
    return_value = (bf == odd_even_fill_rule) ?
      ((w & 1) != 0) :
      (w != 0);

    /* apply the necessary inversion which means xor
     * return_value against complement. The trick we
     * use is that (a xor b) is the same as (a != b).
     */
    return_value = (return_value != complement);

    return return_value;
  }

  /*!
   * \brief
   * Enumeration that describes image processing that is performed when
   * blitting data from an astral::ColorBuffer to an astral::Image.
   */
  enum image_blit_processing_t:uint32_t
    {
      /*!
       * The source astral::ColorBuffer is from rendering a mask for
       * STC path filling and that format is:
       * - .r should be 0.0 or 1.0 with 1.0 meaning pixel is covered
       *      or partially coverted and 0.0 meaning it is not.
       *      This channel is written to in the cover-pass of
       *      stencil-then-cover.
       * - .g stores 1.0 - D where D is the distance to the boundary
       *      between covered and not-covered in pixel units (clamped
       *      to [0, 1]). This distance value represents the distance
       *      to any edge, including false edges.
       * - .b stores 1 - C where C is a signed distance normalized to
       *      [0, 1] from another mask M
       * - .a stores 1 - C where C is a coverage value from another mask M
       * .
       * The processing will first compute a two-channel value F as follows:
       * - .r holds a coverage value that has had a post-processing
       *      applied to remove cancelling edges
       * - .g channel holds a signed distance value normalized to [0, 1] that
       *      has had a post-processing applied to remove cancelling edges
       * .
       * I.e., the channels of F give the dsitance field and coverage values
       * of a mask F. It will then combine the value of F with M to give
       * the final output value written to the the destination astral::Image
       * - .r M.r * F.r, i.e. the coverage value of the intersection of
       *      M and F.
       * - .g min(M.g, F.g), i.e. the normalized signed distance value of
       *      the intersection of M and F.
       * - .b M.r * (1.0 - F.r) the coverage value of the intersection of
       *      M and the complement of F.
       * - .a min(M.g, 1.0 - F.g), i.e. the normalized signed distance value of
       *      the intersection of M and the complement F.
       */
      image_blit_stc_mask_processing,

      /*!
       * The source astral::ColorBuffer is from rendering a mask directly
       * and that format is:
       * - .r stores a coverage value for a mask F
       * - .g stores a signed distance value normalized to [0, 1] for a mask F
       * - .b stores 1 - C where C is a signed distance normalized to
       *     [0, 1] from another mask M
       * - .a stores 1 - C where C is a coverage value from another mask M
       * .
       * The processing will combine the values to produce
       * - .a M.r * F.r, i.e. the coverage value of the intersection of
       *      M and F.
       * - .g min(M.g, F.g), i.e. the normalized signed distance value of
       *      the intersection of M and F.
       * - .b M.r * (1.0 - F.r) the coverage value of the intersection of
       *      M and the complement of F.
       * - .a min(M.g, 1.0 - F.g), i.e. the normalized signed distance value of
       *      the intersection of M and the complement F.
       * .
       */
      image_blit_direct_mask_processing,

      /*!
       * No processing of pixels takes place, i.e. the pixels are bitwise
       * copied from the source astral::ColorBuffer to the destination
       * astral::Image.
       */
      image_processing_none,

      image_processing_count,
    };

  /*!
   * Enumeration to specify image processing that is performed when
   * downsampling data from an astral::ColorBuffer to an astral::Image.
   */
  enum downsampling_processing_t:uint32_t
    {
      /*!
       * Indicates to directly blit the average of the four texels
       */
      downsampling_simple,

      downsampling_processing_count
    };

  /*!
   * \brief
   * Enumeration describing how to interpret a value v
   * into the range [0, 1] which is helpful for tiling.
   */
  enum tile_mode_t:uint32_t
    {
      /*!
       * Indicates to emit transparent-black outside of the
       * image; for gradients this means to emit transparent
       * black when the gradient interpolate is outside of
       * the range [0, 1]; for astral::TileRange::m_mode
       * values this means to NOT apply a repeat mode when
       * the value goes outside of range encoded in
       * astral::TileRange::m_begin and astral::TileRange::m_end
       */
      tile_mode_decal = 0u,

      /*!
       * Indicates to clamp within the image for images and
       * to clamp the interpolate to [0, 1] for gradients.
       */
      tile_mode_clamp,

      /*!
       * Indicates to mirror once and then clamp
       */
      tile_mode_mirror,

      /*!
       * Indicates to repeat.
       */
      tile_mode_repeat,

      /*!
       * Indicates to mirror-repeat.
       */
      tile_mode_mirror_repeat,

      tile_mode_number_modes,
    };

  /*!
   * \brief
   * Enumeration to describe blend modes supported by astral::Renderer.
   * The formula description for each of the PorterDuff blend modes is
   * for where the fragment shader emits pre-multipied by alpha color
   * values.
   */
  enum blend_mode_t:uint32_t
    {
      /*!
       * Porter-Duff clear mode. Letting S be the value from the
       * fragment shader and D be the current value in the framebuffer,
       * replaces the value in the framebuffer with F where
       * F.rgba = (0, 0, 0, 0).
       */
      blend_porter_duff_clear,

      /*!
       * Porter-Duff src mode. Letting S be the value from the
       * fragment shader and D be the current value in the framebuffer,
       * replaces the value in the framebuffer with F where F = S.
       */
      blend_porter_duff_src,

      /*!
       * Porter-Duff dst mode. Letting S be the value from the
       * fragment shader and D be the current value in the framebuffer,
       * replaces the value in the framebuffer with F = D.
       */
      blend_porter_duff_dst,

      /*!
       * Porter-Duff src-over mode. Letting S be the value from the
       * fragment shader and D be the current value in the framebuffer,
       * replaces the value in the framebuffer with F where F is:
       * \code
       * F.a = S.a + D.a * (1 - S.a)
       * F.rgb = S.rgb + D.rgb * (1 - S.a)
       * \endcode
       */
      blend_porter_duff_src_over,

      /*!
       * Porter-Duff dst-over mode. Letting S be the value from the
       * fragment shader and D be the current value in the framebuffer,
       * replaces the value in the framebuffer with F where F is:
       * \code
       * F.a = D.a + S.a * (1 - D.a)
       * F.rgb = D.rgb + S.rgb * (1 - D.a)
       * \endcode
       */
      blend_porter_duff_dst_over,

      /*!
       * Porter-Duff src-in mode. Letting S be the value from the
       * fragment shader and D be the current value in the framebuffer,
       * replaces the value in the framebuffer with F where F is:
       * \code
       * F.a = S.a * D.a
       * F.rgb = S.rgb * D.a
       * \endcode
       */
      blend_porter_duff_src_in,

      /*!
       * Porter-Duff dst-in mode. Letting S be the value from the
       * fragment shader and D be the current value in the framebuffer,
       * replaces the value in the framebuffer with F where F is
       * \code
       * F.a = S.a * D.a
       * F.rgb = D.rgb * S.a
       * \endcode
       */
      blend_porter_duff_dst_in,

      /*!
       * Porter-Duff  mode. Letting S be the value from the
       * fragment shader and D be the current value in the framebuffer,
       * replaces the value in the framebuffer with F where F is:
       * \code
       * F.a = S.a * (1 - D.a)
       * F.rgb =  S.rgb * (1 - D.a)
       * \endcode
       */
      blend_porter_duff_src_out,

      /*!
       * Porter-Duff src-out mode. Letting S be the value from the
       * fragment shader and D be the current value in the framebuffer,
       * replaces the value in the framebuffer with F where F is:
       * \code
       * F.a = D.a * (1.0 - S.a)
       * F.rgb = D.rgb * (1.0 - S.a)
       * \endcode
       */
      blend_porter_duff_dst_out,

      /*!
       * Porter-Duff src-atop mode. Letting S be the value from the
       * fragment shader and D be the current value in the framebuffer,
       * replaces the value in the framebuffer with F where F is:
       * \code
       * F.a = D.a
       * F.rgb = S.rgb * D.a + D.rgb * (1.0 - S.a)
       * \endcode
       */
      blend_porter_duff_src_atop,

      /*!
       * Porter-Duff dst-atop mode. Letting S be the value from the
       * fragment shader and D be the current value in the framebuffer,
       * replaces the value in the framebuffer with F where F is:
       * \code
       * F.a = S.a
       * F.rgb = D.rgb * S.a + S.rgb * (1 - D.a)
       * \endcode
       */
      blend_porter_duff_dst_atop,

      /*!
       * Porter-Duff xor mode. Letting S be the value from the
       * fragment shader and D be the current value in the framebuffer,
       * replaces the value in the framebuffer with F where F is:
       * \code
       * F.a = S.a * (1 - D.a) + D.a * (1 - S.a)
       * F.rgb = S.rgb * (1 - D.a) + D.rgb * (1 - S.a)
       * \endcode
       */
      blend_porter_duff_xor,

      /*!
       * Plus blend mode. Letting S be the value from the
       * fragment shader and D be the current value in the framebuffer,
       * replaces the value in the framebuffer with F where F is:
       * \code
       * F.a = S.a + D.a
       * F.rgb = S.rgb + D.rgb
       * \endcode
       */
      blend_porter_duff_plus,

      /*!
       * Modulate blend mode. Letting S be the value from the
       * fragment shader and D be the current value in the framebuffer,
       * replaces the value in the framebuffer with F where F is:
       * \code
       * F.a = S.a * D.a
       * F.rgb = S.rgb * D.rgb
       * \endcode
       */
      blend_porter_duff_modulate,

      /*!
       * Indicates to blend-max, i.e. the value written to the
       * render target is the max of the value emitted by the
       * shader and the value present in the render target.
       * NOTE: max is performed on whatever values are passed
       *       along, so in rendering a color buffer it maxes
       *       the values in pre-multiplied by alpha space.
       */
      blend_mode_max,

      /*!
       * Indicates to blend-min, i.e. the value written to the
       * render target is the min of the value emitted by the
       * shader and the value present in the render target
       * NOTE: min is performed on whatever values are passed
       *       along, so in rendering a color buffer it mins
       *       the values in pre-multiplied by alpha space.
       */
      blend_mode_min,

      /*!
       * Performs absolute value of difference respecting alpha
       * \code
       * F.a = S.a + D.a * (1 - S.a)
       * F.rgb = S.rgb + D.rgb - 2.0 * min(S.rgb * D.a, D.rgb * S.a)
       * \endcode
       */
      blend_mode_difference,

      /*!
       * Screen blend mode. Letting S be the value from the
       * fragment shader and D be the current value in the framebuffer,
       * replaces the value in the framebuffer with F where F is:
       * \code
       * F = S + D - S * D = S + (1 - S) * D
       * \endcode
       */
      blend_mode_screen,

      /*!
       * Multipliy blend mode. Letting S be the value from the
       * fragment shader and D be the current value in the framebuffer,
       * replaces the value in the framebuffer with F where F is:
       * \code
       * F = S * (1.0 - D.a) + D * (1.0 - S.a) + S * D
       * \endcode
       */
      blend_mode_multiply,

      /*!
       * Overlay blend mode which is astral::blend_mode_multiply
       * or astral::blend_mode_screen or astral::blend_mode_porter_duff_dst
       * depending on the value already in the framebuffer
       */
      blend_mode_overlay,

      /*!
       * Darken blend mode. Letting S be the value from the
       * fragment shader and D be the current value in the framebuffer,
       * replaces the value in the framebuffer with F where F is:
       * \code
       * F.a = S.a + (1.0 - S.a) * D.a
       * F.rgb = S + D - max(S * D.a, D * s.a)
       * \endcode
       */
      blend_mode_darken,

      /*!
       * Darken blend mode. Letting S be the value from the
       * fragment shader and D be the current value in the framebuffer,
       * replaces the value in the framebuffer with F where F is:
       * \code
       * F.a = S.a + (1.0 - S.a) * D.a
       * F.rgb = S + D - min(S * D.a, D * s.a)
       * \endcode
       */
      blend_mode_lighten,

      /*!
       * Color dodge blend mode, brightens the value of F from D
       * by an amount derived from the value S
       */
      blend_mode_color_dodge,

      /*!
       * Color burn blend mode, darkens the value of F from D
       * by an amount derived from the value S
       */
      blend_mode_color_burn,

      /*!
       * Hardlight blend mode which is astral::blend_mode_multiply
       * or astral::blend_mode_screen depending on the value emitted
       * by the fragment shader
       */
      blend_mode_hardlight,

      /*!
       * Hardlight blend mode which is astral::blend_mode_lighten
       * or astral::blend_mode_darken depending on the value emitted
       * by the fragment shader
       */
      blend_mode_softlight,

      /*!
       * Exclusion blend mode. Letting S be the value from the
       * fragment shader and D be the current value in the framebuffer,
       * replaces the value in the framebuffer with F where F is:
       * \code
       * F.a = S.a + (1.0 - S.a) * D.a
       * F.rgb = S.rgb + D.rgb - 2.0 * S.rgb * D.rgb
       *       = (1.0 - D.rgb) * S.srb + (1.0 - S.rgb) * D.rgb
       * \endcode
       */
      blend_mode_exclusion,

      /*!
       * Hue of the value emitted by the fragment shader, but take the
       * saturation and luminosity of the value in the framebuffer
       */
      blend_mode_hue,

      /*!
       * Saturation of the value emitted by the fragment shader, but
       * take the hue and luminosity of the value in the framebuffer
       */
      blend_mode_saturation,

      /*!
       * Saturation and hue of the value emitted by the fragment shader,
       * but take the luminosity of the value in the framebuffer
       */
      blend_mode_color,

      /*!
       * Luminosity of the value emitted by the fragment shader, but
       * take the hue and saturation of the value in the framebuffer
       */
      blend_mode_luminosity,

      number_blend_modes,
    };

  /*!
   * Enumeration to specify what kind of pixel value a blend
   * mode has when blending with a particular pixel color
   */
  enum blend_impact_t:uint32_t
    {
      /*!
       * Indicates that drawing leaves the framebuffer as-is
       */
      blend_impact_none,

      /*!
       * Indicates that drawing replaces the framebuffer value with (0, 0, 0, 0)
       */
      blend_impact_clear_black,

      /*!
       * Indicates that drawing does not leave the framebuffer as-is and
       * whites a different framebuffer value
       */
      blend_impact_intertacts
    };

  /*!
   * Returns the blend impact when drawing clear black (0, 0, 0, 0)
   */
  enum blend_impact_t
  blend_impact_with_clear_black(enum blend_mode_t);

  /*!
   * \brief
   * Enumeration to specify post sampling modes to apply to mask sampling
   */
  enum mask_post_sampling_mode_t:uint32_t
    {
      /*!
       * Take the computed coverage value as-is
       */
      mask_post_sampling_mode_direct = 0,

      /*!
       * Invert the computed coverage value
       */
      mask_post_sampling_mode_invert = 1u,
    };

  /*!
   * Invert an astral::mask_post_sampling_mode_t value.
   */
  inline
  enum mask_post_sampling_mode_t
  invert_post_sampling_mode(enum mask_post_sampling_mode_t v)
  {
    return static_cast<enum mask_post_sampling_mode_t>(v ^ 1u);
  }

  /*!
   * \brief
   * Enumeration to specify post sampling modes to apply to color sampling
   * on a pre-multiplied by alpha color value (r, g, b, a). Each of thse
   * operations when given a valid pre-multiplied by alpha color value
   * results in a valid pre-multiplied by alpha color value.
   */
  enum color_post_sampling_mode_t:uint32_t
    {
      /*!
       * Do not modify the (r, g, b, a) value
       */
      color_post_sampling_mode_direct = 0u,

      /*!
       * Retain alpha and change the color to black, i.e.
       * change a premultiplied by alha color value from
       * (r, g, b, a) to (0, 0, 0, a)
       */
      color_post_sampling_mode_black = color_post_sampling_mode_bits_rgb_zero,

      /*!
       * Retain alpha and change the color to white, i.e.
       * change a premultiplied by alpha color value from
       * (r, g, b, a) to (a, a, a, a)
       */
      color_post_sampling_mode_white = color_post_sampling_mode_bits_rgb_zero | color_post_sampling_mode_bits_rgb_invert,

      /*!
       * Invert alpha and change the color to black, i.e.
       * change a premultiplied by alpha color value from
       * (r, g, b, a) to (0, 0, 0, 1 - a)
       */
      color_post_sampling_mode_black_alpha_invert = color_post_sampling_mode_bits_rgb_zero | color_post_sampling_mode_bits_alpha_invert,

      /*!
       * Invert alpha and change the color to white, i.e.
       * change a premultiplied by alpha color value from
       * (r, g, b, a) to (1 - a, 1 - a, 1 - a, 1 - a)
       */
      color_post_sampling_mode_white_alpha_invert = color_post_sampling_mode_bits_rgb_zero | color_post_sampling_mode_bits_rgb_invert | color_post_sampling_mode_bits_alpha_invert,

      /*!
       * Retain alpha and invert the color, i.e.
       * change a premultiplied by alpha color value from
       * (r, g, b, a) to (a - r, a - g, a - b, a)
       */
      color_post_sampling_mode_rgb_invert = color_post_sampling_mode_bits_rgb_invert,

      /*!
       * Replace (r, g, b, a) with (r, g, b, 1). This makes the using
       * the color value with a number of blend modes effecively ignore
       * the destination value.
       */
      color_post_sampling_mode_rgb_direct_alpha_one = color_post_sampling_mode_bits_alpha_one,

      /*!
       * Change the value to be emitted to (0, 0, 0, 1), i.e. opaque black.
       */
      color_post_sampling_mode_opaque_black = color_post_sampling_mode_black | color_post_sampling_mode_bits_alpha_one,

      /*!
       * Change the value to be emitted to (1, 1, 1, 1), i.e. opaque white.
       */
      color_post_sampling_mode_opaque_white = color_post_sampling_mode_white | color_post_sampling_mode_bits_alpha_one,

      /*!
       * Invert the color and then change the alpha to one, i.e.
       * (r, g, b, a) becomes (a - r, a - g, a - b, 1)
       */
      color_post_sampling_mode_opaque_rgb_invert = color_post_sampling_mode_rgb_invert | color_post_sampling_mode_bits_alpha_one,
    };

  /*!
   * \brief
   * Specifies from what channel of surface to sample
   * the raw mask value
   */
  enum mask_channel_t:uint32_t
    {
      /*!
       * Indicates to sample from the red channel
       */
      mask_channel_red = 0,

      /*!
       * Indicates to sample from the green channel
       */
      mask_channel_green,

      /*!
       * Indicates to sample from the blue channel
       */
      mask_channel_blue,

      /*!
       * Indicates to sample from the alpha channel
       */
      mask_channel_alpha,

      number_mask_channel,
    };

  /*!
   * \brief
   * Enumeration to specify how to interpret a sampled value
   * when using it as a mask
   */
  enum mask_type_t:uint32_t
    {
      /*!
       * Value is intepreted as a raw coverage value
       */
      mask_type_coverage,

      /*!
       * Value is interpreted as a distance field value.
       * With a distance field, the texel filtering is
       * on distance field values which are then coverted
       * to a coverage value. In particular, a distance
       * field mask can be lower resolution than its final
       * display and still have sharp anti-aliased boundary.
       */
      mask_type_distance_field,

      number_mask_type
    };

  /*!
   * \brief
   * Enumeration to specify the image filter
   */
  enum filter_t:uint32_t
    {
      /*!
       * indictes to use nearest filtering when
       * image data is magnified; gives a pixelated
       * look.
       */
      filter_nearest,

      /*!
       * indictes to use linear filtering when
       * image data is magnified; gives a blurry
       * look.
       */
      filter_linear,

      /*!
       * indictes to use cubic filtering when
       * image data is magnified; gives a much
       * sharper look than astral::filter_linear.
       */
      filter_cubic,

      number_filter_modes,
    };

  /*!
   * \brief
   * Enumeration that specifies what mipmap to use
   */
  enum mipmap_t:uint32_t
    {
      /*!
       * Indicates to not use mipmapping at all, i.e.
       * the base image is used.
       */
      mipmap_none,

      /*!
       * Indicates to use an LOD which is closes to
       * to log2 of the minification factor of the
       * sampled image.
       */
      mipmap_nearest,

      /*!
       * Indicates to use the LOD which is the ceiling
       * of the log2 of the minification factor of the
       * sampled image. This will guarantee that the
       * mipmap sampled has a resolution less than or
       * equal to the display resolution.
       */
      mipmap_ceiling,

      /*!
       * Indicates to use the LOD which is the floor
       * of the log2 of the minification factor of the
       * sampled image. This can result in the case
       * where the displayed resolution is less than
       * the resolution of the sampled image which
       * can result in moire effects.
       */
      mipmap_floor,

      /*!
       * Indicates to use a chosen mipmap level
       */
      mipmap_chosen,

      number_mipmap_modes,
    };

  /*!
   * \brief
   * Enumeration to specify anti-aliasing
   */
  enum anti_alias_t:uint32_t
    {
      /*!
       * Indicates to render with anti-aliasing
       */
      with_anti_aliasing,

      /*!
       * Indicates to render without anti-aliasing
       */
      without_anti_aliasing,

      number_anti_alias_modes,
    };

  /*!
   * Enumeration to describe how to apply clipping
   * when rendering a mask.
   */
  enum mask_item_shader_clip_mode_t:uint32_t
    {
     /*!
      * The astral::ItemClip coverage value is computed
      * and if it is less than 0.5, the fragment is
      * effectively discarded.
      */
     mask_item_shader_clip_cutoff = 0u,

     /*!
      * Values from the astral::ItemClip are combined
      * with the output of the mask-item shader's
      * coverage and distance field values.
      */
     mask_item_shader_clip_combine = 1u,
    };

  /*!
   * Enumeration to describe if and how to create
   * a mask for filling a path or combined path.
   */
  enum fill_method_t:uint32_t
    {
      /*!
       * Indicates to not create the mask sparsely,
       * i.e. the mask for the fill is a single
       * image with all tiles backed.
       */
      fill_method_no_sparse,

      /*!
       * Indicates to create the mask sparsely, i.e. many
       * tiles of the mask are ImageMipElement::empty_element
       * and ImageMipElement::white_element. In this method
       * the CPU clips line segments and induces clipped draw
       * to the mask tiles of the conic triangles. This mode
       * can save a massive amount of bandwidth compared to
       * astral::fill_method_no_sparse at the cost that the
       * CPU performs more clipping.
       */
      fill_method_sparse_line_clipping,

      /*!
       * Indicates to create the mask sparsely, i.e. many
       * tiles of the mask are ImageMipElement::empty_element
       * and ImageMipElement::white_element. In this method
       * the CPU clips both the line segments and curves to
       * the tiles. This mode saves even more bandwidth than
       * the mode astral::fill_method_sparse_curve_clipping at
       * the cost of more CPU time.
       */
      fill_method_sparse_curve_clipping,

      number_fill_method_t
    };

  /*!
   * Enumeration to specify how implement clip windows.
   */
  enum clip_window_strategy_t:uint32_t
    {
      /*!
       * Specifies to use \ref ClipWindow passed to the
       * shaders. If HW clip planes are available, then
       * it is expected the they would enforce the planes,
       * without HW clip planes, expect discard/kill in
       * the fragment shader
       */
      clip_window_strategy_shader = 0,

      /*!
       * Specifies to use depth buffer occluding. This will
       * force draw breaks between virtual color buffers
       * to draw to the depth buffer.
       */
      clip_window_strategy_depth_occlude,

      /*!
       * Specifies to use depth buffer occluding. This will
       * force draw breaks between virtual color buffers
       * to draw to the depth buffer. However, the clip
       * window is still used by the rendering backend to
       * provide an early out in fragment shading.
       */
      clip_window_strategy_depth_occlude_hinted,
    };

  /*!
   * Enumeration to just specify the number of
   * distinct \ref clip_window_strategy_t values.
   */
  enum clip_window_strategy_number_t:uint32_t
    {
      /*!
       * Enumeration to just specify the number of
       * distinct \ref clip_window_strategy_t values.
       */
      number_clip_window_strategy = clip_window_strategy_depth_occlude_hinted + 1u
    };

  /*!
   * Enumeration to specify if and how to use uber-shading for
   * color buffer rendering
   */
  enum uber_shader_method_t:uint32_t
    {
      /*!
       * indicates to not use any form of uber shading, i.e.
       * different astral::ItemShaderBackend (and potentially
       * different blend modes as well) will be realized
       * as seperate shaders.
       */
      uber_shader_none,

      /*!
       * indicates that all shaders used within rendering to a
       * fixed color buffer will be accumulated together into
       * a single uber-shader. If what shaders that are needed
       * changes, then a different shader will be used.
       */
      uber_shader_active,

      /*!
       * indicates that all shaders used so far by astral::Renderer
       * (or astral::RendererBackend) will be accumulated into
       * a single uber-shader.
       */
      uber_shader_cumulative,

      /*!
       * Same as uber_shader_active, except that
       * the shader code for blend shaders is accumulated.
       */
      uber_shader_active_blend_cumulative,

      /*!
       * Indicates to have an uber-sader that consists of ALL
       * color-item shaders, all material shader and code to
       * handle all blend modes. This will remove any hitching
       * at the cost that all performance is lower from the
       * potentially much larger shader.
       */
      uber_shader_all,
    };

  /*!
   * Enumeration to specify the the number of
   * distinct \ref uber_shader_method_t values.
   */
  enum uber_shader_number_method_t:uint32_t
    {
      number_uber_shader_method = uber_shader_all + 1u,
    };

  /*!
   * Returns a string corresponing to the enum-value.
   */
  c_string
  label(enum fill_method_t v);

  /*!
   * Returns a string corresponing to the enum-value.
   */
  c_string
  label(enum image_blit_processing_t);

  /*!
   * Returns a string corresponing to the enum-value.
   */
  c_string
  label(enum downsampling_processing_t);

  /*!
   * Returns a string corresponing to the enum-value.
   */
  c_string
  label(enum tile_mode_t);

  /*!
   * Returns a string corresponing to the enum-value.
   */
  c_string
  label(enum filter_t);

  /*!
   * Returns a string corresponing to the enum-value.
   */
  c_string
  label(enum mipmap_t);

  /*!
   * Returns a string corresponing to the enum-value.
   */
  c_string
  label(enum anti_alias_t);

  /*!
   * Returns a string corresponing to the enum-value.
   */
  c_string
  label(enum mask_channel_t);

  /*!
   * Returns a string corresponing to the enum-value.
   */
  c_string
  label(enum mask_type_t);

  /*!
   * Returns a string corresponing to the enum-value.
   */
  c_string
  label(enum fill_rule_t);

  /*!
   * Returns a string corresponing to the enum-value.
   */
  c_string
  label(enum blend_mode_t);

  /*!
   * Returns a string corresponing to the enum-value.
   */
  c_string
  label(enum cap_t);

  /*!
   * Returns a string corresponing to the enum-value.
   */
  c_string
  label(enum join_t);

  /*!
   * Returns a string corresponing to the enum-value.
   */
  c_string
  label(enum clip_window_strategy_t);

  /*!
   * Returns a string corresponing to the enum-value.
   */
  c_string
  label(enum uber_shader_method_t);

/*! @} */
}

#endif

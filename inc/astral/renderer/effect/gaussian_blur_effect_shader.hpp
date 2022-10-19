/*!
 * \file gaussian_blur_effect_shader.hpp
 * \brief file gaussian_blur_effect_shader.hpp
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

#ifndef ASTRAL_GAUSSIAN_BLUR_EFFECT_SHADER_HPP
#define ASTRAL_GAUSSIAN_BLUR_EFFECT_SHADER_HPP

#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/shader/material_shader.hpp>
#include <astral/renderer/effect/effect.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * \brief
   * A astral::GaussianBlurEffectShader holds the astral::MaterialShader
   * values to use to implement Gaussian blur. It also defines enumerations
   * specifying how data is packed into the astral::ItemData read by the shaders.
   *
   * Each pass is to be realized as computing
   * \f[
   * w0 * fetch(0) + sum_{1 <= i <= N} weight(i) * (fetch(offset(i)) + fetch(-offset(i)))
   * \f]
   *
   * - ItemData packing is
   *   - [0] (headers)
   *     - .x().u N of the above formula
   *     - .y().f value of w0 of the above formula
   *     - .z().u astral::RenderValue<ImageSampler> processed value
   *     - .w().u bit-flags, see \ref bits_t
   *     .
   *   - [I] for 1 <= I <= ceiling(N / 2), N as above
   *     - .x().f weight(2 * I - 1)
   *     - .y().f offset(2 * I - 1)
   *     - .z().f weight(2 * I)
   *     - .w().f offset(2 * I)
   */
  class GaussianBlurEffectShader
  {
  public:
    /*!
     * Enumeration to specify what bit fields
     */
    enum bits_t:uint32_t
      {
        /*!
         * If this bit is up, then the fragment shader should
         * sample from LOD = 1 instead of LOD = 0 when rendering.
         */
        sample_from_lod1_bit = 0,

        /*!
         * Bit mask made from \ref sample_from_lod1_bit
         */
        sample_from_lod1_mask = ASTRAL_BIT_MASK(sample_from_lod1_bit),
      };

    /*!
     * Using \ref m_horizontal_blur and \ref m_vertical_blur
     * construct an astral::Effect that implements
     * Gaussian blur.
     */
    reference_counted_ptr<Effect>
    create_effect(void) const;

    /*!
     * Set \ref m_horizontal_blur
     */
    GaussianBlurEffectShader&
    horizontal_blur(const reference_counted_ptr<const MaterialShader> &v)
    {
      m_horizontal_blur = v;
      return *this;
    }

    /*!
     * Set \ref m_horizontal_blur
     */
    GaussianBlurEffectShader&
    vertical_blur(const reference_counted_ptr<const MaterialShader> &v)
    {
      m_vertical_blur = v;
      return *this;
    }

    /*!
     * astral::ItemDataValueMapping for the astral::ItemData
     * to be fed to the shaders
     */
    static
    const ItemDataValueMapping&
    item_data_value_map(void);

    /*!
     * Shader to use for the horizontal filtering pass
     */
    reference_counted_ptr<const MaterialShader> m_horizontal_blur;

    /*!
     * Shader to use for the verical filtering pass
     */
    reference_counted_ptr<const MaterialShader> m_vertical_blur;
  };

  /*!
   * \brief
   * Class that holds and specifies the parameters to feed the
   * Effect::compute_buffer_properties() of the astral::Effect
   * created with GaussianBlurEffectShader::create_effect()
   */
  class GaussianBlurParameters
  {
  public:
    /*!
     * Enumerations to hold the offsets that specify how to interpret
     * the array values fed to Effect::compute_buffer_properties()
     */
    enum offset_t:uint32_t
      {
        /*!
         * Offset into effect_parameters() that stores the
         * radius().
         */
        radius_offset = 0,

        /*!
         * Offset into effect_parameters() that stores
         * max_sample_radius().
         */
        max_sample_radius_offset,

        /*!
         * Offset into effect_parameters() that stores
         * min_render_scale()
         */
        min_render_scale_offset,

        /*!
         * Offset into effect_parameters() that stores
         * include_halo().
         */
        include_halo_offset,

        /*!
         * Offset into effect_parameters() that stores
         * the red channel value for the color modulation
         */
        color_modulation_red_offset,

        /*!
         * Offset into effect_parameters() that stores
         * the green channel value for the color modulation
         */
        color_modulation_green_offset,

        /*!
         * Offset into effect_parameters() that stores
         * the blue channel value for the color modulation
         */
        color_modulation_blue_offset,

        /*!
         * Offset into effect_parameters() that stores
         * the alpha channel value for the color modulation
         */
        color_modulation_alpha_offset,

        /*!
         * Offset into effect_parameters() that stores
         * post_sampling_mode()
         */
        post_sampling_mode_offset,

        /*!
         * Offset into effect_parameters() that stores
         * blur_radius_in_local_coordinates().
         */
        blur_radius_in_local_coordinates_offset,

        /*!
         * Offset into effect_parameters() that stores
         * force_pow2_render_scale().
         */
        force_pow2_render_scale_offset,

        /*!
         * The size of effect_parameters()
         */
        effect_param_size
      };

    /*!
     * Ctor; initializes the blur to a specified radius
     * \param r blur radius in logical coordinates
     */
    explicit
    GaussianBlurParameters(float r = 4.0f)
    {
      radius(r);
      max_sample_radius(4.0f);
      min_render_scale(-1.0f);
      include_halo(true);
      color_modulation(1.0f, 1.0f, 1.0f, 1.0f);
      blur_radius_in_local_coordinates(true);
      post_sampling_mode(color_post_sampling_mode_direct);
      force_pow2_render_scale(false);
    }

    /*!
     * Set the blur by providing a sigma. In what coordinate
     * system the value is interpreted (logical or pixel) is
     * specified by blur_radius_in_local_coordinates().
     */
    GaussianBlurParameters&
    sigma(float v);

    /*!
     * Set the blur by providing a radius. In what coordinate
     * system the value is interpreted (logical or pixel) is
     * specified by blur_radius_in_local_coordinates().
     */
    GaussianBlurParameters&
    radius(float v)
    {
      m_data[radius_offset].f = v;
      return *this;
    }

    /*!
     * Sets the maximum radius of the pixel sample window when
     * performing Guassian blur. If the blur radius in pixels
     * exceeds this value, to emulate blur, the blur effect will
     * sample from an image of lower resolution. Generally,
     * one should leave this value alone as higher values harm
     * performance with typically no visual quaility improvement.
     */
    GaussianBlurParameters&
    max_sample_radius(float v)
    {
      m_data[max_sample_radius_offset].f = v;
      return *this;
    }

    /*!
     * Set the value returned by min_render_scale(void) const.
     * Default value is -1.0.
     */
    GaussianBlurParameters&
    min_render_scale(float v)
    {
      m_data[min_render_scale_offset].f = v;
      return *this;
    }

    /*!
     * Set the value returned by include_halo(void) const.
     * Default value is true.
     */
    GaussianBlurParameters&
    include_halo(bool v)
    {
      m_data[include_halo_offset].u = (v) ? 1u : 0u;
      return *this;
    }

    /*!
     * Set the value returned by color_modulation_red(void) const.
     * Default value is 1.0.
     */
    GaussianBlurParameters&
    color_modulation_red(float v)
    {
      m_data[color_modulation_red_offset].f = v;
      return *this;
    }

    /*!
     * Set the value returned by color_modulation_green(void) const.
     * Default value is 1.0.
     */
    GaussianBlurParameters&
    color_modulation_green(float v)
    {
      m_data[color_modulation_green_offset].f = v;
      return *this;
    }

    /*!
     * Set the value returned by color_modulation_blue(void) const.
     * Default value is 1.0.
     */
    GaussianBlurParameters&
    color_modulation_blue(float v)
    {
      m_data[color_modulation_blue_offset].f = v;
      return *this;
    }

    /*!
     * Set the value returned by color_modulation_alpha(void) const.
     * Default value is 1.0.
     */
    GaussianBlurParameters&
    color_modulation_alpha(float v)
    {
      m_data[color_modulation_alpha_offset].f = v;
      return *this;
    }

    /*!
     * Set all of the color modulation values, equivalent to
     * \code
     * \endcode
     */
    GaussianBlurParameters&
    color_modulation(float red, float green, float blue, float alpha = 1.0f)
    {
      color_modulation_red(red);
      color_modulation_green(green);
      color_modulation_blue(blue);
      color_modulation_alpha(alpha);
      return *this;
    }

    /*!
     * Set the value returned by post_sampling_mode(void) const.
     * Default value is astral::color_post_sampling_mode_direct.
     */
    GaussianBlurParameters&
    post_sampling_mode(enum color_post_sampling_mode_t v)
    {
      m_data[post_sampling_mode_offset].u = v;
      return *this;
    }

    /*!
     * Set the value returned by blur_radius_in_local_coordinates(void) const.
     * Default value is true.
     */
    GaussianBlurParameters&
    blur_radius_in_local_coordinates(bool b)
    {
      m_data[blur_radius_in_local_coordinates_offset].u = b ? 1u : 0u;
      return *this;
    }

    /*!
     * Set the value returned by force_pow2_render_scale(void) const.
     * Default value is false.
     */
    GaussianBlurParameters&
    force_pow2_render_scale(bool b)
    {
      m_data[force_pow2_render_scale_offset].u = b ? 1u : 0u;
      return *this;
    }

    /*!
     * Return the value of the blur's sigma (relative to logical
     * coordinates if blur_radius_in_local_coordinates() is true,
     * relative to pixel coordinate if it is false).
     */
    float
    sigma(void) const;

    /*!
     * Return the value of the blur's radius (relative to logical
     * coordinates if blur_radius_in_local_coordinates() is true,
     * relative to pixel coordinate if it is false).
     */
    float
    radius(void) const
    {
      return m_data[radius_offset].f;
    }

    /*!
     * Returns the max pixel radius, see
     * max_sample_radius_offset(float)
     */
    float
    max_sample_radius(void) const
    {
      return m_data[max_sample_radius_offset].f;
    }

    /*!
     * Specifies the minimum render scale factor allowed for
     * generating the image to which to apply blur. A value
     * less than or equal to zero represents there is no minimum.
     * This values plays a role when the blur radius (in rendered
     * pixels) exceeds the value of max_sample_radius(). The
     * effect will render a smaller image scaled by no less than
     * this value. If it needs to scale down further it will
     * rely on mip-mapping.
     */
    float
    min_render_scale(void) const
    {
      return m_data[min_render_scale_offset].f;
    }

    /*!
     * If true, when performing the effect, include
     * the region around the bounding rect that
     * has the blur halo. Default value is true.
     */
    bool
    include_halo(void) const
    {
      return m_data[include_halo_offset].u == 1u;
    }

    /*!
     * Return the red channel value for color modulation
     */
    float
    color_modulation_red(void) const
    {
      return m_data[color_modulation_red_offset].f;
    }

    /*!
     * Return the green channel value for color modulation
     */
    float
    color_modulation_green(void) const
    {
      return m_data[color_modulation_green_offset].f;
    }

    /*!
     * Return the blue channel value for color modulation
     */
    float
    color_modulation_blue(void) const
    {
      return m_data[color_modulation_blue_offset].f;
    }

    /*!
     * Return the alpha channel value for color modulation
     */
    float
    color_modulation_alpha(void) const
    {
      return m_data[color_modulation_alpha_offset].f;
    }

    /*!
     * Returns the post-sampling operation applied to sampled
     * blurred pixels.
     */
    enum color_post_sampling_mode_t
    post_sampling_mode(void) const
    {
      return static_cast<enum color_post_sampling_mode_t>(m_data[post_sampling_mode_offset].u);
    }

    /*!
     * Returns true if and only if the blur radius/sigma is
     * in local coordinate. If false, the blur radius/sigma
     * are in pixel coordinates.
     */
    bool
    blur_radius_in_local_coordinates(void) const
    {
      return m_data[blur_radius_in_local_coordinates_offset].u == 1u;
    }

    /*!
     * Returns true if and only if when rendering at a lower resolution
     * to force the rendering scale factor to be a power of 2.
     */
    bool
    force_pow2_render_scale(void) const
    {
      return m_data[force_pow2_render_scale_offset].u == 1u;
    }

    /*!
     * Return the effect parameters as an astral::c_array to be used
     * by astral::Effect::compute_buffer_properties().
     */
    c_array<const generic_data>
    effect_parameters(void) const
    {
      return m_data;
    }

  private:
    vecN<generic_data, effect_param_size> m_data;
  };

/*! @} */
}

#endif

/*!
 * \file shader_set_gl3.hpp
 * \brief file shader_set_gl3.hpp
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

#ifndef ASTRAL_SHADER_SET_GL3_HPP
#define ASTRAL_SHADER_SET_GL3_HPP

#include <astral/renderer/image_sampler_bits.hpp>
#include <astral/renderer/gl3/item_shader_gl3.hpp>
#include <astral/renderer/gl3/material_shader_gl3.hpp>
#include <astral/renderer/gl3/stroke_shader_gl3.hpp>
#include <astral/renderer/gl3/shader_library_gl3.hpp>

namespace astral
{
  namespace gl
  {
/*!\addtogroup gl
 * @{
 */

    /*!
     * \brief
     * A ShaderSetGL3 provides the astral::gl::ItemShaderBackendGL3 and
     * astral::gl::MaterialShaderGL3 that are used by the default shaders
     * astral::gl::RenderEngineGL3. The purpose is to enable applications
     * to create shaders which depend on these shaders. In addition, this
     * class documents the symbols and varyings of these shaders.
     */
    class ShaderSetGL3
    {
    public:
      /*!
       * Enumeration to specify the bit and bits masks for
       * the data of the brush varying astral_brush_flags
       */
      enum brush_bits:uint32_t
        {
          /*!
           * Bit0 for the repeat window applied to the
           * gradient
           */
          brush_gradient_repeat_window_bit0 = 0,

          /*!
           * If this bit is down, then there is no active astral::Brush, thus
           * the color emitted by the material is opaque white.
           */
          brush_active_bit = brush_gradient_repeat_window_bit0 + 2u * ImageSamplerBits::tile_mode_num_bits,

          /*!
           * If this bit is up, then the brush has an image.
           */
          brush_has_image_bit = brush_active_bit + 1u,

          /*!
           * If this bit is up, then the brush has a gradient.
           */
          brush_has_gradient_bit = brush_has_image_bit + 1u,

          /*!
           * If this bit is up, then the gradient has an astral::GradientTransformation applied to it.
           */
          brush_has_gradient_transformation_bit = brush_has_gradient_bit + 1u,

          /*!
           * If this bit is up, then the brush works in its own color space.
           */
          brush_specified_colorspace_bit = brush_has_gradient_transformation_bit + 1u,

          /*!
           * Which bit that specifies in what colorspace the brush works in
           * when the bit of \ref brush_specified_colorspace_bit is up.
           */
          brush_colorspace_bit = brush_specified_colorspace_bit + 1u,
        };

      /*!
       * enumeration to descrine the different types of base stroke shaders
       */
      enum stroke_shader_type_t:uint32_t
        {
          /*!
           * Stroke shader set that perfoms stroking without dashing and
           * the shaders do NOT emit distance values.
           */
          only_stroking,

          /*!
           * Stroke shader set that performs dashed stroking and emits
           * distance values.
           */
          dashed_stroking,

          /*!
           * Stroke shader set that perfoms stroking without dashing
           * and emits distance values.
           */
          distance_stroking,

          stroke_shader_type_count
        };

      /*!
       * Common shader libraries available for reuse.
       */
      ShaderLibraryGL3 m_common_libs;

      /*!
       * The shader used for the default brush.
       *
       * It has the following varyings of type ShaderVaryings::interpolator_smooth:
       *  - astral_brush_image_p_x, astral_brush_image_p_y the x-position and
       *    y-position from which to sample the image after the image_transformation transformation
       *    is applied but before the image_transformation window repeat mode is applied.
       *  - astral_brush_gradient_p_x, astral_gradient_p_y the x-position and
       *    y-position from which to sample the gradient after the image_transformation transformation
       *    is applied but before the image_transformation window repeat mode is applied.
       *  .
       *
       * It has the following varyings of type ShaderVaryings::interpolator_flat
       *  - astral_brush_base_color_x, astral_brush_base_color_y,
       *    astral_brush_base_color_z, astral_brush_base_color_w make the
       *    vec4 values for the base color of the brush
       *  .
       *
       * It has the following varyings of type ShaderVaryings::interpolator_uint
       *  - astral_brush_flags flags of what is active and how, see the enumeration
       *    \ref brush_bits for the bit packing.
       *  - astral_brush_image0_x, astral_brush_image0_y, astral_brush0_image_z,
       *    astral_brush_image0_w, astral_brush_image1_x, astral_brush_image1_y,
       *    astral_brush_image1_z, and astral_brush_image1_w represent the values
       *    of the ImageSampler packed; the values can be unpacked with the GLSL
       *    function astral_unpack_image() and repacked with the GLSL function
       *    astral_pack_image().
       *  - astral_brush_gradient0_x, astral_brush_gradient0_y,
       *    astral_brush_gradient0_z, astral_brush_gradient0_w,
       *    astral_brush_gradient1_x, astral_brush_gradient1_y,
       *    astral_brush_gradient1_z, astral_brush_gradient1_w, represent the
       *    values of the Gradient packed; the values can be unpacked
       *    with the GLSL function astral_unpack_gradient() and repacked with
       *    the GLSL function astral_pack_gradient().
       *  - astral_brush_gradient_transformation_x, astral_brush_gradient_transformation_y
       *    astral_brush_gradient_transformation_z, astral_brush_gradient_transformation_w
       *    represent the values of the repeat window applied to the gradient
       *    packed; the values can be unpacked with the GLSL function
       *    astral_unpack_image_transformation_window() and repacked with astral_pack_image_transformation_window().
       *    The pack-flags to feed to astral_pack_image_transformation_window() are derived
       *    from astral_brush_flags at bit \ref brush_gradient_repeat_window_bit0
       */
      reference_counted_ptr<const MaterialShaderGL3> m_brush_shader;

      /*!
       * Sub-shader ID's for \ref m_lighting_shader
       */
      enum lighting_sub_shaders_t: uint32_t
        {
          /*!
           * Subshader of \ref m_lighting_shader that does not
           * anti-alias the shadow.
           */
          lighting_sub_shader_without_aa = 0,

          /*!
           * Subshader of \ref m_lighting_shader that does
           * 4x anti-alias the shadow.
           */
          lighting_sub_shader_aa4,

          /*!
           * Subshader of \ref m_lighting_shader that does
           * 8x anti-alias the shadow.
           */
          lighting_sub_shader_aa8,

          /*!
           * Subshader of \ref m_lighting_shader that does
           * 8x anti-alias the shadow.
           */
          lighting_sub_shader_aa16,

          lighting_sub_shader_count
        };

      /*!
       * astral::gl::MaterialShaderGL3 that performs lighting to realize pixel
       * colors. The subshader as enumerated by \ref lighting_sub_shaders_t
       * provide the shaders for astral::ShaderSet.
       *
       * TODO: document symbols and make extendibly for other purposes.
       */
      reference_counted_ptr<const MaterialShaderGL3> m_lighting_shader;

      /*!
       * Sub-shader ID's for \ref m_scalable_glyph_shader and
       * \ref m_image_glyph_shader
       */
      enum glyph_sub_shaders_t : uint32_t
        {
          /*!
           * indicates that color glyphs will ignore the (red, green, blue)
           * values from the material shader, i.e. they preserve their color
           */
          glyph_sub_shader_preserve_color_glyphs,

          /*!
           * indicates taht color glyphs will have their color modified
           * by the material shader
           */
          glyph_sub_shader_color_glyphs_obey_material,

          glyph_sub_shader_count
        };

      /*!
       * astral::ItemShaderBackendGL3 that performs glyph rendering for scalable glyphs
       */
      reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> m_scalable_glyph_shader;

      /*!
       * astral::ItemShaderBackendGL3 that performs glyph rendering for image glyphs
       */
      reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> m_image_glyph_shader;

      /*!
       * Base shader sets for generating masks for stroking, indexed as
       * m_mask_stroke_shaders[\ref stroke_shader_type_t]
       */
      vecN<StrokeShaderGL3, stroke_shader_type_count> m_mask_stroke_shaders;
      /*!
       * Base shader sets for directly stroking, indexed as
       * m_direct_stroke_shaders[\ref stroke_shader_type_t]
       */
      vecN<StrokeShaderGL3, stroke_shader_type_count> m_direct_stroke_shaders;
    };
/*! @} */

  }
}

#endif

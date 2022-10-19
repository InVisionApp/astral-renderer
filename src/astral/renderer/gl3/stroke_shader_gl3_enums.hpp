/*!
 * \file stroke_shader_gl3_enums.hpp
 * \brief file stroke_shader_gl3_enums.hpp
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

#ifndef ASTRAL_STROKE_SHADER_GL3_ENUMS_HPP
#define ASTRAL_STROKE_SHADER_GL3_ENUMS_HPP

#include <stdint.h>

namespace astral
{
  namespace gl
  {
    namespace StrokeShaderGL3Enums
    {
      /* join shaders use the enumeration join_t, however, the rounded
       * join shader is also used for inner glue as a sub-shader
       */
      enum stroke_join_sub_shader_t:uint32_t
        {
          /* to draw outer joins */
          outer_join_sub_shader = 0,

          /* to draw inner joins */
          inner_join_sub_shader,
        };

      /* organizing the sub-shader ID bits */
      enum stroke_shader_id_bits_t:uint32_t
        {
          /* bit when up indicates that aniamtion is active, this MUST be 0 */
          stroker_shader_animation_bit0 = 0,

          /* shader-subtype bit, used in join and quadratic shaders */
          stroker_shader_subtype_bit0,

          /* bit0 to specify the how and if to dash stroke;
           * the cap style comes from the enumeration cap_t
           * and we use number_cap_t to indicate no dash.
           */
          stroker_shader_cap_style_bit0,

          /* number of bits needed to specify cap type */
          stroker_shader_cap_style_num_bits = 2u,

          stroke_shader_num_bits = stroker_shader_cap_style_bit0 + stroker_shader_cap_style_num_bits,

          /* number of sub-shader needed to this bit packing.
           * stroke_shader_number_with_cap_style cannot be applied to cap shaders!
           */
          stroke_shader_number_with_cap_style = 1u << stroke_shader_num_bits,
          stroke_shader_number_without_cap_style = 1u << stroker_shader_cap_style_bit0,

          /* masks */
          stroker_shader_animation_mask = ASTRAL_MASK(stroker_shader_animation_bit0, 1u),
          stroker_shader_subtype_mask = ASTRAL_MASK(stroker_shader_subtype_bit0, 1u),
          stroker_shader_cap_style_mask = ASTRAL_MASK(stroker_shader_cap_style_bit0, stroker_shader_cap_style_num_bits)
        };

      /*!
       * Gives the sub-shader ID for CommonStrokeShaderGL3::m_join[astral::join_t]
       * for stroking joins; for non-dashed stroking
       * \param p specify is stroking static or animated path
       * \param s specify if stroking inner or outer joins.
       */
      inline
      uint32_t
      sub_shader_id(enum StrokeShader::path_shader_t p,
                    enum stroke_join_sub_shader_t s)
      {
        return pack_bits(stroker_shader_animation_bit0, 1u, p)
          | pack_bits(stroker_shader_subtype_bit0, 1u, s);
      }

      /*!
       * Gives the sub-shader ID for CommonStrokeShaderGL3::m_line
       * and CommonStrokeShaderGL3::m_biarc_curve; for non-dashed stroking
       * \param p specify is stroking static or animated path
       */
      inline
      uint32_t
      sub_shader_id(enum StrokeShader::path_shader_t p)
      {
        return pack_bits(stroker_shader_animation_bit0, 1u, p);
      }

      /*!
       * gives the sub-shader ID for stroking a join with dashed
       * stroking of the named cap style.
       */
      inline
      uint32_t
      sub_shader_id(enum StrokeShader::path_shader_t p,
                    enum stroke_join_sub_shader_t s,
                    enum cap_t c)
      {
        return pack_bits(stroker_shader_animation_bit0, 1u, p)
          | pack_bits(stroker_shader_subtype_bit0, 1u, s)
          | pack_bits(stroker_shader_cap_style_bit0, stroker_shader_cap_style_num_bits, c);
      }

      /*!
       * Gives the sub-shader ID for CommonStrokeShaderGL3::m_line
       * and CommonStrokeShaderGL3::m_biarc_curve; for dashed stroking
       * with the named cap style
       * \param p specify is stroking static or animated path
       */
      inline
      uint32_t
      sub_shader_id(enum StrokeShader::path_shader_t p,
                    enum cap_t c)
      {
        return pack_bits(stroker_shader_animation_bit0, 1u, p)
          | pack_bits(stroker_shader_cap_style_bit0, stroker_shader_cap_style_num_bits, c);
      }

      /*!
       * Gives the sub-shader ID for CommonStrokeShaderGL3::m_cappers
       * for a dashed cap-style s and static or animated path p.
       * \param p specify is stroking static or animated path
       * \param s specify the dashed cap style
       */
      inline
      uint32_t
      sub_shader_id(enum StrokeShader::path_shader_t p,
                    enum StrokeShader::capper_shader_t s)
      {
        return pack_bits(stroker_shader_animation_bit0, 1u, p)
          | pack_bits(stroker_shader_subtype_bit0, 1u, s);
      }

    }
  }
}

#endif

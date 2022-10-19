/*!
 * \file shader_set.hpp
 * \brief file shader_set.hpp
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

#ifndef ASTRAL_SHADER_SET_HPP
#define ASTRAL_SHADER_SET_HPP

#include <astral/util/enum_flags.hpp>
#include <astral/renderer/shader/clip_combine_shader.hpp>
#include <astral/renderer/shader/fill_stc_shader.hpp>
#include <astral/renderer/shader/blit_mask_tile_shader.hpp>
#include <astral/renderer/shader/dynamic_rect_shader.hpp>
#include <astral/renderer/shader/stroke_shader.hpp>
#include <astral/renderer/shader/item_path_shader.hpp>
#include <astral/renderer/shader/glyph_shader.hpp>
#include <astral/renderer/shader/masked_rect_shader.hpp>
#include <astral/renderer/shader/material_shader.hpp>
#include <astral/renderer/shader/light_material_shader.hpp>
#include <astral/renderer/shader/shadow_map_generator_shader.hpp>

namespace astral
{
  /*!
   * \brief
   * Class to embody all the shaders needed for UI rendering
   * by a RenderBackend.
   */
  class ShaderSet
  {
  public:
    /*!
     * Enumeration flags to specify what sides of rect are
     * to get anti-aliasing.
     */
    typedef EnumFlags<enum RectEnums::side_t, 4> RectSideAAList;

    /*!
     * Collection of shaders to perform path rendering via
     * stencil-then-cover.
     */
    FillSTCShader m_stc_shader;

    /*!
     * A shader to perform blitting a clip-in/clip-out
     * of two different render images against a single
     * tile of a mask buffer.
     */
    BlitMaskTileShader m_blit_mask_tile_shader;

    /*!
     * A shader to perform drawing of dynamically
     * sized rectangles with no anti-aliasing.
     * Shader emits vec4(1.0, 1.0, 1.0, 1.0) as
     * color value and coverage as 1.0.
     */
    DynamicRectShader m_dynamic_rect_shader;

    /*!
     * A shader to perform drawing of dynamically
     * sized rectangles with anti-aliasing applied.
     * Shader emits vec4(1.0, 1.0, 1.0, 1.0) as
     * color value and coverage to perform anti-aliasing.
     */
    DynamicRectShader m_dynamic_rect_aa_shader;

    /*!
     * A shader to draw mapped rectangles masked against
     * a single tile of an astral::ImageMipElement
     * WITHOUT anti-aliasing the boundary of the mapped
     * rectangle. Shader emits vec4(1.0, 1.0, 1.0, 1.0)
     * as color value and coverage coming from the mask.
     */
    MaskedRectShader m_masked_rect_shader;

    /*!
     * Shader used to perform tile-by-tile drawing to combine
     * a pre-exising mask M with the generation of a new mask
     * F.
     */
    ClipCombineShader m_clip_combine_shader;

    /*!
     * Shaders to generate masks for stroking, to handle
     * shader data as packed by StrokeShaderItemDataPacker::ItemDataPacker
     */
    reference_counted_ptr<const MaskStrokeShader> m_mask_stroke_shader;

    /*!
     * Shaders to generate masks for dashed stroking, to handle
     * shader data as packed by StrokeShaderItemDataPacker::DashPattern
     */
    reference_counted_ptr<const MaskStrokeShader> m_mask_dashed_stroke_shader;

    /*!
     * Shaders to stroke directly without generating a mask, to handle
     * shader data as packed by StrokeShaderItemDataPacker::ItemDataPacker
     */
    reference_counted_ptr<const DirectStrokeShader> m_direct_stroke_shader;

    /*!
     * Shaders to dashed stroke directly without generating a mask, to handle
     * shader data as packed by StrokeShaderItemDataPacker::DashPattern
     */
    reference_counted_ptr<const DirectStrokeShader> m_direct_dashed_stroke_shader;

    /*!
     * Shader draw astral::ItemPath values directly to a color buffer
     */
    ColorItemPathShader m_color_item_path_shader;

    /*!
     * Shader draw astral::ItemPath values having a single layer
     * to a mask buffer
     */
    MaskItemPathShader m_mask_item_path_shader;

    /*!
     * Shader to render glyphs, it is expected that the shader
     * will NOT apply the material to colored glyphs (but will
     * apply the material to non-colored glyphs).
     */
    GlyphShader m_glyph_shader;

    /*!
     * Shader to render glyphs, it is expected that the shader
     * will apply the material to ALL glyphs always.
     */
    GlyphShader m_glyph_shader_observe_material_always;

    /*!
     * The astral::MaterialShader used for shading an
     * astral::Brush value.
     */
    reference_counted_ptr<const MaterialShader> m_brush_shader;

    /*!
     * Shaders to generate astral::ShadowMap texels
     */
    ShadowMapGeneratorShader m_shadow_map_generator_shader;

    /*!
     * Light material to perform lighting computation from
     * a single light.
     */
    LightMaterialShader m_light_material_shader;

    /*!
     * Light material to perform lighting computation from
     * a single light where the shadow is 4x anti-aliaased
     */
    LightMaterialShader m_light_material_shader_aa4_shadow;

    /*!
     * Light material to perform lighting computation from
     * a single light where the shadow is 8x anti-aliaased
     */
    LightMaterialShader m_light_material_shader_aa8_shadow;

    /*!
     * Light material to perform lighting computation from
     * a single light where the shadow is 16x anti-aliaased
     */
    LightMaterialShader m_light_material_shader_aa16_shadow;

    /*!
     * Return the astral::DynamicRectShader which anti-aliases
     * only those sides listed in the passed RectSideAAList
     */
    const DynamicRectShader&
    dynamic_rect_shader(RectSideAAList Q) const
    {
      return m_dynamic_rect_shaders[Q.m_backing[0]];
    }

    /*!
     * Returns an l-value for the astral::DynamicRectShader
     * which anti-aliases only those sides listed in the passed
     * RectSideAAList
     */
    DynamicRectShader&
    dynamic_rect_shader(RectSideAAList Q)
    {
      return m_dynamic_rect_shaders[Q.m_backing[0]];
    }

    /* TODO: have material shaders that:
     *   - combine m_light_material_shader and m_brush_shader
     *   - a light shader taking a variable number of lights
     *   - a light shader taking a variable number of lights that also modulates a brush
     */

  private:
    /* Indexed by RectSideAAList::backing_value(0), gives
     * an astral::DynamicRectShader which anti-aliases only
     * those sides listed.
     */
    vecN<DynamicRectShader, 16> m_dynamic_rect_shaders;
  };
}

#endif

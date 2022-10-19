/*!
 * \file render_engine_gl3_blend_builder.cpp
 * \brief file render_engine_gl3_blend_builder.cpp
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

#include <astral/util/gl/gl_shader_source.hpp>
#include "render_engine_gl3_blend_builder.hpp"

///////////////////////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::BlendBuilder::PerBlendMode methods

void
astral::gl::RenderEngineGL3::Implement::BlendBuilder::PerBlendMode::
emit_gl_blend_state(void) const
{
  if (m_enable_gl_blend)
    {
      astral_glEnable(ASTRAL_GL_BLEND);
      astral_glBlendEquationSeparate(m_blend_equation_rgb, m_blend_equation_a);
      astral_glBlendFuncSeparate(m_blend_func_src_rgb, m_blend_func_dst_rgb,
                                 m_blend_func_src_a, m_blend_func_dst_a);
    }
  else
    {
      astral_glDisable(ASTRAL_GL_BLEND);
    }
}

/////////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::BlendBuilder methods
astral::gl::RenderEngineGL3::Implement::BlendBuilder::
BlendBuilder(const Config &config)
{
  /* make sure that the first shader added is the fixed-function shader */
  add_shader("astral_blending_fixed_funciton.glsl.resource_string");
  ASTRALassert(0u == fetch_shader_id("astral_blending_fixed_funciton.glsl.resource_string"));

  /* TODO:
   *  - add a field to config to specify how to do blending
   *    (single-src, dual-src, framebuffer-fetch)
   */
  ASTRALunused(config);

  m_info[BackendBlendMode(BackendBlendMode::mask_mode_rendering).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ONE, ASTRAL_GL_ONE, ASTRAL_GL_MAX);
  m_info[BackendBlendMode(BackendBlendMode::shadowmap_mode_rendering).packed_value()] = PerBlendMode(*this, blend_off);

  m_info[BackendBlendMode(false, blend_porter_duff_clear).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ZERO, ASTRAL_GL_ZERO);
  m_info[BackendBlendMode(false, blend_porter_duff_src).packed_value()] = PerBlendMode(*this, blend_off);
  m_info[BackendBlendMode(false, blend_porter_duff_dst).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ZERO, ASTRAL_GL_ONE);
  m_info[BackendBlendMode(false, blend_porter_duff_src_over).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ONE, ASTRAL_GL_ONE_MINUS_SRC_ALPHA);
  m_info[BackendBlendMode(false, blend_porter_duff_dst_over).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ONE_MINUS_DST_ALPHA, ASTRAL_GL_ONE);
  m_info[BackendBlendMode(false, blend_porter_duff_src_in).packed_value()] = PerBlendMode(*this, ASTRAL_GL_DST_ALPHA, ASTRAL_GL_ZERO);
  m_info[BackendBlendMode(false, blend_porter_duff_dst_in).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ZERO, ASTRAL_GL_SRC_ALPHA);
  m_info[BackendBlendMode(false, blend_porter_duff_src_out).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ONE_MINUS_DST_ALPHA, ASTRAL_GL_ZERO);
  m_info[BackendBlendMode(false, blend_porter_duff_dst_out).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ZERO, ASTRAL_GL_ONE_MINUS_SRC_ALPHA);
  m_info[BackendBlendMode(false, blend_porter_duff_src_atop).packed_value()] = PerBlendMode(*this, ASTRAL_GL_DST_ALPHA, ASTRAL_GL_ONE_MINUS_SRC_ALPHA);
  m_info[BackendBlendMode(false, blend_porter_duff_dst_atop).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ONE_MINUS_DST_ALPHA, ASTRAL_GL_SRC_ALPHA);
  m_info[BackendBlendMode(false, blend_porter_duff_xor).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ONE_MINUS_DST_ALPHA, ASTRAL_GL_ONE_MINUS_SRC_ALPHA);
  m_info[BackendBlendMode(false, blend_porter_duff_plus).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ONE, ASTRAL_GL_ONE_MINUS_SRC_ALPHA);
  m_info[BackendBlendMode(false, blend_porter_duff_modulate).packed_value()] = PerBlendMode(*this, ASTRAL_GL_DST_COLOR, ASTRAL_GL_ONE, ASTRAL_GL_FUNC_REVERSE_SUBTRACT, "astral_blending_modulate.glsl.resource_string");
  m_info[BackendBlendMode(false, blend_mode_max).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ONE, ASTRAL_GL_ONE, ASTRAL_GL_MAX);
  m_info[BackendBlendMode(false, blend_mode_min).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ONE, ASTRAL_GL_ONE, ASTRAL_GL_MIN);
  m_info[BackendBlendMode(false, blend_mode_difference).packed_value()] = PerBlendMode(*this, "astral_blending_difference.glsl.resource_string");
  m_info[BackendBlendMode(false, blend_mode_screen).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ONE, ASTRAL_GL_ONE_MINUS_SRC_COLOR);
  m_info[BackendBlendMode(false, blend_mode_multiply).packed_value()] = PerBlendMode(*this, "astral_blending_multiply.glsl.resource_string");
  m_info[BackendBlendMode(false, blend_mode_overlay).packed_value()] = PerBlendMode(*this, "astral_blending_overlay.glsl.resource_string");
  m_info[BackendBlendMode(false, blend_mode_darken).packed_value()] = PerBlendMode(*this, "astral_blending_darken.glsl.resource_string");
  m_info[BackendBlendMode(false, blend_mode_lighten).packed_value()] = PerBlendMode(*this, "astral_blending_lighten.glsl.resource_string");
  m_info[BackendBlendMode(false, blend_mode_color_dodge).packed_value()] = PerBlendMode(*this, "astral_blending_color_dodge.glsl.resource_string");
  m_info[BackendBlendMode(false, blend_mode_color_burn).packed_value()] = PerBlendMode(*this, "astral_blending_color_burn.glsl.resource_string");
  m_info[BackendBlendMode(false, blend_mode_hardlight).packed_value()] = PerBlendMode(*this, "astral_blending_hardlight.glsl.resource_string");
  m_info[BackendBlendMode(false, blend_mode_softlight).packed_value()] = PerBlendMode(*this, "astral_blending_softlight.glsl.resource_string");
  m_info[BackendBlendMode(false, blend_mode_exclusion).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ONE_MINUS_DST_COLOR, ASTRAL_GL_ONE, ASTRAL_GL_ONE_MINUS_SRC_COLOR, ASTRAL_GL_ONE_MINUS_SRC_ALPHA);
  m_info[BackendBlendMode(false, blend_mode_hue).packed_value()] = PerBlendMode(*this, "astral_blending_hue.glsl.resource_string");
  m_info[BackendBlendMode(false, blend_mode_saturation).packed_value()] = PerBlendMode(*this, "astral_blending_saturation.glsl.resource_string");
  m_info[BackendBlendMode(false, blend_mode_color).packed_value()] = PerBlendMode(*this, "astral_blending_color.glsl.resource_string");
  m_info[BackendBlendMode(false, blend_mode_luminosity).packed_value()] = PerBlendMode(*this, "astral_blending_luminosity.glsl.resource_string");

  m_info[BackendBlendMode(true, blend_porter_duff_clear).packed_value()] = PerBlendMode(*this, ASTRAL_GL_DST_COLOR, ASTRAL_GL_ONE, ASTRAL_GL_FUNC_REVERSE_SUBTRACT, "astral_blending_clear.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_porter_duff_src).packed_value()] = PerBlendMode(*this, "astral_blending_src.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_porter_duff_dst).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ZERO, ASTRAL_GL_ONE);
  m_info[BackendBlendMode(true, blend_porter_duff_src_over).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ONE, ASTRAL_GL_ONE_MINUS_SRC_ALPHA);
  m_info[BackendBlendMode(true, blend_porter_duff_dst_over).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ONE_MINUS_DST_ALPHA, ASTRAL_GL_ONE);
  m_info[BackendBlendMode(true, blend_porter_duff_src_in).packed_value()] = PerBlendMode(*this, "astral_blending_src_in.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_porter_duff_dst_in).packed_value()] = PerBlendMode(*this, ASTRAL_GL_DST_COLOR, ASTRAL_GL_ONE, ASTRAL_GL_FUNC_REVERSE_SUBTRACT, "astral_blending_dst_in.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_porter_duff_src_out).packed_value()] = PerBlendMode(*this, "astral_blending_src_out.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_porter_duff_dst_out).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ZERO, ASTRAL_GL_ONE_MINUS_SRC_ALPHA);
  m_info[BackendBlendMode(true, blend_porter_duff_src_atop).packed_value()] = PerBlendMode(*this, ASTRAL_GL_DST_ALPHA, ASTRAL_GL_ONE_MINUS_SRC_ALPHA);
  m_info[BackendBlendMode(true, blend_porter_duff_dst_atop).packed_value()] = PerBlendMode(*this, "astral_blending_dst_atop.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_porter_duff_xor).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ONE_MINUS_DST_ALPHA, ASTRAL_GL_ONE_MINUS_SRC_ALPHA);
  m_info[BackendBlendMode(true, blend_porter_duff_plus).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ONE, ASTRAL_GL_ONE_MINUS_SRC_ALPHA);
  m_info[BackendBlendMode(true, blend_porter_duff_modulate).packed_value()] = PerBlendMode(*this, ASTRAL_GL_DST_COLOR, ASTRAL_GL_ONE, ASTRAL_GL_FUNC_REVERSE_SUBTRACT, "astral_blending_modulate.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_mode_max).packed_value()] = PerBlendMode(*this, "astral_blending_max.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_mode_min).packed_value()] = PerBlendMode(*this, "astral_blending_min.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_mode_difference).packed_value()] = PerBlendMode(*this, "astral_blending_difference.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_mode_screen).packed_value()] = PerBlendMode(*this, ASTRAL_GL_ONE, ASTRAL_GL_ONE_MINUS_SRC_COLOR);
  m_info[BackendBlendMode(true, blend_mode_multiply).packed_value()] = PerBlendMode(*this, "astral_blending_multiply.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_mode_overlay).packed_value()] = PerBlendMode(*this, "astral_blending_overlay.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_mode_darken).packed_value()] = PerBlendMode(*this, "astral_blending_darken.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_mode_lighten).packed_value()] = PerBlendMode(*this, "astral_blending_lighten.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_mode_color_dodge).packed_value()] = PerBlendMode(*this, "astral_blending_color_dodge.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_mode_color_burn).packed_value()] = PerBlendMode(*this, "astral_blending_color_burn.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_mode_hardlight).packed_value()] = PerBlendMode(*this, "astral_blending_hardlight.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_mode_softlight).packed_value()] = PerBlendMode(*this, "astral_blending_softlight.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_mode_exclusion).packed_value()] = PerBlendMode(*this, "astral_blending_exclusion.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_mode_hue).packed_value()] = PerBlendMode(*this, "astral_blending_hue.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_mode_saturation).packed_value()] = PerBlendMode(*this, "astral_blending_saturation.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_mode_color).packed_value()] = PerBlendMode(*this, "astral_blending_color.glsl.resource_string");
  m_info[BackendBlendMode(true, blend_mode_luminosity).packed_value()] = PerBlendMode(*this, "astral_blending_luminosity.glsl.resource_string");
}

unsigned int
astral::gl::RenderEngineGL3::Implement::BlendBuilder::
add_shader(const std::string &shader)
{
  ASTRALassert(m_fetch_shader_id.find(shader) == m_fetch_shader_id.end());

  m_fetch_shader_id[shader] = m_shaders.size();
  m_shaders.push_back(shader);

  return m_shaders.size() - 1u;
}

unsigned int
astral::gl::RenderEngineGL3::Implement::BlendBuilder::
fetch_shader_id(const std::string &shader)
{
  auto iter = m_fetch_shader_id.find(shader);

  if (iter == m_fetch_shader_id.end())
    {
      return add_shader(shader);
    }
  else
    {
      return iter->second;
    }
}

void
astral::gl::RenderEngineGL3::Implement::BlendBuilder::
set_blend_mode_information(BlendModeInformation *dst) const
{
  for (unsigned int i = 0; i < 2; ++i)
    {
      bool partial_coverage(i == 1u);
      for (unsigned int blend_mode = 0; blend_mode < number_blend_modes; ++blend_mode)
        {
          BackendBlendMode b(static_cast<enum blend_mode_t>(blend_mode), partial_coverage);
          dst->requires_framebuffer_pixels(b, m_info[b.packed_value()].pixels_needed());
        }
    }
}

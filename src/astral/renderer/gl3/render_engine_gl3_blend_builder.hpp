/*!
 * \file render_engine_gl3_blend_builder.hpp
 * \brief file render_engine_gl3_blend_builder.hpp
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

#ifndef ASTRAL_RENDER_ENGINE_GL3_BLEND_BUILDER_HPP
#define ASTRAL_RENDER_ENGINE_GL3_BLEND_BUILDER_HPP

#include <vector>
#include <astral/renderer/gl3/render_engine_gl3.hpp>
#include "render_engine_gl3_implement.hpp"

/* Class to encapsulate for each (partial_coverage, blend_mode)
 * value pair:
 *   - GL API blend state
 *   - what GLSL source fragment to use
 */
class astral::gl::RenderEngineGL3::Implement::BlendBuilder
{
public:
  enum blend_off_t
    {
      blend_off
    };

  class PerBlendMode
  {
  public:
    PerBlendMode(void):
      m_shader_id(0),
      m_pixel_needs(BlendModeInformation::does_not_need_framebuffer_pixels),
      m_enable_gl_blend(true),
      m_blend_equation_rgb(ASTRAL_GL_INVALID_ENUM),
      m_blend_equation_a(ASTRAL_GL_INVALID_ENUM),
      m_blend_func_src_rgb(ASTRAL_GL_INVALID_ENUM),
      m_blend_func_src_a(ASTRAL_GL_INVALID_ENUM),
      m_blend_func_dst_rgb(ASTRAL_GL_INVALID_ENUM),
      m_blend_func_dst_a(ASTRAL_GL_INVALID_ENUM)
    {}

    const std::string&
    shader(const BlendBuilder &bb) const
    {
      return bb.m_shaders[m_shader_id];
    }

    unsigned int
    shader_id(void) const
    {
      return m_shader_id;
    }

    enum BlendModeInformation::requires_framebuffer_pixels_t
    pixels_needed(void) const
    {
      return m_pixel_needs;
    }

    void
    emit_gl_blend_state(void) const;

    bool
    requires_emit_gl_blend_state(const PerBlendMode &rhs) const
    {
      return m_enable_gl_blend != rhs.m_enable_gl_blend
        || (m_enable_gl_blend && (m_blend_equation_rgb != rhs.m_blend_equation_rgb
                                  || m_blend_equation_a != rhs.m_blend_equation_a
                                  || m_blend_func_src_rgb != rhs.m_blend_func_src_rgb
                                  || m_blend_func_dst_rgb != rhs.m_blend_func_dst_rgb
                                  || m_blend_func_src_a != rhs.m_blend_func_src_a
                                  || m_blend_func_dst_a != rhs.m_blend_func_dst_a));
    }

  private:
    friend class BlendBuilder;

    PerBlendMode(BlendBuilder &builder, const std::string &shader):
      m_shader_id(builder.fetch_shader_id(shader)),
      m_pixel_needs(BlendModeInformation::requires_framebuffer_pixels_opaque_draw),
      m_enable_gl_blend(false),
      m_blend_equation_rgb(ASTRAL_GL_INVALID_ENUM),
      m_blend_equation_a(ASTRAL_GL_INVALID_ENUM),
      m_blend_func_src_rgb(ASTRAL_GL_INVALID_ENUM),
      m_blend_func_src_a(ASTRAL_GL_INVALID_ENUM),
      m_blend_func_dst_rgb(ASTRAL_GL_INVALID_ENUM),
      m_blend_func_dst_a(ASTRAL_GL_INVALID_ENUM)
    {}

    PerBlendMode(BlendBuilder &builder, enum blend_off_t,
                 const std::string &shader = "astral_blending_fixed_funciton.glsl.resource_string"):
      m_shader_id(builder.fetch_shader_id(shader)),
      m_pixel_needs(BlendModeInformation::does_not_need_framebuffer_pixels),
      m_enable_gl_blend(false),
      m_blend_equation_rgb(ASTRAL_GL_INVALID_ENUM),
      m_blend_equation_a(ASTRAL_GL_INVALID_ENUM),
      m_blend_func_src_rgb(ASTRAL_GL_INVALID_ENUM),
      m_blend_func_src_a(ASTRAL_GL_INVALID_ENUM),
      m_blend_func_dst_rgb(ASTRAL_GL_INVALID_ENUM),
      m_blend_func_dst_a(ASTRAL_GL_INVALID_ENUM)
    {}

    PerBlendMode(BlendBuilder &builder,
                 astral_GLenum src_factor, astral_GLenum dst_factor,
                 astral_GLenum equation = ASTRAL_GL_FUNC_ADD,
                 const std::string &shader = "astral_blending_fixed_funciton.glsl.resource_string"):
      m_shader_id(builder.fetch_shader_id(shader)),
      m_pixel_needs(BlendModeInformation::does_not_need_framebuffer_pixels),
      m_enable_gl_blend(true),
      m_blend_equation_rgb(equation),
      m_blend_equation_a(equation),
      m_blend_func_src_rgb(src_factor),
      m_blend_func_src_a(src_factor),
      m_blend_func_dst_rgb(dst_factor),
      m_blend_func_dst_a(dst_factor)
    {}

    PerBlendMode(BlendBuilder &builder,
                 astral_GLenum src_rgb_factor, astral_GLenum src_a_factor,
                 astral_GLenum dst_rgb_factor, astral_GLenum dst_a_factor,
                 astral_GLenum equation = ASTRAL_GL_FUNC_ADD,
                 const std::string &shader = "astral_blending_fixed_funciton.glsl.resource_string"):
      m_shader_id(builder.fetch_shader_id(shader)),
      m_pixel_needs(BlendModeInformation::does_not_need_framebuffer_pixels),
      m_enable_gl_blend(true),
      m_blend_equation_rgb(equation),
      m_blend_equation_a(equation),
      m_blend_func_src_rgb(src_rgb_factor),
      m_blend_func_src_a(src_a_factor),
      m_blend_func_dst_rgb(dst_rgb_factor),
      m_blend_func_dst_a(dst_a_factor)
    {}

    PerBlendMode(BlendBuilder &builder,
                 astral_GLenum src_rgb_factor, astral_GLenum src_a_factor,
                 astral_GLenum dst_rgb_factor, astral_GLenum dst_a_factor,
                 astral_GLenum equation_rgb, astral_GLenum equation_a,
                 const std::string &shader = "astral_blending_fixed_funciton.glsl.resource_string"):
      m_shader_id(builder.fetch_shader_id(shader)),
      m_pixel_needs(BlendModeInformation::does_not_need_framebuffer_pixels),
      m_enable_gl_blend(true),
      m_blend_equation_rgb(equation_rgb),
      m_blend_equation_a(equation_a),
      m_blend_func_src_rgb(src_rgb_factor),
      m_blend_func_src_a(src_a_factor),
      m_blend_func_dst_rgb(dst_rgb_factor),
      m_blend_func_dst_a(dst_a_factor)
    {}

    /* name of shader resource */
    unsigned int m_shader_id;

    /* how and if needs a copy of the framebuffer pixels */
    enum BlendModeInformation::requires_framebuffer_pixels_t m_pixel_needs;

    /* GL Blend state */
    bool m_enable_gl_blend;
    astral_GLenum m_blend_equation_rgb, m_blend_equation_a;
    astral_GLenum m_blend_func_src_rgb, m_blend_func_src_a;
    astral_GLenum m_blend_func_dst_rgb, m_blend_func_dst_a;
  };

  explicit
  BlendBuilder(const Config &config);

  const PerBlendMode&
  info(BackendBlendMode mode) const
  {
    return m_info[mode.packed_value()];
  }

  const std::string&
  shader(unsigned int shader_id);

  void
  set_blend_mode_information(BlendModeInformation *dst) const;

private:
  unsigned int
  add_shader(const std::string &shader);

  unsigned int
  fetch_shader_id(const std::string &shader);

  vecN<PerBlendMode, BackendBlendMode::number_packed_values> m_info;
  std::vector<std::string> m_shaders;
  std::map<std::string, int> m_fetch_shader_id;
};

#endif

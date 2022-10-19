/*!
 * \file custom_stroke_shader_generator.hpp
 * \brief custom_stroke_shader_generator.hpp
 *
 * Copyright 2021 by InvisionApp.
 *
 * Contact kevinrogovin@invisionapp.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef CUSTOM_STROKE_SHADER_GENERATOR_HPP
#define CUSTOM_STROKE_SHADER_GENERATOR_HPP

#include <astral/renderer/renderer.hpp>
#include <astral/renderer/gl3/render_engine_gl3.hpp>
#include <astral/renderer/gl3/stroke_shader_gl3.hpp>

class CustomStrokeShaderGenerator
{
public:
  CustomStrokeShaderGenerator(astral::gl::RenderEngineGL3 &engine):
    m_engine(engine)
  {}

  void
  generate_stroke_shader(const astral::gl::StrokeShaderGL3 &in_shader,
                         astral::gl::StrokeShaderGL3 &out_shader);

  void
  generate_stroke_shader(const astral::gl::StrokeShaderGL3 &in_shader,
                         astral::reference_counted_ptr<const astral::MaskStrokeShader> &out_shader)
  {
    astral::gl::StrokeShaderGL3 tmp;
    generate_stroke_shader(in_shader, out_shader, tmp);
  }

  void
  generate_stroke_shader(const astral::gl::StrokeShaderGL3 &in_shader,
                         astral::reference_counted_ptr<const astral::MaskStrokeShader> &out_shader,
                         astral::gl::StrokeShaderGL3 &out_gl3_shader)
  {
    generate_stroke_shader(in_shader, out_gl3_shader);
    out_shader = out_gl3_shader.create_mask_stroke_shader(astral::gl::StrokeShaderGL3::include_cap_shaders);
  }

protected:
  astral::gl::RenderEngineGL3 &m_engine;

private:
  virtual
  astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3>
  generate_line_stroke_shader(astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> in_shader) = 0;

  virtual
  astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3>
  generate_biarc_stroke_shader(astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> in_shader) = 0;

  virtual
  astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3>
  generate_join_cap_stroke_shader(astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> in_shader) = 0;

  virtual
  astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3>
  generate_capper_shader(astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> in_shader) = 0;
};


#endif

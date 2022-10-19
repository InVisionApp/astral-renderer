/*!
 * \file wavy_graph_stroke.hpp
 * \brief wavy_graph_stroke.hpp
 *
 * Copyright 2020 by InvisionApp.
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

#ifndef WAVY_GRAPH_STOKE_HPP
#define WAVY_GRAPH_STOKE_HPP

#include "wavy_stroke.hpp"
#include "graph_stroke.hpp"

typedef CompoundStrokeItemDataPacker<WavyPattern, GraphStrokeItemDataPacker> WavyGraphStrokeItemDataPacker;

class WavyGraphStrokeShaderGenerator
{
public:
  WavyGraphStrokeShaderGenerator(astral::gl::RenderEngineGL3 &engine):
    m_GraphStrokeShaderGenerator(engine),
    m_WavyStrokeShaderGenerator("base_shader::astral_chain_stroke_distance_along_contour",
                                "base_shader::astral_chain_stroke_radius",
                                engine)
  {}

  void
  generate_stroke_shader(const astral::gl::StrokeShaderGL3 &in_shader,
                         astral::gl::StrokeShaderGL3 &out_shader)
  {
    astral::gl::StrokeShaderGL3 graph_stroker;

    m_GraphStrokeShaderGenerator.generate_stroke_shader(in_shader, graph_stroker);
    m_WavyStrokeShaderGenerator.generate_stroke_shader(graph_stroker, out_shader);
  }

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

private:
  GraphStrokeShaderGenerator m_GraphStrokeShaderGenerator;
  WavyStrokeShaderGenerator m_WavyStrokeShaderGenerator;
};

#endif

/*!
 * \file graph_stroke.hpp
 * \brief graph_stroke.hpp
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

#ifndef GRAPH_STOKE_HPP
#define GRAPH_STOKE_HPP

#include <astral/renderer/renderer.hpp>
#include <astral/renderer/gl3/render_engine_gl3.hpp>
#include <astral/renderer/gl3/stroke_shader_gl3.hpp>
#include <astral/renderer/gl3/item_shader_gl3.hpp>

#include "compound_stroke_item_data_packer.hpp"
#include "custom_stroke_shader_generator.hpp"

class GraphPattern
{
public:
  GraphPattern(void):
    m_count(3.0f),
    m_thickness(0.05f)
  {}

  /* the number of lines on each side to add */
  float m_count;

  /* the thickness of each line */
  float m_thickness;

  /* spacing along the contour */
  float m_spacing;

  static
  unsigned int
  item_data_size(void)
  {
    return graph_data_size;
  }

  void
  pack_item_data(const astral::StrokeParameters &params, astral::c_array<astral::gvec4> dst) const;

private:
  enum
    {
      graph_data_size = 1
    };
};

typedef CompoundStrokeItemDataPacker<GraphPattern> GraphStrokeItemDataPacker;

class GraphStrokeShaderGenerator:public CustomStrokeShaderGenerator
{
public:
  GraphStrokeShaderGenerator(astral::gl::RenderEngineGL3 &engine):
    CustomStrokeShaderGenerator(engine)
  {}

private:
  astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3>
  generate_edge_stroke_shader(astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> in_shader);

  astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3>
  generate_generic(astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> in_shader);

  virtual
  astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3>
  generate_line_stroke_shader(astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> in_shader) override
  {
    return generate_edge_stroke_shader(in_shader);
  }

  virtual
  astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3>
  generate_biarc_stroke_shader(astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> in_shader) override
  {
    return generate_edge_stroke_shader(in_shader);
  }

  virtual
  astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3>
  generate_join_cap_stroke_shader(astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> in_shader) override;

  virtual
  astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3>
  generate_capper_shader(astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> in_shader) override
  {
    return generate_edge_stroke_shader(in_shader);
  }
};

#endif

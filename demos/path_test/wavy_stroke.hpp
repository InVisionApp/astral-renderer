/*!
 * \file wavy_stroke.hpp
 * \brief wavy_stroke.hpp
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

#ifndef WAVY_STOKE_HPP
#define WAVY_STOKE_HPP

#include <astral/renderer/renderer.hpp>
#include <astral/renderer/gl3/render_engine_gl3.hpp>
#include <astral/renderer/gl3/stroke_shader_gl3.hpp>
#include <astral/renderer/gl3/item_shader_gl3.hpp>

#include "compound_stroke_item_data_packer.hpp"
#include "custom_stroke_shader_generator.hpp"

class WavyPattern
{
public:
  float m_domain_coeff, m_phase;
  astral::vecN<float, 4> m_cos_coeffs;
  astral::vecN<float, 4> m_sin_coeffs;

  static
  unsigned int
  item_data_size(void)
  {
    return wavy_data_size;
  }

  void
  pack_item_data(const astral::StrokeParameters &stroke_params,
                 astral::c_array<astral::gvec4> dst) const
  {
    float sum(0.0f);
    for (int c = 0; c < 4; ++c)
      {
        dst[0][c].f = m_cos_coeffs[c];
        dst[1][c].f = m_sin_coeffs[c];

        sum += astral::t_abs(m_cos_coeffs[c]);
        sum += astral::t_abs(m_sin_coeffs[c]);
      }

    dst[2].x().f = m_domain_coeff;
    dst[2].y().f = 1.0f / sum;
    dst[2].z().f = m_phase;
    dst[2].w().f = stroke_params.m_width;
  }

private:
  enum
    {
      wavy_data_size = 3
    };
};

typedef CompoundStrokeItemDataPacker<WavyPattern> WavyStrokeItemDataPacker;

class WavyStrokeShaderGenerator:public CustomStrokeShaderGenerator
{
public:
  WavyStrokeShaderGenerator(const std::string &chain_stroke_distance_along_contour,
                            const std::string &chain_stroke_radius,
                            astral::gl::RenderEngineGL3 &engine):
    CustomStrokeShaderGenerator(engine),
    m_chain_stroke_distance_along_contour(chain_stroke_distance_along_contour),
    m_chain_stroke_radius(chain_stroke_radius)
  {}

  WavyStrokeShaderGenerator(astral::gl::RenderEngineGL3 &engine):
    WavyStrokeShaderGenerator("astral_chain_stroke_distance_along_contour",
                              "astral_chain_stroke_radius",
                              engine)
  {}

private:
  astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3>
  generate_edge_stroke_shader(astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> in_shader);

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
  generate_capper_shader(astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> in_shader) override;

  void
  compute_coeff_code(astral::gl::ShaderSource &src,
                     const std::string &distance,
                     const std::string &item_data_location,
                     const std::string &out_f);

  std::string m_chain_stroke_distance_along_contour, m_chain_stroke_radius;
};

#endif

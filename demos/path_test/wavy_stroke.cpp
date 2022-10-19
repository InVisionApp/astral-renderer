/*!
 * \file wavy_stroke.cpp
 * \brief wavy_stroke.cpp
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

#include "wavy_stroke.hpp"

///////////////////////////////////////
//WavyStrokeShaderGenerator methods
void
WavyStrokeShaderGenerator::
compute_coeff_code(astral::gl::ShaderSource &str,
                   const std::string &distance,
                   const std::string &item_data_location,
                   const std::string &out_f)
{
  str << "    {\n"
      << "        vec4 tmp, cos_coeffs, sin_coeffs, cos_tuple, sin_tuple;\n"
      << "        float coeff, inverse_sum, phase, width, a, r;\n"
      << "        cos_coeffs = astral_read_item_dataf(" << item_data_location << ");\n"
      << "        sin_coeffs = astral_read_item_dataf(" << item_data_location << " + 1u);\n"
      << "        tmp = astral_read_item_dataf(" << item_data_location << " + 2u);\n"
      << "        coeff = tmp.x;\n"
      << "        inverse_sum = tmp.y;\n"
      << "        phase = tmp.z;\n"
      << "        width = tmp.w;\n"
      << "        r = coeff * " << distance << " + phase;\n"
      << "        cos_tuple = vec4(cos(r), cos(2.0 * r), cos(3.0 * r), cos(4.0 * r));\n"
      << "        sin_tuple = vec4(sin(r), sin(2.0 * r), sin(3.0 * r), sin(4.0 * r));\n"
      << "        a = inverse_sum * (dot(cos_coeffs, cos_tuple) + dot(sin_coeffs, sin_tuple));\n"
      << "        " << out_f << " = abs(a);\n"
      << "    }\n";
}

astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3>
WavyStrokeShaderGenerator::
generate_edge_stroke_shader(astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> in_shader)
{
  /* Modify a line and quadratic frag shaders to chain to in_shader's
   * but to apply an masking operation that makes the stroking width
   * wavy. These both work by modfiying astral_chain_stroke_radius
   * to get smaller to give the wavy effect. We do this by implementing
   * astral_pre_frag_shader() as calling in_shader's astral_pre_frag_shader(),
   * and then modifying the values.
   */
  ASTRALassert(in_shader);

  astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> return_value;
  astral::gl::ShaderSource vert, frag;

  vert << "void astral_pre_vert_shader(in uint sub_shader, in uint item_data_location,\n"
       << "                            in vec4 a0, in AstralTransformation item_transformation)\n"
       << "{\n"
       << "    base::astral_pre_vert_shader(sub_shader, item_data_location + uint(" << WavyPattern::item_data_size() << "),\n"
       << "                                 a0, item_transformation);\n"
       << "}\n\n"
       << "vec2 astral_vert_shader(in uint sub_shader, in uint item_data_location,\n"
       << "                        in vec4 a0,\n"
       << "                        in AstralTransformation item_transformation)\n"
       << "{\n"
       << "    return base::astral_vert_shader(sub_shader, item_data_location + uint(" << WavyPattern::item_data_size() << "),\n"
       << "                                    a0, item_transformation);\n"
       << "}\n\n";

  frag << "void astral_pre_frag_shader(in uint sub_shader, in uint item_data_location)\n"
       << "{\n"
       << "   float f;\n"
       << "    base::astral_pre_frag_shader(sub_shader, item_data_location + uint(" << WavyPattern::item_data_size() << "));\n";

  compute_coeff_code(frag, "base::" + m_chain_stroke_distance_along_contour, "item_data_location", "f");

  frag << "    base::" << m_chain_stroke_radius << " *= f;\n"
       << "}\n"
       << "void astral_frag_shader(in uint sub_shader, in uint item_data_location,\n"
       << "                        out vec4 base_color)\n"
       << "{\n"
       << "    base::astral_frag_shader(sub_shader, item_data_location + uint(" << WavyPattern::item_data_size() << "),\n"
       << "                             base_color);\n"
       << "}\n";

  return_value = astral::gl::ItemShaderBackendGL3::create(m_engine, in_shader->type(), vert, frag,
                                                          astral::gl::ShaderSymbolList(),
                                                          astral::gl::ItemShaderBackendGL3::DependencyList().add("base", *in_shader),
                                                          in_shader->num_sub_shaders());

  return return_value;
}

astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3>
WavyStrokeShaderGenerator::
generate_join_cap_stroke_shader(astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> in_shader)
{
  /* Modfiy the join and cap vertex shaders to change
   * astral_chain_stroke_radius to make the stroke size
   * adjust as the join or cap. We do this by implementing
   * astral_pre_vert_shader() as first calling in_shader's
   * astral_pre_vert_shader() and then modifying the
   * astral_chain_stroke_radius.
   */
  if (!in_shader)
    {
      return in_shader;
    }

  astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> return_value;
  astral::gl::ShaderSource vert, frag;

  vert << "void astral_pre_vert_shader(in uint sub_shader, in uint item_data_location,\n"
       << "                            in vec4 a0, in AstralTransformation item_transformation)\n"
       << "{\n"
       << "    float f;\n"
       << "    base::astral_pre_vert_shader(sub_shader, item_data_location + uint(" << WavyPattern::item_data_size() << "),\n"
       << "                                 a0, item_transformation);\n";

  compute_coeff_code(vert, "base::" + m_chain_stroke_distance_along_contour, "item_data_location", "f");
  vert << "    base::" << m_chain_stroke_radius << " *= f;\n"
       << "}\n\n"
       << "vec2 astral_vert_shader(in uint sub_shader, in uint item_data_location,\n"
       << "                        in vec4 a0,\n"
       << "                        in AstralTransformation item_transformation)\n"
       << "{\n"
       << "    return base::astral_vert_shader(sub_shader, item_data_location + uint(" << WavyPattern::item_data_size() << "),\n"
       << "                                    a0, item_transformation);\n"
       << "}\n\n";

  frag << "void astral_pre_frag_shader(in uint sub_shader, in uint item_data_location)\n"
       << "{\n"
       << "   float f;\n"
       << "    base::astral_pre_frag_shader(sub_shader, item_data_location + uint(" << WavyPattern::item_data_size() << "));\n"
       << "}\n"
       << "void astral_frag_shader(in uint sub_shader, in uint item_data_location,\n"
       << "                        out vec4 base_color)\n"
       << "{\n"
       << "    base::astral_frag_shader(sub_shader, item_data_location + uint(" << WavyPattern::item_data_size() << "),\n"
       << "                             base_color);\n"
       << "}\n";

  return_value = astral::gl::ItemShaderBackendGL3::create(m_engine, in_shader->type(), vert, frag,
                                                          astral::gl::ShaderSymbolList(),
                                                          astral::gl::ItemShaderBackendGL3::DependencyList().add("base", *in_shader),
                                                          in_shader->num_sub_shaders());

  return return_value;
}

astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3>
WavyStrokeShaderGenerator::
generate_capper_shader(astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> in_shader)
{
  /* Modfiy the join and cap vertex shaders to change
   * astral_chain_stroke_radius to make the stroke size
   * adjust as the join or cap. We do this by implementing
   * astral_pre_vert_shader() as first calling in_shader's
   * astral_pre_vert_shader() and then modifying the
   * astral_chain_stroke_radius.
   */
  if (!in_shader)
    {
      return in_shader;
    }

  return generate_edge_stroke_shader(in_shader);
}

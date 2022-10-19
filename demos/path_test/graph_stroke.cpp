/*!
 * \file graph_stroke.cpp
 * \brief graph_stroke.cpp
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

#include "graph_stroke.hpp"

///////////////////////////////////
// GraphPattern methods
void
GraphPattern::
pack_item_data(const astral::StrokeParameters&, astral::c_array<astral::gvec4> dst) const
{
  dst[0].x().f = m_count;
  dst[0].y().f = m_thickness;
  dst[0].z().f = m_spacing;
}

//////////////////////////////////
// GraphStrokeShaderGenerator methods
astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3>
GraphStrokeShaderGenerator::
generate_generic(astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> in_shader)
{
  ASTRALassert(in_shader);

  astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> return_value;
  astral::gl::ShaderSource vert, frag;

  vert << "void astral_pre_vert_shader(in uint sub_shader, in uint item_data_location,\n"
       << "                            in vec4 a0, in AstralTransformation item_transformation)\n"
       << "{\n"
       << "    base_shader::astral_pre_vert_shader(sub_shader, item_data_location + uint(" << GraphPattern::item_data_size() << "),\n"
       << "                                 a0, item_transformation);\n"
       << "}\n\n"
       << "vec2 astral_vert_shader(in uint sub_shader, in uint item_data_location,\n"
       << "                        in vec4 a0,\n"
       << "                        in AstralTransformation item_transformation)\n"
       << "{\n"
       << "    return base_shader::astral_vert_shader(sub_shader, item_data_location + uint(" << GraphPattern::item_data_size() << "),\n"
       << "                                    a0, item_transformation);\n"
       << "}\n\n";

  frag << "void astral_pre_frag_shader(in uint sub_shader, in uint item_data_location)\n"
       << "{\n"
       << "    base_shader::astral_pre_frag_shader(sub_shader, item_data_location + uint(" << GraphPattern::item_data_size() << "));\n"
       << "}\n"
       << "void astral_frag_shader(in uint sub_shader, in uint item_data_location,\n"
       << "                        out vec4 base_color)\n"
       << "{\n"
       << "    base_shader::astral_frag_shader(sub_shader, item_data_location + uint(" << GraphPattern::item_data_size() << "),\n"
       << "                             base_color);\n"
       << "}\n";

  return_value = astral::gl::ItemShaderBackendGL3::create(m_engine, in_shader->type(), vert, frag,
                                                          astral::gl::ShaderSymbolList(),
                                                          astral::gl::ItemShaderBackendGL3::DependencyList().add("base_shader", *in_shader),
                                                          in_shader->num_sub_shaders());

  return return_value;
}

astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3>
GraphStrokeShaderGenerator::
generate_edge_stroke_shader(astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> in_shader)
{
  if (!in_shader)
    {
      return in_shader;
    }

  astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> return_value;
  astral::gl::ShaderSource vert, frag;

  vert << "void astral_pre_vert_shader(in uint sub_shader, in uint item_data_location,\n"
       << "                            in vec4 a0, in AstralTransformation item_transformation)\n"
       << "{\n"
       << "    base_shader::astral_pre_vert_shader(sub_shader, item_data_location + uint(" << GraphPattern::item_data_size() << "),\n"
       << "                                 a0, item_transformation);\n"
       << "}\n\n"
       << "vec2 astral_vert_shader(in uint sub_shader, in uint item_data_location,\n"
       << "                        in vec4 a0,\n"
       << "                        in AstralTransformation item_transformation)\n"
       << "{\n"
       << "    vec2 return_value;\n"
       << "    return_value = base_shader::astral_vert_shader(sub_shader, item_data_location + uint(" << GraphPattern::item_data_size() << "),\n"
       << "                                            a0, item_transformation);\n"
       << "    length_of_contour = base_shader::astral_chain_stroke_contour_length;\n"
       << "    return return_value;\n"
       << "}\n\n";

  frag.add_source("spacing.glsl.resource_string", astral::gl::ShaderSource::from_resource);

  frag << "void astral_pre_frag_shader(in uint sub_shader, in uint item_data_location)\n"
       << "{\n"
       << "    base_shader::astral_pre_frag_shader(sub_shader, item_data_location + uint(" << GraphPattern::item_data_size() << "));\n"
       << "}\n"
       << "void astral_frag_shader(in uint sub_shader, in uint item_data_location,\n"
       << "                        out vec4 base_color)\n"
       << "{\n"
       << "    float m, dx, dy, s_x, f, count_x;\n"
       << "    graph_stroke_properties P;\n"
       << "    base_shader::astral_frag_shader(sub_shader, item_data_location + uint(" << GraphPattern::item_data_size() << "),\n"
       << "                             base_color);\n"
       << "\n"
       << "    load_graph_stroke_properties(item_data_location, P);\n"
       << "    count_x = P.m_count;\n"
       << "\n"
       << "    m = compute_signed_distance(base_shader::astral_chain_stroke_perp_distance_to_curve, P.m_line_width, count_x, base_shader::astral_chain_stroke_radius);\n"
       << "    dx = base_shader::astral_chain_stroke_perp_distance_to_curve_gradient_x;\n"
       << "    dy = base_shader::astral_chain_stroke_perp_distance_to_curve_gradient_y;\n"
       << "    s_x = m * inversesqrt(dx * dx + dy * dy);\n"
       << "    s_x = clamp(s_x, -1.0, 1.0);\n"
       << "\n"
       << "    float s_y, count_y;\n"
       << "    count_y = round(length_of_contour / P.m_spacing);\n"
       << "    m = compute_signed_distance(base_shader::astral_chain_stroke_distance_along_contour, P.m_line_width, count_y, length_of_contour);\n"
       << "    dx = base_shader::astral_chain_stroke_distance_along_contour_gradient_x;\n"
       << "    dy = base_shader::astral_chain_stroke_distance_along_contour_gradient_y;\n"
       << "    s_y = m * inversesqrt(dx * dx + dy * dy);\n"
       << "    s_y = clamp(s_y, -1.0, 1.0);\n"
       << "\n"
       << "    f = astral_combine_signed_distances_union(s_x, s_y);\n";

  frag  << "    f = astral_combine_signed_distances_intersect(f, unpack_signed_distance(base_color.g));\n"
        << "    base_color.r = max(0.0, f);\n"
        << "    base_color.g = pack_signed_distance(f);\n"
        << "}\n";

  astral::gl::ShaderSymbolList symbols;
  astral::gl::ItemShaderBackendGL3::DependencyList dependency_list;

  dependency_list.add("base_shader", *in_shader);
  symbols.add_varying("length_of_contour",  astral::gl::ShaderVaryings::interpolator_flat);

  return_value = astral::gl::ItemShaderBackendGL3::create(m_engine, in_shader->type(), vert, frag,
                                                          symbols, dependency_list,
                                                          in_shader->num_sub_shaders());

  return return_value;
}

astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3>
GraphStrokeShaderGenerator::
generate_join_cap_stroke_shader(astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> in_shader)
{
  if (!in_shader)
    {
      return in_shader;
    }

  /* stroke and cap shaders only render the lines parallel to the stroke */
  astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> return_value;
  astral::gl::ShaderSource vert, frag;

  vert << "void astral_pre_vert_shader(in uint sub_shader, in uint item_data_location,\n"
       << "                            in vec4 a0, in AstralTransformation item_transformation)\n"
       << "{\n"
       << "    base_shader::astral_pre_vert_shader(sub_shader, item_data_location + uint(" << GraphPattern::item_data_size() << "),\n"
       << "                                 a0, item_transformation);\n"
       << "}\n\n"
       << "vec2 astral_vert_shader(in uint sub_shader, in uint item_data_location,\n"
       << "                        in vec4 a0,\n"
       << "                        in AstralTransformation item_transformation)\n"
       << "{\n"
       << "    vec2 return_value;\n"
       << "    return_value = base_shader::astral_vert_shader(sub_shader, item_data_location + uint(" << GraphPattern::item_data_size() << "),\n"
       << "                                            a0, item_transformation);\n"
    // the stroking radius is not available in the fragment shader, so we need to push it down..
       << "    stroke_radius = base_shader::astral_chain_stroke_radius;\n"
       << "    return return_value;\n"
       << "}\n\n";

  frag.add_source("spacing.glsl.resource_string", astral::gl::ShaderSource::from_resource);

  frag << "void astral_pre_frag_shader(in uint sub_shader, in uint item_data_location)\n"
       << "{\n"
       << "    base_shader::astral_pre_frag_shader(sub_shader, item_data_location + uint(" << GraphPattern::item_data_size() << "));\n"
       << "}\n"
       << "void astral_frag_shader(in uint sub_shader, in uint item_data_location,\n"
       << "                        out vec4 base_color)\n"
       << "{\n"
       << "    float m, dx, dy, s_x, f, count_x, L;\n"
       << "    graph_stroke_properties P;\n"
       << "    base_shader::astral_frag_shader(sub_shader, item_data_location + uint(" << GraphPattern::item_data_size() << "),\n"
       << "                             base_color);\n"
       << "\n"
       << "    load_graph_stroke_properties(item_data_location, P);\n"
       << "    count_x = P.m_count;\n"
       << "\n"

       << "#ifdef base_shader::astral_chain_stroke_normalized_distance_to_path\n"
       << "{\n"
       << "    L =  base_shader::astral_chain_stroke_normalized_distance_to_path * stroke_radius;\n"
       << "}\n"
       << "#else\n"
       << "{\n"
       << "    L = stroke_radius * length(vec2(base_shader::astral_chain_stroke_offset_vector_x, base_shader::astral_chain_stroke_offset_vector_y));\n"
       << "}\n"
       << "#endif\n"

       << "    m = compute_signed_distance(L, P.m_line_width, count_x, stroke_radius);\n"
       << "    dx = dFdx(L);\n"
       << "    dy = dFdy(L);\n"
       << "    s_x = m * inversesqrt(dx * dx + dy * dy);\n"
       << "    s_x = clamp(s_x, -1.0, 1.0);\n"
       << "\n"
       << "    f = astral_combine_signed_distances_intersect(s_x, unpack_signed_distance(base_color.g));\n"
       << "    base_color.r = max(0.0, f);\n"
       << "    base_color.g = pack_signed_distance(f);\n"
       << "}\n";

  astral::gl::ShaderSymbolList symbols;
  astral::gl::ItemShaderBackendGL3::DependencyList dependency_list;

  dependency_list.add("base_shader", *in_shader);
  symbols.add_varying("stroke_radius",  astral::gl::ShaderVaryings::interpolator_flat);

  return_value = astral::gl::ItemShaderBackendGL3::create(m_engine, in_shader->type(), vert, frag,
                                                          symbols, dependency_list,
                                                          in_shader->num_sub_shaders());

  return return_value;
}

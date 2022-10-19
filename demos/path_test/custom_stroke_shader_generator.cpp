/*!
 * \file custom_stroke_shader_generator.cpp
 * \brief custom_stroke_shader_generator.cpp
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

#include "custom_stroke_shader_generator.hpp"

/////////////////////////////////////////
// CustomStrokeShaderGenerator methods
void
CustomStrokeShaderGenerator::
generate_stroke_shader(const astral::gl::StrokeShaderGL3 &in_shader,
                       astral::gl::StrokeShaderGL3 &out_shader)
{
  out_shader.m_type = in_shader.m_type;
  out_shader.m_per_cap_shading = in_shader.m_per_cap_shading;

  out_shader.m_line = generate_line_stroke_shader(in_shader.m_line);
  out_shader.m_biarc_curve = generate_biarc_stroke_shader(in_shader.m_biarc_curve);
  for (unsigned int i = 0; i < astral::number_join_t; ++i)
    {
      out_shader.m_join[i] = generate_join_cap_stroke_shader(in_shader.m_join[i]);
    }
  out_shader.m_square_cap = generate_join_cap_stroke_shader(in_shader.m_square_cap);
  out_shader.m_rounded_cap = generate_join_cap_stroke_shader(in_shader.m_rounded_cap);

  for (unsigned int i = 0; i < astral::gl::StrokeShaderGL3::capper_primitive_count; ++i)
    {
      for (unsigned int j = 0; j < astral::number_cap_t; ++j)
        {
          out_shader.m_cappers[i][j] = generate_capper_shader(in_shader.m_cappers[i][j]);
        }
    }
}

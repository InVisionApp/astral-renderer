/*!
 * \file read_dash_pattern.cpp
 * \brief read_dash_pattern.cpp
 *
 * Adapted from: read_dash_pattern.cpp of FastUIDraw (https://github.com/intel/fastuidraw):
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */
#include "read_dash_pattern.hpp"

void
read_dash_pattern(std::vector<astral::StrokeShader::DashPatternElement> &pattern_out,
                  std::istream &input_stream)
{
  astral::StrokeShader::DashPatternElement dsh;

  pattern_out.clear();
  while(input_stream >> dsh.m_draw_length >> dsh.m_skip_length)
    {
      pattern_out.push_back(dsh);
    }
}

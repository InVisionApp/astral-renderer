/*!
 * \file read_dash_pattern.hpp
 * \brief read_dash_pattern.hpp
 *
 * Adapted from: read_dash_pattern.hpp of FastUIDraw (https://github.com/intel/fastuidraw):
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
#ifndef ASTRAL_DEMO_READ_DASH_PATTERN_HPP
#define ASTRAL_DEMO_READ_DASH_PATTERN_HPP

#include <istream>
#include <vector>
#include <astral/renderer/shader/stroke_shader.hpp>

void
read_dash_pattern(std::vector<astral::StrokeShader::DashPatternElement> &pattern_out,
                  std::istream &input_stream);

#endif

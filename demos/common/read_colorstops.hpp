/*!
 * \file read_colorstops.hpp
 * \brief read_colorstops.hpp
 *
 * Adapted from: read_colorstops.hpp of FastUIDraw (https://github.com/intel/fastuidraw):
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
#ifndef ASTRAL_DEMO_READ_COLORSTOPS_HPP
#define ASTRAL_DEMO_READ_COLORSTOPS_HPP

#include <istream>
#include <astral/renderer/colorstop_sequence.hpp>

/* Read a color stop from an std::stream. The file is formatted as:
 *  stop_time red green blue alpha
 *  stop_time red green blue alpha
 *  .
 *  .
 *
 * where each stop_time is a floating point value in the range [0.0, 1.0]
 * and the value red, green, blue and alpha are integers in the range [0,255]
 */
void
read_colorstops(std::vector<astral::ColorStop<astral::FixedPointColor_sRGB>> &seq, std::istream &input);

void
read_colorstops(std::vector<astral::ColorStop<astral::FixedPointColorLinear>> &seq, std::istream &input);

#endif

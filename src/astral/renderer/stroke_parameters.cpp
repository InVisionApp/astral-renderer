/*!
 * \file stroke_parameters.cpp
 * \brief file stroke_parameters.cpp
 *
 * Copyright 2022 by InvisionApp.
 *
 * Contact: kevinrogovin@invisionapp.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 */

#include <astral/renderer/stroke_parameters.hpp>

float
astral::StrokeParameters::hairline_pixel_radius(void)
{
  return ASTRAL_HALF_SQRT2;
}

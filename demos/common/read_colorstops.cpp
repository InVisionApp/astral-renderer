/*!
 * \file read_colorstops.cpp
 * \brief read_colorstops.cpp
 *
 * Adapted from: read_colorstops.cpp of FastUIDraw (https://github.com/intel/fastuidraw):
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
#include "read_colorstops.hpp"

template<typename T>
void
read_colorstops_implement(std::vector<astral::ColorStop<T>> &seq, std::istream &input)
{
  while(input)
    {
      float t;
      unsigned int r, g, b, a;

      input >> t;
      input >> r;
      input >> g;
      input >> b;
      input >> a;

      if (!input.fail())
        {
          seq.push_back(astral::ColorStop<T>()
                        .color(T(r, g, b, a))
                        .t(t));
        }
    }
}

void
read_colorstops(std::vector<astral::ColorStop<astral::FixedPointColor_sRGB>> &seq, std::istream &input)
{
  read_colorstops_implement(seq, input);
}

void
read_colorstops(std::vector<astral::ColorStop<astral::FixedPointColorLinear>> &seq, std::istream &input)
{
  read_colorstops_implement(seq, input);
}

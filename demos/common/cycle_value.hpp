/*!
 * \file cycle_value.hpp
 * \brief cycle_value.hpp
 *
 * Adapted from: cycle_value.hpp of FastUIDraw (https://github.com/intel/fastuidraw):
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
#ifndef ASTRAL_DEMO_CYCLE_VALUE_HPP
#define ASTRAL_DEMO_CYCLE_VALUE_HPP

void
cycle_value(unsigned int &value, bool decrement, unsigned int limit_value);

template<typename T>
void
cycle_value(T &value, bool decrement, unsigned int limit_value)
{
  unsigned int v(value);
  cycle_value(v, decrement, limit_value);
  value = T(v);
}

#endif

/*!
 * \file cycle_value.cpp
 * \brief cycle_value.cpp
 *
 * Adapted from: cycle_value.cpp of FastUIDraw (https://github.com/intel/fastuidraw):
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
#include "cycle_value.hpp"

void
cycle_value(unsigned int &value,
            bool decrement,
            unsigned int limit_value)
{
  if (decrement)
    {
      if (value == 0)
        {
          value = limit_value;
        }
      --value;
    }
  else
    {
      ++value;
      if (value == limit_value)
        {
          value = 0;
        }
    }
}

/*!
 * \file memory_pool.cpp
 * \brief file memory_pool.cpp
 *
 * Copyright 2019 InvisionApp.
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

#include <iostream>
#include <astral/util/memory_pool.hpp>

static bool track_memory_pool_memory_allocations_v = false;

void
astral::
track_memory_pool_memory_allocations(bool v)
{
  track_memory_pool_memory_allocations_v = v;
}

bool
astral::
track_memory_pool_memory_allocations(void)
{
  return track_memory_pool_memory_allocations_v;
}

void
astral::detail::
memory_pool_allocated_subpool(const char *type, size_t PoolSize, const void *pool, unsigned int count)
{
  std::cout << "MemoryPool<" << type << ", " << PoolSize
            << ">(" << pool << "): allocated subpool #"
            << count << "\n" << std::flush;
}

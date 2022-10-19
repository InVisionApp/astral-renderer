/*!
 * \file data_buffer.cpp
 * \brief file data_buffer.cpp
 *
 * Copyright 2018 by Intel.
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

#include <vector>
#include <fstream>
#include <cstring>
#include <astral/util/data_buffer.hpp>


astral::DataBufferBackingStore::
DataBufferBackingStore(unsigned int num_bytes, uint8_t v):
  m_data(num_bytes, v)
{
}

astral::DataBufferBackingStore::
DataBufferBackingStore(c_array<const uint8_t> init_data)
{
  m_data.resize(init_data.size());
  if (!m_data.empty())
    {
      std::memcpy(&m_data[0], init_data.c_ptr(), init_data.size());
    }
}

astral::DataBufferBackingStore::
DataBufferBackingStore(c_string filename)
{
  std::ifstream file(filename, std::ios::binary);
  if (file)
    {
      std::ifstream::pos_type sz;

      file.seekg(0, std::ios::end);
      sz = file.tellg();
      m_data.resize(sz);

      c_array<char> dst;
      dst = make_c_array(m_data).reinterpret_pointer<char>();

      file.seekg(0, std::ios::beg);
      file.read(dst.c_ptr(), dst.size());
    }
}

astral::DataBufferBackingStore::
~DataBufferBackingStore()
{
}

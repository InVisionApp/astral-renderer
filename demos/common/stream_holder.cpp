/*!
 * \file stream_holder.cpp
 * \brief stream_holder.cpp
 *
 * Adapted from: stream_holder.cpp of FastUIDraw (https://github.com/intel/fastuidraw):
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
#include <iostream>
#include <fstream>
#include "stream_holder.hpp"

///////////////////////////
// StreamHolder methods
StreamHolder::
StreamHolder(const std::string &filename):
  m_delete_stream(false)
{
  if (filename == "stderr")
    {
      m_stream = &std::cerr;
    }
  else if (filename == "stdout")
    {
      m_stream = &std::cout;
    }
  else
    {
      m_delete_stream = true;
      m_stream = ASTRALnew std::ofstream(filename.c_str());
    }
}

StreamHolder::
~StreamHolder()
{
  if (m_delete_stream)
    {
      ASTRALdelete(m_stream);
    }
}

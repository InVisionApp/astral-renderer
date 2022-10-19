/*!
 * \file stream_holder.hpp
 * \brief stream_holder.hpp
 *
 * Adapted from: stream_holder.hpp of FastUIDraw (https://github.com/intel/fastuidraw):
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
#ifndef ASTRAL_DEMO_STREAM_HOLDER_HPP
#define ASTRAL_DEMO_STREAM_HOLDER_HPP

#include <ostream>
#include <string>
#include <astral/util/reference_counted.hpp>

class StreamHolder:
  public astral::reference_counted<StreamHolder>::concurrent
{
public:
  StreamHolder(const std::string &filename);
  ~StreamHolder();

  std::ostream&
  stream(void)
  {
    return *m_stream;
  }
private:
  std::ostream *m_stream;
  bool m_delete_stream;
};

#endif

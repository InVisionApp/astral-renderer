/*!
 * \file vertex_data_details.hpp
 * \brief file vertex_data_details.hpp
 *
 * Copyright 2019 by InvisionApp.
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

#ifndef ASTRAL_VERTEX_DATA_DETAILS_HPP
#define ASTRAL_VERTEX_DATA_DETAILS_HPP

#include <astral/util/c_array.hpp>
#include <astral/renderer/vertex_index.hpp>

namespace astral
{
  class Renderer;
  class RenderEncoderBase;
  class VertexDataAllocator;
  class VertexData;

  namespace detail
  {
    ///@cond
    class VertexDataStreamerSize
    {
    public:
    private:
      friend class astral::Renderer;
      friend class astral::VertexDataAllocator;

      explicit
      VertexDataStreamerSize(int sz):
        m_size(sz)
      {}

      int m_size;
    };

    class VertexDataStreamerValues
    {
    public:
    private:
      friend class astral::Renderer;
      friend class astral::VertexData;

      explicit
      VertexDataStreamerValues(c_array<const Vertex> values):
        m_values(values)
      {}

      c_array<const Vertex> m_values;
    };

    ///@endcond
  }
}

#endif

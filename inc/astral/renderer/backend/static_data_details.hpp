/*!
 * \file static_data_details.hpp
 * \brief file static_data_details.hpp
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

#ifndef ASTRAL_STATIC_DATA_DETAILS_HPP
#define ASTRAL_STATIC_DATA_DETAILS_HPP

#include <astral/util/vecN.hpp>
#include <astral/util/c_array.hpp>
#include <astral/renderer/backend/static_data_backing.hpp>

namespace astral
{
  class Renderer;
  class RenderEncoderBase;
  class StaticDataAllocatorCommon;
  class StaticData;

  namespace detail
  {
    ///@cond
    template<enum astral::StaticDataBacking::type_t>
    class StaticDataStreamerSize
    {
    public:
    private:
      friend class astral::Renderer;
      friend class astral::StaticDataAllocatorCommon;

      explicit
      StaticDataStreamerSize(int sz):
        m_size(sz)
      {}

      int m_size;
    };

    template<enum astral::StaticDataBacking::type_t, typename T>
    class StaticDataStreamerValues
    {
    public:
    private:
      friend class astral::Renderer;
      friend class astral::StaticData;

      explicit
      StaticDataStreamerValues(c_array<const T> values):
        m_values(values)
      {}

      c_array<const T> m_values;
    };

    ///@endcond
  }
}

#endif

/*!
 * \file static_data_streamer.hpp
 * \brief file static_data_streamer.hpp
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

#ifndef ASTRAL_STATIC_DATA_STREAMER_HPP
#define ASTRAL_STATIC_DATA_STREAMER_HPP

#include <astral/renderer/backend/static_data_allocator.hpp>

namespace astral
{
  /*!
   * \brief
   * Class to define a single block of static data
   * for the purpose of streaming static data values.
   * \tparam type determines type of data
   */
  template<enum astral::StaticDataBacking::type_t type, typename T>
  class StaticDataStreamerBlock
  {
  public:
    /*!
     * Conveniance typedef
     */
    typedef T ValueType;

    /*!
     * Conveniance typedef
     */
    typedef StaticData ObjectType;

    /*!
     * Conveniance typedef
     */
    typedef detail::StaticDataStreamerSize<type> StreamerSizeType;

    /*!
     * Conveniance typedef
     */
    typedef detail::StaticDataStreamerValues<type, T> StreamerValuesType;

    /*!
     * Ctor, initializing as null.
     */
    StaticDataStreamerBlock(void):
      m_object(nullptr),
      m_offset(0)
    {}

    /*!
     * "Mapped" location to which to write static data
     */
    c_array<ValueType> m_dst;

    /*!
     * The astral::StaticData object that backs the values
     * in \ref m_dst.
     */
    ObjectType *m_object;

    /*!
     * The offset into \ref m_object of where the values
     * in \ref m_dst land
     */
    int m_offset;
  };

  /*!
   * Typedef for generic four tuples of 32-bit static data streaming
   */
  typedef StaticDataStreamerBlock<StaticDataBacking::type32, gvec4> StaticDataStreamerBlock32;

  /*!
   * Typedef for generic four tuples of 16-bit static data streaming
   */
  typedef StaticDataStreamerBlock<StaticDataBacking::type16, u16vec4> StaticDataStreamerBlock16;
}

#endif

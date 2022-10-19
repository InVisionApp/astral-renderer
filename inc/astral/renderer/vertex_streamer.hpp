/*!
 * \file vertex_streamer.hpp
 * \brief file vertex_streamer.hpp
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

#ifndef ASTRAL_VERTEX_STREAMER_HPP
#define ASTRAL_VERTEX_STREAMER_HPP

#include <astral/renderer/backend/vertex_data_details.hpp>

namespace astral
{
  /*!
   * \brief
   * Class to define a single block of vertex data
   * for the purpose of streaming vertex values.
   */
  class VertexStreamerBlock
  {
  public:
    /*!
     * Conveniance typedef
     */
    typedef Vertex ValueType;

    /*!
     * Conveniance typedef
     */
    typedef VertexData ObjectType;

    /*!
     * Conveniance typedef
     */
    typedef detail::VertexDataStreamerSize StreamerSizeType;

    /*!
     * Conveniance typedef
     */
    typedef detail::VertexDataStreamerValues StreamerValuesType;

    /*!
     * Ctor, initializing as null.
     */
    VertexStreamerBlock(void):
      m_object(nullptr),
      m_offset(0)
    {}

    /*!
     * "Mapped" location to which to write vertex data
     */
    c_array<ValueType> m_dst;

    /*!
     * The VertexData object that backs the values in
     * \ref m_dst.
     */
    ObjectType *m_object;

    /*!
     * The offset (in units of astral::Vertex) into
     * \ref m_object of where the values in \ref m_dst
     * land.
     */
    int m_offset;
  };
}

#endif

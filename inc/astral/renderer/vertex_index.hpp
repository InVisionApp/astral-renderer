/*!
 * \file vertex_index.hpp
 * \brief file vertex_index.hpp
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

#ifndef ASTRAL_VERTEX_INDEX_HPP
#define ASTRAL_VERTEX_INDEX_HPP

#include <astral/util/c_array.hpp>
#include <astral/util/vecN.hpp>

namespace astral
{
  /*!
   * \brief
   * A per-vertex value for the GPU to process. The vertex
   * data is sent as-is to the GPU.
   */
  class Vertex
  {
  public:
    enum
      {
        /*!
         * The number of gvec4's in a Vertex
         */
        number_gvec4 = 1,

        /*!
         * The number of element of a Vertex,
         * i.e. 4 * \ref number_gvec4
         */
        number_components = 4 * number_gvec4
      };

    /*!
     * The per-vertex data. A backend will likely collect the
     * vertices into sets of 4 to reduce the vertex count
     * in the GPU API.
     */
    vecN<generic_data, number_components> m_data;
  };

  /*!
   * \brief
   * An index value for the GPU to process
   */
  typedef uint32_t Index;

  /*!
   * \brief
   * A class-struct to encapsulate a request for vertex
   * and index room.
   */
  class VertexIndexCount
  {
  public:
    VertexIndexCount(void):
      m_num_vertices(0),
      m_num_indices(0)
    {}

    /*!
     * Set \ref m_num_vertices
     */
    VertexIndexCount&
    num_vertices(unsigned int v)
    {
      m_num_vertices = v;
      return *this;
    }

    /*!
     * Set \ref m_num_indices
     */
    VertexIndexCount&
    num_indices(unsigned int v)
    {
      m_num_indices = v;
      return *this;
    }

    /*!
     * Number of vertices of the request.
     */
    unsigned int m_num_vertices;

    /*!
     * Number of indices of the request.
     */
    unsigned int m_num_indices;
  };
}

#endif

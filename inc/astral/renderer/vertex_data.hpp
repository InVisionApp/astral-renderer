/*!
 * \file vertex_data.hpp
 * \brief file vertex_data.hpp
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

#ifndef ASTRAL_VERTEX_DATA_HPP
#define ASTRAL_VERTEX_DATA_HPP

#include <vector>
#include <astral/util/reference_counted.hpp>
#include <astral/renderer/vertex_index.hpp>
#include <astral/renderer/backend/vertex_data_backing.hpp>

namespace astral
{
  ///@cond
  class VertexData;
  class VertexDataBacking;
  class VertexDataAllocator;
  ///@endcond

/*!\addtogroup Renderer
 * @{
 */

  /*!
   * \brief
   * A astral::VertexData represents immutable vertex data.
   *
   * One can create an astral::VertexData via
   * or astral::RenderEngine::create(c_array<const Vertex>, c_array<const Index>, unsigned int)
   * or astral::RenderEngine::create(c_array<const Vertex>, unsigned int)
   */
  class VertexData:public reference_counted<VertexData>::non_concurrent_custom_delete
  {
  public:
    ~VertexData();

    /*!
     * Returns the range into the VertexBacking
     * of the vertices of this astral::VertexData.
     */
    range_type<int>
    vertex_range(void) const
    {
      return m_vertex_range ? m_vertex_range->range() : range_type<int>(0, 0);
    }

    /*!
     * Returns true if and only if the VertexData has
     * no vertices.
     */
    bool
    empty(void) const
    {
      return m_number_vertices == 0u;
    }

    /*!
     * Returns the number of vertices of the VertexData.
     */
    unsigned int
    number_vertices(void) const
    {
      return m_number_vertices;
    }

    /*!
     * The astral::VertexDataAllocator that created this
     * astral::VertexData
     */
    VertexDataAllocator&
    allocator(void) const
    {
      return *m_allocator;
    }

    ///@cond
    static
    void
    delete_object(VertexData *p);

    void
    set_values_for_streaming(const detail::VertexDataStreamerValues &values)
    {
      ASTRALunused(m_for_streaming);
      ASTRALassert(m_for_streaming);
      ASTRALassert(values.m_values.size() <= m_number_vertices);
      m_allocator->m_backing->set_vertices(values.m_values, m_vertex_range->range().m_begin);
    }
    ///@endcond

  private:
    friend class VertexDataAllocator;

    VertexData(VertexDataAllocator &allocator,
               const IntervalAllocator::Interval *vertex_range,
               bool for_streaming);

    unsigned int m_number_vertices;
    reference_counted_ptr<VertexDataAllocator> m_allocator;

    const IntervalAllocator::Interval* m_vertex_range;
    bool m_for_streaming;
  };
/*! @} */
}


#endif

/*!
 * \file vertex_data_backing.hpp
 * \brief file vertex_data_backing.hpp
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

#ifndef ASTRAL_VERTEX_DATA_BACKING_HPP
#define ASTRAL_VERTEX_DATA_BACKING_HPP

#include <vector>
#include <set>
#include <astral/util/reference_counted.hpp>
#include <astral/util/interval_allocator.hpp>
#include <astral/renderer/vertex_index.hpp>
#include <astral/renderer/backend/vertex_data_details.hpp>

namespace astral
{
  ///@cond
  class VertexData;
  class VertexDataBacking;
  class VertexDataAllocator;
  ///@endcond

/*!\addtogroup RendererBackend
 * @{
 */

  /*!
   * \brief
   * astral::VertexDataBacking represents the backing of all
   * astral::VertexData objects.
   */
  class VertexDataBacking:
    public reference_counted<VertexDataBacking>::non_concurrent
  {
  public:
    /*!
     * Ctor.
     * \param num_vertices initial number of vertices the backing holds
     */
    VertexDataBacking(unsigned int num_vertices):
      m_num_vertices(num_vertices)
    {}

    virtual
    ~VertexDataBacking()
    {}

    /*!
     * Returns the number of vertices the backing backs.
     */
    unsigned int
    num_vertices(void) const
    {
      return m_num_vertices;
    }

    /*!
     * Call to resize the backing vertex (and token) buffers.
     * Returns the new size which is guaranteed be atleast
     * as large as the passed size.
     */
    unsigned int
    resize_vertices(unsigned int new_size)
    {
      ASTRALassert(new_size > m_num_vertices);
      m_num_vertices = resize_vertices_implement(new_size);
      ASTRALassert(m_num_vertices >= new_size);
      return m_num_vertices;
    }

  private:
    friend class VertexDataAllocator;
    friend class VertexData;

    /*!
     * To be implemented by a derived class to resize
     * the token and vertex buffers. On entry, the
     * value of num_vertices() is the size before the
     * resize. To return the new size of the backing
     * which must be atleast as large as the passed
     * value.
     */
    virtual
    unsigned int
    resize_vertices_implement(unsigned int new_size) = 0;

    /*!
     * To be implemented by a derived class to set vertex values.
     * \param verts vertex values to copy to the backing
     * \param offset offset, in units of \ref Vertex, of
     *               what values to set
     */
    virtual
    void
    set_vertices(c_array<const Vertex> verts, unsigned int offset) = 0;

    unsigned int m_num_vertices;
  };

  /*!
   * \brief
   * An astral::VertexDataAllocator creates astral::VertexData
   * objects.
   */
  class VertexDataAllocator:public reference_counted<VertexDataAllocator>::non_concurrent
  {
  public:
    /*!
     * Ctor.
     * \param backing the astral::VertexDataBacking that backs the
     *                vertex and index data for the GPU to consume.
     */
    static
    reference_counted_ptr<VertexDataAllocator>
    create(VertexDataBacking &backing)
    {
      return ASTRALnew VertexDataAllocator(backing);
    }

    virtual
    ~VertexDataAllocator();

    /*!
     * Create an astral::VertexData object from the passed
     * vertices and indices.
     *
     * NOTE: it is unsafe to call this outside of a Renderer::begin()/Renderer::end()
     *       pair in environments where the 3D API state is affected by anything
     *       else besides Astral.
     *
     * \param verts vertex values to use, values are copied
     * \param indices index values that index into verts, values are copied
     */
    reference_counted_ptr<const VertexData>
    create(c_array<const Vertex> verts,
           c_array<const Index> indices);

    /*!
     * Create an astral::VertexData object from the passed
     * vertices and indices.
     *
     * NOTE: it is unsafe to call this outside of a Renderer::begin()/Renderer::end()
     *       pair in environments where the 3D API state is affected by anything
     *       else besides Astral.
     *
     * \param verts vertex values to use, values are copied
     */
    reference_counted_ptr<const VertexData>
    create(c_array<const Vertex> verts);

    /*!
     * Return the astral::VertexDataBacking, the return
     * value can be null which indicates that data needs
     * to be streamed to the GPU every frame.
     */
    const VertexDataBacking&
    backing(void)
    {
      return *m_backing;
    }

    /*!
     * If backing() is non-null, returns the number of vertices
     * allocated. Othersise, returns 0.
     */
    unsigned int
    number_vertices_allocated(void) const
    {
      return m_number_vertices_allocated;
    }

    /*!
     * During a lock_resources() / unlock_resources() pair,
     * rather than freeing the regions of the freed
     * astral::VertexDataBacking, the freeing
     * is deferred until unlock_resources(). The use case
     * is to make sure that vertex index data is saved
     * until after it is sent to the GPU for processing.
     *
     * Nesting is supported (though ill-advised) and resources
     * are cleared on the call to the outer-most unlock_resources().
     */
    void
    lock_resources(void);

    /*!
     * Release the regions marked for deletion since
     * lock_resources() was called.
     */
    void
    unlock_resources(void);

    ///@cond
    reference_counted_ptr<VertexData>
    create_streamer(detail::VertexDataStreamerSize);
    ///@endcond

  private:
    friend class VertexData;
    class MemoryPool;

    VertexDataAllocator(VertexDataBacking &backing);

    void
    free_vertices(const IntervalAllocator::Interval* R);

    reference_counted_ptr<VertexData>
    create_common(int sz, bool for_streaming);

    reference_counted_ptr<VertexDataBacking> m_backing;
    std::vector<Vertex> m_tmp_verts;
    int m_resources_locked;

    int m_number_vertices_allocated;
    IntervalAllocator m_vertex_interval_allocator;
    std::vector<const IntervalAllocator::Interval*> m_delayed_vertex_frees;
    MemoryPool *m_pool;
  };

/*! @} */
}

#endif

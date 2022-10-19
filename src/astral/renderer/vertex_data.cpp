/*!
 * \file vertex_data.cpp
 * \brief file vertex_data.cpp
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

#include <algorithm>
#include <astral/util/memory_pool.hpp>
#include <astral/renderer/vertex_data.hpp>

class astral::VertexDataAllocator::MemoryPool
{
public:
  template<typename ...Args>
  VertexData*
  create(Args&&... args)
  {
    void *data;
    VertexData *p;

    data = m_pool.allocate();
    p = new(data) VertexData(std::forward<Args>(args)...);

    return p;
  }

  void
  reclaim(VertexData *p)
  {
    m_pool.reclaim(p);
  }

private:
  astral::MemoryPool<VertexData, 512> m_pool;
};

///////////////////////////////////
// astral::VertexData methods
astral::VertexData::
VertexData(VertexDataAllocator &allocator,
           const IntervalAllocator::Interval *vertex_range,
           bool for_streaming):
  m_allocator(&allocator),
  m_vertex_range(vertex_range),
  m_for_streaming(for_streaming)
{
  m_number_vertices = m_vertex_range ? m_vertex_range->range().difference() : 0;
}

astral::VertexData::
~VertexData()
{
  if (m_vertex_range)
    {
      m_allocator->free_vertices(m_vertex_range);
    }
}

void
astral::VertexData::
delete_object(VertexData *p)
{
  ASTRALassert(p);
  ASTRALassert(p->m_allocator);

  reference_counted_ptr<VertexDataAllocator> allocator(p->m_allocator);
  p->~VertexData();

  allocator->m_pool->reclaim(p);
}

///////////////////////////////////////
// astral::VertexDataAllocator methods
astral::VertexDataAllocator::
VertexDataAllocator(VertexDataBacking &backing):
  m_backing(&backing),
  m_resources_locked(0),
  m_number_vertices_allocated(0),
  m_vertex_interval_allocator(m_backing->num_vertices(), 1)
{
  m_pool = ASTRALnew MemoryPool();
}

astral::VertexDataAllocator::
~VertexDataAllocator()
{
  ASTRALdelete(m_pool);
}

void
astral::VertexDataAllocator::
lock_resources(void)
{
  ++m_resources_locked;
}

void
astral::VertexDataAllocator::
unlock_resources(void)
{
  ASTRALassert(m_resources_locked >= 1);

  --m_resources_locked;
  if (m_resources_locked == 0)
    {
      for (const IntervalAllocator::Interval* R: m_delayed_vertex_frees)
        {
          free_vertices(R);
        }
      m_delayed_vertex_frees.clear();
    }
}

astral::reference_counted_ptr<const astral::VertexData>
astral::VertexDataAllocator::
create(c_array<const Vertex> in_verts,
       c_array<const Index> in_indices)
{
  m_tmp_verts.resize(in_indices.size());
  for (unsigned int I = 0; I < in_indices.size(); ++I)
    {
      m_tmp_verts[I] = in_verts[in_indices[I]];
    }
  return create(make_c_array(m_tmp_verts));
}

astral::reference_counted_ptr<astral::VertexData>
astral::VertexDataAllocator::
create_common(int sz, bool for_streaming)
{
  reference_counted_ptr<VertexData> return_value;
  const IntervalAllocator::Interval* vertex_range(nullptr);

  ASTRALassert(sz >= 0);
  if (sz != 0)
    {
      vertex_range = m_vertex_interval_allocator.allocate(sz);
      if (!vertex_range)
        {
          int req_size, new_size;

          req_size = 2 * m_vertex_interval_allocator.layer_length() + sz;
          new_size = m_backing->resize_vertices(req_size);

          ASTRALassert(new_size >= req_size);
          m_vertex_interval_allocator.layer_length(new_size);

          vertex_range = m_vertex_interval_allocator.allocate(sz);
        }
      ASTRALassert(vertex_range);
    }

  return_value = m_pool->create(*this, vertex_range, for_streaming);
  return return_value;
}

astral::reference_counted_ptr<astral::VertexData>
astral::VertexDataAllocator::
create_streamer(detail::VertexDataStreamerSize sz)
{
  reference_counted_ptr<VertexData> return_value;

  return_value = create_common(sz.m_size, true);

  return return_value;
}

astral::reference_counted_ptr<const astral::VertexData>
astral::VertexDataAllocator::
create(c_array<const Vertex> verts)
{
  int sz(verts.size());
  reference_counted_ptr<VertexData> return_value;

  return_value = create_common(sz, false);
  if (!verts.empty())
    {
      m_backing->set_vertices(verts, return_value->m_vertex_range->range().m_begin);
    }
  return return_value;
}

void
astral::VertexDataAllocator::
free_vertices(const IntervalAllocator::Interval* R)
{
  if (m_resources_locked == 0)
    {
      m_vertex_interval_allocator.release(R);
    }
  else
    {
      m_delayed_vertex_frees.push_back(R);
    }
}

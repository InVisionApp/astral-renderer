/*!
 * \file static_data.cpp
 * \brief file static_data.cpp
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

#include <astral/util/memory_pool.hpp>
#include <astral/renderer/static_data.hpp>

class astral::StaticDataAllocatorCommon::MemoryPool
{
public:
  template<typename ...Args>
  StaticData*
  create(Args&&... args)
  {
    void *data;
    StaticData *p;

    data = m_pool.allocate();
    p = new(data) StaticData(std::forward<Args>(args)...);

    return p;
  }

  void
  reclaim(StaticData *p)
  {
    m_pool.reclaim(p);
  }

private:
  astral::MemoryPool<StaticData, 512> m_pool;
};

//////////////////////////////////
// astral::StaticData methods
astral::StaticData::
StaticData(StaticDataAllocatorCommon &allocator,
           const IntervalAllocator::Interval *location,
           bool for_streaming):
  m_allocator(&allocator),
  m_location(location),
  m_for_streaming(for_streaming)
{}

astral::StaticData::
~StaticData()
{
  if (m_location)
    {
      if (m_allocator->m_resources_locked == 0)
        {
          m_allocator->free_data(m_location);
        }
      else
        {
          m_allocator->m_delayed_frees.push_back(m_location);
        }
    }
}

void
astral::StaticData::
delete_object(StaticData *p)
{
  ASTRALassert(p);
  ASTRALassert(p->m_allocator);

  reference_counted_ptr<StaticDataAllocatorCommon> allocator(p->m_allocator);
  p->~StaticData();

  allocator->m_pool->reclaim(p);
}

///////////////////////////////////////
// astral::StaticDataAllocator16 methods
astral::reference_counted_ptr<const astral::StaticData>
astral::StaticDataAllocator16::
create(c_array<const vec4> data)
{
  c_array<const float> src;
  c_array<uint16_t> dst;
  c_array<const u16vec4> converted;

  src = data.reinterpret_pointer<const float>();
  ASTRALassert(src.size() == 4u * data.size());

  m_workroom.resize(src.size());
  dst = make_c_array(m_workroom);

  convert_to_fp16(src, dst);

  converted = dst.reinterpret_pointer<const u16vec4>();
  ASTRALassert(converted.size() * 4 == dst.size());

  return create(converted);
}

////////////////////////////////////
// astral::StaticDataAllocatorCommon methods
astral::StaticDataAllocatorCommon::
StaticDataAllocatorCommon(StaticDataBacking &backing):
  m_backing(&backing),
  m_interval_allocator(m_backing->size(), 1),
  m_resources_locked(0),
  m_amount_allocated(0)
{
  m_pool = ASTRALnew MemoryPool();
}

astral::StaticDataAllocatorCommon::
~StaticDataAllocatorCommon()
{
  ASTRALdelete(m_pool);
}

void
astral::StaticDataAllocatorCommon::
lock_resources(void)
{
  ++m_resources_locked;
}

void
astral::StaticDataAllocatorCommon::
unlock_resources(void)
{
  ASTRALassert(m_resources_locked >= 1);

  --m_resources_locked;
  if (m_resources_locked == 0)
    {
      for (auto R: m_delayed_frees)
        {
          free_data(R);
        }
      m_delayed_frees.clear();
    }
}

astral::reference_counted_ptr<const astral::StaticData>
astral::StaticDataAllocatorCommon::
create_implement(c_array<const u32vec4> data)
{
  const IntervalAllocator::Interval *location;

  location = allocate_data(data);
  return m_pool->create(*this, location, false);
}

astral::reference_counted_ptr<const astral::StaticData>
astral::StaticDataAllocatorCommon::
create_implement(c_array<const u16vec4> data)
{
  const IntervalAllocator::Interval *location;

  location = allocate_data(data);
  return m_pool->create(*this, location, false);
}

astral::reference_counted_ptr<astral::StaticData>
astral::StaticDataAllocatorCommon::
create_streamer_implement(int sz)
{
  const IntervalAllocator::Interval *location;

  location = allocate_data(sz);
  return m_pool->create(*this, location, true);
}

const astral::IntervalAllocator::Interval*
astral::StaticDataAllocatorCommon::
allocate_data(int size)
{
  const astral::IntervalAllocator::Interval *R;

  if (size == 0)
    {
      return nullptr;
    }

  R = m_interval_allocator.allocate(size);
  if (!R)
    {
      unsigned int req_size, new_size;

      req_size = 2u * m_backing->size() + size;
      new_size = m_backing->resize(req_size);
      m_interval_allocator.layer_length(new_size);
      R = m_interval_allocator.allocate(size);
    }

  ASTRALassert(R);
  m_amount_allocated += size;

  return R;
}

const astral::IntervalAllocator::Interval*
astral::StaticDataAllocatorCommon::
allocate_data(c_array<const u32vec4> data)
{
  const IntervalAllocator::Interval* R;

  if (data.size() == 0)
    {
      return 0;
    }
  R = allocate_data(data.size());
  m_backing->set_data(R->range().m_begin, data);

  return R;
}

const astral::IntervalAllocator::Interval*
astral::StaticDataAllocatorCommon::
allocate_data(c_array<const u16vec4> data)
{
  const IntervalAllocator::Interval* R;

  if (data.size() == 0)
    {
      return 0;
    }
  R = allocate_data(data.size());
  m_backing->set_data(R->range().m_begin, data);

  return R;
}

void
astral::StaticDataAllocatorCommon::
free_data(const IntervalAllocator::Interval *R)
{
  ASTRALassert(R);
  m_amount_allocated -= R->range().difference();
  m_interval_allocator.release(R);
}

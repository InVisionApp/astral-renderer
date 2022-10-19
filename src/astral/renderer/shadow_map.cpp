/*!
 * \file shadow_map.cpp
 * \brief file shadow_map.cpp
 *
 * Copyright 2020 by InvisionApp.
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

#include <astral/renderer/shadow_map.hpp>
#include <astral/renderer/render_value.hpp>
#include <astral/util/memory_pool.hpp>

class astral::ShadowMapAtlas::MemoryPool
{
public:
  MemoryPool(void):
    m_number_alive(0)
  {}

  ~MemoryPool()
  {
    ASTRALassert(m_number_alive == 0);
  }

  template<typename ...Args>
  ShadowMap*
  create(Args&&... args)
  {
    void *data;
    ShadowMap *p;

    data = m_pool.allocate();
    p = new(data) ShadowMap(std::forward<Args>(args)...);

    ++m_number_alive;
    return p;
  }

  void
  reclaim(ShadowMap *p)
  {
    ASTRALassert(!p->m_atlas);
    m_pool.reclaim(p);
    --m_number_alive;
  }

private:
  int m_number_alive;
  astral::MemoryPool<ShadowMap, 512> m_pool;
};

///////////////////////////////////////////
// astral::ShadowMapAtlas metods
astral::ShadowMapAtlas::
ShadowMapAtlas(ShadowMapAtlasBacking &backing):
  m_backing(&backing),
  m_interval_allocator(backing.width(), backing.height() >> 2u),
  m_resources_locked(0),
  m_resources_unlock_count(0u)
{
  ASTRALassert((backing.height() & 3u) == 0u);
  m_pool = ASTRALnew MemoryPool();
  m_render_target = m_backing->render_target();
}

astral::ShadowMapAtlas::
~ShadowMapAtlas()
{
  ASTRALdelete(m_pool);
  ASTRALassert(!m_interval_allocator.has_live_intervals());
}

void
astral::ShadowMapAtlas::
lock_resources(void)
{
  ++m_resources_locked;
}

void
astral::ShadowMapAtlas::
unlock_resources(void)
{
  --m_resources_locked;
  ASTRALassert(m_resources_locked >= 0);

  if (m_resources_locked == 0)
    {
      ++m_resources_unlock_count;
      for (const IntervalAllocator::Interval *p : m_delayed_frees)
        {
          m_interval_allocator.release(p);
        }
      m_delayed_frees.clear();
    }
}

const astral::IntervalAllocator::Interval*
astral::ShadowMapAtlas::
allocate_interval(unsigned int length)
{
  const IntervalAllocator::Interval *p;

  p = m_interval_allocator.allocate(length);
  if (!p)
    {
      unsigned int N;

      N = m_backing->height(m_backing->height() + 4);
      m_interval_allocator.number_layers(N >> 2u);
      m_render_target = m_backing->render_target();

      p = m_interval_allocator.allocate(length);
    }

  ASTRALassert(p);
  return p;
}

void
astral::ShadowMapAtlas::
release(const IntervalAllocator::Interval *p)
{
  ASTRALassert(m_resources_locked >= 0);
  if (m_resources_locked == 0)
    {
      m_interval_allocator.release(p);
    }
  else
    {
      m_delayed_frees.push_back(p);
    }
}

astral::reference_counted_ptr<astral::ShadowMap>
astral::ShadowMapAtlas::
create(int D, const vec2 &light_position)
{
  ShadowMap *p;

  ASTRALassert(D > 0);
  if (D > m_interval_allocator.layer_length())
    {
      ASTRALassert(!"Attempted to allocate too large ShadowMap");
      return nullptr;
    }

  p = m_pool->create();

  p->m_atlas = this;
  p->m_light_position = light_position;
  p->m_in_use_marker = 0u;
  p->m_dimensions = D;
  p->m_offscreen_render_index = InvalidRenderValue;
  p->m_shadow_map_id = allocate_shadow_map_id(p);

  p->m_interval = allocate_interval(D);
  ASTRALassert(p->m_interval);
  p->m_atlas_location.x() = p->m_interval->range().m_begin;
  p->m_atlas_location.y() = 4u * p->m_interval->layer();

  return p;
}

void
astral::ShadowMapAtlas::
free_shadow_map_id(ShadowMap *p)
{
  ShadowMapID ID;

  ASTRALassert(p);
  ID = p->m_shadow_map_id;

  ASTRALassert(m_fetcher.size() > ID.m_slot);
  ASTRALassert(m_fetcher[ID.m_slot] == p);

  m_fetcher[ID.m_slot] = nullptr;

  /* when retiring an ID, increment the uniqueness
   * so that when the slot is reused, it gives a
   * unique ImageID value still.
   */
  ++ID.m_uniqueness;
  m_free_ids.push_back(ID);
}

astral::ShadowMapID
astral::ShadowMapAtlas::
allocate_shadow_map_id(ShadowMap *p)
{
  ShadowMapID ID;
  if (!m_free_ids.empty())
    {
      ID = m_free_ids.back();
      m_free_ids.pop_back();
    }
  else
    {
      ID.m_slot = m_fetcher.size();
      ID.m_uniqueness = 0u;
      m_fetcher.push_back(nullptr);
    }

  ASTRALassert(ID.m_slot < m_fetcher.size());
  ASTRALassert(m_fetcher[ID.m_slot] == nullptr);
  m_fetcher[ID.m_slot] = p;
  return ID;
}

astral::ShadowMap*
astral::ShadowMapAtlas::
fetch_shadow_map(ShadowMapID ID)
{
  ShadowMap *p;

  if (!ID.valid() || ID.m_slot >= m_fetcher.size())
    {
      return nullptr;
    }

  p = m_fetcher[ID.m_slot];
  ASTRALassert(!p || p->m_shadow_map_id.m_slot == ID.m_slot);
  if (!p || p->m_shadow_map_id.m_uniqueness != ID.m_uniqueness)
    {
      return nullptr;
    }

  return p;
}

///////////////////////////////////////////
// astral::ShadowMap methods
void
astral::ShadowMap::
mark_as_virtual_render_target(detail::MarkShadowMapAsRenderTarget v)
{
  if (v.m_offscreen_render_index != InvalidRenderValue)
    {
      ASTRALassert(!in_use());
      ASTRALassert(offscreen_render_index() == InvalidRenderValue);

      m_offscreen_render_index = v.m_offscreen_render_index;
      mark_in_use();
    }
  else
    {
      ASTRALassert(in_use());
      ASTRALassert(offscreen_render_index() != InvalidRenderValue);

      m_offscreen_render_index = v.m_offscreen_render_index;
      m_in_use_marker = m_atlas->m_resources_unlock_count;
    }
}

void
astral::ShadowMap::
delete_object(ShadowMap *p)
{
  ASTRALassert(p);
  ASTRALassert(p->m_atlas);
  ASTRALassert(p->m_interval);

  reference_counted_ptr<ShadowMapAtlas> atlas;

  atlas.swap(p->m_atlas);
  atlas->release(p->m_interval);
  atlas->free_shadow_map_id(p);
  atlas->m_pool->reclaim(p);
}

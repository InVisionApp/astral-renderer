/*!
 * \file tile_allocator.cpp
 * \brief file tile_allocator.cpp
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

#include <algorithm>
#include <astral/util/memory_pool.hpp>
#include <astral/util/object_pool.hpp>
#include <astral/util/tile_allocator.hpp>

/* Overview
 * A TileAllocator is how we track the allocation of zones allocated
 * on a tiled image atlas. The tile size is a power of 2, write it
 * as 2^N, which is constant for a TileAllocator's lifetime.
 *
 * In constrast to an "easy" tile allocator, tiles can be broken into
 * sub-tiles where each dimension is also a power of 2. When a tile
 * is first broken, it is either broken into horizontal or vertical
 * stripes, i.e. given a tile T of size (2^A, 2^B), it can be broken
 * into two tiles of size (2^(A-1), 2^B) or (2^A, 2^(B - 1)). The
 * Tile T will have a pointer to both of its children. If one of
 * the children is returned to the freelist while anoher is already
 * on the freelist, then the other child is removed from the freelist
 * and the tile T itself is placed on the freelist. This recurses
 * up all the way the ancestor tile of size (2^N, 2^N), see the
 * method TileAllocator::release_tile().
 *
 * An allocation request is a size (2^A, 2^B) with 0 <= A, B <= N
 * is handled as follows:
 *  1. check the freelist [A][B], if it is non-empty, return
 *     that element and take it off the free list.
 *  2. otherwise create a tile of size (2^A, 2^B).
 *
 * Creating a tile of size (2^A, 2^B) is handled as follows:
 *  1. For (i, j) so that the freelist [A + i][B + j] is
 *     non-empty and the fit is as tight as possible. Split that tile
 *     accordingly. For each (A, B), the list m_order[A][B] is
 *     the list of those (i, j) with i >= A, j >= B sorted so that
 *     the tightest fist comes first. Getting (i, j) just means
 *     walking m_order[A][B] until a free-list is non-empty.
 *     Take that tile. If the tile is bigger than what is wanted,
 *     successively split htat tile until we have a tile of the
 *     required size. The walking of the m_order is in allocate_tile()
 *     and the splitting of a tile is in split_tile().
 *  2. If there is no free element from 2, allocate a tile of (2^N, 2^N)
 *     by incrementing m_alloc_tile_counter and then splitting that
 *     tile.
 *
 * Lastly, to prevent memory allocation noise, tiles are allocated
 * from the MemoryPool, m_pool and returned to there as well.
 */

namespace
{
  astral::uvec2
  order_value(const astral::uvec2 &value)
  {
    astral::uvec2 return_value;

    return_value.x() = astral::t_min(value.x(), value.y());
    return_value.y() = value.x() + value.y();

    return return_value;
  }

  bool
  compare_order_values(const astral::uvec2 &lhs, const astral::uvec2 &rhs)
  {
    return order_value(lhs) < order_value(rhs);
  }
}

class astral::TileAllocator::TileImplement:public Tile
{
public:
  TileImplement(const uvec2 &log2_size,
                const uvec3 &location,
                TileImplement *parent, unsigned int id):
    m_log2_size(log2_size),
    m_location(location),
    m_parent(parent),
    m_children(nullptr, nullptr),
    m_available(false),
    m_child_id(id),
    m_list_location(0u)
  {
  }

  bool
  has_children(void) const
  {
    ASTRALassert((m_children[0] == nullptr) == (m_children[1] == nullptr));
    return m_children[0] != nullptr;
  }

  TileImplement*
  sibling(void) const
  {
    ASTRALassert(m_parent);
    ASTRALassert(m_parent->m_children[m_child_id] == this);
    return m_parent->m_children[1 - m_child_id];
  }

  unsigned int
  size(int coordinate) const
  {
    return 1u << m_log2_size[coordinate];
  }

  uvec2
  size(void) const
  {
    return uvec2(size(0), size(1));
  }

  unsigned int
  area(void) const
  {
    return size(0) * size(1);
  }

  /* size of tile */
  uvec2 m_log2_size;

  /* location of tile
   *  .xy --> location within layer in PIXELS
   *  .z  --> which layer
   */
  uvec3 m_location;

  /* parent tile */
  TileImplement *m_parent;

  /* Children */
  vecN<TileImplement*, 2> m_children;

  /* true if on a tile list */
  bool m_available : 1;

  /* index into m_parent->m_children that gives this */
  unsigned int m_child_id : 1;

  /* index into free-list where this tile is located */
  unsigned int m_list_location : 30;
};

class astral::TileAllocator::TileList
{
public:
  void
  clear(void)
  {
    m_values.clear();
  }

  TileImplement*
  pop_back(void)
  {
    TileImplement *p;

    ASTRALassert(!empty());
    p = m_values.back();
    p->m_available = false;
    m_values.pop_back();

    return p;
  }

  void
  insert(TileImplement *p)
  {
    ASTRALassert(p);
    ASTRALassert(!p->m_available);

    p->m_available = true;
    p->m_list_location = m_values.size();
    m_values.push_back(p);
  }

  void
  remove(TileImplement *p)
  {
    ASTRALassert(p);
    ASTRALassert(p->m_available);
    ASTRALassert(p->m_list_location < m_values.size());
    ASTRALassert(m_values[p->m_list_location] == p);

    unsigned int loc(p->m_list_location);
    p->m_available = false;

    m_values[loc] = m_values.back();
    m_values[loc]->m_list_location = loc;
    m_values.pop_back();
  }

  bool
  empty(void) const
  {
    return m_values.empty();
  }

private:
  std::vector<TileImplement*> m_values;
};

class astral::TileAllocator::RegionImplement:public Region
{
public:
  RegionImplement(void):
    m_size(0, 0),
    m_location(0, 0, 0)
  {}

  void
  clear(void)
  {
    m_size = uvec2(0, 0);
    m_location = uvec3(0, 0, 0);
    m_tiles.clear();
  }

  /* size of tile */
  uvec2 m_size;

  /* location of region
   *  .xy --> location within layer in PIXELS
   *  .z  --> which layer
   */
  uvec3 m_location;

  std::vector<TileImplement*> m_tiles;
};

class astral::TileAllocator::MemoryPool
{
public:
  ~MemoryPool()
  {
    m_tile_pool.clear();
    m_region_pool.clear();
  }

  astral::MemoryPool<TileImplement, 4096> m_tile_pool;
  ObjectPoolClear<RegionImplement> m_region_pool;

  std::vector<TileImplement*> m_workroom;
};

//////////////////////////////////////////
// astral::TileAllocator::Tile methods
astral::uvec2
astral::TileAllocator::Tile::
log2_size(void) const
{
  return static_cast<const TileImplement*>(this)->m_log2_size;
}

astral::uvec3
astral::TileAllocator::Tile::
location(void) const
{
  return static_cast<const TileImplement*>(this)->m_location;
}

//////////////////////////////////////////
// astral::TileAllocator::Region methods
astral::uvec2
astral::TileAllocator::Region::
size(void) const
{
  return static_cast<const RegionImplement*>(this)->m_size;
}

astral::uvec3
astral::TileAllocator::Region::
location(void) const
{
  return static_cast<const RegionImplement*>(this)->m_location;
}

unsigned int
astral::TileAllocator::Region::
number_tiles(void) const
{
  return static_cast<const RegionImplement*>(this)->m_tiles.size();
}

astral::RectT<unsigned int>
astral::TileAllocator::Region::
tile(unsigned int I) const
{
  const RegionImplement *p;
  const TileImplement *t;
  RectT<unsigned int> R;

  p = static_cast<const RegionImplement*>(this);
  ASTRALassert(I < p->m_tiles.size());

  t = p->m_tiles[I];
  R.m_min_point = uvec2(t->m_location.x(), t->m_location.y());
  R.m_max_point = R.m_min_point + t->size();

  return R;
}

///////////////////////////////////////////
// astral::TileAllocator methods
astral::TileAllocator::
TileAllocator(uvec2 log2_max_tile_size,
              uvec2 number_tiles_per_layer,
              unsigned int number_layers):
  m_log2_max_tile_size(log2_max_tile_size),
  m_number_tiles_per_layer(number_tiles_per_layer),
  m_number_layers(number_layers),
  m_alloc_tile_counter(0, 0, 0),
  m_num_tiles_allocated(0u),
  m_space_allocated(0u)
{
  m_pool = ASTRALnew MemoryPool();
  ready_lists();
}

void
astral::TileAllocator::
change_size(uvec2 log2_max_tile_size,
            uvec2 number_tiles_per_layer,
            unsigned int number_layers)
{
  release_all();

  if (m_log2_max_tile_size != log2_max_tile_size
      || m_number_tiles_per_layer != number_tiles_per_layer)
    {
      m_log2_max_tile_size = log2_max_tile_size;
      m_number_tiles_per_layer = number_tiles_per_layer;
      ready_lists();
    }
  m_number_layers = number_layers;

}

void
astral::TileAllocator::
ready_lists(void)
{
  /* set m_order and create m_free_tiles */
  m_order.resize(m_log2_max_tile_size.x() + 1u);
  m_free_tiles.resize(m_log2_max_tile_size.x() + 1u);

  for (unsigned int x = 0; x <= m_log2_max_tile_size.x(); ++x)
    {
      m_order[x].resize(m_log2_max_tile_size.y() + 1u);
      m_free_tiles[x].resize(m_log2_max_tile_size.y() + 1u);
      for (unsigned int y = 0; y <= m_log2_max_tile_size.y(); ++y)
        {
          m_order[x][y].clear();
          create_ordering(&m_order[x][y], x, y);
        }
    }

  m_max_tile_size.x() = 1u << m_log2_max_tile_size.x();
  m_max_tile_size.y() = 1u << m_log2_max_tile_size.y();
}

astral::TileAllocator::
~TileAllocator()
{
  ASTRALassert(m_num_tiles_allocated == 0u);
  ASTRALdelete(m_pool);
}

void
astral::TileAllocator::
create_ordering(std::vector<uvec2> *dst, unsigned int x, unsigned int y)
{
  ASTRALassert(dst->empty());

  for (unsigned int i = 0; i <= m_log2_max_tile_size.x() - x; ++i)
    {
      for (unsigned int j = 0; j <= m_log2_max_tile_size.y() - y; ++j)
        {
          dst->push_back(uvec2(i, j));
        }
    }

  std::sort(dst->begin(), dst->end(), compare_order_values);
  for (uvec2 &v : *dst)
    {
      v.x() += x;
      v.y() += y;
    }

  ASTRALassert(!dst->empty() && dst->back() == m_log2_max_tile_size);
  ASTRALassert(!dst->empty() && dst->front() == uvec2(x, y));
}

void
astral::TileAllocator::
free_tile(Tile *q)
{
  TileImplement *p;

  p = static_cast<TileImplement*>(q);
  ASTRALassert(!p->m_available);
  ASTRALassert(!p->m_children[0]);
  ASTRALassert(!p->m_children[1]);

  m_pool->m_tile_pool.reclaim(p);
}

astral::TileAllocator::TileImplement*
astral::TileAllocator::
create_tile(const uvec2 &log2_size,
            const uvec3 &location,
            TileImplement *parent, unsigned int id)
{
  TileImplement *p;
  void *q;

  q = m_pool->m_tile_pool.allocate();
  p = new(q) TileImplement(log2_size, location, parent, id);
  ASTRALassert(p == q);

  return p;
}

astral::TileAllocator::TileImplement*
astral::TileAllocator::
create_base_tile(void)
{
  uvec3 location;

  location.x() = (m_alloc_tile_counter.x() << m_log2_max_tile_size.x());
  location.y() = (m_alloc_tile_counter.y() << m_log2_max_tile_size.y());
  location.z() = m_alloc_tile_counter.z();

  if (location.z() == m_number_layers)
    {
      return nullptr;
    }

  ++m_alloc_tile_counter.x();
  if (m_alloc_tile_counter.x() == m_number_tiles_per_layer.x())
    {
      m_alloc_tile_counter.x() = 0u;
      ++m_alloc_tile_counter.y();
      if (m_alloc_tile_counter.y() == m_number_tiles_per_layer.y())
        {
          m_alloc_tile_counter.y() = 0u;
          ++m_alloc_tile_counter.z();
        }
    }

  return create_tile(m_log2_max_tile_size, location, nullptr, 0);
}

void
astral::TileAllocator::
release_region(const Region *cp)
{
  RegionImplement *p;

  p = static_cast<RegionImplement*>(const_cast<Region*>(cp));
  for (const Tile *tile : p->m_tiles)
    {
      release_tile(tile);
    }
  m_pool->m_region_pool.reclaim(p);
}

void
astral::TileAllocator::
release_tile(const Tile *cp)
{
  TileImplement *p;

  p = static_cast<TileImplement*>(const_cast<Tile*>(cp));
  ASTRALassert(m_num_tiles_allocated > 0u);
  --m_num_tiles_allocated;
  m_space_allocated -= p->area();

  ASTRALassert(p);
  ASTRALassert(!p->m_available);
  ASTRALassert(!p->has_children());
  ASTRALassert(p->m_log2_size.x() <= m_log2_max_tile_size.x());
  ASTRALassert(p->m_log2_size.y() <= m_log2_max_tile_size.y());
  ASTRALassert(!p->m_parent || p->m_parent->m_children[p->m_child_id] == p);

  while (p->m_parent && p->sibling()->m_available)
    {
      TileImplement *parent;

      parent = p->m_parent;
      ASTRALassert(!parent->m_available);

      /* remove the sibling from the freelist */
      uvec2 sz(p->m_log2_size);
      m_free_tiles[sz.x()][sz.y()].remove(p->sibling());

      /* recycle the memory */
      free_tile(parent->m_children[0]);
      free_tile(parent->m_children[1]);

      /* set parent to not have children */
      parent->m_children[0] = nullptr;
      parent->m_children[1] = nullptr;

      /* ready next iteration */
      p = parent;
    }

  m_free_tiles[p->m_log2_size.x()][p->m_log2_size.y()].insert(p);
  ASTRALassert(p->m_available);
}

const astral::TileAllocator::Region*
astral::TileAllocator::
allocate_region(unsigned int width, unsigned int height)
{
  TileImplement *tile;
  RegionImplement *region;
  unsigned int log2_width, log2_height, tile_width, tile_height;

  log2_width = uint32_log2_ceiling(width);
  log2_height = uint32_log2_ceiling(height);

  tile_width = 1u << log2_width;
  tile_height = 1u << log2_height;

  ASTRALassert(tile_width >= width);
  ASTRALassert(tile_height >= height);
  ASTRALunused(tile_width);
  ASTRALunused(tile_height);

  tile = static_cast<TileImplement*>(const_cast<Tile*>(allocate_tile(log2_width, log2_height)));
  if (!tile)
    {
      return nullptr;
    }

  region = m_pool->m_region_pool.allocate();
  region->m_size = uvec2(width, height);
  region->m_location = tile->location();

  /* TODO: have a heuristic to decide to split in x or y
   *       first.
   */

  /* split the tile in x-direction; the tiles that are not
   * need are placed onto m_pool->m_workroom.
   */
  ASTRALassert(m_pool->m_workroom.empty());
  split_add_tile(0, tile, width, &m_pool->m_workroom);

  /* for each tile that is needed, split in the y-direction */
  ASTRALassert(region->m_tiles.empty());
  for (TileImplement *p : m_pool->m_workroom)
    {
      split_add_tile(1, p, height, &region->m_tiles);
    }

  /* cleanup for the next user of the workroom */
  m_pool->m_workroom.clear();

  return region;
}

const astral::TileAllocator::Tile*
astral::TileAllocator::
allocate_tile(unsigned int log2_width, unsigned int log2_height)
{
  ASTRALassert(log2_width <= m_log2_max_tile_size.x());
  ASTRALassert(log2_height <= m_log2_max_tile_size.y());

  /* step 1 find the free list we have to take from */
  c_array<const uvec2> szs(make_c_array(m_order[log2_width][log2_height]));
  TileImplement *tile(nullptr);

  /* TODO: we should maintain a list of what TileList's are non-empty
   *       instead of incrementing through every single one that
   *       is big enough
   */
  for (unsigned int i = 0; i < szs.size() && !tile; ++i)
    {
      const uvec2 &e(szs[i]);
      TileList &tile_list(m_free_tiles[e.x()][e.y()]);

      if (!tile_list.empty())
        {
          tile = tile_list.pop_back();
        }
    }

  if (tile == nullptr)
    {
      /* create a tile by incrementing m_alloc_tile_counter */
      tile = create_base_tile();
    }

  if (!tile)
    {
      /* exceeded what is allowed */
      return nullptr;
    }

  tile = split_tile(tile, log2_width, log2_height);

  ASTRALassert(tile);
  ASTRALassert(!tile->m_available);
  ASTRALassert(!tile->m_parent || tile->m_parent->m_children[tile->m_child_id] == tile);

  ++m_num_tiles_allocated;
  m_space_allocated += tile->area();

  return tile;
}

void
astral::TileAllocator::
split_add_tile(unsigned int coordinate, TileImplement *tile,
                unsigned int keep_size,
                std::vector<TileImplement*> *dst_allocated)
{
  /* split_add_tile() is only called by allocate_region()
   * on a tile that is either freshly created or removed from a
   * TileList, thus the tile is not actually available and
   * not have chlildren.
   */
  ASTRALassert(tile);
  ASTRALassert(!tile->m_available);
  ASTRALassert(!tile->has_children());

  while (keep_size > 0u)
    {
      uvec2 log2_child_size(tile->m_log2_size);
      uvec3 location(tile->m_location);

      ASTRALassert(tile->size(coordinate) >= keep_size);
      if (keep_size == tile->size(coordinate))
        {
          /* take the entire tile */
          dst_allocated->push_back(tile);
          return;
        }

      ASTRALassert(log2_child_size[coordinate] > 0u);
      log2_child_size[coordinate] -= 1u;

      /* split the tile in the named coordinate. The tile
       * on the minimum side is added to dst_allocated
       * and the tile on the max-side is the next iteration.
       */
      tile->m_children[0] = create_tile(log2_child_size, location, tile, 0);

      location[coordinate] += (1u << log2_child_size[coordinate]);
      tile->m_children[1] = create_tile(log2_child_size, location, tile, 1);

      if (tile->m_children[0]->size(coordinate) <= keep_size)
        {
          /* if the m_children[0] is smaller than the keep size, we
           * use it and add it to the list and the next iteration uses
           * its neigher m_children[1].
           */
          dst_allocated->push_back(tile->m_children[0]);
          keep_size -= tile->m_children[0]->size(coordinate);

          tile = tile->m_children[1];
        }
      else
        {
          TileImplement *free_tile;

          /* if m_children[0] is greater than the keep size, then we
           * put m_children[1] on the freelist now and use m_children[0]
           * for the next iteration
           */
          free_tile = tile->m_children[1];
          tile = tile->m_children[0];

          m_free_tiles[free_tile->m_log2_size.x()][free_tile->m_log2_size.y()].insert(free_tile);
        }
    }

  /* add the left over tile to the free list */
  m_free_tiles[tile->m_log2_size.x()][tile->m_log2_size.y()].insert(tile);
}

astral::TileAllocator::TileImplement*
astral::TileAllocator::
split_tile(TileImplement *tile, unsigned int log2_width, unsigned int log2_height)
{
  /* split_tile() is only called by allocate_tile()
   * on a tile that is either freshly created or removed from a
   * TileList, thus the tile is not actually available and
   * not have chlildren.
   */
  ASTRALassert(tile);
  ASTRALassert(!tile->m_available);
  ASTRALassert(!tile->has_children());

  while (tile->m_log2_size.x() != log2_width
         || tile->m_log2_size.y() != log2_height)
    {
      unsigned int split_coord;
      uvec2 log2_child_size(tile->m_log2_size);
      uvec3 location;

      ASTRALassert(log2_child_size.x() >= log2_width);
      ASTRALassert(log2_child_size.y() >= log2_height);

      /* factor splitting vertially over horizontally;
       * we do this to make the tile allocation more
       * horizontal.
       */
      if (log2_child_size.y() > log2_height)
        {
          split_coord = 1;
          ASTRALassert(log2_child_size.y() > 0u);
        }
      else
        {
          split_coord = 0;
          ASTRALassert(log2_child_size.x() > 0u);
        }

      --log2_child_size[split_coord];
      ASTRALassert(log2_child_size.x() >= log2_width);
      ASTRALassert(log2_child_size.y() >= log2_height);

      /* create the child[0] */
      tile->m_children[0] = create_tile(log2_child_size, tile->m_location, tile, 0);

      /* create the child[1] */
      location = tile->m_location;
      location[split_coord] += (1u << log2_child_size[split_coord]);
      tile->m_children[1] = create_tile(log2_child_size, location, tile, 1);

      /* add child[1] to the free list */
      m_free_tiles[log2_child_size.x()][log2_child_size.y()].insert(tile->m_children[1]);

      /* use tile[0]. We favor tile[0] because that is the min-side of
       * a tile split which allows for if using the TileAllocator to
       * allocate space in an offscreen surface to potentially use
       * a smaller surface.
       */
      tile = tile->m_children[0];

      ASTRALassert(tile);
      ASTRALassert(!tile->m_available);
      ASTRALassert(!tile->has_children());
    }

  return tile;
}

void
astral::TileAllocator::
number_layers(unsigned int new_value)
{
  ASTRALassert(new_value >= m_number_layers);
  m_number_layers = new_value;
}

void
astral::TileAllocator::
release_all(void)
{
  for (unsigned int x = 0; x <= m_log2_max_tile_size.x(); ++x)
    {
      for (unsigned int y = 0; y <= m_log2_max_tile_size.y(); ++y)
        {
          m_free_tiles[x][y].clear();
        }
    }
  m_pool->m_tile_pool.clear();
  m_pool->m_region_pool.clear();
  m_num_tiles_allocated = 0;
  m_alloc_tile_counter = uvec3(0u, 0u, 0u);
}

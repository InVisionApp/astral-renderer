/*!
 * \file tile_allocator.hpp
 * \brief file tile_allocator.hpp
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

#ifndef ASTRAL_TILE_ALLOCATOR_HPP
#define ASTRAL_TILE_ALLOCATOR_HPP

#include <vector>
#include <unordered_set>
#include <astral/util/c_array.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/rect.hpp>

namespace astral
{
/*!\addtogroup Utility
 * @{
 */

  /*!
   * \brief
   * An astral::TileAllocator is for allocating rectangles from a region
   * with the restrictions that:
   *  1. The width and height of each rectangle is a power of 2
   *  2. That power of 2 is no more than a value diven at contruction
   *     of an astral::TileAllocator, see log2_max_tile_size().
   *
   * In contrast to astral::RectAtlas, allocation of a tile on an
   * astral::TileAllocator is essentially O(1); strictly speaking the
   * worse case scenario is O((N - J) * (N - I)) were N is the value
   * of log2_max_tile_size(), I is the log2 of the width of the
   * allocation request and J is the log2 of the height.
   */
  class TileAllocator:noncopyable
  {
  public:
    /*!
     * Represents a region allocated by TileAllocator::allocate_tile().
     */
    class Tile:noncopyable
    {
    public:
      /*!
       * log2 of the size of the tile
       */
      uvec2
      log2_size(void) const;

      /*!
       * The location of the tile, absolute
       *   - (x, y) location in "pixels" within layer
       *   - z layer
       */
      uvec3
      location(void) const;

    private:
      friend class TileAllocator;

      Tile(void)
      {}
    };

    /*!
     * Represents a region allocated by TileAllocator::allocate_region().
     */
    class Region:noncopyable
    {
    public:
      /*!
       * The size of the region
       */
      uvec2
      size(void) const;

      /*!
       * The location of the region, absolute
       *   - (x, y) location in "pixels" within layer
       *   - z layer
       */
      uvec3
      location(void) const;

      /*!
       * Returns the number of tiles that compose the region
       */
      unsigned int
      number_tiles(void) const;

      /*!
       * Returns the rect of the named tile, the tile
       * is always on the same layer as the region.
       */
      RectT<unsigned int>
      tile(unsigned int I) const;

    private:
      friend class TileAllocator;

      Region(void)
      {}
    };

    /*!
     * Ctor.
     * \param log2_max_tile_size log2 of maximum tile size.
     * \param number_tiles_per_layer number of tiles in each dimension
     *                               per layer
     * \param number_layers number of layers
     */
    TileAllocator(uvec2 log2_max_tile_size,
                  uvec2 number_tiles_per_layer,
                  unsigned int number_layers);

    /*!
     * Ctor.
     * \param log2_max_tile_size log2 of maximum tile size.
     * \param number_tiles_per_layer number of tiles in each dimension
     *                               per layer
     * \param number_layers number of layers
     */
    TileAllocator(unsigned int log2_max_tile_size,
                  uvec2 number_tiles_per_layer,
                  unsigned int number_layers):
      TileAllocator(uvec2(log2_max_tile_size),
                    number_tiles_per_layer,
                    number_layers)
    {}

    ~TileAllocator();

    /*!
     * Allocate a tile
     * \param log2_width log2 of the width of the tile. Must be that
     *                   0 <= log2_width < log2_max_tile_size().x()
     * \param log2_height log2 of the height of the tile. Must be that
     *                    0 <= log2_height < log2_max_tile_size().y()
     */
    const Tile*
    allocate_tile(unsigned int log2_width, unsigned int log2_height);

    /*!
     * Allocate a region whose size is not a power of 2; internally
     * a region is composed of neighboring tiles.
     * \param width width of tile with 0 < width <= max_tile_size().x()
     * \param height height of tile with 0 < height <= max_tile_size().y()
     */
    const Region*
    allocate_region(unsigned int width, unsigned int height);

    /*!
     * Release a tile allocated with allocate_tile().
     * \param p Tile to release
     */
    void
    release_tile(const Tile *p);

    /*!
     * Release a region allocated with allocate_region().
     * \param p Region to release
     */
    void
    release_region(const Region *p);

    /*!
     * Release all tiles allocated with allocate_tile(). After
     * this call, all tiles that had been returned by allocate_tile()
     * previously become invalid.
     */
    void
    release_all(void);

    /*!
     * In additiont to calling release_all(), changes the
     * size of the TileAllocator
     */
    void
    change_size(uvec2 log2_max_tile_size,
                uvec2 number_tiles_per_layer,
                unsigned int number_layers);

    /*!
     * Resize the number of layers of the TileAllocator;
     * resize can only increase the number of layers.
     * \param new_value new value of number of layers with
     *                  new_value >= number_layers(void) const
     */
    void
    number_layers(unsigned int new_value);

    /*!
     * Returns the log2 of the tile size.
     */
    uvec2
    log2_max_tile_size(void) const
    {
      return m_log2_max_tile_size;
    }

    /*!
     * Returns the maximum size of a tile
     */
    uvec2
    max_tile_size(void) const
    {
      return m_max_tile_size;
    }

    /*!
     * Returns the number of layers of the TileAllocator
     */
    unsigned int
    number_layers(void) const
    {
      return m_number_layers;
    }

    /*!
     * Returns the maximum number of tiles per layer of
     * the size max_tile_size() allowed; the effective
     * width and height of a backing surface is given by
     * the product of this value with max_tile_size().
     */
    uvec2
    number_tiles_per_layer(void) const
    {
      return m_number_tiles_per_layer;
    }

    /*!
     * Returns the width and height of surface that would
     * back the tiles
     */
    uvec2
    required_backing_size(void) const
    {
      return m_number_tiles_per_layer * m_max_tile_size;
    }

    /*!
     * Returns the number of tiles that have been allocated
     * and not released.
     */
    unsigned int
    num_tiles_allocated(void) const
    {
      return m_num_tiles_allocated;
    }

    /*!
     * Returns the space of the tiles that have been allocated
     * and not released.
     */
    unsigned int
    space_allocated(void) const
    {
      return m_space_allocated;
    }

  private:
    class TileImplement;
    class RegionImplement;
    class TileList;
    class MemoryPool;

    void
    ready_lists(void);

    void
    create_ordering(std::vector<uvec2> *dst, unsigned int x, unsigned int y);

    void
    free_tile(Tile *p);

    TileImplement*
    create_tile(const uvec2 &log2_size,
                const uvec3 &location,
                TileImplement *parent, unsigned int id);

    TileImplement*
    create_base_tile(void);

    TileImplement*
    split_tile(TileImplement *tile, unsigned int log2_width, unsigned int log2_height);

    void
    split_add_tile(unsigned int coordinate, TileImplement *tile,
                    unsigned int keep_size,
                    std::vector<TileImplement*> *dst_allocated);

    /* m_free_tiles[W][H] gives the list of available
     * tiles of size (2^W, 2^H) with W <= m_log2_max_tile_size
     * and H <= m_log2_max_tile_size.
     */
    std::vector<std::vector<TileList>> m_free_tiles;

    /* m_order[W][H] is a an array of uvec2 values V
     * where W + V.x() <= m_log2_max_tile_size and
     * H + V.y() <= m_log2_max_tile_size ordered in ascending
     * order by (min(V.x(), V.y()), V.x() + V.y()).
     * The idea of the sorting is that a perfect
     * match in one dimension is the most strongly
     * favored and if such a match is not available,
     * then have the least number of total splits
     * needed. The last element is the value V where
     * W + V.x() = m_log2_max_tile_size and
     * H + V.y() = m_log2_max_tile_size
     */
    std::vector<std::vector<std::vector<uvec2>>> m_order;

    /* log2 of maximum tile size */
    uvec2 m_log2_max_tile_size;

    /* max tile size */
    uvec2 m_max_tile_size;

    /* number of tiles in each dimension per layer */
    uvec2 m_number_tiles_per_layer;

    /* number of layers */
    unsigned int m_number_layers;

    /* counter from where to allocate next root tile,
     * incremented when a root tile is requested and
     * m_free_root_tile is empty
     */
    uvec3 m_alloc_tile_counter;

    /* pool to allocate tiles */
    MemoryPool *m_pool;
    unsigned int m_num_tiles_allocated, m_space_allocated;
  };
/*! @} */
}

#endif

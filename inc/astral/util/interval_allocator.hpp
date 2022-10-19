/*!
 * \file interval_allocator.hpp
 * \brief file interval_allocator.hpp
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

#ifndef ASTRAL_LINE_ALLOCATOR_HPP
#define ASTRAL_LINE_ALLOCATOR_HPP

#include <set>
#include <vector>
#include <astral/util/util.hpp>

namespace astral
{
/*!\addtogroup Utility
 * @{
 */

  /*!
   * An astral::IntervalAllocator allocates Wx1 intervals from an
   * array of 1D regions.
   */
  class IntervalAllocator:noncopyable
  {
  public:
    /*!
     * An Interval represents an interval allocated from
     * an IntervalAllocator.
     */
    class Interval
    {
    public:
      /*!
       * Returns the location of the interval
       */
      range_type<int>
      range(void) const;

      /*!
       * Returns on what layer the interval resides
       */
      int
      layer(void) const;

    private:
      friend class IntervalAllocator;

      Interval()
      {}
    };

    /*!
     * Ctor.
     * \param length length of each layer
     * \param number_layers number of layers
     */
    IntervalAllocator(unsigned int length, unsigned int number_layers);

    ~IntervalAllocator();

    /*!
     * Allocate an interval; a return value of null means
     * that there is no continous room available for the
     * allocation.
     */
    const Interval*
    allocate(int size);

    /*!
     * Release an interval previously allocated by allocate().
     */
    void
    release(const Interval*);

    /*!
     * Increase the number of layers
     */
    void
    number_layers(unsigned int L);

    /*!
     * Returns the number of layers.
     */
    unsigned int
    number_layers(void) const
    {
      return m_number_layers;
    }

    /*!
     * Returns the length of each layer
     */
    int
    layer_length(void) const
    {
      return m_layer_length;
    }

    /*!
     * Increase the length of each layer
     */
    void
    layer_length(int L);

    /*!
     * Reset and resize the IntervalAllocator
     * \param length length of each layer
     * \param number_layers number of layers
     */
    void
    clear(unsigned int length, unsigned int number_layers);

    /*!
     * Returns true if and only if there is atleast one interval
     * returned by allocate() that has not been passed to release()
     * since the last call to clear().
     */
    bool
    has_live_intervals(void)
    {
      return m_number_allocated != 0u;
    }

    /*!
     * Just for debugging; prints to std::cout all intervals
     * (free and allocated) of a layer and returns the number
     * of allocated intervals on that layer
     */
    unsigned int
    check(unsigned int layer);

    /*!
     * Just for debugging; prints to std::cout all intervals
     * (free and allocated) of all layers and returns the number
     * of allocated intervals across all layers
     */
    unsigned int
    check(void);

  private:
    class MemoryPool;
    class Freelist;
    class IntervalImplement;
    class Layer;

    /* length of each layer */
    int m_layer_length;

    /* number of layers */
    unsigned int m_number_layers;

    /* number of intervals allocated */
    int m_number_allocated;

    /* for each "layer", we have a linked list of Interval values,
     * where an Interval can be allocated or free. When an Interval
     * is allocated or freed it affects the linked list. On allocation
     * a free interval is split to be the exact size needed. On free,
     * the interval is returned and is merged with free neighbors.
     * The entries of the linked list is embodied by IntervalImplement
     * and the linked list itself is embodied by Layer
     */
    std::vector<Layer> m_layer;

    /* for each size possible, we maintain a list of free intervals
     * of that exact size. The free list is embodied by Freelist.
     */
    std::vector<Freelist> m_free_list;

    /* An std::set of those m_free_list objects that have intervals.
     *
     * TODO: do not use a set, but something else to make cache
     *       friendly and to avoid memory allocation noise.
     */
    std::set<int> m_available_sizes;

    /* the memory pool to remove memory allocation noise */
    MemoryPool *m_pool;
  };
/*! @} */

}

#endif

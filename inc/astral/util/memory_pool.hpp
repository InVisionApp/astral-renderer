/*!
 * \file memory_pool.hpp
 * \brief file memory_pool.hpp
 *
 * Copyright 2019 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#ifndef ASTRAL_SIMPLE_POOL_HPP
#define ASTRAL_SIMPLE_POOL_HPP

#include <astral/util/util.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/astral_memory.hpp>

#include <vector>
#include <typeinfo>

namespace astral {

/*!\addtogroup Utility
 * @{
 */

/*!
 * If true is passed, whenever any astral::MemoryPool
 * allocates memory, a message will be printed
 * to std::cout of the MemoryPool type and size.
 */
void
track_memory_pool_memory_allocations(bool);

/*!
 * Returns the last value passed to
 * track_memory_pool_memory_allocations(bool);
 * if it has not been called, return false.
 */
bool
track_memory_pool_memory_allocations(void);

///@cond
namespace detail
{
void
memory_pool_allocated_subpool(const char *T, size_t PoolSize, const void *pool, unsigned int count);
}
///@endcond

/*!
 * \brief
 * Memory pool allocator that does not call dtors.
 *
 * A MemoryPool embodies a memory pool allocator where it
 * internally grows its pool of pools. On clearing a \ref
 * MemoryPool, the descontructor of the objects allocated
 * with the create() are NOT called, the dtor's are also NOT
 * called at the dtor of MemoryPool. A MemoryPool
 * should only be used when the dtor of the type T is a no-op.
 * \tparam T object type the MemoryPool will create
 * \tparam PoolSize the number of T's that are present in a
 *         single sub-pool of the MemoryPool
 */
template<typename T, size_t PoolSize>
class MemoryPool:astral::noncopyable
{
public:
  MemoryPool(void):
    m_total_allocated(0)
  {}

  ~MemoryPool()
  {
    ASTRALassert(m_total_allocated == 0);
    for (SinglePool *p : m_all)
      {
        ASTRALdelete(p);
      }
  }

  /*!
   * Create an object of type T. The memory is allocated
   * from the pool and ctor is called.
   */
  template<typename ...Args>
  T*
  create(Args&&... args)
  {
    void *data;
    T *p;

    data = allocate();
    p = new(data) T(std::forward<Args>(args)...);
    ASTRALassert(p == data);

    return p;
  }

  /*!
   * Allocate memory from the pool without calling the
   * ctor of T.
   */
  void*
  allocate(void)
  {
    void *return_value;

    ++m_total_allocated;
    if (!m_reclaimed.empty())
      {
        return_value = m_reclaimed.back();
        m_reclaimed.pop_back();
        return return_value;
      }

    if (m_usable.empty())
      {
        if (track_memory_pool_memory_allocations())
          {
            detail::memory_pool_allocated_subpool(typeid(T).name(), PoolSize, this, m_all.size());
          }
        m_usable.push_back(ASTRALnew SinglePool());
        m_all.push_back(m_usable.back());
        astral::memory::tag_object(m_all.back(), typeid(T).name());
      }

    return_value = m_usable.back()->allocate();
    if (m_usable.back()->full())
      {
        m_usable.pop_back();
      }

    return return_value;
  }

  /*!
   * Nuke the pool. All obects allocated via create() are
   * invalidated and their memory reclaimed. The dtor's of
   * the objects are NOT called.
   */
  void
  clear(void)
  {
    m_total_allocated = 0;
    m_reclaimed.clear();
    m_usable.clear();
    for (SinglePool *pool : m_all)
      {
        pool->clear();
        m_usable.push_back(pool);
      }
  }

  /*!
   * Return an object returned by create() to the memory
   * pool. The dtor is NOT called. It is an error to
   * pass a pointer that was not returned by allocate()
   * or create().
   */
  void
  reclaim(T *p)
  {
    ASTRALassert(m_total_allocated > 0);
    --m_total_allocated;
    m_reclaimed.push_back(p);
  }

private:
  typedef typename std::aligned_storage<sizeof(T), alignof(T)>::type element;

  class SinglePool
  {
  public:
    SinglePool(void):
      m_allocated(0)
    {}

    void*
    allocate(void)
    {
      unsigned int ct(m_allocated);

      ASTRALassert(m_allocated < m_pool.size());
      ++m_allocated;

      return &m_pool[ct];
    }

    bool
    full(void) const
    {
      return m_allocated == m_pool.size();
    }

    void
    clear(void)
    {
      m_allocated = 0;
    }

  private:
    vecN<element, PoolSize> m_pool;
    unsigned int m_allocated;
  };

  /* We have an array of SinglePool pointers instead
   * a single array because resize'ing an std::vector
   * would change where the objects are in memory
   * which would break the promise that allocate()
   * makes; so we have an array of pointers to SinglePool
   * objects.
   */
  std::vector<void*> m_reclaimed;
  std::vector<SinglePool*> m_usable, m_all;
  unsigned int m_total_allocated;
};

/*!
 * Similair to astral::MemoryPool, but keeps track
 * of all allocated objects since the last call
 * to clear(). The cost of that feature is that
 * astral::MemoryPoolTracked lacks the ability to
 * reclaim the backing of individual objects.
 */
template<typename T, size_t PoolSize>
class MemoryPoolTracked:astral::noncopyable
{
public:
  /*!
   * Create an object of type T. The memory is allocated
   * from the pool and ctor is called. In addition, the
   * following is guaranteed:
   * \code
   * idx = created_objects().size();
   * p = create(...args);
   * ASTRALassert(p == created_object(idx));
   * \endcode
   */
  template<typename ...Args>
  T*
  create(Args&&... args)
  {
    void *q;
    T *p;

    /* This is a little awkard, but necessary, we need
     * to add the m_created list BEFORE issuing the ctor
     * since the ctor might issue create() as well.
     */
    q = m_pool.allocate();
    m_created.push_back(static_cast<T*>(q));
    p = new(q) T(std::forward<Args>(args)...);

    return p;
  }

  /*!
   * Nuke the pool; where all objects that have been
   * returned by create() have their memory reclaimed;
   * this does NOT call their dtors.
   */
  void
  clear(void)
  {
    m_created.clear();
    m_pool.clear();
  }

  /*!
   * Returns an astral::c_array of all objects
   * that have been returned by create() since
   * the last call to clear(). The return value
   * is only guaranteed to be valid until clear()
   * of create() is called again.
   */
  c_array<const pointer<T>>
  created_objects(void) const
  {
    return make_c_array(m_created);
  }

  /*!
   * Equivalent to
   * \code
   * created_objects()[idx]
   * \endcode
   */
  T*
  created_object(unsigned int idx) const
  {
    ASTRALassert(idx < m_created.size());
    return m_created[idx];
  }

private:
  MemoryPool<T, PoolSize> m_pool;
  std::vector<T*> m_created;
};

/*!
 * In contrast to astral::MemoryPool, an
 * astral::MemoryObjectPool will issue the dtor
 * of objects at clear() and at its dtor. However,
 * astral::MemoryObjectPool lacks the ability
 * to reclaim the backing of individual objects.
 * It is the backing of the objects that is reused,
 * not the objects themselves. If one wants to reuse
 * objects, look to astral::ObjectPool.
 */
template<typename T, size_t PoolSize>
class MemoryObjectPool:astral::noncopyable
{
public:
  ~MemoryObjectPool()
  {
    m_backing.clear();
  }

  /*!
   * Create an object of type T. The memory is allocated
   * from the pool and ctor is called. In addition, the
   * following is guaranteed:
   * \code
   * idx = created_objects().size();
   * p = create(...args);
   * ASTRALassert(p == created_object(idx));
   * \endcode
   */
  template<typename ...Args>
  T*
  create(Args&&... args)
  {
    return m_backing.create(std::forward<Args>(args)...);
  }

  /*!
   * Nuke the pool; where all objects that have been
   * returned by create() have their dtor's called
   * and their backing returned for reuse.
   */
  void
  clear(void)
  {
    for (T* p : m_backing.created_objects())
      {
        p->~T();
      }
    m_backing.clear();
  }

  /*!
   * Returns an astral::c_array of all objects
   * that have been returned by create() since
   * the last call to clear(). The return value
   * is only guaranteed to be valid until clear()
   * of create() is called again.
   */
  c_array<const pointer<T>>
  created_objects(void) const
  {
    return m_backing.created_objects();
  }

  /*!
   * Equivalent to
   * \code
   * created_objects()[idx]
   * \endcode
   */
  T*
  created_object(unsigned int idx) const
  {
    return m_backing.created_object(idx);
  }

private:
  MemoryPoolTracked<T, PoolSize> m_backing;
};

/*! @} */

}

#endif

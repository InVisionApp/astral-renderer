/*!
 * \file object_pool.hpp
 * \brief file object_pool.hpp
 *
 * Copyright 2021 by Intel.
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

#ifndef ASTRAL_OBJECT_POOL_HPP
#define ASTRAL_OBJECT_POOL_HPP

#include <astral/util/util.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/astral_memory.hpp>
#include <astral/util/memory_pool.hpp>

namespace astral
{

/*!\addtogroup Utility
 * @{
 */
  /*!
   * Enumeration to specify what happen when an object is
   * recycled in astral::ObjectPool
   */
  enum object_pool_recycle_type_t
    {
      /*!
       * when an object is recycled by astral::ObjectPool,
       * the object is placed directly back on the reuse
       * list.
       */
      object_pool_noop_recycle,

      /*!
       * when an object is recycled by astral::ObjectPool,
       * the member function clear() is called on the object;
       * an astral::ObjectPool may call clear() more than once
       * on the same object even if it is not put into use.
       */
      object_pool_clear_recycle
    };

  ///@cond
  namespace detail
  {
    template<enum astral::object_pool_recycle_type_t>
    class ObjectPoolRecycler
    {};

    template<>
    class ObjectPoolRecycler<astral::object_pool_noop_recycle>
    {
    public:
      template<typename T>
      static
      void
      on_recycle(T*)
      {}
    };

    template<>
    class ObjectPoolRecycler<astral::object_pool_clear_recycle>
    {
    public:
      template<typename T>
      static
      void
      on_recycle(T *p)
      {
        p->clear();
      }
    };
  }
  ///@endcond

  /*!
   * \brief
   * An astral::ObjectPool represents a pool of objects
   * for reuse. The dtor's of the objects are called
   * when the astral::ObjectPool's dtor is called.
   * \tparam tp specifies if the type T has a clear()
   *            member function that is to be called whenever
   *            an object is recycled
   * \tparam T object T of the pool; T must have a public
   *           constructor that takes no arguments
   * \tparam PoolSize number of objects per sub-pool
   */
  template<enum object_pool_recycle_type_t tp, typename T, size_t PoolSize = 512>
  class ObjectPool:astral::noncopyable
  {
  public:
    /*!
     * Ctor to initialize
     */
    ObjectPool(void):
      m_live_count(0u),
      m_current(0u)
    {}

    ~ObjectPool()
    {
      ASTRALassert(m_live_count == 0u);
      for (T *p : m_object_pool)
        {
          p->~T();
        }
      m_pool.clear();
    }

    /*!
     * Clear all T objects returned by allocate()
     * and return them to the astral::ObjectPool
     * for reuse; after this call all objects returned
     * by previous calls to allocate() are invalid.
     */
    void
    clear(void)
    {
      m_reclaimed.clear();
      for (unsigned int i = 0; i < m_current; ++i)
        {
          detail::ObjectPoolRecycler<tp>::on_recycle(m_object_pool[i]);
        }
      m_current = 0;
      m_live_count = 0;
    }

    /*!
     * Fetch an object in the free pool. If no such
     * object is in the free-pool create a new object.
     */
    T*
    allocate(void)
    {
      T *p;

      ++m_live_count;
      if (!m_reclaimed.empty())
        {
          /* when we can reclaim a Image, the ctor is not
           * needed to be called because it already was.
           */
          p = m_reclaimed.back();
          m_reclaimed.pop_back();

          return p;
        }
      else if (m_current >= m_object_pool.size())
        {
          void *vq;
          T *tq;

          vq = m_pool.allocate();
          tq = new(vq) T();

          /* when not reclaiming, we need to call the ctor */
          m_object_pool.push_back(tq);
        }

      ++m_current;
      return m_object_pool[m_current - 1u];
    }

    /*!
     * Reclaim the memort for an object for reuse.
     */
    void
    reclaim(T *p)
    {
      ASTRALassert(m_live_count > 0);
      --m_live_count;
      detail::ObjectPoolRecycler<tp>::on_recycle(p);
      m_reclaimed.push_back(p);
    }

    /*!
     * returns the number of objects that are alive.
     */
    unsigned int
    live_count(void) const
    {
      return m_live_count;
    }

  private:
    unsigned int m_live_count, m_current;
    std::vector<T*> m_reclaimed;
    std::vector<T*> m_object_pool;
    MemoryPool<T, PoolSize> m_pool;
  };

  /*!
   * Conveniance aliasing to give the type for an astral::ObjectPool
   * where obejcts when reclaimed are directly placed on the reuse
   * list.
   */
  template<typename T, size_t PoolSize = 512>
  using ObjectPoolDirect = ObjectPool<object_pool_noop_recycle, T, PoolSize>;

  /*!
   * Conveniance aliasing to give the type for an astral::ObjectPool
   * where obejcts when reclaimed have their clear() method called
   * just before being placed on the reuse list.
   */
  template<typename T, size_t PoolSize = 512>
  using ObjectPoolClear = ObjectPool<object_pool_clear_recycle, T, PoolSize>;
/*! @} */
}

#endif

/*!
 * \file reference_count_atomic.hpp
 * \brief file reference_count_atomic.hpp
 *
 * Copyright 2016 by Intel.
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


#ifndef ASTRAL_REFERENCE_COUNT_ATOMIC_HPP
#define ASTRAL_REFERENCE_COUNT_ATOMIC_HPP

#include <astral/util/util.hpp>
#include <atomic>

namespace astral
{
/*!\addtogroup Utility
 * @{
 */

  /*!
   * \brief
   * Reference counter that is thread safe by
   * having increment and decrement operations
   * by atomic operations, this is usually faster
   * (and much faster) than using a mutex.
   */
  class reference_count_atomic:noncopyable
  {
  public:
    /*!
     * Initializes the counter as zero.
     */
    reference_count_atomic(void) noexcept:
      m_reference_count(0)
    {}

    ~reference_count_atomic() noexcept
    {}

    /*!
     * Increment reference counter by 1.
     */
    void
    add_reference(void) noexcept
    {
      m_reference_count.fetch_add(1, std::memory_order_relaxed);
    }

    /*!
     * Decrements the counter by 1 and returns status of if the counter
     * is 0 after the decrement operation.
     */
    bool
    remove_reference(void) noexcept
    {
      bool return_value;
      int R(m_reference_count.fetch_sub(1, std::memory_order_release));

      ASTRALassert(R >= 1);
      if (R == 1)
        {
          std::atomic_thread_fence(std::memory_order_acquire);
          return_value = true;
        }
      else
        {
          return_value = false;
        }

      return return_value;
    }

    /*!
     * returns the current value of the reference count
     */
    unsigned int
    value(void) const
    {
      return m_reference_count.load();
    }

  private:
    std::atomic<int> m_reference_count;
  };
/*! @} */
}

#endif

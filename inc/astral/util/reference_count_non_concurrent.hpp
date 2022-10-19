/*!
 * \file reference_count_non_concurrent.hpp
 * \brief file reference_count_non_concurrent.hpp
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


#ifndef ASTRAL_REFERENCE_COUNT_NON_CONCURRENT_HPP
#define ASTRAL_REFERENCE_COUNT_NON_CONCURRENT_HPP

#include <astral/util/util.hpp>

namespace astral
{
/*!\addtogroup Utility
 * @{
 */

  /*!
   * \brief
   * Reference counter that is NOT thread safe
   */
  class reference_count_non_concurrent:noncopyable
  {
  public:
    /*!
     * Initializes the counter as zero.
     */
    reference_count_non_concurrent(void) noexcept:
      m_reference_count(0)
    {}

    ~reference_count_non_concurrent() noexcept
    {
      ASTRALassert(m_reference_count == 0);
    }

    /*!
     * Increment reference counter by 1.
     */
    void
    add_reference(void) noexcept
    {
      ++m_reference_count;
    }

    /*!
     * Decrements the counter by 1 and returns status of if the counter
     * is 0 after the decrement operation.
     */
    bool
    remove_reference(void) noexcept
    {
      --m_reference_count;

      ASTRALassert(m_reference_count >= 0);
      return m_reference_count == 0;
    }

    /*!
     * returns the current value of the reference count
     */
    unsigned int
    value(void) const
    {
      return m_reference_count;
    }

  private:
    int m_reference_count;
  };
/*! @} */
}

#endif

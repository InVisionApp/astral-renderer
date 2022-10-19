/*!
 * \file enum_flags.hpp
 * \brief file enum_flags.hpp
 *
 * Copyright 2029 by InvisionApp.
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

#ifndef ASTRAL_ENUM_FLAGS_HPP
#define ASTRAL_ENUM_FLAGS_HPP

#include <astral/util/vecN.hpp>

namespace astral
{
/*!\addtogroup Utility
 * @{
 */

  /*!
   * An astral::EnumFlags is essentially a staticially sized
   * array of bools but packed into an array of uint32_t's.
   * \param T the indexing type to use to access the boolean values
   * \param size the number of bools
   */
  template<typename T, uint32_t size>
  class EnumFlags
  {
  public:
    /*!
     * Enumeration to hold various constants
     */
    enum constants_t:uint32_t
      {
        /*!
         * size of the uint32_t array to back the booleans
         */
        backing_size = 1u + ((size - 1u) >> 5u),

        /*!
         * number of bits used in the last element of \ref m_backing
         */
        number_bits_used_in_last_element = size - 32u * (backing_size - 1u)
      };

    /*!
     * Ctor initializing all values as false
     */
    EnumFlags(void):
      m_backing(0u)
    {}

    /*!
     * Return the value stored at a named slot
     * \param tv slot to query
     */
    bool
    value(T tv) const
    {
      uint32_t e, m;

      e = backing_element(tv);
      m = backing_mask(tv);

      return (m_backing[e] & m) != 0u;
    }

    /*!
     * Set the value returned by value(T).
     * \param tv slot to set
     * \param b new value for slot named by tv
     */
    EnumFlags&
    value(T tv, bool b)
    {
      uint32_t e, m, new_value;

      e = backing_element(tv);
      m = backing_mask(tv);

      new_value = (b) ?
        (m_backing[e] | m) :
        (m_backing[e] & ~m);

      m_backing[e] = new_value;

      return *this;
    }

    /*!
     * Returns the index into backing_values()
     * that holds the bit for the queried slot
     */
    static
    uint32_t
    backing_element(T tv)
    {
      uint32_t v;

      v = static_cast<uint32_t>(tv);
      ASTRALassert(v < size);
      return v >> 5u;
    }

    /*!
     * Returns which bit that used to store for the queried
     * slot
     */
    static
    uint32_t
    backing_bit(T tv)
    {
      uint32_t v, bit;

      v = static_cast<uint32_t>(tv);
      ASTRALassert(v < size);
      bit = v & 31u;
      return bit;
    }

    /*!
     * Returns the mask whose only bit that is
     * up is the bit used to store for the queried
     * slot
     */
    static
    uint32_t
    backing_mask(T tv)
    {
      return 1u << backing_bit(tv);
    }

    /*!
     * Values that back the bools enumerated by the template
     * enum parameter. See backing_element() and backing_mask()
     * on where a flag is located within \ref m_backing.
     */
    vecN<uint32_t, backing_size> m_backing;
  };

/*! @} */
}

#endif

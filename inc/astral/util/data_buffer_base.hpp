/*!
 * \file data_buffer_base.hpp
 * \brief file data_buffer_base.hpp
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

#ifndef ASTRAL_DATA_BUFFER_BASE_HPP
#define ASTRAL_DATA_BUFFER_BASE_HPP

#include <stdint.h>
#include <astral/util/reference_counted.hpp>
#include <astral/util/c_array.hpp>

namespace astral
{
/*!\addtogroup Utility
 * @{
 */

  /*!
   * \brief
   * Base class for passing around buffers of data; derived
   * classes have the responsibility of maintaining storage
   * cleanup at destruction.
   */
  class DataBufferBase:public reference_counted<DataBufferBase>::concurrent
  {
  public:
    /*!
     * Ctor.
     * \param pdata_ro value which data_ro() will return
     * \param pdata_rw value which data_rw() will return
     */
    static
    reference_counted_ptr<DataBufferBase>
    create(c_array<const uint8_t> pdata_ro,
           c_array<uint8_t> pdata_rw)
    {
      return ASTRALnew DataBufferBase(pdata_ro, pdata_rw);
    }

    /*!
     * Return the memory as read-only
     */
    c_array<const uint8_t>
    data_ro(void) const
    {
      return m_data_ro;
    }

    /*!
     * Return the memory as read-write
     */
    c_array<uint8_t>
    data_rw(void)
    {
      return m_data_rw;
    }

  protected:
    /*!
     * Protected ctor for when the data buffer
     * values are not ready until after additional
     * initialization work
     */
    DataBufferBase(void)
    {}

    /*!
     * Protected ctor.
     * \param pdata_ro value which data_ro() will return
     * \param pdata_rw value which data_rw() will return
     */
    DataBufferBase(c_array<const uint8_t> pdata_ro,
                   c_array<uint8_t> pdata_rw):
      m_data_ro(pdata_ro),
      m_data_rw(pdata_rw)
    {}

    /*!
     * Set the values returned by data_ro() and data_rw().
     * \param pdata_ro value which data_ro() will return
     * \param pdata_rw value which data_rw() will return
     */
    void
    set_data(c_array<const uint8_t> pdata_ro,
             c_array<uint8_t> pdata_rw)
    {
      m_data_ro = pdata_ro;
      m_data_rw = pdata_rw;
    }

  private:
    c_array<const uint8_t> m_data_ro;
    c_array<uint8_t> m_data_rw;
  };
/*! @} */

} //namespace astral

#endif

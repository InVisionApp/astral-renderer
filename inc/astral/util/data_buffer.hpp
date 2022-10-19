/*!
 * \file data_buffer.hpp
 * \brief file data_buffer.hpp
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

#ifndef ASTRAL_DATA_BUFFER_HPP
#define ASTRAL_DATA_BUFFER_HPP

#include <astral/util/data_buffer_base.hpp>
#include <vector>

namespace astral {

/*!\addtogroup Utility
 * @{
 */

  /*!
   * \brief
   * Represents a buffer directly stored in memory.
   */
  class DataBufferBackingStore
  {
  public:
    /*!
     * Ctor.
     * Copies a file into memory.
     * \param filename name of file to open
     */
    DataBufferBackingStore(c_string filename);

    /*!
     * Ctor. Allocate memory and fill the buffer
     * with a fixed value.
     * \param num_bytes number of bytes to give the backing store
     * \param init initial value to give each byte
     */
    explicit
    DataBufferBackingStore(unsigned int num_bytes, uint8_t init = uint8_t(0));

    /*!
     * Ctor. Allocates the memory and initializes it with data.
     */
    explicit
    DataBufferBackingStore(c_array<const uint8_t> init_data);

    /*!
     * Move ctor
     */
    DataBufferBackingStore(DataBufferBackingStore &&pstore):
      m_data(std::move(pstore.m_data))
    {}

    ~DataBufferBackingStore();

    /*!
     * Return a pointer to the backing store of the memory.
     */
    c_array<uint8_t>
    data(void)
    {
      return make_c_array(m_data);
    }

    /*!
     * Return a pointer to the backing store of the memory.
     */
    c_array<const uint8_t>
    data(void) const
    {
      return make_c_array(m_data);
    }

  private:
    std::vector<uint8_t> m_data;
  };

  /*!
   * \brief
   * DataBuffer is an implementation of DataBufferBase where the
   * data is directly backed by memory
   */
  class DataBuffer:public DataBufferBase
  {
  public:
    /*!
     * Ctor. Initialize the DataBuffer to be backed by uninitalized
     * memory. Use the pointer returned by data() to set the data.
     * \param num_bytes number of bytes to give the backing store
     * \param init initial value to give each byte
     */
    static
    reference_counted_ptr<DataBuffer>
    create(unsigned int num_bytes, uint8_t init = uint8_t(0))
    {
      return ASTRALnew DataBuffer(num_bytes, init);
    }

    /*!
     * Ctor. Initialize the DataBuffer to be backed by memory
     * whose value is copied from a file.
     */
    static
    reference_counted_ptr<DataBuffer>
    create(c_string filename)
    {
      return ASTRALnew DataBuffer(filename);
    }

    /*!
     * Ctor. Initialize the DataBuffer to be backed by memory
     * whose value is *copied* from a c_array.
     */
    static
    reference_counted_ptr<DataBuffer>
    create(c_array<const uint8_t> init_data)
    {
      return ASTRALnew DataBuffer(init_data);
    }

  private:
    DataBuffer(unsigned int num_bytes, uint8_t init = uint8_t(0)):
      m_store(num_bytes, init)
    {
      DataBufferBase::set_data(m_store.data(), m_store.data());
    }

    explicit
    DataBuffer(c_string filename):
      m_store(filename)
    {
      DataBufferBase::set_data(m_store.data(), m_store.data());
    }

    explicit
    DataBuffer(c_array<const uint8_t> init_data):
      m_store(init_data)
    {
      DataBufferBase::set_data(m_store.data(), m_store.data());
    }

    DataBufferBackingStore m_store;
  };
/*! @} */

} //namespace astral

#endif

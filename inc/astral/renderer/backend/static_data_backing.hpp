/*!
 * \file static_data_backing.hpp
 * \brief file static_data_backing.hpp
 *
 * Copyright 2019 by InvisionApp.
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

#ifndef ASTRAL_STATIC_DATA_BACKING_HPP
#define ASTRAL_STATIC_DATA_BACKING_HPP

#include <vector>
#include <astral/util/vecN.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/reference_counted.hpp>
#include <astral/util/interval_allocator.hpp>

namespace astral
{
/*!\addtogroup RendererBackend
 * @{
 */

  ///@cond
  class StaticData;
  class StaticDataBacking;
  class StaticDataAllocator16;
  class StaticDataAllocator32;
  ///@endcond

  /*!
   * \brief
   * An astral::StaticDataBacking is the backing of an
   * astral::StaticDataAllocator16 or astral::StaticDataAllocator32
   * which views it as a linear array.
   */
  class StaticDataBacking:public reference_counted<StaticDataBacking>::non_concurrent
  {
  public:
    /*!
     * Enumerates the data type that an astral::StaticDataBacking
     * backs.
     */
    enum type_t:uint32_t
      {
        /*!
         * Indicates that each element of the backing
         * is a four-tuple of 32-bit values.
         */
        type32,

        /*!
         * Indicates that each element of the backing
         * is a four tuple of 16-bit values.
         */
        type16
      };

    /*!
     * Ctor.
     * \param tp the type of data to be backed
     * \param size initial size of the linear array
     */
    explicit
    StaticDataBacking(enum type_t tp, unsigned int size):
      m_type(tp),
      m_size(size)
    {}

    virtual
    ~StaticDataBacking()
    {}

    /*!
     * Returns the \ref type_t which specifies
     * the kind of data backed.
     */
    enum type_t
    type(void) const
    {
      return m_type;
    }

    /*!
     * Returns the size when viewed as a linear buffer
     * of the astral::StaticDataBacking regardless of the
     * value of the layout().
     */
    unsigned int
    size(void) const
    {
      return m_size;
    }

    /*!
     * Set the data at linear offset L to the passed data
     * values. May only be called if type() returns
     * StaticDataBacking::type32.
     * \param L linear offset
     * \param data data to copy to backing store
     */
    void
    set_data(unsigned int L, c_array<const u32vec4> data)
    {
      ASTRALassert(type() == type32);
      set_data_implement(L, data.c_ptr(), data.size());
    }

    /*!
     * Set the data at linear offset L to the passed data
     * values. May only be called if type() returns
     * StaticDataBacking::type32.
     * \param L linear offset
     * \param data data to copy to backing store
     */
    void
    set_data(unsigned int L, c_array<const gvec4> data)
    {
      ASTRALassert(type() == type32);
      set_data_implement(L, data.c_ptr(), data.size());
    }

    /*!
     * Set the data at linear offset L to the passed data
     * values. May only be called if type() returns
     * StaticDataBacking::type16.
     * \param L linear offset
     * \param data data to copy to backing store
     */
    void
    set_data(unsigned int L, c_array<const u16vec4> data)
    {
      ASTRALassert(type() == type16);
      set_data_implement(L, data.c_ptr(), data.size());
    }

    /*!
     * Set the data at linear offset L to the passed data
     * values. May only be called if type() returns
     * StaticDataBacking::type16.
     * \param L linear offset
     * \param data data to copy to backing store
     */
    void
    set_data(unsigned int L, c_array<const u32vec2> data)
    {
      ASTRALassert(type() == type16);
      set_data_implement(L, data.c_ptr(), data.size());
    }

    /*!
     * enlarge the backing store size() to atleast this value;
     * returns the actual size it is enlarged to.
     * \param new_size new linear size
     */
    unsigned int
    resize(unsigned int new_size)
    {
      ASTRALassert(new_size > m_size);
      m_size = enlarge_implement(new_size);
      ASTRALassert(m_size >= new_size);

      return m_size;
    }

  private:
    /*!
     * To be implemented by a derived class to grow the backing;
     * The value returned by size() is the size() before the
     * resize request.
     * \param new_size to be resized to atleast this size
     * \reurns the actual new size of the object
     */
    virtual
    unsigned int
    enlarge_implement(unsigned int new_size) = 0;

    /*!
     * To be implemented by a derived calss to set the data
     * \param offset offset into store
     * \param data pointer to data to place in the store.
     *             If type() is \ref type_generic, then data
     *             points to an array of \ref gvec4 values.
     *             If type() is \ref type_fp16, then data
     *             points to an array of \ref u16vec4 values.
     * \param count length of the array pointed to by data
     */
    virtual
    void
    set_data_implement(unsigned int offset, const void *data, unsigned int count) = 0;

    enum type_t m_type;
    unsigned int m_size;
  };
/*! @} */
}

#endif

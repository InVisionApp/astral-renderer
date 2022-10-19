/*!
 * \file limited_vector.hpp
 * \brief file limited_vector.hpp
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

#ifndef ASTRAL_LIMITED_VECTOR_HPP
#define ASTRAL_LIMITED_VECTOR_HPP

#include <new>
#include <astral/util/vecN.hpp>
#include <astral/util/c_array.hpp>

namespace astral
{
/*!\addtogroup Utility
 * @{
 */

  /*!
   * \brief
   * An astral::LimitedVector provides an std::vector like
   * interface for resize operations. However, the backing
   * storage is not in the heap but rather as part of the
   * object AND the maximum size is a template argument.
   * Use this when one needs a dynamic array like behavious
   * but the maximum size needed is small and known at
   * compile time.
   */
  template<typename T, size_t N>
  class LimitedVector
  {
  public:
    /*!
     * \brief
     * STL compliant typedef
     */
    typedef T* pointer;

    /*!
     * \brief
     * STL compliant typedef
     */
    typedef const T* const_pointer;

    /*!
     * \brief
     * STL compliant typedef
     */
    typedef T& reference;

    /*!
     * \brief
     * STL compliant typedef
     */
    typedef const T& const_reference;

    /*!
     * \brief
     * STL compliant typedef
     */
    typedef T value_type;

    /*!
     * \brief
     * STL compliant typedef
     */
    typedef size_t size_type;

    /*!
     * \brief
     * STL compliant typedef
     */
    typedef ptrdiff_t difference_type;

    /*!
     * \brief
     * iterator typedef to pointer
     */
    typedef pointer iterator;

    /*!
     * \brief
     * iterator typedef to const_pointer
     */
    typedef const_pointer const_iterator;

    /*!
     * Ctor, initializes virtual size as 0.
     */
    LimitedVector(void):
      m_size(0)
    {}

    ~LimitedVector()
    {
      for (unsigned int j = 0; j < m_size; ++j)
        {
          T *p;
          p = c_ptr() + j;
          p->~T();
        }
    }

    /*!
     * Return a constant refernce to the j'th element.
     * \param j index of element to return.
     */
    const_reference
    operator[](size_type j) const
    {
      ASTRALassert(j < size());
      return c_ptr()[j];
    }

    /*!
     * Return a constant refernce to the j'th element.
     * \param j index of element to return.
     */
    reference
    operator[](size_type j)
    {
      ASTRALassert(j < size());
      return c_ptr()[j];
    }

    /*!
     * STL compliand size function.
     */
    size_t
    size(void) { return m_size; }

    /*!
     * Remove the last element
     */
    void
    pop_back(void)
    {
      ASTRALassert(m_size > 0u);
      --m_size;

      T *p;
      p = c_ptr() + m_size;
      p->~T();
    }

    /*!
     * Add an element
     */
    void
    push_back(const T &v)
    {
      ASTRALassert(m_size < N);

      T *p;
      p = c_ptr() + m_size;
      new(p) T(v);
      ++m_size;
    }

    /*!
     * Emplace an element
     */
    void
    push_back(T &&v)
    {
      ASTRALassert(m_size < N);

      T *p;
      p = c_ptr() + m_size;
      new(p) T(std::move(v));
      ++m_size;
    }

    /*!
     * Returns true if empty
     */
    bool
    empty(void) const
    {
      return m_size == 0u;
    }

    /*!
     * STL compliant iterator function.
     */
    iterator
    begin(void) { return iterator(c_ptr()); }

    /*!
     * STL compliant iterator function.
     */
    const_iterator
    begin(void) const { return const_iterator(c_ptr()); }

    /*!
     * STL compliant iterator function.
     */
    iterator
    end(void) { return iterator(c_ptr() + static_cast<difference_type>(size())); }

    /*!
     * STL compliant iterator function.
     */
    const_iterator
    end(void) const { return const_iterator(c_ptr() + static_cast<difference_type>(size())); }

    /*!
     * STL compliant back() function.
     */
    reference
    back(void) { ASTRALassert(!empty()); return (*this)[size() - 1]; }

    /*!
     * STL compliant back() function.
     */
    const_reference
    back(void) const { ASTRALassert(!empty()); return (*this)[size() - 1]; }

    /*!
     * STL compliant front() function.
     */
    reference
    front(void) { ASTRALassert(!empty()); return (*this)[0]; }

    /*!
     * STL compliant front() function.
     */
    const_reference
    front(void) const { ASTRALassert(!empty()); return (*this)[0]; }

  private:
    typedef typename std::aligned_storage<sizeof(T), alignof(T)>::type storage_type;

    T*
    c_ptr(void) { return reinterpret_cast<T*>(m_data.c_ptr()); }

    const T*
    c_ptr(void) const { return reinterpret_cast<const T*>(m_data.c_ptr()); }

    vecN<storage_type, N> m_data;
    unsigned int m_size;
  };

/*! @} */

} //namespace


#endif

/*!
 * \file c_array.hpp
 * \brief file c_array.hpp
 *
 * Adapted from: c_array.hpp of WRATH:
 *
 * Copyright 2013 by Nomovok Ltd.
 * Contact: info@nomovok.com
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */


#ifndef ASTRAL_C_ARRAY_HPP
#define ASTRAL_C_ARRAY_HPP

#include <cstdint>
#include <vector>
#include <astral/util/util.hpp>
#include <astral/util/vecN.hpp>

namespace astral
{
/*!\addtogroup Utility
 * @{
 */

/*!
 * \brief
 * A c_array is a wrapper over a
 * C pointer with a size parameter
 * to facilitate bounds checking
 * and provide an STL-like iterator
 * interface.
 */
template<typename T>
class c_array
{
public:

  /*!
   * \brief
   * STL compliant typedef
   */
  typedef T* pointer;

  /*!
   * \brief
   * STL compliant typedef; notice that const_pointer
   * is type T* and not const T*. This is because
   * a c_array is just a HOLDER of a pointer and
   * a length and thus the contents of the value
   * behind the pointer are not part of the value
   * of a c_array.
   */
  typedef T* const_pointer;

  /*!
   * \brief
   * STL compliant typedef
   */
  typedef T& reference;

  /*!
   * \brief
   * STL compliant typedef; notice that const_pointer
   * is type T& and not const T&. This is because
   * a c_array is just a HOLDER of a pointer and
   * a length and thus the contents of the value
   * behind the pointer are not part of the value
   * of a c_array.
   */
  typedef T& const_reference;

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
   * Default ctor, initializing the pointer as nullptr
   * with size 0.
   */
  c_array(void):
    m_size(0),
    m_ptr(nullptr)
  {}

  /*!
   * Ctor initializing the pointer and size
   * \tparam U type U* must be convertible to type T* AND
   *           the size of U must be the same as the size
   *           of T
   * \param pptr pointer value
   * \param sz size, must be no more than the number of elements that pptr points to.
   */
  template<typename U,
           astral::enable_if_t<std::is_convertible<U*, T*>::value, bool> = true,
           astral::enable_if_t<sizeof(U) == sizeof(T), bool> = true>
  c_array(U *pptr, size_type sz):
    m_size(sz),
    m_ptr(sz > 0 ? pptr : nullptr)
  {
    ASTRALstatic_assert(sizeof(U) == sizeof(T));
  }

  /*!
   * Ctor from a vecN, size is the size of the fixed size array
   * \tparam U type U* must be convertible to type T* AND
   *           the size of U must be the same as the size
   *           of T
   * \tparam N size of vecN
   * \param pptr fixed size array that c_array references, must be
   *             in scope as until c_array is changed
   */
  template<typename U, size_type N,
           astral::enable_if_t<std::is_convertible<U*, T*>::value, bool> = true,
           astral::enable_if_t<sizeof(U) == sizeof(T), bool> = true>
  c_array(vecN<U, N> &pptr):
    m_size(N),
    m_ptr(pptr.c_ptr())
  {
    ASTRALstatic_assert(sizeof(U) == sizeof(T));
  }

  /*!
   * Ctor from a vecN, size is the size of the fixed size array
   * \tparam U type U* must be convertible to type T* AND
   *           the size of U must be the same as the size
   *           of T
   * \tparam N size of vecN
   * \param pptr fixed size array that c_array references, must be
   *             in scope as until c_array is changed
   */
  template<typename U, size_type N,
           astral::enable_if_t<std::is_convertible<const U*, T*>::value, bool> = true,
           astral::enable_if_t<sizeof(U) == sizeof(T), bool> = true>
  c_array(const vecN<U, N> &pptr):
    m_size(N),
    m_ptr(pptr.c_ptr())
  {
    ASTRALstatic_assert(sizeof(U) == sizeof(T));
  }

  /*!
   * Ctor from another c_array object.
   * \tparam U type U* must be convertible to type T* AND
   *           the size of U must be the same as the size
   *           of T
   * \param obj value from which to copy
   */
  template<typename U,
           astral::enable_if_t<std::is_convertible<U*, T*>::value, bool> = true,
           astral::enable_if_t<sizeof(U) == sizeof(T), bool> = true>
  c_array(const c_array<U> &obj):
    m_size(obj.m_size),
    m_ptr(obj.m_ptr)
  {
    ASTRALstatic_assert(sizeof(U) == sizeof(T));
  }

  /*!
   * Resets the astral::c_array object to be equivalent to
   * a nullptr, i.e. c_ptr() will return nullptr and size()
   * will return 0.
   */
  void
  reset(void)
  {
    m_size = 0;
    m_ptr = nullptr;
  }

  /*!
   * Set the c_array
   * \param p pointer value
   * \param sz size, must be no more than the number of elements that pptr points to.
   */
  template<typename U>
  void
  set(U *p, unsigned int sz)
  {
    ASTRALstatic_assert(sizeof(U) == sizeof(T));
    m_size = sz;
    m_ptr = p;
  }

  /*!
   * Reinterpret style cast for c_array. It is required
   * that the sizeof(T)*size() evenly divides sizeof(S).
   * \tparam S type to which to be reinterpreted casted
   */
  template<typename S>
  c_array<S>
  reinterpret_pointer(void) const
  {
    S *ptr;
    size_type num_bytes(size() * sizeof(T));
    ASTRALassert(num_bytes % sizeof(S) == 0);
    ptr = reinterpret_cast<S*>(c_ptr());
    return c_array<S>(ptr, num_bytes / sizeof(S));
  }

  /*!
   * Const style cast for c_array. It is required
   * that the sizeof(T) is the same as sizeof(S).
   * \tparam S type to which to be const casted
   */
  template<typename S>
  c_array<S>
  const_cast_pointer(void) const
  {
    S *ptr;
    ASTRALstatic_assert(sizeof(S) == sizeof(T));
    ptr = const_cast<S*>(c_ptr());
    return c_array<S>(ptr, m_size);
  }

  /*!
   * For when T is astral::vecN<TT, N> for a type TT, flattens the astral::c_array<T>
   * into a astral::c_array<TT> refering to the same data.
   */
  c_array<typename unvecN<T>::type>
  flatten_array(void) const
  {
    typename unvecN<T>::type *p;
    p = reinterpret_cast<typename unvecN<T>::type*>(c_ptr());
    return c_array<typename unvecN<T>::type>(p, m_size * unvecN<T>::array_size);
  }

  /*!
   * Pointer of the c_array.
   */
  T*
  c_ptr(void) const
  {
    return m_ptr;
  }

  /*!
   * Pointer to the element one past
   * the last element of the c_array.
   */
  T*
  end_c_ptr(void) const
  {
    return m_ptr + m_size;
  }

  /*!
   * Access named element of c_array, under
   * debug build also performs bounds checking.
   * \param j index
   */
  reference
  operator[](size_type j) const
  {
    ASTRALassert(c_ptr() != nullptr);
    ASTRALassert(j < m_size);
    return c_ptr()[j];
  }

  /*!
   * STL compliant function.
   */
  bool
  empty(void) const
  {
    return m_size == 0;
  }

  /*!
   * STL compliant function.
   */
  size_type
  size(void) const
  {
    return m_size;
  }

  /*!
   * STL compliant function.
   */
  iterator
  begin(void) const
  {
    return iterator(c_ptr());
  }

  /*!
   * STL compliant function.
   */
  iterator
  end(void) const
  {
    return iterator(c_ptr() + static_cast<difference_type>(size()));
  }

  /*!
   * Returns the range of the c_array as an
   * iterator range.
   */
  range_type<iterator>
  range(void) const
  {
    return range_type<iterator>(begin(), end());
  }

  /*!
   * Equivalent to
   * \code
   * operator[](size() - 1 - I)
   * \endcode
   * \param I index from the back to retrieve, I=0
   *          corrseponds to the back of the array.
   */
  reference
  back(size_type I) const
  {
    ASTRALassert(I < size());
    return (*this)[size() - 1 - I];
  }

  /*!
   * STL compliant function.
   */
  reference
  back(void) const
  {
    ASTRALassert(!empty());
    return (*this)[size() - 1];
  }

  /*!
   * STL compliant function.
   */
  reference
  front(void) const
  {
    ASTRALassert(!empty());
    return (*this)[0];
  }

  /*!
   * Returns a sub-array
   * \param pos position of returned sub-array to start
   * \param length length of sub array to return
   */
  c_array
  sub_array(size_type pos, size_type length) const
  {
    ASTRALassert(length == 0 || pos + length <= m_size);
    return c_array(m_ptr + pos, length);
  }

  /*!
   * Returns a sub-array, equivalent to
   * \code
   * sub_array(pos, size() - pos)
   * \endcode
   * \param pos position of returned sub-array to start
   */
  c_array
  sub_array(size_type pos) const
  {
    ASTRALassert(pos <= m_size);
    return c_array(m_ptr + pos, m_size - pos);
  }

  /*!
   * Returns a sub-array, equivalent to
   * \code
   * sub_array(R.m_begin, R.m_end - R.m_begin)
   * \endcode
   * \tparam I type convertible to size_type
   * \param R range of returned sub-array
   */
  template<typename I>
  c_array
  sub_array(range_type<I> R) const
  {
    ASTRALassert(R.m_end >= R.m_begin);
    return sub_array(R.m_begin, R.m_end - R.m_begin);
  }

  /*!
   * Returns true if this c_array and another c_array overlap
   * in memory.
   * \param obj c_array to test against
   */
  bool
  overlapping_memory(c_array<const T> obj) const
  {
    std::uintptr_t this_begin, this_end;
    std::uintptr_t obj_begin, obj_end;

    this_begin = reinterpret_cast<std::uintptr_t>(this->c_ptr());
    this_end = reinterpret_cast<std::uintptr_t>(this->end_c_ptr());

    obj_begin = reinterpret_cast<std::uintptr_t>(obj.c_ptr());
    obj_end = reinterpret_cast<std::uintptr_t>(obj.end_c_ptr());

    return std::max(this_begin, obj_begin) < std::min(this_end, obj_end);
  }

  /*!
   * Provided as a conveniance, equivalent to
   * \code
   * *this = this->sub_array(0, size() - 1);
   * \endcode
   * It is an error to call this when size() is 0.
   */
  void
  pop_back(void)
  {
    ASTRALassert(m_size > 0);
    --m_size;
  }

  /*!
   * Provided as a conveniance, equivalent to
   * \code
   * *this = this->sub_array(1, size() - 1);
   * \endcode
   * It is an error to call this when size() is 0.
   */
  void
  pop_front(void)
  {
    ASTRALassert(m_size > 0);
    --m_size;
    ++m_ptr;
  }

  /*!
   * Returns true if and only if the passed
   * the c_array references exactly the same
   * data as this c_array.
   * \param rhs c_array to which to compare
   */
  template<typename U>
  bool
  same_data(const c_array<U> &rhs) const
  {
    return m_ptr == rhs.m_ptr
      && m_size * sizeof(T) == rhs.m_size * sizeof(U);
  }

private:
  template<typename>
  friend class c_array;

  size_type m_size;
  T *m_ptr;
};

/*!
 * Convert an array of 32-bit floating point values
 * to an array of 16-bit half values
 * \param src fp32 value to convert
 * \param dst location to which to write fp16 values
 */
void
convert_to_fp16(c_array<const float> src, c_array<uint16_t> dst);

/*!
 * Convert an array of 16-bit floating point values
 * to an array of 32-bit float values
 * \param src fp16 value to convert
 * \param dst location to which to write fp32 values
 */
void
convert_to_fp32(c_array<const uint16_t> src, c_array<float> dst);

/*!
 * Conveniance function to pack a single astral::vec2
 * into an fp16-pair returned as a uint32_t.
 * \param src float-pair to pack as an fp16-pair.
 */
inline
uint32_t
pack_as_fp16(vec2 src)
{
  uint32_t return_value;
  uint16_t *ptr(reinterpret_cast<uint16_t*>(&return_value));
  convert_to_fp16(src, c_array<uint16_t>(ptr, 2));
  return return_value;
}

/*!
 * Conveniance function to unpack a uint32_t
 * that holds an fp16-pair into two fp32 values
 * \param src uint32_t holding the fp16 pair
 * \param v0 location to which to unpack the first fp16
 * \param v1 location to which to unpack the second fp16
 */
inline
void
unpack_fp16(uint32_t src, float *v0, float *v1)
{
  const uint16_t *p;
  vec2 return_value;

  p = reinterpret_cast<const uint16_t*>(&src);
  convert_to_fp32(c_array<const uint16_t>(p, 2), return_value);
  *v0 = return_value.x();
  *v1 = return_value.y();
}

/*!
 * Provided as a conveniance, equivalent to
 * \code
 *  pack_as_fp16(vec2(x, y));
 * \endcode
 */
inline
uint32_t
pack_as_fp16(float x, float y)
{
  return pack_as_fp16(vec2(x, y));
}

/*!
 * Generate a astral::c_array<T> from an std::vector<T>.
 * The c_array is invalid to use once the std::vector
 * is resized or goes out of scope.
 * \param p std::vector that backs the storage referenced
 *          by the returned astral::c_array<T>.
 */
template<typename T>
c_array<T>
make_c_array(std::vector<T> &p)
{
  if (p.empty())
    {
      return c_array<T>();
    }
  else
    {
      return c_array<T>(&p[0], p.size());
    }
}

/*!
 * Generate a astral::c_array<T> from an std::vector<T>.
 * The c_array is invalid to use once the std::vector
 * is resized or goes out of scope.
 * \param p std::vector that backs the storage referenced
 *          by the returned astral::c_array<T>.
 */
template<typename T>
c_array<const T>
make_c_array(const std::vector<T> &p)
{
  if (p.empty())
    {
      return c_array<const T>();
    }
  else
    {
      return c_array<const T>(&p[0], p.size());
    }
}

/*!
 * Generate a astral::c_array<const T> from an std::vector<T>.
 * The c_array is invalid to use once the std::vector
 * is resized or goes out of scope.
 * \param p std::vector that backs the storage referenced
 *          by the returned astral::c_array<const T>.
 */
template<typename T>
c_array<const T>
make_const_c_array(std::vector<T> &p)
{
  if (p.empty())
    {
      return c_array<const T>();
    }
  else
    {
      return c_array<const T>(&p[0], p.size());
    }
}

/*!
 * Generate a astral::c_array<const T> from an std::vector<T>.
 * The c_array is invalid to use once the std::vector
 * is resized or goes out of scope.
 * \param p std::vector that backs the storage referenced
 *          by the returned astral::c_array<const T>.
 */
template<typename T>
c_array<const T>
make_const_c_array(const std::vector<T> &p)
{
  if (p.empty())
    {
      return c_array<const T>();
    }
  else
    {
      return c_array<const T>(&p[0], p.size());
    }
}

/*!
 * Convert a astral::c_array<const T> to a astral::c_array<T>,
 * i.e. cast away the const-ness.
 */
template<typename T>
c_array<T>
const_cast_c_array(c_array<const T> p)
{
  T *q;
  q = const_cast<T*>(p.c_ptr());
  return c_array<T>(q, p.size());
}
/*! @} */

} //namespace

#endif

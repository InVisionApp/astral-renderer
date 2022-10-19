/*!
 * \file util.hpp
 * \brief file util.hpp
 *
 * Adapted from: WRATHUtil.hpp and type_tag.hpp of WRATH:
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


#ifndef ASTRAL_UTIL_HPP
#define ASTRAL_UTIL_HPP

#include <type_traits>
#include <stdint.h>
#include <stddef.h>

/*!\addtogroup Utility
 * @{
 */

/*!
 * Macro to round up an uint32_t to a multiple or 4
 */
#define ASTRAL_ROUND_UP_MULTIPLE_OF4(X) (((X) + 3u) & ~3u)

/*!
 * Macro to return how many blocks of size 4 to contain
 * a given size, i.e. the smallest integer N so that
 * 4 * N >= X where X is an uint32_t.
 */
#define ASTRAL_NUMBER_BLOCK4_NEEDED(X) (ASTRAL_ROUND_UP_MULTIPLE_OF4(X) >> 2u)

/*!\def ASTRAL_MAX_VALUE_FROM_NUM_BITS
 * Macro that gives the maximum value that can be
 * held with a given number of bits. Caveat:
 * if X is 32 (or higher), bad things may happen
 * from overflow.
 * \param X number bits
 */
#define ASTRAL_MAX_VALUE_FROM_NUM_BITS(X) ( (uint32_t(1) << uint32_t(X)) - uint32_t(1) )

/*!\def ASTRAL_MASK
 * Macro that generates a 32-bit mask from number
 * bits and location of bit0 to use
 * \param BIT0 first bit of mask
 * \param NUMBITS nuber bits of mask
 */
#define ASTRAL_MASK(BIT0, NUMBITS) (ASTRAL_MAX_VALUE_FROM_NUM_BITS(NUMBITS) << uint32_t(BIT0))

/*!\def ASTRAL_BIT_MASK
 * Macro that generates a 32-bit mask from a bit ID
 */
#define ASTRAL_BIT_MASK(BIT)  (1u << uint32_t(BIT))

/*!\def ASTRAL_MAX_VALUE_FROM_NUM_BITS_U64
 * Macro that gives the maximum value that can be
 * held with a given number of bits, returning an
 * unsigned 64-bit integer.
 * \param X number bits
 */
#define ASTRAL_MAX_VALUE_FROM_NUM_BITS_U64(X) ( (uint64_t(1) << uint64_t(X)) - uint64_t(1) )

/*!\def ASTRAL_MASK_U64
 * Macro that generates a 64-bit mask from number
 * bits and location of bit0 to use
 * \param BIT0 first bit of mask
 * \param NUMBITS nuber bits of mask
 */
#define ASTRAL_MASK_U64(BIT0, NUMBITS) (ASTRAL_MAX_VALUE_FROM_NUM_BITS_U64(NUMBITS) << uint64_t(BIT0))

/*!\def ASTRAL_BIT_MASK_U64
 * Macro that generates a 64-bit mask from a bit ID
 */
#define ASTRAL_BIT_MASK_U64(BIT)  (uint64_t(1u) << uint64_t(BIT))

/*!\def ASTRALunused
 * Macro to stop the compiler from reporting
 * an argument as unused. Typically used on
 * those arguments used in ASTRALassert invocation
 * but otherwise unused.
 * \param X expression of which to ignore the value
 */
#define ASTRALunused(X) do { (void)(X); } while(0)

/*!\def ASTRALassert
 * If ASTRAL_DEBUG is defined, checks if the statement
 * is true and if it is not true prints to std::cerr and
 * then aborts. If ASTRAL_DEBUG is not defined, then
 * macro is empty (and thus the condition is not evaluated).
 * \param X condition to check
 */
#ifdef ASTRAL_DEBUG
#define ASTRALassert(X) do {                                            \
    if (!(X)) {                                                         \
      astral::assert_fail("Assertion '" #X "' failed", __FILE__, __LINE__); \
    } } while(0)
#else
#define ASTRALassert(X)
#endif

/*!\def ASTRALhard_assert
 * ALWAYS checks if condition is true, regardless if ASTRAL_DEBUG is or
 * is not defined. Use this sparingly for the sake of performance.
 */
#define ASTRALhard_assert(X) do {                                       \
    if (!(X)) {                                                         \
      astral::assert_fail("Hard Assertion '" #X "' failed", __FILE__, __LINE__); \
    } } while(0)

/*!\def ASTRALmessaged_assert
 * If ASTRAL_DEBUG is defined, checks if the statement
 * is true and if it is not true prints to std::cerr and
 * then aborts. If ASTRAL_DEBUG is not defined, then
 * macro is empty (and thus the condition is not evaluated).
 * \param X condition to check
 * \param Y string to print if condition is false
 */
#ifdef ASTRAL_DEBUG
#define ASTRALmessaged_assert(X, Y) do {                    \
    if (!(X)) {                                             \
      astral::assert_fail(Y, __FILE__, __LINE__);           \
    } } while(0)
#else
#define ASTRALmessaged_assert(X, Y)
#endif

/*!\ref ASTRALfailure
 * If issued, abort the program with the given error message
 */
#define ASTRALfailure(X) do {                   \
  astral::assert_fail(X, __FILE__, __LINE__);   \
  } while(0)

/*!\def ASTRALstatic_assert
 * Conveniance for using static_assert where message
 * is the condition stringified.
 */
#define ASTRALstatic_assert(X) static_assert(X, #X)

/*! @} */

///@cond
namespace astral
{
  /*!
   * Private function used by macro ASTRALassert, do NOT call.
   */
  void
  assert_fail(const char *str, const char *file, int line);
}
///@endcond

namespace astral
{
/*!\addtogroup Utility
 * @{
 */

  /*!
   * \brief
   * Conveniant typedef for C-style strings.
   */
  typedef const char *c_string;

  /*!
   * \brief
   * Enumeration for simple return codes for functions
   * for success or failure.
   */
  enum return_code:uint32_t
    {
      /*!
       * Routine failed
       */
      routine_fail,

      /*!
       * Routine suceeded
       */
      routine_success
    };

  /*!
   * \brief
   * Conveniance union for aliasing int32_t, uint32_t and float values
   */
  union generic_data
  {
    /*!
     * The field to access when the value is an int32_t
     */
    int32_t i;

    /*!
     * The field to access when the value is an uint32_t
     */
    uint32_t u;

    /*!
     * The field to access when the value is a float
     */
    float f;
  };

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  template<typename T>
  inline
  const T&
  t_min(const T &a, const T &b)
  {
    return (a < b) ? a : b;
  }

  /*!
   * Conveniance overload avoiding to rely on std::
   */
  template<typename T>
  inline
  const T&
  t_max(const T &a, const T &b)
  {
    return (a < b) ? b : a;
  }

  /*!
   * Conveniance overload avoiding giving clamp
   */
  template<typename T>
  inline
  const T&
  t_clamp(const T &x, const T &min_value, const T &max_value)
  {
    return t_max(min_value, t_min(x, max_value));
  }

  /*!
   * Return the sign of a value, an input of 0 returns 1.0.
   */
  template<typename T>
  inline
  T
  t_sign(const T &a)
  {
    return (a < T(0)) ? T(-1) : T(1);
  }

  /*!
   * Return the sign of a value, an input of 0 returns 1.0.
   */
  template<typename T>
  inline
  T
  t_sign_prefer_positive(const T &a)
  {
    return (a < T(0)) ? T(-1) : T(1);
  }

  /*!
   * Return the sign of a value, an input of 0 returns -1.0.
   */
  template<typename T>
  inline
  T
  t_sign_prefer_negative(const T &a)
  {
    return (a > T(0)) ? T(1) : T(-11);
  }

  /*!
   * Returns the floor of the log2 of an unsinged integer,
   * i.e. the value K so that 2^K <= x < 2^{K+1}
   */
  uint32_t
  uint32_log2_floor(uint32_t v);

  /*!
   * Returns the ceiling of the log2 of an unsinged integer,
   * i.e. the value K so that 2^K < x <= 2^K
   */
  uint32_t
  uint32_log2_ceiling(uint32_t v);

  /*!
   * Returns the floor of the log2 of an unsinged integer,
   * i.e. the value K so that 2^K <= x < 2^{K+1}
   */
  uint64_t
  uint64_log2_floor(uint64_t v);

  /*!
   * Returns the ceiling of the log2 of an unsinged integer,
   * i.e. the value K so that 2^K < x <= 2^K
   */
  uint64_t
  uint64_log2_ceiling(uint64_t v);

  /*!
   * Returns the number of bits required to hold a 32-bit
   * unsigned integer value.
   */
  uint32_t
  number_bits_required(uint32_t v);

  /*!
   * Returns the number of bits required to hold a 32-bit
   * unsigned integer value.
   */
  uint64_t
  uint64_number_bits_required(uint64_t v);

  /*!
   * Returns true if a uint32_t is
   * an exact non-zero power of 2.
   * \param v uint32_t to query
   */
  inline
  bool
  uint32_is_power_of_2(uint32_t v)
  {
    return v && !(v & (v - uint32_t(1u)));
  }

  /*!
   * Returns true if a uint64_t is
   * an exact non-zero power of 2.
   * \param v uint64_t to query
   */
  inline
  bool
  uint64_is_power_of_2(uint64_t v)
  {
    return v && !(v & (v - uint64_t(1u)));
  }

  /*!
   * Given v > 0, compute N so that N is a power
   * of 2 and so that N / 2 < v <= N. When v is 0,
   * returns 0.
   */
  inline
  uint32_t
  next_power_of_2(uint32_t v)
  {
    /* taken from Bithacks */
    --v;
    v |= v >> 1u;
    v |= v >> 2u;
    v |= v >> 4u;
    v |= v >> 8u;
    v |= v >> 16u;
    ++v;

    return v;
  }

  /*!
   * Given v > 0, compute N so that N is a power
   * of 2 and so that N / 2 < v <= N. When v is 0,
   * returns 0.
   */
  inline
  uint64_t
  uint64_next_power_of_2(uint64_t v)
  {
    /* taken from Bithacks */
    --v;
    v |= v >> 1u;
    v |= v >> 2u;
    v |= v >> 4u;
    v |= v >> 8u;
    v |= v >> 16u;
    v |= v >> 32u;
    ++v;

    return v;
  }

  /*!
   * Given if a bit should be up or down returns
   * an input value with that bit made to be up
   * or down.
   * \param input_value value to return with the named bit(s) changed
   * \param to_apply if true, return value has bits made up, otherwise has bits down
   * \param bitfield_value bits to make up or down as according to to_apply
   */
  inline
  uint32_t
  apply_bit_flag(uint32_t input_value, bool to_apply,
                 uint32_t bitfield_value)
  {
    return to_apply ?
      input_value | bitfield_value:
      input_value & (~bitfield_value);
  }

  /*!
   * Given if a bit should be up or down returns
   * an input value with that bit made to be up
   * or down.
   * \param input_value value to return with the named bit(s) changed
   * \param to_apply if true, return value has bits made up, otherwise has bits down
   * \param bitfield_value bits to make up or down as according to to_apply
   */
  inline
  uint64_t
  uint64_apply_bit_flag(uint64_t input_value, bool to_apply,
                        uint64_t bitfield_value)
  {
    return to_apply ?
      input_value | bitfield_value:
      input_value & (~bitfield_value);
  }

  /*!
   * Pack the lowest N bits of a value at a bit.
   * \param bit0 bit location of return value at which to pack
   * \param num_bits number of bits from value to pack
   * \param value value to pack
   */
  inline
  uint32_t
  pack_bits(uint32_t bit0, uint32_t num_bits, uint32_t value)
  {
    uint32_t mask;
    mask = (1u << num_bits) - 1u;
    ASTRALassert(bit0 + num_bits <= 32u);
    ASTRALassert(value <= mask);
    return (value & mask) << bit0;
  }

  /*!
   * Pack the lowest N bits of a value at a bit.
   * \param bit0 bit location of return value at which to pack
   * \param num_bits number of bits from value to pack
   * \param value value to pack
   */
  inline
  uint64_t
  uint64_pack_bits(uint64_t bit0, uint64_t num_bits, uint64_t value)
  {
    uint64_t mask;
    mask = (uint64_t(1u) << num_bits) - uint64_t(1u);
    ASTRALassert(bit0 + num_bits <= 64u);
    ASTRALassert(value <= mask);
    return (value & mask) << bit0;
  }

  /*!
   * Unpack N bits from a bit location.
   * \param bit0 starting bit from which to unpack
   * \param num_bits number bits to unpack
   * \param value value from which to unpack
   */
  inline
  uint32_t
  unpack_bits(uint32_t bit0, uint32_t num_bits, uint32_t value)
  {
    ASTRALassert(bit0 + num_bits <= 32u);

    uint32_t mask;
    mask = (uint32_t(1u) << num_bits) - uint32_t(1u);
    return (value >> bit0) & mask;
  }

  /*!
   * Unpack N bits from a bit location.
   * \param bit0 starting bit from which to unpack
   * \param num_bits number bits to unpack
   * \param value value from which to unpack
   */
  inline
  uint64_t
  uint64_unpack_bits(uint64_t bit0, uint64_t num_bits, uint64_t value)
  {
    ASTRALassert(bit0 + num_bits <= 64u);

    uint64_t mask;
    mask = (uint64_t(1u) << num_bits) - uint64_t(1u);
    return (value >> bit0) & mask;
  }

  /*!
   * Pack a pair of uint32_t values each of which
   * is no more than 0xFFFF into a single uint32_t.
   * \param v0 value to pack into the low 16 bits
   * \param v1 value to pack into the hight 16 bits
   */
  inline
  uint32_t
  pack_pair(uint32_t v0, uint32_t v1)
  {
    ASTRALassert(v0 <= 0xFFFFu);
    ASTRALassert(v1 <= 0xFFFFu);

    return v0 | (v1 << 16u);
  }

  /*!
   * Unpack a pair of uint32_t values each of which
   * is no more than 0xFFFF from a single uint32_t
   * \param v value from which to unpack
   * \param v0 detination to which to write the low 16 bits
   * \param v1 detination to which to write the high 16 bits
   */
  inline
  void
  unpack_pair(uint32_t v, uint32_t *v0, uint32_t *v1)
  {
    *v0 = v & 0xFFFFu;
    *v1 = v >> 16u;
  }

  /*!
   * \brief
   * A class reprenting the STL range
   * [m_begin, m_end).
   */
  template<typename T>
  class range_type
  {
  public:
    /*!
     * Typedef to identify template argument type
     */
    typedef T type;

    /*!
     * Iterator to first element
     */
    type m_begin;

    /*!
     * iterator to one past the last element
     */
    type m_end;

    /*!
     * Ctor.
     * \param b value with which to initialize m_begin
     * \param e value with which to initialize m_end
     */
    range_type(T b, T e):
      m_begin(b),
      m_end(e)
    {}

    /*!
     * Empty ctor, m_begin and m_end are uninitialized.
     */
    range_type(void)
    {}

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * m_end - m_begin
     * \endcode
     */
    template<typename W = T>
    W
    difference(void) const
    {
      return m_end - m_begin;
    }

    /*!
     * Equality operator
     * \param rhs right hand of operand
     */
    bool
    operator==(const range_type &rhs) const
    {
      return m_begin == rhs.m_begin
        && m_end == rhs.m_end;
    }

    /*!
     * Inequality operator
     * \param rhs right hand of operand
     */
    bool
    operator!=(const range_type &rhs) const
    {
      return !operator==(rhs);
    }

    /*!
     * Increment both range_type::m_begin and range_type::m_end.
     * \param v value by which to increment
     */
    template<typename W>
    void
    operator+=(const W &v)
    {
      m_begin += v;
      m_end += v;
    }

    /*!
     * Decrement both range_type::m_begin and range_type::m_end.
     * \param v value by which to decrement
     */
    template<typename W>
    void
    operator-=(const W &v)
    {
      m_begin -= v;
      m_end -= v;
    }

    /*!
     * Abosrbs another range into this range; the type T must work
     * with astral::t_min() and astral::t_max().
     */
    void
    absorb(const range_type &R)
    {
      m_begin = t_min(R.m_begin, m_begin);
      m_end = t_max(R.m_end, m_end);
    }
  };

  /*!
   * For type T's which support comparison swap, makes
   * sure that the returned astal::range_type has that
   * range_type::m_begin < range_type::m_end
   */
  template<typename T>
  range_type<T>
  create_range(T a, T b)
  {
    if (a < b)
      {
        return range_type<T>(a, b);
      }
    else
      {
        return range_type<T>(b, a);
      }
  }

  /*!
   * \brief
   * Class for which copy ctor and assignment operator
   * are private functions.
   */
  class noncopyable
  {
  public:
    noncopyable(void)
    {}

  private:
    noncopyable(const noncopyable &obj) = delete;

    noncopyable(noncopyable &&obj) = delete;

    noncopyable&
    operator=(const noncopyable &rhs) = delete;

    noncopyable&
    operator=(noncopyable &&rhs) = delete;
  };

  /*!
   * Conveniance typedef that is present in std at C++14
   * or higher, but Astral only requires C++11.
   */
  template<bool B, class T = void >
  using enable_if_t = typename std::enable_if<B, T>::type;

  /*!
   * \brief
   * A template type-tag, can be helpful in performing
   * template driven function overloading
   */
  template<typename T>
  class type_tag
  {
  public:
    /*!
     * The type of the type_tag
     */
    typedef T type;
  };

  /*!
   * Conveniance class to define a pointer type
   * so that a user of the class won't need to
   * use C's mind-bending *'s in multiple places.
   */
  template<typename T>
  class pointer_type
  {
  public:
    /*!
     * Typedef for pointer
     */
    typedef T *type;
  };

  /*!
   * The main purpose of this typedef is to declare a
   * a pointer to a constant without using the mind
   * bending syntax from C with the extra *'s, especially
   * for the case of a const array of const pointers
   * with the class astral::c_array
   */
  template<typename T>
  using pointer = typename pointer_type<T>::type;

/*! @} */

}

#endif

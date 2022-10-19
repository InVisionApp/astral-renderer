/*!
 * \file demo_macros.hpp
 * \brief file demo_macros.hpp
 *
 * Copyright 2020 InvisionApp.
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

#ifndef ASTRAL_DEMO_MACROS_HPP
#define ASTRAL_DEMO_MACROS_HPP

#include <astral/util/c_array.hpp>
#include <astral/util/util.hpp>

/* Macro to be used on statically size array, i.e.
 * Type Array[N] to get the value N
 */
#define ARRAY_SIZE(X) (sizeof(X) / sizeof(X[0]))

#define MAKE_C_ARRAY(X, T) astral::c_array<T>(X, ARRAY_SIZE(X))

inline
uint32_t
bit_cast_float_to_uint(float f)
{
  astral::generic_data v;

  v.f = f;
  return v.u;
}

class print_float_and_bits1
{
public:
  explicit
  print_float_and_bits1(float f)
  {
    m_d.f = f;
  }

  friend
  std::ostream&
  operator<<(std::ostream &ostr, print_float_and_bits1 v)
  {
    ostr << v.m_d.f << "(0x" << std::hex << v.m_d.u << ")" << std::dec;
    return ostr;
  }

private:
  astral::generic_data m_d;
};

template<size_t N>
class print_float_and_bitsN
{
public:
  explicit
  print_float_and_bitsN(const astral::vecN<float, N> &f)
  {
    for (unsigned int i = 0; i < N; ++i)
      {
        m_d[i].f = f[i];
      }
  }

  friend
  std::ostream&
  operator<<(std::ostream &ostr, const print_float_and_bitsN &v)
  {
    ostr << "(" << v.m_d[0].f;
    for (unsigned int i = 1; i < N; ++i)
      {
        ostr << ", " << v.m_d[i].f;
      }
    ostr << ")(0x" << std::hex << v.m_d[0].u;
    for (unsigned int i = 1; i < N; ++i)
      {
        ostr << ", 0x" << std::hex<< v.m_d[i].u;
      }
    ostr << ")" << std::dec;

    return ostr;
  }

private:
  astral::vecN<astral::generic_data, N> m_d;
};

inline
print_float_and_bits1
print_float_and_bits(float f)
{
  return print_float_and_bits1(f);
}

template<size_t N>
print_float_and_bitsN<N>
print_float_and_bits(const astral::vecN<float, N> &f)
{
  return print_float_and_bitsN<N>(f);
}

#endif

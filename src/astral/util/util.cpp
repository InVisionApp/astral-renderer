/*!
 * \file util.cpp
 * \brief file util.cpp
 *
 * Adapted from: WRATHUtil.cpp of WRATH:
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
#include <astral/util/util.hpp>

#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <cstring>
#ifdef __linux__
#include <execinfo.h>
#include <unistd.h>
#include <cxxabi.h>
#endif

#include <astral/util/astral_memory.hpp>
#include <astral/util/util.hpp>
#include <astral/util/c_array.hpp>

#ifdef __linux__
std::string
demangled_function_name(const char *backtrace_string)
{
  if (!backtrace_string)
    {
      return "";
    }

  /* backtrace gives the symbol as follows:
   *  library_name(mangled_function_name+offset) [return_address]
   */
  const char *open_paren, *plus, *end;

  end = backtrace_string + std::strlen(backtrace_string);
  open_paren = std::find(backtrace_string, end, '(');
  if (open_paren == end)
    {
      return "";
    }
  ++open_paren;
  plus = std::find(open_paren, end, '+');
  if (plus == end)
    {
      return "";
    }

  char* demangle_c_string;
  int status;
  std::string tmp(open_paren, plus);

  demangle_c_string = abi::__cxa_demangle(tmp.c_str(),
                                          nullptr,
                                          nullptr,
                                          &status);
  if (demangle_c_string == nullptr)
    {
      return "";
    }

  std::string return_value(demangle_c_string);
  free(demangle_c_string);
  return return_value;
}
#endif


uint32_t
astral::
uint32_log2_floor(uint32_t v)
{
  uint32_t return_value(0);
  while(v >>= 1)
    {
      ++return_value;
    }
  return return_value;
}

uint32_t
astral::
uint32_log2_ceiling(uint32_t v)
{
  uint32_t return_value;

  return_value = uint32_log2_floor(v);
  if (!uint32_is_power_of_2(v))
    {
      ++return_value;
    }

  return return_value;
}

uint32_t
astral::
number_bits_required(uint32_t v)
{
  if (v == 0)
    {
      return 0;
    }

  uint32_t return_value(0);
  for(uint32_t mask = 1u; mask <= v; mask <<= 1u, ++return_value)
    {}

  return return_value;
}

uint64_t
astral::
uint64_log2_floor(uint64_t v)
{
  uint64_t return_value(0);
  while(v >>= 1)
    {
      ++return_value;
    }
  return return_value;
}

uint64_t
astral::
uint64_log2_ceiling(uint64_t v)
{
  uint64_t return_value;

  return_value = uint64_log2_floor(v);
  if (!uint64_is_power_of_2(v))
    {
      ++return_value;
    }

  return return_value;
}

uint64_t
astral::
uint64_number_bits_required(uint64_t v)
{
  if (v == 0)
    {
      return 0;
    }

  uint64_t return_value(0);
  for(uint64_t mask = 1u; mask <= v; mask <<= 1u, ++return_value)
    {}

  return return_value;
}

void
astral::
convert_to_fp16(c_array<const float> src, c_array<uint16_t> dst)
{
  c_array<const uint32_t> srcU;

  ASTRALassert(src.size() == dst.size());
  srcU = src.reinterpret_pointer<const uint32_t>();

  /* We will rely on simple bit-twidding to do the conversion,
   * if the number would end-up denormalized we will round it
   * down to zero.
   */
  for (unsigned int idx = 0, end_idx = src.size(); idx < end_idx; ++idx)
    {
      uint32_t u(srcU[idx]);

      /* leading bit in fp16 and fp32 is the sign bit */
      uint32_t sign = (u >> 31u) << 15u;

      /* fp32 8 bits after leading bit are exponents */
      uint32_t exponent = unpack_bits(23u, 8u, u);

      /* we only want the leading 10 bits on the mantissa */
      uint32_t mantissa = unpack_bits(0u, 23u, u) >> 13u;

      /* fp32 exponent bias is 127, fp16 bias is 15.
       * Thus, fp32 - 127 = fp16 - 15 which gives
       * fp32 = fp16 + 112
       *  - denormalized for fp16 happens at fp16 == 0 which is fp32 == 112
       *  - inf for fp16 happens at fp16 == 15 whic is fp32 == 143
       */
      if (exponent >= 143u)
        {
          /* to big for fp16, realize as inf */
          dst[idx] = sign | pack_bits(10u, 5u, 31u) | sign;
        }
      else if (exponent <= 112u)
        {
          /* denormalized in fp16, make it zero. */
          dst[idx] = sign;
        }
      else
        {
          ASTRALassert(exponent >= 112u);

          //bias exponent from fp32 to fp16
          exponent -= (127u - 15u);
          ASTRALassert(exponent <= 31u);

          // shift exponent to location in fp16
          exponent = exponent << 10u;

          //manitssa is already shifted
          dst[idx] = sign | exponent | mantissa;
        }
    }
}

void
astral::
convert_to_fp32(c_array<const uint16_t> src, c_array<float> dst)
{
  c_array<uint32_t> dstU;

  ASTRALassert(src.size() == dst.size());
  dstU = dst.reinterpret_pointer<uint32_t>();
  for (unsigned int idx = 0, end_idx = src.size(); idx < end_idx; ++idx)
    {
      uint32_t u(src[idx]);
      uint32_t sign = (u & 0x8000u) << 16u;
      uint32_t exponent = (u & 0x7C00u) >> 10u;
      uint32_t mantissa = u & 0x03FFu;

      if (exponent == 31u)
        {
          dstU[idx] = sign | 0x7F800000u | mantissa;
        }
      else if (exponent == 0u)
        {
          /* denormalized-number, thus the number stored the value
           * in the mantissa / 2^24
           */
          float v;

          v = float(mantissa) * float(5.96046448e-8);
          dst[idx] = (sign == 0u) ? v : -v;
        }
      else
        {
          //bias exponent from fp16 to fp32
          exponent += (127u - 15u);

          // shift exponent to fp32
          exponent = exponent << 23u;

          //shift matissa from fp16's 10-bits to fp32's 23-bits
          mantissa = mantissa << 13u;

          dstU[idx] = sign | exponent | mantissa;
        }
    }
}

void
astral::
assert_fail(c_string str, c_string file, int line)
{
  std::cerr << "[" << file << "," << line << "]: " << str << "\n";

  #ifdef __linux__
    {
      #define STACK_MAX_BACKTRACE_SIZE 30
      void *backtrace_data[STACK_MAX_BACKTRACE_SIZE];
      char **backtrace_strings;
      size_t backtrace_size;

      std::cerr << "Backtrace:\n" << std::flush;
      backtrace_size = backtrace(backtrace_data, STACK_MAX_BACKTRACE_SIZE);
      backtrace_strings = backtrace_symbols(backtrace_data, backtrace_size);
      for (size_t i = 0; i < backtrace_size; ++i)
        {
          std::cerr << "\t" << backtrace_strings[i]
                    << "{" << demangled_function_name(backtrace_strings[i])
                    << "}\n";
        }
      free(backtrace_strings);
    }
  #endif

  std::abort();
}

/*!
 * \file unpack_source_generator.cpp
 * \brief file unpack_source_generator.cpp
 *
 * Copyright 2019 by Intel.
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

#include <vector>
#include <string>
#include <sstream>

#include <astral/util/gl/unpack_source_generator.hpp>

class astral::gl::UnpackSourceGenerator::UnpackElement
{
public:
  UnpackElement(void):
    m_type(padding_type)
  {}

  UnpackElement(c_string name,
                UnpackSourceGenerator::type_t tp,
                enum UnpackSourceGenerator::cast_t cast):
    m_name(name),
    m_type(tp),
    m_cast(cast),
    m_bit0(-1),
    m_num_bits(-1)
  {}

  UnpackElement(c_string name,
                unsigned int bit0, unsigned int num_bits,
                type_t tp,
                enum cast_t cast):
    m_name(name),
    m_type(tp),
    m_cast(cast),
    m_bit0(bit0),
    m_num_bits(num_bits)
  {}

  bool
  is_bitfield(void) const
  {
    return m_bit0 >= 0;
  }

  std::string m_name;
  type_t m_type;
  enum cast_t m_cast;
  int m_bit0, m_num_bits;
};

////////////////////////////////////////
// UnpackSourceGenerator methods
astral::gl::UnpackSourceGenerator::
UnpackSourceGenerator(c_string name, unsigned int stride):
  m_struct(name),
  m_stride(stride)
{
}

astral::gl::UnpackSourceGenerator::
~UnpackSourceGenerator()
{
}

astral::gl::UnpackSourceGenerator&
astral::gl::UnpackSourceGenerator::
set(unsigned int offset, c_string field_name, type_t type, enum cast_t cast)
{
  if (offset >= m_elements.size())
    {
      m_elements.resize(offset + 1);
    }
  m_elements[offset].push_back(UnpackElement(field_name, type, cast));
  return *this;
}

astral::gl::UnpackSourceGenerator&
astral::gl::UnpackSourceGenerator::
set(unsigned int offset,
    unsigned int bit0, unsigned int num_bits,
    c_string field_name, type_t type, enum cast_t cast)
{
  if (offset >= m_elements.size())
    {
      m_elements.resize(offset + 1);
    }
  m_elements[offset].push_back(UnpackElement(field_name, bit0, num_bits, type, cast));
  return *this;
}

const astral::gl::UnpackSourceGenerator&
astral::gl::UnpackSourceGenerator::
stream_unpack_function(ShaderSource &dst,
                       c_string function_name,
                       c_string extract_macro) const
{
  unsigned int number_blocks;
  std::ostringstream str;
  const astral::c_string swizzles[] =
    {
      ".x",
      ".xy",
      ".xyz",
      ".xyzw"
    };
  const astral::c_string utemp_components[] =
    {
      "utemp.x",
      "utemp.y",
      "utemp.z",
      "utemp.w"
    };

  str << "void\n" << function_name << "(in uint location, out "
      << m_struct << " out_value)\n"
      << "{\n"
      << "\tuvec4 utemp;\n"
      << "\tuint tempbits;\n"
      << "\tfloat ftemp;\n"
      << "\tlocation *= uint(" << m_stride << ");\n";

  number_blocks = ASTRAL_NUMBER_BLOCK4_NEEDED(m_elements.size());
  ASTRALassert(4 * number_blocks >= m_elements.size());

  for(unsigned int b = 0, i = 0; b < number_blocks; ++b)
    {
      unsigned int cmp, li;
      astral::c_string swizzle;

      li = m_elements.size() - i;
      cmp = astral::t_min(4u, li);
      ASTRALassert(cmp >= 1 && cmp <= 4);

      swizzle = swizzles[cmp - 1];
      str << "\tutemp" << swizzle << " = " << extract_macro
          << "(int(location) + " << b << ")" << swizzle << ";\n";

      /* perform bit cast to correct type. */
      for(unsigned int k = 0; k < 4 && i < m_elements.size(); ++k, ++i)
        {
          for (const auto &e : m_elements[i])
            {
              astral::c_string src;

              src = utemp_components[k];
              if (e.is_bitfield())
                {
                  str << "\ttempbits = ASTRAL_EXTRACT_BITS("
                      << e.m_bit0 << ", " << e.m_num_bits
                      << ", " << src << ");\n";
                  src = "tempbits";
                }

              if (e.m_cast == reinterpret_to_float_bits)
                {
                  str << "\tftemp = uintBitsToFloat(" << src << ");\n";
                  src = "ftemp";
                }

              switch(e.m_type)
                {
                case int_type:
                  str << "\tout_value"
                      << e.m_name << " = "
                      << "int(" << src << ");\n";
                  break;
                case uint_type:
                  str << "\tout_value"
                      << e.m_name << " = "
                      << "uint(" << src << ");\n";
                  break;
                case float_type:
                  str << "\tout_value"
                      << e.m_name << " = "
                      << "float(" << src << ");\n";
                  break;

                default:
                case padding_type:
                  str << "\t//Padding at component " << src << "\n";
                }
            }
        }
    }

  str << "}\n\n";
  dst.add_source(str.str().c_str(), gl::ShaderSource::from_string);

  return *this;
}

const astral::gl::UnpackSourceGenerator&
astral::gl::UnpackSourceGenerator::
stream_unpack_size_value(ShaderSource &dst,
                         c_string const_name) const
{
  std::ostringstream str;

  str << "const uint " << const_name << " = uint("
      << ASTRAL_NUMBER_BLOCK4_NEEDED(m_elements.size())
      << ");\n";
  dst.add_source(str.str().c_str(), gl::ShaderSource::from_string);

  return *this;
}

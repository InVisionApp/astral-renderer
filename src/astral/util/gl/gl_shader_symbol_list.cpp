/*!
 * \file gl_shader_symbol_list.cpp
 * \brief file gl_shader_symbol_list.cpp
 *
 * Copyright 2020 by Intel.
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

#include <astral/util/gl/gl_shader_symbol_list.hpp>

enum astral::gl::ShaderSymbolList::symbol_type_t
astral::gl::ShaderSymbolList::
symbol_type(enum ShaderVaryings::interpolator_type_t tp)
{
  static const symbol_type_t values[ShaderVaryings::interpolator_number_types] =
    {
      [ShaderVaryings::interpolator_smooth] = symbol_type_float,
      [ShaderVaryings::interpolator_flat] = symbol_type_float,
      [ShaderVaryings::interpolator_uint] = symbol_type_uint,
      [ShaderVaryings::interpolator_int] = symbol_type_int,
    };

  ASTRALassert(tp < ShaderVaryings::interpolator_number_types);
  return values[tp];
}

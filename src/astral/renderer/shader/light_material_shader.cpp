/*!
 * \file light_material_shader.cpp
 * \brief file light_material_shader.cpp
 *
 * Copyright 2020 by InvisionApp.
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

#include <astral/renderer/shader/light_material_shader.hpp>

namespace
{
  class InitedItemDataValueMapping
  {
  public:
    InitedItemDataValueMapping(void)
    {
      m_value
        .add(astral::ItemDataValueMapping::render_value_shadow_map,
             astral::ItemDataValueMapping::x_channel, 2)
        .add(astral::ItemDataValueMapping::render_value_transformation,
             astral::ItemDataValueMapping::y_channel, 2);
    }

    astral::ItemDataValueMapping m_value;
  };
}

const astral::ItemDataValueMapping&
astral::LightMaterialShader::
intrepreted_value_map(void)
{
  static InitedItemDataValueMapping R;
  return R.m_value;
}

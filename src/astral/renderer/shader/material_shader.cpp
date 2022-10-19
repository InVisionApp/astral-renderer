/*!
 * \file material_shader.cpp
 * \brief file material_shader.cpp
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

#include <astral/renderer/shader/material_shader.hpp>
#include <astral/renderer/render_engine.hpp>

static unsigned int material_shader_root_unique_id = 0;

astral::MaterialShader::
MaterialShader(RenderEngine &engine,
               unsigned int num_sub_shaders,
               const Properties &P):
  m_properties(P),
  m_ID(engine.allocate_material_id(detail::SubShaderCount(num_sub_shaders))),
  m_num_sub_shaders(num_sub_shaders),
  m_root_unique_id(material_shader_root_unique_id++)
{
  ASTRALassert(m_ID != 0u);
}

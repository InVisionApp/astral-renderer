/*!
 * \file item_shader_backend.cpp
 * \brief file item_shader_backend.cpp
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

#include <astral/renderer/shader/item_shader.hpp>
#include <astral/renderer/render_engine.hpp>

static unsigned int currentID = 0u;

astral::ItemShaderBackend::
ItemShaderBackend(RenderEngine &engine, unsigned int num_sub_shaders):
  m_begin_shaderID(engine.allocate_shader_id(detail::SubShaderCount(num_sub_shaders))),
  m_num_sub_shaders(num_sub_shaders),
  m_uniqueID(currentID++)
{
}

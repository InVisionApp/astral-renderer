/*!
 * \file dynamic_rect_shader.cpp
 * \brief file dynamic_rect_shader.cpp
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

#include <astral/renderer/shader/dynamic_rect_shader.hpp>
#include <astral/renderer/render_engine.hpp>

astral::reference_counted_ptr<const astral::VertexData>
astral::DynamicRectShader::
create_rect(RenderEngine &engine)
{
  vecN<Vertex, 4> v;
  vecN<Index, 6> i(0, 1, 2, 0, 2, 3);

  v[0].m_data[0].f = 0.0f;
  v[0].m_data[1].f = 0.0f;

  v[1].m_data[0].f = 1.0f;
  v[1].m_data[1].f = 0.0f;

  v[2].m_data[0].f = 1.0f;
  v[2].m_data[1].f = 1.0f;

  v[3].m_data[0].f = 0.0f;
  v[3].m_data[1].f = 1.0f;

  return engine.vertex_data_allocator().create(v, i);
}

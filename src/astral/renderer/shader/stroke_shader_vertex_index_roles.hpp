/*!
 * \file stroke_shader_vertex_index_roles.hpp
 * \brief file stroke_shader_vertex_index_roles.hpp
 *
 * Copyright 2021 by InvisionApp.
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

#ifndef ASTRAL_STROKE_SHADER_VERTEX_INDEX_ROLES_HPP
#define ASTRAL_STROKE_SHADER_VERTEX_INDEX_ROLES_HPP

class astral::StrokeShader::VertexIndexRoles
{
public:
  c_array<const uint32_t> m_indices;
  c_array<const uint32_t> m_vertex_roles;

  static
  VertexIndexRoles
  lines(void);

  static
  VertexIndexRoles
  biarcs(void);

  static
  VertexIndexRoles
  joins(void);

  static
  VertexIndexRoles
  caps(void);

  static
  VertexIndexRoles
  roles(enum primitive_type_t tp);
};

#endif

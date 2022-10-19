/*!
 * \file material_shader_gl3.cpp
 * \brief file material_shader_gl3.cpp
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

#include <astral/renderer/gl3/material_shader_gl3.hpp>
#include "shader_implement_gl3.hpp"

astral::reference_counted_ptr<const astral::gl::MaterialShaderGL3>
astral::gl::MaterialShaderGL3::
create(RenderEngineGL3 &engine,
       const ShaderSource &vertex_src,
       const ShaderSource &fragment_src,
       const ShaderSymbolList &symbols,
       const Properties &properties,
       const DependencyList &dependencies,
       unsigned int number_sub_shaders)
{
  return ASTRALnew Implement(engine, vertex_src, fragment_src,
                             symbols, properties, dependencies, number_sub_shaders);
}

const astral::gl::ShaderSource&
astral::gl::MaterialShaderGL3::
vertex_src(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_src[detail::vertex_shader_stage];
}

const astral::gl::ShaderSource&
astral::gl::MaterialShaderGL3::
fragment_src(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_src[detail::fragment_shader_stage];
}

const astral::gl::ShaderSymbolList&
astral::gl::MaterialShaderGL3::
symbols(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_raw_symbols;
}

const astral::gl::MaterialShaderGL3::DependencyList&
astral::gl::MaterialShaderGL3::
dependencies(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_dependencies;
}

unsigned int
astral::gl::MaterialShaderGL3::
shader_builder_index(detail::ShaderIndexArgument) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_shader_builder_index;
}

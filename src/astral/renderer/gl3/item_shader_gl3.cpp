/*!
 * \file item_shader_gl3.cpp
 * \brief file item_shader_gl3.cpp
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

#include <astral/renderer/gl3/item_shader_gl3.hpp>
#include <astral/renderer/gl3/render_engine_gl3.hpp>
#include <astral/renderer/render_enums.hpp>

#include "shader_implement_gl3.hpp"

astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3>
astral::gl::ItemShaderBackendGL3::
create(RenderEngineGL3 &engine,
       enum ItemShader::type_t type,
       const ShaderSource &vertex_src,
       const ShaderSource &fragment_src,
       const ShaderSymbolList &symbols,
       const DependencyList &dependencies,
       unsigned int number_sub_shaders)
{
  return ASTRALnew Implement(engine, type, vertex_src, fragment_src,
                             symbols, dependencies, number_sub_shaders);
}

enum astral::ItemShader::type_t
astral::gl::ItemShaderBackendGL3::
type(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_type;
}

const astral::gl::ShaderSource&
astral::gl::ItemShaderBackendGL3::
vertex_src(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_src[detail::vertex_shader_stage];
}

const astral::gl::ShaderSource&
astral::gl::ItemShaderBackendGL3::
fragment_src(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_src[detail::fragment_shader_stage];
}

const astral::gl::ShaderSymbolList&
astral::gl::ItemShaderBackendGL3::
symbols(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_raw_symbols;
}

const astral::gl::ItemShaderBackendGL3::DependencyList&
astral::gl::ItemShaderBackendGL3::
dependencies(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_dependencies;
}

unsigned int
astral::gl::ItemShaderBackendGL3::
shader_builder_index(detail::ShaderIndexArgument) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_shader_builder_index;
}

astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3>
astral::gl::ItemShaderBackendGL3::
color_shader_from_mask_shader(void) const
{
  const Implement *p;

  ASTRALassert(type() == ItemShader::mask_item_shader);

  p = static_cast<const Implement*>(this);
  if (!p->m_color_shader_from_mask_shader)
    {
      p->m_color_shader_from_mask_shader = create(p->m_engine,
                                                  ItemShader::color_item_shader,
                                                  ShaderSource()
                                                  .add_source(p->m_src[detail::vertex_shader_stage])
                                                  .add_source("astral_color_item_shader_from_mask_shader.vert.glsl.resource_string", ShaderSource::from_resource),
                                                  ShaderSource()
                                                  .add_source(p->m_src[detail::fragment_shader_stage])
                                                  .add_source("astral_color_item_shader_from_mask_shader.frag.glsl.resource_string", ShaderSource::from_resource),
                                                  p->m_raw_symbols,
                                                  p->m_dependencies,
                                                  num_sub_shaders());
    }

  return p->m_color_shader_from_mask_shader;
}

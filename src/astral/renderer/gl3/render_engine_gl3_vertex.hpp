/*!
 * \file render_engine_gl3_vertex.hpp
 * \brief file render_engine_gl3_vertex.hpp
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

#ifndef ASTRAL_RENDER_ENGINE_GL3_VERTEX_HPP
#define ASTRAL_RENDER_ENGINE_GL3_VERTEX_HPP

#include <astral/util/gl/astral_gl.hpp>
#include <astral/renderer/vertex_data.hpp>
#include <astral/renderer/gl3/render_engine_gl3.hpp>

#include "render_engine_gl3_implement.hpp"
#include "render_engine_gl3_static_data.hpp"

/*!
 * \brief
 * An astral::gl::RenderEngineGL3::Implement::VertexBacking implements
 * astral::VertexDataBacking for GL3; it maps directly to
 * a RenderEngineGL3::Implement::StaticDataBackingBase object
 */
class astral::gl::RenderEngineGL3::Implement::VertexBacking:
  public astral::VertexDataBacking
{
public:
  /*!
   * Ctor
   * \param num_vertices the _initial_ number of vertices in the store
   */
  explicit
  VertexBacking(reference_counted_ptr<StaticDataBackingBase> store):
    VertexDataBacking(store->size()),
    m_store(store)
  {
  }

  ~VertexBacking()
  {
  }

  astral_GLuint
  texture(void) const
  {
    return m_store->texture();
  }

  astral_GLenum
  binding_point(void) const
  {
    return m_store->binding_point();
  }

private:
  virtual
  unsigned int
  resize_vertices_implement(unsigned int new_size) override final
  {
    return m_store->resize(new_size);
  }

  virtual
  void
  set_vertices(c_array<const Vertex> verts, unsigned int start) override final
  {
    ASTRALassert(sizeof(Vertex) == sizeof(gvec4));
    m_store->set_data(start, verts.reinterpret_pointer<const gvec4>());
  }

  reference_counted_ptr<StaticDataBackingBase> m_store;
};

#endif

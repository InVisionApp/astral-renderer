/*!
 * \file renderer_virtual_buffer_proxy.hpp
 * \brief file renderer_virtual_buffer_proxy.hpp
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

#ifndef ASTRAL_RENDERER_VIRTUAL_BUFFER_PROXY_HPP
#define ASTRAL_RENDERER_VIRTUAL_BUFFER_PROXY_HPP

#include <astral/renderer/renderer.hpp>

#include "renderer_clip_geometry.hpp"

/* The class that is the backing for RenderSupportTypes::Proxy.
 * Its purpose is to specify a redering region and is also
 * used to construct VirtualBuffer objects
 */
class astral::RenderSupportTypes::Proxy::Backing
{
public:
  Backing(const Transformation &tr, const Renderer::Implement::ClipGeometryGroup &geom):
    m_transformation(tr),
    m_clip_geometry(geom)
  {}

  /* The value of transformation() of the VirtualBuffer
   * that created the proxy at the time of creation.
   */
  Transformation m_transformation;

  /* The clipping region defined by the Proxy. */
  Renderer::Implement::ClipGeometryGroup m_clip_geometry;
};

#endif

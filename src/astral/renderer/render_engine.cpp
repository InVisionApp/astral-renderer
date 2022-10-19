/*!
 * \file render_engine.cpp
 * \brief file render_engine.cpp
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

#include <astral/renderer/render_engine.hpp>
#include <astral/renderer/backend/render_backend.hpp>

////////////////////////////////
// astral::RenderEngine methods
astral::RenderEngine::
RenderEngine(const Properties &P,
             const reference_counted_ptr<ColorStopSequenceAtlasBacking> &colorstop_sequence_backing,
             const reference_counted_ptr<VertexDataBacking> &vertex_data_backing,
             const reference_counted_ptr<StaticDataBacking> &data_backing32,
             const reference_counted_ptr<StaticDataBacking> &data_backing16,
             const reference_counted_ptr<ImageAtlasIndexBacking> &image_index_backing,
             const reference_counted_ptr<ImageAtlasColorBacking> &image_color_backing,
             const reference_counted_ptr<ShadowMapAtlasBacking> &shadow_map_backing):
  m_properties(P),
  m_ID_count(0u),
  m_material_ID_count(1u) //note that is starts at 1, this is because MaterialShader::ID() is never 0.
{
  ASTRALassert(data_backing32->type() == StaticDataBacking::type32);
  ASTRALassert(data_backing16->type() == StaticDataBacking::type16);

  m_vertex_data_allocator = VertexDataAllocator::create(*vertex_data_backing);
  m_static_data_allocator32 = StaticDataAllocator32::create(*data_backing32);
  m_static_data_allocator16 = StaticDataAllocator16::create(*data_backing16);
  m_colorstop_sequence_atlas = ColorStopSequenceAtlas::create(colorstop_sequence_backing);
  m_image_atlas = ImageAtlas::create(*image_color_backing, *image_index_backing);
  m_shadow_map_atlas = ShadowMapAtlas::create(*shadow_map_backing);
}

astral::RenderEngine::
~RenderEngine()
{
}

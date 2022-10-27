/*!
 * \file renderer_virtual_buffer.cpp
 * \brief file renderer_virtual_buffer.cpp
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

#include <astral/util/ostream_utility.hpp>
#include "renderer_virtual_buffer.hpp"
#include "renderer_storage.hpp"
#include "renderer_workroom.hpp"

namespace
{
  astral::BoundingBox<int>
  restrict_rect_to_nonempty_tiles(const astral::Image &image,
                                  const astral::RectT<int> &in_out_rect)
  {
    ASTRALassert(!image.mip_chain().empty());
    ASTRALassert(image.mip_chain().front());

    const astral::ImageMipElement &im(*image.mip_chain().front());

    if (!im.has_white_or_empty_elements())
      {
        /* all tiles backed, no oppurtunity for making it smaller */
        return astral::BoundingBox<int>(in_out_rect);
      }

    astral::BoundingBox<int> tile_bb;
    for (unsigned int i = 0, endi = im.number_elements(astral::ImageMipElement::color_element); i < endi; ++i)
      {
        const int lod(0);
        astral::uvec2 id;
        astral::ivec2 min_pt, max_pt;

        id = im.element_tile_id(astral::ImageMipElement::color_element, i);
        for (unsigned int coord = 0; coord < 2; ++coord)
          {
            min_pt[coord] = astral::ImageAtlas::tile_start(id[coord], lod);
            max_pt[coord] = astral::ImageAtlas::tile_end(id[coord], lod);
          }
        tile_bb.union_point(min_pt);
        tile_bb.union_point(max_pt);
      }

    tile_bb.intersect_against(astral::BoundingBox<int>(in_out_rect));
    return tile_bb;
  }
}

///////////////////////////////////////////////////
// astral::Renderer::Implement::VirtualBuffer methods
astral::Renderer::VirtualBuffer::
VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
              const Transformation &initial_transformation,
              const Implement::ClipGeometryGroup &clip_geometry,
              enum Implement::DrawCommandList::render_type_t render_type,
              enum image_blit_processing_t blit_processing,
              enum colorspace_t colorspace,
              enum fill_rule_t stc_fill_rule,
              ImageCreationSpec image_create_spec):
  m_renderer(renderer),
  m_use_pixel_rect_tile_culling(renderer.m_default_use_pixel_rect_tile_culling),
  m_render_accuracy(renderer.m_default_render_accuracy),
  m_use_sub_ubers(true),
  m_transformation_stack(*renderer.m_storage->allocate_transformation_stack()),
  m_renderer_begin_cnt(renderer.m_begin_cnt),
  m_creation_tag(C),
  m_colorspace(colorspace),
  m_finish_issued(false),
  m_render_index(render_index),
  m_uses_this_buffer_list(nullptr),
  m_dependency_list(nullptr),
  m_remaining_dependencies(0u),
  m_users_that_completed_rendering(0u),
  m_command_list(nullptr),
  m_clip_geometry(clip_geometry),
  m_pause_snapshot_counter(0),
  m_start_z(0u),
  m_stc_fill_rule(stc_fill_rule),
  m_blit_rects(nullptr),
  m_image_create_spec(image_create_spec),
  m_images_with_mips(nullptr),
  m_last_mip_only(nullptr),
  m_uses_shadow_map(false)
{
  uvec2 sz(m_clip_geometry.bounding_geometry().image_size());

  m_transformation_stack.push_back(initial_transformation);
  if (sz.x() > 0 && sz.y() > 0)
    {
      m_clip_window = renderer.create_clip_window(m_clip_geometry.bounding_geometry().pixel_rect().as_rect().m_min_point,
                                                  m_clip_geometry.bounding_geometry().pixel_rect().as_rect().size());

      m_type = image_buffer;
      m_command_list = renderer.m_storage->allocate_command_list(render_type, blit_processing, m_clip_geometry.bounding_geometry().pixel_rect());
      m_uses_this_buffer_list = renderer.m_storage->allocate_buffer_list();
      m_dependency_list = renderer.m_storage->allocate_buffer_list();

      /* NOTE: create_backing_image() requires m_clip_geometry to be ready
       *       AND that m_command_list is not nullptr.
       */
      if (m_image_create_spec.m_create_immediately)
        {
          create_backing_image();
        }
    }
  else
    {
      m_type = degenerate_buffer;
    }
}

astral::Renderer::VirtualBuffer::
VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
              ivec2 image_size, enum Implement::DrawCommandList::render_type_t render_type,
              enum image_blit_processing_t blit_processing,
              enum colorspace_t colorspace,
              enum fill_rule_t stc_fill_rule,
              ImageCreationSpec image_create_spec):
  m_renderer(renderer),
  m_use_pixel_rect_tile_culling(renderer.m_default_use_pixel_rect_tile_culling),
  m_render_accuracy(renderer.m_default_render_accuracy),
  m_use_sub_ubers(true),
  m_transformation_stack(*renderer.m_storage->allocate_transformation_stack()),
  m_renderer_begin_cnt(renderer.m_begin_cnt),
  m_creation_tag(C),
  m_colorspace(colorspace),
  m_finish_issued(false),
  m_render_index(render_index),
  m_uses_this_buffer_list(nullptr),
  m_dependency_list(nullptr),
  m_remaining_dependencies(0u),
  m_users_that_completed_rendering(0u),
  m_command_list(nullptr),
  m_pause_snapshot_counter(0),
  m_start_z(0u),
  m_stc_fill_rule(stc_fill_rule),
  m_blit_rects(nullptr),
  m_image_create_spec(image_create_spec),
  m_images_with_mips(nullptr),
  m_last_mip_only(nullptr),
  m_uses_shadow_map(false)
{
  m_transformation_stack.push_back(Transformation());

  if (image_size.x() > 0 && image_size.y() > 0)
    {
      m_clip_geometry = Implement::ClipGeometryGroup(renderer.m_storage->create_clip(image_size, renderer, &m_clip_window));

      m_type = image_buffer;
      m_command_list = renderer.m_storage->allocate_command_list(render_type, blit_processing, m_clip_geometry.bounding_geometry().pixel_rect());
      m_uses_this_buffer_list = renderer.m_storage->allocate_buffer_list();
      m_dependency_list = renderer.m_storage->allocate_buffer_list();

      /* NOTE: create_backing_image() requires m_clip_geometry to be ready
       *       AND that m_command_list is not nullptr.
       */
      if (m_image_create_spec.m_create_immediately)
        {
          create_backing_image();
        }
    }
  else
    {
      m_type = degenerate_buffer;
      m_clip_geometry = Implement::ClipGeometryGroup(renderer.m_storage->create_clip(ivec2(0, 0)));
    }
}

astral::Renderer::VirtualBuffer::
VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
              uvec2 sz, enum colorspace_t colorspace,
              c_array<const uvec2> empty_tiles,
              c_array<const uvec2> fully_covered_tiles,
              c_array<const std::pair<uvec2, TileSource>> shared_tiles,
              c_array<const std::pair<uvec2, TileSourceImage>> image_shared_tiles):
  m_renderer(renderer),
  m_use_pixel_rect_tile_culling(renderer.m_default_use_pixel_rect_tile_culling),
  m_render_accuracy(renderer.m_default_render_accuracy),
  m_use_sub_ubers(true),
  m_transformation_stack(*renderer.m_storage->allocate_transformation_stack()),
  m_renderer_begin_cnt(renderer.m_begin_cnt),
  m_creation_tag(C),
  m_type(assembled_buffer),
  m_colorspace(colorspace),
  m_finish_issued(false),
  m_render_index(render_index),
  m_uses_this_buffer_list(renderer.m_storage->allocate_buffer_list()),
  m_dependency_list(renderer.m_storage->allocate_buffer_list()),
  m_remaining_dependencies(0u),
  m_users_that_completed_rendering(0u),
  m_command_list(nullptr),
  m_pause_snapshot_counter(0),
  m_start_z(0u),
  m_stc_fill_rule(number_fill_rule),
  m_blit_rects(nullptr),
  m_images_with_mips(nullptr),
  m_last_mip_only(nullptr),
  m_uses_shadow_map(false)
{
  ASTRALassert(sz != uvec2(0, 0));

  /* this VirtualBuffer depends on each of the VirtualBuffers
   * listed in shared_tiles[].second
   */
  ASTRALassert(m_renderer.m_workroom->m_shared_tiles.empty());
  for (const auto &p : shared_tiles)
    {
      VirtualBuffer &src(m_renderer.m_storage->virtual_buffer(p.second.m_src_render_index));
      ImageAtlas::TileElement E;

      ASTRALassert(src.m_image);
      ASTRALassert(src.m_image->mip_chain().size() == 1);

      E.m_src = src.m_image->mip_chain().front();
      E.m_tile = p.second.m_src_tile;
      m_renderer.m_workroom->m_shared_tiles.push_back(std::make_pair(p.first, E));

      /* mark that this buffer relies on the VirtualBuffer src */
      add_dependency(src);
    }

  for (const auto &p : image_shared_tiles)
    {
      ImageAtlas::TileElement E;

      ASTRALassert(p.second.m_src_image);
      ASTRALassert(!p.second.m_src_image->mip_chain().empty());
      ASTRALassert(p.second.m_src_image->mip_chain().front());

      E.m_src = p.second.m_src_image->mip_chain().front();
      E.m_tile = p.second.m_src_tile;
      m_renderer.m_workroom->m_shared_tiles.push_back(std::make_pair(p.first, E));

      add_dependency(p.second.m_src_image->ID());
    }

  vecN<reference_counted_ptr<const ImageMipElement>, 1> mip;

  mip[0] = m_renderer.m_engine->image_atlas().create_mip_element(sz, 1, empty_tiles, fully_covered_tiles,
                                                                 make_c_array(m_renderer.m_workroom->m_shared_tiles));
  m_renderer.m_workroom->m_shared_tiles.clear();

  #ifdef ASTRAL_DEBUG
    {
      /* all colored tiles of the created image should NOT be unique */
      for (unsigned int i = 0, endi = mip[0]->number_elements(ImageMipElement::color_element); i < endi; ++i)
        {
          ASTRALassert(mip[0]->color_tile_is_shared(i));
        }
    }
  #endif

  m_image = m_renderer.m_engine->image_atlas().create_rendered_image(detail::RenderedImageTag(render_index), mip, colorspace);
  m_image->default_use_prepadding(true);

  /* the ctor created the image; we need to set m_image_create_spec
   * so that fetch_image() operates correctly.
   */
  m_image_create_spec
    .create_immediately(true)
    .default_use_prepadding_true(true);
}

astral::Renderer::VirtualBuffer::
VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
              const astral::Image &image,
              vecN<range_type<unsigned int>, 2> tile_range,
              c_array<const uvec2> empty_tiles,
              c_array<const uvec2> fully_covered_tiles,
              c_array<const uvec2> shared_tiles):
  m_renderer(renderer),
  m_use_pixel_rect_tile_culling(renderer.m_default_use_pixel_rect_tile_culling),
  m_render_accuracy(renderer.m_default_render_accuracy),
  m_use_sub_ubers(true),
  m_transformation_stack(*renderer.m_storage->allocate_transformation_stack()),
  m_renderer_begin_cnt(renderer.m_begin_cnt),
  m_creation_tag(C),
  m_type(assembled_buffer),
  m_colorspace(image.colorspace()),
  m_finish_issued(false),
  m_render_index(render_index),
  m_uses_this_buffer_list(renderer.m_storage->allocate_buffer_list()),
  m_dependency_list(renderer.m_storage->allocate_buffer_list()),
  m_remaining_dependencies(0u),
  m_users_that_completed_rendering(0u),
  m_command_list(nullptr),
  m_pause_snapshot_counter(0),
  m_start_z(0u),
  m_stc_fill_rule(number_fill_rule),
  m_blit_rects(nullptr),
  m_images_with_mips(nullptr),
  m_last_mip_only(nullptr),
  m_uses_shadow_map(false)
{
  vecN<reference_counted_ptr<const ImageMipElement>, 1> mip;
  const ImageMipElement *mip_src;

  ASTRALassert(image.mip_chain().size() == 1);

  /* all source from the same Image, so that image is the only dependency */
  if (!shared_tiles.empty())
    {
      add_dependency(image.ID());
    }

  mip_src = image.mip_chain().front().get();
  mip[0] = mip_src->create_sub_mip(tile_range, empty_tiles, fully_covered_tiles, shared_tiles);

  #ifdef ASTRAL_DEBUG
    {
      /* all colored tiles of the created image should NOT be unique */
      for (unsigned int i = 0, endi = mip[0]->number_elements(ImageMipElement::color_element); i < endi; ++i)
        {
          ASTRALassert(mip[0]->color_tile_is_shared(i));
        }
    }
  #endif

  m_image = m_renderer.m_engine->image_atlas().create_rendered_image(detail::RenderedImageTag(render_index), mip, image.colorspace());
  m_image->default_use_prepadding(image.default_use_prepadding());

  /* the ctor created the image; we need to set m_image_create_spec
   * so that fetch_image() operates correctly.
   */
  m_image_create_spec
    .create_immediately(true)
    .default_use_prepadding_true(true);
}

astral::Renderer::VirtualBuffer::
VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
              VirtualBuffer &mip_chain, VirtualBuffer &mip_chain_tail):
  m_renderer(renderer),
  m_use_pixel_rect_tile_culling(mip_chain.m_use_pixel_rect_tile_culling),
  m_render_accuracy(renderer.m_default_render_accuracy),
  m_use_sub_ubers(true),
  m_transformation_stack(*renderer.m_storage->allocate_transformation_stack()),
  m_renderer_begin_cnt(renderer.m_begin_cnt),
  m_creation_tag(C),
  m_type(assembled_buffer),
  m_colorspace(mip_chain.colorspace()),
  m_finish_issued(false),
  m_render_index(render_index),
  m_uses_this_buffer_list(renderer.m_storage->allocate_buffer_list()),
  m_dependency_list(renderer.m_storage->allocate_buffer_list()),
  m_remaining_dependencies(0u),
  m_users_that_completed_rendering(0u),
  m_command_list(nullptr),
  m_pause_snapshot_counter(0),
  m_start_z(0u),
  m_stc_fill_rule(number_fill_rule),
  m_blit_rects(nullptr),
  m_images_with_mips(nullptr),
  m_last_mip_only(nullptr),
  m_uses_shadow_map(false)
{
  ASTRALassert(mip_chain.m_image);
  ASTRALassert(!mip_chain.m_image->mip_chain().empty());
  ASTRALassert(mip_chain_tail.m_image);
  ASTRALassert(mip_chain_tail.m_image->mip_chain().size() == 1u);
  ASTRALassert(mip_chain.colorspace() == mip_chain_tail.colorspace());

  Implement::WorkRoom &workroom(*m_renderer.m_workroom);

  ASTRALassert(workroom.m_mip_chain.empty());
  for (const auto &p : mip_chain.m_image->mip_chain())
    {
      workroom.m_mip_chain.push_back(p);
    }

  for (const auto &p : mip_chain_tail.m_image->mip_chain())
    {
      workroom.m_mip_chain.push_back(p);
    }
  ASTRALassert(workroom.m_mip_chain.size() == mip_chain.m_image->mip_chain().size() + mip_chain_tail.m_image->mip_chain().size());

  m_image = m_renderer.m_engine->image_atlas().create_rendered_image(detail::RenderedImageTag(render_index),
                                                                     make_c_array(workroom.m_mip_chain),
                                                                     mip_chain.m_image->colorspace());
  m_image->default_use_prepadding(mip_chain.m_image->default_use_prepadding());

  add_dependency(mip_chain);
  add_dependency(mip_chain_tail);


  /* mark as finished, the dependencies are already finished */
  issue_finish();

  ASTRALassert(workroom.m_mip_chain.size() == m_image->mip_chain().size());
  workroom.m_mip_chain.clear();

  /* the ctor created the image; we need to set m_image_create_spec
   * so that fetch_image() operates correctly.
   */
  m_image_create_spec
    .create_immediately(true)
    .default_use_prepadding_true(true);
}

astral::Renderer::VirtualBuffer::
VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer, RenderTarget &rt,
              u8vec4 clear_color, enum colorspace_t colorspace, RenderValue<Brush> clear_brush,
              const SubViewport *region):
  m_renderer(renderer),
  m_use_pixel_rect_tile_culling(renderer.m_default_use_pixel_rect_tile_culling),
  m_render_accuracy(renderer.m_default_render_accuracy),
  m_use_sub_ubers(true),
  m_transformation_stack(*renderer.m_storage->allocate_transformation_stack()),
  m_renderer_begin_cnt(renderer.m_begin_cnt),
  m_creation_tag(C),
  m_type(render_target_buffer),
  m_colorspace(colorspace),
  m_clear_brush(clear_brush),
  m_finish_issued(false),
  m_render_index(render_index),
  m_uses_this_buffer_list(renderer.m_storage->allocate_buffer_list()),
  m_dependency_list(renderer.m_storage->allocate_buffer_list()),
  m_remaining_dependencies(0u),
  m_users_that_completed_rendering(0u),
  m_pause_snapshot_counter(0),
  m_render_target(&rt),
  m_render_target_clear_color(clear_color),
  m_start_z(0u),
  m_stc_fill_rule(number_fill_rule),
  m_blit_rects(nullptr),
  m_images_with_mips(nullptr),
  m_last_mip_only(nullptr),
  m_uses_shadow_map(false)
{
  m_transformation_stack.push_back(Transformation());

  if (region)
    {
      m_region = *region;
      m_clip_geometry = Implement::ClipGeometryGroup(renderer.m_storage->create_clip(region->m_size));

      /* Recall that the clip-window is in coordinates BEFORE the ScaleTranslate that places
       * it on the surface.
       */
      m_clip_window = renderer.create_clip_window(m_clip_geometry.bounding_geometry().pixel_rect().as_rect().m_min_point,
                                                  m_clip_geometry.bounding_geometry().pixel_rect().as_rect().size());

      /* Now place the virtual buffer onto the render target by setting m_render_scale_translate */
      ScaleTranslate tr;

      tr.m_translate = vec2(region->m_xy);
      m_render_scale_translate = m_renderer.create_value(tr);
    }
  else
    {
      m_clip_geometry = Implement::ClipGeometryGroup(renderer.m_storage->create_clip(rt.size()));
    }
  m_command_list = renderer.m_storage->allocate_command_list(Implement::DrawCommandList::render_color_image,
                                                             image_processing_none,
                                                             m_clip_geometry.bounding_geometry().pixel_rect());
}

astral::Renderer::VirtualBuffer::
VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
              VirtualBuffer &src_buffer, const RectT<int> image_region,
              enum sub_buffer_type_t tp):
  m_renderer(renderer),
  m_use_pixel_rect_tile_culling(src_buffer.m_use_pixel_rect_tile_culling),
  m_render_accuracy(renderer.m_default_render_accuracy),
  m_use_sub_ubers(true),
  m_transformation_stack(*renderer.m_storage->allocate_transformation_stack()),
  m_renderer_begin_cnt(renderer.m_begin_cnt),
  m_creation_tag(C),
  m_type(sub_image_buffer),
  m_colorspace(src_buffer.m_colorspace),
  m_clear_brush(src_buffer.m_clear_brush),
  m_finish_issued(false),
  m_render_index(render_index),
  m_uses_this_buffer_list(renderer.m_storage->allocate_buffer_list()),
  m_dependency_list(renderer.m_storage->allocate_buffer_list()),
  m_remaining_dependencies(0u),
  m_users_that_completed_rendering(0u),
  m_command_list(nullptr),
  m_clip_geometry(src_buffer.m_clip_geometry),
  m_pause_snapshot_counter(0),
  m_start_z(0u),
  m_stc_fill_rule(src_buffer.m_stc_fill_rule),
  m_blit_rects(src_buffer.m_blit_rects),
  m_images_with_mips(nullptr),
  m_last_mip_only(nullptr),
  m_uses_shadow_map(false)
{
  BoundingBox<float> pixel_region;

  ASTRALassert(image_buffer == src_buffer.m_type);
  ASTRALassert(src_buffer.m_image);

  m_image = src_buffer.m_image;
  for (int c = 0; c < 2; ++c)
    {
      int max_sz(m_image->size()[c]);

      m_render_rect.m_min_point[c] = image_region.m_min_point[c];
      m_render_rect.m_max_point[c] = t_min(max_sz, image_region.m_max_point[c]);
    }

  /* map image coordinates to pixel coordinates */
  pixel_region = m_clip_geometry.bounding_geometry().image_transformation_pixel().inverse().apply_to_bb(BoundingBox<float>(image_region));

  /* create m_clip_window */
  m_clip_window = renderer.create_clip_window(pixel_region.as_rect().m_min_point,
                                              pixel_region.as_rect().size());

  if (tp == sub_image_buffer_copy_commands_from_parent)
    {
      if (src_buffer.render_type() == Implement::DrawCommandList::render_color_image)
        {
          m_command_list = renderer.m_storage->allocate_command_list(src_buffer.render_type(),
                                                                     src_buffer.blit_processing(),
                                                                     pixel_region);

          /* copy the commands from src_buffer that intersect pixel_region into m_command_list */
          m_command_list->copy_commands(*src_buffer.m_command_list, //src
                                        RenderValue<Transformation>(), //transformation, no value means work in pixel coordinates
                                        pixel_region, 0.0f, //region and padding
                                        true, //delete any command fully contained within pixel_region
                                        OnAddDependencyImpl(this)); //add dependencies here

          /* color buffers have no STC data to copy */
        }
      else
        {
          /* DrawCommandList does not track region covered by each draw under
           * mask rendering. So we just copy the command list pointer.
           *
           * TODO: make mask rendering also track region of each draw.
           */
          ASTRALassert(src_buffer.render_type() == Implement::DrawCommandList::render_mask_image);
          m_command_list = src_buffer.m_command_list;

          Implement::STCData::DataSet &stc_backing(m_renderer.m_storage->stc_data_set());
          for (unsigned int i = 0; i < FillSTCShader::pass_count; ++i)
            {
              unsigned int begin, end;

              begin = stc_backing.m_stc_data[i].size();

              Implement::STCData::copy_stc(&stc_backing.m_stc_data[i], &stc_backing.m_stc_subelement_backing[i],
                                           src_buffer.m_stc[i], pixel_region, true);

              end = stc_backing.m_stc_data[i].size();

              m_stc[i] = Implement::STCData::VirtualArray(begin, end);
            }

          /* we also need to copy the dependencies by hand too */
          ASTRALassert(src_buffer.m_dependency_list);
          for (VirtualBuffer *b : *src_buffer.m_dependency_list)
            {
              add_dependency(*b);
            }
        }
    }
  else
    {
      m_transformation_stack.push_back(Transformation());
      m_command_list = renderer.m_storage->allocate_command_list(src_buffer.render_type(),
                                                                 src_buffer.blit_processing(),
                                                                 pixel_region);
    }

  /* Image was created before ctor; we need to set m_image_create_spec
   * so that fetch_image() operates correctly.
   */
  m_image_create_spec
    .create_immediately(true)
    .default_use_prepadding_true(true);
}

astral::Renderer::VirtualBuffer::
VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
              const Transformation &initial_transformation,
              Implement::ClipGeometryGroup &geometry,
              enum Implement::DrawCommandList::render_type_t render_type,
              enum image_blit_processing_t blit_processing,
              enum colorspace_t colorspace,
              enum fill_rule_t stc_fill_rule,
              c_array<const vecN<range_type<int>, 2>> tile_regions,
              c_array<VirtualBuffer*> out_virtual_buffers):
  m_renderer(renderer),
  m_use_pixel_rect_tile_culling(renderer.m_default_use_pixel_rect_tile_culling),
  m_render_accuracy(renderer.m_default_render_accuracy),
  m_use_sub_ubers(true),
  m_transformation_stack(*renderer.m_storage->allocate_transformation_stack()),
  m_renderer_begin_cnt(renderer.m_begin_cnt),
  m_creation_tag(C),
  m_type(image_buffer),
  m_colorspace(colorspace),
  m_finish_issued(false),
  m_render_index(render_index),
  m_uses_this_buffer_list(nullptr),
  m_dependency_list(nullptr),
  m_remaining_dependencies(0u),
  m_users_that_completed_rendering(0u),
  m_command_list(nullptr),
  m_clip_geometry(geometry),
  m_pause_snapshot_counter(0),
  m_start_z(0u),
  m_stc_fill_rule(stc_fill_rule),
  m_blit_rects(nullptr),
  m_images_with_mips(nullptr),
  m_last_mip_only(nullptr),
  m_uses_shadow_map(false)
{
  ASTRALassert(m_clip_geometry.bounding_geometry().image_size().x() > 0 && m_clip_geometry.bounding_geometry().image_size().y() > 0);
  ASTRALassert(out_virtual_buffers.size() == tile_regions.size());

  m_image = make_partially_backed_image(renderer, m_render_index, m_clip_geometry.bounding_geometry().image_size(), colorspace, tile_regions);
  m_clip_window = renderer.create_clip_window(m_clip_geometry.bounding_geometry().pixel_rect().as_rect().m_min_point,
                                              m_clip_geometry.bounding_geometry().pixel_rect().as_rect().size());
  m_uses_this_buffer_list = renderer.m_storage->allocate_buffer_list();
  m_dependency_list = renderer.m_storage->allocate_buffer_list();

  /* The child VirtualBuffer objects expect this to have a command list so that
   * they can fetch the render_type().
   *
   * TODO: have a few special command list objects that are reused that are for
   *       command lists that are to never have commands added to them.
   */
  m_command_list = renderer.m_storage->allocate_command_list(render_type, blit_processing, m_clip_geometry.bounding_geometry().pixel_rect());

  /* is this really needed? Moreover, can we make the transformation stack also
   * shared for this case?
   */
  m_transformation_stack.push_back(initial_transformation);

  ivec2 tile_count(m_image->mip_chain().front()->tile_count());
  ivec2 image_size(m_image->size());

  for (unsigned int i = 0; i < out_virtual_buffers.size(); ++i)
    {
      RenderEncoderBase encoder;
      vecN<range_type<int>, 2> tiles;
      RectT<int> image_region;

      tiles = tile_regions[i];

      /* Because a neighboring tile can be empty, that means
       * the pixels of padding shared with such a neighboring
       * tile are never blitted.
       *
       * The right thing to do is a bit of a pain.
       *  1) set encoder.m_render_rect as with the padding
       *  2) add to encoder.m_blit_rects the following rects
       *    a) the region without the padding
       *    b) for each side and each tile whose neighbor is
       *       an empty tile, an additional rect of the padding.
       *       This is not just a single rect for each side
       *       because a side can have multiple neighbors when
       *       either of the ranges from tile_regions[i] is
       *       bigger than one in size.
       *
       * For now, we allow for the overdraw in the image-atlas.
       * Note that we do NOT include the padding on the start
       * This is beause those pixels do not really exist; also
       * the t_min() on max_point() prevents us from going past
       * the image as well.
       */
      int Px(tiles.x().m_begin == 0 ? 0 : ImageAtlas::tile_padding);
      int Py(tiles.y().m_begin == 0 ? 0 : ImageAtlas::tile_padding);

      image_region
        .min_point(tiles.x().m_begin * ImageAtlas::tile_size_without_padding - Px,
                   tiles.y().m_begin * ImageAtlas::tile_size_without_padding - Py)
        .max_point(t_min(tiles.x().m_end * ImageAtlas::tile_size_without_padding + ImageAtlas::tile_padding, image_size.x()),
                   t_min(tiles.y().m_end * ImageAtlas::tile_size_without_padding + ImageAtlas::tile_padding, image_size.y()));

      /* NOTE: the called ctor to VirtualBuffer below requires that the src buffer (which is this)
       *       has m_type with value image_buffer, this is why our ctor set it to image_buffer.
       */
      encoder = m_renderer.m_storage->create_virtual_buffer(VB_TAG, *this, image_region, sub_image_buffer_renderer);
      out_virtual_buffers[i] = &encoder.virtual_buffer();

      /* make this depend on out_virtual_buffers[i]; note that we allow for the
       * dependency to be unfinished, this is because the virtual buffers were just
       * created;
       */
      add_dependency(*out_virtual_buffers[i], true);
    }

  /* set the type to assembled buffer so it is not drawn directly */
  m_type = assembled_buffer;

  /* the ctor created the image; we need to set m_image_create_spec
   * so that fetch_image() operates correctly.
   */
  m_image_create_spec
    .create_immediately(true)
    .default_use_prepadding_true(true);
}

astral::Renderer::VirtualBuffer::
VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
              reference_counted_ptr<ShadowMap> shadow_map,
              vec2 light_p):
  m_renderer(renderer),
  m_use_pixel_rect_tile_culling(renderer.m_default_use_pixel_rect_tile_culling),
  m_render_accuracy(renderer.m_default_render_accuracy),
  m_use_sub_ubers(true),
  m_transformation_stack(*renderer.m_storage->allocate_transformation_stack()),
  m_renderer_begin_cnt(renderer.m_begin_cnt),
  m_creation_tag(C),
  m_type(shadowmap_buffer),
  m_colorspace(colorspace_linear), /* shadowmap's are not really rendered in a colorspace */
  m_finish_issued(false),
  m_render_index(render_index),
  m_uses_this_buffer_list(renderer.m_storage->allocate_buffer_list()),
  m_dependency_list(renderer.m_storage->allocate_buffer_list()),
  m_remaining_dependencies(0u),
  m_users_that_completed_rendering(0u),
  m_command_list(renderer.m_storage->allocate_command_list_for_shadow_map()),
  m_pause_snapshot_counter(0),
  m_start_z(0u),
  m_stc_fill_rule(number_fill_rule),
  m_blit_rects(nullptr),
  m_images_with_mips(nullptr),
  m_last_mip_only(nullptr),
  m_shadow_map(shadow_map),
  m_pre_transformation(Transformation(-light_p)),
  m_uses_shadow_map(false)
{
  m_shadow_map->mark_as_virtual_render_target(detail::MarkShadowMapAsRenderTarget(render_index));
  m_transformation_stack.push_back(Transformation());
}

void
astral::Renderer::VirtualBuffer::
create_backing_image(void)
{
  ASTRALassert(m_command_list);
  ASTRALassert(m_type == image_buffer);

  if (m_image)
    {
      return;
    }

  uvec2 sz(m_clip_geometry.bounding_geometry().image_size());
  if (sz.x() > 0 && sz.y() > 0)
    {
      c_array<const uvec2> empty_tiles;

      if (render_type() == Implement::DrawCommandList::render_color_image)
        {
          BoundingBox<int> bb;

          if (m_finish_issued)
            {
              /* No more commands will be added, thus we can view any tile not
               * hit by a command as an empty tile.
               */
              empty_tiles = m_renderer.m_workroom->m_tile_hit_detection.compute_empty_tiles(*m_renderer.m_storage, clip_geometry(),
                                                                                            *command_list(), m_use_pixel_rect_tile_culling, &bb);
            }
          else
            {
              /* Commands can still be added, so only the clip_geometry() can
               * be used to cull tiles.
               */
              empty_tiles = m_renderer.m_workroom->m_tile_hit_detection.compute_empty_tiles(*m_renderer.m_storage, clip_geometry(),
                                                                                            m_use_pixel_rect_tile_culling, &bb);
            }

          if (!bb.empty())
            {
              m_render_rect.m_min_point = bb.as_rect().m_min_point;
              m_render_rect.m_max_point = bb.as_rect().m_max_point;
            }
          else
            {
              m_render_rect.m_min_point = ivec2(0, 0);
              m_render_rect.m_max_point = ivec2(0, 0);
            }

          /* when performing down sampling, we need a slack of one or two pixels;
           * so add the slack.
           *
           * TODO: not actually very clear why we need the slack....
           */
          int required_slack(2);

          m_render_rect.m_min_point.x() = t_max(0, m_render_rect.m_min_point.x() - required_slack);
          m_render_rect.m_min_point.y() = t_max(0, m_render_rect.m_min_point.y() - required_slack);

          m_render_rect.m_max_point.x() = t_min(m_clip_geometry.bounding_geometry().image_size().x(), m_render_rect.m_max_point.x() + required_slack);
          m_render_rect.m_max_point.y() = t_min(m_clip_geometry.bounding_geometry().image_size().y(), m_render_rect.m_max_point.y() + required_slack);
        }
      else
        {
          /* If the VirtualBuffer has not yet been finished or is not rendering
           * color values, then we assume that all tiles will be backed.
           */
          m_render_rect.m_min_point = ivec2(0, 0);
          m_render_rect.m_max_point = ivec2(sz);
        }

      if (empty_tiles.empty())
        {
          m_image = m_renderer.m_engine->image_atlas().create_rendered_image(detail::RenderedImageTag(m_render_index),
                                                                             1u, sz, m_colorspace);
        }
      else
        {
          c_array<const uvec2> fully_covered_tiles;
          reference_counted_ptr<ImageMipElement> mip;
          int area_used, image_area;

          mip = m_renderer.m_engine->image_atlas().create_mip_element(sz, empty_tiles, fully_covered_tiles);
          mip->number_mipmap_levels(1);

          vecN<reference_counted_ptr<const ImageMipElement>, 1> mip_chain(mip);
          m_image = m_renderer.m_engine->image_atlas().create_rendered_image(detail::RenderedImageTag(m_render_index),
                                                                             mip_chain, m_colorspace);

          area_used = m_render_rect.width() * m_render_rect.height();
          image_area = sz.x() * sz.y();
          ASTRALassert(m_render_rect.width() <= static_cast<int>(sz.x()));
          ASTRALassert(m_render_rect.height() <= static_cast<int>(sz.y()));

          //std::cout << this << "(render_index = " << m_render_index
          //        << "), " << empty_tiles.size() << " empty tiles, render_size = "
          //        << m_render_rect.size() << ", orginal size = " << sz << "\n";

          m_renderer.m_stats[number_skipped_color_buffer_pixels] += image_area - area_used;
        }

      if (m_image_create_spec.m_default_use_prepadding_true)
        {
          m_image->default_use_prepadding(true);
        }
    }
}

astral::Renderer::VirtualBuffer::
~VirtualBuffer()
{
  if (m_image)
    {
      #if 0
        {
          std::cout << "Huh, {" << m_render_index << "}.image = " << m_image.get() << " survived to dtor, type = "
                    << m_type << ", m_uses_this_buffer_list.size() = "
                    << m_uses_this_buffer_list->size()
                    << ", m_users_that_completed_rendering = "
                    << m_users_that_completed_rendering << ", tag = ["
                    << m_creation_tag.m_file << ", " << m_creation_tag.m_line << "], ref_count = "
                    << m_image->reference_count() << "\n";
        }
      #endif
    }
}

astral::reference_counted_ptr<const astral::Image>
astral::Renderer::VirtualBuffer::
create_assembled_image(CreationTag C, Renderer::Implement &renderer,
                       const astral::Image &image,
                       vecN<range_type<unsigned int>, 2> tile_range,
                       c_array<const uvec2> empty_tiles,
                       c_array<const uvec2> fully_covered_tiles,
                       c_array<const uvec2> shared_tiles)
{
  if (!shared_tiles.empty() && image.offscreen_render_index() != InvalidRenderValue)
    {
      RenderEncoderImage vb;

      vb = renderer.m_storage->create_virtual_buffer(C, image, tile_range,
                                                     empty_tiles, fully_covered_tiles,
                                                     shared_tiles);

      vb.finish();

      return vb.image();
    }

  /* image is not a rendered image, thus we can use the ImageAtlas::create_image()
   * because the returned Image value does NOT have any depencies.
   */
  vecN<reference_counted_ptr<const ImageMipElement>, 1> mip;
  const ImageMipElement *mip_src;
  reference_counted_ptr<Image> return_value;

  ASTRALassert(image.mip_chain().size() == 1);

  if (!shared_tiles.empty())
    {
      /* we need to mark it as in use, sorry for the const_cast */
      astral::Image *p;

      p = const_cast<astral::Image*>(&image);
      p->mark_in_use();

      unsigned int ri(p->offscreen_render_index());
      renderer.m_storage->virtual_buffer(ri).issue_finish();
    }

  mip_src = image.mip_chain().front().get();
  mip[0] = mip_src->create_sub_mip(tile_range, empty_tiles, fully_covered_tiles, shared_tiles);

  return_value = renderer.m_engine->image_atlas().create_image(mip, image.colorspace());
  return_value->default_use_prepadding(image.default_use_prepadding());

  return return_value;
}

astral::reference_counted_ptr<const astral::Image>
astral::Renderer::VirtualBuffer::
create_assembled_image(CreationTag C, Renderer::Implement &renderer,
                       uvec2 sz, enum colorspace_t colorspace,
                       c_array<const uvec2> empty_tiles,
                       c_array<const uvec2> fully_covered_tiles,
                       c_array<const std::pair<uvec2, TileSource>> encoder_shared_tiles,
                       c_array<const std::pair<uvec2, TileSourceImage>> image_shared_tiles)
{
  RenderEncoderImage assemblage;
  reference_counted_ptr<Image> return_value;
  bool need_virtual_buffer(!encoder_shared_tiles.empty());

  for (unsigned int i = 0, endi = image_shared_tiles.size(); i < endi && !need_virtual_buffer; ++i)
    {
      need_virtual_buffer = image_shared_tiles[i].second.m_src_image
        && image_shared_tiles[i].second.m_src_image->offscreen_render_index() != InvalidRenderValue;
    }

  if (need_virtual_buffer)
    {
      assemblage = renderer.m_storage->create_virtual_buffer(C, sz, colorspace,
                                                             empty_tiles,
                                                             fully_covered_tiles,
                                                             encoder_shared_tiles,
                                                             image_shared_tiles);
      assemblage.finish();
      return_value = assemblage.image();
    }
  else
    {
      ASTRALassert(encoder_shared_tiles.empty());
      ASTRALassert(renderer.m_workroom->m_shared_tiles.empty());
      for (const auto &p : image_shared_tiles)
        {
          ImageAtlas::TileElement E;

          ASTRALassert(p.second.m_src_image);
          ASTRALassert(!p.second.m_src_image->mip_chain().empty());
          ASTRALassert(p.second.m_src_image->mip_chain().front());

          E.m_src = p.second.m_src_image->mip_chain().front();
          E.m_tile = p.second.m_src_tile;
          renderer.m_workroom->m_shared_tiles.push_back(std::make_pair(p.first, E));
        }

      vecN<reference_counted_ptr<const ImageMipElement>, 1> mip;

      mip[0] = renderer.m_engine->image_atlas().create_mip_element(sz, 1, empty_tiles, fully_covered_tiles,
                                                                   make_c_array(renderer.m_workroom->m_shared_tiles));
      renderer.m_workroom->m_shared_tiles.clear();

      return_value = renderer.m_engine->image_atlas().create_image(mip, colorspace);
      return_value->default_use_prepadding(true);
    }

  return return_value;
}

void
astral::Renderer::VirtualBuffer::
generate_next_mipmap_level(void)
{
  ASTRALassert(m_images_with_mips);
  ASTRALassert(m_last_mip_only);
  ASTRALassert(!m_images_with_mips->empty());
  ASTRALassert(m_last_mip_only->size() == m_images_with_mips->size());
  ASTRALassert(m_images_with_mips->front() == this);
  ASTRALhard_assert(finish_issued());

  /* the algorithm assumes that each ImageMipElement holds two
   * mipmap levels.
   */
  ASTRALstatic_assert(ImageMipElement::maximum_number_of_mipmaps == 2u);

  uint32_t LOD(m_images_with_mips->size()); //LOD to generate
  if ((LOD & 1u) == 1u)
    {
      /* Odd level mipmap means that we issue Image::downsample_pixels()
       * from the scratch render target to generate the mipmap level
       */
      reference_counted_ptr<ImageMipElement> mip_element;

      ASTRALassert(m_dangling_mip_chain);
      ASTRALassert(m_dangling_mip_chain->type() == image_buffer);

      ASTRALassert(m_dangling_mip_chain->m_command_list->render_type() == Implement::DrawCommandList::render_color_image);
      ASTRALassert(m_dangling_mip_chain->m_image);
      ASTRALassert(m_dangling_mip_chain->m_image->number_mipmap_levels() == 1u);

      mip_element = m_dangling_mip_chain->m_image->mip_chain().front().const_cast_ptr<ImageMipElement>();
      mip_element->number_mipmap_levels(2u);

      /* Note that we -repliciate- the previous entry. This is because
       * m_dangling_mip_chain->m_image->mip_chain() is just the last made
       * mip-level
       */
      m_images_with_mips->push_back(m_images_with_mips->back());
      m_last_mip_only->push_back(m_last_mip_only->back());

      ASTRALassert(m_dangling_mip_chain->m_image->number_mipmap_levels() == 2u);
      m_dangling_mip_chain = nullptr;
    }
 else
   {
     ASTRALassert(!m_dangling_mip_chain);
     ASTRALassert(m_last_mip_only);
     ASTRALassert(!m_last_mip_only->empty());
     ASTRALassert(m_last_mip_only->back());
     ASTRALassert(m_last_mip_only->back()->m_image);

     /* the VirtualBuffer that generated the LOD - 1. That VirtualBuffer's
      * m_image includes the mipmap's from 0 to LOD - 1 inclusive.
      * We will make this VirtualBuffer depend on that one to make
      * sure that the image is generated.
      */
     VirtualBuffer &vb_image(*m_images_with_mips->back());
     ASTRALassert(vb_image.m_image->number_mipmap_levels() == LOD);

     /* Step 1. make an encoder E of a quarter the resolution of the
      *         last element of m_last_mip_only; this is because
      *
      * TODO: have a special VirtualBuffer type that is only for
      *       downsampling. Its properties are:
      *         - does not support partial coverage
      *         - only does raw blit of downsampling previous image
      *         - does NOT work in pre-multiplied by alpha
      */
     const Image &last_image(*m_last_mip_only->back()->m_image);
     ivec2 image_size(last_image.size()), sz(image_size.x() >> 2, image_size.y() >> 2);

     RenderEncoderImage E;
     E = RenderEncoderImage(this).encoder_image(sz);

     /* Step 2. Render to the E a blit rect of last mipmap level supported */

     /* scale from size of the last_image to size of the LOD we are generating */
     E.scale(vec2(sz) / vec2(image_size));

     /* Use draw_image() which will draw the image tile-by-tile.
      * Note that we requrest to draw MipmapLevel(1); this is
      * because last_image's mipchain has only the last ImageMipElement.
      * Note that we use blend_porter_duff_src_over instead of
      * blend_porter_duff_src. We can do this because the pixels
      * start as clear-black (0, 0, 0, 0) which gives that src-over
      * is the same as src in this case. We need to do this because
      * with blend_porter_duff_src, draw image will still draw
      * clear black rectangles (thus forcing tiles to be backed).
      * Anti-aliasing in the draw is off because the blend mode
      * blend_porter_duff_src with anti-aliasing induces framebuffer
      * fetch reads. The filtering is linear to induce the downsampling
      * of the 2x2 block to create the miplevel.
      */
     E.draw_image(last_image, MipmapLevel(1u),
                  RenderEncoderBase::ImageDraw(filter_linear).with_aa(false),
                  blend_porter_duff_src_over);
     E.finish();

     ASTRALassert(E.image());
     ASTRALassert(E.image()->mip_chain().size() == 1u);

     /* Step 3. Save E to m_dangling_mip_chain */
     m_dangling_mip_chain = &E.virtual_buffer();

     /* Step 4. Make a new VirtualBuffer B which is the mipchain
      *         of the last element of m_images_with_mips and E
      */
     RenderEncoderImage B;
     B = m_renderer.m_storage->create_virtual_buffer(VB_TAG, vb_image, *m_dangling_mip_chain);

     /* Step 5. Add B to m_images_with_mips and add E (aliased
      *         via m_dangling_mip_chain to m_last_mip_only
      */
     m_images_with_mips->push_back(&B.virtual_buffer());
     m_last_mip_only->push_back(m_dangling_mip_chain);

     ASTRALassert(B.virtual_buffer().finish_issued());
     ASTRALassert(B.image());

     /* B holds miplevens [0, LOD] inclusive, which means it has
      * LOD + 1 mipmaps.
      */
     ASTRALassert(B.image()->number_mipmap_levels() == LOD + 1u);
   }
}

const astral::reference_counted_ptr<astral::Image>&
astral::Renderer::VirtualBuffer::
image_last_mip_only(unsigned int lod, unsigned int *actualLOD)
{
  /* make sure it is generated */
  image_with_mips(lod);

  lod = t_min(lod, m_max_lod_supported);
  if (lod & 1)
    {
      *actualLOD = lod - 1u;
    }
  else
    {
      *actualLOD = lod;
    }

  ASTRALassert(m_last_mip_only->operator[](lod)->finish_issued());
  return m_last_mip_only->operator[](lod)->fetch_image();
}

const astral::reference_counted_ptr<astral::Image>&
astral::Renderer::VirtualBuffer::
image_with_mips(unsigned int lod)
{
  if (type() == degenerate_buffer)
    {
      m_max_lod_supported = 0u;
      return m_image;
    }

  ASTRALhard_assert(finish_issued());
  ASTRALassert(type() == image_buffer);
  if (!m_images_with_mips)
    {
      m_images_with_mips = m_renderer.m_storage->allocate_buffer_list();
      m_last_mip_only = m_renderer.m_storage->allocate_buffer_list();

      m_images_with_mips->push_back(this);
      m_last_mip_only->push_back(this);
      m_dangling_mip_chain = this;

      uvec2 sz(m_image->size());
      m_max_lod_supported = t_min(uint32_log2_floor(sz.x()), uint32_log2_floor(sz.y()));
    }

  lod = t_min(lod, m_max_lod_supported);
  while (lod >= m_images_with_mips->size())
    {
      generate_next_mipmap_level();
    }

  ASTRALassert(m_images_with_mips->operator[](lod)->finish_issued());
  return m_images_with_mips->operator[](lod)->fetch_image();
}

void
astral::Renderer::VirtualBuffer::
add_depth_rect_shader_to_uber(Renderer::Implement &renderer, RenderBackend::UberShadingKey &uber_key)
{
  RenderValues st;

  st.m_transformation = renderer.m_identity;
  st.m_material = renderer.m_black_brush;
  st.m_blend_mode = BackendBlendMode(blend_porter_duff_src_over, false);

  uber_key.add_shader(*renderer.m_default_shaders.m_dynamic_rect_shader, st);
}

void
astral::Renderer::VirtualBuffer::
draw_depth_rect(RenderBackend::UberShadingKey::Cookie uber_key_cookie, unsigned int f)
{
  ivec2 sz;
  RectT<int> rect;
  vecN<gvec4, DynamicRectShader::item_data_size> rect_data;
  RenderValues st;

  ASTRALassert(type() == image_buffer || type() == sub_image_buffer
               || (type() == render_target_buffer && m_clip_window.clip_window_value_type() != clip_window_not_present));


  if (type() != render_target_buffer)
    {
      sz = offscreen_render_size();
      rect.m_min_point = m_location_in_color_buffer.m_location;
    }
  else
    {
      rect.m_min_point = m_region.m_xy;
      sz = m_region.m_size;
    }
  rect.m_max_point = rect.m_min_point + sz;

  DynamicRectShader::pack_item_data(rect, rect_data);

  st.m_transformation = m_renderer.m_identity;
  st.m_material = m_renderer.m_black_brush;
  st.m_blend_mode = BackendBlendMode(blend_porter_duff_src_over, false);
  st.m_item_data = m_renderer.create_item_data(rect_data, no_item_data_value_mapping);

  m_renderer.m_backend->draw_render_data(f, *m_renderer.m_default_shaders.m_dynamic_rect_shader, st,
                                         uber_key_cookie, RenderValue<ScaleTranslate>(),
                                         RenderBackend::ClipWindowValue(),
                                         m_location_in_color_buffer.m_permute_xy,
                                         m_renderer.m_dynamic_rect->vertex_range());
}

void
astral::Renderer::VirtualBuffer::
location_in_color_buffer(Implement::WorkRoom::ImageBufferLocation E)
{
  ScaleTranslate tr;

  ASTRALassert(E.valid());
  ASTRALassert(!m_location_in_color_buffer.valid());
  ASTRALassert(!m_render_scale_translate.valid());

  /* Recall that the translate is the transformation
   * from m_image coordinates to where the stuff is
   * rendered in the color buffer. Because of this,
   * m_render_rect.min_point() needs to map to
   * E.location()
   */
  m_location_in_color_buffer = E;
  tr = image_transformation_pixel();
  tr.m_translate += vec2(m_location_in_color_buffer.m_location) - vec2(m_render_rect.m_min_point);

  m_render_scale_translate = m_renderer.create_value(tr);

  ASTRALassert(m_command_list);
}

void
astral::Renderer::VirtualBuffer::
realize_as_sub_buffers(RectT<int> region)
{
  /* 1. Create VirtualBuffer objects whose purpose is
   *    to render sub-regions of the VirtualBuffer
   * 2. Make this VirtualBuffer depend on those child
   *    VirtualBuffer objects.
   * 3. Change this VirtualBuffer type so that Renderer
   *    does not attempt to render its contents.
   */
  ASTRALassert(region.width() > max_renderable_buffer_size
               || region.height() > max_renderable_buffer_size);

  ASTRALassert(type() == image_buffer);

  ivec2 num_buffers, lastsize, computed_size;

  computed_size = region.size();
  num_buffers = computed_size / ivec2(max_renderable_buffer_size);
  for (int c = 0; c < 2; ++c)
    {
      lastsize[c] = computed_size[c] - num_buffers[c] * max_renderable_buffer_size;
      if (lastsize[c] != 0)
        {
          ++num_buffers[c];
        }
      else
        {
          lastsize[c] = max_renderable_buffer_size;
        }
    }

  /* We must add the dependencies after creating ALL of the child buffers
   * because if this is a mask buffer, the ctor for the child buffers will
   * copy the contents of this's dependency list which would make the younger
   * siblings depend on the older siblings which is not the case.
   */
  ASTRALassert(m_renderer.m_workroom->m_tmp_buffer_list.empty());
  for (int x = 0, px = region.m_min_point.x(); x < num_buffers.x(); ++x, px += max_renderable_buffer_size)
    {
      int size_x;

      size_x = (x + 1 == num_buffers.x()) ? lastsize.x() : max_renderable_buffer_size;
      for (int y = 0, py = region.m_min_point.y(); y < num_buffers.y(); ++y, py += max_renderable_buffer_size)
        {
          int size_y;
          RectT<int> image_rect;
          BoundingBox<int> sub_image_rect;
          RenderEncoderBase V;

          size_y = (y + 1 == num_buffers.y()) ? lastsize.y() : max_renderable_buffer_size;

          image_rect
            .min_point(px, py)
            .max_point(px + size_x, py + size_y);

          ASTRALhard_assert(size_x > 0);
          ASTRALhard_assert(size_y > 0);

          sub_image_rect = restrict_rect_to_nonempty_tiles(*m_image, image_rect);
          if (!sub_image_rect.empty())
            {
              V = m_renderer.m_storage->create_virtual_buffer(VB_TAG, *this, sub_image_rect.as_rect(), sub_image_buffer_copy_commands_from_parent);
              V.finish();

              m_renderer.m_workroom->m_tmp_buffer_list.push_back(V);
            }
        }
    }

  for (RenderEncoderBase V : m_renderer.m_workroom->m_tmp_buffer_list)
    {
      VirtualBuffer &buffer(V.virtual_buffer());
      add_dependency(buffer);
    }
  m_renderer.m_workroom->m_tmp_buffer_list.clear();

  /* prevent Renderer from trying to render the contents of this
   * VirtualBuffer directly
   */
  m_type = assembled_buffer;
}

void
astral::Renderer::VirtualBuffer::
on_renderer_end(void)
{
  if (!finish_issued())
    {
      issue_finish();
    }

  if (m_image && m_image->tile_allocation_failed())
    {
      return;
    }

  if (m_image && m_image->reference_count() == 1 && m_uses_this_buffer_list->empty())
    {
      #if 0
        {
          std::cout << "{ " << m_render_index << "}, [" << m_creation_tag.m_file
                    << ", " << m_creation_tag.m_line << "], type = " << m_type
                    << ", render_type = " << render_type()
                    << " has no one using its image, " << m_image.get()
                    << " of size " << m_image->size() << " with "
                    << m_image->mip_chain().front()->number_elements(ImageMipElement::color_element)
                    << " real tiles, so we won't render it\n";
        }
      #endif

      m_image = nullptr;
      m_type = degenerate_buffer;
    }

  switch (type())
    {
    case image_buffer:
      {
        astral::ivec2 buffer_size(m_render_rect.size());

        if (buffer_size.x() > max_renderable_buffer_size
            || buffer_size.y() > max_renderable_buffer_size)
          {
            realize_as_sub_buffers(m_render_rect);
          }
      }
      break;

    case shadowmap_buffer:
      /* Only those ShadowMaps that are not directly rendered
       * to the ShadowMapAtlas need to be rendered to an offscreen
       * buffer.
       */
      if (uses_shadow_map() || remaining_dependencies() != 0)
        {
          /* TODO: if shadow_map()->dimensions() exceeeds max_renderable_buffer_size,
           *       then the ShadowMap must be rendered in pieces
           */
          ASTRALassert(shadow_map()->dimensions() <= max_renderable_buffer_size);
        }
      break;

    case sub_image_buffer:
      {
        ASTRALhard_assert(m_render_rect.width() <= max_renderable_buffer_size);
        ASTRALhard_assert(m_render_rect.height() <= max_renderable_buffer_size);
      }
      break;

    case render_target_buffer:
      /* nothing to do */
      break;

    case degenerate_buffer:
    case assembled_buffer:
      if (m_uses_this_buffer_list && m_remaining_dependencies == 0
          && !m_uses_this_buffer_list->empty())
        {
          /* It is possible that an assembled_buffer (or worse
           * degenerate_buffer) is somehow depended on but that
           * buffer depends on nothing itself. In that case,
           * render_performed() will not get executed, so we
           * execute it here *now*.
           *
           * We issue this print because this should be avoided
           * because having such VirtualBuffer objects does
           * nothing.
           */
          std::cout << "{" << m_render_index << "}, [" << m_creation_tag.m_file << ", "
                    << m_creation_tag.m_line << "]: type = " << m_type
                    << "\n";
          render_performed_implement(nullptr, nullptr);
        }
      break;

    default:
      ASTRALfailure("Invalid VirtualBuffer::m_type");
      break;
    }
}

void
astral::Renderer::VirtualBuffer::
on_renderer_end_abort(void)
{
  /* if there is a backing image, mark it as freely editable  */
  if (m_image && !m_image->tile_allocation_failed() && m_image->offscreen_render_index() != InvalidRenderValue)
    {
      m_image->mark_as_usual_image(detail::RenderedImageTag(InvalidRenderValue));
    }
}

void
astral::Renderer::VirtualBuffer::
add_scratch_area(BoundingBox<int> *dst) const
{
  ASTRALassert(type() == image_buffer || type() == sub_image_buffer || type() == shadowmap_buffer);
  if (type() == image_buffer || type() == sub_image_buffer)
    {
      ivec2 p;

      p = m_location_in_color_buffer.m_location + offscreen_render_size();
      if (m_location_in_color_buffer.m_permute_xy)
        {
          std::swap(p.x(), p.y());
        }
      dst->union_point(p);
    }
  else
    {
      dst->union_point(ivec2(m_location_in_depth_buffer) + ivec2(m_shadow_map->dimensions(), 4));
    }
}

enum astral::return_code
astral::Renderer::VirtualBuffer::
about_to_render_content(void)
{
  /* if m_image is non-null, make sure its color tiles are allocated, note that we need to
   * check if the render index is still InvalidRenderValue; this is for the case where
   * a large VirtualBuffer is broken into several VirtualBuffer objects.
   */
  if (m_image)
    {
      if (m_image->offscreen_render_index() != InvalidRenderValue)
        {
          m_image->mark_as_usual_image(detail::RenderedImageTag(InvalidRenderValue));
        }

      /* mark_as_usual_image() will force the color tiles to be backed; it is possible that
       * the color tiles failed to get allocated, this is why we check afterwards.
       */
      if (m_image->tile_allocation_failed())
        {
          ++m_renderer.m_stats[number_virtual_buffer_backing_allocation_failed];

          /* Allow for dependent buffers can still get rendered, even though
           * this buffer's content is utter garbage.
           */
          render_performed(nullptr);

          return routine_fail;
        }
      else
        {
          ++m_renderer.m_stats[number_non_degenerate_virtual_buffers];
          m_renderer.m_stats[number_virtual_buffer_pixels] += area();
          if (command_list()->renders_to_color_buffer())
            {
              m_renderer.m_stats[number_color_virtual_buffer_pixels] += area();
            }
          else
            {
              m_renderer.m_stats[number_mask_virtual_buffer_pixels] += area();
            }
        }
    }

  return routine_success;
}

void
astral::Renderer::VirtualBuffer::
render_performed_implement(ColorBuffer *color_src, DepthStencilBuffer *depth_src) const
{
  /* only one may be non-null at most */
  ASTRALassert(!color_src || !depth_src);

  /* assembled buffers have render_performed() without having about_to_render_content()
   * get called; thus we need to still need to remove the RenderedImageTag
   */
  if (m_image && m_image->offscreen_render_index() != InvalidRenderValue)
    {
      m_image->mark_as_usual_image(detail::RenderedImageTag(InvalidRenderValue));
    }

  /* blit the contents of color_src to m_image */
  if (m_image && !m_image->tile_allocation_failed())
    {
      if (m_type == image_buffer || m_type == sub_image_buffer)
        {
          unsigned int cnt(0);
          ivec2 sz(m_image->size());

          ASTRALassert(color_src);
          ASTRALassert(m_image);
          ASTRALassert(m_image->number_mipmap_levels() <= 2u);

          if (!m_blit_rects)
            {
              cnt = m_image->copy_pixels(0, //dst LOD
                                         m_render_rect.m_min_point, //dst min-corner
                                         m_render_rect.size(), //size
                                         *color_src,
                                         m_location_in_color_buffer.m_location, //src min corner
                                         blit_processing(),
                                         m_location_in_color_buffer.m_permute_xy);
            }
          else
            {
              for (const RectT<int> &R: *m_blit_rects)
                {
                  ivec2 R_sz;
                  RectT<int> blit_rect;

                  /* clip the rect to m_render_rect and blit the intersection */
                  if (RectT<int>::compute_intersection(R, m_render_rect, &blit_rect)
                      && blit_rect.m_max_point.x() > blit_rect.m_min_point.x()
                      && blit_rect.m_max_point.y() > blit_rect.m_min_point.y())
                    {
                      ivec2 src_min_corner;

                      /* The contents from color_src on the rect S where
                       *
                       *   S.m_min_corner = m_location_in_color_buffer.m_location
                       *   S.size() = m_render_rect.size()
                       *
                       * maps to in the image the rect m_render_rect. Thus, the
                       * transformation from *color_src coordinates to image
                       * coordinate is given by
                       *
                       *   image_coordinate = V + color_src_coordinate
                       *
                       *   color_src_coordinate = image_coordinate - V
                       *
                       * where
                       *
                       *   V = m_render_rect.m_min_corner - m_location_in_color_buffer.m_location
                       *
                       * We have that blit_rect is in image coordinate and the value
                       * of stc_min_corner is blit_rect.m_min_point in color_src
                       * which gives
                       *
                       *  src_min_corner = blit_rect.m_min_point - V
                       *                 = blit_rect.m_min_point - m_render_rect.m_min_corner + m_location_in_color_buffer.m_location
                       */

                      src_min_corner =  blit_rect.m_min_point
                        + m_location_in_color_buffer.m_location
                        - m_render_rect.m_min_point;

                      cnt += m_image->copy_pixels(0, //dst LOD
                                                  blit_rect.m_min_point, //dst min-corner
                                                  blit_rect.size(), //size
                                                  *color_src,
                                                  src_min_corner,
                                                  blit_processing(),
                                                  m_location_in_color_buffer.m_permute_xy);
                    }
                }
            }

          /* We have to do a full-blit even if blit-rects is provided
           * because there is no guarnatee that the blit-rects have
           * their corners at even integers.
           */
          if (m_image->number_mipmap_levels() >= 2u && m_render_rect.width() >= 2 && m_render_rect.height() >= 2)
            {
              ASTRALassert(m_image->number_mipmap_levels() == 2);
              cnt += m_image->downsample_pixels(1, //dst LOD
                                                m_render_rect.m_min_point / 2, //dst min-corner
                                                m_render_rect.size() / 2, //size in destination
                                                *color_src,
                                                m_location_in_color_buffer.m_location, //src min-corner
                                                downsampling_processing(),
                                                m_location_in_color_buffer.m_permute_xy);
            }

          m_renderer.m_stats[number_pixels_blitted] += cnt;
        }
    }

  if (m_shadow_map)
    {
      m_shadow_map->mark_as_virtual_render_target(detail::MarkShadowMapAsRenderTarget(InvalidRenderValue));
      if (depth_src)
        {
          ShadowMapAtlasBacking &backing(m_renderer.m_engine->shadow_map_atlas().backing());
          backing.copy_pixels(m_shadow_map->atlas_location(),
                              uvec2(m_shadow_map->dimensions(), 4),
                              *depth_src, m_location_in_depth_buffer);
        }
    }

  /* Every buffer that depends on this buffer
   * now have their m_remaining_dependencies
   * decremented by the number of times they
   * used this buffer.
   */
  for (VirtualBuffer *p : *m_uses_this_buffer_list)
    {
      --(p->m_remaining_dependencies);
      if (p->type() == assembled_buffer && p->m_remaining_dependencies == 0)
        {
          /* the buffers that make up the buffer p are ready,
           * so mark it as ready by called render_performed(),
           * but pass nullptr because such a buffer does not
           * blit.
           */
          ASTRALassert(p->m_image);
          p->render_performed(nullptr);
        }
    }

  for (VirtualBuffer *p : *m_dependency_list)
    {
      ++(p->m_users_that_completed_rendering);
      if (p->m_users_that_completed_rendering == p->m_uses_this_buffer_list->size())
        {
          p->m_image = nullptr;
        }
    }
}

astral::Renderer::Implement::VirtualBuffer*
astral::Renderer::VirtualBuffer::
add_dependency(unsigned int render_index)
{
  if (render_index != InvalidRenderValue)
    {
      return add_dependency(m_renderer.m_storage->virtual_buffer(render_index));
    }
  return nullptr;
}

astral::Renderer::Implement::VirtualBuffer*
astral::Renderer::VirtualBuffer::
add_dependency(VirtualBuffer &b, bool allow_unfinised)
{
  if (b.type() != degenerate_buffer && type() != degenerate_buffer)
    {
      ASTRALassert(m_dependency_list);
      ASTRALassert(b.m_uses_this_buffer_list);
      ASTRALhard_assert(allow_unfinised || b.finish_issued());

      ++m_remaining_dependencies;
      m_dependency_list->push_back(&b);
      b.m_uses_this_buffer_list->push_back(this);

      m_uses_shadow_map = m_uses_shadow_map || b.type() == shadowmap_buffer;
      return &b;
    }
  return nullptr;
}

astral::Renderer::Implement::VirtualBuffer*
astral::Renderer::VirtualBuffer::
add_dependency(RenderEncoderBase b)
{
  if (b.valid())
    {
      return add_dependency(b.virtual_buffer());
    }
  return nullptr;
}

void
astral::Renderer::VirtualBuffer::
add_dependency(const Image &image)
{
  add_dependency(image.ID());
}

astral::Renderer::Implement::VirtualBuffer*
astral::Renderer::VirtualBuffer::
add_dependency(const ImageID &ID)
{
  Image *p;

  p = m_renderer.m_engine->image_atlas().fetch_image(ID);
  if (p)
    {
      p->mark_in_use();
      return add_dependency(p->offscreen_render_index());
    }
  return nullptr;
}

astral::Renderer::Implement::VirtualBuffer*
astral::Renderer::VirtualBuffer::
add_dependency(const ShadowMapID &ID)
{
  ShadowMap *p;

  m_uses_shadow_map = true;
  p = m_renderer.m_engine->shadow_map_atlas().fetch_shadow_map(ID);
  if (p)
    {
      p->mark_in_use();
      return add_dependency(p->offscreen_render_index());
    }
  return nullptr;
}

astral::RenderValue<astral::ImageSampler>
astral::Renderer::VirtualBuffer::
create_image_sampler(enum filter_t filter) const
{
  ASTRALassert(m_image);

  ImageSampler im(*m_image, filter, mipmap_none);
  return m_renderer.create_value(im);
}

void
astral::Renderer::VirtualBuffer::
add_occluder(RenderValue<Transformation> tr, const Rect &rect)
{
  Renderer::Implement::DrawCommandList *p;

  p = command_list();
  ASTRALassert(!p || !finish_issued());

  if (!p)
    {
      return;
    }

  const ColorItemShader &shader(*m_renderer.m_default_shaders.m_dynamic_rect_shader);
  const VertexData &vertex_data(*m_renderer.m_dynamic_rect);
  RenderSupportTypes::Item<ColorItemShader> item(shader, vertex_data);
  Implement::DrawCommandVerticesShaders vertices_and_shaders(*m_renderer.m_storage, item);

  Implement::DrawCommand el(vertices_and_shaders);
  vecN<gvec4, DynamicRectShader::item_data_size> rect_data;

  DynamicRectShader::pack_item_data(rect, rect_data);
  el.m_render_values.m_transformation = tr;
  el.m_render_values.m_material = m_renderer.m_black_brush;
  el.m_render_values.m_item_data = m_renderer.create_item_data(rect_data, no_item_data_value_mapping);
  el.m_render_values.m_blend_mode = BackendBlendMode(blend_porter_duff_src_over, false);

  RenderSupportTypes::RectRegion region;
  region.m_rect = BoundingBox<float>(rect);

  p->add_occluder(el, &region, tr);
}

void
astral::Renderer::VirtualBuffer::
draw_generic_implement(RenderValue<Transformation> tr,
                       const RenderSupportTypes::RectRegion *region,
                       const Implement::DrawCommandVerticesShaders &item,
                       ItemData item_data,
                       const ItemMaterial &material,
                       BackendBlendMode blend_mode,
                       RenderValue<EmulateFramebufferFetch> framebuffer_copy,
                       enum mask_item_shader_clip_mode_t clip_mode)
{
  Renderer::Implement::DrawCommandList *p;

  p = command_list();
  ASTRALassert(!p || !finish_issued());
  ASTRALassert(!item_data.valid() || item_data.valid_for(RenderEncoderBase(this)));
  ASTRALassert(!material.m_material.brush().valid() || material.m_material.brush().valid_for(RenderEncoderBase(this)));
  ASTRALassert(!material.m_material.shader_data().valid() || material.m_material.shader_data().valid_for(RenderEncoderBase(this)));

  if (!p)
    {
      return;
    }

  if (material.m_clip.m_clip_element && !material.m_clip.m_clip_out && !material.m_clip.m_clip_element->mask_details())
    {
      /* having a Clip element with a null MaskDetails means
       * that the mask is effectively zero-coverage. We skip
       * the draw if it is to be clipped-in by that mask.
       */
      return;
    }
  ASTRALassert(item.m_shader_type == blend_mode.item_shader_type());
  ASTRALassert(p->renders_to_color_buffer() == (blend_mode.item_shader_type() == ItemShader::color_item_shader));
  ASTRALassert(p->renders_to_shadow_map() == (blend_mode.item_shader_type() == ItemShader::shadow_map_item_shader));
  ASTRALassert(p->renders_to_mask_buffer() == (blend_mode.item_shader_type() == ItemShader::mask_item_shader));

  ASTRALassert((type() == shadowmap_buffer) == (p->renders_to_shadow_map()));
  ASTRALassert(type() != degenerate_buffer && type() != assembled_buffer);
  ASTRALassert(m_dependency_list);

  Implement::DrawCommand el(item);
  unsigned int dependency_list_start(m_dependency_list->size());
  bool is_opaque;

  ASTRALassert(blend_mode.valid());

  el.m_render_values.m_transformation = tr;
  el.m_render_values.m_material_transformation = material.m_material_transformation_logical;
  el.m_render_values.m_material = material.m_material;
  el.m_render_values.m_item_data = item_data;
  el.m_render_values.m_blend_mode = blend_mode;
  el.m_render_values.m_framebuffer_copy = framebuffer_copy;
  el.m_render_values.m_mask_shader_clip_mode = clip_mode;

  /* We need to check against material.m_clip.m_clip_element->mask_details()
   * because a mask that rejects everything with m_clip_out as true is then
   * a mask that accepts everything.
   */
  if (material.m_clip.m_clip_element && material.m_clip.m_clip_element->mask_details())
    {
      const Implement::ClipElement *p;

      ASTRALassert(material.m_clip.m_clip_element->mask_details());
      ASTRALassert(material.m_clip.m_clip_element->mask_details()->m_mask);

      p = static_cast<const Implement::ClipElement*>(material.m_clip.m_clip_element.get());
      el.m_render_values.m_clip_mask = p->render_value();
      el.m_render_values.m_clip_mask_filter = material.m_clip.m_filter;
      el.m_render_values.m_clip_out = material.m_clip.m_clip_out;

      add_dependency(material.m_clip.m_clip_element->mask_details()->m_mask->ID());
    }

  add_dependency(m_renderer.m_backend->image_id(material.m_material.brush()));

  c_array<const ImageID> image_ids;
  c_array<const ShadowMapID> shadow_map_ids;

  image_ids = m_renderer.m_backend->image_id(item_data);
  for (const auto &id: image_ids)
    {
      add_dependency(id);
    }

  image_ids = m_renderer.m_backend->image_id(material.m_material.shader_data());
  for (const auto &id: image_ids)
    {
      add_dependency(id);
    }

  shadow_map_ids = m_renderer.m_backend->shadow_map_id(item_data);
  for (const auto &id: shadow_map_ids)
    {
      add_dependency(id);
    }

  shadow_map_ids = m_renderer.m_backend->shadow_map_id(material.m_material.shader_data());
  for (const auto &id: shadow_map_ids)
    {
      add_dependency(id);
    }

  if (framebuffer_copy.valid())
    {
      const EmulateFramebufferFetch &fbp(framebuffer_copy.value());

      ASTRALassert(fbp.m_image.valid());
      add_dependency(fbp.m_image.value().image_id());
    }

  Implement::DrawCommandList::DependencyList DL(m_dependency_list,
                                                dependency_list_start,
                                                m_dependency_list->size());

  is_opaque = m_renderer.pre_process_command(p->renders_to_color_buffer(), el);
  p->add_command(is_opaque, el, region, tr, DL);
}

void
astral::Renderer::VirtualBuffer::
draw_generic(RenderValue<Transformation> tr,
             const RenderSupportTypes::RectRegion *region,
             const RenderSupportTypes::Item<ColorItemShader> &item,
             const ItemMaterial &material,
             enum blend_mode_t blend_mode,
             RenderValue<EmulateFramebufferFetch> framebuffer_copy)
{
  if (item.empty())
    {
      return;
    }

  /* False is passed for BackendBlendMode ctor value of emits partial coverage
   * because draw_generic_implement() calls Renderer::Implement::pre_process_command()
   * which modifies the BackendBlendMode and setting the partial coverage
   * field correctly.
   */
  draw_generic_implement(tr, region,
                         Implement::DrawCommandVerticesShaders(*m_renderer.m_storage, item),
                         item.m_item_data,
                         material,
                         BackendBlendMode(blend_mode, false),
                         framebuffer_copy);
}

void
astral::Renderer::VirtualBuffer::
draw_generic(RenderValue<Transformation> tr,
             const RenderSupportTypes::RectRegion *region,
             const RenderSupportTypes::ColorItem &item,
             const ItemMaterial &material,
             enum blend_mode_t blend_mode,
             RenderValue<EmulateFramebufferFetch> framebuffer_copy)
{
  if (item.m_sub_items.empty())
    {
      return;
    }

  /* False is passed for BackendBlendMode ctor value of emits partial coverage
   * because draw_generic_implement() calls Renderer::Implement::pre_process_command()
   * which modifies the BackendBlendMode and setting the partial coverage
   * field correctly.
   */
  draw_generic_implement(tr, region,
                         Implement::DrawCommandVerticesShaders(*m_renderer.m_storage, item),
                         item.m_item_data,
                         material,
                         BackendBlendMode(blend_mode, false),
                         framebuffer_copy);
}

void
astral::Renderer::VirtualBuffer::
draw_generic(RenderValue<Transformation> tr,
             const RenderSupportTypes::Item<MaskItemShader> &item,
             const ItemMask &clip,
             enum mask_item_shader_clip_mode_t clip_mode)
{
  if (item.empty())
    {
      return;
    }

  ItemMaterial material(RenderValue<Brush>(), clip);
  draw_generic_implement(tr, nullptr,
                         Implement::DrawCommandVerticesShaders(*m_renderer.m_storage, item),
                         item.m_item_data,
                         material,
                         BackendBlendMode(BackendBlendMode::mask_mode_rendering),
                         RenderValue<EmulateFramebufferFetch>(),
                         clip_mode);
}

void
astral::Renderer::VirtualBuffer::
draw_generic(RenderValue<Transformation> tr,
             const RenderSupportTypes::Item<ShadowMapItemShader> &item)
{
  if (item.empty())
    {
      return;
    }

  draw_generic_implement(tr, nullptr,
                         Implement::DrawCommandVerticesShaders(*m_renderer.m_storage, item),
                         item.m_item_data,
                         ItemMaterial(),
                         BackendBlendMode(BackendBlendMode::shadowmap_mode_rendering),
                         RenderValue<EmulateFramebufferFetch>());
}

void
astral::Renderer::VirtualBuffer::
copy_commands(VirtualBuffer &src,
              RenderValue<Transformation> pixel_transformation_logical,
              const BoundingBox<float> &logical_bb,
              float logical_slack,
              bool delete_contained_cmds)
{
  Implement::DrawCommandList *src_p, *p;

  p = command_list();
  src_p = src.command_list();
  if (p && src_p && !logical_bb.empty())
    {
      unsigned int number_copied;

      if (src.m_clear_brush.valid())
        {
          /* add a draw command to render logical_bb
           * with the padding added with the named color.
           * Note that the rect is drawn without anti-aliasing
           * and the blend is porter_duff_src.
           */
          RenderEncoderBase encoder(this);
          Rect rect(logical_bb.as_rect());

          rect.outset(logical_slack, logical_slack);

          encoder.save_transformation();
          encoder.transformation(pixel_transformation_logical);
          encoder.draw_rect(rect, false, src.m_clear_brush, blend_porter_duff_src);
          encoder.restore_transformation();
        }

      ASTRALassert(m_dependency_list);
      number_copied = p->copy_commands(*src_p,
                                       pixel_transformation_logical,
                                       logical_bb, logical_slack,
                                       delete_contained_cmds,
                                       OnAddDependencyImpl(this));

      m_renderer.m_stats[number_commands_copied] += number_copied;
    }
}

astral::RenderSupportTypes::Proxy
astral::Renderer::VirtualBuffer::
generate_child_proxy(const RelativeBoundingBox &logical_rect,
                     unsigned int pixel_slack,
                     RenderScaleFactor scale_factor)
{
  RenderSupportTypes::Proxy return_value;

  if (!logical_rect.m_bb.empty() && m_type != degenerate_buffer)
    {
      Implement::ClipGeometryGroup clip_geometry;

      clip_geometry = child_clip_geometry(scale_factor, logical_rect, pixel_slack);
      return_value = m_renderer.m_storage->create_virtual_buffer_proxy(transformation(), clip_geometry);
    }

  return return_value;
}

astral::RenderEncoderBase
astral::Renderer::VirtualBuffer::
generate_buffer_from_proxy(RenderSupportTypes::Proxy proxy,
                           enum Implement::DrawCommandList::render_type_t render_type,
                           enum image_blit_processing_t blit_processing,
                           enum colorspace_t colorspace,
                           enum fill_rule_t fill_rule,
                           ImageCreationSpec image_create_spec)
{
  RenderEncoderBase return_value;

  if (!proxy.m_data || proxy.m_data->m_clip_geometry.bounding_geometry().image_size() == ivec2(0, 0))
    {
      /* return a degenerate VirtualBuffer, i.e. it has state tracking,
       * but all rendering commands are dropped.
       */
      return_value = m_renderer.m_storage->create_virtual_buffer(VB_TAG, ivec2(0, 0),
                                                                 render_type, blit_processing,
                                                                 colorspace, fill_rule,
                                                                 image_create_spec);
    }
  else
    {
      return_value = m_renderer.m_storage->create_virtual_buffer(VB_TAG, proxy.m_data->m_transformation,
                                                                 proxy.m_data->m_clip_geometry,
                                                                 render_type, blit_processing,
                                                                 colorspace, fill_rule,
                                                                 image_create_spec);
    }

  return_value.virtual_buffer().m_use_pixel_rect_tile_culling = m_use_pixel_rect_tile_culling;
  return_value.render_accuracy(m_render_accuracy);
  return_value.use_sub_ubers(m_use_sub_ubers);

  return return_value;
}

astral::RenderEncoderBase
astral::Renderer::VirtualBuffer::
generate_child_buffer(const RelativeBoundingBox &logical_rect,
                      enum Implement::DrawCommandList::render_type_t render_type,
                      enum image_blit_processing_t blit_processing,
                      enum colorspace_t colorspace,
                      enum fill_rule_t fill_rule,
                      unsigned int pixel_slack,
                      RenderScaleFactor in_scale_factor,
                      ImageCreationSpec image_create_spec)
{
  RenderEncoderBase return_value;

  if (!logical_rect.m_bb.empty() && (m_type != degenerate_buffer || !logical_rect.m_inherit_clipping_of_parent))
    {
      Implement::ClipGeometryGroup clip_geometry;

      clip_geometry = child_clip_geometry(in_scale_factor, logical_rect, pixel_slack);
      return_value = m_renderer.m_storage->create_virtual_buffer(VB_TAG, transformation(), clip_geometry,
                                                                 render_type, blit_processing,
                                                                 colorspace, fill_rule,
                                                                 image_create_spec);
    }
  else
    {
      /* create a degenerate child buffer */
      return_value = m_renderer.m_storage->create_virtual_buffer(VB_TAG, ivec2(0, 0),
                                                                 render_type, blit_processing,
                                                                 colorspace, fill_rule,
                                                                 image_create_spec);
      return_value.transformation(transformation());
    }

  return_value.virtual_buffer().m_use_pixel_rect_tile_culling = m_use_pixel_rect_tile_culling;
  return_value.render_accuracy(m_render_accuracy);
  return_value.use_sub_ubers(m_use_sub_ubers);

  return return_value;
}

astral::Renderer::Implement::ClipGeometryGroup
astral::Renderer::VirtualBuffer::
child_clip_geometry(RenderScaleFactor in_scale_factor, const RelativeBoundingBox &logical_rect, unsigned int pixel_slack)
{
  vec2 sf(in_scale_factor.m_scale_factor);

  if (in_scale_factor.m_relative)
    {
      sf *= scale_factor();
    }

  return Implement::ClipGeometryGroup(m_renderer, transformation(),
                                      m_transformation_stack.back().singular_values().x(),
                                      sf, logical_rect, clip_geometry(), pixel_slack);
}

void
astral::Renderer::VirtualBuffer::
issue_finish(void)
{
  if (finish_issued())
    {
      return;
    }

  /* create_backing_image() relies on m_finish_issue value */
  m_finish_issued = true;

  if (m_command_list)
    {
      m_command_list->unpause_snapshot();
    }

  if (type() == image_buffer)
    {
      create_backing_image();
    }

  /* We cannot allow for circular dependencies; if this
   * VirtualBuffer V depends on buffer D, we cannot allow
   * for D to use the contents of V. To guarantee that,
   * then D must be ended as well.
   */
  if (m_dependency_list)
    {
      for (VirtualBuffer *V : *m_dependency_list)
        {
          if (!V->finish_issued())
            {
              V->issue_finish();
            }
        }
    }
}

void
astral::Renderer::VirtualBuffer::
begin_pause_snapshot(void)
{
  ++m_pause_snapshot_counter;
  if (m_command_list)
    {
      m_command_list->pause_snapshot();
    }
}

void
astral::Renderer::VirtualBuffer::
end_pause_snapshot(void)
{
  ASTRALassert(m_pause_snapshot_counter >= 1);
  --m_pause_snapshot_counter;
  if (m_command_list && m_pause_snapshot_counter == 0)
    {
      m_command_list->unpause_snapshot();
    }
}

void
astral::Renderer::VirtualBuffer::
pause_snapshot_counter(int v)
{
  m_pause_snapshot_counter = v;
  if (m_command_list && m_pause_snapshot_counter == 0)
    {
      m_command_list->unpause_snapshot();
    }
}

astral::reference_counted_ptr<const astral::RenderClipElement>
astral::Renderer::VirtualBuffer::
clip_element(enum mask_type_t mask_type,
             enum mask_channel_t mask_channel)
{
  unsigned int idx;

  ASTRALassert(type() == image_buffer || type() == degenerate_buffer);
  ASTRALassert(finish_issued());

  /* MAYBE: we should have an interface that gives a RenderClipElement
   *        that supports both mask value types.
   */
  idx = mask_channel + number_mask_channel * mask_type;
  if (!m_clip_elements[idx])
    {
      Implement::ClipElement *p;

      ASTRALassert(finish_issued());
      p = m_renderer.m_storage->create_clip_element(clip_geometry().bounding_geometry(),
                                                    clip_geometry().token(),
                                                    fetch_image(), mask_type, mask_channel);
      m_clip_elements[idx] = p;
    }

  return m_clip_elements[idx];
}

/////////////////////////////////////////////////
// astral::Renderer::VirtualBuffer::SorterCommon methods
astral::Renderer::VirtualBuffer::SorterCommon::
SorterCommon(Renderer::Implement &renderer):
  m_buffers(renderer.m_storage->virtual_buffers())
{}

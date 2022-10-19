/*!
 * \file render_clip_node.cpp
 * \brief file render_clip_node.cpp
 *
 * Copyright 20220 by InvisionApp.
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

#include "render_clip_node.hpp"
#include "renderer_virtual_buffer.hpp"
#include "renderer_storage.hpp"

///////////////////////////////////////////////////
// astral::RenderClipNode::Backing::Begin methods
bool
astral::RenderClipNode::Backing::Begin::
init(enum clip_node_flags_t flags,
     const BoundingBox<float> &clip_in_rect,
     const BoundingBox<float> &clip_out_rect,
     RenderClipNode::Backing *out_encoders) const
{
  RenderScaleFactor scale_factor;

  ASTRALassert(!out_encoders->m_clip_in.valid());
  ASTRALassert(!out_encoders->m_clip_out.valid());

  if (flags & clip_node_clip_in)
    {
      out_encoders->m_clip_in = m_virtual_buffer->generate_child_buffer(clip_in_rect,
                                                                        m_virtual_buffer->colorspace(),
                                                                        clip_node_padding(), scale_factor,
                                                                        Renderer::VirtualBuffer::ImageCreationSpec());
    }

  if (flags & clip_node_clip_out)
    {
      out_encoders->m_clip_out = m_virtual_buffer->generate_child_buffer(clip_out_rect,
                                                                         m_virtual_buffer->colorspace(),
                                                                         clip_node_padding(), scale_factor,
                                                                         Renderer::VirtualBuffer::ImageCreationSpec());
    }

  out_encoders->m_has_clip_in = out_encoders->m_clip_in.valid()
    && out_encoders->m_clip_in.virtual_buffer().type() != VirtualBuffer::degenerate_buffer;

  out_encoders->m_has_clip_out = out_encoders->m_clip_out.valid()
    && out_encoders->m_clip_out.virtual_buffer().type() != VirtualBuffer::degenerate_buffer;

  if (!out_encoders->m_has_clip_in && !out_encoders->m_has_clip_out)
    {
      /* early out, nothing to draw */
      return false;
    }

  /* we want the computed rects in mask coordinates */
  BoundingBox<float> in_rect, out_rect;

  if (out_encoders->m_has_clip_in)
    {
      in_rect = inverse_transformation().apply_to_bb(out_encoders->m_clip_in.pixel_bounding_box());
      out_encoders->m_has_clip_in = out_encoders->m_has_clip_in && !in_rect.empty();
    }

  if (out_encoders->m_has_clip_out)
    {
      out_rect = inverse_transformation().apply_to_bb(out_encoders->m_clip_out.pixel_bounding_box());
      out_encoders->m_has_clip_out = out_encoders->m_has_clip_out && !out_rect.empty();
    }

  if (out_encoders->m_has_clip_in && out_encoders->m_has_clip_out)
    {
      out_encoders->m_non_empty_intersection = Rect::compute_intersection(in_rect.as_rect(), out_rect.as_rect(), &out_encoders->m_dual_clip_rect);
    }
  else
    {
      out_encoders->m_non_empty_intersection = false;
    }

  if (out_encoders->m_non_empty_intersection)
    {
      out_encoders->m_num_clip_in_rects = in_rect.as_rect().compute_difference(out_encoders->m_dual_clip_rect, &out_encoders->m_clip_in_rects);
      out_encoders->m_num_clip_out_rects = out_rect.as_rect().compute_difference(out_encoders->m_dual_clip_rect, &out_encoders->m_clip_out_rects);
    }
  else
    {
      if (out_encoders->m_has_clip_in)
        {
          out_encoders->m_num_clip_in_rects = 1u;
          out_encoders->m_clip_in_rects[0] = in_rect.as_rect();
        }
      else
        {
          out_encoders->m_num_clip_in_rects = 0u;
        }

      if (out_encoders->m_has_clip_out)
        {
          out_encoders->m_num_clip_out_rects = 1u;
          out_encoders->m_clip_out_rects[0] = out_rect.as_rect();
        }
      else
        {
          out_encoders->m_num_clip_out_rects = 0u;
        }
    }

  if (out_encoders->m_num_clip_in_rects == 0u
      && !out_encoders->m_has_clip_out
      && !out_encoders->m_non_empty_intersection)
    {
      return false;
    }

  return true;
}

astral::RenderClipNode
astral::RenderClipNode::Backing::Begin::
begin_clip_node_pixel_implement(enum blend_mode_t blend_mode,
                                enum clip_node_flags_t flags,
                                const BoundingBox<float> &pclip_in_bbox,
                                const BoundingBox<float> &pclip_out_bbox,
                                enum filter_t mask_filter,
                                const ItemMask &clip)
{
  RenderClipNode::Backing *return_value;
  AutoRestore restorer(*this);

  ASTRALassert(!finished());

  /* If m_mask does not have an image, then the draw is just literally
   * a blit of the clipped-out content; make it so.
   */
  return_value = renderer_implement().m_storage->create_clip_node();
  return_value->m_parent_encoder = *this;

  if (!m_mask->m_mask)
    {
      Transformation current_tr(transformation());
      unsigned int padding(clip_node_padding());
      vec2 rel_scale_factor(1.0f);

      if (flags & clip_node_clip_in)
        {
          /* Generate a degenerate rencoder for clip-in */
          return_value->m_clip_in = virtual_buffer().generate_child_buffer(BoundingBox<float>(), colorspace(), padding, rel_scale_factor,
                                                                           Renderer::VirtualBuffer::ImageCreationSpec());
        }

      if (m_clip_combine)
        {
          /* lack of mask on m_clip_combine means there really is nothing to draw */
          if (flags & clip_node_clip_out)
            {
              return_value->m_clip_out = virtual_buffer().generate_child_buffer(BoundingBox<float>(), colorspace(), padding, rel_scale_factor,
                                                                                Renderer::VirtualBuffer::ImageCreationSpec());
            }
        }
      else if (flags & clip_node_clip_out)
        {
          transformation(Transformation());
          return_value->m_clip_out = virtual_buffer().generate_child_buffer(pclip_out_bbox, colorspace(), padding, rel_scale_factor,
                                                                            Renderer::VirtualBuffer::ImageCreationSpec());
          if (blend_mode == number_blend_modes && return_value->m_clip_out.virtual_buffer().type() != VirtualBuffer::degenerate_buffer)
            {
              bool delete_contained_commands(false);
              return_value->m_clip_out.virtual_buffer().copy_commands(virtual_buffer(),
                                                                      RenderValue<Transformation>(),
                                                                      return_value->m_clip_out.pixel_bounding_box(), 0.0f,
                                                                      delete_contained_commands);
              blend_mode = blend_porter_duff_src;
            }

          return_value->m_blend_mode = blend_mode;
          return_value->m_has_clip_out = (return_value->m_clip_out.virtual_buffer().type() != VirtualBuffer::degenerate_buffer);
          return_value->m_blit_clip_out_content_only = return_value->m_has_clip_out;
        }

      return RenderClipNode(return_value);
    }

  /* TODO:
   *  - an interface or intelligence to detect if the tiles of the
   *    image behind clip and the image behind mask_buffer
   *    perfectly align. In this case, we can have the shaders used
   *    in blit_full_tiles() and blit_partial_tiles() pack what tile
   *    to use from clip instead of going through the generic
   *    masking code.
   */

  BoundingBox<float> clip_in_pixel_rect, clip_out_pixel_rect, clip_in_bbox;
  bool non_empty;
  Transformation current_tr(transformation());

  /* we will do all of our computation in the coordinate system of
   * m_mask->m_mask. Thus we need to move (0, 0) to m_mask->m_min_corner
   */
  ScaleTranslate mask_transformation_pixel(m_mask->m_mask_transformation_pixel);
  mask_transformation_pixel.m_translate += vec2(m_mask->m_min_corner);

  ScaleTranslate pixel_transformation_mask(mask_transformation_pixel.inverse());

  this->transformation(Transformation(pixel_transformation_mask));

  /* get the clipping boxes in mask coordinates */
  clip_in_bbox = mask_transformation_pixel.apply_to_bb(pclip_in_bbox);
  return_value->m_clip_out_bbox = mask_transformation_pixel.apply_to_bb(pclip_out_bbox);

  return_value->m_mask_bbox.clear();
  return_value->m_mask_bbox.union_point(m_mask->m_min_corner);
  return_value->m_mask_bbox.union_point(m_mask->m_min_corner + m_mask->m_size);

  if (m_clip_combine)
    {
      BoundingBox<float> bb;

      /* intersect clip_in_bbox against MI where
       *   MI = mask_transformation_pixel.apply_to_bb(CI) and
       *   CI = m_clip_combine.clip_in()->mask_details()->pixel_rect()
       */
      if (m_clip_combine->clip_in()->mask_details())
        {
          bb = m_clip_combine->clip_in()->mask_details()->pixel_rect();
          clip_in_bbox.intersect_against(mask_transformation_pixel.apply_to_bb(bb));
        }
      else
        {
          clip_in_bbox.clear();
        }

      /* intersect clip_out_bbox against MO where
       *    MO = mask_transformation_pixel.apply_to_bb(CO) and
       *    CO = m_clip_combine.clip_out()->mask_details()->pixel_rect()
       */
      if (m_clip_combine->clip_out()->mask_details())
        {
          bb = m_clip_combine->clip_out()->mask_details()->pixel_rect();
          return_value->m_clip_out_bbox.intersect_against(mask_transformation_pixel.apply_to_bb(bb));
        }
      else
        {
          return_value->m_clip_out_bbox.clear();
        }
    }
  else
    {
      /* Before running clip_node_rects(), we intersect
       * clip_in_bbox against the bounding box of mask
       */
      clip_in_bbox.intersect_against(return_value->m_mask_bbox);
    }

  non_empty = init(flags,
                   clip_in_bbox, return_value->m_clip_out_bbox,
                   return_value);

  if (!non_empty)
    {
      ASTRALassert(!return_value->m_clip_in.valid() || return_value->m_clip_in.degenerate());
      ASTRALassert(!return_value->m_clip_out.valid() || return_value->m_clip_out.degenerate());
      return RenderClipNode(return_value);
    }

  /* Groans: clip_node_rects will initialize the transformations
   *         of out_clip_in and out_clip_out to the current value
   *         of transformation(), but they should instead be initialized
   *         with the transformation at entry of this function.
   */
  if (return_value->m_has_clip_in)
    {
      clip_in_pixel_rect = return_value->m_clip_in.pixel_bounding_box();
      return_value->m_clip_in.transformation(current_tr);
    }

  if (return_value->m_has_clip_out)
    {
      clip_out_pixel_rect = return_value->m_clip_out.pixel_bounding_box();
      return_value->m_clip_out.transformation(current_tr);
    }

  /* When the blend_mode has the value number_blend_modes, that indicates
   * to render the content as if it was rendered directly to the surface.
   * To do that, we have the clip_out and clip_in absorb commands from
   * this encoder over their region and then blit the content with the
   * mode blend_porter_duff_src
   */
  if (blend_mode == number_blend_modes)
    {
      if (return_value->m_has_clip_in)
        {
          bool delete_contained_commands(false);
          return_value->m_clip_in.virtual_buffer().copy_commands(virtual_buffer(),
                                                                 RenderValue<Transformation>(),
                                                                 clip_in_pixel_rect, 0.0f,
                                                                 delete_contained_commands);
        }

      if (return_value->m_has_clip_out)
        {
          bool delete_contained_commands(false);
          return_value->m_clip_out.virtual_buffer().copy_commands(virtual_buffer(),
                                                                  RenderValue<Transformation>(),
                                                                  clip_out_pixel_rect, 0.0f,
                                                                  delete_contained_commands);
        }

      blend_mode = blend_porter_duff_src;
    }

  return_value->m_pixel_transformation_mask = pixel_transformation_mask;
  return_value->m_blend_mode = blend_mode;
  return_value->m_mask_filter = mask_filter;
  return_value->m_mask_image = m_mask->m_mask;
  return_value->m_clip_combine = m_clip_combine;
  return_value->m_mask_channel = m_mask->m_mask_channel;
  return_value->m_mask_type = m_mask->m_mask_type;
  return_value->m_additional_clipping = clip;

  /* TODO: when sparse offscreen color rendering is implemented,
   *       instead of having occluders, mark the tiles that could
   *       be visible instead of (or perhaps in addition to) marking
   *       the regions that are not visible.
   */

  c_array<const Rect> intersection_rects(&return_value->m_dual_clip_rect,
                                         return_value->m_non_empty_intersection ? 1 : 0);

  if (m_clip_combine)
    {
      if (return_value->m_has_clip_in)
        {
          add_tile_occluders(this->transformation_value(),
                             return_value->m_clip_in, return_value->clip_in_rects(),
                             m_clip_combine->clip_in_tile_range(),
                             Renderer::Implement::ClipCombineResult::full_clip_out_element);

          add_tile_occluders(this->transformation_value(),
                             return_value->m_clip_in, intersection_rects,
                             m_clip_combine->clip_in_tile_range(),
                             Renderer::Implement::ClipCombineResult::full_clip_out_element);
        }

      if (return_value->m_has_clip_out)
        {
          add_tile_occluders(this->transformation_value(),
                             return_value->m_clip_out, return_value->clip_out_rects(),
                             m_clip_combine->clip_out_tile_range(),
                             Renderer::Implement::ClipCombineResult::full_clip_in_element);

          add_tile_occluders(this->transformation_value(),
                             return_value->m_clip_out, intersection_rects,
                             m_clip_combine->clip_out_tile_range(),
                             Renderer::Implement::ClipCombineResult::full_clip_in_element);
        }
    }
  else
    {
      if (return_value->m_has_clip_in)
        {
          add_tile_occluders(this->transformation_value(),
                             return_value->m_clip_in, return_value->clip_in_rects(),
                             ImageMipElement::empty_element);

          add_tile_occluders(this->transformation_value(),
                             return_value->m_clip_in, intersection_rects,
                             ImageMipElement::empty_element);
        }

      if (return_value->m_has_clip_out)
        {
          add_tile_occluders(this->transformation_value(),
                             return_value->m_clip_out, return_value->clip_out_rects(),
                             ImageMipElement::white_element);

          add_tile_occluders(this->transformation_value(),
                             return_value->m_clip_out, intersection_rects,
                             ImageMipElement::white_element);
        }
    }

  return RenderClipNode(return_value);
}

void
astral::RenderClipNode::Backing::Begin::
add_tile_occluders(RenderValue<Transformation> pixel_transformation_mask,
                   RenderEncoderBase encoder, c_array<const Rect> rects,
                   vecN<range_type<unsigned int>, 2> tile_range,
                   enum Renderer::Implement::ClipCombineResult::combine_element_t tp)
{
  const ImageMipElement &mask(*m_mask->m_mask->mip_chain().front());

  ASTRALassert(m_clip_combine);
  ASTRALassert(encoder.valid());

  /* NOTE: we cycle through rects 1st because that array can be empty */
  for (const Rect &rect : rects)
    {
      BoundingBox<float> bb(rect);
      for (unsigned int y = tile_range.y().m_begin; y < tile_range.y().m_end; ++y)
        {
          for (unsigned int x = tile_range.x().m_begin; x < tile_range.x().m_end; ++x)
            {
              uvec2 L(x, y);
              if (m_clip_combine->tile_property(L).m_classification == tp)
                {
                  vec2 min_pt(mask.tile_location(L)), size(mask.tile_size(L));
                  BoundingBox<float> R(min_pt, min_pt + size);

                  R.intersect_against(bb);
                  if (!R.empty())
                    {
                      encoder.m_virtual_buffer->add_occluder(pixel_transformation_mask, R.as_rect());
                    }
                }
            }
        }
    }
}

void
astral::RenderClipNode::Backing::Begin::
add_tile_occluders(RenderValue<Transformation> pixel_transformation_mask,
                   RenderEncoderBase encoder, c_array<const Rect> rects,
                   enum ImageMipElement::element_type_t tp)
{
  const ImageMipElement &mask(*m_mask->m_mask->mip_chain().front());

  ASTRALassert(!m_clip_combine);
  ASTRALassert(encoder.valid());

  /* NOTE: we cycle through rects 1st because that array can be empty */
  for (const Rect &rect : rects)
    {
      BoundingBox<float> bb(rect);
      for (unsigned int k = 0, endk = mask.number_elements(tp); k < endk; ++k)
        {
          vec2 min_pt(mask.element_location(tp, k)), size(mask.element_size(tp, k));
          BoundingBox<float> R(min_pt, min_pt + size);

          R.intersect_against(bb);
          if (!R.empty())
            {
              encoder.m_virtual_buffer->add_occluder(pixel_transformation_mask, R.as_rect());
            }
        }
    }
}

///////////////////////////////////////////////////
// astral::RenderEncoderBase::Clipnode::End methods
void
astral::RenderClipNode::Backing::End::
end_clip_node_implement(void) const
{
  AutoRestore restorer(*this);

  ItemMaterial im_brush_clip_in, im_brush_clip_out;
  std::vector<RectT<int>> *clip_in_blit_rects(nullptr), *clip_out_blit_rects(nullptr);
  RenderValue<ScaleTranslate> clip_in_tr, clip_out_tr;
  Renderer::Implement::WorkRoom &W(*renderer_implement().m_workroom);

  ASTRALassert(!m_clip_node.m_end_clip_node_called);

  if (m_clip_node.m_clip_in.valid())
    {
      m_clip_node.m_clip_in.finish();
    }

  if (m_clip_node.m_clip_out.valid())
    {
      m_clip_node.m_clip_out.finish();
    }

  if (!m_clip_node.m_has_clip_in && !m_clip_node.m_has_clip_out)
    {
      return;
    }

  if (m_clip_node.m_end_clip_node_called)
    {
      return;
    }

  m_clip_node.m_end_clip_node_called = true;
  if (m_clip_node.m_blit_clip_out_content_only)
    {
      RenderValue<ImageSampler> im;
      ItemMaterial material;
      ScaleTranslate image_transformation_pixel;
      Brush brush;

      ASTRALassert(m_clip_node.m_has_clip_out);

      /* entire dawing takes place in pixel coordinates */
      transformation(Transformation());
      im = m_clip_node.m_clip_out.virtual_buffer().create_image_sampler(filter_nearest);
      image_transformation_pixel = m_clip_node.m_clip_out.image_transformation_pixel();

      brush
        .image(im)
        .image_transformation(create_value(Transformation(image_transformation_pixel)));

      material = ItemMaterial(create_value(brush), m_clip_node.m_additional_clipping);
      draw_rect(m_clip_node.m_clip_out.pixel_bounding_box().as_rect(), material, m_clip_node.m_blend_mode);

      return;
    }

  transformation(Transformation(m_clip_node.m_pixel_transformation_mask));

  /* the rects drawn are non-intersecting */
  begin_pause_snapshot();

  if (m_clip_node.m_has_clip_in)
    {
      VirtualBuffer &vb(m_clip_node.m_clip_in.virtual_buffer());
      RenderValue<ImageSampler> im;
      Transformation image_transformation;
      ScaleTranslate sc(vb.image_transformation_pixel() * m_clip_node.m_pixel_transformation_mask);

      im = vb.create_image_sampler(filter_nearest);
      image_transformation = Transformation(sc);
      clip_in_tr = create_value(sc);

      clip_in_blit_rects = renderer_implement().m_storage->allocate_rect_array();
      im_brush_clip_in = ItemMaterial(create_value(Brush()
                                                   .image(im)
                                                   .image_transformation(create_value(image_transformation))),
                                      m_clip_node.m_additional_clipping);
    }

  if (m_clip_node.m_has_clip_out)
    {
      VirtualBuffer &vb(m_clip_node.m_clip_out.virtual_buffer());
      RenderValue<ImageSampler> im;
      Transformation image_transformation;
      ScaleTranslate sc(vb.image_transformation_pixel() * m_clip_node.m_pixel_transformation_mask);

      im = vb.create_image_sampler(filter_nearest);
      image_transformation = Transformation(sc);
      clip_out_tr = create_value(sc);

      clip_out_blit_rects = renderer_implement().m_storage->allocate_rect_array();
      im_brush_clip_out = ItemMaterial(create_value(Brush()
                                                    .image(im)
                                                    .image_transformation(create_value(image_transformation))),
                                       m_clip_node.m_additional_clipping);
    }

  ASTRALassert(W.m_clip_in.empty());
  ASTRALassert(W.m_clip_out.empty());
  ASTRALassert(W.m_intersection.empty());

  const Renderer::Implement::ClipCombineResult *clip_combine;
  clip_combine = static_cast<const Renderer::Implement::ClipCombineResult*>(m_clip_node.m_clip_combine.get());

  if (clip_combine)
    {
      ClippedTileCollector collector;

      /* The clip-in rects are content that only has the clip-in
       * content hitting it, thus clip-out contribution is zero.
       */
      collector = ClippedTileCollector(nullptr);
      collector[Renderer::Implement::ClipCombineResult::full_clip_in_element] = &W.m_clip_in.m_full_tiles;
      collector[Renderer::Implement::ClipCombineResult::partial_clip_in_element] = &W.m_clip_in.m_color_tiles;
      collector[Renderer::Implement::ClipCombineResult::mixed_combine_element] = &W.m_clip_in.m_color_tiles;
      for (const Rect &rect : m_clip_node.clip_in_rects())
        {
          compute_tiles(rect,
                        clip_combine->clip_in_tile_range(),
                        collector);
        }

      /* The clip-out rects are content that only has the clip-out
       * content hitting it, thus clip-in contribution is zero.
       */
      collector = ClippedTileCollector(nullptr);
      collector[Renderer::Implement::ClipCombineResult::full_clip_out_element] = &W.m_clip_out.m_full_tiles;
      collector[Renderer::Implement::ClipCombineResult::partial_clip_out_element] = &W.m_clip_out.m_color_tiles;
      collector[Renderer::Implement::ClipCombineResult::mixed_combine_element] = &W.m_clip_out.m_color_tiles;
      for (const Rect &rect : m_clip_node.clip_out_rects())
        {
          compute_tiles(rect,
                        clip_combine->clip_out_tile_range(),
                        collector);
        }

      /* for the intersect rect, walk the entire tile-range
       *  to add rects. There are six classes of rects
       */
      collector = ClippedTileCollector(nullptr);
      collector[Renderer::Implement::ClipCombineResult::full_clip_in_element] = &W.m_clip_in.m_full_tiles;
      collector[Renderer::Implement::ClipCombineResult::partial_clip_in_element] = &W.m_clip_in.m_color_tiles;
      collector[Renderer::Implement::ClipCombineResult::full_clip_out_element] = &W.m_clip_out.m_full_tiles;
      collector[Renderer::Implement::ClipCombineResult::partial_clip_out_element] = &W.m_clip_out.m_color_tiles;
      collector[Renderer::Implement::ClipCombineResult::mixed_combine_element] = &W.m_intersection;
      if (m_clip_node.m_non_empty_intersection)
        {
          compute_tiles(m_clip_node.m_dual_clip_rect,
                        clip_combine->tile_range_entire(),
                        collector);
        }
    }
  else
    {
      for (const Rect &rect : m_clip_node.clip_in_rects())
        {
          compute_tiles(rect,
                        &W.m_clip_in.m_color_tiles, //partial mask-tiles are partial clip-in
                        &W.m_clip_in.m_full_tiles,  //full mask-tiles are full clip-in
                        nullptr                     //empty mask-tiles have no clip-in
                        );
        }

      for (const Rect &rect : m_clip_node.clip_out_rects())
        {
          compute_tiles(rect,
                        &W.m_clip_out.m_color_tiles, //partial mask-tiles are partial clip-out
                        nullptr,                     //full mask-tiles have no clip-out
                        &W.m_clip_out.m_full_tiles   //empty mask-tiles are full clip-out
                        );
        }

      if (m_clip_node.m_non_empty_intersection)
        {
          compute_tiles(m_clip_node.m_dual_clip_rect,
                        &W.m_intersection,          //partial mask-tiles are mix tiles
                        &W.m_clip_in.m_full_tiles,  //full mask-tiles are full clip-in
                        &W.m_clip_out.m_full_tiles  //empty mask-tiles are full clip-out
                        );
        }
    }

  /* now draw the rects in an order to reduce shader changes:
   *  - blit_full_tiles() draws via draw_rect(rect, false, brush)
   *  - blit_partial_tiles() draws via MaskedRectShader
   *  - BlitMaskTileShader is used for partial color tiles of intersection
   */
  blit_full_tiles(make_c_array(W.m_clip_in.m_full_tiles), im_brush_clip_in, m_clip_node.m_blend_mode);
  blit_full_tiles(make_c_array(W.m_clip_out.m_full_tiles), im_brush_clip_out, m_clip_node.m_blend_mode);

  if (!clip_combine && m_clip_node.m_has_clip_out)
    {
      /* We need to draw the portion of the clip-out rect
       * that is not covered by the bounding box of mask_buffer;
       * note that this portion does not intersect the clip_in
       * region because the clip_in region as already been
       * intersected against the mask buffer region.
       */
      unsigned int num_rects;
      vecN<Rect, 4> rects;
      BoundingBox<float> clip_bb;

      num_rects = m_clip_node.m_clip_out_bbox.as_rect().compute_difference(m_clip_node.m_mask_bbox.as_rect(), rects);
      if (num_rects > 0)
        {
          for (unsigned int i = 0; i < num_rects; ++i)
            {
              draw_rect(rects[i], false, im_brush_clip_out, m_clip_node.m_blend_mode);
            }

          if (clip_out_blit_rects)
            {
              add_raw_blit_rects(clip_out_blit_rects, rects, clip_out_tr.value());
            }
        }
    }

  if (clip_out_blit_rects)
    {
      /* we can skip a fair amount of blitting to the image atlas
       * by only blitting those regions that are either partially
       * covered or not covered at all by the mask.
       */
      add_blit_rects(clip_out_blit_rects, make_c_array(W.m_clip_out.m_full_tiles), clip_out_tr.value());
      add_blit_rects(clip_out_blit_rects, make_c_array(W.m_clip_out.m_color_tiles), clip_out_tr.value());
      add_blit_rects(clip_out_blit_rects, make_c_array(W.m_intersection), clip_out_tr.value());

      m_clip_node.m_clip_out.virtual_buffer().specify_blit_rects(clip_out_blit_rects);
    }

  if (clip_in_blit_rects)
    {
      /* we can skip a fair amount of blitting to the image atlas
       * by only blitting those regions that are either partially
       * covered or not covered at all by the mask.
       */
      add_blit_rects(clip_in_blit_rects, make_c_array(W.m_clip_in.m_full_tiles), clip_in_tr.value());
      add_blit_rects(clip_in_blit_rects, make_c_array(W.m_clip_in.m_color_tiles), clip_in_tr.value());
      add_blit_rects(clip_in_blit_rects, make_c_array(W.m_intersection), clip_in_tr.value());

      m_clip_node.m_clip_in.virtual_buffer().specify_blit_rects(clip_in_blit_rects);
    }

  enum mask_type_t clip_in_mask_type, clip_out_mask_type;
  enum mask_channel_t clip_in_mask_channel, clip_out_mask_channel;

  if (clip_combine)
    {
      const MaskDetails *clip_in_mask, *clip_out_mask;

      clip_in_mask = clip_combine->clip_in()->mask_details();
      clip_out_mask = clip_combine->clip_out()->mask_details();

      if (clip_in_mask)
        {
          clip_in_mask_type = clip_in_mask->m_mask_type;
          clip_in_mask_channel = clip_in_mask->m_mask_channel;
        }
      else
        {
          ASTRALassert(W.m_clip_in.m_color_tiles.empty());

          /* invalid values that should not get used */
          clip_in_mask_type = number_mask_type;
          clip_in_mask_channel = number_mask_channel;
        }

      if (clip_out_mask)
        {
          clip_out_mask_type = clip_out_mask->m_mask_type;
          clip_out_mask_channel = clip_out_mask->m_mask_channel;
        }
      else
        {
          ASTRALassert(W.m_clip_out.m_color_tiles.empty());

          /* invalid values that should not get used */
          clip_out_mask_type = number_mask_type;
          clip_out_mask_channel = number_mask_channel;
        }
    }
  else
    {
      clip_in_mask_type = clip_out_mask_type = m_clip_node.m_mask_type;
      clip_in_mask_channel = clip_out_mask_channel = m_clip_node.m_mask_channel;
    }

  blit_partial_tiles(make_c_array(W.m_clip_in.m_color_tiles),
                     m_clip_node.m_mask_filter, false,
                     clip_in_mask_type, clip_in_mask_channel,
                     im_brush_clip_in, m_clip_node.m_blend_mode);

  blit_partial_tiles(make_c_array(W.m_clip_out.m_color_tiles),
                     m_clip_node.m_mask_filter, !clip_combine, //only invert if clip_node_pixel() is against a raw mask
                     clip_out_mask_type, clip_out_mask_channel,
                     im_brush_clip_out, m_clip_node.m_blend_mode);

  if (!W.m_intersection.empty())
    {
      uint32_t clip_image_bits;
      PackedImageMipElement clip_in_image, clip_out_image;
      const ImageMipElement &mask_mip(*m_clip_node.m_mask_image->mip_chain().front());
      vecN<ImageID, 3> deps(m_clip_node.m_mask_image->ID(),
                            im_brush_clip_in.m_material.brush().value().m_image.value().image_id(),
                            im_brush_clip_out.m_material.brush().value().m_image.value().image_id());
      vecN<gvec4, BlitMaskTileShader::item_data_size> item_data;
      const MaterialShader *shader;

      shader = (clip_combine) ?
        &render_engine().default_shaders().m_blit_mask_tile_shader.shader(BlitMaskTileShader::clip_combine_variant) :
        &render_engine().default_shaders().m_blit_mask_tile_shader.shader(BlitMaskTileShader::mask_details_variant);

      ASTRALassert(m_clip_node.m_clip_in.image());
      ASTRALassert(!m_clip_node.m_clip_in.image()->mip_chain().empty());
      ASTRALassert(m_clip_node.m_clip_in.image()->mip_chain().front());

      ASTRALassert(m_clip_node.m_clip_out.image());
      ASTRALassert(!m_clip_node.m_clip_out.image()->mip_chain().empty());
      ASTRALassert(m_clip_node.m_clip_out.image()->mip_chain().front());

      clip_image_bits = ImageSamplerBits::value(filter_nearest, mipmap_none, 0, colorspace());
      clip_in_image = PackedImageMipElement(clip_node_padding(), *m_clip_node.m_clip_in.image()->mip_chain().front(), clip_image_bits);
      clip_out_image = PackedImageMipElement(clip_node_padding(), *m_clip_node.m_clip_out.image()->mip_chain().front(), clip_image_bits);

      for (const auto &R : W.m_intersection)
        {
          if (clip_combine)
            {
              BlitMaskTileShader::pack_item_data(clip_in_tr,
                                                 clip_in_image,
                                                 clip_out_tr,
                                                 clip_out_image,
                                                 mask_mip, R.m_tile,
                                                 clip_combine->mask_type(),
                                                 clip_combine->clip_in_channel(),
                                                 clip_combine->clip_out_channel(),
                                                 m_clip_node.m_mask_filter, item_data);
            }
          else
            {
              BlitMaskTileShader::pack_item_data(clip_in_tr,
                                                 clip_in_image,
                                                 clip_out_tr,
                                                 clip_out_image,
                                                 mask_mip, R.m_tile,
                                                 m_clip_node.m_mask_type,
                                                 m_clip_node.m_mask_channel,
                                                 m_clip_node.m_mask_filter,
                                                 item_data);
            }

          Material material(*shader, create_item_data(item_data, BlitMaskTileShader::intrepreted_value_map(), deps));

          draw_rect(R.m_rect, false, ItemMaterial(material, m_clip_node.m_additional_clipping), m_clip_node.m_blend_mode);
        }
    }

  /* clear the arrays for later use */
  W.m_clip_in.clear();
  W.m_clip_out.clear();
  W.m_intersection.clear();

  end_pause_snapshot();
}

void
astral::RenderClipNode::Backing::End::
compute_tiles(const Rect &rect,
              const vecN<range_type<unsigned int>, 2> &tile_range,
              const ClippedTileCollector &out_tiles) const
{
  const Renderer::Implement::ClipCombineResult *clip_combine;
  const ImageMipElement &mask(*m_clip_node.m_mask_image->mip_chain().front());
  BoundingBox<float> bb(rect);

  clip_combine = static_cast<const Renderer::Implement::ClipCombineResult*>(m_clip_node.m_clip_combine.get());
  bb.intersect_against(m_clip_node.m_mask_bbox);

  ASTRALassert(clip_combine);
  for (unsigned int y = tile_range.y().m_begin; y < tile_range.y().m_end; ++y)
    {
      for (unsigned int x = tile_range.x().m_begin; x < tile_range.x().m_end; ++x)
        {
          uvec2 L(x, y);
          vec2 min_pt(mask.tile_location(L)), size(mask.tile_size(L));
          BoundingBox<float> R(min_pt, min_pt + size);
          enum Renderer::Implement::ClipCombineResult::combine_element_t tp;

          R.intersect_against(bb);
          tp = clip_combine->tile_property(L).m_classification;

          if (!R.empty() && out_tiles[tp])
            {
              ClippedTile V;

              V.m_rect = R.as_rect();
              V.m_tile = L;
              out_tiles[tp]->push_back(V);
            }
        }
    }
}

void
astral::RenderClipNode::Backing::End::
compute_tiles(const Rect &rect,
              std::vector<ClippedTile> *out_color_tiles,
              std::vector<ClippedTile> *out_full_tiles,
              std::vector<ClippedTile> *out_empty_tiles) const
{
  const float recip_tile_size_without_padding = 1.0f / static_cast<float>(ImageAtlas::tile_size_without_padding);
  BoundingBox<float> bb;
  uvec2 min_tile, max_tile;
  const ImageMipElement &mask(*m_clip_node.m_mask_image->mip_chain().front());
  vecN<std::vector<ClippedTile>*, ImageMipElement::number_element_type> out_tiles;

  ASTRALassert(!m_clip_node.m_clip_combine);
  bb = BoundingBox<float>(rect);

  /* intersect bb against the rect [0, X]x[0, Y]
   * where (X, Y) is the size of mask. The caller
   * guarantees that rect is already in coordinates
   * of mask.m_mask (i.e. mask.m_min_corner has
   * already been taken into account).
   */
  bb.intersect_against(m_clip_node.m_mask_bbox);

  out_tiles[ImageMipElement::color_element] = out_color_tiles;
  out_tiles[ImageMipElement::white_element] = out_full_tiles;
  out_tiles[ImageMipElement::empty_element] = out_empty_tiles;

  if (bb.empty())
    {
      /* empty intersection, nothing left to do */
      return;
    }

  /* get the tile bounds from mapped_pixel_rect; Recall that
   * the n'th tile covers [-P + n * Z, (n + 1) * Z + P] where
   * Z = ImageAtlas::tile_size_without_padding and
   * P = ImageAtlas::tile_padding; thus the tiles we can
   * view as the disjoint covering [n * Z, (n + 1) * Z].
   *
   * The only corner case we need to handle is if the
   * mask has zero index layers; in this case there is only
   * one tile, the entire ImageMipElement
   */
  if (mask.number_index_levels() == 0)
    {
      min_tile = max_tile = uvec2(0, 0);
    }
  else
    {
      for (unsigned int c = 0; c < 2; ++c)
        {
          min_tile[c] = static_cast<unsigned int>(t_max(0.0f, bb.min_point()[c] * recip_tile_size_without_padding));
          max_tile[c] = static_cast<unsigned int>(t_max(0.0f, bb.max_point()[c] * recip_tile_size_without_padding));
          max_tile[c] = t_min(max_tile[c], mask.tile_count()[c] - 1u);
        }
    }

  /* walk the tile range, adding it the output */
  for (unsigned int ty = min_tile.y(); ty <= max_tile.y(); ++ty)
    {
      for (unsigned int tx = min_tile.x(); tx <= max_tile.x(); ++tx)
        {
          uvec2 L(tx, ty);
          vec2 min_pt(mask.tile_location(L)), size(mask.tile_size(L));
          BoundingBox<float> R(min_pt, min_pt + size);
          enum ImageMipElement::element_type_t tp;

          R.intersect_against(bb);
          tp = mask.tile_type(L);

          if (!R.empty() && out_tiles[tp])
            {
              ClippedTile V;

              V.m_rect = R.as_rect();
              V.m_tile = L;
              out_tiles[tp]->push_back(V);
            }
        }
    }
}

void
astral::RenderClipNode::Backing::End::
add_raw_blit_rects(std::vector<astral::RectT<int>> *blit_rects,
                   astral::c_array<const astral::Rect> tiles,
                   astral::ScaleTranslate blit_transformation_tile)
{
  for (const astral::Rect &tile : tiles)
    {
      astral::BoundingBox<float> frect(tile);
      astral::BoundingBox<float> bb(blit_transformation_tile.apply_to_bb(frect));
      astral::RectT<int> ibb(bb.as_rect());

      if (ibb.m_max_point.x() < bb.as_rect().m_max_point.x())
        {
          ++ibb.m_max_point.x();
        }

      if (ibb.m_max_point.y() < bb.as_rect().m_max_point.y())
        {
          ++ibb.m_max_point.y();
        }

      blit_rects->push_back(ibb);
    }
}

void
astral::RenderClipNode::Backing::End::
add_blit_rects(std::vector<RectT<int>> *blit_rects,
               c_array<const ClippedTile> tiles,
               ScaleTranslate blit_transformation_tile)
{
  for (const ClippedTile &tile : tiles)
    {
      BoundingBox<float> frect(tile.m_rect);
      BoundingBox<float> bb(blit_transformation_tile.apply_to_bb(frect));
      RectT<int> ibb(bb.as_rect());

      if (ibb.m_max_point.x() < bb.as_rect().m_max_point.x())
        {
          ++ibb.m_max_point.x();
        }

      if (ibb.m_max_point.y() < bb.as_rect().m_max_point.y())
        {
          ++ibb.m_max_point.y();
        }

      blit_rects->push_back(ibb);
    }
}

void
astral::RenderClipNode::Backing::End::
blit_full_tiles(c_array<const ClippedTile> tiles,
                const ItemMaterial &material,
                enum blend_mode_t blend_mode) const
{
  for (const auto &R : tiles)
    {
      draw_rect(R.m_rect, false, material, blend_mode);
    }
}

void
astral::RenderClipNode::Backing::End::
blit_partial_tiles(c_array<const ClippedTile> tiles,
                   enum filter_t mask_filter,
                   bool invert_coverage,
                   enum mask_type_t mask_type,
                   enum mask_channel_t mask_channel,
                   const ItemMaterial &material,
                   enum blend_mode_t blend_mode) const
{
  if (tiles.empty())
    {
      /* if there are no partial tiles, then it is possible
       * for the m_clip_node.m_mask_image to be null; so we
       * need to early out before we dereference it.
       */
      return;
    }

  ASTRALassert(m_clip_node.m_mask_image);
  ASTRALassert(!m_clip_node.m_mask_image->mip_chain().empty());
  ASTRALassert(m_clip_node.m_mask_image->mip_chain().front());

  const Image &mask_image(*m_clip_node.m_mask_image);
  const ImageMipElement &mask_mip(*mask_image.mip_chain().front());
  vecN<ImageID, 1> deps(mask_image.ID());
  enum mask_post_sampling_mode_t post_sampling_mode;

  post_sampling_mode = (invert_coverage) ?
    mask_post_sampling_mode_invert :
    mask_post_sampling_mode_direct;

  for (const auto &R : tiles)
    {
      vecN<gvec4, MaskedRectShader::item_data_size> item_data;
      uvec2 tile_id(R.m_tile);
      Rect rgn;

      rgn = MaskedRectShader::pack_item_data(mask_mip, tile_id,
                                             R.m_rect,
                                             post_sampling_mode,
                                             mask_type,
                                             mask_channel,
                                             mask_filter,
                                             item_data);
      if (rgn.width() > 0.0f && rgn.height() > 0.0f)
        {
          RectItem item(*default_shaders().m_masked_rect_shader,
                        create_item_data(item_data, no_item_data_value_mapping, deps));

          draw_custom_rect(rgn, item, material, blend_mode);
        }
    }
}

/////////////////////////////////////
// astral::RenderClipNode methods
astral::RenderEncoderImage
astral::RenderClipNode::
clip_in(void) const
{
  return m_backing ?
    m_backing->m_clip_in :
    RenderEncoderImage();
}

astral::RenderEncoderImage
astral::RenderClipNode::
clip_out(void) const
{
  return m_backing ?
    m_backing->m_clip_out :
    RenderEncoderImage();
};

astral::RenderEncoderBase
astral::RenderClipNode::
parent_encoder(void) const
{
  return m_backing ?
    m_backing->m_parent_encoder :
    RenderEncoderBase();
};

bool
astral::RenderClipNode::
ended(void) const
{
  return m_backing ?
    m_backing->m_end_clip_node_called :
    true;
};

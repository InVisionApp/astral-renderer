/*!
 * \file renderer_filler_line_clipping.cpp
 * \brief file renderer_filler_line_clipping.cpp
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
#include "renderer_storage.hpp"
#include "renderer_virtual_buffer.hpp"
#include "renderer_streamer.hpp"
#include "renderer_filler_common_clipper.hpp"

/////////////////////////////////////////////////////////
// astral::Renderer::Implement::Filler::CommonClipper::WindingRect methods
astral::c_array<const astral::VertexStreamerBlock>
astral::Renderer::Implement::Filler::CommonClipper::WindingRect::
blocks(Filler::CommonClipper &filler, bool last_rect_X, bool last_rect_Y, bool increase_winding)
{
  if (m_ready)
    {
      ASTRALassert(last_rect_X == m_last_rect_X);
      ASTRALassert(last_rect_Y == m_last_rect_Y);
      ASTRALassert(increase_winding == m_increase_winding);
      return filler.m_renderer.m_vertex_streamer->blocks(m_vert_blocks[FillSTCShader::pass_contour_stencil]);
    }

  m_ready = true;
  m_last_rect_X = last_rect_X;
  m_last_rect_Y = last_rect_Y;
  m_increase_winding = increase_winding;

  vecN<vec2, 5> pts;
  c_array<const std::pair<FillSTCShader::ConicTriangle, bool>> no_tris;
  c_array<const FillSTCShader::LineSegment> no_segs;
  vec2 sz;

  /* The rect passed to the encoder is not transformed at all. Now,
   * the size of the rect without padding is given by sz. We need
   * to add to the size, the size of the padding. This is the correct
   * value because the area rasterized is those pixels (x, y) which
   * sastisfy
   *  0 < x + A < sz.x()
   *  0 < y + B < sz.y()
   *
   * where A and B are the offset from the min-min corner of the pixel
   * to the sample point. For single sampling rendering, A = B = 0.5.
   */
  sz.x() = ImageAtlas::tile_size;
  sz.y() = ImageAtlas::tile_size;

  pts[0] = vec2(0.0f, 0.0f);
  pts[1] = vec2(sz.x(), 0.0f);
  pts[2] = vec2(sz.x(), sz.y());
  pts[3] = vec2(0.0f, sz.y());
  pts[4] = pts[0];

  if (!m_increase_winding)
    {
      /* matches the convention of clockwise increases
       * the winding number
       */
      std::reverse(pts.begin(), pts.end());
    }

  ASTRALassert(filler.m_builder.empty());
  filler.m_builder.add_raw(pts, no_tris, no_segs);
  filler.create_blocks_from_builder(FillSTCShader::pass_contour_stencil, &m_vert_blocks);
  filler.m_builder.clear();

  /* only the contour stencil pass should have anything */
  return filler.m_renderer.m_vertex_streamer->blocks(m_vert_blocks[FillSTCShader::pass_contour_stencil]);
}

/////////////////////////////////////////////////
// astral::Renderer::Implement::Filler::CommonClipper methods
astral::c_string
astral::Renderer::Implement::Filler::CommonClipper::
label(enum side_t side)
{
  static c_string values[4] =
    {
      [minx_side] = "minx_side",
      [miny_side] = "miny_side",
      [maxx_side] = "maxx_side",
      [maxy_side] = "maxy_side",
    };

  return values[side];
}

int
astral::Renderer::Implement::Filler::CommonClipper::
side_value(int V, enum side_t s)
{
  switch (s)
    {
    case minx_side:
      return minx_side_value(V);

    case maxx_side:
      return maxx_side_value(V);

    case miny_side:
      return miny_side_value(V);

    case maxy_side:
      return maxy_side_value(V);
    }

  ASTRALassert(!"Bad side_t value");
  return 0;
}

void
astral::Renderer::Implement::Filler::CommonClipper::
set_subrect_values(ivec2 total_size, const ClipElement *clip_element)
{
  m_reciprocal_elementary_rect_size = 1.0f / static_cast<float>(ImageAtlas::tile_size_without_padding);

  /* Note that we pass the size of the rectangle a single padding,
   * this is because we render post padding as well. The nature
   * of Image includes pre-padding on the images and when
   * clipping we already have that the number of tiles is atleast
   * two in both directions.
   */
  if (clip_element)
    {
      const ImageMipElement *mip;

      mip = clip_element->mip_front();

      m_total_size = ivec2(clip_element->image()->size());
      m_number_elementary_rects = ivec2(mip->tile_count());
    }
  else
    {
      m_total_size = total_size;
      m_number_elementary_rects = ImageAtlas::tile_count(m_total_size);
    }

  unsigned int cnt;

  ASTRALassert(m_stc_builders_for_rects.empty());
  cnt = m_number_elementary_rects.x() * m_number_elementary_rects.y();
  m_stc_builders_for_rects.resize(cnt, nullptr);
}

astral::Renderer::Implement::STCData::BuilderSet*
astral::Renderer::Implement::Filler::CommonClipper::
stc_builder_for_rect(int X, int Y)
{
  unsigned int id(subrect_id(X, Y));

  ASTRALassert(id < m_stc_builders_for_rects.size());
  if (!m_stc_builders_for_rects[id])
    {
      m_stc_builders_for_rects[id] = m_stc_builder_pool.allocate();
      m_stc_builders_for_rects[id]->start();
    }

  return m_stc_builders_for_rects[id];
}

void
astral::Renderer::Implement::Filler::CommonClipper::
create_blocks_from_builder(FillSTCShader::PassSet pass_set,
                           vecN<range_type<unsigned int>, FillSTCShader::pass_count> *out_vert_blocks)
{
  /* Get the Storage requirements for the contour */
  vecN<unsigned int, FillSTCShader::pass_count> num_verts;
  unsigned int num_static_size2;
  unsigned int num_static_size3;

  m_builder.storage_requirement(pass_set, &num_verts,
                                &num_static_size2,
                                &num_static_size3);

  /* allocate streaming room for the contour */
  vecN<range_type<unsigned int>, FillSTCShader::pass_count> &vert_blocks(*out_vert_blocks);
  vecN<c_array<const VertexStreamerBlock>, FillSTCShader::pass_count> vert_blocks_p;
  range_type<unsigned int> static_size2_blocks, static_size3_blocks;

  /* request the room first */
  for (unsigned int p = 0; p < FillSTCShader::pass_count; ++p)
    {
      vert_blocks[p] = m_renderer.m_vertex_streamer->request_blocks(*m_renderer.m_engine, num_verts[p]);
    }

  static_size2_blocks = m_renderer.m_static_streamer->request_blocks(*m_renderer.m_engine, num_static_size2, 2);
  static_size3_blocks = m_renderer.m_static_streamer->request_blocks(*m_renderer.m_engine, num_static_size3, 3);

  /* Use m_builder to set the values */
  vert_blocks_p = m_renderer.m_vertex_streamer->blocks(vert_blocks);
  FillSTCShader::pack_render_data(m_builder, pass_set, vert_blocks_p,
                                  m_renderer.m_static_streamer->blocks(static_size2_blocks),
                                  m_renderer.m_static_streamer->blocks(static_size3_blocks));
}

void
astral::Renderer::Implement::Filler::CommonClipper::
cleanup_common(void)
{
  m_builder.clear();
  m_builder_helper.clear();
  m_stc_builder_pool.clear();
  m_stc_builders_for_rects.clear();
  for (auto &w : m_winding_rects)
    {
      w.reset();
    }
}

astral::reference_counted_ptr<const astral::Image>
astral::Renderer::Implement::Filler::CommonClipper::
create_sparse_image_from_rects(ItemData item_data,
                               const ClipElement *clip_element,
                               enum clip_combine_mode_t clip_combine_mode,
                               TileTypeTable *out_clip_combine_tile_data)
{
  bool combine_clip_with_full_tile, combine_clip_with_empty_tile;

  if (clip_combine_mode == clip_combine_both)
    {
      /* If we need both clip-in and clip-out, then if
       * clip_element is present, we need to combine
       * the clip_element's tiles with empty and full
       * tiles of the path fill.
       */
      combine_clip_with_full_tile = (clip_element != nullptr);
      combine_clip_with_empty_tile = (clip_element != nullptr);
    }
  else
    {
      /* Empty tiles are always discarded if the
       * path fill is empty and we need only clip-in
       * by the path-fill.
       */
      combine_clip_with_empty_tile = false;

      if (clip_element)
        {
          vecN<enum mask_channel_t, number_mask_type> mask_channels(clip_element->mask_channels());
          bool channels_happy;

          channels_happy = (mask_channels[mask_type_coverage] == mask_channel_clip_in(mask_type_coverage))
            && (mask_channels[mask_type_distance_field] == mask_channel_clip_in(mask_type_distance_field));

          /* if the channels match, then we can recycle the tiles from the clip
           * element when the path fill gives a full tile.
           */
          combine_clip_with_full_tile = !channels_happy;
        }
      else
        {
          /* no clip_element, then no combining period */
          combine_clip_with_full_tile = false;
        }
    }

  ASTRALassert(m_empty_tiles.empty());
  ASTRALassert(m_fully_covered_tiles.empty());
  ASTRALassert(m_element_tiles.empty());

  if (out_clip_combine_tile_data)
    {
      out_clip_combine_tile_data->set_size(m_number_elementary_rects);
    }

  for (int Y = 0; Y < m_number_elementary_rects.y(); ++Y)
    {
      for (int X = 0; X < m_number_elementary_rects.x(); ++X)
        {
          RenderEncoderImage encoder(rect_encoder(X, Y));
          int winding_offset(rect_winding_offset(X, Y));
          ivec2 XY(X, Y);
          enum ClipCombineShader::mode_t combine_shader_mode;
          enum ImageMipElement::element_type_t fill_tile_type_if_encoder_present;
          bool skip_stc(false), reuse_clip_tile(false);

          fill_tile_type_if_encoder_present = ImageMipElement::color_element;
          combine_shader_mode = ClipCombineShader::emit_complement_values_to_blue_alpha;

          /* When clip_element is present, we need to combine the path fill
           * with the values of clip_element.
           *   - encoder.valid() true --> draw the tile of clip_element with ClipCombineShader
           *   - encoder.valid() false
           *       case 1: clip_element->empty_tile()   --> then tile remains empty
           *       case 2: clip_element->full_tile()    --> then F is full or empty and the Image
           *                                                objects made by ClipCombineResult for
           *                                                clip_in and clip_out have tiles setup.
           *       case 3: clip_element->partial_tile() --> we need to make a tile that blits the
           *                                                contents of M combined with F directly
           *                                                and that tile has no post-processing
           *                                                applied to it.
           *
           * If we only need clip-in, then an empty tile remains an empty tile and we can
           * reuse the tile from clip_element if its formats match. These descisions are
           * made by the booleans combine_clip_with_full_tile and combine_clip_with_empty_tile.
           */
          if (!encoder.valid() && clip_element && clip_element->partial_tile(XY))
            {
              bool fill_tile_is_full, needs_tile_from_clip_combine;

              fill_tile_is_full = apply_fill_rule(m_fill_rule, winding_offset);

              needs_tile_from_clip_combine = (fill_tile_is_full) ?
                combine_clip_with_full_tile :
                combine_clip_with_empty_tile;

              if (needs_tile_from_clip_combine)
                {
                  ivec2 image_size;

                  /* Let M = clip_element, F = path fill and F is either full or empty.
                   * Recall that we need ClipIn = (M intersect F) and ClipOut = (M \ F).
                   * So we need to issue a shader that does the right thing for the
                   * two cases of F empty and F full. The shader needs to emit:
                   *  .r --> M intersect F : coverage
                   *  .g --> M intersect F : distance
                   *  .b --> M \ F : coverage
                   *  .a --> M \ F : distance
                   *
                   * which has two different cases F = empty, F = full.
                   * F = Full
                   *  .r --> M : coverage
                   *  .g --> M : distance
                   *  .b --> 0.0
                   *  .a --> 0.0
                   *
                   * F = Empty
                   *  .r --> 0.0
                   *  .g --> 0.0
                   *  .b --> M : coverage
                   *  .a --> M : distance
                   */
                  if (fill_tile_is_full)
                    {
                      combine_shader_mode = ClipCombineShader::emit_direct_values_to_red_green;
                      fill_tile_type_if_encoder_present = ImageMipElement::white_element;
                    }
                  else
                    {
                      combine_shader_mode = ClipCombineShader::emit_direct_values_to_blue_alpha;
                      fill_tile_type_if_encoder_present = ImageMipElement::empty_element;
                    }

                  /* the tile gets backed, but has no STC data so we should no emit winding rects */
                  winding_offset = 0;

                  /* make sure that STC data is NOT added and that an STCData::BuilderSet is not requested */
                  skip_stc = true;

                  /* We gain nothing by making the image on demand, since the image is exactly
                   * one tile. In addition, the assert code to make sure the image size and
                   * tile count is correct, need the backing image to be made immediately.
                   */
                  image_size = ivec2(clip_element->mip_front()->tile_size(uvec2(X, Y), true));
                  encoder = m_renderer.m_storage->create_virtual_buffer(VB_TAG, image_size,
                                                                        DrawCommandList::render_mask_image,
                                                                        image_processing_none,
                                                                        colorspace_linear, number_fill_rule,
                                                                        VirtualBuffer::ImageCreationSpec()
                                                                        .create_immediately(true));

                  ASTRALassert(encoder.virtual_buffer().fetch_image());
                  ASTRALassert(encoder.virtual_buffer().fetch_image()->mip_chain().size() == 1);
                  ASTRALassert(encoder.virtual_buffer().fetch_image()->mip_chain().front()->number_elements(ImageMipElement::empty_element) == 0u);
                  ASTRALassert(encoder.virtual_buffer().fetch_image()->mip_chain().front()->number_elements(ImageMipElement::white_element) == 0u);
                  ASTRALassert(encoder.virtual_buffer().fetch_image()->mip_chain().front()->number_elements(ImageMipElement::color_element) == 1u);
                  ASTRALassert(encoder.virtual_buffer().fetch_image()->mip_chain().front()->tile_count() == uvec2(1, 1));
                }
              else if (fill_tile_is_full)
                {
                  reuse_clip_tile = true;
                }
            }

          if (encoder.valid())
            {
              std::pair<uvec2, VirtualBuffer::TileSource> E;
              STCData::BuilderSet *stc_builder(nullptr);

              if (!skip_stc)
                {
                  stc_builder = stc_builder_for_rect(X, Y);
                  ASTRALassert(stc_builder);
                }

              if (m_fill_rule == odd_even_fill_rule || m_fill_rule == complement_odd_even_fill_rule)
                {
                  if (winding_offset & 1)
                    {
                      encoder.virtual_buffer().invert_stc_fill_rule();
                    }
                }
              else if (winding_offset != 0)
                {
                  /* With a non-zero fill rule, we need to add
                   * rects of the correct winding to the STC data
                   */
                  bool last_rect_X(X + 1 == m_number_elementary_rects.x());
                  bool last_rect_Y(Y + 1 == m_number_elementary_rects.y());
                  bool increases(winding_offset > 0);
                  int count(t_abs(winding_offset));
                  c_array<const VertexStreamerBlock> blocks;

                  ASTRALassert(stc_builder);

                  blocks = winding_rect(last_rect_X, last_rect_Y, increases);
                  for (int i = 0; i < count; ++i)
                    {
                      /* the transformation is the identity because the rect
                       * from winding_rect() is always relative to (0, 0).
                       *
                       * The data is exactly just a rect with no anti-aliasing
                       * so the pass is FillSTCShader::pass_contour_stencil
                       */
                      for (const auto &f : blocks)
                        {
                          stc_builder->add_stc_pass(FillSTCShader::pass_contour_stencil,
                                                    f.m_object,
                                                    range_type<int>(f.m_offset, f.m_offset + f.m_dst.size()),
                                                    m_renderer.m_identity, item_data);
                        }
                    }
                }

              if (clip_element && clip_element->partial_tile(ivec2(X, Y)))
                {
                  /* The tiles of clip_element and the mask to be generated
                   * -perfectly- align. This is why we take the tile with
                   * padding and the transformation is the identity.
                   */
                  const Image *image(clip_element->image());
                  const ImageMipElement *mip(image->mip_chain().front().get());
                  ImageID image_id(image->ID());
                  c_array<const ImageID> dependencies(&image_id, 1);
                  bool include_padding(true);
                  bool tile_has_padding(mip->tile_padding(0) != 0);
                  uvec2 tile(X, Y);
                  uvec2 tile_size(mip->tile_size(tile, include_padding));
                  uvec3 tile_index_atlas_location(mip->tile_index_atlas_location(tile));
                  ItemData item_data;
                  const MaskItemShader *shader(m_renderer.m_default_shaders.m_clip_combine_shader.get());
                  vecN<gvec4, ClipCombineShader::item_data_size> data;

                  ClipCombineShader::pack_item_data(tile_index_atlas_location,
                                                    !include_padding && tile_has_padding,
                                                    tile_size,
                                                    clip_element->mask_channels(),
                                                    combine_shader_mode, data);

                  item_data = m_renderer.create_item_data(data, no_item_data_value_mapping, dependencies);
                  RenderSupportTypes::Item<MaskItemShader> item(*shader, item_data, *m_renderer.m_dynamic_rect);

                  encoder.virtual_buffer().draw_generic(m_renderer.m_identity, item);
                }

              if (stc_builder)
                {
                  /* set the STCData from the Rect's Builder */
                  vecN<STCData::VirtualArray, FillSTCShader::pass_count> stc;

                  stc = stc_builder->end(&m_renderer.m_storage->stc_data_set());
                  encoder.virtual_buffer().stc_data(stc);
                }

              if (out_clip_combine_tile_data)
                {
                  out_clip_combine_tile_data->fill_tile_type(XY) = fill_tile_type_if_encoder_present;
                }

              encoder.finish();

              ASTRALassert(encoder.image());
              ASTRALassert(encoder.image()->size().x() <= ImageAtlas::tile_size);
              ASTRALassert(encoder.image()->size().y() <= ImageAtlas::tile_size);
              ASTRALassert(encoder.image()->mip_chain().size() == 1u);
              ASTRALassert(encoder.image()->mip_chain().front()->number_elements(ImageMipElement::empty_element) == 0u);
              ASTRALassert(encoder.image()->mip_chain().front()->number_elements(ImageMipElement::white_element) == 0u);
              ASTRALassert(encoder.image()->mip_chain().front()->number_elements(ImageMipElement::color_element) == 1u);
              ASTRALassert(encoder.image()->mip_chain().front()->tile_count() == uvec2(1, 1));

              /* note that the min-corner is not the same as (minx_side, maxx_side);
               * this is because those values INCLUDE the padding.
               */
              E.first = uvec2(X, Y);
              E.second.m_src_render_index = encoder.virtual_buffer().render_index();
              E.second.m_src_tile = uvec2(0, 0);
              m_element_tiles.push_back(E);
            }
          else if (reuse_clip_tile)
            {
              std::pair<uvec2, VirtualBuffer::TileSourceImage> E;

              E.first = uvec2(X, Y);
              E.second.m_src_image = clip_element->image();
              E.second.m_src_tile = uvec2(X, Y);
              m_image_tiles.push_back(E);

              m_renderer.m_stats[number_tiles_skipped_from_sparse_filling] += 1u;
              if (out_clip_combine_tile_data)
                {
                  out_clip_combine_tile_data->fill_tile_type(XY) = ImageMipElement::color_element;
                }
            }
          else
            {
              bool skip_rect;
              enum ImageMipElement::element_type_t v;

              skip_rect = clip_element && clip_element->empty_tile(ivec2(X, Y));
              if (!skip_rect && apply_fill_rule(m_fill_rule, winding_offset))
                {
                  m_fully_covered_tiles.push_back(uvec2(X, Y));
                  v = ImageMipElement::white_element;
                }
              else
                {
                  m_empty_tiles.push_back(uvec2(X, Y));
                  v = ImageMipElement::empty_element;
                }

              m_renderer.m_stats[number_tiles_skipped_from_sparse_filling] += 1u;

              if (out_clip_combine_tile_data)
                {
                  out_clip_combine_tile_data->fill_tile_type(XY) = v;
                }
            }
        }
    }

  reference_counted_ptr<const Image> return_value;

  return_value = VirtualBuffer::create_assembled_image(VB_TAG, m_renderer,
                                                       uvec2(m_total_size), colorspace_linear,
                                                       make_c_array(m_empty_tiles),
                                                       make_c_array(m_fully_covered_tiles),
                                                       make_c_array(m_element_tiles),
                                                       make_c_array(m_image_tiles));

  m_empty_tiles.clear();
  m_image_tiles.clear();
  m_fully_covered_tiles.clear();
  m_element_tiles.clear();

  if (!return_value->default_use_prepadding())
    {
      Image *cheating;

      cheating = const_cast<Image*>(return_value.get());
      cheating->default_use_prepadding(true);
    }

  return return_value;
}

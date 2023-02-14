/*!
 * \file renderer_filler.cpp
 * \brief file renderer_filler.cpp
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
#include "renderer_clip_element.hpp"
#include "renderer_filler.hpp"
#include "renderer_stc_data_builder_helper.hpp"

void
astral::Renderer::Implement::Filler::
create_mask(float logical_tol,
            enum fill_rule_t fill_rule, enum anti_alias_t aa_mode,
            const CombinedPath &path, const CullGeometrySimple &cull_geometry,
            c_array<const BoundingBox<float>> restrict_bbs,
            const Transformation &image_transformation_logical,
            const ClipElement *clip_element,
            enum clip_combine_mode_t clip_combine_mode,
            TileTypeTable *out_clip_combine_tile_data,
            MaskDetails *out_data)
{
  ivec2 rect_size(cull_geometry.image_size());
  reference_counted_ptr<const Image> mask_image;

  ASTRALassert(out_data);
  ASTRALassert(!clip_element || &clip_element->cull_geometry() == &cull_geometry);

  if (rect_size.x() > 0 && rect_size.y() > 0)
    {
      m_region = BoundingBox<float>(vec2(0.0f), vec2(rect_size));
      m_aa_mode = aa_mode;
      m_fill_rule = fill_rule;

      m_cached_combined_path.set(logical_tol, m_region, image_transformation_logical, path);

      mask_image = create_sparse_mask(rect_size, restrict_bbs, path, clip_element, clip_combine_mode, out_clip_combine_tile_data);
      if (!mask_image)
        {
          mask_image = create_mask_non_sparse(rect_size, path, clip_element, out_clip_combine_tile_data);
        }
    }
  compute_mask_details(cull_geometry, mask_image, out_data);
}

void
astral::Renderer::Implement::Filler::
compute_mask_details(const CullGeometrySimple &cull_geometry,
                     const reference_counted_ptr<const Image> &mask_image,
                     MaskDetails *out_data)
{
  out_data->m_mask_transformation_pixel = cull_geometry.image_transformation_pixel();
  out_data->m_mask = mask_image;

  if (mask_image)
    {
      /* the rect specified by the input clip geometry includes
       * the padding around the path's render. The padding is
       * there to make sure that sampling with filtering is
       * correct. However, the actual rect that contains the
       * path is the padding less in each dimension. So we can
       * remove that padding from the mask. In addition, the
       * shaders of MaskDrawerImage operate directly on the tiles
       * of a mask and when they sample at the boundary of the
       * tiles of the boudnary of the image with filtering, they
       * might fetch texels outside of the tiles. Thus, we must
       * restrict the sampling of texels.
       */
      uint32_t S(ImageAtlas::tile_padding);
      vec2 tr(S, S);

      out_data->m_min_corner = tr;
      out_data->m_size = vec2(mask_image->size() - uvec2(S + S));

      out_data->m_mask_transformation_pixel.m_translate -= tr;
    }
  else
    {
      out_data->m_min_corner = vec2(0);
      out_data->m_size = vec2(0);
    }
}

astral::reference_counted_ptr<const astral::Image>
astral::Renderer::Implement::Filler::
create_mask_non_sparse(ivec2 rect_size, const CombinedPath &combined_path,
                       const ClipElement *clip_element,
                       TileTypeTable *out_clip_combine_tile_data)
{
  RenderEncoderImage im;

  /* the function non_sparse_handle_clipping(), requires that the backing
   * image of the virtaul buffer is already created.
   */
  im = m_renderer.m_storage->create_virtual_buffer(VB_TAG, rect_size, m_fill_rule,
                                                   VirtualBuffer::ImageCreationSpec()
                                                   .create_immediately(true));

  if (!im.valid())
    {
      return nullptr;
    }

  /* We allow for long curves because if a mask is being rendered non-sparse,
   * chances are it is drawing paths small-ish, potentially even minified.
   * In this case, we do not want to make the curves smaller as that will
   * add oodles of vertex load.
   */
  const enum contour_fill_approximation_t tp(contour_fill_approximation_allow_long_curves);
  vecN<STCData::VirtualArray, FillSTCShader::pass_count> stc;

  /* add the STC data from the combined path to im */
  m_builder.start();
  STCData::BuilderSet::Helper(m_builder).add_stc_path(im.virtual_buffer(), combined_path, tp, m_aa_mode, m_cached_combined_path);
  stc = m_builder.end(&m_renderer.m_storage->stc_data_set());
  im.virtual_buffer().stc_data(stc);

  /* apply the clipping data to im */
  non_sparse_handle_clipping(im, clip_element, out_clip_combine_tile_data);

  im.finish();

  /* make sure that padding usage is correct AND finish the encoder */
  ASTRALassert(im.image());
  im.image()->default_use_prepadding(false);

  return im.image();
}

void
astral::Renderer::Implement::Filler::
create_mask_via_item_path_shader(Renderer::Implement &renderer, const MaskItemPathShader &shader,
                                 float tol, enum fill_rule_t fill_rule,
                                 const CombinedPath &combined_path, const CullGeometrySimple &cull_geometry,
                                 const Transformation &image_transformation_logical,
                                 const ClipElement *clip_element,
                                 TileTypeTable *out_clip_combine_tile_data, MaskDetails *out_data)
{
  ivec2 rect_size(cull_geometry.image_size());
  reference_counted_ptr<Image> mask_image;

  ASTRALassert(out_data);
  ASTRALassert(combined_path.paths<AnimatedPath>().empty());
  ASTRALassert(!clip_element || &clip_element->cull_geometry() == &cull_geometry);

  if (rect_size.x() > 0 && rect_size.y() > 0)
    {
      RenderEncoderImage im;
      c_array<const Path* const> paths(combined_path.paths<Path>());

      /* the function non_sparse_handle_clipping(), requires that the backing
       * image of the virtaul buffer is already created.
       */
      im = renderer.m_storage->create_virtual_buffer(VB_TAG, rect_size, number_fill_rule,
                                                     VirtualBuffer::ImageCreationSpec()
                                                     .create_immediately(true));
      ASTRALassert(im.valid());

      im.render_accuracy(tol);
      im.transformation(image_transformation_logical);

      for (unsigned int i = 0; i < paths.size(); ++i)
        {
          const vec2 *translate;
          const float2x2 *matrix;
          const ItemPath *item_path;
          float item_path_tol;
          unsigned int sz;
          c_array<gvec4> data;
          BoundingBox<float> bb;

          translate = combined_path.get_translate<Path>(i);
          matrix = combined_path.get_matrix<Path>(i);

          if (translate || matrix)
            {
              im.save_transformation();
              if (translate)
                {
                  im.translate(*translate);
                }

              if (matrix)
                {
                  im.concat(*matrix);
                }
            }

          /* get the correct ItemPath object */
          item_path_tol = im.compute_tolerance();
          item_path = &paths[i]->item_path(item_path_tol);

          /* create a ItemPath::Layer value from the ItemPath
           * value with the correct fill rule to apply.
           *
           * TODO: this is crappy interface here, since
           *       drawing a mask does not need a color
           *       value.
           */
          ItemPath::Layer item_path_layer(*item_path);
          item_path_layer
            .fill_rule(fill_rule)
            .color(vec4(1.0f, 1.0f, 1.0f, 1.0f)); //does not matter

          /* Interface for realizing ItemData takes an array of ItemPath::Layer values */
          c_array<const ItemPath::Layer> layer_as_array(&item_path_layer, 1);

          /* pack the ItemData for shading */
          sz = MaskItemPathShader::item_data_size(layer_as_array.size());
          renderer.m_workroom->m_item_data_workroom.resize(sz);
          data = make_c_array(renderer.m_workroom->m_item_data_workroom);
          bb = MaskItemPathShader::pack_item_data(*renderer.m_engine, layer_as_array, data);

          /* if the bounding box is non-empty, then draw the ItemPath value */
          if (!bb.empty())
            {
              ItemData item_data(im.create_item_data(data, no_item_data_value_mapping));
              RenderSupportTypes::Item<MaskItemShader> item(*shader.get(), item_data, *renderer.m_dynamic_rect);

              im.virtual_buffer().draw_generic(im.transformation_value(), item);
            }

          if (translate || matrix)
            {
              im.restore_transformation();
            }
        }
      non_sparse_handle_clipping(im, clip_element, out_clip_combine_tile_data);
      im.finish();

      ASTRALassert(im.image());
      im.image()->default_use_prepadding(false);

      mask_image = im.image();
    }
  compute_mask_details(cull_geometry, mask_image, out_data);
}

void
astral::Renderer::Implement::Filler::
non_sparse_handle_clipping(RenderEncoderBase im,
                           const ClipElement *clip_element,
                           TileTypeTable *out_clip_combine_tile_data)
{
  ASTRALassert(im.valid());
  ASTRALassert(im.virtual_buffer().image_create_spec().m_create_immediately);
  ASTRALassert(im.virtual_buffer().fetch_image());
  ASTRALassert(!im.virtual_buffer().fetch_image()->mip_chain().empty());

  /* TODO: if clip_element is non-null
   *   - Instead of requiring that the image exists already, instead get the
   *     tile count from a computation on what the size of the image would be.
   *   - MAYBE: in theory, masks can also have occluders...
   *   - MAYBE: create another image that takes the tiles from im
   *            except those tiles that are empy in clip_element
   *            are empty.
   */
  if (out_clip_combine_tile_data)
    {
      out_clip_combine_tile_data->set_size(im.virtual_buffer().fetch_image()->mip_chain().front()->tile_count());
      out_clip_combine_tile_data->fill_tile_type_all(ImageMipElement::color_element);
    }

  if (clip_element)
    {
      const Image *image(clip_element->image());
      const ImageMipElement *mip(image->mip_chain().front().get());
      ImageID image_id(image->ID());
      Renderer::Implement &renderer(im.renderer_implement());

      /* Add the ClipCombineShader draws */
      for (unsigned int t = 0, endt = mip->number_elements(ImageMipElement::color_element); t < endt; ++t)
        {
          uvec2 tile, tile_location, tile_size;
          uvec3 tile_index_atlas_location;
          const MaskItemShader *shader(renderer.m_default_shaders.m_clip_combine_shader.get());
          vecN<gvec4, ClipCombineShader::item_data_size> data;
          ItemDataDependencies dependencies;
          Transformation tr;
          ItemData item_data;

          /* note that we do NOT include the padding. This is because the
           * renders are going to a single VirtualBuffer, not each tile
           * is a seperate VirtualBuffer
           */
          bool include_padding(false);
          bool tile_has_padding(mip->tile_padding(0) != 0);

          tile = mip->element_tile_id(ImageMipElement::color_element, t);
          tile_location = mip->tile_location(tile);
          tile_index_atlas_location = mip->tile_index_atlas_location(tile);
          tile_size = mip->tile_size(tile, include_padding);

          /* We need to have the tile draw at the min-min corner of the tile */
          tr.m_translate = vec2(tile_location);
          ClipCombineShader::pack_item_data(tile_index_atlas_location,
                                            !include_padding && tile_has_padding,
                                            tile_size,
                                            clip_element->mask_channels(),
                                            ClipCombineShader::emit_complement_values_to_blue_alpha,
                                            data);

          if (t == 0)
            {
              /* We only add the dependency on the first tile to add since
               * it is the same dependency for all tiles.
               */
              dependencies.m_images = c_array<const ImageID>(&image_id, 1);
            }

          item_data = im.create_item_data(data, no_item_data_value_mapping, dependencies);
          RenderSupportTypes::Item<MaskItemShader> item(*shader, item_data, *renderer.m_dynamic_rect);
          im.virtual_buffer().draw_generic(im.create_value(tr), item);
        }

      /* TODO: specify blit-rects so that those tiles of clip_element that
       *       are empty are skipped in the blit
       */

      /* TODO: create another image that takes the tiles from im
       *       except those tiles that are empy in clip_element
       *       are empty.
       */

      /* MAYBE-TODO: create another image that takes the tiles from im
       *             except those tiles that are empy in clip_element
       *             are empty.
       */
    }
}

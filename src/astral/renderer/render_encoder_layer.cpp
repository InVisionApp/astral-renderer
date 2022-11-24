/*!
 * \file render_encoder_layer.cpp
 * \brief file render_encoder_layer.cpp
 *
 * Copyright 2022 by InvisionApp.
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

#include <astral/renderer/renderer.hpp>
#include "renderer_implement.hpp"
#include "renderer_storage.hpp"
#include "render_encoder_layer.hpp"

////////////////////////////////////////////////////
// astral::RenderEncoderLayer::Backing methods
astral::RenderEncoderLayer::Backing::
Backing(RenderEncoderBase pparent_encoder,
        const BoundingBox<float> &bb, RenderScaleFactor scale_factor,
        enum colorspace_t colorspace, const vec4 &color,
        enum blend_mode_t blend_mode, enum filter_t filter_mode,
        const ItemMask &clip):
  m_parent_encoder(pparent_encoder),
  m_transformation(m_parent_encoder.transformation()),
  m_blend_mode(blend_mode),
  m_clip(clip),
  m_rect(bb),
  m_color(color),
  m_filter_mode(filter_mode),
  m_effect_data(nullptr),
  m_end_layer_called(false)
{
  /* the padding is 2 pixels to support the various
   * filter modes.
   *
   * TODO: check the filter mode and adjust the padding.
   *       Note that unless a clamping window is present,
   *       nearest filtering will still need a padding of
   *       1 for when the texel coordinate goes beyond
   *       Width - 0.5 (or Height - 0.5).
   */
  const unsigned int padding(2u);
  RelativeBoundingBox rel_bb(bb);
  BoundingBox<float> restrict_pixel_rect;

  if (clip.m_clip_element && !clip.m_clip_out)
    {
      const MaskDetails *mask(clip.m_clip_element->mask_details());
      if (mask)
        {
          restrict_pixel_rect = mask->pixel_rect();
        }
      rel_bb.m_pixel_bb = &restrict_pixel_rect;
    }

  m_encoder = m_parent_encoder.encoder_image_relative(rel_bb, scale_factor, colorspace, padding);
}

astral::RenderEncoderLayer::Backing::
Backing(RenderEncoderBase pparent_encoder, Renderer::Implement::Storage &storage,
        const Effect &effect,
        const EffectParameters &effect_parameters,
        const BoundingBox<float> &in_logical_rect,
        enum colorspace_t colorspace,
        enum blend_mode_t blend_mode,
        const ItemMask &clip):
  m_parent_encoder(pparent_encoder),
  m_blend_mode(blend_mode),
  m_clip(clip),
  m_rect(in_logical_rect),
  m_effect_data(storage.allocate_effect_data()),
  m_end_layer_called(false)
{
  RenderEncoderBase::AutoRestore auto_restore(m_parent_encoder);
  Effect::OverridableBufferProperties overridable_properties;
  Effect::BufferParameters buffer_parameters;
  Effect::BlitParameters blit_params;

  /* Transform to effect coordinates, then save the transformation value */
  m_parent_encoder.translate(effect_parameters.m_effect_transformation_logical);
  m_transformation = m_parent_encoder.transformation();

  /* save the effect value */
  m_effect_data->m_effect = &effect;

  /* compute the buffer properties */
  buffer_parameters.m_custom_data = effect_parameters.m_data;
  buffer_parameters.m_pixel_transformation_logical = m_transformation;
  buffer_parameters.m_singular_values = m_parent_encoder.singular_values();
  buffer_parameters.m_logical_rect = m_rect.as_rect();
  buffer_parameters.m_render_scale_factor = m_parent_encoder.render_scale_factor();

  effect.compute_overridable_buffer_properties(buffer_parameters, &overridable_properties);
  effect.compute_buffer_properties(overridable_properties, buffer_parameters,
                                   &m_effect_data->m_processed_params,
                                   &m_effect_data->m_buffer_properties);

  if (m_clip.m_clip_element && !m_clip.m_clip_out)
    {
      const MaskDetails *mask(m_clip.m_clip_element->mask_details());
      if (mask)
        {
          m_pixel_bb_backing = mask->pixel_rect();
        }
    }

  m_effect_data->m_logical_slack = overridable_properties.m_logical_slack;

  /* note the false in the RenderScaleFactor ctor; this is because
   * the value of effect_render_scale_factor is absolute, not relative
   * to this encoder.
   */
  m_encoder = m_parent_encoder.encoder_image_relative(effect_rect(),
                                                      RenderScaleFactor(overridable_properties.m_render_scale_factor, false),
                                                      colorspace,
                                                      effect_pixel_slack());
}

astral::RenderEncoderLayer::Backing::
Backing(RenderEncoderBase pparent_encoder,
        Renderer::Implement::Storage &storage,
        ScratchSpace &scratch,
        const EffectCollectionBase &effects,
        const BoundingBox<float> &in_logical_rect,
        enum colorspace_t colorspace,
        const ItemMask &clip):
  m_parent_encoder(pparent_encoder),
  m_transformation(m_parent_encoder.transformation()),
  m_clip(clip),
  m_rect(in_logical_rect),
  m_effect_data(storage.allocate_effect_data()),
  m_end_layer_called(false)
{
  float effect_render_scale_factor;
  unsigned int effect_pixel_slack, num_effects;

  ASTRALassert(m_effect_data && !m_effect_data->m_effect);

  num_effects = effects.number_effects();
  scratch.m_translate_and_paddings.resize(num_effects);
  scratch.m_buffer_parameters.resize(num_effects);
  scratch.m_overridable_properties.resize(num_effects);
  scratch.m_effect_transformation_logical.resize(num_effects);
  scratch.m_translate_capture_bb.resize(num_effects);

  /* Step 1: compute the value of OverridableBufferProperties
   *         for each input effect, saving them to the scratch
   *         space.
   */
  for (unsigned int i = 0; i < num_effects; ++i)
    {
      RenderEncoderBase::AutoRestore auto_restore(m_parent_encoder);
      const EffectParameters &effect_parameters(effects.effect_parameters(i));
      const Effect &effect(effects.effect(i));

      m_parent_encoder.translate(effect_parameters.m_effect_transformation_logical);

      scratch.m_buffer_parameters[i].m_custom_data = effect_parameters.m_data;
      scratch.m_buffer_parameters[i].m_pixel_transformation_logical = m_parent_encoder.transformation();
      scratch.m_buffer_parameters[i].m_singular_values = m_parent_encoder.singular_values();
      scratch.m_buffer_parameters[i].m_logical_rect = m_rect.as_rect();
      scratch.m_buffer_parameters[i].m_render_scale_factor = m_parent_encoder.render_scale_factor();

      effect.compute_overridable_buffer_properties(scratch.m_buffer_parameters[i], &scratch.m_overridable_properties[i]);

      scratch.m_effect_transformation_logical[i] = effect_parameters.m_effect_transformation_logical;
      scratch.m_translate_capture_bb[i] = effects.translate_capture_bb(i);
      scratch.m_translate_and_paddings[i].m_logical_padding = scratch.m_overridable_properties[i].m_logical_slack;
      scratch.m_translate_and_paddings[i].m_logical_translate = effect_parameters.m_effect_transformation_logical + scratch.m_translate_capture_bb[i];
    }

  /* Step 2: compute the intersection of the logical rect padded and translated per effect */
  RelativeBoundingBox rr(m_rect, 0.0f);
  if (m_clip.m_clip_element && !m_clip.m_clip_out)
    {
      const MaskDetails *mask(m_clip.m_clip_element->mask_details());
      if (mask)
        {
          m_pixel_bb_backing = mask->pixel_rect();
        }
      rr.m_pixel_bb = &m_pixel_bb_backing;
    }

  Renderer::VirtualBuffer &virtual_buffer(*m_parent_encoder.m_virtual_buffer);
  Renderer::Implement &renderer(virtual_buffer.m_renderer);

  virtual_buffer.clip_geometry().compute_intersection(storage, m_transformation,
                                                      m_parent_encoder.singular_values().x(),
                                                      rr,
                                                      make_c_array(scratch.m_translate_and_paddings),
                                                      &scratch.m_intersection);

  /* Step 3: compute the max render scale factor and padding of those effects that
   *         are active
   */
  unsigned int num_active_effects;
  num_active_effects = scratch.m_intersection.num_polygon_groups();

  for (unsigned int G = 0; G < num_active_effects; ++G)
    {
      int src;

      src = scratch.m_intersection.polygon_group_source(G);
      if (G == 0)
        {
          effect_render_scale_factor = scratch.m_overridable_properties[src].m_render_scale_factor;
        }
      else
        {
          effect_render_scale_factor = t_max(scratch.m_overridable_properties[src].m_render_scale_factor,
                                             effect_render_scale_factor);
        }
    }

  /* Step 4: prepare m_effect_data->m_collection only taking those effects
   *         that scratch.m_intersection lists. In addition, compute the
   *         max over all active effects the pixel slack
   */
  m_effect_data->m_collection.resize(num_active_effects);
  for (unsigned int G = 0; G < num_active_effects; ++G)
    {
      int src;

      src = scratch.m_intersection.polygon_group_source(G);

      const Effect &effect(effects.effect(src));
      range_type<unsigned int> R;
      Effect::OverridableBufferProperties B;

      m_effect_data->m_collection[G].m_effect = &effect;
      m_effect_data->m_collection[G].m_blend_mode = effects.blend_mode(src);
      m_effect_data->m_collection[G].m_logical_slack = scratch.m_translate_and_paddings[src].m_logical_padding;
      m_effect_data->m_collection[G].m_effect_transformation_logical = scratch.m_effect_transformation_logical[src];
      m_effect_data->m_collection[G].m_translate_capture_bb = scratch.m_translate_capture_bb[src];

      /* We take the logical padding that we had recorded earlier, but
       * we override the render scale factor with the value that will
       * be used.
       */
      B.m_logical_slack = scratch.m_translate_and_paddings[src].m_logical_padding;
      B.m_render_scale_factor = effect_render_scale_factor;

      scratch.m_generic_data.clear();
      effect.compute_buffer_properties(B, scratch.m_buffer_parameters[src],
                                       &scratch.m_generic_data,
                                       &m_effect_data->m_collection[G].m_buffer_properties);

      R.m_begin = m_effect_data->m_processed_params.size();
      R.m_end = R.m_begin + scratch.m_generic_data.size();
      m_effect_data->m_processed_params.resize(R.m_end);
      std::copy(scratch.m_generic_data.begin(),
                scratch.m_generic_data.end(),
                m_effect_data->m_processed_params.begin() + R.m_begin);

      m_effect_data->m_collection[G].m_processed_params_range = R;

      if (G == 0)
        {
          effect_pixel_slack = m_effect_data->m_collection[G].m_buffer_properties.m_pixel_slack;
        }
      else
        {
          effect_pixel_slack = t_max(effect_pixel_slack,
                                     m_effect_data->m_collection[G].m_buffer_properties.m_pixel_slack);
        }
    }

  /* Construct the clip-geometry encompasing the zones that the effects hit */
  Renderer::Implement::ClipGeometryGroup clip_geometry(renderer,
                                                       effect_render_scale_factor,
                                                       scratch.m_intersection,
                                                       effect_pixel_slack);

  /* Generate m_encoder */
  m_encoder = renderer.m_storage->create_virtual_buffer(VB_TAG, m_transformation, clip_geometry,
                                                        Renderer::Implement::DrawCommandList::render_color_image,
                                                        image_processing_none, colorspace,
                                                        number_fill_rule,
                                                        Renderer::VirtualBuffer::ImageCreationSpec());
}

astral::RelativeBoundingBox
astral::RenderEncoderLayer::Backing::
effect_rect(void) const
{
  ASTRALassert(m_effect_data);
  ASTRALassert(m_effect_data->m_collection.empty());
  ASTRALassert(m_effect_data->m_effect);

  RelativeBoundingBox return_value(m_rect, m_effect_data->m_logical_slack);
  if (m_clip.m_clip_element && !m_clip.m_clip_out)
    {
      return_value.m_pixel_bb = &m_pixel_bb_backing;
    }

  return return_value;
}

astral::RelativeBoundingBox
astral::RenderEncoderLayer::Backing::
effect_rect(unsigned int I) const
{
  ASTRALassert(m_effect_data);
  ASTRALassert(I < m_effect_data->m_collection.size());
  ASTRALassert(!m_effect_data->m_effect);

  BoundingBox<float> rect(m_rect);
  rect.translate(m_effect_data->m_collection[I].m_translate_capture_bb);

  RelativeBoundingBox return_value(rect, m_effect_data->m_collection[I].m_logical_slack);
  if (m_clip.m_clip_element && !m_clip.m_clip_out)
    {
      return_value.m_pixel_bb = &m_pixel_bb_backing;
    }

  return return_value;
}

void
astral::RenderEncoderLayer::Backing::
end_layer_blit(void) const
{
  ASTRALassert(!m_effect_data);

  Transformation image_transformation_logical;
  Brush brush;
  ImageSampler image;
  const Image &im(*m_encoder.image());
  SubImage sub_image(im);
  RenderEncoderBase::AutoRestore auto_restore(m_parent_encoder);

  image_transformation_logical = Transformation(m_encoder.image_transformation_pixel())
    * m_transformation;

  image = ImageSampler(sub_image, m_filter_mode, mipmap_none);
  brush
    .base_color(m_color)
    .image(m_parent_encoder.create_value(image))
    .image_transformation(m_parent_encoder.create_value(image_transformation_logical));

  m_parent_encoder.transformation(m_transformation);
  m_parent_encoder.draw_rect(m_rect.as_rect(), false,
                             ItemMaterial(m_parent_encoder.create_value(brush), m_clip),
                             m_blend_mode);
}

void
astral::RenderEncoderLayer::Backing::
end_layer_effect(void) const
{
  ASTRALassert(m_encoder.finished());
  ASTRALassert(m_effect_data);
  ASTRALassert(m_effect_data->m_effect);
  ASTRALassert(m_effect_data->m_collection.empty());

  RenderEncoderBase::AutoRestore auto_restore(m_parent_encoder);
  Effect::BlitParameters blit_params;
  BoundingBox<float> sub_image_rect;
  Image *entire_image(nullptr);

  blit_params.m_logical_rect = effect_rect();
  blit_params.m_content_transformation_logical = Transformation(m_encoder.image_transformation_pixel())
    * m_transformation;

  entire_image = m_encoder.image_with_mips(effect_required_lod()).get();
  sub_image_rect = blit_params.m_content_transformation_logical.apply_to_bb(blit_params.m_logical_rect.bb_with_padding());
  sub_image_rect.enlarge(vec2(effect_pixel_slack()));
  sub_image_rect.intersect_against(BoundingBox<float>(vec2(0.0f, 0.0f), vec2(entire_image->size())));

  SubImageT<float> content_f(*entire_image, sub_image_rect.min_point(), sub_image_rect.size());
  SubImage content(content_f.type_cast_enlarge<unsigned int>());

  if (content.m_min_corner != uvec2(0, 0))
    {
      /* We need to provide the transformation from logical to the SubImage
       * we pass to the effect, thus we need to append a translation to the
       * value of blit_params.m_content_transformation_logical. That last
       * translation needs to map subimage.m_min_corner to (0, 0).
       */
      Transformation tr;

      tr.m_translate = -vec2(content.m_min_corner);
      blit_params.m_content_transformation_logical = tr * blit_params.m_content_transformation_logical;
    }

  m_parent_encoder.transformation(m_transformation);
  m_effect_data->m_effect->render_effect(m_parent_encoder,
                                         make_c_array(m_effect_data->m_processed_params),
                                         m_effect_data->m_workroom,
                                         content,
                                         blit_params, m_blend_mode,
                                         m_clip);
}

void
astral::RenderEncoderLayer::Backing::
end_layer_effect_of_collection(unsigned int I ) const
{
  ASTRALassert(m_effect_data);
  ASTRALassert(I < m_effect_data->m_collection.size());
  ASTRALassert(!m_effect_data->m_effect);

  RenderEncoderBase::AutoRestore auto_restore(m_parent_encoder);
  Effect::BlitParameters blit_params;
  BoundingBox<float> sub_image_rect;
  Image *entire_image(nullptr);
  c_array<const generic_data> processed_params;

  blit_params.m_logical_rect = effect_rect(I);
  blit_params.m_content_transformation_logical = Transformation(m_encoder.image_transformation_pixel()) * m_transformation;

  entire_image = m_encoder.image_with_mips(effect_required_lod(I)).get();
  sub_image_rect = blit_params.m_content_transformation_logical.apply_to_bb(blit_params.m_logical_rect.bb_with_padding());
  sub_image_rect.enlarge(vec2(effect_pixel_slack(I)));
  sub_image_rect.intersect_against(BoundingBox<float>(vec2(0.0f, 0.0f), vec2(entire_image->size())));

  SubImageT<float> content_f(*entire_image, sub_image_rect.min_point(), sub_image_rect.size());
  SubImage content(content_f.type_cast_enlarge<unsigned int>());

  if (content.m_min_corner != uvec2(0, 0))
    {
      /* We need to provide the transformation from logical to the SubImage
       * we pass to the effect, thus we need to append a translation to the
       * value of blit_params.m_content_transformation_logical. That last
       * translation needs to map subimage.m_min_corner to (0, 0).
       */
      Transformation tr;

      tr.m_translate = -vec2(content.m_min_corner);
      blit_params.m_content_transformation_logical = tr * blit_params.m_content_transformation_logical;
    }

  processed_params = make_c_array(m_effect_data->m_processed_params);
  processed_params = processed_params.sub_array(m_effect_data->m_collection[I].m_processed_params_range);

  m_parent_encoder.transformation(m_transformation * Transformation(m_effect_data->m_collection[I].m_effect_transformation_logical));
  m_effect_data->m_collection[I].m_effect->render_effect(m_parent_encoder,
                                                         processed_params,
                                                         m_effect_data->m_workroom,
                                                         content,
                                                         blit_params,
                                                         m_effect_data->m_collection[I].m_blend_mode,
                                                         m_clip);
}

void
astral::RenderEncoderLayer::Backing::
end_layer(Renderer::Implement::Storage &storage)
{
  ASTRALassert(!m_end_layer_called);

  if (!m_encoder.finished())
    {
      m_encoder.finish();
    }

  if (m_encoder.degenerate())
    {
      if (m_effect_data != nullptr)
        {
          storage.reclaim_effect_data(m_effect_data);
          m_effect_data = nullptr;
        }

      m_end_layer_called = true;
      m_clip = ItemMask();

      return;
    }

  if (m_effect_data != nullptr)
    {
      if (m_effect_data->m_effect)
        {
          end_layer_effect();
        }
      else
        {
          for (unsigned int i = 0, endi = m_effect_data->m_collection.size(); i < endi; ++i)
            {
              end_layer_effect_of_collection(i);
            }
        }

      storage.reclaim_effect_data(m_effect_data);
      m_effect_data = nullptr;
    }
  else
    {
      end_layer_blit();
    }

  m_end_layer_called = true;
  m_clip = ItemMask();
}

/////////////////////////////////////////////
// astral::RenderEncoderLayer methods
astral::RenderEncoderImage
astral::RenderEncoderLayer::
encoder(void) const
{
  return (m_backing) ?
    m_backing->encoder() :
    RenderEncoderImage();
}

astral::RenderEncoderBase
astral::RenderEncoderLayer::
parent_encoder(void) const
{
  return (m_backing) ?
    m_backing->parent_encoder() :
    RenderEncoderBase();
}

bool
astral::RenderEncoderLayer::
ended(void) const
{
  return (m_backing) ?
    m_backing->end_layer_called() :
    true;
}

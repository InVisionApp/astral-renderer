/*!
 * \file render_encoder_layer.hpp
 * \brief file render_encoder_layer.hpp
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

#ifndef ASTRAL_RENDER_ENCODER_LAYER_HPP
#define ASTRAL_RENDER_ENCODER_LAYER_HPP

#include <astral/renderer/renderer.hpp>
#include "renderer_clip_geometry.hpp"

class astral::RenderEncoderLayer::Backing
{
public:
  class EffectData
  {
  public:
    class CollectionEntry
    {
    public:
      /* effect to apply */
      reference_counted_ptr<const Effect> m_effect;

      /* blend mode to apply */
      enum blend_mode_t m_blend_mode;

      /* value of OverridableBufferProperties::m_logical_slack */
      float m_logical_slack;

      /* buffer properties from m_effect */
      Effect::BufferProperties m_buffer_properties;

      /* transformation from logical to effect coordinates */
      vec2 m_effect_transformation_logical;

      /* translation to apply the caputre bounding box in logical coordinates */
      vec2 m_translate_capture_bb;

      /* range into m_processed_params for the entry */
      range_type<unsigned int> m_processed_params_range;
    };

    void
    clear(void)
    {
      m_collection.clear();
      m_workroom.clear();
      m_processed_params.clear();
      m_effect = nullptr;
    }

    /* common if multiple effects or a single effect */
    EffectWorkRoom m_workroom;
    std::vector<generic_data> m_processed_params;

    /* if there is only one effect */
    reference_counted_ptr<const Effect> m_effect;
    float m_logical_slack;
    Effect::BufferProperties m_buffer_properties;

    /* non-empty if there are multiple effects */
    std::vector<CollectionEntry> m_collection;
  };

  class ScratchSpace
  {
  public:
    std::vector<generic_data> m_generic_data;
    std::vector<Renderer::Implement::ClipGeometryGroup::TranslateAndPadding> m_translate_and_paddings;
    std::vector<vec2> m_effect_transformation_logical;
    std::vector<vec2> m_translate_capture_bb;
    Renderer::Implement::ClipGeometryGroup::Intersection m_intersection;

    std::vector<Effect::BufferParameters> m_buffer_parameters;
    std::vector<Effect::OverridableBufferProperties> m_overridable_properties;
  };

  Backing(RenderEncoderBase pparent_encoder,
          const BoundingBox<float> &bb, RenderScaleFactor scale_factor,
          enum colorspace_t colorspace, const vec4 &color,
          enum blend_mode_t blend_mode, enum filter_t filter_mode,
          const ItemMask &clip);

  Backing(RenderEncoderBase pparent_encoder,
          Renderer::Implement::Storage &storage,
          const Effect &effect,
          const EffectParameters &effect_parameters,
          const BoundingBox<float> &in_logical_rect,
          enum colorspace_t colorspace,
          enum blend_mode_t blend_mode,
          const ItemMask &clip);

  Backing(RenderEncoderBase pparent_encoder,
          Renderer::Implement::Storage &storage,
          ScratchSpace &scratch,
          const EffectCollectionBase &effects,
          const BoundingBox<float> &in_logical_rect,
          enum colorspace_t colorspace,
          const ItemMask &clip);

  RenderEncoderBase
  parent_encoder(void) const
  {
    return m_parent_encoder;
  }

  RenderEncoderImage
  encoder(void) const
  {
    return m_encoder;
  }

  bool
  end_layer_called(void) const
  {
    return m_end_layer_called;
  }

  void
  end_layer(Renderer::Implement::Storage &storage);

private:
  /* Compute and return the RelativeBoundingBox to pass to Effect::render_effect(). */
  RelativeBoundingBox
  effect_rect(void) const;

  unsigned int
  effect_required_lod(void) const
  {
    ASTRALassert(m_effect_data);
    ASTRALassert(m_effect_data->m_collection.empty());
    ASTRALassert(m_effect_data->m_effect);
    return m_effect_data->m_buffer_properties.m_required_lod;
  }

  unsigned int
  effect_pixel_slack(void) const
  {
    ASTRALassert(m_effect_data);
    ASTRALassert(m_effect_data->m_collection.empty());
    ASTRALassert(m_effect_data->m_effect);
    return m_effect_data->m_buffer_properties.m_pixel_slack;
  }

  RelativeBoundingBox
  effect_rect(unsigned int I) const;

  unsigned int
  effect_required_lod(unsigned int I) const
  {
    ASTRALassert(m_effect_data);
    ASTRALassert(I < m_effect_data->m_collection.size());
    ASTRALassert(!m_effect_data->m_effect);
    return m_effect_data->m_collection[I].m_buffer_properties.m_required_lod;
  }

  unsigned int
  effect_pixel_slack(unsigned int I) const
  {
    ASTRALassert(m_effect_data);
    ASTRALassert(I < m_effect_data->m_collection.size());
    ASTRALassert(!m_effect_data->m_effect);
    return m_effect_data->m_collection[I].m_buffer_properties.m_pixel_slack;
  }

  void
  end_layer_effect(void) const;

  void
  end_layer_blit(void) const;

  void
  end_layer_effect_of_collection(unsigned int) const;

  RenderEncoderImage m_encoder;
  RenderEncoderBase m_parent_encoder;

  /* commont values regardless if for color blit or Effect blit */
  Transformation m_transformation;
  enum blend_mode_t m_blend_mode;
  ItemMask m_clip;
  BoundingBox<float> m_rect;

  /* parameters if a color blit, only used if m_effect_data is nullptr */
  vec4 m_color;
  enum filter_t m_filter_mode;

  /* non-null only when an effect or effect collection is applied */
  EffectData *m_effect_data;

  bool m_end_layer_called;

  /* used to back RelativeBoundingBox::m_pixel_bb of effect_rect() */
  BoundingBox<float> m_pixel_bb_backing;
};

#endif

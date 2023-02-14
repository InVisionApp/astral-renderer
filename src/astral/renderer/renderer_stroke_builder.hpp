/*!
 * \file render_stroke_builder.hpp
 * \brief file render_stroke_builder.hpp
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

#ifndef ASTRAL_RENDER_STROKE_BUILDER_HPP
#define ASTRAL_RENDER_STROKE_BUILDER_HPP

#include <astral/renderer/renderer.hpp>
#include <astral/renderer/shader/stroke_query.hpp>
#include "renderer_implement.hpp"
#include "renderer_cached_transformation.hpp"
#include "renderer_cull_geometry.hpp"

class astral::RenderEncoderStrokeMask::Backing:astral::noncopyable
{
public:
  enum transformation_changed_bit_masks_t:uint32_t
    {
      transformation_matrix_changed = 1u,
      transformation_translation_changed = 2u,
      transformation_completely_changed = transformation_matrix_changed | transformation_translation_changed,
    };

  Backing(void);

  ~Backing();

  void
  clear(void);

  void
  init(Renderer::Implement &renderer,
       const Renderer::Implement::CullGeometryGroup &parent_cull_geometry,
       const StrokeMaskProperties &mask_params,
       const Transformation &pixel_transformation_logical,
       float render_accuracy);

  float
  render_scale_factor(void) const;

private:
  friend class astral::RenderEncoderStrokeMask;
  class DataEntry;

  enum dirty_t:uint32_t
    {
      active_primitives_dirty = 1u,
      item_data_dirty = 2u,
      stroke_radii_dirty = 4u,
      tol_dirty = 8u,
      caps_joins_collapse_dirty = 16u,

      all_dirty = ~0u
    };

  void
  ready_derived_data(void);

  void
  on_change_packer_or_stroke_params(void)
  {
    uint32_t M;

    M = active_primitives_dirty
      | item_data_dirty
      | stroke_radii_dirty
      | caps_joins_collapse_dirty;

    m_dirty_mask |= M;

    m_join_expanse_factor = m_current_packer->join_stroke_inflate_factor_with_miter(m_current_stroke_params.m_miter_limit,
                                                                                    m_current_stroke_params.m_join,
                                                                                    m_current_stroke_params.m_cap);

    m_edge_expanse_factor = m_current_packer->edge_stroke_inflate_factor(m_current_stroke_params.m_join,
                                                                         m_current_stroke_params.m_cap);
  }

  void
  on_change_transformation(enum RenderEncoderStrokeMask::transformation_type_t tp, uint32_t mask)
  {
    uint32_t s, v0, v1;

    s = (tp == RenderEncoderStrokeMask::pixel_transformation_logical) ?
      caps_joins_collapse_dirty : 0u;

    v0 = (mask & transformation_matrix_changed) ?
      tol_dirty | s:
      0u;

    v1 = (tp == RenderEncoderStrokeMask::logical_transformation_path) ?
      item_data_dirty :
      0u;

    m_dirty_mask |= (v0 | v1);
  }

  ItemData
  create_stroke_item_data(RenderValue<Transformation> logical_transformation_path, float animation_t) const;

  /* Bounding box values are in path coordinates,
   * returns true if any of the boxes intersect the
   * limiting pixel rect coming from the parent
   * clip geoemtry.
   *
   * Returns true if any of the bb's intersect the
   * the bounding region of m_mask_params.m_restrict_bb
   */
  bool
  add_bb(BoundingBox<float> edge_bb,
         BoundingBox<float> join_bb,
         BoundingBox<float> cap_bb);

  BoundingBox<float>
  compute_cap_bb(const Contour &contour);

  BoundingBox<float>
  compute_cap_bb(const AnimatedContour &contour);

  void
  add_combined_path(const CombinedPath &path);

  template<typename T>
  void
  add_combined_path_worker(const CombinedPath &path);

  void
  compute_mask(void);

  void
  add_ref(const Contour &p)
  {
    m_ref_contours.push_back(&p);
  }

  void
  add_ref(const AnimatedContour &p)
  {
    m_ref_animated_contours.push_back(&p);
  }

  Renderer::Implement *m_renderer;

  StrokeMaskProperties m_mask_params;
  BoundingBox<float> m_restrict_bb_backing;
  float m_render_accuracy;
  Renderer::Implement::CullGeometryGroup m_parent_cull_geometry;

  vecN<std::vector<Renderer::Implement::CachedTransformation>, 2> m_transformation;

  reference_counted_ptr<const MaskStrokeShader> m_current_shader;
  StrokeParameters m_current_stroke_params;
  float m_current_t, m_join_expanse_factor, m_edge_expanse_factor;
  StrokeShader::ItemDataPacker m_null_packer;
  const StrokeShader::ItemDataPackerBase *m_current_packer;
  bool m_caps_joins_collapse;
  ItemMask m_current_clip;
  enum mask_item_shader_clip_mode_t m_current_clip_mode;

  uint32_t m_dirty_mask;
  ItemData m_item_data;
  StrokeQuery::StrokeRadii m_stroke_radii;
  float m_tol;
  vecN<StrokeQuery::ActivePrimitives, StrokeShader::path_shader_count> m_active_primitives;

  /* data to render */
  std::vector<DataEntry> m_contours;

  /* save ref's to source Contour and AnimatedContour objects */
  std::vector<reference_counted_ptr<const Contour>> m_ref_contours;
  std::vector<reference_counted_ptr<const AnimatedContour>> m_ref_animated_contours;

  /* the mask as a clip element */
  vecN<reference_counted_ptr<const RenderClipElement>, number_mask_type> m_clip_element;

  /* bounding box in pixel coordinates of strokes */
  BoundingBox<float> m_pixel_bb;

  bool m_mask_ready;
  vecN<MaskDetails, number_mask_type> m_mask_details;
};


#endif

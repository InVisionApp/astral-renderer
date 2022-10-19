/*!
 * \file render_stroke_builder.cpp
 * \brief file render_stroke_builder.cpp
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

#include <type_traits>
#include "renderer_stroke_builder.hpp"
#include "renderer_shared_util.hpp"
#include "renderer_workroom.hpp"
#include "renderer_storage.hpp"
#include "renderer_virtual_buffer.hpp"

namespace
{
  template<typename T>
  class TimeMatters
  {};

  template<>
  class TimeMatters<astral::Path>
  {
  public:
    typedef std::false_type value;
  };

  template<>
  class TimeMatters<astral::AnimatedPath>
  {
  public:
    typedef std::true_type value;
  };
}

class astral::RenderEncoderStrokeMask::Backing::DataEntry
{
public:
  DataEntry(const Backing &src, const StrokeShader::CookedData &cooked_data):
    m_shader(src.m_current_shader),
    m_item_data(src.m_item_data),
    m_pixel_transformation_logical(src.m_transformation[RenderEncoderStrokeMask::pixel_transformation_logical].back().render_value(*src.m_renderer)),
    m_logical_transformation_path(src.m_transformation[RenderEncoderStrokeMask::logical_transformation_path].back().render_value(*src.m_renderer)),
    m_active_attributes(src.m_active_primitives[cooked_data.path_shader()]),
    m_stroke_radii(src.m_stroke_radii),
    m_cap(src.m_current_stroke_params.m_cap),
    m_join(src.m_current_stroke_params.m_join),
    m_glue_join(src.m_current_stroke_params.m_glue_join),
    m_glue_cusp_join(src.m_current_stroke_params.m_glue_cusp_join),
    m_cooked_data(&cooked_data),
    m_t(src.m_current_t),
    m_clip(src.m_current_clip),
    m_clip_mode(src.m_current_clip_mode)
  {}

  DataEntry(const Backing &src, RenderValue<Transformation> logical_transformation_path,
            ItemData item_data, float t, const StrokeShader::CookedData &cooked_data):
    m_shader(src.m_current_shader),
    m_item_data(item_data),
    m_pixel_transformation_logical(src.m_transformation[RenderEncoderStrokeMask::pixel_transformation_logical].back().render_value(*src.m_renderer)),
    m_logical_transformation_path(logical_transformation_path),
    m_active_attributes(src.m_active_primitives[cooked_data.path_shader()]),
    m_stroke_radii(src.m_stroke_radii),
    m_cap(src.m_current_stroke_params.m_cap),
    m_join(src.m_current_stroke_params.m_join),
    m_glue_join(src.m_current_stroke_params.m_glue_join),
    m_glue_cusp_join(src.m_current_stroke_params.m_glue_cusp_join),
    m_cooked_data(&cooked_data),
    m_t(t),
    m_clip(src.m_current_clip),
    m_clip_mode(src.m_current_clip_mode)
  {}

  void
  add_to_query(StrokeQuery &stroke_query, unsigned int client_id) const
  {
    stroke_query.add_element(client_id,
                             m_pixel_transformation_logical.value(),
                             m_logical_transformation_path.value(),
                             *m_cooked_data, m_t, m_active_attributes,
                             m_stroke_radii);
  }

  void
  draw_content(RenderEncoderMask dst, const DataEntry *last_entry,
               const StrokeQuery::Source &content) const;

private:
  void
  draw_content_helper(RenderEncoderMask dst, const StrokeQuery::Source &content,
                      enum StrokeShader::primitive_type_t P, const MaskItemShader *shader) const;

  /* what StrokeShader to use */
  reference_counted_ptr<const MaskStrokeShader> m_shader;

  /* what ItemData to use */
  ItemData m_item_data;

  /* transformation from logical to pixel coordinates */
  RenderValue<Transformation> m_pixel_transformation_logical;

  /* transformation from path to logical coordinates */
  RenderValue<Transformation> m_logical_transformation_path;

  /* stroking parameters */
  StrokeQuery::ActivePrimitives m_active_attributes;
  StrokeQuery::StrokeRadii m_stroke_radii;
  enum cap_t m_cap;
  enum join_t m_join, m_glue_join, m_glue_cusp_join;

  /* the StrokeShader::CookedData extracted from m_data */
  const StrokeShader::CookedData *m_cooked_data;

  /* animation time if an animated contour */
  float m_t;

  /* how to clip */
  ItemMask m_clip;
  enum mask_item_shader_clip_mode_t m_clip_mode;
};

/////////////////////////////////////////
// astral::RenderEncoderStrokeMask::Backing::DataEntry methods
void
astral::RenderEncoderStrokeMask::Backing::DataEntry::
draw_content(RenderEncoderMask dst, const DataEntry *last_entry, const StrokeQuery::Source &content) const
{
  enum StrokeShader::path_shader_t path_shader(m_cooked_data->path_shader());
  const MaskStrokeShader::ShaderSet &shader(m_shader->shader_set(m_cap));
  const MaskStrokeShader::ItemShaderSet &shader_subset(shader.m_subset[path_shader]);

  if (!last_entry || last_entry->m_pixel_transformation_logical != m_pixel_transformation_logical)
    {
      dst.transformation(m_pixel_transformation_logical);
    }

  draw_content_helper(dst, content, StrokeShader::line_segments_primitive,
                      shader_subset.m_line_segment_shader.get());

  draw_content_helper(dst, content, StrokeShader::biarc_curves_primitive,
                      shader_subset.m_biarc_curve_shader.get());

  draw_content_helper(dst, content, StrokeShader::segments_cappers_primitive,
                      shader_subset.m_line_capper_shaders[StrokeShader::capper_shader_start].get());

  draw_content_helper(dst, content, StrokeShader::segments_cappers_primitive,
                      shader_subset.m_line_capper_shaders[StrokeShader::capper_shader_end].get());

  draw_content_helper(dst, content, StrokeShader::biarc_curves_cappers_primitive,
                      shader_subset.m_quadratic_capper_shaders[StrokeShader::capper_shader_start].get());

  draw_content_helper(dst, content, StrokeShader::biarc_curves_cappers_primitive,
                      shader_subset.m_quadratic_capper_shaders[StrokeShader::capper_shader_end].get());

  draw_content_helper(dst, content, StrokeShader::glue_primitive,
                      shader_subset.m_inner_glue_shader.get());

  if (m_glue_join != join_none)
    {
      draw_content_helper(dst, content, StrokeShader::glue_primitive,
                          shader_subset.m_join_shaders[m_glue_join].get());
    }

  if (m_glue_cusp_join != join_none)
    {
      draw_content_helper(dst, content, StrokeShader::glue_cusp_primitive,
                          shader_subset.m_join_shaders[m_glue_cusp_join].get());
    }

  draw_content_helper(dst, content, StrokeShader::inner_glue_primitive,
                      shader_subset.m_inner_glue_shader.get());

  if (m_join != join_none)
    {
      draw_content_helper(dst, content, StrokeShader::joins_primitive,
                          shader_subset.m_join_shaders[m_join].get());
    }

  if (m_cap != cap_flat)
    {
      draw_content_helper(dst, content, StrokeShader::caps_primitive,
                          shader_subset.m_cap_shader.get());
    }
}

void
astral::RenderEncoderStrokeMask::Backing::DataEntry::
draw_content_helper(RenderEncoderMask dst, const StrokeQuery::Source &content,
                    enum StrokeShader::primitive_type_t P,
                    const MaskItemShader *shader) const
{
  if (!shader || !m_active_attributes.value(P))
    {
      return;
    }

  c_array<const range_type<int>> ranges(content.vertex_ranges(P));
  if (ranges.empty())
    {
      return;
    }

  RenderEncoderMask::Item item(*shader, m_item_data,
                               m_cooked_data->vertex_data(P),
                               ranges);

  dst.draw_generic(item, m_clip, m_clip_mode);
}

//////////////////////////////////////////
// astral::RenderEncoderStrokeMask::Backing methods
astral::RenderEncoderStrokeMask::Backing::
Backing(void):
  m_renderer(nullptr)
{}

astral::RenderEncoderStrokeMask::Backing::
~Backing()
{}

void
astral::RenderEncoderStrokeMask::Backing::
clear(void)
{
  m_renderer = nullptr;
  std::fill(m_mask_details.begin(), m_mask_details.end(), MaskDetails());
  m_current_clip = ItemMask();
  m_current_shader.clear();
  m_contours.clear();
  m_transformation[RenderEncoderStrokeMask::pixel_transformation_logical].clear();
  m_transformation[RenderEncoderStrokeMask::logical_transformation_path].clear();
  m_pixel_bb.clear();

  m_ref_contours.clear();
  m_ref_animated_contours.clear();
  for (unsigned int m = 0; m < m_clip_element.size(); ++m)
    {
      m_clip_element[m].clear();
    }
}

void
astral::RenderEncoderStrokeMask::Backing::
init(Renderer::Implement &renderer,
     const Renderer::Implement::ClipGeometryGroup &parent_clip_geometry,
     const StrokeMaskProperties &mask_params,
     const Transformation &pixel_transformation_logical,
     float render_accuracy)
{
  ASTRALassert(m_renderer == nullptr);
  ASTRALassert(m_contours.empty());
  ASTRALassert(m_pixel_bb.empty());
  ASTRALassert(m_transformation[RenderEncoderStrokeMask::pixel_transformation_logical].empty());
  ASTRALassert(m_transformation[RenderEncoderStrokeMask::logical_transformation_path].empty());

  m_renderer = &renderer;
  m_mask_params = mask_params;
  m_render_accuracy = render_accuracy;
  m_parent_clip_geometry = parent_clip_geometry;
  m_current_clip_mode = mask_item_shader_clip_cutoff;

  m_mask_params.m_restrict_bb = &m_restrict_bb_backing;
  m_restrict_bb_backing = parent_clip_geometry.bounding_geometry().pixel_rect();
  if (mask_params.m_restrict_bb)
    {
      m_restrict_bb_backing.intersect_against(*mask_params.m_restrict_bb);
    }
  m_mask_ready = false;

  m_current_shader = m_renderer->m_engine->default_shaders().m_mask_stroke_shader;
  m_current_stroke_params = StrokeParameters();
  m_current_t = 0.0f;
  m_current_packer = &m_null_packer;
  m_dirty_mask = all_dirty;
  on_change_packer_or_stroke_params();

  m_transformation[RenderEncoderStrokeMask::pixel_transformation_logical].push_back(pixel_transformation_logical);
  m_transformation[RenderEncoderStrokeMask::logical_transformation_path].push_back(Transformation());
}

astral::vec2
astral::RenderEncoderStrokeMask::Backing::
render_scale_factor(void) const
{
  vec2 sf(m_mask_params.m_render_scale_factor.m_scale_factor);
  if (m_mask_params.m_render_scale_factor.m_relative)
    {
      sf *= m_parent_clip_geometry.bounding_geometry().scale_factor();
    }

  return sf;
}

astral::ItemData
astral::RenderEncoderStrokeMask::Backing::
create_stroke_item_data(RenderValue<Transformation> logical_transformation_path, float animation_t) const
{
  unsigned int sz;
  c_array<gvec4> packed_data;

  sz = m_current_packer->item_data_size(m_current_stroke_params);
  m_renderer->m_workroom->m_item_data_workroom.resize(sz);

  packed_data = make_c_array(m_renderer->m_workroom->m_item_data_workroom);
  m_current_packer->pack_item_data(logical_transformation_path, m_current_stroke_params, animation_t, packed_data);

  return m_renderer->create_item_data(packed_data, m_current_packer->intrepreted_value_map());
}

void
astral::RenderEncoderStrokeMask::Backing::
ready_derived_data(void)
{
  if (m_dirty_mask & item_data_dirty)
    {
      const auto &logical_transformation_path(m_transformation[RenderEncoderStrokeMask::logical_transformation_path].back());

      m_item_data = create_stroke_item_data(logical_transformation_path.render_value(*m_renderer), m_current_t);
    }

  if (m_dirty_mask & stroke_radii_dirty)
    {
      m_stroke_radii = StrokeQuery::StrokeRadii(m_current_stroke_params, *m_current_packer);
    }

  if (m_dirty_mask & tol_dirty)
    {
      const auto &pixel_transformation_logical(m_transformation[RenderEncoderStrokeMask::pixel_transformation_logical].back());
      const auto &logical_transformation_path(m_transformation[RenderEncoderStrokeMask::logical_transformation_path].back());

      m_tol = pixel_transformation_logical.compute_tol(m_render_accuracy, &logical_transformation_path.transformation().m_matrix);
    }

  if (m_dirty_mask & caps_joins_collapse_dirty)
    {
      const auto &pixel_transformation_logical(m_transformation[RenderEncoderStrokeMask::pixel_transformation_logical].back());
      m_caps_joins_collapse = m_current_packer->caps_joins_collapse(pixel_transformation_logical.transformation().m_matrix,
                                                                    render_scale_factor(), m_current_stroke_params);
    }

  if (m_dirty_mask & active_primitives_dirty)
    {
      for (unsigned int i = 0; i < StrokeShader::path_shader_count; ++i)
        {
          enum StrokeShader::path_shader_t P;

          P = static_cast<enum StrokeShader::path_shader_t>(i);
          m_active_primitives[P] = StrokeQuery::ActivePrimitives(m_caps_joins_collapse, m_current_stroke_params,
                                                                 m_current_shader->shader_set(m_current_stroke_params.m_cap),
                                                                 P);
        }
    }

  m_dirty_mask = 0u;
}

bool
astral::RenderEncoderStrokeMask::Backing::
add_bb(BoundingBox<float> edge_bb,
       BoundingBox<float> join_bb,
       BoundingBox<float> cap_bb)
{
  float r;
  const Transformation &logical_transformation_path(m_transformation[RenderEncoderStrokeMask::logical_transformation_path].back().transformation());
  const Transformation &pixel_transformation_logical(m_transformation[RenderEncoderStrokeMask::pixel_transformation_logical].back().transformation());
  bool return_value(false);

  ASTRALassert(m_mask_params.m_restrict_bb == &m_restrict_bb_backing);

  r = t_max(0.0f, m_current_stroke_params.m_width * 0.5f);
  if (m_current_stroke_params.m_draw_edges)
    {
      edge_bb = logical_transformation_path.apply_to_bb(edge_bb);
      edge_bb.enlarge(vec2(m_edge_expanse_factor * r));
      edge_bb = pixel_transformation_logical.apply_to_bb(edge_bb);
      if (r <= 0.0f)
        {
          edge_bb.enlarge(vec2(StrokeParameters::hairline_pixel_radius()));
        }

      edge_bb.intersect_against(*m_mask_params.m_restrict_bb);
      return_value = return_value || !edge_bb.empty();

      m_pixel_bb.union_box(edge_bb);
    }

  if (m_current_stroke_params.m_join != join_none && r > 0.0f)
    {
      join_bb = logical_transformation_path.apply_to_bb(join_bb);
      join_bb.enlarge(vec2(m_join_expanse_factor * r));
      join_bb = pixel_transformation_logical.apply_to_bb(join_bb);

      join_bb.intersect_against(*m_mask_params.m_restrict_bb);
      return_value = return_value || !join_bb.empty();

      m_pixel_bb.union_box(join_bb);
    }

  if (m_current_stroke_params.m_cap != cap_flat && r > 0.0f)
    {
      float f;

      f = (m_current_stroke_params.m_cap == cap_square) ? 1.0f : ASTRAL_SQRT2;
      cap_bb = logical_transformation_path.apply_to_bb(cap_bb);
      cap_bb.enlarge(vec2(f * r));
      cap_bb = pixel_transformation_logical.apply_to_bb(cap_bb);

      cap_bb.intersect_against(*m_mask_params.m_restrict_bb);
      return_value = return_value || !cap_bb.empty();

      m_pixel_bb.union_box(cap_bb);
    }

  return return_value;
}

astral::BoundingBox<float>
astral::RenderEncoderStrokeMask::Backing::
compute_cap_bb(const Contour &contour)
{
  BoundingBox<float> return_value;

  if (m_current_stroke_params.m_cap != cap_flat && !contour.closed() && !contour.curves().empty())
    {
      return_value.union_point(contour.curves().front().start_pt());
      return_value.union_point(contour.curves().front().end_pt());
    }

  return return_value;
}

astral::BoundingBox<float>
astral::RenderEncoderStrokeMask::Backing::
compute_cap_bb(const AnimatedContour &contour)
{
  BoundingBox<float> return_value;

  if (m_current_stroke_params.m_cap != cap_flat && !contour.closed() && !contour.start_contour().curves().empty())
    {
      vec2 p0, p1;

      p0 = contour.start_contour().curves().front().start_pt();
      p1 = contour.end_contour().curves().front().start_pt();
      return_value.union_point(m_current_t * p0 + (1.0f - m_current_t) * p1);

      p0 = contour.start_contour().curves().front().end_pt();
      p1 = contour.end_contour().curves().front().end_pt();
      return_value.union_point(m_current_t * p0 + (1.0f - m_current_t) * p1);
    }

  return return_value;
}

template<typename T>
void
astral::RenderEncoderStrokeMask::Backing::
add_combined_path_worker(const CombinedPath &combined_path)
{
  /* minor tricks to help performance:
   *   1. Do the bounding box test *once*.
   *   2. Directly handle/add the translation and matrix entries
   *   3. Directly make ItemData for animated paths instead of using
   *      the cached value
   */
  constexpr bool time_matters = typename TimeMatters<T>::value();
  Renderer::Implement::CachedTransformation &cached_logical_transformation_path(m_transformation[logical_transformation_path].back());
  Renderer::Implement::CachedTransformation &cached_pixel_transformation_logical(m_transformation[pixel_transformation_logical].back());
  auto paths(combined_path.paths<T>());

  for (unsigned int i = 0, endi = paths.size(); i < endi; ++i)
    {
      const vec2 *translate;
      const float2x2 *matrix;
      RenderValue<Transformation> logical_transformation_path;
      float t, tol;
      ItemData item_data;
      const T &path(*paths[i]);

      translate = combined_path.get_translate<T>(i);
      matrix = combined_path.get_matrix<T>(i);
      t = combined_path.get_t<T>(i);

      logical_transformation_path = cached_logical_transformation_path.create_transformation(*m_renderer, translate, matrix);
      if (matrix)
        {
          float2x2 temp;

          temp = cached_logical_transformation_path.transformation().m_matrix * (*matrix);
          tol = cached_pixel_transformation_logical.compute_tol(m_render_accuracy, &temp);
        }
      else
        {
          tol = m_tol;
        }

      if (matrix || translate || (time_matters && t != m_current_t))
        {
          item_data = create_stroke_item_data(logical_transformation_path, t);
        }
      else
        {
          item_data = m_item_data;
        }

      for (unsigned int c = 0, end_c = path.number_contours(); c < end_c; ++c)
        {
          const StrokeShader::CookedData *cooked_data;

          cooked_data = &path.contour(c).stroke_render_data(tol, *m_renderer->m_engine);
          m_contours.push_back(DataEntry(*this, logical_transformation_path, item_data, t, *cooked_data));
          add_ref(path.contour(c));
        }
    }
}

void
astral::RenderEncoderStrokeMask::Backing::
add_combined_path(const CombinedPath &combined_path)
{
  BoundingBox<float> bb;
  const Transformation &logical_transformation_path(m_transformation[RenderEncoderStrokeMask::logical_transformation_path].back().transformation());
  const Transformation &pixel_transformation_logical(m_transformation[RenderEncoderStrokeMask::pixel_transformation_logical].back().transformation());
  float r(0.5f * m_current_stroke_params.m_width);

  bb = combined_path.compute_bounding_box(r * m_edge_expanse_factor, r * m_join_expanse_factor, m_current_stroke_params.m_cap);
  bb = logical_transformation_path.apply_to_bb(bb);
  if (m_current_stroke_params.m_width <= 0.0f)
    {
      bb.enlarge(vec2(StrokeParameters::hairline_pixel_radius()));
    }
  bb = pixel_transformation_logical.apply_to_bb(bb);

  bb.intersect_against(*m_mask_params.m_restrict_bb);
  if (bb.empty())
    {
      return;
    }

  m_pixel_bb.union_box(bb);
  ready_derived_data();

  add_combined_path_worker<Path>(combined_path);
  add_combined_path_worker<AnimatedPath>(combined_path);
}

void
astral::RenderEncoderStrokeMask::Backing::
compute_mask(void)
{
  ASTRALassert(!m_mask_ready);
  ASTRALassert(m_renderer);

  m_mask_ready = true;

  /* Step 1: figure out the bounding box in pixel coordinates */
  BoundingBox<float> bb;

  bb = m_pixel_bb;
  if (m_mask_params.m_restrict_bb)
    {
      bb.intersect_against(*m_mask_params.m_restrict_bb);
    }

  if (bb.empty())
    {
      /* mask is empty, early out */
      return;
    }

  /* Step 2: construct the necessary ClipGeoemtry for bb */
  const Transformation identity;
  const float identity_norm(1.0f);
  const int pixel_padding(ImageAtlas::tile_padding);
  Renderer::Implement::ClipGeometryGroup clip_geometry;

  clip_geometry = Renderer::Implement::ClipGeometryGroup(*m_renderer, identity, identity_norm, render_scale_factor(),
                                                         bb, m_parent_clip_geometry, pixel_padding);

  if (clip_geometry.bounding_geometry().image_size() == ivec2(0, 0))
    {
      /* mask will be empty anyways */
      return;
    }

  /* Step 3: run the query */
  Renderer::Implement::WorkRoom::Stroke &workroom(m_renderer->m_workroom->m_stroke);
  StrokeQuery &stroke_query(*workroom.m_query);
  ivec2 rect_size(clip_geometry.bounding_geometry().image_size());
  Transformation image_transformation_pixel(clip_geometry.bounding_geometry().image_transformation_pixel());

  /* Recall that clip_geometry.sub_rects() is empty if it has not clipping sub-geometry
   * and that StrokeQuery::begin_query() interprets an empty array as that all of the
   * rect specified by rect_size is covered.
   */
  stroke_query.begin_query(clip_geometry.bounding_geometry().image_transformation_pixel(),
                           rect_size, m_mask_params.m_sparse_mask,
                           clip_geometry.sub_rects(*m_renderer->m_storage));

  unsigned int client_id(0u);
  for (const auto &c : m_contours)
    {
      c.add_to_query(stroke_query, client_id++);
    }

  stroke_query.end_query(Renderer::VirtualBuffer::max_renderable_buffer_size);

  /* Step 3: make the image and the needed encoders */
  unsigned int count;
  RenderEncoderMask encoder;

  count = stroke_query.elements().size();
  workroom.m_render_encoders.clear();

  if (count == 0)
    {
      /* empty mask, nothing left to do */
      return;
    }

  if (count == 1)
    {
      encoder = m_renderer->m_storage->create_virtual_buffer(VB_TAG, Transformation(), clip_geometry, number_fill_rule,
                                                             Renderer::VirtualBuffer::ImageCreationSpec());
      workroom.m_render_encoders.push_back(encoder);
    }
  else
    {
      workroom.m_tmp_tile_regions.resize(count);
      workroom.m_tmp_virtual_buffer_pointers.clear();
      workroom.m_tmp_virtual_buffer_pointers.resize(count, nullptr);
      workroom.m_render_encoders.clear();
      workroom.m_render_encoders.resize(count);

      for (unsigned int i = 0; i < count; ++i)
        {
          workroom.m_tmp_tile_regions[i] = stroke_query.elements()[i].tile_range();
        }

      encoder = m_renderer->m_storage->create_virtual_buffer(VB_TAG, Transformation(), clip_geometry, number_fill_rule,
                                                             make_c_array(workroom.m_tmp_tile_regions),
                                                             make_c_array(workroom.m_tmp_virtual_buffer_pointers));

      for (unsigned int i = 0; i < count; ++i)
        {
          workroom.m_render_encoders[i] = RenderEncoderMask(workroom.m_tmp_virtual_buffer_pointers[i]);
        }
    }

  /* Step 4: draw the content to the encoders */
  for (unsigned int i = 0; i < workroom.m_render_encoders.size(); ++i)
    {
      const StrokeQuery::ResultRect &q(stroke_query.elements()[i]);
      const DataEntry *last_entry(nullptr);

      for (const StrokeQuery::Source &s : q.sources())
        {
          unsigned int client_id(s.ID());
          m_contours[client_id].draw_content(workroom.m_render_encoders[i], last_entry, s);
          last_entry = &m_contours[client_id];
        }
    }

  encoder.finish();

  /* Step 5: fill the fields of m_mask_details */
  m_mask_details[0].m_mask = encoder.image();
  m_mask_details[0].m_mask_transformation_pixel = clip_geometry.bounding_geometry().image_transformation_pixel();
  if (m_mask_details[0].m_mask)
    {
      vec2 tr(pixel_padding, pixel_padding);

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
      m_mask_details[0].m_min_corner = tr;
      m_mask_details[0].m_size = vec2(m_mask_details[0].m_mask->size() - uvec2(2u * pixel_padding));
      m_mask_details[0].m_mask_transformation_pixel.m_translate -= tr;
    }
  else
    {
      m_mask_details[0].m_min_corner = vec2(0);
      m_mask_details[0].m_size = vec2(0);
    }

  vecN<enum mask_channel_t, number_mask_type> mask_channels;
  for (unsigned int i = 0; i < m_mask_details.size(); ++i)
    {
      if (i != 0)
        {
          m_mask_details[i] = m_mask_details[0];
        }

      enum mask_type_t m;

      m = static_cast<enum mask_type_t>(i);
      m_mask_details[m].m_mask_type = m;
      mask_channels[m] = m_mask_details[m].m_mask_channel = RenderEncoderStrokeMask::mask_channel(m);
    }

  for (unsigned int m = 0; m < number_mask_type; ++m)
    {
      m_clip_element[m] = m_renderer->m_storage->create_clip_element(clip_geometry.bounding_geometry(),
                                                                     clip_geometry.token(),
                                                                     encoder.image(),
                                                                     mask_channels,
                                                                     static_cast<enum mask_type_t>(m));
    }
}

//////////////////////////////////////////
// astral::RenderEncoderStrokeMask methods
astral::Renderer&
astral::RenderEncoderStrokeMask::
renderer(void) const
{
  ASTRALassert(valid());
  return *builder().m_renderer;
}

astral::RenderEngine&
astral::RenderEncoderStrokeMask::
render_engine(void) const
{
  ASTRALassert(valid());
  return *builder().m_renderer->m_engine;
}

float
astral::RenderEncoderStrokeMask::
render_accuracy(void) const
{
  ASTRALassert(valid());
  return builder().m_render_accuracy;
}

void
astral::RenderEncoderStrokeMask::
render_accuracy(float v) const
{
  ASTRALassert(valid());
  builder().m_dirty_mask |= Backing::tol_dirty;
  builder().m_render_accuracy = v;
}

const astral::Transformation&
astral::RenderEncoderStrokeMask::
transformation(enum transformation_type_t tp) const
{
  return builder().m_transformation[tp].back().transformation();
}

astral::RenderValue<astral::Transformation>
astral::RenderEncoderStrokeMask::
transformation_value(enum transformation_type_t tp) const
{
  return builder().m_transformation[tp].back().render_value(*builder().m_renderer);
}

astral::vec2
astral::RenderEncoderStrokeMask::
singular_values(enum transformation_type_t tp) const
{
  return builder().m_transformation[tp].back().singular_values();
}

const astral::Transformation&
astral::RenderEncoderStrokeMask::
inverse_transformation(enum transformation_type_t tp) const
{
  return builder().m_transformation[tp].back().inverse();
}

void
astral::RenderEncoderStrokeMask::
transformation(enum transformation_type_t tp, const Transformation &v) const
{
  builder().m_transformation[tp].back().transformation(v);
  builder().on_change_transformation(tp, Backing::transformation_completely_changed);
}

void
astral::RenderEncoderStrokeMask::
transformation(enum transformation_type_t tp, RenderValue<Transformation> v) const
{
  builder().m_transformation[tp].back().transformation(v);
  builder().on_change_transformation(tp, Backing::transformation_completely_changed);
}

void
astral::RenderEncoderStrokeMask::
transformation_translate(enum transformation_type_t tp, float x, float y) const
{
  builder().m_transformation[tp].back().transformation_translate(x, y);
  builder().on_change_transformation(tp, Backing::transformation_translation_changed);
}

void
astral::RenderEncoderStrokeMask::
transformation_matrix(enum transformation_type_t tp, const float2x2 &matrix) const
{
  builder().m_transformation[tp].back().transformation_matrix(matrix);
  builder().on_change_transformation(tp, Backing::transformation_matrix_changed);
}

void
astral::RenderEncoderStrokeMask::
concat(enum transformation_type_t tp, const Transformation &v) const
{
  builder().m_transformation[tp].back().concat(v);
  builder().on_change_transformation(tp, Backing::transformation_completely_changed);
}

void
astral::RenderEncoderStrokeMask::
concat(enum transformation_type_t tp, const float2x2 &v) const
{
  builder().m_transformation[tp].back().concat(v);
  builder().on_change_transformation(tp, Backing::transformation_matrix_changed);
}

void
astral::RenderEncoderStrokeMask::
translate(enum transformation_type_t tp, float x, float y) const
{
  builder().m_transformation[tp].back().translate(x, y);
  builder().on_change_transformation(tp, Backing::transformation_translation_changed);
}

void
astral::RenderEncoderStrokeMask::
scale(enum transformation_type_t tp, float sx, float sy) const
{
  builder().m_transformation[tp].back().scale(sx, sy);
  builder().on_change_transformation(tp, Backing::transformation_matrix_changed);
}

void
astral::RenderEncoderStrokeMask::
rotate(enum transformation_type_t tp, float radians) const
{
  builder().m_transformation[tp].back().rotate(radians);
  builder().on_change_transformation(tp, Backing::transformation_matrix_changed);
}

void
astral::RenderEncoderStrokeMask::
save_transformation(enum transformation_type_t tp) const
{
  std::vector<Renderer::Implement::CachedTransformation> &st(builder().m_transformation[tp]);
  st.push_back(st.back());
}

unsigned int
astral::RenderEncoderStrokeMask::
save_transformation_count(enum transformation_type_t tp) const
{
  std::vector<Renderer::Implement::CachedTransformation> &st(builder().m_transformation[tp]);
  ASTRALassert(st.size() >= 1u);

  return st.size() - 1u;
}

void
astral::RenderEncoderStrokeMask::
restore_transformation(enum transformation_type_t tp) const
{
  std::vector<Renderer::Implement::CachedTransformation> &st(builder().m_transformation[tp]);
  ASTRALassert(st.size() >= 2u);

  st.pop_back();
  builder().on_change_transformation(tp, Backing::transformation_completely_changed);
}

void
astral::RenderEncoderStrokeMask::
restore_transformation(enum transformation_type_t tp, unsigned int cnt) const
{
  std::vector<Renderer::Implement::CachedTransformation> &st(builder().m_transformation[tp]);
  cnt = t_min(cnt, save_transformation_count(tp));
  st.resize(cnt + 1u);
  builder().on_change_transformation(tp, Backing::transformation_completely_changed);
}

const astral::StrokeMaskProperties&
astral::RenderEncoderStrokeMask::
mask_params(void) const
{
  return builder().m_mask_params;
}

astral::vec2
astral::RenderEncoderStrokeMask::
render_scale_factor(void) const
{
  return builder().render_scale_factor();
}

void
astral::RenderEncoderStrokeMask::
set_item_clip(const ItemMask &clip, enum mask_item_shader_clip_mode_t clip_mode) const
{
  builder().m_current_clip = clip;
  builder().m_current_clip_mode = clip_mode;
}

void
astral::RenderEncoderStrokeMask::
set_shader(const MaskStrokeShader &shader) const
{
  builder().m_current_shader = &shader;
  builder().m_dirty_mask |= Backing::active_primitives_dirty;
}

void
astral::RenderEncoderStrokeMask::
set_item_packer(const StrokeShader::ItemDataPackerBase *packer) const
{
  builder().m_current_packer = (packer) ? packer : &builder().m_null_packer;
  builder().on_change_packer_or_stroke_params();
}

void
astral::RenderEncoderStrokeMask::
set_stroke_params(const StrokeParameters &stroke_params, float t) const
{
  builder().m_current_stroke_params = stroke_params;
  builder().m_current_t = t;
  builder().on_change_packer_or_stroke_params();
}

void
astral::RenderEncoderStrokeMask::
add_path(const CombinedPath &path) const
{
  builder().add_combined_path(path);
}

void
astral::RenderEncoderStrokeMask::
add_path(const Path &path) const
{
  typedef Backing::DataEntry DataEntry;
  Backing &bb(builder());
  bool should_add;

  should_add = bb.add_bb(path.bounding_box(),
                         path.join_bounding_box(),
                         path.open_contour_endpoint_bounding_box());
  if (should_add)
    {
      bb.ready_derived_data();
      for (unsigned int i = 0, endi = path.number_contours(); i < endi; ++i)
        {
          bb.m_contours.push_back(DataEntry(bb, path.contour(i).stroke_render_data(bb.m_tol, *bb.m_renderer->m_engine)));
          bb.m_ref_contours.push_back(&path.contour(i));
        }
    }
}

void
astral::RenderEncoderStrokeMask::
add_path(const AnimatedPath &path) const
{
  typedef Backing::DataEntry DataEntry;
  Backing &bb(builder());
  bool should_add;
  float t(bb.m_current_t);

  should_add = bb.add_bb(path.bounding_box(t),
                         path.join_bounding_box(t),
                         path.open_contour_endpoint_bounding_box(t));
  if (should_add)
    {
      bb.ready_derived_data();
      for (unsigned int i = 0, endi = path.number_contours(); i < endi; ++i)
        {
          bb.m_contours.push_back(DataEntry(bb, path.contour(i).stroke_render_data(bb.m_tol, *bb.m_renderer->m_engine)));
          bb.m_ref_animated_contours.push_back(&path.contour(i));
        }
    }
}

void
astral::RenderEncoderStrokeMask::
add_contour(const Contour &contour) const
{
  typedef Backing::DataEntry DataEntry;
  Backing &bb(builder());
  bool should_add;

  should_add = bb.add_bb(contour.bounding_box(),
                         contour.join_bounding_box(),
                         bb.compute_cap_bb(contour));

  if (should_add)
    {
      bb.ready_derived_data();
      bb.m_contours.push_back(DataEntry(bb, contour.stroke_render_data(bb.m_tol, *bb.m_renderer->m_engine)));
      bb.m_ref_contours.push_back(&contour);
    }
}

void
astral::RenderEncoderStrokeMask::
add_contour(const AnimatedContour &contour) const
{
  typedef Backing::DataEntry DataEntry;
  Backing &bb(builder());
  float t(bb.m_current_t);
  bool should_add;

  should_add = bb.add_bb(contour.bounding_box(t),
                         contour.join_bounding_box(t),
                         bb.compute_cap_bb(contour));

  if (should_add)
    {
      bb.ready_derived_data();
      bb.m_contours.push_back(DataEntry(bb, contour.stroke_render_data(bb.m_tol, *bb.m_renderer->m_engine)));
      bb.m_ref_animated_contours.push_back(&contour);
    }
}

void
astral::RenderEncoderStrokeMask::
finish(void) const
{
  Backing &bb(builder());

  if (!bb.m_mask_ready)
    {
      bb.compute_mask();
    }
}

const astral::MaskDetails&
astral::RenderEncoderStrokeMask::
mask_details(enum mask_type_t type) const
{
  Backing &bb(builder());

  if (!bb.m_mask_ready)
    {
      bb.compute_mask();
    }

  return bb.m_mask_details[type];
}

const astral::reference_counted_ptr<const astral::RenderClipElement>&
astral::RenderEncoderStrokeMask::
clip_element(enum mask_type_t type) const
{
  Backing &bb(builder());

  if (!bb.m_mask_ready)
    {
      bb.compute_mask();
    }

  return bb.m_clip_element[type];
}

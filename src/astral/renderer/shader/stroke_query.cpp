/*!
 * \file stroke_query.hpp
 * \brief file stroke_query.hpp
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

#include <astral/renderer/image.hpp>
#include "stroke_query_implement.hpp"

///////////////////////////////////////
// astral::StrokeQuery::Source methods
astral::StrokeQuery::Source::
Source(unsigned int client_id, StrokeQuery::Implement &p):
  m_ID(client_id)
{
  for (unsigned int i = 0; i < StrokeShader::number_primitive_types; ++i)
    {
      m_idxs[i] = p.m_ids_backing.allocate();
    }
}

///////////////////////////////////////////////////
// astral::StrokeQuery::StrokeRadii methods
astral::StrokeQuery::StrokeRadii::
StrokeRadii(const StrokeParameters &params,
            const StrokeShader::ItemDataPackerBase &packer)
{
  static const float cap_factors[number_cap_t] =
    {
      [cap_flat] = 0.0f,
      [cap_rounded] = 1.0f,
      [cap_square] = cap_square,
    };

  const float r(params.m_width * 0.5f);

  m_edge_radius = (params.m_draw_edges) ?
    r * packer.edge_stroke_inflate_factor(params.m_join, params.m_cap) :
    0.0f;

  m_join_radius = (params.m_join != join_none) ?
    r * packer.join_stroke_inflate_factor(params.m_join, params.m_cap) :
    0.0f;

  /* join_stroke_inflate_factor() does NOT take into account miter-joins, so
   * we must do it here.
   *
   * TODO: instead of a blind radius for joins, we hold onto the miter-limit
   *       and have the query calculate the actul miter.
   */
  if (params.m_join == join_miter)
    {
      m_join_radius = t_max(m_join_radius, r * params.m_miter_limit);
    }

  m_cap_radius = cap_factors[params.m_cap] * r;

  m_max_radius = t_max(m_edge_radius, t_max(m_cap_radius, m_join_radius));
}

///////////////////////////////////////////////////
// astral::StrokeQuery::ActivePrimitives methods
astral::StrokeQuery::ActivePrimitives::
ActivePrimitives(bool caps_joins_collapse,
                 const StrokeParameters &stroke_params,
                 bool include_inner_glue,
                 const MaskStrokeShader::ItemShaderSet *shaders)
{
  if (stroke_params.m_draw_edges)
    {
      if (!shaders || shaders->m_line_segment_shader)
        {
          value(StrokeShader::line_segments_primitive, true);
        }

      if (!shaders || shaders->m_biarc_curve_shader)
        {
          value(StrokeShader::biarc_curves_primitive, true);
        }

      if (!caps_joins_collapse && stroke_params.m_width > 0.0f)
        {
          if (!shaders
              || shaders->m_line_capper_shaders[StrokeShader::capper_shader_start]
              || shaders->m_line_capper_shaders[StrokeShader::capper_shader_end])
            {
              value(StrokeShader::segments_cappers_primitive, true);
            }

          if (!shaders
              || shaders->m_quadratic_capper_shaders[StrokeShader::capper_shader_start]
              || shaders->m_quadratic_capper_shaders[StrokeShader::capper_shader_end])
            {
              value(StrokeShader::biarc_curves_cappers_primitive, true);
            }

          if (stroke_params.m_glue_join != join_none
              && (!shaders || shaders->m_join_shaders[stroke_params.m_glue_join]))
            {
              value(StrokeShader::glue_primitive, true);
            }

          if (stroke_params.m_glue_cusp_join != join_none
              && (!shaders || shaders->m_join_shaders[stroke_params.m_glue_cusp_join]))
            {
              value(StrokeShader::glue_cusp_primitive, true);
            }

          if (include_inner_glue && (!shaders || shaders->m_inner_glue_shader))
            {
              value(StrokeShader::inner_glue_primitive, true);
            }
        }
    }

  if (!caps_joins_collapse && stroke_params.m_width > 0.0f)
    {
      if (!shaders || shaders->m_cap_shader)
        {
          value(StrokeShader::caps_primitive, true);
        }

      if (stroke_params.m_join != join_none
          && (!shaders || shaders->m_join_shaders[stroke_params.m_join]))
        {
          value(StrokeShader::joins_primitive, true);
        }
    }
}

////////////////////////////////////////////
// astral::StrokeQuery::ResultRect methods
astral::StrokeQuery::ResultRect::
ResultRect(StrokeQuery::Implement &p):
  m_sources(p.m_query_src_pool.allocate())
{}

///////////////////////////////////////////////////////////
// astral::StrokeQuery::Implement::RectHierarchy methods
astral::StrokeQuery::Implement::RectHierarchy::
RectHierarchy(range_type<int> x, range_type<int> y):
  m_range(x, y),
  m_children(nullptr, nullptr),
  m_query_result(nullptr)
{
  int dx(x.m_end - x.m_begin), dy(y.m_end - y.m_begin);

  ASTRALassert(dx > 0 && dy > 0);
  if (dx == 1 && dy == 1)
    {
      m_splitting_coordinate = unlit_leaf;
      m_splitting_value = -1;
    }
  else
    {
      m_splitting_coordinate = (dx > dy) ? 0 : 1;
      m_splitting_value = (m_range[m_splitting_coordinate].m_begin + m_range[m_splitting_coordinate].m_end) / 2;

      ASTRALassert(m_splitting_value > m_range[m_splitting_coordinate].m_begin);
      ASTRALassert(m_splitting_value < m_range[m_splitting_coordinate].m_end);
    }
}

void
astral::StrokeQuery::Implement::RectHierarchy::
insert(RectHierarchyPool &pool, ivec2 R)
{
  ASTRALassert(R.x() >= m_range.x().m_begin);
  ASTRALassert(R.x() < m_range.x().m_end);
  ASTRALassert(R.y() >= m_range.y().m_begin);
  ASTRALassert(R.y() < m_range.y().m_end);

  if (is_node())
    {
      if (!m_children[0])
        {
          /* create the children */
          vecN<range_type<int>, 2> V;

          ASTRALassert(!m_children[1]);

          V[1 - m_splitting_coordinate] = m_range[1 - m_splitting_coordinate];

          V[m_splitting_coordinate].m_begin = m_range[m_splitting_coordinate].m_begin;
          V[m_splitting_coordinate].m_end = m_splitting_value;
          m_children[0] = pool.create(V.x(), V.y());

          V[m_splitting_coordinate].m_begin = m_splitting_value;
          V[m_splitting_coordinate].m_end = m_range[m_splitting_coordinate].m_end;
          m_children[1] = pool.create(V.x(), V.y());
        }

      ASTRALassert(m_children[0]);
      ASTRALassert(m_children[1]);
      if (R[m_splitting_coordinate] < m_splitting_value)
        {
          m_children[0]->insert(pool, R);
        }
      else
        {
          m_children[1]->insert(pool, R);
        }
    }
  else
    {
      m_splitting_coordinate = lit_leaf;
    }
}

bool
astral::StrokeQuery::Implement::RectHierarchy::
can_merge_children(unsigned int max_rect_size)
{
  ASTRALassert(m_children[0]);
  ASTRALassert(m_children[1]);

  if (!m_children[0]->is_lit_leaf() || !m_children[1]->is_lit_leaf())
    {
      return false;
    }

  unsigned int begin, end, size;

  begin = m_range[m_splitting_coordinate].m_begin;
  end = m_range[m_splitting_coordinate].m_end;
  size = ImageAtlas::tile_size_without_padding * (end - begin);

  return size <= max_rect_size;
}

void
astral::StrokeQuery::Implement::RectHierarchy::
merge(unsigned int max_rect_size)
{
  if (m_children[0])
    {
      ASTRALassert(m_children[1]);
      ASTRALassert(m_splitting_coordinate < 2);

      m_children[0]->merge(max_rect_size);
      m_children[1]->merge(max_rect_size);
      m_has_content = m_children[0]->m_has_content || m_children[1]->m_has_content;
      if (can_merge_children(max_rect_size))
        {
          /* both children are lit-leaves, so make this a lit leaf and kill
           * the children; because the memory is allocated from a pool,
           * that just means setting the pointers to nullptr.
           */
          m_children[0] = nullptr;
          m_children[1] = nullptr;
          m_splitting_coordinate = lit_leaf;
        }
    }
  else
    {
      ASTRALassert(!m_children[1]);
      m_has_content = is_lit_leaf();
    }
}

unsigned int
astral::StrokeQuery::Implement::RectHierarchy::
count(void) const
{
  if (m_children[0])
    {
      ASTRALassert(is_node());
      ASTRALassert(m_children[1]);
      return m_children[0]->count() + m_children[1]->count();
    }
  else
    {
      ASTRALassert(!m_children[1]);
      return (is_lit_leaf()) ? 1 : 0;
    }
}

void
astral::StrokeQuery::Implement::RectHierarchy::
create_result_elements(StrokeQuery::Implement &qr, std::vector<ResultRect> *dst)
{
  if (m_children[0])
    {
      ASTRALassert(is_node());
      ASTRALassert(m_children[1]);
      m_children[0]->create_result_elements(qr, dst);
      m_children[1]->create_result_elements(qr, dst);
      m_query_result = nullptr;
    }
  else if (is_lit_leaf())
    {
      /* convert the range of elementary rect to a single rect value */
      dst->push_back(ResultRect(qr));
      dst->back().m_range = m_range;
      m_query_result = &dst->back();
    }
}

void
astral::StrokeQuery::Implement::RectHierarchy::
add_sources_lit_leaf(StrokeQuery::Implement &caller,
                     const QueryElement &q,
                     const detail::StrokeDataHierarchy::Base &base)
{
  unsigned int bb_idx;
  vecN<range_type<int>, 2> R;

  ASTRALassert(m_query_result);
  bb_idx = q.m_box_location + base.id();

  ASTRALassert(bb_idx < caller.m_workroom_boxes.size());
  R = caller.m_workroom_boxes[bb_idx];

  if (R.x().m_begin >= m_range.x().m_end
      || R.y().m_begin >= m_range.y().m_end
      || m_range.x().m_begin >= R.x().m_end
      || m_range.y().m_begin >= R.y().m_end)
    {
      return;
    }

  if ((R.x().m_begin >= m_range.x().m_begin
       && R.x().m_end <= m_range.x().m_end
       && R.y().m_begin >= m_range.y().m_begin
       && R.y().m_end <= m_range.y().m_end)
      || !base.is_node())
    {
      /* element is completely contained or has no children so just take it directly */
      base.add_elements(q.m_active_primitives, m_query_result->m_sources->back().m_idxs);
    }
  else
    {
      /* element is not completely contained, recurse to children */
      for (unsigned int leaf = 0, num_leaves = base.number_child_leaves(); leaf < num_leaves; ++leaf)
        {
          add_sources_lit_leaf(caller, q, base.child_leaf(leaf, *q.m_hierarchy));
        }
      for (unsigned int node = 0, num_nodes = base.number_child_nodes(); node < num_nodes; ++node)
        {
          add_sources_lit_leaf(caller, q, base.child_node(node, *q.m_hierarchy));
        }
    }
}

void
astral::StrokeQuery::Implement::RectHierarchy::
add_sources(StrokeQuery::Implement &caller,
            const QueryElement &q,
            const detail::StrokeDataHierarchy::Base &base)
{
  if (!m_has_content)
    {
      return;
    }

  unsigned int bb_idx;
  vecN<range_type<int>, 2> R;

  bb_idx = q.m_box_location + base.id();
  ASTRALassert(bb_idx < caller.m_workroom_boxes.size());

  R = caller.m_workroom_boxes[bb_idx];
  if (R.x().m_begin >= m_range.x().m_end
      || R.y().m_begin >= m_range.y().m_end
      || m_range.x().m_begin >= R.x().m_end
      || m_range.y().m_begin >= R.y().m_end)
    {
      return;
    }

  if (is_lit_leaf())
    {
      Source e(q.m_client_id, caller);

      m_query_result->m_sources->push_back(e);
      add_sources_lit_leaf(caller, q, base);
    }
  else if (m_children[0])
    {
      ASTRALassert(m_children[1]);

      m_children[0]->add_sources(caller, q, base);
      m_children[1]->add_sources(caller, q, base);
    }
}

////////////////////////////////////////////////
// astral::StrokeQuery::Implement methods
astral::StrokeQuery::Implement::
Implement(void):
  m_mode(mode_no_query)
{
  m_pool = ASTRALnew RectHierarchyPool();
}

astral::StrokeQuery::Implement::
~Implement(void)
{
  m_query_src_pool.clear();
  m_ids_backing.clear();
  ASTRALdelete(m_pool);
}

void
astral::StrokeQuery::Implement::
clear_implement(void)
{
  m_query_elements.clear();
  m_elementary_rects_is_lit.clear();
  m_lit_elementary_rect_list.clear();
  m_workroom_is_lit.clear();
  m_workroom_lit_list.clear();
  m_workroom_boxes.clear();
  m_query_src_pool.clear();
  m_ids_backing.clear();
  m_pool->clear();
  m_result_elements.clear();
  m_empty_tiles.clear();
  m_rect_hierarchy = nullptr;
  m_mode = mode_no_query;
}

void
astral::StrokeQuery::Implement::
compute_elementary_rect_hits(Rect R,
                             range_type<int> *out_x_range,
                             range_type<int> *out_y_range)
{
  R.m_min_point *= m_reciprocal_elementary_rect_size;
  R.m_max_point *= m_reciprocal_elementary_rect_size;

  out_x_range->m_begin = t_max(0, t_min(m_number_elementary_rects.x(), static_cast<int>(R.m_min_point.x())));
  out_x_range->m_end = t_max(0, t_min(m_number_elementary_rects.x(), 1 + static_cast<int>(R.m_max_point.x())));

  out_y_range->m_begin = t_max(0, t_min(m_number_elementary_rects.y(), static_cast<int>(R.m_min_point.y())));
  out_y_range->m_end = t_max(0, t_min(m_number_elementary_rects.y(), 1 + static_cast<int>(R.m_max_point.y())));

  /* TODO: Check if R intersects the padding of the tile at m_begin or m_end - 1
   *       and if so, then enlarge the range as well.
   */
}

void
astral::StrokeQuery::Implement::
begin_query_implement(const ScaleTranslate &rect_transformation_elements,
                      const ivec2 &rect_size, bool sparse_query,
                      c_array<const BoundingBox<float>> restrict_rects)
{
  clear();
  m_mode = mode_adding_elements;

  if (rect_size.x() <= 0 || rect_size.y() <= 0)
    {
      m_mode = mode_empty_rect_adding_element;
      return;
    }

  m_sparse_query = sparse_query;
  m_rect_transformation_elements = Transformation(rect_transformation_elements);

  if (!m_sparse_query)
    {
      m_end_elementary_rect_size = rect_size;
      m_number_elementary_rects = ivec2(1, 1);
      return;
    }

  unsigned int N;

  m_number_elementary_rects = ImageAtlas::tile_count(rect_size, &m_end_elementary_rect_size);
  N = m_number_elementary_rects.x() * m_number_elementary_rects.y();

  /* should we make the elementary rect size a multiple of the
   * tile size for large stroking radius?
   */
  m_reciprocal_elementary_rect_size = 1.0f / static_cast<float>(ImageAtlas::tile_size_without_padding);

  ASTRALassert(m_elementary_rects_is_lit.empty());
  if (restrict_rects.empty())
    {
      m_elementary_rects_is_lit.resize(N, elementary_rect_unlit);
    }
  else
    {
      m_elementary_rects_is_lit.resize(N, elementary_rect_cannot_be_lit);
      mark_rects_as_unlit(restrict_rects);
    }
}

void
astral::StrokeQuery::Implement::
mark_rects_as_unlit(c_array<const BoundingBox<float>> restrict_rects)
{
  /* TODO: Make hierarchy so that we can quickly cull
   *       elementary rects in light_elementary_rects_of_query_element()
   */
  for (const BoundingBox<float> &R : restrict_rects)
    {
      if (R.empty())
        {
          continue;
        }

      range_type<int> x_range, y_range;

      compute_elementary_rect_hits(R.as_rect(), &x_range, &y_range);
      if (x_range.m_begin < x_range.m_end && y_range.m_begin < y_range.m_end)
        {
          for (int y = y_range.m_begin; y < y_range.m_end; ++y)
            {
              for (int x = x_range.m_begin; x < x_range.m_end; ++x)
                {
                  unsigned int rect_id;
                  ivec2 R(x, y);

                  rect_id = compute_elementary_rect_id(R);
                  ASTRALassert(rect_id < m_elementary_rects_is_lit.size());

                  m_elementary_rects_is_lit[rect_id] = elementary_rect_unlit;
                }
            }
        }
    }
}

void
astral::StrokeQuery::Implement::
add_element_implement(unsigned int client_id,
                      const Transformation &element_transformation_stroking,
                      const Transformation &stroking_transformation_path,
                      const StrokeShader::CookedData &path,
                      float animation_t,
                      ActivePrimitives active_primitives,
                      StrokeRadii stroke_radii)
{
  if (m_mode == mode_empty_rect_adding_element)
    {
      return;
    }

  QueryElementDetailed query_element;

  unsigned int sz, box_location;
  Transformation rect_transformation_stroking;

  ASTRALassert(m_mode == mode_adding_elements);

  sz = path.m_hierarchy_size;
  box_location = m_workroom_boxes.size();
  m_workroom_boxes.resize(box_location + sz);

  query_element.m_client_id = client_id;
  query_element.m_box_location = box_location;
  query_element.m_hierarchy = path.m_hierarchy;
  query_element.m_active_primitives = active_primitives;

  m_query_elements.push_back(query_element);

  if (!m_sparse_query)
    {
      /* no need to light rects if not doing a sparse query */
      return;
    }

  rect_transformation_stroking = m_rect_transformation_elements * element_transformation_stroking;

  query_element.m_stroke_radii = stroke_radii;
  query_element.m_animation_t = animation_t;
  query_element.m_rect_transformation_stroking = &rect_transformation_stroking;
  query_element.m_stroking_transformation_path = &stroking_transformation_path;

  light_elementary_rects_of_query_element(m_query_elements.back().m_hierarchy->root(),
                                          query_element);
}

void
astral::StrokeQuery::Implement::
light_elementary_rects_of_query_element(const detail::StrokeDataHierarchy::Base &base,
                                        const QueryElementDetailed &q)
{
  /* compute the range of rects that the bounding box
   * the hierarchy hits
   */
  unsigned int bb_idx;
  BoundingBox<float> bb;
  const detail::StrokeDataHierarchy &hierarchy(*q.m_hierarchy);

  bb_idx = q.m_box_location + base.id();
  bb = base.bounding_box(*q.m_rect_transformation_stroking,
                         *q.m_stroking_transformation_path,
                         q.m_animation_t, q.m_stroke_radii);

  ASTRALassert(bb_idx < m_workroom_boxes.size());

  if (bb.empty())
    {
      m_workroom_boxes[bb_idx].x() = range_type<int>(-1, -1);
      m_workroom_boxes[bb_idx].y() = range_type<int>(-1, -1);
      return;
    }
  range_type<int> &x_range(m_workroom_boxes[bb_idx].x());
  range_type<int> &y_range(m_workroom_boxes[bb_idx].y());

  bb.enlarge(vec2(ImageAtlas::tile_padding));
  compute_elementary_rect_hits(bb.as_rect(), &x_range, &y_range);
  if (x_range.m_begin == x_range.m_end || y_range.m_begin == y_range.m_end)
    {
      /* does not occupy a single elementary rect */
      return;
    }

  /* if the entire element is within a single elementary rect
   * or if it has no children, light the box or boxes
   */
  if (!base.is_node() || (x_range.m_end == x_range.m_begin + 1 && y_range.m_end == y_range.m_begin + 1))
    {
      for (int x = x_range.m_begin; x < x_range.m_end; ++x)
        {
          for (int y = y_range.m_begin; y < y_range.m_end; ++y)
            {
              unsigned int rect_id;
              ivec2 R(x, y);

              rect_id = compute_elementary_rect_id(R);
              ASTRALassert(rect_id < m_elementary_rects_is_lit.size());

              if (m_elementary_rects_is_lit[rect_id] == elementary_rect_unlit)
                {
                  m_elementary_rects_is_lit[rect_id] = elementary_rect_lit;
                  m_lit_elementary_rect_list.push_back(R);
                }
            }
        }
    }
  else
    {
      /* otherwise recurse to the children */
      for (unsigned int leaf = 0, num_leaves = base.number_child_leaves(); leaf < num_leaves; ++leaf)
        {
          light_elementary_rects_of_query_element(base.child_leaf(leaf, hierarchy), q);
        }

      for (unsigned int node = 0, num_nodes = base.number_child_nodes(); node < num_nodes; ++node)
        {
          light_elementary_rects_of_query_element(base.child_node(node, hierarchy), q);
        }
    }
}

void
astral::StrokeQuery::Implement::
end_query_implement(unsigned int max_rect_size)
{
  if (m_mode == mode_empty_rect_adding_element)
    {
      m_mode = mode_query_ended;
      return;
    }

  ASTRALassert(m_mode == mode_adding_elements);
  m_mode = mode_query_ended;

  /* if most of the elementary rectangle are lit, just
   * have a single large rectangle: the passed rectangle
   *
   * TODO: enable this optimization by determining what
   *       is meant by most.
   */
  if (m_lit_elementary_rect_list.size() * 4 >= m_elementary_rects_is_lit.size() * 3 && false)
    {
      m_sparse_query = false;
    }

  /* not a sparse query */
  if (!m_sparse_query)
    {
      m_result_elements.push_back(ResultRect(*this));
      m_result_elements[0].m_range.x() = range_type<int>(0, m_number_elementary_rects.x());
      m_result_elements[0].m_range.y() = range_type<int>(0, m_number_elementary_rects.y());

      for (const QueryElement &query_element : m_query_elements)
        {
          Source e(query_element.m_client_id, *this);

          query_element.m_hierarchy->root().add_elements(query_element.m_active_primitives, e.m_idxs);
          m_result_elements[0].m_sources->push_back(e);
        }

      return;
    }

  /* add to m_empty_tiles those rects of m_elementary_rects_is_lit that are not lit */
  ASTRALassert(m_empty_tiles.empty());
  for (unsigned int i = 0, endi = m_elementary_rects_is_lit.size(); i < endi; ++i)
    {
      if (m_elementary_rects_is_lit[i] != elementary_rect_lit)
        {
          ivec2 xy;

          xy = compute_rect_from_id(i);
          m_empty_tiles.push_back(uvec2(xy));
        }
    }

  create_rect_hierarchy(max_rect_size);
  for (const QueryElement &q : m_query_elements)
    {
      m_rect_hierarchy->add_sources(*this, q, q.m_hierarchy->root());
    }
}

void
astral::StrokeQuery::Implement::
create_rect_hierarchy(unsigned int max_rect_size)
{
  m_rect_hierarchy = m_pool->create(range_type<int>(0, m_number_elementary_rects.x()),
                                    range_type<int>(0, m_number_elementary_rects.y()));
  for (ivec2 R : m_lit_elementary_rect_list)
    {
      m_rect_hierarchy->insert(*m_pool, R);
    }
  m_rect_hierarchy->merge(max_rect_size);

  unsigned int total;
  total = m_rect_hierarchy->count();

  /* We now have the rect-hierarchy to use, each element
   * of the rect hierarchy is to have a single
   * ResultRect value; we reserve the number of
   * hierarchy rect on m_result_elements to make sure
   * that it does not reallocate during create_result_elements()
   * to guarantee that the pointers to the elements within
   * the std::vector remain valid.
   */
  ASTRALassert(m_result_elements.empty());
  m_result_elements.reserve(total);
  m_rect_hierarchy->create_result_elements(*this, &m_result_elements);
  ASTRALassert(m_result_elements.size() == total);
}

//////////////////////////////////////
// astral::StrokeQuery methods
astral::reference_counted_ptr<astral::StrokeQuery>
astral::StrokeQuery::
create(void)
{
  return ASTRALnew Implement();
}

astral::StrokeQuery::Implement&
astral::StrokeQuery::
implement(void)
{
  return *static_cast<Implement*>(this);
}

const astral::StrokeQuery::Implement&
astral::StrokeQuery::
implement(void) const
{
  return *static_cast<const Implement*>(this);
}

void
astral::StrokeQuery::
begin_query(const ScaleTranslate &rect_transformation_elements,
            const ivec2 &rect_size, bool sparse_query,
            c_array<const BoundingBox<float>> restrict_rects)
{
  implement().begin_query_implement(rect_transformation_elements,
                                    rect_size, sparse_query,
                                    restrict_rects);
}

void
astral::StrokeQuery::
add_element(unsigned int ID,
            const Transformation &element_transformation_stroking,
            const Transformation &stroking_transformation_path,
            const StrokeShader::CookedData &path, float animation_t,
            ActivePrimitives active_primitives,
            StrokeRadii stroke_radii)
{
  implement().add_element_implement(ID,
                                    element_transformation_stroking,
                                    stroking_transformation_path,
                                    path, animation_t,
                                    active_primitives, stroke_radii);
}

void
astral::StrokeQuery::
end_query(unsigned int max_size)
{
  implement().end_query_implement(max_size);
}

astral::c_array<const astral::StrokeQuery::ResultRect>
astral::StrokeQuery::
elements(void) const
{
  return implement().elements_implement();
}

astral::c_array<const astral::uvec2>
astral::StrokeQuery::
empty_tiles(void) const
{
  return implement().empty_tiles_implement();
}

bool
astral::StrokeQuery::
is_sparse(void) const
{
  return implement().is_sparse_implement();
}

astral::ivec2
astral::StrokeQuery::
end_elementary_rect_size(void) const
{
  return implement().end_elementary_rect_size_implement();
}

astral::ivec2
astral::StrokeQuery::
number_elementary_rects(void) const
{
  return implement().number_elementary_rects_implement();
}

void
astral::StrokeQuery::
clear(void)
{
  implement().clear_implement();
}

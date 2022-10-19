/*!
 * \file stroke_data_hierarchy.cpp
 * \brief file stroke_data_hierarchy.cpp
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

#include <limits>
#include "stroke_data_hierarchy.hpp"
#include "stroke_shader_vertex_index_roles.hpp"

//////////////////////////////////////////////////
// astral::detail::StrokeDataHierarchy::Base methods
astral::detail::StrokeDataHierarchy::Base::
Base(const vecN<range_type<unsigned int>, StrokeShader::number_primitive_types> &ranges,
     unsigned int *id, Ordering *pelement_ordering):
  m_id(*id),
  m_child_nodes_range(0, 0),
  m_child_leaves_range(0, 0),
  m_is_node(false)
{
  Ordering &element_ordering(*pelement_ordering);

  ++(*id);
  for (unsigned int primitive_type = 0; primitive_type < StrokeShader::number_primitive_types; ++primitive_type)
    {
      range_type<unsigned int> R;
      enum StrokeShader::primitive_type_t tp(static_cast<enum StrokeShader::primitive_type_t>(primitive_type));
      StrokeShader::VertexIndexRoles vertex_index_roles(StrokeShader::VertexIndexRoles::roles(tp));

      R = ranges[primitive_type];
      ASTRALassert(R.m_begin <= R.m_end);

      m_vertex_ranges[primitive_type].m_begin = element_ordering[primitive_type].size() * vertex_index_roles.m_indices.size();
      for (unsigned int v = R.m_begin; v < R.m_end; ++v)
        {
          element_ordering[primitive_type].push_back(v);
        }
      m_vertex_ranges[primitive_type].m_end = element_ordering[primitive_type].size() * vertex_index_roles.m_indices.size();
    }
}

astral::detail::StrokeDataHierarchy::Base::
Base(const StrokeDataHierarchy &src,
     enum node_type_t node_type,
     range_type<unsigned int> child_range,
     unsigned int *id):
  m_id(*id),
  m_child_nodes_range(0, 0),
  m_child_leaves_range(0, 0),
  m_is_node(true),
  m_vertex_ranges(range_type<int>(std::numeric_limits<int>::max(), 0u))
{
  vecN<int, StrokeShader::number_primitive_types> total_verts(0u);

  ++(*id);
  if (node_type == node_of_nodes)
    {
      m_child_nodes_range = child_range;
      ASTRALassert(m_child_nodes_range.m_begin < m_child_nodes_range.m_end);

      for (unsigned int node = m_child_nodes_range.m_begin; node < m_child_nodes_range.m_end; ++node)
        {
          const Base &base(src.get_node(node));
          total_verts += absorb_ranges(base);
        }
    }
  else
    {
      m_child_leaves_range = child_range;
      ASTRALassert(m_child_leaves_range.m_begin < m_child_leaves_range.m_end);

      for (unsigned int leaf = m_child_leaves_range.m_begin; leaf < m_child_leaves_range.m_end; ++leaf)
        {
          const Base &base(src.get_leaf(leaf));
          total_verts += absorb_ranges(base);
        }
    }

  for (unsigned int primitive_type = 0; primitive_type < StrokeShader::number_primitive_types; ++primitive_type)
    {
      ASTRALassert(m_vertex_ranges[primitive_type].m_begin <= m_vertex_ranges[primitive_type].m_end);
      ASTRALassert(total_verts[primitive_type] == m_vertex_ranges[primitive_type].difference());
    }
}

astral::vecN<int, astral::StrokeShader::number_primitive_types>
astral::detail::StrokeDataHierarchy::Base::
absorb_ranges(const Base &base)
{
  vecN<int, StrokeShader::number_primitive_types> return_value;

  for (unsigned int primitive_type = 0; primitive_type < StrokeShader::number_primitive_types; ++primitive_type)
    {
      return_value[primitive_type] = base.m_vertex_ranges[primitive_type].difference();
      m_vertex_ranges[primitive_type].absorb(base.m_vertex_ranges[primitive_type]);
    }
  return return_value;
}

void
astral::detail::StrokeDataHierarchy::Base::
add_elements(const StrokeQuery::ActivePrimitives active_primitives,
             vecN<std::vector<range_type<int>>, StrokeShader::number_primitive_types> *ptr_dst) const
{
  vecN<std::vector<range_type<int>>, StrokeShader::number_primitive_types> &dst(*ptr_dst);
  vecN<std::vector<range_type<int>>*, StrokeShader::number_primitive_types> tmp;

  for (unsigned int i = 0; i < StrokeShader::number_primitive_types; ++i)
    {
      tmp[i] = &dst[i];
    }
  add_elements(active_primitives, tmp);
}

void
astral::detail::StrokeDataHierarchy::Base::
add_elements(const StrokeQuery::ActivePrimitives active_primitives,
             const vecN<std::vector<range_type<int>>*, StrokeShader::number_primitive_types> &dst) const
{
  for (unsigned int i = 0; i < StrokeShader::number_primitive_types; ++i)
    {
      enum StrokeShader::primitive_type_t tp;

      tp = static_cast<enum StrokeShader::primitive_type_t>(i);
      if (active_primitives.value(tp) && m_vertex_ranges[tp].m_begin < m_vertex_ranges[tp].m_end)
        {
          dst[i]->push_back(m_vertex_ranges[tp]);
        }
    }
}

const astral::detail::StrokeDataHierarchy::Base&
astral::detail::StrokeDataHierarchy::Base::
child_leaf(unsigned int I, const StrokeDataHierarchy &src) const
{
  ASTRALassert(I < number_child_leaves());
  return src.get_leaf(I + m_child_leaves_range.m_begin);
}

const astral::detail::StrokeDataHierarchy::Base&
astral::detail::StrokeDataHierarchy::Base::
child_node(unsigned int I, const StrokeDataHierarchy &src) const
{
  ASTRALassert(I < number_child_nodes());
  return src.get_node(I + m_child_nodes_range.m_begin);
}

/////////////////////////////////////////////////////////
// astral::detail::StrokeDataHierarchy::AABB methods
astral::BoundingBox<float>
astral::detail::StrokeDataHierarchy::AABB::
compute(const Transformation &pixel_transformation_logical,
        const Transformation &logical_transformation_path,
        const StrokeQuery::StrokeRadii &stroke_params) const
{
  float r(stroke_params.edge_radius());
  BoundingBox<float> logical_bb;

  if (m_flags & join_point)
    {
      r = t_max(r, stroke_params.join_radius());
    }

  if (m_flags & cap_point)
    {
      r = t_max(r, stroke_params.cap_radius());
    }

  logical_bb = logical_transformation_path.apply_to_bb(m_bb);
  logical_bb.enlarge(vec2(r, r));
  return pixel_transformation_logical.apply_to_bb(logical_bb);
}

void
astral::detail::StrokeDataHierarchy::AABB::
init(c_array<const vec2> pts)
{
  ASTRALassert(pts.size() == 2u || pts.size() == 3u);

  m_bb.clear();
  m_flags = 0u;

  if (pts.size() == 3u)
    {
      ContourCurve C(pts, ContourCurve::continuation_curve);
      m_bb.union_box(C.tight_bounding_box());
    }
  else
    {
      m_bb.union_point(pts[0]);
      m_bb.union_point(pts[1]);
    }
}

//////////////////////////////////////////////////////////
// astral::detail::StrokeDataHierarchy::StaticLeafBuilder methods
bool
astral::detail::StrokeDataHierarchy::StaticLeafBuilder::
element_is_degenerate(const RawData::Info &info)
{
  return (info.m_type != StrokeShader::line_segments_primitive
          && info.m_type !=  StrokeShader::biarc_curves_primitive);
}

bool
astral::detail::StrokeDataHierarchy::StaticLeafBuilder::
should_emit_data(const RawData::Info *prev_info, const RawData::Info &info)
{
  /* if the new element is on a different contour,
   * emit the current leaf.
   *
   * ISSUE: if a path is a bunch of point contours
   *        this will make a bunch of leaves which
   *        means each point-contour is added
   *        seperately. Hopefully, the Node objects
   *        will get the culling well.
   */
  if (prev_info && prev_info->m_source_info.m_contour_id != info.m_source_info.m_contour_id)
    {
      return true;
    }

  /* check if the element is effectively degenerate
   * and if so, return false. Degenerate elements
   * are elements that are points (i.e. joins and caps)
   */
  if (element_is_degenerate(info))
    {
      return false;
    }

  /* if m_aabb is initializd, then that means
   * the current run already has a curve or
   * line segment. To take advantage of the
   * size-dependent tessellation coming from
   * ContourApproximator, means we just say
   * emit at each new curve or line segment.
   */
  if (m_aabb_inited)
    {
      return true;
    }

  return false;
}

void
astral::detail::StrokeDataHierarchy::StaticLeafBuilder::
absorb_element(const RawData::Info &info, const std::vector<Join> *inner_glue)
{
  c_array<vec2> pts(m_pts);
  uint32_t flags(0u);

  ASTRALassert(m_currentR[info.m_type] == info.m_id);
  switch (info.m_type)
    {
    case StrokeShader::line_segments_primitive:
      pts = pts.sub_array(0, 2);
      pts[0] = m_input.m_line_segments[info.m_id].m_pts[0];
      pts[1] = m_input.m_line_segments[info.m_id].m_pts[1];
      break;

    case StrokeShader::biarc_curves_primitive:
      pts[0] = m_input.m_biarc_curves[info.m_id].m_pts[0];
      pts[1] = m_input.m_biarc_curves[info.m_id].m_pts[1];
      pts[2] = m_input.m_biarc_curves[info.m_id].m_pts[2];
      break;

    case StrokeShader::inner_glue_primitive:
      ASTRALassert(inner_glue);
      pts = pts.sub_array(0, 1);
      pts[0] = (*inner_glue)[info.m_id].m_p;
      break;

    case StrokeShader::glue_primitive:
      pts = pts.sub_array(0, 1);
      pts[0] = m_input.m_glue[info.m_id].m_p;
      break;

    case StrokeShader::glue_cusp_primitive:
      pts = pts.sub_array(0, 1);
      pts[0] = m_input.m_glue_cusp[info.m_id].m_p;
      break;

    case StrokeShader::joins_primitive:
      pts = pts.sub_array(0, 1);
      pts[0] = m_input.m_joins[info.m_id].m_p;
      flags |= AABB::join_point;
      break;

    case StrokeShader::caps_primitive:
      pts = pts.sub_array(0, 1);
      pts[0] = m_input.m_caps[info.m_id].m_p;
      flags |= AABB::cap_point;
      break;

    default:
      pts = c_array<vec2>();
    }

  if (m_aabb_inited)
    {
      for (vec2 p : pts)
        {
          m_aabb.union_point(p, flags);
        }
    }
  else if (!element_is_degenerate(info))
    {
      m_aabb_inited = true;
      m_aabb.init(pts);

      for (const vec2 &p : m_waiting_pts)
        {
          m_aabb.union_point(p, m_waiting_flags);
        }
      m_waiting_pts.clear();
    }
  else
    {
      m_waiting_flags |= flags;
      for (auto p : pts)
        {
          m_waiting_pts.push_back(p);
        }
    }
  ++m_currentR[info.m_type];
}

void
astral::detail::StrokeDataHierarchy::StaticLeafBuilder::
emit_data(std::vector<StaticLeafData> *dst)
{
  dst->push_back(StaticLeafData());

  m_currentR[StrokeShader::segments_cappers_primitive] = m_currentR[StrokeShader::line_segments_primitive];
  m_currentR[StrokeShader::biarc_curves_cappers_primitive] = m_currentR[StrokeShader::biarc_curves_primitive];
  for (unsigned int i = 0; i < StrokeShader::number_primitive_types; ++i)
    {
      dst->back().m_elements[i].m_begin = m_prevR[i];
      dst->back().m_elements[i].m_end = m_currentR[i];
    }

  if (m_aabb_inited)
    {
     dst->back().m_aabb = m_aabb;
    }
  else
    {
      dst->back().m_aabb.init_as_empty();
      for (vec2 p : m_waiting_pts)
        {
          dst->back().m_aabb.union_point(p, m_waiting_flags);
        }
      m_waiting_pts.clear();
    }

  StrokeQuery::StrokeRadii S;
  dst->back().m_containing_bb = dst->back().m_aabb.compute(Transformation(), Transformation(), S);

  ASTRALassert(m_waiting_pts.empty());
  m_aabb_inited = false;
  m_waiting_flags = 0u;
  m_prevR = m_currentR;
}

void
astral::detail::StrokeDataHierarchy::StaticLeafBuilder::
create_leaves(std::vector<StaticLeafData> *out_leaves,
              BoundingBox<float> *out_bb,
              const RawData &input)
{
  StaticLeafBuilder builder(input);
  const RawData::Info *prev_v(nullptr);

  for (const RawData::Info &v : input.m_info)
    {
      if (builder.should_emit_data(prev_v, v))
        {
          builder.emit_data(out_leaves);
          out_bb->union_box(out_leaves->back().m_containing_bb);
        }
      builder.absorb_element(v);
      prev_v = &v;
    }
  builder.emit_data(out_leaves);
  out_bb->union_box(out_leaves->back().m_containing_bb);
}

/////////////////////////////////////////////////////////////
// astral::detail::StrokeDataHierarchy::AnimatedLeafBuilder methods
void
astral::detail::StrokeDataHierarchy::AnimatedLeafBuilder::
create_leaves(std::vector<StaticLeafData> *out_leaves0,
              std::vector<StaticLeafData> *out_leaves1,
              BoundingBox<float> *out_bb0,
              BoundingBox<float> *out_bb1,
              const RawAnimatedData &input)
{
  AnimatedLeafBuilder builder(input);
  const RawData::Info *prev_v0(nullptr), *prev_v1(nullptr);

  ASTRALassert(input.m_start.m_info.size() == input.m_end.m_info.size());
  for (unsigned int i = 0, endi = input.m_start.m_info.size(); i < endi; ++i)
    {
      const RawData::Info &v0(input.m_start.m_info[i]);
      const RawData::Info &v1(input.m_end.m_info[i]);

      if (builder.should_emit_data(prev_v0, v0, prev_v1, v1))
        {
          builder.emit_data(out_leaves0, out_leaves1);
          out_bb0->union_box(out_leaves0->back().m_containing_bb);
          out_bb1->union_box(out_leaves1->back().m_containing_bb);
        }
      builder.absorb_element(v0, v1);
      prev_v0 = &v0;
      prev_v1 = &v1;
    }
  builder.emit_data(out_leaves0, out_leaves1);
  out_bb0->union_box(out_leaves0->back().m_containing_bb);
  out_bb1->union_box(out_leaves1->back().m_containing_bb);
}

///////////////////////////////////////////////////////////////////////////
// astral::detail::StrokeDataHierarchy::StaticHierarchy methods
void
astral::detail::StrokeDataHierarchy::StaticHierarchy::
compute_split(unsigned int coord, const BoundingBox<float> &bb,
              c_array<const StaticLeafData> leaves,
              Split *out_value)
{
  vecN<BoundingBox<float>, 2> bs;

  if (coord == 0)
    {
      bs = bb.split_x();
    }
  else
    {
      bs = bb.split_y();
    }

  for (const StaticLeafData &data : leaves)
    {
      bool b0, b1;

      b0 = bs[0].intersects(data.m_containing_bb);
      b1 = bs[1].intersects(data.m_containing_bb);

      if (b0 && b1)
        {
          out_value->m_both.push_back(data);
          out_value->m_bb_both.union_box(data.m_containing_bb);
        }
      else if (b0)
        {
          out_value->m_before.push_back(data);
          out_value->m_bb_before.union_box(data.m_containing_bb);
        }
      else if (b1)
        {
          out_value->m_after.push_back(data);
          out_value->m_bb_after.union_box(data.m_containing_bb);
        }
    }
}

astral::detail::StrokeDataHierarchy::StaticHierarchy::Node
astral::detail::StrokeDataHierarchy::StaticHierarchy::
create_node(c_array<const StaticLeafData> leaf_data,
            unsigned int *id, Ordering *element_ordering)
{
  range_type<unsigned int> children;

  ASTRALassert(!leaf_data.empty());
  children.m_begin = m_leaves.size();
  for (const StaticLeafData &data : leaf_data)
    {
      m_leaves.push_back(Leaf(data.m_aabb, data.m_elements, id, element_ordering));
    }
  children.m_end = m_leaves.size();

  return Node(*this, Node::node_of_leaves, children, id);
}

astral::detail::StrokeDataHierarchy::StaticHierarchy::Node
astral::detail::StrokeDataHierarchy::StaticHierarchy::
create_hierarchy(unsigned int max_depth, unsigned int split_threshhold,
                 const BoundingBox<float> &bb, c_array<const StaticLeafData> leaf_data,
                 unsigned int *id, Ordering *element_ordering)
{
  ASTRALassert(!leaf_data.empty());
  if (max_depth > 0u && leaf_data.size() >= split_threshhold)
    {
      std::vector<Node> tmp;
      Split splitX, splitY, *split(nullptr);

      compute_split(0, bb, leaf_data, &splitX);
      compute_split(1, bb, leaf_data, &splitY);

      if (splitX.m_both.size() <= splitY.m_both.size())
        {
          split = &splitX;
        }
      else
        {
          split = &splitY;
        }

      if (!split->m_before.empty())
        {
          tmp.push_back(create_hierarchy(max_depth - 1u, split_threshhold,
                                         split->m_bb_before, make_c_array(split->m_before),
                                         id, element_ordering));
        }

      if (!split->m_after.empty())
        {
          tmp.push_back(create_hierarchy(max_depth - 1u, split_threshhold,
                                         split->m_bb_after, make_c_array(split->m_after),
                                         id, element_ordering));
        }

      if (!split->m_both.empty())
        {
          tmp.push_back(create_node(make_c_array(split->m_both), id, element_ordering));
        }

      range_type<unsigned int> children;

      children.m_begin = m_nodes.size();
      for (const auto &c : tmp)
        {
          m_nodes.push_back(c);
        }
      children.m_end = m_nodes.size();

      return Node(*this, Node::node_of_nodes, children, id);
    }
  else
    {
      return create_node(leaf_data, id, element_ordering);
    }
}

astral::reference_counted_ptr<astral::detail::StrokeDataHierarchy>
astral::detail::StrokeDataHierarchy::StaticHierarchy::
create(const RawData &input, unsigned int *out_hierarchy_size,
       Ordering *element_ordering)
{
  static const unsigned int max_depth = 8u;
  static const unsigned int split_thresh = 4u;

  reference_counted_ptr<StaticHierarchy> return_value;
  std::vector<StaticLeafData> leaves;
  BoundingBox<float> bb;

  *out_hierarchy_size = 0;
  StaticLeafBuilder::create_leaves(&leaves, &bb, input);
  return_value = ASTRALnew StaticHierarchy();
  return_value->m_leaves.reserve(leaves.size());

  Node root(return_value->create_hierarchy(max_depth, split_thresh,
                                           bb, make_c_array(leaves),
                                           out_hierarchy_size,
                                           element_ordering));
  return_value->m_nodes.push_back(root);
  ASTRALassert(return_value->m_leaves.size() == leaves.size());

  return return_value;
}

/////////////////////////////////////////////////////////////////////////////
// astral::detail::StrokeDataHierarchy::AnimatedHierarchy methods
void
astral::detail::StrokeDataHierarchy::AnimatedHierarchy::
compute_split(unsigned int coord0, unsigned int coord1,
              const BoundingBox<float> &bb0,
              c_array<const StaticLeafData> leaves0,
              const BoundingBox<float> &bb1,
              c_array<const StaticLeafData> leaves1,
              AnimatedSplit *out_value)
{
  vecN<BoundingBox<float>, 2> bs0, bs1;

  if (coord0 == 0)
    {
      bs0 = bb0.split_x();
    }
  else
    {
      bs0 = bb0.split_y();
    }

  if (coord1 == 0)
    {
      bs1 = bb1.split_x();
    }
  else
    {
      bs1 = bb1.split_y();
    }

  ASTRALassert(leaves0.size() == leaves1.size());
  for (unsigned int I = 0; I < leaves0.size(); ++I)
    {
      const StaticLeafData &data0(leaves0[I]);
      const StaticLeafData &data1(leaves1[I]);
      bool b0, b1;

      b0 = bs0[0].intersects(data0.m_containing_bb)
        || bs1[0].intersects(data1.m_containing_bb);

      b1 = bs0[1].intersects(data0.m_containing_bb)
        || bs1[1].intersects(data1.m_containing_bb);

      if (b0 && b1)
        {
          out_value->m_s0.m_both.push_back(data0);
          out_value->m_s1.m_both.push_back(data1);

          out_value->m_s0.m_bb_both.union_box(data0.m_containing_bb);
          out_value->m_s1.m_bb_both.union_box(data1.m_containing_bb);
        }
      else if (b0)
        {
          out_value->m_s0.m_before.push_back(data0);
          out_value->m_s1.m_before.push_back(data1);

          out_value->m_s0.m_bb_before.union_box(data0.m_containing_bb);
          out_value->m_s1.m_bb_before.union_box(data1.m_containing_bb);
        }
      else if (b1)
        {
          out_value->m_s0.m_after.push_back(data0);
          out_value->m_s1.m_after.push_back(data1);

          out_value->m_s0.m_bb_after.union_box(data0.m_containing_bb);
          out_value->m_s1.m_bb_after.union_box(data1.m_containing_bb);
        }
    }

  ASTRALassert(out_value->m_s0.m_both.size() == out_value->m_s1.m_both.size());
  ASTRALassert(out_value->m_s0.m_after.size() == out_value->m_s1.m_after.size());
  ASTRALassert(out_value->m_s0.m_before.size() == out_value->m_s1.m_before.size());
}

astral::detail::StrokeDataHierarchy::AnimatedHierarchy::Node
astral::detail::StrokeDataHierarchy::AnimatedHierarchy::
create_node(c_array<const StaticLeafData> leaf_data0,
            c_array<const StaticLeafData> leaf_data1,
            unsigned int *id, Ordering *element_ordering)
{
  range_type<unsigned int> children;

  ASTRALassert(leaf_data0.size() == leaf_data1.size());
  ASTRALassert(!leaf_data0.empty());

  children.m_begin = m_leaves.size();
  for (unsigned int I = 0; I < leaf_data0.size(); ++I)
    {
      const StaticLeafData &data0(leaf_data0[I]);
      const StaticLeafData &data1(leaf_data1[I]);

      ASTRALassert(data0.m_elements == data1.m_elements);
      m_leaves.push_back(Leaf(data0.m_aabb, data1.m_aabb,
                              data0.m_elements,
                              id, element_ordering));
    }
  children.m_end = m_leaves.size();

  return Node(*this, Node::node_of_leaves, children, id);
}

astral::detail::StrokeDataHierarchy::AnimatedHierarchy::Node
astral::detail::StrokeDataHierarchy::AnimatedHierarchy::
create_hierarchy(unsigned int max_depth, unsigned int split_threshhold,
                 const BoundingBox<float> &bb0, c_array<const StaticLeafData> leaf_data0,
                 const BoundingBox<float> &bb1, c_array<const StaticLeafData> leaf_data1,
                 unsigned int *id, Ordering *element_ordering)
{
  ASTRALassert(leaf_data0.size() == leaf_data1.size());
  ASTRALassert(!leaf_data0.empty());

  if (max_depth > 0u && leaf_data0.size() >= split_threshhold)
    {
      AnimatedSplit splitXX, splitXY, splitYX, splitYY, *splitX, *splitY, *split;
      std::vector<Node> tmp;

      compute_split(0, 0, bb0, leaf_data0, bb1, leaf_data1, &splitXX);
      compute_split(0, 1, bb0, leaf_data0, bb1, leaf_data1, &splitXY);
      splitX = (splitXX.m_s0.m_both.size() < splitXY.m_s0.m_both.size()) ? &splitXX : &splitXY;

      compute_split(1, 0, bb0, leaf_data0, bb1, leaf_data1, &splitYX);
      compute_split(1, 1, bb0, leaf_data0, bb1, leaf_data1, &splitYY);
      splitY = (splitYX.m_s0.m_both.size() < splitYY.m_s0.m_both.size()) ? &splitYX : &splitYY;

      split = (splitX->m_s0.m_both.size() <= splitY->m_s0.m_both.size()) ? splitX : splitY;

      if (!split->m_s0.m_before.empty())
        {
          tmp.push_back(create_hierarchy(max_depth - 1u, split_threshhold,
                                         split->m_s0.m_bb_before,
                                         make_c_array(split->m_s0.m_before),
                                         split->m_s1.m_bb_before,
                                         make_c_array(split->m_s1.m_before),
                                         id, element_ordering));
        }

      if (!split->m_s0.m_after.empty())
        {
          tmp.push_back(create_hierarchy(max_depth - 1u, split_threshhold,
                                         split->m_s0.m_bb_after,
                                         make_c_array(split->m_s0.m_after),
                                         split->m_s1.m_bb_after,
                                         make_c_array(split->m_s1.m_after),
                                         id, element_ordering));
        }

      if (!split->m_s0.m_both.empty())
        {
          tmp.push_back(create_node(make_c_array(split->m_s0.m_both),
                                    make_c_array(split->m_s1.m_both),
                                    id, element_ordering));
        }

      range_type<unsigned int> children;
      children.m_begin = m_nodes.size();
      for (const auto &c : tmp)
        {
          m_nodes.push_back(c);
        }
      children.m_end = m_nodes.size();

      return Node(*this, Node::node_of_nodes, children, id);
    }
  else
    {
      return create_node(leaf_data0, leaf_data1, id, element_ordering);
    }
}

astral::reference_counted_ptr<astral::detail::StrokeDataHierarchy>
astral::detail::StrokeDataHierarchy::AnimatedHierarchy::
create(const RawAnimatedData &input,
       unsigned int *out_hierarchy_size,
       Ordering *element_ordering)
{
  static const unsigned int max_depth = 8u;
  static const unsigned int split_thresh = 4u;

  reference_counted_ptr<AnimatedHierarchy> return_value;
  std::vector<StaticLeafData> leaves0;
  std::vector<StaticLeafData> leaves1;
  BoundingBox<float> bb0, bb1;

  *out_hierarchy_size = 0;
  AnimatedLeafBuilder::create_leaves(&leaves0, &leaves1, &bb0, &bb1, input);
  return_value = ASTRALnew AnimatedHierarchy();
  return_value->m_leaves.reserve(leaves0.size());

  Node root(return_value->create_hierarchy(max_depth, split_thresh,
                                           bb0, make_c_array(leaves0),
                                           bb1, make_c_array(leaves1),
                                           out_hierarchy_size,
                                           element_ordering));
  return_value->m_nodes.push_back(root);
  ASTRALassert(return_value->m_leaves.size() == leaves0.size());

  return return_value;
}

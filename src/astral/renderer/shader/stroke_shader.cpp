/*!
 * \file stroke_shader.cpp
 * \brief file stroke_shader.cpp
 *
 * Copyright 2019 by InvisionApp.
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
#include <astral/util/ostream_utility.hpp>
#include <astral/renderer/shader/stroke_shader.hpp>
#include <astral/renderer/render_engine.hpp>
#include <astral/renderer/renderer.hpp>
#include "stroke_data_hierarchy.hpp"
#include "stroke_shader_vertex_index_roles.hpp"

class astral::StrokeShader::Packer
{
public:
  class VirtualOrdering
  {
  public:
    VirtualOrdering(const Ordering *p, enum primitive_type_t P):
      m_backed(p != nullptr)
    {
      if (p)
        {
          m_array = make_c_array(p->operator[](P));
        }
    }

    unsigned int
    operator[](unsigned int v) const
    {
      return m_backed ? m_array[v] : v;
    }

    bool
    same_size(unsigned int sz)
    {
      return m_backed ? (m_array.size() == sz) : true;
    }

  private:
    bool m_backed;
    c_array<const unsigned int> m_array;
  };

  uint32_t m_static_data_stride;
  uint32_t m_static_data_offset;
  VertexIndexRoles m_d;

  static
  uint32_t
  line_segment_stride(bool animated)
  {
    return (animated) ?
      line_segment_pair_static_data_size:
      line_segment_static_data_size;
  }

  static
  uint32_t
  quadratic_curve_stride(bool animated)
  {
    return (animated) ?
      quadratic_curve_pair_static_data_size:
      quadratic_curve_data_data_size;
  }

  static
  uint32_t
  get_flags(const Base &base)
  {
    return base.m_flags;
  }

  static
  uint32_t
  get_flags(const Cap &cap)
  {
    return cap.m_flags;
  }

  static
  uint32_t
  get_flags(const Join&)
  {
    return 0u;
  }

  static
  float
  vertex_data_value(const Base &base)
  {
    ASTRALassert(base.m_distance_from_contour_start >= 0.0f);
    ASTRALassert(base.m_distance_from_edge_start >= 0.0f);
    ASTRALassert(base.m_contour_length >= 0.0f);
    ASTRALassert(base.m_edge_length >= 0.0f);
    ASTRALassert(base.m_primitive_length >= 0.0f);

    return base.m_edge_length;
  }

  static
  float
  vertex_data_value(const Join &join)
  {
    ASTRALassert(join.m_contour_length >= 0.0f);
    ASTRALassert(join.m_pre_edge_length >= 0.0f);
    ASTRALassert(join.m_post_edge_length >= 0.0f);
    return join.m_distance_from_edge_start;
  }

  static
  float
  vertex_data_value(const Cap &cap)
  {
    ASTRALunused(cap);
    return 0.0f;
  }

  template<typename T>
  reference_counted_ptr<const VertexData>
  generic_create_vertex_data(RenderEngine &engine,
                             c_array<const T> values0, c_array<const T> values1,
                             VirtualOrdering ordering);

  static
  reference_counted_ptr<const VertexData>
  create_line_vertex_data(bool for_animated_path, unsigned int static_data_offset,
                          RenderEngine &engine,
                          c_array<const LineSegment> values0,
                          c_array<const LineSegment> values1,
                          VirtualOrdering ordering);

  static
  reference_counted_ptr<const VertexData>
  create_biarc_vertex_data(bool for_animated_path, unsigned int static_data_offset,
                           RenderEngine &engine,
                           c_array<const Quadratic> values0,
                           c_array<const Quadratic> values1,
                           VirtualOrdering ordering);

  template<typename T>
  static
  reference_counted_ptr<const VertexData>
  create_capper_vertex_data(unsigned int static_data_stride, unsigned int static_data_offset,
                            RenderEngine &engine,
                            c_array<const T> values0,
                            c_array<const T> values1,
                            VirtualOrdering ordering);

  static
  reference_counted_ptr<const VertexData>
  create_join_vertex_data(bool for_animated_path, unsigned int static_data_offset,
                          RenderEngine &engine,
                          c_array<const Join> values0,
                          c_array<const Join> values1,
                          VirtualOrdering ordering);

  static
  reference_counted_ptr<const VertexData>
  create_cap_vertex_data(bool for_animated_path, unsigned int static_data_offset,
                         RenderEngine &engine,
                         c_array<const Cap> values0,
                         c_array<const Cap> values1,
                         VirtualOrdering ordering);

  static
  uint32_t
  pack_static_data(c_array<const LineSegment> src, VirtualOrdering ordering,
                   uint32_t &offset, c_array<gvec4> dst);

  static
  uint32_t
  pack_static_data(c_array<const LineSegment> src0, c_array<const LineSegment> src1,
                   VirtualOrdering ordering,
                   uint32_t &offset, c_array<gvec4> dst);

  static
  uint32_t
  pack_static_data(c_array<const Quadratic> src, VirtualOrdering ordering,
                   uint32_t &offset, c_array<gvec4> dst);

  static
  uint32_t
  pack_static_data(c_array<const Quadratic> src0, c_array<const Quadratic> src1,
                   VirtualOrdering ordering,
                   uint32_t &offset, c_array<gvec4> dst);

  static
  uint32_t
  pack_static_data(c_array<const Join> src, VirtualOrdering ordering,
                   uint32_t &offset, c_array<gvec4> dst);

  static
  uint32_t
  pack_static_data(c_array<const Join> src0, c_array<const Join> src1,
                   VirtualOrdering ordering,
                   uint32_t &offset, c_array<gvec4> dst);

  static
  uint32_t
  pack_static_data(astral::c_array<const Cap> src, VirtualOrdering ordering,
                   uint32_t &offset, c_array<gvec4> dst);

  static
  uint32_t
  pack_static_data(c_array<const Cap> src0, c_array<const Cap> src1,
                   VirtualOrdering ordering,
                   uint32_t &offset, c_array<gvec4> dst);

  static
  float
  compute_length(const ContourCurve &input);

  static
  Base*
  process_curve(const ContourCurve &input,
                const RawData::SourceInfo &source_info,
                std::vector<LineSegment> *dst_segs,
                std::vector<Quadratic> *dst_quads,
                std::vector<RawData::Info> *dst_info,
                BoundingBox<float> *bb);

  static
  void
  add_glue_join(const RawData::SourceInfo &source_info,
                float distance_from_contour_start,
                float distance_from_edge_start,
                const ContourCurve &into,
                const ContourCurve &leaving,
                enum primitive_type_t tp,
                std::vector<Join> *dst, std::vector<RawData::Info> *dst_info)
  {
    add_join_implement(source_info,
                       distance_from_contour_start,
                       distance_from_edge_start,
                       into, leaving, tp,
                       dst, dst_info);
  }

  static
  void
  add_non_closing_join(const RawData::SourceInfo &source_info,
                       float distance_from_contour_start,
                       const ContourCurve &into,
                       const ContourCurve &leaving,
                       enum primitive_type_t tp,
                       std::vector<Join> *dst, std::vector<RawData::Info> *dst_info)
  {
    /* A real join is given -1.0 as the distance_from_edge_start
     * to indicate it is a real join but not a closing join.
     */
    add_join_implement(source_info,
                       distance_from_contour_start,
                       -1.0f,
                       into, leaving, tp,
                       dst, dst_info);
    ASTRALassert(dst->back().is_real_join());
  }

  static
  void
  add_closing_join(const RawData::SourceInfo &source_info,
                   const ContourCurve &into,
                   const ContourCurve &leaving,
                   enum primitive_type_t tp,
                   std::vector<Join> *dst, std::vector<RawData::Info> *dst_info)
  {
    /* the closing join is given as distance from the
     * contour start as 0; the -2.0 is to indicate
     * that is a closing join.
     */
    add_join_implement(source_info,
                       0.0f, -2.0f,
                       into, leaving, tp,
                       dst, dst_info);
    ASTRALassert(dst->back().is_closing_join());
    ASTRALassert(dst->back().is_real_join());
  }

  static
  void
  add_cap(const RawData::SourceInfo &source_info, const ContourCurve &at,
          bool is_start_cap, std::vector<Cap> *dst,
          std::vector<RawData::Info> *dst_info);

private:
  static
  void
  add_join_implement(const RawData::SourceInfo &source_info,
                     float distance_from_contour_start,
                     float distance_from_edge_start,
                     const ContourCurve &into,
                     const ContourCurve &leaving,
                     enum primitive_type_t tp,
                     std::vector<Join> *dst, std::vector<RawData::Info> *dst_info);
};

//////////////////////////////////////////////////
// astral::StrokeShader::VertexIndexRoles methods
astral::StrokeShader::VertexIndexRoles
astral::StrokeShader::VertexIndexRoles::
roles(enum primitive_type_t tp)
{
  switch (tp)
    {
    case line_segments_primitive:
      return lines();

    case biarc_curves_primitive:
      return biarcs();

    case glue_primitive:
    case glue_cusp_primitive:
    case joins_primitive:
    case inner_glue_primitive:
      return joins();

    case caps_primitive:
    case segments_cappers_primitive:
    case biarc_curves_cappers_primitive:
      return caps();

    default:
      ASTRALassert(!"Invalid primitive_type_t value");
      return VertexIndexRoles();
    }
}

astral::StrokeShader::VertexIndexRoles
astral::StrokeShader::VertexIndexRoles::
lines(void)
{
  VertexIndexRoles G;

  static const uint32_t line_indices[12] =
    {
      0, 1, 4,
      0, 5, 4,
      1, 2, 3,
      1, 3, 4
    };
  G.m_indices = c_array<const uint32_t>(line_indices, 12u);

  static const uint32_t vertex_role[6] =
    {
      [0] = line_offset_negate_normal,
      [1] = line_offset_base_point,
      [2] = line_offset_normal,
      [3] = line_offset_normal | line_is_end_point_mask,
      [4] = line_offset_base_point | line_is_end_point_mask,
      [5] = line_offset_negate_normal | line_is_end_point_mask,
    };
  G.m_vertex_roles = c_array<const uint32_t>(vertex_role, 6u);

  return G;
}

astral::StrokeShader::VertexIndexRoles
astral::StrokeShader::VertexIndexRoles::
biarcs(void)
{
  VertexIndexRoles G;

  static const uint32_t vertex_role[24] =
    {
      /* 0 */ biarc_offset_omega,
      /* 1 */ biarc_offset_zeta,
      /* 2 */ biarc_offset_towards_center,
      /* 3 */ biarc_offset_base_point,
      /* 4 */ biarc_offset_away_from_center,
      /* 5 */ biarc_offset_top,

      /* 6 */ biarc_is_end_point_mask | biarc_offset_top,
      /* 7 */ biarc_is_end_point_mask | biarc_offset_away_from_center,
      /* 8 */ biarc_is_end_point_mask | biarc_offset_base_point,
      /* 9 */ biarc_is_end_point_mask | biarc_offset_towards_center,
      /* 10 */ biarc_is_end_point_mask | biarc_offset_zeta,
      /* 11 */ biarc_is_end_point_mask | biarc_offset_omega,

      /* 12 */ biarc_is_second_arc_mask | biarc_is_end_point_mask | biarc_offset_omega,
      /* 13 */ biarc_is_second_arc_mask | biarc_is_end_point_mask | biarc_offset_zeta,
      /* 14 */ biarc_is_second_arc_mask | biarc_is_end_point_mask | biarc_offset_towards_center,
      /* 15 */ biarc_is_second_arc_mask | biarc_is_end_point_mask | biarc_offset_base_point,
      /* 16 */ biarc_is_second_arc_mask | biarc_is_end_point_mask | biarc_offset_away_from_center,
      /* 17 */ biarc_is_second_arc_mask | biarc_is_end_point_mask | biarc_offset_top,

      /* 18 */ biarc_is_second_arc_mask | biarc_offset_top,
      /* 19 */ biarc_is_second_arc_mask | biarc_offset_away_from_center,
      /* 20 */ biarc_is_second_arc_mask | biarc_offset_base_point,
      /* 21 */ biarc_is_second_arc_mask | biarc_offset_towards_center,
      /* 22 */ biarc_is_second_arc_mask | biarc_offset_zeta,
      /* 23 */ biarc_is_second_arc_mask | biarc_offset_omega,
    };

  /* TODO: this is way too many triangles (a whopping 20 triangles);
   *       the entire point of the arc is to reduce the geometry load,
   *       but at 20 triangles we could make a fake a sequence of
   *       line segments (10 to be precise because the interior rects
   *       do not need to include the middle line). There has got to be
   *       a way to lower this count.
   *
   * This sequence of indices ASSUMES that the provoking vertex or
   * flat varying is the LAST vertex. This is the default convention
   * in OpenGL and OpenGL ES. In addition, for OpenGL ES 3.1 or earlier,
   * it is the only convention.
   *
   * The main issue is that the vertex shader will subtract the arc-radius
   * from the stroking radius and pass the arc-radius as 0 when the stroking
   * radius exceeds the arc-radius on the point biarc_offset_omega *ONLY*.
   * Thus, that vertex needs to be the provoking vertex for the triangles
   * that fill the inversion inner stroke and should not be the provoking
   * vertex for anything else.
   */
  static const uint32_t curve_indices[] =
    {
      /* BLOCK 1: triangles that fill the region between
       * the inner and outer offsets of the first arc.
       *
       * When the stroking radius is greater than the arc radius,
       * the triangles [2, 5, 6] and [6, 7, 9] fill the region
       * between the outer offset and the arc-center while the
       * triangle [2, 6, 9] degenerates to a point (arc-center).
       */
      2, 4, 5,
      2, 5, 6,
      2, 6, 9,
      6, 7, 9,

      /* BLOCK 2: These 3 triangles degenerate to a point when the stroking
       * radius is smaller than the arc-radius of the first arc. When the
       * stroking radius exceeds the arc-radius, they for a fan from the
       * arc-center of the first arc that covers the inverted portion of
       * the stroke from the first arc. Because they are of this inverted
       * arc, it is critical that the provoking vertex "lies" to the fragment
       * shader, i.e. the providing vertex must be an biarc_offset_omega
       * vertex which is index 0 or 11.
       */
      10, 2, 11,
      11, 2, 0,
      1, 2, 0,

      /* BLOCK 3: triangles for the second arc that do the same purpose
       * as triangles of BLOCK 1 for the first arc.
       *
       * When the stroking radius is greater than the arc radius,
       * the triangles [12 + 2, 12 + 5, 12 + 6] and [12 + 6, 12 + 7, 12 + 9]
       * fill the region between the outer offset and the arc-center while the
       * triangle [2, 6, 9] degenerates to a point (arc-center).
       */
      12 + 2, 12 + 4, 12 + 5,
      12 + 2, 12 + 5, 12 + 6,
      12 + 2, 12 + 6, 12 + 9,
      12 + 6, 12 + 7, 12 + 9,

      /* BLOCK 4: triangles for the second arc that do the same purpose
       * as triangles of BLOCK 2 for the first arc.
       */
      12 + 10, 12 + 2, 12 + 11,
      12 + 11, 12 + 2, 12 + 0,
      12 + 1, 12 + 2, 12 + 0,

      /*
       * The triangles [10, 9, 7] and [12 + 10, 12 + 9, 12 + 7]
       * are in exact arithmetic degenerate; these triangles are
       * present to eliminate T-intersections caused by two arcs
       * having different centers so there is no crack between the
       * end of the first arc and the start of the second arc.
       */
      10, 9, 7,
      12 + 10, 12 + 9, 12 + 7,

      /*
       * The triangles [1, 2, 4] and [12 + 1, 12 + 2, 12 + 4]
       * are in exact arithmetic degenerate; these triangles are
       * present to eliminate T-intersections introduced by arc
       * centers on biarc edges adjacent to neighboring stroke
       * segments (line, biarc, glue, join, or cap).
       */
      1, 2, 4,
      12 + 1, 12 + 2, 12 + 4,

      /*
       * The triangles [1, 3, 4] and [12 + 1, 12 + 3, 12 + 4]
       * are in exact arithmetic degenerate; these triangles are
       * present to eliminate T-intersections introduced by base
       * points on rounded and bevel joins adjacent to the biarc.
       */
      1, 3, 4,
      12 + 1, 12 + 3, 12 + 4,
    };

  size_t num_indices = sizeof(curve_indices) / sizeof(uint32_t);
  G.m_indices = c_array<const uint32_t>(curve_indices, num_indices);
  G.m_vertex_roles = c_array<const uint32_t>(vertex_role, 24u);

  return G;
}

astral::StrokeShader::VertexIndexRoles
astral::StrokeShader::VertexIndexRoles::
joins(void)
{
  VertexIndexRoles G;

  static const uint32_t join_vertices[] =
    {
      join_point_on_path,
      join_point_edge_boundary,
      join_point_beyond_boundary,
      join_point_leave_mask | join_point_beyond_boundary,
      join_point_leave_mask | join_point_edge_boundary,
    };

  G.m_vertex_roles = c_array<const uint32_t>(join_vertices, 5u);

  /* NOTE! The way to fill the triangles for the join
   * is -important-. For miter and bevel joins, the
   * join_point_on_path will be given for the anti-aliasing
   * interpolate a value of 1 and all others 0. By making
   * the triangles center fan the vertex join_point_on_path,
   * the interpolate will be 0 on the boundary vertices
   * of the stroke. The shader for joins will then do
   * the standard trick of using fwidth() to get so that
   * only the boundary pixels emit a coverage value of
   * less than one. Note that this will perfectly match
   * what line segments do as well.
   */
  static const Index join_indices[] =
    {
      0, 1, 2,
      0, 2, 3,
      0, 3, 4
    };
  G.m_indices = c_array<const uint32_t>(join_indices, 9u);

  return G;
}

astral::StrokeShader::VertexIndexRoles
astral::StrokeShader::VertexIndexRoles::
caps(void)
{
  VertexIndexRoles G;

  static const uint32_t cap_vertices[] =
    {
      cap_point_path,
      cap_point_edge_boundary,
      cap_point_beyond_boundary,
      cap_point_side_mask | cap_point_beyond_boundary,
      cap_point_side_mask | cap_point_edge_boundary,
    };

  G.m_vertex_roles = c_array<const uint32_t>(cap_vertices, 5u);

  static const Index cap_indices[] =
    {
      0, 1, 2,
      0, 2, 3,
      0, 3, 4
    };
  G.m_indices = c_array<const uint32_t>(cap_indices, 9u);

  return G;
}

///////////////////////////////////////////////////
// astral::StrokeShader::Packer methods
uint32_t
astral::StrokeShader::Packer::
pack_static_data(c_array<const LineSegment> src, VirtualOrdering ordering,
                 uint32_t &offset, c_array<gvec4> dst)
{
  unsigned int return_value(offset);

  ASTRALassert(ordering.same_size(src.size()));
  for (unsigned int a = 0; a < src.size(); ++a, offset += line_segment_static_data_size)
    {
      unsigned int i(ordering[a]);

      dst[offset + 0u].x().f = src[i].m_pts[0].x();
      dst[offset + 0u].y().f = src[i].m_pts[0].y();
      dst[offset + 0u].z().f = src[i].m_pts[1].x();
      dst[offset + 0u].w().f = src[i].m_pts[1].y();

      dst[offset + 1u].x().f = src[i].m_distance_from_contour_start;
      dst[offset + 1u].y().f = src[i].m_primitive_length;
      dst[offset + 1u].z().f = src[i].m_distance_from_edge_start;
      dst[offset + 1u].w().f = src[i].m_contour_length;
    }

  return return_value;
}

uint32_t
astral::StrokeShader::Packer::
pack_static_data(c_array<const LineSegment> src0, c_array<const LineSegment> src1,
                 VirtualOrdering ordering,
                 uint32_t &offset, astral::c_array<gvec4> dst)
{
  unsigned int return_value(offset);

  ASTRALassert(ordering.same_size(src0.size()));
  ASTRALassert(src0.size() == src1.size());
  for (unsigned int a = 0; a < src0.size(); ++a, offset += line_segment_pair_static_data_size)
    {
      unsigned int i(ordering[a]);

      dst[offset + 0u].x().f = src0[i].m_pts[0].x();
      dst[offset + 0u].y().f = src0[i].m_pts[0].y();
      dst[offset + 0u].z().f = src0[i].m_pts[1].x();
      dst[offset + 0u].w().f = src0[i].m_pts[1].y();

      dst[offset + 1u].x().f = src1[i].m_pts[0].x();
      dst[offset + 1u].y().f = src1[i].m_pts[0].y();
      dst[offset + 1u].z().f = src1[i].m_pts[1].x();
      dst[offset + 1u].w().f = src1[i].m_pts[1].y();

      dst[offset + 2u].x().f = src0[i].m_distance_from_contour_start;
      dst[offset + 2u].y().f = src1[i].m_distance_from_contour_start;

      dst[offset + 2u].z().f = src0[i].m_primitive_length;
      dst[offset + 2u].w().f = src1[i].m_primitive_length;

      dst[offset + 3u].x().f = src0[i].m_distance_from_edge_start;
      dst[offset + 3u].y().f = src1[i].m_distance_from_edge_start;
      dst[offset + 3u].z().f = src0[i].m_contour_length;
      dst[offset + 3u].w().f = src1[i].m_contour_length;
    }
  return return_value;
}

uint32_t
astral::StrokeShader::Packer::
pack_static_data(c_array<const Quadratic> src,
                 VirtualOrdering ordering,
                 uint32_t &offset, c_array<gvec4> dst)
{
  unsigned int return_value(offset);

  ASTRALassert(ordering.same_size(src.size()));
  for (unsigned int a = 0; a < src.size(); ++a, offset += quadratic_curve_data_data_size)
    {
      unsigned int i(ordering[a]);

      dst[offset + 0u].x().f = src[i].m_pts[0].x();
      dst[offset + 0u].y().f = src[i].m_pts[0].y();
      dst[offset + 0u].z().f = src[i].m_pts[1].x();
      dst[offset + 0u].w().f = src[i].m_pts[1].y();

      dst[offset + 1u].x().f = src[i].m_pts[2].x();
      dst[offset + 1u].y().f = src[i].m_pts[2].y();
      dst[offset + 1u].z().f = src[i].m_distance_from_contour_start;
      dst[offset + 1u].w().f = src[i].m_primitive_length;

      dst[offset + 2u].x().f = src[i].m_distance_from_edge_start;
      dst[offset + 2u].y().f = src[i].m_contour_length;
    }

  return return_value;
}

uint32_t
astral::StrokeShader::Packer::
pack_static_data(c_array<const Quadratic> src0, c_array<const Quadratic> src1,
                 VirtualOrdering ordering,
                 uint32_t &offset, c_array<gvec4> dst)
{
  unsigned int return_value(offset);

  ASTRALassert(ordering.same_size(src0.size()));
  ASTRALassert(src0.size() == src1.size());
  for (unsigned int a = 0; a < src0.size(); ++a, offset += quadratic_curve_pair_static_data_size)
    {
      unsigned int i(ordering[a]);

      dst[offset + 0u].x().f = src0[i].m_pts[0].x();
      dst[offset + 0u].y().f = src0[i].m_pts[0].y();
      dst[offset + 0u].z().f = src1[i].m_pts[0].x();
      dst[offset + 0u].w().f = src1[i].m_pts[0].y();

      dst[offset + 1u].x().f = src0[i].m_pts[1].x();
      dst[offset + 1u].y().f = src0[i].m_pts[1].y();
      dst[offset + 1u].z().f = src1[i].m_pts[1].x();
      dst[offset + 1u].w().f = src1[i].m_pts[1].y();

      dst[offset + 2u].x().f = src0[i].m_pts[2].x();
      dst[offset + 2u].y().f = src0[i].m_pts[2].y();
      dst[offset + 2u].z().f = src1[i].m_pts[2].x();
      dst[offset + 2u].w().f = src1[i].m_pts[2].y();

      dst[offset + 3u].x().f = src0[i].m_distance_from_contour_start;
      dst[offset + 3u].y().f = src1[i].m_distance_from_contour_start;
      dst[offset + 3u].z().f = src0[i].m_primitive_length;
      dst[offset + 3u].w().f = src1[i].m_primitive_length;

      dst[offset + 4u].x().f = src0[i].m_distance_from_edge_start;
      dst[offset + 4u].y().f = src1[i].m_distance_from_edge_start;
      dst[offset + 4u].z().f = src0[i].m_contour_length;
      dst[offset + 4u].w().f = src1[i].m_contour_length;
    }

  return return_value;
}

uint32_t
astral::StrokeShader::Packer::
pack_static_data(c_array<const Join> src,
                 VirtualOrdering ordering,
                 uint32_t &offset, c_array<gvec4> dst)
{
  unsigned int return_value(offset);

  ASTRALassert(ordering.same_size(src.size()));
  for (unsigned int a = 0; a < src.size(); ++a, offset += join_static_data_size)
    {
      unsigned int i(ordering[a]);

      dst[offset + 0u].x().f = src[i].m_p.x();
      dst[offset + 0u].y().f = src[i].m_p.y();
      dst[offset + 0u].z().f = src[i].m_pre_p.x();
      dst[offset + 0u].w().f = src[i].m_pre_p.y();

      dst[offset + 1u].x().f = src[i].m_post_p.x();
      dst[offset + 1u].y().f = src[i].m_post_p.y();

      dst[offset + 2u].x().f = src[i].m_distance_from_contour_start;
      dst[offset + 2u].y().f = src[i].m_pre_edge_length;
      dst[offset + 2u].z().f = src[i].m_post_edge_length;
      dst[offset + 2u].w().f = src[i].m_contour_length;

      ASTRALassert(src[i].m_pre_edge_length >= 0.0f);
      ASTRALassert(src[i].m_post_edge_length >= 0.0f);
      ASTRALassert(src[i].m_contour_length >= 0.0f);
    }

  return return_value;
}

uint32_t
astral::StrokeShader::Packer::
pack_static_data(c_array<const Join> src0, c_array<const Join> src1,
                 VirtualOrdering ordering,
                 uint32_t &offset, c_array<gvec4> dst)
{
  unsigned int return_value(offset);

  ASTRALassert(ordering.same_size(src0.size()));
  ASTRALassert(src0.size() == src1.size());
  for (unsigned int a = 0; a < src0.size(); ++a, offset += join_pair_static_data_size)
    {
      unsigned int i(ordering[a]);

      dst[offset + 0u].x().f = src0[i].m_p.x();
      dst[offset + 0u].y().f = src0[i].m_p.y();
      dst[offset + 0u].z().f = src0[i].m_pre_p.x();
      dst[offset + 0u].w().f = src0[i].m_pre_p.y();

      dst[offset + 1u].x().f = src0[i].m_post_p.x();
      dst[offset + 1u].y().f = src0[i].m_post_p.y();
      dst[offset + 1u].z().f = src1[i].m_p.x();
      dst[offset + 1u].w().f = src1[i].m_p.y();

      dst[offset + 2u].x().f = src1[i].m_pre_p.x();
      dst[offset + 2u].y().f = src1[i].m_pre_p.y();
      dst[offset + 2u].z().f = src1[i].m_post_p.x();
      dst[offset + 2u].w().f = src1[i].m_post_p.y();

      dst[offset + 3u].x().f = src0[i].m_distance_from_contour_start;
      dst[offset + 3u].y().f = src1[i].m_distance_from_contour_start;
      dst[offset + 3u].z().f = src0[i].m_contour_length;
      dst[offset + 3u].w().f = src1[i].m_contour_length;

      dst[offset + 4u].x().f = src0[i].m_pre_edge_length;
      dst[offset + 4u].y().f = src1[i].m_pre_edge_length;
      dst[offset + 4u].z().f = src0[i].m_post_edge_length;
      dst[offset + 4u].w().f = src1[i].m_post_edge_length;
    }

  return return_value;
}

uint32_t
astral::StrokeShader::Packer::
pack_static_data(c_array<const Cap> src,
                 VirtualOrdering ordering,
                 uint32_t &offset, c_array<gvec4> dst)
{
  unsigned int return_value(offset);

  ASTRALassert(ordering.same_size(src.size()));
  for (unsigned int a = 0; a < src.size(); ++a, offset += cap_static_data_size)
    {
      unsigned int i(ordering[a]);

      dst[offset + 0u].x().f = src[i].m_p.x();
      dst[offset + 0u].y().f = src[i].m_p.y();
      dst[offset + 0u].z().f = src[i].m_neighbor_p.x();
      dst[offset + 0u].w().f = src[i].m_neighbor_p.y();

      dst[offset + 1u].x().f = src[i].m_contour_length;
      dst[offset + 1u].y().f = src[i].m_edge_length;
    }

  return return_value;
}

uint32_t
astral::StrokeShader::Packer::
pack_static_data(c_array<const Cap> src0, c_array<const Cap> src1,
                 VirtualOrdering ordering,
                 uint32_t &offset, c_array<gvec4> dst)
{
  unsigned int return_value(offset);

  ASTRALassert(ordering.same_size(src0.size()));
  ASTRALassert(src0.size() == src1.size());
  for (unsigned int a = 0; a < src0.size(); ++a, offset += cap_pair_static_data_size)
    {
      unsigned int i(ordering[a]);

      dst[offset + 0u].x().f = src0[i].m_p.x();
      dst[offset + 0u].y().f = src0[i].m_p.y();
      dst[offset + 0u].z().f = src0[i].m_neighbor_p.x();
      dst[offset + 0u].w().f = src0[i].m_neighbor_p.y();

      dst[offset + 1u].x().f = src1[i].m_p.x();
      dst[offset + 1u].y().f = src1[i].m_p.y();
      dst[offset + 1u].z().f = src1[i].m_neighbor_p.x();
      dst[offset + 1u].w().f = src1[i].m_neighbor_p.y();

      dst[offset + 2u].x().f = src0[i].m_contour_length;
      dst[offset + 2u].y().f = src1[i].m_contour_length;
      dst[offset + 2u].z().f = src0[i].m_edge_length;
      dst[offset + 2u].w().f = src1[i].m_edge_length;
    }

  return return_value;
}

template<typename T>
astral::reference_counted_ptr<const astral::VertexData>
astral::StrokeShader::Packer::
generic_create_vertex_data(RenderEngine &engine,
                           c_array<const T> values0,
                           c_array<const T> values1,
                           VirtualOrdering ordering)
{
  unsigned int num_verts, num_indices, data_count;

  data_count = values0.size();
  num_verts = m_d.m_vertex_roles.size() * data_count;
  num_indices = m_d.m_indices.size() * data_count;

  std::vector<astral::Vertex> vertices(num_verts);
  std::vector<astral::Index> indices(num_indices);

  ASTRALassert(values0.size() == values1.size());
  ASTRALassert(ordering.same_size(values0.size()));
  for (unsigned int c = 0, vert = 0, idx = 0, o = m_static_data_offset;
       c < data_count; ++c, o += m_static_data_stride)
    {
      for (unsigned int k = 0; k < m_d.m_indices.size(); ++k, ++idx)
        {
          indices[idx] = m_d.m_indices[k] + vert;
        }
      for (unsigned int v = 0; v < m_d.m_vertex_roles.size(); ++v, ++vert)
        {
          uint32_t flag_bits, role_bits, id_bits;

          ASTRALassert(get_flags(values0[ordering[c]]) == get_flags(values1[ordering[c]]));
          flag_bits = pack_bits(flags_bit0, flags_number_bits, get_flags(values0[ordering[c]]));
          role_bits = pack_bits(role_bit0, role_number_bits, m_d.m_vertex_roles[v]);
          id_bits = ordering[c] << id_bit0;

          vertices[vert].m_data[0].u = o;
          vertices[vert].m_data[1].u = role_bits | flag_bits | id_bits;
          vertices[vert].m_data[2].f = vertex_data_value(values0[ordering[c]]);
          vertices[vert].m_data[3].f = vertex_data_value(values1[ordering[c]]);
        }
    }

  return engine.vertex_data_allocator().create(make_c_array(vertices), make_c_array(indices));
}

astral::reference_counted_ptr<const astral::VertexData>
astral::StrokeShader::Packer::
create_line_vertex_data(bool for_animated_path, unsigned int static_data_offset,
                        RenderEngine &engine,
                        c_array<const LineSegment> values0,
                        c_array<const LineSegment> values1,
                        VirtualOrdering ordering)
{
  Packer G;

  G.m_d = VertexIndexRoles::lines();
  G.m_static_data_offset = static_data_offset;
  G.m_static_data_stride = (for_animated_path) ?
    line_segment_pair_static_data_size:
    line_segment_static_data_size;

  return G.generic_create_vertex_data(engine, values0, values1, ordering);
}

astral::reference_counted_ptr<const astral::VertexData>
astral::StrokeShader::Packer::
create_biarc_vertex_data(bool for_animated_path, unsigned int static_data_offset,
                         RenderEngine &engine,
                         c_array<const Quadratic> values0,
                         c_array<const Quadratic> values1,
                         VirtualOrdering ordering)
{
  Packer G;

  G.m_d = VertexIndexRoles::biarcs();
  G.m_static_data_offset = static_data_offset;
  G.m_static_data_stride = (for_animated_path) ?
    quadratic_curve_pair_static_data_size:
    quadratic_curve_data_data_size;

  return G.generic_create_vertex_data(engine, values0, values1, ordering);
}

astral::reference_counted_ptr<const astral::VertexData>
astral::StrokeShader::Packer::
create_join_vertex_data(bool for_animated_path, unsigned int static_data_offset,
                        RenderEngine &engine,
                        c_array<const Join> values0,
                        c_array<const Join> values1,
                        VirtualOrdering ordering)
{
  Packer G;

  G.m_d = VertexIndexRoles::joins();
  G.m_static_data_offset = static_data_offset;
  G.m_static_data_stride = (for_animated_path) ?
    join_pair_static_data_size:
    join_static_data_size;

  return G.generic_create_vertex_data(engine, values0, values1, ordering);
}

template<typename T>
astral::reference_counted_ptr<const astral::VertexData>
astral::StrokeShader::Packer::
create_capper_vertex_data(unsigned int static_data_stride, unsigned int static_data_offset,
                          RenderEngine &engine,
                          c_array<const T> values0,
                          c_array<const T> values1,
                          VirtualOrdering ordering)
{
  Packer G;

  G.m_d = VertexIndexRoles::caps();
  G.m_static_data_offset = static_data_offset;
  G.m_static_data_stride = static_data_stride;

  return G.generic_create_vertex_data(engine, values0, values1, ordering);
}

astral::reference_counted_ptr<const astral::VertexData>
astral::StrokeShader::Packer::
create_cap_vertex_data(bool for_animated_path, unsigned int static_data_offset,
                       RenderEngine &engine,
                       c_array<const Cap> values0,
                       c_array<const Cap> values1,
                       VirtualOrdering ordering)
{
  Packer G;

  G.m_d = VertexIndexRoles::caps();
  G.m_static_data_offset = static_data_offset;
  G.m_static_data_stride = (for_animated_path) ?
    cap_pair_static_data_size:
    cap_static_data_size;

  return G.generic_create_vertex_data(engine, values0, values1, ordering);
}

float
astral::StrokeShader::Packer::
compute_length(const astral::ContourCurve &input)
{
  if (input.type() == astral::ContourCurve::quadratic_bezier)
    {
      typedef double real;
      const real small = 1e-6;

      real a, b, c, y0, y1, B, C, Q, D, r0, r1, F0, F1;
      Polynomial<vecN<real, 2>, 2> gamma;
      Polynomial<vecN<real, 2>, 1> gamma_prime;
      Polynomial<real, 2> integrand_sq;
      vecN<real, 2> start(input.start_pt());
      vecN<real, 2> end(input.end_pt());
      vecN<real, 2> control(input.control_pt(0));

      gamma.coeff(2) = start - real(2) * control + end;
      gamma.coeff(1) = real(2) * (control - start);
      gamma.coeff(0) = start;

      gamma_prime = gamma.derivative();
      integrand_sq = poly_dot(gamma_prime, gamma_prime);

      a = integrand_sq.coeff(2);
      b = integrand_sq.coeff(1);
      c = integrand_sq.coeff(0);

      if (astral::t_abs(a) < small)
        {
          return (input.start_pt() - input.end_pt()).magnitude();
        }

      /* We need to integrate
       *
       *  g(t) = sqrt(at^2 + bt + c)
       *
       * over [0, 1]. Now for some high school calculas:
       *
       *  g(t) = sqrt(a) * sqrt(t^2 + Bt + C)
       *
       * where B = b/a, C = c/a
       *
       *  g(t) = sqrt(a) * sqrt(t^2 + Bt + C)
       *       = sqrt(a) * sqrt((t + Q)^2 + D)
       *
       * where Q = B / 2, D = C - B^2 / 4. Now let
       * y = t + Q, then we need to integrate
       *
       *  f(y) = sqrt(y^2 + D)
       *
       * which from a table (or do the integration substition doing
       * y = sqrt(D) * sinh(t) for D > 0 and y = sqrt(D) * cosh(t)
       * for D < 0) to get F' = f where
       *
       *            y * sqrt(y^2 + D) + D * log(y + sqrt(y^2 + D))
       *   F(y) = ------------------------------------------------
       *                             2
       *
       * see for instance http://www.physics.umd.edu/hep/drew/IntegralTable.pdf (30)
       */
      B = b / a;
      C = c / a;
      Q = B / real(2);
      D = C - Q * Q;

      y0 = Q;
      y1 = real(1) + Q;

      /* r0 = sqrt(y0 * y0 + D)
       *    = sqrt(Q * Q + D)
       *    = sqrt(Q * Q + C - Q * Q)
       *    = sqrt(C)
       */
      r0 = t_sqrt(t_max(real(0), C));

      /* r1 = sqrt(y1 * y1 + D)
       *    = sqrt((Q + 1) * (Q + 1) + D)
       *    = sqrt(Q * Q + 2Q + 1 + C - Q * Q)
       *    = sqrt(2Q + 1 + C)
       */
      r1 = t_sqrt(t_max(real(0), real(2) * Q + real(1) + C));

      const real tiny_log(1e-12);
      F0 = y0 * r0 + D * t_log(t_max(tiny_log, y0 + r0));
      F1 = y1 * r1 + D * t_log(t_max(tiny_log, y1 + r1));

      return t_sqrt(a) * (F1 - F0) / real(2);
    }
  else
    {
      ASTRALassert(input.type() == astral::ContourCurve::line_segment);
      return (input.start_pt() - input.end_pt()).magnitude();
    }
}

astral::StrokeShader::Base*
astral::StrokeShader::Packer::
process_curve(const ContourCurve &input,
              const RawData::SourceInfo &source_info,
              std::vector<LineSegment> *dst_segs,
              std::vector<Quadratic> *dst_quads,
              std::vector<RawData::Info> *dst_info,
              BoundingBox<float> *bb)
{
  Base *return_value;

  if (input.type() == ContourCurve::quadratic_bezier)
    {
      Quadratic v;

      v.m_pts[0] = input.start_pt();
      v.m_pts[1] = input.control_pt(0);
      v.m_pts[2] = input.end_pt();

      bb->union_point(v.m_pts[0]);
      bb->union_point(v.m_pts[1]);
      bb->union_point(v.m_pts[2]);

      dst_info->push_back(RawData::Info(biarc_curves_primitive, dst_quads->size(), source_info));
      dst_quads->push_back(v);

      return_value = &dst_quads->back();
    }
  else
    {
      LineSegment v;

      ASTRALassert(input.type() == ContourCurve::line_segment);
      v.m_pts[0] = input.start_pt();
      v.m_pts[1] = input.end_pt();

      bb->union_point(v.m_pts[0]);
      bb->union_point(v.m_pts[1]);

      dst_info->push_back(RawData::Info(line_segments_primitive, dst_segs->size(), source_info));
      dst_segs->push_back(v);

      return_value = &dst_segs->back();
    }

  return_value->m_distance_from_contour_start = -1.0f;
  return_value->m_distance_from_edge_start = -1.0f;
  return_value->m_contour_length = -1.0f;
  return_value->m_edge_length = -1.0f;
  return_value->m_primitive_length = -1.0f;

  return return_value;
}

void
astral::StrokeShader::Packer::
add_join_implement(const RawData::SourceInfo &source_info,
                   float distance_from_contour_start,
                   float distance_from_edge_start,
                   const ContourCurve &into,
                   const ContourCurve &leaving,
                   enum primitive_type_t tp,
                   std::vector<Join> *dst, std::vector<RawData::Info> *dst_info)
{
  Join J;

  J.m_distance_from_contour_start = distance_from_contour_start;
  J.m_distance_from_edge_start = distance_from_edge_start;
  J.m_p = leaving.start_pt();
  J.m_pre_p = (into.number_control_pts() != 0u) ? into.control_pt(0) : into.start_pt();
  J.m_post_p = (leaving.number_control_pts() != 0u) ? leaving.control_pt(0) : leaving.end_pt();

  /* to be filled later */
  J.m_contour_length = -1.0f;
  J.m_pre_edge_length = -1.0f;
  J.m_post_edge_length = -1.0f;

  dst_info->push_back(RawData::Info(tp, dst->size(), source_info));
  dst->push_back(J);
}

void
astral::StrokeShader::Packer::
add_cap(const RawData::SourceInfo &source_info, const ContourCurve &curve,
        bool is_start_cap, std::vector<Cap> *dst,
        std::vector<RawData::Info> *dst_info)
{
  Cap C;

  C.m_flags = (is_start_cap) ? 0u : cap_end_mask;
  if (is_start_cap)
    {
      C.m_p = curve.start_pt();
      C.m_neighbor_p = (curve.number_control_pts() != 0u) ? curve.control_pt(0) : curve.end_pt();
    }
  else
    {
      C.m_p = curve.end_pt();
      C.m_neighbor_p = (curve.number_control_pts() != 0u) ? curve.control_pt(0) : curve.start_pt();
    }

  dst_info->push_back(RawData::Info(caps_primitive, dst->size(), source_info));
  dst->push_back(C);
}

//////////////////////////////////////////////////////////////
// astral::StrokeShader::RawData methods
astral::StrokeShader::RawData::
RawData(void)
{
  m_current.m_contour_id = 0;
}

astral::StrokeShader::RawData::
~RawData()
{
}

astral::StrokeShader::RawData&
astral::StrokeShader::RawData::
add_contour(bool is_closed, c_array<const ContourCurve> curves)
{
  return add_contour_implement(is_closed, curves,
                               c_array<const ContourCurve>(), nullptr);
}

astral::StrokeShader::RawData&
astral::StrokeShader::RawData::
add_point_cap(vec2 p)
{
  Cap C;
  vec2 N(2.0f * p.magnitude() + 1.0f, 0.0);

  m_current.m_edge_id = 0;
  m_current.m_sub_edge_id = 0;
  m_bb.union_point(p);

  C.m_p = p;
  C.m_contour_length = 0.0f;
  C.m_edge_length = 0.0f;

  C.m_flags = 0u;
  C.m_neighbor_p = C.m_p + N;
  m_info.push_back(Info(caps_primitive, m_caps.size(), m_current));
  m_caps.push_back(C);

  C.m_flags = cap_end_mask;
  C.m_neighbor_p = C.m_p - N;
  m_info.push_back(Info(caps_primitive, m_caps.size(), m_current));
  m_caps.push_back(C);

  ++m_current.m_contour_id;

  return *this;
}

astral::StrokeShader::RawData&
astral::StrokeShader::RawData::
add_contour_implement(bool is_closed,
                      c_array<const ContourCurve> curves,
                      c_array<const ContourCurve> pair_curves,
                      std::vector<Join> *dst_inner_glue)
{
  if (curves.empty())
    {
      return *this;
    }

  ASTRALassert(!dst_inner_glue || pair_curves.size() == curves.size());

  float edge_length(0.0f), contour_length(0.0f);
  unsigned int c_si(m_line_segments.size()), c_sa(m_biarc_curves.size());
  unsigned int e_si(m_line_segments.size()), e_sa(m_biarc_curves.size());
  Base *prev_base(nullptr);
  Join *prev_join(nullptr);
  const ContourCurve *prev_curve(nullptr);
  unsigned int c_sj(m_joins.size()), first_join(m_joins.size());
  unsigned int c_sg(m_glue.size()), e_sg(m_glue.size());
  unsigned int c_sgb(m_glue_cusp.size()), e_sgb(m_glue_cusp.size());
  unsigned int c_sig(dst_inner_glue ? dst_inner_glue->size() : 0);
  unsigned int e_sig(dst_inner_glue ? dst_inner_glue->size() : 0);

  m_current.m_edge_id = 0;
  m_current.m_sub_edge_id = 0;

  if (!is_closed)
    {
      Packer::add_cap(m_current, curves.front(), true, &m_caps, &m_info);
    }

  for (unsigned int i = 0; i < curves.size(); ++i)
    {
      const auto &curve(curves[i]);

      float d(Packer::compute_length(curve));
      Base *base;
      enum ContourCurve::continuation_t continuation(curve.continuation());

      /* if the curves of an animated pair do not have matching continuation
       * types, then we add a join that will respect the join style,
       * and also add an inner join to prevent cracks.
       */
      if (dst_inner_glue && continuation != pair_curves[i].continuation())
        {
          continuation = ContourCurve::not_continuation_curve;
          if (prev_curve)
            {
              Packer::add_glue_join(m_current, contour_length, edge_length,
                                    *prev_curve, curve, inner_glue_primitive,
                                    dst_inner_glue, &m_info);
            }
        }

      if (continuation == ContourCurve::not_continuation_curve)
        {
          ++m_current.m_edge_id;
          m_current.m_sub_edge_id = 0;

          if (prev_base)
            {
              if (prev_join)
                {
                  prev_join->m_post_edge_length = edge_length;
                }

              prev_base->m_flags |= end_edge_mask;

              ASTRALassert(prev_curve);
              Packer::add_non_closing_join(m_current, contour_length,
                                           *prev_curve, curve, joins_primitive,
                                           &m_joins, &m_info);

              m_joins.back().m_pre_edge_length = edge_length;
              prev_join = &m_joins.back();
            }
        }
      else if (prev_curve)
        {
          std::vector<Join> *dst_join;
          enum primitive_type_t primitive_type;

          ++m_current.m_sub_edge_id;
          if (continuation == ContourCurve::continuation_curve)
            {
              dst_join = &m_glue;
              primitive_type = glue_primitive;
            }
          else
            {
              dst_join = &m_glue_cusp;
              primitive_type = glue_cusp_primitive;
            }

          Packer::add_glue_join(m_current, contour_length, edge_length,
                                *prev_curve, curve, primitive_type,
                                dst_join, &m_info);
        }

      if (continuation == ContourCurve::not_continuation_curve)
        {
          for (; e_si < m_line_segments.size(); ++e_si)
            {
              m_line_segments[e_si].m_edge_length = edge_length;
            }

          for (; e_sa < m_biarc_curves.size(); ++e_sa)
            {
              m_biarc_curves[e_sa].m_edge_length = edge_length;
            }

          for (; e_sg < m_glue.size(); ++e_sg)
            {
              m_glue[e_sg].m_pre_edge_length = edge_length;
              m_glue[e_sg].m_post_edge_length = edge_length;
            }

          for (; e_sgb < m_glue_cusp.size(); ++e_sgb)
            {
              m_glue_cusp[e_sgb].m_pre_edge_length = edge_length;
              m_glue_cusp[e_sgb].m_post_edge_length = edge_length;
            }

          if (dst_inner_glue)
            {
              std::vector<Join> &inner_glue(*dst_inner_glue);

              for (; e_sig < inner_glue.size(); ++e_sig)
                {
                  inner_glue[e_sig].m_pre_edge_length = edge_length;
                  inner_glue[e_sig].m_post_edge_length = edge_length;
                }
            }

          edge_length = 0.0f;
        }

      base = Packer::process_curve(curve, m_current, &m_line_segments, &m_biarc_curves, &m_info, &m_bb);
      base->m_distance_from_contour_start = contour_length;
      base->m_distance_from_edge_start = edge_length;
      base->m_primitive_length = d;
      base->m_flags = (continuation == ContourCurve::not_continuation_curve) ? start_edge_mask : 0u;
      prev_base = base;
      prev_curve = &curve;

      if (i == 0)
        {
          base->m_flags |= start_contour_mask;
        }

      if (i + 1u == curves.size())
        {
          base->m_flags |= end_contour_mask;
        }

      if (is_closed)
        {
          base->m_flags |= contour_closed_mask;
        }

      edge_length += d;
      contour_length += d;
    }

  if (prev_join)
    {
      prev_join->m_post_edge_length = edge_length;
    }

  if (is_closed)
    {
      float D(edge_length);

      if (first_join < m_joins.size())
        {
          D = m_joins[first_join].m_pre_edge_length;
        }

      Packer::add_closing_join(m_current, curves.back(), curves.front(),
                               joins_primitive, &m_joins, &m_info);
      m_joins.back().m_pre_edge_length = edge_length;
      m_joins.back().m_post_edge_length = D;
    }
  else
    {
      // set the length of the front cap
      m_caps.back().m_contour_length = contour_length;
      m_caps.back().m_edge_length = edge_length;

      Packer::add_cap(m_current, curves.back(), false, &m_caps, &m_info);
      m_caps.back().m_contour_length = contour_length;
      m_caps.back().m_edge_length = edge_length;
    }

  if (prev_base)
    {
      prev_base->m_flags |= end_edge_mask;
    }

  for (; e_si < m_line_segments.size(); ++e_si)
    {
      m_line_segments[e_si].m_edge_length = edge_length;
    }

  for (; e_sa < m_biarc_curves.size(); ++e_sa)
    {
      m_biarc_curves[e_sa].m_edge_length = edge_length;
    }

  for (; e_sg < m_glue.size(); ++e_sg)
    {
      m_glue[e_sg].m_pre_edge_length = edge_length;
      m_glue[e_sg].m_post_edge_length = edge_length;
    }

  for (; e_sgb < m_glue_cusp.size(); ++e_sgb)
    {
      m_glue_cusp[e_sgb].m_pre_edge_length = edge_length;
      m_glue_cusp[e_sgb].m_post_edge_length = edge_length;
    }

  for (; c_si < m_line_segments.size(); ++c_si)
    {
      m_line_segments[c_si].m_contour_length = contour_length;
    }

  for (; c_sa < m_biarc_curves.size(); ++c_sa)
    {
      m_biarc_curves[c_sa].m_contour_length = contour_length;
    }

  for (; c_sj < m_joins.size(); ++c_sj)
    {
      m_joins[c_sj].m_contour_length = contour_length;
    }

  for (; c_sg < m_glue.size(); ++c_sg)
    {
      m_glue[c_sg].m_contour_length = contour_length;
    }

  for (; c_sgb < m_glue_cusp.size(); ++c_sgb)
    {
      m_glue_cusp[c_sgb].m_contour_length = contour_length;
    }

  if (dst_inner_glue)
    {
      std::vector<Join> &inner_glue(*dst_inner_glue);

      for (; e_sig < inner_glue.size(); ++e_sig)
        {
          inner_glue[e_sig].m_pre_edge_length = edge_length;
          inner_glue[e_sig].m_post_edge_length = edge_length;
        }
      for (; c_sig < inner_glue.size(); ++c_sig)
        {
          inner_glue[c_sig].m_contour_length = contour_length;
        }
    }

  ++m_current.m_contour_id;
  return *this;
}

////////////////////////////////////////////////////////
// astral::StrokeShader::RawAnimatedData
astral::StrokeShader::RawAnimatedData&
astral::StrokeShader::RawAnimatedData::
add_contour(bool is_closed,
            c_array<const ContourCurve> curves_start,
            c_array<const ContourCurve> curves_end)
{
  ASTRALassert(curves_start.size() == curves_end.size());

  m_start.add_contour_implement(is_closed, curves_start,
                                curves_end, &m_start_inner_glue);

  m_end.add_contour_implement(is_closed, curves_end,
                              curves_start, &m_end_inner_glue);

  return *this;
}

astral::StrokeShader::RawAnimatedData&
astral::StrokeShader::RawAnimatedData::
add_point_cap(vec2 p, vec2 q)
{
  m_start.add_point_cap(p);
  m_end.add_point_cap(q);

  return *this;
}

//////////////////////////////////////////
// astral::StrokeShader::SimpleCookedData methods
void
astral::StrokeShader::SimpleCookedData::
clear(void)
{
  m_static_data.clear();
  for (auto &p : m_vertex_data)
    {
      p.clear();
    }
}

///////////////////////////////////////////
// astral::StrokeShader::CookedData methods
astral::StrokeShader::CookedData::
CookedData(void)
{
}

astral::StrokeShader::CookedData::
CookedData(const CookedData &obj) = default;

astral::StrokeShader::CookedData&
astral::StrokeShader::CookedData::
operator=(const CookedData &rhs) = default;

astral::StrokeShader::CookedData::
~CookedData(void)
{
}

void
astral::StrokeShader::CookedData::
clear(void)
{
  m_base.clear();
  m_hierarchy.clear();
}

//////////////////////////////////
// astral::StrokeShader methods
void
astral::StrokeShader::
create_static_render_data(RenderEngine &engine, const RawData &input,
                          const Ordering *ordering, SimpleCookedData *output)
{
  uint32_t render_data_size(0u), offset(0);

  ASTRALassert(output);

  render_data_size += line_segment_static_data_size * input.line_segments().size();
  render_data_size += quadratic_curve_data_data_size * input.biarc_curves().size();
  render_data_size += join_static_data_size * input.glue().size();
  render_data_size += join_static_data_size * input.glue_cusp().size();
  render_data_size += join_static_data_size * input.joins().size();
  render_data_size += cap_static_data_size * input.caps().size();

  std::vector<gvec4> static_data_backing(render_data_size);
  astral::c_array<gvec4> static_data(make_c_array(static_data_backing));

  output->m_segments_offset = Packer::pack_static_data(input.line_segments(),
                                                       Packer::VirtualOrdering(ordering, line_segments_primitive),
                                                       offset, static_data);

  output->m_biarc_curves_offset = Packer::pack_static_data(input.biarc_curves(),
                                                           Packer::VirtualOrdering(ordering, biarc_curves_primitive),
                                                           offset, static_data);

  output->m_glue_offset = Packer::pack_static_data(input.glue(),
                                                   Packer::VirtualOrdering(ordering, glue_primitive),
                                                   offset, static_data);

  output->m_glue_cusp_offset = Packer::pack_static_data(input.glue_cusp(),
                                                         Packer::VirtualOrdering(ordering, glue_cusp_primitive),
                                                         offset, static_data);

  output->m_joins_offset = Packer::pack_static_data(input.joins(),
                                                    Packer::VirtualOrdering(ordering, joins_primitive),
                                                    offset, static_data);

  output->m_caps_offset = Packer::pack_static_data(input.caps(),
                                                   Packer::VirtualOrdering(ordering, caps_primitive),
                                                   offset, static_data);

  ASTRALassert(offset == render_data_size);
  output->m_static_data = engine.static_data_allocator32().create(static_data);
}

void
astral::StrokeShader::
create_static_render_data(RenderEngine &engine, const RawAnimatedData &input,
                          const Ordering *ordering, SimpleCookedData *output)
{
  uint32_t render_data_size(0u), offset(0);
  const RawData &input0(input.start());
  const RawData &input1(input.end());

  ASTRALassert(output);

  render_data_size += line_segment_pair_static_data_size * input0.line_segments().size();
  render_data_size += quadratic_curve_pair_static_data_size * input0.biarc_curves().size();
  render_data_size += join_pair_static_data_size * input0.glue().size();
  render_data_size += join_pair_static_data_size * input0.glue_cusp().size();
  render_data_size += join_pair_static_data_size * input0.joins().size();
  render_data_size += cap_pair_static_data_size * input0.caps().size();
  render_data_size += join_pair_static_data_size * input.start_inner_glue().size();

  std::vector<gvec4> static_data_backing(render_data_size);
  astral::c_array<gvec4> static_data(make_c_array(static_data_backing));

  output->m_segments_offset = Packer::pack_static_data(input0.line_segments(), input1.line_segments(),
                                                       Packer::VirtualOrdering(ordering, line_segments_primitive),
                                                       offset, static_data);

  output->m_biarc_curves_offset = Packer::pack_static_data(input0.biarc_curves(), input1.biarc_curves(),
                                                           Packer::VirtualOrdering(ordering, biarc_curves_primitive),
                                                           offset, static_data);

  output->m_glue_offset = Packer::pack_static_data(input0.glue(), input1.glue(),
                                                   Packer::VirtualOrdering(ordering, glue_primitive),
                                                   offset, static_data);

  output->m_glue_cusp_offset = Packer::pack_static_data(input0.glue_cusp(), input1.glue_cusp(),
                                                        Packer::VirtualOrdering(ordering, glue_cusp_primitive),
                                                        offset, static_data);

  output->m_joins_offset = Packer::pack_static_data(input0.joins(), input1.joins(),
                                                    Packer::VirtualOrdering(ordering, joins_primitive),
                                                    offset, static_data);

  output->m_caps_offset = Packer::pack_static_data(input0.caps(), input1.caps(),
                                                   Packer::VirtualOrdering(ordering, caps_primitive),
                                                   offset, static_data);

  output->m_inner_glue_offset = Packer::pack_static_data(input.start_inner_glue(), input.end_inner_glue(),
                                                         Packer::VirtualOrdering(ordering, inner_glue_primitive),
                                                         offset, static_data);

  ASTRALassert(offset == render_data_size);
  output->m_static_data = engine.static_data_allocator32().create(static_data);
}

void
astral::StrokeShader::
create_render_data(RenderEngine &engine, const RawData &input, SimpleCookedData *output)
{
  output->clear();
  output->m_for_animated_path = false;
  output->m_inner_glue_offset = 0u;

  /* feed the reordered data into the data builders */
  create_static_render_data(engine, input, nullptr, output);
  create_vertex_render_data(engine, input, input, nullptr, output);

  /* empty VertexData for inner glue joins */
  output->m_vertex_data[inner_glue_primitive] = engine.vertex_data_allocator().create(c_array<const Vertex>());
}

void
astral::StrokeShader::
create_render_data(RenderEngine &engine, const RawData &input, CookedData *output)
{
  Ordering ordering;
  unsigned int h_sz;
  reference_counted_ptr<detail::StrokeDataHierarchy> h;

  output->clear();
  output->m_base.m_for_animated_path = false;
  output->m_base.m_inner_glue_offset = 0u;

  /* first create the hierarchy */
  h = detail::StrokeDataHierarchy::StaticHierarchy::create(input, &h_sz, &ordering);

  /* feed the reordered data into the data builders */
  create_static_render_data(engine, input, &ordering, &output->m_base);
  create_vertex_render_data(engine, input, input, &ordering, &output->m_base);

  /* empty VertexData for inner glue joins */
  output->m_base.m_vertex_data[inner_glue_primitive] = engine.vertex_data_allocator().create(c_array<const Vertex>());

  output->m_hierarchy_size = h_sz;
  output->m_hierarchy = h;
}

void
astral::StrokeShader::
create_render_data(RenderEngine &engine, const RawAnimatedData &input, SimpleCookedData *output)
{
  ASTRALassert(input.start_inner_glue().size() == input.end_inner_glue().size());
  output->clear();
  output->m_for_animated_path = true;

  create_static_render_data(engine, input, nullptr, output);
  create_vertex_render_data(engine, input.start(), input.end(), nullptr, output);

  output->m_vertex_data[inner_glue_primitive]
    = Packer::create_join_vertex_data(output->m_for_animated_path,
                                      output->m_static_data->location() + output->m_inner_glue_offset,
                                      engine, input.start_inner_glue(), input.end_inner_glue(),
                                      Packer::VirtualOrdering(nullptr, inner_glue_primitive));
}

void
astral::StrokeShader::
create_render_data(RenderEngine &engine, const RawAnimatedData &input, CookedData *output)
{
  Ordering ordering;
  unsigned int h_sz;
  reference_counted_ptr<detail::StrokeDataHierarchy> h;

  ASTRALassert(input.start_inner_glue().size() == input.end_inner_glue().size());
  output->clear();
  output->m_base.m_for_animated_path = true;

  /* first create the hierarchy */
  h = detail::StrokeDataHierarchy::AnimatedHierarchy::create(input, &h_sz, &ordering);

  /* feed the reordered data into the data builders */
  create_static_render_data(engine, input, &ordering, &output->m_base);
  create_vertex_render_data(engine, input.start(), input.end(), &ordering, &output->m_base);

  output->m_base.m_vertex_data[inner_glue_primitive]
    = Packer::create_join_vertex_data(output->m_base.m_for_animated_path,
                                      output->m_base.m_static_data->location() + output->m_base.m_inner_glue_offset,
                                      engine, input.start_inner_glue(), input.end_inner_glue(),
                                      Packer::VirtualOrdering(&ordering, inner_glue_primitive));

  output->m_hierarchy_size = h_sz;
  output->m_hierarchy = h;
}

void
astral::StrokeShader::
create_vertex_render_data(RenderEngine &engine, const RawData &input0, const RawData &input1,
                          const Ordering *ordering, SimpleCookedData *output)
{
  /* create vertex/index data for edges */
  output->m_vertex_data[line_segments_primitive]
    = Packer::create_line_vertex_data(output->m_for_animated_path,
                                      output->m_static_data->location() + output->m_segments_offset,
                                      engine, input0.line_segments(), input1.line_segments(),
                                      Packer::VirtualOrdering(ordering, line_segments_primitive));

  output->m_vertex_data[biarc_curves_primitive]
    = Packer::create_biarc_vertex_data(output->m_for_animated_path,
                                       output->m_static_data->location() + output->m_biarc_curves_offset,
                                       engine, input0.biarc_curves(), input1.biarc_curves(),
                                       Packer::VirtualOrdering(ordering, biarc_curves_primitive));

  output->m_vertex_data[segments_cappers_primitive]
     = Packer::create_capper_vertex_data(Packer::line_segment_stride(output->m_for_animated_path),
                                         output->m_static_data->location() + output->m_segments_offset,
                                         engine, input0.line_segments(), input1.line_segments(),
                                         Packer::VirtualOrdering(ordering, segments_cappers_primitive));

  output->m_vertex_data[biarc_curves_cappers_primitive]
     = Packer::create_capper_vertex_data(Packer::quadratic_curve_stride(output->m_for_animated_path),
                                         output->m_static_data->location() + output->m_biarc_curves_offset,
                                         engine, input0.biarc_curves(), input1.biarc_curves(),
                                         Packer::VirtualOrdering(ordering, biarc_curves_cappers_primitive));

  /* create vertex/index data for glue */
  output->m_vertex_data[glue_primitive]
     = Packer::create_join_vertex_data(output->m_for_animated_path,
                                       output->m_static_data->location() + output->m_glue_offset,
                                       engine, input0.glue(), input1.glue(),
                                       Packer::VirtualOrdering(ordering, glue_primitive));

  output->m_vertex_data[glue_cusp_primitive]
     = Packer::create_join_vertex_data(output->m_for_animated_path,
                                       output->m_static_data->location() + output->m_glue_cusp_offset,
                                       engine, input0.glue_cusp(), input1.glue_cusp(),
                                       Packer::VirtualOrdering(ordering, glue_cusp_primitive));

  /* create vertex/index data for joins */
  output->m_vertex_data[joins_primitive]
    = Packer::create_join_vertex_data(output->m_for_animated_path,
                                      output->m_static_data->location() + output->m_joins_offset,
                                      engine, input0.joins(), input1.joins(),
                                      Packer::VirtualOrdering(ordering, joins_primitive));

  /* create vertex/index data for caps */
  output->m_vertex_data[caps_primitive]
     = Packer::create_cap_vertex_data(output->m_for_animated_path,
                                      output->m_static_data->location() + output->m_caps_offset,
                                      engine, input0.caps(), input1.caps(),
                                      Packer::VirtualOrdering(ordering, caps_primitive));

}

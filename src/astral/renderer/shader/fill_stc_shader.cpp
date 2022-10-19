/*!
 * \file fill_stc_shader.cpp
 * \brief file fill_stc_shader.cpp
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

#include <astral/util/ostream_utility.hpp>
#include <astral/renderer/shader/fill_stc_shader.hpp>
#include <astral/renderer/render_engine.hpp>

namespace
{
  inline
  void
  pack_vertex(astral::vec2 p0, astral::vec2 p1, astral::Vertex *R)
  {
    ASTRALassert(R);

    R->m_data[0].f = p0.x();
    R->m_data[1].f = p0.y();
    R->m_data[2].f = p1.x();
    R->m_data[3].f = p1.y();
  }

  void
  pack_expandable_conic_triangle(int static_data_location,
                                 astral::vecN<astral::Vertex, 4> *pdst_verts,
                                 astral::vecN<astral::Index, 6> *pdst_indices)
  {
    static const astral::Index quad[6] = { 0u, 1u, 2u, 0u, 2u, 3u, };
    static const uint32_t types[4] =
      {
        astral::FillSTCShader::ConicTriangle::min_major_min_minor,
        astral::FillSTCShader::ConicTriangle::max_major_min_minor,
        astral::FillSTCShader::ConicTriangle::max_major_max_minor,
        astral::FillSTCShader::ConicTriangle::min_major_max_minor,
      };

    astral::vecN<astral::Vertex, 4> &dst_verts(*pdst_verts);
    astral::vecN<astral::Index, 6> &dst_indices(*pdst_indices);

    for (unsigned int k = 0; k < 6; ++k)
      {
        dst_indices[k] = quad[k];
      }

    for (unsigned int v = 0; v < 4; ++v)
      {
        dst_verts[v].m_data[0].u = static_data_location;
        dst_verts[v].m_data[1].u = 0u;
        dst_verts[v].m_data[2].u = 0u;
        dst_verts[v].m_data[3].u = types[v];
      }
  }

  void
  pack_line_segment(int static_data_location,
                    astral::vecN<astral::Vertex, 4> *pdst_verts,
                    astral::vecN<astral::Index, 6> *pdst_indices)
  {
    static const astral::Index quad[6] = { 0u, 1u, 2u, 0u, 2u, 3u, };
    static const float start_end_picker[4] = { 0.0f, 1.0f, 1.0f, 0.0f };
    static const float normal_vector_multiplier[4] = {1.0f, 1.0f, -1.0f, -1.0f };

    astral::vecN<astral::Vertex, 4> &dst_verts(*pdst_verts);
    astral::vecN<astral::Index, 6> &dst_indices(*pdst_indices);

    for (unsigned int k = 0; k < 6; ++k)
      {
        dst_indices[k] = quad[k];
      }

    for (unsigned int v = 0; v < 4; ++v)
      {
        dst_verts[v].m_data[0].u = static_data_location;
        dst_verts[v].m_data[1].f = start_end_picker[v];
        dst_verts[v].m_data[2].f = normal_vector_multiplier[v];
      }
  }

  class AddContourHelper
  {
  public:
    astral::c_array<const std::pair<astral::ContourCurve, bool>> m_curves;
    bool m_closing_curve_has_aa;
  };

  inline
  unsigned int
  curve_count(const astral::c_array<const astral::ContourCurve> &curves)
  {
    return curves.size();
  }

  inline
  const astral::ContourCurve&
  fetch_curve(unsigned int I, const astral::c_array<const astral::ContourCurve> &curves)
  {
    return curves[I];
  }

  inline
  bool
  curve_has_aa(unsigned int, const astral::c_array<const astral::ContourCurve>&)
  {
    return true;
  }

  inline
  unsigned int
  curve_count(const AddContourHelper &A)
  {
    return A.m_curves.size();
  }

  inline
  const astral::ContourCurve&
  fetch_curve(unsigned int I, const AddContourHelper &A)
  {
    return A.m_curves[I].first;
  }

  inline
  bool
  curve_has_aa(unsigned int I, const AddContourHelper &A)
  {
    return (I < A.m_curves.size()) ?
      A.m_curves[I].second :
      A.m_closing_curve_has_aa;
  }

}

/////////////////////////////////////////////////
// astral::FillSTCShader::ConicTriangle methods
static const float ConicTriangleTextureCoordinates[3][2] =
  {
    {0.0f, 0.0f},
    {0.5f, 0.0f},
    {1.0f, 1.0f},
  };


astral::vec2
astral::FillSTCShader::ConicTriangle::
texture_coordinate(unsigned int I) const
{
  /*
   * Derivation on the value of the texture coordinate:
   * The curve that we are to handle is given parametrically
   * by
   *
   *           [a, w * b, c](t)     (1-t)(1-t) a + 2t(1-t)wb + ttc
   *  f(t) =  ------------------ = -------------------------------
   *              [1, w, 1](t)        (1-t)(1-t) + 2t(1-t)w + tt
   *
   * Let 0 <= t <= 1, let
   *
   *   D = [1, w, 1](t) = (1-t)(1-t) + 2t(1-t)w + tt
   *   A = (1-t)(1-t) / D
   *   B = 2t(1-t)w / D
   *   C = tt / D
   *
   *              (1-t)(1-t) a + 2t(1-t)wb + ttc
   * then f(t) = --------------------------------
   *                 (1-t)(1-t) + 2t(1-t)w + tt
   *
   *           = A * a + B * b + C * c
   *
   * with A, B, C >= 0 and A + B + C = 1. Thus for 0 <= t <= 1,
   * f(t) is a point in the triangle {a, b, c} with the above
   * barycentric coordinates. Let (X, Y, Z) be the texture
   * interpolated across the triangle {a, b, c} where it is
   * given the following values at the vertices {a, b, c}
   *
   *  (X, Y, Z)@a = (0, 0, w)
   *  (X, Y, Z)@b = (1/2, 0, 1)
   *  (X, Y, Z)@c = (w, w, w)
   *
   * Let (A, B, C) be the barycentric coordinates of f(t).
   * We now verify that XX - YZ = 0 for points on the curve f(t):
   *
   *   X = B/2 + Cw
   *     = w(t(1-t) + tt) / D
   *     = wt(1 - t + t) / D
   *     = t(w/D)
   *
   *   Y = Cw
   *     = wtt / D
   *     = tt(w/D)
   *
   *   Z = Aw + B + Cw
   *     = ((1-t)(1-t) + 2t(1-t) + tt)(w/D)
   *     = (1 - 2t + tt + 2t - 2tt + tt)(w/D)
   *     = (w/D)
   *
   * Then,
   *
   * XX - YZ = (ww/DD) * (tt - tt) = 0
   *
   * Thus using the texture coordinates above gives that
   * XX - YZ is 0 along the curve f within the triangle
   * {a, b, c}.
   */
  return vec2(ConicTriangleTextureCoordinates[I][0], ConicTriangleTextureCoordinates[I][1]);
}

///////////////////////////////////////////////////
// astral::FillSTCShader::Data methods
astral::c_array<const astral::FillSTCShader::LineSegment>
astral::FillSTCShader::Data::
aa_line_segments_all(void) const
{
  if (!m_aa_line_segments_all_ready)
    {
      m_aa_line_segments_all_ready = true;
      m_aa_line_segments_all.resize(m_aa_explicit_line_segments.size() + m_aa_implicit_line_segments.size());

      std::copy(m_aa_explicit_line_segments.begin(),
                m_aa_explicit_line_segments.end(),
                m_aa_line_segments_all.begin());

      std::copy(m_aa_implicit_line_segments.begin(),
                m_aa_implicit_line_segments.end(),
                m_aa_line_segments_all.begin() + m_aa_explicit_line_segments.size());
    }

  return make_c_array(m_aa_line_segments_all);
}

void
astral::FillSTCShader::Data::
storage_requirement(PassSet pass_set,
                    vecN<unsigned int, pass_count> *out_number_vertices,
                    unsigned int *out_number_gvec4s_block_size2,
                    unsigned int *out_number_gvec4s_block_size3) const
{
  if (pass_set.has_pass(pass_conic_triangles_stencil))
    {
      /* Each conic triangle in the stencil pass is just one triangle to render */
      (*out_number_vertices)[pass_conic_triangles_stencil] = 3u * m_conic_triangles.size();
    }
  else
    {
      (*out_number_vertices)[pass_conic_triangles_stencil] = 0u;
    }

  if (pass_set.has_pass(pass_conic_triangle_fuzz))
    {
      /* Each conic triangle in aa-pass is a rectangle which is two triangles */
      (*out_number_vertices)[pass_conic_triangle_fuzz] = 6u * m_number_aa_conics;
    }
  else
    {
      (*out_number_vertices)[pass_conic_triangle_fuzz] = 0u;
    }

  if (pass_set.has_pass(pass_contour_fuzz))
    {
      /* Each anti-aliased edge is one rectangle which is two triangles */
      (*out_number_vertices)[pass_contour_fuzz] = 6u * aa_line_segments_all().size();

      /* each anti-aliased line segment requires two gvec4's because
       * the coordinates of each vertex of the line segment at the
       * start and end of animation and for a fixed line segment the
       * data needs to be together.
       */
      *out_number_gvec4s_block_size2 = 2u * aa_line_segments_all().size();
    }
  else
    {
      (*out_number_vertices)[pass_contour_fuzz] = 0u;
      *out_number_gvec4s_block_size2 = 0u;
    }

  (*out_number_vertices)[pass_contour_stencil] = 0u;
  if (pass_set.has_pass(pass_contour_stencil))
    {
      /* Each line-contour in the stencil pass generates a triangle fan */
      for (const auto &line_contour: m_contour_line_ranges)
        {
          unsigned int n;

          n = line_contour.difference();
          if (n >= 3u)
            {
              (*out_number_vertices)[pass_contour_stencil] += 3u * (n - 2u);
            }
        }
    }

  /* each conic triangle requires three gvec4's because the
   * coordinates of each vertex of the conic triangle at the
   * start and end of animation and they for a fixed conic
   * triangle the data needs to be together.
   */
  if (pass_set.has_pass(pass_conic_triangles_stencil) || pass_set.has_pass(pass_conic_triangle_fuzz))
    {
      *out_number_gvec4s_block_size3 = 3u * m_conic_triangles.size();
    }
  else
    {
      *out_number_gvec4s_block_size3 = 0u;
    }
}

astral::FillSTCShader::Data&
astral::FillSTCShader::Data::
add_contour(c_array<const ContourCurve> curves)
{
  add_contour_implement(curves);
  return *this;
}

astral::FillSTCShader::Data&
astral::FillSTCShader::Data::
add_contour(c_array<const std::pair<ContourCurve, bool>> curves,
            bool closing_curve_has_aa)
{
  AddContourHelper A;

  A.m_curves = curves;
  A.m_closing_curve_has_aa = closing_curve_has_aa;
  add_contour_implement(A);
  return *this;
}

astral::FillSTCShader::Data&
astral::FillSTCShader::Data::
add_raw(c_array<const vec2> line_contour,
        c_array<const std::pair<ConicTriangle, bool>> conic_triangles,
        c_array<const LineSegment> aa_line_segments)
{
  range_type<unsigned int> R;
  unsigned int sz;

  R.m_begin = m_contour_pts.size();
  R.m_end = m_contour_pts.size() + line_contour.size();
  m_contour_line_ranges.push_back(R);

  m_contour_pts.resize(R.m_begin + line_contour.size());
  std::copy(line_contour.begin(), line_contour.end(),
            m_contour_pts.begin() + R.m_begin);
  ASTRALassert(line_contour.empty() || line_contour.back() == line_contour.front());

  sz = m_conic_triangles.size();
  m_conic_triangles.resize(sz + conic_triangles.size());
  std::copy(conic_triangles.begin(), conic_triangles.end(),
            m_conic_triangles.begin() + sz);

  sz = m_aa_explicit_line_segments.size();
  m_aa_explicit_line_segments.resize(sz + aa_line_segments.size());
  std::copy(aa_line_segments.begin(), aa_line_segments.end(),
            m_aa_explicit_line_segments.begin() + sz);

  for (const auto &c : conic_triangles)
    {
      if (c.second)
        {
          ++m_number_aa_conics;
        }
    }

  m_aa_line_segments_all_ready = m_aa_line_segments_all_ready
    && aa_line_segments.empty();

  return *this;
}

template<typename T>
void
astral::FillSTCShader::Data::
add_contour_implement(const T &A)
{
  unsigned int contour_pt_size(m_contour_pts.size());
  unsigned int num_curves(curve_count(A));

  for (unsigned int I = 0; I < num_curves; ++I)
    {
      const ContourCurve &C(fetch_curve(I, A));
      bool has_aa(curve_has_aa(I, A));

      m_contour_pts.push_back(C.start_pt());
      switch (C.type())
        {
        case ContourCurve::line_segment:
          {
            if (has_aa)
              {
                LineSegment L;

                L.m_pts[0] = C.start_pt();
                L.m_pts[1] = C.end_pt();
                m_aa_explicit_line_segments.push_back(L);
                m_aa_line_segments_all_ready = false;
              }
          }
          break;

        case ContourCurve::quadratic_bezier:
          {
            std::pair<FillSTCShader::ConicTriangle, bool> tmp;

            /* The last point loaded is the 1st point of the triangle;
             * the orientation is also critical: it is required that
             * the triangle as the edge FROM the last point TO the
             * first point. This is done by simply adding the points
             * of the triangle in order from the source edge since the
             * control point is in-between.
             */
            tmp.first.m_pts[0] = C.start_pt();
            tmp.first.m_pts[1] = C.control_pt(0);
            tmp.first.m_pts[2] = C.end_pt();
            tmp.second = has_aa;
            if (has_aa)
              {
                ++m_number_aa_conics;
              }

            m_conic_triangles.push_back(tmp);
          }
          break;

        default:
          ASTRALassert(!"Given a contour that has a curve with more than one control point");
        }
    }

  if (num_curves > 0u)
    {
      range_type<unsigned int> R;

      /* add the start point to close the loop; we ALWAYS add
       * this line segment even if the start and end point
       * match to make sure that when handling animated paths
       * that the geometries verbs are exactly the same.
       */
      if (curve_has_aa(num_curves, A))
        {
          LineSegment L;

          L.m_pts[0] = fetch_curve(num_curves - 1u, A).end_pt();
          L.m_pts[1] = fetch_curve(0u, A).start_pt();
          m_aa_implicit_line_segments.push_back(L);
          m_aa_line_segments_all_ready = false;
        }

      m_contour_pts.push_back(fetch_curve(num_curves - 1u, A).end_pt());
      R.m_begin = contour_pt_size;
      R.m_end = m_contour_pts.size();
      m_contour_line_ranges.push_back(R);
    }
}

//////////////////////////////////////////////
// astral::FillSTCShader::AnimatedData methods
astral::FillSTCShader::AnimatedData&
astral::FillSTCShader::AnimatedData::
add_contour(c_array<const ContourCurve> start_curves,
            c_array<const ContourCurve> end_curves)
{
  ASTRALassert(start_curves.size() == end_curves.size());
  #ifdef ASTRAL_DEBUG
    {
      for (unsigned int i = 0; i < start_curves.size(); ++i)
        {
          ASTRALassert(start_curves[i].type() == end_curves[i].type());
        }
    }
  #endif

  m_start_data.add_contour(start_curves);
  m_end_data.add_contour(end_curves);

  return *this;
}

///////////////////////////////////////////
// astral::FillSTCShader methods
astral::FillSTCShader::CookedData
astral::FillSTCShader::
create_cooked_data(RenderEngine &engine, const Data &start_data, const Data &end_data)
{
  CookedData return_value;
  vecN<unsigned int, pass_count> number_vertices;
  unsigned int number_gvec4s_block_size2, number_gvec4s_block_size3;
  vecN<VertexStreamerBlock, 4> vert_block;
  StaticDataStreamerBlock32 data_block2, data_block3;
  std::vector<Vertex> vertex_data_backing;
  std::vector<gvec4> data_block2_backing, data_block3_backing;
  vecN<c_array<const VertexStreamerBlock>, pass_count> vert_block_ptr;
  unsigned int sz(0u);

  ASTRALassert(start_data.m_contour_line_ranges.size() == end_data.m_contour_line_ranges.size());
  ASTRALassert(start_data.m_conic_triangles.size() == end_data.m_conic_triangles.size());
  ASTRALassert(start_data.m_aa_explicit_line_segments.size() == end_data.m_aa_explicit_line_segments.size());
  ASTRALassert(start_data.m_aa_implicit_line_segments.size() == end_data.m_aa_implicit_line_segments.size());

  start_data.storage_requirement(with_anti_aliasing,
                                 &number_vertices,
                                 &number_gvec4s_block_size2,
                                 &number_gvec4s_block_size3);

  for (unsigned int p = 0; p < pass_count; ++p)
    {
      sz += number_vertices[p];
    }
  vertex_data_backing.resize(sz);
  for (unsigned int p = 0, r = 0; p < pass_count; ++p)
    {
      return_value.m_pass_range[p].m_begin = r;
      return_value.m_pass_range[p].m_end = r + number_vertices[p];
      vert_block[p].m_dst = make_c_array(vertex_data_backing).sub_array(r, number_vertices[p]);
      vert_block_ptr[p] = c_array<const VertexStreamerBlock>(&vert_block[p], 1);
      r += number_vertices[p];
    }

  /* the implicit edges are added at the end, so to skip them drop that number of rects */
  return_value.m_aa_line_pass_without_implicit_closing_edge = return_value.m_pass_range[pass_contour_fuzz];
  return_value.m_aa_line_pass_without_implicit_closing_edge.m_end -= start_data.m_aa_implicit_line_segments.size() * 6u;

  data_block2_backing.resize(number_gvec4s_block_size2);
  data_block2.m_dst = make_c_array(data_block2_backing);

  data_block3_backing.resize(number_gvec4s_block_size3);
  data_block3.m_dst = make_c_array(data_block3_backing);

  /* pack the data */
  pack_render_data(start_data, end_data, with_anti_aliasing, vert_block_ptr,
                   c_array<const StaticDataStreamerBlock32>(&data_block2, 1),
                   c_array<const StaticDataStreamerBlock32>(&data_block3, 1));

  /* create the StaticData objects */
  return_value.m_block_size2 = engine.static_data_allocator32().create(data_block2.m_dst);
  return_value.m_block_size3 = engine.static_data_allocator32().create(data_block3.m_dst);

  /* adjust the vertex values for the actual location of the static data */
  for (Vertex &vert : vert_block[pass_contour_fuzz].m_dst)
    {
      vert.m_data[0].u += return_value.m_block_size2->location();
    }
  for (Vertex &vert : vert_block[pass_conic_triangles_stencil].m_dst)
    {
      vert.m_data[0].u += return_value.m_block_size3->location();
    }
  for (Vertex &vert : vert_block[pass_conic_triangle_fuzz].m_dst)
    {
      vert.m_data[0].u += return_value.m_block_size3->location();
    }

  /* create the vertex data */
  return_value.m_vertex_data = engine.vertex_data_allocator().create(make_c_array(vertex_data_backing));

  return return_value;
}

void
astral::FillSTCShader::
pack_conic_render_data(const Data &render0, const Data &render1, PassSet pass_set,
                       c_array<const VertexStreamerBlock> dst_stencil,
                       c_array<const VertexStreamerBlock> dst_fuzz,
                       c_array<const StaticDataStreamerBlock32> dst_static)
{
  /* walk the conic triangle list to simutaneously build the data into
   * dst_static_data_block3, dst_vertices[pass_conic_triangles_stencil]
   * and dst_vertices[pass_conic_triangle_fuzz].
   */
  unsigned int conic_fuzz_block(0u), conic_fuzz_vert(0u);
  unsigned int conic_stencil_block(0u), conic_stencil_vert(0u);
  unsigned int conic_static_block(0u), conic_static_loc(0u);

  ASTRALassert(render0.m_conic_triangles.size() == render1.m_conic_triangles.size());
  for (unsigned int t = 0; t < render0.m_conic_triangles.size(); ++t)
    {
      unsigned int static_data_offset;

      while (conic_static_loc >= dst_static[conic_static_block].m_dst.size())
        {
          conic_static_loc = 0u;
          ++conic_static_block;
        }

      ASTRALassert(conic_static_loc + 3u <= dst_static[conic_static_block].m_dst.size());

      /* compute the static_data_offset for the current triangle. Note
       * that we allow for the StaticData objet to be nullptr; In that
       * case it is expected that the caller will adjust the location
       * aftwerwards itself.
       */
      static_data_offset = conic_static_loc + dst_static[conic_static_block].m_offset;
      if (dst_static[conic_static_block].m_object)
        {
          static_data_offset += dst_static[conic_static_block].m_object->location();
        }

      if (pass_set.has_pass(pass_conic_triangles_stencil))
        {
          while (conic_stencil_vert >= dst_stencil[conic_stencil_block].m_dst.size())
            {
              conic_stencil_vert = 0;
              ++conic_stencil_block;
            }

          ASTRALassert(conic_stencil_vert + 3u <= dst_stencil[conic_stencil_block].m_dst.size());

          /* pack the stencil conic triangle vertex data */
          for (unsigned int k = 0; k < 3u; ++k, ++conic_stencil_vert)
            {
              dst_stencil[conic_stencil_block].m_dst[conic_stencil_vert].m_data[0].u = static_data_offset + k;
              dst_stencil[conic_stencil_block].m_dst[conic_stencil_vert].m_data[1].f = ConicTriangleTextureCoordinates[k][0];
              dst_stencil[conic_stencil_block].m_dst[conic_stencil_vert].m_data[2].f = ConicTriangleTextureCoordinates[k][1];
              dst_stencil[conic_stencil_block].m_dst[conic_stencil_vert].m_data[3].u = static_data_offset;
            }
        }

      /* the fuzz for a conic triangle is a rectangle realized as two triangles */
      ASTRALassert(render0.m_conic_triangles[t].second == render1.m_conic_triangles[t].second);
      if (pass_set.has_pass(pass_conic_triangle_fuzz) && render0.m_conic_triangles[t].second)
        {
          vecN<Vertex, 4> tmp_verts;
          vecN<Index, 6> tmp_indices;
          pack_expandable_conic_triangle(static_data_offset,
                                         &tmp_verts, &tmp_indices);
          for (unsigned int sub_t = 0; sub_t < 2u; ++sub_t)
            {
              while (conic_fuzz_vert >= dst_fuzz[conic_fuzz_block].m_dst.size())
                {
                  conic_fuzz_vert = 0;
                  ++conic_fuzz_block;
                }

              ASTRALassert(conic_fuzz_vert + 3u <= dst_fuzz[conic_fuzz_block].m_dst.size());
              for (unsigned i = 0; i < 3; ++i, ++conic_fuzz_vert)
                {
                  dst_fuzz[conic_fuzz_block].m_dst[conic_fuzz_vert] = tmp_verts[tmp_indices[i + 3u * sub_t]];
                }
            }
        }

      /* pack the geometry of the conic triangle into the static data */
      for (unsigned int k = 0; k < 3u; ++k, ++conic_static_loc)
        {
          dst_static[conic_static_block].m_dst[conic_static_loc].x().f = render0.m_conic_triangles[t].first.m_pts[k].x();
          dst_static[conic_static_block].m_dst[conic_static_loc].y().f = render0.m_conic_triangles[t].first.m_pts[k].y();
          dst_static[conic_static_block].m_dst[conic_static_loc].z().f = render1.m_conic_triangles[t].first.m_pts[k].x();
          dst_static[conic_static_block].m_dst[conic_static_loc].w().f = render1.m_conic_triangles[t].first.m_pts[k].y();
        }
    }

  ASTRALassert(render0.m_conic_triangles.empty() || !pass_set.has_pass(pass_conic_triangles_stencil) || conic_stencil_vert == dst_stencil[conic_stencil_block].m_dst.size());
  ASTRALassert(render0.m_conic_triangles.empty() || !pass_set.has_pass(pass_conic_triangles_stencil) || conic_stencil_block + 1u == dst_stencil.size());
  ASTRALassert(render0.m_conic_triangles.empty() || !pass_set.has_pass(pass_conic_triangle_fuzz) || conic_fuzz_vert == dst_fuzz[conic_fuzz_block].m_dst.size());
  ASTRALassert(render0.m_conic_triangles.empty() || !pass_set.has_pass(pass_conic_triangle_fuzz) || conic_fuzz_block + 1u == dst_fuzz.size());
  ASTRALassert(render0.m_conic_triangles.empty() || conic_static_loc == dst_static[conic_static_block].m_dst.size());
  ASTRALassert(render0.m_conic_triangles.empty() || conic_static_block + 1u == dst_static.size());
}

void
astral::FillSTCShader::
pack_line_stencil_render_data(const Data &render0, const Data &render1,
                              c_array<const VertexStreamerBlock> dst_stencil)
{
  /* walk the line-contour data list to build the data into
   * dst_vertices[pass_contour_stencil]
   */
  unsigned int line_stencil_block(0u), line_stencil_vert(0u);
  bool all_contours_empty(true);
  c_array<const vec2> render_pts0, render_pts1;

  render_pts0 = make_c_array(render0.m_contour_pts);
  render_pts1 = make_c_array(render1.m_contour_pts);
  ASTRALassert(render0.m_contour_line_ranges.size() == render1.m_contour_line_ranges.size());
  for (unsigned int line_contour = 0, end = render0.m_contour_line_ranges.size(); line_contour < end; ++line_contour)
    {
      c_array<const vec2> pts0, pts1;

      pts0 = render_pts0.sub_array(render0.m_contour_line_ranges[line_contour]);
      pts1 = render_pts1.sub_array(render1.m_contour_line_ranges[line_contour]);
      ASTRALassert(pts0.size() == pts1.size());

      if (pts0.size() >= 3u)
        {
          Vertex center, prev;

          all_contours_empty = false;
          pack_vertex(pts0[0], pts1[0], &center);
          pack_vertex(pts0[1], pts1[1], &prev);
          for (unsigned int t = 2; t < pts0.size(); ++t, line_stencil_vert += 3u)
            {
              while (line_stencil_vert >= dst_stencil[line_stencil_block].m_dst.size())
                {
                  line_stencil_vert = 0;
                  ++line_stencil_block;
                }

              ASTRALassert(line_stencil_vert + 3u <= dst_stencil[line_stencil_block].m_dst.size());

              dst_stencil[line_stencil_block].m_dst[line_stencil_vert + 0u] = center;
              dst_stencil[line_stencil_block].m_dst[line_stencil_vert + 1u] = prev;
              pack_vertex(pts0[t], pts1[t], &prev);
              dst_stencil[line_stencil_block].m_dst[line_stencil_vert + 2u] = prev;
            }
        }
    }

  ASTRALunused(all_contours_empty);
  ASTRALassert(all_contours_empty || line_stencil_vert == dst_stencil[line_stencil_block].m_dst.size());
  ASTRALassert(all_contours_empty || line_stencil_block + 1u == dst_stencil.size());
}

void
astral::FillSTCShader::
pack_line_fuzz_render_data(const Data &render0, const Data &render1,
                           c_array<const VertexStreamerBlock> dst_fuzz,
                           c_array<const StaticDataStreamerBlock32> dst_static)
{
  /* walk the anti-alias line segment list to simutaneously build the
   * data into dst_static_data_block2 and dst_vertices[pass_contour_fuzz]
   */
  unsigned int line_fuzz_block(0u), line_fuzz_vert(0u);
  unsigned int line_fuzz_static_block(0u), line_fuzz_static_loc(0u);
  c_array<const LineSegment> render0_aa_line_segments, render1_aa_line_segments;

  render0_aa_line_segments = render0.aa_line_segments_all();
  render1_aa_line_segments = render1.aa_line_segments_all();

  ASTRALassert(render0_aa_line_segments.size() == render1_aa_line_segments.size());
  for (unsigned int line = 0, end = render0_aa_line_segments.size(); line < end; ++line)
    {
      unsigned int static_data_offset;

      while (line_fuzz_static_loc >= dst_static[line_fuzz_static_block].m_dst.size())
        {
          line_fuzz_static_loc = 0u;
          ++line_fuzz_static_block;
        }

      static_data_offset = line_fuzz_static_loc + dst_static[line_fuzz_static_block].m_offset;
      if (dst_static[line_fuzz_static_block].m_object)
        {
          static_data_offset += dst_static[line_fuzz_static_block].m_object->location();
        }

      ASTRALassert(line_fuzz_static_loc + 2u <= dst_static[line_fuzz_static_block].m_dst.size());
      for (unsigned int k = 0; k < 2u; ++k, ++line_fuzz_static_loc)
        {
          dst_static[line_fuzz_static_block].m_dst[line_fuzz_static_loc].x().f = render0_aa_line_segments[line].m_pts[k].x();
          dst_static[line_fuzz_static_block].m_dst[line_fuzz_static_loc].y().f = render0_aa_line_segments[line].m_pts[k].y();
          dst_static[line_fuzz_static_block].m_dst[line_fuzz_static_loc].z().f = render1_aa_line_segments[line].m_pts[k].x();
          dst_static[line_fuzz_static_block].m_dst[line_fuzz_static_loc].w().f = render1_aa_line_segments[line].m_pts[k].y();
        }

      /* each line segment induces one rectangle that is realized as two triangles */
      vecN<Vertex, 4> tmp_verts;
      vecN<Index, 6> tmp_indices;
      pack_line_segment(static_data_offset, &tmp_verts, &tmp_indices);
      for (unsigned int sub_t = 0; sub_t < 2u; ++sub_t)
        {
          while (line_fuzz_vert >= dst_fuzz[line_fuzz_block].m_dst.size())
            {
              line_fuzz_vert = 0;
              ++line_fuzz_block;
            }

          ASTRALassert(line_fuzz_vert + 3u <= dst_fuzz[line_fuzz_block].m_dst.size());
          for (unsigned i = 0; i < 3; ++i, ++line_fuzz_vert)
            {
              dst_fuzz[line_fuzz_block].m_dst[line_fuzz_vert] = tmp_verts[tmp_indices[i + 3u * sub_t]];
            }
        }
    }

  ASTRALassert(render0_aa_line_segments.empty() || line_fuzz_vert == dst_fuzz[line_fuzz_block].m_dst.size());
  ASTRALassert(render0_aa_line_segments.empty() || line_fuzz_block + 1u == dst_fuzz.size());
  ASTRALassert(render0_aa_line_segments.empty() || line_fuzz_static_loc == dst_static[line_fuzz_static_block].m_dst.size());
  ASTRALassert(render0_aa_line_segments.empty() || line_fuzz_static_block + 1u == dst_static.size());
}

void
astral::FillSTCShader::
pack_render_data(const Data &render0, const Data &render1, PassSet pass_set,
                 vecN<c_array<const VertexStreamerBlock>, pass_count> dst_vertices,
                 c_array<const StaticDataStreamerBlock32> dst_static_data_block2,
                 c_array<const StaticDataStreamerBlock32> dst_static_data_block3)
{
  if (pass_set.has_pass(pass_conic_triangles_stencil) || pass_set.has_pass(pass_conic_triangle_fuzz))
    {
      pack_conic_render_data(render0, render1, pass_set,
                             dst_vertices[pass_conic_triangles_stencil],
                             dst_vertices[pass_conic_triangle_fuzz],
                             dst_static_data_block3);
    }

  if (pass_set.has_pass(pass_contour_stencil))
    {
      pack_line_stencil_render_data(render0, render1, dst_vertices[pass_contour_stencil]);
    }

  if (pass_set.has_pass(pass_contour_fuzz))
    {
      pack_line_fuzz_render_data(render0, render1, dst_vertices[pass_contour_fuzz],
                                 dst_static_data_block2);
    }
}

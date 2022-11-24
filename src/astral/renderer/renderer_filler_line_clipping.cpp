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
#include "renderer_filler_line_clipping.hpp"

// change to 1 to have release build do asserts
#if 0
  #ifndef ASTRAL_DEBUG
    #undef ASTRALassert
    #include <assert.h>
    #define ASTRALassert(X) assert(X)
  #endif
#endif

// change to 1 to have stdout spam contour mapping data
#if 0
  #define MAP_LOG(X) do { std::cout << X; } while(0)
#else
  #define MAP_LOG(X)
#endif

class astral::Renderer::Implement::Filler::LineClipper::Helper
{
public:
  template<typename T>
  static
  void
  map_contours(Filler::LineClipper &filler, const CombinedPath &path);

private:
  static
  c_array<const ContourCurve>
  unmapped_curves(Filler::LineClipper &filler, const CachedCombinedPath::PerObject &tr_tol,
                  const Contour &contour, float t);

  static
  c_array<const ContourCurve>
  unmapped_curves(Filler::LineClipper &filler, const CachedCombinedPath::PerObject &tr_tol,
                  const AnimatedContour &contour, float t);
};

////////////////////////////////////////////
// astral::Renderer::Implement::Filler::LineClipper::Helper methods
astral::c_array<const astral::ContourCurve>
astral::Renderer::Implement::Filler::LineClipper::Helper::
unmapped_curves(Filler::LineClipper &filler, const CachedCombinedPath::PerObject &tr_tol,
                const Contour &contour, float t)
{
  ASTRALunused(filler);
  ASTRALunused(t);
  ASTRALassert(0.0f <= t && t <= 1.0f);

  /* We want shorter curves because the line-clipper only
   * performs clipping on the line segments.
   *
   * It might be tempting to use contour_fill_approximation_allow_long_curves
   * if the calling Renderer is using HW clip-planes. However, that is
   * not the case because by having smaller curves, this increases the
   * number of sub-rects not touched by any curve.
   */
  return contour.fill_approximated_geometry(tr_tol.m_tol, contour_fill_approximation_tessellate_long_curves);
}

astral::c_array<const astral::ContourCurve>
astral::Renderer::Implement::Filler::LineClipper::Helper::
unmapped_curves(Filler::LineClipper &filler, const CachedCombinedPath::PerObject &tr_tol,
                const AnimatedContour &contour, float t)
{
  const auto &curves(contour.fill_approximated_geometry(tr_tol.m_tol, contour_fill_approximation_tessellate_long_curves));

  ASTRALassert(0.0f <= t && t <= 1.0f);
  ASTRALassert(curves.m_start.size() == curves.m_end.size());
  filler.m_workroom_curves.resize(curves.m_start.size());
  for (unsigned int j = 0, endj = curves.m_start.size(); j < endj; ++j)
    {
      filler.m_workroom_curves[j] = ContourCurve(curves.m_start[j], curves.m_end[j], t);
    }

  return make_c_array(filler.m_workroom_curves);
}

template<typename T>
void
astral::Renderer::Implement::Filler::LineClipper::Helper::
map_contours(Filler::LineClipper &filler, const CombinedPath &combined_path)
{
  auto paths(combined_path.paths<T>());

  for (unsigned int I = 0; I < paths.size(); ++I)
    {
      const T *path(paths[I]);
      const auto &tr_tol(filler.m_cached_combined_path.get_value<T>(I));
      float t(combined_path.get_t<T>(I));
      unsigned int cnt(path->number_contours());

      ++filler.m_total_num_paths;
      filler.m_total_num_contours += cnt;

      if (tr_tol.m_culled)
        {
          ++filler.m_num_culled_paths;
          filler.m_num_culled_contours += cnt;
          continue;
        }

      /* The end condition filler.m_number_lit <= filler.m_thresh_lit is an early
       * out to skip mapping further contours once too many rects are lit.
       */
      for (unsigned int C = 0; C < cnt && filler.m_number_lit <= filler.m_thresh_lit; ++C)
        {
          const auto &contour(path->contour(C));
          c_array<const ContourCurve> curves;
          BoundingBox<float> mapped_bb(tr_tol.m_buffer_transformation_path.apply_to_bb(contour.bounding_box(t)));

          /* check if the contour can be culled */
          if (!filler.m_region.intersects(mapped_bb))
            {
              ++filler.m_num_culled_contours;
              continue;
            }

          curves = unmapped_curves(filler, tr_tol, contour, t);
          filler.m_renderer.m_stats[number_sparse_fill_curves_mapped] += curves.size();

          if (!curves.empty())
            {
              MappedContour M(filler, curves, contour.closed(),
                              tr_tol.m_buffer_transformation_path);

              if (M.m_subrect_range.x().m_begin != M.m_subrect_range.x().m_end
                  && M.m_subrect_range.y().m_begin != M.m_subrect_range.y().m_end)
                {
                  bool all_skipped = true;

                  /* We can still skip a contour if its
                   * range of sub-rects are all to be skipped.
                   */
                  for (int y = M.m_subrect_range.y().m_begin; y != M.m_subrect_range.y().m_end && all_skipped; ++y)
                    {
                      for (int x = M.m_subrect_range.x().m_begin; x != M.m_subrect_range.x().m_end && all_skipped; ++x)
                        {
                          all_skipped = all_skipped && filler.subrect(x, y).skip_rect();
                        }
                    }

                  if (!all_skipped)
                    {
                      ++filler.m_renderer.m_stats[number_sparse_fill_contours_mapped];
                      filler.m_mapped_contours.push_back(M);
                      filler.m_number_lit += filler.m_mapped_contours.back().light_rects(filler);
                    }
                  else
                    {
                      ++filler.m_renderer.m_stats[number_sparse_fill_late_culled_contours];
                      ++filler.m_num_late_culled_contours;
                    }
                }
              else
                {
                  ++filler.m_renderer.m_stats[number_sparse_fill_late_culled_contours];
                  ++filler.m_num_late_culled_contours;
                }
            }
        }
    }
}

/////////////////////////////////////////////////////////
// astral::Renderer::Implement::Filler::LineClipper::Intersection methods
astral::Renderer::Implement::Filler::LineClipper::Intersection::
Intersection(enum line_t tp, float ref_p, const ContourCurve &curve)
{
  int coord;
  float p1, p2, p3, A, B, C;
  float w, t1, t2;
  bool use_t1, use_t2;

  ASTRALassert(curve.number_control_pts() <= 1u);
  coord = fixed_coordinate(tp);

  /* translate the curve so that we are solving for
   * either curve.eval(t)[coord] == 0
   */
  p1 = curve.start_pt()[coord] - ref_p;
  p3 = curve.end_pt()[coord] - ref_p;
  if (curve.type() == ContourCurve::line_segment)
    {
      p2 = 0.5f * (p1 + p3);
    }
  else
    {
      p2 = curve.control_pt(0)[coord] - ref_p;
    }

  /* See astral_banded_rays.glsl.resource_string */
  use_t1 = (p3 <= 0.0f && t_max(p1, p2) > 0.0f) || (p1 > 0.0f && p2 < 0.0f);
  use_t2 = (p1 <= 0.0f && t_max(p2, p3) > 0.0f) || (p3 > 0.0f && p2 < 0.0f);

  w = (curve.type() != ContourCurve::line_segment) ? curve.conic_weight() : 1.0f;
  A = p1 - (2.0f * w) * p2 + p3;
  B = p1 - w * p2;
  C = p1;

  /* Question: should we do relatively zero, or just zero? */
  if (curve.type() != ContourCurve::line_segment && t_abs(A) > 0.0f)
    {
      float D;

      D = B * B - A * C;
      if (D < 0.0f)
        {
          t1 = t2 = 0.0f;
          use_t1 = use_t2 = false;
        }
      else
        {
          float recipA, rootD;

          recipA = 1.0f / A;
          rootD = t_sqrt(D);

          /* The roots are given by
           *
           *   t1 = (B - sqrt(D)) / A
           *   t2 = (B + sqrt(D)) / A
           *
           * We can avoid some catostrophic cancellation. If
           * B < 0, we take t2 and multiply the numerator
           * and denomiator by (B - sqrt(D)) which simplifies to
           * the numerator becoming A * C.
           *
           * Likewise if B > 0, we can take t1 and multiple the
           * numerator and denomiator by (B + sqrt(D)) which
           * simplifies to the numerator becoming A * C.
           */
          if (B < 0.0f)
            {
              t1 = (B - rootD) * recipA;
              t2 = C / (B - rootD);
            }
          else
            {
              t1 = C / (B + rootD);
              t2 = (B + rootD) * recipA;
            }
        }
    }
  else
    {
      t1 = t2 = 0.5f * C / B;
    }

  /* The winding effect MUST follow the convention that
   * is used in Renderer: Clockwise increments the winding
   * number and CounterClockwise decrements where the
   * y-axis increases downwardly.
   */
  if (use_t1 && use_t2)
    {
      m_count = 2;
      m_values[0].m_t = t1;
      m_values[1].m_t = t2;

      m_values[0].m_winding_effect = -1;
      m_values[1].m_winding_effect = 1;
    }
  else if (use_t1 || use_t2)
    {
      m_count = 1;
      m_values[0].m_t = (use_t1) ? t1 : t2;
      m_values[0].m_winding_effect = (use_t1) ? -1 : 1;
    }
  else
    {
      m_count = 0;
    }

  for (unsigned int i = 0; i < m_count; ++i)
    {
      m_values[i].m_position = curve.eval_at(m_values[i].m_t);
      m_values[i].m_position[coord] = ref_p;
    }
}

unsigned int
astral::Renderer::Implement::Filler::LineClipper::Intersection::
light_rects(Filler::LineClipper &filler, enum line_t l, int v, enum SubRect::lit_by_t tp) const
{
  ivec2 rect_coord;
  int fixed, varying;
  unsigned int return_value(0u);

  fixed = fixed_coordinate(l);
  varying = 1 - fixed;

  rect_coord[fixed] = v;
  for (unsigned int i = 0; i < m_count; ++i)
    {
      float wf;
      range_type<int> R;

      wf = m_values[i].m_position[varying];
      R = filler.subrect_from_coordinate(wf, varying);

      for(; R.m_begin < R.m_end; ++R.m_begin)
        {
          rect_coord[varying] = R.m_begin;

          return_value += filler.subrect(rect_coord).light_rect(tp);
          if (tp == SubRect::lit_by_path)
            {
              MAP_LOG("\t\t\tIntersectionLight" << rect_coord << "@" << m_values[i].m_position << "\n");
            }
        }
    }

  return return_value;
}

void
astral::Renderer::Implement::Filler::LineClipper::Intersection::
add_to_lit_by_curves(Filler::LineClipper &filler, enum line_t l, int v) const
{
  ivec2 rect_coord;
  int fixed, varying;

  fixed = fixed_coordinate(l);
  varying = 1 - fixed;

  rect_coord[fixed] = v;
  for (unsigned int i = 0; i < m_count; ++i)
    {
      float wf;
      range_type<int> R;

      wf = m_values[i].m_position[varying];
      R = filler.subrect_from_coordinate(wf, varying);

      for(; R.m_begin < R.m_end; ++R.m_begin)
        {
          rect_coord[varying] = R.m_begin;

          ASTRALassert(filler.subrect(rect_coord).m_lit[SubRect::lit_by_current_contour]);
          filler.m_lit_by_curves.insert(filler.subrect_id(rect_coord));
        }
    }
}

/////////////////////////////////////////////////////////
// astral::Renderer::Implement::Filler::LineClipper::MappedCurve methods
astral::Renderer::Implement::Filler::LineClipper::MappedCurve::
MappedCurve(Filler::LineClipper &filler,
            const ContourCurve &curve,
            const Transformation &tr,
            const ContourCurve *prev):
  m_mapped_curve(curve, tr)
{
  if (prev)
    {
      m_mapped_curve.start_pt(tr.apply_to_point(prev->end_pt()));
    }

  MAP_LOG("\tInput = " << curve << ", mapped = " << m_mapped_curve << "\n");
  m_bb = m_mapped_curve.tight_bounding_box();

  /* Step 1. compute the x-range and y-range of the curve; we use the
   *         tight bounding box of m_mapped_curve to do so.
   *
   * NOTE: the actual set of rects that intersect the curve might be a
   *       STRICT subset of m_subrect_range; this is because that range
   *       is like a bounding box and will include rects that a diagnaol
   *       line segment actually misses.
   */
  m_subrect_range = filler.subrect_range_from_coordinate(m_bb.min_point(), m_bb.max_point());
  MAP_LOG("\tRange = " << m_subrect_range);
  if (m_subrect_range.x().m_begin < m_subrect_range.x().m_end)
    {
      MAP_LOG(", minx = " << filler.minx_side_value(m_subrect_range.x().m_begin));
      MAP_LOG(", maxx = " << filler.maxx_side_value(m_subrect_range.x().m_end - 1));
    }
  if (m_subrect_range.y().m_begin < m_subrect_range.y().m_end)
    {
      MAP_LOG(", miny = " << filler.miny_side_value(m_subrect_range.y().m_begin));
      MAP_LOG(", maxy = " << filler.maxy_side_value(m_subrect_range.y().m_end - 1));
    }
  MAP_LOG("\n");

  /* Step 2. compute the intersections, recall that
   *         Filler::LineClipper::m_intersection_backing is the
   *         backing store of the intersections
   *
   * TODO: if padding is zero, avoid double comptuation on shared
   *       boundaries between neighboring rects.
   */
  for (int s = 0; s < 4; ++s)
    {
      enum side_t ss;
      enum line_t ll;
      int l;

      ss = static_cast<enum side_t>(s);
      ll = line_t_from_side_t(ss);
      l = fixed_coordinate(ll);
      m_intersections[ss] = filler.m_intersection_backing.size();

      for (int v = m_subrect_range[l].m_begin; v < m_subrect_range[l].m_end; ++v)
        {
          MAP_LOG("\t\tSide = " << label(ss) << ", fixed coordinate = " << l);
          MAP_LOG("@" << filler.side_value(v, ss));
          MAP_LOG(":\n");

          filler.m_intersection_backing.push_back(Intersection(ll, filler.side_value(v, ss), m_mapped_curve));
        }
    }
}

unsigned int
astral::Renderer::Implement::Filler::LineClipper::MappedCurve::
light_rects(Filler::LineClipper &filler, enum SubRect::lit_by_t tp) const
{
  unsigned int return_value(0u);
  vecN<range_type<int>, 2> r;

  MAP_LOG("\tMapped = " << m_mapped_curve << "\n");

  /* Detecting if a curve intersects a sub-rect (X, Y) is more
   * subtle than checking if MappedCurve::m_subrect_range contains
   * (X, Y). Doing such a simple check would view a diaganol curve
   * hitting way more rects than it actually does. Instead, we look
   * at the intersections of each curve against the boundaries of
   * the sub-rects.
   */

  /* Step 1: tag the rects that the start and end point touch */
  r = filler.subrect_from_coordinate(m_mapped_curve.start_pt());
  for (int y = r.y().m_begin; y < r.y().m_end; ++y)
    {
      for (int x = r.x().m_begin; x < r.x().m_end; ++x)
        {
          MAP_LOG("\t\tLight" << ivec2(x, y) << "\n");
          return_value += filler.subrect(x, y).light_rect(tp);
        }
    }

  r = filler.subrect_from_coordinate(m_mapped_curve.end_pt());
  for (int y = r.y().m_begin; y < r.y().m_end; ++y)
    {
      for (int x = r.x().m_begin; x < r.x().m_end; ++x)
        {
          MAP_LOG("\t\tLight" << ivec2(x, y) << "\n");
          return_value += filler.subrect(x, y).light_rect(tp);
        }
    }

  /* Step 2: for each intersection, tag the rect of the intersection */
  for (int x = m_subrect_range.x().m_begin; x < m_subrect_range.x().m_end; ++x)
    {
      const Intersection &intersection0(get_intersection(filler, minx_side, x));
      const Intersection &intersection1(get_intersection(filler, maxx_side, x));

      MAP_LOG("\t\tmin_x = " << x << "\n");
      return_value += intersection0.light_rects(filler, x_fixed, x, tp);
      MAP_LOG("\t\tmax_x = " << x << "\n");
      return_value += intersection1.light_rects(filler, x_fixed, x, tp);
    }

  for (int y = m_subrect_range.y().m_begin; y < m_subrect_range.y().m_end; ++y)
    {
      const Intersection &intersection0(get_intersection(filler, miny_side, y));
      const Intersection &intersection1(get_intersection(filler, maxy_side, y));

      MAP_LOG("\t\tmin_y = " << y << "\n");
      return_value += intersection0.light_rects(filler, y_fixed, y, tp);
      MAP_LOG("\t\tmax_y = " << y << "\n");
      return_value += intersection1.light_rects(filler, y_fixed, y, tp);
    }

  return return_value;
}

void
astral::Renderer::Implement::Filler::LineClipper::MappedCurve::
add_data_to_subrects(Filler::LineClipper &filler, range_type<int> x_range) const
{
  vecN<range_type<int>, 2> r;
  vecN<range_type<unsigned int>, FillSTCShader::pass_count> blocks;

  /* compute all the sub-rects that this curve touches */
  ASTRALassert(filler.m_lit_by_curves.empty());

  r = filler.subrect_from_coordinate(m_mapped_curve.start_pt());
  for (int y = r.y().m_begin; y < r.y().m_end; ++y)
    {
      for (int x = r.x().m_begin; x < r.x().m_end; ++x)
        {
          filler.m_lit_by_curves.insert(filler.subrect_id(x, y));
        }
    }

  r = filler.subrect_from_coordinate(m_mapped_curve.end_pt());
  for (int y = r.y().m_begin; y < r.y().m_end; ++y)
    {
      for (int x = r.x().m_begin; x < r.x().m_end; ++x)
        {
          filler.m_lit_by_curves.insert(filler.subrect_id(x, y));
        }
    }

  for (int x = m_subrect_range.x().m_begin; x < m_subrect_range.x().m_end; ++x)
    {
      const Intersection &intersection0(get_intersection(filler, minx_side, x));
      const Intersection &intersection1(get_intersection(filler, maxx_side, x));

      intersection0.add_to_lit_by_curves(filler, x_fixed, x);
      intersection1.add_to_lit_by_curves(filler, x_fixed, x);
    }

  for (int y = m_subrect_range.y().m_begin; y < m_subrect_range.y().m_end; ++y)
    {
      const Intersection &intersection0(get_intersection(filler, miny_side, y));
      const Intersection &intersection1(get_intersection(filler, maxy_side, y));

      intersection0.add_to_lit_by_curves(filler, y_fixed, y);
      intersection1.add_to_lit_by_curves(filler, y_fixed, y);
    }

  ASTRALassert(filler.m_builder.empty());
  if (m_mapped_curve.type() != ContourCurve::line_segment)
    {
      c_array<const vec2> empty_line_contour;
      vecN<std::pair<FillSTCShader::ConicTriangle, bool>, 1> conic_tri;
      c_array<const FillSTCShader::LineSegment> seg;

      conic_tri[0].second = true;
      conic_tri[0].first.m_pts[0] = m_mapped_curve.start_pt();
      conic_tri[0].first.m_pts[1] = m_mapped_curve.control_pt(0);
      conic_tri[0].first.m_pts[2] = m_mapped_curve.end_pt();
      filler.m_builder.add_raw(empty_line_contour, conic_tri, seg);
    }
  else if (filler.m_aa_mode == with_anti_aliasing)
    {
      c_array<const vec2> empty_line_contour;
      c_array<const std::pair<FillSTCShader::ConicTriangle, bool>> conic_tri;
      vecN<FillSTCShader::LineSegment, 1> seg;

      seg[0].m_pts[0] = m_mapped_curve.start_pt();
      seg[0].m_pts[1] = m_mapped_curve.end_pt();
      filler.m_builder.add_raw(empty_line_contour, conic_tri, seg);
    }

  filler.create_blocks_from_builder(filler.m_aa_mode, &blocks);

  /* add the contour fuzz to the lit rects */
  if (filler.m_aa_mode == with_anti_aliasing)
    {
      FillSTCShader::PassSet pass_set;

      pass_set
        .add_pass(FillSTCShader::pass_contour_fuzz)
        .add_pass(FillSTCShader::pass_conic_triangle_fuzz);

      for (unsigned int ID : filler.m_lit_by_curves.elements())
        {
          SubRect &SR(filler.m_elementary_rects[ID]);

          ASTRALassert(SR.m_lit[SubRect::lit_by_current_contour]);
          SR.add_blocks(filler, pass_set, blocks);
        }
    }

  /* We also need to add the STC triangle to any rect that
   * intersects the STC triangle (not just those that intersect
   * the curve)
   */
  if (m_mapped_curve.type() != ContourCurve::line_segment)
    {
      for (int y = m_subrect_range.y().m_begin; y < m_subrect_range.y().m_end; ++y)
        {
          for (int x = m_subrect_range.x().m_begin; x < m_subrect_range.x().m_end; ++x)
            {
              ivec2 R(x, y);
              SubRect &SR(filler.subrect(R));

              if (SR.m_lit[SubRect::lit_by_current_contour])
                {
                  /* If the RenerBackend relies does not have HW-clip planes,
                   * then we are relying on that the caller will have tessellated
                   * the conic curves to around the same size as the tiles so that
                   * these conics do not induce a massive amount of fragments
                   * killed by the clipping (that is via discard or depth buffer
                   * occlusion).
                   */
                  SR.add_blocks(filler, FillSTCShader::pass_conic_triangles_stencil, blocks);
                }
            }
        }
    }

  /* modidy the winding offset for those rects not touched by any curve */
  add_winding_offset(filler, x_range);

  /* clear m_builder for the next user */
  filler.m_builder.clear();

  /* clear the list for the next user */
  filler.m_lit_by_curves.clear();
}

void
astral::Renderer::Implement::Filler::LineClipper::MappedCurve::
add_winding_offset(Filler::LineClipper &filler, range_type<int> x_range) const
{
  for (int y = m_subrect_range.y().m_begin; y < m_subrect_range.y().m_end; ++y)
    {
      const Intersection &intersection(get_intersection(filler, miny_side, y));
      c_array<const Intersection::PerPoint> points(intersection.values());
      int effect;
      range_type<int> R;
      range_type<float> f;

      if (points.size() == 1)
        {
          int v;
          range_type<int> b;

          /* only one intersection against the line, get all rects before the intersection */
          effect = points[0].m_winding_effect;
          v = points[0].m_position.x();
          b = filler.subrect_from_coordinate(v);

          R.m_begin = x_range.m_begin;
          R.m_end = t_min(x_range.m_end, b.m_end);

          f.m_begin = filler.minx_side_value(R.m_begin);
          f.m_end = v;
        }
      else if (points.size() == 2)
        {
          /* get teh rects between the intersections */
          float v0, v1;
          range_type<int> b0, b1;

          v0 = points[0].m_position.x();
          v1 = points[1].m_position.x();

          b0 = filler.subrect_from_coordinate(v0);
          b1 = filler.subrect_from_coordinate(v1);

          /* the curve at the -end- of the range is the one whose effect
           * matters because the ray goes to positive infinity from the
           * test point
           */
          if (v0 < v1)
            {
              effect = points[1].m_winding_effect;
              R.m_begin = t_max(x_range.m_begin, b0.m_begin + 1);
              R.m_end = t_min(x_range.m_end, b1.m_end);

              f.m_begin = v0;
              f.m_end = v1;
            }
          else
            {
              effect = points[0].m_winding_effect;
              R.m_begin = t_max(x_range.m_begin, b1.m_begin + 1);
              R.m_end = t_min(x_range.m_end, b0.m_end);

              f.m_begin = v1;
              f.m_end = v0;
            }
        }
      else
        {
          R.m_begin = R.m_end = x_range.m_begin;
          effect = 0;
        }

      for (int x = R.m_begin; x < R.m_end; ++x)
        {
          float fx;
          SubRect &SR(filler.subrect(x, y));

          fx = (filler.minx_side_value(x) + filler.maxx_side_value(x)) * 0.5f;
          if (fx > f.m_begin && fx < f.m_end && !SR.m_lit[SubRect::lit_by_current_contour])
            {
              SR.m_winding_offset += effect;
            }
        }
    }
}

const astral::Renderer::Implement::Filler::LineClipper::Intersection&
astral::Renderer::Implement::Filler::LineClipper::MappedCurve::
get_intersection(const Filler::LineClipper &filler, enum side_t ss, int xy) const
{
  enum line_t ll;
  int F, idx;

  ll = line_t_from_side_t(ss);
  F = fixed_coordinate(ll);

  idx = (m_subrect_range[F].m_begin <= xy && xy < m_subrect_range[F].m_end) ?
    m_intersections[ss] + xy - m_subrect_range[F].m_begin :
    0;

  return filler.m_intersection_backing[idx];
}

/////////////////////////////////////////////////////////
// astral::Renderer::Implement::Filler::LineClipper::MappedContour methods
astral::Renderer::Implement::Filler::LineClipper::MappedContour::
MappedContour(Filler::LineClipper &filler,
              c_array<const ContourCurve> contour,
              bool is_closed, const Transformation &tr)
{
  ASTRALassert(!contour.empty());

  m_curves.m_begin = filler.m_mapped_curve_backing.size();

  /* start this way to initialize m_subrect_range */
  const ContourCurve *prev = (is_closed) ? &contour.back() : nullptr;

  if (!is_closed)
    {
      /* add a closing curve before the rest of the contour */
      ContourCurve C(contour.back().end_pt(), contour.front().start_pt(), ContourCurve::not_continuation_curve);

      filler.m_mapped_curve_backing.push_back(MappedCurve(filler, C, tr, nullptr));
      m_subrect_range.x().absorb(filler.m_mapped_curve_backing.back().m_subrect_range.x());
      m_subrect_range.y().absorb(filler.m_mapped_curve_backing.back().m_subrect_range.y());
    }

  filler.m_mapped_curve_backing.push_back(MappedCurve(filler, contour.front(), tr, prev));
  m_subrect_range = filler.m_mapped_curve_backing.back().m_subrect_range;
  prev = &contour.front();
  contour = contour.sub_array(1);

  /* now do the rest of the curves */
  for (const auto &C : contour)
    {
      /* ISSUE: a conic triangle can cover a huge number of rects and when it
       *        does there will be a lage amount of draw that is clipped to
       *        the rects it is drawn to. For platforms that support HW-clipping
       *        no fragments are rasterized. However, for platforms that do not
       *        support HW clipping, that draw is either clipped via discard
       *        or with depth occluding. At best, we are looking at oodles of
       *        framerate burned. At worst, the fragment shader for the discard
       *        gets spawned as well. What we should do instead is to break the
       *        into the locations where it intersects the rect-boundariers. Then,
       *        those conic triangles will be (mostly) inside of the rect-region.
       *        The other solution which carries a different overhead is to have
       *        Contour::fill_render_data() and AnimatedContour::fill_render_data()
       *        break up the conics as according to tolerance as well (just as the
       *        stroking data is done). This will make the curves smaller too. The
       *        main issue with that strategy is that everything gets that overhead
       *        (curve-clipping and shadow generation); so we have a ral TODO.
       *
       * TODO: if the render backend does not support HW clip planes, break C
       *       into pieces where the pieces are where the curve intersects a
       *       horizontal or vertical rect-line boudnary.
       */
      filler.m_mapped_curve_backing.push_back(MappedCurve(filler, C, tr, prev));

      m_subrect_range.x().absorb(filler.m_mapped_curve_backing.back().m_subrect_range.x());
      m_subrect_range.y().absorb(filler.m_mapped_curve_backing.back().m_subrect_range.y());
      prev = &C;
    }

  m_curves.m_end = filler.m_mapped_curve_backing.size();

  ASTRALassert(curves(filler).empty() ||
               curves(filler).front().m_mapped_curve.start_pt()
               == curves(filler).back().m_mapped_curve.end_pt());
}

void
astral::Renderer::Implement::Filler::LineClipper::MappedContour::
add_data_to_subrects(Filler::LineClipper &filler) const
{
  for (int y = m_subrect_range.y().m_begin; y < m_subrect_range.y().m_end; ++y)
    {
      for (int x = m_subrect_range.x().m_begin; x < m_subrect_range.x().m_end; ++x)
        {
          filler.subrect(x, y).m_lit[SubRect::lit_by_current_contour] = false;
        }
    }

  /* set m_lit[SubRect::lit_by_current_contour] to true
   * for any rect any curve touches.
   */
  for (int i = m_curves.m_begin; i < m_curves.m_end; ++i)
    {
      ASTRALassert(m_subrect_range.x().m_begin <= filler.m_mapped_curve_backing[i].m_subrect_range.x().m_begin);
      ASTRALassert(m_subrect_range.y().m_begin <= filler.m_mapped_curve_backing[i].m_subrect_range.y().m_begin);
      ASTRALassert(m_subrect_range.x().m_end >= filler.m_mapped_curve_backing[i].m_subrect_range.x().m_end);
      ASTRALassert(m_subrect_range.y().m_end >= filler.m_mapped_curve_backing[i].m_subrect_range.y().m_end);
      filler.m_mapped_curve_backing[i].light_rects(filler, SubRect::lit_by_current_contour);
    }

  /* Add the STC for conic triangles and the anti-alias fuzz;
   * we have to do this AFTER all rect that could be lit are lit.
   */
  for (int i = m_curves.m_begin; i < m_curves.m_end; ++i)
    {
      filler.m_mapped_curve_backing[i].add_data_to_subrects(filler, m_subrect_range.x());
    }
}

unsigned int
astral::Renderer::Implement::Filler::LineClipper::MappedContour::
light_rects(Filler::LineClipper &filler) const
{
  unsigned int return_value(0u);
  for (int i = m_curves.m_begin; i < m_curves.m_end; ++i)
    {
      return_value += filler.m_mapped_curve_backing[i].light_rects(filler, SubRect::lit_by_path);
    }

  return return_value;
}

/////////////////////////////////////////////////////////
// astral::Renderer::Implement::Filler::LineClipper::ClippedCurve methods
astral::Renderer::Implement::Filler::LineClipper::ClippedCurve::
ClippedCurve(int curve, Filler::LineClipper &filler):
  m_parent_curve(curve),
  m_type(unclipped_type)
{
  ASTRALassert(m_parent_curve >= 0);
  ASTRALassert(m_parent_curve < static_cast<int>(filler.m_mapped_curve_backing.size()));

  const MappedCurve &src(filler.m_mapped_curve_backing[m_parent_curve]);
  m_start_pt = src.m_mapped_curve.start_pt();
  m_end_pt = src.m_mapped_curve.end_pt();
}

//////////////////////////////////////////////////////////
// astral::Renderer::Implement::Filler::LineClipper::ContourClipper methods
astral::Renderer::Implement::Filler::LineClipper::ContourClipper::
ContourClipper(Filler::LineClipper &filler,
               c_array<const ClippedCurve> src,
               enum side_t side, int R,
               std::vector<ClippedCurve> *dst):
  m_clip_side(side),
  m_clip_line(line_t_from_side_t(side)),
  m_fc(fixed_coordinate(m_clip_line)),
  m_R(R),
  m_R_value(filler.side_value(m_R, m_clip_side))
{
  ASTRALassert(dst);

  dst->clear();
  for (const auto &curve : src)
    {
      float start_d(compute_clip_distance(curve.start_pt()));
      bool start_in(start_d >= 0.0f);
      float end_d(compute_clip_distance(curve.end_pt()));
      bool end_in(end_d >= 0.0f);

      if (end_in && start_in)
        {
          add_curve(dst, curve);
        }
      else if (end_in || start_in)
        {
          vec2 p;

          p = compute_induced_point(curve.start_pt(), start_d,
                                    curve.end_pt(), end_d);

          if (end_in)
            {
              ClippedCurve E(curve, p, curve.end_pt());
              add_curve(dst, E);
            }
          else
            {
              ClippedCurve S(curve, curve.start_pt(), p);
              add_curve(dst, S);
            }
        }
    }

  if (!dst->empty() && dst->back().end_pt() != dst->front().start_pt())
    {
      dst->push_back(ClippedCurve(dst->back().end_pt(), dst->front().start_pt()));
    }
}

void
astral::Renderer::Implement::Filler::LineClipper::ContourClipper::
add_curve(std::vector<ClippedCurve> *dst, const ClippedCurve &curve)
{
  if (!dst->empty() && dst->back().end_pt() != curve.start_pt())
    {
      dst->push_back(ClippedCurve(dst->back().end_pt(), curve.start_pt()));
    }

  if (!dst->empty() && dst->back().is_cancelling_edge(curve))
    {
      dst->pop_back();
    }
  else
    {
      dst->push_back(curve);
    }
}

///////////////////////////////////////////////////////
// astral::Renderer::Implement::Filler::LineClipper::SubRect methods
astral::Renderer::Implement::Filler::LineClipper::SubRect::
SubRect(ivec2 R, Filler::LineClipper &filler, const ClipElement *clip_element):
  m_lit(false, false),
  m_winding_offset(0),
  m_filler(filler),
  m_ID(R)
{
  m_center.x() = 0.5f * (m_filler.minx_side_value(R.x()) + m_filler.maxx_side_value(R.x()));
  m_center.y() = 0.5f * (m_filler.miny_side_value(R.y()) + m_filler.maxy_side_value(R.y()));

  if (filler.m_range_values.empty())
    {
      m_skip_rect = false;
    }
  else
    {
      m_skip_rect = true;
      for (unsigned int i = 0, endi = filler.m_range_values.size(); i < endi && m_skip_rect; ++i)
        {
          const vecN<range_type<int>, 2> &bb(filler.m_range_values[i]);
          if (m_ID.x() >= bb.x().m_begin && m_ID.x() < bb.x().m_end
              && m_ID.y() >= bb.y().m_begin && m_ID.y() < bb.y().m_end)
            {
              m_skip_rect = false;
            }
        }
    }

  m_skip_rect = m_skip_rect || (clip_element && clip_element->empty_tile(m_ID));
}

void
astral::Renderer::Implement::Filler::LineClipper::SubRect::
add_edge_hugging_contour(c_array<const ClippedCurve> contour)
{
  if (m_skip_rect)
    {
      return;
    }

  for (const auto &curve : contour)
    {
      ASTRALassert(curve.type() == ClippedCurve::edge_hugger_type);
      if (curve.start_pt().x() > m_center.x() && curve.end_pt().x() > m_center.x())
        {
          /* The winding effect MUST follow the convention that
           * is used in Renderer: Clockwise increments the winding
           * number and CounterClockwise decrements where the
           * y-axis increases downwardly.
           */
          if (curve.start_pt().y() <= m_center.y() && curve.end_pt().y() > m_center.y())
            {
              ++m_winding_offset;
            }
          else if (curve.end_pt().y() <= m_center.y() && curve.start_pt().y() > m_center.y())
            {
              --m_winding_offset;
            }
        }
    }
}

void
astral::Renderer::Implement::Filler::LineClipper::SubRect::
add_stc_contour_data(LineClipper &clipper, c_array<const ClippedCurve> curves)
{
  if (m_skip_rect)
    {
      return;
    }

  c_array<const std::pair<FillSTCShader::ConicTriangle, bool>> conic_tri;
  c_array<const FillSTCShader::LineSegment> seg;
  vecN<range_type<unsigned int>, FillSTCShader::pass_count> vert_blocks;

  ASTRALassert(!curves.empty());
  ASTRALassert(m_filler.m_builder.empty());

  m_filler.m_workroom_line_contour.clear();
  for (const auto &C : curves)
    {
      m_filler.m_workroom_line_contour.push_back(C.start_pt());
    }
  m_filler.m_workroom_line_contour.push_back(curves.front().start_pt());
  m_filler.m_builder.add_raw(make_c_array(m_filler.m_workroom_line_contour), conic_tri, seg);

  /* for each clipped curve in curves, add a triangle
   * (that is mostly likely degenerate) that prevents
   * T-intersections.
   */
  for (const auto &C : curves)
    {
      if (C.type() == ClippedCurve::clipped_type)
        {
          const ContourCurve &src(C.contour_curve(m_filler).m_mapped_curve);
          vecN<vec2, 5> pts;

          pts[0] = src.start_pt();
          pts[1] = src.end_pt();
          pts[2] = C.end_pt();
          pts[3] = C.start_pt();
          pts[4] = src.start_pt();

          m_filler.m_builder.add_raw(pts, conic_tri, seg);
        }
    }

  m_filler.create_blocks_from_builder(FillSTCShader::pass_contour_stencil, &vert_blocks);
  add_blocks(clipper, FillSTCShader::pass_contour_stencil, vert_blocks);

  // cleanup
  m_filler.m_builder.clear();
}

void
astral::Renderer::Implement::Filler::LineClipper::SubRect::
add_blocks(LineClipper &clipper, FillSTCShader::PassSet pass_set,
           const vecN<range_type<unsigned int>, FillSTCShader::pass_count> &vert_blocks)
{
  if (m_skip_rect)
    {
      return;
    }

  /* make sure that the encoder exists, although we do not
   * manipulate the VirtualBuffer directly here.
   */
  ready_encoder();

  ASTRALassert(m_encoder.valid());
  ASTRALassert(m_tr.valid());
  ASTRALassert(m_filler.m_item_data.valid());

  STCData::BuilderSet *stc_builder;

  stc_builder = clipper.stc_builder_for_rect(m_ID.x(), m_ID.y());
  ASTRALassert(stc_builder);

  for (unsigned int pass = 0; pass < FillSTCShader::pass_count; ++pass)
    {
      enum FillSTCShader::pass_t pass_t;
      c_array<const VertexStreamerBlock> blocks;

      pass_t = static_cast<enum FillSTCShader::pass_t>(pass);
      if (pass_set.has_pass(pass_t))
        {
          blocks = m_filler.m_renderer.m_vertex_streamer->blocks(vert_blocks[pass_t]);
          for (const auto &block : blocks)
            {
              ASTRALassert(block.m_object);
              ASTRALassert(!block.m_dst.empty());
              stc_builder->add_stc_pass(pass_t, block.m_object,
                                        range_type<int>(block.m_offset, block.m_offset + block.m_dst.size()),
                                        m_tr, m_filler.m_item_data);
            }
        }
    }
}

void
astral::Renderer::Implement::Filler::LineClipper::SubRect::
ready_encoder(void)
{
  ASTRALassert(m_lit[lit_by_current_contour]);
  ASTRALassert(!m_skip_rect);
  if (!m_encoder.valid())
    {
      ivec2 size;
      Transformation tr;

      /* We gain nothing by making the image on demand, since the image is exactly
       * one tile. In addition, the assert code to make sure the image size and
       * tile count is correct, need the backing image to be made immediately.
       *
       * In addition, later logic also requires that the backing images are also
       * ready.
       */
      size = ivec2(ImageAtlas::tile_size);
      m_encoder = m_filler.m_renderer.m_storage->create_virtual_buffer(VB_TAG, size, m_filler.m_fill_rule,
                                                                       VirtualBuffer::ImageCreationSpec()
                                                                       .create_immediately(true)
                                                                       .default_use_prepadding_true(true));

      ASTRALassert(m_encoder.virtual_buffer().fetch_image());
      ASTRALassert(m_encoder.virtual_buffer().fetch_image()->mip_chain().size() == 1);
      ASTRALassert(m_encoder.virtual_buffer().fetch_image()->mip_chain().front()->number_elements(ImageMipElement::empty_element) == 0u);
      ASTRALassert(m_encoder.virtual_buffer().fetch_image()->mip_chain().front()->number_elements(ImageMipElement::white_element) == 0u);
      ASTRALassert(m_encoder.virtual_buffer().fetch_image()->mip_chain().front()->number_elements(ImageMipElement::color_element) == 1u);

      /* we need the transformation that maps (minx_side(), miny_side()) to (0, 0) */
      tr.translate(-m_filler.minx_side_value(m_ID.x()), -m_filler.miny_side_value(m_ID.y()));
      m_tr = m_encoder.create_value(tr);
    }
  ASTRALassert(m_encoder.valid());
  ASTRALassert(m_tr.valid());
}

/////////////////////////////////////////////////////////
// astral::Renderer::Implement::Filler::LineClipper methods
astral::reference_counted_ptr<const astral::Image>
astral::Renderer::Implement::Filler::LineClipper::
create_sparse_mask(ivec2 rect_size,
                   c_array<const BoundingBox<float>> restrict_bbs,
                   const CombinedPath &path,
                   const ClipElement *clip_element,
                   enum clip_combine_mode_t clip_combine_mode,
                   TileTypeTable *out_clip_combine_tile_data)
{
  reference_counted_ptr<const Image> return_value;

  ASTRALassert(rect_size.x() > 0 && rect_size.y() > 0);

  create_subrects(rect_size, clip_element, restrict_bbs);

  MAP_LOG("\n\nRectSize = " << rect_size << ", rect_count = " << m_number_elementary_rects << "\n");
  if (map_contours_and_light_rects(path))
    {
      /* build the Image; those rects with no curve intersecting
       * get realized as fully covered or not-covered; those with curves
       * get the STC groove.
       */
      #warning "restrict_bbs should be used by Filler::LineClipper::build_sparse_image()"
      return_value = build_sparse_image(clip_element, clip_combine_mode, out_clip_combine_tile_data);
    }

  cleanup();
  return return_value;
}

void
astral::Renderer::Implement::Filler::LineClipper::
cleanup(void)
{
  cleanup_common();
  m_mapped_curve_backing.clear();
  m_mapped_contours.clear();
  m_intersection_backing.clear();
  m_elementary_rects.clear();
  m_number_lit = 0u;
  m_num_culled_paths = 0u;
  m_num_culled_contours = 0u;
  m_total_num_paths = 0u;
  m_total_num_contours = 0u;
  m_num_late_culled_contours = 0u;
}

void
astral::Renderer::Implement::Filler::LineClipper::
create_subrects(ivec2 mask_size, const ClipElement *clip_element,
                c_array<const BoundingBox<float>> bbs)
{
  unsigned int num;

  ASTRALassert(m_elementary_rects.empty());
  set_subrect_values(mask_size, clip_element);

  m_range_values.clear();
  for (const BoundingBox<float> &bb : bbs)
    {
      m_range_values.push_back(subrect_range_from_coordinate(bb.min_point(), bb.max_point()));
    }

  num = number_subrects();
  m_elementary_rects.reserve(num);
  for (int y = 0; y < m_number_elementary_rects.y(); ++y)
    {
      for (int x = 0; x < m_number_elementary_rects.x(); ++x)
        {
          m_elementary_rects.push_back(SubRect(ivec2(x, y), *this, clip_element));
        }
    }
}

bool
astral::Renderer::Implement::Filler::LineClipper::
map_contours_and_light_rects(const CombinedPath &combined_path)
{
  if (m_number_elementary_rects.x() < 3
      || m_number_elementary_rects.y() < 3)
    {
      /* if either dimension is less than 3-rects, then there
       * is little hope that there is sparse filling.
       *
       * NOTE: this is not exactly true, if the original rect
       *       is in a corner of the screen and much of
       *       the contours are offscreen then some of the
       *       sub-rects will likely not have any contours.
       *       The real reason is to avoid needing to do
       *       the solves for small screen path-fills which
       *       usually don't have sparse filling.
       */
      return false;
    }

  ASTRALassert(m_mapped_curve_backing.empty());
  ASTRALassert(m_mapped_contours.empty());
  ASTRALassert(m_intersection_backing.empty());
  ASTRALassert(m_number_lit == 0u);
  ASTRALassert(m_num_culled_paths == 0u);
  ASTRALassert(m_num_culled_contours == 0u);
  ASTRALassert(m_num_late_culled_contours == 0u);
  ASTRALassert(m_total_num_paths == 0u);
  ASTRALassert(m_total_num_contours == 0u);

  /* init m_intersection_backing with a single null value */
  m_intersection_backing.push_back(Intersection());

  m_thresh_lit = (3u * m_elementary_rects.size()) / 4u;
  Helper::map_contours<Path>(*this, combined_path);
  Helper::map_contours<AnimatedPath>(*this, combined_path);

  if (m_number_lit > m_thresh_lit)
    {
      return false;
    }

  //  std::cout << "\n\n\n\n\nWorthy for sparse with " << m_number_lit
  //        << " rects against " << m_elementary_rects.size()
  //        << " total rects, num culled paths = " << m_num_culled_paths
  //        << " of " << m_total_num_paths << ", num culled contours "
  //        << m_num_culled_contours + m_num_late_culled_contours
  //        << "(late = " << m_num_late_culled_contours << ") of "
  //        << m_total_num_contours << " contours\n";

  return true;
}

void
astral::Renderer::Implement::Filler::LineClipper::
create_clipped_contour(const MappedContour &contour,
                       std::vector<ClippedCurve> *out_contour)
{
  out_contour->clear();
  for (int i = contour.m_curves.m_begin; i < contour.m_curves.m_end; ++i)
    {
      out_contour->push_back(ClippedCurve(i, *this));
    }
}

astral::c_array<const astral::Renderer::Implement::Filler::LineClipper::ClippedCurve>
astral::Renderer::Implement::Filler::LineClipper::
clip_contour(c_array<const ClippedCurve> in_contour,
             enum side_t side, int box_row_col,
             std::vector<ClippedCurve> *workroom)
{
  if (in_contour.empty())
    {
      return c_array<const ClippedCurve>();
    }

  ++m_renderer.m_stats[number_sparse_fill_contours_clipped];

  ContourClipper clipper(*this, in_contour, side, box_row_col, workroom);

  c_array<const ClippedCurve> tmp(make_c_array(*workroom));
  while (!tmp.empty() && tmp.front().is_cancelling_edge(tmp.back()))
    {
      tmp.pop_front();
      if (!tmp.empty())
        {
          tmp.pop_back();
        }
    }

  return tmp;
}

bool
astral::Renderer::Implement::Filler::LineClipper::
contour_is_edge_huggers_only(c_array<const ClippedCurve> contour)
{
  for (const auto &v : contour)
    {
      if (v.type() != ClippedCurve::edge_hugger_type)
        {
          return false;
        }
    }

  return true;
}

void
astral::Renderer::Implement::Filler::LineClipper::
process_subrects_contour_is_huggers_only(c_array<const ClippedCurve> contour,
                                         const vecN<range_type<int>, 2> &boxes)
{
  /* for those boxes that are classified as touched by contour,
   * we add the winding offset computed from the contour instead
   * of adding the contour to the box.
   */
  ASTRALassert(boxes.x().m_begin < boxes.x().m_end);
  ASTRALassert(boxes.y().m_begin < boxes.y().m_end);

  /* ODD: if we compute the winding offset once for the entire
   * edge hugger at the center of the box region and use that
   * we get rendering issues, but if we do it seperately for
   * each rect, it comes out ok.
   */
  for (int y = boxes.y().m_begin; y < boxes.y().m_end; ++y)
    {
      for (int x = boxes.x().m_begin; x < boxes.x().m_end; ++x)
        {
          SubRect &SR(subrect(x, y));
          if (SR.m_lit[SubRect::lit_by_current_contour])
            {
              SR.add_edge_hugging_contour(contour);
            }
        }
    }
}

void
astral::Renderer::Implement::Filler::LineClipper::
process_mapped_contour(const MappedContour &contour)
{
  c_array<const ClippedCurve> current;

  ASTRALassert(contour.m_subrect_range.x().m_begin < contour.m_subrect_range.x().m_end);

  unsigned int cnt(contour.m_subrect_range.x().difference() * contour.m_subrect_range.x().difference());
  m_renderer.m_stats[number_sparse_fill_subrects_clipping] += cnt;

  /* TODO: have MappedContour::add_data_to_subrects() return a
   *       code indicating that doing sparse stroking on the
   *       contour is a waste of time and instead the function
   *       will just add the STC data of the untransformed contour
   *       (but with the correct transformation and item data)
   *       to the rects it hits. The motivation is to prevent
   *       streaming vertices when sparse filling won't help.
   *       The expected use case is a wide and short combined
   *       path that has been rotated. Many rects would be empty
   *       completely, making sparse worth while, but streaming
   *       would make the computation unnecessarily large.
   */

  /* TODO: In addition, have add_data_to_subrects() compute the
   *       number of rects lit by curves for each row and column;
   *       we can use those counts to reduce the clipping load and
   *       early out sooner and also skip entire rows or columns.
   */

  /* walk the intersections computed for the passed mapped contour
   * and add the necessary anti-alias fuzz and conic triangle data.
   * Also, rects that do not have any interectsions, update the
   * winding offset values from the mapped contour.
   */
  contour.add_data_to_subrects(*this);

  /* First realize the MappedContour as a clipped contour */
  create_clipped_contour(contour, &m_clipped_contourA[0]);
  current = make_c_array(m_clipped_contourA[0]);
  int work(1);

  /* - clip it against maxx_side on contour.m_subrect_range.x().m_end - 1
   * - clip it against minx_side on contour.m_subrect_range.x().m_begin
   * - clip it against maxy_side on contour.m_subrect_range.y().m_end - 1
   * - clip it against miny_side on contour.m_subrect_range.y().m_begin
   */
  current = clip_contour(current, maxx_side,
                         contour.m_subrect_range.x().m_end - 1,
                         &m_clipped_contourA[work]);
  work = 1 - work;

  current = clip_contour(current, minx_side,
                         contour.m_subrect_range.x().m_begin,
                         &m_clipped_contourA[work]);
  work = 1 - work;

  current = clip_contour(current, maxy_side,
                         contour.m_subrect_range.y().m_end - 1,
                         &m_clipped_contourA[work]);
  work = 1 - work;

  current = clip_contour(current, miny_side,
                         contour.m_subrect_range.y().m_begin,
                         &m_clipped_contourA[work]);
  work = 1 - work;

#define EARLY_OUT(X, R) do {                            \
    if (contour_is_edge_huggers_only(X))                \
      {                                                 \
        process_subrects_contour_is_huggers_only(X, R); \
        return;                                         \
      }                                                 \
    } while(0)

  /* We clip from the outside in to give EARLY_OUT() a
   * better chance of happening
   */
  vecN<range_type<int>, 2> current_range(contour.m_subrect_range);
  while (current_range.x().difference() > 2 && current_range.y().difference() > 2)
    {
      /* At entry we have that current is clipped against
       *  (minx_side, current_range.x().m_begin)
       *  (maxx_side, current_range.x().m_end - 1)
       *  (miny_side, current_range.y().m_begin)
       *  (maxy_side, current_range.y().m_end - 1)
       */
      c_array<const ClippedCurve> tmp;

      /* handle the left column */
      tmp = clip_contour(current, maxx_side, current_range.x().m_begin, &m_clipped_contourA[work]);
      process_mapped_contour_column(tmp, current_range.x().m_begin, current_range.y());

      /* remove the left column */
      ++current_range.x().m_begin;
      current = clip_contour(current, minx_side, current_range.x().m_begin, &m_clipped_contourA[work]);
      work = 1 - work;
      EARLY_OUT(current, current_range);

      /* handle the right column */
      tmp = clip_contour(current, minx_side, current_range.x().m_end - 1, &m_clipped_contourA[work]);
      process_mapped_contour_column(tmp, current_range.x().m_end - 1, current_range.y());

      /* remove the right column */
      --current_range.x().m_end;
      current = clip_contour(current, maxx_side, current_range.x().m_end - 1, &m_clipped_contourA[work]);
      work = 1 - work;
      EARLY_OUT(current, current_range);

      /* handle the top row */
      tmp = clip_contour(current, maxy_side, current_range.y().m_begin, &m_clipped_contourA[work]);
      process_mapped_contour_row(tmp, current_range.y().m_begin, current_range.x());

      /* remove the top row */
      ++current_range.y().m_begin;
      current = clip_contour(current, miny_side, current_range.y().m_begin, &m_clipped_contourA[work]);
      work = 1 - work;
      EARLY_OUT(current, current_range);

      /* handle the bottom row */
      tmp = clip_contour(current, miny_side, current_range.y().m_end - 1, &m_clipped_contourA[work]);
      process_mapped_contour_row(tmp, current_range.y().m_end - 1, current_range.x());

      /* remove the bottom row */
      --current_range.y().m_end;
      current = clip_contour(current, maxy_side, current_range.y().m_end - 1, &m_clipped_contourA[work]);
      work = 1 - work;
      EARLY_OUT(current, current_range);
    }

  /* now cut off from the left and right sides */
  while (current_range.x().difference() > 2)
    {
      /* At entry we have that current is clipped against
       *  (minx_side, current_range.x().m_begin)
       *  (maxx_side, current_range.x().m_end - 1)
       *  (miny_side, current_range.y().m_begin)
       *  (maxy_side, current_range.y().m_end - 1)
       */

      c_array<const ClippedCurve> tmp;

      /* handle the left column */
      tmp = clip_contour(current, maxx_side, current_range.x().m_begin, &m_clipped_contourA[work]);
      process_mapped_contour_column(tmp, current_range.x().m_begin, current_range.y());

      /* remove the left column */
      ++current_range.x().m_begin;
      current = clip_contour(current, minx_side, current_range.x().m_begin, &m_clipped_contourA[work]);
      work = 1 - work;
      EARLY_OUT(current, current_range);

      /* handle the right column */
      tmp = clip_contour(current, minx_side, current_range.x().m_end - 1, &m_clipped_contourA[work]);
      process_mapped_contour_column(tmp, current_range.x().m_end - 1, current_range.y());

      /* remove the right column */
      --current_range.x().m_end;
      current = clip_contour(current, maxx_side, current_range.x().m_end - 1, &m_clipped_contourA[work]);
      work = 1 - work;
      EARLY_OUT(current, current_range);
    }

  /* now cut off from the bottom and top */
  while (current_range.y().difference() > 2)
    {
      c_array<const ClippedCurve> tmp;

      /* handle the top row */
      tmp = clip_contour(current, maxy_side, current_range.y().m_begin, &m_clipped_contourA[work]);
      process_mapped_contour_row(tmp, current_range.y().m_begin, current_range.x());

      /* remove the top row */
      ++current_range.y().m_begin;
      current = clip_contour(current, miny_side, current_range.y().m_begin, &m_clipped_contourA[work]);
      work = 1 - work;
      EARLY_OUT(current, current_range);

      /* handle the bottom row */
      tmp = clip_contour(current, miny_side, current_range.y().m_end - 1, &m_clipped_contourA[work]);
      process_mapped_contour_row(tmp, current_range.y().m_end - 1, current_range.x());

      /* remove the bottom row */
      --current_range.y().m_end;
      current = clip_contour(current, maxy_side, current_range.y().m_end - 1, &m_clipped_contourA[work]);
      work = 1 - work;
      EARLY_OUT(current, current_range);
    }

  /* at this point the number of box rows and box columns
   * is less than three, we just walk the columns instead
   */
  for (; current_range.x().m_begin < current_range.x().m_end; ++current_range.x().m_begin)
    {
      /* at entry we have that current is clipped against (minx_side, current_range.x().m_begin).
       * To clip to the column requires that it is clipped against (maxx_side, current_range.x().m_begin)
       */
      if (current_range.x().m_begin + 1 != current_range.x().m_end)
        {
          c_array<const ClippedCurve> tmp;

          /* Clip it against (maxx_side, current_range.x().m_begin) */
          tmp = clip_contour(current, maxx_side, current_range.x().m_begin, &m_clipped_contourA[work]);
          process_mapped_contour_column(tmp, current_range.x().m_begin, current_range.y());

          /* The next iteration reqires the contour to be clipped
           * against (minx_side, current_range.x().m_begin + 1)
           */
          current = clip_contour(current, minx_side, current_range.x().m_begin + 1, &m_clipped_contourA[work]);
          work = 1 - work;
          EARLY_OUT(current, current_range);
        }
      else
        {
          /* we are on the last column, it was already clipped against
           * (maxx_side, current_range.x().m_end - 1)
           */
          process_mapped_contour_column(current, current_range.x().m_begin, current_range.y());
        }
    }

#undef EARLY_OUT
}

void
astral::Renderer::Implement::Filler::LineClipper::
process_mapped_contour_row(c_array<const ClippedCurve> current,
                           int box_row, range_type<int> box_col_range)
{
  /* At entry, the contour is clipped as follows:
   *   - clipped against (minx_side, box_col_range.m_begin)
   *   - clipped against (maxx_side, box_col_range.m_end - 1)
   *   - clipped against (miny_side, box_row)
   *   - clipped against (maxy_side, box_row)
   *
   * We need to process the row of boxes box_row
   */
  int work(0);

  for (int j = box_col_range.m_begin; j < box_col_range.m_end && !current.empty(); ++j)
    {
      /* at entry we have that current is clipped against (miny_side, j).
       * To clip to the box requires that it is clipped against (maxy_side, j)
       */
      if (j + 1 != box_col_range.m_end)
        {
          c_array<const ClippedCurve> tmp;

          /* Clip it against (maxy_side, j) */
          tmp = clip_contour(current, maxx_side, j, &m_clipped_contourB[work]);
          process_subrect(tmp, j, box_row);

          /* for the next iteration, clip it against (miny_side, j + 1) */
          current = clip_contour(current, minx_side, j + 1, &m_clipped_contourB[work]);
          work = 1 - work;
        }
      else
        {
          /* we are on the last row, it was already cliped against (maxy_side, j) by caller */
          process_subrect(current, j, box_row);
        }
    }
}

void
astral::Renderer::Implement::Filler::LineClipper::
process_mapped_contour_column(c_array<const ClippedCurve> current,
                              int box_col, range_type<int> box_row_range)
{
  /* At entry, the contour is clipped as follows:
   *   - clipped against (minx_side, box_col)
   *   - clipped against (maxx_side, box_col)
   *   - clipped against (miny_side, box_row_range.m_begin)
   *   - clipped against (maxy_side, box_row_range.m_end - 1)
   *
   * We need to process the column of boxes box_col
   */
  int work(0);

  for (int j = box_row_range.m_begin; j < box_row_range.m_end && !current.empty(); ++j)
    {
      /* at entry we have that current is clipped against (miny_side, j).
       * To clip to the box requires that it is clipped against (maxy_side, j)
       */
      if (j + 1 != box_row_range.m_end)
        {
          c_array<const ClippedCurve> tmp;

          /* Clip it against (maxy_side, j) */
          tmp = clip_contour(current, maxy_side, j, &m_clipped_contourB[work]);
          process_subrect(tmp, box_col, j);

          /* for the next iteration, clip it against (miny_side, j + 1) */
          current = clip_contour(current, miny_side, j + 1, &m_clipped_contourB[work]);
          work = 1 - work;
        }
      else
        {
          /* we are on the last row, it was already cliped against (maxy_side, j) by caller */
          process_subrect(current, box_col, j);
        }
    }
}

void
astral::Renderer::Implement::Filler::LineClipper::
process_subrect(c_array<const ClippedCurve> contour, int box_col, int box_row)
{
  ivec2 R(box_col, box_row);
  SubRect &SR(subrect(R));

  if (SR.skip_rect())
    {
      return;
    }

  if (SR.m_lit[SubRect::lit_by_current_contour])
    {
      if (contour_is_edge_huggers_only(contour))
        {
          SR.add_edge_hugging_contour(contour);
        }
      else
        {
          SR.add_stc_contour_data(*this, contour);
        }
    }
}

astral::reference_counted_ptr<const astral::Image>
astral::Renderer::Implement::Filler::LineClipper::
build_sparse_image(const ClipElement *clip_element,
                   enum clip_combine_mode_t clip_combine_mode,
                   TileTypeTable *out_clip_combine_tile_data)
{
  /* process_mapped_contour() will give those SubRects that
   * have contours going thorigh them a RenderEncoderMask.
   * In addition, if a contour C clipped against a SubRect R
   * is only edge huggers, then R.m_winding_offset will get
   * incremented/decremented by the effect of C on R's
   * winding number. Lastly, if a contour C clipped against
   * R does have curves, then process_mapped_contour() adds
   * the STC data to R's VirtualBuffer.
   *
   * At the end, if the base fill rule is odd-even, for each
   * SubRect if the m_winding_offset is odd, then take the
   * inverse fill-rule for its RenderEncoderMask. If the base
   * fill rule is non-zero, add m_winding_offset rects of the
   * correct orientation to the STCData of that RenderEncoderMask
   */

  /* initialize the CustomSet to handle m_elementary_rects.size() */
  m_lit_by_curves.init(m_lit_by_curves_backing, m_elementary_rects.size());

  /* create item data *now* because processing the mapped contours needs it */
  vecN<gvec4, FillSTCShader::item_data_size> item_data;
  float time(0.0f);
  float scale_factor(1.0f);

  FillSTCShader::pack_item_data(time, scale_factor, item_data);
  m_item_data = m_renderer.create_item_data(item_data, no_item_data_value_mapping);

  /* for each contour:
   *  - add its curves to the rects it hits
   *  - increment/decrement the winding offset
   *    for each rect it does not hit but winds
   *    around
   */
  for (const auto &M : m_mapped_contours)
    {
      process_mapped_contour(M);
    }

  return create_sparse_image_from_rects(m_item_data, clip_element, clip_combine_mode, out_clip_combine_tile_data);
}

/*!
 * \file renderer_filler_curve_clipping.cpp
 * \brief file renderer_filler_curve_clipping.cpp
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
#include "renderer_filler_curve_clipping.hpp"

class astral::Renderer::Implement::Filler::CurveClipper::Helper
{
public:
  template<typename T>
  static
  void
  map_contours(Filler::CurveClipper &filler, const CombinedPath &path);

private:
  static
  c_array<const ContourCurve>
  unmapped_curves(Filler::CurveClipper &filler, const CachedCombinedPath::PerObject &tr_tol,
                  const Contour &contour, float t);

  static
  c_array<const ContourCurve>
  unmapped_curves(Filler::CurveClipper &filler, const CachedCombinedPath::PerObject &tr_tol,
                  const AnimatedContour &contour, float t);
};

class astral::Renderer::Implement::Filler::CurveClipper::ClipLog
{
public:
  explicit
  ClipLog(astral::Renderer &renderer,
          const MappedContour &current):
    m_logger(renderer.implement().m_clipping_error_callback),
    m_current(current)
  {}

  reference_counted_ptr<SparseFillingErrorCallBack> m_logger;
  const MappedContour &m_current;
};

#define CLIP_ERROR_LOG(_log, X) do { \
  if (_log.m_logger)                 \
    {                                \
      std::ostringstream str;        \
      str << X;                                                         \
      if (_log.m_current.m_src_contour)                                 \
        {                                                               \
          _log.m_logger->report_error(*_log.m_current.m_src_contour, str.str()); \
        }                                                               \
      else                                                              \
        {                                                               \
          _log.m_logger->report_error(*_log.m_current.m_src_animated_contour, _log.m_current.m_src_animated_contour_time, str.str()); \
        }                                                               \
    }                                                                   \
} while(0)

////////////////////////////////////////////
// astral::Renderer::Implement::Filler::CurveClipper::Helper methods
astral::c_array<const astral::ContourCurve>
astral::Renderer::Implement::Filler::CurveClipper::Helper::
unmapped_curves(Filler::CurveClipper &filler, const CachedCombinedPath::PerObject &tr_tol,
                const Contour &contour, float t)
{
  ASTRALunused(filler);
  ASTRALunused(t);
  ASTRALassert(0.0f <= t && t <= 1.0f);
  /* Because curves are clipped to each sub-rect, it is better to
   * have fewer curves since after clipping all curves will be
   * no bigger than a sub-rect.
   */
  return contour.fill_approximated_geometry(tr_tol.m_tol, contour_fill_approximation_allow_long_curves);
}

astral::c_array<const astral::ContourCurve>
astral::Renderer::Implement::Filler::CurveClipper::Helper::
unmapped_curves(Filler::CurveClipper &filler, const CachedCombinedPath::PerObject &tr_tol,
                const AnimatedContour &contour, float t)
{
  const auto &curves(contour.fill_approximated_geometry(tr_tol.m_tol, contour_fill_approximation_allow_long_curves));

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
astral::Renderer::Implement::Filler::CurveClipper::Helper::
map_contours(Filler::CurveClipper &filler, const CombinedPath &combined_path)
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
              MappedContour M(filler, contour, t,
                              curves, contour.closed(),
                              tr_tol.m_buffer_transformation_path);

              if (M.m_subrect_range.x().m_begin != M.m_subrect_range.x().m_end
                  && M.m_subrect_range.y().m_begin != M.m_subrect_range.y().m_end)
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
        }
    }
}

/////////////////////////////////////////////////////////
// astral::Renderer::Implement::Filler::CurveClipper::Intersection methods
astral::Renderer::Implement::Filler::CurveClipper::Intersection::
Intersection(enum line_t tp, float ref_p, const ContourCurve &curve)
{
  int coord;
  float p1, p2, p3, A, B, C;
  float w, t1, t2, max_B_C;
  bool use_t1, use_t2;
  const float rel_quad_thresh(1e-6);

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

  /* Different from astral_banded_rays.glsl.resource_string
   * because we want always to exclude 0.0 and 1.0.
   */
  use_t1 = (p3 < 0.0f && t_max(p1, p2) > 0.0f) || (p1 > 0.0f && p2 < 0.0f);
  use_t2 = (p1 < 0.0f && t_max(p2, p3) > 0.0f) || (p3 > 0.0f && p2 < 0.0f);

  w = (curve.type() != ContourCurve::line_segment) ? curve.conic_weight() : 1.0f;
  A = p1 - (2.0f * w) * p2 + p3;
  B = p1 - w * p2;
  C = p1;
  max_B_C = t_max(t_abs(B), t_abs(C));

  /* Question: should we do relatively zero, or just zero? */
  if (curve.type() != ContourCurve::line_segment && t_abs(A) > rel_quad_thresh * max_B_C)
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

  /* a double root numerically, view as a no roots */
  if (use_t1 && use_t2 && t1 == t2)
    {
      use_t1 = use_t2 = false;
    }

  if (use_t1 && use_t2)
    {
      m_count = 2;
      m_data[0] = t_min(t1, t2);
      m_data[1] = t_max(t1, t2);
    }
  else if (use_t1 || use_t2)
    {
      m_count = 1;
      m_data[0] = (use_t1) ? t1 : t2;
    }
  else
    {
      m_count = 0;
    }
}

bool
astral::Renderer::Implement::Filler::CurveClipper::Intersection::
on_one_open_side(int coord, const ContourCurve &curve, float ref_p)
{
  float p1, p2, p3;
  bool use_t1, use_t2, negative_descr;

  p1 = curve.start_pt()[coord] - ref_p;
  p3 = curve.end_pt()[coord] - ref_p;
  if (curve.type() == ContourCurve::line_segment)
    {
      return (p1 < 0.0f && p3 < 0.0f) || (p1 > 0.0f && p3 > 0.0f);
    }

  p2 = curve.control_pt(0)[coord] - ref_p;
  negative_descr = (p2 * p2 < p1 * p3);
  use_t1 = (p3 < 0.0f && t_max(p1, p2) > 0.0f) || (p1 > 0.0f && p2 < 0.0f);
  use_t2 = (p1 < 0.0f && t_max(p2, p3) > 0.0f) || (p3 > 0.0f && p2 < 0.0f);

  return negative_descr || (!use_t1 && !use_t2 && p1 != 0.0f && p3 != 0.0f);
}

unsigned int
astral::Renderer::Implement::Filler::CurveClipper::Intersection::
light_rects(Filler::CurveClipper &filler, const ContourCurve &curve, enum line_t l, int v) const
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

      wf = curve.eval_at(m_data[i])[varying];
      R = filler.subrect_from_coordinate(wf, varying);

      for(; R.m_begin < R.m_end; ++R.m_begin)
        {
          rect_coord[varying] = R.m_begin;
          return_value += filler.subrect(rect_coord).light_rect();
        }
    }

  return return_value;
}

/////////////////////////////////////////////////////////
// astral::Renderer::Implement::Filler::CurveClipper::MappedCurve methods
astral::Renderer::Implement::Filler::CurveClipper::MappedCurve::
MappedCurve(Filler::CurveClipper &filler,
            const ContourCurve &curve,
            const Transformation &tr,
            const ContourCurve *prev):
  m_mapped_curve(curve, tr)
{
  if (prev)
    {
      m_mapped_curve.start_pt(tr.apply_to_point(prev->end_pt()));
    }

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

  /* Step 2. compute the intersections, recall that
   *         Filler::CurveClipper::m_intersection_backing is the
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
          filler.m_intersection_backing.push_back(Intersection(ll, filler.side_value(v, ss), m_mapped_curve));
        }
    }
}

unsigned int
astral::Renderer::Implement::Filler::CurveClipper::MappedCurve::
light_rects(Filler::CurveClipper &filler)
{
  unsigned int return_value(0u);
  vecN<range_type<int>, 2> r;

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
          return_value += filler.subrect(x, y).light_rect();
        }
    }

  r = filler.subrect_from_coordinate(m_mapped_curve.end_pt());
  for (int y = r.y().m_begin; y < r.y().m_end; ++y)
    {
      for (int x = r.x().m_begin; x < r.x().m_end; ++x)
        {
          return_value += filler.subrect(x, y).light_rect();
        }
    }

  /* Step 2: for each intersection, tag the rect of the intersection */
  for (int x = m_subrect_range.x().m_begin; x < m_subrect_range.x().m_end; ++x)
    {
      const Intersection &intersection0(get_intersection(filler, minx_side, x));
      const Intersection &intersection1(get_intersection(filler, maxx_side, x));

      return_value += intersection0.light_rects(filler, m_mapped_curve, x_fixed, x);
      return_value += intersection1.light_rects(filler, m_mapped_curve, x_fixed, x);
    }

  for (int y = m_subrect_range.y().m_begin; y < m_subrect_range.y().m_end; ++y)
    {
      const Intersection &intersection0(get_intersection(filler, miny_side, y));
      const Intersection &intersection1(get_intersection(filler, maxy_side, y));

      return_value += intersection0.light_rects(filler, m_mapped_curve, y_fixed, y);
      return_value += intersection1.light_rects(filler, m_mapped_curve, y_fixed, y);
    }

  return return_value;
}

const astral::Renderer::Implement::Filler::CurveClipper::Intersection&
astral::Renderer::Implement::Filler::CurveClipper::MappedCurve::
get_intersection(const Filler::CurveClipper &filler, enum side_t ss, int xy) const
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
// astral::Renderer::Implement::Filler::CurveClipper::MappedContour methods
astral::Renderer::Implement::Filler::CurveClipper::MappedContour::
MappedContour(Filler::CurveClipper &filler,
              c_array<const ContourCurve> contour,
              bool is_closed, const Transformation &tr):
  m_src_contour(nullptr),
  m_src_animated_contour(nullptr),
  m_src_animated_contour_time(-1.0f)
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
      filler.m_mapped_curve_backing.push_back(MappedCurve(filler, C, tr, prev));
      m_subrect_range.x().absorb(filler.m_mapped_curve_backing.back().m_subrect_range.x());
      m_subrect_range.y().absorb(filler.m_mapped_curve_backing.back().m_subrect_range.y());
      prev = &C;
    }

  m_curves.m_end = filler.m_mapped_curve_backing.size();

  ASTRALassert(curves(filler).empty() ||
               !points_different(curves(filler).front().m_mapped_curve.start_pt(),
                                 curves(filler).back().m_mapped_curve.end_pt()));
}

unsigned int
astral::Renderer::Implement::Filler::CurveClipper::MappedContour::
light_rects(Filler::CurveClipper &filler)
{
  unsigned int return_value(0u);

  for (int i = m_curves.m_begin; i < m_curves.m_end; ++i)
    {
      return_value += filler.m_mapped_curve_backing[i].light_rects(filler);
    }

  return return_value;
}

/////////////////////////////////////////////////////////
// astral::Renderer::Implement::Filler::CurveClipper::ClippedCurve methods
astral::Renderer::Implement::Filler::CurveClipper::ClippedCurve::
ClippedCurve(int curve, Filler::CurveClipper &filler):
  m_parent_curve(curve)
{
  ASTRALassert(m_parent_curve >= 0);
  ASTRALassert(m_parent_curve < static_cast<int>(filler.m_mapped_curve_backing.size()));

  const MappedCurve &src(filler.m_mapped_curve_backing[m_parent_curve]);

  m_curve.first = src.m_mapped_curve;
  m_curve.second = true;
}

astral::Renderer::Implement::Filler::CurveClipper::ClippedCurve::
ClippedCurve(const ContourCurve &clipped_curve, bool hugs_boundary):
  m_parent_curve(-1),
  m_curve(clipped_curve, !hugs_boundary)
{
}

astral::Renderer::Implement::Filler::CurveClipper::Intersection
astral::Renderer::Implement::Filler::CurveClipper::ClippedCurve::
intersection(Filler::CurveClipper &filler, enum side_t clip_side, int R) const
{
  float v;
  enum line_t ll;

  ll = line_t_from_side_t(clip_side);
  v = filler.side_value(R, clip_side);

  if (m_parent_curve >= 0)
    {
      ASTRALassert(m_parent_curve < static_cast<int>(filler.m_mapped_curve_backing.size()));

      /* get the curve */
      const MappedCurve &src(filler.m_mapped_curve_backing[m_parent_curve]);
      return src.get_intersection(filler, clip_side, R);
    }
  else
    {
      return Intersection(ll, v, m_curve.first);
    }
}

bool
astral::Renderer::Implement::Filler::CurveClipper::ClippedCurve::
is_cancelling_edge(const ClippedCurve &rhs) const
{
  if (m_curve.first.type() != rhs.m_curve.first.type())
    {
      return false;
    }

  if (m_curve.first.end_pt() != rhs.m_curve.first.start_pt())
    {
      return false;
    }

  if (m_curve.first.start_pt() != rhs.m_curve.first.end_pt())
    {
      return false;
    }

  if (m_curve.first.type() == ContourCurve::line_segment)
    {
      return true;
    }

  return rhs.m_curve.first.control_pt(0) == m_curve.first.control_pt(0)
    && rhs.m_curve.first.conic_weight() == m_curve.first.conic_weight();
}

//////////////////////////////////////////////////////////
// astral::Renderer::Implement::Filler::CurveClipper::ClippedContourBuilder methods
void
astral::Renderer::Implement::Filler::CurveClipper::ClippedContourBuilder::
add_curve_impl(const ClippedCurve &c)
{
  if (!m_dst->empty() && points_different(m_dst->back().end_pt(), c.start_pt()))
    {
      /* if a curve start exactly on the clipping line and leaves
       * from the region to return back and then re-enters, the
       * clipper clips the first segment, but the previous curve
       * ends where the started. We (without warning) will add
       * an edge hugger is the end point of the previous curve
       * and the start point of the new curve are on the boundary
       */
      if (m_dst->back().end_pt()[m_fc] == m_R_value
          && c.start_pt()[m_fc] == m_R_value)
        {
          CLIP_ERROR_LOG(m_clip_log, "Adding missing edge hugger " << m_dst->back().end_pt() << " vs " << c.start_pt() << "while clipping side = " << label(m_clip_side) << ", R = " << m_R << "@" << m_R_value);
          add_edge_hugger(c);
        }
      else
        {
          CLIP_ERROR_LOG(m_clip_log, "Warning: forced a match " << m_dst->back().end_pt() << " vs " << c.start_pt() << "while clipping side = " << label(m_clip_side) << ", R = " << m_R << "@" << m_R_value);

          ContourCurve C(m_dst->back().end_pt(), c.start_pt(), ContourCurve::not_continuation_curve);
          m_dst->push_back(ClippedCurve(C, true));
        }
    }

  if (!m_dst->empty() && m_dst->back().is_cancelling_edge(c))
    {
      m_dst->pop_back();
    }
  else if (c.curve().type() != ContourCurve::line_segment || c.start_pt() != c.end_pt())
    {
      m_dst->push_back(c);
    }
}

void
astral::Renderer::Implement::Filler::CurveClipper::ClippedContourBuilder::
add_edge_hugger(const ClippedCurve &curve)
{
  ASTRALassert(!m_dst->empty());

  if (m_dst->back().end_pt() != curve.start_pt())
    {
      if (!edge_hugger_detected(curve))
        {
          CLIP_ERROR_LOG(m_clip_log, "EdgeHugger requested to connect back().end_pt() = "
                         << m_dst->back().end_pt() << " to curve.start_pt() = "
                         << curve.start_pt() << " is not an exact edge hugger on coordinate idx = "
                         << m_fc << " with value " << m_R_value);
        }

      ContourCurve C(m_dst->back().end_pt(), curve.start_pt(), ContourCurve::not_continuation_curve);
      m_dst->push_back(ClippedCurve(C, true));
    }
}

void
astral::Renderer::Implement::Filler::CurveClipper::ClippedContourBuilder::
add_curve(bool new_curve_clipped, const ClippedCurve &c)
{
  if (!m_dst->empty() && m_prev_clipped)
    {
      add_edge_hugger(c);
    }

  add_curve_impl(c);
  m_prev_clipped = new_curve_clipped;
}

void
astral::Renderer::Implement::Filler::CurveClipper::ClippedContourBuilder::
close_clipping_contour(void)
{
  /* we need to make an edge hugger from dst->back() to dst->front() */
  ASTRALassert(!m_dst->empty());

  /* the need for a closing curve is a little icky to test;
   * basically, a closing curve is needed if the first curve
   * added was partially clipped which means, in exact arithmatic,
   * if the first of the input contour was partially or completely
   * clipped.
   *
   * Chances are we would be best to just add a closing curve always
   * that is never anti-aliased.
   */
  if (points_different(m_dst->back().end_pt(), m_dst->front().start_pt()))
    {
      if (m_prev_clipped || m_first_element_clipped || edge_hugger_detected(m_dst->front()))
        {
          if (!m_prev_clipped && !m_first_element_clipped)
            {
              CLIP_ERROR_LOG(m_clip_log, "Warning: adding closing edge hugger although flags do not indicate to do so");
            }

          add_edge_hugger(m_dst->front());
        }
      else
        {
          CLIP_ERROR_LOG(m_clip_log, "Warning: contour forced to be closed " << label(m_clip_side) << ", R = " << m_R << "@" << m_filler.side_value(m_R, m_clip_side));

          /* chances are the added curve should be an edge-hugger
           * but numerical round-off prevents us from seeing that,
           * so it should not give anti-aliasing and classified
           * as an edge-hugger.
           */
          ContourCurve C(m_dst->back().end_pt(), m_dst->front().start_pt(), ContourCurve::not_continuation_curve);
          add_curve_impl(ClippedCurve(C, true));
        }
    }
}

void
astral::Renderer::Implement::Filler::CurveClipper::ClippedContourBuilder::
clip_curve(const ClippedCurve &curve)
{
  clip_curve_implement(curve);
  if (m_prev_clipped && m_num_curves_processed == 0)
    {
      m_first_element_clipped = true;
    }
  ++m_filler.m_renderer.m_stats[number_sparse_fill_curves_clipped];
  ++m_num_curves_processed;
}

void
astral::Renderer::Implement::Filler::CurveClipper::ClippedContourBuilder::
clip_curve_implement(const ClippedCurve &curve)
{
  if (Intersection::on_one_open_side(m_fc, curve.curve(), m_R_value))
    {
      /* Curve is either competely clipped or unclipped because
       * the clipping line does not intersect the curve; The
       * question we need to answer is "on what side of the
       * clip-line is the curve?" When Intersection::on_one_open_side()
       * returns false, it means there is a hard guarantee
       * that the entire curve does not intersect the clipping
       * line, thus we can choose any point to do the job,
       * so we take the start point.
       */

      if (inside_region(curve.start_pt()))
        {
          add_curve(false, curve);
        }
      else
        {
          m_prev_clipped = true;
        }

      return;
    }

  if (curve.curve().type() == ContourCurve::line_segment
      && curve.start_pt()[1 - m_fc] == curve.end_pt()[1 - m_fc])
    {
      vec2 s, e, p, m;
      bool useS;

      s = curve.start_pt();
      e = curve.end_pt();

      p[m_fc] = m_R_value;
      p[1 - m_fc] = s[1 - m_fc];

      ContourCurve S(s, p, ContourCurve::continuation_curve);
      ClippedCurve SS(S, curve.hugs_boundary());

      ContourCurve E(p, e, ContourCurve::continuation_curve);
      ClippedCurve EE(E, curve.hugs_boundary());

      /* when choosing the test point m, use the side
       * which has the greatest spread
       */
      if (t_abs(p[m_fc] - s[m_fc]) > t_abs(p[m_fc] - e[m_fc]))
        {
          m = 0.5f * (p + s);
          useS = inside_region(m);
        }
      else
        {
          m = 0.5f * (p + e);
          useS = !inside_region(m);
        }

      if (useS)
        {
          add_curve(true, SS);
        }
      else
        {
          add_curve(false, EE);
        }

      return;
    }

  Intersection intersection;
  c_array<const float> data;

  /* get the intersection for the named boundary */
  intersection = curve.intersection(m_filler, m_clip_side, m_R);
  data = intersection.data();

  /* Remove t = 0.0 solutions; this is ugly. The
   * Intersection ctor has a numerically stable way
   * to check if a solution is in the open interval
   * (0, 1), but the actual t-value might be off
   * numerically because it comes a computation
   * involving sqrt and divide; for now, we just
   * murder the solution if it is less than or equal
   * to zero.
   */
  if (!data.empty() && data.front() <= 0.0f)
    {
      data.pop_front();
    }

  /* Remove t = 1.0 solutions; this is also ugly
   * for the same reasons that removing the t <= 0
   * solutions are.
   */
  if (!data.empty() && data.back() >= 1.0f)
    {
      data.pop_back();
    }

  /* clip the curve against the intersection(s). In exact arithmatic,
   * a complete lack of such intersections means that the curve is
   * entirely unclipped (we can rule our entirely unclipped because
   * of the first if() in the chain. However, the vagaries of round-off
   * coupled with the igly filtering of t's makes things more amusing.
   */
  ASTRALassert(data.size() <= 2u);

  if (data.empty())
    {
      /* After throwing away t's too close to zero or one,
       * it is completely clipped or uncliped. Use the center
       * of the tight bounding box to make a descision.
       *
       * Perhaps we could rely on the status of the previous clipping?
       */
      ++m_filler.m_renderer.m_stats[number_sparse_fill_awkward_fully_clipped_or_unclipped];
      const BoundingBox<float> &bb(curve.curve().tight_bounding_box());
      vec2 p;

      ASTRALassert(!bb.empty());
      p = 0.5f * (bb.min_point() + bb.max_point());
      if (inside_region(p))
        {
          add_curve(false, curve);
        }
      else
        {
          m_prev_clipped = true;
        }

      return;
    }

  if (data.size() == 2u)
    {
      ASTRALassert(curve.curve().number_control_pts() == 1u);
      ASTRALassert(!curve.hugs_boundary());

      /* curve intersects the clip-line twice which means we either
       *  A) add the curve restricted to [data[0], data[1]] OR
       *  B) add the the curves
       *       i) [0.0, data[0]]
       *       ii) edge hugger from connecting i) to iii)
       *       iii) [data[1], 1.0]
       *
       * regardless of which, we will split the curve at the
       * intersection points.
       */
      ContourCurveSplit S0(false, curve.curve(), data[0]);
      S0.force_coordinate(ContourCurveSplit::Coordinate(m_fc), m_R_value);

      float ss;
      ss = (data[1] - data[0]) / (1.0f - data[0]);
      ContourCurveSplit S1(false, S0.after_t(), ss);
      S1.force_coordinate(ContourCurveSplit::Coordinate(m_fc), m_R_value);

      /* we need to decide if we have (A) or (B) in a reliable way;
       * we compute the centers of each of the three boudning boxes
       * and which ever is furthest from the line is the one that
       * chooses.
       *
       * Perhaps we can decide from the status of the previous curve?
       */
      BoundingBox<float> S0before_bb(S0.before_t().tight_bounding_box());
      BoundingBox<float> S1before_bb(S1.before_t().tight_bounding_box());
      BoundingBox<float> S1after_bb(S1.after_t().tight_bounding_box());
      vec2 p0_before, p1_before, p1_after;
      float d0_before, d1_before, d1_after;
      bool is_case_A;

      p0_before = 0.5f * (S0before_bb.min_point() + S0before_bb.max_point());
      p1_before = 0.5f * (S1before_bb.min_point() + S1before_bb.max_point());
      p1_after = 0.5f * (S1after_bb.min_point() + S1after_bb.max_point());

      d0_before = t_abs(m_R_value - p0_before[m_fc]);
      d1_before = t_abs(m_R_value - p1_before[m_fc]);
      d1_after = t_abs(m_R_value - p1_after[m_fc]);

      if (d0_before > t_max(d1_before, d1_after))
        {
          is_case_A = !inside_region(p0_before);
        }
      else if (d1_before > d1_after)
        {
          is_case_A = inside_region(p1_before);
        }
      else
        {
          is_case_A = !inside_region(p1_after);
        }

      if (is_case_A)
        {
          ClippedCurve C(S1.before_t(), false);
          add_curve(true, C);
          return;
        }
      else
        {
          ClippedCurve A(S0.before_t(), false);
          ClippedCurve B(S1.after_t(), false);

          // add A
          add_curve(true, A);

          // A exited the region and add B; add_curve()
          // automatically adds the necessary edge hugger
          add_curve(true, B);
          return;
        }
    }

  if (data.size() == 1u)
    {
      ASTRALassert(data.size() == 1u);

      ContourCurveSplit S(false, curve.curve(), data[0]);
      S.force_coordinate(ContourCurveSplit::Coordinate(m_fc), m_R_value);

      /* curve intersects the clip-line once at data[0] which means one of:
       *  A) add the curve restricted to [0.0, data[0]] OR
       *  B) add the curve restricted to [data[0], 1.0]
       *
       * We compute the cennters of the bounding box of S.before_t()
       * and S.after_t() and use which ever is furthers from the
       * clipping line
       *
       * Perhaps we can decide from the status of the previous curve?
       */
      BoundingBox<float> Sbefore_bb(S.before_t().tight_bounding_box());
      BoundingBox<float> Safter_bb(S.after_t().tight_bounding_box());
      vec2 p_before, p_after;
      float d_before, d_after;
      bool is_case_A;

      p_before = 0.5f * (Sbefore_bb.min_point() + Sbefore_bb.max_point());
      p_after = 0.5f * (Safter_bb.min_point() + Safter_bb.max_point());

      d_before = t_abs(m_R_value - p_before[m_fc]);
      d_after = t_abs(m_R_value - p_after[m_fc]);

      if (d_before > d_after)
        {
          is_case_A = inside_region(p_before);
        }
      else
        {
          is_case_A = !inside_region(p_after);
        }

      if (is_case_A)
        {
          /* add the curve [m_param_range.m_begin, data[I]] */
          ClippedCurve M(S.before_t(), curve.hugs_boundary());

          add_curve(true, M);
          return;
        }
      else
        {
          ClippedCurve M(S.after_t(), curve.hugs_boundary());

          add_curve(false, M);
          return;
        }
    }

  ASTRALassert(!"Should never reach here");
}

/////////////////////////////////////////////////////////
// astral::Renderer::Implement::Filler methods
astral::reference_counted_ptr<const astral::Image>
astral::Renderer::Implement::Filler::CurveClipper::
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

  if (map_contours_and_light_rects(path))
    {
      /* build the Image; those rects with no curve intersecting
       * get realized as fully covered or not-covered; those with curves
       * get the STC groove.
       */
      #warning "restrict_bbs should be used by Filler::CurveClipper::build_sparse_image()"
      return_value = build_sparse_image(clip_element, clip_combine_mode, out_clip_combine_tile_data);
    }

  cleanup();
  return return_value;
}

void
astral::Renderer::Implement::Filler::CurveClipper::
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
astral::Renderer::Implement::Filler::CurveClipper::
create_subrects(ivec2 mask_size, const ClipElement *clip_element,
                c_array<const BoundingBox<float>> restrict_bbs)
{
  unsigned int num;
  bool default_value_skip_rect;

  ASTRALassert(m_elementary_rects.empty());
  set_subrect_values(mask_size, clip_element);

  default_value_skip_rect = !restrict_bbs.empty();
  num = number_subrects();
  m_elementary_rects.clear();
  m_elementary_rects.resize(num, SubRect(default_value_skip_rect));

  if (default_value_skip_rect)
    {
      for (const BoundingBox<float> &bb : restrict_bbs)
        {
          vecN<range_type<int>, 2> rect_ids;

          rect_ids = subrect_range_from_coordinate(bb.min_point(), bb.max_point());
          for (int y = rect_ids.y().m_begin; y < rect_ids.y().m_end; ++y)
            {
              for (int x = rect_ids.x().m_begin; x < rect_ids.x().m_end; ++x)
                {
                  subrect(x, y).m_skip_rect = false;
                }
            }
        }
    }

  if (clip_element)
    {
      for (int y = 0; y < m_number_elementary_rects.y(); ++y)
        {
          for (int x = 0; x < m_number_elementary_rects.x(); ++x)
            {
              subrect(x, y).m_skip_rect = subrect(x, y).m_skip_rect || clip_element->empty_tile(ivec2(x, y));
            }
        }
    }
}

bool
astral::Renderer::Implement::Filler::CurveClipper::
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
astral::Renderer::Implement::Filler::CurveClipper::
create_clipped_contour(const MappedContour &contour,
                       std::vector<ClippedCurve> *out_contour)
{
  out_contour->clear();
  for (int i = contour.m_curves.m_begin; i < contour.m_curves.m_end; ++i)
    {
      out_contour->push_back(ClippedCurve(i, *this));
    }
}

void
astral::Renderer::Implement::Filler::CurveClipper::
print_clipped_contour(c_array<const ClippedCurve> contour, int tab, std::ostream &str)
{
  std::string prefix(tab, '\t');
  for (unsigned int i = 0; i < contour.size(); ++i)
    {
      const auto &curve(contour[i]);
      const auto &v(curve.as_contour());

      str << prefix << "#" << i << ":" << v.first
          << ", bb = " << v.first.tight_bounding_box().as_rect()
          << ", draw = " << std::boolalpha << v.second
          << ", hugs = " << curve.hugs_boundary()
          << "\n";
    }
}

bool
astral::Renderer::Implement::Filler::CurveClipper::
all_are_edge_huggers(c_array<const ClippedCurve> contour)
{
  for (const auto &c : contour)
    {
      if (!c.hugs_boundary())
        {
          return false;
        }
    }

  return true;
}

astral::c_array<const astral::Renderer::Implement::Filler::CurveClipper::ClippedCurve>
astral::Renderer::Implement::Filler::CurveClipper::
clip_contour(c_array<const ClippedCurve> in_contour,
             enum side_t side, int box_row_col,
             ClipLog &clip_log,
             std::vector<ClippedCurve> *workroom)
{
  if (in_contour.empty())
    {
      return c_array<const ClippedCurve>();
    }

  ++m_renderer.m_stats[number_sparse_fill_contours_clipped];

  ClippedContourBuilder builder(*this, clip_log, in_contour, side, box_row_col, workroom);
  for (const auto &C : in_contour)
    {
      builder.clip_curve(C);
    }

  if (!workroom->empty())
    {
      builder.close_clipping_contour();
    }

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

void
astral::Renderer::Implement::Filler::CurveClipper::
process_mapped_contour(const MappedContour &contour)
{
  c_array<const ClippedCurve> current;
  ClipLog clip_log(m_renderer, contour);

  ASTRALassert(contour.m_subrect_range.x().m_begin < contour.m_subrect_range.x().m_end);

  unsigned int cnt(contour.m_subrect_range.x().difference() * contour.m_subrect_range.x().difference());
  m_renderer.m_stats[number_sparse_fill_subrects_clipping] += cnt;

  /* Step 1: first realize the MappedContour as a
   *         clipped contour
   */
  create_clipped_contour(contour, &m_clipped_contourA[0]);
  current = make_c_array(m_clipped_contourA[0]);
  int work(1);

  /* Step 2: clip it against maxx_side on contour.m_subrect_range.x().m_end - 1 */
  current = clip_contour(current, maxx_side,
                         contour.m_subrect_range.x().m_end - 1,
                         clip_log, &m_clipped_contourA[work]);
  work = 1 - work;

  /* Step 3: clip it against minx_side on contour.m_subrect_range.x().m_begin */
  current = clip_contour(current, minx_side,
                         contour.m_subrect_range.x().m_begin,
                         clip_log, &m_clipped_contourA[work]);
  work = 1 - work;

  /* Step 4: clip it against maxy_side on contour.m_subrect_range.y().m_end - 1 */
  current = clip_contour(current, maxy_side,
                         contour.m_subrect_range.y().m_end - 1,
                         clip_log, &m_clipped_contourA[work]);
  work = 1 - work;

  /* Step 5: clip it against miny_side on contour.m_subrect_range.y().m_begin */
  current = clip_contour(current, miny_side,
                         contour.m_subrect_range.y().m_begin,
                         clip_log, &m_clipped_contourA[work]);
  work = 1 - work;

  if (all_are_edge_huggers(current))
    {
      ++m_renderer.m_stats[number_sparse_fill_contour_skip_clipping];
      process_subrects_all_edge_huggers(current, contour.m_subrect_range);
      return;
    }

#define EARLY_OUT(X, R) do {                            \
    if (all_are_edge_huggers(X))                        \
      {                                                 \
        process_subrects_all_edge_huggers(X, R);        \
        return;                                         \
      } } while(0)

  /* We clip from the outside in to give all_edge_huggers() a
   * better chance of being true.
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
      tmp = clip_contour(current, maxx_side, current_range.x().m_begin, clip_log, &m_clipped_contourA[work]);
      process_mapped_contour_column(tmp, clip_log, current_range.x().m_begin, current_range.y());

      /* remove the left column */
      ++current_range.x().m_begin;
      current = clip_contour(current, minx_side, current_range.x().m_begin, clip_log, &m_clipped_contourA[work]);
      work = 1 - work;
      EARLY_OUT(current, current_range);

      /* handle the right column */
      tmp = clip_contour(current, minx_side, current_range.x().m_end - 1, clip_log, &m_clipped_contourA[work]);
      process_mapped_contour_column(tmp, clip_log, current_range.x().m_end - 1, current_range.y());

      /* remove the right column */
      --current_range.x().m_end;
      current = clip_contour(current, maxx_side, current_range.x().m_end - 1, clip_log, &m_clipped_contourA[work]);
      work = 1 - work;
      EARLY_OUT(current, current_range);

      /* handle the top row */
      tmp = clip_contour(current, maxy_side, current_range.y().m_begin, clip_log, &m_clipped_contourA[work]);
      process_mapped_contour_row(tmp, clip_log, current_range.y().m_begin, current_range.x());

      /* remove the top row */
      ++current_range.y().m_begin;
      current = clip_contour(current, miny_side, current_range.y().m_begin, clip_log, &m_clipped_contourA[work]);
      work = 1 - work;
      EARLY_OUT(current, current_range);

      /* handle the bottom row */
      tmp = clip_contour(current, miny_side, current_range.y().m_end - 1, clip_log, &m_clipped_contourA[work]);
      process_mapped_contour_row(tmp, clip_log, current_range.y().m_end - 1, current_range.x());

      /* remove the bottom row */
      --current_range.y().m_end;
      current = clip_contour(current, maxy_side, current_range.y().m_end - 1, clip_log, &m_clipped_contourA[work]);
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
      tmp = clip_contour(current, maxx_side, current_range.x().m_begin, clip_log, &m_clipped_contourA[work]);
      process_mapped_contour_column(tmp, clip_log, current_range.x().m_begin, current_range.y());

      /* remove the left column */
      ++current_range.x().m_begin;
      current = clip_contour(current, minx_side, current_range.x().m_begin, clip_log, &m_clipped_contourA[work]);
      work = 1 - work;
      EARLY_OUT(current, current_range);

      /* handle the right column */
      tmp = clip_contour(current, minx_side, current_range.x().m_end - 1, clip_log, &m_clipped_contourA[work]);
      process_mapped_contour_column(tmp, clip_log, current_range.x().m_end - 1, current_range.y());

      /* remove the right column */
      --current_range.x().m_end;
      current = clip_contour(current, maxx_side, current_range.x().m_end - 1, clip_log, &m_clipped_contourA[work]);
      work = 1 - work;
      EARLY_OUT(current, current_range);
    }

  /* now cut off from the bottom and top */
  while (current_range.y().difference() > 2)
    {
      c_array<const ClippedCurve> tmp;

      /* handle the top row */
      tmp = clip_contour(current, maxy_side, current_range.y().m_begin, clip_log, &m_clipped_contourA[work]);
      process_mapped_contour_row(tmp, clip_log, current_range.y().m_begin, current_range.x());

      /* remove the top row */
      ++current_range.y().m_begin;
      current = clip_contour(current, miny_side, current_range.y().m_begin, clip_log, &m_clipped_contourA[work]);
      work = 1 - work;
      EARLY_OUT(current, current_range);

      /* handle the bottom row */
      tmp = clip_contour(current, miny_side, current_range.y().m_end - 1, clip_log, &m_clipped_contourA[work]);
      process_mapped_contour_row(tmp, clip_log, current_range.y().m_end - 1, current_range.x());

      /* remove the bottom row */
      --current_range.y().m_end;
      current = clip_contour(current, maxy_side, current_range.y().m_end - 1, clip_log, &m_clipped_contourA[work]);
      work = 1 - work;
      EARLY_OUT(current, current_range);
    }

  /* at this point the number of box rows and box columns
   * is less than three, we just walk the columns instead
   */
  for (int i = current_range.x().m_begin; i < current_range.x().m_end; ++i)
    {
      /* at entry we have that current is clipped against (minx_side, i).
       * To clip to the column requires that it is clipped against (maxx_side, i)
       */
      if (i + 1 != current_range.x().m_end)
        {
          c_array<const ClippedCurve> tmp;

          /* Clip it against (maxx_side, i) */
          tmp = clip_contour(current, maxx_side, i, clip_log, &m_clipped_contourA[work]);
          process_mapped_contour_column(tmp, clip_log, i, current_range.y());

          /* The next iteration reqires the contour to be clipped against (minx_side, i + 1) */
          current = clip_contour(current, minx_side, i + 1, clip_log, &m_clipped_contourA[work]);
          work = 1 - work;
        }
      else
        {
          /* we are on the last column, it was already cliped against (maxx_side, i) */
          process_mapped_contour_column(current, clip_log, i, current_range.y());
        }
    }
}

void
astral::Renderer::Implement::Filler::CurveClipper::
process_mapped_contour_row(c_array<const ClippedCurve> current,
                           ClipLog &clip_log, int box_row,
                           range_type<int> box_col_range)
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

  for (int j = box_col_range.m_begin; j < box_col_range.m_end; ++j)
    {
      /* at entry we have that current is clipped against (miny_side, j).
       * To clip to the box requires that it is clipped against (maxy_side, j)
       */
      if (j + 1 != box_col_range.m_end)
        {
          if (all_are_edge_huggers(current))
            {
              vecN<range_type<int>, 2> boxes;

              boxes.x().m_begin = j;
              boxes.x().m_end = box_col_range.m_end;

              boxes.y().m_begin = box_row;
              boxes.y().m_end = box_row + 1;

              process_subrects_all_edge_huggers(current, boxes);
              return;
            }

          c_array<const ClippedCurve> tmp;

          /* Clip it against (maxy_side, j) */
          tmp = clip_contour(current, maxx_side, j, clip_log, &m_clipped_contourB[work]);
          process_subrect(tmp, j, box_row);

          /* for the next iteration, clip it against (miny_side, j + 1) */
          current = clip_contour(current, minx_side, j + 1, clip_log, &m_clipped_contourB[work]);
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
astral::Renderer::Implement::Filler::CurveClipper::
process_mapped_contour_column(c_array<const ClippedCurve> current,
                              ClipLog &clip_log, int box_col,
                              range_type<int> box_row_range)
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

  for (int j = box_row_range.m_begin; j < box_row_range.m_end; ++j)
    {
      /* at entry we have that current is clipped against (miny_side, j).
       * To clip to the box requires that it is clipped against (maxy_side, j)
       */
      if (j + 1 != box_row_range.m_end)
        {
          if (all_are_edge_huggers(current))
            {
              vecN<range_type<int>, 2> boxes;

              boxes.x().m_begin = box_col;
              boxes.x().m_end = box_col + 1;

              boxes.y().m_begin = j;
              boxes.y().m_end = box_row_range.m_end;

              process_subrects_all_edge_huggers(current, boxes);
              return;
            }

          c_array<const ClippedCurve> tmp;

          /* Clip it against (maxy_side, j) */
          tmp = clip_contour(current, maxy_side, j, clip_log, &m_clipped_contourB[work]);
          process_subrect(tmp, box_col, j);

          /* for the next iteration, clip it against (miny_side, j + 1) */
          current = clip_contour(current, miny_side, j + 1, clip_log, &m_clipped_contourB[work]);
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
astral::Renderer::Implement::Filler::CurveClipper::
process_subrects_all_edge_huggers(c_array<const ClippedCurve> contour,
                                  const vecN<range_type<int>, 2> &boxes)
{
  ASTRALassert(boxes.x().m_begin < boxes.x().m_end);
  ASTRALassert(boxes.y().m_begin < boxes.y().m_end);

  unsigned int cnt(boxes.x().difference() * boxes.y().difference());
  int winding_offset(0);
  vec2 min_p(minx_side_value(boxes.x().m_begin), miny_side_value(boxes.y().m_begin));
  vec2 max_p(maxx_side_value(boxes.x().m_end - 1), maxy_side_value(boxes.y().m_end - 1));
  vec2 center_p;

  m_renderer.m_stats[number_sparse_fill_subrect_skip_clipping] += cnt;

  center_p = 0.5f * (min_p + max_p);
  for (const auto &curve : contour)
    {
      ASTRALassert(curve.hugs_boundary());
      if (curve.start_pt().x() > center_p.x() && curve.end_pt().x() > center_p.x())
        {
          /* The winding effect MUST follow the convention that
           * is used in Renderer: Clockwise increments the winding
           * number and CounterClockwise decrements where the
           * y-axis increases downwardly.
           */
          if (curve.start_pt().y() < center_p.y() && center_p.y() < curve.end_pt().y())
            {
              ++winding_offset;
            }
          else if (curve.start_pt().y() > center_p.y() && center_p.y() > curve.end_pt().y())
            {
              --winding_offset;
            }
        }
    }

  for (int y = boxes.y().m_begin; y < boxes.y().m_end; ++y)
    {
      for (int x = boxes.x().m_begin; x < boxes.x().m_end; ++x)
        {
          subrect(x, y).m_winding_offset += winding_offset;
        }
    }
}

void
astral::Renderer::Implement::Filler::CurveClipper::
process_subrect(c_array<const ClippedCurve> contour, int box_col, int box_row)
{
  SubRect &SR(subrect(box_col, box_row));

  if (SR.m_skip_rect)
    {
      return;
    }

  /* first see if all curves of contour are edge huggers */
  bool all_edge_huggers(true);
  int winding_offset(0);
  vec2 min_p(minx_side_value(box_col), miny_side_value(box_row));
  vec2 max_p(maxx_side_value(box_col), maxy_side_value(box_row));
  vec2 center_p;

  center_p = 0.5f * (min_p + max_p);
  for (unsigned int i = 0; i < contour.size() && all_edge_huggers; ++i)
    {
      const auto &curve(contour[i]);

      all_edge_huggers = all_edge_huggers && curve.hugs_boundary();
      if (all_edge_huggers
          && curve.start_pt().x() > center_p.x()
          && curve.end_pt().x() > center_p.x())
        {
          /* if the curve is an edge hugger, then it is to the right of p */
          if (curve.start_pt().y() < center_p.y() && center_p.y() < curve.end_pt().y())
            {
              ++winding_offset;
            }
          else if (curve.start_pt().y() > center_p.y() && center_p.y() > curve.end_pt().y())
            {
              --winding_offset;
            }
        }
    }

  if (all_edge_huggers)
    {
      SR.m_winding_offset += winding_offset;
    }
  else
    {
      /* The original lighting of rects is done on the mapped
       * curves; however as we clip the curves, numerical error
       * might push a curve that was near the boundary of a
       * rect over; in order to keep what is rendered consistent,
       * we have a secondary lighting done by the clipped contours,
       * stored on m_curves_added. When a rect is first lit this
       * way, we create the VirtualBuffer and the STCData object
       */
      if (!SR.m_curves_added)
        {
          ivec2 size;

          ASTRALassert(!SR.m_encoder.valid());
          ASTRALassert(!SR.m_stc_builder);

          /* We gain nothing by making the image on demand, since the image is exactly
           * one tile. In addition, the assert code to make sure the image size and
           * tile count is correct, need the backing image to be made immediately.
           *
           * In addition, later logic also requires that the backing images are also
           * ready.
           */
          size = ivec2(ImageAtlas::tile_size);
          SR.m_curves_added = true;
          SR.m_encoder = m_renderer.m_storage->create_virtual_buffer(VB_TAG, size, m_fill_rule,
                                                                     VirtualBuffer::ImageCreationSpec()
                                                                     .create_immediately(true)
                                                                     .default_use_prepadding_true(true));

          ASTRALassert(SR.m_encoder.virtual_buffer().fetch_image());
          ASTRALassert(SR.m_encoder.virtual_buffer().fetch_image()->mip_chain().size() == 1);
          ASTRALassert(SR.m_encoder.virtual_buffer().fetch_image()->mip_chain().front()->number_elements(ImageMipElement::empty_element) == 0u);
          ASTRALassert(SR.m_encoder.virtual_buffer().fetch_image()->mip_chain().front()->number_elements(ImageMipElement::white_element) == 0u);
          ASTRALassert(SR.m_encoder.virtual_buffer().fetch_image()->mip_chain().front()->number_elements(ImageMipElement::color_element) == 1u);

          Transformation tr;

          /* we need the transformation that maps (minx_side(), miny_side()) to (0, 0) */
          tr.translate(-minx_side_value(box_col), -miny_side_value(box_row));
          SR.m_tr = SR.m_encoder.create_value(tr);

          SR.m_stc_builder = stc_builder_for_rect(box_col, box_row);
        }

      add_stc_data(SR.m_stc_builder, SR.m_tr, contour);
    }
}

void
astral::Renderer::Implement::Filler::CurveClipper::
add_stc_data(STCData::BuilderSet *stc_builder,
             RenderValue<Transformation> tr,
             c_array<const ClippedCurve> contour)
{
  vecN<range_type<unsigned int>, FillSTCShader::pass_count> vert_blocks;

  /* use m_builder to build the STCData */
  ASTRALassert(tr.valid());
  ASTRALassert(m_item_data.valid());
  ASTRALassert(m_builder.empty());
  ASTRALassert(m_builder_helper.empty());

  for (const auto &curve : contour)
    {
      m_builder_helper.push_back(curve.as_contour());
    }

  /* FillSTCShader::Data::add_contouor() automatically closes
   * the contour; since the input contpur is already closed
   * then that closing edge does not need anti-aliasing.
   */
  m_builder.add_contour(make_c_array(m_builder_helper), false);
  create_blocks_from_builder(m_aa_mode, &vert_blocks);

  for (unsigned int pass = 0; pass < FillSTCShader::pass_count; ++pass)
    {
      enum FillSTCShader::pass_t pass_t;
      auto blocks(m_renderer.m_vertex_streamer->blocks(vert_blocks[pass]));

      pass_t = static_cast<enum FillSTCShader::pass_t>(pass);
      for (const auto &block : blocks)
        {
          ASTRALassert(block.m_object);
          ASTRALassert(!block.m_dst.empty());
          stc_builder->add_stc_pass(pass_t, block.m_object,
                                    range_type<int>(block.m_offset, block.m_offset + block.m_dst.size()),
                                    tr, m_item_data);
        }
    }

  // cleanup
  m_builder.clear();
  m_builder_helper.clear();
}

astral::reference_counted_ptr<const astral::Image>
astral::Renderer::Implement::Filler::CurveClipper::
build_sparse_image(const ClipElement *clip_element,
                   enum clip_combine_mode_t clip_combine_mode,
                   TileTypeTable *out_clip_combine_tile_data)
{
  /* process_mapped_contour() will give those SubRects that
   * have contours going thorigh them a RenderEncoderBase.
   * In addition, if a contour C clipped against a SubRect R
   * is only edge huggers, then R.m_winding_offset will get
   * incremented/decremented by the effect of C on R's
   * winding number. Lastly, if a contour C clipped against
   * R does have curves, then process_mapped_contour() adds
   * the STC data to R's VirtualBuffer.
   *
   * At the end, if the base fill rule is odd-even, for each
   * SubRect if the m_winding_offset is odd, then take the
   * inverse fill-rule for its RenderEncoderBase. If the base
   * fill rule is non-zero, add m_winding_offset rects of the
   * correct orientation to the STCData of that RenderEncoderBase
   */

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

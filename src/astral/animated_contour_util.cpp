/*!
 * \file animated_contour_util.cpp
 * \brief file animated_contour_util.cpp
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

#include <algorithm>
#include "animated_contour_util.hpp"

//////////////////////////////////
// astral::detail methods
float
astral::detail::
approximate_length(const ContourCurve &C)
{
  switch (C.type())
    {
    case ContourCurve::cubic_bezier:
      return (C.end_pt() - C.control_pt(1)).magnitude()
        + (C.control_pt(1) - C.control_pt(0)).magnitude()
        + (C.control_pt(0) - C.start_pt()).magnitude();

    /* The right thing would be to compute with
     * a nasty integral the length of the quadratic
     * curve, but just adding the distance to the
     * control points is not that far off.
     * With conics, we should also let the weight
     * play some role.
     */
    case ContourCurve::quadratic_bezier:
    case ContourCurve::conic_curve:
      return (C.end_pt() - C.control_pt(0)).magnitude()
        + (C.control_pt(0) - C.start_pt()).magnitude();

    case ContourCurve::conic_arc_curve:
      return C.arc_radius() * astral::t_abs(C.arc_angle());

    case ContourCurve::line_segment:
      return (C.end_pt() - C.start_pt()).magnitude();

    default:
      return 0.0f;
    }
}

float
astral::detail::
approximate_lengths(c_array<const ContourCurve> contour,
                    c_array<float> out_lengths)
{
  float return_value(0.0f);

  ASTRALassert(contour.size() == out_lengths.size());
  for (unsigned int E = 0; E < contour.size(); ++E)
    {
      float d;

      d = approximate_length(contour[E]);
      out_lengths[E] = d;
      return_value += d;
    }

  return return_value;
}

//////////////////////////////////////////////
// astral::detail::SimplifiedContour methods
astral::detail::SimplifiedContour::
SimplifiedContour(c_array<const ContourCurve> C,
                  c_array<const float> L)
{
  if (C.empty())
    {
      ASTRALassert(L.empty());
      m_contour_length = 0.0f;
      m_start_pt = astral::vec2(0.0f, 0.0f);
    }
  else
    {
      float length_from_start(0.0f);
      unsigned int num_edges(C.size());

      ASTRALassert(L.size() == num_edges);
      m_start_pt = C[0].start_pt();

      for (unsigned int E = 0; E < num_edges; ++E)
        {
          if (L[E] > 0.0f)
            {
              Edge e(C[E], L[E], length_from_start);

              m_edges.push_back(e);
              length_from_start += L[E];
            }
        }
      m_contour_length = length_from_start;
    }
}

////////////////////////////////////////////////////////////////////
// astral::detail::ContourCommonPartitioner::InputPointInfo methods
void
astral::detail::ContourCommonPartitioner::InputPointInfo::
add_pts(std::vector<InputPointInfo> &dst, enum point_src_t tp,
        const SimplifiedContour &input)
{
  InputPointInfo P;
  float recip(1.0f / input.contour_length());

  /* NOTE: we -deliberatly- do NOT include the start point */
  P.m_src = tp;
  for (unsigned int E = 0; E < input.size(); ++E)
    {
      P.m_idx = E;
      P.m_rel_length = recip * input[E].length_from_contour_start_to_edge_end();
      dst.push_back(P);
    }
  /* remove the end point to make sure that when the array are merged it is not there twice */
  dst.pop_back();
}

////////////////////////////////////////////////////////////////////
// astral::detail::ContourCommonPartitioner methods
astral::detail::ContourCommonPartitioner::
ContourCommonPartitioner(const SimplifiedContour &st,
                         const SimplifiedContour &ed)
{
  std::vector<InputPointInfo> tmp;

  tmp.reserve(st.size() + ed.size() + 2);
  InputPointInfo::add_pts(tmp, from_st, st);
  InputPointInfo::add_pts(tmp, from_ed, ed);

  /* Do a stable-sort to make sure that if there are degenerate
   * edges (i.e. length is zero), that they stay in order after
   * the sort.
   */
  std::stable_sort(tmp.begin(), tmp.end());

  /* walk the sorted array tmp, and when two neighboring elements are
   * "close" in their relative lengths AND have a different value for
   * m_src, merge them together.
   */
  const float thresh(1e-2);
  const InputPointInfo *prev_pt(nullptr);
  for (const InputPointInfo &p : tmp)
    {
      /* check if we should merge */
      if (prev_pt && prev_pt->m_src != p.m_src
          && t_abs(prev_pt->m_rel_length - p.m_rel_length) < thresh)
        {
          ASTRALassert(!m_parition_points.empty());
          ASTRALassert(m_parition_points.back().m_idx[p.m_src] == -1);
          ASTRALassert(m_parition_points.back().m_idx[prev_pt->m_src] == prev_pt->m_idx);

          m_parition_points.back().m_idx[p.m_src] = p.m_idx;
          m_parition_points.back().m_rel_length[p.m_src] = p.m_rel_length;
          prev_pt = nullptr; //to prevent more than 2 points merged to same.
        }
      else
        {
          PartitionPoint v;

          v.m_idx[p.m_src] = p.m_idx;
          v.m_rel_length[p.m_src] = p.m_rel_length;
          m_parition_points.push_back(v);

          prev_pt = &p; //to allow merging into the point just added
        }
    }

  /* add the end of the partition */
  PartitionPoint v;

  v.m_idx[from_st] = st.size() - 1;
  v.m_idx[from_ed] = ed.size() - 1;
  v.m_rel_length[from_st] = v.m_rel_length[from_ed] = 1.0f;
  m_parition_points.push_back(v);
}

////////////////////////////////////////////
// astral::detail::ContourBuilder methods
astral::detail::ContourBuilder::
ContourBuilder(c_array<const ContourCommonPartitioner::PartitionPoint> partition,
               const SimplifiedContour &input,
               enum ContourCommonPartitioner::point_src_t tp)
{
  /* we must not allow any curves to be dropped */
  m_contour.santize_curves_on_adding(false);

  /* Given the partition points and the SimplifiedContour,
   * build m_path as a single contour breaking the edges
   * of input as dictated by input.
   */
  unsigned int edge_start(0);
  float t_start(0.0f);

  m_contour.start(input.start_pt());
  for (unsigned int p = 0; p < partition.size(); ++p)
    {
      /* Recall that m_idx is the index of an edge and
       * we mark edges with their ending points.
       */
      if (partition[p].m_idx[tp] != -1)
        {
          /* Partition the edge named by m_idx[tp]
           * in time along the point partition[edge_start]
           * to partition[p]; we will succesively split
           * the bezier curve. However, we cannot use
           * directly the parition points in time. We
           * need to make the points in time relative
           * to the "current" curve of the the spit. The
           * The initial currnt of the split is the
           * entire curve itself.
           */
          range_type<float> range_rel_contour;
          unsigned int edge;

          edge = partition[p].m_idx[tp];
          range_rel_contour.m_begin = t_start;
          range_rel_contour.m_end = partition[p].m_rel_length[tp];

          ContourCurve current_curve(input[edge]);

          for (unsigned int E = edge_start; E < p; ++E)
            {
              float t_rel, t_contour, range;

              range = range_rel_contour.m_end - range_rel_contour.m_begin;
              t_contour = partition[E].rel_length();
              t_rel = (t_contour - range_rel_contour.m_begin) / range;

              /* split the current edge at t_rel */
              ContourCurveSplit split(false, current_curve, t_rel);

              /* add the pre-side of the split. Note that
               * since m_path is only being used to track
               * edge positions and not wether or not the
               * contour is closed we do not bother with
               * line_close() and quadratic_close().
               */
              m_contour.curve_to(split.before_t());

              /* update current to split.after_t() */
              range_rel_contour.m_begin = t_contour;
              current_curve = split.after_t();
            }

          m_contour.curve_to(current_curve);

          edge_start = p + 1;
          t_start = partition[p].m_rel_length[tp];
        }
    }
}

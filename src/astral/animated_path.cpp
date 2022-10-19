/*!
 * \file animated_path.cpp
 * \brief file animated_path.cpp
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

#include <numeric>
#include <algorithm>
#include <astral/animated_path.hpp>
#include <astral/util/bounding_box.hpp>
#include <astral/util/ostream_utility.hpp>
#include <astral/renderer/render_engine.hpp>
#include <astral/renderer/render_data.hpp>

#include "contour_approximator.hpp"
#include "animated_contour_util.hpp"
#include "generic_lod.hpp"

namespace
{
  void
  generate_contour_info(unsigned int c,
                        astral::c_array<const astral::ContourCurve> contour,
                        astral::AnimatedPath::ContourInfo &info)
  {
    info.m_contour = c;
    info.m_lengths.resize(contour.size());
    info.m_total_length = astral::detail::approximate_lengths(contour, astral::make_c_array(info.m_lengths));
  }

  class ContourWithInfoComparer
  {
  public:
    explicit
    ContourWithInfoComparer(const astral::AnimatedPath::ContourSorterBase &s):
      m_s(s)
    {}

    bool
    operator()(const astral::AnimatedPath::ContourWithInfo &lhs,
               const astral::AnimatedPath::ContourWithInfo &rhs) const
    {
      return (lhs.m_contour->closed() != rhs.m_contour->closed()) ?
        lhs.m_contour->closed() < rhs.m_contour->closed() :
        m_s.compare(lhs, rhs);
    }

  private:
    const astral::AnimatedPath::ContourSorterBase &m_s;
  };

  class PathPreparationData
  {
  public:
    enum contour_type_t:uint32_t
      {
        open_contours,
        closed_contours,
      };

    explicit
    PathPreparationData(const astral::Path &p,
                        const astral::AnimatedPath::ContourSorterBase &s);

    static
    void
    process_contour_list(enum contour_type_t tp,
                         const PathPreparationData &start,
                         const PathPreparationData &end,
                         astral::AnimatedPath &dst);

    astral::c_array<const astral::AnimatedPath::ContourWithInfo>
    contours(enum contour_type_t tp) const
    {
      return (tp == open_contours) ?
        m_open_contours :
        m_closed_contours;
    }

  private:
    std::vector<astral::AnimatedPath::ContourWithInfo> m_contours;
    astral::c_array<const astral::AnimatedPath::ContourWithInfo> m_open_contours, m_closed_contours;
    astral::vec2 m_path_center;
  };

}

////////////////////////////////////////
// PathPreparationData methods
PathPreparationData::
PathPreparationData(const astral::Path &p,
                    const astral::AnimatedPath::ContourSorterBase &s)
{
  unsigned int num_contours;

  num_contours = p.number_contours();
  m_contours.resize(num_contours);
  for (unsigned int i = 0; i < num_contours; ++i)
    {
      m_contours[i].m_contour = &p.contour(i);
      generate_contour_info(i, m_contours[i].m_contour->curves(), m_contours[i].m_info);
    }
  std::stable_sort(m_contours.begin(), m_contours.end(), ContourWithInfoComparer(s));

  /* The contour sorting always sorts first by open/closed with open contours
   * coming first, this the index into m_contours_sorted which refers to the
   * first closed contour is also the number of open contours.
   */
  unsigned int first_closed_at(num_contours);
  for (unsigned int C = 0; C < num_contours; ++C)
    {
      if (m_contours[C].m_contour->closed())
        {
          first_closed_at = C;
          C = num_contours;
        }
    }

  astral::c_array<const astral::AnimatedPath::ContourWithInfo> contours_sorted(astral::make_c_array(m_contours));
  m_open_contours = contours_sorted.sub_array(0, first_closed_at);
  m_closed_contours = contours_sorted.sub_array(first_closed_at);
  if (!p.bounding_box().empty())
    {
      m_path_center = p.bounding_box().as_rect().center_point();
    }
  else
    {
      m_path_center = astral::vec2(0.0f, 0.0f);
    }
}

void
PathPreparationData::
process_contour_list(enum contour_type_t tp,
                     const PathPreparationData &start,
                     const PathPreparationData &end,
                     astral::AnimatedPath &dst)
{
  unsigned int num;
  astral::c_array<const astral::AnimatedPath::ContourWithInfo> start_contours;
  astral::c_array<const astral::AnimatedPath::ContourWithInfo> end_contours;

  start_contours = start.contours(tp);
  end_contours = end.contours(tp);

  num = astral::t_max(start_contours.size(), end_contours.size());
  for (unsigned int I = 0; I < num; ++I)
    {
      astral::vec2 start_center(0.0f, 0.0f), end_center(0.0f, 0.0f);
      const astral::ContourData *start_contour(nullptr), *end_contour(nullptr);
      astral::c_array<const float> st_lengths, ed_lengths;

      if (I < start_contours.size())
        {
          start_contour = start_contours[I].m_contour;
          start_center = start_contour->bounding_box().as_rect().center_point();
          st_lengths = astral::make_c_array(start_contours[I].m_info.m_lengths);
        }

      if (I < end_contours.size())
        {
          end_contour = end_contours[I].m_contour;
          end_center = end_contour->bounding_box().as_rect().center_point();
          ed_lengths = astral::make_c_array(end_contours[I].m_info.m_lengths);
        }

      ASTRALassert(start_contour || end_contour);
      if (!start_contour)
        {
          ASTRALassert(end_contour);
          start_center = start.m_path_center;
        }
      else if (!end_contour)
        {
          ASTRALassert(start_contour);
          end_center = end.m_path_center;
        }

      dst.add_animated_contour(start_contour, start_center, st_lengths,
                               end_contour, end_center, ed_lengths);

    }
}

////////////////////////////////////////////////
// astral::AnimatedPath methods
astral::AnimatedPath::
AnimatedPath(const Path &start_path,
             const Path &end_path,
             const ContourSorterBase &sorter)
{
  add_animated_contours(start_path, end_path, sorter);
}

astral::AnimatedPath::
AnimatedPath(void)
{}

astral::AnimatedPath::
~AnimatedPath()
{}

astral::AnimatedPath&
astral::AnimatedPath::
add_animated_contours(const Path &start_path,
                      const Path &end_path,
                      const ContourSorterBase &s)
{
  /* first sort the contours in start_path and end_path */
  PathPreparationData start(start_path, s);
  PathPreparationData end(end_path, s);

  PathPreparationData::process_contour_list(PathPreparationData::open_contours, start, end, *this);
  PathPreparationData::process_contour_list(PathPreparationData::closed_contours, start, end, *this);

  return *this;
}

astral::BoundingBox<float>
astral::AnimatedPath::
bounding_box(float t) const
{
  return compute_bb(t, m_start_bb, m_end_bb);
}

astral::BoundingBox<float>
astral::AnimatedPath::
join_bounding_box(float t) const
{
  return compute_bb(t, m_start_join_bb, m_end_join_bb);
}

astral::BoundingBox<float>
astral::AnimatedPath::
open_contour_endpoint_bounding_box(float t) const
{
  return compute_bb(t, m_start_cap_bb, m_end_cap_bb);
}

astral::BoundingBox<float>
astral::AnimatedPath::
compute_bb(float t,
           const BoundingBox<float> &b0,
           const BoundingBox<float> &b1)
{
  vec2 min0(0.0f), min1(0.0f), max0(0.0f), max1(0.0f);
  BoundingBox<float> return_value;
  float s(1.0f - t);

  if (!b0.empty())
    {
      min0 = b0.min_point();
      max0 = b0.max_point();
    }

  if (!b1.empty())
    {
      min1 = b1.min_point();
      max1 = b1.max_point();
    }

  return_value.union_point(s * min0 + t * min1);
  return_value.union_point(s * max0 + t * max1);

  return return_value;
}

astral::AnimatedPath&
astral::AnimatedPath::
add_animated_contour(const ContourData *pst_contour, vec2 st_center, c_array<const float> st_lengths,
                     const ContourData *ped_contour, vec2 ed_center, c_array<const float> ed_lengths)
{
  return add_animated_contour(AnimatedContour::create(pst_contour, st_center, st_lengths,
                                                      ped_contour, ed_center, ed_lengths));
}

astral::AnimatedPath&
astral::AnimatedPath::
add_animated_contour(bool contours_are_closed,
                     c_array<const ContourCurve> st_contour, vec2 st_center,
                     c_array<const ContourCurve> ed_contour, vec2 ed_center)
{
  return add_animated_contour(AnimatedContour::create(contours_are_closed,
                                                      st_contour, st_center,
                                                      ed_contour, ed_center));
}

astral::AnimatedPath&
astral::AnimatedPath::
add_animated_contour(bool contours_are_closed,
                     c_array<const ContourCurve> st_contour, vec2 st_center, c_array<const float> st_lengths,
                     c_array<const ContourCurve> ed_contour, vec2 ed_center, c_array<const float> ed_lengths)
{
  return add_animated_contour(AnimatedContour::create(contours_are_closed,
                                                      st_contour, st_center, st_lengths,
                                                      ed_contour, ed_center, ed_lengths));
}

astral::AnimatedPath&
astral::AnimatedPath::
add_animated_contour(bool contours_are_closed,
                     c_array<const CompoundCurve> st_contour,
                     c_array<const CompoundCurve> ed_contour)
{
  return add_animated_contour(AnimatedContour::create(contours_are_closed, st_contour, ed_contour));
}

astral::AnimatedPath&
astral::AnimatedPath::
add_animated_contour_raw(bool contours_are_closed,
                         c_array<const ContourCurve> st_contour,
                         c_array<const ContourCurve> ed_contour)
{
  return add_animated_contour(AnimatedContour::create_raw(contours_are_closed, st_contour, ed_contour));
}

astral::AnimatedPath&
astral::AnimatedPath::
add_animated_contour_raw(bool contours_are_closed,
                         vec2 st_contour,
                         c_array<const ContourCurve> ed_contour)
{
  return add_animated_contour(AnimatedContour::create_raw(contours_are_closed, st_contour, ed_contour));
}

astral::AnimatedPath&
astral::AnimatedPath::
add_animated_contour_raw(bool contours_are_closed,
                         c_array<const ContourCurve> st_contour,
                         vec2 ed_contour)
{
  return add_animated_contour(AnimatedContour::create_raw(contours_are_closed, st_contour, ed_contour));
}

astral::AnimatedPath&
astral::AnimatedPath::
add_animated_contour_raw(vec2 st_contour, vec2 ed_contour)
{
  return add_animated_contour(AnimatedContour::create_raw(st_contour, ed_contour));
}

astral::AnimatedPath&
astral::AnimatedPath::
add_animated_contour(reference_counted_ptr<AnimatedContour> contour)
{
  if (!contour)
    {
      return *this;
    }

  m_path.push_back(contour);

  /* update all the boxes */
  m_start_bb.union_box(contour->start_contour().bounding_box());
  m_end_bb.union_box(contour->end_contour().bounding_box());

  m_start_join_bb.union_box(contour->start_contour().join_bounding_box());
  m_end_join_bb.union_box(contour->end_contour().join_bounding_box());

  ASTRALassert(contour->start_contour().closed() == contour->end_contour().closed());
  ASTRALassert(contour->start_contour().curves().size() == contour->end_contour().curves().size());

  if (!contour->start_contour().closed())
    {
      m_start_cap_bb.union_point(contour->start_contour().start());
      m_end_cap_bb.union_point(contour->end_contour().start());

      if (!contour->start_contour().curves().empty())
        {
          m_start_cap_bb.union_point(contour->start_contour().curves().back().end_pt());
          m_end_cap_bb.union_point(contour->end_contour().curves().back().end_pt());
        }
    }

  return *this;
}

astral::AnimatedPath&
astral::AnimatedPath::
clear(void)
{
  m_path.clear();
  return *this;
}

/*!
 * \file clip_util.cpp
 * \brief file clip_util.cpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#include <astral/util/clip_util.hpp>

namespace
{
  inline
  float
  compute_clip_dist(const astral::vec3 &clip_eq,
                    const astral::vec2 &pt)
  {
    return clip_eq.x() * pt.x() + clip_eq.y() * pt.y() + clip_eq.z();
  }

  inline
  astral::vec2
  compute_intersection(const astral::vec2 &p0, float d0,
                       const astral::vec2 &p1, float d1)
  {
    float t;

    t = d0 / (d0 - d1);
    return (1.0f - t) * p0 + t * p1;
  }

  inline
  bool
  compute_clip_dist(const astral::vec3 &clip_eq,
                    const astral::vec3 &pt)
  {
    return astral::dot(pt, clip_eq);
  }

  inline
  astral::vec3
  compute_intersection(const astral::vec3 &p0, float d0,
                       const astral::vec3 &p1, float d1)
  {
    float t;

    t = d0 / (d0 - d1);
    return (1.0f - t) * p0 + t * p1;
  }

  template<typename T>
  bool
  clip_against_planeT(const astral::vec3 &clip_eq,
                      astral::c_array<const T> pts,
                      std::vector<T> &out_pts)
  {
    if (pts.empty())
      {
        return false;
      }

    T prev(pts.back());
    float prev_d(compute_clip_dist(clip_eq, prev));
    bool prev_in(prev_d >= 0.0f), unclipped(true);

    out_pts.clear();
    for (unsigned int i = 0; i < pts.size(); ++i)
      {
        const T &current(pts[i]);
        float current_d(compute_clip_dist(clip_eq, pts[i]));
        bool current_in(current_d >= 0.0f);

        unclipped = unclipped && current_in;
        if (current_in != prev_in)
          {
            T q;
            q = compute_intersection(prev, prev_d,
                                     current, current_d);
            out_pts.push_back(q);
          }

        if (current_in && (out_pts.empty() || current != out_pts.back()))
          {
            out_pts.push_back(current);
          }
        prev = current;
        prev_d = current_d;
        prev_in = current_in;
      }

    return unclipped;
  }

  template<typename T>
  bool
  clip_against_planesT(astral::c_array<const astral::vec3> clip_eq,
                       astral::c_array<const T> in_pts,
                       unsigned int *out_idx,
                       astral::vecN<std::vector<T>, 2> &scratch_space)
  {
    unsigned int src(0), dst(1), i;
    bool unclipped(true);

    ASTRALassert(&scratch_space[0] != &scratch_space[1]);
    ASTRALassert(!in_pts.overlapping_memory(make_c_array(scratch_space[src])));

    scratch_space[src].resize(in_pts.size());
    std::copy(in_pts.begin(), in_pts.end(), scratch_space[src].begin());

    for(i = 0; i < clip_eq.size() && !scratch_space[src].empty(); ++i, std::swap(src, dst))
      {
        bool r;
        r = clip_against_planeT<T>(clip_eq[i],
                                   make_c_array(scratch_space[src]),
                                   scratch_space[dst]);
        unclipped = unclipped && r;
      }
    *out_idx = src;
    return unclipped;
  }
}

bool
astral::
clip_against_plane(const vec3 &clip_eq, c_array<const vec2> pts,
                   std::vector<vec2> &out_pts)
{
  return clip_against_planeT<vec2>(clip_eq, pts, out_pts);
}

bool
astral::
clip_against_planes(c_array<const vec3> clip_eq, c_array<const vec2> in_pts,
                    unsigned int *out_idx,
                    vecN<std::vector<vec2>, 2> &scratch_space)
{
  return clip_against_planesT<vec2>(clip_eq, in_pts, out_idx, scratch_space);
}

bool
astral::
clip_against_plane(const vec3 &clip_eq, c_array<const vec3> pts,
                   std::vector<vec3> &out_pts)
{
  return clip_against_planeT<vec3>(clip_eq, pts, out_pts);
}

bool
astral::
clip_against_planes(c_array<const vec3> clip_eq, c_array<const vec3> in_pts,
                    unsigned int *out_idx,
                    vecN<std::vector<vec3>, 2> &scratch_space)
{
  return clip_against_planesT<vec3>(clip_eq, in_pts, out_idx, scratch_space);
}

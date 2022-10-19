/*!
 * \file clip_util.hpp
 * \brief file clip_util.hpp
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


#ifndef ASTRAL_CLIP_UTIL_HPP
#define ASTRAL_CLIP_UTIL_HPP

#include <vector>
#include <astral/util/vecN.hpp>
#include <astral/util/c_array.hpp>

namespace astral
{
/*!\addtogroup Utility
 * @{
 */

  /*!
   * Clip a polygon against a single plane. The clip equation
   * clip_eq and the polygon pts are both in the same coordinate
   * system (likely local).
   * \param clip_eq single clip equation representing clipping a astral::vec2 p to
   *                p.x() * clip_eq.x() + p.y() * clip_eq.y() + clip_eq.z() >= 0
   * \param pts points of a convex polygon to be clipped
   * \param out_pts location to which to place the convex polygon clipped
   * \returns true if the input polygon was completely unclipped.
   */
  bool
  clip_against_plane(const vec3 &clip_eq, c_array<const vec2> pts,
                     std::vector<vec2> &out_pts);

  /*!
   * Clip a polygon against several planes. The clip equations
   * clip_eq and the polygon pts are both in the same coordinate
   * system (likely local).
   * \param clip_eq array of clip-equations
   * \param in_pts pts of input polygon
   * \param[out] out_idx location to which to write index into
   *                     scratch_space where clipped polygon is
   *                     written
   * \param scratch_space scratch spase for computation
   * \returns true if the input polygon was completely unclipped.
   */
  bool
  clip_against_planes(c_array<const vec3> clip_eq, c_array<const vec2> in_pts,
                      unsigned int *out_idx,
                      vecN<std::vector<vec2>, 2> &scratch_space);

  /*!
   * Clip a polygon against several planes. The clip equations
   * clip_eq and the polygon pts are both in the same coordinate
   * system (likely local).
   * \param clip_eq array of clip-equations
   * \param in_pts pts of input polygon
   * \param[out] out_pts location to which to write a c_array<>
   *                     value for the clipped polygon; the c_array<>
   *                     will point to one of the elements of
   *                     scratch_space
   * \param scratch_space scratch spase for computation
   * \returns true if the input polygon was completely unclipped.
   */
  inline
  bool
  clip_against_planes(c_array<const vec3> clip_eq, c_array<const vec2> in_pts,
                      c_array<const vec2> *out_pts,
                      vecN<std::vector<vec2>, 2> &scratch_space)
  {
    unsigned int idx;
    bool return_value;

    return_value = clip_against_planes(clip_eq, in_pts, &idx, scratch_space);
    *out_pts = make_c_array(scratch_space[idx]);
    return return_value;
  }

  /*!
   * Clip a polygon against a single plane. The clip equation
   * clip_eq and the polygon pts are both in the same coordinate
   * system (likely clip-coordinates).
   * \param clip_eq single clip equation representing clipping a astral::vec3 p to
   *                p.x() * clip_eq.x() + p.y() * clip_eq.y() + p.z() * clip_eq.z() >= 0
   * \param pts points of a convex polygon to be clipped
   * \param out_pts location to which to place the convex polygon clipped
   * \returns true if the input polygon was completely unclipped.
   */
  bool
  clip_against_plane(const vec3 &clip_eq, c_array<const vec3> pts,
                     std::vector<vec3> &out_pts);

  /*!
   * Clip a polygon against several planes. The clip equations
   * clip_eq and the polygon pts are both in the same coordinate
   * system (likely clip-coordinates).
   * \param clip_eq array of clip-equations
   * \param in_pts pts of input polygon
   * \param[out] out_idx location to which to write index into
   *                     scratch_space where clipped polygon is
   *                     written
   * \param scratch_space scratch spase for computation
   */
  bool
  clip_against_planes(c_array<const vec3> clip_eq, c_array<const vec3> in_pts,
                      unsigned int *out_idx,
                      vecN<std::vector<vec3>, 2> &scratch_space);

  /*!
   * Clip a polygon against several planes. The clip equations
   * clip_eq and the polygon pts are both in the same coordinate
   * system (likely clip-coordinates).
   * \param clip_eq array of clip-equations
   * \param in_pts pts of input polygon
   * \param[out] out_pts location to which to write a c_array<>
   *                     value for the clipped polygon; the c_array<>
   *                     will point to one of the elements of
   *                     scratch_space
   * \param scratch_space scratch spase for computation
   */
  inline
  bool
  clip_against_planes(c_array<const vec3> clip_eq, c_array<const vec3> in_pts,
                      c_array<const vec3> *out_pts,
                      vecN<std::vector<vec3>, 2> &scratch_space)
  {
    unsigned int idx;
    bool return_value;

    return_value = clip_against_planes(clip_eq, in_pts, &idx, scratch_space);
    *out_pts = make_c_array(scratch_space[idx]);
    return return_value;
  }
/*! @} */
}

#endif

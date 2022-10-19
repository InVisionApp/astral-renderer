/*!
 * \file combined_path.cpp
 * \brief combined_path.cpp
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

#include <astral/renderer/combined_path.hpp>

///////////////////////////////////
// astral::CombinedPath methods
void
astral::CombinedPath::
add_bb(const BoundingBox<float> &in_path_rect,
       const vec2 *ptranslate,
       const float2x2 *pmatrix,
       BoundingBox<float> *out_bb)
{
  if (in_path_rect.empty())
    {
      return;
    }

  vec2 translate;
  float2x2 matrix;
  const Rect &path_rect(in_path_rect.as_rect());

  translate = (ptranslate) ? *ptranslate : vec2(0.0f, 0.0f);
  if (pmatrix)
    {
      matrix = *pmatrix;
    }

  ASTRALassert(out_bb);
  for (unsigned int i = 0; i < 4; ++i)
    {
      enum Rect::corner_t c;

      c = static_cast<enum Rect::corner_t>(i);
      out_bb->union_point(matrix * path_rect.point(c) + translate);
    }
}

astral::BoundingBox<float>
astral::CombinedPath::
compute_bounding_box(float pstroke_inflate, float pjoin_inflate, enum cap_t cap_style) const
{
  BoundingBox<float> bb, cap_bb, join_bb;
  vec2 cap_inflate, stroke_inflate(pstroke_inflate), join_inflate(pjoin_inflate);

  cap_inflate = (cap_style == cap_square) ?
    vec2(1.41421356237f * pstroke_inflate) :
    vec2(pstroke_inflate);

  for (unsigned int i = 0; i < m_paths.size(); ++i)
    {
      const vec2 *translate(get_translate<Path>(i));
      const float2x2 *matrix(get_matrix<Path>(i));
      BoundingBox<float> tmp;

      add_bb(m_paths[i]->bounding_box(), translate, matrix, &bb);
      add_bb(m_paths[i]->join_bounding_box(), translate, matrix, &join_bb);

      if (cap_style != cap_flat)
        {
          add_bb(m_paths[i]->open_contour_endpoint_bounding_box(), translate, matrix, &cap_bb);
        }
    }

  for (unsigned int i = 0; i < m_animated_paths.size(); ++i)
    {
      float t(get_t<AnimatedPath>(i));
      const vec2 *translate(get_translate<AnimatedPath>(i));
      const float2x2 *matrix(get_matrix<AnimatedPath>(i));
      BoundingBox<float> tmp;

      add_bb(m_animated_paths[i]->bounding_box(t), translate, matrix, &bb);
      add_bb(m_animated_paths[i]->join_bounding_box(t), translate, matrix, &join_bb);

      if (cap_style != cap_flat)
        {
          add_bb(m_animated_paths[i]->open_contour_endpoint_bounding_box(t), translate, matrix, &cap_bb);
        }
    }

  bb.enlarge(stroke_inflate);
  join_bb.enlarge(join_inflate);
  cap_bb.enlarge(cap_inflate);

  bb.union_box(join_bb);
  bb.union_box(cap_bb);

  return bb;
}

/*!
 * \file transformed_bounding_box.cpp
 * \brief file transformed_bounding_box.cpp
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

#include <astral/util/transformed_bounding_box.hpp>

///////////////////////////////////////////////
// astral::TransformedBoundingBox methods
astral::TransformedBoundingBox::
TransformedBoundingBox(const BoundingBox<float> &bb):
  m_bb(bb),
  m_is_aligned_bb(true)
{
  if (!m_bb.empty())
    {
      init_values_from_aabb();
    }
}

astral::TransformedBoundingBox::
TransformedBoundingBox(const BoundingBox<float> &bb, const Transformation &tr):
  m_is_aligned_bb(true)
{
  if (!bb.empty())
    {
      enum matrix_type_t tp;

      ASTRALassert(!bb.empty());
      tp = compute_matrix_type(tr.m_matrix);

      if (tp == matrix_diagonal || tp == matrix_anti_diagonal)
        {
          m_bb = tr.apply_to_bb(bb);
          init_values_from_aabb();
        }
      else
        {
          m_is_aligned_bb = false;
          for (unsigned int i = 0; i < 4; ++i)
            {
              enum Rect::corner_t c;

              c = static_cast<enum Rect::corner_t>(i);
              m_pts[i] = tr.apply_to_point(bb.as_rect().point(c));
              m_bb.union_point(m_pts[i]);
            }

          /* get the vectors perpindicular to the sides; we do not need
           * to normalize the values either because that has the exact
           * same effect as just normalized the created dot() values.
           * However, would it be more numerically stable if we normalize?
           */
          m_normals[0] = m_pts[Rect::maxx_miny_corner] - m_pts[Rect::minx_miny_corner];
          m_normals[1] = m_pts[Rect::minx_maxy_corner] - m_pts[Rect::minx_miny_corner];
          for (unsigned int i = 0; i < 2u; ++i)
            {
              m_normals[i] = vec2(-m_normals[i].y(), m_normals[i].x());
            }

          for (unsigned int i = 0; i < 2u; ++i)
            {
              m_intervals[i].m_min = m_intervals[i].m_max = dot(m_pts[0], m_normals[i]);
              for (unsigned int p = 1; p < 4u; ++p)
                {
                  float v;

                  v = dot(m_pts[p], m_normals[i]);
                  m_intervals[i].m_min = t_min(m_intervals[i].m_min, v);
                  m_intervals[i].m_max = t_max(m_intervals[i].m_max, v);
                }
            }
        }
    }
}

void
astral::TransformedBoundingBox::
init_values_from_aabb(void)
{
  ASTRALassert(!m_bb.empty());
  ASTRALassert(m_is_aligned_bb);

  for (unsigned int i = 0; i < 4; ++i)
    {
      enum Rect::corner_t c;

      c = static_cast<enum Rect::corner_t>(i);
      m_pts[i] = m_bb.as_rect().point(c);
    }
  m_normals[0] = vec2(1.0f, 0.0f);
  m_normals[1] = vec2(0.0f, 1.0f);

  for (unsigned int i = 0; i < 2u; ++i)
    {
      m_intervals[i] = Interval(m_bb.as_rect().m_min_point[i], m_bb.as_rect().m_max_point[i]);
    }
}

bool
astral::TransformedBoundingBox::
intersects(const BoundingBox<float> &bb) const
{
  bool bbs_intersect;

  bbs_intersect = bb.intersects(m_bb);
  if (!bbs_intersect || m_is_aligned_bb)
    {
      return bbs_intersect;
    }

  ASTRALassert(!m_bb.empty());
  for (unsigned int i = 0; i < 2; ++i)
    {
      Interval I;

      I.m_min = I.m_max = dot(m_normals[i], bb.as_rect().m_min_point);
      for (unsigned int p = 1; p < 4u; ++p)
        {
          enum Rect::corner_t c;
          float v;

          c = static_cast<enum Rect::corner_t>(p);
          v = dot(m_normals[i], bb.as_rect().point(c));
          I.m_min = t_min(I.m_min, v);
          I.m_max = t_max(I.m_max, v);
        }

      if (!m_intervals[i].intersects(I))
        {
          return false;
        }
    }

  return true;
}

bool
astral::TransformedBoundingBox::
axis_seperates(unsigned int axis, const vecN<vec2, 4> &pts) const
{
  Interval I;

  I.m_min = I.m_max = dot(m_normals[axis], pts[0]);
  for (unsigned int p = 1; p < 4u; ++p)
    {
      float v;

      v = dot(m_normals[axis], pts[p]);
      I.m_min = t_min(I.m_min, v);
      I.m_max = t_max(I.m_max, v);
    }

  return !m_intervals[axis].intersects(I);
}

bool
astral::TransformedBoundingBox::
intersects(const TransformedBoundingBox &bb) const
{
  if (bb.is_axis_aligned())
    {
      return intersects(bb.containing_aabb());
    }

  if (is_axis_aligned())
    {
      return bb.intersects(containing_aabb());
    }

  ASTRALassert(!m_bb.empty());
  ASTRALassert(!bb.m_bb.empty());

  /* test if m_normals[0] or m_normals[1] seperates */
  for (unsigned int i = 0; i < 2u; ++i)
    {
      if (axis_seperates(i, bb.m_pts))
        {
          return false;
        }
    }

  /* test if bb.m_normals[0] or bb.m_normals[1] seperates */
  for (unsigned int i = 0; i < 2u; ++i)
    {
      if (bb.axis_seperates(i, m_pts))
        {
          return false;
        }
    }

  return true;
}

bool
astral::TransformedBoundingBox::
contains(const vec2 &pt) const
{
  if (empty())
    {
      return false;
    }

  for (unsigned int i = 0; i < 2u; ++i)
    {
      float d;

      d = dot(pt, m_normals[i]);
      if (!m_intervals[i].contains(d))
        {
          return false;
        }
    }

  return true;
}

bool
astral::TransformedBoundingBox::
contains(const TransformedBoundingBox &bb) const
{
  if (bb.empty())
    {
      return true;
    }

  for (unsigned int p = 0; p < 4; ++p)
    {
      if (!contains(bb.m_pts[p]))
        {
          return false;
        }
    }

  return true;
}

bool
astral::TransformedBoundingBox::
contains(const BoundingBox<float> &bb) const
{
  if (bb.empty())
    {
      return true;
    }

  for (unsigned int p = 0; p < 4; ++p)
    {
      enum Rect::corner_t c;

      c = static_cast<enum Rect::corner_t>(p);
      if (!contains(bb.as_rect().point(c)))
        {
          return false;
        }
    }

  return true;
}

//////////////////////////////////////////////////////
// astral::TransformedBoundingBox::Normalized methods
astral::TransformedBoundingBox::Normalized::
Normalized(const TransformedBoundingBox &bb):
  TransformedBoundingBox(bb)
{
  normalize_values();
}

astral::TransformedBoundingBox::Normalized::
Normalized(const BoundingBox<float> &bb):
  TransformedBoundingBox(bb)
{
  //normal vectors already normalized
}

astral::TransformedBoundingBox::Normalized::
Normalized(const BoundingBox<float> &bb, const Transformation &tr):
  TransformedBoundingBox(bb, tr)
{
  normalize_values();
}

void
astral::TransformedBoundingBox::Normalized::
normalize_values(void)
{
  if (!m_is_aligned_bb)
    {
      /* normalize m_normals[] and rescale m_intervals[] */
      for (unsigned int i = 0; i < 2; ++i)
        {
          float recip_norm;

          recip_norm = 1.0f / t_sqrt(dot(m_normals[i], m_normals[i]));
          m_normals[i] *= recip_norm;
          m_intervals[i].m_min *= recip_norm;
          m_intervals[i].m_max *= recip_norm;
        }
    }
}

astral::BoundingBox<float>
astral::TransformedBoundingBox::Normalized::
compute_intersection(const BoundingBox<float> &bb) const
{
  astral::BoundingBox<float> return_value;

  if (m_is_aligned_bb)
    {
      return_value = m_bb;
    }
  else
    {
      vecN<Interval, 2> intervals;

      for (unsigned int i = 0; i < 2; ++i)
        {
          intervals[i].m_min = intervals[i].m_max = dot(m_normals[i], bb.as_rect().m_min_point);
          for (unsigned int p = 1; p < 4u; ++p)
            {
              enum Rect::corner_t c;
              float v;

              c = static_cast<enum Rect::corner_t>(p);
              v = dot(m_normals[i], bb.as_rect().point(c));
              intervals[i].m_min = t_min(intervals[i].m_min, v);
              intervals[i].m_max = t_max(intervals[i].m_max, v);
            }

          if (!m_intervals[i].intersects(intervals[i]))
            {
              return return_value;
            }

          intervals[i].m_min = t_max(m_intervals[i].m_min, intervals[i].m_min);
          intervals[i].m_max = t_min(m_intervals[i].m_max, intervals[i].m_max);
        }

      /* add the four corners of the interval computation */
      return_value.union_point(intervals[0].m_min * m_normals[0] + intervals[1].m_min * m_normals[1]);
      return_value.union_point(intervals[0].m_min * m_normals[0] + intervals[1].m_max * m_normals[1]);
      return_value.union_point(intervals[0].m_max * m_normals[0] + intervals[1].m_min * m_normals[1]);
      return_value.union_point(intervals[0].m_max * m_normals[0] + intervals[1].m_max * m_normals[1]);
    }

  /* intersect against the original bb as well */
  return_value.intersect_against(bb);

  return return_value;
}

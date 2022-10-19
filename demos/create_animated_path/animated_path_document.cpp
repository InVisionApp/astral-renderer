/*!
 * \file animated_path_document.cpp
 * \brief file animated_path_document.cpp
 *
 * Copyright 2021 InvisionApp.
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

#include <iostream>
#include <algorithm>
#include <numeric>
#include <astral/util/ostream_utility.hpp>
#include "animated_path_document.hpp"

namespace
{
  template<typename T>
  std::istream&
  read_binary(std::istream &stream, T *out_value)
  {
    void *p;

    p = out_value;
    stream.read(static_cast<char*>(p), sizeof(T));

    return stream;
  }

  template<typename T, size_t N>
  std::istream&
  read_binary(std::istream &stream, astral::vecN<T, N> *out_value)
  {
    for (size_t i = 0; i < N && stream; ++i)
      {
        read_binary<T>(stream, &out_value->operator[](i));
      }
    return stream;
  }

  template<typename T>
  void
  write_binary(std::ostream &stream, const T &value)
  {
    const void *p;

    p = &value;
    stream.write(static_cast<const char*>(p), sizeof(T));
  }

  template<typename T, size_t N>
  void
  write_binary(std::ostream &stream, const astral::vecN<T, N> &value)
  {
    for (size_t i = 0; i < N && stream; ++i)
      {
        write_binary<T>(stream, value[i]);
      }
  }
}

#define LOAD_ERROR(X) do { std::cerr << "[" << __FILE__ << ", " << __LINE__ << "]: Load error "#X"\n"; } while(0)

//////////////////////////////////////
// AnimatedPathDocument::ContourPairing methods
void
AnimatedPathDocument::ContourPairing::
pair(unsigned int A, unsigned int B)
{
  ASTRALassert(A < m_pair_lookup[start_path].size());
  ASTRALassert(B < m_pair_lookup[end_path].size());

  if (m_pair_lookup[start_path][A] != -1 && m_pair_lookup[start_path][A] != -2)
    {
      remove_pairing(m_pair_lookup[start_path][A]);
    }

  if (m_pair_lookup[end_path][B] != -1 && m_pair_lookup[end_path][B] != -2)
    {
      remove_pairing(m_pair_lookup[end_path][B]);
    }

  m_pair_lookup[start_path][A] = m_pairs.size();
  m_pair_lookup[end_path][B] = m_pairs.size();
  m_pairs.push_back(astral::ivec2(A, B));
  ASTRALassert(consistent());
}

void
AnimatedPathDocument::ContourPairing::
remove_pairing(enum path_t path, unsigned int contour)
{
  int idx;

  ASTRALassert(contour < m_pair_lookup[path].size());
  idx = m_pair_lookup[path][contour];

  if (idx != -1 && idx != -2)
    {
      remove_pairing(idx);
    }
  else if (idx == -2)
    {
      m_pair_lookup[path][contour] = -1;
    }
}

/* idx is an index into m_pairs */
void
AnimatedPathDocument::ContourPairing::
remove_pairing(unsigned int idx)
{
  ASTRALassert(idx < m_pairs.size());

  swap_with_back(idx);
  idx = m_pairs.size() - 1;

  m_pair_lookup[start_path][m_pairs[idx][start_path]] = -1;
  m_pair_lookup[end_path][m_pairs[idx][end_path]] = -1;

  m_pairs.pop_back();
  ASTRALassert(consistent());
}

/* idx is an index into m_pairs */
void
AnimatedPathDocument::ContourPairing::
swap_with_back(unsigned int idx)
{
  ASTRALassert(idx < m_pairs.size());

  unsigned int back(m_pairs.size() - 1);
  if (idx == back)
    {
      return;
    }

  std::swap(m_pairs[idx], m_pairs[back]);

  m_pair_lookup[start_path][m_pairs[idx][start_path]] = idx;
  m_pair_lookup[start_path][m_pairs[back][start_path]] = back;

  m_pair_lookup[end_path][m_pairs[idx][end_path]] = idx;
  m_pair_lookup[end_path][m_pairs[back][end_path]] = back;

  ASTRALassert(consistent());
}

bool
AnimatedPathDocument::ContourPairing::
consistent(void) const
{
  astral::ivec2 sizes;

  for (int path = 0; path < 2; ++path)
    {
      sizes[path] = m_pair_lookup[path].size();
    }

  for (int pair_idx = 0, endi = m_pairs.size(); pair_idx < endi; ++pair_idx)
    {
      for (int path = 0; path < 2; ++path)
        {
          int contour;

          contour = m_pairs[pair_idx][path];
          if (contour >= sizes[path] || contour < 0)
            {
              ASTRALassert(false);
              return false;
            }

          if (m_pair_lookup[path][contour] != pair_idx)
            {
              return false;
            }
        }
    }

  int num_pairs(m_pairs.size());
  for (int path = 0; path < 2; ++path)
    {
      for (int contour = 0; contour < sizes[path]; ++contour)
        {
          int pair_idx;

          pair_idx = m_pair_lookup[path][contour];
          if (pair_idx != -1 && (pair_idx >= num_pairs || pair_idx < 0 || m_pairs[pair_idx][path] != contour))
            {
              ASTRALassert(false);
              return false;
            }
        }
    }

  return true;
}

/* BinaryContourPairing:
 *   N = number of Pairs as uint32_t
 *   N (ContourA, ContourB) as (uint32_t, uint32_t)
 *   K = number of contours of path A that collapse to a named point
 *   K (contourA, pt) (uint32_t, float, float)
 *   L = number of contours of path B that collapse to a named point
 *   L (contourB, pt) (uint32_t, float, float)
 */

enum astral::return_code
AnimatedPathDocument::ContourPairing::
load_from_file(unsigned int contour_countA,
               unsigned int contour_countB,
               std::istream &input_stream)
{
  uint32_t N;

  m_pair_lookup[start_path].clear();
  m_pair_lookup[end_path].clear();
  m_pairs.clear();

  if (!read_binary(input_stream, &N))
    {
      return astral::routine_fail;
    }

  m_pair_lookup[start_path].resize(contour_countA, -1);
  m_pair_lookup[end_path].resize(contour_countB, -1);

  m_collapse_points[start_path].resize(contour_countA, astral::vec2(0.0f, 0.0f));
  m_collapse_points[end_path].resize(contour_countB, astral::vec2(0.0f, 0.0f));

  for (unsigned int i = 0; i < N; ++i)
    {
      uint32_t A, B;

      if (!read_binary(input_stream, &A))
        {
          return astral::routine_fail;
        }

      if (!read_binary(input_stream, &B))
        {
          return astral::routine_fail;
        }

      pair(A, B);
    }

  for (unsigned int P = 0; P < 2; ++P)
    {
      uint32_t K;

      if (!read_binary(input_stream, &K))
        {
          return astral::routine_fail;
        }

      for (unsigned int i = 0; i < K; ++i)
        {
          uint32_t contour;
          astral::vec2 pt;

          if (!read_binary(input_stream, &contour))
            {
              return astral::routine_fail;
            }

          if (!read_binary(input_stream, &pt))
            {
              return astral::routine_fail;
            }
          collapse_to_a_point(static_cast<enum path_t>(P), contour, pt);
        }
    }

  return astral::routine_success;
}

void
AnimatedPathDocument::ContourPairing::
save_point_collapse_values(enum path_t path, std::ostream &dst) const
{
  uint32_t K(0);
  long location_K, location_end;

  location_K = dst.tellp();
  write_binary(dst, K);

  for (uint32_t i = 0, endi = m_pair_lookup[path].size(); i < endi; ++i)
    {
      if (m_pair_lookup[path][i] == -2)
        {
          write_binary(dst, i);
          write_binary(dst, m_collapse_points[path][i]);
          ++K;
        }
    }

  location_end = dst.tellp();
  dst.seekp(location_K);
  write_binary(dst, K);
  dst.seekp(location_end);
}

void
AnimatedPathDocument::ContourPairing::
save_to_file(std::ostream &dst) const
{
  uint32_t N(m_pairs.size());

  write_binary(dst, N);
  for (unsigned int i = 0; i < N; ++i)
    {
      write_binary(dst, m_pairs[i]);
    }

  save_point_collapse_values(start_path, dst);
  save_point_collapse_values(end_path, dst);
}

//////////////////////////////////////////////////////
// AnimatedPathDocument::ContourPointSequence methods
class AnimatedPathDocument::ContourPointSequence::Sorter
{
public:
  Sorter(astral::c_array<const ContourPoint> pts):
    m_pts(pts)
  {}

  bool
  operator()(unsigned int lhs, unsigned int rhs) const
  {
    return m_pts[lhs] < m_pts[rhs];
  }

private:
  astral::c_array<const ContourPoint> m_pts;
};

astral::c_array<const AnimatedPathDocument::ContourPoint>
AnimatedPathDocument::ContourPointSequence::
sorted_points(const astral::Contour &contour) const
{
  if (m_dirty)
    {
      std::vector<unsigned int> tmp(m_pts.size());
      Sorter sorter(astral::make_c_array(m_pts));
      std::vector<unsigned int>::iterator iter;

      m_dirty = false;
      m_sorted_points.clear();

      std::iota(tmp.begin(), tmp.end(), 0);
      std::sort(tmp.begin(), tmp.end(), sorter);

      iter = std::find(tmp.begin(), tmp.end(), m_first_point);
      for (auto i = iter; i != tmp.end(); ++i)
        {
          m_sorted_points.push_back(m_pts[*i]);
        }
      for (auto i = tmp.begin(); i != iter; ++i)
        {
          m_sorted_points.push_back(m_pts[*i]);
        }

      if (m_sorted_points.empty())
        {
          ContourPoint C;

          C.t() = 0.0f;
          C.curve() = 0;
          m_sorted_points.push_back(C);

          C.t() = 1.0f;
          C.curve() = contour.number_curves() - 1u;
          m_sorted_points.push_back(C);
        }
      else if (contour.closed())
        {
          m_sorted_points.push_back(m_sorted_points.front());
        }
    }

  return astral::make_c_array(m_sorted_points);
}

void
AnimatedPathDocument::ContourPointSequence::
on_contour_reverse(unsigned int curve_count)
{
  m_dirty = true;
  for (ContourPoint &p : m_pts)
    {
      p.t() = 1.0f - p.t();
      p.curve() = curve_count - 1 - p.curve();
    }
}

AnimatedPathDocument::PointIndex
AnimatedPathDocument::ContourPointSequence::
nearest_point(bool is_open_contour, astral::vec2 p) const
{
  PointIndex return_value;
  unsigned int start(1u);
  float dist;

  start = (is_open_contour) ? 2u : 0u;
  if (start >= m_pts.size())
    {
      return return_value;
    }

  dist = (m_pts[start].m_position - p).L1norm();
  return_value.m_value = start;

  for (unsigned int i = start + 1u, endi = m_pts.size(); i < endi; ++i)
    {
      float d;

      d = (m_pts[i].m_position - p).L1norm();
      if (d < dist)
        {
          dist = d;
          return_value.m_value = i;
        }
    }

  return return_value;
}

/*
 * BinaryContourPointSequence:
 *   m_first_point
 *   N = number of points as uint32_t {can be 0}
 *   N BinaryContourPoints
 */
enum astral::return_code
AnimatedPathDocument::ContourPointSequence::
load_from_file(std::istream &input_stream)
{
  uint32_t N;

  clear();
  if (!read_binary(input_stream, &m_first_point))
    {
      LOAD_ERROR("ContourPointSequence: m_first_point");
      return astral::routine_fail;
    }

  if (!read_binary(input_stream, &N))
    {
      LOAD_ERROR("ContourPointSequence: N");
      return astral::routine_fail;
    }

  for (unsigned int i = 0; i < N; ++i)
    {
      ContourPoint C;
      if (astral::routine_fail == load_from_file(input_stream, &C))
        {
          return astral::routine_fail;
        }
      add(C);
    }
  return astral::routine_success;
}

void
AnimatedPathDocument::ContourPointSequence::
save_to_file(std::ostream &dst) const
{
  uint32_t N(m_pts.size());

  write_binary(dst, m_first_point);
  write_binary(dst, N);
  for (unsigned int i = 0; i < N; ++i)
    {
      save_to_file(dst, m_pts[i]);
    }
}

/*
 * BinaryContourPoint:
 *   Curve as uint32_t
 *   Time as float
 *   position as (float, float)
 */
enum astral::return_code
AnimatedPathDocument::ContourPointSequence::
load_from_file(std::istream &input_stream, ContourPoint *C)
{
  if (!read_binary(input_stream, &C->curve()))
    {
      LOAD_ERROR("ContourPoint: CurveID");
      return astral::routine_fail;
    }

  if (!read_binary(input_stream, &C->t()))
    {
      LOAD_ERROR("ContourPoint: t");
      return astral::routine_fail;
    }

  if (!read_binary(input_stream, &C->m_position))
    {
      LOAD_ERROR("ContourPoint: position");
      return astral::routine_fail;
    }
  return astral::routine_success;
}

void
AnimatedPathDocument::ContourPointSequence::
save_to_file(std::ostream &dst, const ContourPoint &C)
{
  write_binary(dst, C.curve());
  write_binary(dst, C.t());
  write_binary(dst, C.m_position);
}

////////////////////////////////////////////////
// AnimatedPathDocument::PerPathContour methods
enum astral::return_code
AnimatedPathDocument::PerPathContour::
mark_as_first(PointIndex P)
{
  if (!m_contour->closed())
    {
      return astral::routine_fail;
    }

  m_pts.mark_as_first(P);
  return astral::routine_success;
}

void
AnimatedPathDocument::PerPathContour::
add_required_points(void)
{
  ASTRALassert(m_pts.empty());

  add_point(0u, 0.0f);
  mark_as_first(PointIndex(0));
  if (!contour().closed())
    {
      add_point(contour().curves().size() - 1u, 1.0f);
    }
}

void
AnimatedPathDocument::PerPathContour::
clear_points(void)
{
  m_pts.clear();
  add_required_points();
}

void
AnimatedPathDocument::PerPathContour::
contour(astral::reference_counted_ptr<astral::Contour> C)
{
  m_as_path_dirty = true;
  m_pts.clear();
  m_contour = C;
  add_required_points();
}

enum astral::return_code
AnimatedPathDocument::PerPathContour::
delete_point(PointIndex P)
{
  if (is_constant_point(P) || m_pts.number_points() == 1)
    {
      return astral::routine_fail;
    }

  m_pts.delete_point(P);
  return astral::routine_success;
}

enum astral::return_code
AnimatedPathDocument::PerPathContour::
modify_point(PointIndex P, unsigned int new_curve, float new_t)
{
  if (is_constant_point(P))
    {
      return astral::routine_fail;
    }

  astral::vec2 position;

  position = m_contour->curve(new_curve).eval_at(new_t);
  m_pts.modify(P, new_curve, new_t, position);
  return astral::routine_success;
}

////////////////////////////////////////
// AnimatedPathDocument::Path methods
unsigned int
AnimatedPathDocument::Path::
nearest_contour(float tol, astral::vec2 p) const
{
  unsigned int return_value;
  float dist;

  if (m_contours.empty())
    {
      return -1;
    }

  return_value = 0u;
  dist = m_contours[0].contour().distance_to_contour(tol, p).m_distance;

  for (unsigned int i = 1, endi = m_contours.size(); i < endi; ++i)
    {
      astral::Contour::PointQueryResult Q;

      Q = m_contours[i].contour().distance_to_contour(tol, p);
      if (Q.m_closest_curve >= 0u && Q.m_distance < dist)
        {
          return_value = i;
          dist = Q.m_distance;
        }
    }

  return return_value;
}

void
AnimatedPathDocument::Path::
copy_from_path(const astral::Path &src)
{
  m_contours.clear();
  m_anchor_point = src.bounding_box().as_rect().center_point();

  /* just copy the contours */
  for (unsigned int i = 0, endi = src.number_contours(); i < endi; ++i)
    {
      const astral::ContourData &data(src.contour(i));

      /* TODO: support point-contours */
      if (!data.curves().empty())
        {
          PerPathContour V;

          V.contour(astral::Contour::create(data));
          m_contours.push_back(V);
        }
    }
}

/* BinaryPath:
 *   Anchor Point as (float, float)
 *   N = number of contours as uint32_t
 *   N contours packed as BinaryContour
 *   N BinaryContourPointSequence
 */
enum astral::return_code
AnimatedPathDocument::Path::
load_from_file(std::istream &input_stream)
{
  uint32_t N;

  m_contours.clear();
  m_anchor_point = astral::vec2(0.0f, 0.0f);

  if (!read_binary(input_stream, &N))
    {
      LOAD_ERROR("Path: N");
      return astral::routine_fail;
    }

  if (!read_binary(input_stream, &m_anchor_point))
    {
      LOAD_ERROR("Path: aAnchor_point");
      return astral::routine_fail;
    }

  m_contours.resize(N);
  for (unsigned int i = 0; i < N; ++i)
    {
      astral::reference_counted_ptr<astral::Contour> C;

      C = load_contour_from_file(input_stream);
      if (!C)
        {
          return astral::routine_fail;
        }
      m_contours[i].contour(C);
    }

  for (unsigned int i = 0; i < N; ++i)
    {
      if (astral::routine_fail == m_contours[i].load_pts_from_file(input_stream))
        {
          return astral::routine_fail;
        }
    }
  return astral::routine_success;
}

void
AnimatedPathDocument::Path::
save_to_file(std::ostream &dst) const
{
  uint32_t N;

  N = number_contours();
  write_binary(dst, N);
  write_binary(dst, m_anchor_point);

  for (unsigned int i = 0; i < N; ++i)
    {
      save_to_file(dst, m_contours[i].contour());
    }

  for (unsigned int i = 0; i < N; ++i)
    {
      m_contours[i].save_pts_to_file(dst);
    }
}

/*
 * BinaryContour:
 *   N = number of curves as uint32_t
 *   uint32_t: 0 = open, 1 = closed
 *   N BinaryCurves as uint32_t
 */
astral::reference_counted_ptr<astral::Contour>
AnimatedPathDocument::Path::
load_contour_from_file(std::istream &input_stream)
{
  uint32_t N, closed;

  if (!read_binary(input_stream, &N))
    {
      LOAD_ERROR("Contour: N");
      return nullptr;
    }

  if (!read_binary(input_stream, &closed))
    {
      LOAD_ERROR("Contour: closed");
      return nullptr;
    }

  astral::ContourData contour;
  for (unsigned int i = 0; i < N; ++i)
    {
      if (astral::routine_fail == append_curve_from_file(input_stream, &contour))
        {
          return nullptr;
        }
    }

  if (closed != 0u)
    {
      contour.close();
    }

  return astral::Contour::create(contour);
}

void
AnimatedPathDocument::Path::
save_to_file(std::ostream &dst, const astral::ContourData &contour)
{
  uint32_t N, closed;

  N = contour.number_curves();
  write_binary(dst, N);

  closed = (contour.closed()) ? 1u : 0u;
  write_binary(dst, closed);

  for (unsigned int i = 0; i < N; ++i)
    {
      save_to_file(dst, contour.curve(i));
    }
}

/*
 * BinaryCurve:
 *   ContourCurve::type() as uint32_t
 *   start_pt() as (float, float)
 *   end_pt() as (float, float)
 *   control_pt(0) if quadratic, conic or cubic as (float, float)
 *   control_pt(1) if cubic  as (float, float)
 *   weight/angle if conic/conic_arc as float
 */
enum astral::return_code
AnimatedPathDocument::Path::
append_curve_from_file(std::istream &input_stream, astral::ContourData *dst)
{
  uint32_t type;

  if (!read_binary(input_stream, &type))
    {
      LOAD_ERROR("Curve: type");
      return astral::routine_fail;
    }

  enum astral::ContourCurve::continuation_t cnt(astral::ContourCurve::not_continuation_curve);
  astral::vec2 start_pt, end_pt;
  astral::vecN<astral::vec2, 2> control_pts;
  float VVV;

  if (!read_binary(input_stream, &start_pt))
    {
      LOAD_ERROR("Curve: start point");
      return astral::routine_fail;
    }

  if (!read_binary(input_stream, &end_pt))
    {
      LOAD_ERROR("Curve: end point");
      return astral::routine_fail;
    }

  if (type == astral::ContourCurve::quadratic_bezier
      || type == astral::ContourCurve::conic_curve
      || type == astral::ContourCurve::cubic_bezier)
    {
      if (!read_binary(input_stream, &control_pts[0]))
        {
          LOAD_ERROR("Curve: control_point(0)");
          return astral::routine_fail;
        }

      if (type == astral::ContourCurve::cubic_bezier)
        {
          if (!read_binary(input_stream, &control_pts[1]))
            {
              LOAD_ERROR("Curve: control_point(1)");
              return astral::routine_fail;
            }
        }
    }

  if (type == astral::ContourCurve::conic_curve
      || type == astral::ContourCurve::conic_arc_curve)
    {
      if (!read_binary(input_stream, &VVV))
        {
          LOAD_ERROR("Curve: conic_weight or angle");
          return astral::routine_fail;
        }
    }

  switch (type)
    {
    case astral::ContourCurve::line_segment:
      dst->curve_to(astral::ContourCurve(start_pt, end_pt, cnt));
      break;
    case astral::ContourCurve::quadratic_bezier:
      dst->curve_to(astral::ContourCurve(start_pt, control_pts[0], end_pt, cnt));
      break;
    case astral::ContourCurve::cubic_bezier:
      dst->curve_to(astral::ContourCurve(start_pt, control_pts[0], control_pts[1], end_pt, cnt));
      break;
    case astral::ContourCurve::conic_curve:
      dst->curve_to(astral::ContourCurve(start_pt, VVV, control_pts[0], end_pt, cnt));
      break;
    case astral::ContourCurve::conic_arc_curve:
      dst->curve_to(astral::ContourCurve(start_pt, VVV, end_pt, cnt));
      break;

    default:
      LOAD_ERROR("Curve: invalid type");
      return astral::routine_fail;
    }

  return astral::routine_success;
}

void
AnimatedPathDocument::Path::
save_to_file(std::ostream &dst, const astral::ContourCurve &curve)
{
  uint32_t type;

  type = curve.type();
  write_binary(dst, type);
  write_binary(dst, curve.start_pt());
  write_binary(dst, curve.end_pt());

  if (type == astral::ContourCurve::quadratic_bezier
      || type == astral::ContourCurve::conic_curve
      || type == astral::ContourCurve::cubic_bezier)
    {
      write_binary(dst, curve.control_pt(0));
      if (type == astral::ContourCurve::cubic_bezier)
        {
          write_binary(dst, curve.control_pt(1));
        }
    }

  if (type == astral::ContourCurve::conic_curve)
    {
      write_binary(dst, curve.conic_weight());
    }
  else if (type == astral::ContourCurve::conic_arc_curve)
    {
      write_binary(dst, curve.arc_angle());
    }
}


/////////////////////////////////////////////
// AnimatedPathDocument::CompoundCurveSequence methods
AnimatedPathDocument::CompoundCurveSequence::
CompoundCurveSequence(const astral::Transformation &transformation,
                      const astral::Contour &contour,
                      astral::c_array<const ContourPoint> pts,
                      unsigned int number_segments)
{
  CurrentCurve current(contour, pts[0]);
  const ContourPoint *prev_pt(&pts[0]);
  astral::range_type<unsigned int> R;

  for (unsigned int i = 1; i < pts.size() && number_segments > 1u; ++i, --number_segments)
    {
      astral::range_type<unsigned int> R;

      R = add_curves(transformation, contour, &current, pts[i - 1], pts[i]);
      m_compound_curve_range.push_back(R);
      prev_pt = &pts[i];
    }

  ASTRALassert(number_segments == 1u);
  R = add_curves(transformation, contour, &current, *prev_pt, pts.back());
  m_compound_curve_range.push_back(R);

  /* now create m_compoud_curves using m_compound_curve_range[] */
  ASTRALassert(m_lengths_backing.size() == m_curves_backing.size());
  astral::c_array<const float> lengths;
  astral::c_array<const astral::ContourCurve> curves_backing;

  lengths = astral::make_c_array(m_lengths_backing);
  curves_backing = astral::make_c_array(m_curves_backing);

  m_compoud_curves.resize(m_compound_curve_range.size());
  for (unsigned int i = 0, endi = m_compoud_curves.size(); i < endi; ++i)
    {
      m_compoud_curves[i].m_curves = curves_backing.sub_array(m_compound_curve_range[i]);
      m_compoud_curves[i].m_parameter_space_lengths = lengths.sub_array(m_compound_curve_range[i]);
    }
}

void
AnimatedPathDocument::CompoundCurveSequence::
add_curve(const astral::Transformation &transformation, const astral::ContourCurve &C)
{
  float L(0.0f);

  /* compute some length to be used for the animation,
   * these are not real lengths, just something that sort
   * of acts like a length;
   */
  switch (C.type())
    {
    case astral::ContourCurve::cubic_bezier:
      L = (C.end_pt() - C.control_pt(1)).magnitude()
        + (C.control_pt(1) - C.control_pt(0)).magnitude()
        + (C.control_pt(0) - C.start_pt()).magnitude();
      break;

    case astral::ContourCurve::quadratic_bezier:
    case astral::ContourCurve::conic_curve:
      L = (C.end_pt() - C.control_pt(0)).magnitude()
        + (C.control_pt(0) - C.start_pt()).magnitude();
      break;

    case astral::ContourCurve::conic_arc_curve:
      L = C.arc_radius() * astral::t_abs(C.arc_angle());
      break;

    case astral::ContourCurve::line_segment:
      L = (C.end_pt() - C.start_pt()).magnitude();
      break;
    }

  astral::ContourCurve new_curve(C, transformation);
  ASTRALassert(m_curves_backing.empty() || new_curve.start_pt() == m_curves_backing.back().end_pt());

  m_curves_backing.push_back(new_curve);
  m_lengths_backing.push_back(L);
}

void
AnimatedPathDocument::CompoundCurveSequence::
add_curves_implement(const astral::Transformation &transformation, const astral::Contour &contour,
                     CurrentCurve *current, const ContourPoint &pt_from, const ContourPoint &pt_to)
{
  ASTRALassert(pt_from.curve() == current->m_curve_id);
  if (pt_from == pt_to)
    {
      return;
    }

  if (pt_from.curve() == pt_to.curve())
    {
      add_curve(transformation, current->split_at(pt_to));
    }
  else
    {
      /* add the curves until we hit to */
      add_curve(transformation, current->m_curve);
      for (unsigned int i = pt_from.curve() + 1; i < pt_to.curve(); ++i)
        {
          add_curve(transformation, contour.curve(i));
        }

      /* now initialize current */
      *current = CurrentCurve(contour.curve(pt_to.curve()), pt_to.curve());
      add_curve(transformation, current->split_at(pt_to));
    }
}

astral::range_type<unsigned int>
AnimatedPathDocument::CompoundCurveSequence::
add_curves(const astral::Transformation &transformation, const astral::Contour &contour,
           CurrentCurve *current, const ContourPoint &from, const ContourPoint &to)
{
  astral::range_type<unsigned int> R;

  R.m_begin = m_curves_backing.size();
  if (from < to)
    {
      add_curves_implement(transformation, contour, current, from, to);
    }
  else
    {
      ContourPoint tmp;

      tmp.curve() = contour.number_curves() - 1;
      tmp.t() = 1.0f;
      add_curves_implement(transformation, contour, current, from, tmp);

      tmp.curve() = 0u;
      tmp.t() = 0.0f;
      /* we need to intialize *current to the start of the first curve */
      *current = CurrentCurve(contour.curve(0), 0);
      add_curves_implement(transformation, contour, current, tmp, to);
    }

  R.m_end = m_curves_backing.size();
  return R;
}

/////////////////////////////////////////////
// AnimatedPathDocument methods
AnimatedPathDocument::
AnimatedPathDocument(const astral::Path &pathA,
                     const astral::Path &pathB):
  m_pairing(pathA, pathB),
  m_dirty(true)
{
  m_paths[start_path].copy_from_path(pathA);
  m_paths[end_path].copy_from_path(pathB);

  /* pair the contours by order */
  for (unsigned int i = 0, endi = astral::t_min(pathA.number_contours(), pathB.number_contours()); i < endi; ++i)
    {
      pair_contours(i, i);
    }
}

void
AnimatedPathDocument::
clear_points(enum path_t path)
{
  unsigned int N(m_paths[path].number_contours());

  for (unsigned int i = 0; i < N; ++i)
    {
      clear_points(path, i);
    }
}

enum astral::return_code
AnimatedPathDocument::
mark_as_first(enum path_t path, unsigned int contour, PointIndex P)
{
  enum astral::return_code R;

  ASTRALassert(contour < m_paths[path].number_contours());
  R = m_paths[path].m_contours[contour].mark_as_first(P);

  if (R == astral::routine_success)
    {
      m_dirty = true;
    }

  return R;
}

/* AnimatedPathDocument file format (binary horror)
 *
 * File:
 *   8-bytes of MAGIC
 *   PathA as BinaryPath
 *   PathB as BinaryPath
 *   BinaryContourPairing
 */

static
astral::vecN<uint8_t, 8>
AnimatedPathDocumentMagic(void)
{
  astral::vecN<uint8_t, 8> return_value('A', 'N', 'I', 'M', 'A', 'T', 'E', 'D');
  return return_value;
}

astral::reference_counted_ptr<AnimatedPathDocument>
AnimatedPathDocument::
load_from_file(std::istream &input_stream)
{
  astral::vecN<uint8_t, 8> magic;
  astral::reference_counted_ptr<AnimatedPathDocument> return_value;
  unsigned int nA, nB;

  if(!read_binary(input_stream, &magic))
    {
      LOAD_ERROR("Short magic");
      return nullptr;
    }

  if (magic != AnimatedPathDocumentMagic())
    {
      LOAD_ERROR("Bad magic");
      return nullptr;
    }

  return_value = ASTRALnew AnimatedPathDocument();

  if (astral::routine_fail == return_value->m_paths[start_path].load_from_file(input_stream))
    {
      LOAD_ERROR("StartPath");
      return nullptr;
    }

  if (astral::routine_fail == return_value->m_paths[end_path].load_from_file(input_stream))
    {
      LOAD_ERROR("EndPath");
      return nullptr;
    }

  nA = return_value->m_paths[start_path].number_contours();
  nB = return_value->m_paths[end_path].number_contours();
  if (astral::routine_fail == return_value->m_pairing.load_from_file(nA, nB, input_stream))
    {
      LOAD_ERROR("ContourPairing\n");
      return nullptr;
    }

  return return_value;
}

void
AnimatedPathDocument::
save_to_file(std::ostream &dst) const
{
  write_binary(dst, AnimatedPathDocumentMagic());
  m_paths[start_path].save_to_file(dst);
  m_paths[end_path].save_to_file(dst);
  m_pairing.save_to_file(dst);
}

astral::reference_counted_ptr<astral::AnimatedContour>
AnimatedPathDocument::
create_animated_contour(const astral::Transformation &trA, const PerPathContour &contourA,
                        const astral::Transformation &trB, const PerPathContour &contourB)
{
  astral::c_array<const ContourPoint> ptsA, ptsB;
  unsigned int num_segments;

  ptsA = contourA.sorted_points();
  ptsB = contourB.sorted_points();

  ASTRALassert(ptsA.size() >= 2u);
  ASTRALassert(ptsB.size() >= 2u);

  num_segments = astral::t_min(ptsA.size(), ptsB.size()) - 1u;

  CompoundCurveSequence A(trA, contourA.contour(), ptsA, num_segments);
  CompoundCurveSequence B(trB, contourB.contour(), ptsB, num_segments);

  ASTRALassert(A.compound_curves().size() == B.compound_curves().size());
  return astral::AnimatedContour::create(contourA.contour().closed() || contourB.contour().closed(),
                                         A.compound_curves(), B.compound_curves());
}

void
AnimatedPathDocument::
create_translated_contour(const astral::Transformation &tr,
                          astral::c_array<const astral::ContourCurve> curves,
                          std::vector<astral::ContourCurve> *out_curves)
{
  out_curves->clear();
  for (const auto &curve : curves)
    {
      out_curves->push_back(astral::ContourCurve(curve, tr));
    }
}

const astral::AnimatedPath&
AnimatedPathDocument::
animated_path(void) const
{
  if (m_dirty)
    {
      m_dirty = false;
      m_animated_path.clear();

      /* we need to create the animated path. We will take the line
       * that unpaired contours are simply not included.
       */
      astral::vecN<astral::Transformation, 2> tr;
      astral::c_array<const astral::vecN<int32_t, 2>> pairs;
      std::vector<astral::ContourCurve> tmp_curves;

      tr[start_path].m_translate = -m_paths[start_path].m_anchor_point;
      tr[end_path].m_translate = -m_paths[end_path].m_anchor_point;

      pairs = m_pairing.pairs();
      for (const auto &p : pairs)
        {
          astral::reference_counted_ptr<astral::AnimatedContour> V;

          V = create_animated_contour(tr[start_path], m_paths[start_path].m_contours[p.x()],
                                      tr[end_path], m_paths[end_path].m_contours[p.y()]);

          m_animated_path.add_animated_contour(V);
        }

      /* unpaired contours will be collapsed to the anchor point */
      for (unsigned contour = 0, end_contour = m_paths[start_path].number_contours(); contour < end_contour; ++contour)
        {
          astral::vec2 pt;
          int idx;

          idx = m_pairing.query_pairing(start_path, contour, &pt);
          if (idx == -1 || idx == -2)
            {
              astral::reference_counted_ptr<astral::AnimatedContour> V;
              const PerPathContour &C(m_paths[start_path].m_contours[contour]);

              if (idx == -1)
                {
                  pt = astral::vec2(0.0f, 0.0f);
                }
              else
                {
                  pt = tr[start_path].apply_to_point(pt);
                }

              create_translated_contour(tr[start_path], C.contour().curves(), &tmp_curves);
              V = astral::AnimatedContour::create_raw(C.contour().closed(),
                                                      astral::make_c_array(tmp_curves), pt);

              m_animated_path.add_animated_contour(V);
            }
        }

      for (unsigned contour = 0, end_contour = m_paths[end_path].number_contours(); contour < end_contour; ++contour)
        {
          astral::vec2 pt;
          int idx;

          idx = m_pairing.query_pairing(end_path, contour, &pt);
          if (idx == -1 || idx == -2)
            {
              astral::reference_counted_ptr<astral::AnimatedContour> V;
              const PerPathContour &C(m_paths[end_path].m_contours[contour]);

              if (idx == -1)
                {
                  pt = astral::vec2(0.0f, 0.0f);
                }
              else
                {
                  pt = tr[end_path].apply_to_point(pt);
                }

              create_translated_contour(tr[end_path], C.contour().curves(), &tmp_curves);
              V = astral::AnimatedContour::create_raw(C.contour().closed(), pt,
                                                      astral::make_c_array(tmp_curves));

              m_animated_path.add_animated_contour(V);
            }
        }
    }

  return m_animated_path;
}

/*!
 * \file animated_path_reflect.cpp
 * \brief animated_path_reflect.cpp
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
#include <astral/util/polynomial.hpp>
#include "animated_path_reflect.hpp"

static
astral::Polynomial<double, 1>
create_polynomial_degree1(const astral::ContourCurve &curve)
{
  astral::Polynomial<double, 1> t, one_minus_t;

  t.coeff(0) = 0.0;
  t.coeff(1) = 1.0;

  one_minus_t.coeff(0) = 1.0;
  one_minus_t.coeff(1) = -1.0;

  return double(curve.start_pt().y()) * one_minus_t + double(curve.end_pt().y()) * t;
}

static
astral::Polynomial<double, 2>
create_polynomial_degree2(const astral::ContourCurve &curve)
{
  astral::Polynomial<double, 1> t, one_minus_t;

  t.coeff(0) = 0.0;
  t.coeff(1) = 1.0;

  one_minus_t.coeff(0) = 1.0;
  one_minus_t.coeff(1) = -1.0;

  return double(curve.start_pt().y()) * one_minus_t * one_minus_t
    + (2.0 * double(curve.control_pt(0).y() * curve.conic_weight())) * one_minus_t * t
    +  double(curve.end_pt().y()) * t * t;
}

static
astral::Polynomial<double, 3>
create_polynomial_degree3(const astral::ContourCurve &curve)
{
  astral::Polynomial<double, 1> t, one_minus_t;

  t.coeff(0) = 0.0;
  t.coeff(1) = 1.0;

  one_minus_t.coeff(0) = 1.0;
  one_minus_t.coeff(1) = -1.0;

  return double(curve.start_pt().y()) * one_minus_t * one_minus_t * one_minus_t
    + (3.0 * double(curve.control_pt(0).y())) * one_minus_t * one_minus_t * t
    + (3.0 * double(curve.control_pt(1).y())) * one_minus_t * t * t
    + double(curve.end_pt().y()) * t * t * t;
}

static
void
compute_intersection_against_x_axis(const astral::ContourCurve &curve, std::vector<float> *dst)
{
  astral::BoundingBox<float> bb;
  astral::vecN<double, 4> solutions_backing;
  astral::c_array<double> solutions(solutions_backing);
  unsigned int cnt(0);

  dst->clear();
  bb = curve.control_point_bounding_box();

  if (bb.min_point().y() > 0.0f || bb.max_point().y() < 0.0f)
    {
      return;
    }

  switch (curve.number_control_pts())
    {
    case 0:
      {
        auto P(create_polynomial_degree1(curve));
        cnt = astral::solve_polynomial(P, solutions);
      }
      break;

    case 1:
      {
        auto P(create_polynomial_degree2(curve));
        cnt = astral::solve_polynomial(P, solutions);
      }
      break;

    case 2:
      {
        auto P(create_polynomial_degree3(curve));
        cnt = astral::solve_polynomial(P, solutions);
      }
      break;
    }

  for (unsigned int i = 0; i < cnt; ++i)
    {
      if (solutions[i] >= 0.00 && solutions[i] < 1.0)
        {
          dst->push_back(solutions[i]);
        }
    }
}

static
void
set_contour(astral::ContourData *C, astral::c_array<const astral::ContourCurve> curves, bool closed)
{
  C->clear();
  if (!curves.empty())
    {
      C->start(curves.front().start_pt());
      for (const auto &curve: curves)
        {
          C->curve_to(curve);
        }

      if (closed)
        {
          C->close();
        }
    }
}

static
void
set_contour(astral::ContourData *C, const std::vector<astral::ContourCurve> &curves, bool closed)
{
  set_contour(C, astral::make_c_array(curves), closed);
}

static
astral::reference_counted_ptr<astral::AnimatedContour>
create_animated_reflection_closed(const astral::Contour &C, const Line &H,
                                  astral::ContourData *out_reflected,
                                  astral::ContourData *out_unreflected)
{
  /* Step 1. get the original geometry of the contour
   *         and cmpute the axis-aligned bounding box
   *         that is parallel to H.m_v
   */
  astral::Transformation tr;
  astral::BoundingBox<float> bb;

  /* assume that norm(H.m_v) is one */
  tr.m_matrix.row_col(0, 0) = H.m_v.x();
  tr.m_matrix.row_col(0, 1) = H.m_v.y();
  tr.m_matrix.row_col(1, 0) = -H.m_v.y();
  tr.m_matrix.row_col(1, 1) = H.m_v.x();
  for (unsigned int i = 0, endi = C.number_curves(); i < endi; ++i)
    {
      astral::ContourCurve curve(C.curve(i), tr);
      bb.union_box(curve.tight_bounding_box());
    }

  float V(bb.as_rect().center_point().y());

  /* Let Z be the transformation that maps the line
   * { (x, V) | x in R } to { (x, 0) | x in R }
   */
  astral::Transformation Z(astral::vec2(0.0f, -V));

  /* then the transformation Z * tr maps C to the coordinate
   * system where we do the reflection; map C by Z * tr.
   */
  tr = Z * tr;
  std::vector<astral::ContourCurve> rotatedC;
  rotatedC.reserve(C.number_curves());
  for (unsigned int i = 0, endi = C.number_curves(); i < endi; ++i)
    {
      rotatedC.push_back(astral::ContourCurve(C.curve(i), tr));
    }

  /* Step 2: Find the first and last time the contour
   *         rotatedC intersects the x-axis.
   */
  std::pair<unsigned int, float> last_time(rotatedC.size() - 1, 0.5f), first_time(0u, 0.5);
  bool found(false);
  std::vector<float> dst;

  for (unsigned int i = 0, endi = rotatedC.size(); i < endi; ++i)
    {
      compute_intersection_against_x_axis(rotatedC[i], &dst);
      for (float t : dst)
        {
          std::pair<unsigned int, float> I(i, t);

          if (!found || I <= first_time)
            {
              first_time = I;
            }

          if (!found || I >= last_time)
            {
              last_time = I;
            }

          found = true;
        }
    }

  /* Step 4: Construct A to be C restricted reodered
   *         so that split_first_time.after_t() comes first.
   *         Also mark where split_last_time.after_t() is added
   */
  std::vector<astral::ContourCurve> A;
  unsigned int split_last_time_after_t_added_at;

  if (first_time.first != last_time.first)
    {
      astral::ContourCurveSplit split_first_time(false, C.curve(first_time.first), first_time.second);
      astral::ContourCurveSplit split_last_time(false, C.curve(last_time.first), last_time.second);

      if (first_time.second > 0.0f)
        {
          A.push_back(split_first_time.after_t());
        }
      else
        {
          A.push_back(C.curve(first_time.first));
        }

      for (unsigned int I = first_time.first + 1; I < last_time.first; ++I)
        {
          A.push_back(C.curve(I));
        }

      if (last_time.second > 0.0f)
        {
          A.push_back(split_last_time.before_t());
          split_last_time_after_t_added_at = A.size();
          A.push_back(split_last_time.after_t());
        }
      else
        {
          split_last_time_after_t_added_at = A.size();
          A.push_back(C.curve(last_time.first));
        }

      for (unsigned int I = last_time.first + 1; I < rotatedC.size(); ++I)
        {
          A.push_back(C.curve(I));
        }
      for (unsigned int I = 0; I < first_time.first; ++I)
        {
          A.push_back(C.curve(I));
        }

      if (first_time.second > 0.0f)
        {
          A.push_back(split_first_time.before_t());
        }
    }
  else
    {
      /* special case where the same curve is the first and last time splits */
      float rel_t;

      rel_t = (last_time.second - first_time.second) / (1.0f - first_time.second);
      astral::ContourCurveSplit alpha(false, C.curve(first_time.first), first_time.second);
      astral::ContourCurveSplit beta(false, alpha.after_t(), rel_t);

      if (first_time.second > 0.0f)
        {
          A.push_back(beta.before_t());
          split_last_time_after_t_added_at = A.size();
          A.push_back(beta.after_t());
        }
      else
        {
          split_last_time_after_t_added_at = A.size();
          A.push_back(beta.after_t());
        }

      for (unsigned int I = last_time.first + 1; I < rotatedC.size(); ++I)
        {
          A.push_back(C.curve(I));
        }
      for (unsigned int I = 0; I < first_time.first; ++I)
        {
          A.push_back(C.curve(I));
        }

      if (first_time.second > 0.0f)
        {
          A.push_back(alpha.before_t());
        }
      else
        {
          A.push_back(beta.before_t());
        }
    }

  /* Step 5: Construct B to be A reversed and reflected by H; by
   *         both reversing and reflecting the overall orientation
   *         is preserved.
   */
  std::vector<astral::ContourCurve> B;
  astral::Transformation reflect(H.reflect_transformation());
  for (auto iter = A.rbegin(); iter != A.rend(); ++iter)
    {
      iter->reset_generation();
      B.push_back(astral::ContourCurve(iter->reverse_curve(), reflect));
    }

  /* Step 6: we now have the A and B pair nicely for reflecting
   *         animation, we now construct the AnimatedContour.
   */
  std::vector<float> all_ones_backing(B.size(), 1.0f);
  astral::c_array<const float> all_ones(astral::make_c_array(all_ones_backing));
  astral::c_array<const astral::ContourCurve> A_as_c_array(astral::make_c_array(A));
  astral::c_array<const astral::ContourCurve> B_as_c_array(astral::make_c_array(B));
  astral::vecN<astral::AnimatedContour::CompoundCurve, 2> compound_curvesA;
  astral::vecN<astral::AnimatedContour::CompoundCurve, 2> compound_curvesB;

  compound_curvesA[0].m_curves = A_as_c_array.sub_array(0, split_last_time_after_t_added_at);
  compound_curvesA[0].m_parameter_space_lengths = all_ones.sub_array(0, compound_curvesA[0].m_curves.size());

  compound_curvesA[1].m_curves = A_as_c_array.sub_array(split_last_time_after_t_added_at);
  compound_curvesA[1].m_parameter_space_lengths = all_ones.sub_array(0, compound_curvesA[1].m_curves.size());

  compound_curvesB[0].m_curves = B_as_c_array.sub_array(0, A.size() - split_last_time_after_t_added_at);
  compound_curvesB[0].m_parameter_space_lengths = all_ones.sub_array(0, compound_curvesB[0].m_curves.size());

  compound_curvesB[1].m_curves = B_as_c_array.sub_array(A.size() - split_last_time_after_t_added_at);
  compound_curvesB[1].m_parameter_space_lengths = all_ones.sub_array(0, compound_curvesB[1].m_curves.size());

  if (out_reflected)
    {
      set_contour(out_reflected, B, true);
    }

  if (out_unreflected)
    {
      set_contour(out_unreflected, A, true);
    }

  return astral::AnimatedContour::create(true, compound_curvesA, compound_curvesB);
}

static
astral::reference_counted_ptr<astral::AnimatedContour>
create_animated_reflection_open(const astral::Contour &C, const Line &H,
                                astral::ContourData *out_reflected,
                                astral::ContourData *out_unreflected)
{
  std::vector<astral::ContourCurve> R;
  astral::Transformation reflect(H.reflect_transformation());

  for (const auto &curve : C.curves())
    {
      R.push_back(astral::ContourCurve(curve, reflect));
    }

  if (out_reflected)
    {
      set_contour(out_reflected, R, C.closed());
    }

  if (out_unreflected)
    {
      set_contour(out_unreflected, C.curves(), C.closed());
    }

  return astral::AnimatedContour::create_raw(C.closed(), C.curves(), astral::make_c_array(R));
}

astral::reference_counted_ptr<astral::AnimatedContour>
create_animated_reflection(const astral::Contour &C, const Line &H,
                           astral::ContourData *out_reflected,
                           astral::ContourData *out_unreflected)
{
  if (C.closed())
    {
      return create_animated_reflection_closed(C, H, out_reflected, out_unreflected);
    }
  else
    {
      return create_animated_reflection_open(C, H, out_reflected, out_unreflected);
    }
}

void
create_animated_reflection(astral::AnimatedPath *dst,
                           const astral::Path &src,
                           Line &H,
                           astral::Path *out_reflected,
                           astral::Path *out_unreflected)
{
  if (out_reflected)
    {
      out_reflected->clear();
    }

  if (out_unreflected)
    {
      out_unreflected->clear();
    }

  for (unsigned int c = 0, endc = src.number_contours(); c < endc; ++c)
    {
      astral::ContourData R, U;
      astral::ContourData *pR, *pU;

      if (out_reflected)
        {
          pR = &R;
        }
      else
        {
          pR = nullptr;
        }

      if (out_unreflected)
        {
          pU = &U;
        }
      else
        {
          pU = nullptr;
        }

      dst->add_animated_contour(create_animated_reflection(src.contour(c), H, pR, pU));
      if (out_reflected)
        {
          out_reflected->add_contour(*pR);
        }
      if (out_unreflected)
        {
          out_unreflected->add_contour(*pU);
        }

    }
}

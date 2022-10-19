/*!
 * \file contour_approximator.cpp
 * \brief file contour_approximator.cpp
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

#include <astral/util/ostream_utility.hpp>
#include <astral/util/polynomial.hpp>
#include "contour_approximator.hpp"

static
void
compute_cusps(const astral::ContourCurve &C, std::vector<float> *out_t)
{
  using namespace astral;

  if (C.type() != ContourCurve::cubic_bezier || C.flatness() == 0.0f)
    {
      return;
    }

  /*  1. Let X = { 0 < t < 1 | x'(t) = 0 } and Y = { 0 < t < 1 | y'(t) = 0 }
   *  2. If a is an element of X and b is an element of Y with |a - b| < epsilon,
   *     then add (a + b) * 0.5 to out_t. A good value of epsilon would be
   *     what?
   */

  /* The derivative of a N-degree Beizer curve [p0, p1, p2, .. pN]
   * is the (N - 1) degree Bezier curve given by
   * [N * (p1 - p0), N * (p2 - p1), .... , N * (pN - pN-1)]
   *
   * Since, we need only the locations where the derivatie is zero,
   * we can drop the multiply by N. We do the computation in double
   * to pick up more bits of precision.
   */
  dvec2 p0, p1, p2;

  p0 = dvec2(C.control_pt(0)) - dvec2(C.start_pt());
  p1 = dvec2(C.control_pt(1)) - dvec2(C.control_pt(0));
  p2 = dvec2(C.end_pt()) - dvec2(C.control_pt(1));

  Polynomial<dvec2, 2> P;

  P.coeff(0) = p0;
  P.coeff(1) = 2.0 * (p1 - p0);
  P.coeff(2) = p0 - 2.0 * p1 + p2;

  vecN<Polynomial<double, 2>, 2> PP = convert(P);
  vecN<double, 2> x_solutions_backing, y_solutions_backing;
  c_array<double> x_solutions(x_solutions_backing), y_solutions(y_solutions_backing);
  unsigned int num_solutions_x, num_solutions_y;

  num_solutions_x = solve_polynomial(PP.x(), x_solutions);
  num_solutions_y = solve_polynomial(PP.y(), y_solutions);

  x_solutions = x_solutions.sub_array(0, num_solutions_x);
  y_solutions = y_solutions.sub_array(0, num_solutions_y);

  static const double epsilon = 1e-5;
  for (unsigned int i = 0; i < x_solutions.size(); ++i)
    {
      double x;

      x = x_solutions[i];

      /* drop solutions past or close to the end-points */
      if (x < epsilon || x > 1.0 - epsilon)
        {
          continue;
        }

      for (unsigned int j = 0; j < y_solutions.size(); ++j)
        {
          if (t_abs(x - y_solutions[j]) < epsilon)
            {
              out_t->push_back(x);
              if (j == 0)
                {
                  y_solutions.pop_front();
                }
              else
                {
                  ASTRALassert(j == 1);
                  y_solutions.pop_back();
                }
              break;
            }
        }
    }
}

static
void
merge_similiar_values(std::vector<float> *dst)
{
  if (dst->size() < 2)
    {
      return;
    }

  const static float epsilon = 1e-5;

  std::vector<float> tmp;
  for (float f : *dst)
    {
      if (tmp.empty() || f > tmp.back() + epsilon)
        {
          tmp.push_back(f);
        }
    }

  dst->swap(tmp);
}

////////////////////////////////////////////////////
// astral::detail::ContourApproximator methods
void
astral::detail::ContourApproximator::
add_cubic(bool approximate_as_line_segment,
          ApproximatedContour *quadratic_contour,
          TessedContour *tessed_contour,
          bool has_quadratic,
          const ContourCurve &C)
{
  ASTRALassert(C.type() == ContourCurve::cubic_bezier);

  if (approximate_as_line_segment)
    {
      m_error = t_max(m_error, error_to_line_appoximation(C));
      if (has_quadratic)
        {
          vec2 ctl;

          ctl = 0.5f * (C.start_pt() + C.end_pt());
          quadratic_contour->push_back(ContourCurve(C.start_pt(), ctl, C.end_pt(), C.continuation()));
        }
      else
        {
          quadratic_contour->push_back(ContourCurve(C.start_pt(), C.end_pt(), C.continuation()));
        }
      tessed_contour->push_back(C);

      if (C.generation() < generation_ignore_size && size_of_lines_contributes_to_error(m_sz_mode))
        {
          vec2 d(C.start_pt() - C.end_pt());
          m_error = t_max(m_error, compute_error_from_size(d.magnitude()));
        }
    }
  else
    {
      QuadraticBezierCurve Q;
      float error;

      error = compute_quadratic_appoximation(C, &Q);
      quadratic_contour->push_back(ContourCurve(Q[0], Q[1], Q[2], C.continuation()));
      tessed_contour->push_back(C);

      if (m_mode == approximate_to_quadratic_error_to_biarc)
        {
          error += compute_biarc_error_rel_length<double>(Q);
        }

      m_error = t_max(m_error, error);
      if (size_of_curve_contributes_to_error(m_sz_mode)
          && C.generation() < generation_ignore_size)
        {
          m_error = t_max(m_error, compute_error_from_size(Q));
        }
    }
}

void
astral::detail::ContourApproximator::
add_quadratic(ApproximatedContour *quadratic_contour,
              TessedContour *tessed_contour, const ContourCurve &C)
{
  ASTRALassert(C.type() == ContourCurve::quadratic_bezier);

  float error;
  QuadraticBezierCurve Q;

  error = compute_quadratic_appoximation(C, &Q);
  quadratic_contour->push_back(ContourCurve(Q[0], Q[1], Q[2], C.continuation()));
  tessed_contour->push_back(C);

  if (m_mode == approximate_to_quadratic_error_to_biarc)
    {
      error += compute_biarc_error_rel_length<double>(Q);
    }

  m_error = t_max(m_error, error);
  if (size_of_curve_contributes_to_error(m_sz_mode)
      && C.generation() < generation_ignore_size)
    {
      m_error = t_max(m_error, compute_error_from_size(Q));
    }
}

void
astral::detail::ContourApproximator::
add_conic(ApproximatedContour *quadratic_contour,
          TessedContour *tessed_contour,
          const ContourCurve &C)
{
  ASTRALassert(C.type() == ContourCurve::conic_curve || C.type() == ContourCurve::conic_arc_curve);

  QuadraticBezierCurve Q;
  float error;

  tessed_contour->push_back(C);

  if (m_mode == approximate_to_conic_or_quadratic)
    {
      Q[0] = C.start_pt();
      Q[1] = C.control_pt(0);
      Q[2] = C.end_pt();
      quadratic_contour->push_back(C);
      error = 0.0f;
    }
  else
    {
      error = compute_quadratic_appoximation(C, &Q);
      quadratic_contour->push_back(ContourCurve(Q[0], Q[1], Q[2], C.continuation()));
    }

  if (m_mode == approximate_to_quadratic_error_to_biarc)
    {
      error += compute_biarc_error_rel_length<double>(Q);
    }

  m_error = t_max(m_error, error);
  if (size_of_curve_contributes_to_error(m_sz_mode)
      && C.generation() < generation_ignore_size)
    {
      m_error = t_max(m_error, compute_error_from_size(Q));
    }
}

void
astral::detail::ContourApproximator::
add_line(ApproximatedContour *quadratic_contour,
         TessedContour *tessed_contour,
         bool force_quadratic, const ContourCurve &C)
{
  ASTRALassert(C.type() == ContourCurve::line_segment);

  tessed_contour->push_back(C);
  if (force_quadratic)
    {
      vec2 ctl;

      ctl = 0.5f * (C.start_pt() + C.end_pt());
      quadratic_contour->push_back(ContourCurve(C.start_pt(), ctl, C.end_pt(), C.continuation()));
    }
  else
    {
      quadratic_contour->push_back(C);
    }

  if (size_of_lines_contributes_to_error(m_sz_mode)
      && C.generation() < generation_ignore_size)
    {
      vec2 d;

      d = C.start_pt() - C.end_pt();
      m_error = t_max(m_error, compute_error_from_size(d.magnitude()));
    }
}

astral::reference_counted_ptr<const astral::detail::ContourApproximator>
astral::detail::ContourApproximator::
create_refinement_implement(c_array<const SourceTag> tags,
                            c_array<ApproximatedContour> dst_geometry,
                            std::vector<SourceTag> *dst_tags) const
{
  return ASTRALnew ContourApproximator(make_c_array(m_tessed_contours),
                                       m_error * 0.5f, dst_geometry, m_mode,
                                       m_sz_mode, m_split_singleton_contours,
                                       dont_split_cubics_at_cusps, //if cusp splitting needed to be done, then this already did it
                                       tags, dst_tags);
}

float
astral::detail::ContourApproximator::
compute_error_from_size(const QuadraticBezierCurve &p)
{
  BoundingBox<float> bb;

  bb.union_point(p[0]);
  bb.union_point(p[1]);
  bb.union_point(p[2]);

  return compute_error_from_size(bb);
}

void
astral::detail::ContourApproximator::
post_process_approximation(c_array<ApproximatedContour> contours)
{
  if (contours.empty() || contours[0].empty())
    {
      return;
    }

  /* 1e-4 is one part in 10,000 to decide if a curve is flat. The
   * measure of flatness is relative to the distance between the
   * end-point. In this case we are declaring that a curve is
   * flat if the control point is less than flat_epsilon times
   * that distance from the line segment. Now, the curves coming
   * in are also already tessellated too (usually) so that they
   * are not more than 128 pixels in length (about), thus 1 part
   * in 10,000 is much less than a pixel anyways.
   */
  const static float flat_epsilon(1e-4);

  /* 5e-5 is one part in 500,000 to decide if a curve is degenerate.
   * A curve is regarded as degenerate if the two end points are
   * relatively this close. It is worth noting that the accuracy of
   * fp32 is something like 1 part in 2^23, which corresponds to
   * 6-7 digits of accuracy, so taking 1 part in 500,000 is quite
   * close, but hopefully not so close that the tangent vectors of
   * such short line segments are junk.
   */
  const static float degenerate_epsilone(5e-5);

  /* We want to remove curves that are degenerate and also remove
   * the situation where a control point is the same as an end-point
   * of a curve. Curves can be line segments, quadratic bezier
   * curves and conics only.
   *
   * The main fly in the ointment is that if there is more than
   * one contour, then we can remove a curve only if we remove the
   * curve from all other contours too. In addition, if we can only
   * convert a curve to a line only if we convert all curves to
   * to a line.
   */
  std::vector<ApproximatedContour> out_contours(contours.size());
  std::vector<vec2> start_pts(contours.size()), end_pts(contours.size());
  std::vector<bool> can_remove_curve(contours[0].size(), true);

  for (unsigned int contour = 0, end_contour = contours.size(); contour < end_contour; ++contour)
    {
      start_pts[contour] = contours[contour].front().start_pt();
      end_pts[contour] = contours[contour].back().end_pt();

      /* We cannot remove a curve if either of its start or end would be merged
       * with a bevel glue join. If we remove such a curve, then there is a risk
       * that the glue join joining the curves not removed would add a rounded
       * glue join that would not honor the bevel requirement (for cusps).
       */
      for (unsigned int curve = 0, end_curve = contours[contour].size(); curve < end_curve; ++curve)
        {
          if (contours[contour][curve].continuation() == ContourCurve::continuation_curve_cusp)
            {
              can_remove_curve[curve] = false;
            }

          if (curve + 1 < end_curve && contours[contour][curve + 1u].continuation() == ContourCurve::continuation_curve_cusp)
            {
              can_remove_curve[curve] = false;
            }
        }
    }

  for (unsigned int curve = 0, end_curve = contours[0].size(); curve < end_curve; ++curve)
    {
      unsigned int num_degenerate(0), num_flat(0), num_non_linear(0);

      for (unsigned int contour = 0, end_contour = contours.size(); contour < end_contour; ++contour)
        {
          ASTRALassert(contours[contour].size() == end_curve);

          const ContourCurve &C(contours[contour][curve]);

          ASTRALassert(C.number_control_pts() <= 1u);
          num_degenerate += (C.is_degenerate(degenerate_epsilone)) ? 1u : 0u;
          num_flat += (C.is_flat(flat_epsilon)) ? 1u : 0u;
          num_non_linear += (C.number_control_pts() > 0u && !C.control_pt_is_degenerate(0, flat_epsilon)) ? 1u : 0u;
        }

      if (num_degenerate == contours.size() && can_remove_curve[curve])
        {
          /* all curves are degenerate, or close to degenerate. */
        }
      else if (num_flat == contours.size())
        {
          /* replace all conic curves present with a line segment */
          for (unsigned int contour = 0, end_contour = contours.size(); contour < end_contour; ++contour)
            {
              const ContourCurve &inC(contours[contour][curve]);
              ContourCurve outC(inC.start_pt(), inC.end_pt(), inC.continuation());

              outC.generation(inC.generation());
              out_contours[contour].push_back(outC);
            }
        }
      else
        {
          for (unsigned int contour = 0, end_contour = contours.size(); contour < end_contour; ++contour)
            {
              const ContourCurve &inC(contours[contour][curve]);
              if (inC.number_control_pts() > 0 && inC.control_pt_is_degenerate(0, flat_epsilon))
                {
                  /* replace with a line segment if all others are to become line segments */
                  if (num_non_linear == 0u)
                    {
                      const ContourCurve &inC(contours[contour][curve]);
                      ContourCurve outC(inC.start_pt(), inC.end_pt(), inC.continuation());

                      outC.generation(inC.generation());
                      out_contours[contour].push_back(outC);
                    }
                  else
                    {
                      /* replace with a flat conic, but make the control
                       * point closer to the end point it was originally
                       * closest to.
                       */
                      float d0 = (inC.start_pt() - inC.control_pt(0)).L1norm();
                      float d1 = (inC.end_pt() - inC.control_pt(0)).L1norm();
                      float f = (d0 < d1) ? 0.01f : 0.99f;

                      vec2 ctl = inC.start_pt() + f * (inC.end_pt() - inC.start_pt());
                      ContourCurve outC(inC.start_pt(), ctl, inC.end_pt(), inC.continuation());

                      outC.generation(inC.generation());
                      out_contours[contour].push_back(outC);
                    }
                }
              else
                {
                  out_contours[contour].push_back(inC);
                }
            }
        }
    }

  for (unsigned int contour = 0, end_contour = contours.size(); contour < end_contour; ++contour)
    {
      /* make the start point and end points the same as it was before filtering.
       * By just making the start and end bit-wise agree with the input, we
       * avoid changing closed curves to open curves by accident.
       */
      if (!out_contours[contour].empty())
        {
          out_contours[contour].front().start_pt(start_pts[contour]);
          out_contours[contour].back().end_pt(end_pts[contour]);

          /* make the start of the next curve the end point of the previous curve */
          for (unsigned int curve = 1, end_curve = out_contours[contour].size(); curve < end_curve; ++curve)
            {
              out_contours[contour][curve].start_pt(out_contours[contour][curve - 1].end_pt());
            }
        }

      /* swap out_contours[] with contours[] to "write" the data */
      contours[contour].swap(out_contours[contour]);
    }
}

astral::detail::ContourApproximator::
ContourApproximator(const CurveFetcherBase &in_Cs, float target_tol,
                    c_array<ApproximatedContour> quads,
                    enum mode_t pmode,
                    enum max_size_mode_t psz_mode,
                    bool split_singleton_contours,
                    enum split_cubics_at_cusp_mode_t split_cubic_cusps_mode,
                    c_array<const SourceTag> tags,
                    std::vector<SourceTag> *dst_tags):
  m_mode(pmode),
  m_sz_mode(psz_mode),
  m_split_singleton_contours(split_singleton_contours),
  m_error(0.0f)
{
  ASTRALassert(!in_Cs.empty());
  ASTRALassert(quads.size() == in_Cs.number_contours());

  const CurveFetcherBase *pCs(&in_Cs);
  std::vector<TessedContour> split_Cs_backing;
  CurveFetcher<TessedContour> split_Cs;
  std::vector<SourceTag> cusp_tags;

  if (split_cubic_cusps_mode == split_cubics_at_cusps)
    {
      std::vector<float> split_locations;
      unsigned int number_curves(in_Cs.get_curves(0).size());

      split_Cs_backing.resize(in_Cs.number_contours());
      for (unsigned int curve = 0; curve < number_curves; ++curve)
        {
          SourceTag src_tag;

          if (tags.empty())
            {
              src_tag.m_source_curve = curve;
              src_tag.m_source_range = range_type<float>(0.0f, 1.0f);
            }
          else
            {
              src_tag = tags[curve];
            }

          split_locations.clear();
          for (unsigned int contour = 0; contour < in_Cs.number_contours(); ++contour)
            {
              compute_cusps(in_Cs.get_curves(contour)[curve], &split_locations);
            }

          std::sort(split_locations.begin(), split_locations.end());
          merge_similiar_values(&split_locations);

          if (split_locations.empty())
            {
              cusp_tags.push_back(src_tag);
            }
          else
            {
              SourceTag S;

              S.m_source_curve = curve;
              S.m_source_range.m_begin = 0.0f;
              for (unsigned int i = 0, endi = split_locations.size(); i < endi; ++i)
                {
                  S.m_source_range.m_end = split_locations[i];
                  cusp_tags.push_back(S);
                }
              S.m_source_range.m_end = 1.0f;
              cusp_tags.push_back(S);
            }

          /* the values of split_locations are absolute to the entire curve,
           * but we will successively split the curve.
           */
          range_type<float> range_curve(0.0f, 1.0f);
          for (unsigned int i = 0, endi = split_locations.size(); i < endi; ++i)
            {
              float t_rel, t_curve, range;

              range = range_curve.m_end - range_curve.m_begin;
              t_curve = split_locations[i];
              t_rel = (t_curve - range_curve.m_begin) / range;

              split_locations[i] = t_rel;
              range_curve.m_begin = t_curve;
            }

          for (unsigned int contour = 0; contour < in_Cs.number_contours(); ++contour)
            {
              ContourCurve current_curve(in_Cs.get_curves(contour)[curve]);

              for (unsigned int i = 0, endi = split_locations.size(); i < endi; ++i)
                {
                  ContourCurveSplit split(false, current_curve, split_locations[i],
                                          ContourCurve::continuation_curve_cusp);

                  split_Cs_backing[contour].push_back(split.before_t());
                  current_curve = split.after_t();
                }
              split_Cs_backing[contour].push_back(current_curve);
            }
        }

      /* assign the CurveFetcher AFTER adding the curves; it should be enough
       * to do this just after resizeing split_Cs_backing, but tests have
       * showed that it needs to be done after adding the curves which makes
       * very little sense.
       */
      split_Cs = CurveFetcher<TessedContour>(make_c_array(split_Cs_backing));

      /* source from split_Cs instead */
      pCs = &split_Cs;
      tags = make_c_array(cusp_tags);
    }

  const CurveFetcherBase &Cs(*pCs);
  unsigned int number_curves(Cs.get_curves(0).size());

  m_tessed_contours.resize(Cs.number_contours());
  for (unsigned int i = 0; i < Cs.number_contours(); ++i)
    {
      quads[i].clear();
    }

  ASTRALassert(tags.size() == number_curves || tags.empty());
  if (dst_tags)
    {
      dst_tags->clear();
    }

  for (unsigned int curve = 0; curve < number_curves; ++curve)
    {
      bool should_split, has_quadratic;

      should_split = false;
      has_quadratic = false;
      for (unsigned int contour = 0; contour < Cs.number_contours(); ++contour)
        {
          const ContourCurve &p(Cs.get_curves(contour)[curve]);
          QuadraticBezierCurve Q;
          float error(0.0f);
          bool curve_touches_glue_cusp;

          ASTRALassert(Cs.get_curves(contour).size() == number_curves);

          /* if the curve's continuation type or the continuation
           * type of the next curve is continuation_curve_cusp,
           * then the cubic must be approximated by line segments
           * only.
           */
          curve_touches_glue_cusp = Cs.curve_has_glue_cusp(contour, curve);
          switch (p.type())
            {
            case ContourCurve::cubic_bezier:
              {
                if (curve_touches_glue_cusp)
                  {
                    error = error_to_line_appoximation(p);
                    if (p.generation() < generation_ignore_size && size_of_lines_contributes_to_error(m_sz_mode))
                      {
                        vec2 d(p.start_pt() - p.end_pt());
                        error = t_max(error, compute_error_from_size(d.magnitude()));
                      }
                  }
                else
                  {
                    error = compute_quadratic_appoximation(p, &Q);
                    has_quadratic = true;
                  }
              }
              break;

            case ContourCurve::conic_curve:
            case ContourCurve::conic_arc_curve:
              if (m_mode != approximate_to_conic_or_quadratic)
                {
                  error = compute_quadratic_appoximation(p, &Q);
                }
              else
                {
                  Q = QuadraticBezierCurve(p.start_pt(), p.control_pt(0), p.end_pt());
                }
              has_quadratic = true;
              break;

            case ContourCurve::quadratic_bezier:
              Q = QuadraticBezierCurve(p.start_pt(), p.control_pt(0), p.end_pt());
              has_quadratic = true;
              break;

            case ContourCurve::line_segment:
              break;

            default:
              ASTRALassert(!"Bad curve type");
              break;
            }

          if (m_mode == approximate_to_quadratic_error_to_biarc
              && p.type() != ContourCurve::line_segment)
            {
              error += compute_biarc_error_rel_length<double>(Q);
            }

          if (p.generation() < generation_ignore_size)
            {
              if (size_of_lines_contributes_to_error(m_sz_mode) && p.type() == ContourCurve::line_segment)
                {
                  vec2 d(p.start_pt() - p.end_pt());
                  error = t_max(error, compute_error_from_size(d.magnitude()));
                }
              else if (size_of_curve_contributes_to_error(m_sz_mode) && p.type() != ContourCurve::line_segment)
                {
                  error = t_max(error, compute_error_from_size(Q));
                }
            }

          should_split = should_split || (error > target_tol && target_tol > 0.0f)
            || (number_curves == 1 && m_split_singleton_contours);
        }

      if (dst_tags)
        {
          SourceTag src_tag;

          if (tags.empty())
            {
              src_tag.m_source_curve = curve;
              src_tag.m_source_range = range_type<float>(0.0f, 1.0f);
            }
          else
            {
              src_tag = tags[curve];
            }

          if (should_split)
            {
              SourceTag a, b;

              a.m_source_curve = b.m_source_curve = src_tag.m_source_curve;
              a.m_source_range.m_begin = src_tag.m_source_range.m_begin;
              a.m_source_range.m_end = b.m_source_range.m_begin = 0.5f * (src_tag.m_source_range.m_begin + src_tag.m_source_range.m_end);
              b.m_source_range.m_end = src_tag.m_source_range.m_end;
              dst_tags->push_back(a);
              dst_tags->push_back(b);
            }
          else
            {
              dst_tags->push_back(src_tag);
            }
        }

      for (unsigned int contour = 0; contour < Cs.number_contours(); ++contour)
        {
          const ContourCurve &p(Cs.get_curves(contour)[curve]);
          bool curve_touches_glue_cusp;

          curve_touches_glue_cusp = Cs.curve_has_glue_cusp(contour, curve);

          switch (p.type())
            {
            case ContourCurve::line_segment:
              if (should_split)
                {
                  ContourCurveSplit split(true, p);

                  add_line(&quads[contour], &m_tessed_contours[contour], has_quadratic, split.before_t());
                  add_line(&quads[contour], &m_tessed_contours[contour], has_quadratic, split.after_t());
                }
              else
                {
                  add_line(&quads[contour], &m_tessed_contours[contour], has_quadratic, p);
                }
              break;

            case ContourCurve::quadratic_bezier:
              if (should_split)
                {
                  ContourCurveSplit split(true, p);

                  add_quadratic(&quads[contour], &m_tessed_contours[contour], split.before_t());
                  add_quadratic(&quads[contour], &m_tessed_contours[contour], split.after_t());
                }
              else
                {
                  add_quadratic(&quads[contour], &m_tessed_contours[contour], p);
                }
              break;

            case ContourCurve::conic_arc_curve:
            case ContourCurve::conic_curve:
              if (should_split)
                {
                  ContourCurveSplit split(true, p);

                  add_conic(&quads[contour], &m_tessed_contours[contour], split.before_t());
                  add_conic(&quads[contour], &m_tessed_contours[contour], split.after_t());
                }
              else
                {
                  add_conic(&quads[contour], &m_tessed_contours[contour], p);
                }
              break;

            case ContourCurve::cubic_bezier:
              if (should_split)
                {
                  ContourCurveSplit split(true, p);

                  add_cubic(curve_touches_glue_cusp, &quads[contour], &m_tessed_contours[contour], has_quadratic, split.before_t());
                  add_cubic(curve_touches_glue_cusp, &quads[contour], &m_tessed_contours[contour], has_quadratic, split.after_t());
                }
              else
                {
                  add_cubic(curve_touches_glue_cusp, &quads[contour], &m_tessed_contours[contour], has_quadratic, p);
                }
              break;

            default:
              ASTRALassert(!"Invalid curve degree!");
            }
        }
    }
  post_process_approximation(quads);
}

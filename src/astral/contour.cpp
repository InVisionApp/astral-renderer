/*!
 * \file contour.cpp
 * \brief file path.cpp
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

#include <tuple>
#include <ostream>
#include <astral/contour.hpp>

#include "contour_approximator.hpp"
#include "generic_lod.hpp"

namespace
{
  class SantizedCurve
  {
  public:
    SantizedCurve(void):
      m_count(0)
    {}

    void
    push_back(const astral::ContourCurve &C)
    {
      ASTRALassert(m_count < m_backing.size());
      m_backing[m_count] = C;
      ++m_count;
    }

    void
    clear(void)
    {
      m_count = 0;
    }

    astral::c_array<const astral::ContourCurve>
    curves(void) const
    {
      return astral::c_array<const astral::ContourCurve>(m_backing.c_ptr(), m_count);
    }

  private:
    unsigned int m_count;
    astral::vecN<astral::ContourCurve, 3> m_backing;
  };

  /*
   * \param dst locaiton to which to add curves
   * \param curve orginal curve
   * \param rotated_curve curve rotated so the trace of the curve is horizontal
   */
  void
  handle_curve_traces_lines(SantizedCurve *dst,
                            const astral::ContourCurve &curve,
                            const astral::ContourCurve &rotated_curve)
  {
    using namespace astral;

    vecN<float, 2> solutions_backing, ignore_backing;
    c_array<float> solutions(solutions_backing), ignore(ignore_backing);

    rotated_curve.compute_critical_points(&solutions, &ignore);
    std::sort(solutions.begin(), solutions.end());

    /* walk the solutions to create the needed line segments */
    vec2 start(curve.start_pt());
    enum ContourCurve::continuation_t continuation(curve.continuation());
    float min_c_x(t_min(rotated_curve.start_pt().x(), rotated_curve.end_pt().x()));
    float max_c_x(t_max(rotated_curve.start_pt().x(), rotated_curve.end_pt().x()));

    for (unsigned int i = 0; i < solutions.size(); ++i)
      {
        if (solutions[i] >= 0.0f && solutions[i] <= 1.0f)
          {
            float px;

            /* ISSUE: this is fine for when the number of control
             *        points is one, but can be incorrect for
             *        cubics. Consider the following situation:
             *
             *  p0 = 0
             *  p1 = 4
             *  p2 = 0.5
             *  p3 = 1
             *
             * In this case the tangent at p3 should be going to
             * positive-x, but if we remove p2 (and keep p1) the
             * tangent is incorrect. This off by sign does not
             * matter for joins, but does matter for caps; dropping
             * p2 makes the cap point into the curve.
             *
             * TODO: add a paremeter if the curve is the first
             *       or last curve of a contour and if it is,
             *       then make sure the tangents are preserved.
             */
            px = rotated_curve.eval_at(solutions[i]).x();
            if (px < min_c_x || px > max_c_x)
              {
                vec2 p;

                p = curve.eval_at(solutions[i]);
                dst->push_back(ContourCurve(start, p, continuation));

                continuation = ContourCurve::continuation_curve_cusp;
                start = p;
              }
          }
      }

    dst->push_back(ContourCurve(start, curve.end_pt(), continuation));
  }

  bool
  points_are_collinear(astral::vec2 a, astral::vec2 b, astral::vec2 c, astral::vec2 *out_direction)
  {
    astral::vec2 v, w;
    float det;

    v = b - a;
    w = c - a;
    det = v.x() * w.y() - v.y() * w.x();

    if (dot(v, v) > dot(w, w))
      {
        *out_direction = v;
      }
    else
      {
        *out_direction = w;
      }

    return det == 0.0f;
  }

  bool
  sanitize_curve(astral::ContourCurve curve, SantizedCurve *dst)
  {
    using namespace astral;

    bool curves_modified(false);

    dst->clear();

    /* Note that we are doing absolute compare against zero.
     * This sanitizing is meant to catch and remove curves that
     * should never be allowed.
     */

    /* if the curve is a cubic that is actually a quadratic, replace it with a quadratic */
    if (curve.type() == ContourCurve::cubic_bezier)
      {
        detail::QuadraticBezierCurve Q;
        float error;

        error = detail::compute_quadratic_appoximation(curve, &Q);
        if (error == 0.0f)
          {
            curve = ContourCurve(Q[0], Q[1], Q[2], curve.continuation());
            curves_modified = true;
          }
      }

    if (curve.type() == ContourCurve::conic_curve || curve.type() == ContourCurve::conic_arc_curve)
      {
        if (curve.conic_weight() <= 0.0f)
          {
            curve = ContourCurve(curve.start_pt(), curve.end_pt(), curve.continuation());
            curves_modified = true;
          }
        else if (curve.conic_weight() == 1.0f)
          {
            curve = ContourCurve(curve.start_pt(), curve.control_pt(0), curve.end_pt(), curve.continuation());
            curves_modified = true;
          }
      }

    if (curve.start_pt() == curve.end_pt())
      {
        if (curve.type() == ContourCurve::line_segment)
          {
            /* remove degenerate line segment */
            curves_modified = true;
          }
        else if (curve.type() == ContourCurve::quadratic_bezier)
          {
            /* if the curve is a quadratic that starts and ends in the same place,
             * replace it with two line segments. We have that the curve is
             *
             * f(t) = At^2 + 2t(1-t)B + A(1-t)^2
             *
             * translating by -A gives the translation of the curve g(t)
             *
             * g(t) = 2t(1-t)(B - A)
             *
             * which is furthest from the origin at t = 0.5. Thus, this
             * oddball quadratic curve goes from A to f(0.5) then back to A.
             * A simple computation gives that
             *
             * f(0.5) = 0.5f * (A + B)
             */
            vec2 p;

            p = 0.5f * (curve.start_pt() + curve.control_pt(0));

            curves_modified = true;
            dst->push_back(ContourCurve(curve.start_pt(), p, curve.continuation()));
            dst->push_back(ContourCurve(p, curve.end_pt(), ContourCurve::continuation_curve));
          }
        else if (curve.type() == ContourCurve::conic_curve || curve.type() == ContourCurve::conic_arc_curve)
          {
            /* This is a bigger hassle than the quadratic curve case. Translating
             * start_pt() to (0, 0), we have the curve is
             *
             *             B * (2wt - 2wt^2)
             *   g(t) = --------------------------------
             *            1 + t(2w - 2) + t^2 (2 - 2w)
             *
             * thus we are left to optimize h(t) on [0, 1] where
             *
             *              2wt - 2wt^2
             *   h(t) = ---------------------------
             *            1 + t(2w - 2) + t^2(2 - 2w)
             *
             */
            Polynomial<float, 2> num, den;

            num.coeff(0) = 0.0f;
            num.coeff(1) = 2.0f * curve.conic_weight();
            num.coeff(2) = -2.0f * curve.conic_weight();

            den.coeff(0) = 1.0f;
            den.coeff(1) = 2.0f * curve.conic_weight() - 2.0f;
            den.coeff(2) = 2.0f - 2.0f * curve.conic_weight();

            /* We need to set the derivative of the fraction to zero
             * which means solving for
             *
             * num * den.derivative() - den * num.derivative() = 0.0.
             */
            Polynomial<float, 3> Q;

            Q = num  * den.derivative() - den * num.derivative();

            /* Now, the leading coefficient of that polynomial
             * is zero because both num and den are polynomials
             * of the same degree.
             */
            Polynomial<float, 2> QQ;
            vecN<float, 2> solutions_backing;
            c_array<float> solutions(solutions_backing);
            unsigned int num_solutions;
            bool is_degenerate(true);
            float maxV(0.0f), maxAt;

            QQ = Q.without_leading_term();
            num_solutions = solve_polynomial(QQ, solutions);

            for (unsigned int i = 0; i < num_solutions; ++i)
              {
                if (solutions[i] >= 0.0f && solutions[i] <= 1.0f)
                  {
                    float V;

                    V = t_abs(num.eval(solutions[i]) / den.eval(solutions[i]));
                    if (is_degenerate || V > maxV)
                      {
                        maxV = t_max(V, maxV);
                        maxAt = solutions[i];
                      }

                    is_degenerate = false;
                  }
              }

            curves_modified = true;
            if (!is_degenerate)
              {
                vec2 p;

                p = curve.eval_at(maxAt);
                dst->push_back(ContourCurve(curve.start_pt(), p, curve.continuation()));
                dst->push_back(ContourCurve(p, curve.end_pt(), ContourCurve::continuation_curve));
              }
          }
        else if (curve.type() == ContourCurve::cubic_bezier)
          {
            vec2 line_direction;
            if (points_are_collinear(curve.control_pt(0), curve.control_pt(1), curve.end_pt(), &line_direction))
              {
                /* the curve traces a line that goes to a point and then comes back,
                 * we need to compute that line
                 */
                Transformation map_line_segment_to_x_axis;

                map_line_segment_to_x_axis.m_matrix.row_col(0, 0) = line_direction.x();
                map_line_segment_to_x_axis.m_matrix.row_col(0, 1) = line_direction.y();
                map_line_segment_to_x_axis.m_matrix.row_col(1, 0) = -line_direction.y();
                map_line_segment_to_x_axis.m_matrix.row_col(1, 1) = line_direction.x();

                handle_curve_traces_lines(dst, curve, ContourCurve(curve, map_line_segment_to_x_axis));
                curves_modified = true;
              }
            else
              {
                dst->push_back(curve);
              }
          }
        else
          {
            dst->push_back(curve);
          }
        }
    else if (curve.type() != ContourCurve::line_segment && curve.flatness() == 0.0f)
      {
        /* If the control point/points is/are not within the line segment
         * fromt start_pt() to end_pt(), we need to realize the curve
         * as two line (or three line segments) to get the trace correct.
         */

        /* create a transformation that maps start_pt() to (0, 0)
         * and maps the line segment conneting start_pt() to end_pt()
         * to the x-axis.
         */
        Transformation map_line_segment_to_x_axis;
        vec2 V;

        V = curve.end_pt() - curve.start_pt();
        map_line_segment_to_x_axis.m_matrix.row_col(0, 0) = V.x();
        map_line_segment_to_x_axis.m_matrix.row_col(0, 1) = V.y();
        map_line_segment_to_x_axis.m_matrix.row_col(1, 0) = -V.y();
        map_line_segment_to_x_axis.m_matrix.row_col(1, 1) = V.x();

        handle_curve_traces_lines(dst, curve, ContourCurve(curve, map_line_segment_to_x_axis));
        curves_modified = true;
      }
    else
      {
        dst->push_back(curve);
      }

    return curves_modified;
  }

}

class astral::Contour::DataGenerator:public reference_counted<DataGenerator>::non_concurrent
{
public:
  DataGenerator(void):
    m_stroke_mode(CA::size_of_lines_and_curves_contributes_to_error),
    m_item_path_mode(CA::ignore_size_of_curve)
  {
    m_fill_mode[contour_fill_approximation_allow_long_curves] = CA::ignore_size_of_curve;
    m_fill_mode[contour_fill_approximation_tessellate_long_curves] = CA::size_of_only_curves_contributes_to_error;
  }

  c_array<const ContourCurve>
  item_path_approximated_geometry(const ContourData *C, float tol, float *out_tol,
                                    c_array<const detail::ContourApproximator::SourceTag> *out_tags = nullptr)
  {
    const auto &e(m_item_path.fetch(tol, m_item_path_mode, C));
    if (out_tol)
      {
        *out_tol = e.error();
      }

    if (out_tags)
      {
        *out_tags = e.src_tags();
      }

    return e.value();
  }

  c_array<const ContourCurve>
  fill_approximated_geometry(const ContourData *C, float tol,
                             enum contour_fill_approximation_t tp, float *out_tol,
                             c_array<const detail::ContourApproximator::SourceTag> *out_tags = nullptr)
  {
    const auto &e(m_fill[tp].fetch(tol, m_fill_mode[tp], C));
    if (out_tol)
      {
        *out_tol = e.error();
      }
    if (out_tags)
      {
        *out_tags = e.src_tags();
      }

    return e.value();
  }

  const FillSTCShader::CookedData&
  fill_render_data(const ContourData *C, float tol, RenderEngine &engine,
                   enum contour_fill_approximation_t tp, float *out_tol)
  {
    const auto &e(m_fill[tp].fetch(tol, m_fill_mode[tp], C));
    if (out_tol)
      {
        *out_tol = e.error();
      }
    return e.render_data<0>(*C, engine);
  }

  c_array<const ContourCurve>
  stroke_approximated_geometry(const ContourData *C, float tol, float *out_tol)
  {
    const auto &e(m_stroke.fetch(tol, m_stroke_mode, C));
    if (out_tol)
      {
        *out_tol = e.error();
      }

    return e.value();
  }

  const StrokeShader::CookedData&
  stroke_render_data(const ContourData *C,
                     float tol, RenderEngine &engine,
                     float *out_tol)
  {
    const auto &e(m_stroke.fetch(tol, m_stroke_mode, C));
    if (out_tol)
      {
        *out_tol = e.error();
      }

    if (e.render_data_ready<1>())
      {
        e.reset_render_data<1>();
      }

    return e.render_data<0>(*C, engine);
  }

  const StrokeShader::SimpleCookedData&
  simple_stroke_render_data(const ContourData *C,
                     float tol, RenderEngine &engine,
                     float *out_tol)
  {
    const auto &e(m_stroke.fetch(tol, m_stroke_mode, C));
    if (out_tol)
      {
        *out_tol = e.error();
      }

    if (e.render_data_ready<0>())
      {
        return e.render_data<0>(*C, engine).simple_data();
      }
    else
      {
        return e.render_data<1>(*C, engine);
      }
  }

private:
  class EmptyRenderDataElement
  {};

  typedef std::tuple<FillSTCShader::CookedData> FillRenderData;
  typedef std::tuple<StrokeShader::CookedData, StrokeShader::SimpleCookedData> StrokeRenderData;
  typedef std::tuple<EmptyRenderDataElement> EmptyRenderData;

  static
  void
  generate_render_data(c_array<const ContourCurve> input,
                       const ContourData &raw_contour,
                       RenderEngine &engine, FillSTCShader::CookedData *output)
  {
    FillSTCShader::Data geometry;

    ASTRALunused(raw_contour);
    geometry.add_contour(input);
    *output = FillSTCShader::create_cooked_data(engine, geometry);
  }

  static
  void
  generate_render_data(c_array<const ContourCurve> input,
                       const ContourData &raw_contour,
                       RenderEngine &engine, StrokeShader::CookedData *output)
  {
    StrokeShader::RawData builder;

    if (input.empty())
      {
        builder.add_point_cap(raw_contour.start());
      }
    else
      {
        builder.add_contour(raw_contour.closed(), input);
      }
    StrokeShader::create_render_data(engine, builder, output);
  }

  static
  void
  generate_render_data(c_array<const ContourCurve> input,
                       const ContourData &raw_contour,
                       RenderEngine &engine, StrokeShader::SimpleCookedData *output)
  {
    StrokeShader::RawData builder;

    if (input.empty())
      {
        builder.add_point_cap(raw_contour.start());
      }
    else
      {
        builder.add_contour(raw_contour.closed(), input);
      }
    StrokeShader::create_render_data(engine, builder, output);
  }

  static
  void
  generate_render_data(c_array<const ContourCurve>,
                       const ContourData&,
                       RenderEngine&, EmptyRenderData*)
  {
  }

  /* \tparam mode approximation mode to pass to ContourApproximator, this mode
   *              determines how curves are approximated
   * \tparam split_singleton_contours if true, if a contour only has a single
   *                                           curve, then that curve is split
   *                                           into two. This is needed for stroking
   * \param T the object type to return in render_data()
   */
  template<enum detail::ContourApproximator::mode_t mode,
           bool split_singleton_contours,
           enum detail::ContourApproximator::split_cubics_at_cusp_mode_t split_cubic_cusps_mode,
           typename T>
  class Entry
  {
  public:
    /*!
     * Ctor.
     * \param sz_mode determines if long curves are to be split
     * \param C source ContourData values
     */
    explicit
    Entry(enum detail::ContourApproximator::max_size_mode_t sz_mode, const ContourData *C):
      m_render_data_ready(false)
    {
      m_approximation = ASTRALnew detail::ContourApproximator(C, &m_quadratic_data_backing, mode,
                                                              sz_mode, split_singleton_contours,
                                                              split_cubic_cusps_mode,
                                                              &m_src_tags);
      m_error = m_approximation->error();
    }

    c_array<const ContourCurve>
    value(void) const
    {
      return make_c_array(m_quadratic_data_backing);
    }

    c_array<const detail::ContourApproximator::SourceTag>
    src_tags(void) const
    {
      return make_c_array(m_src_tags);
    }

    unsigned int
    size(void) const
    {
      return m_quadratic_data_backing.size();
    }

    float
    error(void) const { return m_error; }

    Entry
    create_refinement(enum detail::ContourApproximator::max_size_mode_t, const ContourData*)
    {
      Entry return_value(*m_approximation, make_c_array(m_src_tags));
      finalize();
      return return_value;
    }

    void
    finalize(void) { m_approximation = nullptr; }

    bool
    finalized(void) const { return m_approximation.get() == nullptr; }

    template<unsigned int N>
    const typename std::tuple_element<N, T>::type&
    render_data(const ContourData &raw_contour, RenderEngine &engine) const
    {
      if (!m_render_data_ready[N])
        {
          m_render_data_ready[N] = true;
          generate_render_data(value(), raw_contour, engine, &std::get<N>(m_render_data));
        }
      return std::get<N>(m_render_data);
    }

    template<unsigned int N>
    bool
    render_data_ready(void) const
    {
      return m_render_data_ready[N];
    }

    template<unsigned int N>
    void
    reset_render_data(void) const
    {
      if (m_render_data_ready[N])
        {
          m_render_data_ready[N] = false;
          std::get<N>(m_render_data) = typename std::tuple_element<N, T>::type();
        }
    }

  private:
    explicit
    Entry(const detail::ContourApproximator &A, c_array<const detail::ContourApproximator::SourceTag> A_tags):
      m_render_data_ready(false)
    {
      m_approximation = A.create_refinement(A_tags, &m_quadratic_data_backing, &m_src_tags);
      m_error = m_approximation->error();
    }

    float m_error;
    detail::ContourApproximator::ApproximatedContour m_quadratic_data_backing;
    std::vector<detail::ContourApproximator::SourceTag> m_src_tags;
    reference_counted_ptr<const detail::ContourApproximator> m_approximation;
    mutable T m_render_data;
    mutable vecN<bool, std::tuple_size<T>::value> m_render_data_ready;
  };

  /* NOTE: Stroking requires that no contour is a single curve; this requirement
   *       comes from the implementation of the shaders in the GL3 backend.
   */
  typedef detail::ContourApproximator CA;
  typedef Entry<CA::approximate_to_quadratic, false, CA::dont_split_cubics_at_cusps, FillRenderData> FillEntry;
  typedef Entry<CA::approximate_to_quadratic_error_to_biarc, true, CA::split_cubics_at_cusps, StrokeRenderData> StrokeEntry;
  typedef Entry<CA::approximate_to_conic_or_quadratic, false, CA::dont_split_cubics_at_cusps, EmptyRenderData> ItemPathEntry;

  enum CA::max_size_mode_t m_stroke_mode, m_item_path_mode;
  vecN<enum CA::max_size_mode_t, number_contour_fill_approximation> m_fill_mode;

  vecN<detail::GenericLOD<FillEntry>, number_contour_fill_approximation> m_fill;
  detail::GenericLOD<StrokeEntry> m_stroke;
  detail::GenericLOD<ItemPathEntry> m_item_path;
};

///////////////////////////////
// astral::ContourData methods
static const float MAX_ARC_ANGLE = 0.5f * ASTRAL_PI;

astral::ContourData&
astral::ContourData::
operator=(ContourData &&obj) noexcept
{
  if (this != &obj)
    {
      m_last_end_pt = obj.m_last_end_pt;
      m_closed = obj.m_closed;
      m_santize_curves_on_adding = obj.m_santize_curves_on_adding;
      m_sanitized = obj.m_sanitized;
      m_bb = obj.m_bb;
      m_control_point_bb = obj.m_control_point_bb;
      m_curves = std::move(obj.m_curves);
    }

  return *this;
}

void
astral::ContourData::
close(bool force_add)
{
  ASTRALassert(!m_closed);
  if (!m_curves.empty() && (force_add || m_curves.front().start_pt() != m_curves.back().end_pt()))
    {
      line_to(m_curves.front().start_pt(), ContourCurve::not_continuation_curve);
    }
  m_closed = true;
}

astral::ContourData
astral::ContourData::
reverse(void) const
{
  ContourData R;
  unsigned int N(m_curves.size());

  R.m_closed = m_closed;
  R.m_santize_curves_on_adding = m_santize_curves_on_adding;
  R.m_sanitized = m_sanitized;
  R.m_bb = m_bb;
  R.m_control_point_bb = m_control_point_bb;
  R.m_join_bb = m_join_bb;
  R.m_curves.reserve(N);

  if (N > 0u)
    {
      for (unsigned int dst = 0, src = N - 1u; dst < N; ++dst, --src)
        {
          enum ContourCurve::continuation_t ct;
          unsigned int next_src(src + 1u);

          next_src = (next_src < N) ? next_src : 0;
          ct = m_curves[next_src].continuation();
          R.m_curves.push_back(m_curves[src].reverse_curve(ct));
        }
      R.m_last_end_pt = R.m_curves.back().end_pt();
    }
  else
    {
      R.m_last_end_pt = m_last_end_pt;
    }

  return R;
}

void
astral::ContourData::
inplace_reverse(void)
{
  std::swap(m_start_pt, m_last_end_pt);
  if (m_curves.empty())
    {
      return;
    }

  enum ContourCurve::continuation_t first_ct(m_curves[0].continuation());
  for (unsigned int i = 0, endi = m_curves.size(); i < endi; ++i)
    {
      unsigned int next_curve(i + 1u);
      enum ContourCurve::continuation_t ct;

      ct = (next_curve < endi) ? m_curves[next_curve].continuation() : first_ct;
      m_curves[i] = m_curves[i].reverse_curve(ct);
    }
  std::reverse(m_curves.begin(), m_curves.end());
  mark_dirty();
}

void
astral::ContourData::
make_curve_first(unsigned int S)
{
  ASTRALassert(S < m_curves.size());
  ASTRALassert(m_closed);

  if (S == 0)
    {
      return;
    }

  std::vector<ContourCurve> tmp;

  for (unsigned int i = S, endi = m_curves.size(); i < endi; ++i)
    {
      tmp.push_back(m_curves[i]);
    }
  for (unsigned int i = 0; i < S; ++i)
    {
      tmp.push_back(m_curves[i]);
    }

  ASTRALassert(tmp.size() == m_curves.size());
  m_curves.swap(tmp);

  m_start_pt = m_curves.front().start_pt();
  m_last_end_pt = m_curves.back().end_pt();
  mark_dirty();
}

unsigned int
astral::ContourData::arc_curve_stats::
number_arcs(float angle)
{
  const float two_pi(2.0f * ASTRAL_PI);
  unsigned int return_value(1);

  angle = t_abs(angle);
  angle = angle - two_pi * t_floor(angle / two_pi);

  if (angle <= MAX_ARC_ANGLE)
    {
      return return_value;
    }

  return_value += int(angle / MAX_ARC_ANGLE);
  return return_value;
}

void
astral::ContourData::
arc_to(float angle, vec2 end_pt,
       enum ContourCurve::continuation_t cont_tp,
       arc_curve_stats *out_data)
{
  const float two_pi(2.0f * ASTRAL_PI);
  const float thresh(1e-5);
  float abs_angle(t_abs(angle));
  float dir_angle(angle > 0.0f ? 1.0f : -1.0f);
  vec2 start_pt(m_last_end_pt);
  unsigned int max_out_sz;

  if (m_last_end_pt == end_pt && m_santize_curves_on_adding)
    {
      if (out_data)
        {
          out_data->m_number_curves = 0;
        }
      return;
    }

  /* NOTE: the curves added in arc_to() are NOT filtered,
   *       because their nature is that they are never degenerate.
   */
  m_last_end_pt = end_pt;

  max_out_sz = (out_data) ? out_data->m_parameter_lengths.size() : 0u;
  abs_angle = abs_angle - two_pi * t_floor(abs_angle / two_pi);

  if (abs_angle <= MAX_ARC_ANGLE)
    {
      if (abs_angle < thresh)
        {
          m_curves.push_back(ContourCurve(start_pt, end_pt, cont_tp));
        }
      else
        {
          m_curves.push_back(ContourCurve(start_pt, dir_angle * abs_angle, end_pt, cont_tp));
        }

      if (out_data)
        {
          if (!out_data->m_parameter_lengths.empty())
            {
              out_data->m_parameter_lengths[0] = 1.0f;
            }
          out_data->m_number_curves = 1u;
        }

      update_bbs();
      return;
    }

  /* TODO:
   *  Let ContourData be a friend to ContourCurve so that
   *  it can directly set the conic weight and angle values
   *  instead of making the ctor recompute some of these
   *  values.
   */

  float phi, s, c, A, radius;
  float rel(1.0f / abs_angle);
  vec2 m(0.5f * (start_pt + end_pt));
  vec2 v(end_pt - start_pt);
  vec2 n(-v.y(), v.x());
  vec2 center, start_center;
  unsigned int current(0);

  c = t_cos(0.5f * abs_angle);
  s = t_sin(0.5f * abs_angle);
  A = 0.5f * c / s;
  center = m + (dir_angle * A) * n;
  start_center = start_pt - center;
  phi = start_center.atan();
  radius = start_center.magnitude();

  while (abs_angle > MAX_ARC_ANGLE)
    {
      float phi_plus;
      vec2 ed;

      phi_plus = phi + dir_angle * MAX_ARC_ANGLE;
      ed = center + radius * vec2(t_cos(phi_plus), t_sin(phi_plus));

      m_curves.push_back(ContourCurve(start_pt, dir_angle * MAX_ARC_ANGLE, ed, cont_tp));
      m_bb.union_box(m_curves.back().tight_bounding_box());

      start_pt = ed;
      phi = phi_plus;
      cont_tp = ContourCurve::continuation_curve;
      abs_angle -= MAX_ARC_ANGLE;

      if (current < max_out_sz)
        {
          out_data->m_parameter_lengths[current] = MAX_ARC_ANGLE * rel;
        }
      ++current;
    }

  if (abs_angle < thresh)
    {
      m_curves.push_back(ContourCurve(start_pt, end_pt, cont_tp));
    }
  else
    {
      m_curves.push_back(ContourCurve(start_pt, dir_angle * abs_angle, end_pt, cont_tp));
    }
  update_bbs();

  if (current < max_out_sz)
    {
      out_data->m_parameter_lengths[current] = abs_angle * rel;
    }
  ++current;

  if (out_data)
    {
      out_data->m_number_curves = current;
    }
}

void
astral::ContourData::
make_as_rounded_rect(const RoundedRect &rect, enum contour_direction_t d,
                     RoundedRect::Point starting_point)
{
  clear();

  /* here are the points or a RoundedRect and rect */
  vecN<vec2, 8> rr_pts;
  vecN<vec2, 4> pts;
  vecN<bool, 4> null_corner_radius(false);
  enum ContourCurve::continuation_t ct(ContourCurve::not_continuation_curve);

  /* The point ordering has been carefully arranged to match the
   * ordering from RoundedRect::Point::point_index().
   */
  pts[0] = rect.point(Rect::minx_miny_corner);
  null_corner_radius[0] = rect.m_corner_radii[Rect::minx_miny_corner].x() <= 0.0f || rect.m_corner_radii[Rect::minx_miny_corner].y() <= 0.0f;
  if (null_corner_radius[0])
    {
      rr_pts[7] = rr_pts[0] = pts[0];
    }
  else
    {
      rr_pts[7] = pts[0] + vec2(0.0f, rect.m_corner_radii[Rect::minx_miny_corner].y());
      rr_pts[0] = pts[0] + vec2(rect.m_corner_radii[Rect::minx_miny_corner].x(), 0.0f);
    }

  pts[1] = rect.point(Rect::maxx_miny_corner);
  null_corner_radius[1] = rect.m_corner_radii[Rect::maxx_miny_corner].x() <= 0.0f || rect.m_corner_radii[Rect::maxx_miny_corner].y() <= 0.0f;
  if (null_corner_radius[1])
    {
      rr_pts[1] = rr_pts[2] = pts[1];
    }
  else
    {
      rr_pts[1] = pts[1] - vec2(rect.m_corner_radii[Rect::maxx_miny_corner].x(), 0.0f);
      rr_pts[2] = pts[1] + vec2(0.0f, rect.m_corner_radii[Rect::maxx_miny_corner].y());
    }

  pts[2] = rect.point(Rect::maxx_maxy_corner);
  null_corner_radius[2] = rect.m_corner_radii[Rect::maxx_maxy_corner].x() <= 0.0f || rect.m_corner_radii[Rect::maxx_maxy_corner].y() <= 0.0f;
  if (null_corner_radius[2])
    {
      rr_pts[3] = rr_pts[4] = pts[2];
    }
  else
    {
      rr_pts[3] = pts[2] - vec2(0.0f, rect.m_corner_radii[Rect::maxx_maxy_corner].y());
      rr_pts[4] = pts[2] - vec2(rect.m_corner_radii[Rect::maxx_maxy_corner].x(), 0.0f);
    }

  pts[3] = rect.point(Rect::minx_maxy_corner);
  null_corner_radius[3] = rect.m_corner_radii[Rect::minx_maxy_corner].x() <= 0.0f || rect.m_corner_radii[Rect::minx_maxy_corner].y() <= 0.0f;
  if (null_corner_radius[3])
    {
      rr_pts[5] = rr_pts[6] = pts[3];
    }
  else
    {
      rr_pts[5] = pts[3] + vec2(rect.m_corner_radii[Rect::minx_maxy_corner].x(), 0.0f);
      rr_pts[6] = pts[3] - vec2(0.0f, rect.m_corner_radii[Rect::minx_maxy_corner].y());
    }

  unsigned int S(starting_point.point_index());

  start(rr_pts[S]);
  for (unsigned int i = 1; i < 8u; ++i)
    {
      unsigned int I;

      I = (i + S) & 7u;

      /* If I is even then we are connecting to an arc */
      if ((I & 1u) == 0u)
        {
          unsigned int ptI;

          ptI = (I >> 1u);
          if (!null_corner_radius[ptI])
            {
              conic_to(ASTRAL_HALF_SQRT2, pts[ptI], rr_pts[I], ct);
            }
        }
      else if (rr_pts[I] != m_last_end_pt)
        {
          line_to(rr_pts[I], ct);
        }
    }

  /* now also do the closing arc */
  if ((S & 1u) == 0u && !null_corner_radius[S >> 1u])
    {
      conic_close(ASTRAL_HALF_SQRT2, pts[S >> 1u], ct);
    }
  else
    {
      close();
    }

  if (d == contour_direction_counter_clockwise)
    {
      inplace_reverse();
    }
}

void
astral::ContourData::
make_as_rect(const Rect &rect, enum contour_direction_t d,
             enum Rect::corner_t starting_point)
{
  clear();

  vecN<vec2, 4> pts;
  unsigned int S(Rect::point_index(starting_point));
  enum ContourCurve::continuation_t ct(ContourCurve::not_continuation_curve);

  for (unsigned int i = 0; i < 4u; ++i)
    {
      unsigned int I(i + S);
      enum Rect::corner_t src_p;

      I = (i + S) & 3u;
      src_p = Rect::corner_from_point_index(I);
      pts[i] = rect.point(src_p);
    }

  if (d == contour_direction_counter_clockwise)
    {
      start(pts[0]);
      line_to(pts[3], ct);
      line_to(pts[2], ct);
      line_to(pts[1], ct);
      line_close(ct);
    }
  else
    {
      start(pts[0]);
      line_to(pts[1], ct);
      line_to(pts[2], ct);
      line_to(pts[3], ct);
      line_close(ct);
    }
}

void
astral::ContourData::
make_as_oval(const Rect &rect, enum contour_direction_t d,
             enum Rect::side_t starting_point)
{
  clear();

  vecN<vec2, 4> pts, ctl_pts;
  enum ContourCurve::continuation_t ct(ContourCurve::not_continuation_curve);

  ASTRALassert(0 == Rect::point_index(Rect::miny_side));

  ctl_pts[0] = rect.point(Rect::minx_miny_corner);
  ctl_pts[1] = rect.point(Rect::maxx_miny_corner);
  ctl_pts[2] = rect.point(Rect::maxx_maxy_corner);
  ctl_pts[3] = rect.point(Rect::minx_maxy_corner);

  pts[0] = 0.5f * (ctl_pts[0] + ctl_pts[1]);
  pts[1] = 0.5f * (ctl_pts[1] + ctl_pts[2]);
  pts[2] = 0.5f * (ctl_pts[2] + ctl_pts[3]);
  pts[3] = 0.5f * (ctl_pts[3] + ctl_pts[0]);

  unsigned int S(Rect::point_index(starting_point));

  start(pts[S]);
  for (unsigned int i = 1; i < 4u; ++i)
    {
      unsigned int I;

      I = i + S;
      I = (I >= 4u) ? I - 4u : I;
      conic_to(ASTRAL_HALF_SQRT2, ctl_pts[I], pts[I], ct);
    }
  conic_close(ASTRAL_HALF_SQRT2, ctl_pts[S], ct);

  if (d == contour_direction_counter_clockwise)
    {
      inplace_reverse();
    }
}

void
astral::ContourData::
set_values(const ContourData &obj)
{
  *this = obj;
  mark_dirty();
}

void
astral::ContourData::
curve_to(const ContourCurve &curve)
{
  if (m_curves.empty())
    {
      m_start_pt = m_last_end_pt = curve.start_pt();
    }

  ASTRALassert(curve.start_pt() == m_last_end_pt);

  if (m_santize_curves_on_adding)
    {
      SantizedCurve s;

      sanitize_curve(curve, &s);
      c_array<const ContourCurve> curves(s.curves());

      for (const ContourCurve &c : curves)
        {
          m_curves.push_back(c);
        }

      if (!curves.empty())
        {
          update_bbs();
          mark_dirty();
        }
    }
  else
    {
      m_sanitized = false;
      m_curves.push_back(curve);
      update_bbs();
      mark_dirty();
    }

  m_last_end_pt = curve.end_pt();
}

bool
astral::ContourData::
sanitize(void)
{
  if (m_sanitized)
    {
      return false;
    }

  bool curves_modified(false);
  std::vector<ContourCurve> tmp;

  for (const ContourCurve &curve : m_curves)
    {
      SantizedCurve s;
      bool m;

      m = sanitize_curve(curve, &s);
      curves_modified = curves_modified || m;
      for (const ContourCurve &c : s.curves())
        {
          tmp.push_back(c);
        }
    }

  if (curves_modified)
    {
      tmp.swap(m_curves);
      m_bb.clear();
      m_join_bb.clear();
      m_control_point_bb.clear();
      for (const ContourCurve &curve : m_curves)
        {
          m_bb.union_box(curve.tight_bounding_box());
          m_control_point_bb.union_box(curve.control_point_bounding_box());
          if (curve.continuation() == ContourCurve::not_continuation_curve)
            {
              m_join_bb.union_point(curve.start_pt());
            }
        }
      mark_dirty();
    }

  m_sanitized = true;

  return curves_modified;
}

namespace std
{
  ostream&
  operator<<(ostream &ostr, const astral::ContourData &contour)
  {
    if (contour.closed())
      {
        ostr << "[ ";
      }
    else
      {
        ostr << "{ ";
      }

    astral::c_array<const astral::ContourCurve> curves(contour.curves());
    for (unsigned int i = 0, endi = curves.size(); i < endi; ++i)
      {
        ostr << curves[i].start_pt() << " ";
        if (curves[i].number_control_pts() != 0)
          {
            ostr << " [[";
            for (unsigned int c = 0; c < curves[i].number_control_pts(); ++c)
              {
                ostr << curves[i].control_pt(c) << " ";
              }

            if (curves[i].is_conic())
              {
                ostr << "w = " << curves[i].conic_weight() << " ";
              }

            ostr << "]] ";
          }
      }

    if (contour.closed())
      {
        ostr << "]\n";
      }
    else
      {
        if (!curves.empty())
          {
            ostr << curves.back().end_pt() << " ";
          }

        ostr << "}\n";
      }

    return ostr;
  }

} //namespace std

///////////////////////////////////
// astral::Contour methods
astral::Contour::
Contour(void)
{}

astral::Contour::
Contour(const ContourData &obj):
  ContourData(obj)
{}

astral::Contour::
~Contour()
{
}

void
astral::Contour::
mark_dirty(void)
{
  m_data_generator = nullptr;
}

astral::Contour::DataGenerator&
astral::Contour::
data_generator(void) const
{
  if (!m_data_generator)
    {
      m_data_generator = ASTRALnew DataGenerator();
    }

  return *m_data_generator;
}

astral::c_array<const astral::ContourCurve>
astral::Contour::
item_path_approximated_geometry(float tol, float *out_tol) const
{
  return data_generator().item_path_approximated_geometry(this, tol, out_tol);
}

astral::c_array<const astral::ContourCurve>
astral::Contour::
fill_approximated_geometry(float tol, enum contour_fill_approximation_t ct, float *out_tol) const
{
  return data_generator().fill_approximated_geometry(this, tol, ct, out_tol);
}

const astral::FillSTCShader::CookedData&
astral::Contour::
fill_render_data(float tol, RenderEngine &engine,
                 enum contour_fill_approximation_t ct, float *out_tol) const
{
  return data_generator().fill_render_data(this, tol, engine, ct, out_tol);
}

astral::c_array<const astral::ContourCurve>
astral::Contour::
stroke_approximated_geometry(float tol, float *out_tol) const
{
  return data_generator().stroke_approximated_geometry(this, tol, out_tol);
}

const astral::StrokeShader::CookedData&
astral::Contour::
stroke_render_data(float tol, RenderEngine &engine, float *out_tol) const
{
  return data_generator().stroke_render_data(this, tol, engine, out_tol);
}

const astral::StrokeShader::SimpleCookedData&
astral::Contour::
simple_stroke_render_data(float tol, RenderEngine &engine, float *out_tol) const
{
  return data_generator().simple_stroke_render_data(this, tol, engine, out_tol);
}

astral::Contour::PointQueryResult
astral::Contour::
distance_to_contour(float tol, const vec2 &pt, float distance_cull) const
{
  c_array<const ContourCurve> curves;
  c_array<const detail::ContourApproximator::SourceTag> tags;
  PointQueryResult w;

  /* if the point is outside of bb and its distance is greater
   * than max_value with max_value >= 0.0, then early out
   */
  if (distance_cull >= 0.0f)
    {
      const BoundingBox<float> &bb(bounding_box());

      if (!bb.contains(pt))
        {
          float D;

          D = bb.distance_to_boundary(pt);
          if (D >= distance_cull)
            {
              return w;
            }
        }
    }

  curves = data_generator().item_path_approximated_geometry(this, tol, nullptr, &tags);
  if (curves.empty())
    {
      return w;
    }

  w.m_distance = detail::compute_l1_distace_to_curve(pt, curves.front(), &w.m_winding_impact, &w.m_closest_point_t);
  w.m_closest_curve = tags.front().m_source_curve;
  w.m_closest_point_t = tags.front().remap_to_source(w.m_closest_point_t);

  for (unsigned int i = 1; i < curves.size(); ++i)
    {
      const ContourCurve &curve(curves[i]);
      float dist, t;

      dist = detail::compute_l1_distace_to_curve(pt, curve, &w.m_winding_impact, &t);
      if (dist < w.m_distance)
        {
          w.m_distance = dist;
          w.m_closest_curve = tags[i].m_source_curve;
          w.m_closest_point_t = tags[i].remap_to_source(t);
        }
    }

  if (!closed())
    {
      /* we need to update the winding impact from the implicit closing line segment */

      /* the continuation type does not matter for the computation,
       * but the ctor needs a continuation type value.
       */
      ContourCurve curve(curves.back().end_pt(), curves.front().start_pt(), ContourCurve::not_continuation_curve);
      float ignored;

      /* we do not care about the distance, only the impact on winding */
      ignored = detail::compute_l1_distace_to_curve(pt, curve, &w.m_winding_impact, &ignored);
      ASTRALunused(ignored);
    }

  return w;
}

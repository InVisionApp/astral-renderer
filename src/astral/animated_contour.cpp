/*!
 * \file animated_contour.cpp
 * \brief file animated_contour.cpp
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

#include <astral/animated_contour.hpp>

#include "animated_contour_util.hpp"
#include "contour_approximator.hpp"
#include "generic_lod.hpp"

namespace
{
  template<typename T>
  void
  set_as_collapse(bool contours_are_closed,
                  astral::ContourData *dst_contour, astral::c_array<const T> src_contour,
                  astral::ContourData *dst_pt, astral::vec2 src_pt)
  {
    dst_contour->start(src_contour.front().start_pt());
    dst_pt->start(src_pt);

    for (const auto &e : src_contour)
      {
        /* This is, admittedly, a little peculiar looking.
         * The motivation is to keep arc curve when they
         * are animated to/from a point; to do that we
         * will make the point curve a conic with a weight
         * of 0.
         */
        if (e.type() == astral::ContourCurve::conic_arc_curve)
          {
            dst_pt->arc_to(e.arc_angle(), src_pt, e.continuation());
          }
        else
          {
            dst_pt->line_to(src_pt, e.continuation());
          }
        dst_contour->curve_to(e);
      }

    if (contours_are_closed)
      {
        if (src_contour.back().end_pt() != src_contour.front().start_pt())
          {
            dst_pt->line_to(src_pt, src_contour.back().continuation());
          }
        dst_pt->close();
        dst_contour->close();
      }
  }
}

class astral::AnimatedContour::DataGenerator:
  public reference_counted<DataGenerator>::non_concurrent
{
public:
  DataGenerator(void):
    m_stroke_mode(CA::size_of_lines_and_curves_contributes_to_error)
  {
    m_fill_mode[contour_fill_approximation_allow_long_curves] = CA::ignore_size_of_curve;
    m_fill_mode[contour_fill_approximation_tessellate_long_curves] = CA::size_of_only_curves_contributes_to_error;
  }

  Approximation
  fill_approximated_geometry(const ContourData &start,
                             const ContourData &end,
                             float tol,
                             enum contour_fill_approximation_t tp,
                             float *out_tol)
  {
    const auto &e(m_fill[tp].fetch(tol, m_fill_mode[tp], start, end));
    if (out_tol)
      {
        *out_tol = e.error();
      }

    return e.value();
  }

  const FillSTCShader::CookedData&
  fill_render_data(const ContourData &start, const ContourData &end,
                   float tol, RenderEngine &engine,
                   enum contour_fill_approximation_t tp, float *out_tol)
  {
    const auto &e(m_fill[tp].fetch(tol, m_fill_mode[tp], start, end));
    if (out_tol)
      {
        *out_tol = e.error();
      }
    return e.render_data<0>(start, end, engine);
  }

  Approximation
  stroke_approximated_geometry(const ContourData &start,
                               const ContourData &end,
                               float tol, float *out_tol)
  {
    const auto &e(m_stroke.fetch(tol, m_stroke_mode, start, end));
    if (out_tol)
      {
        *out_tol = e.error();
      }

    return e.value();
  }

  const StrokeShader::CookedData&
  stroke_render_data(const ContourData &start,
                     const ContourData &end,
                     float tol, RenderEngine &engine,
                     float *out_tol)
  {
    const auto &e(m_stroke.fetch(tol, m_stroke_mode, start, end));
    if (out_tol)
      {
        *out_tol = e.error();
      }

    if (e.render_data_ready<1>())
      {
        e.reset_render_data<1>();
      }

    return e.render_data<0>(start, end, engine);
  }

  const StrokeShader::SimpleCookedData&
  simple_stroke_render_data(const ContourData &start,
                     const ContourData &end,
                     float tol, RenderEngine &engine,
                     float *out_tol)
  {
    const auto &e(m_stroke.fetch(tol, m_stroke_mode, start, end));
    if (out_tol)
      {
        *out_tol = e.error();
      }

    if (e.render_data_ready<0>())
      {
        return e.render_data<0>(start, end, engine).simple_data();
      }
    else
      {
        return e.render_data<1>(start, end, engine);
      }
  }

private:
  typedef std::tuple<FillSTCShader::CookedData> FillRenderData;
  typedef std::tuple<StrokeShader::CookedData, StrokeShader::SimpleCookedData> StrokeRenderData;

  static
  void
  generate_render_data(c_array<const ContourCurve> approx_start,
                       c_array<const ContourCurve> approx_end,
                       const ContourData &raw_start,
                       const ContourData &raw_end,
                       RenderEngine &engine, FillSTCShader::CookedData *output)
  {
    FillSTCShader::AnimatedData B;

    ASTRALunused(raw_start);
    ASTRALunused(raw_end);
    B.add_contour(approx_start, approx_end);
    *output = FillSTCShader::create_cooked_data(engine, B);
  }

  static
  void
  generate_render_data(c_array<const ContourCurve> approx_start,
                       c_array<const ContourCurve> approx_end,
                       const ContourData &raw_start,
                       const ContourData &raw_end,
                       RenderEngine &engine, StrokeShader::CookedData *output)
  {
    StrokeShader::RawAnimatedData builder;

    ASTRALassert(raw_start.closed() == raw_end.closed());
    if (!approx_start.empty())
      {
        builder.add_contour(raw_start.closed(),
                            approx_start,
                            approx_end);
      }
    else
      {
        builder.add_point_cap(raw_start.start(),
                              raw_end.start());
      }

    StrokeShader::create_render_data(engine, builder, output);
  }

  static
  void
  generate_render_data(c_array<const ContourCurve> approx_start,
                       c_array<const ContourCurve> approx_end,
                       const ContourData &raw_start,
                       const ContourData &raw_end,
                       RenderEngine &engine, StrokeShader::SimpleCookedData *output)
  {
    StrokeShader::RawAnimatedData builder;

    ASTRALassert(raw_start.closed() == raw_end.closed());
    if (!approx_start.empty())
      {
        builder.add_contour(raw_start.closed(),
                            approx_start,
                            approx_end);
      }
    else
      {
        builder.add_point_cap(raw_start.start(),
                              raw_end.start());
      }

    StrokeShader::create_render_data(engine, builder, output);
  }

  template<enum detail::ContourApproximator::mode_t mode,
           bool split_singleton_contours,
           enum detail::ContourApproximator::split_cubics_at_cusp_mode_t split_cubic_cusps_mode,
           typename T>
  class Entry
  {
  public:
    explicit
    Entry(enum detail::ContourApproximator::max_size_mode_t sz_mode,
          const ContourData &start, const ContourData &end):
      m_render_data_ready(false)
    {
      vecN<c_array<const ContourCurve>, 2> contour_pair;

      contour_pair[0] = start.curves();
      contour_pair[1] = end.curves();

      m_approximation = ASTRALnew detail::ContourApproximator(contour_pair,
                                                              m_curves_backing,
                                                              mode, sz_mode,
                                                              split_singleton_contours,
                                                              split_cubic_cusps_mode);
      m_error = m_approximation->error();
    }

    Approximation
    value(void) const
    {
      Approximation return_value;

      return_value.m_start = make_c_array(m_curves_backing[0]);
      return_value.m_end = make_c_array(m_curves_backing[1]);
      return return_value;
    }

    unsigned int
    size(void) const
    {
      ASTRALassert(m_curves_backing[0].size() == m_curves_backing[1].size());
      return m_curves_backing[0].size();
    }

    float
    error(void) const { return m_error; }

    Entry
    create_refinement(enum detail::ContourApproximator::max_size_mode_t sz_mode,
                      const ContourData &raw_start,
                      const ContourData &raw_end)
    {
      ASTRALunused(sz_mode);
      ASTRALunused(raw_start);
      ASTRALunused(raw_end);

      Entry return_value(*m_approximation);
      finalize();
      return return_value;
    }

    void
    finalize(void) { m_approximation = nullptr; }

    bool
    finalized(void) const { return m_approximation.get() == nullptr; }

    template<unsigned int N>
    const typename std::tuple_element<N, T>::type&
    render_data(const ContourData &raw_start,
                const ContourData &raw_end,
                RenderEngine &engine) const
    {
      if (!m_render_data_ready[N])
        {
          m_render_data_ready[N] = true;
          generate_render_data(make_c_array(m_curves_backing[0]),
                               make_c_array(m_curves_backing[1]),
                               raw_start, raw_end,
                               engine, &std::get<N>(m_render_data));
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
    Entry(const detail::ContourApproximator &A):
      m_render_data_ready(false)
    {
      m_approximation = A.create_refinement(m_curves_backing);
      m_error = m_approximation->error();
    }

    float m_error;
    vecN<detail::ContourApproximator::ApproximatedContour, 2> m_curves_backing;
    reference_counted_ptr<const detail::ContourApproximator> m_approximation;
    mutable T m_render_data;
    mutable vecN<bool, std::tuple_size<T>::value> m_render_data_ready;
  };

  typedef detail::ContourApproximator CA;

  /* NOTE: Stroking requires that no contour is a single curve; this requirement
   *       comes from the implementation of the shaders in the GL3 backend.
   */
  typedef Entry<CA::approximate_to_quadratic, false, CA::dont_split_cubics_at_cusps, FillRenderData> FillEntry;
  typedef Entry<CA::approximate_to_quadratic_error_to_biarc, true, CA::split_cubics_at_cusps, StrokeRenderData> StrokeEntry;

  enum CA::max_size_mode_t m_stroke_mode;
  vecN<enum CA::max_size_mode_t, number_contour_fill_approximation> m_fill_mode;

  vecN<detail::GenericLOD<FillEntry>, number_contour_fill_approximation> m_fill;
  detail::GenericLOD<StrokeEntry> m_stroke;
};

////////////////////////////////////
// astral::AnimatedContour methods
astral::AnimatedContour::
AnimatedContour(void)
{
  /* To make sure that the number of curves is the same
   * between the two, we cannot have Contour removing
   * curves, so Contour will not sanitize for us.
   */
  m_start.santize_curves_on_adding(false);
  m_end.santize_curves_on_adding(false);
}

astral::AnimatedContour::
~AnimatedContour(void)
{}

astral::reference_counted_ptr<astral::AnimatedContour>
astral::AnimatedContour::
create(const ContourData *pst_contour, vec2 st_center, c_array<const float> st_lengths,
       const ContourData *ped_contour, vec2 ed_center, c_array<const float> ed_lengths)
{
  if (!pst_contour && !ped_contour)
    {
      return nullptr;
    }

  c_array<const ContourCurve> st_contour, ed_contour;
  bool contours_are_closed;

  ASTRALassert(!pst_contour || !ped_contour || pst_contour->closed() == ped_contour->closed());
  if (pst_contour)
    {
      st_contour = pst_contour->curves();
      contours_are_closed = pst_contour->closed();
      st_center = pst_contour->start();
    }

  if (ped_contour)
    {
      ed_contour = ped_contour->curves();
      contours_are_closed = ped_contour->closed();
      ed_center = ped_contour->start();
    }

  if (st_contour.empty() && ed_contour.empty())
    {
      return create_raw(st_center, ed_center);
    }
  else if (st_contour.empty())
    {
      ASTRALassert(ped_contour);
      return create_raw(ped_contour->closed(),
                        ed_center, ed_contour);
    }
  else if (ed_contour.empty())
    {
      ASTRALassert(pst_contour);
      return create_raw(pst_contour->closed(),
                        st_contour, st_center);
    }
  else
    {
      return create(contours_are_closed,
                    st_contour, st_center, st_lengths,
                    ed_contour, ed_center, ed_lengths);
    }
}

astral::reference_counted_ptr<astral::AnimatedContour>
astral::AnimatedContour::
create(bool contours_are_closed,
       c_array<const ContourCurve> st_contour, vec2 st_center,
       c_array<const ContourCurve> ed_contour, vec2 ed_center)
{
  std::vector<float> st_lengths(st_contour.size());
  std::vector<float> ed_lengths(ed_contour.size());

  detail::approximate_lengths(st_contour, make_c_array(st_lengths));
  detail::approximate_lengths(ed_contour, make_c_array(ed_lengths));
  return create(contours_are_closed,
                st_contour, st_center, make_c_array(st_lengths),
                ed_contour, ed_center, make_c_array(ed_lengths));
}

astral::reference_counted_ptr<astral::AnimatedContour>
astral::AnimatedContour::
create(bool contours_are_closed,
       c_array<const ContourCurve> st_contour, vec2 st_center, c_array<const float> st_lengths,
       c_array<const ContourCurve> ed_contour, vec2 ed_center, c_array<const float> ed_lengths)
{
  if (st_contour.empty() && ed_contour.empty())
    {
      /* or should we return a moving point from st_center to ed_center ? */
      return nullptr;
    }

  /* Step 1: simplify the contours to remove zero-length edges */
  detail::SimplifiedContour st_simplified(st_contour, st_lengths);
  detail::SimplifiedContour ed_simplified(ed_contour, ed_lengths);

  if (st_simplified.empty() && ed_simplified.empty())
    {
      /* or should we return a moving point from st_center to ed_center ? */
      return nullptr;
    }

  if (st_simplified.empty() || ed_simplified.empty())
    {
      reference_counted_ptr<AnimatedContour> return_value;

      return_value = ASTRALnew AnimatedContour();
      if (ed_simplified.empty())
        {
          ASTRALassert(!st_simplified.empty());
          set_as_collapse(contours_are_closed,
                          &return_value->m_start, st_simplified.edges(),
                          &return_value->m_end, st_center);
          return return_value;
        }
      else
        {
          ASTRALassert(st_simplified.empty());
          set_as_collapse(contours_are_closed,
                          &return_value->m_end, ed_simplified.edges(),
                          &return_value->m_start, ed_center);
          return return_value;
        }
    }

  /* Step 2: if the contours are closed, change the starting point of
   * st_simplified so that unit vector from its center to the first
   * point is as close as possible to the unit vector from the center
   * of ed to its first point.
   *
   * TODO: making the first points match in direction is not as good
   * as making more than that match; for example given a contour point
   * in the start, find the edge in time (and where) it is and add the
   * sum across all these differences. However, that make the search
   * acts as O(n * n * log(m)) where m is the number of edges in
   * ed_simplified and n is the number of edges in st_simplified.
   *
   * ISSUE: would it be better to give the caller an option to
   *        contol the start point matching on the contours?
   */
  if (contours_are_closed)
    {
      float min_distance;
      vec2 ed_n;
      unsigned int best_idx(0);

      ed_n = (ed_simplified.start_pt() - ed_center).unit_vector();
      for (unsigned int E = 0, endE = st_simplified.size(); E < endE; ++E)
        {
          float dist;
          vec2 st_n;

          st_n = (st_simplified.edge_start_pt(E) - st_center).unit_vector();
          dist = (st_n - ed_n).magnitude();
          if (E == 0 || dist < min_distance)
            {
              min_distance = dist;
              best_idx = E;
            }
        }

      if (best_idx != 0u)
        {
          /* Shift the contents of st_simplified so that the edge named
           * by st_simplified[best_idx] becomes st_simplified[0].
           */
          std::vector<detail::SimplifiedContour::Edge> new_edges;
          float total(0.0f);

          new_edges.reserve(st_simplified.size());
          for (unsigned int E = 0, endE = st_simplified.size(); E < endE; ++E)
            {
              unsigned int F(E + best_idx);

              F = (F >= endE) ? (F - endE) : F;
              ASTRALassert(new_edges.size() == E);

              new_edges.push_back(st_simplified[F]);
              new_edges.back().length_from_contour_start_to_edge_start(total);
              total += st_simplified[F].length();
            }
          st_simplified.set(new_edges, new_edges.back().end_pt());
        }
    }

  /* Step 3: Compute a common paritioning in time [0, 1]
   * for the simplified contours.
   */
  detail::ContourCommonPartitioner contour_partitioner(st_simplified, ed_simplified);

  /* Step 4: create edges from the partitioning; the two simplified
   * contours, st_partitioned and ed_partitioned will have the same
   * number of edges and the I'th edge of st_partitioned is to
   * animated against the I'th edge of ed_partitioned.
   */
  detail::ContourBuilder st_partitioned(contour_partitioner.parition_points(), st_simplified, detail::ContourCommonPartitioner::from_st);
  detail::ContourBuilder ed_partitioned(contour_partitioner.parition_points(), ed_simplified, detail::ContourCommonPartitioner::from_ed);

  return create_raw(contours_are_closed,
                    st_partitioned.contour().curves(),
                    ed_partitioned.contour().curves());
}

astral::reference_counted_ptr<astral::AnimatedContour>
astral::AnimatedContour::
create(bool contours_are_closed,
       c_array<const CompoundCurve> st_contour,
       c_array<const CompoundCurve> ed_contour)
{
  ASTRALassert(st_contour.size() == ed_contour.size());
  if (st_contour.empty() || ed_contour.empty())
    {
      return nullptr;
    }

  unsigned int sz(t_min(st_contour.size(), ed_contour.size()));
  reference_counted_ptr<AnimatedContour> return_value;

  return_value = ASTRALnew AnimatedContour();

  ContourData &st_dst(return_value->m_start);
  ContourData &ed_dst(return_value->m_end);

  for (unsigned int i = 0; i < sz; ++i)
    {
      detail::SimplifiedContour st_simplified(st_contour[i].m_curves, st_contour[i].m_parameter_space_lengths);
      detail::SimplifiedContour ed_simplified(ed_contour[i].m_curves, ed_contour[i].m_parameter_space_lengths);
      detail::ContourCommonPartitioner partitioner(st_simplified, ed_simplified);
      detail::ContourBuilder st_partitioned(partitioner.parition_points(), st_simplified, detail::ContourCommonPartitioner::from_st);
      detail::ContourBuilder ed_partitioned(partitioner.parition_points(), ed_simplified, detail::ContourCommonPartitioner::from_ed);
      c_array<const ContourCurve> st(st_partitioned.contour().curves());
      c_array<const ContourCurve> ed(ed_partitioned.contour().curves());

      ASTRALassert(st.size() == ed.size());
      for (unsigned int k = 0; k < st.size(); ++k)
        {
          st_dst.curve_to(st[k]);
          ed_dst.curve_to(ed[k]);
        }
    }

  if (contours_are_closed && !st_dst.curves().empty() && !ed_dst.curves().empty())
    {
      bool force_add;

      /* At entry, we have that st_dst and ed_dst have the exact same number
       * of curves; we need to make sure that continues to be true. The funciton
       * ContourData::close() only adds a line segment if either force_add is
       * true or if the start and end points differ. We need to make sure that
       * either both add a line segment or both don't to keep the curve counts
       * the same.
       */
      force_add = st_dst.curves().front().start_pt() != st_dst.curves().back().end_pt()
        || ed_dst.curves().front().start_pt() != ed_dst.curves().back().end_pt();

      st_dst.close(force_add);
      ed_dst.close(force_add);
    }

  ASTRALassert(return_value->m_start.closed() == return_value->m_end.closed());
  ASTRALassert(return_value->m_start.curves().size() == return_value->m_end.curves().size());

  return return_value;
}

astral::reference_counted_ptr<astral::AnimatedContour>
astral::AnimatedContour::
create_raw(bool contours_are_closed,
           c_array<const ContourCurve> st_contour,
           c_array<const ContourCurve> ed_contour)
{
  ASTRALassert(st_contour.size() == ed_contour.size());
  if (st_contour.empty() || ed_contour.empty())
    {
      return nullptr;
    }

  unsigned int sz(t_min(st_contour.size(), ed_contour.size()));
  reference_counted_ptr<AnimatedContour> return_value;

  return_value = ASTRALnew AnimatedContour();

  ContourData &st_dst(return_value->m_start);
  ContourData &ed_dst(return_value->m_end);

  for (unsigned int i = 0; i + 1u < sz; ++i)
    {
      st_dst.curve_to(st_contour[i]);
      ed_dst.curve_to(ed_contour[i]);
    }

  if (contours_are_closed)
    {
      st_dst.curve_close(st_contour.back());
      ed_dst.curve_close(ed_contour.back());
    }
  else
    {
      st_dst.curve_to(st_contour.back());
      ed_dst.curve_to(ed_contour.back());
    }

  ASTRALassert(st_dst.curves().size() == ed_dst.curves().size());

  return return_value;
}

astral::reference_counted_ptr<astral::AnimatedContour>
astral::AnimatedContour::
create_raw(bool contours_are_closed,
           vec2 st_contour,
           c_array<const ContourCurve> ed_contour)
{
  if (ed_contour.empty())
    {
      /* or should we return a contour that just sits at st_contour? */
      return nullptr;
    }

  reference_counted_ptr<AnimatedContour> return_value;

  return_value = ASTRALnew AnimatedContour();
  set_as_collapse(contours_are_closed,
                  &return_value->m_end, ed_contour,
                  &return_value->m_start, st_contour);
  return return_value;
}

astral::reference_counted_ptr<astral::AnimatedContour>
astral::AnimatedContour::
create_raw(bool contours_are_closed,
           c_array<const ContourCurve> st_contour,
           vec2 ed_contour)
{
  if (st_contour.empty())
    {
      /* or should we return a contour that just sits at st_contour? */
      return nullptr;
    }

  reference_counted_ptr<AnimatedContour> return_value;

  return_value = ASTRALnew AnimatedContour();
  set_as_collapse(contours_are_closed,
                  &return_value->m_start, st_contour,
                  &return_value->m_end, ed_contour);
  return return_value;
}

astral::reference_counted_ptr<astral::AnimatedContour>
astral::AnimatedContour::
create_raw(vec2 st_contour, vec2 ed_contour)
{
  reference_counted_ptr<AnimatedContour> return_value;

  return_value = ASTRALnew AnimatedContour();
  return_value->m_start.start(st_contour);
  return_value->m_end.start(ed_contour);
  return return_value;
}

astral::AnimatedContour::DataGenerator&
astral::AnimatedContour::
data_generator(void) const
{
  if (!m_data_generator)
    {
      m_data_generator = ASTRALnew DataGenerator();
    }

  return *m_data_generator;
}

astral::AnimatedContour::Approximation
astral::AnimatedContour::
fill_approximated_geometry(float tol, enum contour_fill_approximation_t ct, float *out_tol) const
{
  return data_generator().fill_approximated_geometry(m_start, m_end, tol, ct, out_tol);
}

const astral::FillSTCShader::CookedData&
astral::AnimatedContour::
fill_render_data(float tol, RenderEngine &engine,
                 enum contour_fill_approximation_t ct, float *out_tol) const
{
  return data_generator().fill_render_data(m_start, m_end, tol, engine, ct, out_tol);
}

astral::AnimatedContour::Approximation
astral::AnimatedContour::
stroke_approximated_geometry(float tol, float *out_tol) const
{
  return data_generator().stroke_approximated_geometry(m_start, m_end, tol, out_tol);
}

const astral::StrokeShader::CookedData&
astral::AnimatedContour::
stroke_render_data(float tol, RenderEngine &engine, float *out_tol) const
{
  return data_generator().stroke_render_data(m_start, m_end, tol, engine, out_tol);
}

const astral::StrokeShader::SimpleCookedData&
astral::AnimatedContour::
simple_stroke_render_data(float tol, RenderEngine &engine, float *out_tol) const
{
  return data_generator().simple_stroke_render_data(m_start, m_end, tol, engine, out_tol);
}

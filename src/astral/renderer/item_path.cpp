/*!
 * \file item_path.cpp
 * \brief file item_path.cpp
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
#include <sstream>

#include <astral/path.hpp>
#include <astral/util/ostream_utility.hpp>
#include <astral/util/polynomial.hpp>
#include <astral/util/limited_vector.hpp>
#include <astral/renderer/combined_path.hpp>
#include <astral/renderer/static_data.hpp>
#include <astral/renderer/render_engine.hpp>
#include <astral/renderer/item_path.hpp>

/* Overview
 *
 *  1. ItemPath normalizes all input geometry to [-1, 1]x[-1, 1]
 *  2. ItemPath is to have a sequence of verical bands that are the same width
 *     that partitions [-1, 1]x[-1, 1]. The number of vertical bands is V.
 *  3. ItemPath is to have a sequence of horizonal bands that are the same height
 *     that partitions [-1, 1]x[-1, 1]. The number of horizontal bands is H.
 *  4. The winding number at p = (x, y) is computed as follows.
 *     a. let h be the horizontal band the p lies in, i.e. h = (0.5 * y + 1) * H
 *     b. If x <= 0, compute the winding number by examining the ray from (0, y)
 *        to (-infinity, y) computing the intersection of the ray against all curves
 *        in the band. If x > 0, then use the ray from (0, y) to (infinity, y).
 *  5. The winding number can also be computed by using vertical band v where
 *     v = (0.5 * x + 1) * V.
 *  6. To optimize, each horizontal band is partioned at x = 0, curves that are
 *     entirely on one side of the partition are added to only that side. In addition
 *     curves are sorted as follows. Let I(C) = [m, M] where m is the x-minimum of the
 *     the curve C and M is the x-maximum of the curve C. For the partition of the
 *     horizontal band x > 0, sort the curves with the those with largest M coming
 *     first; For x < 0, sort the curves with those with most negative m coming
 *     first. The ordering allows one to stop the loop over curves as soon as one
 *     encounters a curve when the point p is between the ending of the interval
 *     and 0.
 *  7. Do the same for vertical.
 *  8. Anti-aliasing is computed by tracking the nearest curve found in the
 *     horizontal or verical band.
 *
 * Items 1-7, give the requirements what is needed to be done. The code below
 * performs it as follows:
 *
 *  1. The class Band is a list of curve values. We start with the two horizontal
 *     bands H- = [-1, 0]x[-1, 1] H+ = [0, 1]x[-1, 1]. A band can be split parallel
 *     to its direction to reduce the number of curves per band.
 *  2. The deepest number of splittings gives log2(H)
 *  3. When a horizontal band is split, the curve is split as well at the point
 *     splitting to reflect that what it hits in the band is a smaller region.
 *  4. Curves in a band are represented by BandedCurve. That class stores the
 *     original source curve (before any splittings) along with a bounding box;
 *     the bounding box is inflated to account for numerical round-off error.
 *  5. We apply 1-4 to vertical as well.
 */

/* this value should be big enough that fp16
 * easily picks up on it on the range [-1, 1].
 * Fp16 has accuracy of one part in 2048 at
 * 1.0, so we use 0.0005 which is 1 in 2000.
 */
const float BoundingBoxFuzz = 0.0005f;

class astral::ItemPath::Geometry::InputCurve
{
public:
  InputCurve(const Transformation &tr, const ContourCurve &v):
    m_start_pt(tr.apply_to_point(v.start_pt())),
    m_end_pt(tr.apply_to_point(v.end_pt())),
    m_type(v.is_conic() ? ContourCurve::conic_curve : v.type()),
    m_bb(tr.apply_to_bb(v.tight_bounding_box()))
  {
    switch (m_type)
      {
      case ContourCurve::line_segment:
        m_control_pt = 0.5f * (m_start_pt + m_end_pt);
        m_conic_weight = 1.0f;
        break;

      case ContourCurve::conic_curve:
      case ContourCurve::quadratic_bezier:
        m_control_pt = tr.apply_to_point(v.control_pt(0));
        m_conic_weight = v.conic_weight();
        break;

      default:
        ASTRALassert(!"Bad curve type fed to ItemPath::Geometry::Curve ctor");
      }
  }

  InputCurve(const ScaleTranslate &tr, const InputCurve &v):
    m_start_pt(tr.apply_to_point(v.m_start_pt)),
    m_control_pt(tr.apply_to_point(v.m_control_pt)),
    m_end_pt(tr.apply_to_point(v.m_end_pt)),
    m_conic_weight(v.m_conic_weight),
    m_type(v.m_type),
    m_bb(tr.apply_to_bb(v.m_bb))
  {
  }

  vec2 m_start_pt, m_control_pt, m_end_pt;
  float m_conic_weight;
  enum ContourCurve::type_t m_type;
  BoundingBox<float> m_bb;
};

/* A BandRegion represents an interval [i / 2^n, (i + 1) / 2^n]
 * where i is given by raw_start(), the value of (i + 1) is given
 * by raw_end() and the value of n is given by generation().
 * The default ctor creates the band region [0, 1]. Spitting
 * a band region [i / 2^n, (i + 1) / 2^n] creates the two region
 * [(2 * i) / 2^(n + 1), (2 * i + 1) / 2^(n + 1)],
 * [(2 * i + 1) / 2^(n + 1), (2 * i + 2) / 2^(n + 1)].
 */
class astral::ItemPath::BandRegion
{
public:
  BandRegion(void):
    m_start(0u),
    m_log2_denominator(0u)
  {
  }

  vecN<BandRegion, 2>
  split(void) const
  {
    vecN<BandRegion, 2> children;

    children[0].m_log2_denominator = m_log2_denominator + 1u;
    children[1].m_log2_denominator = m_log2_denominator + 1u;
    children[0].m_start = 2u * m_start;
    children[1].m_start = 2u * m_start + 1u;

    return children;
  }

  /* Gives the value for raw_start() for a BandRegion
   * covering the same interval whose generation() is G.
   */
  unsigned int
  start(unsigned int G) const
  {
    ASTRALassert(G >= m_log2_denominator);
    return m_start << (G - m_log2_denominator);
  }

  /* Gives the value for raw_end() for a BandRegion
   * covering the same interval whose generation() is G.
   */
  unsigned int
  end(unsigned int G) const
  {
    ASTRALassert(G >= m_log2_denominator);
    return (m_start + 1u) << (G - m_log2_denominator);
  }

  unsigned int
  raw_start(void) const
  {
    return m_start;
  }

  unsigned int
  raw_end(void) const
  {
    return m_start + 1u;
  }

  /* Returns the start of the interval represented
   * by the BandRegion under the mapping that takes
   * [0, 1] to [-1 , 1].
   */
  float
  boundary_start(void) const
  {
    float den;

    den = static_cast<float>(1u << m_log2_denominator);
    return -1.0f + 2.0f * static_cast<float>(raw_start()) / den ;
  }

  /* Returns the end of the interval represented
   * by the BandRegion under the mapping that takes
   * [0, 1] to [-1 , 1].
   */
  float
  boundary_end(void) const
  {
    float den;

    den = static_cast<float>(1u << m_log2_denominator);
    return -1.0f + 2.0f * static_cast<float>(raw_end()) / den;
  }

  /* Returns the middle of the interval represented
   * by the BandRegion under the mapping that takes
   * [0, 1] to [-1 , 1].
   */
  float
  boundary_middle(void) const
  {
    float den;

    den = static_cast<float>(1u << (1u + m_log2_denominator));
    return -1.0f + 2.0f * static_cast<float>(raw_start() + raw_end()) / den;
  }

  unsigned int
  generation(void) const
  {
    return m_log2_denominator;
  }

private:
  unsigned int m_start;
  unsigned int m_log2_denominator;
};

class astral::ItemPath::BandedCurve
{
public:
  /* When rendering, each band is split in the middle to
   * for two smaller bands; fragments before the split
   * compute the winding number by computing the intersections
   * of curves against the ray that originates at the fragment
   * and goes parrallel to the band to negative infinity.
   * Fragments after us a ray going to positive infinity.
   */
  enum band_side_t:uint32_t
    {
      /* Uses a ray to negative infinity */
      min_band_side = 0,

      /* Uses a ray to positive infinity */
      max_band_side
    };

  /* Enumeration to decribe band direction */
  enum band_t:uint32_t
    {
      /* band is horizontal, so band region is y-values */
      horizontal_band,

      /* band is vertical, so band region is x-values */
      vertical_band,
    };

  /*
   * Gives the coordinate along the band, i.e.
   * horizontal bands give 0 and vertical give 1
   */
  static
  unsigned int
  coordinate_of_band_direction(enum band_t b)
  {
    return (b == horizontal_band) ? 0 : 1;
  }

  /* Gives the coordinate that specifies the
   * boundary of the band, i.e. horizontal
   * bands give 1 and vertical give 0
   */
  static
  unsigned int
  coordinate_of_band_boundary(enum band_t b)
  {
    return (b == horizontal_band) ? 1 : 0;
  }

  static
  void
  init(enum band_t band,
       const ScaleTranslate &tr,
       const std::vector<Geometry::InputCurve> &curves,
       std::vector<BandedCurve> *min_side,
       std::vector<BandedCurve> *max_side);

  static
  void
  band_split(enum band_t band, BandRegion rgn,
             const std::vector<BandedCurve> &curves,
             std::vector<BandedCurve> *child0,
             std::vector<BandedCurve> *child1);

  static
  void
  sort_and_filter_curves(enum band_side_t band_side, enum band_t band,
                         std::vector<BandedCurve> &curves);

  float
  cost(enum band_side_t side, enum band_t band) const
  {
    vec2 v;

    /* The cost of a curve is the length within the band
     * that it is active. For min_band_side, that means the
     * distance from 0 to the min-side of the bounding box;
     * for max_band_side, that means the distance from 0
     * to the max-side of the bounding box.
     */
    v = (side == min_band_side) ? -m_bb.min_point() : m_bb.max_point();
    return t_max(v[coordinate_of_band_direction(band)], 0.0f);
  }

  bool
  is_flat(enum band_t band) const
  {
    unsigned int c(coordinate_of_band_boundary(band));
    return (m_start_pt[c] == m_end_pt[c]
            && (m_type == ContourCurve::line_segment
                || m_control_pt[c] == m_start_pt[c]));
  }

  void
  pack_data(std::vector<vec4> *dst, enum band_t band, enum band_side_t side) const;

private:
  class Sorter;

  explicit
  BandedCurve(const Geometry::InputCurve &trC):
    m_start_pt(trC.m_start_pt),
    m_control_pt(trC.m_control_pt),
    m_end_pt(trC.m_end_pt),
    m_conic_weight(trC.m_conic_weight),
    m_type(trC.m_type),
    m_bb(trC.m_bb),
    m_src_curve(trC)
  {
    /* Enlarge from the right bouning box from InputCurve
     * by a value that corresponds to the lowest accuracy
     * of fp16 on [-1, 1].
     */
    m_bb.enlarge(vec2(BoundingBoxFuzz));
  }

  /* split the curve at 0 in the band direction */
  void
  split_curve_at0(enum band_t band,
                  std::vector<BandedCurve> *before0,
                  std::vector<BandedCurve> *after0) const;

  /* split the curve for splitting a band region */
  void
  split_curve_on_band_split(enum band_t band, float split_value,
                            std::vector<BandedCurve> *child0,
                            std::vector<BandedCurve> *child1) const;

  void
  split_curve_implement(unsigned int coordinate,
                        float coordinate_value,
                        std::vector<BandedCurve> *child0,
                        std::vector<BandedCurve> *child1) const;

  void
  split_curve_parametrically(float t, BandedCurve *pre, BandedCurve *post);

  ContourCurve
  contour_curve(void) const;

  vec2 m_start_pt, m_control_pt, m_end_pt;
  float m_conic_weight;
  enum ContourCurve::type_t m_type;
  BoundingBox<float> m_bb;
  Geometry::InputCurve m_src_curve;
};

class astral::ItemPath::BandedCurve::Sorter
{
public:
  Sorter(enum band_side_t band_side, enum band_t band):
    m_band_side(band_side),
    m_coordinate(coordinate_of_band_direction(band))
  {}

  bool
  operator()(const BandedCurve &lhs, const BandedCurve &rhs) const
  {
    if (m_band_side == min_band_side)
      {
        /* we want the smallest mins to come first */
        return lhs.m_bb.min_point()[m_coordinate] < rhs.m_bb.min_point()[m_coordinate];
      }
    else
      {
        /* we want the largest maxes to come first */
        return lhs.m_bb.max_point()[m_coordinate] > rhs.m_bb.max_point()[m_coordinate];
      }
  }

  /* Assumes the input curves are sorted by operator() const.
   * Drops those curves from the array that are completely outside
   * of the half-band.
   */
  void
  filter(std::vector<BandedCurve> &curves)
  {
    /* In order to support glyph thickening, we cannot filter out
     * curves on the horizontal band
     */
    if (m_coordinate == 0)
      {
        return;
      }

    while (!curves.empty())
      {
        if (m_band_side == min_band_side)
          {
            if (curves.back().m_bb.min_point()[m_coordinate] > 0.0f)
              {
                curves.pop_back();
              }
            else
              {
                return;
              }
          }
        else
          {
            if (curves.back().m_bb.max_point()[m_coordinate] < 0.0f)
              {
                curves.pop_back();
              }
            else
              {
                return;
              }
          }
      }
  }

private:
  enum band_side_t m_band_side;
  unsigned int m_coordinate;
};

class astral::ItemPath::Band
{
public:
  static
  void
  create_bands(enum BandedCurve::band_t band,
               const GenerationParams &params,
               const ScaleTranslate &tr,
               const std::vector<Geometry::InputCurve> &curves,
               std::vector<Band> *out_bands);

  unsigned int
  relative_data_offset(enum BandedCurve::band_side_t side) const
  {
    return m_relative_data_offset[side];
  }

  unsigned int
  number_curves(enum BandedCurve::band_side_t side) const
  {
    return m_curves[side].size();
  }

  BandRegion
  region(void) const
  {
    return m_region;
  }

  float
  weighted_cost(void) const
  {
    float v, s;

    v = m_costs[0] + m_costs[1];
    s = region().boundary_end() - region().boundary_start();

    /* 0.25 because the squre is [-1, 1]x[-1, 1]
     * which has area 4.
     */
    return 0.25f * v * s;
  }

  /* Will resize dst to pack the data, and
   * m_relative_data_offset is the size of
   * dst on entry
   */
  void
  pack_data(std::vector<vec4> *dst, enum BandedCurve::band_t band);

private:
  class CreateStatus
  {
  public:
    vec2 m_costs;
    bool m_has_children;
  };

  explicit
  Band(BandRegion rgn):
    m_region(rgn)
  {}

  static
  CreateStatus
  create_bands_implement(BandRegion rgn,
                         enum BandedCurve::band_t band,
                         const GenerationParams &params,
                         std::vector<BandedCurve> &min_side, //will be stolen
                         std::vector<BandedCurve> &max_side, //will be stolen
                         std::vector<Band> *out_bands);

  static
  vec2
  compute_cost(enum BandedCurve::band_t band,
               const std::vector<BandedCurve> &min_side,
               const std::vector<BandedCurve> &max_side);

  BandRegion m_region;
  vecN<std::vector<BandedCurve>, 2> m_curves;
  vecN<unsigned int, 2> m_relative_data_offset;
  vec2 m_costs;
};

////////////////////////////////////////////////////////
// astral::ItemPath::BandedCurve methods
void
astral::ItemPath::BandedCurve::
split_curve_at0(enum band_t band,
                std::vector<BandedCurve> *before0,
                std::vector<BandedCurve> *after0) const
{
  split_curve_implement(coordinate_of_band_direction(band),
                        0.0f, before0, after0);
}

void
astral::ItemPath::BandedCurve::
split_curve_on_band_split(enum band_t band, float split_value,
                          std::vector<BandedCurve> *child0,
                          std::vector<BandedCurve> *child1) const
{
  split_curve_implement(coordinate_of_band_boundary(band),
                        split_value, child0, child1);
}

void
astral::ItemPath::BandedCurve::
split_curve_parametrically(float t, BandedCurve *pre, BandedCurve *post)
{
  ContourCurveSplit C(false, contour_curve(), t);

  ASTRALassert(pre != this);
  ASTRALassert(post != this);

  /* The value of m_src_curve is what is packed to item data
   * which must be consistent same for the shader to work correctly.
   * Thus, we save that value from the parent, thus the value is
   * preserved on splitting. However, we use the bounding box of
   * split curve to pass on to the shader the early out value
   */
  *pre = BandedCurve(Geometry::InputCurve(Transformation(), C.before_t()));
  pre->m_src_curve = m_src_curve;

  *post = BandedCurve(Geometry::InputCurve(Transformation(), C.after_t()));
  post->m_src_curve = m_src_curve;
}

void
astral::ItemPath::BandedCurve::
split_curve_implement(unsigned int coordinate,
                      float coordinate_value,
                      std::vector<BandedCurve> *child0,
                      std::vector<BandedCurve> *child1) const
{
  unsigned int num_solutions, num_filtered_solutions;
  vecN<double, 2> ts, filtered_ts;
  double S, E, CV;

  S = static_cast<double>(m_start_pt[coordinate]);
  E = static_cast<double>(m_end_pt[coordinate]);
  CV = static_cast<double>(coordinate_value);

  if (m_type == ContourCurve::line_segment)
    {
      Polynomial<double, 1> poly;

      poly.coeff(0) = S - CV;
      poly.coeff(1) = E - S;
      num_solutions = solve_polynomial(poly, c_array<double>(ts));
    }
  else
    {
      double C;
      Polynomial<double, 2> poly;

      C = static_cast<double>(m_control_pt[coordinate]);
      poly.coeff(0) = S - CV;
      poly.coeff(1) = 2.0 * (C - S);
      poly.coeff(2) = S - 2.0 * C + E;
      num_solutions = solve_polynomial(poly, c_array<double>(ts));
    }

  num_filtered_solutions = 0;
  for (unsigned int i = 0; i < num_solutions; ++i)
    {
      if (ts[i] >= 0.0 && ts[i] <= 1.0)
        {
          filtered_ts[num_filtered_solutions] = ts[i];
          ++num_filtered_solutions;
        }
    }

  /* sort the solutions in increasing order */
  std::sort(filtered_ts.begin(), filtered_ts.begin() + num_filtered_solutions);

  BandedCurve curve(*this);
  double domain_start(0.0), domain_end(1.0);
  LimitedVector<BandedCurve, 3> b0, b1;

  for (unsigned int i = 0; i < num_filtered_solutions; ++i)
    {
      /* We want to split the original curve at filtered_ts[i],
       * where as the curve is the original curve on the
       * domain [domain_start, domain_end] remapped to the
       * domain [0, 1]. We need to split curve at rel_t where
       *
       *  filtered_ts[i] = domain_start + (domain_end - domain_start) * rel_t
       */
      double rel_t;
      rel_t = (filtered_ts[i] - domain_start) / (domain_end - domain_start);

      BandedCurve before_c(curve), after_c(curve);
      curve.split_curve_parametrically(rel_t, &before_c, &after_c);
      curve = after_c;
      domain_start = filtered_ts[i];

      /* It might be tempting to say that before_c goes in exactly
       * one of b0 or b1, but that is not true. Because of round-off
       * error, it is highly unlikely that the original curve
       * evaluated at the solution point is 0. Also, the ctor of
       * BandedCurve inflates m_bb by BoundingBoxFuzz to make
       * sure that fp16 arithematic is ok.
       */
      if (before_c.m_bb.min_point()[coordinate] <= coordinate_value)
        {
          b0.push_back(before_c);
        }

      if (before_c.m_bb.max_point()[coordinate] >= coordinate_value)
        {
          b1.push_back(before_c);
        }
    }

  if (curve.m_bb.min_point()[coordinate] <= coordinate_value)
    {
      b0.push_back(curve);
    }

  if (curve.m_bb.max_point()[coordinate] >= coordinate_value)
    {
      b1.push_back(curve);
    }

  /* We don't allow the curve to be added more than once
   * because there is nothing gained because any curve is
   * active from the far side of the glyph box to the center.
   * The cost of a single curve closer to the boundary
   * is the smaller than two curves where one of them ends
   * at the sample place as the original curve.
   */
  if (b0.size() >= 2)
    {
      child0->push_back(*this);
    }
  else if (!b0.empty())
    {
      child0->push_back(b0.front());
    }

  if (b1.size() >= 2)
    {
      child1->push_back(*this);
    }
  else if (!b1.empty())
    {
      child1->push_back(b1.front());
    }
}

astral::ContourCurve
astral::ItemPath::BandedCurve::
contour_curve(void) const
{
  switch (m_type)
    {
    case ContourCurve::line_segment:
      return ContourCurve(m_start_pt, m_end_pt,
                          ContourCurve::not_continuation_curve);

    case ContourCurve::quadratic_bezier:
      return ContourCurve(m_start_pt, m_control_pt, m_end_pt,
                          ContourCurve::not_continuation_curve);

    case ContourCurve::conic_curve:
      return ContourCurve(m_start_pt, m_conic_weight, m_control_pt, m_end_pt,
                          ContourCurve::not_continuation_curve);

    default:
      ASTRALassert(!"Bad BandedCurve::m_type value");
      return ContourCurve(m_start_pt, m_end_pt,
                          ContourCurve::not_continuation_curve);
    }
}

void
astral::ItemPath::BandedCurve::
init(enum band_t band,
     const ScaleTranslate &tr,
     const std::vector<Geometry::InputCurve> &curves,
     std::vector<BandedCurve> *min_side,
     std::vector<BandedCurve> *max_side)
{
  unsigned int coordinate(coordinate_of_band_direction(band));
  for (const Geometry::InputCurve &C : curves)
    {
      BandedCurve bC(Geometry::InputCurve(tr, C));

      if (!bC.is_flat(band))
        {
          if (band == horizontal_band)
            {
              /* to support bolding glyphs, we need to add the geometry to both sides always */
              min_side->push_back(bC);
              max_side->push_back(bC);
            }
          else if (bC.m_bb.min_point()[coordinate] < 0.0f && bC.m_bb.max_point()[coordinate] > 0.0f)
            {
              bC.split_curve_at0(band, min_side, max_side);

            }
          else if (bC.m_bb.min_point()[coordinate] < 0.0f)
            {
              ASTRALassert(bC.m_bb.max_point()[coordinate] <= 0.0f);
              min_side->push_back(bC);
            }
          else
            {
              ASTRALassert(bC.m_bb.max_point()[coordinate] >= 0.0f);
              max_side->push_back(bC);
            }
        }
    }
}

void
astral::ItemPath::BandedCurve::
band_split(enum band_t band, BandRegion rgn,
           const std::vector<BandedCurve> &curves,
           std::vector<BandedCurve> *child0,
           std::vector<BandedCurve> *child1)
{
  unsigned int coordinate(coordinate_of_band_boundary(band));
  float split_value(rgn.boundary_middle());

  for (const BandedCurve &bC : curves)
    {
      if (bC.m_bb.min_point()[coordinate] < split_value && bC.m_bb.max_point()[coordinate] > split_value)
        {
          bC.split_curve_on_band_split(band, split_value, child0, child1);
        }
      else if (bC.m_bb.min_point()[coordinate] < split_value)
        {
          ASTRALassert(bC.m_bb.max_point()[coordinate] <= split_value);
          child0->push_back(bC);
        }
      else
        {
          ASTRALassert(bC.m_bb.max_point()[coordinate] >= split_value);
          child1->push_back(bC);
        }
    }
}

void
astral::ItemPath::BandedCurve::
sort_and_filter_curves(enum band_side_t band_side, enum band_t band,
                       std::vector<BandedCurve> &curves)
{
  Sorter sorter(band_side, band);

  /* sort the curves with those at the edge of the [-1, 1] along
   * the direction of the band coming first.
   */
  std::sort(curves.begin(), curves.end(), sorter);

  /* now drop all those curves that start and end past the
   * boundary that the half bands share
   */
  sorter.filter(curves);
}

void
astral::ItemPath::BandedCurve::
pack_data(std::vector<vec4> *dst, enum band_t band, enum band_side_t side) const
{
  unsigned int offset(dst->size());
  c_array<vec4> dst_ptr;
  vec2 r;
  float f;

  dst->resize(dst->size() + 2);
  dst_ptr = make_c_array(*dst).sub_array(offset);

  f = (side == min_band_side) ? -1.0f : 1.0f;
  r = (side == min_band_side) ? m_bb.min_point() : m_bb.max_point();
  if (band == horizontal_band)
    {
      dst_ptr[0] = vec4(f * m_src_curve.m_start_pt.x(), m_src_curve.m_start_pt.y(),
                        f * m_src_curve.m_control_pt.x(), m_src_curve.m_control_pt.y());
      dst_ptr[1] = vec4(f * m_src_curve.m_end_pt.x(), m_src_curve.m_end_pt.y(),
                        m_src_curve.m_conic_weight, f * r.x());
    }
  else
    {
      dst_ptr[0] = vec4(f * m_src_curve.m_start_pt.y(), m_src_curve.m_start_pt.x(),
                        f * m_src_curve.m_control_pt.y(), m_src_curve.m_control_pt.x());
      dst_ptr[1] = vec4(f * m_src_curve.m_end_pt.y(), m_src_curve.m_end_pt.x(),
                        m_src_curve.m_conic_weight, f * r.y());
    }
}

//////////////////////////////////////////////////
// astral::ItemPath::Band methods
void
astral::ItemPath::Band::
create_bands(enum BandedCurve::band_t band,
             const GenerationParams &params,
             const ScaleTranslate &tr,
             const std::vector<Geometry::InputCurve> &curves,
             std::vector<Band> *out_bands)
{
  std::vector<BandedCurve> min_side, max_side;
  BandedCurve::init(band, tr, curves, &min_side, &max_side);
  create_bands_implement(BandRegion(), band, params,
                         min_side, max_side, out_bands);
}

astral::vec2
astral::ItemPath::Band::
compute_cost(enum BandedCurve::band_t band,
             const std::vector<BandedCurve> &min_side,
             const std::vector<BandedCurve> &max_side)
{
  vec2 return_value(0.0f, 0.0f);
  for (const BandedCurve &c : min_side)
    {
      return_value[BandedCurve::min_band_side] += c.cost(BandedCurve::min_band_side, band);
    }
  for (const BandedCurve &c : max_side)
    {
      return_value[BandedCurve::max_band_side] += c.cost(BandedCurve::max_band_side, band);
    }

  return return_value;
}

astral::ItemPath::Band::CreateStatus
astral::ItemPath::Band::
create_bands_implement(BandRegion rgn,
                       enum BandedCurve::band_t band,
                       const GenerationParams &params,
                       std::vector<BandedCurve> &min_side, //will be stolen
                       std::vector<BandedCurve> &max_side, //will be stolen
                       std::vector<Band> *out_bands)
{
  CreateStatus return_value;

  return_value.m_costs = compute_cost(band, min_side, max_side);
  if (rgn.generation() >= params.m_max_recursion
      || (return_value.m_costs[0] < params.m_cost && return_value.m_costs[1] < params.m_cost))
    {
      /* the curves need to be sorted and possibly filtered */
      BandedCurve::sort_and_filter_curves(BandedCurve::min_band_side, band, min_side);
      BandedCurve::sort_and_filter_curves(BandedCurve::max_band_side, band, max_side);

      out_bands->push_back(Band(rgn));
      out_bands->back().m_costs = return_value.m_costs;
      out_bands->back().m_curves[BandedCurve::min_band_side].swap(min_side);
      out_bands->back().m_curves[BandedCurve::max_band_side].swap(max_side);

      return_value.m_has_children = false;
    }
  else
    {
      std::vector<BandedCurve> min_side_child0, min_side_child1;
      std::vector<BandedCurve> max_side_child0, max_side_child1;
      vecN<BandRegion, 2> children(rgn.split());
      CreateStatus c0, c1;

      BandedCurve::band_split(band, rgn, min_side, &min_side_child0, &min_side_child1);
      BandedCurve::band_split(band, rgn, max_side, &max_side_child0, &max_side_child1);

      c0 = create_bands_implement(children[0], band, params,
                                  min_side_child0, max_side_child0,
                                  out_bands);

      c1 = create_bands_implement(children[1], band, params,
                                  min_side_child1, max_side_child1,
                                  out_bands);

      if (!c0.m_has_children
          && !c1.m_has_children
          && return_value.m_costs[0] <= t_min(c0.m_costs[0], c1.m_costs[0])
          && return_value.m_costs[1] <= t_min(c0.m_costs[1], c1.m_costs[1]))
        {
          out_bands->pop_back();
          out_bands->pop_back();

          BandedCurve::sort_and_filter_curves(BandedCurve::min_band_side, band, min_side);
          BandedCurve::sort_and_filter_curves(BandedCurve::max_band_side, band, max_side);

          out_bands->push_back(Band(rgn));
          out_bands->back().m_costs = return_value.m_costs;
          out_bands->back().m_curves[BandedCurve::min_band_side].swap(min_side);
          out_bands->back().m_curves[BandedCurve::max_band_side].swap(max_side);

          return_value.m_has_children = false;
        }
      else
        {
          return_value.m_has_children = true;
        }
    }

  return return_value;
}

void
astral::ItemPath::Band::
pack_data(std::vector<vec4> *dst,
          enum BandedCurve::band_t band)
{
  unsigned int cnt;

  cnt = m_curves[0].size() + m_curves[1].size();
  dst->reserve(dst->size() + 2u * cnt);

  for (unsigned int side = 0; side < 2; ++side)
    {
      m_relative_data_offset[side] = dst->size();
      for (const auto &C : m_curves[side])
        {
          C.pack_data(dst, band,
                      static_cast<enum BandedCurve::band_side_t>(side));
        }
    }
}

//////////////////////////////////////
// astral::ItemPath::Geometry methods
astral::ItemPath::Geometry::
Geometry(void):
  m_error(0.0f)
{
}

astral::ItemPath::Geometry::
~Geometry()
{
}

astral::ItemPath::Geometry&
astral::ItemPath::Geometry::
add(const Transformation &tr, const Contour &contour, RelativeThreshhold tol)
{
  return add(tr, contour, tol.absolute_threshhold(contour.bounding_box()));
}
astral::ItemPath::Geometry&
astral::ItemPath::Geometry::
add(const CombinedPath &path, RelativeThreshhold tol)
{
  return add(path, tol.absolute_threshhold(path.compute_bounding_box()));
}

astral::ItemPath::Geometry&
astral::ItemPath::Geometry::
add(const CombinedPath &paths, float tol)
{
  auto static_paths(paths.paths<Path>());
  for (unsigned int i = 0; i < static_paths.size(); ++i)
    {
      Transformation path_tr;
      const vec2 *translate(paths.get_translate<Path>(i));
      const float2x2 *matrix(paths.get_matrix<Path>(i));

      if (translate)
        {
          path_tr.m_translate = *translate;
        }

      if (matrix)
        {
          path_tr.m_matrix = *matrix;
        }

      for (unsigned int c = 0, endc = static_paths[i]->number_contours(); c < endc; ++c)
        {
          add(path_tr, static_paths[i]->contour(c), tol);
        }
    }

  return *this;
}

astral::ItemPath::Geometry&
astral::ItemPath::Geometry::
add(const Transformation &tr, const Contour &contour, float tol)
{
  const BoundingBox<float> &bb(contour.bounding_box());
  c_array<const ContourCurve> curves;
  float svd(compute_singular_values(tr.m_matrix).x());
  float out_error;

  tol /= svd;

  curves = contour.item_path_approximated_geometry(tol, &out_error);
  m_error = t_max(m_error, out_error);

  return add(tr, curves, contour.closed(), &bb);
}

astral::ItemPath::Geometry&
astral::ItemPath::Geometry::
add(const Transformation &tr, c_array<const ContourCurve> curves,
    bool is_closed, const BoundingBox<float> *bb)
{
  if (curves.empty())
    {
      return *this;
    }

  if (bb)
    {
      m_bb.union_box(tr.apply_to_bb(*bb));
    }

  m_curves.reserve(m_curves.size() + curves.size());
  for (const auto &C : curves)
    {
      ASTRALassert(C.type() != ContourCurve::cubic_bezier);
      m_curves.push_back(InputCurve(tr, C));

      if (!bb)
        {
          m_bb.union_box(tr.apply_to_bb(C.tight_bounding_box()));
        }
    }

  /* close the contour */
  if (!is_closed)
    {
      ContourCurve C(curves.back().end_pt(), curves.front().start_pt(),
                     ContourCurve::not_continuation_curve);
      m_curves.push_back(InputCurve(tr, C));
    }

  return *this;
}

//////////////////////////////////////
// astral::ItemPath methods
astral::ItemPath::
ItemPath(const Geometry &geometry, GenerationParams params):
  m_params(params)
{
  /* prevent degenerate paths */
  if (geometry.m_bb.size() == vec2(0.0f, 0.0f) || geometry.m_curves.empty())
    {
      m_properties.m_error = 0.0f;
      m_properties.m_number_bands = uvec2(0u, 0u);

      m_header_data.resize(1);
      m_header_data[0].x().u = m_properties.m_number_bands.x();
      m_header_data[0].y().u = m_properties.m_number_bands.y();

      m_properties.m_fp16_data_size = m_band_data.size();
      m_properties.m_generic_data_size = m_header_data.size();

      return;
    }

  /* Step 0: compute the transformation from the bounding
   *         box of layer to [-1, 1]x[-1, 1]
   */
  m_tr.m_scale = vec2(2.0f, 2.0f) / geometry.m_bb.size();
  m_tr.m_translate = -m_tr.m_scale * geometry.m_bb.min_point() - vec2(1.0f, 1.0f);

  /* Step 1: break into vertical and horizontal bands */
  std::vector<Band> horizontal_bands, vertical_bands;

  Band::create_bands(BandedCurve::horizontal_band, params, m_tr, geometry.m_curves, &horizontal_bands);
  Band::create_bands(BandedCurve::vertical_band, params, m_tr, geometry.m_curves, &vertical_bands);

  /* Step 2: Store the horizontal and vertical bands into fp16 shared data;
   *         the bands are already sorted so that the shader can do an early
   *         out
   */
  unsigned int max_horiz_generation(0), max_vert_generation(0);
  for (Band &b : horizontal_bands)
    {
      b.pack_data(&m_band_data, BandedCurve::horizontal_band);
      max_horiz_generation = t_max(max_horiz_generation, b.region().generation());
    }
  for (Band &b : vertical_bands)
    {
      b.pack_data(&m_band_data, BandedCurve::vertical_band);
      max_vert_generation = t_max(max_vert_generation, b.region().generation());
    }

  /* Step 3: Store the header of the bands, the creation of the
   *         bands via Band::create_bands() guarantees that the
   *         bands are sorted in increasing BandRegion order.
   *         The shader will require that the bands are the same
   *         size, so we take the greatest generation number
   *         to determine the number of entries needed.
   */
  unsigned int num_horiz_bands, num_vert_bands;

  num_horiz_bands = (1u << max_horiz_generation);
  num_vert_bands = (1u << max_vert_generation);
  m_header_data.resize(2u + num_horiz_bands + num_vert_bands);

  m_header_data[0].x().u = num_horiz_bands;
  m_header_data[0].y().u = num_vert_bands;

  for (unsigned int h = 0, current = 0; h < num_horiz_bands; ++h)
    {
      if (horizontal_bands[current].region().end(max_horiz_generation) == h)
        {
          ++current;
        }

      ASTRALassert(horizontal_bands[current].region().start(max_horiz_generation) <= h);
      ASTRALassert(horizontal_bands[current].region().end(max_horiz_generation) > h);

      m_header_data[1 + h].x().u = horizontal_bands[current].relative_data_offset(BandedCurve::min_band_side);
      m_header_data[1 + h].y().u = horizontal_bands[current].number_curves(BandedCurve::min_band_side);

      m_header_data[1 + h].z().u = horizontal_bands[current].relative_data_offset(BandedCurve::max_band_side);
      m_header_data[1 + h].w().u = horizontal_bands[current].number_curves(BandedCurve::max_band_side);
    }

  for (unsigned int v = 0, current = 0; v < num_vert_bands; ++v)
    {
      if (vertical_bands[current].region().end(max_vert_generation) == v)
        {
          ++current;
        }

      ASTRALassert(vertical_bands[current].region().start(max_vert_generation) <= v);
      ASTRALassert(vertical_bands[current].region().end(max_vert_generation) > v);

      m_header_data[1 + num_horiz_bands + v].x().u = vertical_bands[current].relative_data_offset(BandedCurve::min_band_side);
      m_header_data[1 + num_horiz_bands + v].y().u = vertical_bands[current].number_curves(BandedCurve::min_band_side);

      m_header_data[1 + num_horiz_bands + v].z().u = vertical_bands[current].relative_data_offset(BandedCurve::max_band_side);
      m_header_data[1 + num_horiz_bands + v].w().u = vertical_bands[current].number_curves(BandedCurve::max_band_side);
    }

  m_properties.m_error = geometry.m_error;
  m_properties.m_bb = geometry.m_bb;
  m_properties.m_number_bands = uvec2(num_horiz_bands, num_vert_bands);
  m_properties.m_fp16_data_size = m_band_data.size();
  m_properties.m_generic_data_size = m_header_data.size();

  m_properties.m_average_render_cost = vec2(0.0f, 0.0f);
  for (const auto &b : horizontal_bands)
    {
      m_properties.m_average_render_cost.x() += b.weighted_cost();
    }
  for (const auto &b : vertical_bands)
    {
      m_properties.m_average_render_cost.y() += b.weighted_cost();
    }
}

astral::ItemPath::
~ItemPath()
{
}

uint32_t
astral::ItemPath::
header_location(RenderEngine &engine) const
{
  if (!m_bands)
    {
      std::vector<vec4> tmp;
      std::vector<gvec4> htmp;

      /* we free the memory m_band_data and m_header_data
       * by doing std::vector::swap() since std::vector::clear()
       * does not deallocate.
       */
      tmp.swap(m_band_data);
      htmp.swap(m_header_data);

      ASTRALassert(!m_header);
      m_bands = engine.static_data_allocator16().create(make_c_array(tmp));

      /* we need to adjust the the value for the offset location of m_bands */
      for (unsigned int h = 0; h < m_properties.m_number_bands.x(); ++h)
        {
          htmp[1 + h].x().u += m_bands->location();
          htmp[1 + h].z().u += m_bands->location();
        }

      for (unsigned int v = 0; v < m_properties.m_number_bands.y(); ++v)
        {
          htmp[1 + m_properties.m_number_bands.x() + v].x().u += m_bands->location();
          htmp[1 + m_properties.m_number_bands.x() + v].z().u += m_bands->location();
        }

      m_header = engine.static_data_allocator32().create(make_const_c_array(htmp));
    }

  return m_header->location();
}

astral::BoundingBox<float>
astral::ItemPath::
pack_data(RenderEngine &engine, c_array<const Layer> layers, c_array<gvec4> dst)
{
  BoundingBox<float> return_value;

  if (!layers.empty())
    {
      for (unsigned int i = 0; i < layers.size(); ++i)
        {
          const Layer &layer(layers[i]);
          const ItemPath &item_path(layer.m_item_path);
          ScaleTranslate tr;

          /* We have that
           *   layer.m_transformation : CallerCoordinates --> PathCoordinates
           *   item_path.m_tr       : PathCoordinates   --> [-1, 1]x[-1, 1]
           *
           * and we want
           *
           *   tr : CallerCoordinates --> [-1, 1]x[-1, 1]
           *
           * thus
           *
           *  tr = item_path.m_tr * layer.m_transformation
           */
          tr = item_path.m_tr * layer.m_transformation;
          return_value.union_box(layer.m_transformation.apply_to_bb(item_path.m_properties.m_bb));

          dst[3 * i + 0u].x().f = layer.m_color.x();
          dst[3 * i + 0u].y().f = layer.m_color.y();
          dst[3 * i + 0u].z().f = layer.m_color.z();
          dst[3 * i + 0u].w().f = layer.m_color.w();

          dst[3 * i + 1u].x().f = tr.m_scale.x();
          dst[3 * i + 1u].y().f = tr.m_scale.y();
          dst[3 * i + 1u].z().f = tr.m_translate.x();
          dst[3 * i + 1u].w().f = tr.m_translate.y();

          dst[3 * i + 2u].x().u = item_path.header_location(engine);
          dst[3 * i + 2u].z().u = layer.m_fill_rule;
          dst[3 * i + 2u].w().f = 1.0f;
        }
      dst.back().w().f = -1.0f;
    }
  return return_value;
}

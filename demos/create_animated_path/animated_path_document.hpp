/*!
 * \file animated_path_document.hpp
 * \brief file animated_path_document.hpp
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

#ifndef DEMO_ANINAMATED_PATH_DOCUMENT_HPP
#define DEMO_ANINAMATED_PATH_DOCUMENT_HPP

#include <ostream>
#include <istream>
#include <astral/path.hpp>
#include <astral/animated_path.hpp>
#include <astral/renderer/renderer.hpp>

class AnimatedPathDocument:public astral::reference_counted<AnimatedPathDocument>::non_concurrent
{
public:
  enum path_t
    {
      start_path,
      end_path
    };

  class PointIndex
  {
  public:
    enum invalid_value_t:uint32_t
      {
        invalid_value = 0xFFFFFFFFu
      };

    explicit
    PointIndex(uint32_t v = invalid_value):
      m_value(v)
    {}

    bool
    valid(void) const
    {
      return m_value != invalid_value;
    }

    uint32_t m_value;
  };

  /* Simple class to specify a point along a contour */
  class ContourPoint
  {
  public:
    unsigned int
    curve(void) const
    {
      return m_data.first;
    }

    unsigned int&
    curve(void)
    {
      return m_data.first;
    }

    float
    t(void) const
    {
      return m_data.second;
    }

    float&
    t(void)
    {
      return m_data.second;
    }

    bool
    operator<(const ContourPoint &rhs) const
    {
      return m_data < rhs.m_data;
    }

    bool
    operator!=(const ContourPoint &rhs) const
    {
      return m_data != rhs.m_data;
    }

    bool
    operator==(const ContourPoint &rhs) const
    {
      return m_data == rhs.m_data;
    }

    friend
    std::ostream&
    operator<<(std::ostream &str, const ContourPoint &obj)
    {
      str << "(Curve = " << obj.curve() << ", t = " << obj.t() << ")";
      return str;
    }

    astral::vec2 m_position;

  private:
    std::pair<uint32_t, float> m_data;
  };

  /* create from a pair of paths */
  explicit
  AnimatedPathDocument(const astral::Path &pathA,
                       const astral::Path &pathB);

  const astral::AnimatedPath&
  animated_path(void) const;

  void
  pair_contours(unsigned int A, unsigned int B)
  {
    m_dirty = true;
    m_pairing.pair(A, B);
  }

  void
  remove_pairing(enum path_t path, unsigned int contour)
  {
    m_dirty = true;
    m_pairing.remove_pairing(path, contour);
  }

  void
  collapse_to_a_point(enum path_t path, unsigned int contour, const astral::vec2 &pt)
  {
    m_dirty = true;
    m_pairing.collapse_to_a_point(path, contour, pt);
  }

  /* returns what contour on the other path is paired
   * with the passed contour; a negative value indicates
   * that it is not paired.
   *   -1 --> collapse to the anchor point of the other path
   *   -2 --> collapse to a point returned in *out_collapse_pt
   */
  int
  query_pairing(enum path_t path, unsigned int contour, astral::vec2 *out_collapse_pt) const
  {
    return m_pairing.query_pairing(path, contour, out_collapse_pt);
  }

  int
  query_pairing(enum path_t path, unsigned int contour) const
  {
    astral::vec2 ignored;
    return m_pairing.query_pairing(path, contour, &ignored);
  }

  void
  clear_pairing(void)
  {
    m_dirty = true;
    m_pairing.clear();
  }

  PointIndex
  add_point(enum path_t path, unsigned int contour, unsigned int curve, float t)
  {
    ASTRALassert(contour < m_paths[path].number_contours());
    m_dirty = true;
    return m_paths[path].m_contours[contour].add_point(curve, t);
  }

  /* Command is ignored for the point at the start and end
   * for open contours
   */
  enum astral::return_code
  modify_point(enum path_t path, unsigned int contour,
               PointIndex P, unsigned int new_curve, float new_t)
  {
    ASTRALassert(contour < m_paths[path].number_contours());

    m_dirty = true;
    return m_paths[path].m_contours[contour].modify_point(P, new_curve, new_t);
  }

  /* Command is ignored and returns failure for open contours */
  enum astral::return_code
  mark_as_first(enum path_t path, unsigned int contour, PointIndex P);

  bool
  is_marked_as_first(enum path_t path, unsigned int contour, PointIndex P) const
  {
    ASTRALassert(contour < m_paths[path].number_contours());
    return m_paths[path].m_contours[contour].is_marked_as_first(P);
  }

  /* invalidates all previous PointIndex values returned before hand;
   * in addition, not all points can be deleted. Namely:
   *   - a closed contour must have atleast one point
   *   - an open contour must have the the point at the start of the
   *     curve and a point at the end of the curve
   */
  enum astral::return_code
  delete_point(enum path_t path, unsigned int contour, PointIndex P)
  {
    ASTRALassert(contour < m_paths[path].number_contours());
    m_dirty = true;
    return m_paths[path].m_contours[contour].delete_point(P);
  }

  unsigned int
  number_contours(enum path_t path) const
  {
    return m_paths[path].number_contours();
  }

  const astral::vec2&
  anchor_point(enum path_t path) const
  {
    return m_paths[path].m_anchor_point;
  }

  void
  anchor_point(enum path_t path, const astral::vec2 &p)
  {
    m_dirty = true;
    m_paths[path].m_anchor_point = p;
  }

  const astral::Contour&
  contour(enum path_t path, unsigned int contour) const
  {
    ASTRALassert(contour < m_paths[path].number_contours());
    return m_paths[path].m_contours[contour].contour();
  }

  const astral::Path&
  contour_as_path(enum path_t path, unsigned int contour) const
  {
    ASTRALassert(contour < m_paths[path].number_contours());
    return m_paths[path].m_contours[contour].as_path();
  }

  unsigned int
  number_points(enum path_t path, unsigned int contour) const
  {
    ASTRALassert(contour < m_paths[path].number_contours());
    return m_paths[path].m_contours[contour].number_points();
  }

  const ContourPoint&
  point_information(enum path_t path, unsigned int contour, PointIndex P) const
  {
    ASTRALassert(contour < m_paths[path].number_contours());
    return m_paths[path].m_contours[contour].point_information(P);
  }

  astral::c_array<const ContourPoint>
  sorted_points(enum path_t path, unsigned int contour) const
  {
    ASTRALassert(contour < m_paths[path].number_contours());
    return m_paths[path].m_contours[contour].sorted_points();
  }

  PointIndex
  nearest_point(enum path_t path, unsigned int contour, astral::vec2 p) const
  {
    ASTRALassert(contour < m_paths[path].number_contours());
    return m_paths[path].m_contours[contour].nearest_point(p);
  }

  unsigned int
  nearest_contour(float tol, enum path_t path, astral::vec2 p) const
  {
    return m_paths[path].nearest_contour(tol, p);
  }

  astral::Contour::PointQueryResult
  query_contour(float tol, enum path_t path, unsigned int contour, astral::vec2 p) const
  {
    ASTRALassert(contour < m_paths[path].number_contours());
    return m_paths[path].m_contours[contour].contour().distance_to_contour(tol, p);
  }

  const astral::Path&
  as_path(enum path_t path, unsigned int contour) const
  {
    ASTRALassert(contour < m_paths[path].number_contours());
    return m_paths[path].m_contours[contour].as_path();
  }

  void
  reverse_contour(enum path_t path, unsigned int contour)
  {
    ASTRALassert(contour < m_paths[path].number_contours());
    m_paths[path].reverse_contour(contour);
    m_dirty = true;
  }

  void
  clear_points(enum path_t path, unsigned int contour)
  {
    ASTRALassert(contour < m_paths[path].number_contours());
    m_paths[path].m_contours[contour].clear_points();
    m_dirty = true;
  }

  void
  clear_points(enum path_t path);

  void
  clear(void)
  {
    clear_pairing();
    clear_points(start_path);
    clear_points(end_path);
  }

  /* load from a file */
  static
  astral::reference_counted_ptr<AnimatedPathDocument>
  load_from_file(std::istream &input_stream);

  void
  save_to_file(std::ostream &dst) const;

private:
  class ContourPairing
  {
  public:
    ContourPairing(const astral::Path &A, const astral::Path &B)
    {
      m_pair_lookup[start_path].resize(A.number_contours(), -1);
      m_pair_lookup[end_path].resize(B.number_contours(), -1);

      m_collapse_points[start_path].resize(A.number_contours(), astral::vec2(0.0f, 0.0f));
      m_collapse_points[end_path].resize(B.number_contours(), astral::vec2(0.0f, 0.0f));
    }

    ContourPairing(void)
    {}

    void
    clear(void)
    {
      std::fill(m_pair_lookup[start_path].begin(), m_pair_lookup[start_path].end(), -1);
      std::fill(m_pair_lookup[end_path].begin(), m_pair_lookup[end_path].end(), -1);
      m_pairs.clear();
    }

    void
    pair(unsigned int A, unsigned int B);

    astral::c_array<const astral::vecN<int32_t, 2>>
    pairs(void) const
    {
      return astral::make_c_array(m_pairs);
    }

    int
    query_pairing(enum path_t path, unsigned int contour, astral::vec2 *out_collapse_pt) const
    {
      int idx;

      /* get the index into pairs that stores the pairing */
      ASTRALassert(contour < m_pair_lookup[path].size());
      idx = m_pair_lookup[path][contour];

      /* get the "other" value */
      ASTRALassert(idx == -1 || idx == -2 || (idx >= 0 && idx < static_cast<int>(m_pairs.size())));
      *out_collapse_pt = m_collapse_points[path][contour];
      return (idx >= 0) ? m_pairs[idx][1 - path] : idx;
    }

    void
    collapse_to_a_point(enum path_t path, unsigned int contour, const astral::vec2 &pt)
    {
      remove_pairing(path, contour);

      ASTRALassert(m_pair_lookup[path][contour] == -1);
      m_pair_lookup[path][contour] = -2;
      m_collapse_points[path][contour] = pt;
    }

    void
    remove_pairing(enum path_t path, unsigned int contour);

    enum astral::return_code
    load_from_file(unsigned int contour_countA,
                   unsigned int contour_countB,
                   std::istream &input_stream);

    void
    save_to_file(std::ostream &dst) const;

  private:
    /* idx is an index into m_pairs */
    void
    remove_pairing(unsigned int idx);

    /* idx is an index into m_pairs */
    void
    swap_with_back(unsigned int idx);

    bool
    consistent(void) const;

    void
    save_point_collapse_values(enum path_t path, std::ostream &dst) const;

    /* m_pair_lookup[path][i] = index into m_pairs holding
     * the pairing of the i'th contour of path
     */
    astral::vecN<std::vector<int>, 2> m_pair_lookup;

    /* m_collapse_points[path][i] = collapse point for
     * i'th contour of path
     */
    astral::vecN<std::vector<astral::vec2>, 2> m_collapse_points;

    /* List of pairings
     *  [start_path] = which contour from start_path
     *  [end_path] = which contour from end_path
     */
    std::vector<astral::vecN<int32_t, 2>> m_pairs;
  };

  /* NOTE: if the contour is open then it is the caller's responsibility to
   *       to make sure that (curve = 0, t = 0.0) and (curve = last, t = 1.0)
   *       are explicitely added.
   */
  class ContourPointSequence
  {
  public:
    ContourPointSequence(void):
      m_first_point(0),
      m_dirty(true)
    {}

    void
    clear(void)
    {
      m_pts.clear();
      m_dirty = true;
      m_first_point = 0;
    }

    PointIndex
    add(const ContourPoint &pt)
    {
      m_dirty = true;
      m_pts.push_back(pt);

      return PointIndex(m_pts.size() - 1u);
    }

    void
    modify(PointIndex P, unsigned int new_curve, float new_t, astral::vec2 new_p)
    {
      ASTRALassert(P.m_value < m_pts.size());

      m_dirty = true;
      m_pts[P.m_value].curve() = new_curve;
      m_pts[P.m_value].t() = new_t;
      m_pts[P.m_value].m_position = new_p;
    }

    void
    on_contour_reverse(unsigned int curve_count);

    void
    mark_as_first(PointIndex P)
    {
      ASTRALassert(P.m_value < m_pts.size());

      m_dirty = true;
      m_first_point = P.m_value;
    }

    const ContourPoint&
    point(PointIndex P) const
    {
      ASTRALassert(P.m_value < m_pts.size());
      return m_pts[P.m_value];
    }

    unsigned int
    number_points(void) const
    {
      return m_pts.size();
    }

    void
    delete_point(PointIndex P)
    {
      ASTRALassert(P.m_value < m_pts.size());
      if (P.m_value != m_pts.size() - 1)
        {
          std::swap(m_pts.back(), m_pts[P.m_value]);
        }

      if (m_pts.size() - 1 == m_first_point)
        {
          m_first_point = P.m_value;
        }
      else if (m_first_point == P.m_value)
        {
          m_first_point = 0;
        }

      m_dirty = true;
      m_pts.pop_back();
    }

    PointIndex
    nearest_point(bool is_open_contour, astral::vec2 p) const;

    /* returns an array of points, if the contour is closed,
     * adds a point at the end to signifiy the closing curve
     */
    astral::c_array<const ContourPoint>
    sorted_points(const astral::Contour &contour) const;

    enum astral::return_code
    load_from_file(std::istream &input_stream);

    void
    save_to_file(std::ostream &dst) const;

    unsigned int
    first_point(void) const
    {
      return m_first_point;
    }

    bool
    empty(void) const
    {
      return m_pts.empty();
    }

  private:
    class Sorter;

    static
    enum astral::return_code
    load_from_file(std::istream &input_stream, ContourPoint *dst);

    static
    void
    save_to_file(std::ostream &dst, const ContourPoint &src);

    // raw points in order they were added
    std::vector<ContourPoint> m_pts;
    uint32_t m_first_point;

    mutable bool m_dirty;
    mutable std::vector<ContourPoint> m_sorted_points;
  };

  class PerPathContour
  {
  public:
    PerPathContour(void):
      m_as_path_dirty(true)
    {}

    void
    reverse_contour(void)
    {
      m_as_path_dirty = true;
      m_contour->inplace_reverse();
      m_pts.on_contour_reverse(m_contour->curves().size());
    }

    PointIndex
    add_point(unsigned int curve, float t)
    {
      ContourPoint P;

      P.curve() = curve;
      P.t() = t;
      P.m_position = m_contour->curve(curve).eval_at(t);

      return m_pts.add(P);
    }

    enum astral::return_code
    modify_point(PointIndex P, unsigned int new_curve, float new_t);

    enum astral::return_code
    mark_as_first(PointIndex P);

    bool
    is_marked_as_first(PointIndex P) const
    {
      return m_contour->closed() ?
        P.valid() && m_pts.first_point() == P.m_value :
        P.valid() && P.m_value == 0;
    }

    enum astral::return_code
    delete_point(PointIndex P);

    unsigned int
    number_points(void) const
    {
      return m_pts.number_points();
    }

    const ContourPoint&
    point_information(PointIndex P) const
    {
      return m_pts.point(P);
    }

    PointIndex
    nearest_point(astral::vec2 p) const
    {
      return m_pts.nearest_point(!contour().closed(), p);
    }

    void
    clear_points(void);

    astral::c_array<const ContourPoint>
    sorted_points(void) const
    {
      return m_pts.sorted_points(*m_contour);
    }

    const astral::Contour&
    contour(void) const
    {
      return *m_contour;
    }

    /* changing contour clears the points */
    void
    contour(astral::reference_counted_ptr<astral::Contour> C);

    const astral::Path&
    as_path(void) const
    {
      if (m_as_path_dirty)
        {
          m_as_path.clear();
          m_as_path.add_contour(*m_contour);
          m_as_path_dirty = false;
        }

      return m_as_path;
    }

    enum astral::return_code
    load_pts_from_file(std::istream &input_stream)
    {
      return m_pts.load_from_file(input_stream);
    }

    void
    save_pts_to_file(std::ostream &dst) const
    {
      m_pts.save_to_file(dst);
    }

  private:
    bool
    is_constant_point(PointIndex P)
    {
      return (m_contour->closed()) ?
        false :
        P.m_value < 2u;
    }

    void
    add_required_points(void);

    ContourPointSequence m_pts;
    astral::reference_counted_ptr<astral::Contour> m_contour;
    mutable bool m_as_path_dirty;
    mutable astral::Path m_as_path;
  };

  class Path
  {
  public:
    Path(void):
      m_anchor_point(0.0f, 0.0f)
    {}

    void
    copy_from_path(const astral::Path &src);

    enum astral::return_code
    load_from_file(std::istream &input_stream);

    void
    save_to_file(std::ostream &dst) const;

    void
    reverse_contour(unsigned int C)
    {
      ASTRALassert(C < m_contours.size());
      m_contours[C].reverse_contour();
    }

    unsigned int
    number_contours(void) const
    {
      return m_contours.size();
    }

    unsigned int
    nearest_contour(float tol, astral::vec2 p) const;

    astral::vec2 m_anchor_point;
    std::vector<PerPathContour> m_contours;

  private:
    static
    astral::reference_counted_ptr<astral::Contour>
    load_contour_from_file(std::istream &input_stream);

    static
    void
    save_to_file(std::ostream &dst, const astral::ContourData &contour);

    static
    enum astral::return_code
    append_curve_from_file(std::istream &input_stream, astral::ContourData *dst);

    static
    void
    save_to_file(std::ostream &dst, const astral::ContourCurve &curve);
  };

  class CompoundCurveSequence
  {
  public:
    CompoundCurveSequence(const astral::Transformation &transformation,
                          const astral::Contour &contour,
                          astral::c_array<const ContourPoint> pts,
                          unsigned int number_segments);

    astral::c_array<const astral::AnimatedContour::CompoundCurve>
    compound_curves(void) const
    {
      return astral::make_c_array(m_compoud_curves);
    }

  private:
    /* Adding curves needs to be done with extreme care. It might be tempting
     * to have add_curves_implement() partition curves from scratch. However,
     * this can lead to not-quite matching curves from round-off error. Thus,
     * we have CurrentCurve which embodies the current curve getting worked
     * on in the add curve sequence. The start of it is the ending of the last
     * curve added.
     */
    class CurrentCurve
    {
    public:
      /* The ctor initializes CurrentCurve to start at the given ContourPoint,
       * this is why it takes the split ContourCurveSplit::after_t().
       */
      CurrentCurve(const astral::ContourData &contour, const ContourPoint &pt):
        m_curve(astral::ContourCurveSplit(false, contour.curve(pt.curve()), pt.t()).after_t()),
        m_curve_id(pt.curve()),
        m_range(pt.t(), 1.0f)
      {}

      /* Initializes to start at the start of the given curve */
      CurrentCurve(const astral::ContourCurve &curve, unsigned int id):
        m_curve(curve),
        m_curve_id(id),
        m_range(0.0f, 1.0f)
      {}

      astral::ContourCurve
      split_at(const ContourPoint &from)
      {
        float rel_t;

        ASTRALassert(from.curve() == m_curve_id);
        rel_t = (from.t() - m_range.m_begin) / (m_range.m_end - m_range.m_begin);

        astral::ContourCurveSplit split(false, m_curve, rel_t);
        m_curve = split.after_t();
        m_range.m_begin = from.t();

        return split.before_t();
      }

      astral::ContourCurve m_curve;
      unsigned int m_curve_id;
      astral::range_type<float> m_range;
    };

    astral::range_type<unsigned int>
    add_curves(const astral::Transformation &transformation, const astral::Contour &contour,
               CurrentCurve *current, const ContourPoint &from, const ContourPoint &to);

    void
    add_curves_implement(const astral::Transformation &transformation, const astral::Contour &contour,
                         CurrentCurve *current, const ContourPoint &from, const ContourPoint &to);

    void
    add_curve(const astral::Transformation &transformation,
              const astral::ContourCurve &curve);

    std::vector<astral::AnimatedContour::CompoundCurve> m_compoud_curves;

    /* index into m_curves_backing */
    std::vector<astral::range_type<unsigned int>> m_compound_curve_range;

    std::vector<float> m_lengths_backing;
    std::vector<astral::ContourCurve> m_curves_backing;
  };

  // private ctor for loading.
  AnimatedPathDocument(void):
    m_dirty(true)
  {}

  static
  astral::reference_counted_ptr<astral::AnimatedContour>
  create_animated_contour(const astral::Transformation &trA, const PerPathContour &contourA,
                          const astral::Transformation &trB, const PerPathContour &contourB);

  static
  void
  create_translated_contour(const astral::Transformation &tr,
                            astral::c_array<const astral::ContourCurve> curves,
                            std::vector<astral::ContourCurve> *out_curves);

  astral::vecN<Path, 2> m_paths;
  ContourPairing m_pairing;

  mutable bool m_dirty;
  mutable astral::AnimatedPath m_animated_path;
};

#endif

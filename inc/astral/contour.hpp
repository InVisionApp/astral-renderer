/*!
 * \file contour.hpp
 * \brief file contour.hpp
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

#ifndef ASTRAL_CONTOUR_HPP
#define ASTRAL_CONTOUR_HPP

#include <vector>
#include <astral/util/c_array.hpp>
#include <astral/util/rounded_rect.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/bounding_box.hpp>
#include <astral/util/reference_counted.hpp>
#include <astral/contour_curve.hpp>
#include <astral/renderer/vertex_data.hpp>
#include <astral/renderer/shader/fill_stc_shader.hpp>
#include <astral/renderer/shader/stroke_shader.hpp>

namespace astral
{
  class RenderEngine;

/*!\addtogroup Paths
 * @{
 */

  /*!
   * Enumeration to control the orientation of specifying a
   * contour as a simple shape.
   */
  enum contour_direction_t:uint32_t
    {
      /*!
       * Indicates to walk the geometry of a simple shape
       * in a clockwise direction (assuming that y-coordinate
       * increases from the top to the bottom of the coordinate
       * system).
       */
      contour_direction_clockwise,

      /*!
       * Indicates to walk the geometry of a simple shape in a
       * counter clockwise direction (assuming that y-coordinate
       * increases from the top to the bottom of the coordinate
       * system).
       */
      contour_direction_counter_clockwise,
    };

  /*!
   * Enumeration to describe tessellation of curves for the purpose
   * of filling a path.
   */
  enum contour_fill_approximation_t:uint32_t
    {
      /*!
       * Tessellate as needed for error and also tessellate long curves
       * into shorter curves
       */
      contour_fill_approximation_tessellate_long_curves,

      /*!
       * Tessellation only as needed for error
       */
      contour_fill_approximation_allow_long_curves,

      number_contour_fill_approximation
    };

  /*!
   * \brief
   * A \ref ContourData represents the geometry of a single contour of a Path.
   */
  class ContourData
  {
  public:
    /*!
     * \brief
     * Class to describe how many curves were added
     * when calling \ref arc_to() or \ref arc_close().
     */
    class arc_curve_stats
    {
    public:
      /*!
       * Returns an upper bound for the number of \ref ContourCurve objects
       * that are used to represent an arc.
       * \param radians angle of arc in radians, the sign of the
       *                angle gives the arc direction. The value
       *                provided should be no more in absolute
       *                value than 2.0 * \ref ASTRAL_PI.
       */
      static
      unsigned int
      number_arcs(float radians);

      /*!
       * The number of curves added.
       */
      unsigned int m_number_curves;

      /*!
       * Location to which to write the pamamter
       * lengths of curve added; a single call to
       * ContourData::arc_to() or ContourData::arc_curve()
       * will add no more than \ref number_arcs() arcs.
       */
      c_array<float> m_parameter_lengths;
    };

    /*!
     * Ctor, initializes the \ref Contour as empty.
     */
    ContourData(void):
      m_start_pt(0.0f, 0.0f),
      m_last_end_pt(0.0f, 0.0f),
      m_santize_curves_on_adding(true),
      m_sanitized(true),
      m_closed(false)
    {}

    /*!
     * Copy ctor
     */
    ContourData(const ContourData&) = default;

    /*!
     * Move ctor
     */
    ContourData(ContourData &&obj) noexcept:
      m_start_pt(obj.m_start_pt),
      m_last_end_pt(obj.m_last_end_pt),
      m_curves(std::move(obj.m_curves)),
      m_santize_curves_on_adding(obj.m_santize_curves_on_adding),
      m_sanitized(obj.m_sanitized),
      m_closed(obj.m_closed),
      m_bb(obj.m_bb),
      m_control_point_bb(obj.m_control_point_bb)
    {}

    virtual
    ~ContourData()
    {}

    /*!
     * assignment move operator
     */
    ContourData&
    operator=(ContourData &&) noexcept;

    /*!
     * assignment operator
     */
    ContourData&
    operator=(const ContourData &obj) = default;

    /*!
     * Clear the \ref Contour, i.e. clear all curves
     * from the contour.
     */
    void
    clear(void)
    {
      m_last_end_pt = vec2(0.0f, 0.0f);
      m_closed = false;
      m_sanitized = true;
      m_curves.clear();
      m_bb.clear();
      m_join_bb.clear();
      m_control_point_bb.clear();
      mark_dirty();
    }

    /*!
     * If true, wen adding curves to this astral::ContourData,
     * curves are filtered as specified in santize(). Default
     * value is true.
     */
    bool
    santize_curves_on_adding(void) const
    {
      return m_santize_curves_on_adding;
    }

    /*!
     * Set the value returned by santize_curves_on_adding().
     * Does not effect any curves already added. To sanitize
     * the curves already added, call sanitize().
     */
    void
    santize_curves_on_adding(bool v)
    {
      m_santize_curves_on_adding = v;
    }

    /*!
     * Returns true if all curves added to the contour
     * have been sanitized.
     */
    bool
    is_sanitized(void) const
    {
      return m_sanitized;
    }

    /*!
     * Returns if the contour is empty.
     */
    bool
    empty(void) const
    {
      return m_curves.empty();
    }

    /*!
     * Start the contour, the contour must be empty()
     * for it to be legal to start it.
     * \param p first point of the first curve of the Contour
     */
    void
    start(vec2 p)
    {
      ASTRALassert(empty());
      m_start_pt = m_last_end_pt = p;
      m_bb.union_point(p);
    }

    /*!
     * Start the contour with a curve, the contour must be empty()
     * for it to be legal to start it.
     * \param curve curve with which to start the contour
     */
    void
    start(const ContourCurve &curve)
    {
      ASTRALassert(empty());
      m_start_pt = curve.start_pt();
      curve_to(curve);
    }

    /*!
     * Returne the current point, i.e. the starting point of the
     * next curve added.
     */
    vec2
    current_pt(void) const
    {
      return m_last_end_pt;
    }

    /*!
     * Generic curve_to() passing a \ref ContourCurve. If no curves
     * have yet been added, the start of the contour becomes the start
     * of the passed curve. If a curve has already been added, it is
     * an error to pass a \ref ContourCurve whose value from \ref
     * ContourCurve::start_pt() is not the current point of the contour.
     * \param curve curve to add
     */
    void
    curve_to(const ContourCurve &curve);

    /*!
     * Add a line segment to the contour connecting the
     * last point added to the passed point.
     * \param p end point of line segment
     * \param cont_tp curve continuation type; used to determine
     *                how the join between the curve and its predecessor
     *                is drawn when stroked
     */
    void
    line_to(vec2 p, enum ContourCurve::continuation_t cont_tp)
    {
      curve_to(ContourCurve(m_last_end_pt, p, cont_tp));
    }

    /*!
     * Add a quadratic curve to the contour connecting the
     * last point added to the passed point.
     * \param c control point of curve
     * \param p end point of the curve
     * \param cont_tp curve continuation type; used to determine
     *                how the join between the curve and its predecessor
     *                is drawn when stroked
     */
    void
    quadratic_to(vec2 c, vec2 p,
                 enum ContourCurve::continuation_t cont_tp)
    {
      curve_to(ContourCurve(m_last_end_pt, c, p, cont_tp));
    }

    /*!
     * Add a conic curve to the contour connecting the
     * last point added to the passed point.
     * \param w conic weight; a weight of 1 is the same
     *          as a quadratic bezier curve where as a
     *          weight of 0 is the same as a line segment.
     * \param c control point of curve
     * \param p end point of the curve
     * \param cont_tp curve continuation type; used to determine
     *                how the join between the curve and its predecessor
     *                is drawn when stroked
     */
    void
    conic_to(float w, vec2 c, vec2 p,
             enum ContourCurve::continuation_t cont_tp)
    {
      curve_to(ContourCurve(m_last_end_pt, w, c, p, cont_tp));
    }

    /*!
     * Add a cubic curve to the contour connecting the
     * last point added to the passed point.
     * \param c0 first control point of curve
     * \param c1 second control point of curve
     * \param p end point of the curve
     * \param cont_tp curve continuation type; used to determine
     *                how the join between the curve and its predecessor
     *                is drawn when stroked
     */
    void
    cubic_to(vec2 c0, vec2 c1, vec2 p,
             enum ContourCurve::continuation_t cont_tp)
    {
      curve_to(ContourCurve(m_last_end_pt, c0, c1, p, cont_tp));
    }

    /*!
     * Add a line, quadratic or cubic connecting the
     * last point added to the passed point.
     * \param ctl_pts array of control point, the size of
     *                the array must be no greater than 2
     * \param p end point of the curve
     * \param cont_tp curve continuation type; used to determine
     *                how the join between the curve and its predecessor
     *                is drawn when stroked
     */
    void
    curve_to(c_array<const vec2> ctl_pts, vec2 p,
             enum ContourCurve::continuation_t cont_tp)
    {
      curve_to(ContourCurve(m_last_end_pt, ctl_pts, p, cont_tp));
    }

    /*!
     * Generic curve_to() passing a \ref ContourCurve. If not curves
     * hae yet been added, the start of the contour becomes the start
     * of the passed curve. If a curve has already been added, it is
     * an error to pass a \ref ContourCurve whose value from \ref
     * ContourCurve::start_pt() is not the current point of the contour.
     * \param curve curve to add
     * \param cont_tp override the value of ContourCurve::continuation()
     */
    void
    curve_to(const ContourCurve &curve,
             enum ContourCurve::continuation_t cont_tp)
    {
      curve_to(ContourCurve(curve, cont_tp));
    }

    /*!
     * Add an arc to the contour connecting the last point added
     * to the passed point.
     * \param radians angle of arc in radians, the sign of the
     *                angle gives the arc direction. The value
     *                provided should be no more in absolute
     *                value than 2.0 * \ref ASTRAL_PI.
     * \param end_pt end point of the curve
     * \param cont_tp curve continuation type; used to determine
     *                how the join between the curve and its predecessor
     *                is drawn when stroked
     * \param out_data if non-null, place to which to write how
     *                 curves were added
     */
    void
    arc_to(float radians, vec2 end_pt,
           enum ContourCurve::continuation_t cont_tp,
           arc_curve_stats *out_data = nullptr);

    /*!
     * Mark the contour as closed, closing it with a line segment
     * \param cont_tp curve continuation type; used to determine
     *                how the join between the curve and its predecessor
     *                is drawn when stroked
     */
    void
    line_close(enum ContourCurve::continuation_t cont_tp)
    {
      line_to(m_start_pt, cont_tp);
      m_closed = true;
    }

    /*!
     * Mark the contour as closed, closing it with a quadratic curve
     * \param c control point of curve
     * \param cont_tp curve continuation type; used to determine
     *                how the join between the curve and its predecessor
     *                is drawn when stroked
     */
    void
    quadratic_close(vec2 c,
                    enum ContourCurve::continuation_t cont_tp)
    {
      quadratic_to(c, m_start_pt, cont_tp);
      m_closed = true;
    }

    /*!
     * Mark the contour as closed, closing it with a quadratic curve
     * \param w conic weight; a weight of 1 is the same
     *          as a quadratic bezier curve where as a
     *          weight of 0 is the same as a line segment.
     * \param c control point of curve
     * \param cont_tp curve continuation type; used to determine
     *                how the join between the curve and its predecessor
     *                is drawn when stroked
     */
    void
    conic_close(float w, vec2 c,
                enum ContourCurve::continuation_t cont_tp)
    {
      conic_to(w, c, m_start_pt, cont_tp);
      m_closed = true;
    }

    /*!
     * Mark the contour as closed, closing it with a cubic curve
     * \param c0 first control point of curve
     * \param c1 second control point of curve
     * \param cont_tp curve continuation type; used to determine
     *                how the join between the curve and its predecessor
     *                is drawn when stroked
     */
    void
    cubic_close(vec2 c0, vec2 c1,
                enum ContourCurve::continuation_t cont_tp)
    {
      cubic_to(c0, c1, m_start_pt, cont_tp);
      m_closed = true;
    }

    /*!
     * Mark the contour as closed, closing it with a
     * line, quadratic or cubic curve
     * \param ctl_pts array of control point, the size of
     *                the array must be no greater than 2
     * \param cont_tp curve continuation type; used to determine
     *                how the join between the curve and its predecessor
     *                is drawn when stroked
     */
    void
    curve_close(c_array<const vec2> ctl_pts,
                enum ContourCurve::continuation_t cont_tp)
    {
      curve_to(ctl_pts, m_start_pt, cont_tp);
      m_closed = true;
    }

    /*!
     * Close the contour with an arc.
     * \param radians angle of arc in radians, the sign of the
     *                angle gives the arc direction.
     * \param cont_tp curve continuation type; used to determine
     *                how the join between the curve and its predecessor
     *                is drawn when stroked
     * \param out_data if non-null, place to which to write how
     *                 curves were added
     */
    void
    arc_close(float radians,
              enum ContourCurve::continuation_t cont_tp,
              arc_curve_stats *out_data = nullptr)
    {
      arc_to(radians, m_start_pt, cont_tp, out_data);
      m_closed = true;
    }

    /*!
     * Mark the contour as closed, closing it with the
     * passed curve
     */
    void
    curve_close(const ContourCurve &curve)
    {
      curve_to(curve);
      m_closed = true;
    }

    /*!
     * Marks the contour as closed, if the starting
     * point and the ending point are different, adds
     * a line_to() to the starting point as well.
     * \param force_add if true adds a line connecting
     *                  the end to teh start even if the
     *                  points are the same
     */
    void
    close(bool force_add = false);

    /*!
     * Changes the contour to be the contour of a rounded rect;
     * all previously added curves are removed. It is expected
     * that the rectangle added has already been sanitized (see
     * RoundedRect::sanitize_simple()and RoundedRect::santitize_scale()).
     * Rectangles that are not sanitized produce incorrect results.
     * \param rect astral::RoundedRect specifying the new
     *             data for the contour.
     * \param d orientation
     * \param starting_point starting point of the contour
     */
    void
    make_as_rounded_rect(const RoundedRect &rect, enum contour_direction_t d,
                         RoundedRect::Point starting_point);

    /*!
     * Changes the contour to be the contour of a rect;
     * all previously added curves are removed. It is expected
     * that the rectangle added has already been standardized
     * (see Rect::standardize()). Rectangles that are not
     * standardized produce incorrect results
     * \param rect astral::Rect specifying the new
     *             data for the contour.
     * \param d orientation
     * \param starting_point starting point of the contour
     */
    void
    make_as_rect(const Rect &rect, enum contour_direction_t d,
                 enum Rect::corner_t starting_point = Rect::minx_miny_corner);

    /*!
     * Changes the contour to be the contour of an ellipse;
     * all previously added curves are removed.
     * \param rect astral::Rect specifying the dimensions
     *             and location of the oval the new
     *             data for the contour; the rectangle must
     *             be standardized, see Rect::standardize()
     * \param d orientation
     * \param starting_point side of the starting point of the contour
     */
    void
    make_as_oval(const Rect &rect, enum contour_direction_t d,
                 enum Rect::side_t starting_point);

    /*!
     * Santizes the curves of the contour which means:
     * - removes all line segments who start and end points are equal
     * - convert a cubic that is exactly a quadratic into a quadratic
     * - convert any non-linesegment curve into a line segment if the
     *   curve has that ContourCurve::flatness() is 0.0
     * - convert quadratic and conic curves whose start and end points
     *   are the same into two line segment whose trace together is the
     *   same as the original curve
     * .
     * Note that santizing preserves the starting point and ending
     * point of the contour. Returns true if the contour was modified.
     */
    bool
    sanitize(void);

    /*!
     * Set the values of this ContourData to another.
     */
    void
    set_values(const ContourData &obj);

    /*!
     * Returns the number of curves of the Contour
     */
    unsigned int
    number_curves(void) const
    {
      return m_curves.size();
    }

    /*!
     * Returns the points of the named curve of the contour
     * \param N which curve with 0 <= N < number_curves().
     */
    const ContourCurve&
    curve(unsigned int N) const
    {
      ASTRALassert(N < m_curves.size());
      return m_curves[N];
    }

    /*!
     * Returns the starting point of the Contour,
     * as specified by start().
     */
    vec2
    start(void) const
    {
      return m_start_pt;
    }

    /*!
     * Returns all the curves of the contour. NOTE: the
     * backing of the returned c_array is only guarnateed
     * to be valid until the geometry of the contour
     * is changed.
     */
    c_array<const ContourCurve>
    curves(void) const
    {
      return make_c_array(m_curves);
    }

    /*!
     * Returns true if the contour is closed.
     */
    bool
    closed(void) const
    {
      return m_closed;
    }

    /*!
     * Returns a tight bounding box of the \ref Contour
     */
    const BoundingBox<float>&
    bounding_box(void) const
    {
      return m_bb;
    }

    /*!
     * Returns same result as bounding_box(float),
     * presence is for allowing to have template functions
     * that work with Contour and AnimatedContour objects
     */
    const BoundingBox<float>&
    bounding_box(float) const
    {
      return bounding_box();
    }

    /*!
     * Returns the bounding box of the joins of the
     * \ref Contour
     */
    const BoundingBox<float>&
    join_bounding_box(void) const
    {
      return m_join_bb;
    }

    /*!
     * Returns same result as join_bounding_box(float),
     * presence is for allowing to have template functions
     * that work with Contour and AnimatedContour objects
     */
    const BoundingBox<float>&
    join_bounding_box(float) const
    {
      return join_bounding_box();
    }

    /*!
     * Returns a bounding box that is the union of
     * ContourCurve::control_point_bounding_box()
     * of the curves that comprise the contour.
     */
    const BoundingBox<float>&
    control_point_bounding_box(void) const
    {
      return m_control_point_bb;
    }

    /*!
     * Returns same result as control_point_bounding_box(float),
     * presence is for allowing to have template functions
     * that work with Contour and AnimatedContour objects
     */
    const BoundingBox<float>&
    control_point_bounding_box(float) const
    {
      return control_point_bounding_box();
    }

    /*!
     * Return the contour reversed.
     */
    ContourData
    reverse(void) const;

    /*!
     * Reverse this contour in-place
     */
    void
    inplace_reverse(void);

    /*!
     * Set the named curve to be the first curve of the
     * contour; may only be called if this contour is
     * finished and closed
     * \param I index into curves() of the curve to become
     *          the first curve.
     */
    void
    make_curve_first(unsigned int I);

  private:
    virtual
    void
    mark_dirty(void)
    {}

    void
    update_bbs(void)
    {
      m_bb.union_box(m_curves.back().tight_bounding_box());
      m_control_point_bb.union_box(m_curves.back().control_point_bounding_box());
      if (m_curves.back().continuation() == ContourCurve::not_continuation_curve)
        {
          m_join_bb.union_point(m_curves.back().start_pt());
        }
    }

    vec2 m_start_pt, m_last_end_pt;
    std::vector<ContourCurve> m_curves;
    bool m_santize_curves_on_adding, m_sanitized, m_closed;
    BoundingBox<float> m_bb, m_join_bb, m_control_point_bb;
  };

  /*!
   * \brief
   * An astral::Contour represents a single contour of am
   * astal::Path.
   */
  class Contour:
    public reference_counted<Contour>::non_concurrent,
    public ContourData
  {
  public:
    /*!
     * Class to provide the output information for querying
     * the winding impact, distance and nearest point on the
     * contour to a point.
     */
    class PointQueryResult
    {
    public:
      PointQueryResult(void):
        m_distance(-1.0f),
        m_closest_curve(-1),
        m_closest_point_t(-1.0f),
        m_winding_impact(0)
      {}

      /*!
       * Gives the distance to the contour from the query point;
       * a negative value indicates that the test was skipped.
       */
      float m_distance;

      /*!
       * The curve holding the closest point to the query point; this
       * is an index to feed to array returned by ContourData::curves().
       * A negative index indicates that the test was skipped.
       */
      int m_closest_curve;

      /*!
       * The "time" on the curve at which the closest point came.
       */
      float m_closest_point_t;

      /*!
       * The impact on the winding number
       */
      int m_winding_impact;
    };

    /*!
     * Create an empty Contour
     */
    static
    reference_counted_ptr<Contour>
    create(void)
    {
      return ASTRALnew Contour();
    }

    /*!
     * Create a \ref Contour from the value of a \ref ContourData.
     * Values are -copied-.
     */
    static
    reference_counted_ptr<Contour>
    create(const ContourData &obj)
    {
      return ASTRALnew Contour(obj);
    }

    ~Contour();

    /*!
     * Returns the contour geometry with all cubic curves
     * approximated by quadratic curves. The backing of the
     * arrays in the returned object are only guaranteed to
     * be present until the Contour is modified.
     * \param error_tol the requested value for which the error is
     *                  to be no more than
     * \param actual_error if non-null, location to which to write
     *                     the actual error in the returned
     *                     approximation
     */
    c_array<const ContourCurve>
    item_path_approximated_geometry(float error_tol,
                                      float *actual_error = nullptr) const;

    /*!
     * Returns the contour geometry with all cubic and conic
     * curves approximated by quadratic curves. The backing
     * of the arrays in the returned object are only guaranteed to
     * be present until the Contour is modified.
     * \param error_tol the requested value for which the error is
     *                  to be no more than
     * \param ct specifies nature of tessellation
     * \param actual_error if non-null, location to which to write
     *                     the actual error in the returned
     *                     approximation
     */
    c_array<const ContourCurve>
    fill_approximated_geometry(float error_tol,
                               enum contour_fill_approximation_t ct,
                               float *actual_error = nullptr) const;

    /*!
     * Returns the contour geometry with all cubic curves and
     * conics approximated by quadratic curves with the error
     * of approximating the original contour with the returned
     * quadratic curves approximated by bi-arcs as in the paper
     * "Approximation of quadratic Bezier curves by arc splines"
     * by D.J. Walton and D.S. Meek added to the error. The backing
     * of the arrays in the returned object are only guaranteed
     * to be present until the \ref Contour is modified.
     * \param error_tol the requested value for which the error is
     *                  to be no more than
     * \param actual_error if non-null, location to which to write
     *                     the actual error in the returned
     *                     approximation
     */
    c_array<const ContourCurve>
    stroke_approximated_geometry(float error_tol,
                                 float *actual_error = nullptr) const;

    /*!
     * Returns the data for drawing the contour filled
     * using stencil-then cover.
     * \param tol error tolerance allowed between the actual
     *            path and the rendered path in path coordinates
     * \param engine the astral::RenderEngine that does the
     *               rendering; the passed value must be the
     *               same for the lifetime of the astral::Path.
     * \param ct specifies nature of tessellation
     * \param actual_error if non-null, location to which to write
     *                     the actual error in the returned
     *                     approximation
     */
    const FillSTCShader::CookedData&
    fill_render_data(float tol, RenderEngine &engine,
                     enum contour_fill_approximation_t ct,
                     float *actual_error = nullptr) const;

    /*!
     * Returns the data for drawing the contour stroked
     * \param tol error tolerance allowed between the actual
     *            path and the rendered path in path coordinates
     * \param engine the astral::RenderEngine that does the
     *               rendering; the passed value must be the
     *               same for the lifetime of the astral::Path.
     * \param actual_error if non-null, location to which to write
     *                     the actual error in the returned
     *                     approximation
     */
    const StrokeShader::CookedData&
    stroke_render_data(float tol, RenderEngine &engine,
                       float *actual_error = nullptr) const;

    /*!
     * Returns the data for drawing the contour stroked; in
     * constrast to stroke_render_data(), the returned data
     * does not support a querying for sparse stroking via
     * astral::StrokeQuery.
     * \param tol error tolerance allowed between the actual
     *            path and the rendered path in path coordinates
     * \param engine the astral::RenderEngine that does the
     *               rendering; the passed value must be the
     *               same for the lifetime of the astral::Path.
     * \param actual_error if non-null, location to which to write
     *                     the actual error in the returned
     *                     approximation
     */
    const StrokeShader::SimpleCookedData&
    simple_stroke_render_data(float tol, RenderEngine &engine,
                              float *actual_error = nullptr) const;

    /*!
     * Computes the L1-distance to the contour.
     * \param tol tolerance of query
     * \param pt the query point
     * \param distance_cull if non-negative and if the query point is atleast
     *                      this distance outside of the bounding box, the
     *                      function will early out.
     */
    PointQueryResult
    distance_to_contour(float tol, const vec2 &pt, float distance_cull) const;

    /*!
     * Computes the L1-distance to the contour.
     * \param tol tolerance of query
     * \param pt the query point
     */
    PointQueryResult
    distance_to_contour(float tol, const vec2 &pt) const
    {
      return distance_to_contour(tol, pt, -1.0f);
    }

  private:
    class DataGenerator;

    explicit
    Contour(const ContourData &obj);

    Contour(void);

    virtual
    void
    mark_dirty(void) override final;

    DataGenerator&
    data_generator(void) const;

    mutable reference_counted_ptr<DataGenerator> m_data_generator;
  };

/*! @} */
}

#endif

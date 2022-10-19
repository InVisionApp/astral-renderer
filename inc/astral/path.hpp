/*!
 * \file path.hpp
 * \brief file path.hpp
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

#ifndef ASTRAL_PATH_HPP
#define ASTRAL_PATH_HPP

#include <vector>
#include <astral/contour.hpp>
#include <astral/util/bounding_box.hpp>
#include <astral/renderer/vertex_data.hpp>
#include <astral/renderer/shader/fill_stc_shader.hpp>
#include <astral/renderer/shader/stroke_shader.hpp>

namespace astral
{
  class RenderEngine;
  class ItemPath;

/*!\addtogroup Paths
 * @{
 */

  /*!
   * \brief
   * A Path is an array of \ref Contour objects.
   */
  class Path
  {
  public:
    /*!
     * Conveniance typedef
     */
    typedef ContourData::arc_curve_stats arc_curve_stats;

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
        m_closest_contour(-1),
        m_closest_curve(-1),
        m_closest_point_t(-1.0f),
        m_winding_impact(0)
      {}

      /*!
       * Given a Contour::PointQueryResult from a named
       * contour, absorb the result; i.e. add to the winding
       * impact and min against the distance value.
       */
      void
      absorb(int contour, const Contour::PointQueryResult &v)
      {
        m_winding_impact += v.m_winding_impact;
        if (v.m_closest_curve >= 0 && (m_closest_contour < 0 || v.m_distance < m_distance))
          {
            m_distance = v.m_distance;
            m_closest_contour = contour;
            m_closest_curve = v.m_closest_curve;
            m_closest_point_t = v.m_closest_point_t;
          }
      }

      /*!
       * Gives the distance to the contour from the query point;
       * a negative value indicates that the test was skipped.
       */
      float m_distance;

      /*!
       * Which contour holds the closest point; a negative value
       * indicates that test was early culled.
       */
      int m_closest_contour;

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
     * Ctor, initializes the Path as empty
     */
    Path(void);

    /*!
     * Move ctor.
     */
    Path(Path &&) noexcept;

    ~Path();

    /*!
     * Copy ctor
     */
    Path(const Path&);

    /*!
     * Assignment operator
     */
    Path&
    operator=(const Path&);

    /*!
     * Move assignment operator
     */
    Path&
    operator=(Path &&obj) noexcept;

    /*!
     * Swap operator.
     */
    void
    swap(Path &obj);

    /*!
     * If true, wen adding curves to this astral::Path,
     * curves are filtered as specified in santize().
     * Default value is true.
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
    santize_curves_on_adding(bool v);

    /*!
     * Returns true if and only if all contours of this
     * astral::Path return true for Contour::is_sanitized()
     */
    bool
    is_sanitized(void) const;

    /*!
     * Start a new contour.
     */
    Path&
    move(vec2 pt);

    /*!
     * Start a new contour with a curve
     */
    Path&
    start_contour(const ContourCurve &curve);

    /*!
     * Add a line to the current contour connecting the
     * previous end-point to the passed point. It is an
     * error to call line_to() without ever calling move()
     * first.
     * \param pt end-point of line to add
     * \param cont_tp edge continuation type
     */
    Path&
    line_to(const vec2 &pt,
            enum ContourCurve::continuation_t cont_tp = ContourCurve::not_continuation_curve)
    {
      prepare_to_add_curve();
      m_contours.back()->line_to(pt, cont_tp);
      return *this;
    }

    /*!
     * Add a quadratic bezier to the current contour
     * connecting the previous end-point to the passed
     * passed via a control point.  It is an error to
     * call quadratic_to() without ever calling move()
     * first.
     * \param ct control point of the quadratic to add
     * \param pt end-point of quadratic to add
     * \param cont_tp edge continuation type
     */
    Path&
    quadratic_to(const vec2 &ct, const vec2 &pt,
                 enum ContourCurve::continuation_t cont_tp = ContourCurve::not_continuation_curve)
    {
      prepare_to_add_curve();
      m_contours.back()->quadratic_to(ct, pt, cont_tp);
      return *this;
    }

    /*!
     * Add a conic curve to the current contour
     * connecting the previous end-point to the passed
     * passed via a control point.  It is an error to
     * call conic_to() without ever calling move()
     * first.
     * \param w conic weight; a weight of 1 is the same
     *          as a quadratic bezier curve where as a
     *          weight of 0 is the same as a line segment.
     * \param ct control point of the quadratic to add
     * \param pt end-point of quadratic to add
     * \param cont_tp edge continuation type
     */
    Path&
    conic_to(float w, const vec2 &ct, const vec2 &pt,
             enum ContourCurve::continuation_t cont_tp = ContourCurve::not_continuation_curve)
    {
      prepare_to_add_curve();
      m_contours.back()->conic_to(w, ct, pt, cont_tp);
      return *this;
    }

    /*!
     * Add an arc to the curent contour connecting the
     * previous end-point to the passed point.
     * \param radians angle of arc in radians, the sign of the
     *                angle gives the arc direction.
     * \param pt end point of the curve
     * \param cont_tp curve continuation type; used to determine
     *                how the join between the curve and its predecessor
     *                is drawn when stroked
     * \param out_data if non-null, place to which to write how
     *                 curves were added
     */
    Path&
    arc_to(float radians, vec2 pt,
           enum ContourCurve::continuation_t cont_tp = ContourCurve::not_continuation_curve,
           arc_curve_stats *out_data = nullptr)
    {
      prepare_to_add_curve();
      m_contours.back()->arc_to(radians, pt, cont_tp, out_data);
      return *this;
    }

    /*!
     * Add a cubic bezier to the current contour
     * connecting the previous end-point to the passed
     * passed via a control point.  It is an error to
     * call quadratic_to() without ever calling move()
     * first.
     * \param ct1 first control point of the cubic to add
     * \param ct2 second control point of the cubic to add
     * \param pt end-point of quadratic to add
     * \param cont_tp edge continuation type
     */
    Path&
    cubic_to(const vec2 &ct1, const vec2 &ct2, const vec2 &pt,
             enum ContourCurve::continuation_t cont_tp = ContourCurve::not_continuation_curve)
    {
      prepare_to_add_curve();
      m_contours.back()->cubic_to(ct1, ct2, pt, cont_tp);
      return *this;
    }

    /*!
     * Add a line, quadratic or cubic connecting the
     * last point added to the passed point.
     * \param ctl_pts array of control point, the size of
     *                the array must be no greater than 2
     * \param pt end point of the curve
     * \param cont_tp edge continuation type
     */
    Path&
    curve_to(c_array<const vec2> ctl_pts, vec2 pt,
             enum ContourCurve::continuation_t cont_tp = ContourCurve::not_continuation_curve)
    {
      prepare_to_add_curve();
      m_contours.back()->curve_to(ctl_pts, pt, cont_tp);
      return *this;
    }

    /*!
     * Generic curve_to() passing a \ref ContourCurve. It is an
     * error to pass a \ref ContourCurve whose value from
     * \ref ContourCurve::start_pt() is not the current
     * point of the contour.
     */
    Path&
    curve_to(const ContourCurve &curve,
             enum ContourCurve::continuation_t cont_tp)
    {
      prepare_to_add_curve();
      m_contours.back()->curve_to(curve, cont_tp);
      return *this;
    }

    /*!
     * Add a line segment from the last point to the
     * first point of the current contour. Afterwards,
     * any new geometry added will be onto a new contour,
     * if move() is not called first, then the starting
     * point of the new contour is the ending point of
     * the contour that is closed by this call.
     * \param cont_tp edge continuation type
     */
    Path&
    line_close(enum ContourCurve::continuation_t cont_tp = ContourCurve::not_continuation_curve)
    {
      prepare_to_add_curve();
      m_contours.back()->line_close(cont_tp);
      return *this;
    }

    /*!
     * Add a quadratic curve from the last point to the
     * first point of the current contour via the passed
     * control point. Afterwards, any new geometry added
     * will be onto a new contour, if move() is not called
     * first, then the starting point of the new contour
     * is the ending point of the contour that is closed
     * by this call.
     * \param ct control point of closing quadratic
     * \param cont_tp edge continuation type
     */
    Path&
    quadratic_close(const vec2 &ct,
                    enum ContourCurve::continuation_t cont_tp = ContourCurve::not_continuation_curve)
    {
      prepare_to_add_curve();
      m_contours.back()->quadratic_close(ct, cont_tp);
      return *this;
    }

    /*!
     * Add a conic curve from the last point to the
     * first point of the current contour via the passed
     * control point. Afterwards, any new geometry added
     * will be onto a new contour, if move() is not called
     * first, then the starting point of the new contour
     * is the ending point of the contour that is closed
     * by this call.
     * \param w conic weight; a weight of 1 is the same
     *          as a quadratic bezier curve where as a
     *          weight of 0 is the same as a line segment.
     * \param ct control point of closing quadratic
     * \param cont_tp edge continuation type
     */
    Path&
    conic_close(float w, const vec2 &ct,
                enum ContourCurve::continuation_t cont_tp = ContourCurve::not_continuation_curve)
    {
      prepare_to_add_curve();
      m_contours.back()->conic_close(w, ct, cont_tp);
      return *this;
    }

    /*!
     * Add an arc from the last point to the
     * first point of the current contour via the passed
     * control point. Afterwards, any new geometry added
     * will be onto a new contour, if move() is not called
     * first, then the starting point of the new contour
     * is the ending point of the contour that is closed
     * by this call.
     * \param radians angle of arc in radians, the sign of the
     *                angle gives the arc direction.
     * \param cont_tp curve continuation type; used to determine
     *                how the join between the curve and its predecessor
     *                is drawn when stroked
     * \param out_data if non-null, place to which to write how
     *                 curves were added
     */
    Path&
    arc_close(float radians,
              enum ContourCurve::continuation_t cont_tp = ContourCurve::not_continuation_curve,
              arc_curve_stats *out_data = nullptr)
    {
      prepare_to_add_curve();
      m_contours.back()->arc_close(radians, cont_tp, out_data);
      return *this;
    }

    /*!
     * Add a cubic curve from the last point to the
     * first point of the current contour via the passed
     * control point. Afterwards, any new geometry added
     * will be onto a new contour, if move() is not called
     * first, then the starting point of the new contour
     * is the ending point of the contour that is closed
     * by this call.
     * \param ct1 first control point of the cubic to add
     * \param ct2 second control point of the cubic to add
     * \param cont_tp edge continuation type
     */
    Path&
    cubic_close(const vec2 &ct1, const vec2 &ct2,
                enum ContourCurve::continuation_t cont_tp = ContourCurve::not_continuation_curve)
    {
      prepare_to_add_curve();
      m_contours.back()->cubic_close(ct1, ct2, cont_tp);
      return *this;
    }

    /*!
     * Add a line, quadratic or cubic curve from the last
     * point to the first point of the current contour.
     * Afterwards, any new geometry added will be onto a
     * new contour, if move() is not called first, then the
     * starting point of the new contour is the ending point
     * of the contour that is closed by this call.
     * \param ctl_pts array of control point, the size of
     *                the array must be no greater than 2
     * \param cont_tp edge continuation type
     */
    Path&
    curve_close(c_array<const vec2> ctl_pts,
                enum ContourCurve::continuation_t cont_tp = ContourCurve::not_continuation_curve)
    {
      prepare_to_add_curve();
      m_contours.back()->curve_close(ctl_pts, cont_tp);
      return *this;
    }

    /*!
     * Marks the current contour as closed, if the
     * starting point and the ending point of the
     * current contor are different, adds a line_to()
     * to the starting point of the current contour.
     * Afterwards, any new geometry added will be onto a
     * new contour, if move() is not called first, then the
     * starting point of the new contour is the ending point
     * of the contour that is closed by this call.
     */
    Path&
    close(void);

    /*!
     * Adds the passed contour to this path; if a contour
     * is started, then the added contour will come just
     * before the current contour.
     * \param C contour to add
     * \param p if non-null, place to write the ID to pass
     *          to contour() to fetch the contour added.
     */
    Path&
    add_contour(const ContourData &C, unsigned int *p = nullptr);

    /*!
     * Adds a rounded rect contour to this path; it is expected
     * that the rectangle added has already been sanitized (see
     * RoundedRect::sanitize_simple()and RoundedRect::santitize_scale()).
     * Rectangles that are not sanitized produce incorrect results.
     * \param rect rounded rect to add
     * \param d direction to run the rounded rect
     * \param p if non-null, place to write the ID to pass
     *          to contour() to fetch the contour added.
     */
    Path&
    add_rounded_rect(const RoundedRect &rect, enum contour_direction_t d,
                     unsigned int *p = nullptr)
    {
      RoundedRect::Point starting_point(RoundedRect::miny_side, d == contour_direction_counter_clockwise);
      return add_rounded_rect(rect, d, starting_point, p);
    }

    /*!
     * Adds a rounded rect contour to this path; it is expected
     * that the rectangle added has already been sanitized (see
     * RoundedRect::sanitize_simple()and RoundedRect::santitize_scale()).
     * Rectangles that are not sanitized produce incorrect results.
     * \param rect rounded rect to add
     * \param d direction to run the rounded rect
     * \param starting_point starting point of the contour
     * \param p if non-null, place to write the ID to pass
     *          to contour() to fetch the contour added.
     */
    Path&
    add_rounded_rect(const RoundedRect &rect, enum contour_direction_t d,
                     RoundedRect::Point starting_point, unsigned int *p = nullptr)
    {
      ContourData C;

      C.make_as_rounded_rect(rect, d, starting_point);
      return add_contour(C, p);
    }

    /*!
     * Adds a rect contour to this path; it is expected
     * that the rectangle added has already been standardized
     * (see Rect::standardize()). Rectangles that are not
     * standardized produce incorrect results.
     * \param rect rounded rect to add
     * \param d direction to run the rounded rect
     * \param starting_point starting point of the contour
     * \param p if non-null, place to write the ID to pass
     *          to contour() to fetch the contour added.
     */
    Path&
    add_rect(const Rect &rect, enum contour_direction_t d,
             enum Rect::corner_t starting_point, unsigned int *p = nullptr)
    {
      ContourData C;

      C.make_as_rect(rect, d, starting_point);
      return add_contour(C, p);
    }

    /*!
     * Adds a rect contour to this path; it is expected
     * that the rectangle added has already been standardized
     * (see Rect::standardize()). Rectangles that are not
     * standardized produce incorrect results.
     * \param rect rectangle to add
     * \param d direction to run the rounded rect
     * \param p if non-null, place to write the ID to pass
     *          to contour() to fetch the contour added.
     */
    Path&
    add_rect(const Rect &rect, enum contour_direction_t d,
             unsigned int *p = nullptr)
    {
      return add_rect(rect, d, Rect::minx_miny_corner, p);
    }

    /*!
     * Adds an oval contour to this path
     * \param rect rectangle specifying oval to add; the rectangle
     *             must be standardized, see Rect::standardize()
     * \param d direction to run the rounded rect
     * \param starting_point side of the starting point of the contour
     * \param p if non-null, place to write the ID to pass
     *          to contour() to fetch the contour added.
     */
    Path&
    add_oval(const Rect &rect, enum contour_direction_t d,
             enum Rect::side_t starting_point, unsigned int *p = nullptr)
    {
      ContourData C;

      C.make_as_oval(rect, d, starting_point);
      return add_contour(C, p);
    }

    /*!
     * Adds an oval contour to this path
     * \param rect rectangle specifying oval to add; the rectangle
     *             must be standardized, see Rect::standardize()
     * \param d direction to run the rounded rect
     * \param p if non-null, place to write the ID to pass
     *          to contour() to fetch the contour added.
     */
    Path&
    add_oval(const Rect &rect, enum contour_direction_t d,
             unsigned int *p = nullptr)
    {
      return add_oval(rect, d, Rect::maxx_side, p);
    }

    /*!
     * Issue santize on all contours of the path. Returns
     * true if any contour was modified.
     */
    bool
    sanitize(void);

    /*!
     * Returns the number of contours of this astral::Path.
     */
    unsigned int
    number_contours(void) const
    {
      return m_contours.size();
    }

    /*!
     * Returns the named contour of this astral::Path.
     * \param C which contour with 0 <= C < number_contours()
     */
    const Contour&
    contour(unsigned int C) const
    {
      ASTRALassert(C < number_contours());
      return *m_contours[C];
    }

    /*!
     * Returns the bounding box of the astral::Path,
     * computed from taking the union of the value of
     * ContourData::bounding_box() of each of the
     * contours of the path
     */
    const BoundingBox<float>&
    bounding_box(void) const;

    /*!
     * Returns same result as bounding_box(float),
     * presence is for allowing to have template functions
     * that work with Path and AnimatedPath objects
     */
    const BoundingBox<float>&
    bounding_box(float) const
    {
      return bounding_box();
    }

    /*!
     * Returns the bounding box of the astral::Path,
     * computed from taking the union of the value of
     * ContourData::control_point_bounding_box() of
     * each of the contours of the path
     */
    const BoundingBox<float>&
    control_point_bounding_box(void) const;

    /*!
     * Returns same result as control_point_bounding_box(float),
     * presence is for allowing to have template functions
     * that work with Path and AnimatedPath objects
     */
    const BoundingBox<float>&
    control_point_bounding_box(float) const
    {
      return control_point_bounding_box();
    }

    /*!
     * Returns the bounding box containing the end points of
     * all open contours of the path. This is needed for getting
     * the bonding box of the area covered when stroking with
     * cap stype astral::cap_square.
     */
    const BoundingBox<float>&
    open_contour_endpoint_bounding_box(void) const;

    /*!
     * Returns same result as open_contour_endpoint_bounding_box(float),
     * presence is for allowing to have template functions
     * that work with Path and AnimatedPath objects
     */
    const BoundingBox<float>&
    open_contour_endpoint_bounding_box(float) const
    {
      return open_contour_endpoint_bounding_box();
    }

    /*!
     * Returns the bounding box of all joins of the
     * path.
     */
    const BoundingBox<float>&
    join_bounding_box(void) const;

    /*!
     * Returns same result as join_bounding_box(float),
     * presence is for allowing to have template functions
     * that work with Path and AnimatedPath objects
     */
    const BoundingBox<float>&
    join_bounding_box(float) const
    {
      return join_bounding_box();
    }

    /*!
     * Return this astral::Path realized as an astral::ItemPath.
     */
    const ItemPath&
    item_path(float tol, float *actual_error = nullptr) const;

    /*!
     * Clear the path.
     */
    void
    clear(void);

    /*!
     * Template class helper; returns the value
     * StrokeShaderTypes::static_path_shader
     */
    static
    enum StrokeShader::path_shader_t
    stroke_shader_enum(void)
    {
      return StrokeShader::static_path_shader;
    }

    /*!
     * Computes the L1-distance to the path.
     * \param tol tolerance of query
     * \param pt the query point
     * \param distance_cull if non-negative will cull from the query any
     *                      contours for which pt is outside of their tight
     *                      bounding box and this distance from the box
     *                      boundary
     */
    PointQueryResult
    distance_to_path(float tol, const vec2 &pt, float distance_cull) const;

    /*!
     * Computes the L1-distance to the contour.
     * \param tol tolerance of query
     * \param pt the query point
     */
    PointQueryResult
    distance_to_path(float tol, const vec2 &pt) const
    {
      return distance_to_path(tol, pt, -1.0f);
    }

  private:
    class DataGenerator;

    void
    mark_dirty(void);

    void
    prepare_to_add_curve(void);

    DataGenerator&
    data_generator(void) const;

    void
    ready_bb(void) const;

    bool m_santize_curves_on_adding;
    std::vector<reference_counted_ptr<Contour>> m_contours;
    mutable reference_counted_ptr<DataGenerator> m_data_generator;
    mutable bool m_bb_ready;
    mutable BoundingBox<float> m_bb;
    mutable BoundingBox<float> m_join_bb;
    mutable BoundingBox<float> m_cap_bb;
    mutable BoundingBox<float> m_control_point_bb;
  };

/*! @} */

}

#endif

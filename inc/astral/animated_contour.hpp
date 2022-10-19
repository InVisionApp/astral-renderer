/*!
 * \file animated_contour.hpp
 * \brief file animated_contour.hpp
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

#ifndef ASTRAL_ANIMATED_CONTOUR_HPP
#define ASTRAL_ANIMATED_CONTOUR_HPP

#include <astral/contour.hpp>

namespace astral
{

/*!\addtogroup Paths
 * @{
 */

  /*!
   * An astral::AnimatedContour represents a single animated contour.
   */
  class AnimatedContour:
    public reference_counted<Contour>::non_concurrent
  {
  public:
    /*!
     * Represents an approximation to an astral::AnimatedContour.
     * The approximation is so that
     * - m_start.size() == m_end.size()
     * - for each i, m_start[i].type() == m_end[i].type()
     * .
     * which allows for animation by interpolating the
     * data vales of the curve.
     */
    class Approximation
    {
    public:
      /*!
       * The approximation of the start contour so
       * that the number of curves and types of each
       * curve match with those in \ref m_end.
       */
      c_array<const ContourCurve> m_start;

      /*!
       * The approximation of the end contour so
       * that the number of curves and types of each
       * curve match with those in \ref m_start.
       */
      c_array<const ContourCurve> m_end;
    };

    /*!
     * \brief
     * A \ref CompoundCurve is to be used to specify that
     * a set of curves is to be animated with another set
     * of curves.
     */
    class CompoundCurve
    {
    public:
      /*!
       * Curves of the compound curve
       */
      c_array<const ContourCurve> m_curves;

      /*!
       * The parameter space -length- of i'th curve
       * of \ref m_curves[i] is given by the i'th
       * element of \ref m_parameter_space_lengths.
       * These parameter lengths are what is used
       * to match against another \ref CompoundCurve
       * values.
       */
      c_array<const float> m_parameter_space_lengths;
    };

    ~AnimatedContour(void);

    /*!
     * Construct an animated contour. The two input contours do not need
     * to have the same number of curves. Internally, computes a version
     * of each contour with curvers paritioned to match the other generated
     * contour with a rough estimate of the lengths to induce which curves
     * match between the contours.
     * \param contours_are_closed if false, when the animated contour is stroked, caps
     *                            are added at the start and end of the animated
     *                            contour
     * \param st_contour the contour at the start of the animation
     * \param st_center the center of the starting contour, used only when animated
     *                  between closed contours to select the starting point of the
     *                  starting contour.
     * \param ed_contour the contour at the end of the animation
     * \param ed_center the center of the closing contour, used only when animated
     *                  between closed contours to select the starting point of the
     *                  starting contour.
     *
     */
    static
    reference_counted_ptr<AnimatedContour>
    create(bool contours_are_closed,
           c_array<const ContourCurve> st_contour, vec2 st_center,
           c_array<const ContourCurve> ed_contour, vec2 ed_center);

    /*!
     * Construct an animated contour, providing finer control over the matching
     * by the caller prodviding the lengths of each of the curves of the passed
     * contours.
     * \param contours_are_closed if false, when the animated contour is stroked, caps
     *                            are added at the start and end of the animated
     *                            contour
     * \param st_contour the contour at the start of the animation, an empty value
     *                   indicates to animate from the point at ed_center
     * \param st_center the center of the starting contour, used only when animated
     *                  between closed contours to select the starting point of the
     *                  starting contour.
     * \param st_lengths an array of length of the number of curves of st_contour;
     *                   these values normalized against the entire length of the contour
     *                   are used to match the curves of the start contour against the
     *                   curves of the end contour. Providing edited values of the
     *                   lengths allows the caller to control how the curves are
     *                   partitioned and matched.
     * \param ed_contour the contour at the end of the animation, an empty value
     *                   indicates to animate to the point at st_center
     * \param ed_center the center of the closing contour, used only when animated
     *                  between closed contours to select the starting point of the
     *                  starting contour.
     * \param ed_lengths an array of length of the number of curves of ed_contour;
     *                   these values normalized against the entire length of the contour
     *                   are used to match the curves of the start contour against the
     *                   curves of the end contour. Providing edited values of the
     *                   lengths allows the caller to control how the curves are
     *                   partitioned and matched.
     */
    static
    reference_counted_ptr<AnimatedContour>
    create(bool contours_are_closed,
           c_array<const ContourCurve> st_contour, vec2 st_center, c_array<const float> st_lengths,
           c_array<const ContourCurve> ed_contour, vec2 ed_center, c_array<const float> ed_lengths);

    /*!
     * An override that extracts the array of curves from the \ref ContourData objects
     * passed along with the closed-ness of the contours. It is legal to pass null
     * pointers for the \ref ContourData objects, these will be interpreted as empty
     * arrays. However, if both pointers are non-null, then either both \ref ContourData
     * have ContourData::closed() is true or both have it false.
     */
    static
    reference_counted_ptr<AnimatedContour>
    create(const ContourData *pst_contour, vec2 st_center, c_array<const float> st_lengths,
           const ContourData *ped_contour, vec2 ed_center, c_array<const float> ed_lengths);

    /*!
     * Construct an animated contour where \ref CompoundCurve values are paired.
     * The number of \ref CompoundCurve vales must match.
     * \param contours_are_closed if false, when the animated contour is stroked, caps
     *                            are added at the start and end of the animated
     *                            contour
     * \param st_contour the contour at the start of animation
     * \param ed_contour the contour at the end of animation. It is required
     *                   that st_contour and ed_contour have the same size.
     *                   The i'th entry of st_contour animates to the i'th
     *                   entry of ed_contour.
     */
    static
    reference_counted_ptr<AnimatedContour>
    create(bool contours_are_closed,
           c_array<const CompoundCurve> st_contour,
           c_array<const CompoundCurve> ed_contour);

    /*!
     * In contrast to create() routines, this is a simple matching
     * of curves where only the number of curves must match.
     * \param contours_are_closed if false, when the animated contour is stroked, caps
     *                            are added at the start and end of the animated
     *                            contour
     * \param st_contour the contour at the start of animation
     * \param ed_contour the contour at the end of animation. It is required
     *                   that st_contour and ed_contour have the same size, but
     *                   it is NOT necesary for the curve types to match.
     */
    static
    reference_counted_ptr<AnimatedContour>
    create_raw(bool contours_are_closed,
               c_array<const ContourCurve> st_contour,
               c_array<const ContourCurve> ed_contour);

    /*!
     * Create an animated contour that starts as a single point to expand
     * into a contour.
     * \param contours_are_closed if false, when the animated contour is stroked, caps
     *                            are added at the start and end of the animated
     *                            contour
     * \param st_contour single point at start of animation
     * \param ed_contour the contour at the end of animation
     */
    static
    reference_counted_ptr<AnimatedContour>
    create_raw(bool contours_are_closed,
               vec2 st_contour,
               c_array<const ContourCurve> ed_contour);

    /*!
     * Create an animated contour that starts as a contour and collapses
     * to a single point.
     * \param contours_are_closed if false, when the animated contour is stroked, caps
     *                            are added at the start and end of the animated
     *                            contour
     * \param st_contour the contour at the start of animation
     * \param ed_contour single point at the end of animation
     */
    static
    reference_counted_ptr<AnimatedContour>
    create_raw(bool contours_are_closed,
               c_array<const ContourCurve> st_contour,
               vec2 ed_contour);

    /*!
     * Create an animated contour that is a single point (that will be
     * given caps when stroked) moving.
     * \param st_contour single point at start of animation
     * \param ed_contour single point at the end of animation
     */
    static
    reference_counted_ptr<AnimatedContour>
    create_raw(vec2 st_contour, vec2 ed_contour);

    /*!
     * Returns the contour at the start of the animation
     */
    const ContourData&
    start_contour(void) const
    {
      return m_start;
    }

    /*!
     * Returns the contour at the end of the animation
     */
    const ContourData&
    end_contour(void) const
    {
      return m_end;
    }

    /*!
     * Returns true if the animated contour is closed.
     */
    bool
    closed(void) const
    {
      ASTRALassert(m_start.closed() == m_end.closed());
      return m_start.closed();
    }

    /*!
     * Returns a conservative value for the bounding box
     * of the edges of the animated contour at a time t.
     */
    BoundingBox<float>
    bounding_box(float t) const
    {
      BoundingBox<float> return_value;

      BoundingBox<float>::interpolate(m_start.bounding_box(),
                                      m_end.bounding_box(),
                                      t, &return_value);
      return return_value;
    }

    /*!
     * Returns a conservative value for the bounding box
     * of the joins of the animated contour at a time t.
     */
    BoundingBox<float>
    join_bounding_box(float t) const
    {
      BoundingBox<float> return_value;

      BoundingBox<float>::interpolate(m_start.join_bounding_box(),
                                      m_end.join_bounding_box(),
                                      t, &return_value);
      return return_value;
    }

    /*!
     * Returns the contour geometries approximated for filling
     * For animated contours, this means all curves are approximated
     * to quadratic curves even for filling.
     * \param error_tol the requested value for which the error is
     *                  to be no more than
     * \param ct specifies nature of tessellation
     * \param actual_error if non-null, location to which to write
     *                     the actual error in the returned
     *                     approximation
     */
    Approximation
    fill_approximated_geometry(float error_tol,
                               enum contour_fill_approximation_t ct,
                               float *actual_error = nullptr) const;

    /*!
     * Returns the contour geometries approximated for stroking.
     * The error is error between the approximation PLUS the
     * error between the quadratic curves and approximating them
     * by arcs of a circle.
     * \param error_tol the requested value for which the error is
     *                  to be no more than
     * \param actual_error if non-null, location to which to write
     *                     the actual error in the returned
     *                     approximation
     */
    Approximation
    stroke_approximated_geometry(float error_tol,
                                 float *actual_error = nullptr) const;

    /*!
     * Returns the data for drawing the path filled
     * for the named rendering pass.
     * \param tol error tolerance allowed between the actual
     *            path and the rendered path in path coordinates
     * \param engine the \ref RenderEngine that does the
     *               rendering
     * \param ct specifies nature of tessellation
     * \param out_error if non-null, location to which to write
     *                  the actual error in the returned
     *                  approximation
     */
    const FillSTCShader::CookedData&
    fill_render_data(float tol, RenderEngine &engine,
                     enum contour_fill_approximation_t ct,
                     float *out_error = nullptr) const;

    /*!
     * Returns the data for drawing the path stroked
     * \param tol error tolerance allowed between the actual
     *            path and the rendered path in path coordinates
     * \param engine the \ref RenderEngine that does the
     *               rendering
     * \param out_error if non-null, location to which to write
     *                  the actual error in the returned
     *                  approximation
     */
    const StrokeShader::CookedData&
    stroke_render_data(float tol, RenderEngine &engine,
                       float *out_error = nullptr) const;

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

  private:
    class DataGenerator;

    AnimatedContour(void);

    DataGenerator&
    data_generator(void) const;

    ContourData m_start, m_end;
    mutable reference_counted_ptr<DataGenerator> m_data_generator;
  };

/*! @} */
}

#endif

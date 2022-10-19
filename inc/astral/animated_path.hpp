/*!
 * \file animated_path.hpp
 * \brief file animated_path.hpp
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

#ifndef ASTRAL_ANIMATED_PATH_HPP
#define ASTRAL_ANIMATED_PATH_HPP

#include <vector>
#include <cstring>
#include <astral/util/c_array.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/util.hpp>
#include <astral/util/bounding_box.hpp>
#include <astral/path.hpp>
#include <astral/animated_contour.hpp>
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
   * \brief
   * An AnimatedPath represents animating between a pair of
   * \ref Path objects.
   */
  class AnimatedPath
  {
  public:
    /*!
     * Conveniance typedef
     */
    typedef AnimatedContour::CompoundCurve CompoundCurve;

    /*!
     * \brief
     * Information packet to describe a contour of a
     * \ref Path used to decide how to sort contours
     * of a path for the purpose of contour pairing
     * with another path.
     */
    class ContourInfo
    {
    public:
      /*!
       * The contour id from the \ref Path
       */
      unsigned int m_contour;

      /*!
       * An approximation of the lengths of each of the
       * edges of the contour.
       */
      std::vector<float> m_lengths;

      /*!
       * An approximation of the total length of the contour.
       */
      float m_total_length;
    };

    /*!
     * \brief
     * Class that conains a the geometry of a contour
     * and values of a \ref ContourInfo.
     */
    class ContourWithInfo
    {
    public:
      ContourWithInfo(void)
      {}

      /*!
       * The curves of a contour
       */
      const ContourData *m_contour;

      /*!
       * The information about \ref m_contour
       */
      ContourInfo m_info;
    };

    /*!
     * \brief
     * Base class to specify how to auto-match contours. Contour
     * auto matching is performed by first sorting the two lists of
     * of contours and then matching contours in the sorted list
     * order.
     */
    class ContourSorterBase
    {
    public:
      virtual
      ~ContourSorterBase()
      {}

      /*!
       * To be implemented by a derived class to compare two contours
       * \param lhs contpur info of the left hand side comparison operand.
       * \param rhs contpur info of the left hand side comparison operand.
       */
      virtual
      bool
      compare(const ContourWithInfo &lhs, const ContourWithInfo &rhs) const = 0;
    };

    /*!
     * \brief
     * Sorts the contours in the order they appear in the
     * original \ref Path.
     */
    class SimpleContourSorter:public ContourSorterBase
    {
    public:
      virtual
      bool
      compare(const ContourWithInfo &lhs, const ContourWithInfo &rhs) const override
      {
        return lhs.m_info.m_contour < rhs.m_info.m_contour;
      }
    };

    /*!
     * \brief
     * Sorts the contours by their length, with largest
     * elements first.
     */
    class LengthContourSorter:public ContourSorterBase
    {
    public:
      virtual
      bool
      compare(const ContourWithInfo &lhs, const ContourWithInfo &rhs) const override
      {
        return lhs.m_info.m_total_length > rhs.m_info.m_total_length;
      }
    };

    /*!
     * \brief
     * Sorts the contours by their area, with largest
     * elements first
     */
    class AreaContourSorter:public ContourSorterBase
    {
    public:
      virtual
      bool
      compare(const ContourWithInfo &lhs, const ContourWithInfo &rhs) const override
      {
        vec2 lhs_sz(lhs.m_contour->bounding_box().as_rect().size());
        vec2 rhs_sz(rhs.m_contour->bounding_box().as_rect().size());

        return lhs_sz.x() * lhs_sz.y() > rhs_sz.x() * rhs_sz.y();
      }
    };

    /*!
     * Ctor.
     * \param start_path the \ref Path at time t = 0
     * \param end_path the \ref Path at time t = 1
     * \param sorter how to sort contours for pairing
     */
    AnimatedPath(const Path &start_path,
                 const Path &end_path,
                 const ContourSorterBase &sorter);

    /*!
     * Ctor. Initializes as interpolating between empty paths.
     */
    AnimatedPath(void);

    ~AnimatedPath();

    /*!
     * Clear the contents of the \ref AnimatedPath
     */
    AnimatedPath&
    clear(void);

    /*!
     * Returns a conservative value for the bounding box
     * of the edges of the animated path at a time t.
     */
    BoundingBox<float>
    bounding_box(float t) const;

    /*!
     * Returns a conservative value for the bounding box
     * of the joins of the animated path at a time t.
     */
    BoundingBox<float>
    join_bounding_box(float t) const;

    /*!
     * Returns the bounding box containing the end points of
     * all open contours of the path. This is needed for getting
     * the bonding box of the area covered when stroking with
     * cap stype astral::cap_square.
     */
    BoundingBox<float>
    open_contour_endpoint_bounding_box(float t) const;

    /*!
     * Returns the number of contours
     */
    unsigned int
    number_contours(void) const
    {
      return m_path.size();
    }

    /*!
     * Returns the named contour of this astral::AnimatedPath.
     * \param C which contour with 0 <= C < number_contours()
     */
    const AnimatedContour&
    contour(unsigned int C) const
    {
      ASTRALassert(C < m_path.size());
      return *m_path[C];
    }

    /*!
     * Add an animated contour to this.
     */
    AnimatedPath&
    add_animated_contour(reference_counted_ptr<AnimatedContour> contour);

    /*!
     * Adds an animated contour to the path. The two input contours do not
     * need to have the same number of curves. Internally, computes a version
     * of each contour with curvers paritioned to mach the other generated
     * contour with a rough estimate of the lengths to induce which curves
     * match between the contours.
     * \param contours_are_closed if false, when the animated contour is stroked, caps
     *                                      are added at the start and end of the animated
     *                                      contour
     * \param st_contour the contour at the start of the animation
     * \param st_center the center of the starting contour, used only when animated
     *                  between closed contours to select the starting point of the
     *                  starting contour.
     * \param ed_contour the contour at the end of the animation
     * \param ed_center the center of the closing contour, used only when animated
     *                  between closed contours to select the starting point of the
     *                  starting contour.
     */
    AnimatedPath&
    add_animated_contour(bool contours_are_closed,
                         c_array<const ContourCurve> st_contour, vec2 st_center,
                         c_array<const ContourCurve> ed_contour, vec2 ed_center);

    /*!
     * Adds an animated contour to the path, providing finer control over the matching
     * by the caller prodviding the lengths of each of the curves of the passed
     * contours.
     * \param contours_are_closed if false, when the animated contour is stroked, caps
     *                                      are added at the start and end of the animated
     *                                      contour
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
    AnimatedPath&
    add_animated_contour(bool contours_are_closed,
                         c_array<const ContourCurve> st_contour, vec2 st_center, c_array<const float> st_lengths,
                         c_array<const ContourCurve> ed_contour, vec2 ed_center, c_array<const float> ed_lengths);

    /*!
     * An override that extracts the array of curves from the \ref ContourData objects
     * passed along with the closed-ness of the contours. It is legal to pass null
     * pointers for the \ref ContourData objects, these will be interpreted as empty
     * arrays. However, if both pointers are non-null, then either both \ref ContourData
     * have ContourData::closed() is true or both have it false.
     */
    AnimatedPath&
    add_animated_contour(const ContourData *pst_contour, vec2 st_center, c_array<const float> st_lengths,
                         const ContourData *ped_contour, vec2 ed_center, c_array<const float> ed_lengths);

    /*!
     * Add an animated contour where \ref CompoundCurve values are paired.
     */
    AnimatedPath&
    add_animated_contour(bool contours_are_closed,
                         c_array<const CompoundCurve> st_contour,
                         c_array<const CompoundCurve> ed_contour);

    /*!
     * In contrast to \ref add_animated_contour(), this routine does simple matching
     * where the i'th curve is matched across the contour pair given. It is an
     * error if the passed arrays are of different sizes.
     */
    AnimatedPath&
    add_animated_contour_raw(bool contours_are_closed,
                             c_array<const ContourCurve> st_contour,
                             c_array<const ContourCurve> ed_contour);

    /*!
     * Add an animated contour that starts as a single point to expand
     * into a contour
     */
    AnimatedPath&
    add_animated_contour_raw(bool contours_are_closed,
                             vec2 st_contour,
                             c_array<const ContourCurve> ed_contour);

    /*!
     * Add an animated contour that starts as a contour and collapses
     * to a point
     */
    AnimatedPath&
    add_animated_contour_raw(bool contours_are_closed,
                             c_array<const ContourCurve> st_contour,
                             vec2 ed_contour);

    /*!
     * Add an animated point contour
     */
    AnimatedPath&
    add_animated_contour_raw(vec2 st_contour, vec2 ed_contour);

    /*!
     * Add the contours of two paths (of not necessarily the same verbs)
     * to this \ref AnimatedPath. Internally, uses \ref add_animated_contour().
     * \param start_path the \ref Path at time t = 0
     * \param end_path the \ref Path at time t = 1
     * \param sorter how to sort contours for pairing
     */
    AnimatedPath&
    add_animated_contours(const Path &start_path,
                          const Path &end_path,
                          const ContourSorterBase &sorter);

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * clear();
     * add_animated_contours(start_path, end_path, sorter);
     * \endcode
     * \param start_path the \ref Path at time t = 0
     * \param end_path the \ref Path at time t = 1
     * \param sorter how to sort contours for pairing
     */
    AnimatedPath&
    set(const Path &start_path,
        const Path &end_path,
        const ContourSorterBase &sorter)
    {
      clear();
      return add_animated_contours(start_path, end_path, sorter);
    }

    /*!
     * Template class helper.
     */
    static
    enum StrokeShader::path_shader_t
    stroke_shader_enum(void)
    {
      return StrokeShader::animated_path_shader;
    }

  private:

    static
    BoundingBox<float>
    compute_bb(float t,
               const BoundingBox<float> &b0,
               const BoundingBox<float> &b1);

    std::vector<reference_counted_ptr<AnimatedContour>> m_path;
    BoundingBox<float> m_start_bb, m_end_bb;
    BoundingBox<float> m_start_cap_bb, m_end_cap_bb;
    BoundingBox<float> m_start_join_bb, m_end_join_bb;
  };

/*! @} */

}

#endif

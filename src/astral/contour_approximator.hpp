/*!
 * \file contour_approximator.hpp
 * \brief file contour_approximator.hpp
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

#ifndef ASTRAL_CONTOUR_APPROXIMATOR_HPP
#define ASTRAL_CONTOUR_APPROXIMATOR_HPP

#include <vector>
#include <numeric>
#include <astral/util/vecN.hpp>
#include <astral/util/c_array.hpp>
#include <astral/contour.hpp>
#include "contour_curve_util.hpp"

namespace astral
{
  namespace detail
  {
    /*!
     * Class used to approximate contours where cubic bezier
     * curves are approximated by a pair of quadratic bezier
     * curves. The approximation produces a curve where the
     * tangent vector -direction- along the curve is continuous.
     */
    class ContourApproximator:
      public reference_counted<ContourApproximator>::non_concurrent
    {
    public:
      /*!
       * Specifies the source of a curve
       */
      class SourceTag
      {
      public:
        float
        remap_to_source(float t) const
        {
          return m_source_range.m_begin + t * (m_source_range.m_end - m_source_range.m_begin);
        }

        /*!
         * The index into the array of the starting curves
         */
        int m_source_curve;

        /*!
         * The range into the domain of the original curve
         */
        range_type<float> m_source_range;
      };

      enum mode_t:uint32_t
        {
          /*!
           * All curves are converted to quadratic curves
           */
          approximate_to_quadratic = 0,

          /*!
           * All curves are converted to quadratic curves and the
           * error returned is the error between the quadratic
           * curves and the biarcs that approximate the quadratic
           * curves as computed by \ref WaltonMeekBiArc
           */
          approximate_to_quadratic_error_to_biarc,

          /*!
           * All curves are converted to quadratic or conic curves
           */
          approximate_to_conic_or_quadratic,
        };

      enum split_cubics_at_cusp_mode_t
        {
          /*!
           * Split cubic curves at their cusps
           */
          split_cubics_at_cusps,

          /*!
           * Skip splitting cubics at their cusps.
           */
          dont_split_cubics_at_cusps
        };

      /*!
       * Enumeration to control if the size of a curve influences
       * the error and therefore the tessellation.
       */
      enum max_size_mode_t:uint32_t
        {
          /*!
           * The size of the curve does not influence the
           * error. For path filling this is correct value
           * to use.
           */
          ignore_size_of_curve = 0u,

          /*!
           * The size of a curves and line-segments contributes
           * to the error. By doing so, the axis-aligned bounding
           * boxes can be finer. This is important (usually) for
           * path stroking to reduce overdraw.
           *
           * NOTE: the code assumes that this comes strictly
           *       after ignore_size_of_curve
           */
          size_of_lines_and_curves_contributes_to_error,

          /*!
           * The size of curves, but not line segments, contributes
           * to the error. The use case is for STC-filling to reduce
           * the area of the stencil-prepare pass for the conic
           * triangles.
           *
           * NOTE: the code assumes that this comes strictly
           *       after size_of_lines_and_curves_contributes_to_error
           */
          size_of_only_curves_contributes_to_error,
        };

      enum
        {
          pixel_box_size = 128
        };

      typedef std::vector<ContourCurve> ApproximatedContour;

      static
      bool
      size_of_curve_contributes_to_error(enum max_size_mode_t v)
      {
        return v >= size_of_lines_and_curves_contributes_to_error;
      }

      static
      bool
      size_of_lines_contributes_to_error(enum max_size_mode_t v)
      {
        return v == size_of_lines_and_curves_contributes_to_error;
      }

      /*!
       * Ctor. The passed contours must have the exact same number
       * of curves The passed contours are tessellated -together-,
       * i.e. if the I'th curve of any contour is split, then so are
       * all of the I'th curves of all contours.
       * \param Cs array of contours objects to tessellate together
       * \param dst_geometry the tessellation of Cs[I] will be written
       *                     to dst_geometry[I], the size of dst_geometry
       *                     must be the same as the size of Cs; in adddition,
       *                     it is guaranteed that the number of curves written
       *                     to dst_geometry[I] for Cs[I][curve] is the same.
       *                     The purpose is that by matching the number of curves,
       *                     and their curev types, one can animate between
       *                     dst_geometry[I] and dst_geometry[J] for any valid
       *                     I and J
       * \param dst_tags if non-null, location to which to write the source tag information
       * \param mode specifies to what to approximate the input contours
       * \param sz_mode specifies if the size of curves contributes to error
       *                to force tessellation.
       * \param split_singleton_contours if true, always split a contour if
       *                                 it has only a single curve
       * \param split_cubic_cusps_mode speficies if to split cubics at their cusps.
       * \param dst_tags if non-null location to which to write the tag array
       */
      template<typename T>
      ContourApproximator(c_array<const T> Cs,
                          c_array<ApproximatedContour> dst_geometry,
                          enum mode_t mode, enum max_size_mode_t sz_mode,
                          bool split_singleton_contours,
                          enum split_cubics_at_cusp_mode_t split_cubic_cusps_mode,
                          std::vector<SourceTag> *dst_tags = nullptr):
        ContourApproximator(Cs, -1.0f, dst_geometry, mode, sz_mode,
                            split_singleton_contours,
                            split_cubic_cusps_mode,
                            c_array<const SourceTag>(), dst_tags)
      {}

      /*!
       * Provided as a conveniance.
       */
      template<typename T, size_t N>
      ContourApproximator(const vecN<T, N> &Cs, vecN<ApproximatedContour, N> &dst_geometry,
                          enum mode_t mode, enum max_size_mode_t sz_mode,
                          bool split_singleton_contours,
                          enum split_cubics_at_cusp_mode_t split_cubic_cusps_mode,
                          std::vector<SourceTag> *dst_tags = nullptr):
        ContourApproximator(c_array<const T>(Cs),
                            c_array<ApproximatedContour>(dst_geometry),
                            mode, sz_mode, split_singleton_contours,
                            split_cubic_cusps_mode, dst_tags)
      {}

      /*!
       * Provided as a conveniance.
       */
      template<typename T>
      ContourApproximator(const T &C, ApproximatedContour *dst_geometry,
                          enum mode_t mode, enum max_size_mode_t sz_mode,
                          bool split_singleton_contours,
                          enum split_cubics_at_cusp_mode_t split_cubic_cusps_mode,
                          std::vector<SourceTag> *dst_tags = nullptr):
        ContourApproximator(c_array<const T>(&C, 1),
                            c_array<ApproximatedContour>(dst_geometry, 1),
                            mode, sz_mode, split_singleton_contours,
                            split_cubic_cusps_mode, dst_tags)
      {}

      /*!
       * Refine the approximation by creating and returning a
       * new ContourApproximator object that allows those cubics
       * that were split in this to be split again in the returned
       * object AND the target tolerance is set to half of error().
       * \param tags for each curve, the tag of its source
       * \param dst_geometry the tessellation of Cs[I] will be written
       *                     to dst_geometry[I], the size of dst_geometry
       *                     must be the same as the size of Cs; in adddition,
       *                     it is guaranteed that the "verbs" of the
       *                     approximations is the same, i.e. each approximation
       *                     will have the same number of curves and the
       *                     degree of the I'th curve for all approximations
       *                     is the same.
       * \param dst_tags location to which to write the tagging information
       */
      reference_counted_ptr<const ContourApproximator>
      create_refinement(c_array<const SourceTag> tags,
                        c_array<ApproximatedContour> dst_geometry,
                        std::vector<SourceTag> *dst_tags) const
      {
        return create_refinement_implement(tags, dst_geometry, dst_tags);
      }

      /*!
       * Creates refinement without tagging
       */
      reference_counted_ptr<const ContourApproximator>
      create_refinement(c_array<ApproximatedContour> dst_geometry) const
      {
        return create_refinement_implement(c_array<const SourceTag>(), dst_geometry, nullptr);
      }

      /*!
       * Overload to reduce typing
       */
      reference_counted_ptr<const ContourApproximator>
      create_refinement(c_array<const SourceTag> tags,
                        ApproximatedContour *dst_geometry,
                        std::vector<SourceTag> *dst_tags) const
      {
        return create_refinement(tags, c_array<ApproximatedContour>(dst_geometry, 1), dst_tags);
      }

      /*!
       * Overload to reduce typing
       */
      reference_counted_ptr<const ContourApproximator>
      create_refinement(ApproximatedContour *dst_geometry) const
      {
        return create_refinement(c_array<ApproximatedContour>(dst_geometry, 1));
      }

      /*!
       * Return the error between the source contour and the
       * approximation by quadratic curves.
       */
      float
      error(void) const
      {
        return m_error;
      }

      /*!
       * Return the mode_t passed to ctor
       */
      enum mode_t
      mode(void) const
      {
        return m_mode;
      }

      /*!
       * Return the sz_mode_t passed to ctor
       */
      enum max_size_mode_t
      sz_mode(void) const
      {
        return m_sz_mode;
      }

    private:
      enum
        {
          /*!
           * Once ContourCurve.generation() is at this value, the
           * "error" from size which forces tesselaltion is ignored.
           * The reason is that excessive tessellation induces
           * numerical error which in turn generates significant
           * round off error.
           */
          generation_ignore_size = 6,
        };

      typedef std::vector<ContourCurve> TessedContour;

      class CurveFetcherBase
      {
      public:
        explicit
        CurveFetcherBase(unsigned int sz):
          m_number_contours(sz)
        {}

        virtual
        c_array<const ContourCurve>
        get_curves(unsigned int contour) const = 0;

        unsigned int
        number_contours(void) const
        {
          return m_number_contours;
        }

        bool
        empty(void) const
        {
          return m_number_contours == 0;
        }

        bool
        curve_has_glue_cusp(unsigned int contour, unsigned int curve) const
        {
          c_array<const ContourCurve> curves(get_curves(contour));

          if (curves[curve].continuation() == ContourCurve::continuation_curve_cusp)
            {
              return true;
            }

          if (curve + 1 < curves.size() && curves[curve + 1].continuation() == ContourCurve::continuation_curve_cusp)
            {
              return true;
            }

          return false;
        }

      private:
        unsigned int m_number_contours;
      };

      static
      c_array<const ContourCurve>
      get_curves_implement(const ContourData *p)
      {
        return p->curves();
      }

      static
      c_array<const ContourCurve>
      get_curves_implement(const TessedContour &p)
      {
        return make_c_array(p);
      }

      static
      c_array<const ContourCurve>
      get_curves_implement(c_array<const ContourCurve> p)
      {
        return p;
      }

      template<typename T>
      class CurveFetcher:public CurveFetcherBase
      {
      public:
        explicit
        CurveFetcher(c_array<const T> p):
          CurveFetcherBase(p.size()),
          m_p(p)
        {}

        CurveFetcher(void):
          CurveFetcherBase(0)
        {}

        virtual
        c_array<const ContourCurve>
        get_curves(unsigned int contour) const override
        {
          return get_curves_implement(m_p[contour]);
        }

      private:
        c_array<const T> m_p;
      };

      template<typename T>
      ContourApproximator(c_array<const T> Cs, float target_tol,
                          c_array<ApproximatedContour> dst_geometry,
                          enum mode_t mode, enum max_size_mode_t sz_mode,
                          bool split_singleton_contours,
                          enum split_cubics_at_cusp_mode_t split_cubic_cusps_mode,
                          c_array<const SourceTag> tags,
                          std::vector<SourceTag> *dst_tags):
        ContourApproximator(CurveFetcher<T>(Cs), target_tol,
                            dst_geometry, mode, sz_mode,
                            split_singleton_contours,
                            split_cubic_cusps_mode,
                            tags, dst_tags)
      {}

      ContourApproximator(const CurveFetcherBase &Cs, float target_tol,
                          c_array<ApproximatedContour> dst_geometry,
                          enum mode_t mode, enum max_size_mode_t sz_mode,
                          bool split_singleton_contours,
                          enum split_cubics_at_cusp_mode_t split_cubic_cusps_mode,
                          c_array<const SourceTag> tags,
                          std::vector<SourceTag> *dst_tags);

      reference_counted_ptr<const ContourApproximator>
      create_refinement_implement(c_array<const SourceTag> tags,
                                  c_array<ApproximatedContour> dst_geometry,
                                  std::vector<SourceTag> *dst_tags) const;

      void
      add_cubic(bool approximate_as_line_segment,
                ApproximatedContour *quadratic_contour,
                TessedContour *contour_tessed,
                bool has_quadratic,
                const ContourCurve &C);

      void
      add_quadratic(ApproximatedContour *quadratic_contour,
                    TessedContour *contour_tessed,
                    const ContourCurve &C);

      void
      add_conic(ApproximatedContour *quadratic_contour,
                TessedContour *contour_tessed,
                const ContourCurve &C);

      void
      add_line(ApproximatedContour *quadratic_contour,
               TessedContour *contour_tessed,
               bool force_quadratic, const ContourCurve &C);

      static
      float
      compute_error_from_size(float sz)
      {
        /* pixel_box_size represents the size to max-out
         * the size of a box in pixels. When the error goal
         * is E, then essentially the zoom in factor is
         * 1 / E, thus to control the box size to be no
         * more than a value Z, we just say the error from
         * a box of size Z is the size of the box divided
         * by pixel_box_size
         */
        return sz / static_cast<float>(pixel_box_size);
      }

      static
      float
      compute_error_from_size(const BoundingBox<float> &bb)
      {
        vec2 sz(bb.size());
        return compute_error_from_size(t_max(sz.x(), sz.y()));
      }

      static
      float
      compute_error_from_size(const QuadraticBezierCurve &Q);

      static
      void
      post_process_approximation(c_array<ApproximatedContour> contours);

      enum mode_t m_mode;
      enum max_size_mode_t m_sz_mode;
      bool m_split_singleton_contours;
      float m_error;

      /* we save the tessellation of the cubics to smaller cubics
       * so that we can just -resume- tessellation instead of
       * restarting it.
       */
      std::vector<TessedContour> m_tessed_contours;
    };

  } // of namespace detail
} // of namespace astral

#endif

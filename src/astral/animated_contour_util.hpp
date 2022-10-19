/*!
 * \file animated_contour_util.hpp
 * \brief file animated_contour_util.hpp
 *
 * Copyright 2020 by InvisionApp.
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

#ifndef ASTRAL_ANIMATION_UTIL_HPP
#define ASTRAL_ANIMATION_UTIL_HPP

#include <astral/contour.hpp>

namespace astral
{
  namespace detail
  {
    /*!
     * Give an approximation to the length of a curve,
     * the approximation is not very accurate but good
     * enough for curve matching
     */
    float
    approximate_length(const ContourCurve &C);

    /*!
     * For each curve in an array of curves, compute
     * their approximage length saving the result. Also
     * return sthe sum of the lengths.
     */
    float
    approximate_lengths(c_array<const ContourCurve> contour,
                        c_array<float> out_lengths);

    /*!
     * SimplifiedContour simplifies an input contour to remove
     * degenerate geometry
     */
    class SimplifiedContour
    {
    public:
      /*!
       * Essentially a ContourCurve with additional length
       * information.
       */
      class Edge:public ContourCurve
      {
      public:
        Edge(const ContourCurve &curve,
             float length, float length_from_start):
          ContourCurve(curve),
          m_length(length),
          m_length_from_contour_start_to_edge_start(length_from_start)
        {
        }

        float
        length(void) const
        {
          return m_length;
        }

        float
        length_from_contour_start_to_edge_start(void) const
        {
          return m_length_from_contour_start_to_edge_start;
        }

        void
        length_from_contour_start_to_edge_start(float v)
        {
          m_length_from_contour_start_to_edge_start = v;
        }

        float
        length_from_contour_start_to_edge_end(void) const
        {
          return m_length + m_length_from_contour_start_to_edge_start;
        }

      private:
        float m_length;
        float m_length_from_contour_start_to_edge_start;
      };

      SimplifiedContour(c_array<const ContourCurve> contour,
                        c_array<const float> L);

      vec2
      edge_start_pt(unsigned int E) const
      {
        return (E == 0) ?
          m_start_pt:
          m_edges[E - 1].end_pt();
      }

      const Edge&
      operator[](unsigned int E) const
      {
        ASTRALassert(E < m_edges.size());
        return m_edges[E];
      }

      c_array<const Edge>
      edges(void) const
      {
        return make_c_array(m_edges);
      }

      unsigned int
      size(void) const
      {
        return m_edges.size();
      }

      bool
      empty(void) const
      {
        return m_edges.empty();
      }

      vec2
      start_pt(void) const
      {
        return m_start_pt;
      }

      void
      set(std::vector<Edge> &edges, vec2 st)
      {
        m_start_pt = st;
        m_edges.swap(edges);
      }

      float
      contour_length(void) const
      {
        return m_contour_length;
      }

    private:
      vec2 m_start_pt;
      std::vector<Edge> m_edges;
      float m_contour_length;
    };

    /* Given two contours, creates a partition in time that includes all the
     * points of the souce contours. In addition, perform intelligent merging
     * of st-ed points so that if two contours have a similair length pattern,
     * then we don't end up generating lots of extra points. NOTE: the point
     * at time = 0 is NOT included in the array paritioned_points().
     */
    class ContourCommonPartitioner
    {
    public:
      enum point_src_t { from_st = 0, from_ed = 1};

      /*!
       * A PartitionPoint represents a point in the start
       * contour and a point in the end contour that are
       * to be matched in animation.
       */
      class PartitionPoint
      {
      public:
        PartitionPoint(void):
          m_idx(-1, -1),
          m_rel_length(-1.0f, -1.0f)
        {}

        float
        rel_length(void) const
        {
          ASTRALassert(m_idx[0] != -1 || m_idx[1] != -1);
          return (m_idx[0] != -1) ?
            m_rel_length[0]:
            m_rel_length[1];
        }

        /* m_idx[] indicates what edge ENDS at
         * the partition point and m_rel_lenght[]
         * indicates the length from the start of
         * the contour to the END of the edge named
         * by m_idx[]
         */
        vecN<int, 2> m_idx;
        vecN<float, 2> m_rel_length;
      };

      ContourCommonPartitioner(const SimplifiedContour &st,
                               const SimplifiedContour &ed);

      c_array<const PartitionPoint>
      parition_points(void) const
      {
        return make_c_array(m_parition_points);
      }

    private:
      class InputPointInfo
      {
      public:
        enum point_src_t m_src;
        int m_idx;
        float m_rel_length;

        bool
        operator<(const InputPointInfo &rhs) const
        {
          return m_rel_length < rhs.m_rel_length;
        }

        static
        void
        add_pts(std::vector<InputPointInfo> &dst, enum point_src_t tp,
                const SimplifiedContour &input);
      };

      std::vector<PartitionPoint> m_parition_points;
    };

    class ContourBuilder
    {
    public:
      ContourBuilder(c_array<const ContourCommonPartitioner::PartitionPoint> partition,
                     const SimplifiedContour &input,
                     enum ContourCommonPartitioner::point_src_t tp);

      const ContourData&
      contour(void) const
      {
        return m_contour;
      }

    private:
      ContourData m_contour;
    };
  }
}


#endif

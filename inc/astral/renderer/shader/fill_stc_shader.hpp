/*!
 * \file fill_stc_shader.hpp
 * \brief file fill_stc_shader.hpp
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

#ifndef ASTRAL_FILL_STC_SHADER_HPP
#define ASTRAL_FILL_STC_SHADER_HPP

#include <utility>
#include <astral/contour_curve.hpp>
#include <astral/renderer/render_data.hpp>
#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/shader/item_shader.hpp>
#include <astral/renderer/shader/item_data.hpp>
#include <astral/renderer/vertex_streamer.hpp>
#include <astral/renderer/static_data_streamer.hpp>

namespace astral
{
  class RenderEngine;

  /*!
   * \brief
   * Class to encase shaders for performing animated path
   * rendering via stencil-then-cover. Fill shaders render
   * to a surface that will have post-processing performed
   * on it. The meaning of each channel when rendering to
   * it are:
   * - .r : should be 0.0 or 1.0 with 1.0 meaning pixel is covered
   *        or partially coverted and 0.0 meaning it is not.
   *        This channel is only written to in the cover-pass
   *        of stencil-then-cover.
   * - .g : stores 1.0 - D where D is the distance to the boundary
   *        between covered and not-covered in pixel units (clamped
   *        to [0, 1]). This distance value represents also taking
   *        into acount the distance to false edges.
   * - .b : this value represents a coverage value taking into
   *        account false edges
   * - .a : unused
   * .
   * When rendering the blend mode is to max on each channel.
   * The post process pass uses these values together to generate
   * the channel values described for reading so that false edges
   * (for example cancelling edges from paths) do not generate
   * partial coverage.
   */
  class FillSTCShader
  {
  public:
    /*!
     * \brief
     * Enumeration describing how item data is packed for
     * an astral::FillSTCShader
     */
    enum item_data_packing:uint32_t
      {
        /*!
         * The item data is packed as:
         *  - Size = 1
         *  - ItemData::m_data[0].x().f = time interpolate
         *  - ItemData::m_data[0].y().f = recirpocal of scale_factor.x()
         *  - ItemData::m_data[0].z().f = recirpocal of scale_factor.y()
         */
        item_data_size = 1,
      };

    /*!
     * \brief
     * Enumeration for the shaders of an astral::FillSTCShader
     */
    enum pass_t:uint32_t
      {
        /*!
         * Simple shader to draw triangles which are made from
         * a triangle fan from contour data of a sequence of
         * \ref ClosedLineContour. This is used in the prepare
         * stencil pass. The shader is to handle animated paths
         * as well. The packing of vertex is:
         * - Vertex::m_data[0].f --> path0 position vec2::x()
         * - Vertex::m_data[1].f --> path0 position vec2::y()
         * - Vertex::m_data[2].f --> path1 position vec2::x()
         * - Vertex::m_data[3].f --> path1 position vec2::y()
         */
        pass_contour_stencil = 0,

        /*!
         * Shader to perfom per-pixel inside test against a
         * \ref ConicTriangle. The shader is to handle animated
         * paths as well. The packing of a vertex is:
         * - Vertex::m_data[0].u --> StaticData::location() of this vertex
         * - Vertex::m_data[1].f --> virtual texture x-coordinate
         * - Vertex::m_data[2].f --> virtual texture y-coordinate
         * - Vertex::m_data[3].u --> StaticData::location() of 0'th vertex of the triangle
         * .
         *
         * The astral::StaticData whose location is stored at
         * Vertex::m_data[0].u is a single gvec4 whose data is
         * - vec4::x().f --> x-position of point of path0
         * - vec4::y().f --> y-position of point of path0
         * - vec4::x().f --> x-position of point of path1
         * - vec4::y().f --> y-position of point of path1
         * .
         */
        pass_conic_triangles_stencil,

        /*!
         * Shader is used draw the anti-alias fuzz around
         * a path. The shader is to handle animated paths
         * as well.
         * - Vertex::m_data[0].u --> StaticData::location()
         * - Vertex::m_data[1].f --> 0 or 1 to pick to use start or end point
         * - Vertex::m_data[2].f --> -1 or +1 to pick multiplier for nomral vector
         * .
         *
         * The astral::StaticData whose location is stored at
         * Vertex::m_data[0].u is two gvec4's whose data is
         * - [0].x().f --> LineSegment::m_pts[0].x() of path 0
         * - [0].y().f --> LineSegment::m_pts[0].y() of path 0
         * - [0].z().f --> LineSegment::m_pts[0].x() of path 1
         * - [0].w().f --> LineSegment::m_pts[0].y() of path 1
         * - [1].x().f --> LineSegment::m_pts[1].x() of path 0
         * - [1].y().f --> LineSegment::m_pts[1].y() of path 0
         * - [1].z().f --> LineSegment::m_pts[1].x() of path 1
         * - [1].w().f --> LineSegment::m_pts[1].y() of path 1
         * .
         */
        pass_contour_fuzz,

        /*!
         * This shader is used draw the anti-alias fuzz around
         * a path. The shader is to handle animated paths as well.
         * The packing of vertex is:
         * - Vertex::m_data[0].u --> StaticData::location()
         * - Vertex::m_data[1].f --> free
         * - Vertex::m_data[2].f --> free
         * - Vertex::m_data[3].u --> value enumerated by corner_t
         * .
         *
         * The astral::StaticData whose location is stored at
         * Vertex::m_data[0].u is three gvec4's whose data is
         * - [0].x().f --> ConicTriangle::m_pts[0].x() of path 0
         * - [0].y().f --> ConicTriangle::m_pts[0].y() of path 0
         * - [0].z().f --> ConicTriangle::m_pts[0].x() of path 1
         * - [0].w().f --> ConicTriangle::m_pts[0].y() of path 1
         * - [1].x().f --> ConicTriangle::m_pts[1].x() of path 0
         * - [1].y().f --> ConicTriangle::m_pts[1].y() of path 0
         * - [1].z().f --> ConicTriangle::m_pts[1].x() of path 1
         * - [1].w().f --> ConicTriangle::m_pts[1].y() of path 1
         * - [2].x().f --> ConicTriangle::m_pts[2].x() of path 0
         * - [2].y().f --> ConicTriangle::m_pts[2].y() of path 0
         * - [2].z().f --> ConicTriangle::m_pts[2].x() of path 1
         * - [2].w().f --> ConicTriangle::m_pts[2].y() of path 1
         * .
         */
        pass_conic_triangle_fuzz,


        pass_count,
      };

    /*!
     * \brief
     * A closed contour coming from the astral::Contour without the
     * control points.
     */
    class ClosedLineContour
    {
    public:
      /*!
       * The point of the contour.
       */
      c_array<const vec2> m_pts;
    };

    /*!
     * \brief
     * A triangle for rendering a curved portion of the filled path
     * where one side is from a conic or quadartic bezier curve.
     *
     * A triangle for rendering a curved portion of the filled path
     * where one side is from a conic; a conic's parametric form is
     * given by the rational Bezier curve [p0, w * p1, p2] / [1, w, 1](t)
     * where the endpoints are p0 and p2, the control point is p1 and w
     * is the scalar weight of the control point. Note that a quadratic
     * bezier cure is just a conic with w = 1.
     */
    class ConicTriangle
    {
    public:
      /*!
       * \brief
       * Enumeration bit-field to specify which corner of a bounding box
       */
      enum corner_t
        {
          /*!
           * Bit mask to indicate the max-side along
           * the major axis, where the major axis
           * goes from the start point to the end point
           * of the curve.
           */
          max_major = 1,

          /*!
           * Bit mask to indicate the max-side along
           * the minor axis. The minor axis is perpindicular
           * to the major axis so that (major, minor)
           * is positively oriented.
           */
          max_minor = 2,

          /*!
           * The min-min corner
           */
          min_major_min_minor = 0,

          /*!
           * The min-max corner
           */
          min_major_max_minor = max_minor,

          /*!
           * The max-min corner
           */
          max_major_min_minor = max_major,

          /*!
           * The max-max corner
           */
          max_major_max_minor = max_major | max_minor,
        };

      /*!
       * The three points of a triangle.
       */
      vecN<vec2, 3> m_pts;

      /*!
       * Use to determine in the fragment shader is covered.
       * A fragment is covered if (x * x - y) < 0. The
       * texture coordinates are realized as:
       *   start   -> (0.0, 0.0)
       *   control -> (0.5, 0.0)
       *   end     -> (1.0, 1.0)
       */
      vec2
      texture_coordinate(unsigned int I) const;
    };

    /*!
     * \brief
     * A \ref LineSegment represents the geometric data of a line
     * segment of a source contour.
     */
    class LineSegment
    {
    public:
      /*!
       * The points of the line segment with [0] being the
       * start point and [1] being the end point.
       */
      vecN<vec2, 2> m_pts;
    };

    /*!
     * Class to specify a set of \ref pass_t values.
     * This is a lightweight object (indeed just a single
     * uint32_t) value and essentially just encapsulates
     * bit-and'ing, bit-or'ing and bit masking.
     */
    class PassSet
    {
    public:
      /*!
       * Default ctor initializing as no passes in the set
       */
      PassSet(void):
        m_pass_list(0u)
      {}

      /*!
       * Initialize as holding a single pass
       * \param pass what pass to be inside
       */
      PassSet(enum pass_t pass):
        m_pass_list(mask(pass))
      {}

      /*!
       * Ctor to initialize from an astral::anti_alias_t value;
       * the passes \ref pass_conic_triangles_stencil and \ref
       * pass_contour_stencil are always present
       * \param aa_mode if has value astral::with_anti_aliasing,
       *                the also add the passes \ref pass_contour_fuzz
       *                and \ref pass_conic_triangle_fuzz
       */
      PassSet(enum anti_alias_t aa_mode):
        m_pass_list(mask(pass_contour_stencil) | mask(pass_conic_triangles_stencil))
      {
        uint32_t v, r;

        r = mask(pass_contour_fuzz) | mask(pass_conic_triangle_fuzz);
        v = (aa_mode == with_anti_aliasing) ? r : 0u;
        m_pass_list |= v;
      }

      /*!
       * Add a pass to the PassSet
       * \param pass pass to add
       */
      PassSet&
      add_pass(enum pass_t pass)
      {
        m_pass_list |= mask(pass);
        return *this;
      }

      /*!
       * Remove a pass to the PassSet
       * \param pass pass to remove
       */
      PassSet&
      remove_pass(enum pass_t pass)
      {
        m_pass_list &= ~mask(pass);
        return *this;
      }

      /*!
       * Returns true if the PassSet contains the named
       * pass
       * \param pass pass to query
       */
      bool
      has_pass(enum pass_t pass) const
      {
        return (m_pass_list & mask(pass)) != 0u;
      }

    private:
      static
      uint32_t
      mask(enum pass_t pass)
      {
        return 1u << pass;
      }

      uint32_t m_pass_list;
    };

    /*!
     * \brief
     * Backs the data as found in astral::c_array values
     * of a \ref Data
     */
    class Data
    {
    public:
      Data(void):
        m_number_aa_conics(0u),
        m_aa_line_segments_all_ready(true)
      {}

      /*!
       * Add a contour to this \ref Data where each
       * curve of the contour is a line segment, quadratic
       * bezier curve or a conic. In addition, each curve
       * is viewed as having an anti-aliasing boundary.
       */
      Data&
      add_contour(c_array<const ContourCurve> curves);

      /*!
       * Add a contour to this \ref Data where each
       * curve of the contour is a line segment, quadratic
       * bezier curve or a conic. The .second of each
       * element in array determines if the curve specified
       * by the .first is to be given an anti-alias boundary.
       */
      Data&
      add_contour(c_array<const std::pair<ContourCurve, bool>> curves,
                  bool closing_curve_has_aa);

      /*!
       * Add data directly
       * \param line_contour CLOSED loop of points giving a line-segment
       *                     contour which is used in the pass \ref
       *                     pass_contour_stencil. The caller must make sure
       *                     that if non-empty its start and end points
       *                     are the same. In terms of rendering, it is
       *                     actually a triangle fan.
       * \param conic_triangles list of conic triangles. Those with .second
       *                        as true are used in the stencil and anti-alias
       *                        passes, i.e. \ref pass_conic_triangles_stencil
       *                        and \ref pass_conic_triangle_fuzz. Those with
       *                        .second as false are only used in the stencil
       *                        pass \ref pass_conic_triangles_stencil
       * \param aa_line_segments sequence of line segments anti-alias, these
       *                         are added to \ref pass_contour_fuzz pass.
       */
      Data&
      add_raw(c_array<const vec2> line_contour,
              c_array<const std::pair<ConicTriangle, bool>> conic_triangles,
              c_array<const LineSegment> aa_line_segments);

      /*!
       * Reset this \ref Data object to empty, i.e. as if add_contour()
       * was never called.
       */
      Data&
      clear(void)
      {
        m_number_aa_conics = 0u;
        m_contour_pts.clear();
        m_contour_line_ranges.clear();
        m_conic_triangles.clear();
        m_aa_explicit_line_segments.clear();
        m_aa_implicit_line_segments.clear();
        m_aa_line_segments_all.clear();
        m_aa_line_segments_all_ready = true;

        return *this;
      }

      /*!
       * Returns true if Data is empty of content
       */
      bool
      empty(void) const
      {
        return m_number_aa_conics == 0u
          && m_contour_pts.empty()
          && m_contour_line_ranges.empty()
          && m_conic_triangles.empty()
          && m_aa_explicit_line_segments.empty();
      }

      /*!
       * Returns the number of vertices and number of
       * static data gvec4's needed to pack this Data
       * assuming all line segments are given anti-aliasing
       * fuzz
       * \param pass_set which passes to include
       * \param out_number_vertices location to which to write
       *                            the needed number of vertices
       * \param out_number_gvec4s_block_size2 the number of gvec4's needed
       *                                      that can be allocated in blocks
       *                                      of size 2.
       * \param out_number_gvec4s_block_size3 the number of gvec4's needed
       *                                      that can be allocated in blocks
       *                                      of size 3.
       */
      void
      storage_requirement(PassSet pass_set,
                          vecN<unsigned int, pass_count> *out_number_vertices,
                          unsigned int *out_number_gvec4s_block_size2,
                          unsigned int *out_number_gvec4s_block_size3) const;

    private:
      friend class FillSTCShader;

      template<typename T>
      void
      add_contour_implement(const T &curves);

      c_array<const LineSegment>
      aa_line_segments_all(void) const;

      std::vector<vec2> m_contour_pts;
      std::vector<range_type<unsigned int>> m_contour_line_ranges;
      std::vector<std::pair<ConicTriangle, bool>> m_conic_triangles;
      std::vector<LineSegment> m_aa_explicit_line_segments;
      std::vector<LineSegment> m_aa_implicit_line_segments;
      unsigned int m_number_aa_conics;

      mutable bool m_aa_line_segments_all_ready;
      mutable std::vector<LineSegment> m_aa_line_segments_all;
    };

    /*!
     * Builder for creating render data for animated paths and contours.
     */
    class AnimatedData
    {
    public:
      /*!
       * Add an animated contour. The number of curves and
       * curve types must be the same. In addition, each
       * curve must be a line segment or quadratic bezier
       * curve.
       */
      AnimatedData&
      add_contour(c_array<const ContourCurve> start_curves,
                  c_array<const ContourCurve> end_curves);

      /*!
       * Returns the number of vertices and number of
       * static data gvec4's needed to pack this Data
       * assuming all line segments are given anti-aliasing
       * fuzz
       * \param pass_set which passes to include
       * \param out_number_vertices location to which to write
       *                            the needed number of vertices
       * \param out_number_gvec4s_block_size2 the number of gvec4's needed
       *                                      that can be allocated in blocks
       *                                      of size 2.
       * \param out_number_gvec4s_block_size3 the number of gvec4's needed
       *                                      that can be allocated in blocks
       *                                      of size 3.
       */
      void
      storage_requirement(PassSet pass_set,
                          vecN<unsigned int, pass_count> *out_number_vertices,
                          unsigned int *out_number_gvec4s_block_size2,
                          unsigned int *out_number_gvec4s_block_size3) const
      {
        return m_start_data.storage_requirement(pass_set, out_number_vertices,
                                                out_number_gvec4s_block_size2,
                                                out_number_gvec4s_block_size3);
      }

    private:
      friend class FillSTCShader;
      Data m_start_data, m_end_data;
    };

    /*!
     * \brief
     * Represents the data for rendering a filled path or
     * filled animated path.
     */
    class CookedData
    {
    public:
      /*!
       * source of all vertices
       */
      reference_counted_ptr<const VertexData> m_vertex_data;

      /*!
       * For each pass, a range into \ref m_vertex_data
       */
      vecN<range_type<int>, pass_count> m_pass_range;

      /*!
       * The same as \ref m_pass_range[\ref pass_contour_fuzz]
       * but lacks the data for the implicit edge from the
       * end to the start of the contour.
       */
      range_type<int> m_aa_line_pass_without_implicit_closing_edge;

    private:
      friend class FillSTCShader;
      reference_counted_ptr<const StaticData> m_block_size2, m_block_size3;
    };

    /*!
     * The shaders of doing stencil rendering
     */
    vecN<reference_counted_ptr<const MaskItemShader>, pass_count> m_shaders;

    /*!
     * Shader for rendering the covering rect. Unlike
     * the shaders listed in \ref m_shaders, this shader
     * takes item data the same as astral::DynamicRectShader
     */
    reference_counted_ptr<const MaskItemShader> m_cover_shader;

    /*!
     * Create and return the \ref CookedData value to render a path
     * \param engine the astral::RenderEngine from which to allocate data
     * \param render the render to render
     */
    static
    CookedData
    create_cooked_data(RenderEngine &engine, const Data &render)
    {
      return create_cooked_data(engine, render, render);
    }

    /*!
     * Create and return the \ref CookedData value to render
     * an animated path
     *
     * \param engine the astral::RenderEngine from which to allocate data
     * \param render animated data to render
     */
    static
    CookedData
    create_cooked_data(RenderEngine &engine, const AnimatedData &render)
    {
      return create_cooked_data(engine, render.m_start_data, render.m_end_data);
    }

    /*!
     * Pack the rendering data for filling a static path.
     * \param render the path
     * \param pass_set which passes to include
     * \param dst_vertices blocks to which to pack the vertex data
     * \param dst_static_data_block2 blocks of size 2 to which to pack the static data
     * \param dst_static_data_block3 blocks of size 3 to which to pack the static data
     */
    static
    void
    pack_render_data(const Data &render, PassSet pass_set,
                     vecN<c_array<const VertexStreamerBlock>, pass_count> dst_vertices,
                     c_array<const StaticDataStreamerBlock32> dst_static_data_block2,
                     c_array<const StaticDataStreamerBlock32> dst_static_data_block3)
    {
      pack_render_data(render, render, pass_set, dst_vertices,
                       dst_static_data_block2, dst_static_data_block3);
    }

    /*!
     * Pack the rendering data for filling an animated path
     * \param render animated data to render
     * \param pass_set which passes to include
     * \param dst_vertices blocks to which to pack the vertex data
     * \param dst_static_data_block2 blocks of size 2 to which to pack the static data
     * \param dst_static_data_block3 blocks of size 3 to which to pack the static data
     */
    static
    void
    pack_render_data(const AnimatedData &render, PassSet pass_set,
                     vecN<c_array<const VertexStreamerBlock>, pass_count> dst_vertices,
                     c_array<const StaticDataStreamerBlock32> dst_static_data_block2,
                     c_array<const StaticDataStreamerBlock32> dst_static_data_block3)
    {
      pack_render_data(render.m_start_data,
                       render.m_end_data,
                       pass_set, dst_vertices,
                       dst_static_data_block2,
                       dst_static_data_block3);
    }

    /*!
     * Pack item data the astral::MaskItemShader values
     * of an astral::FillSTCShader accept
     * \param t time of animated path with 0 <= t <= 1
     * \param scale_factor the scale factor of the render target
     *                     at which the filled path is rendered,
     *                     i.e. the value of \ref
     *                     RenderEncoderBase::render_scale_factor()
     * \param dst location to which to place the packed values
     */
    static
    void
    pack_item_data(float t, vec2 scale_factor, c_array<gvec4> dst)
    {
      ASTRALassert(dst.size() == item_data_size);
      dst[0].x().f = t;
      dst[0].y().f = 1.0f / scale_factor.x();
      dst[0].z().f = 1.0f / scale_factor.y();
    }

  private:
    static
    void
    pack_conic_render_data(const Data &render0, const Data &render1, PassSet pass_set,
                           c_array<const VertexStreamerBlock> dst_stencil,
                           c_array<const VertexStreamerBlock> dst_fuzz,
                           c_array<const StaticDataStreamerBlock32> dst_static);

    static
    void
    pack_line_stencil_render_data(const Data &render0, const Data &render1,
                                  c_array<const VertexStreamerBlock> dst_stencil);

    static
    void
    pack_line_fuzz_render_data(const Data &render0, const Data &render1,
                               c_array<const VertexStreamerBlock> dst_fuzz,
                               c_array<const StaticDataStreamerBlock32> dst_static);

    static
    CookedData
    create_cooked_data(RenderEngine &engine, const Data &render0, const Data &render1);

    static
    void
    pack_render_data(const Data &render0, const Data &render1, PassSet pass_set,
                     vecN<c_array<const VertexStreamerBlock>, pass_count> dst_vertices,
                     c_array<const StaticDataStreamerBlock32> dst_static_data_block2,
                     c_array<const StaticDataStreamerBlock32> dst_static_data_block3);
  };
}

#endif

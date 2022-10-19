/*!
 * \file stroke_query.hpp
 * \brief file stroke_query.hpp
 *
 * Copyright 2021 by InvisionApp.
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

#ifndef ASTRAL_STROKE_QUERY_HPP
#define ASTRAL_STROKE_QUERY_HPP

#include <astral/renderer/shader/stroke_shader.hpp>
#include <astral/util/enum_flags.hpp>

namespace astral
{
  /*!
   * \brief
   * An astral::StrokeQuery is to query what pixel-rects to render
   * and what content to each pixel-rect to render when stroking
   * sparsely. A StrokeQuery is a  very heavy object and should be
   * reused.
   */
  class StrokeQuery:public reference_counted<StrokeQuery>::non_concurrent
  {
  private:
    class Implement;

  public:
    /*!
     * astral::EnumFlag value to track what primitive types a StrokeQuery
     * should track in its output
     */
    class ActivePrimitives:public EnumFlags<StrokeShader::primitive_type_t, StrokeShader::number_primitive_types>
    {
    public:
      /*!
       * Ctor that leaves all fields uninitialized
       */
      ActivePrimitives(void)
      {}

      /*!
       * Ctor.
       * Initializes exactly those primitive types required by an
       * astral::StrokeParameters that a set of shaders supports
       */
      ActivePrimitives(bool caps_joins_collapse,
                       const StrokeParameters &stroke_params,
                       bool include_inner_glue,
                       const MaskStrokeShader::ItemShaderSet &shaders):
        ActivePrimitives(caps_joins_collapse, stroke_params, include_inner_glue, &shaders)
      {}

      /*!
       * Ctor.
       * Initializes exactly those primitive types required by an
       * astral::StrokeParameters that a set of shaders supports,
       * if no shader is passed then only the requirements of the
       * astral::StrokeParameters are taken into account
       */
      ActivePrimitives(bool caps_joins_collapse,
                       const StrokeParameters &stroke_params,
                       bool include_inner_glue,
                       const MaskStrokeShader::ItemShaderSet *shaders = nullptr);

      /*!
       * Ctor.
       * Initializes exactly those primitive types required by an
       * astral::StrokeParameters that a set of shaders supports
       */
      ActivePrimitives(bool caps_joins_collapse,
                       const StrokeParameters &stroke_params,
                       const MaskStrokeShader::ShaderSet &shaders,
                       enum StrokeShader::path_shader_t P):
        ActivePrimitives(caps_joins_collapse, stroke_params,
                         P == StrokeShader::animated_path_shader,
                         shaders.m_subset[P])
      {}
    };

    /*!
     * Class to represent the various radii use in a stroke query
     */
    class StrokeRadii
    {
    public:
      /*!
       * Ctor to initialize all radii as zero.
       */
      StrokeRadii(void):
        m_edge_radius(0.0f),
        m_cap_radius(0.0f),
        m_join_radius(0.0f),
        m_max_radius(0.0f)
      {}

      /*!
       * Direct ctor.
       * \param edge_radius value for edge_radius() to return
       * \param join_radius value for join_radius() to return
       * \param cap_radius value for cap_radius() to return
       */
      StrokeRadii(float edge_radius, float join_radius, float cap_radius):
        m_edge_radius(edge_radius),
        m_cap_radius(cap_radius),
        m_join_radius(join_radius),
        m_max_radius(t_max(edge_radius, t_max(cap_radius, join_radius)))
      {}

      /*!
       * Derive the radii from an astral::StrokeParameters and
       * a StrokeShader::ItemDataPackerBase.
       */
      StrokeRadii(const StrokeParameters &stroke_params,
                  const StrokeShader::ItemDataPackerBase &packer);

      /*!
       * The _radius_ around the edges of the area of the stroke
       */
      float
      edge_radius(void) const
      {
        return m_edge_radius;
      }

      /*!
       * The _radius_ around the caps of the area of the stroke
       */
      float
      cap_radius(void) const
      {
        return m_cap_radius;
      }

      /*!
       * The _radius_ around the joins of the area of the stroke
       */
      float
      join_radius(void) const
      {
        return m_join_radius;
      }

      /*!
       * The max of each of the radii
       */
      float
      max_radius(void) const
      {
        return m_max_radius;
      }

    private:
      float m_edge_radius;
      float m_cap_radius;
      float m_join_radius;
      float m_max_radius;
    };

    /*!
     * \brief
     * Specifies what element added with StrokeQuery::add_element()
     * and what portion of it to draw.
     */
    class Source
    {
    public:
      /*!
       * The ID of the data source as fed to StrokeQuery::add_element()
       */
      unsigned int
      ID(void) const
      {
        return m_ID;
      }

      /*!
       * Array of ranges into CookedData::vertex_data()
       * for the content of this Source to render.
       */
      c_array<const range_type<int>>
      vertex_ranges(enum StrokeShader::primitive_type_t P) const
      {
        return make_const_c_array(*m_idxs[P]);
      }

    private:
      friend class Implement;

      explicit
      Source(unsigned int ID, StrokeQuery::Implement &p);

      unsigned int m_ID;
      vecN<std::vector<range_type<int>>*, StrokeShader::number_primitive_types> m_idxs;
    };

    /*!
     * A ResultRect represents a range of tiles and what portions
     * of the stroke intersect the range of tiles. Recall that the
     * rectangle passed to StrokeQuery::query_render() is broken
     * into tiles with all but the last row and column being of size
     * ImageAtlas::tile_size_without_padding.
     */
    class ResultRect
    {
    public:
      /*!
       * Returns the range of tiles covered
       */
      const vecN<range_type<int>, 2>&
      tile_range(void) const
      {
        return m_range;
      }

      /*!
       * Array of source values each giving from what
       * StrokeShader::CookedData and what portions of
       * it to render. One is guaranteed that the value
       * of Source::ID() is unique within the returned
       * array
       */
      c_array<const Source>
      sources(void) const
      {
        return make_c_array(*m_sources);
      }

    private:
      friend class Implement;

      explicit
      ResultRect(StrokeQuery::Implement &p);

      /* gives a range in each coordinate of elementary
       * pixel rects the ResultRect covers
       */
      vecN<range_type<int>, 2> m_range;

      /* list of sources */
      std::vector<Source> *m_sources;
    };

    /*!
     * Create an astral::StrokeQuery object.
     */
    static
    reference_counted_ptr<StrokeQuery>
    create(void);

    virtual
    ~StrokeQuery()
    {}

    /*!
     * Begin a query. Within a begin_query()/end_query() pair,
     * stroke commands can be added with add_element(). Starting
     * a new query implicitly ends the current query and clear the
     * object.
     * \param rect_transformation_elements transformation from
     *                                     element coordinates to rect
     *                                     coordinates
     * \param rect_size size of the rect, the rectangle's min_corner
     *                  is (0, 0) always.
     * \param sparse_query if true attempt to minimize the area
     *                     of hits, if false elements() will have
     *                     size 1 holding all hits
     * \param restrict_rects if non-empty, ignore intersections outside
     *                       of the union of these rects
     */
    void
    begin_query(const ScaleTranslate &rect_transformation_elements,
                const ivec2 &rect_size, bool sparse_query,
                c_array<const BoundingBox<float>> restrict_rects = c_array<const BoundingBox<float>>());

    /*!
     * End the current query, commands may no longer be added.
     * \param max_size maximum size of the effective pixel-rect
     *                 for each \ref ResultRect listed in
     *                 elements() after the call.
     */
    void
    end_query(unsigned int max_size);

    /*!
     * Add an element to teh current query.
     * \param ID user provided ID to identify the element
     * \param element_transformation_stroking transformation from stroking
     *                                        coordinates to element coordinates
     * \param stroking_transformation_path transformation from path coordinates
     *                                     to stroking coordinates
     * \param path cooked data for stroking the path
     * \param animation_t if path is animted, the animation interpolate
     * \param active_primitives specifies what primitives are included in the query
     * \param stroke_radii the radii around the edges, joins and caps for
     *                     the query
     */
    void
    add_element(unsigned int ID,
                const Transformation &element_transformation_stroking,
                const Transformation &stroking_transformation_path,
                const StrokeShader::CookedData &path,
                float animation_t,
                ActivePrimitives active_primitives,
                StrokeRadii stroke_radii);

    /*!
     * Returns the StrokeShader::ResultRect values;
     * these are the rects which intersect the stroke.
     */
    c_array<const ResultRect>
    elements(void) const;

    /*!
     * Tiles that are empty, the region represented by an
     * empty tile is ... document this.
     */
    c_array<const uvec2>
    empty_tiles(void) const;

    /*!
     * Returns true if and only if the query is sparse,
     * i.e. there is more than one tile, counting empty
     * and non-empty tiles together.
     */
    bool
    is_sparse(void) const;

    /*!
     * Returns the width and height of the last column
     * and row of the rect into which the stroked paths
     * were broken into. This size does NOT include any
     * padding.
     */
    ivec2
    end_elementary_rect_size(void) const;

    /*!
     * Returns the number of elementary rects into
     * which the stroked paths were broken into.
     */
    ivec2
    number_elementary_rects(void) const;

    /*!
     * Clear the query to empty.
     */
    void
    clear(void);
    
  private:
    StrokeQuery(void)
    {}

    Implement&
    implement(void);

    const Implement&
    implement(void) const;
  };
}

#endif

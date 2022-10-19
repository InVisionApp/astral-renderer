/*!
 * \file stroke_shader.hpp
 * \brief file stroke_shader.hpp
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

#ifndef ASTRAL_STROKE_SHADER_HPP
#define ASTRAL_STROKE_SHADER_HPP

#include <astral/util/vecN.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/rect.hpp>
#include <astral/util/transformation.hpp>
#include <astral/contour_curve.hpp>
#include <astral/renderer/render_data.hpp>
#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/stroke_parameters.hpp>
#include <astral/renderer/shader/stroke_shader_item_data_packer.hpp>
#include <astral/renderer/shader/item_shader.hpp>
#include <astral/renderer/shader/item_data.hpp>
#include <astral/renderer/shader/stroke_support.hpp>

namespace astral
{
  class StrokeQuery;

  /*!
   * \brief
   * astral::CommonStrokeShader embodies the data and
   * concepts for stroking.
   *
   * Data concepts: a path can be tessellated for stroking
   * into a sequence of line segments, quadratic bezier
   * curves and arc-curve values. The data of these
   * is handled by StrokeShader::LineSegment,
   * and StrokeShader::Quadratic. The data needed
   * for stroking the joins is represented by
   * StrokeShader::Join and the data needed
   * for stroking caps is represented by
   * StrokeShader::Cap.
   *
   * Shader concepts: each of the StrokeShader::Base
   * derived types needs a shader to draw its data. In addition,
   * the shader and data for static and animated paths is
   * also different. For each cap style and join style,
   * one needs a shader pair for animated and static path
   * shading as well.
   *
   * In addition, because tessellation will not make neighboring
   * elements of a single tessellated curve line up tangentially
   * (especailly for animation), one needs to have glue-joins
   * as well for the cracks between such elements. Glue join data
   * is also represented by the values of StrokeShader::Join
   * but are shaded with glue shaders that should shade the joins
   * always as rounded joins. There is a shader for both the outside
   * of the meeting point and inside of the meeting point. Failure to
   * render either of these results in render cracks.
   *
   * Lastly, there is a concept of "cappers" which is only needed
   * to implement dashed stroking. For each line segment, arc
   * and quadratic curve, there is a capper. Each such capper
   * value is shaded twice as enumerated by StrokeShader::capper_shader_t.
   * The purpose of a capper is to implement capped dash stroking
   * (typically for rounded caps). The idea of a capper is to allow
   * for a cap that started in a line segment, arc or quadratic curve
   * to finish by the capper. The main purpose is that a curve will
   * typically get tessellated into multiple curves near where it
   * sharply turns, by shading the cap not hugging the curve via a capper
   * one can get the rounded caps to look better.
   */
  class StrokeShader:public StrokeShaderItemDataPacker
  {
  public:
    /*!
     * \brief
     * Enumeration to specify static or animated path shader
     */
    enum path_shader_t:uint32_t
      {
        /*!
         * Enumeration to indicate to choose the shader
         * values for rendering staic paths, i.e. astral::Path
         */
        static_path_shader = 0,

        /*!
         * Enumeration to indicate to choose the shader
         * values for rendering animated paths, i.e.
         * astral::AnimatedPath
         */
        animated_path_shader,

        path_shader_count,
      };

    /*!
     * \brief
     * enumeration to describe the capper shaders
     */
    enum capper_shader_t:uint32_t
      {
        /*!
         * Indicates to draw a capper for the start of a
         * StrokeShader::LineSegment or StrokeShader::Quadratic
         * value
         */
        capper_shader_start = 0,

        /*!
         * Indicates to draw a capper for the end of a
         * StrokeShader::LineSegmentor StrokeShader::Quadratic
         * value
         */
        capper_shader_end,

        number_capper_shader
      };

    /*!
     * \brief
     * Enumerations to give the sizes of the static data
     */
    enum static_data_sizes:uint32_t
      {
        /*!
         * How many elements in static data a StrokeShader::LineSegment takes
         */
        line_segment_static_data_size = 2,

        /*!
         * How many elements in static data a StrokeShader::QuadraticCurve takes
         */
        quadratic_curve_data_data_size = 3,

        /*!
         * How many elements in static data a StrokeShader::Join takes
         */
        join_static_data_size = 3,

        /*!
         * How many elements in static data a StrokeShader::Cap takes
         */
        cap_static_data_size = 2,

        /*!
         * How many elements in static data a pair of StrokeShader::LineSegment take
         */
        line_segment_pair_static_data_size = 4,

        /*!
         * How many elements in static data a pair of StrokeShader::QuadraticCurve take
         */
        quadratic_curve_pair_static_data_size = 5,

        /*!
         * How many elements in static data a pair of StrokeShader::Join take
         */
        join_pair_static_data_size = 5,

        /*!
         * How many elements in static data a pair of StrokeShader::Cap take
         */
        cap_pair_static_data_size = 3,
      };

    /*!
     * \brief
     * Enumeration to specify the various primitive types.
     */
    enum primitive_type_t:uint32_t
      {
        /*!
         * Specifies the data associated to
         * StrokeShader::LineSegment data.
         */
        line_segments_primitive,

        /*!
         * Specifies the data associated to
         * StrokeShader::Quadratic data where
         * each quadratic curve is to be stroked
         * by as a bi-arc which is computed in
         * the vertex shading.
         */
        biarc_curves_primitive,

        /*!
         * Specifies the data associated to
         * StrokeShader::Join data for the glue
         * between neighboring curves that are
         * from the same edge; these glue joins
         * are to be drawn always as rounded
         * joins
         */
        glue_primitive,

        /*!
         * Specifies the data associated to
         * StrokeShader::Join data for the glue
         * between neighboring curves that are
         * from the same edge; these glue joins
         * are to be drawn always as bevel joins.
         * Their purpose is to correctly observe
         * cusps of cubic curves.
         */
        glue_cusp_primitive,

        /*!
         * Specifies the data associated to
         * StrokeShader::Join data for the
         * joins.
         */
        joins_primitive,

        /*!
         * Specifies the data associated to
         * StrokeShader::Join data for the inner
         * glue that is needed for animated
         * path stroking
         */
        inner_glue_primitive,

        /*!
         * Specifies the data associated to
         * StrokeShader::Cap data to stroke
         * caps
         */
        caps_primitive,

        /*!
         * Specifies the data associated to
         * for stroking the capper along line
         * segments for dashed stroking. The
         * vertex data type is enumerated by
         * StrokeShader::cap_point_t but the data
         * offset is to StrokeShader::LineSegment
         * values.
         */
        segments_cappers_primitive,

        /*!
         * Specifies the data associated to
         * for stroking the capper along bi-arc
         * curves for dashed stroking. The
         * vertex data type is enumerated by
         * StrokeShader::cap_point_t but the data
         * offset is to StrokeShader::Quadratic
         * values.
         */
        biarc_curves_cappers_primitive,

        number_primitive_types
      };

    /*!
     * \brief
     * Enumeration specifying a role of a vertex when rendering
     * a StrokeShader::Quadratic as a biarc. Astral strokes quadratic
     * curves by rendering a biarc whose approximation is done on
     * GPU at vertex shading.
     *
     * A biarc is a pair of circular arcs with independent centers and radii.
     * The two arcs connect at a point at which they have the same tangent,
     * thus making the biarc a G<SUP>1</SUP>-continuous curve. Because quadratic Bezier
     * curves (parabola) do not have inflection points, meaning tangents never change
     * abruptly, the approximation biarc doesn’t have an inflection point either.
     * This guarantees that a biarc used for approximating a quadratic Bezier
     * curve can never have an S shape, but it always has a C shape.
     *
     * Astral measures the approximation error and subdivides a given quadratic
     * Bezier curve into smaller biarc segments. The subdivision threshold works
     * in pixel space and can be externally controlled. Once the curve is
     * approximated at the target accuracy, Astral then creates polygons to cover
     * the given thickness of the input curve and manipulates them in the vertex
     * shader. The fragment shader finally performs rasterization of the biarc stroke
     * based on distance from arc centers.
     *
     * When a biarc curve is stroked the two arcs of the biarc is offset inwards
     * and outwards to create inner and outer curves, respectively. The offset
     * distance (stroking radius) is half of the stroke width. How a biarc curve
     * is stroked for a given stroke width can be illustrated as below:
     *
     * \image html biarc_labels.svg "Biarc properties"
     *
     * A stroked biarc is rendered using stroke shaders. After the triangles
     * generated in this method they are fed to the vertex shader for tessellation.
     * A number of vertices need to be defined to efficiently tessellate (triangulate)
     * a biarc. The Tessellation optimized for efficiency in several ways:
     * - Minimize the number of triangles sent to GPU to reduce load on triangle setup
     * - Prevent triangles from overlapping (as much as possible) to avoid overdraw.
     * - Triangles leave minimal empty space around stroked biarc to reduce wasted fragment shader runs.
     *
     * \image html biarc_no_inversion.svg "Biarc triangulation with no inversion"
     *
     * Every vertex has a role in the triangulation process, thus they are named
     * accordingly. Some vertex roles have been abbreviated in the figure above
     * for readability.
     *
     *  Vertex roles:
     * - top : StrokeShader::biarc_offset_top
     * - afc : StrokeShader::biarc_offset_away_from_center
     * - bp  : StrokeShader::biarc_offset_base_point
     * - tc  : StrokeShader::biarc_offset_towards_center
     * - ζ   : StrokeShader::biarc_offset_zeta
     * - ω   : StrokeShader::biarc_offset_omega
     *
     * Although C<SUB>1</SUB> and C<SUB>2</SUB> are not vertex roles, they are positions some vertices
     * are assigned to depending on inversion. They denote the centers of the arcs.
     *
     * The names of the vertices include subscripts representing either end
     * of either arc. The numerical subscripts, 1 and 2, denote the first arc
     * and the second arc, respectively. The alphabetical subscripts, A and
     * B, denote the left end and the right end of an arc, respectively.
     *
     * Some of the vertices coincide because of one of the following reasons:
     * - They are in a position shared by both arcs.
     * - They are part of a hidden (degenerate) triangle.
     *
     * If the stroking radius is equal to the radius of an arc, the inner
     * offset of the arc will collapse to a point (cusp) at the arc center.
     * If the stroking radius is greater than the radius of the arc, the inner
     * offset will invert. Because a biarc usually have two different arc radii,
     * its arcs may not invert at the same time. Below are different cases based
     * on inversion states of the two arcs.
     *
     * \image html biarc_second_arc_cusp.svg "Biarc triangulation where the inner offset curve of the second arc is a cusp point at C2"
     *
     * Note that the triangle [tc<SUB>2A</SUB>-top<SUB>2A</SUB>-tc<SUB>2B</SUB>] collapsed to a line segment which,
     * in Computer Graphics, is called a degenerate triangle. Degenerate triangles
     * may be rasterized with no pixel or several pixels along the line segment.
     * This is used for preventing T-intersections (T-junctions).
     *
     * \image html biarc_first_arc_cusp_second_arc_inverted.svg "Biarc triangulation where the inner offset curve of the second arc invert and that of the first curve is a cusp point at C1"
     *
     * As the stroking radius passes the arc radius, the inner offset curve begins
     * inverting at the arc center. The inverted portions need to be triangulated too.
     * Because only the vertex shader knows whether an offset curve will invert, triangles
     * for the inverted portion must be fed to the vertex shader in all cases, as the vertex
     * shader cannot create new triangles. Because of this limitation, the triangles that
     * cover the inverted portions simply become degenerate when there is no inversion.
     * The inverted triangles are drawn in red. Note that, there is some inevitable overlap
     * between the inverted portions of an arc "folds" onto the other arc.
     *
     * \image html biarc_both_arcs_inverted.svg "Biarc triangulation where the inner offset curves of both arcs invert"
     *
     * In order to minimize the number of triangles, the base points (bp<SUB>1A</SUB>,
     * bp<SUB>1B</SUB>, bp<SUB>2A</SUB>, and bp<SUB>2B</SUB>) are not used in the triangulation.
     * When the inversion states taken into account, stroke triangles can be formed as follows:
     *
     * - [tc<SUB>1A</SUB> , afc<SUB>1A</SUB> , top<SUB>1A</SUB>]
     * - [tc<SUB>1A</SUB> , top<SUB>1A</SUB> , top<SUB>1B</SUB>]
     * - [tc<SUB>1A</SUB> , top<SUB>1B</SUB> , tc<SUB>1B</SUB>]
     * - [top<SUB>1B</SUB> , afc<SUB>1B</SUB> , tc<SUB>1B</SUB>]
     *
     * Similarly for the second arc (in a symmetrical fashion),
     *
     * - [tc<SUB>2B</SUB> , afc<SUB>2B</SUB> , top<SUB>2B</SUB>]
     * - [tc<SUB>2B</SUB> , top<SUB>2B</SUB> , top<SUB>2A</SUB>]
     * - [tc<SUB>2B</SUB> , top<SUB>2A</SUB> , tc<SUB>2A</SUB>]
     * - [top<SUB>2A</SUB> , afc<SUB>2A</SUB> , tc<SUB>2A</SUB>]
     *
     * Note that the triangles [tc<SUB>1A</SUB> , top<SUB>1B</SUB> , tc<SUB>1B</SUB>] and [tc<SUB>2B</SUB> , top<SUB>2A</SUB> , tc<SUB>2A</SUB>]
     * will collapse to a degenerate triangle on inversion, as tc points of the same arc will meet at the arc center.
     *
     * And the triangles that fill the inverted region can be written as:
     *
     *  - [ζ<SUB>1B</SUB> , tc<SUB>1A</SUB> , ω<SUB>1B</SUB>]
     *  - [ω<SUB>1B</SUB> , tc<SUB>1A</SUB> , ω<SUB>1A</SUB>]
     *  - [ζ<SUB>1A</SUB> , tc<SUB>1A</SUB> , ω<SUB>1A</SUB>]
     *
     * Similarly for the second arc (in a symmetrical fashion),
     *
     *  - [ζ<SUB>2A</SUB> , tc<SUB>2B</SUB> , ω<SUB>2A</SUB>]
     *  - [ω<SUB>2A</SUB> , tc<SUB>2B</SUB> , ω<SUB>2B</SUB>]
     *  - [ζ<SUB>2B</SUB> , tc<SUB>2B</SUB> , ω<SUB>2B</SUB>]
     *
     * The triangles listed so far fill covers the entire stroked biarc including the
     * inverted regions. However, there are T-intersections in this tessellation.
     * <a target=”_blank”
     * href="https://computergraphics.stackexchange.com/questions/1461/why-do-t-junctions-in-meshes-result-in-cracks/1464">
     * T-intersections may cause visual artifacts (cracks)</a>.
     *
     * One way to eliminate t-intersections is to split the triangle at the T-forming vertex, however this complicates
     * the simple triangulation this method utilizes. An alternate approach is to introduce a degenerate triangle to fill
     * the crack between an edge endpoints and the splitting vertex.
     *
     * When there is an inversion, the center of an arc poses a T-intersection to the other arc of the same biarc
     * because the two arcs may have different centers. Therefore, we need to add degenerate triangles on the shared
     * edge of both arc strokes. Thus we form those degenerate triangles as follows:
     *
     * - [ζ<SUB>1B</SUB> , tc<SUB>1B</SUB> , afc<SUB>1B</SUB>]
     * - [ζ<SUB>2A</SUB> , tc<SUB>2A</SUB> , afc<SUB>2A</SUB>]
     *
     * The arc center on an edge also poses T-intersections to any neighboring geometry such as
     * line, glue, join, cap, or another biarc. There we need to add degenerate triangles to the
     * ends of the biarc segment (opposite ends of the two arcs) as follows:
     *
     * - [ζ<SUB>1A</SUB> , afc<SUB>1A</SUB> , ω<SUB>1A</SUB>]
     * - [ζ<SUB>2B</SUB> , afc<SUB>2B</SUB> , ω<SUB>2B</SUB>]
     *
     * We did not include any base points in the biarc tessellation, however, neighboring geometries
     * such as rounded and bevel joins uses the base points. This may cause t-intersections on a biarc
     * edge, if the edge is shared by a rounded or bevel join. Therefore we need to add degenerate triangles
     * similar to those above, except this time instead of arc centers  we will use base points on the ends
     * of the biarc segment (opposite ends of the two arcs) as follows:
     *
     * - [ζ<SUB>1A</SUB> , bp<SUB>1A</SUB> , ω<SUB>1A</SUB>]
     * - [ζ<SUB>2B</SUB> , bp<SUB>2B</SUB> , ω<SUB>2B</SUB>]
     *
     * This finalizes all the triangles required for efficiently tessellating a biarc stroke
     * and eliminating any potential rendering cracks, and brings the triangle count to <STRONG>20</STRONG>.
     */
    enum biarc_point_t:uint32_t
      {
        /*!
         * number of bit to specify the point offset type
         */
        biarc_offset_type_number_bits = 3,

        /*!
         * bit0 giving the offset type
         */
        biarc_offset_type_bit0 = 0,

        /*!
         * if this bit is up, then specifies
         * the base point is the end point of
         * one of the arcs, if down then base
         * point is the start of one of the arcs.
         * Which arc is determined by if the bit
         * StrokeShader::biarc_is_second_arc_mask
         * is up
         */
        biarc_is_end_point_bit = biarc_offset_type_bit0 + biarc_offset_type_number_bits,

        /*!
         * if this bit is up, then specifies
         * that the point is of the second
         * arc of the biarc. If down, then the
         * point is of the first arc of the
         * biarc.
         */
        biarc_is_second_arc_bit = biarc_is_end_point_bit + 1u,

        /*!
         * Number bits total needed to specify a biarc-point
         */
        biarc_total_bits = biarc_is_second_arc_bit + 1u,

        /*!
         * Mask generated from \ref biarc_offset_type_bit0 and biarc_offset_type_number_bits
         */
        biarc_offset_type_mask = ASTRAL_MASK(biarc_offset_type_bit0, biarc_offset_type_number_bits),

        /*!
         * Mask generated from \ref biarc_is_end_point_bit
         */
        biarc_is_end_point_mask = ASTRAL_MASK(biarc_is_end_point_bit, 1u),

        /*!
         * Mask generated from \ref biarc_is_second_arc_bit
         */
        biarc_is_second_arc_mask = ASTRAL_MASK(biarc_is_second_arc_bit, 1u),

        /*!
         * On curve inversion at the same "height"
         * as the inverted inner stroking offset curve.
         * When not inverted, then same value as
         * StrokeShader::biarc_offset_towards_center
         */
        biarc_offset_omega = 0,

        /*!
         * Indicates to to offset from the point on the arc towards
         * the center of the circle that defines arc by
         * stroke radius; regardless of the stroking radius.
         */
        biarc_offset_zeta,

        /*!
         * Indicates to offset from the point on the arc towards
         * the center of the circle that defines arc by
         * stroke radius, but no more than the arc radius, i.e.
         * on inversion is the center of the circle that
         * defines the arc
         */
        biarc_offset_towards_center,

        /*!
         * Indicates that no offset is applied
         * from the base point.
         */
        biarc_offset_base_point,

        /*!
         * Indicates to offset from the point on the arc away
         * from the center of the circle that defines arc by
         * stroke radius.
         */
        biarc_offset_away_from_center,

        /*!
         * The same height of the outer-stroking boundary
         */
        biarc_offset_top,
      };

    /*!
     * \brief
     * Enumeration specifying a role of a vertex when rendering
     * a StrokeShader::LineSegment
     */
    enum line_point_t:uint32_t
      {
        /*!
         * number of bit to specify the point offset type
         */
        line_offset_type_number_bits = 2,

        /*!
         * bit0 giving the offset type
         */
        line_offset_type_bit0 = 0,

        /*!
         * if this bit is up, then specifies
         * the base point as the end point of
         * the line segment, if down then base
         * point is the start point.
         */
        line_is_end_point_bit = line_offset_type_bit0 + line_offset_type_number_bits,

        /*!
         * Number bits total needed to specify a line-point
         */
        line_total_bits = line_is_end_point_bit + 1u,

        /*!
         * Mask generated from \ref line_offset_type_bit0 and \ref line_offset_type_number_bits
         */
        line_offset_type_mask = ASTRAL_MASK(line_offset_type_bit0, line_offset_type_number_bits),

        /*!
         * Mask generated from \ref line_is_end_point_bit
         */
        line_is_end_point_mask = ASTRAL_MASK(line_is_end_point_bit, 1u),

        /*!
         * Indicates to negate the normal
         * vector to get the offset direction
         */
        line_offset_negate_normal = 0,

        /*!
         * Indicates that no offset is applied
         * from the base point.
         */
        line_offset_base_point,

        /*!
         * Indicates to use the normal
         * vector to get the offset direction
         */
        line_offset_normal,
      };

    /*!
     * \brief
     * Enumeration to describe the points of a join.
     * A shader porcesses the different join points
     * to be able to realize each of the join types.
     */
    enum join_point_t:uint32_t
      {
        /*!
         * number of bit to specify the point offset type
         */
        join_offset_type_number_bits = 2,

        /*!
         * bit0 giving the offset type
         */
        join_offset_type_bit0 = 0,

        /*!
         * if this bit is up, then specifies
         * a point that is on the side in
         * common with the edge that leaves
         * from the join
         */
        join_point_leave_bit = join_offset_type_bit0 + join_offset_type_number_bits,

        /*!
         * Number bits total needed to specify a line-point
         */
        join_total_bits = join_point_leave_bit + 1u,

        /*!
         * Mask generated from \ref join_offset_type_bit0 and \ref join_offset_type_number_bits
         */
        join_offset_type_mask = ASTRAL_MASK(join_offset_type_bit0, join_offset_type_number_bits),

        /*!
         * Mask generated from \ref join_point_leave_bit
         */
        join_point_leave_mask = ASTRAL_MASK(join_point_leave_bit, 1u),

        /*!
         * Indicates that point of the join that is where the
         * two edges of the path meet.
         */
        join_point_on_path = 0,

        /*!
         * Indicates the join point that represents the
         * point on the stroked edge boundary that is
         * common with the join.
         */
        join_point_edge_boundary,

        /*!
         * Indicates the point that neighbors \ref
         * join_point_edge_boundary used to cover the
         * geometry of the join.
         */
        join_point_beyond_boundary,
      };

    /*!
     * \brief
     * Enumeration to describe the points of a cap.
     *
     * Each cap stroke has 5 points to be able to express
     * - flat caps (to induce anti-aliasing)
     * - rounded caps
     * - butt caps
     * .
     * and the glue between the cap and edge to guarantee
     * that there is no rasterization crack between the
     * edge and cap
     */
    enum cap_point_t:uint32_t
      {
        /*!
         * number of bit to specify the point offset type
         */
        cap_offset_type_number_bits = 2,

        /*!
         * bit0 giving the offset type
         */
        cap_offset_type_bit0 = 0,

        /*!
         * this bit specifies which side of the
         * stroke the cap point is on.
         */
        cap_point_side_bit = cap_offset_type_bit0 + cap_offset_type_number_bits,

        /*!
         * Number bits total needed to specify a line-point
         */
        cap_total_bits = cap_point_side_bit + 1u,

        /*!
         * Mask generated from \ref cap_offset_type_bit0 and \ref cap_offset_type_number_bits
         */
        cap_offset_type_mask = ASTRAL_MASK(cap_offset_type_bit0, cap_offset_type_number_bits),

        /*!
         * Mask generated from \ref cap_point_side_bit
         */
        cap_point_side_mask = ASTRAL_MASK(cap_point_side_bit, 1u),

        /*!
         * Indicates the point on the original path
         */
        cap_point_path = 0,

        /*!
         * Indicates the cap point that represents the
         * point on the stroked edge boundary that is
         * common with the cap.
         */
        cap_point_edge_boundary,

        /*!
         * Indicates the point that neighbors \ref
         * cap_point_edge_boundary used to cover the
         * geometry of the cap.
         */
        cap_point_beyond_boundary,
      };

    /*!
     * \brief
     * Enumeration to specify the interpretation of
     * StrokeShader::Base::m_flags.
     */
    enum base_flags_t:uint32_t
      {
        /*!
         * If the bit of StrokeShader::start_edge_mask is up,
         * then that indicates the StrokeShader::Base is the
         * first value of an edge, i.e. it starts an edge
         */
        start_edge_bit = 0,

        /*!
         * If the bit of StrokeShader::end_edge_mask is up,
         * then that indicates the StrokeShader::Base is the
         * last value of an edge, i.e. it ends an
         * edge
         */
        end_edge_bit,

        /*!
         * If the bit of StrokeShader::start_edge_contour is up,
         * then that indicates the StrokeShader::Base is the
         * first value of a contour, i.e. it starts a contour
         */
        start_contour_bit,

        /*!
         * If the bit of StrokeShader::end_edge_contour is up,
         * then that indicates the StrokeShader::Base is the
         * last value of a contour, i.e. it ends a contour
         */
        end_contour_bit,

        /*!
         * If the bit of StrokeShader::contour_closed_bit is up,
         * then that indicates the StrokeShader::Base is on a
         * contour that is closed.
         */
        contour_closed_bit,

        /*!
         * Number of bits needed to encode \ref base_flags_t
         */
        base_flags_number_bits,

        /*!
         * Mask generated from \ref start_edge_bit
         */
        start_edge_mask = ASTRAL_MASK(start_edge_bit, 1u),

        /*!
         * Mask generated from \ref end_edge_bit
         */
        end_edge_mask = ASTRAL_MASK(end_edge_bit, 1u),

        /*!
         * Mask generated from \ref start_contour_bit
         */
        start_contour_mask = ASTRAL_MASK(start_contour_bit, 1u),

        /*!
         * Mask generated from \ref end_contour_bit
         */
        end_contour_mask = ASTRAL_MASK(end_contour_bit, 1u),

        /*!
         * Mask generated from \ref contour_closed_bit
         */
        contour_closed_mask = ASTRAL_MASK(contour_closed_bit, 1u),
      };

    /*!
     * \brief
     * Enumeration to specify the interpretation of
     * StrokeShader::Cap::m_flags.
     */
    enum cap_flags_t:uint32_t
      {
        /*!
         * Indicates that the StrokeShader::Cap is a cap that ends
         * an open coutour.
         */
        cap_end_mask = 1,
      };

    /*!
     * Enumeration to specify how StrokeShader::Cap::m_flags or
     * StrokeShader::Base::m_flags is combined with one a
     * \ref biarc_point_t, \ref line_point_t, \ref join_point_t
     * or \ref cap_point_t value into a single 32-bit value.
     */
    enum role_flags_t:uint32_t
      {
        /*!
         * Number of bits to encode the \ref biarc_point_t,
         * \ref line_point_t, \ref join_point_t or \ref
         * cap_point_t value
         */
        role_number_bits = 5,

        /*!
         * Number of bits to encode the \ref Cap::m_flags or
         * \ref Base::m_flags value
         */
        flags_number_bits = 5,

        /*!
         * First bit where to encode the \ref biarc_point_t,
         * \ref line_point_t, \ref join_point_t or \ref
         * cap_point_t value
         */
        role_bit0 = 0,

        /*!
         * First bit where to encode the \ref Cap::m_flags or
         * \ref Base::m_flags value
         */
        flags_bit0 = role_number_bits + role_bit0,

        /*!
         * First bit to mark the ID of a primitive.
         * The number of bits used is all of the
         * remaining bits.
         */
        id_bit0 = flags_number_bits + flags_bit0,
      };

    /*!
     * \brief
     * Common base class providing properties for all curves types
     * that can be stroked
     */
    class Base
    {
    public:
      /*!
       * The distance from the start of the contour to the
       * start of the curve.
       */
      float m_distance_from_contour_start;

      /*!
       * The distance from the last join to the start of the
       * curve.
       */
      float m_distance_from_edge_start;

      /*!
       * The length of the contour
       */
      float m_contour_length;

      /*!
       * The length of the edge
       */
      float m_edge_length;

      /*!
       * The length of the StrokeShader::LineSegment or
       * StrokeShader::Quadratic.
       */
      float m_primitive_length;

      /*!
       * The flags of the StrokeShader::Base,
       * see StrokeShader::base_flags_t
       */
      uint32_t m_flags;
    };

    /*!
     * \brief
     * The data necessary to stroke a join.
     * The static data for non-animated paths is packed as:
     * - [0].xy.f --> Join::m_p
     * - [0].zw.f --> Join::m_pre_p
     * - [1].xy.f --> Join::m_post_p
     * - [2].x.f  --> Join::m_distance_from_contour_start
     * - [2].y.f  --> Join::m_pre_edge_length
     * - [2].z.f  --> Join::m_post_edge_length
     * - [2].w.f  --> Join::m_contour_length
     * .
     * and the static data for animated is packed as:
     * - [0].xy.f --> input0 Join::m_p
     * - [0].zw.f --> input0 Join::m_pre_p
     * - [1].xy.f --> input0 Join::m_post_p
     * - [1].zw.f --> input1 Join::m_p
     * - [2].xy.f --> input1 Join::m_pre_p
     * - [2].zw.f --> input1 Join::m_post_p
     * - [3].x.f  --> input0 Join::m_distance_from_contour_start
     * - [3].y.f  --> input1 Join::m_distance_from_contour_start
     * - [3].z.f  --> input0 Join::m_contour_length
     * - [3].w.f  --> input1 Join::m_contour_length
     * - [4].x.f  --> input0 Join::m_pre_edge_length
     * - [4].y.f  --> input1 Join::m_pre_edge_length
     * - [4].z.f  --> input0 Join::m_post_edge_length
     * - [4].w.f  --> input1 Join::m_post_edge_length
     * .
     * The vertex data for both animated and non-animated joins is
     * packed as:
     * - Vertex::m_data[0].u --> location of static data
     * - Vertex::m_data[1].u --> the value of \ref join_point_t
     * - Vertex::m_data[2].f --> input0 Join::m_distance_from_edge_start
     * - Vertex::m_data[3].f --> input1 Join::m_distance_from_edge_start
     * .
     */
    class Join
    {
    public:
      /*!
       * The position of the join
       */
      vec2 m_p;

      /*!
       * Join::m_p - Join::m_pre_p gives the direction
       * of the tangent of the stroking entering the join;
       */
      vec2 m_pre_p;

      /*!
       * Join::m_post - Join::m_p gives the direction
       * of the tangent of the stroking leaving the join;
       */
      vec2 m_post_p;

      /*!
       * The distance from the start of the contour to the
       * join; the closing join has this value as 0.0.
       */
      float m_distance_from_contour_start;

      /*!
       * The distance from the start of the edge to the join;
       * this value is that distance for glue joins; for real
       * closing joins, the value is -2.0 and for real joins
       * that are not closing joins the value is -1.0
       */
      float m_distance_from_edge_start;

      /*!
       * The length of the contour
       */
      float m_contour_length;

      /*!
       * The length of the edge that goes into the join
       */
      float m_pre_edge_length;

      /*!
       * The lenght of the edge that leaves the join
       */
      float m_post_edge_length;

      /*!
       * Returns true if this Join is a real join
       * that connects two edges of a contour.
       */
      bool
      is_real_join(void) const
      {
        return m_distance_from_edge_start < 0.0f;
      }

      /*!
       * Returns true if this Join is a glue join
       * that is drawn to prevent rendering cracks
       * between primitives of an edge.
       */
      bool
      is_glue_join(void) const
      {
        return m_distance_from_edge_start >= 0.0f;
      }

      /*!
       * Returns true if this Join is the closing join
       * of a contour.
       */
      bool
      is_closing_join(void) const
      {
        return m_distance_from_edge_start < -1.5f;
      }
    };

    /*!
     * \brief
     * The data necessary to stroke a cap.
     * The static data for non-animated paths is packed as:
     * - [0].xy.f --> Cap::m_p
     * - [0].zw.f --> Cap::m_neighbor_p
     * - [1].x.f  --> Cap::m_contour_length
     * - [1].y.f  --> Cap::m_edge_length
     * .
     * and the static data for animated is packed as:
     * - [0].xy.f --> input0 Cap::m_p
     * - [0].zw.f --> input0 Cap::m_neighbor_p
     * - [1].xy.f --> input1 Cap::m_p
     * - [1].zw.f --> input1 Cap::m_neighbor_p
     * - [2].x.f  --> input0 Cap::m_contour_length
     * - [2].y.f  --> input1 Cap::m_contour_length
     * - [2].z.f  --> input0 Cap::m_edge_length
     * - [2].w.f  --> input1 Cap::m_edge_length
     * .
     * The vertex data for both animated and non-animated caps is
     * packed as:
     * - Vertex::m_data[0].u --> location of static data
     * - Vertex::m_data[1].u --> the value of \ref cap_point_t and Cap::m_flags
     *                           bit-packed as specified in \ref role_flags_t
     * - Vertex::m_data[2].f --> 0.0f
     * - Vertex::m_data[3].f --> 0.0f
     * .
     */
    class Cap
    {
    public:
      /*!
       * The position of the cap
       */
      vec2 m_p;

      /*!
       * This is the neighbor point coming from the
       * curve that this StrokeShader::Cap caps.
       * The tangent vector into the cap is in the direction
       * of Cap::m_p - Cap::m_neighbor_p
       */
      vec2 m_neighbor_p;

      /*!
       * The length of the contour of the cap
       */
      float m_contour_length;

      /*!
       * the legnth of the edge on which the
       * cap resides.
       */
      float m_edge_length;

      /*!
       * The flags of the StrokeShader::Cap, see \ref
       * cap_flags_t
       */
      uint32_t m_flags;
    };

    /*!
     * \brief
     * A class to represent a quadratic bezier curve from a contour.
     * The static data for non-animated paths is packed as:
     * - [0].xy.f --> Quadratic::m_pts[0].xy
     * - [0].zw.f --> Quadratic::m_pts[1].xy
     * - [1].xy.f --> Quadratic::m_pts[2].xy
     * - [1].y.f  --> Base::m_distance_from_contour_start
     * - [1].z.f  --> Base::m_primitive_length
     * - [2].x.f  --> Base::m_distance_from_edge_start
     * - [2].y.f  --> Base::m_contour_length
     * - [2].zw   --> Free
     * .
     * The vertex data for non-animated Quadratic is packed as:
     * - Vertex::m_data[0].u --> location of static data
     * - Vertex::m_data[1].u --> the value of \ref biarc_point_t and Base::m_flags
     *                           bit-packed as specified in \ref role_flags_t
     * - Vertex::m_data[2].f --> Base::m_edge_length
     * - Vertex::m_data[3].f --> Base::m_edge_length
     * .
     * The static data for animated is packed as:
     * - [0].xy.f --> input0 Quadratic::m_pts[0].xy
     * - [0].zw.f --> input1 Quadratic::m_pts[0].xy
     * - [1].xy.f --> input0 Quadratic::m_pts[1].xy
     * - [1].zw.f --> input1 Quadratic::m_pts[1].xy
     * - [2].xy.f --> input0 Quadratic::m_pts[2].xy
     * - [2].zw.f --> input1 Quadratic::m_pts[2].xy
     * - [3].x.f  --> input0 Base::m_distance_from_contour_start
     * - [3].y.f  --> input1 Base::m_distance_from_contour_start
     * - [3].z.f  --> input0 Base::m_primitive_length
     * - [3].w.f  --> input1 Base::m_primitive_length
     * - [4].x.f  --> input0 Base::m_distance_from_edge_start
     * - [4].y.f  --> input1 Base::m_distance_from_edge_start
     * - [4].z.f  --> input0 Base::m_contour_length
     * - [4].w.f  --> input1 Base::m_contour_length
     * .
     * The vertex data for animated Quadratic is packed as:
     * - Vertex::m_data[0].u --> location of static data
     * - Vertex::m_data[1].u --> the value of \ref biarc_point_t and Base::m_flags
     *                           bit-packed as specified in \ref role_flags_t
     * - Vertex::m_data[2].f --> input0 Base::m_edge_length
     * - Vertex::m_data[3].f --> input1 Base::m_edge_length
     * .
     */
    class Quadratic:public Base
    {
    public:
      /*!
       * The 3 points that define the quadratic bezier curve.
       */
      vecN<vec2, 3> m_pts;
    };

    /*!
     * \brief
     * A class to represent a line segment from a contour.
     * The static data for non-animated paths is packed as:
     * - [0].xy.f --> LineSement::m_pts[0].xy
     * - [0].zw.f --> LineSement::m_pts[1].xy
     * - [1].x.f  --> Base::m_distance_from_contour_start
     * - [1].y.f  --> Base::m_primitive_length
     * - [1].z.f  --> Base::m_distance_from_edge_start
     * - [1].w.f  --> Base::m_contour_length
     * .
     * The vertex data for non-animated LineSegment is packed as:
     * - Vertex::m_data[0].u --> location of static data
     * - Vertex::m_data[1].u --> the value of \ref line_point_t and Base::m_flags
     *                           bit-packed as specified in \ref role_flags_t
     * - Vertex::m_data[2].f --> Base::m_edge_length
     * - Vertex::m_data[3].f --> Base::m_edge_length
     * .
     * The static data for animated is packed as:
     * - [0].xy.f --> input0 LineSement::m_pts[0].xy
     * - [0].zw.f --> input0 LineSement::m_pts[1].zw
     * - [1].xy.f --> input1 LineSement::m_pts[0].xy
     * - [1].zw.f --> input1 LineSement::m_pts[1].zw
     * - [2].x.f  --> input0 LineSegment::m_distance_from_contour_start
     * - [2].y.f  --> input1 LineSegment::m_distance_from_contour_start
     * - [2].z.f  --> input0 LineSegment::m_primitive_length
     * - [2].w.f  --> input1 LineSegment::m_primitive_length
     * - [3].x.f  --> input0 LineSegment::m_distance_from_edge_start
     * - [3].y.f  --> input1 LineSegment::m_distance_from_edge_start
     * - [3].z.f  --> input0 LineSegment::m_contour_length
     * - [3].w.f  --> input1 LineSegment::m_contour_length
     * .
     * The vertex data for animated LineSegment is packed as:
     * - Vertex::m_data[0].u --> location of static data
     * - Vertex::m_data[1].u --> the value of \ref line_point_t and Base::m_flags
     *                           bit-packed as specified in \ref role_flags_t
     * - Vertex::m_data[2].f --> input0 Base::m_edge_length
     * - Vertex::m_data[3].f --> input1 Base::m_edge_length
     * .
     */
    class LineSegment:public Base
    {
    public:
      /*!
       * The 2 points that define the line segment.
       */
      vecN<vec2, 2> m_pts;
    };

    /*!
     * \brief
     * Class to specify contours of a path for the purpose
     * of realizing a StrokeShader::CookedData value.
     */
    class RawData
    {
    public:
      RawData(void);

      ~RawData();

      /*!
       * Add the curves, joins and caps (if closed) from an array
       * of curves. For each astral::ContourCurve in the passed array,
       * ContourCurve::type() must be ContourCurve::line_segment or
       * astral::ContourCurve::quadratic_bezier.
       * \param is_closed if true add a join between the last curve
       *                  and first curve; otherwise add caps
       * \param curves array of curves to add
       */
      RawData&
      add_contour(bool is_closed, c_array<const ContourCurve> curves);

      /*!
       * Add the cap for a an empty contour
       */
      RawData&
      add_point_cap(vec2 p);

      /*!
       * The list of StrokeShader::LineSegment values added
       * so far.
       */
      c_array<const LineSegment>
      line_segments(void) const
      {
        return make_c_array(m_line_segments);
      }

      /*!
       * The list of StrokeShader::Quadratic values added
       * so far.
       */
      c_array<const Quadratic>
      biarc_curves(void) const
      {
        return make_c_array(m_biarc_curves);
      }

      /*!
       * The list of StrokeShader::Join values added
       * so far.
       */
      c_array<const Join>
      joins(void) const
      {
        return make_c_array(m_joins);
      }

      /*!
       * The list of StrokeShader::Join values added
       * so far that are for glue.
       */
      c_array<const Join>
      glue(void) const
      {
        return make_c_array(m_glue);
      }

      /*!
       * The list of StrokeShader::Join values added
       * so far that are for bevel glue.
       */
      c_array<const Join>
      glue_cusp(void) const
      {
        return make_c_array(m_glue_cusp);
      }

      /*!
       * The list of StrokeShader::Cap values added
       * so far.
       */
      c_array<const Cap>
      caps(void) const
      {
        return make_c_array(m_caps);
      }

    private:
      friend class StrokeShader;
      friend class astral::detail::StrokeDataHierarchy;

      class SourceInfo
      {
      public:
        unsigned int m_contour_id;
        unsigned int m_edge_id;
        unsigned int m_sub_edge_id;
      };

      class Info
      {
      public:
        Info(enum primitive_type_t tp, unsigned int ID,
             const SourceInfo &source_info):
          m_type(tp),
          m_id(ID),
          m_source_info(source_info)
        {}

        enum primitive_type_t m_type;
        unsigned int m_id;
        SourceInfo m_source_info;
      };

      RawData&
      add_contour_implement(bool is_closed,
                            c_array<const ContourCurve> curves,
                            c_array<const ContourCurve> pair_curves,
                            std::vector<Join> *dst_inner_glue);

      SourceInfo m_current;
      std::vector<LineSegment> m_line_segments;
      std::vector<Quadratic> m_biarc_curves;
      std::vector<Join> m_glue, m_joins, m_glue_cusp;
      std::vector<Cap> m_caps;
      std::vector<Info> m_info;
      BoundingBox<float> m_bb;
    };

    /*!
     * \brief
     * Analoguos to StrokeShader::RawData, but for animated
     * paths
     */
    class RawAnimatedData
    {
    public:
      RawAnimatedData(void)
      {
      }

      /*!
       * Add the curves, joins and caps (if closed) from an array
       * pair of curves. For each astral::ContourCurve in the passed,
       * array ContourCurve::type() must be ContourCurve::line_segment
       * or astral::ContourCurve::quadratic_bezier.
       * \param is_closed if true, indicates that the contour
       *                  is closed and no caps will be added;
       *                  if false, then caps will be added
       * \param curves_start the contour at the start of the
       *                     animation, this must be identical
       *                     in size to curves_end
       * \param curves_end the contour at the end of the
       *                   animation, this must be identical
       *                   in size to curves_start
       */
      RawAnimatedData&
      add_contour(bool is_closed,
                  c_array<const ContourCurve> curves_start,
                  c_array<const ContourCurve> curves_end);

      /*!
       * Add the cap for a an empty contour
       */
      RawAnimatedData&
      add_point_cap(vec2 start, vec2 end);

      /*!
       * The StrokeShader::RawData value associated
       * to the animated path at the start.
       */
      const RawData&
      start(void) const
      {
        return m_start;
      }

      /*!
       * The StrokeShader::RawData value associated
       * to the animated path at the end.
       */
      const RawData&
      end(void) const
      {
        return m_end;
      }

      /*!
       * The inner glue values for the animated path
       * at the start
       */
      c_array<const Join>
      start_inner_glue(void) const
      {
        return make_c_array(m_start_inner_glue);
      }

      /*!
       * The inner glue values for the animated path
       * at the start
       */
      c_array<const Join>
      end_inner_glue(void) const
      {
        return make_c_array(m_end_inner_glue);
      }

    private:
      friend class astral::detail::StrokeDataHierarchy;

      RawData m_start, m_end;
      std::vector<Join> m_start_inner_glue;
      std::vector<Join> m_end_inner_glue;
    };

    /*!
     * \brief
     * Holds the astral::StaticData and astral::VertexData
     * created from a StrokeShader::RawData. In constast to
     * StrokeShader::CookedData, a StrokeShader::SimpleCookedData
     * does not hold hierarchy. Its main purpose is to be
     * used when a caller knows that ALL primitives (or nearly)
     * all are to be used; the most common case is for stroking
     * without generating a mask.
     */
    class SimpleCookedData
    {
    public:
      /*!
       * Specifies which shader class to use: static
       * path shading or animated path shading.
       */
      enum path_shader_t
      path_shader(void) const
      {
        return m_for_animated_path ?
          animated_path_shader :
          static_path_shader;
      }

      /*!
       * astral::VertexData for the named primitive type
       */
      const VertexData&
      vertex_data(enum primitive_type_t P) const
      {
        return *m_vertex_data[P];
      }

    private:
      friend class StrokeShader;

      void
      clear(void);

      bool m_for_animated_path;

      reference_counted_ptr<const StaticData> m_static_data;
      vecN<reference_counted_ptr<const VertexData>, number_primitive_types> m_vertex_data;

      uint32_t m_segments_offset;
      uint32_t m_biarc_curves_offset;
      uint32_t m_joins_offset;
      uint32_t m_glue_offset;
      uint32_t m_glue_cusp_offset;
      uint32_t m_inner_glue_offset;
      uint32_t m_caps_offset;
    };

    /*!
     * \brief
     * Holds the astral::StaticData and astral::VertexData
     * created from a StrokeShader::RawData. In addition,
     * it holds a hierarchy that astral::StrokeQuery uses.
     */
    class CookedData
    {
    public:
      /*!
       * Default ctor, initializes as empty.
       */
      CookedData(void);

      /*!
       * Copy ctor; not a deep copy.
       */
      CookedData(const CookedData &obj);

      ~CookedData();

      /*!
       * assignment operator; not a deep copy.
       */
      CookedData&
      operator=(const CookedData &rhs);

      /*!
       * Return this CookedData as a SimpleCookedData
       */
      const SimpleCookedData&
      simple_data(void) const
      {
        return m_base;
      }

      /*!
       * Specifies which shader class to use: static
       * path shading or animated path shading.
       */
      enum path_shader_t
      path_shader(void) const
      {
        return m_base.path_shader();
      }

      /*!
       * astral::VertexData for the named primitive type
       */
      const VertexData&
      vertex_data(enum primitive_type_t P) const
      {
        return m_base.vertex_data(P);
      }

    private:
      friend class StrokeShader;
      friend class StrokeQuery;

      void
      clear(void);

      SimpleCookedData m_base;
      unsigned int m_hierarchy_size;
      reference_counted_ptr<detail::StrokeDataHierarchy> m_hierarchy;
    };

    /*!
     * Fill in the fields of a StrokeShader::CookedData for the
     * purpose of stroking a path.
     * \param engine the astral::RenderEngine used
     * \param input specifies the path geometry
     * \param output provides the data to feed the renderer
     */
    static
    void
    create_render_data(RenderEngine &engine, const RawData &input, CookedData *output);

    /*!
     * Fill in the fields of a StrokeShader::CookedData for the
     * purpose of stroking an animated path
     * \param engine the astral::RenderEngine used
     * \param input specifies the path geometry
     * \param output provides the data to feed the renderer
     */
    static
    void
    create_render_data(RenderEngine &engine, const RawAnimatedData &input, CookedData *output);

    /*!
     * Fill in the fields of a StrokeShader::CookedData for the
     * purpose of stroking a path.
     * \param engine the astral::RenderEngine used
     * \param input specifies the path geometry
     * \param output provides the data to feed the renderer
     */
    static
    void
    create_render_data(RenderEngine &engine, const RawData &input, SimpleCookedData *output);

    /*!
     * Fill in the fields of a StrokeShader::CookedData for the
     * purpose of stroking an animated path
     * \param engine the astral::RenderEngine used
     * \param input specifies the path geometry
     * \param output provides the data to feed the renderer
     */
    static
    void
    create_render_data(RenderEngine &engine, const RawAnimatedData &input, SimpleCookedData *output);

  private:
    friend class StrokeQuery;
    friend class astral::detail::StrokeDataHierarchy;

    class VertexIndexRoles;
    class Packer;
    typedef vecN<std::vector<unsigned int>, number_primitive_types> Ordering;

    static
    void
    create_static_render_data(RenderEngine &engine, const RawData &input,
                              const Ordering *ordering, SimpleCookedData *output);

    static
    void
    create_vertex_render_data(RenderEngine &engine, const RawData &input0, const RawData &input1,
                              const Ordering *ordering, SimpleCookedData *output);

    static
    void
    create_static_render_data(RenderEngine &engine, const RawAnimatedData &input,
                              const Ordering *ordering, SimpleCookedData *output);
  };

  /*!
   * Template class to define astral::MaskStrokeShader and
   * astral::DirectStrokeShader.
   */
  template<typename T>
  class StrokeShaderT:
    public StrokeShader,
    public reference_counted<StrokeShaderT<T>>::non_concurrent
  {
  public:
    /*!
     * \brief
     * StrokeShader::ItemShaderSet embodies a family of
     * shader values to stroke all primitive types of
     * either an animated path or static path with a specific
     * cap style.
     */
    class ItemShaderSet
    {
    public:
      /*!
       * The shader to handle StrokeShader::LineSegment
       */
      reference_counted_ptr<const T> m_line_segment_shader;

      /*!
       * The shader to handle StrokeShader::Quadratic data where
       * the quadratic curves are approximated to biarcs
       * by the shader.
       */
      reference_counted_ptr<const T> m_biarc_curve_shader;

      /*!
       * For each astral::join_t a shader to stroke a join, index as
       * \ref m_join_shaders[astral::join_t].
       */
      vecN<reference_counted_ptr<const T>, number_join_t> m_join_shaders;

      /*!
       * The shader for handling StrokeShader::inner_glue_primitive
       * and the inner side of StrokeShader::glue_primitive
       * These are needed to prevent gaps in the rendering between
       * neigboring curves of an edge for animation and/or when the
       * stroking radius is large and a curve inverts.
       */
      reference_counted_ptr<const T> m_inner_glue_shader;

      /*!
       * Shaders for drawing the cappers for StrokeShader::LineSegment
       * values, indexed by [\ref capper_shader_t]
       */
      vecN<reference_counted_ptr<const T>, number_capper_shader> m_line_capper_shaders;

      /*!
       * Shaders for drawing the cappers for StrokeShader::Quadratic
       * values, indexed by [\ref capper_shader_t]
       */
      vecN<reference_counted_ptr<const T>, number_capper_shader> m_quadratic_capper_shaders;

      /*!
       * shader used to render the caps at the start and end of
       * open contours.
       */
      reference_counted_ptr<const T> m_cap_shader;
    };

    /*!
     * \brief
     * StrokeShader::ShaderSet embodies a set of shaders to
     * stroke static and animated paths with a specific cap
     * style under any join style
     */
    class ShaderSet
    {
    public:
      /*!
       * Shaders to stroke static and animated paths, indexed
       * by StrokeShader::path_shader_t
       */
      vecN<ItemShaderSet, path_shader_count> m_subset;
    };

    /*!
     * \brief
     * StrokeShader::ShaderSetFamily is an array of
     * StrokeShader::ShaderSet values indexed by
     * astral::cap_t, i.e. ShaderSetFamily[astral::cap_t]
     * gives the StrokeShader::ShaderSet to stroke with
     * the named cap style.
     */
    typedef vecN<ShaderSet, number_cap_t> ShaderSetFamily;

    /*!
     * Ctor.
     * \param shader_set_family the family of shaders that the created
     *                              astral::StrokeShader will use
     */
    static
    reference_counted_ptr<StrokeShaderT>
    create(const ShaderSetFamily &shader_set_family)
    {
      return ASTRALnew StrokeShaderT(shader_set_family);
    }

    /*!
     * Gives an astral::cap_t, returns the \ref ShaderSet
     * for stroking with that cap style
     */
    const ShaderSet&
    shader_set(enum cap_t c) const
    {
      return m_shader_set_family[c];
    }

    /*!
     * Returns the StrokeShader::ShaderSetFamily backing
     * this astral::StrokeShader
     */
    const ShaderSetFamily&
    shader_set_family(void)
    {
      return m_shader_set_family;
    }

    /*!
     * The unique ID for this astral::StrokeShaderT. For each
     * class T, the value of unique_id() is essentially just
     * the number of astral::StrokeShaderT<T> objects created
     * before it. In particular, it can be used as index for
     * an array to hold values instead of a map keyed by pointers
     * to StrokeShaderT<T>.
     */
    unsigned int
    unique_id(void) const
    {
      return m_unique_id;
    }

  private:
    explicit
    StrokeShaderT(const ShaderSetFamily &shader_set_family):
      m_shader_set_family(shader_set_family),
      m_unique_id(sm_unique_id_counter++)
    {
    }

    ShaderSetFamily m_shader_set_family;
    unsigned int m_unique_id;

    static unsigned int sm_unique_id_counter;
  };

  template<typename T>
  unsigned int StrokeShaderT<T>::sm_unique_id_counter = 0u;

  /*!
   * Represents the shaders needed to generate the mask
   * to stroke a path.
   */
  typedef StrokeShaderT<MaskItemShader> MaskStrokeShader;

  /*!
   * Represents the shaders needed to directly stroke
   * a path; drawing directly WILL overdraw and should
   * not be used when the material applied to the path
   * is transparent (unless one wants to have that
   * overdraw visible).
   */
  typedef StrokeShaderT<ColorItemShader> DirectStrokeShader;
}

#endif

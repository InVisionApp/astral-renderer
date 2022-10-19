/*!
 * \file stroke_shader_gl3.hpp
 * \brief file stroke_shader_gl3.hpp
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

#ifndef ASTRAL_STROKE_SHADER_GL3_HPP
#define ASTRAL_STROKE_SHADER_GL3_HPP

#include <astral/renderer/shader/stroke_shader.hpp>
#include <astral/renderer/gl3/item_shader_gl3.hpp>

namespace astral
{
  namespace gl
  {

/*!\addtogroup gl
 * @{
 */
    /*!
     * Class that defines all the shaders that creates sub-shaders
     * to fill in the fields of \ref StrokeShader.
     */
    class StrokeShaderGL3
    {
    public:
      /*!
       * The GL3 backend provides two \ref StrokeShaderGL3
       * objects, this enumeration allows one to specify
       * which.
       */
      enum type_t:uint32_t
        {
          /*!
           * The shaders only perform stroking and
           * do NOT emit distance values
           */
          stroking_only,

          /*!
           * The shaders perform stroking and emit
           * distance values along the contour to
           * enable effects and shader based dashed
           * stroking
           */
          emit_distances,
        };

      /*!
       * Enumeration for choosing a shader to draw cappers
       */
      enum capper_primitive_t:uint32_t
        {
          /*!
           * Draw cappers of line segments
           */
          line_segment_capper = 0,

          /*!
           * Draw cappers of quadratic curves
           */
          quadratic_capper,

          capper_primitive_count
        };

      /*!
       * Bit masks that affect the stroke shader
       * created by create_stroke_shader()
       */
      enum create_stroke_shader_flags_t:uint32_t
        {
          /*!
           * If this bit is up, fill StrokeShader::ItemShaderSet::m_cap_shader
           * for every cap style. If down, only fill it for cap_flat.
           */
          include_cap_shaders = 1,

          /*!
           * if this bit is up, set StrokeShader::ItemShaderSet::m_line_capper_shaders
           * and StrokeShader::ItemShaderSet::m_quadratic_capper_shaders
           */
          include_capper_shaders = 2,
        };

      /*!
       * Fills the fields of an astral::MaskStrokeShader, creating
       * sub-shaders of the astral::gl::ItemShaderBackendGL3
       * values of this. It is required that for each non-null
       * astral::gl::ItemShaderBackendGL3 the shader code is for
       * mask shading.
       * \param flags what shaders to generate, see \ref
       *              create_stroke_shader_flags_t
       */
      reference_counted_ptr<MaskStrokeShader>
      create_mask_stroke_shader(uint32_t flags) const;

      /*!
       * Fills the fields of an astral::DirectStrokeShader, creating
       * sub-shaders of the astral::gl::ItemShaderBackendGL3
       * values of this. It is required that for each non-null
       * astral::gl::ItemShaderBackendGL3 the shader code is for
       * direct shading.
       * \param flags what shaders to generate, see \ref
       *              create_stroke_shader_flags_t
       * \param shaders_emit_transparent_fragments if true the shaders may emit
       *                                           transparent fragments
       */
      reference_counted_ptr<DirectStrokeShader>
      create_direct_stroke_shader(uint32_t flags, bool shaders_emit_transparent_fragments = false) const;

      /*!
       * Generate a StrokeShaderGL3 from this StrokeShaderGL3 where
       * the shaders of the return value are the results of
       * ItemShaderBackendGL3::color_shader_from_mask_shader() applied
       * to each non-null shader of this. It is required that for each
       * non-null astral::gl::ItemShaderBackendGL3 of this that the shader
       * code is for mask shading.
       */
      StrokeShaderGL3
      color_shader_from_mask_shader(void) const;

      /*!
       * Specifies if the shaders do or do not emit
       * distance values.
       */
      enum type_t m_type;

      /*!
       * If this is true, the \ref m_line, \ref m_biarc_curve
       * and \ref m_join shaders have sub-shaders for each cap
       * style.
       */
      bool m_per_cap_shading;

      /*!
       * Shader that performs line stroking. The shader
       * gives the following symbols and varyings.
       *
       * - Always present
       *   - Varyings, computed by astral_vert_shader().
       *     - astral_chain_stroke_radius: stroking radius, taken directly from item data (flat)
       *     - astral_chain_stroke_perp_distance_to_curve : perpindicular distance to segment (smooth)
       *   - Vertex Shader Symbols, computed by astral_vert_shader()
       *     - astral_chain_stroke_edge_start_x : x-coordinate of the starting point of edge in logical coordinates
       *     - astral_chain_stroke_edge_start_y : y-coordinate of the starting point of edge in logical coordinates
       *     - astral_chain_stroke_edge_end_x : x-coordinate of the ending point of edge in logical coordinates
       *     - astral_chain_stroke_edge_end_y : y-coordinate of the ending point of edge in logical coordinates
       *   - Fragment Shader Symbols, computed by astral_pre_frag_shader()
       *     - astral_chain_stroke_perp_distance_to_curve_gradient_x
       *     - astral_chain_stroke_perp_distance_to_curve_gradient_y
       * - Present only when \ref m_type is \ref emit_distances
       *   - Varyings, computed by astral_vert_shader()
       *     - astral_chain_stroke_distance_along_contour_start: distance at start of the line segment (flat)
       *     - astral_chain_stroke_distance_along_contour_end: distance at end of the line segment     (flat)
       *     - astral_chain_stroke_distance_along_contour : distance along contour from contour start interpolated (smooth)
       *     - astral_chain_stroke_boundary_flags: flags descirbing what to do about caps (uint)
       *   - Vertex Shader Symbols, computed by astral_vert_shader()
       *     - astral_chain_stroke_distance_from_start_contour_minus_from_start_edge : difference between distance from start of contour to start of edge
       *     - astral_chain_stroke_contour_length : length of contour
       *     - astral_chain_stroke_edge_length : length of edge
       *   - Fragment Shader Symbols, computed by astral_pre_frag_shader()
       *     - astral_chain_stroke_distance_along_contour_gradient_x
       *     - astral_chain_stroke_distance_along_contour_gradient_y
       */
      reference_counted_ptr<const ItemShaderBackendGL3> m_line;

      /*!
       * Shader that performs bi-arc stroking. The shader
       * gives the following symbols and varyings.
       *
       * - Always present
       *   - Varyings, computed by astral_vert_shader().
       *     - astral_chain_stroke_radius: stroking radius, taken directly from item data (flat)
       *     - astral_chain_stroke_arc_radius: radius of arc of arc-approximation to curve, negative value indicates curve approximated by a line (flat)
       *   - Vertex Shader Symbols, computed by astral_vert_shader()
       *     - astral_chain_stroke_start_pt_x : x-coordinate of start point of quadratic bezier curve
       *     - astral_chain_stroke_start_pt_y : y-coordinate of start point of quadratic bezier curve
       *     - astral_chain_stroke_control_pt_x : x-coordinate of control point of quadratic bezier curve
       *     - astral_chain_stroke_control_pt_y : y-coordinate of control point of quadratic bezier curve
       *     - astral_chain_stroke_end_pt_x : x-coordinate of end point of quadratic bezier curve
       *     - astral_chain_stroke_end_pt_y : y-coordinate of end point of quadratic bezier curve
       *     - astral_chain_stroke_arc_center_x : x-coordinate of center of arc of arc-approximation to curve
       *     - astral_chain_stroke_arc_center_y : y-coordinate of center of arc of arc-approximation to curve
       *   - Fragment Shader Symbols, computed by astral_pre_frag_shader()
       *     - astral_chain_stroke_perp_distance_to_curve : perpindicular distance to curve
       *     - astral_chain_stroke_perp_distance_to_curve_gradient_x
       *     - astral_chain_stroke_perp_distance_to_curve_gradient_y
       * - Present only when \ref m_type is \ref emit_distances
       *   - Varyings, computed by astral_vert_shader().
       *     - astral_chain_stroke_distance_along_contour_start: distance at start of the line segment (flat)
       *     - astral_chain_stroke_distance_along_contour_end: distance at end of the line segment     (flat)
       *     - astral_chain_stroke_boundary_flags: flags descirbing what to do about caps (uint)
       *   - Vertex Shader Symbols, computed by astral_vert_shader().
       *     - astral_chain_stroke_distance_from_start_contour_minus_from_start_edge : difference between distance from start of contour to start of edge
       *     - astral_chain_stroke_contour_length : length of contour
       *     - astral_chain_stroke_edge_length : length of edge
       *   - Fragment Shader Symbols, computed by astral_pre_frag_shader()
       *     - astral_chain_stroke_distance_along_contour : distance along contour from contour start
       *     - astral_chain_stroke_distance_along_contour_gradient_x
       *     - astral_chain_stroke_distance_along_contour_gradient_y
       */
      reference_counted_ptr<const ItemShaderBackendGL3> m_biarc_curve;

      /*!
       * Shaders that performs join stroking, enumerated by astral::join_t.
       * The shaders give the following symbols and varyings.
       *
       * - \ref join_miter and \ref join_bevel shader
       *   - Always present
       *     - Varyings (none)
       *     - Vertex Shader Symbols, computed by astral_pre_vert_shader()
       *       - astral_chain_stroke_radius : stroking radius, taken directly from item data
       *       - Position \f$ P\f$ of the join
       *         - astral_chain_stroke_position_x : x-coordinate of \f$ P \f$
       *         - astral_chain_stroke_position_y : y-coordinate of \f$ P \f$
       *       - Vector \f$ V \f$ where the position of the current vertex is given by \f$ P + r * V\f$ where \f$r\f$ is astral_chain_stroke_radius
       *         - astral_chain_stroke_offset_vector_x : x-coordinate of \f$ V \f$
       *         - astral_chain_stroke_offset_vector_y : y-coordinate of \f$ V \f$
       *     - Fragment Shader Symbols (none)
       *   - Present only when \ref m_type is \ref emit_distances
       *     - Varyings (none)
       *     - Vertex Shader Symbols, computed by astral_pre_vert_shader()
       *       - astral_chain_stroke_distance_along_contour : distance along contour from contour start
       *       - astral_chain_stroke_contour_length : length of contour
       *       - astral_chain_stroke_edge_into_join_length : length of edge going into join
       *       - astral_chain_stroke_edge_leaving_join_length : length of edge leaving join
       *     - Fragment Shader Symbols (none)
       * - \ref join_rounded shader
       *   - Always present
       *     - Varyings, computed by astral_pre_vert_shader()
       *       - a vec2 value \f$ V \f$. The fragment is inside the rounded join is \f$ ||V|| < 1 \f$
       *         - astral_chain_stroke_offset_vector_x : x-coordinate of vector \f$ V \f$ (smooth)
       *         - astral_chain_stroke_offset_vector_y : y-coordinate of vector \f$ V \f$ (smooth)
       *     - Vertex Shader Symbols, computed by astral_pre_vert_shader()
       *         - astral_chain_stroke_radius : stroking radius, taken directly from item data
       *         - Position \f$ P \f$ of the join
       *           - astral_chain_stroke_position_x : x-coordinate of \f$ P \f$
       *           - astral_chain_stroke_position_y : y-coordinate of \f$ P \f$
       *     - Fragment Shader Symbols (none)
       *   - Present only when \ref m_type is \ref emit_distances
       *     - Varyings (no additional symbols)
       *     - Vertex Shader Symbols, computed by astral_pre_vert_shader()
       *       - astral_chain_stroke_distance_along_contour : distance along contour from contour start
       *       - astral_chain_stroke_contour_length : length of contour
       *       - astral_chain_stroke_edge_into_join_length : length of edge going into join
       *       - astral_chain_stroke_edge_leaving_join_length : length of edge leaving join
       *     - Fragment Shader Symbols (none)
       */
      vecN<reference_counted_ptr<const ItemShaderBackendGL3>, number_join_t> m_join;

      /*!
       * Shader that performs square cap stroking. The shader gives the
       * following symbols and varyings.
       *
       * - Always present
       *   - Varyings, computed by astral_pre_vert_shader().
       *   - Vertex Shader Symbols, computed by astral_pre_vert_shader()
       *     - astral_chain_stroke_radius : stroking radius, taken directly from item data
       *     - Position \f$ P \f$ of the cap
       *       - astral_chain_stroke_position_x : x-coordinate of \f$ P \f$
       *       - astral_chain_stroke_position_y : y-coordinate of \f$ P \f$
       *     - Vector \f$ V \f$ where the position of the current vertex is given by \f$ P + r * V\f$ where \f$r\f$ is astral_chain_stroke_radius
       *       - astral_chain_stroke_offset_vector_x : x-coordinate of \f$ V \f$
       *       - astral_chain_stroke_offset_vector_y : y-coordinate of \f$ V \f$
       *   - Fragment Shader Symbols (none)
       * - Present only when \ref m_type is \ref emit_distances
       *   - Varyings (none)
       *   - Vertex Shader Symbols, computed by astral_pre_vert_shader().
       *     - astral_chain_stroke_distance_along_contour : distance along contour from contour start
       *     - astral_chain_stroke_contour_length : length of contour
       *     - astral_chain_stroke_edge_length : length of edge on which the cap resides
       *   - Fragment Shader Symbols (none)
       */
      reference_counted_ptr<const ItemShaderBackendGL3> m_square_cap;

      /*!
       * Shader that performs rounded cap stroking. The shader
       * gives the following symbols and varyings.
       *
       * - Always present
       *   - Varyings, computed by astral_pre_vert_shader().
       *      - a vec2 value \f$ V \f$. The fragment is inside the rounded cap is \f$ ||V|| < 1 \f$
       *      - astral_chain_stroke_offset_vector_x : x-coordinate of vector \f$ V \f$ (smooth)
       *      - astral_chain_stroke_offset_vector_y : y-coordinate of vector \f$ V \f$ (smooth)
       *   - Vertex Shader Symbols, computed by astral_pre_vert_shader()
       *     - astral_chain_stroke_radius : stroking radius, taken directly from item data
       *     - Position \f$ P \f$ of the cap
       *       - astral_chain_stroke_position_x : x-coordinate of \f$ P \f$
       *       - astral_chain_stroke_position_y : y-coordinate of \f$ P \f$
       *   - Fragment Shader Symbols (none)
       * - Present only when \ref m_type is \ref emit_distances
       *   - Varyings, computed by astral_pre_vert_shader().
       *   - Vertex Shader Symbols, computed by astral_pre_vert_shader().
       *     - astral_chain_stroke_distance_along_contour : distance along contour from contour start
       *     - astral_chain_stroke_contour_length : length of contour
       *     - astral_chain_stroke_edge_length : length of edge on which the cap resides
       *   - Fragment Shader Symbols (none)
       */
      reference_counted_ptr<const ItemShaderBackendGL3> m_rounded_cap;

      /*!
       * Shaders to draw cappers, enumerated as [capper_primitive_t][cap_t] where
       * - capper_primitive_t is one of \ref line_segment_capper, \ref quadratic_capper
       * - cap_t is one of \ref cap_rounded, \ref cap_square
       * .
       * These shaders are nullptr if \ref m_type is not \ref emit_distances.
       * The shaders give the following public symbols and varyings.
       *
       * - Varyings, computed by astral_pre_vert_shader().
       *   - astral_chain_stroke_radius : stroking radius (flat)
       * - Varyings, computed by astral_vert_shader().
       *   - vector \f$ V \f$ from the starting point along the point of the cap to the fragment,
       *     this value is used in the fragment shader to compute coverage
       *     -  astral_chain_stroke_pt_x : x-coordinate of \f$ V \f$
       *     -  astral_chain_stroke_pt_y : y-coordinate of \f$ V \f$
       * - Vertex Shader Symbols, computed by astral_pre_vert_shader()
       *   - astral_chain_stroke_capper_result information about capper, of GLSL type astral_stroke_compute_stroke_location_result
       * - Fragment Shader Symbols (none)
       */
      vecN<vecN<reference_counted_ptr<const ItemShaderBackendGL3>, number_cap_t>, capper_primitive_count> m_cappers;
    };

/*! @} */
  }
}

#endif

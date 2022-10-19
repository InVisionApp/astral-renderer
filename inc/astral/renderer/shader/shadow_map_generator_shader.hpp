/*!
 * \file shadow_map_generator_shader.hpp
 * \brief file shadow_map_generator_shader.hpp
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

#ifndef ASTRAL_SHADOW_MAP_GENERATOR_SHADER_HPP
#define ASTRAL_SHADOW_MAP_GENERATOR_SHADER_HPP

#include <utility>
#include <astral/contour_curve.hpp>
#include <astral/renderer/shader/item_shader.hpp>
#include <astral/renderer/shader/item_data.hpp>
#include <astral/renderer/shader/fill_stc_shader.hpp>
#include <astral/renderer/shadow_map.hpp>

namespace astral
{
  /*!
   * An astral::ShadowMapGeneratorShader is an astral::ItemShader
   * that is used to render to an astral::ShadowMap the
   * occlude depth values. The shaders assume that the current
   * transformation maps from path coordinates to normalized
   * coordinates of the shadow map, [-1, 1]x[-1, 1] and that the
   * light is at (0, 0) in these nomralized coordinates.
   *
   * Special Notes:
   *    - The funky packing of a ShadowMap allows for
   *      the geometry to only be sent twice down the pipeline
   *      and the shader expands it (correctly) to the texels
   *      hit.
   *    - The shaders use the fuzz data so that they can easily
   *      generate rects.
   *    - The shaders should never go outside of the texel
   *      region of the destination shadow map
   *    - We can avoid shader changes (just as in STC) by
   *      running all line invocations and then all conic
   *      shader invocations.
   *    - The shaders may render directly to the backing surface
   *      of the astral::ShadowMapAtlasBacking.
   */
  class ShadowMapGeneratorShader
  {
  public:
    /*!
     * \brief
     * Enumeration giving the offsets for how the item data is packed
     */
    enum item_data_packing:uint32_t
      {
        /*!
         * Number of item datas consumed.
         */
        item_data_size = 1,
      };

    /*!
     * Enumeration to specify which pair of sides a shader
     * generates.
     */
    enum side_pair_t:uint32_t
      {
        /*!
         * Indicates computing the depth values on the min-x
         * and max-x sides
         */
        x_sides = 0,

        /*!
         * Indicates computing the depth values on the min-y
         * and max-y sides
         */
        y_sides,

        number_side_pair
      };

    /*!
     * Enumeration to describe what primitive type a
     * shader handles.
     */
    enum primitive_type_t
      {
        /*!
         * Shader to render to depth render target for handling
         * FillSTCShader::LineSegment values. It consumes the
         * same attribute (and static) data as the shader
         * FillSTC::m_shaders[FillSTCShader::pass_contour_fuzz]
         */
        line_segment_primitive,

        /*!
         * Shader to render to depth render target for handling
         * FillSTCShader::ConicTriangle values. It consumes the
         * same attribute (and static) data as the shader
         * FillSTC::m_shaders[FillSTCShader::pass_conic_triangle_fuzz]
         */
        conic_triangle_primitive,

        number_primitive_types,
      };

    /*!
     * Shader that clears the depth values (to 1.0), consumes
     * the same attribute and item data as the shader of an
     * astral::DynamicRectShader.
     */
    reference_counted_ptr<const ShadowMapItemShader> m_clear_shader;

    /*!
     * Returns a reference for the shader to handle the named
     * \ref primitive_type_t for the named \ref side_pair_t
     */
    reference_counted_ptr<const ShadowMapItemShader>&
    shader(enum primitive_type_t P, enum side_pair_t S)
    {
      return m_shaders[P][S];
    }

    /*!
     * Returns the shader to handle the named \ref primitive_type_t
     * for the named \ref side_pair_t
     */
    const reference_counted_ptr<const ShadowMapItemShader>&
    shader(enum primitive_type_t P, enum side_pair_t S) const
    {
      return m_shaders[P][S];
    }

    /*!
     * Pack item data that all shaders except \ref m_clear_shader
     * of an astral::ShadowMapGeneratorShader consume.
     *
     * ISSUE: Operating on the fuzz data only means that false
     *        edges from filling can generate occluders. It
     *        might be worthwhile for mask to have a channel
     *        indicating a boundary texel and for m_line_shader
     *        and m_conic_triangle_shader to sample from the
     *        mask and use that to remove false edges.
     *
     * ISSUE: this only support fill boundary, we may wish
     *        to also support generating shadowing from
     *        stroking or even custom shaders. For stroking,
     *        we could still use the STC data, but we approximate
     *        in the vertex shader the quadratic or conic curves
     *        by biarcs (note that for conic curves, this just
     *        means we ignore the conic weight essentially)
     *        and run the computation in the fragment shader
     *        for the arcs and line segments inflated. Stroking
     *        radius that is so large it causes arc inversion
     *        may be problematic to handle correctly.
     *
     * \param t time interpolate for animation
     * \param dst location to which to write the packed values
     */
    static
    void
    pack_item_data(float t, c_array<gvec4> dst)
    {
      ASTRALassert(dst.size() == item_data_size);
      dst[0].x().f = t;
    }

  private:
    vecN<vecN<reference_counted_ptr<const ShadowMapItemShader>, number_side_pair>, number_primitive_types> m_shaders;
  };
}

#endif

/*!
 * \file item_shader_gl3.hpp
 * \brief file item_shader_gl3.hpp
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

#ifndef ASTRAL_ITEM_SHADER_GL3_HPP
#define ASTRAL_ITEM_SHADER_GL3_HPP

#include <astral/renderer/shader/item_shader.hpp>
#include <astral/renderer/gl3/shader_gl3_detail.hpp>
#include <astral/renderer/gl3/named_shader_list_gl3.hpp>
#include <astral/util/gl/gl_shader_source.hpp>
#include <astral/util/gl/gl_shader_varyings.hpp>
#include <astral/util/gl/gl_shader_symbol_list.hpp>

namespace astral
{
  namespace gl
  {
    class RenderEngineGL3;

/*!\addtogroup gl
 * @{
 */

    /*!
     * \brief
     * An astral::gl::ItemShaderBackendGL3 is the GL-backend implementation for
     * astral::ItemShaderBackend. An astral::gl::ItemShaderBackendGL3 consists of
     * - a vertex shader
     * - a fragment shader
     * - a list of symbols, which includes varyings.
     *
     * There are three kinds of shaders:
     *  - astral::ItemShader::mask_item_shader which correponds to astral::MaskItemShader
     *  - astral::ItemShader::color_item_shader which correponds to astral::ColorItemShader
     *  - astral::ItemShader::shadow_map_item_shader which correponds to astral::ShadowMapItemShader
     *  .
     *
     * The shader code for each of these types is to implement the GLSL functions
     * with very specific names and symbols as follows.
     *
     * For vertex shading for all shader types, the functions to implement
     * have the following meanings to each of the arguments:
     *  - sub_shader is the value of ItemShader::subshader()
     *  - item_data_location is the location of the astra::ItemData of the draw;
     *    the data can be read with the GLSL functions astral_read_item_dataf()
     *    and astral_read_item_datau()
     *  - a0 is the value of the vertex to be processed
     *  - item_transformation is transformation from logical to pixel coordinates
     *  - item_p is the coordinate of the vertex in item coordinates
     *  .
     *
     * For astral::MaskItemShader the vertex shader code is to implement the two functions:
     * \code
     * void astral_pre_vert_shader(in uint sub_shader, in uint item_data_location,
     *                             in vec4 a0,
     *                             in AstralTransformation item_transformation);
     *
     * vec2 astral_vert_shader(in uint sub_shader, in uint item_data_location,
     *                         in vec4 a0,
     *                         in AstralTransformation item_transformation)
     * \endcode
     *
     * where astral_vert_shader() returns the location of the procssed vertex
     * in pixel coordinates.
     *
     * For astral::MaskItemShader the frament shader code is to implement the two functions:
     * \code
     * void astral_pre_frag_shader(in uint sub_shader, in uint item_data_location);
     * void astral_frag_shader(in uint sub_shader, in uint item_data_location, out vec4 out_mask_value)
     * \encode
     * where out_mask_value is the value to writen to the render target which is
     * then processed by the astral::image_blit_processing_t applied to the mask.
     *
     * For astral::ColorItemShader the vertex shader code is to implement the two functions:
     * \code
     * void astral_pre_vert_shader(in uint sub_shader, in uint item_data_location,
     *                             in vec4 a0,
     *                             in AstralTransformation item_transformation);
     *
     * vec2 astral_vert_shader(in uint sub_shader, in uint item_data_location,
     *                         in vec4 a0,
     *                         in AstralTransformation item_transformation,
     *                         out vec2 item_p)
     * \endcode
     *
     * where astral_vert_shader() returns the location of the procssed vertex
     * in pixel coordinates.
     *
     * For astral::ColorItemShader the frament shader code is to implement the two functions:
     * \code
     * void astral_pre_frag_shader(in uint sub_shader, in uint item_data_location);
     * void astral_frag_shader(in uint sub_shader, in uint color_space, in uint item_data_location, out float coverage, out vec4 base_color)
     * \encode
     * where
     *  - color_space is the colorspace of the rendering
     *  - coverage is emitted by the shader to implement anti-aliasing
     *  - base_color is the color fed to the material which will modify the color;
     *    this value is to be with alpha pre-multiplied. In most cases this should
     *    just be solid white (1, 1, 1, 1).
     *  .
     * An astral::ColorItemShader the frament shader can choose to ignore the RGB
     * (but alpha) values of material shading by setting astral_material_alpha_only
     * to true. When true, the material shader is fed (1, 1, 1, 1) as its input and
     * the final color emitted is base_color * A where A is the alpha component of
     * the output of the material shader.
     *
     * For astral::ShadowMapItemShader, the vertex shader code is to implement the
     * two functions:
     * \code
     * void astral_pre_vert_shader(in uint sub_shader, in uint item_data_location,
     *                             in vec4 a0,
     *                             in AstralTransformation item_transformation);
     *
     * vec2 astral_vert_shader(in uint sub_shader, in uint item_data_location,
     *                         in vec4 a0,
     *                         in AstralTransformation item_transformation)
     * \endcode
     *
     * where the return value of astral_vert_shader() is as follows:
     *  - .x is the texel-coordinate for where in the generated ShadowMap the vertex lands
     *  - .y is 0.0, 1.0, 2.0, 3.0 or 4.0. The y-range [0, 1] is the texel for the min-x side,
     *    the y-range [1, 2] is the texel for the max-x side, the y-range [2, 3] is the texel
     *    for the min-y side and the y-range [3, 4] is the texel for the max-y side
     *  .
     *
     * For astral::ShadowMapItemShader, the fragment shader code is to implement the
     * two functions:
     * \code
     * void astral_pre_frag_shader(in uint sub_shader, in uint item_data_location);
     * void astral_frag_shader(in uint sub_shader, in uint item_data_location, out float depth_value);
     * \endcode
     * where depth_value is written by the shader as the distance in units of the
     * coordinate system to which item_transformation maps from the light.
     */
    class ItemShaderBackendGL3:public ItemShaderBackend
    {
    public:
      /*!
       * Typedef to define list of shaders on which an
       * astral::gl::ItemShaderBackendGL3 depends.
       */
      typedef NamedShaderList<ItemShaderBackendGL3> DependencyList;

      /*!
       * Ctor
       * \param engine \ref RenderEngine of the created shader
       * \param type shader type
       * \param vertex_src vertex shader source
       * \param fragment_src fragment shader source
       * \param symbols varyings between shader stages and
       *                symbols for each shader stage
       * \param dependencies list of other material shaders used
       *                     by created shader
       * \param number_sub_shaders number of sub-shaders
       */
      static
      reference_counted_ptr<const ItemShaderBackendGL3>
      create(RenderEngineGL3 &engine,
             enum ItemShader::type_t type,
             const ShaderSource &vertex_src,
             const ShaderSource &fragment_src,
             const ShaderSymbolList &symbols,
             const DependencyList &dependencies,
             unsigned int number_sub_shaders = 1);

      /*!
       * Ctor
       * \param engine \ref RenderEngine of the created shader
       * \param type shader type
       * \param vertex_src vertex shader source
       * \param fragment_src fragment shader source
       * \param symbols varyings between shader stages and
       *                symbols for each shader stage
       * \param number_sub_shaders number of sub-shaders
       */
      static
      reference_counted_ptr<const ItemShaderBackendGL3>
      create(RenderEngineGL3 &engine,
             enum ItemShader::type_t type,
             const ShaderSource &vertex_src,
             const ShaderSource &fragment_src,
             const ShaderSymbolList &symbols,
             unsigned int number_sub_shaders = 1)
      {
        return create(engine, type, vertex_src, fragment_src, symbols,
                      DependencyList(), number_sub_shaders);
      }

      /*!
       * Returns the ItemShader::type_t of the backend shader
       */
      enum ItemShader::type_t
      type(void) const;

      /*!
       * Returns the vertex shader source.
       */
      const ShaderSource&
      vertex_src(void) const;

      /*!
       * Returns the fragment shader source.
       */
      const ShaderSource&
      fragment_src(void) const;

      /*!
       * Symbols of the shader
       */
      const ShaderSymbolList&
      symbols(void) const;

      /*!
       * List of shaders on which this shaders depends
       */
      const DependencyList&
      dependencies(void) const;

      /*!
       * Should only be called if this ItemShaderBackendGL3 provides
       * the shaders for mask shading, i.e. astral::MaskItemShader.
       * Returns an object for item shading, i.e. astral::ColorItemShader
       * where the color emitted is solit white (1, 1, 1, 1) and
       * the coverage is the value emitted by the original mask
       * shader's .r channel (which for astral::image_blit_direct_mask_processing
       * is the coverage).
       */
      reference_counted_ptr<const ItemShaderBackendGL3>
      color_shader_from_mask_shader(void) const;

      ///@cond
      unsigned int
      shader_builder_index(detail::ShaderIndexArgument) const;
      ///@endcond

    private:
      friend class RenderEngineGL3;
      class Implement;

      ItemShaderBackendGL3(RenderEngine &engine, unsigned int number_sub_shaders):
        ItemShaderBackend(engine, number_sub_shaders)
      {}
    };

/*! @} */
  }
}

#endif

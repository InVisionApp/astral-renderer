/*!
 * \file material_shader_gl3.hpp
 * \brief file material_shader_gl3.hpp
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

#ifndef ASTRAL_MATERIAL_SHADER_GL3_HPP
#define ASTRAL_MATERIAL_SHADER_GL3_HPP

#include <astral/renderer/shader/material_shader.hpp>
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
     * An astral::gl::MaterialShaderGL3 is the GL-backend implementation for
     * astral::MaterialShader. An astral::gl::MaterialShaderGL3 consists of
     * - a vertex shader
     * - a fragment shader
     * - a list of varyings
     *
     * The vertex shader source shall contain two functions. The first
     * function is
     * \code
     * void astral_material_pre_vert_shader(in uint sub_shader,
     *                                      in uint material_data_location,
     *                                      in uint material_brush_location,
     *                                      in vec2 material_p,
     *                                      in AstralTransformation pixel_transformation_material)
     * \endcode
     * which is called by the caller to allow for the shader to prepare
     * any symbol values it makes available to callers. The second function
     * \code
     * void astral_material_vert_shader(in uint sub_shader,
     *                                  in uint material_data_location,
     *                                  in uint material_brush_location,
     *                                  in vec2 material_p,
     *                                  in AstralTransformation pixel_transformation_material);
     * \endcode
     * which performs any necessary vertex shader work given the
     * input arguments
     * - sub_shader Sub shader ID
     * - material_brush_location the location of Material::brush() packed
     * - material_data_location the location of Material::shader_data()
     *   which can be read with the GLSL functions astral_read_item_dataf() and
     *   astral_read_item_datau()
     * - material_p is the material coordinates
     * - pixel_transformation_material is the transformation from material to pixel coordinates
     *
     * The fragment shader source shall contain two functions. The first
     * function is
     * \code
     * void astral_material_pre_frag_shader(in uint sub_shader, in uint color_space)
     * \endcode
     * which is called by the caller to allow for the shader to prepare
     * any symbol values it makes available to callers. The second function
     * \code
     * void astral_material_frag_shader(in uint sub_shader, in uint color_space,
     *                                  inout vec4 item_color, inout float coverage)
     * \endcode
     * modifies the inout values to apply the brush
     * - color space is in what color space the value of item_color resides where
     *   astral::colorspace_linear is ASTRAL_COLORSPACE_LINEAR and astral::colorspace_srgb
     *   is ASTRAL_COLORSPACE_SRGB
     * - item_color is the color value emitted by the item shader, the input value and
     *   output value are to be with alpha pre-multiplied.
     * - coverage is the coverage value emitted by the item shader
     * which are in turn fed to blending.
     */
    class MaterialShaderGL3:public MaterialShader
    {
    public:
      /*!
       * Typedef to define list of shaders on which an MaterialShaderGL3 depends.
       */
      typedef NamedShaderList<MaterialShaderGL3> DependencyList;

      /*!
       * Ctor
       * \param engine \ref RenderEngine of the created shader
       * \param vertex_src vertex shader source
       * \param fragment_src fragment shader source
       * \param symbols varyings between shader stages and
       *                symbols for each shader stage
       * \param properties properties of the shader
       * \param dependencies list of other material shaders used
       *                     by created shader
       * \param number_sub_shaders number of sub-shaders
       */
      static
      reference_counted_ptr<const MaterialShaderGL3>
      create(RenderEngineGL3 &engine,
             const ShaderSource &vertex_src,
             const ShaderSource &fragment_src,
             const ShaderSymbolList &symbols,
             const Properties &properties,
             const DependencyList &dependencies,
             unsigned int number_sub_shaders = 1);

      /*!
       * Ctor
       * \param engine \ref RenderEngine of the created shader
       * \param vertex_src vertex shader source
       * \param fragment_src fragment shader source
       * \param symbols varyings between shader stages and
       *                symbols for each shader stage
       * \param properties properties of the shader
       * \param number_sub_shaders number of sub-shaders
       */
      static
      reference_counted_ptr<const MaterialShaderGL3>
      create(RenderEngineGL3 &engine,
             const ShaderSource &vertex_src,
             const ShaderSource &fragment_src,
             const ShaderSymbolList &symbols,
             const Properties &properties,
             unsigned int number_sub_shaders = 1)
      {
        return create(engine, vertex_src, fragment_src, symbols,
                      properties, DependencyList(), number_sub_shaders);
      }

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

      ///@cond
      unsigned int
      shader_builder_index(detail::ShaderIndexArgument) const;
      ///@endcond

    private:
      friend class RenderEngineGL3;
      class Implement;

      MaterialShaderGL3(RenderEngine &engine, unsigned int number_sub_shaders,
                        const Properties &properties):
        MaterialShader(engine, number_sub_shaders, properties)
      {}
    };

/*! @} */
  }
}

#endif

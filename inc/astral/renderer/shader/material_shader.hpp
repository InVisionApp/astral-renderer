/*!
 * \file material_shader.hpp
 * \brief file material_shader.hpp
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

#ifndef ASTRAL_MATERIAL_SHADER_HPP
#define ASTRAL_MATERIAL_SHADER_HPP

#include <astral/util/reference_counted.hpp>
#include <astral/renderer/shader/item_data.hpp>

namespace astral
{
  class RenderEngine;

  /*!
   * \brief
   * Abstractly embodies how to color values for each pixel of an item.
   */
  class MaterialShader:public reference_counted<MaterialShader>::non_concurrent
  {
  public:
    /*!
     * \brief
     * Provides basic properties describing a \ref MaterialShader
     */
    class Properties
    {
    public:
      Properties(void):
        m_emits_transparent_fragments(false),
        m_reduces_coverage(false),
        m_uses_framebuffer_pixels(false)
      {}

      /*!
       * Sets \ref m_emits_transparent_fragments
       */
      Properties&
      emits_transparent_fragments(bool v)
      {
        m_emits_transparent_fragments = v;
        return *this;
      }

      /*!
       * Sets \ref m_reduces_coverage
       */
      Properties&
      reduces_coverage(bool v)
      {
        m_reduces_coverage = v;
        return *this;
      }

      /*!
       * Sets \ref m_uses_framebuffer_pixels
       */
      Properties&
      uses_framebuffer_pixels(bool v)
      {
        m_uses_framebuffer_pixels = v;
        return *this;
      }

      /*!
       * Provided as a conveniance, equivalent to
       * \code
       * !m_emits_transparent_fragments && !m_modifies_coverage
       * \endcode
       */
      bool
      emits_opaque(void) const
      {
        return !m_emits_transparent_fragments
          && !m_reduces_coverage;
      }

      /*!
       * If true, indicates that the shader may emit fragments
       * with alpha strictly less than one.
       */
      bool m_emits_transparent_fragments;

      /*!
       * If true, the material shader may reduce the coverage value
       * for fragments from 1 to a lower value.
       */
      bool m_reduces_coverage;

      /*!
       * If true, the material shader makes use of pixels in the framebuffer
       */
      bool m_uses_framebuffer_pixels;
    };

    /*!
     * Ctor.
     * \param engine astral::RenderEngine that can use the astral::MaterialShader
     *               It is illegal to create a astral::MaterialShader for an
     *               astral::RenderEngine during a begin()/end() pair of any
     *               astral::RenderBackend created by the astral::RenderEngine.
     * \param num_sub_shaders number of sub-shaders of the created astral::MaterialShader
     * \param props properties of the shader
     */
    MaterialShader(RenderEngine &engine, unsigned int num_sub_shaders,
                   const Properties &props = Properties());

    /*!
     * Ctor that comes from a sub-shader of a parent MaterialShader
     * \param parent shader
     * \param sub_shader_id which sub-shader of the parent
     * \param props properties of the shader
     */
    MaterialShader(const MaterialShader &parent, unsigned int sub_shader_id,
                   const Properties &props):
      m_properties(props),
      m_ID(parent.m_ID + sub_shader_id),
      m_num_sub_shaders(parent.num_sub_shaders() - sub_shader_id),
      m_root_unique_id(parent.m_root_unique_id),
      m_root(&parent.root())
    {
      ASTRALassert(sub_shader_id < parent.num_sub_shaders());
    }

    /*!
     * Ctor that comes from a sub-shader of a parent MaterialShader;
     * properties are inherited from the parent shader
     * \param parent shader
     * \param sub_shader_id which sub-shader of the parent
     */
    MaterialShader(const MaterialShader &parent, unsigned int sub_shader_id):
      m_properties(parent.properties()),
      m_ID(parent.m_ID + sub_shader_id),
      m_num_sub_shaders(parent.num_sub_shaders() - sub_shader_id),
      m_root_unique_id(parent.m_root_unique_id),
      m_root(&parent.root())
    {
      ASTRALassert(sub_shader_id < parent.num_sub_shaders());
    }

    virtual
    ~MaterialShader()
    {}

    /*!
     * Returns the ID of the shader, this value is used
     * to identify the shader by an implementation of
     * astral::RenderBackend and often used to implement
     * uber and sub-uber shading. In contrast to ItemShader,
     * this value is NEVER zero; the zero value is used
     * to indicate that the material is jsut a brush.
     */
    unsigned int
    ID(void) const
    {
      return m_ID;
    }

    /*!
     * Returns the number of sub-shaders
     */
    unsigned int
    num_sub_shaders(void) const
    {
      return m_num_sub_shaders;
    }

    /*!
     * Returns the "root" astral::MaterialShader. For shaders
     * that are not sub-shaders, it is this. For shaders that
     * are sub-shaders it is the root of the parent shader.
     */
    const MaterialShader&
    root(void) const
    {
      const MaterialShader *p;

      p = (m_root.get()) ? m_root.get() : this;
      return *p;
    }

    /*!
     * Returns the properties of this shader
     */
    const Properties&
    properties(void) const
    {
      return m_properties;
    }

    /*!
     * Each -root- MaterialShader is given a unique ID. The very
     * first root MaterialShader is given a unique ID of 1, the
     * next root MaterialShader is given a unique ID of 2. The
     * values can be used as an index into an array to get store
     * additional properties per shader.
     */
    unsigned int
    root_unique_id(void) const
    {
      return m_root_unique_id;
    }

  private:
    Properties m_properties;
    unsigned int m_ID, m_num_sub_shaders, m_root_unique_id;
    reference_counted_ptr<const MaterialShader> m_root;
  };
}

#endif

/*!
 * \file material.hpp
 * \brief file material.hpp
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

#ifndef ASTRAL_MATERIAL_HPP
#define ASTRAL_MATERIAL_HPP

#include <astral/renderer/brush.hpp>
#include <astral/renderer/shader/material_shader.hpp>
#include <astral/renderer/shader/item_data.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * An astral::Material represents how pixels are colored
   * on an item.
   */
  class Material
  {
  public:
    /*!
     * Ctor for an "empty" material. Such a material indicates
     * that the color emitted comes from the astral::ItemShader
     */
    Material(void):
      m_shader(nullptr),
      m_brush(),
      m_shader_data()
    {}

    /*!
     * Ctor inidicates that the material is applying a brush.
     */
    Material(RenderValue<Brush> br):
      m_shader(nullptr),
      m_brush(br),
      m_shader_data()
    {}

    /*!
     * Ctor inidicates that the material is implemented by an
     * astral::MaterialShader
     * \param shader astral::MaterialShader that produces color values
     * \param shader_data astral::ItemData consumed by the shader
     * \param br astral::Brush consumed by the shader
     */
    Material(const MaterialShader &shader,
                   ItemData shader_data = ItemData(),
                   RenderValue<Brush> br = RenderValue<Brush>()):
      m_shader(&shader),
      m_brush(br),
      m_shader_data(shader_data)
    {}

    /*!
     * Ctor inidicates that the material is implemented by an
     * astral::MaterialShader
     * \param shader astral::MaterialShader that produces color values
     * \param shader_data astral::ItemData consumed by the shader
     * \param br astral::Brush consumed by the shader
     */
    Material(const MaterialShader &shader,
                   RenderValue<Brush> br,
                   ItemData shader_data = ItemData()):
      m_shader(&shader),
      m_brush(br),
      m_shader_data(shader_data)
    {}

    /*!
     * If non-null, material is implemented by the returned
     * astral::MaterialShader; otherwise material is shaded
     * by the default brush shader, ShaderSet::m_brush_shader
     * using the values of brush().
     */
    const MaterialShader*
    material_shader(void) const
    {
      return m_shader;
    }

    /*!
     * The brush of the material; if material_shader() is
     * null and if brush() is not valid, then the color
     * emitted is the color emitted by the astral::ItemShader
     */
    RenderValue<Brush>
    brush(void) const
    {
      return m_brush;
    }

    /*!
     * The shader data of the material; only
     * used if material_shader() does not return
     * nullptr.
     */
    ItemData
    shader_data(void) const
    {
      return m_shader_data;
    }

    /*!
     * Returns true if this render material has no effect,
     * i.e. material_shader() is nullptr and brush() has
     * RenderValue::valid() false.
     */
    bool
    empty_material(void) const
    {
      return !m_shader && !m_brush.valid();
    }

    /*!
     * Returns true if the material emits partially coveraged fragments
     */
    bool
    emits_partial_coverage(void) const
    {
      return (m_shader && m_shader->properties().m_reduces_coverage);
    }

    /*!
     * Returns true if the material emits fragments with alpha less than one
     */
    bool
    emits_transparent_fragments(void) const
    {
      return (m_shader && m_shader->properties().m_emits_transparent_fragments)
        || (!m_shader && m_brush.valid() && !m_brush.value().m_opaque);
    }

    /*!
     * Returns true if the material uses pixels from the framebuffer
     */
    bool
    uses_framebuffer_pixels(void) const
    {
      return m_shader && m_shader->properties().m_uses_framebuffer_pixels;
    }

  private:
    const MaterialShader *m_shader;
    RenderValue<Brush> m_brush;
    ItemData m_shader_data;
  };

/*! @} */
}

#endif

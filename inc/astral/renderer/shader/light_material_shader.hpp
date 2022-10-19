/*!
 * \file light_material_shader.hpp
 * \brief file light_material_shader.hpp
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

#ifndef ASTRAL_LIGHT_MATERIAL_SHADER_HPP
#define ASTRAL_LIGHT_MATERIAL_SHADER_HPP

#include <astral/renderer/shader/material_shader.hpp>
#include <astral/renderer/shader/item_data.hpp>
#include <astral/renderer/render_value.hpp>
#include <astral/renderer/shadow_map.hpp>
#include <astral/util/transformation.hpp>

namespace astral
{
  /*!
   * \brief
   * An astral::LightMaterialShader represents performing a
   * 2D lighting computation where shadows are compute
   * via an astral::ShadowMap
   */
  class LightMaterialShader
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
        item_data_size = 3,
      };

    /*!
     * Light properties given in item coordinates.
     */
    class LightProperties
    {
    public:
      LightProperties(void):
        m_light_z(-1.0f),
        m_color(255u, 255u, 255u, 255u),
        m_shadow_color(0u, 0u, 0u, 255u),
        m_light_direction(1.0f, 0.0f),
        m_directional_cos_half_angle(-2.0f),
        m_shadow_fall_off(-1.0f),
        m_shadow_fall_off_length(0.0f)
      {}

      /*!
       * If positive, indicates to perform per-pixel lighting computation
       * as well and this value gives the z-value of the location of the
       * light.
       */
      float m_light_z;

      /*!
       * The color when in a pixel is lit by the light.
       * Let L(p) the the light intensity at a point p,
       * including the occlusion by a shadow. The color
       * emitted by the shader is then the value of
       * mix(m_shadow_color, m_color, L(p)).
       */
      u8vec4 m_color;

      /*!
       * The color when in a pixel is not lit by the light.
       * Let L(p) the the light intensity at a point p,
       * including the occlusion by a shadow. The color
       * emitted by the shader is then the value of
       * mix(m_shadow_color, m_color, L(p)).
       */
      u8vec4 m_shadow_color;

      /*!
       * The direction of the light in local coordinates
       * for directional lighting. This must be a unit
       * vector.
       */
      vec2 m_light_direction;

      /*!
       * The cosine of half the angle of the directional
       * light; the directional light will light a point
       * p if the abs(cos(theta)) is no more than this
       * value where theta is the angle between the vector
       * p - \ref m_position and \ref m_light_direction.
       * Note that a value less than -1.0 means that the
       * light is not directional.
       */
      float m_directional_cos_half_angle;

      /*!
       * When \ref m_shadow_fall_off > 0, provides a shadow
       * fall off value, i.e. a length for when the shadow
       * casting stops.
       */
      float m_shadow_fall_off;

      /*!
       * When \ref m_shadow_fall_off > 0, provides the lenght
       * of the shadow fall off a shadow to give a smooth transition
       * between in and outside of the shadow.
       */
      float m_shadow_fall_off_length;

      /*!
       * astral::RenderValue<astral::ShadowMap&> to shadow map
       */
      RenderValue<const ShadowMap&> m_shadow_map;

      /* Should we also have the light strength fall off with
       * distance as well?
       */

      /* Should we give an option to discretize the light
       * intensity to give a cell-shaded look?
       */

      /* should we have an option that the light does
       * not emit a color and instead the light intensity
       * is a coverage value to emit?
       */

      /*!
       * Set \ref m_light_z
       */
      LightProperties&
      light_z(float v)
      {
        m_light_z = v;
        return *this;
      }

      /*!
       * Set \ref m_color
       */
      LightProperties&
      color(u8vec4 v)
      {
        m_color = v;
        return *this;
      }

      /*!
       * Set \ref m_shadow_color
       */
      LightProperties&
      shadow_color(u8vec4 v)
      {
        m_shadow_color = v;
        return *this;
      }

      /*!
       * Set \ref m_light_direction
       */
      LightProperties&
      light_direction(vec2 v)
      {
        m_light_direction = v;
        return *this;
      }

      /*!
       * Set \ref m_directional_cos_half_angle
       */
      LightProperties&
      directional_angle_radians(float v)
      {
        m_directional_cos_half_angle = t_cos(0.5f * v);
        return *this;
      }

      /*!
       * Set \ref m_directional_cos_half_angle
       */
      LightProperties&
      directional_angle_degrees(float v)
      {
        return directional_angle_radians(v * ASTRAL_PI / 180.0f);
      }

      /*!
       * Set \ref m_shadow_map
       */
      LightProperties&
      shadow_map(RenderValue<const ShadowMap&> v)
      {
        m_shadow_map = v;
        return *this;
      }

      /*!
       * Set \ref m_shadow_map
       */
      LightProperties&
      shadow_fall_off(float v)
      {
        m_shadow_fall_off = v;
        return *this;
      }

      /*!
       * Set \ref m_shadow_map_length
       */
      LightProperties&
      shadow_fall_off_length(float v)
      {
        m_shadow_fall_off_length = v;
        return *this;
      }
    };

    /*!
     * Empty ctor, leaving the object wihout a shader
     */
    LightMaterialShader(void)
    {}

    /*!
     * Ctor giving it a shader.
     * \param sh shader of the created object
     */
    LightMaterialShader(const reference_counted_ptr<const MaterialShader> &sh):
      m_shader(sh)
    {}

    /*!
     * Ctor giving it a shader.
     * \param sh shader of the created object
     */
    LightMaterialShader(const MaterialShader *sh):
      m_shader(sh)
    {}

    /*!
     * Cast operator
     */
    operator const reference_counted_ptr<const MaterialShader>&() const { return m_shader; }

    /*!
     * Cast operator
     */
    operator reference_counted_ptr<const MaterialShader>&() { return m_shader; }

    /*!
     * overload operator*
     */
    const MaterialShader& operator*(void) const { return *m_shader; }

    /*!
     * overload operator->
     */
    const MaterialShader* operator->(void) const { ASTRALassert(m_shader); return m_shader.get(); }

    /*!
     * Return the pointer to the underlying shader
     */
    const MaterialShader* get(void) const { return m_shader.get(); }

    /*!
     * Pack item data the astral::MaterialShader of an
     * astral::LightMaterialShader accepts
     *
     * TODO: document packing.
     *
     * \param shadow_transformation_material transformation from material
     *                                       coordinate to shadow map coordinates
     * \param props light properties
     * \param dst location to which to pack the numbers
     */
    static
    void
    pack_item_data(RenderValue<Transformation> shadow_transformation_material,
                   const LightProperties &props, c_array<gvec4> dst)
    {
      ASTRALassert(dst.size() == item_data_size);
      ASTRALassert(props.m_shadow_map.valid());

      vec2 light_position = props.m_shadow_map.value().light_position();
      if (shadow_transformation_material.valid())
        {
          light_position = shadow_transformation_material.value().inverse().apply_to_point(light_position);
        }

      dst[0].x().f = light_position.x();
      dst[0].y().f = light_position.y();
      dst[0].z().f = props.m_light_direction.x();
      dst[0].w().f = props.m_light_direction.y();

      dst[1].x().u = pack_u8vec4(props.m_color);
      dst[1].y().u = pack_u8vec4(props.m_shadow_color);
      dst[1].z().f = props.m_directional_cos_half_angle;
      dst[1].w().f = props.m_light_z;

      dst[2].x().u = props.m_shadow_map.cookie();
      dst[2].y().u = shadow_transformation_material.cookie();

      dst[2].z().f = props.m_shadow_fall_off;
      dst[2].w().f = t_max(0.0f, props.m_shadow_fall_off_length);
    }

    /*!
     * Returns a astral::ItemDataValueMapping for the item data
     * of a astral::LightMaterialShader
     */
    static
    const ItemDataValueMapping&
    intrepreted_value_map(void);

  private:
    reference_counted_ptr<const MaterialShader> m_shader;
  };
}

#endif

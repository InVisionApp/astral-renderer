/*!
 * \file blend_mode.hpp
 * \brief file blend_mode.hpp
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

#ifndef ASTRAL_BACKEND_BLEND_MODE
#define ASTRAL_BACKEND_BLEND_MODE

#include <stdint.h>
#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/shader/item_shader.hpp>

namespace astral
{
/*!\addtogroup RendererBackend
 * @{
 */

  /*!
   * A BackendBlendMode represents all the information an astral::RenderBackend
   * needs to convert how to blend into GPU state and/or shader epilogue.
   */
  class BackendBlendMode
  {
  public:
    /*!
     * Enum type to wrap \ref mask_mode_rendering
     */
    enum mask_rendering_t:uint32_t
      {
        /*!
         * Enumeration tag used in ctor to indicate that blending
         * is for mask rendering
         */
        mask_mode_rendering
      };

    /*!
     * Enum type to wrap \ref shadowmap_mode_rendering
     */
    enum shadow_rendering_t:uint32_t
      {
        /*!
         * Enumeration tag used in ctor to indicate that blending
         * is for shadowmap rendering
         */
        shadowmap_mode_rendering
      };

    enum
      {
        /*!
         * One plus the largest value that packed_value() can emit
         */
        number_packed_values = 2u * number_blend_modes + ItemShader::color_item_shader
      };

    /*!
     * Ctor
     * \param blend_mode value returne by blend_mode(void) const
     * \param emits_partial_coverage value returne by emits_partial_coverage(void) const
     */
    BackendBlendMode(enum blend_mode_t blend_mode, bool emits_partial_coverage):
      m_value(blend_mode * 2u + (emits_partial_coverage ? 1u : 0u))
    {
      ASTRALassert(blend_mode < number_blend_modes);
    }

    /*!
     * Ctor
     * \param blend_mode value returne by blend_mode(void) const
     * \param emits_partial_coverage value returne by emits_partial_coverage(void) const
     */
    BackendBlendMode(bool emits_partial_coverage, enum blend_mode_t blend_mode):
      BackendBlendMode(blend_mode, emits_partial_coverage)
    {}

    /*!
     * Ctor; initializes to indicate that blending is for mask rendering
     */
    explicit
    BackendBlendMode(enum mask_rendering_t):
      m_value(2u * number_blend_modes + ItemShader::mask_item_shader)
    {
      ASTRALstatic_assert(ItemShader::color_item_shader > ItemShader::mask_item_shader);
    }

    /*!
     * Ctor; initializes to indicate that blending is for shadowmap rendering
     */
    explicit
    BackendBlendMode(enum shadow_rendering_t):
      m_value(2u * number_blend_modes  + ItemShader::shadow_map_item_shader)
    {
      ASTRALstatic_assert(ItemShader::color_item_shader > ItemShader::shadow_map_item_shader);
    }

    /*!
     * Ctor for an invalid blend mode which specifies nothing and valid()
     * returns false.
     */
    BackendBlendMode(void):
      m_value(~0u)
    {}

    /*!
     * Create BackendBlendMode whose packed_value() returns the value passed;
     * the value passed must be no more than \ref number_packed_values. The
     * purpose of this function is to provide a caller of BackendBlendMode a
     * simple means to iterate through all legal values.
     */
    static
    BackendBlendMode
    from_packed_value(uint32_t value)
    {
      BackendBlendMode return_value;

      ASTRALassert(value < number_packed_values);
      return_value.m_value = value;
      return return_value;
    }

    /*!
     * Comparison operator
     */
    bool
    operator!=(BackendBlendMode rhs) const
    {
      return m_value != rhs.m_value;
    }

    /*!
     * Comparison operator
     */
    bool
    operator==(BackendBlendMode rhs) const
    {
      return m_value == rhs.m_value;
    }

    /*!
     * Returns true if this BackendBlendMode was consrtucted
     * with arguments.
     */
    bool
    valid(void) const
    {
      return m_value != ~0u;
    }

    /*!
     * Specifies what astral::ItemShader::type_t this
     * astral::BackendBlendMode is for.
     */
    enum ItemShader::type_t
    item_shader_type(void) const
    {
      uint32_t r;

      r = (m_value >= 2u * number_blend_modes) ?
        m_value - 2u * number_blend_modes:
	static_cast<uint32_t>(ItemShader::color_item_shader);

      ASTRALassert(valid());
      return static_cast<enum ItemShader::type_t>(r);
    }

    /*!
     * Returns the astral::blend_mode_t of this astral::BackendBlendMode;
     * a return value of astral::number_blend_modes indicates that
     * the BackendBlendMode is not for color rendering
     */
    enum blend_mode_t
    blend_mode(void) const
    {
      uint32_t b;

      ASTRALassert(valid());
      b = m_value >> 1u;
      return static_cast<enum blend_mode_t>(b);
    }

    /*!
     * Returns true if shader emits partially covered pixels
     */
    bool
    emits_partial_coverage(void) const
    {
      uint32_t b;

      ASTRALassert(valid());
      b = m_value & 1u;
      return (b != 0u) && item_shader_type() == ItemShader::color_item_shader;
    }

    /*!
     * Returns this BackendBlendMode as a uint32_t value in the range
     * [0, \ref number_packed_values).
     */
    uint32_t
    packed_value(void) const
    {
      ASTRALassert(valid());
      return m_value;
    }

  private:
    /* Packing:
     *   0 <= m_value < 2 * number_blend_modes --> color blending with bit0 specifying
     *                                             coverage and rest of the bits giving
     *                                             the blend mode
     *   m_value >= 2 * number_blend_modes     --> m_value - 2 * number_blend_modes
     *                                             gives the ItemShader::type_t value
     */
    uint32_t m_value;
  };

/*! @} */
}

#endif

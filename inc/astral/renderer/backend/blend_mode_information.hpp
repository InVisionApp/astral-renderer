/*!
 * \file blend_mode_information.hpp
 * \brief file blend_mode_information.hpp
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

#ifndef ASTRAL_BLEND_MODE_INFORMATION
#define ASTRAL_BLEND_MODE_INFORMATION

#include <astral/util/vecN.hpp>
#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/backend/blend_mode.hpp>

namespace astral
{
/*!\addtogroup RendererBackend
 * @{
 */
    /*!
     * Class used by an astral::RenderBackend to specify
     * if the current values in a framebuffer are needed
     * as an input surface to implement a blend mode.
     */
    class BlendModeInformation
    {
    public:
      /*!
       * Enumeration to describe if and how a blend mode
       * requires a surface holding a copy of the pixel
       * values to execute a blend.
       */
      enum requires_framebuffer_pixels_t:uint32_t
        {
          /*!
           * Indicates to implement the blend mode, the backend
           * does NOT require the pixels of the framebuffer.
           */
          does_not_need_framebuffer_pixels,

          /*!
           * Indicates to implement the blend mode, the backend
           * DOES require the a copy of the pixels of the framebuffer,
           * but when drawn (using as input a surface holding a copy of
           * the values of the pixels of the framebuffer), the draw
           * covers and does not blend with the framebuffer
           */
          requires_framebuffer_pixels_opaque_draw,

          /*!
           * Indicates to implement the blend mode, the backend
           * DOES require a copy of the the pixels of the framebuffer
           * and when drawn the pixels of the framebuffer drawn to
           * affect the draw via GPU fixed function blending
           */
          requires_framebuffer_pixels_blend_draw,
        };

      /*!
       * Ctor. Initializes that no blend mode ever requires
       * the current values of the framebuffer are needed
       * as an input surface.
       */
      BlendModeInformation(void):
        m_values(does_not_need_framebuffer_pixels)
      {}

      /*!
       * Set the value returned by requires_framebuffer_pixels(BackendBlendMode) const
       * \param blend_mode which blend mode
       * \param new_value new value for if and how when shading with the
       *                  given blend mode and coverage mode if a surface holding a
       *                  copy of the framebuffer pixels are needed
       */
      BlendModeInformation&
      requires_framebuffer_pixels(BackendBlendMode blend_mode,
                                  enum requires_framebuffer_pixels_t new_value)
      {
        m_values[blend_mode.packed_value()] = new_value;
        return *this;
      }

      /*!
       * Returns if a copy of the framebuffer pixels are needed to execute
       * a blend mode.
       */
      enum requires_framebuffer_pixels_t
      requires_framebuffer_pixels(BackendBlendMode blend_mode) const
      {
        return m_values[blend_mode.packed_value()];
      }

    private:
      vecN<enum requires_framebuffer_pixels_t, BackendBlendMode::number_packed_values> m_values;
    };

/*! @} */
}

#endif

/*!
 * \file render_backend_enums.hpp
 * \brief file render_backend_enums.hpp
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

#ifndef ASTRAL_RENDER_BACKEND_ENUMS_HPP
#define ASTRAL_RENDER_BACKEND_ENUMS_HPP

namespace astral
{
/*!\addtogroup RendererBackend
 * @{
 */

  /*!
   * The enumerations of astral::color_post_sampling_mode_t
   * are made from bits of astral::color_post_sampling_mode_bits.
   * A backend shall execute the following code in the exact
   * order below to implement astral::color_post_sampling_mode_t
   * value C on a premultiplied alpha color value (r, g, b, a)
   *
   * \code
   * if (C & color_post_sampling_mode_bits_alpha_invert)
   *   {
   *       a = 1.0 - a;
   *   }
   *
   * if (C & color_post_sampling_mode_bits_rgba_zero)
   *   {
   *     (r, g, b) = (0, 0, 0)
   *   }
   *
   * if (C & color_post_sampling_mode_bits_rgb_invert)
   *   {
   *     (r, g, b) = (a - r, a - g, a - b)
   *   }
   *
   * if (C & color_post_sampling_mode_bits_alpha_one)
   *  {
   *     a = 1
   *  }
   *
   * \endcode
   *
   * The above code will produce a premultiplied alpha color
   * value from a premultiplied alpha color given a VALID
   * astral::color_post_sampling_mode_t; this is because
   * color_post_sampling_mode_bits_rgb_invert can only be
   * up if color_post_sampling_mode_bits_rgba_zero is up or
   * if color_post_sampling_mode_bits_invert_alpha is down.
   */
  enum color_post_sampling_mode_bits:uint32_t
    {
      /*!
       * Indicates that the alpha channel should be
       * inverted, see code in enumeration description
       */
      color_post_sampling_mode_bits_alpha_invert = 1u,

      /*!
       * Indicates that the (r, g, b) channels should be
       * set to zero, see code in enumeration description
       */
      color_post_sampling_mode_bits_rgb_zero = 2u,

      /*!
       * Indicates that the (r, g, b) channels should be
       * inverted to (a - r, a - g, a - b) AFTER performing
       * the modifications necessary if the any of the bits
       * astral::color_post_sampling_mode_bits_invert_alpha
       * and/or astral::color_post_sampling_mode_bits_rgb_zero
       * are up.
       */
      color_post_sampling_mode_bits_rgb_invert = 4u,

      /*!
       * Indicates to set alpha as 1 after performing the processing
       * of astral::color_post_sampling_mode_bits_alpha_invert,
       * astral::color_post_sampling_mode_bits_rgb_zero and
       * astral::color_post_sampling_mode_bits_rgb_invert
       */
      color_post_sampling_mode_bits_alpha_one = 8u,
    };

  /*!
   * Enumeration to describe if a RenderValue<ClipWindow> is present,
   * and if present, to also describe how to use it.
   */
  enum clip_window_value_type_t : uint32_t
    {
      /*!
       * Indicates that no clip window is present
       */
      clip_window_not_present = 0u,

      /*!
       * Indicates that a clip window is present and must
       * be enforced by the backend
       */
      clip_window_present_enforce,

      /*!
       * indicates that a clip window is present, but
       * a backend does not need to enforce it. A backend
       * uses this mode to early out in fragment shading.
       */
      clip_window_present_optional,

      clip_window_value_type_count
    };

/*! @} */
}

#endif

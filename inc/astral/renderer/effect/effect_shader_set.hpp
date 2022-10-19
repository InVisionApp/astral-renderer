/*!
 * \file effect_shader_set.hpp
 * \brief file effect_shader_set.hpp
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

#ifndef ASTRAL_EFFECT_SHADER_SET_HPP
#define ASTRAL_EFFECT_SHADER_SET_HPP

#include <astral/renderer/effect/gaussian_blur_effect_shader.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * \brief
   * An astral::EffectShaderSet represents the shaders used by
   * astral::Effect objects of an astral::EffectSet.
   */
  class EffectShaderSet
  {
  public:
    /*!
     * Shaders used by the astral::Effect that implement Gaussian blur.
     */
    GaussianBlurEffectShader m_gaussian_blur_shader;
  };

/*! @} */
}

#endif

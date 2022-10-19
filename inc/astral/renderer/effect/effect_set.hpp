/*!
 * \file effect_set.hpp
 * \brief file effect_set.hpp
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

#ifndef ASTRAL_EFFECT_SET_HPP
#define ASTRAL_EFFECT_SET_HPP

#include <astral/renderer/effect/effect.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * \brief
   * Class that defines the types and holds references
   * to a standard set of effects
   */
  class EffectSet
  {
  public:
    /*!
     * Effect that implements Gaussian blur; the
     * effect parameters are packed as in
     * astral::GaussianBlurParameters.
     */
    reference_counted_ptr<Effect> m_gaussian_blur;
  };

/*! @} */
}

#endif

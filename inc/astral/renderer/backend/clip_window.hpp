/*!
 * \file clip_window.hpp
 * \brief file clip_window.hpp
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


#ifndef ASTRAL_CLIP_WINDOW_HPP
#define ASTRAL_CLIP_WINDOW_HPP

#include <astral/util/vecN.hpp>
#include <astral/util/c_array.hpp>

namespace astral
{
/*!\addtogroup RendererBackend
 * @{
 */

  /*!
   * \brief
   * Clipping window to be enforced by the GPU (hopefully
   * by hardware clipping planes).
   */
  class ClipWindow
  {
  public:
    /*!
     * Rectangle representing the clipping window
     */
    Rect m_values;
  };

/*! @} */
}

#endif

/*!
 * \file render_data.hpp
 * \brief file render_data.hpp
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

#ifndef ASTRAL_RENDER_DATA_HPP
#define ASTRAL_RENDER_DATA_HPP

#include <astral/renderer/static_data.hpp>
#include <astral/renderer/vertex_data.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * \brief
   * Conveniance class to hold references to both an
   * astral::StaticData and astral::VertexData.
   */
  class RenderData
  {
  public:
    /*!
     * Clear both \ref m_static_data and \ref m_vertex_data
     */
    void
    clear(void)
    {
      m_static_data.clear();
      m_vertex_data.clear();
    }

    /*!
     * Reference to static data
     */
    reference_counted_ptr<const StaticData> m_static_data;

    /*!
     * Reference to vertex data
     */
    reference_counted_ptr<const VertexData> m_vertex_data;
  };

/*! @} */
}

#endif

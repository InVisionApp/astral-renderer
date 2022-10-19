/*!
 * \file shader_detail.hpp
 * \brief file shader_detail.hpp
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

#ifndef ASTRAL_SHADER_DETAIL_HPP
#define ASTRAL_SHADER_DETAIL_HPP

namespace astral
{
  class RendeEngine;
  class ItemShaderBackend;
  class MaterialShader;

  namespace detail
  {
    ///@cond
    class SubShaderCount
    {
    private:
      friend class astral::RenderEngine;
      friend class astral::ItemShaderBackend;
      friend class astral::MaterialShader;

      explicit
      SubShaderCount(unsigned int v):
        m_v(v)
      {}

      unsigned int m_v;
    };
    ///@endcond
  }
}

#endif

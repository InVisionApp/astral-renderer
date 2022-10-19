/*!
 * \file shader_gl3_detail.hpp
 * \brief file shader_gl3_detail.hpp
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

#ifndef ASTRAL_SHADER_GL_DETAIL_HPP
#define ASTRAL_SHADER_GL_DETAIL_HPP

namespace astral
{
  namespace gl
  {
    class RenderEngineGL3;
    class ItemShaderBackendGL3;
    class MaterialShaderGL3;

    namespace detail
    {
      ///@cond
      class ShaderIndexArgument
      {
      private:
        friend class astral::gl::RenderEngineGL3;
        friend class astral::gl::ItemShaderBackendGL3;
        friend class astral::gl::MaterialShaderGL3;

        ShaderIndexArgument(void)
        {}
      };
      ///@endcond
    }
  }
}

#endif

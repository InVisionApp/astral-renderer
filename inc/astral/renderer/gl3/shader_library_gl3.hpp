/*!
 * \file shader_library_gl3.hpp
 * \brief file shader_library_gl3.hpp
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

#ifndef ASTRAL_SHADER_LIBRARY_GL3_HPP
#define ASTRAL_SHADER_LIBRARY_GL3_HPP

#include <astral/util/gl/gl_shader_source.hpp>

namespace astral
{
  namespace gl
  {
/*!\addtogroup gl
 * @{
 */
    /*!
     * \brief
     * A ShaderLibraryGL3 provides a set of astral::ShaderLibrary
     * objects that applications can reuse for their own shaders
     */
    class ShaderLibraryGL3
    {
    public:
      /*!
       * \ref ShaderLibrary providing utility functions and macros
       * for stroke shading. See \ref GLSLStroke for the list of
       * GLSL functions and structs it provides.
       */
      reference_counted_ptr<const ShaderLibrary> m_stroke_lib;

      /*!
       * \ref ShaderLibrary providing utility functions for gradient
       * computations. See \ref GLSLGradient for the list of
       * GLSL functions and structs it provides.
       */
      reference_counted_ptr<const ShaderLibrary> m_gradient_lib;

      /*!
       * \ref ShaderLibrary providing utility functions for image
       * sampling. See \ref GLSLImage for the list of
       * GLSL functions and structs it provides.
       */
      reference_counted_ptr<const ShaderLibrary> m_image_lib;

      /*!
       * \ref ShaderLibrary providing functions for shading
       * \ref ItemPath objects. See \ref GLSLItemPath for
       * the list of GLSL functions and structs it provides.
       */
      reference_counted_ptr<const ShaderLibrary> m_item_path_lib;
    };
/*! @} */

  }
}

#endif

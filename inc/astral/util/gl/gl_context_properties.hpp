/*!
 * \file gl_context_properties.hpp
 * \brief file gl_context_properties.hpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */


#ifndef ASTRAL_GL_CONTEXT_PROPERTIES_HPP
#define ASTRAL_GL_CONTEXT_PROPERTIES_HPP

#include <set>
#include <string>
#include <astral/util/util.hpp>
#include <astral/util/vecN.hpp>

namespace astral
{
  namespace gl
  {

/*!\addtogroup gl_util
 * @{
 */
    /*!
     * \brief
     * ContextProperties provides an interface to
     * query GL/GLES version and extensions.
     */
    namespace ContextProperties
    {
      /*!
       * Return the GL/GLES version of the GL context
       * with the major version in [0] and the minor
       * version in [1].
       */
      vecN<int, 2>
      version(void);

      /*!
       * Returns the GL major version, equivalent to
       * \code
       * version()[0]
       * \endcode
       */
      inline
      int
      major_version(void)
      {
        return version()[0];
      }

      /*!
       * Returns the GL minor version, equivalent to
       * \code
       * version()[1]
       * \endcode
       */
      inline
      int
      minor_version(void)
      {
        return version()[1];
      }

      /*!
       * Returns true if the context is OpenGL ES,
       * returns false if the context is OpenGL.
       */
      bool
      is_es(void);

      /*!
       * Returns true if the context supports the named
       * extension.
       */
      bool
      has_extension(c_string ext);

      /*!
       * Returns the extension list as an std::set
       */
      const std::set<std::string>&
      extension_set(void);
    };
/*! @} */
  }
}

#endif

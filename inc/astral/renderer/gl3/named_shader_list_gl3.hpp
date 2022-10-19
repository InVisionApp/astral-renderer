/*!
 * \file named_shader_list_gl3.hpp
 * \brief file named_shader_list_gl3.hpp
 *
 * Copyright 2020 by InvisionApp.
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

#ifndef ASTRAL_NAMED_SHADER_LIST_GL_HPP
#define ASTRAL_NAMED_SHADER_LIST_GL_HPP

#include <string>
#include <vector>
#include <astral/util/reference_counted.hpp>

namespace astral
{
  namespace gl
  {
/*!\addtogroup gl
 * @{
 */
    /*!
     * Class to specify a list of shaders and names to use to
     * access them from another shader.
     */
    template<typename T>
    class NamedShaderList
    {
    public:
      /*!
       * An entry in the list.
       */
      class Entry
      {
      public:
        /*!
         * Ctor.
         */
        Entry(const std::string &name, const T &shader):
          m_name(name),
          m_shader(&shader)
        {}

        /*!
         * Name by which to name the shader
         */
        std::string m_name;

        /*!
         * The shader accessed via \ref m_name
         */
        reference_counted_ptr<const T> m_shader;
      };

      /*!
       * Add an entry to \ref m_values
       * \param name value for Entry::m_name of added entry
       * \param shader value for Entry::m_shader of added entry
       */
      NamedShaderList&
      add(const std::string &name, const T &shader)
      {
        m_values.push_back(Entry(name, shader));
        return *this;
      }

      /*!
       * List of entries; it is an error if any two entries
       * have the same value for Entry::m_name
       */
      std::vector<Entry> m_values;
    };
/*! @} */

  }
}

#endif

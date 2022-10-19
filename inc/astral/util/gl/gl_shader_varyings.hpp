/*!
 * \file gl_shader_varyings.hpp
 * \brief file gl_shader_varyings.hpp
 *
 * Adapted from: varying_list.hpp of FastUIDraw:
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

#ifndef ASTRAL_GL_SHADER_VARYINGS_HPP
#define ASTRAL_GL_SHADER_VARYINGS_HPP

#include <vector>
#include <string>
#include <iterator>
#include <algorithm>
#include <astral/util/vecN.hpp>
#include <astral/util/c_array.hpp>

namespace astral
{
  namespace gl
  {

/*!\addtogroup gl_util
 * @{
 */
    /*!
     * \brief
     * A \ref ShaderVaryings represents the varyings of a GLSL shader.
     */
    class ShaderVaryings
    {
    public:
      /*!
       * \brief
       * Enumeration to define the interpolator type of a varying
       */
      enum interpolator_type_t:uint32_t
        {
          interpolator_smooth, /*!< corresponds to "smooth" of type float in GLSL */
          interpolator_flat, /*!< corresponds to "flat" of type float in GLSL */
          interpolator_uint, /*!< corresponds to "flat" of type uint in GLSL */
          interpolator_int, /*!< corresponds to "flat" of type int in GLSL */

          interpolator_number_types,
        };

      ShaderVaryings(void):
        m_varying_count(0)
      {}

      /*!
       * Returns the names of the varyings of the
       * specified interpolator type.
       * \param tp interpolator type
       */
      c_array<const std::string>
      varyings(enum interpolator_type_t tp) const
      {
        ASTRALassert(tp < interpolator_number_types);
        return make_c_array(m_varyings[tp]);
      }

      /*!
       * Returns the number of varyings by type.
       */
      const vecN<unsigned int, interpolator_number_types>&
      varying_count(void) const
      {
        return m_varying_count;
      }

      /*!
       * Add a varying
       * \param name name by which to reference the varying
       * \param tp interpolator type of the varying
       */
      ShaderVaryings&
      add_varying(const std::string &name, enum interpolator_type_t tp)
      {
        ASTRALassert(tp < interpolator_number_types);
        m_varyings[tp].push_back(name);
        ++m_varying_count[tp];
        return *this;
      }

      /*!
       * Add varying from another \ref ShaderVaryings value
       * \param src \ref ShaderVaryings from which to copy
       *            all varyings
       */
      ShaderVaryings&
      add_varyings(const ShaderVaryings &src)
      {
        for (unsigned int tp = 0; tp < interpolator_number_types; ++tp)
          {
            m_varying_count[tp] += src.m_varying_count[tp];
            std::copy(src.m_varyings[tp].begin(),
                      src.m_varyings[tp].end(),
                      std::back_inserter(m_varyings[tp]));
          }
        return *this;
      }

      /* TODO: define/implement an alias-interface for varyings.
       *       Needed for eventual support of chaining shaders
       */

    private:
      vecN<std::vector<std::string>, interpolator_number_types> m_varyings;
      vecN<unsigned int, interpolator_number_types> m_varying_count;
    };
/*! @} */
  }
}

#endif

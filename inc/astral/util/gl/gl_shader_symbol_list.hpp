/*!
 * \file gl_shader_symbol_list.hpp
 * \brief file gl_shader_symbol_list.hpp
 *
 * Copyright 2020 by Intel.
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

#ifndef ASTRAL_GL_SHADER_SYMBOL_LIST_HPP
#define ASTRAL_GL_SHADER_SYMBOL_LIST_HPP

#include <utility>
#include <string>
#include <iterator>
#include <astral/util/vecN.hpp>
#include <astral/util/gl/gl_shader_varyings.hpp>

namespace astral
{
  namespace gl
  {

/*!\addtogroup gl_util
 * @{
 */
    /*!
     * Class to specify the following of a shader:
     *  - list of varyings
     *  - list of exported symbols from vertex shader
     *  - list of exported symbols from fragment shader
     *  - list of links between vertex shader symbols and varyings
     *  - list of links between fragment shader symbols and varyings
     *  - list of additional functions in the vertex shader
     *  - list of additional functions in the fragment shader
     */
    class ShaderSymbolList
    {
    public:
      /*!
       * Enumeration to specify the type of the symbol
       */
      enum symbol_type_t:uint32_t
        {
          /*!
           * Symbol is a scalar floating point value
           */
          symbol_type_float,

          /*!
           * Symbol is a scalar unsigned int value
           */
          symbol_type_uint,

          /*!
           * Symbol is a scalar int value
           */
          symbol_type_int,

          number_symbol_type
        };

      /*!
       * Given an ShaderVaryings::interpolator_type_t value, return
       * the symbolc type of it.
       */
      static
      enum symbol_type_t
      symbol_type(enum ShaderVaryings::interpolator_type_t tp);

      ShaderSymbolList(void)
      {}

      /*!
       * Ctor to initialize with the values of a ShaderVarying
       * \param varyings varyings with which to start
       */
      ShaderSymbolList(const ShaderVaryings &varyings):
        m_varyings(varyings)
      {}

      /*!
       * Add a varying; it is an error to add the same named varying twice
       * and it is also an error to add a varying with the same name as
       * vertex or fragment shader symbol.
       * \param name name of varying to add
       * \param tp varying type
       */
      ShaderSymbolList&
      add_varying(const std::string &name, enum ShaderVaryings::interpolator_type_t tp)
      {
        m_varyings.add_varying(name, tp);
        return *this;
      }

      /*!
       * Copy all symbols of another ShaderSymbolList into this ShaderSymbolList
       * \param symbols \ref ShaderSymbolList from which to copy
       */
      ShaderSymbolList&
      add_symbols(const ShaderSymbolList &symbols)
      {
        m_varyings.add_varyings(symbols.m_varyings);

        for (unsigned int i = 0; i < number_symbol_type; ++i)
          {
            std::copy(symbols.m_vertex_shader_symbols[i].begin(),
                      symbols.m_vertex_shader_symbols[i].end(),
                      std::back_inserter(m_vertex_shader_symbols[i]));
            std::copy(symbols.m_fragment_shader_symbols[i].begin(),
                      symbols.m_fragment_shader_symbols[i].end(),
                      std::back_inserter(m_fragment_shader_symbols[i]));
          }
        std::copy(symbols.m_vertex_aliases.begin(),
                  symbols.m_vertex_aliases.end(),
                  std::back_inserter(m_vertex_aliases));
        std::copy(symbols.m_fragment_aliases.begin(),
                  symbols.m_fragment_aliases.end(),
                  std::back_inserter(m_fragment_aliases));

        /* Should we also be copying the locals or not? */
        std::copy(symbols.m_vertex_shader_locals.begin(),
                  symbols.m_vertex_shader_locals.end(),
                  std::back_inserter(m_vertex_shader_locals));

        std::copy(symbols.m_fragment_shader_locals.begin(),
                  symbols.m_fragment_shader_locals.end(),
                  std::back_inserter(m_fragment_shader_locals));

        return *this;
      }

      /*!
       * Add a vertex shader symbol; it is an error to add the same named
       * symbol twoce and it is also an error to add a symbol with the same
       * name as a varying
       * \param name name of symbol to add
       * \param tp symbol type
       */
      ShaderSymbolList&
      add_vertex_shader_symbol(enum symbol_type_t tp, const std::string &name)
      {
        m_vertex_shader_symbols[tp].push_back(name);
        return *this;
      }

      /*!
       * Link two symbols together in the vertex shader making them the same
       * variable. It is an error to link between symbols with different
       * types.
       * \param s0 name of varying added with add_varying() or symbol added
       *           with add_vertex_shader_symbol()
       * \param s1 name of varying added with add_varying() or symbol added
       *           with add_vertex_shader_symbol()
       */
      ShaderSymbolList&
      add_vertex_shader_alias(const std::string &s0, const std::string &s1)
      {
        m_vertex_aliases.push_back(std::make_pair(s0, s1));
        return *this;
      }

      /*!
       * Add a fragment shader symbol; it is an error to add the same named
       * symbol twice and it is also an error to add a symbol with the same
       * name as a varying. However, one can link varyings to symbols.
       * \param name name of symbol to add
       * \param tp symbol type
       */
      ShaderSymbolList&
      add_fragment_shader_symbol(enum symbol_type_t tp, const std::string &name)
      {
        m_fragment_shader_symbols[tp].push_back(name);
        return *this;
      }

      /*!
       * Link two symbols together in the fragment shader making them the
       * same variable. It is an error to link between symbols with different
       * types. However, one can link varyings to symbols.
       * \param s0 name of varying added with add_varying() or symbol added
       *           with add_fragment_shader_symbol()
       * \param s1 name of varying added with add_varying() or symbol added
       *           with add_fragment_shader_symbol()
       */
      ShaderSymbolList&
      add_fragment_shader_alias(const std::string &s0, const std::string &s1)
      {
        m_fragment_aliases.push_back(std::make_pair(s0, s1));
        return *this;
      }

      /*!
       * Add a vertex shader local
       */
      ShaderSymbolList&
      add_vertex_shader_local(const std::string &nm)
      {
        m_vertex_shader_locals.push_back(nm);
        return *this;
      }

      /*!
       * Add a set of vertex shader locals
       */
      template<typename iterator>
      ShaderSymbolList&
      add_vertex_shader_locals(iterator begin, iterator end)
      {
        for (; begin != end; ++begin)
          {
            m_vertex_shader_locals.push_back(*begin);
          }
        return *this;
      }

      /*!
       * Add a fragment shader local
       */
      ShaderSymbolList&
      add_fragment_shader_local(const std::string &nm)
      {
        m_fragment_shader_locals.push_back(nm);
        return *this;
      }

      /*!
       * Add a set of fragment shader locals
       */
      template<typename iterator>
      ShaderSymbolList&
      add_fragment_shader_locals(iterator begin, iterator end)
      {
        for (; begin != end; ++begin)
          {
            m_fragment_shader_locals.push_back(*begin);
          }
        return *this;
      }

      /*!
       * Add a shader local to both vertex and fragment list
       */
      ShaderSymbolList&
      add_shader_local(const std::string &nm)
      {
        m_vertex_shader_locals.push_back(nm);
        m_fragment_shader_locals.push_back(nm);
        return *this;
      }

      /*!
       * Add a set of shader locals to both vertex and fragment list
       */
      template<typename iterator>
      ShaderSymbolList&
      add_shader_locals(iterator begin, iterator end)
      {
        for (; begin != end; ++begin)
          {
            m_vertex_shader_locals.push_back(*begin);
            m_fragment_shader_locals.push_back(*begin);
          }
        return *this;
      }

      /*!
       * The varyings fed from the vertex shader to the fragment shader
       */
      ShaderVaryings m_varyings;

      /*!
       * The list of exported symbols from the vertex shader
       */
      vecN<std::vector<std::string>, number_symbol_type> m_vertex_shader_symbols;

      /*!
       * The list of exported symbols from the fragment shader
       */
      vecN<std::vector<std::string>, number_symbol_type> m_fragment_shader_symbols;

      /*!
       * List of aliases linking vertex shader symbols to other
       * symbols and varyings. Each pair declares that the two
       * symbols refer to the same variable within the vertex
       * shader. The linking is also transitive, thus a single
       * variable can be referenced by any number of symbols.
       */
      std::vector<std::pair<std::string, std::string>> m_vertex_aliases;

      /*!
       * List of aliases linking fragment shader symbols to other
       * symbols and varyings. Each pair declares that the two
       * symbols refer to the same variable within the fragment
       * shader. The linking is also transitive, thus a single
       * variable can be referenced by any number of symbols.
       */
      std::vector<std::pair<std::string, std::string>> m_fragment_aliases;

      /*!
       * List of additional local -names- that are defined
       * in the vertex shader. It is illegal to list the same
       * local twice. A local is something whose declaration and
       * backing are within the shader. Locals cannot alias. By
       * listing it, shade assembly can, via macros, will give
       * it a unique name instance.
       */
      std::vector<std::string> m_vertex_shader_locals;

      /*!
       * List of additional local -names- that are defined
       * in the fragment shader. It is illegal to list the same
       * local twice. A local is something whose declaration and
       * backing are within the shader. Locals cannot alias. By
       * listing it, shade assembly can, via macros, will give
       * it a unique name instance.
       */
      std::vector<std::string> m_fragment_shader_locals;
    };
/*! @} */

  }
}


#endif

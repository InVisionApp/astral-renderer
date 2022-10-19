/*!
 * \file shader_implement_gl3.hpp
 * \brief file shader_implement_gl3.hpp
 *
 * Copyright 2021 by InvisionApp.
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

#ifndef ASTRAL_SHADER_IMPLEMENT_GL3_HPP
#define ASTRAL_SHADER_IMPLEMENT_GL3_HPP

#include <vector>
#include <map>

#include <astral/renderer/shader/item_shader.hpp>
#include <astral/renderer/gl3/shader_gl3_detail.hpp>
#include <astral/renderer/gl3/named_shader_list_gl3.hpp>
#include <astral/util/gl/gl_shader_source.hpp>
#include <astral/util/gl/gl_shader_varyings.hpp>
#include <astral/util/gl/gl_shader_symbol_list.hpp>
#include <astral/renderer/gl3/render_engine_gl3.hpp>
#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/gl3/material_shader_gl3.hpp>
#include <astral/renderer/gl3/item_shader_gl3.hpp>

namespace astral
{
  namespace gl
  {
    namespace detail
    {
      enum shader_stage_t:uint32_t
        {
          vertex_shader_stage = 0u,
          fragment_shader_stage,

          number_shader_stages
        };

      /* class to track the number of varyings needed
       * to back the varying spelled out by ShaderVaryings
       */
      class BackingVaryingCount
      {
      public:
        enum type_t: uint32_t
          {
            flat_backing = 0,
            smooth_backing,

            number_types,
          };

        static
        type_t
        get_type(enum ShaderVaryings::interpolator_type_t i)
        {
          return (i == ShaderVaryings::interpolator_smooth) ? smooth_backing : flat_backing;
        }

        BackingVaryingCount(void):
          m_values(0, 0)
        {}

        BackingVaryingCount(const vecN<unsigned int, ShaderVaryings::interpolator_number_types> &v):
          BackingVaryingCount()
        {
          add_varyings(v);
        }

        BackingVaryingCount&
        add_varyings(enum type_t t, unsigned count = 1u)
        {
          m_values[t] += count;
          return *this;
        }

        BackingVaryingCount&
        add_varyings(enum ShaderVaryings::interpolator_type_t i, unsigned count = 1u)
        {
          enum type_t t;

          t = get_type(i);
          m_values[t] += count;

          return *this;
        }

        BackingVaryingCount&
        add_varyings(const vecN<unsigned int, ShaderVaryings::interpolator_number_types> &v)
        {
          for (unsigned int i = 0; i < ShaderVaryings::interpolator_number_types; ++i)
            {
              enum type_t t;

              t = get_type(static_cast<enum ShaderVaryings::interpolator_type_t>(i));
              m_values[t] += v[i];
            }

          return *this;
        }

        BackingVaryingCount&
        max_against(BackingVaryingCount v)
        {
          m_values = component_max(m_values, v.m_values);
          return *this;
        }

        unsigned int
        value(enum type_t t) const
        {
          return m_values[t];
        }

        const uvec2&
        raw_values(void) const
        {
          return m_values;
        }

        unsigned int
        total(void) const
        {
          return m_values.L1norm();
        }

      private:
        uvec2 m_values;
      };

      /* class to describe a variable backing's type */
      class VariableBackingType
      {
      public:
        VariableBackingType(enum ShaderVaryings::interpolator_type_t tp):
          m_data(tp)
        {}

        VariableBackingType(enum ShaderSymbolList::symbol_type_t tp):
          m_data(tp + ShaderVaryings::interpolator_number_types)
        {}

        bool
        is_varying(void) const
        {
          return m_data < ShaderVaryings::interpolator_number_types;
        }

        enum ShaderVaryings::interpolator_type_t
        interpolator_type(void) const
        {
          ASTRALassert(is_varying());
          return static_cast<enum ShaderVaryings::interpolator_type_t>(m_data);
        }

        enum ShaderSymbolList::symbol_type_t
        symbol_type(void) const
        {
          ASTRALassert(!is_varying());
          return static_cast<enum ShaderSymbolList::symbol_type_t>(m_data - ShaderVaryings::interpolator_number_types);
        }

        bool
        operator<(VariableBackingType rhs) const
        {
          return m_data < rhs.m_data;
        }

      private:
        unsigned int m_data;
      };

      /* Class describes how a variable is backed as an interpolator
       * ot a global along with what type of interpolate or global
       */
      class VariableBacking
      {
      public:
        VariableBacking(VariableBackingType tp, unsigned int slot):
          m_data(tp, slot)
        {}

        unsigned int
        slot(void) const
        {
          return m_data.second;
        }

        VariableBackingType
        type(void) const
        {
          return m_data.first;
        }

        /* may only be called if type().is_varying() is false */
        std::string
        glsl_name(const std::string &tag, const vecN<unsigned int, ShaderSymbolList::number_symbol_type> &symbol_count) const;

        bool
        operator<(const VariableBacking &rhs) const
        {
          return m_data < rhs.m_data;
        }

      private:
        /* .first = type
         * .second = slot
         */
        std::pair<VariableBackingType, unsigned int> m_data;
      };

      class ShaderImplementBase;

      /* In contrast to NamedShaderList<T>, WeakNamedShaderList
       * only contains weak pointers and they always point to
       * a ShaderImplementBase object whic holds all of the
       * data anyways. The main downside is that something
       * else must hold the strong references. The upside is
       * that we can avoid virtual function silliness (or
       * confusing template code to directly use the class
       * NamedShaderList<T>.
       */
      class WeakNamedShaderList
      {
      public:
        class Entry
        {
        public:
          Entry(void):
            m_shader(nullptr)
          {}

          std::string m_name;
          const ShaderImplementBase *m_shader;
        };

        /* Ctor
         * \param src NamedShaderList from which to extract values
         * \param f functor that converts const T* to const ShaderImplementBase*
         *          correctly; the pattern we are having is that MaterialShaderGL3
         *          and ItemShaderBackendGL3 are implemented as an Implement
         *          member class of each and that T::Implement inherits from both
         *          ShaderImplementBase and T (for T = MaterialShaderGL3 and
         *          T = ItemShaderBackendGL3). Thus, we cannot just cast the
         *          pointer directly here, but the T::Implement class will,
         *          by providing the functor.
         */
        template<typename T, typename F>
        WeakNamedShaderList(const NamedShaderList<T> &src, F f)
        {
          for (const auto &e : src.m_values)
            {
              Entry E;

              E.m_name = e.m_name;
              E.m_shader = f(e.m_shader.get());

              m_values.push_back(E);
            }
        }

        std::vector<Entry> m_values;
      };

      /* The symbols of a shader distilled for easier streaming and realization */
      class DistilledShaderSymbols
      {
      public:
        void
        clear(void)
        {
          m_local_symbols[vertex_shader_stage].clear();
          m_local_symbols[vertex_shader_stage].clear();
          m_variables[vertex_shader_stage].clear();
          m_variables[fragment_shader_stage].clear();
          std::fill(m_symbol_counts[vertex_shader_stage].begin(), m_symbol_counts[vertex_shader_stage].end(), 0u);
          std::fill(m_symbol_counts[fragment_shader_stage].begin(), m_symbol_counts[fragment_shader_stage].end(), 0u);
          std::fill(m_varying_counts.begin(), m_varying_counts.end(), 0u);
        }

        /* List of local symbols defined by ShaderSymbolList, i.e.
         * only ShaderSymbolList::m_vertex_shader_locals or
         * ShaderSymbolList::m_fragment_shader_locals
         */
        vecN<std::vector<std::string>, 2> m_local_symbols;

        /* For each VariableBacking, a list of symbols it backs.
         * This includes symbols from the shader AND all of its dependencies.
         */
        vecN<std::map<VariableBacking, std::vector<std::string>>, number_shader_stages> m_variables;

        /* counts for each variable backing type needed */
        vecN<vecN<unsigned int, ShaderSymbolList::number_symbol_type>, number_shader_stages> m_symbol_counts;
        vecN<unsigned int, ShaderVaryings::interpolator_number_types> m_varying_counts;
      };

      class ShaderImplementBase
      {
      public:
        template<typename T, typename F>
        ShaderImplementBase(const ShaderSource &vertex_src,
                            const ShaderSource &fragment_src,
                            const ShaderSymbolList &symbols,
                            F f, const NamedShaderList<T> &dependencies,
                            unsigned int shader_builder_index):
          m_weak_dependencies(dependencies, f),
          m_src(vertex_src, fragment_src),
          m_raw_symbols(symbols),
          m_shader_builder_index(shader_builder_index)
        {
          ASTRALstatic_assert(vertex_shader_stage == 0u);
          ASTRALstatic_assert(fragment_shader_stage == 1u);
          ctor_body();
        }

        /* Streams the backings of varyings.
         * \param tag for the shader type (i.e. item or material shader)
         * \param count number of varyings
         * \param dst ShaderSource to which to stream
         */
        static
        void
        stream_varying_backings(const std::string &tag, BackingVaryingCount count, ShaderSource &dst);

        /* Streams the backings of symbols.
         * \param tag for the shader type (i.e. item or material shader)
         * \param count number of symbols of each type that are to backed;
         * \param dst ShaderSource to which to stream
         */
        static
        void
        stream_symbol_backings(const std::string &tag,
                               const vecN<unsigned int, ShaderSymbolList::number_symbol_type> &count,
                               ShaderSource &dst);

        /* Streams the shader, including aliasing of varyings where the shader
         * is referred to with the named prefix; prefix cannot be empty.
         * \param stage which shader stage, a vertex shader will stream a function
         *              "astral_tag_write_varying()" which writes the varyings
         *              to the actual backng varyings and a named fragment will
         *              stream a function "astral_tag_load_varyings()" that
         *              will load the varyings from the backing.
         * \param tag tag for the shader type (i.e. item or material shader)
         * \param varying_count number of total varyings that are backed, this
         *                      value can be larger than the sum over all varying
         *                      types of distilled_symbols(); the use case if this
         *                      shader and others are getting streamed to the same
         *                      location to build an uber-shader
         * \param symbol_count number of symbols of each type that are to be backed;
         *                     this value can be greater than the value from
         *                     distilled_symbols(); the use case if this shader
         *                     and others are getting streamed to the same location
         *                     to build an uber-shader
         * \param functions list of functions to be localized, this is entirely
         *                  determined by stage and if streaming ItemShaderBackendGL3
         *                  or MaterialShaderGL3 content
         * \param dst ShaderSource to which to stream
         */
        void
        stream_shader(const std::string &tag, enum shader_stage_t stage, const std::string &prefix,
                      BackingVaryingCount varying_count,
                      const vecN<unsigned int, ShaderSymbolList::number_symbol_type> &symbol_count,
                      c_array<const c_string> functions, ShaderSource &dst) const;

        const DistilledShaderSymbols&
        distilled_symbols(void) const
        {
          return m_distilled_symbols;
        }

        /* note that these are all const, so might as well make them public */
        const WeakNamedShaderList m_weak_dependencies;
        const vecN<ShaderSource, 2> m_src;
        const ShaderSymbolList m_raw_symbols;
        const unsigned int m_shader_builder_index;

      private:
        void
        ctor_body(void);

        void
        stream_read_load_varyings(const std::string &tag, enum shader_stage_t stage,
                                  const std::string &prefix, BackingVaryingCount varying_count);

        void
        stream_variable_backings(const std::string &tag, enum shader_stage_t stage, const std::string &prefix,
                                 BackingVaryingCount varying_count,
                                 const vecN<unsigned int, ShaderSymbolList::number_symbol_type> &symbol_count,
                                 ShaderSource &dst) const;

        void
        unstream_variable_backings(enum shader_stage_t stage, const std::string &prefix,
                                   ShaderSource &dst) const;

        void
        stream_shader_implement(enum shader_stage_t stage,
                                c_array<const c_string> functions,
                                const std::string &prefix, ShaderSource &dst) const;

        void
        stream_symbols(enum shader_stage_t stage, const std::string &label,
                       c_array<const c_string> functions,
                       const std::string &prefix, ShaderSource &dst) const;

        void
        unstream_symbols(enum shader_stage_t stage, const std::string &label,
                         c_array<const c_string> functions,
                         const std::string &prefix, ShaderSource &dst) const;

        /* used to do the magic for streaming */
        DistilledShaderSymbols m_distilled_symbols;
      };
    }

    class MaterialShaderGL3::Implement:
      public MaterialShaderGL3,
      public detail::ShaderImplementBase
    {
    public:
      Implement(RenderEngineGL3 &engine,
                const ShaderSource &vertex_src,
                const ShaderSource &fragment_src,
                const ShaderSymbolList &symbols,
                const Properties &properties,
                const DependencyList &dependencies,
                unsigned int number_sub_shaders):
        MaterialShaderGL3(engine, number_sub_shaders, properties),
        detail::ShaderImplementBase(vertex_src, fragment_src,
                                    symbols, functor, dependencies,
                                    engine.allocate_material_shader_index(detail::ShaderIndexArgument(), this)),
        m_dependencies(dependencies)
      {}

      static
      const detail::ShaderImplementBase*
      functor(const MaterialShaderGL3 *p)
      {
        return static_cast<const Implement*>(p);
      }

      DependencyList m_dependencies;
    };

    class ItemShaderBackendGL3::Implement:
      public ItemShaderBackendGL3,
      public detail::ShaderImplementBase
    {
    public:
      Implement(RenderEngineGL3 &engine,
                enum ItemShader::type_t type,
                const ShaderSource &vertex_src,
                const ShaderSource &fragment_src,
                const ShaderSymbolList &symbols,
                const DependencyList &dependencies,
                unsigned int number_sub_shaders):
        ItemShaderBackendGL3(engine, number_sub_shaders),
        detail::ShaderImplementBase(vertex_src, fragment_src,
                                    symbols, functor, dependencies,
                                    engine.allocate_item_shader_index(detail::ShaderIndexArgument(), this, type)),
        m_engine(engine),
        m_type(type),
        m_dependencies(dependencies)
      {}

      static
      const detail::ShaderImplementBase*
      functor(const ItemShaderBackendGL3 *p)
      {
        return static_cast<const Implement*>(p);
      }

      /* This MUST be a weak reference because RenderEngineGL3 has refernces
       * to all shaders made with it.
       */
      RenderEngineGL3 &m_engine;

      enum ItemShader::type_t m_type;
      DependencyList m_dependencies;
      mutable reference_counted_ptr<const ItemShaderBackendGL3> m_color_shader_from_mask_shader;
    };
  }
}


#endif

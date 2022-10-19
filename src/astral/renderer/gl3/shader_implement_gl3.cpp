/*!
 * \file shader_implement_gl3.cpp
 * \brief file shader_implement_gl3.cpp
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

#include <astral/util/ostream_utility.hpp>
#include "shader_implement_gl3.hpp"

namespace
{
  class EquivalenceClass:public astral::reference_counted<EquivalenceClass>::non_concurrent
  {
  public:
    EquivalenceClass(enum astral::gl::ShaderSymbolList::symbol_type_t tp,
                     const std::string &name):
      m_type(tp),
      m_interpolator_type(astral::gl::ShaderVaryings::interpolator_number_types),
      m_emitted(false)
    {
      m_names.insert(name);
    }

    EquivalenceClass(enum astral::gl::ShaderVaryings::interpolator_type_t tp,
                     const std::string &name):
      m_type(astral::gl::ShaderSymbolList::symbol_type(tp)),
      m_interpolator_type(tp),
      m_emitted(false)
    {
      m_names.insert(name);
    }

    void
    absorb(const EquivalenceClass &v)
    {
      ASTRALassert(compatible(v));

      std::copy(v.m_names.begin(), v.m_names.end(), std::inserter(m_names, m_names.end()));
      if (v.is_varying())
        {
          m_interpolator_type = v.m_interpolator_type;
        }
    }

    void
    absorb(const std::string &v)
    {
      m_names.insert(v);
    }

    const std::set<std::string>&
    names(void) const
    {
      return m_names;
    }

    void
    emit(bool add_to_varying_backing,
         std::map<astral::gl::detail::VariableBacking, std::vector<std::string>> &out_variables,
         astral::vecN<unsigned int, astral::gl::ShaderSymbolList::number_symbol_type> &symbol_counts,
         astral::vecN<unsigned int, astral::gl::ShaderVaryings::interpolator_number_types> &varying_counts,
         std::map<std::string, astral::gl::detail::VariableBacking> &varying_backing);

  private:
    bool
    is_varying(void) const
    {
      return m_interpolator_type != astral::gl::ShaderVaryings::interpolator_number_types;
    }

    bool
    compatible(const EquivalenceClass &v) const
    {
      return m_type == v.m_type
        && (!is_varying() || !v.is_varying() || m_interpolator_type == v.m_interpolator_type);
    }

    /* List of all names that are aliased together */
    std::set<std::string> m_names;

    /* variable type */
    enum astral::gl::ShaderSymbolList::symbol_type_t m_type;

    /* interpolator type if interpolator */
    enum astral::gl::ShaderVaryings::interpolator_type_t m_interpolator_type;

    /* if emit has been called */
    bool m_emitted;
  };

  class EquivalenceClassHoard:astral::noncopyable
  {
  public:
    void
    generate_distilled_symbols(const astral::gl::detail::WeakNamedShaderList &weak_list,
                               const astral::gl::ShaderSymbolList &symbols,
                               astral::gl::detail::DistilledShaderSymbols *out_symbols);

  private:
    typedef std::map<std::string, astral::reference_counted_ptr<EquivalenceClass>> eq_map;

    void
    add_symbols(const astral::gl::detail::WeakNamedShaderList &weak_list,
                const astral::gl::ShaderSymbolList &symbols);

    void
    add_symbols(const std::string &prefix, const astral::gl::detail::DistilledShaderSymbols &symbols);

    void
    add_symbols(enum astral::gl::detail::shader_stage_t H, const std::string &prefix,
                const std::map<astral::gl::detail::VariableBacking, std::vector<std::string>> &variables);

    astral::reference_counted_ptr<EquivalenceClass>
    add_symbol(enum astral::gl::detail::shader_stage_t H,
               const std::string &name, astral::gl::detail::VariableBackingType type);

    void
    add_alias(enum astral::gl::detail::shader_stage_t H,
              astral::reference_counted_ptr<EquivalenceClass> eq_class, const std::string &name);

    void
    add_alias(enum astral::gl::detail::shader_stage_t H,
              const std::string &a, const std::string &b);

    astral::vecN<eq_map,2 > m_hoards;
  };

  /* Given an astral::gl::ShaderSymbolList::symbol_type_t, returnss the GLSL
   * type to back cnt such varyings with cnt no more than 4 and atleast 1.
   */
  astral::c_string
  glsl_type(enum astral::gl::ShaderSymbolList::symbol_type_t t, unsigned int cnt)
  {
    static const astral::c_string types[astral::gl::ShaderSymbolList::number_symbol_type][4] =
      {
        [astral::gl::ShaderSymbolList::symbol_type_float] = { [0] = "float", [1] = "vec2", [2] = "vec3", [3] = "vec4" },
        [astral::gl::ShaderSymbolList::symbol_type_uint] =  { [0] = "uint", [1] = "uvec2", [2] = "uvec3", [3] = "uvec4" },
        [astral::gl::ShaderSymbolList::symbol_type_int] =   { [0] = "int", [1] = "ivec2", [2] = "ivec3", [3] = "ivec4" },
      };

    ASTRALassert(cnt > 0u && cnt < 5u);
    ASTRALassert(t < astral::gl::ShaderSymbolList::number_symbol_type);
    return types[t][cnt - 1u];
  }

  /* All flat varyings are collapsed to uint type; Flat varyings are thus bit
   * casted as necesary to and from uint. However, smooth float varyings have
   */

  astral::c_string
  glsl_varying_qualifier(enum astral::gl::detail::BackingVaryingCount::type_t t)
  {
    return (t == astral::gl::detail::BackingVaryingCount::flat_backing) ?
      "flat":
      "";
  }

  astral::c_string
  glsl_varying_type(enum astral::gl::detail::BackingVaryingCount::type_t t, unsigned int cnt)
  {
    enum astral::gl::ShaderSymbolList::symbol_type_t s;

    s = (t == astral::gl::detail::BackingVaryingCount::flat_backing) ?
      astral::gl::ShaderSymbolList::symbol_type_uint :
      astral::gl::ShaderSymbolList::symbol_type_float;

    return glsl_type(s, cnt);
  }

  astral::c_string
  glsl_scalar_type(enum astral::gl::ShaderVaryings::interpolator_type_t t)
  {
    static const astral::c_string types[astral::gl::ShaderVaryings::interpolator_number_types] =
      {
        [astral::gl::ShaderVaryings::interpolator_smooth] = "float",
        [astral::gl::ShaderVaryings::interpolator_flat] = "float",
        [astral::gl::ShaderVaryings::interpolator_uint] = "uint",
        [astral::gl::ShaderVaryings::interpolator_int] = "int",
      };

    ASTRALassert(t < astral::gl::ShaderVaryings::interpolator_number_types);
    return types[t];
  }

  astral::c_string
  interpolator_cast_to_varying(enum astral::gl::ShaderVaryings::interpolator_type_t t)
  {
    static const astral::c_string types[astral::gl::ShaderVaryings::interpolator_number_types] =
      {
        [astral::gl::ShaderVaryings::interpolator_smooth] = "",
        [astral::gl::ShaderVaryings::interpolator_flat] = "floatBitsToUint",
        [astral::gl::ShaderVaryings::interpolator_uint] = "",
        [astral::gl::ShaderVaryings::interpolator_int] = "uint",
      };

    ASTRALassert(t < astral::gl::ShaderVaryings::interpolator_number_types);
    return types[t];
  }

  astral::c_string
  interpolator_cast_from_varying(enum astral::gl::ShaderVaryings::interpolator_type_t t)
  {
    static const astral::c_string types[astral::gl::ShaderVaryings::interpolator_number_types] =
      {
        [astral::gl::ShaderVaryings::interpolator_smooth] = "",
        [astral::gl::ShaderVaryings::interpolator_flat] = "uintBitsToFloat",
        [astral::gl::ShaderVaryings::interpolator_uint] = "",
        [astral::gl::ShaderVaryings::interpolator_int] = "int",
      };

    ASTRALassert(t < astral::gl::ShaderVaryings::interpolator_number_types);
    return types[t];
  }

  /* Varyings are backed with vecN (N = 1, 2, 3, 4). This gives
   * the GLSL name of the backing for the named vecN.
   */
  std::string
  glsl_vecN_backing(const std::string &tag,
                    enum astral::gl::detail::BackingVaryingCount::type_t tp,
                    unsigned int which_vecN)
  {
    std::ostringstream str;

    // the type(tp, 1) is just to differentiate between the different types
    str << "astral_" << tag << "_varying_" << glsl_varying_qualifier(tp) << which_vecN;
    return str.str();
  }

  /* Symbols are backed with [iu]vecN (N = 1, 2, 3, 4). This gives
   * the GLSL name of the backing for the named [iu]vecN.
   */
  std::string
  glsl_vecN_backing(const std::string &tag,
                    enum astral::gl::ShaderSymbolList::symbol_type_t tp,
                    unsigned int which_vecN)
  {
    std::ostringstream str;

    // the type(tp, 1) is just to differentiate between the different types
    str << "astral_" << tag << "_global_" << glsl_type(tp, 1) << which_vecN;
    return str.str();
  }

  /* This gives the GLSL name of the backing for a varying
   * \param t interpolate type
   * \param count number of varyings that are backed
   * \param slot which scalar varying of the type t
   */
  std::string
  glsl_backing(const std::string &tag,
               enum astral::gl::detail::BackingVaryingCount::type_t t,
               astral::gl::detail::BackingVaryingCount count,
               unsigned int slot)
  {
    std::ostringstream str;
    unsigned int which_vecN, which_component;

    which_vecN = slot >> 2u;
    which_component = slot & 0x3u;

    str << glsl_vecN_backing(tag, t, which_vecN);

    /* If the last declared varying is a scalar and if we are
     * asking for it, then a field extract is not necessary.
     */
    if (count.value(t) != slot + 1u || which_component != 0u)
      {
        static const char exts[4] = { 'x', 'y', 'z', 'w' };
        str << "." << exts[which_component];
      }
    return str.str();
  }

  /* This gives the GLSL name of the backing for a symbol
   * \param t symbol type
   * \param count number of symbols that are backed
   * \param slot which scalar varying of the type t
   */
  std::string
  glsl_backing(const std::string &tag, enum astral::gl::ShaderSymbolList::symbol_type_t t,
               const astral::vecN<unsigned int, astral::gl::ShaderSymbolList::number_symbol_type> &count,
               unsigned int slot)
  {
    std::ostringstream str;
    unsigned int which_vecN, which_component;

    which_vecN = slot >> 2u;
    which_component = slot & 0x3u;

    str << glsl_vecN_backing(tag, t, which_vecN);

    /* If the last declared varying is a scalar and if we are
     * asking for it, then a field extract is not necessary.
     */
    if (count[t] != slot + 1u || which_component != 0u)
      {
        const char exts[4] = { 'x', 'y', 'z', 'w' };
        str << "." << exts[which_component];
      }
    return str.str();
  }
}

/////////////////////////////////////////
// EquivalenceClass methods
void
EquivalenceClass::
emit(bool add_to_varying_backing,
     std::map<astral::gl::detail::VariableBacking, std::vector<std::string>> &out_variables,
     astral::vecN<unsigned int, astral::gl::ShaderSymbolList::number_symbol_type> &symbol_counts,
     astral::vecN<unsigned int, astral::gl::ShaderVaryings::interpolator_number_types> &varying_counts,
     std::map<std::string, astral::gl::detail::VariableBacking> &varying_backing)
{
  if (m_emitted)
    {
      return;
    }

  m_emitted = true;

  std::vector<std::string> *dst;
  if (is_varying())
    {
      if (add_to_varying_backing)
        {
          unsigned int slot;

          slot = varying_counts[m_interpolator_type];
          ++varying_counts[m_interpolator_type];

          astral::gl::detail::VariableBacking backing(m_interpolator_type, slot);

          for (const std::string &nm : m_names)
            {
              ASTRALassert(varying_backing.find(nm) == varying_backing.end());
              varying_backing.insert({nm, backing});
            }

          ASTRALassert(out_variables.find(backing) == out_variables.end());
          dst = &out_variables[backing];
        }
      else
        {
          /* find a slot, keep checking until we find it in varying_backing */
          int slot(-1);

          for (const std::string &nm : m_names)
            {
              std::map<std::string, astral::gl::detail::VariableBacking>::const_iterator iter = varying_backing.find(nm);
              if (iter != varying_backing.end())
                {
                  slot = iter->second.slot();
                  ASTRALassert(iter->second.type().interpolator_type() == m_interpolator_type);
                  break;
                }
            }

          ASTRALassert(slot != -1);
          astral::gl::detail::VariableBacking backing(m_interpolator_type, slot);

          ASTRALassert(out_variables.find(backing) == out_variables.end());
          dst = &out_variables[backing];
        }
    }
  else
    {
      astral::gl::detail::VariableBacking backing(m_type, symbol_counts[m_type]);

      ASTRALassert(out_variables.find(backing) == out_variables.end());
      dst = &out_variables[backing];
      ++symbol_counts[m_type];
    }

  for (const std::string &nm : m_names)
    {
      dst->push_back(nm);
    }
}

//////////////////////////////////////////
// EquivalenceClassHoard methods
astral::reference_counted_ptr<EquivalenceClass>
EquivalenceClassHoard::
add_symbol(enum astral::gl::detail::shader_stage_t H, const std::string &name, astral::gl::detail::VariableBackingType type)
{
  eq_map &eq_map(m_hoards[H]);
  astral::reference_counted_ptr<EquivalenceClass> eq;

  if (type.is_varying())
    {
      eq = ASTRALnew EquivalenceClass(type.interpolator_type(), name);
    }
  else
    {
      eq = ASTRALnew EquivalenceClass(type.symbol_type(), name);
    }

  ASTRALassert(eq_map.find(name) == eq_map.end());
  eq_map[name] = eq;

  return eq;
}

void
EquivalenceClassHoard::
add_alias(enum astral::gl::detail::shader_stage_t H, astral::reference_counted_ptr<EquivalenceClass> eq, const std::string &name)
{
  eq_map &eq_map(m_hoards[H]);

  ASTRALassert(eq_map.find(name) == eq_map.end());
  eq_map[name] = eq;
  eq->absorb(name);
}

void
EquivalenceClassHoard::
add_alias(enum astral::gl::detail::shader_stage_t H, const std::string &a, const std::string &b)
{
  eq_map &eq_map(m_hoards[H]);
  eq_map::iterator iterA, iterB;

  iterA = eq_map.find(a);
  iterB = eq_map.find(b);
  if (iterA != eq_map.end() && iterB != eq_map.end())
    {
      astral::reference_counted_ptr<EquivalenceClass> eq_a, eq_b;

      eq_a = iterA->second;
      eq_b = iterB->second;

      /* have eq_a absorb all of eq_b */
      eq_a->absorb(*eq_b);

      /* every element that uses eq_b must now use eq_a */
      for (const std::string &nm : eq_b->names())
        {
          ASTRALassert(eq_map[nm] == eq_b);
          eq_map[nm] = eq_a;
        }
      ASTRALassert(iterB->second == eq_a);
    }
  else if (iterA != eq_map.end())
    {
      iterA->second->absorb(b);
      eq_map[b] = iterA->second;
    }
  else if (iterB != eq_map.end())
    {
      iterB->second->absorb(a);
      eq_map[a] = iterB->second;
    }
}

void
EquivalenceClassHoard::
add_symbols(const std::string &prefix, const astral::gl::detail::DistilledShaderSymbols &symbols)
{
  for (unsigned int h = 0; h < astral::gl::detail::number_shader_stages; ++h)
    {
      enum astral::gl::detail::shader_stage_t H;

      H = static_cast<enum astral::gl::detail::shader_stage_t>(h);
      add_symbols(H, prefix, symbols.m_variables[H]);
    }
}

void
EquivalenceClassHoard::
add_symbols(enum astral::gl::detail::shader_stage_t H, const std::string &prefix,
            const std::map<astral::gl::detail::VariableBacking, std::vector<std::string>> &variables)
{
  for (const auto &variable : variables)
    {
      astral::reference_counted_ptr<EquivalenceClass> eq;

      ASTRALassert(!variable.second.empty());

      eq = add_symbol(H, prefix + "::" + variable.second.front(), variable.first.type());
      for (unsigned int i = 1, endi = variable.second.size(); i < endi; ++i)
        {
          add_alias(H, eq, variable.second[i]);
        }
    }
}

void
EquivalenceClassHoard::
add_symbols(const astral::gl::detail::WeakNamedShaderList &weak_list,
            const astral::gl::ShaderSymbolList &symbols)
{
  /* The order is delicate. First symbols are added, then aliases are made. */

  /* first add all elements of the fetcher */
  for (const astral::gl::detail::WeakNamedShaderList::Entry &e : weak_list.m_values)
    {
      add_symbols(e.m_name, e.m_shader->distilled_symbols());
    }

  /* add elements from symbols */
  for (unsigned int i = 0; i < astral::gl::ShaderSymbolList::number_symbol_type; ++i)
    {
      enum astral::gl::ShaderSymbolList::symbol_type_t tp;

      tp = static_cast<enum astral::gl::ShaderSymbolList::symbol_type_t>(i);
      for (const std::string &name : symbols.m_vertex_shader_symbols[tp])
        {
          add_symbol(astral::gl::detail::vertex_shader_stage, name, tp);
        }
      for (const std::string &name : symbols.m_fragment_shader_symbols[tp])
        {
          add_symbol(astral::gl::detail::fragment_shader_stage, name, tp);
        }
    }

  /* add the varyings from symbols */
  for (unsigned int i = 0; i < astral::gl::ShaderVaryings::interpolator_number_types; ++i)
    {
      enum astral::gl::ShaderVaryings::interpolator_type_t tp;

      tp = static_cast<enum astral::gl::ShaderVaryings::interpolator_type_t>(i);
      for (const std::string &name : symbols.m_varyings.varyings(tp))
        {
          add_symbol(astral::gl::detail::vertex_shader_stage, name, tp);
          add_symbol(astral::gl::detail::fragment_shader_stage, name, tp);
        }
    }

  /* add the aliases of symbols */
  for (const std::pair<std::string, std::string> &alias : symbols.m_vertex_aliases)
    {
      add_alias(astral::gl::detail::vertex_shader_stage, alias.first, alias.second);
    }

  for (const std::pair<std::string, std::string> &alias : symbols.m_fragment_aliases)
    {
      add_alias(astral::gl::detail::fragment_shader_stage, alias.first, alias.second);
    }
}

void
EquivalenceClassHoard::
generate_distilled_symbols(const astral::gl::detail::WeakNamedShaderList &weak_list,
                           const astral::gl::ShaderSymbolList &symbols,
                           astral::gl::detail::DistilledShaderSymbols *out_symbols)
{
  add_symbols(weak_list, symbols);

  out_symbols->clear();

  std::map<std::string, astral::gl::detail::VariableBacking> varying_backing;

  for (const auto &e : m_hoards[astral::gl::detail::vertex_shader_stage])
    {
      e.second->emit(true,
                     out_symbols->m_variables[astral::gl::detail::vertex_shader_stage],
                     out_symbols->m_symbol_counts[astral::gl::detail::vertex_shader_stage],
                     out_symbols->m_varying_counts,
                     varying_backing);
    }

  for (const auto &e : m_hoards[astral::gl::detail::fragment_shader_stage])
    {
      e.second->emit(false,
                     out_symbols->m_variables[astral::gl::detail::fragment_shader_stage],
                     out_symbols->m_symbol_counts[astral::gl::detail::fragment_shader_stage],
                     out_symbols->m_varying_counts,
                     varying_backing);
    }
}

///////////////////////////////////////////////////////
// astral::gl::detail::VariableBacking methods
std::string
astral::gl::detail::VariableBacking::
glsl_name(const std::string &tag, const vecN<unsigned int, ShaderSymbolList::number_symbol_type> &symbol_count) const
{
  ASTRALassert(!type().is_varying());
  return glsl_backing(tag, type().symbol_type(), symbol_count, slot());
}

///////////////////////////////////////////
// astral::gl::detail::ShaderImplementBase methods
void
astral::gl::detail::ShaderImplementBase::
ctor_body(void)
{
  /* compute the symbols coming from backed variables */
  EquivalenceClassHoard Q;

  Q.generate_distilled_symbols(m_weak_dependencies, m_raw_symbols, &m_distilled_symbols);

  /* Only take the symbols of the root shader, don't add the child
   * symbols.
   */
  for (const std::string &nm : m_raw_symbols.m_vertex_shader_locals)
    {
      m_distilled_symbols.m_local_symbols[vertex_shader_stage].push_back(nm);
    }

  for (const std::string &nm : m_raw_symbols.m_fragment_shader_locals)
    {
      m_distilled_symbols.m_local_symbols[fragment_shader_stage].push_back(nm);
    }
}

void
astral::gl::detail::ShaderImplementBase::
stream_varying_backings(const std::string &tag, BackingVaryingCount count, ShaderSource &stream)
{
  stream << "/////////////////////////////////////////\n"
         << "// Stream varying backings for " << tag << ", count = " << count.raw_values() << "\n";

  for (unsigned int i = 0; i < BackingVaryingCount::number_types; ++i)
    {
      unsigned int cnt4, r4;
      enum BackingVaryingCount::type_t t;

      t = static_cast<enum BackingVaryingCount::type_t>(i);
      cnt4 = count.value(t) >> 2u;
      r4 = count.value(t) & 3u;
      for (unsigned int which = 0u; which < cnt4; ++which)
        {
          stream << glsl_varying_qualifier(t)
                 << " astral_varying "
                 << glsl_varying_type(t, 4) << " "
                 << glsl_vecN_backing(tag, t, which)
                 << ";\n";
        }

      if (r4 > 0u)
        {
          stream << glsl_varying_qualifier(t)
                 << " astral_varying "
                 << glsl_varying_type(t, r4) << " "
                 << glsl_vecN_backing(tag, t, cnt4)
                 << ";\n";
        }
    }
}

void
astral::gl::detail::ShaderImplementBase::
stream_symbol_backings(const std::string &tag,
                       const vecN<unsigned int, ShaderSymbolList::number_symbol_type> &count,
                       ShaderSource &stream)
{
  stream << "/////////////////////////////////////////\n"
         << "// Stream symbol backings for " << tag << ", counts = " << count << "\n";
  for (unsigned int i = 0; i < ShaderSymbolList::number_symbol_type; ++i)
    {
      unsigned int sz(count[i]), cnt4, r4;
      enum ShaderSymbolList::symbol_type_t t;

      t = static_cast<enum ShaderSymbolList::symbol_type_t>(i);
      cnt4 = sz >> 2u;
      r4 = sz & 3u;

      for (unsigned int which = 0u; which < cnt4; ++which)
        {
          stream << glsl_type(t, 4u) << " "
                 << glsl_vecN_backing(tag, t, which)
                 << ";\n";
        }

      if (r4 > 0u)
        {
          stream << glsl_type(t, r4) << " "
                 << glsl_vecN_backing(tag, t, cnt4)
                 << ";\n";
        }
    }
}

void
astral::gl::detail::ShaderImplementBase::
stream_shader(const std::string &tag, enum shader_stage_t stage, const std::string &prefix,
              BackingVaryingCount varying_count,
              const vecN<unsigned int, ShaderSymbolList::number_symbol_type> &symbol_count,
              c_array<const c_string> functions, ShaderSource &dst) const
{
  ASTRALassert(prefix != "");

  dst << "//Stream it, functions = " << functions << "\n";
  stream_variable_backings(tag, stage, prefix, varying_count, symbol_count, dst);
  stream_shader_implement(stage, functions, prefix, dst);
  unstream_variable_backings(stage, prefix, dst);
}

void
astral::gl::detail::ShaderImplementBase::
stream_variable_backings(const std::string &tag, enum shader_stage_t stage, const std::string &prefix,
                         BackingVaryingCount varying_count,
                         const vecN<unsigned int, ShaderSymbolList::number_symbol_type> &symbol_count,
                         ShaderSource &dst) const
{
  std::ostringstream load_write_fcn;
  BackingVaryingCount varying_global_slot;

  if (stage == vertex_shader_stage)
    {
      load_write_fcn << "void " << prefix << "astral_" << tag << "_write_varyings(void)\n";
    }
  else
    {
      load_write_fcn << "void " << prefix << "astral_" << tag << "_load_varyings(void)\n";
    }
  load_write_fcn << "{\n";

  /* just walk through m_distilled_symbols */
  dst << "\n\n//BEGIN stream_variable_backings()\n";
  for (const auto &e : m_distilled_symbols.m_variables[stage])
    {
      if (e.first.type().is_varying())
        {
          if (!e.second.empty())
            {
              enum BackingVaryingCount::type_t t;
              std::ostringstream shadow_str;
              unsigned int slot;

              t = BackingVaryingCount::get_type(e.first.type().interpolator_type());
              slot = varying_global_slot.value(t);

              /* We are abusing glsl_vecN_backing(), all we want is to
               * differentiate between different (t, slot) pairs.
               */
              shadow_str << prefix << "_shadow_" << glsl_vecN_backing(tag, t, slot);

              std::string shadow(shadow_str.str());

              /* stream the global that holds the varying value */
              dst << glsl_scalar_type(e.first.type().interpolator_type())
                  << " " << shadow << ";\n";

              if (stage == vertex_shader_stage)
                {
                  load_write_fcn << "\t" << glsl_backing(tag, t, varying_count, slot)
                                 << " = " << interpolator_cast_to_varying(e.first.type().interpolator_type())
                                 << "(" << shadow << ");\n";
                }
              else
                {
                  load_write_fcn << "\t" << shadow << " = "
                                 << interpolator_cast_from_varying(e.first.type().interpolator_type())
                                 << "(" << glsl_backing(tag, t, varying_count, slot) << ");\n";
                }
              varying_global_slot.add_varyings(t, 1u);

              for (const std::string &nm : e.second)
                {
                  dst << "#define " << prefix << nm << " " << shadow << "\n";
                }
            }
        }
      else
        {
          std::string backing;

          backing = e.first.glsl_name(tag, symbol_count);
          for (const std::string &nm : e.second)
            {
              dst << "#define " << prefix << nm << " " << backing << "\n";
            }
        }
    }
  load_write_fcn << "}\n";
  dst << load_write_fcn.str()
      << "//END stream_variable_backings()\n";
}

void
astral::gl::detail::ShaderImplementBase::
unstream_variable_backings(enum shader_stage_t stage, const std::string &prefix,
                           ShaderSource &dst) const
{
  dst << "\n\n//BEGIN unstream_variable_backings()\n";
  for (const auto &e : m_distilled_symbols.m_variables[stage])
    {
      for (const std::string &nm : e.second)
        {
          dst << "#undef " << prefix << nm << "\n";
        }
    }
  dst << "//END unstream_variable_backings()\n";
}

void
astral::gl::detail::ShaderImplementBase::
stream_shader_implement(enum shader_stage_t stage,
                        c_array<const c_string> functions,
                        const std::string &prefix, ShaderSource &dst) const
{
  dst << "\n\n//BEGIN stream_shader_implement(prefix = " << prefix << ")\n";
  for (const auto &e : m_weak_dependencies.m_values)
    {
      e.m_shader->stream_shader_implement(stage, functions, prefix + e.m_name + "::", dst);
    }

  stream_symbols(stage, "", functions, prefix, dst);
  dst.add_source(m_src[stage]);
  unstream_symbols(stage, "", functions, prefix, dst);
  dst << "//END stream_shader_implement(prefix = " << prefix << ")\n";
}

void
astral::gl::detail::ShaderImplementBase::
stream_symbols(enum shader_stage_t stage, const std::string &label,
               c_array<const c_string> functions,
               const std::string &prefix, ShaderSource &dst) const
{
  dst << "\n\n//BEGIN stream_symbols(label = " << label << ", prefix = " << prefix << ")\n";
  for (const auto &e : m_weak_dependencies.m_values)
    {
      e.m_shader->stream_symbols(stage, label + e.m_name + "::", functions, prefix, dst);
    }

  ASTRALassert(prefix != "");

  /* stream local symbols */
  for (const std::string &e : m_distilled_symbols.m_local_symbols[stage])
    {
      dst << "#define " << label << e << " " << prefix << label << e << "\n";
    }

  for (const auto &e : m_distilled_symbols.m_variables[stage])
    {
      for (const std::string &nm : e.second)
        {
          dst << "#define " << label << nm << " " << prefix << label << nm << "\n";
        }
    }

  for (c_string e : functions)
    {
      dst << "#define " << label << e << " " << prefix << label << e << "\n";
    }
  dst << "//END stream_symbols(label = " << label << ", prefix = " << prefix << ")\n";
}

void
astral::gl::detail::ShaderImplementBase::
unstream_symbols(enum shader_stage_t stage, const std::string &label,
                 c_array<const c_string> functions,
                 const std::string &prefix, ShaderSource &dst) const
{
  dst << "\n\n//BEGIN unstream_symbols(label = " << label << ", prefix = " << prefix << ")\n";
  for (const auto &e : m_weak_dependencies.m_values)
    {
      e.m_shader->unstream_symbols(stage, label + e.m_name + "::", functions, prefix, dst);
    }

  ASTRALassert(prefix != "");

  for (const std::string &e : m_distilled_symbols.m_local_symbols[stage])
    {
      dst << "#undef " << label << e << "\n";
    }

  for (c_string e : functions)
    {
      dst << "#undef " << label << e << "\n";
    }

  for (const auto &e : m_distilled_symbols.m_variables[stage])
    {
      for (const std::string &nm : e.second)
        {
          dst << "#undef " << label << nm << "\n";
        }
    }

  dst << "//END unstream_symbols(label = " << label << ", prefix = " << prefix << ")\n";
}

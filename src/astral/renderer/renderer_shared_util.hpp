/*!
 * \file renderer_shared_util.hpp
 * \brief file renderer_shared_util.hpp
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

#ifndef ASTRAL_RENDERER_SHARED_UTIL_HPP
#define ASTRAL_RENDERER_SHARED_UTIL_HPP

#include <astral/util/vecN.hpp>
#include <astral/util/matrix.hpp>

namespace astral
{
  namespace detail
  {
    /* interface similair as set, but always O(1) speed
     * and to not allocate or deallocate when reused
     * acting on same or smaller maximum value; the object
     * does NOT contain its backing storage, the caller
     * needs to provide it and maintain it.
     */
    class CustomSet
    {
    public:
      void
      init(std::vector<unsigned int> &backing_storage,
           unsigned int max_value_plus_one)
      {
        c_array<unsigned int> storage;

        backing_storage.resize(2u * max_value_plus_one);
        storage = make_c_array(backing_storage);

        m_set_size = 0u;
        m_set = storage.sub_array(0, max_value_plus_one);
        m_locs = storage.sub_array(max_value_plus_one, max_value_plus_one);
        std::fill(m_locs.begin(), m_locs.end(), ~0u);
      }

      void
      clear(void)
      {
        for (unsigned int i = 0; i < m_set_size; ++i)
          {
            m_locs[m_set[i]] = ~0u;
          }
        m_set_size = 0;
      }

      void
      insert(unsigned int I)
      {
        ASTRALassert(I < m_locs.size());

        if(!is_element(I))
          {
            ASTRALassert(m_set_size < m_set.size());
            m_locs[I] = m_set_size;
            m_set[m_set_size] = I;
            ++m_set_size;
          }
      }

      void
      erase(unsigned int I)
      {
        ASTRALassert(I < m_locs.size());
        ASTRALassert(is_element(I));

        unsigned int loc_I;

        loc_I = m_locs[I];
        ASTRALassert(loc_I < m_set_size);
        ASTRALassert(m_set[loc_I] == I);

        if (loc_I != m_set_size - 1)
          {
            unsigned int v, loc_v;

            v = m_set[m_set_size - 1u];
            ASTRALassert(v < m_locs.size());

            loc_v = m_locs[v];
            ASTRALassert(loc_v < m_set_size);
            ASTRALassert(m_set[loc_v] == v);
            ASTRALunused(loc_v);

            m_locs[v] = loc_I;
            m_set[loc_I] = v;
          }

        m_locs[I] = ~0u;
        --m_set_size;
      }

      c_array<const unsigned int>
      elements(void) const
      {
        return m_set.sub_array(0, m_set_size);
      }

      bool
      is_element(unsigned int I)
      {
        ASTRALassert(I < m_locs.size());
        return m_locs[I] != ~0u;
      }

      bool
      empty(void) const
      {
        return m_set_size == 0u;
      }

    private:
      unsigned int m_set_size;
      c_array<unsigned int> m_set;
      c_array<unsigned int> m_locs;
    };
  }
}


#endif

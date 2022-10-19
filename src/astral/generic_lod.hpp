/*!
 * \file generic_lod.hpp
 * \brief file generic_lod.hpp
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

#ifndef ASTRAL_GENERIC_LOD_HPP
#define ASTRAL_GENERIC_LOD_HPP

#include <algorithm>
#include <vector>

#define ASTRAL_GENERIC_LOD_SUCCESSIVE_LOD_RATIO 0.25f

namespace astral
{
  namespace detail
  {
    /*!
     * Very generic LOD chain template class; it maintains a list
     * T values reverse orderer by T::error() and gives a method
     * to fetch the first element in the list whose error is no
     * more than a passed error. Requirements on the class T:
     *  - a method error() that return a float giving the error of
     *    it against what it is approximating
     *  - a method create_refinement(Args&&... args) that creates
     *    and returns a T that is a closer approximation than the
     *    object on which it was called
     *  - a method finalize() that marks that the T will not accept
     *    create_refinement() to be called.
     *  - a method finalized() that returns true if either finalize()
     *    or create_refinement() have been called.
     *  - a promise from the caller that create_refinement() is never
     *    called if finalized() returns true.
     *  - a method size() which gives a notion of how big the T is,
     *    this value is used to abort refinement if the ratio between
     *    the start object and the current is too large
     */
    template<typename T, unsigned int MaxIterations = 24u>
    class GenericLOD
    {
    public:
      /*!
       * \param max_ratio do not refine once the ratio of
       *                  a refinement against the first
       *                  element -strictly- exceeds
       *                  this value.
       */
      explicit
      GenericLOD(unsigned int max_ratio = 100u):
        m_iteration_count(0u),
        m_max_ratio(max_ratio)
      {
      }

      template<typename ...Args>
      const T&
      fetch_default(Args&&... args)
      {
        if (m_entries.empty())
          {
            m_entries.push_back(T(std::forward<Args>(args)...));
            m_max_size = m_max_ratio * m_entries.back().size();
          }
        return m_entries.front();
      }

      template<typename ...Args>
      unsigned int
      fetch_index(float tol, Args&&... args)
      {
        if (m_entries.empty())
          {
            m_entries.push_back(T(std::forward<Args>(args)...));
            m_max_size = m_max_ratio * m_entries.back().size();
          }

        if (tol <= 0.0f)
          {
            return 0;
          }

        if (m_entries.back().error() <= tol)
          {
            auto iter = std::lower_bound(m_entries.begin(), m_entries.end(), tol, &reverse_compare_error);

            ASTRALassert(iter != m_entries.end());
            ASTRALassert(iter->error() <= tol);

            return iter - m_entries.begin();
          }

        if (m_entries.back().finalized())
          {
            return m_entries.size() - 1;
          }

        while (m_iteration_count < MaxIterations
               && m_entries.back().size() <= m_max_size
               && !m_entries.back().finalized())
          {
            T v(create_refinement(m_entries.back(), std::forward<Args>(args)...));

            while (m_iteration_count < MaxIterations
                   && v.size() <= m_max_size
                   && v.error() >= ASTRAL_GENERIC_LOD_SUCCESSIVE_LOD_RATIO * m_entries.back().error())
              {
                v = create_refinement(v, std::forward<Args>(args)...);
              }

            if (v.error() < m_entries.back().error())
              {
                m_entries.push_back(v);
              }

            if (m_entries.back().error() <= tol)
              {
                return m_entries.size() - 1;
              }
          }

        if (!m_entries.back().finalized())
          {
            m_entries.back().finalize();
          }

        return m_entries.size() - 1;
      }

      template<typename ...Args>
      const T&
      fetch(float tol, Args&&... args)
      {
        unsigned int I;

        I = fetch_index(tol, std::forward<Args>(args)...);
        return m_entries[I];
      }

      c_array<const T>
      all_elements(void)
      {
        return make_c_array(m_entries);
      }

    private:
      static
      bool
      reverse_compare_error(const T &lhs, float rhs)
      {
        return lhs.error() > rhs;
      }

      template<typename ...Args>
      T
      create_refinement(T &v, Args&&... args)
      {
        ++m_iteration_count;
        return v.create_refinement(std::forward<Args>(args)...);
      }

      unsigned int m_iteration_count, m_max_ratio, m_max_size;
      std::vector<T> m_entries;
    };

  } // of namespace detail
} // of namespace astral


#endif

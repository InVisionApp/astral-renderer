/*!
 * \file renderer_cached_combined_path.hpp
 * \brief file renderer_cached_combined_path.hpp
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

#ifndef ASTRAL_RENDERER_CACHED_COMBINED_PATH_HPP
#define ASTRAL_RENDERER_CACHED_COMBINED_PATH_HPP

#include <astral/renderer/renderer.hpp>

/*!
 * A CachedCombinedPath takes a CombinedPath and caches
 *   - the transformation for each Path/AnimatedPath
 *   - the bounding box of each Path/AnimatedPath in logical coordinates
 *   - the tolerance to use for each Path/AnimatedPath when fetching data
 */
class astral::Renderer::Implement::CachedCombinedPath
{
public:
  class PerObject
  {
  public:
    /* Transformation from Path/AnimatedPath
     * coordinates to VirtualBuffer coordinates
     */
    Transformation m_buffer_transformation_path;

    /* tolerance to use when fetching approximations
     * or cooked data
     */
    float m_tol;

    /* Bounding box in logical coordinates for each
     * Path/AnimatedPath;
     */
    BoundingBox<float> m_logical_bb;

    /* Bounding box in buffer coordinates for each
     * Path/AnimatedPath;
     */
    BoundingBox<float> m_buffer_bb;

    /* if true, the object can be saefly skipped in drawing  */
    bool m_culled;
  };

  /* Set the values to that from a CombinedPath for filling */
  void
  set(float logical_tol,
      const BoundingBox<float> &buffer_region,
      const Transformation &buffer_transformation_logical,
      const CombinedPath &path)
  {
    m_logical_bb.clear();
    set_implement<Path>(logical_tol, buffer_region, buffer_transformation_logical, path, &m_static_trs);
    set_implement<AnimatedPath>(logical_tol, buffer_region, buffer_transformation_logical, path, &m_animated_trs);
  }

  /* Get the values for the indexed Path or AnimatedPath
   * \tparam T is Path or AnimatedPath
   */
  template<typename T>
  c_array<const PerObject>
  get_values(void) const { return get_values(type_tag<T>()); }

  /* Conveneiance, same as get_values<T>()[I] */
  template<typename T>
  const PerObject&
  get_value(unsigned int I) const { return get_value(type_tag<T>(), I); }

  /* Get the bounding box of the path */
  const BoundingBox<float>&
  logical_bb(void) const
  {
    return m_logical_bb;
  }

private:
  c_array<const PerObject>
  get_values(type_tag<Path>) const
  {
    return make_c_array(m_static_trs);
  }

  const PerObject&
  get_value(type_tag<Path>, unsigned int I) const
  {
    ASTRALassert(I < m_static_trs.size());
    return m_static_trs[I];
  }

  /* Fetch the values for the AnimatedPath values */
  c_array<const PerObject>
  get_values(type_tag<AnimatedPath>) const
  {
    return make_c_array(m_animated_trs);
  }

  const PerObject&
  get_value(type_tag<AnimatedPath>, unsigned int I) const
  {
    ASTRALassert(I < m_animated_trs.size());
    return m_animated_trs[I];
  }

  template<typename T>
  void
  set_implement(float logical_tol,
                const BoundingBox<float> &buffer_region,
                const Transformation &buffer_transformation_logical,
                const CombinedPath &combined_path, std::vector<PerObject> *dst)
  {
    auto paths(combined_path.paths<T>());

    dst->clear();
    dst->resize(paths.size());
    for (unsigned int i = 0; i < paths.size(); ++i)
      {
        const float2x2 *matrix(combined_path.get_matrix<T>(i));
        const vec2 *translate(combined_path.get_translate<T>(i));
        float t(combined_path.get_t<T>(i));
        Transformation tr;

        (*dst)[i].m_tol = logical_tol;
        (*dst)[i].m_buffer_transformation_path = buffer_transformation_logical;
        if (translate)
          {
            (*dst)[i].m_buffer_transformation_path.translate(*translate);
            tr.m_translate = *translate;
          }

        if (matrix)
          {
            (*dst)[i].m_buffer_transformation_path.m_matrix = (*dst)[i].m_buffer_transformation_path.m_matrix * (*matrix);
            tr.m_matrix = *matrix;

            vec2 svd(compute_singular_values(*matrix));
            (*dst)[i].m_tol /= svd.x();
          }

        (*dst)[i].m_logical_bb = tr.apply_to_bb(paths[i]->bounding_box(t));
        (*dst)[i].m_buffer_bb = (*dst)[i].m_buffer_transformation_path.apply_to_bb(paths[i]->bounding_box(t));
        (*dst)[i].m_culled = !buffer_region.intersects((*dst)[i].m_buffer_bb);

        m_logical_bb.union_box((*dst)[i].m_logical_bb);
      }
  }

  std::vector<PerObject> m_static_trs, m_animated_trs;
  BoundingBox<float> m_logical_bb;
};

#endif

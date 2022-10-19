/*!
 * \file renderer_stc_data_builder.hpp
 * \brief file renderer_stc_data_builder.hpp
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

#ifndef ASTRAL_RENDERER_STC_DATA_BUILDER_HPP
#define ASTRAL_RENDERER_STC_DATA_BUILDER_HPP

#include <astral/renderer/renderer.hpp>
#include "renderer_stc_data.hpp"
#include "renderer_virtual_buffer.hpp"
#include "renderer_cached_combined_path.hpp"

/* Class that provides the implementation to make
 * BuilderSet add_stc() calls from CombinedPath
 * input data.
 */
class astral::Renderer::Implement::STCData::BuilderSet::Helper
{
public:
  Helper(BuilderSet &dst):
    m_dst(dst)
  {}

  /* Add the STCData of a CombinedPath wit the current transformation applied
   * \param virtual_buffer VirtualBuffer used to provide current transformation
   * \param combined_path list of Path and AnimatedPath objects to add
   * \param ct specifies nature of tessellation
   * \param aa_mode determines if anti-alias fuzz of the data is to be added
   */
  void
  add_stc_path(VirtualBuffer &virtual_buffer,
               const CombinedPath &combined_path,
               enum contour_fill_approximation_t ct,
               enum anti_alias_t aa_mode) const
  {
    add_stc_path_implement<Path>(virtual_buffer, combined_path, ct, aa_mode);
    add_stc_path_implement<AnimatedPath>(virtual_buffer, combined_path, ct, aa_mode);
  }

  /* Add the STCData of a CombinedPath but use a CachedCombinedPath
   * for culling and to store the transformation values
   * \param virtual_buffer VirtualBuffer used to provide current transformation
   * \param combined_path list of Path and AnimatedPath objects to add
   * \param ct specifies nature of tessellation
   * \param aa_mode determines if anti-alias fuzz of the data is to be added
   * \param cached_values object made with combined_path that holds the
   *                      transformation values from each element of combined_path
   *                      to pixel coordinates
   */
  void
  add_stc_path(VirtualBuffer &virtual_buffer,
               const CombinedPath &combined_path,
               enum contour_fill_approximation_t ct,
               enum anti_alias_t aa_mode,
               const CachedCombinedPath &cached_values) const
  {
    add_stc_path_implement<Path>(virtual_buffer, combined_path, ct, aa_mode, cached_values);
    add_stc_path_implement<AnimatedPath>(virtual_buffer, combined_path, ct, aa_mode, cached_values);
  }

private:

  /* Impementatin of adding STCData of a CombinedPath
   * \tparam T is either Path or AnimatedPath to indicates if to add
   *           the Path's or AnimatedPath's STC data
   */
  template<typename T>
  void
  add_stc_path_implement(VirtualBuffer &virtual_buffer,
                         const CombinedPath &combined_path,
                         enum contour_fill_approximation_t ct,
                         enum anti_alias_t aa_mode) const
  {
    auto paths(combined_path.paths<T>());
    for (unsigned int i = 0; i < paths.size(); ++i)
      {
        float t(combined_path.get_t<T>(i));
        const vec2 *translate(combined_path.get_translate<T>(i));
        const float2x2 *matrix(combined_path.get_matrix<T>(i));
        float tol;
        RenderValue<Transformation> tr;
        ItemData item_data;

        tol = virtual_buffer.compute_tol(matrix);
        tr = virtual_buffer.create_transformation(translate, matrix);
        item_data = generate_stc_fill_item_data(virtual_buffer, t);

        for (unsigned int c = 0, endc = paths[i]->number_contours(); c < endc; ++c)
          {
            const auto &D(paths[i]->contour(c).fill_render_data(tol, *virtual_buffer.m_renderer.m_engine, ct));
            BoundingBox<float> bb;

            bb = tr.value().apply_to_bb(paths[i]->contour(c).bounding_box(t));
            m_dst.add_stc(D, aa_mode, tr, item_data, &bb);
          }
      }
  }

  /* Add the STCData of a CombinedPath but use a CachedCombinedPath
   * to cull Path or AnimatedPath objects and the cached transformation
   * values from the CachedCombinedPath
   * \tparam T is either Path or AnimatedPath to indicates if to add
   *           the Path's or AnimatedPath's STC data
   */
  template<typename T>
  void
  add_stc_path_implement(VirtualBuffer &virtual_buffer,
                         const CombinedPath &combined_path,
                         enum contour_fill_approximation_t ct,
                         enum anti_alias_t aa_mode,
                         const CachedCombinedPath &cached_values) const
  {
    auto paths(combined_path.paths<T>());

    ASTRALassert(paths.size() == cached_values.get_values<T>().size());
    for (unsigned int i = 0; i < paths.size(); ++i)
      {
        const auto &value(cached_values.get_value<T>(i));

        if (!value.m_culled)
          {
            float t(combined_path.get_t<T>(i));
            Transformation tr_value;
            RenderValue<Transformation> tr;
            ItemData item_data;

            tr_value = value.m_buffer_transformation_path;
            tr = virtual_buffer.m_renderer.create_value(tr_value);
            item_data = generate_stc_fill_item_data(virtual_buffer, t);
            for (unsigned int c = 0, endc = paths[i]->number_contours(); c < endc; ++c)
              {
                float tol(value.m_tol);
                const auto &D(paths[i]->contour(c).fill_render_data(tol, *virtual_buffer.m_renderer.m_engine, ct));
                BoundingBox<float> bb;

                bb = tr_value.apply_to_bb(paths[i]->contour(c).bounding_box(t));
                m_dst.add_stc(D, aa_mode, tr, item_data, &bb);
              }
          }
      }
  }

  /* Conveniance function to generate an ItemData
   * used by an STCData to render its contents.
   */
  ItemData
  generate_stc_fill_item_data(VirtualBuffer &virtual_buffer, float time) const
  {
    vecN<gvec4, FillSTCShader::item_data_size> data;

    FillSTCShader::pack_item_data(time, virtual_buffer.scale_factor(), data);
    return virtual_buffer.m_renderer.create_item_data(data, no_item_data_value_mapping);
  }

  BuilderSet &m_dst;
};

#endif

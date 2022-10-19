/*!
 * \file render_encoder_shadowmap_util.hpp
 * \brief file render_encoder_shadowmap_util.hpp
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

#ifndef ASTRAL_RENDER_ENCODER_SHADOWMAP_UTIL_HPP
#define ASTRAL_RENDER_ENCODER_SHADOWMAP_UTIL_HPP

#include <astral/renderer/renderer.hpp>

namespace astral
{
  namespace detail
  {
    template<typename T>
    void
    add_shadowmap_path_implement(RenderEncoderShadowMap encoder,
                                 const CombinedPath &paths,
                                 bool include_implicit_closing_edge);
  }
}

template<typename T>
void
astral::detail::
add_shadowmap_path_implement(RenderEncoderShadowMap encoder,
                             const CombinedPath &combined_path,
                             bool include_implicit_closing_edge)
{
  auto paths(combined_path.paths<T>());
  ItemData item_data;
  float last_t(0.0f);
  const ShadowMapGeneratorShader &shadow_shaders(encoder.default_shaders().m_shadow_map_generator_shader);

  for (unsigned int i = 0; i < paths.size(); ++i)
    {
      float t(combined_path.get_t<T>(i));
      const vec2 *translate(combined_path.get_translate<T>(i));
      const float2x2 *matrix(combined_path.get_matrix<T>(i));
      float tol;

      tol = encoder.compute_tolerance(matrix);
      if (matrix || translate)
        {
          encoder.save_transformation();
          if (translate)
            {
              encoder.translate(*translate);
            }

          if (matrix)
            {
              encoder.concat(*matrix);
            }
        }

      if (!item_data.valid() || last_t != t)
        {
          vecN<gvec4, ShadowMapGeneratorShader::item_data_size> data;

          ShadowMapGeneratorShader::pack_item_data(t, data);
          item_data = encoder.create_item_data(data, no_item_data_value_mapping);
        }

      for (unsigned int c = 0, endc = paths[i]->number_contours(); c < endc; ++c)
        {
          const auto &data(paths[i]->contour(c).fill_render_data(tol, encoder.render_engine(), contour_fill_approximation_allow_long_curves));
          for (unsigned int s = 0; s < ShadowMapGeneratorShader::number_side_pair; ++s)
            {
              enum ShadowMapGeneratorShader::primitive_type_t P;
              enum ShadowMapGeneratorShader::side_pair_t S;

              S = static_cast<enum ShadowMapGeneratorShader::side_pair_t>(s);
              P = ShadowMapGeneratorShader::line_segment_primitive;
                {
                  vecN<range_type<int>, 1> V;
                  const ShadowMapItemShader &shader(*shadow_shaders.shader(P, S));

                  if (include_implicit_closing_edge)
                    {
                      V[0] = data.m_pass_range[FillSTCShader::pass_contour_fuzz];
                    }
                  else
                    {
                      V[0] = data.m_aa_line_pass_without_implicit_closing_edge;
                    }
                  RenderSupportTypes::Item<ShadowMapItemShader> item(shader, item_data, *data.m_vertex_data, V);

                  encoder.draw_generic(item);
                }

              P = ShadowMapGeneratorShader::conic_triangle_primitive;
                {
                  vecN<range_type<int>, 1> V(data.m_pass_range[FillSTCShader::pass_conic_triangle_fuzz]);
                  const ShadowMapItemShader &shader(*shadow_shaders.shader(P, S));
                  RenderSupportTypes::Item<ShadowMapItemShader> item(shader, item_data, *data.m_vertex_data, V);

                  encoder.draw_generic(item);
                }

            }
        }

      if (matrix || translate)
        {
          encoder.restore_transformation();
        }
    }
}

#endif

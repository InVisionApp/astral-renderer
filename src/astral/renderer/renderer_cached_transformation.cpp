/*!
 * \file renderer_cached_transformation.cpp
 * \brief file renderer_cached_transformation.cpp
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

#include "renderer_cached_transformation.hpp"

////////////////////////////////////////////////
// astral::Render::CachedTransformation methods
astral::RenderValue<astral::Transformation>
astral::Renderer::Implement::CachedTransformation::
render_value(Renderer &renderer,
             const Transformation *post_transformation) const
{
  if (!m_cached_transformation.valid())
    {
      if (post_transformation)
        {
          m_cached_transformation = renderer.create_value(*post_transformation * m_transformation);
        }
      else
        {
          m_cached_transformation = renderer.create_value(m_transformation);
        }
    }
  return m_cached_transformation;
}

void
astral::Renderer::Implement::CachedTransformation::
transformation(RenderValue<Transformation> v)
{
  reset(true, true);
  if (v.valid())
    {
      transformation(v.value());
    }
  else
    {
      transformation(Transformation());

      m_inverse_ready = true;
      m_inverse = Transformation();

      m_singular_values_ready = true;
      m_singular_values = vec2(1.0f, 1.0f);

      m_matrix_type_ready = true;
      m_matrix_type = matrix_diagonal;
    }
}

astral::vecN<float, 2>
astral::Renderer::Implement::CachedTransformation::
singular_values(void) const
{
  if (!m_singular_values_ready)
    {
      m_singular_values_ready = true;
      m_singular_values = compute_singular_values(m_transformation.m_matrix, matrix_type());
    }
  return m_singular_values;
}

float
astral::Renderer::Implement::CachedTransformation::
surface_pixel_size_in_logical_coordinates(vec2 render_scale_factor) const
{
  if (render_scale_factor != m_last_render_scale_factor || !m_pixel_size_ready)
    {
      m_pixel_size_ready = true;
      m_last_render_scale_factor = render_scale_factor;

      float2x2 scale_tr, final_tr;
      vec2 sv;

      scale_tr.row_col(0, 0) = render_scale_factor.x();
      scale_tr.row_col(1, 1) = render_scale_factor.y();
      final_tr = scale_tr * m_transformation.m_matrix;

      /* multiplying by a diagnol matrix preserves the matrix type */
      sv = compute_singular_values(final_tr, matrix_type());

      /* protect against degenerate matrices */
      const float tiny(1e-6);
      m_pixel_size = (sv.y() > tiny) ? 1.0f / sv.y() : 0.0f;
    }

  return m_pixel_size;
}

const astral::Transformation&
astral::Renderer::Implement::CachedTransformation::
inverse(void) const
{
  if (!m_inverse_ready)
    {
      m_inverse_ready = true;
      m_inverse = m_transformation.inverse();
    }
  return m_inverse;
}

/*!
 * \file renderer_uber_shading_key_collection.cpp
 * \brief file renderer_uber_shading_key_collection.cpp
 *
 * Copyright 2022 by InvisionApp.
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

#include "renderer_uber_shading_key_collection.hpp"

astral::RenderBackend::UberShadingKey&
astral::Renderer::Implement::UberShadingKeyCollection::
scratch(RenderBackend &backend)
{
  if (!m_scratch)
    {
      m_scratch = backend.create_uber_shading_key();
    }

  return *m_scratch;
}

astral::RenderBackend::UberShadingKey::Cookie
astral::Renderer::Implement::UberShadingKeyCollection::
generate_stroke_uber(Renderer::Implement &implement,
                     enum clip_window_value_type_t shader_clipping,
                     const DirectStrokeShader::ShaderSet &shader,
                     const MaterialShader *material_shader,
                     BackendBlendMode blend_mode)
{
  RenderBackend::UberShadingKey &K(scratch(*implement.m_backend));

  K.begin_accumulate(shader_clipping, uber_shader_active);
  for (unsigned int P = 0; P < DirectStrokeShader::path_shader_count; ++P)
    {
      const DirectStrokeShader::ItemShaderSet &iss(shader.m_subset[P]);

      if (iss.m_line_segment_shader)
        {
          K.add_shader(*iss.m_line_segment_shader, material_shader, blend_mode);
        }

      if (iss.m_biarc_curve_shader)
        {
          K.add_shader(*iss.m_biarc_curve_shader, material_shader, blend_mode);
        }

      for (unsigned int J = 0; J < number_join_t; ++J)
        {
          if (iss.m_join_shaders[J])
            {
              K.add_shader(*iss.m_join_shaders[J], material_shader, blend_mode);
            }
        }

      if (iss.m_inner_glue_shader)
        {
          K.add_shader(*iss.m_inner_glue_shader, material_shader, blend_mode);
        }

      for (unsigned int C = 0; C < DirectStrokeShader::number_capper_shader; ++C)
        {
          if (iss.m_line_capper_shaders[C])
            {
              K.add_shader(*iss.m_line_capper_shaders[C], material_shader, blend_mode);
            }

          if (iss.m_quadratic_capper_shaders[C])
            {
              K.add_shader(*iss.m_quadratic_capper_shaders[C], material_shader, blend_mode);
            }
        }

      if (iss.m_cap_shader)
        {
          K.add_shader(*iss.m_cap_shader, material_shader, blend_mode);
        }
    }

  K.end_accumulate();

  return K.cookie();
}

astral::RenderBackend::UberShadingKey::Cookie
astral::Renderer::Implement::UberShadingKeyCollection::
stroke_uber(Renderer::Implement &implement,
            enum clip_window_value_type_t shader_clipping,
            const DirectStrokeShader &stroke_shader,
            const MaterialShader *material_shader,
            enum cap_t C, BackendBlendMode blend_mode)
{
  PerShaderClippingValue &P(m_per_shader_clipping[shader_clipping]);
  unsigned int S, M, B;

  S = stroke_shader.unique_id();
  if (S >= P.m_stroke_ubers.size())
    {
      P.m_stroke_ubers.resize(S + 1);
    }

  if (!material_shader)
    {
      material_shader = implement.m_default_shaders.m_brush_shader.get();
    }

  B = blend_mode.packed_value();
  M = material_shader->root_unique_id();
  if (M >= P.m_stroke_ubers[S].m_per_shader_set[C].m_per_material.size())
    {
      P.m_stroke_ubers[S].m_per_shader_set[C].m_per_material.resize(M + 1);
    }

  if (!P.m_stroke_ubers[S].m_per_shader_set[C].m_per_material[M].m_entries[B].m_ready)
    {
      P.m_stroke_ubers[S].m_per_shader_set[C].m_per_material[M].m_entries[B].m_ready = true;
      P.m_stroke_ubers[S].m_per_shader_set[C].m_per_material[M].m_entries[B].m_value = generate_stroke_uber(implement, shader_clipping,
                                                                                                            stroke_shader.shader_set(C),
                                                                                                            material_shader,
                                                                                                            blend_mode);
    }

  return P.m_stroke_ubers[S].m_per_shader_set[C].m_per_material[M].m_entries[B].m_value;
}

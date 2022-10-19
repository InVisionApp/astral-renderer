/*!
 * \file renderer_uber_shading_key_collection.hpp
 * \brief file renderer_uber_shading_key_collection.hpp
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

#ifndef ASTRAL_RENDERER_UBER_SHADING_KEY_COLLECTION_HPP
#define ASTRAL_RENDERER_UBER_SHADING_KEY_COLLECTION_HPP

#include <astral/renderer/renderer.hpp>
#include <astral/renderer/backend/render_backend.hpp>

#include "renderer_implement.hpp"

/* For drawing masks and direct stroking, multiple shaders
 * are involved. To reduce shader switches when no uber-shading
 * is active, we use a smaller uber-shader that just includes
 * those shaders for the mask draw or direct stroke. The
 * uber-shader to use is keyed by the (material_shader, blend_mode).
 * To make life simple, we use a two dimensionsal array
 * and store the cookie value.
 */
class astral::Renderer::Implement::UberShadingKeyCollection:astral::noncopyable
{
public:
  /* Returns the uber-shading key to use for direct stroking
   * \param implement the Renderer::Implement that spawns it all
   * \param shader_clipping if and how shader is to include shader-clipping
   * \param stroke_shader the stroke shader to use
   * \param material_shader what material shader to apply, a nullptr
   *                        value indicates to use the default brush
   * \param C the cap style to apply
   * \param blend_mode what blend mode
   */
  RenderBackend::UberShadingKey::Cookie
  stroke_uber(Renderer::Implement &implement,
              enum clip_window_value_type_t shader_clipping,
              const DirectStrokeShader &stroke_shader,
              const MaterialShader *material_shader,
              enum cap_t C, BackendBlendMode blend_mode);

private:
  class Entry
  {
  public:
    Entry(void):
      m_ready(false)
    {}

    /* if the Entry has been computed */
    bool m_ready;

    /* cookie, an invalid cookie means that the
     * underlying backend says that there is no
     * uber-shading needed since there was only
     * one real shader.
     */
    RenderBackend::UberShadingKey::Cookie m_value;
  };

  class PerRootMaterialShader
  {
  public:
    /* m_entries[blend_mode] gives the uber-shader cookie
     * for the named blend-mode.
     */
    vecN<Entry, BackendBlendMode::number_packed_values> m_entries;
  };

  class PerStrokeShaderSet
  {
  public:
    /* m_per_material[M] gives the PerRootMaterialShader
     * to use for a named DirectStrokeShader::ShaderSet
     */
    std::vector<PerRootMaterialShader> m_per_material;
  };

  class PerStrokeShader
  {
  public:
    /* m_per_shader_set[C] gives the PerStrokeShaderSet
     * to use for a cap style C
     */
    vecN<PerStrokeShaderSet, number_cap_t> m_per_shader_set;
  };

  class PerShaderClippingValue
  {
  public:
    /* m_stroke_ubers[S].m_per_shader_set[C].m_per_material[M].m_entries[B]
     * is the Entry to use for S = DirectStrokeShader::unique_id()
     * M = MaterialShader::root_unique_id(), C = cap-style, B = blend-mode
     */
    std::vector<PerStrokeShader> m_stroke_ubers;
  };

  RenderBackend::UberShadingKey&
  scratch(RenderBackend &backend);

  RenderBackend::UberShadingKey::Cookie
  generate_stroke_uber(Renderer::Implement &implement,
                       enum clip_window_value_type_t shader_clipping,
                       const DirectStrokeShader::ShaderSet &shader,
                       const MaterialShader *material_shader,
                       BackendBlendMode blend_mode);

  reference_counted_ptr<RenderBackend::UberShadingKey> m_scratch;

  vecN<PerShaderClippingValue, clip_window_value_type_count> m_per_shader_clipping;
};

#endif

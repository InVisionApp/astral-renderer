/*!
 * \file render_backend.cpp
 * \brief file render_backend.cpp
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

#include <iostream>
#include <astral/renderer/backend/render_backend.hpp>
#include <astral/renderer/render_engine.hpp>

///////////////////////////////
// astral::RenderBackend methods
astral::RenderBackend::
RenderBackend(RenderEngine &engine):
  m_engine(&engine),
  m_rendering(false),
  m_number_renders(0)
{
  m_brush_shader = m_engine->default_shaders().m_brush_shader;
}

astral::RenderBackend::
~RenderBackend()
{
  ASTRALassert(!m_rendering);
  ASTRALassert(!m_current_rt);
}

astral::c_string
astral::RenderBackend::
render_stats_label(unsigned int idx) const
{
  static const c_string labels[number_render_stats] =
    {
      [stats_number_draws] = "backend_number_draws",
      [stats_vertices] = "backend_number_vertices",
      [stats_render_targets] = "backend_number_render_targets",
      [stats_vertex_backing_size] = "backend_vertex_backing_size",
      [stats_vertics_on_store] = "backend_vertices_on_store",
      [stats_static_data32_backing_size] = "backend_static_data_backing32_size",
      [stats_static_data32_on_store] = "backend_static_data32_on_store",
      [stats_static_data16_backing_size] = "backend_static_data16_backing_size",
      [stats_static_data16_on_store] = "backend_static_data16_on_store",
    };
  c_string return_value;

  if (idx < number_render_stats)
    {
      return_value = labels[idx];
    }
  else
    {
      return_value = render_stats_label_derived(idx - number_render_stats);
    }

  ASTRALassert(return_value);
  return return_value;
}

unsigned int
astral::RenderBackend::
render_stats_size(void) const
{
  return number_render_stats + render_stats_size_derived();
}

unsigned int
astral::RenderBackend::
stat_index(enum render_backend_stats_t st) const
{
  return st;
}

unsigned int
astral::RenderBackend::
stat_index(DerivedStat st) const
{
  return st.m_value + number_render_stats;
}

void
astral::RenderBackend::
begin(void)
{
  ASTRALassert(!m_current_rt);
  ASTRALassert(!m_rendering);

  m_rendering = true;
  std::fill(m_base_stats.begin(), m_base_stats.end(), 0);
  on_begin();
}

void
astral::RenderBackend::
end(c_array<unsigned int> out_stats)
{
  ASTRALassert(m_rendering);
  ASTRALassert(!m_current_rt);

  VertexDataAllocator &va(m_engine->vertex_data_allocator());
  const VertexDataBacking &vertex_backing(va.backing());

  m_base_stats[stats_vertex_backing_size] = vertex_backing.num_vertices();
  m_base_stats[stats_vertics_on_store] = va.number_vertices_allocated();

  StaticDataAllocator32 &sd32(m_engine->static_data_allocator32());
  const StaticDataBacking &sd32_backing(sd32.backing());

  m_base_stats[stats_static_data32_backing_size] = sd32_backing.size();
  m_base_stats[stats_static_data32_on_store] = sd32.amount_allocated();

  StaticDataAllocator16 &sd16(m_engine->static_data_allocator16());
  const StaticDataBacking &sd16_backing(sd16.backing());

  m_base_stats[stats_static_data16_backing_size] = sd16_backing.size();
  m_base_stats[stats_static_data16_on_store] = sd16.amount_allocated();

  ASTRALassert(out_stats.size() >= render_stats_size());
  std::copy(m_base_stats.begin(), m_base_stats.end(), out_stats.begin());

  on_end(out_stats.sub_array(number_render_stats));
  ++m_number_renders;
  m_rendering = false;
}

void
astral::RenderBackend::
begin_render_target(const ClearParams &clear_params, RenderTarget &rt)
{
  ASTRALassert(m_rendering);
  ASTRALassert(!m_current_rt);

  ++m_base_stats[stats_render_targets];
  m_current_rt = &rt;

  rt.active_status(detail::RenderTargetRenderBackendStatus(this));
  on_begin_render_target(clear_params, rt);
}

void
astral::RenderBackend::
end_render_target(void)
{
  ASTRALassert(m_rendering);
  ASTRALassert(m_current_rt);
  ASTRALassert(m_current_rt->active_status(detail::RenderTargetRenderBackendStatusQuery()) == this);

  m_current_rt->active_status(detail::RenderTargetRenderBackendStatus(nullptr));
  on_end_render_target(*m_current_rt);
  m_current_rt = nullptr;
}

void
astral::RenderBackend::
draw_render_data(unsigned int z,
                 const ItemShader &shader,
                 const RenderValues &st,
                 UberShadingKey::Cookie uber_shader_cookie,
                 RenderValue<ScaleTranslate> tr,
                 ClipWindowValue cl,
                 bool permute_xy,
                 c_array<const range_type<int>> Rs)
{
  pointer<const ItemShader> shader_ptr(&shader);
  c_array<const pointer<const ItemShader>> shaders(&shader_ptr, 1);

  m_tmp_R.resize(Rs.size());
  for (unsigned int i = 0; i < Rs.size(); ++i)
    {
      m_tmp_R[i].first = 0;
      m_tmp_R[i].second = Rs[i];
    }

  draw_render_data(z, shaders, st, uber_shader_cookie, tr, cl, permute_xy, make_c_array(m_tmp_R));
}

void
astral::RenderBackend::
draw_render_data(unsigned int z,
                 c_array<const pointer<const ItemShader>> shaders,
                 const RenderValues &st,
                 UberShadingKey::Cookie uber_shader_cookie,
                 RenderValue<ScaleTranslate> tr,
                 ClipWindowValue cl,
                 bool permute_xy,
                 c_array<const std::pair<unsigned int, range_type<int>>> Rs)
{
  ASTRALassert(m_rendering);
  if (Rs.empty())
    {
      return;
    }

  on_draw_render_data(z, shaders, st, uber_shader_cookie, tr, cl, permute_xy, Rs);
  ++m_base_stats[stats_number_draws];

  for (const auto &R : Rs)
    {
      m_base_stats[stats_vertices] += R.second.difference();
    }
}

astral::RenderValue<astral::Brush>
astral::RenderBackend::
create_value(Brush value)
{
  RenderValue<Brush> R;

  ASTRALassert(m_rendering);
  if (!value.m_opaque)
    {
      value.m_opaque = value.m_base_color.w() >= 1.0f;
      if (value.m_opaque && value.m_image.valid())
        {
          const ImageSampler &im(fetch(value.m_image));
          value.m_opaque = (im.color_post_sampling_mode() & color_post_sampling_mode_bits_alpha_one) != 0.0
            || (im.image_opaque()
                && im.x_tile_mode() != tile_mode_decal
                && im.y_tile_mode() != tile_mode_decal);
        }

      if (value.m_opaque && value.m_gradient.valid())
        {
          const Gradient &gr(fetch(value.m_gradient));

          ASTRALassert(gr.m_colorstops);
          value.m_opaque = gr.m_colorstops->opaque()
            && gr.m_type != Gradient::radial_unextended_clear
            && gr.m_interpolate_tile_mode != tile_mode_decal;
        }
    }

  R.init(allocate_render_brush(value), *this);
  return R;
}

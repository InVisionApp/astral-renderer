/*!
 * \file renderer_draw_command.cpp
 * \brief file renderer_draw_command.cpp
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

#include "renderer_draw_command.hpp"
#include "renderer_storage.hpp"
#include "renderer_workroom.hpp"

/////////////////////////////////////////////////////////
// astral::Renderer::Implement::DrawCommandVerticesShaders methods
class astral::Renderer::Implement::DrawCommandVerticesShaders::Helper
{
public:
  static
  void
  set_color_shader_properties(DrawCommandVerticesShaders *dst, const ColorItemShader &shader)
  {
    const ColorItemShader::Properties &shader_props(shader.properties());

    dst->m_an_item_shader_emits_partially_covered_fragments = shader_props.m_emits_partially_covered_fragments;
    dst->m_an_item_shader_emits_transparent_fragments = shader_props.m_emits_transparent_fragments;
  }

  static
  void
  set_color_shader_properties(DrawCommandVerticesShaders *dst, const MaskItemShader&)
  {
    dst->m_an_item_shader_emits_partially_covered_fragments = false;
    dst->m_an_item_shader_emits_transparent_fragments = false;
  }

  static
  void
  set_color_shader_properties(DrawCommandVerticesShaders *dst, const ShadowMapItemShader&)
  {
    dst->m_an_item_shader_emits_partially_covered_fragments = false;
    dst->m_an_item_shader_emits_transparent_fragments = false;
  }

  template<typename T>
  static
  void
  fill_fields(DrawCommandVerticesShaders *dst, Storage &storage, const RenderSupportTypes::Item<T> &item)
  {
    dst->m_shaders = storage.allocate_shader_ptr(item.m_shader);
    dst->m_vertex_range = storage.allocate_vertex_ranges(item);
    dst->m_only_shader = &item.m_shader;
    set_color_shader_properties(dst, item.m_shader);
  }
};

astral::Renderer::Implement::DrawCommandVerticesShaders::
DrawCommandVerticesShaders(Storage &storage, const RenderSupportTypes::Item<ColorItemShader> &item):
  m_shader_type(ItemShader::color_item_shader)
{
  Helper::fill_fields<ColorItemShader>(this, storage, item);
}

astral::Renderer::Implement::DrawCommandVerticesShaders::
DrawCommandVerticesShaders(Storage &storage, const RenderSupportTypes::Item<MaskItemShader> &item):
  m_shader_type(ItemShader::mask_item_shader)
{
  Helper::fill_fields<MaskItemShader>(this, storage, item);
}

astral::Renderer::Implement::DrawCommandVerticesShaders::
DrawCommandVerticesShaders(Storage &storage, const RenderSupportTypes::Item<ShadowMapItemShader> &item):
  m_shader_type(ItemShader::shadow_map_item_shader)
{
  Helper::fill_fields<ShadowMapItemShader>(this, storage, item);
}

astral::Renderer::Implement::DrawCommandVerticesShaders::
DrawCommandVerticesShaders(Storage &storage, const RenderSupportTypes::ColorItem &item):
  m_only_shader(nullptr),
  m_an_item_shader_emits_partially_covered_fragments(item.emits_partially_covered_fragments()),
  m_an_item_shader_emits_transparent_fragments(item.emits_transparent_fragments()),
  m_shader_type(ItemShader::color_item_shader)
{
  m_shaders = storage.allocate_shader_ptrs(item.m_shaders);
  m_vertex_range = storage.allocate_vertex_ranges(item.m_vertex_datas, item.m_sub_items);
}

/////////////////////////////////////////
// astral::Renderer::Implement::DrawCommand methods
void
astral::Renderer::Implement::DrawCommand::
send_to_backend(Renderer::Implement &renderer,
                RenderBackend::UberShadingKey::Cookie uber_shader_key,
                RenderValue<ScaleTranslate> tr,
                RenderBackend::ClipWindowValue cl,
                unsigned int add_z,
                bool permute_xy) const
{
  ASTRALassert(m_vertices_and_shaders.m_vertex_range.m_begin < m_vertices_and_shaders.m_vertex_range.m_end);
  if (!uber_shader_key.valid())
    {
      uber_shader_key = m_sub_uber_shader_key[cl.clip_window_value_type()];
    }

  if (!m_draw_deleted)
    {
      renderer.m_backend->draw_render_data(m_z + add_z,
                                           renderer.m_storage->fetch_shader_ptrs(m_vertices_and_shaders.m_shaders),
                                           m_render_values, uber_shader_key, tr, cl, permute_xy,
                                           renderer.m_storage->fetch_vertex_ranges(m_vertices_and_shaders.m_vertex_range));
    }
}

////////////////////////////////////////////////////
// astral::Renderer::Implement::DrawCommandList::HitDetectionElement methods
astral::Renderer::Implement::DrawCommandList::HitDetectionElement::
HitDetectionElement(Storage *store, const BoundingBox<float> &bb, unsigned int generation):
  m_children_spawned(false),
  m_generation(generation),
  m_children(nullptr),
  m_bb(bb)
{
  ASTRALassert(store);
  m_draws = store->allocate_unsigned_int_array();
}

bool
astral::Renderer::Implement::DrawCommandList::HitDetectionElement::
should_spawn(void) const
{
  /* Running
   *  ./snapshot_testGL-release swap_interval 0 num_grid_y 60 num_grid_linear_gradient_rects 20 num_grid_radial_gradient_rects 20 num_grid_sweep_gradient_rects 20 num_bg_rects_x 20 num_bg_rects_y 20 bg_rect_spacing 0.25 show_framerate true
   *
   * Gives (max_size, max_generation):
   *
   * 15, 8: ms/frame = 12.5363
   * 10, 10: ms/frame = 12.5363
   * 20, 10: ms/frame = 12.482
   * 20, 20: ms/frame = 12.503
   *  8, 10: ms/frame = 14.8389
   *  8, 20: ms/frame = 14.9491
   * 15, 20: ms/frame = 12.5115
  */

  /* maximum number of rects before a split */
  const unsigned int max_size = 30;

  /* maximum generation allowed */
  const unsigned int max_generation = 5;

  return !m_children_spawned
    && m_draws->size() >= max_size
    && m_generation < max_generation;
}

void
astral::Renderer::Implement::DrawCommandList::HitDetectionElement::
spawn_children(DrawCommandList &list)
{
  vecN<BoundingBox<float>, 2> x_split;
  vecN<vecN<BoundingBox<float>, 2>, 2> child_bbs;
  std::vector<unsigned int> *new_draws;

  /* create the children */
  ASTRALassert(!m_children_spawned);
  m_children_spawned = true;

  x_split = m_bb.split_x();
  for (unsigned int i = 0; i < 2; ++i)
    {
      child_bbs[i] = x_split[i].split_y();
      for (unsigned int j = 0; j < 2; ++j)
        {
          unsigned int child(2u * i + j);

          m_children[child] = list.m_element_backing.create(list.m_storage, child_bbs[i][j], m_generation + 1u);
        }
    }

  /* make a new array for m_draws because add() is going
   * to add elements to it and run the old contents of m_draws
   * though add().
   */
  new_draws = list.m_storage->allocate_unsigned_int_array();
  std::swap(m_draws, new_draws);
  ASTRALassert(m_draws->empty());

  for (unsigned int I : *new_draws)
    {
      add(list, I);
    }

  /* return new_draws back to storage */
  list.m_storage->recycle_unsigned_int_array(new_draws);
}

void
astral::Renderer::Implement::DrawCommandList::HitDetectionElement::
add(DrawCommandList &list, unsigned int rect_draw_id)
{
  const RectDraw &rect_draw(list.m_processed_rect_draws[rect_draw_id]);

  ASTRALassert(rect_draw.m_rect.intersects(m_bb));
  if (m_children_spawned)
    {
      for (unsigned int i = 0; i < 4u; ++i)
        {
          HitDetectionElement *child(m_children[i]);

          if (rect_draw.m_rect.contains(child->m_bb))
            {
              /* if the draw to add completely contains the bounding
               * box of a child, then we do NOT add it the child
               * and instead add it to this and immediately return
               */
              m_draws->push_back(rect_draw_id);
              return;
            }
          else if (rect_draw.m_rect.intersects(child->m_bb))
            {
              child->add(list, rect_draw_id);
            }
        }
    }
  else
    {
      m_draws->push_back(rect_draw_id);
      if (should_spawn())
        {
          spawn_children(list);
        }
    }
}

void
astral::Renderer::Implement::DrawCommandList::HitDetectionElement::
query(DrawCommandList &list,
      RenderValue<Transformation> pixel_transformation_logical,
      const BoundingBox<float> &logical_bb,
      float logical_padding,
      bool delete_completely_contained,
      QueryResult *out_query)
{
  ASTRALassert(out_query);
  out_query->clear();

  BoundingBox<float> logical_bb_with_padding(logical_bb, vec2(logical_padding));

  if (pixel_transformation_logical.valid())
    {
      TransformedBoundingBox pixel_bb(logical_bb, pixel_transformation_logical.value());
      TransformedBoundingBox pixel_bb_with_padding(logical_bb_with_padding, pixel_transformation_logical.value());
      query_implement(list, pixel_bb, pixel_bb_with_padding, delete_completely_contained, out_query);
    }
  else
    {
      TransformedBoundingBox pixel_bb(logical_bb);
      TransformedBoundingBox pixel_bb_with_padding(logical_bb_with_padding);
      query_implement(list, pixel_bb, pixel_bb_with_padding, delete_completely_contained, out_query);
    }
}

void
astral::Renderer::Implement::DrawCommandList::HitDetectionElement::
query_implement(DrawCommandList &list,
                const TransformedBoundingBox &pixel_bb,
                const TransformedBoundingBox &pixel_bb_with_padding,
                bool delete_completely_contained,
                QueryResult *out_query)
{
  bool run_delete(false);

  if (!pixel_bb_with_padding.intersects(m_bb))
    {
      return;
    }

  if (pixel_bb_with_padding.contains(m_bb))
    {
      query_take_all(list, pixel_bb, delete_completely_contained, out_query);
      return;
    }

  for (unsigned int I : *m_draws)
    {
      RectDraw &draw(list.m_processed_rect_draws[I]);

      if (draw.m_status == status_unchecked)
        {
          draw.m_status = status_checked;
          if (delete_completely_contained && pixel_bb.contains(draw.m_rect))
            {
              run_delete = true;
              draw.m_status = status_to_be_deleted;
              out_query->m_fully_contained.push_back(I);
            }
          else if (pixel_bb_with_padding.intersects(draw.m_rect))
            {
              out_query->m_partially_hit.push_back(I);
            }
          else
            {
              out_query->m_not_hit_but_tested.push_back(I);
            }
        }
    }

  if (run_delete)
    {
      remove_deleted_draws(list, out_query->m_workroom);
    }

  if (m_children_spawned)
    {
      for (unsigned int child = 0; child < 4; ++child)
        {
          m_children[child]->query_implement(list, pixel_bb, pixel_bb_with_padding, delete_completely_contained, out_query);
        }
    }
}

void
astral::Renderer::Implement::DrawCommandList::HitDetectionElement::
query_take_all(DrawCommandList &list,
               const TransformedBoundingBox &pixel_bb,
               bool delete_completely_contained,
               QueryResult *out_query)
{
  bool run_delete(false);

  for (unsigned int I : *m_draws)
    {
      RectDraw &draw(list.m_processed_rect_draws[I]);

      if (draw.m_status == status_unchecked)
        {
          draw.m_status = status_checked;
          if (delete_completely_contained && pixel_bb.contains(draw.m_rect.containing_aabb()))
            {
              run_delete = true;
              draw.m_status = status_to_be_deleted;
              out_query->m_fully_contained.push_back(I);
            }
          else
            {
              out_query->m_partially_hit.push_back(I);
            }
        }
    }

  if (run_delete)
    {
      remove_deleted_draws(list, out_query->m_workroom);
    }

  if (m_children_spawned)
    {
      for (unsigned int child = 0; child < 4; ++child)
        {
          m_children[child]->query_take_all(list, pixel_bb, delete_completely_contained, out_query);
        }
    }
}

void
astral::Renderer::Implement::DrawCommandList::HitDetectionElement::
remove_deleted_draws(DrawCommandList &list, std::vector<unsigned int> &workroom)
{
  ASTRALassert(workroom.empty());
  for (unsigned int I : *m_draws)
    {
      const RectDraw &draw(list.m_processed_rect_draws[I]);

      ASTRALassert(draw.m_status != status_unchecked);
      if (draw.m_status != status_to_be_deleted)
        {
          workroom.push_back(I);
        }
    }

  m_draws->swap(workroom);
  workroom.clear();
}

////////////////////////////////////////////
// astral::Renderer::Implement::DrawCommandList methods
void
astral::Renderer::Implement::DrawCommandList::
untag_elements(c_array<const unsigned int> processed_rect_draw_idxs)
{
  for (unsigned int I : processed_rect_draw_idxs)
    {
      ASTRALassert(m_processed_rect_draws[I].m_status == status_checked);
      m_processed_rect_draws[I].m_status = status_unchecked;
    }
}

void
astral::Renderer::Implement::DrawCommandList::
add_command(enum command_list_t tp,
            const DrawCommand &cmd, unsigned int z,
            const TransformedBoundingBox *region,
            const DependencyList &dependency_list)
{
  /* a region must be provided if this has a hit detection heirarchy
   * unless the draw is for an occluder.
   */
  ASTRALassert((region == nullptr) == (m_hit_detection_root == nullptr) || tp == occluder_command_list);

  /* no clip can be applied when rendering to an STC mask */
  ASTRALassert(!cmd.m_render_values.m_clip_mask.valid() || m_blit_processing != image_blit_stc_mask_processing);

  if (region)
    {
      RectDraw RD;

      /* onle RGBA color rendering supports copying commands */
      ASTRALassert(renders_to_color_buffer());
      ASTRALassert(m_hit_detection_root);

      RD.m_list = tp;
      RD.m_command = m_commands[tp].size();
      RD.m_dependencies = dependency_list;
      RD.m_rect = *region;

      if (!RD.m_rect.intersects(hit_detection_root().bb()))
        {
          /* cull the draw because it does not intersect
           * the region covered by the command list.
           */
          return;
        }

      if (!m_pause_snapshot)
        {
          m_unprocessed_rect_draws.push_back(RD);
        }
      else
        {
          m_pause_snapshot_rect_draws.push_back(RD);
        }

      /* we only set the value of FirstShaderUsed for color rendering */
      if (m_first_shader_used[tp].m_first_item_shader_unique_id != -1)
        {
          c_array<const pointer<const ItemShader>> item_shaders;

          item_shaders = m_storage->fetch_shader_ptrs(cmd.m_vertices_and_shaders.m_shaders);
          ASTRALassert(!item_shaders.empty());
          ASTRALassert(item_shaders.front());

          m_first_shader_used[tp].m_first_item_shader_unique_id = item_shaders.front()->backend().uniqueID();
          if (cmd.m_render_values.m_material.material_shader())
            {
              m_first_shader_used[tp].m_first_material_shader_unique_id = cmd.m_render_values.m_material.material_shader()->root_unique_id();
            }
        }
    }

  m_commands[tp].push_back(cmd);
  m_commands[tp].back().m_z = z;
  m_commands[tp].back().m_order = m_current_draw;
  ++m_current_draw;
}

void
astral::Renderer::Implement::DrawCommandList::
add_command(enum command_list_t tp,
            const DrawCommand &cmd, unsigned int z,
            const RenderSupportTypes::RectRegion *region,
            RenderValue<Transformation> pixel_transformation_region,
            const DependencyList &dependency_list)
{
  /* region should be non-null whenever this DrawCommandList
   * is tracking hit detection.
   */
  ASTRALassert((region == nullptr) == (m_hit_detection_root == nullptr));

  if (region)
    {
      if (pixel_transformation_region.valid())
        {
          TransformedBoundingBox BB(region->m_rect, pixel_transformation_region.value());
          add_command(tp, cmd, z, &BB, dependency_list);
        }
      else
        {
          TransformedBoundingBox BB(region->m_rect);
          add_command(tp, cmd, z, &BB, dependency_list);
        }
    }
  else
    {
      add_command(tp, cmd, z, nullptr, dependency_list);
    }
}

void
astral::Renderer::Implement::DrawCommandList::
process_unprocessed_regions(void)
{
  if (m_unprocessed_rect_draws.empty())
    {
      return;
    }

  for (const RectDraw &src : m_unprocessed_rect_draws)
    {
      m_processed_rect_draws.push_back(src);
      hit_detection_root().add(*this, m_processed_rect_draws.size() - 1u);
    }
  m_unprocessed_rect_draws.clear();
}

void
astral::Renderer::Implement::DrawCommandList::
copy_commands_helper(DrawCommandList &src,
                     c_array<const unsigned int> src_processed_rect_draw_idxs,
                     const OnAddDependency &on_add_dependency)
{
  for (unsigned int I : src_processed_rect_draw_idxs)
    {
      const RectDraw &src_draw(src.m_processed_rect_draws[I]);
      unsigned int z;

      if (src_draw.m_dependencies.m_buffer_list)
        {
          c_array<VirtualBuffer* const> vbs(make_c_array(*src_draw.m_dependencies.m_buffer_list));
          for (unsigned int d = src_draw.m_dependencies.m_range.m_begin; d < src_draw.m_dependencies.m_range.m_end; ++d)
            {
              on_add_dependency(vbs[d]);
            }
        }

      switch (src_draw.m_list)
        {
        case occluder_command_list:
          z = 0u;
          break;

        case opaque_command_list:
          ++m_current_z;
          z = m_current_z;
          break;

        case typical_command_list:
          z = m_current_z;
          break;

        case number_command_list:
          z = 0u;
          ASTRALfailure("Invalid command list type");
          break;
        }

      add_command(src_draw.m_list,
                  src.m_commands[src_draw.m_list][src_draw.m_command],
                  z, &src_draw.m_rect, src_draw.m_dependencies);
    }
}

unsigned int
astral::Renderer::Implement::DrawCommandList::
copy_commands(DrawCommandList &src,
              RenderValue<Transformation> pixel_transformation_logical,
              const BoundingBox<float> &logical_bb,
              float logical_padding,
              bool delete_contained_cmds,
              const OnAddDependency &on_add_dependency)
{
  unsigned int draws_added(0);

  ASTRALassert(src.renders_to_color_buffer());
  ASTRALassert(src.m_hit_detection_root);
  ASTRALassert(renders_to_color_buffer());
  ASTRALassert(m_hit_detection_root);

  src.process_unprocessed_regions();
  ASTRALassert(src.m_unprocessed_rect_draws.empty());

  ASTRALassert(m_query_tmp.m_partially_hit.empty());
  ASTRALassert(m_query_tmp.m_fully_contained.empty());
  ASTRALassert(m_query_tmp.m_not_hit_but_tested.empty());
  src.hit_detection_root().query(src, pixel_transformation_logical,
                                 logical_bb, logical_padding,
                                 delete_contained_cmds, &m_query_tmp);

  /* bring into a single list of elements to copy and sort the list by
   * command ID to preserve draw order
   */
  m_query_tmp.m_workroom.resize(m_query_tmp.m_partially_hit.size() + m_query_tmp.m_fully_contained.size());
  std::copy(m_query_tmp.m_partially_hit.begin(),
            m_query_tmp.m_partially_hit.end(),
            m_query_tmp.m_workroom.begin());
  std::copy(m_query_tmp.m_fully_contained.begin(),
            m_query_tmp.m_fully_contained.end(),
            m_query_tmp.m_workroom.begin() + m_query_tmp.m_partially_hit.size());

  std::sort(m_query_tmp.m_workroom.begin(), m_query_tmp.m_workroom.end(), SortRectDrawsByDrawOrder(src));

  /* now add the commands in draw order */
  copy_commands_helper(src, make_c_array(m_query_tmp.m_workroom), on_add_dependency);

  src.untag_elements(make_c_array(m_query_tmp.m_partially_hit));
  src.untag_elements(make_c_array(m_query_tmp.m_not_hit_but_tested));

  if (delete_contained_cmds)
    {
      for (unsigned int I : m_query_tmp.m_fully_contained)
        {
          RectDraw &draw(src.m_processed_rect_draws[I]);
          enum command_list_t list(draw.m_list);
          unsigned int cmd(draw.m_command);

          ASTRALassert(draw.m_status == status_to_be_deleted);
          src.m_commands[list][cmd].m_draw_deleted = true;
        }
    }
  else
    {
      src.untag_elements(make_c_array(m_query_tmp.m_fully_contained));
    }

  draws_added = m_query_tmp.m_partially_hit.size() + m_query_tmp.m_fully_contained.size();
  m_query_tmp.clear();

  return draws_added;
}

void
astral::Renderer::Implement::DrawCommandList::
flush_pause_snapshot_rect_draws(void)
{
  for (const RectDraw &d : m_pause_snapshot_rect_draws)
    {
      m_unprocessed_rect_draws.push_back(d);
    }
  m_pause_snapshot_rect_draws.clear();
}

void
astral::Renderer::Implement::DrawCommandList::
add_command(bool is_opaque, const DrawCommand &cmd,
            const RenderSupportTypes::RectRegion *region,
            RenderValue<Transformation> pixel_transformation_region,
            const DependencyList &dependency_list)
{
  /* only color renders should have region info */
  ASTRALassert(!region || renders_to_color_buffer());

  /* only RGBA rendering gets opaque commands */
  ASTRALassert(!is_opaque || renders_to_color_buffer());

  ASTRALassert(cmd.m_render_values.m_blend_mode.valid());

  if (is_opaque)
    {
      /* make it so that the opaque element occludes everything
       * that has come before, but later elements are not occluded
       * by it. The z-test is NOT strict, so just incrementing
       * m_current_z and using that does the job.
       */
      ++m_current_z;
      add_command(opaque_command_list, cmd, m_current_z,
                  region, pixel_transformation_region, dependency_list);
    }
  else
    {
      add_command(typical_command_list, cmd, m_current_z,
                  region, pixel_transformation_region, dependency_list);

      /* only color rendering should be incrementing the m_current_z
       * and only on opaque draws added.
       */
      ASTRALassert(renders_to_color_buffer() || m_current_z == 0u);
    }
}

void
astral::Renderer::Implement::DrawCommandList::
send_occluders_to_backend(Renderer::Implement &renderer,
                          RenderBackend::UberShadingKey::Cookie uber_shader_key,
                          RenderValue<ScaleTranslate> tr,
                          RenderBackend::ClipWindowValue cl,
                          unsigned int start_z, bool permute_xy)
{
  const enum command_list_t tp(occluder_command_list);
  for (auto iter = m_commands[tp].begin(), end = m_commands[tp].end(); iter != end; ++iter)
    {
      /* NOTE: these occluders are to occlude all content and each of their
       *       m_z values is 0. We will send the z as 1 + m_current_z + start_z
       *       to make it also occlude the opaque commands.
       */
      iter->send_to_backend(renderer, uber_shader_key, tr, cl, 1 + m_current_z + start_z, permute_xy);
    }
}

void
astral::Renderer::Implement::DrawCommandList::
send_opaque_commands_to_backend(Renderer::Implement &renderer,
                                RenderBackend::UberShadingKey::Cookie uber_shader_key,
                                RenderValue<ScaleTranslate> tr,
                                RenderBackend::ClipWindowValue cl,
                                unsigned int start_z, bool permute_xy)
{
  const enum command_list_t tp(opaque_command_list);
  for (auto iter = m_commands[tp].rbegin(), end = m_commands[tp].rend(); iter != end; ++iter)
    {
      iter->send_to_backend(renderer, uber_shader_key, tr, cl, start_z, permute_xy);
    }
}

void
astral::Renderer::Implement::DrawCommandList::
send_commands_to_backend(Renderer::Implement &renderer,
                         RenderBackend::UberShadingKey::Cookie uber_shader_key,
                         RenderValue<ScaleTranslate> tr,
                         RenderBackend::ClipWindowValue cl,
                         unsigned int start_z, bool permute_xy)
{
  for (const DrawCommand &e : m_commands[typical_command_list])
    {
      e.send_to_backend(renderer, uber_shader_key, tr, cl, start_z, permute_xy);
    }
}

void
astral::Renderer::Implement::DrawCommandList::
add_commands_detailed_to_list(std::vector<DrawCommandDetailed> *dst,
                              RenderValue<ScaleTranslate> tr,
                              RenderBackend::ClipWindowValue cl,
                              unsigned int start_z, bool permute_xy) const
{
  ASTRALassert(m_commands[opaque_command_list].empty());
  ASTRALassert(m_current_z == 0u);

  for (const DrawCommand &cmd : m_commands[typical_command_list])
    {
      DrawCommandDetailed V;

      V.m_cmd = &cmd;
      V.m_scale_translate = tr;
      V.m_clip_window = cl;
      V.m_start_z = start_z;
      V.m_permute_xy = permute_xy;

      dst->push_back(V);
    }
}

void
astral::Renderer::Implement::DrawCommandList::
walk_rects_of_draws(RectWalker &rect_walker) const
{
  for (const RectDraw &v : m_pause_snapshot_rect_draws)
    {
      rect_walker(v.m_list, v.m_rect);
      if (rect_walker.early_out())
        {
          return;
        }
    }

  for (const RectDraw &v : m_unprocessed_rect_draws)
    {
      rect_walker(v.m_list, v.m_rect);
      if (rect_walker.early_out())
        {
          return;
        }
    }

  for (const RectDraw &v : m_processed_rect_draws)
    {
      rect_walker(v.m_list, v.m_rect);
      if (rect_walker.early_out())
        {
          return;
        }
    }
}

void
astral::Renderer::Implement::DrawCommandList::
send_commands_sorted_by_shader_to_backend(Renderer::Implement &renderer,
                                          c_array<const unsigned int>::iterator begin,
                                          c_array<const unsigned int>::iterator end)
{
  ASTRALassert(renderer.m_workroom->m_draw_list.empty());
  RenderBackend::UberShadingKey::Cookie no_uber;

  for (auto iter = begin; iter != end; ++iter)
    {
      VirtualBuffer &buffer(renderer.m_storage->virtual_buffer(*iter));
      DrawCommandList *cmd_list(buffer.command_list());

      cmd_list->add_commands_detailed_to_list(&renderer.m_workroom->m_draw_list,
                                              buffer.render_scale_translate(),
                                              buffer.clip_window(),
                                              buffer.start_z(),
                                              buffer.permute_xy_when_rendering());
    }

  /* sort the commands by shader, we use stable sort to give the
   * backend a chance to merge draws coming from the same VirtualBuffer
   */
  std::stable_sort(renderer.m_workroom->m_draw_list.begin(), renderer.m_workroom->m_draw_list.end());

  /* now draw them */
  for (const DrawCommandDetailed &e : renderer.m_workroom->m_draw_list)
    {
      e.m_cmd->send_to_backend(renderer, no_uber, e.m_scale_translate,
                               e.m_clip_window, e.m_start_z,
                               e.m_permute_xy);
    }

  /* cleanup for the next user */
  renderer.m_workroom->m_draw_list.clear();
}

void
astral::Renderer::Implement::DrawCommandList::
accumulate_shaders(Storage &storage, enum command_list_t tp, RenderBackend::UberShadingKey &backend) const
{
  for (const DrawCommand &cmd : m_commands[tp])
    {
      c_array<const pointer<const ItemShader>> shaders(storage.fetch_shader_ptrs(cmd.m_vertices_and_shaders.m_shaders));
      for (const ItemShader *shader: shaders)
        {
          ASTRALassert(shader);
          backend.add_shader(*shader, cmd.m_render_values);
        }
    }
}

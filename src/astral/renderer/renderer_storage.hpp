/*!
 * \file renderer_storage.hpp
 * \brief file renderer_storage.hpp
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

#ifndef ASTRAL_RENDERER_STORAGE_HPP
#define ASTRAL_RENDERER_STORAGE_HPP

#include <iostream>

#include <astral/renderer/renderer.hpp>
#include <astral/util/memory_pool.hpp>
#include <astral/util/object_pool.hpp>

#include "renderer_implement.hpp"
#include "renderer_draw_command.hpp"
#include "renderer_cull_geometry.hpp"
#include "renderer_clip_element.hpp"
#include "renderer_stc_data.hpp"
#include "renderer_virtual_buffer.hpp"
#include "renderer_virtual_buffer_proxy.hpp"
#include "renderer_stroke_builder.hpp"
#include "render_clip_node.hpp"
#include "render_encoder_layer.hpp"

/* To avoid malloc noise, we have pools for various objects
 * and the Storage class holds those pools
 */
class astral::Renderer::Implement::Storage:public reference_counted<Storage>::non_concurrent
{
public:
  class EffectDataHolder:astral::noncopyable
  {
  public:
    EffectDataHolder(Storage &st):
      m_st(st),
      m_data(st.m_render_effect_data.allocate())
    {}

    ~EffectDataHolder()
    {
      m_st.m_render_effect_data.reclaim(m_data);
    }

    EffectWorkRoom&
    workroom(void)
    {
      return m_data->m_workroom;
    }

    std::vector<generic_data>&
    processed_params(void)
    {
      return m_data->m_processed_params;
    }

  private:
    Storage &m_st;
    RenderEncoderLayer::Backing::EffectData *m_data;
  };

  explicit
  Storage(Renderer::Implement &renderer):
    m_renderer(renderer)
  {}

  ~Storage()
  {
    clear();
  }

  void
  clear(void)
  {
    m_virtual_buffers.clear();

    m_command_lists.clear();
    m_cached_transformations.clear();
    m_buffer_lists.clear();
    m_stc_data_set.clear();
    m_unsigned_int_array_pool.clear();
    m_virtual_buffer_proxies.clear();
    m_clip_geometries.clear();
    m_cull_geometry_sub_rects.clear();
    m_vertex_ranges.clear();
    m_shader_ptrs.clear();
    m_blit_rects.clear();
    m_clip_nodes.clear();
    m_encoder_layers.clear();
    m_render_effect_data.clear();
    m_stroke_builders.clear();
    m_cull_geometry_backing.clear();
  }

  DrawCommandList*
  allocate_command_list(enum DrawCommandList::render_type_t tp,
                        enum image_blit_processing_t blit_processing,
                        const BoundingBox<float> &bb)
  {
    DrawCommandList *return_value(m_command_lists.allocate());

    return_value->init(tp, blit_processing, bb, this);
    return return_value;
  }

  DrawCommandList*
  allocate_command_list_for_shadow_map(void)
  {
    DrawCommandList *return_value(m_command_lists.allocate());

    return_value->init_as_render_shadow_map();
    return return_value;
  }

  std::vector<CachedTransformation>*
  allocate_transformation_stack(void)
  {
    return m_cached_transformations.allocate();
  }

  template<typename ...Args>
  CullGeometry
  create_clip(Args&&... args)
  {
    return CullGeometry(&m_cull_geometry_backing, std::forward<Args>(args)...);
  }

  /*!
   * Have an internal array store copies of CullGeometry values,
   * these values can be fetched by calling backed_clip_geometries().
   * In addition, allocate backing rects which are fetched by
   * backed_cull_geometry_sub_rects() using the value written to
   * out_rects_token
   */
  range_type<unsigned int>
  create_backed_rects_and_clips(c_array<const CullGeometry> values, range_type<unsigned int> *out_rects_token)
  {
    range_type<unsigned int> return_value;

    return_value.m_begin = m_clip_geometries.size();
    for (const CullGeometry &V: values)
      {
        m_clip_geometries.push_back(V);
      }
    return_value.m_end = m_clip_geometries.size();

    out_rects_token->m_begin = m_cull_geometry_sub_rects.size();
    out_rects_token->m_end = m_cull_geometry_sub_rects.size() + values.size();
    m_cull_geometry_sub_rects.resize(out_rects_token->m_end);

    return return_value;
  }

  /* Return a c-array of CullGeometry values made
   * with create_backed_rects_and_clips().
   */
  c_array<const CullGeometry>
  backed_clip_geometries(range_type<unsigned int> R)
  {
    c_array<const CullGeometry> Q;

    Q = make_c_array(m_clip_geometries);
    return Q.sub_array(R);
  }

  range_type<unsigned int>
  create_intersected_backed_cull_geometry_rects(range_type<unsigned int> R, const BoundingBox<float> &pixel_rect)
  {
    range_type<unsigned int> return_value;

    return_value.m_begin = m_cull_geometry_sub_rects.size();
    for (unsigned int i = R.m_begin; i < R.m_end; ++i)
      {
        BoundingBox<float> tmp(pixel_rect, m_cull_geometry_sub_rects[i]);
        if (!tmp.empty())
          {
            m_cull_geometry_sub_rects.push_back(tmp);
          }
      }
    return_value.m_end = m_cull_geometry_sub_rects.size();
    return R;
  }

  /* Return a c-array of Rect values made
   * with create_backed_rects_and_clips().
   */
  c_array<BoundingBox<float>>
  backed_cull_geometry_sub_rects(range_type<unsigned int> R)
  {
    return make_c_array(m_cull_geometry_sub_rects).sub_array(R);
  }

  std::vector<VirtualBuffer*>*
  allocate_buffer_list(void)
  {
    return m_buffer_lists.allocate();
  }

  /* Where ALL STCData for ALL VirtualBuffer objects reside. */
  STCData::DataSet&
  stc_data_set(void)
  {
    return m_stc_data_set;
  }

  /* Return value is to be passed to fetch_vertex_ranges() to get the data */
  template<typename T>
  range_type<unsigned int>
  allocate_vertex_ranges(const RenderSupportTypes::Item<T> &item)
  {
    c_array<const range_type<int>> in_values;
    vecN<range_type<int>, 1> one_value;

    if (item.m_draw_all)
      {
        one_value[0].m_begin = 0u;
        one_value[0].m_end = item.m_vertex_data.number_vertices();
        in_values = one_value;
      }
    else
      {
        in_values = item.m_vertex_data_ranges;
      }

    return allocate_vertex_ranges(item.m_vertex_data, in_values);
  }

  /* Return value is to be passed to fetch_vertex_ranges() to get the data */
  range_type<unsigned int>
  allocate_vertex_ranges(const VertexData &data, c_array<const range_type<int>> in_values)
  {
    /* TODO: put this behind a .cpp file  */

    unsigned int sz(m_vertex_ranges.size());
    unsigned int src_begin(data.vertex_range().m_begin);

    ASTRALassert(!in_values.empty());
    m_vertex_ranges.resize(sz + in_values.size());
    for (unsigned int i = 0; i < in_values.size(); ++i)
      {
        // shader
        m_vertex_ranges[i + sz].first = 0u;

        // vertex range into the astral::VertexDataAllocator
        ASTRALassert(in_values[i].m_begin >= 0);
        ASTRALassert(in_values[i].m_begin <= in_values[i].m_end);
        ASTRALassert(in_values[i].m_end <= static_cast<int>(data.number_vertices()));

        m_vertex_ranges[i + sz].second.m_begin = in_values[i].m_begin + src_begin;
        m_vertex_ranges[i + sz].second.m_end = in_values[i].m_end + src_begin;
      }

    return range_type<unsigned int>(sz, m_vertex_ranges.size());
  }

  /* Return value is to be passed to fetch_vertex_ranges() to get the data */
  range_type<unsigned int>
  allocate_vertex_ranges(c_array<const pointer<const VertexData>> vertex_datas,
                         c_array<const RenderSupportTypes::ColorItem::SubItem> sub_draws)
  {
    /* TODO: put this behind a .cpp file  */
    range_type<unsigned int> return_value;

    return_value.m_begin = m_vertex_ranges.size();
    for (const RenderSupportTypes::ColorItem::SubItem &sub_draw : sub_draws)
      {
        std::pair<unsigned int, range_type<int>> V;

        ASTRALassert(sub_draw.m_vertex_data < vertex_datas.size());
        ASTRALassert(vertex_datas[sub_draw.m_vertex_data]);
        ASTRALassert(sub_draw.m_vertices.m_begin >= 0);
        ASTRALassert(sub_draw.m_vertices.m_begin <= sub_draw.m_vertices.m_end);
        ASTRALassert(sub_draw.m_vertices.m_end <= static_cast<int>(vertex_datas[sub_draw.m_vertex_data]->number_vertices()));

        unsigned int src_begin;

        src_begin = vertex_datas[sub_draw.m_vertex_data]->vertex_range().m_begin;

        V.first = sub_draw.m_shader;
        V.second.m_begin = sub_draw.m_vertices.m_begin + src_begin;
        V.second.m_end = sub_draw.m_vertices.m_end + src_begin;

        m_vertex_ranges.push_back(V);
      }
    return_value.m_end = m_vertex_ranges.size();

    return return_value;
  }

  c_array<const std::pair<unsigned int, range_type<int>>>
  fetch_vertex_ranges(range_type<unsigned int> V) const
  {
    c_array<const std::pair<unsigned int, range_type<int>>> return_value;

    return_value = make_c_array(m_vertex_ranges);
    return_value = return_value.sub_array(V);

    return return_value;
  }

  /* Return value is to be passed to fetch_shader_ptrs() to get the data */
  range_type<unsigned int>
  allocate_shader_ptrs(c_array<const pointer<const ColorItemShader>> shaders)
  {
    range_type<unsigned int> return_value;

    return_value.m_begin = m_shader_ptrs.size();
    return_value.m_end = m_shader_ptrs.size() + shaders.size();

    m_shader_ptrs.resize(return_value.m_end);
    std::copy(shaders.begin(), shaders.end(), m_shader_ptrs.begin() + return_value.m_begin);

    return return_value;
  }

  /* Return value is to be passed to fetch_shader_ptrs() to get the data */
  range_type<unsigned int>
  allocate_shader_ptr(const ItemShader &shader)
  {
    range_type<unsigned int> return_value;

    return_value.m_begin = m_shader_ptrs.size();
    return_value.m_end = m_shader_ptrs.size() + 1;
    m_shader_ptrs.push_back(&shader);

    return return_value;
  }

  c_array<const pointer<const ItemShader>>
  fetch_shader_ptrs(range_type<unsigned int> V) const
  {
    c_array<const pointer<const ItemShader>> return_value;

    return_value = make_c_array(m_shader_ptrs);
    return_value = return_value.sub_array(V);

    return return_value;
  }

  std::vector<unsigned int>*
  allocate_unsigned_int_array(void)
  {
    return m_unsigned_int_array_pool.allocate();
  }

  void
  recycle_unsigned_int_array(std::vector<unsigned int> *p)
  {
    m_unsigned_int_array_pool.reclaim(p);
  }

  std::vector<RectT<int>>*
  allocate_rect_array(void)
  {
    return m_blit_rects.allocate();
  }

  template<typename ...Args>
  RenderEncoderBase
  create_virtual_buffer(const VirtualBuffer::CreationTag &C, Args&&... args)
  {
    VirtualBuffer *b;
    unsigned int render_index;

    render_index = m_virtual_buffers.created_objects().size();
    b = m_virtual_buffers.create(C, render_index, m_renderer, std::forward<Args>(args)...);

    ASTRALassert(b->render_index() == render_index);
    ASTRALassert(render_index < m_virtual_buffers.created_objects().size());
    ASTRALassert(m_virtual_buffers.created_object(render_index) == b);

    return RenderEncoderBase(b);
  }

  template<typename ...Args>
  RenderSupportTypes::Proxy
  create_virtual_buffer_proxy(Args&&... args)
  {
    RenderSupportTypes::Proxy::Backing *b;

    b = m_virtual_buffer_proxies.create(std::forward<Args>(args)...);
    return RenderSupportTypes::Proxy(b);
  }

  template<typename ...Args>
  ClipElement*
  create_clip_element(Args&&... args)
  {
    ClipElement *b;

    b = m_clip_elements.allocate();
    b->init(m_renderer, std::forward<Args>(args)...);
    return b;
  }

  ClipElement*
  create_empty_clip_element(enum mask_type_t preferred_mask_type)
  {
    vecN<enum mask_channel_t, number_mask_type> mask_channels;

    mask_channels[mask_type_coverage] = MaskUsage::mask_channel(mask_type_coverage);
    mask_channels[mask_type_distance_field] = MaskUsage::mask_channel(mask_type_distance_field);

    return create_clip_element(CullGeometrySimple(),
                               CullGeometryGroup::Token(),
                               nullptr, mask_channels, preferred_mask_type);
  }

  void
  reclaim_clip_element(ClipElement *p)
  {
    m_clip_elements.reclaim(p);
  }

  RenderEncoderStrokeMask::Backing*
  create_stroke_builder(const CullGeometryGroup &parent_cull_geometry,
                        const StrokeMaskProperties &mask_params,
                        const Transformation &pixel_transformation_logical,
                        float render_accuracy)
  {
    RenderEncoderStrokeMask::Backing *return_value;

    return_value = m_stroke_builders.allocate();
    return_value->init(m_renderer, parent_cull_geometry,
                       mask_params, pixel_transformation_logical,
                       render_accuracy);

    return return_value;
  }

  template<typename ...Args>
  ClipCombineResult*
  create_clip_combine_result(Args&&... args)
  {
    ClipCombineResult *b;

    b = m_clip_combine_results.allocate();
    b->init(m_renderer, std::forward<Args>(args)...);
    return b;
  }

  void
  reclaim_clip_combine_result(ClipCombineResult *p)
  {
    m_clip_combine_results.reclaim(p);
  }

  RenderClipNode::Backing*
  create_clip_node(void)
  {
    return m_clip_nodes.create();
  }

  template<typename ...Args>
  RenderEncoderLayer::Backing*
  create_render_encoder_layer(Args&&... args)
  {
    return m_encoder_layers.create(std::forward<Args>(args)...);
  }

  RenderEncoderLayer::Backing::EffectData*
  allocate_effect_data(void)
  {
    return m_render_effect_data.allocate();
  }

  void
  reclaim_effect_data(RenderEncoderLayer::Backing::EffectData *p)
  {
    m_render_effect_data.reclaim(p);
  }

  /* Returns the named active virtual buffer, i.e. the returned buffer
   * will have buffer_id == VirtualBuffer::m_render_index
   */
  VirtualBuffer&
  virtual_buffer(unsigned int buffer_id)
  {
    ASTRALassert(buffer_id < m_virtual_buffers.created_objects().size());
    ASTRALassert(buffer_id == m_virtual_buffers.created_object(buffer_id)->render_index());
    return *m_virtual_buffers.created_object(buffer_id);
  }

  c_array<VirtualBuffer* const>
  virtual_buffers(void) const
  {
    return m_virtual_buffers.created_objects();
  }

  unsigned int
  number_virtual_buffers(void) const
  {
    return m_virtual_buffers.created_objects().size();
  }

  CullGeometry::Backing&
  cull_geometry_backing(void)
  {
    return m_cull_geometry_backing;
  }

private:
  Renderer::Implement &m_renderer;
  ObjectPoolClear<DrawCommandList> m_command_lists;
  ObjectPoolClear<std::vector<CachedTransformation>> m_cached_transformations;
  ObjectPoolClear<std::vector<VirtualBuffer*>> m_buffer_lists;
  STCData::DataSet m_stc_data_set;
  ObjectPoolClear<std::vector<unsigned int>> m_unsigned_int_array_pool;
  MemoryPool<RenderSupportTypes::Proxy::Backing, 4096> m_virtual_buffer_proxies;
  std::vector<CullGeometry> m_clip_geometries;
  std::vector<BoundingBox<float>> m_cull_geometry_sub_rects;
  std::vector<std::pair<unsigned int, range_type<int>>> m_vertex_ranges;
  std::vector<pointer<const ItemShader>> m_shader_ptrs;
  ObjectPoolClear<std::vector<RectT<int>>> m_blit_rects;
  MemoryObjectPool<VirtualBuffer, 4096> m_virtual_buffers;
  MemoryObjectPool<RenderClipNode::Backing, 4096> m_clip_nodes;
  MemoryObjectPool<RenderEncoderLayer::Backing, 4096> m_encoder_layers;
  ObjectPoolClear<RenderEncoderLayer::Backing::EffectData> m_render_effect_data;
  ObjectPoolClear<RenderEncoderStrokeMask::Backing> m_stroke_builders;
  CullGeometry::Backing m_cull_geometry_backing;

  /* These are NOT cleared every frame since clip objects
   * can be reused across frames.
   */
  ObjectPoolDirect<ClipElement, 128> m_clip_elements;
  ObjectPoolDirect<ClipCombineResult, 128> m_clip_combine_results;
};

/* placed here because we want it inlined, but requires the defintion of Storage */
inline
astral::c_array<const astral::Renderer::Implement::STCData>
astral::Renderer::VirtualBuffer::
stc_data_values(enum FillSTCShader::pass_t pass) const
{
  return m_stc[pass].values(m_renderer.m_storage->stc_data_set().m_stc_data[pass]);
}

#define VB_TAG Renderer::VirtualBuffer::CreationTag(__FILE__, __LINE__)

#endif

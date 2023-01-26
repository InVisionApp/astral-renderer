/*!
 * \file renderer_workroom.hpp
 * \brief file renderer_workroom.hpp
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

#ifndef ASTRAL_RENDERER_WORKROOM_HPP
#define ASTRAL_RENDERER_WORKROOM_HPP

#include <astral/util/layered_rect_atlas.hpp>
#include <astral/util/interval_allocator.hpp>
#include <astral/util/tile_allocator.hpp>
#include <astral/renderer/shader/stroke_query.hpp>
#include <astral/renderer/renderer.hpp>

#include "renderer_implement.hpp"
#include "renderer_draw_command.hpp"
#include "renderer_cull_geometry.hpp"
#include "render_clip_node.hpp"
#include "renderer_uber_shading_key_collection.hpp"
#include "renderer_tile_hit_detection.hpp"
#include "render_encoder_layer.hpp"

class astral::Renderer::Implement::WorkRoom:
  public reference_counted<WorkRoom>::non_concurrent
{
public:
  class BufferList
  {
  public:
    void
    clear(void)
    {
      m_remaining.clear();
      m_ready_to_render_later.clear();
    }

    /* add a buffer to the BufferList to get rendered */
    void
    add_buffer(unsigned int I)
    {
      m_remaining.push_back(I);
    }

    bool
    buffers_remaining(void)
    {
      return !m_remaining.empty() || !m_ready_to_render_later.empty();
    }

  protected:
    class ReadyBufferHelperBase;
    class ReadyImageBufferHelper;
    class ReadyShadowMapBufferHelper;

    c_array<unsigned int>
    choose_ready_buffers_common(Renderer::Implement &renderer, ReadyBufferHelperBase *helper);

  private:
    /* what buffers remain to be rendered */
    std::vector<unsigned int> m_remaining;

    /* buffers that remain to be rendered that do not have all dependencies met */
    std::vector<unsigned int> m_not_ready_to_render;

    /* buffers that remain that have all dependencies met */
    std::vector<unsigned int> m_ready_to_render;

    /* buffers that remain that have all dependencies met and fit in the offscreen buffer */
    std::vector<unsigned int> m_ready_to_render_now;

    /* buffers that remain that have all dependencies met but did not fit */
    std::vector<unsigned int> m_ready_to_render_later;
  };

  class ImageBufferLocation
  {
  public:
    /* base class to choose the location of an VirtualBuffer in
     * the scratch render target.
     */
    class Chooser;

    ImageBufferLocation(void):
      m_permute_xy(false),
      m_location(-1, -1)
    {}

    /* Sets m_permute_xy as false
     * \param v absolute location in scratch render target
     */
    explicit
    ImageBufferLocation(ivec2 v):
      m_permute_xy(false),
      m_location(v)
    {}

    /* Sets m_permute_xy as false
     * \param (x, y) absolute location in scratch render target
     */
    ImageBufferLocation(int x, int y):
      m_permute_xy(false),
      m_location(x, y)
    {}

    /* \param b value for m_permute_xy
     * \param v *absolute* location in scratch render target
     */
    ImageBufferLocation(bool b, ivec2 v):
      ImageBufferLocation(b, v.x(), v.y())
    {}

    /* \param b value for m_permute_xy
     * \param (x, y) *absolute* location in scratch render target
     */
    ImageBufferLocation(bool b, int x, int y):
      m_permute_xy(b),
      m_location((b) ? y : x,
                 (b) ? x : y)
    {}

    bool
    valid(void) const
    {
      return m_location.x() >= 0
        && m_location.y() >= 0;
    }

    /* if realization in scratch render target is to have
     * the x and y coordinates flipped
     */
    bool m_permute_xy;

    /* If m_permute_xy is false, then the absolute location in the
     * scratch render target. If m_permute_xy is true, then the
     * absolute location in the render target is given by
     * (m_location.y(), m_location.x()).
     *
     * The reason why we store the location this way is that
     * it dramtically simplifies all of the logic for the blitting
     * from the scratch buffer to the astral::Image objects to
     * just passing the value of m_permute_xy.
     */
    ivec2 m_location;
  };

  class ImageBufferList:public BufferList
  {
  public:
    ImageBufferList(void);
    ~ImageBufferList();

    /* Returns a list of ID's of those image buffers that are ready to render
     * and have been given a location on the scratch render target for their
     * render.
     */
    c_array<unsigned int>
    choose_ready_buffers(Renderer::Implement &renderer);

  private:
    reference_counted_ptr<ImageBufferLocation::Chooser> m_region_allocator;
  };

  class ShadowMapBufferList:public BufferList
  {
  public:
    ShadowMapBufferList(void):
      m_interval_allocator(0, 0)
    {}

    /* Returns a list of ID's of those shadow map buffers that are ready to render
     * and have been given a location on the scratch render target for their
     * render.
     */
    c_array<unsigned int>
    choose_ready_buffers(Renderer::Implement &renderer);

  private:
    IntervalAllocator m_interval_allocator;
  };

  class Stroke:astral::noncopyable
  {
  public:
    Stroke(void)
    {
      m_query = StrokeQuery::create();
    }

    reference_counted_ptr<StrokeQuery> m_query;
    std::vector<RenderEncoderMask> m_render_encoders;
    std::vector<vecN<range_type<int>, 2>> m_tmp_tile_regions;
    std::vector<VirtualBuffer*> m_tmp_virtual_buffer_pointers;
  };

  class ColorItem:astral::noncopyable
  {
  public:
    void
    clear(void)
    {
      m_shaders.clear();
      m_vertex_datas.clear();
      m_sub_items.clear();
    }

    std::vector<pointer<const ColorItemShader>> m_shaders;
    std::vector<pointer<const VertexData>> m_vertex_datas;
    std::vector<RenderSupportTypes::ColorItem::SubItem> m_sub_items;
  };

  explicit
  WorkRoom(RenderBackend &backend)
  {
    for (unsigned int i = 0; i < m_uber_shading_key.size(); ++i)
      {
        m_uber_shading_key[i] = backend.create_uber_shading_key();
      }
  }

  /* work room used to store draw data values when creating ItemData objects */
  std::vector<gvec4> m_item_data_workroom;

  /* virtual buffers organized by fill-rule */
  vecN<std::vector<unsigned int>, number_fill_rule> m_by_fill_rule;

  /* work room for creating a VirtualBuffer of type assembled_buffer */
  std::vector<std::pair<uvec2, ImageAtlas::TileElement>> m_shared_tiles;

  /* buffer list for rendering image buffers */
  ImageBufferList m_image_buffer_list;

  /* buffer list for rendering shadowmap buffers that have dependencies */
  ShadowMapBufferList m_shadowmap_buffer_list;

  /* list of shadowmap virtual buffers that have no dependendencies;
   * these shadowmap buffers are rendered directly to the shadowmap
   * atlas.
   */
  std::vector<unsigned int> m_direct_shadowmap_buffers;

  /* work room used by DrawCommandList::send_commands_sorted_by_shader_to_backend() */
  std::vector<DrawCommandDetailed> m_draw_list;

  /* work room used by RenderEncoderBase::clip_node_pixel() */
  RenderClipNode::Backing::ClippedTile::Collection m_clip_in, m_clip_out;
  std::vector<RenderClipNode::Backing::ClippedTile> m_intersection;

  /* scratch buffer of tiles enumerated by ImageMipElement::element_type_t;
   * used by ClipCombineResult to create sub-images.
   */
  vecN<std::vector<uvec2>, ImageMipElement::number_element_type> m_tile_scratch;

  /* scratch room used by VirtualBuffer to explicitely construct a mip-map chain */
  std::vector<reference_counted_ptr<const ImageMipElement>> m_mip_chain;

  /* generic tmp vector */
  std::vector<unsigned int> m_tmp;

  /* work room for stroking */
  Stroke m_stroke;

  /* work room for tracking an array of encoders */
  std::vector<RenderEncoderBase> m_tmp_buffer_list;

  /* work room for generating RenderSupportTypes::ColorItem values */
  ColorItem m_color_item;

  /* uber shading key when doing uber shading */
  vecN<reference_counted_ptr<RenderBackend::UberShadingKey>, number_uber_shader_method> m_uber_shading_key;

  /* sub-uber-shaders for stroking and mask drawing */
  UberShadingKeyCollection m_sub_ubers;

  /* workroom to figure out what tiles of a color render are hit */
  TileHitDetection m_tile_hit_detection;

  /* work room for an array of CullGeometry values; used when
   * constructing a CullGeometryGroup
   */
  std::vector<CullGeometry> m_clip_geometries;
  CullGeometryGroup::Intersection m_clip_geometry_intersection;

  /* scratch space for RenderEncoderLayer */
  RenderEncoderLayer::Backing::ScratchSpace m_render_encoder_layer;

  /* buffers that will get rendered in the current round */
  std::vector<unsigned int> m_renderable_image_buffers, m_renderable_shadowmap_buffers;
};

#endif

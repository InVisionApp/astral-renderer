/*!
 * \file render_clip_node.hpp
 * \brief file render_clip_node.hpp
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

#ifndef ASTRAL_RENDER_CLIP_NODE_HPP
#define ASTRAL_RENDER_CLIP_NODE_HPP

#include <astral/renderer/renderer.hpp>
#include "renderer_implement.hpp"
#include "renderer_clip_element.hpp"

class astral::RenderClipNode::Backing
{
public:
  /* class used to implement begin_clip_node_*() */
  class Begin;

  /* class used to implement end_clip_node() */
  class End;

  /* class for tile blitting */
  class ClippedTile;

  Backing(void):
    m_blit_clip_out_content_only(false),
    m_has_clip_in(false),
    m_has_clip_out(false),
    m_non_empty_intersection(false),
    m_num_clip_in_rects(0u),
    m_num_clip_out_rects(0u),
    m_end_clip_node_called(false)
  {}

  c_array<const Rect>
  clip_in_rects(void) const
  {
    return c_array<const Rect>(&m_clip_in_rects[0], m_num_clip_in_rects);
  }

  c_array<const Rect>
  clip_out_rects(void) const
  {
    return c_array<const Rect>(&m_clip_out_rects[0], m_num_clip_out_rects);
  }

  static
  unsigned int
  clip_node_padding(void)
  {
    return 0u;
  }

  /* encoder that created the clip-node */
  RenderEncoderBase m_parent_encoder;

  /* encoders for the clipped content */
  RenderEncoderImage m_clip_in, m_clip_out;

  /* This is true if the mask is null; in this case
   * there is no clipped-in content to draw and the
   * clipped-out content should just be blitted.
   */
  bool m_blit_clip_out_content_only;

  /* Rects used issue the blit of the clipped content */
  ScaleTranslate m_pixel_transformation_mask;
  bool m_has_clip_in, m_has_clip_out, m_non_empty_intersection;
  unsigned int m_num_clip_in_rects, m_num_clip_out_rects;
  vecN<Rect, 4> m_clip_in_rects, m_clip_out_rects;
  Rect m_dual_clip_rect;

  /* how to blit */
  enum blend_mode_t m_blend_mode;
  enum filter_t m_mask_filter;
  reference_counted_ptr<const Image> m_mask_image;
  reference_counted_ptr<const RenderClipCombineResult> m_clip_combine;
  BoundingBox<float> m_mask_bbox, m_clip_out_bbox;

  /* use only when clipping against a MaskDetails */
  enum mask_channel_t m_mask_channel;
  enum mask_type_t m_mask_type;

  bool m_end_clip_node_called;

  /* additional clipping to apply to entire blit */
  ItemMask m_additional_clipping;
};

/* This beast carries additional state specifying if clip_node_pixel()
 * is against a MaskDetails or RenderClipCombineResult so that we
 * can reduce code duplication
 */
class astral::RenderClipNode::Backing::Begin:public astral::RenderEncoderBase
{
public:
  Begin(RenderEncoderBase b, const MaskDetails &mask):
    RenderEncoderBase(b),
    m_mask(&mask),
    m_clip_combine(nullptr)
  {}

  Begin(RenderEncoderBase b, const Renderer::Implement::ClipCombineResult &combine):
    RenderEncoderBase(b),
    m_mask(&combine.raw_fill()),
    m_clip_combine(&combine)
  {}

  RenderClipNode
  begin_clip_node_pixel_implement(enum blend_mode_t blend_mode,
                                  enum clip_node_flags_t flags,
                                  const BoundingBox<float> &pclip_in_bbox,
                                  const BoundingBox<float> &pclip_out_bbox,
                                  enum filter_t mask_filter,
                                  const ItemMask &clip);

private:
  /* initializes the fields of RenderClipNode for dual clipping
   * \param encoder which has clip_node_pixel() called; it is required
   *                that encoder's current value of transformation()
   *                maps from mask coordinates to pixel coordinates
   * \param flags which of clip-in and clip-out to have
   * \param clip_in_rect clip-in rect in mask coordinates, i.e. mask
   *                     coordinates
   * \param clip_out_rect clip-out rect in mask coordinates, i.e. mask
   *                      coordinates
   * \param out_encoders location to which to write RenderEncoderImage
   *                     values to be used to render clipped content.
   */
  bool //returns false if nothing to draw
  init(enum clip_node_flags_t flags,
       const BoundingBox<float> &clip_in_rect,
       const BoundingBox<float> &clip_out_rect,
       RenderClipNode::Backing *out_encoders) const;

  void
  add_tile_occluders(RenderValue<Transformation> pixel_transformation_mask,
                     RenderEncoderBase encoder, c_array<const Rect> rects,
                     vecN<range_type<unsigned int>, 2> tile_range,
                     enum Renderer::Implement::ClipCombineResult::combine_element_t tp);

  void
  add_tile_occluders(RenderValue<Transformation> pixel_transformation_mask,
                     RenderEncoderBase encoder, c_array<const Rect> rects,
                     enum ImageMipElement::element_type_t tp);

  /* if non-null, then clip_node_pixel() is against a MaskDetails */
  const MaskDetails *m_mask;

  /* if non-null, then clip_node_pixel() is against a ClipCombineResult */
  const Renderer::Implement::ClipCombineResult *m_clip_combine;
};

class astral::RenderClipNode::Backing::End:public RenderEncoderBase
{
public:
  End(RenderEncoderBase encoder, RenderClipNode::Backing &clip_node):
    RenderEncoderBase(encoder),
    m_clip_node(clip_node)
  {}

  void
  end_clip_node_implement(void) const;

private:
  typedef vecN<std::vector<ClippedTile>*, Renderer::Implement::ClipCombineResult::invalid_combine_element> ClippedTileCollector;

  void
  compute_tiles(const Rect &rect,
                std::vector<ClippedTile> *out_color_tiles,
                std::vector<ClippedTile> *out_full_tiles,
                std::vector<ClippedTile> *out_empty_tiles) const;

  void
  compute_tiles(const Rect &rect,
                const vecN<range_type<unsigned int>, 2> &tile_range,
                const ClippedTileCollector &collector) const;

  static
  void
  add_blit_rects(std::vector<RectT<int>> *blit_rects,
                 c_array<const ClippedTile> tiles,
                 ScaleTranslate blit_transformation_tile);

  void
  blit_full_tiles(c_array<const ClippedTile> tiles,
                  const ItemMaterial &material,
                  enum blend_mode_t blend_mode) const;

  void
  blit_partial_tiles(c_array<const ClippedTile> tiles,
                     enum filter_t mask_filter,
                     bool invert_coverage,
                     enum mask_type_t mask_type,
                     enum mask_channel_t mask_channel,
                     const ItemMaterial &material,
                     enum blend_mode_t blend_mode) const;

  static
  void
  add_raw_blit_rects(std::vector<astral::RectT<int>> *blit_rects,
                     astral::c_array<const astral::Rect> tiles,
                     astral::ScaleTranslate blit_transformation_tile);

  RenderClipNode::Backing &m_clip_node;
};

class astral::RenderClipNode::Backing::ClippedTile
{
public:
  class Collection
  {
  public:
    void
    clear(void)
    {
      m_full_tiles.clear();
      m_color_tiles.clear();
    }

    bool
    empty(void) const
    {
      return m_full_tiles.empty()
        && m_color_tiles.empty();
    }

    std::vector<ClippedTile> m_full_tiles, m_color_tiles;
  };

  /* Tiled ID fed to ImageMipElement::tile_location(),
   * ImageMipElement::tile_size()
   */
  uvec2 m_tile;

  /* pixel_rect clipped to tile, in coordinates of mask */
  Rect m_rect;
};

#endif

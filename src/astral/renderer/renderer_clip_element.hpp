/*!
 * \file renderer_clip_element.hpp
 * \brief file renderer_clip_element.hpp
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

#ifndef ASTRAL_RENDERER_CLIP_ELEMENT_HPP
#define ASTRAL_RENDERER_CLIP_ELEMENT_HPP

#include <astral/renderer/renderer.hpp>
#include "renderer_implement.hpp"
#include "renderer_cull_geometry.hpp"
#include "renderer_filler.hpp"

class astral::Renderer::Implement::ClipElement:public RenderClipElement
{
public:
  ClipElement(void):
    m_renderer(nullptr)
  {}

  void
  init(Renderer::Implement &renderer,
       const CullGeometrySimple &clip_geometry,
       CullGeometryGroup::Token token,
       const reference_counted_ptr<const Image> &image,
       enum mask_type_t mask_type,
       enum mask_channel_t mask_channel);

  void
  init(Renderer::Implement &renderer,
       const CullGeometrySimple &clip_geometry,
       CullGeometryGroup::Token token,
       const reference_counted_ptr<const Image> &image,
       vecN<enum mask_channel_t, number_mask_type> channels,
       enum mask_type_t preferred_mask_type);

  void
  init(Renderer::Implement &renderer, const ClipElement &src,
       enum mask_type_t mask_type);

  ~ClipElement()
  {}

  const CullGeometrySimple&
  clip_geometry(void) const
  {
    return m_clip_geometry;
  }

  CullGeometryGroup::Token
  clip_geometry_token(void) const
  {
    return m_clip_geometry_token;
  }

  /* returns the mask channel for the type returned by mask_type(void) const */
  enum mask_channel_t
  mask_channel(void) const
  {
    return m_mask_channels[m_preferred_mask_type];
  }

  const vecN<enum mask_channel_t, number_mask_type>&
  mask_channels(void) const
  {
    return m_mask_channels;
  }

  bool
  supports_mask_type(enum mask_type_t v) const
  {
    return m_mask_channels[v] != number_mask_channel;
  }

  const astral::Image*
  image(void) const
  {
    return m_mask_details.m_mask.get();
  }

  bool
  full_tile(ivec2 R) const
  {
    return !m_mip_front || m_mip_front->tile_type(uvec2(R)) == ImageMipElement::white_element;
  }

  bool
  empty_tile(ivec2 R) const
  {
    return !m_mip_front || m_mip_front->tile_type(uvec2(R)) == ImageMipElement::empty_element;
  }

  bool
  partial_tile(ivec2 R) const
  {
    return m_mip_front && m_mip_front->tile_type(uvec2(R)) == ImageMipElement::color_element;
  }

  const ImageMipElement*
  mip_front(void) const
  {
    return m_mip_front.get();
  }

  RenderValue<const RenderClipElement*>
  render_value(void) const;

private:
  friend class RenderClipElement;

  Renderer::Implement *m_renderer;
  CullGeometrySimple m_clip_geometry;
  CullGeometryGroup::Token m_clip_geometry_token;
  reference_counted_ptr<const ImageMipElement> m_mip_front;
  MaskDetails m_mask_details;

  mutable RenderValue<const RenderClipElement*> m_render_value;

  /* enumerated by mask_type_t to give what
   * channel for each type; a value of number_mask_channel
   * means that mask type is not supported
   */
  vecN<enum mask_channel_t, number_mask_type> m_mask_channels;

  /* which mask value type is guaranteed to have an element */
  enum mask_type_t m_preferred_mask_type;
};

class astral::Renderer::Implement::ClipCombineResult:public RenderClipCombineResult
{
public:
  /* Enumeration to describe nature of a tile from
   * raw_fill().m_mask
   */
  enum combine_element_t:uint32_t
    {
      /* Tile is fully covered by the clip-in content
       * only; zero content from clip-out.
       */
      full_clip_in_element,

      /* Tile is fully covered by the clip-out content
       * only; zero content from clip-in.
       */
      full_clip_out_element,

      /* Tile is partially covered by the clip-in content
       * only; zero content from clip-out.
       */
      partial_clip_in_element,

      /* Tile is partially covered by the clip-out content
       * only; zero content from clip-in.
       */
      partial_clip_out_element,

      /* Tile has both clip-in and clip-out content */
      mixed_combine_element,

      /* tile is completely empty, no clip-out or clip-in
       * content
       */
      empty_combine_element,

      invalid_combine_element,
    };

  /* A ClipCombineResult needs to hold onto the meta-data
   * for the tiles so that clip_node_pixel() can be efficient.
   */
  class TileProperties
  {
  public:
    /* The tile type for the tile for the clip-in image */
    enum ImageMipElement::element_type_t m_clip_in_tile_type;

    /* The tile type for the tile for the clip-out image */
    enum ImageMipElement::element_type_t m_clip_out_tile_type;

    /* the tile classification */
    enum combine_element_t m_classification;
  };

  ClipCombineResult(void):
    m_renderer(nullptr)
  {}

  void
  init(Renderer::Implement &renderer, float render_tol,
       const Transformation &pixel_transformation_logical,
       const RenderClipElement &clip_element,
       const CombinedPath &path,
       const RenderClipCombineParams &params,
       enum Filler::clip_combine_mode_t clip_combine_mode);

  void
  init(Renderer::Implement &renderer,
       const ClipCombineResult &src,
       enum mask_type_t M);

  vecN<range_type<unsigned int>, 2>
  tile_range_entire(void) const
  {
    vecN<range_type<unsigned int>, 2> return_value;

    return_value.x().m_begin = 0u;
    return_value.x().m_end = m_tile_count.x();

    return_value.y().m_begin = 0u;
    return_value.y().m_end = m_tile_count.y();

    return return_value;
  }

  const TileProperties&
  tile_property(uvec2 tile) const
  {
    unsigned int idx;

    idx = tile_index(tile);
    return m_tile_properties[idx];
  }

  const MaskDetails&
  raw_fill(void) const
  {
    return m_raw_fill;
  }

  enum mask_channel_t
  clip_in_channel(void) const
  {
    return m_clip_in_channel;
  }

  enum mask_channel_t
  clip_out_channel(void) const
  {
    return m_clip_out_channel;
  }

  /* Returns range of tiles into raw_fill().m_mask
   * that clip-in region hits
   */
  const vecN<range_type<unsigned int>, 2>&
  clip_in_tile_range(void) const
  {
    return m_clip_in_tile_range;
  }

  /* Returns range of tiles into raw_fill().m_mask
   * that clip-out region hits
   */
  const vecN<range_type<unsigned int>, 2>&
  clip_out_tile_range(void) const
  {
    return m_clip_out_tile_range;
  }

  static
  reference_counted_ptr<const RenderClipElement>
  create_clip(Renderer::Implement &renderer,
              const vecN<enum mask_channel_t, number_mask_type> &mask_channels,
              const Image &image, const vecN<range_type<unsigned int>, 2> &tile_range,
              const CullGeometrySimple &clip_geometry, CullGeometryGroup::Token token,
              enum mask_type_t preferred_mask_type)
  {
    return create_clip_implement(renderer, nullptr,
                                 mask_channels, image, tile_range,
                                 clip_geometry, token, nullptr, preferred_mask_type);
  }

  static
  c_string
  label(enum combine_element_t Q);

private:
  friend class RenderClipCombineResult;

  class ClipInTileTypeTable;
  class ClipOutTileTypeTable;
  class ClassificationTable;

  unsigned int
  tile_index(uvec2 tile) const
  {
    ASTRALassert(tile.x() < m_tile_count.x());
    ASTRALassert(tile.y() < m_tile_count.y());

    return tile.x() + tile.y() * m_tile_count.x();
  }

  TileProperties&
  tile_property(uvec2 tile)
  {
    unsigned int idx;

    idx = tile_index(tile);
    return m_tile_properties[idx];
  }

  static
  reference_counted_ptr<const Image>
  create_image_implement(Renderer::Implement &renderer, ClipCombineResult *pthis,
                         const Image &image, const vecN<range_type<unsigned int>, 2> &tile_range,
                         enum ImageMipElement::element_type_t TileProperties::*v);

  static
  reference_counted_ptr<const RenderClipElement>
  create_clip_implement(Renderer::Implement &renderer, ClipCombineResult *pthis,
                        const vecN<enum mask_channel_t, number_mask_type> &mask_channels,
                        const Image &image, const vecN<range_type<unsigned int>, 2> &tile_range,
                        const CullGeometrySimple &clip_geometry, CullGeometryGroup::Token token,
                        enum ImageMipElement::element_type_t TileProperties::*v,
                        enum mask_type_t preferred_mask_type);

  reference_counted_ptr<const RenderClipElement>
  create_clip(const vecN<enum mask_channel_t, number_mask_type> &mask_channels,
              const Image &image, const vecN<range_type<unsigned int>, 2> &tile_range,
              const CullGeometrySimple &clip_geometry, CullGeometryGroup::Token token,
              enum ImageMipElement::element_type_t TileProperties::*v,
              enum mask_type_t preferred_mask_type)
  {
    return create_clip_implement(*m_renderer, this,
                                 mask_channels, image, tile_range,
                                 clip_geometry, token, v, preferred_mask_type);
  }

  Renderer::Implement *m_renderer;
  vecN<range_type<unsigned int>, 2> m_clip_in_tile_range, m_clip_out_tile_range;
  reference_counted_ptr<const RenderClipElement> m_clip_in, m_clip_out;
  enum mask_channel_t m_clip_in_channel, m_clip_out_channel;
  enum mask_type_t m_mask_type;
  MaskDetails m_raw_fill;

  uvec2 m_tile_count;
  std::vector<TileProperties> m_tile_properties;
  Filler::TileTypeTable m_mask_tiles_before_combine;
};

#endif

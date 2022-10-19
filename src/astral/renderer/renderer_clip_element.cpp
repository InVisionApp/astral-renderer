/*!
 * \file renderer_clip_element.cpp
 * \brief file renderer_clip_element.cpp
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

#include "renderer_clip_element.hpp"
#include "renderer_storage.hpp"
#include "renderer_filler.hpp"
#include "renderer_workroom.hpp"

namespace
{
  /* If the table_result is color_element, but the actual source of tiles image
   * is a full or empty tile, we need to use that value as the tile source
   */
  enum astral::ImageMipElement::element_type_t
  post_process_tile_type(enum astral::ImageMipElement::element_type_t from_table,
                         enum astral::ImageMipElement::element_type_t mip_result)
  {
    return (from_table == astral::ImageMipElement::color_element) ?
      mip_result :
      from_table;
  }

  class SubImageTileRange
  {
  public:
    SubImageTileRange(void):
      m_empty(true),
      m_value(astral::range_type<unsigned int>(0, 0))
    {}

    const astral::vecN<astral::range_type<unsigned int>, 2>&
    value(void) const
    {
      return m_value;
    }

    bool
    empty(void) const
    {
      return m_empty;
    }

    void
    add(unsigned int x, unsigned int y)
    {
      if (m_empty)
        {
          m_value.x() = astral::range_type<unsigned int>(x, x + 1);
          m_value.y() = astral::range_type<unsigned int>(y, y + 1);
          m_empty = false;
        }
      else
        {
          m_value.x().m_begin = astral::t_min(m_value.x().m_begin, x);
          m_value.x().m_end = astral::t_max(m_value.x().m_end, x + 1);

          m_value.y().m_begin = astral::t_min(m_value.y().m_begin, y);
          m_value.y().m_end = astral::t_max(m_value.y().m_end, y + 1);
        }
    }

  private:
    bool m_empty;
    astral::vecN<astral::range_type<unsigned int>, 2> m_value;
  };
}

////////////////////////////////////////////////////////////////////////////
// astral::Renderer::Implement::ClipCombineResult::ClipInTileTypeTable implementation
class astral::Renderer::Implement::ClipCombineResult::ClipInTileTypeTable
{
public:
  static
  enum ImageMipElement::element_type_t
  value(enum ImageMipElement::element_type_t clip_tile,
        enum ImageMipElement::element_type_t mask_tile,
        enum ImageMipElement::element_type_t mip_result)
  {
    return post_process_tile_type(singleton().m_values[clip_tile][mask_tile], mip_result);
  }

private:
  enum
    {
      number_element_type = ImageMipElement::number_element_type
    };

  static
  const ClipInTileTypeTable&
  singleton(void)
  {
    static ClipInTileTypeTable R;
    return R;
  }

  ClipInTileTypeTable(void)
  {
    for (unsigned int i = 0; i < number_element_type; ++i)
      {
        /* intersection of empty clip is always empty */
        m_values[ImageMipElement::empty_element][i] = ImageMipElement::empty_element;

        /* intersection of full clip is always what the mask is */
        m_values[ImageMipElement::white_element][i] = static_cast<enum astral::ImageMipElement::element_type_t>(i);
      }

    m_values[ImageMipElement::color_element][ImageMipElement::empty_element] = ImageMipElement::empty_element;
    m_values[ImageMipElement::color_element][ImageMipElement::white_element] = ImageMipElement::color_element;
    m_values[ImageMipElement::color_element][ImageMipElement::color_element] = ImageMipElement::color_element;
  }

  vecN<vecN<enum ImageMipElement::element_type_t, number_element_type>, number_element_type> m_values;
};


////////////////////////////////////////////////////////////////////////////
// astral::Renderer::Implement::ClipCombineResult::ClipOutTileTypeTable implementation
class astral::Renderer::Implement::ClipCombineResult::ClipOutTileTypeTable
{
public:
  static
  enum ImageMipElement::element_type_t
  value(enum ImageMipElement::element_type_t clip_tile,
        enum ImageMipElement::element_type_t mask_tile,
        enum ImageMipElement::element_type_t mip_result)
  {
    return post_process_tile_type(singleton().m_values[clip_tile][mask_tile], mip_result);
  }

private:
  enum
    {
      number_element_type = ImageMipElement::number_element_type
    };

  static
  const ClipOutTileTypeTable&
  singleton(void)
  {
    static ClipOutTileTypeTable R;
    return R;
  }

  static
  enum ImageMipElement::element_type_t
  invert(enum ImageMipElement::element_type_t v)
  {
    static const enum ImageMipElement::element_type_t values[number_element_type] =
      {
        [ImageMipElement::empty_element] = ImageMipElement::white_element,
        [ImageMipElement::white_element] = ImageMipElement::empty_element,
        [ImageMipElement::color_element] = ImageMipElement::color_element,
      };

    return values[v];
  }

  ClipOutTileTypeTable(void)
  {
    for (unsigned int i = 0; i < number_element_type; ++i)
      {
        enum ImageMipElement::element_type_t e;

        e = static_cast<enum astral::ImageMipElement::element_type_t>(i);

        /* intersection of empty clip is always empty */
        m_values[ImageMipElement::empty_element][i] = ImageMipElement::empty_element;

        /* intersection of full clip against the mask complement is the invert of the mask */
        m_values[ImageMipElement::white_element][i] = invert(e);
      }

    m_values[ImageMipElement::color_element][ImageMipElement::empty_element] = ImageMipElement::color_element;
    m_values[ImageMipElement::color_element][ImageMipElement::white_element] = ImageMipElement::empty_element;
    m_values[ImageMipElement::color_element][ImageMipElement::color_element] = ImageMipElement::color_element;
  }

  vecN<vecN<enum ImageMipElement::element_type_t, number_element_type>, number_element_type> m_values;
};

////////////////////////////////////////////////////////////////////////////
// astral::Renderer::Implement::ClipCombineResult::ClassificationTable implementation
class astral::Renderer::Implement::ClipCombineResult::ClassificationTable
{
public:
  static
  enum combine_element_t
  value(enum ImageMipElement::element_type_t clip_in_tile,
        enum ImageMipElement::element_type_t clip_out_tile)
  {
    return singleton().m_values[clip_in_tile][clip_out_tile];
  }

private:
  enum
    {
      number_element_type = ImageMipElement::number_element_type
    };

  static
  const ClassificationTable&
  singleton(void)
  {
    static ClassificationTable R;
    return R;
  }

  ClassificationTable(void)
  {
    /* initialize all with a bad value */
    for (unsigned int i = 0; i < number_element_type; ++i)
      {
        for (unsigned int j = 0; j < number_element_type; ++j)
          {
            m_values[i][j] = invalid_combine_element;
          }
      }

    /* if clip-in tile is marked as empty, then it is
     * full_clip_out_element, partial_clip_out_element
     * or empty_element
     */
    for (unsigned int i = 0; i < number_element_type; ++i)
      {
        /* keyed by the value of the clip-out */
        static const combine_element_t table[number_element_type] =
          {
            [ImageMipElement::empty_element] = empty_combine_element,
            [ImageMipElement::white_element] = full_clip_out_element,
            [ImageMipElement::color_element] = partial_clip_out_element,
          };

        ASTRALassert(invalid_combine_element == m_values[ImageMipElement::empty_element][i]
                     || table[i] == m_values[ImageMipElement::empty_element][i]);

        m_values[ImageMipElement::empty_element][i] = table[i];
      }

    /* if clip-out tile is marked as empty, then it is
     * full_clip_in_element, partial_clip_in_element
     * or empty_element
     */
    for (unsigned int i = 0; i < number_element_type; ++i)
      {
        /* keyed by the value of the clip-in */
        static const combine_element_t table[number_element_type] =
          {
            [ImageMipElement::empty_element] = empty_combine_element,
            [ImageMipElement::white_element] = full_clip_in_element,
            [ImageMipElement::color_element] = partial_clip_in_element,
          };

        ASTRALassert(invalid_combine_element == m_values[i][ImageMipElement::empty_element]
                     || table[i] == m_values[i][ImageMipElement::empty_element]);

        m_values[i][ImageMipElement::empty_element] = table[i];
      }

    /* if both elements are partial then we have a combine */
    m_values[ImageMipElement::color_element][ImageMipElement::color_element] = mixed_combine_element;

    #if 0
      {
        std::cout << "TableOfPain:\n";
        for (unsigned int clip_in = 0; clip_in < number_element_type; ++clip_in)
          {
            for (unsigned int clip_out = 0; clip_out < number_element_type; ++clip_out)
              {
                std::cout << "\tclip_in = " << astral::label(static_cast<enum ImageMipElement::element_type_t>(clip_in))
                          << ", clip_out = " << astral::label(static_cast<enum ImageMipElement::element_type_t>(clip_out))
                          << " ---> " << label(m_values[clip_in][clip_out]) << "\n";
              }
          }
      }
    #endif
  }

  vecN<vecN<enum combine_element_t, number_element_type>, number_element_type> m_values;
};

////////////////////////////////////////
// astral::Renderer::Implement::ClipElement methods
void
astral::Renderer::Implement::ClipElement::
init(Renderer::Implement &renderer,
     const ClipGeometrySimple &clip_geometry,
     ClipGeometryGroup::Token token,
     const reference_counted_ptr<const Image> &image,
     enum mask_type_t mask_type,
     enum mask_channel_t mask_channel)
{
  vecN<enum mask_channel_t, 2> mask_channels(number_mask_channel);

  mask_channels[mask_type] = mask_channel;
  init(renderer, clip_geometry, token, image, mask_channels, mask_type);
}

void
astral::Renderer::Implement::ClipElement::
init(Renderer::Implement &renderer,
     const ClipElement &src,
     enum mask_type_t M)
{
  ASTRALassert(src.m_renderer == &renderer);
  ASTRALassert(src.m_mask_channels[M] != number_mask_channel);
  ASTRALunused(renderer);

  m_renderer = src.m_renderer;
  m_clip_geometry = src.m_clip_geometry;
  m_clip_geometry_token = src.m_clip_geometry_token;
  m_mip_front = src.m_mip_front;
  m_mask_channels = src.m_mask_channels;

  m_render_value = RenderValue<const RenderClipElement*>();
  m_preferred_mask_type = M;

  m_mask_details = src.m_mask_details;
  m_mask_details.m_mask_channel = m_mask_channels[M];
  m_mask_details.m_mask_type = M;
}

void
astral::Renderer::Implement::ClipElement::
init(Renderer::Implement &renderer,
     const ClipGeometrySimple &clip_geometry,
     ClipGeometryGroup::Token token,
     const reference_counted_ptr<const Image> &image,
     vecN<enum mask_channel_t, number_mask_type> mask_channels,
     enum mask_type_t preferred_mask_type)
{
  ASTRALassert(!m_renderer);
  ASTRALassert(!m_mask_details.m_mask);
  ASTRALassert(!m_mip_front);

  m_renderer = &renderer;
  m_clip_geometry = clip_geometry;
  m_clip_geometry_token = token;
  m_mask_details.m_mask = image;
  m_mask_channels = mask_channels;

  /* make sure preferred mask type is supported */
  if (mask_channels[preferred_mask_type] == number_mask_channel)
    {
      ASTRALstatic_assert(number_mask_type == 2u);

      preferred_mask_type = static_cast<enum mask_type_t>(1 - preferred_mask_type);
      ASTRALassert(mask_channels[preferred_mask_type] != number_mask_channel);
    }

  m_preferred_mask_type = preferred_mask_type;
  if (m_mask_details.m_mask)
    {
      /* We always assume that the image was made with 2 pixels
       * of padding so that filtering works reliably.
       */
      vec2 padding(2.0f, 2.0f);

      m_mip_front = m_mask_details.m_mask->mip_chain().front();

      m_mask_details.m_min_corner = padding;
      m_mask_details.m_size = vec2(m_mask_details.m_mask->size()) - 2.0f * padding;
      m_mask_details.m_mask_type = m_preferred_mask_type;
      m_mask_details.m_mask_channel = m_mask_channels[m_preferred_mask_type];

      /* m_clip_geometry.image_transformation_pixel() is the transformation
       * from pixel coordinate to image coordinates. However, we need the
       * transformation to the sub-image that starts at the sub-image starting
       * at padding.
       */
      m_mask_details.m_mask_transformation_pixel = m_clip_geometry.image_transformation_pixel();
      m_mask_details.m_mask_transformation_pixel.m_translate -= padding;
    }
}

astral::RenderValue<const astral::RenderClipElement*>
astral::Renderer::Implement::ClipElement::
render_value(void) const
{
  ASTRALassert(m_renderer);
  if (m_render_value.backend() != m_renderer->m_backend.get())
    {
      m_render_value = m_renderer->m_backend->create_value(this);
    }

  ASTRALassert(m_render_value.valid());
  ASTRALassert(m_render_value.backend() == m_renderer->m_backend.get());

  return m_render_value;
}

////////////////////////////////////////
// astral::RenderClipElement methods
const astral::MaskDetails*
astral::RenderClipElement::
mask_details(void) const
{
  const Renderer::Implement::ClipElement *p;

  p = static_cast<const Renderer::Implement::ClipElement*>(this);
  ASTRALassert(p->m_renderer);

  if (p->m_mask_details.m_mask)
    {
      return &p->m_mask_details;
    }
  else
    {
      return nullptr;
    }
}

enum astral::mask_type_t
astral::RenderClipElement::
preferred_mask_type(void) const
{
  const Renderer::Implement::ClipElement *p;

  p = static_cast<const Renderer::Implement::ClipElement*>(this);
  ASTRALassert(p->m_renderer);
  return p->m_preferred_mask_type;
}

enum astral::mask_channel_t
astral::RenderClipElement::
mask_channel(enum mask_type_t v) const
{
  const Renderer::Implement::ClipElement *p;

  p = static_cast<const Renderer::Implement::ClipElement*>(this);
  ASTRALassert(p->m_renderer);
  return p->m_mask_channels[v];
}

astral::reference_counted_ptr<const astral::RenderClipElement>
astral::RenderClipElement::
intersect(BoundingBox<float> pixel_rect) const
{
  reference_counted_ptr<const RenderClipElement> return_value;
  const Renderer::Implement::ClipElement *p;

  p = static_cast<const Renderer::Implement::ClipElement*>(this);
  ASTRALassert(p->m_renderer);

  if (!p->m_mask_details.m_mask)
    {
      return this;
    }

  pixel_rect.intersect_against(p->m_mask_details.pixel_rect());
  if (pixel_rect.empty())
    {
      /* make an empty RenderClipElement */
      return p->m_renderer->m_storage->create_empty_clip_element(p->preferred_mask_type());
    }

  /* map to image coordnates to figure what tiles are hit by Q */
  BoundingBox<float> Q;
  Q = p->clip_geometry().image_transformation_pixel().apply_to_bb(pixel_rect);

  /* enlarge Q by 2 pixels on each side to make sure that pixel
   * padding is captured. Is this really needed?
   */
  Q.enlarge(vec2(2.0f, 2.0f));

  /* now, finally compute the tile range */
  const Rect &QQ(Q.as_rect());
  vecN<range_type<unsigned int>, 2> tile_range;
  ivec2 num_tiles(p->m_mask_details.m_mask->mip_chain().front()->tile_count());

  tile_range.x().m_begin = t_max(0, ImageAtlas::tile_from_texel(QQ.m_min_point.x(), 0));
  tile_range.y().m_begin = t_max(0, ImageAtlas::tile_from_texel(QQ.m_min_point.y(), 0));
  tile_range.x().m_end = t_min(num_tiles.x(), 1 + ImageAtlas::tile_from_texel(QQ.m_max_point.x(), 0));
  tile_range.y().m_end = t_min(num_tiles.y(), 1 + ImageAtlas::tile_from_texel(QQ.m_max_point.y(), 0));

  return_value = Renderer::Implement::ClipCombineResult::create_clip(*p->m_renderer, p->mask_channels(),
                                                                     *p->m_mask_details.m_mask, tile_range,
                                                                     p->clip_geometry(),
                                                                     p->clip_geometry_token(),
                                                                     p->preferred_mask_type());

  /* almost done, now we need to apply again pixel_rect
   * to the mask details of return_value
   */
  if (return_value->mask_details())
    {
      Renderer::Implement::ClipElement *q;

      q = const_cast<Renderer::Implement::ClipElement*>(static_cast<const Renderer::Implement::ClipElement*>(return_value.get()));
      q->m_mask_details.instersect_against_pixel_rect(pixel_rect);
    }

  return return_value;
}

astral::reference_counted_ptr<const astral::RenderClipElement>
astral::RenderClipElement::
as_mask_type(enum mask_type_t M) const
{
  reference_counted_ptr<const RenderClipElement> return_value;
  const Renderer::Implement::ClipElement *p;

  p = static_cast<const Renderer::Implement::ClipElement*>(this);
  ASTRALassert(p->m_renderer);

  if (!p->m_mask_details.m_mask || p->m_preferred_mask_type == M)
    {
      return this;
    }

  if (p->m_mask_channels[M] == number_mask_channel)
    {
      return nullptr;
    }

  reference_counted_ptr<const RenderClipElement> R;

  R = p->m_renderer->m_storage->create_clip_element(*p, M);
  return R;
}

astral::reference_counted_ptr<const astral::RenderClipElement>
astral::RenderClipElement::
as_mask_type_fall_back_this(enum mask_type_t M) const
{
  reference_counted_ptr<const RenderClipElement> return_value;
  const Renderer::Implement::ClipElement *p;

  p = static_cast<const Renderer::Implement::ClipElement*>(this);
  ASTRALassert(p->m_renderer);

  if (!p->m_mask_details.m_mask || p->m_preferred_mask_type == M || p->m_mask_channels[M] == number_mask_channel)
    {
      return this;
    }

  reference_counted_ptr<const RenderClipElement> R;

  R = p->m_renderer->m_storage->create_clip_element(*p, M);
  return R;
}

void
astral::RenderClipElement::
delete_object(RenderClipElement *in_p)
{
  Renderer::Implement::ClipElement *p;
  Renderer::Implement *r;

  p = static_cast<Renderer::Implement::ClipElement*>(in_p);

  r = p->m_renderer;
  ASTRALassert(r);

  /* release the reference to m_image */
  p->m_mask_details.m_mask = nullptr;
  p->m_mip_front = nullptr;

  /* make it really marked as not active */
  p->m_renderer = nullptr;

  /* it is -critical- to reset m_render_value because if this
   * object is recycled within the current Renderer::Implement::begin()/end()
   * pair, then without resetting m_render_value will have that
   * ClipElement::render_value() returns the RenderValue<> of
   * what it was before the recycle.
   */
  p->m_render_value = RenderValue<const RenderClipElement*>();

  /* reclaim memory for later */
  r->m_storage->reclaim_clip_element(p);
}

//////////////////////////////////////////////
// astral::Renderer::Implement::ClipCombineResult methods
astral::c_string
astral::Renderer::Implement::ClipCombineResult::
label(enum combine_element_t Q)
{
  static c_string labels[invalid_combine_element + 1] =
    {
     [full_clip_in_element] = "full_clip_in_element",
     [full_clip_out_element] = "full_clip_out_element",
     [partial_clip_in_element] = "partial_clip_in_element",
     [partial_clip_out_element] = "partial_clip_out_element",
     [mixed_combine_element] = "mixed_combine_element",
     [empty_combine_element] = "empty_combine_element",
     [invalid_combine_element] = "invalid_combine_element",
    };

  return labels[Q];
}

astral::reference_counted_ptr<const astral::Image>
astral::Renderer::Implement::ClipCombineResult::
create_image_implement(Renderer::Implement &renderer, ClipCombineResult *pthis,
                       const Image &image, const vecN<range_type<unsigned int>, 2> &tile_range,
                       enum ImageMipElement::element_type_t TileProperties::*v)
{
  WorkRoom &workroom(*renderer.m_workroom);
  vecN<std::vector<uvec2>, ImageMipElement::number_element_type> &tile_scratch(workroom.m_tile_scratch);
  reference_counted_ptr<const astral::Image> return_value;

  ASTRALassert(tile_scratch[ImageMipElement::empty_element].empty());
  ASTRALassert(tile_scratch[ImageMipElement::white_element].empty());
  ASTRALassert(tile_scratch[ImageMipElement::color_element].empty());
  ASTRALassert(!image.mip_chain().empty());
  ASTRALassert(image.mip_chain().front());

  for (unsigned int y = tile_range.y().m_begin; y < tile_range.y().m_end; ++y)
    {
      for (unsigned int x = tile_range.x().m_begin; x < tile_range.x().m_end; ++x)
        {
          uvec2 tile(x, y);
          enum ImageMipElement::element_type_t value;

          if (pthis)
            {
              TileProperties &tp(pthis->tile_property(tile));

              value = tp.*v;
            }
          else
            {
              value = image.mip_chain().front()->tile_type(tile);
            }

          if (value != ImageMipElement::color_element)
            {
              tile.x() -= tile_range.x().m_begin;
              tile.y() -= tile_range.y().m_begin;
            }

          tile_scratch[value].push_back(tile);
        }
    }

  return_value = VirtualBuffer::create_assembled_image(VB_TAG, renderer, image, tile_range,
                                                       make_c_array(tile_scratch[ImageMipElement::empty_element]),
                                                       make_c_array(tile_scratch[ImageMipElement::white_element]),
                                                       make_c_array(tile_scratch[ImageMipElement::color_element]));

  tile_scratch[ImageMipElement::empty_element].clear();
  tile_scratch[ImageMipElement::white_element].clear();
  tile_scratch[ImageMipElement::color_element].clear();

  return return_value;
}

astral::reference_counted_ptr<const astral::RenderClipElement>
astral::Renderer::Implement::ClipCombineResult::
create_clip_implement(Renderer::Implement &renderer, ClipCombineResult *pthis,
                      const vecN<enum mask_channel_t, number_mask_type> &mask_channels,
                      const Image &image, const vecN<range_type<unsigned int>, 2> &tile_range,
                      const ClipGeometrySimple &clip_geometry,
                      ClipGeometryGroup::Token token,
                      enum ImageMipElement::element_type_t TileProperties::*v,
                      enum mask_type_t preferred_mask_type)
{
  reference_counted_ptr<astral::RenderClipElement> return_value;
  reference_counted_ptr<const Image> sub_image;
  ClipGeometrySimple sub_geometry;
  ClipGeometryGroup::Token sub_token;
  uvec2 first_tile, last_tile, image_begin, image_end;
  const ImageMipElement *mip;

  ASTRALassert(!image.mip_chain().empty());
  ASTRALassert(image.mip_chain().front());

  mip = image.mip_chain().front().get();
  sub_image = create_image_implement(renderer, pthis, image, tile_range, v);

  ASTRALassert(tile_range.x().m_begin < tile_range.x().m_end);
  ASTRALassert(tile_range.y().m_begin < tile_range.y().m_end);

  first_tile = uvec2(tile_range.x().m_begin, tile_range.y().m_begin);
  last_tile = uvec2(tile_range.x().m_end - 1u, tile_range.y().m_end - 1u);

  image_begin = mip->tile_location(first_tile);
  image_end = mip->tile_location(last_tile) + mip->tile_size(last_tile);

  sub_geometry = clip_geometry.sub_geometry(image_begin, image_end);
  sub_token = token.intersect_against(*renderer.m_storage, sub_geometry.pixel_rect());

  return_value = renderer.m_storage->create_clip_element(sub_geometry, token, sub_image, mask_channels, preferred_mask_type);

  return return_value;
}

void
astral::Renderer::Implement::ClipCombineResult::
init(Renderer::Implement &renderer, const ClipCombineResult &src, enum mask_type_t M)
{
  ASTRALassert(src.m_mask_type != M);
  ASTRALassert(&renderer == src.m_renderer);

  m_renderer = &renderer;
  m_clip_in_tile_range = src.m_clip_in_tile_range;
  m_clip_out_tile_range = src.m_clip_out_tile_range;

  m_clip_in = src.m_clip_in->as_mask_type(M);
  ASTRALassert(m_clip_in);

  m_clip_out = src.m_clip_out->as_mask_type(M);
  ASTRALassert(m_clip_out);

  m_clip_in_channel = m_clip_in->mask_channel(M);
  ASTRALassert(m_clip_in_channel != number_mask_channel);

  m_clip_out_channel = m_clip_out->mask_channel(M);
  ASTRALassert(m_clip_out_channel != number_mask_channel);

  m_mask_type = M;
  m_raw_fill = src.m_raw_fill;

  m_tile_count = src.m_tile_count;
  m_tile_properties.resize(src.m_tile_properties.size());
  std::copy(src.m_tile_properties.begin(), src.m_tile_properties.end(), m_tile_properties.begin());
  src.m_mask_tiles_before_combine.copy(&m_mask_tiles_before_combine);
}

void
astral::Renderer::Implement::ClipCombineResult::
init(Renderer::Implement &renderer, float render_tol,
     const Transformation &pixel_transformation_logical,
     const RenderClipElement &in_clip_element,
     const CombinedPath &path,
     const RenderClipCombineParams &params,
     enum Filler::clip_combine_mode_t clip_combine_mode)
{
  const ClipElement *clip_element;
  const ImageMipElement *mip, *mip_result;
  ScaleTranslate mask_transformation_pixel;
  Transformation mask_transformation_logical;
  MaskDetails raw;
  vecN<enum mask_channel_t, number_mask_type> mask_channels;
  SubImageTileRange clip_in_range, clip_out_range;
  bool use_mask_shader;

  ASTRALassert(!m_renderer);

  clip_element = static_cast<const ClipElement*>(&in_clip_element);

  m_renderer = &renderer;
  m_mask_type = clip_element->preferred_mask_type();
  m_clip_in_channel = Filler::mask_channel_clip_in(m_mask_type);
  m_clip_out_channel = Filler::mask_channel_clip_out(m_mask_type);

  if (!clip_element->image())
    {
      ClipGeometrySimple empty;
      ClipGeometryGroup::Token empty_token;

      m_renderer = &renderer;

      m_clip_in = m_renderer->m_storage->create_clip_element(empty, empty_token, nullptr, m_mask_type, m_clip_in_channel);
      m_clip_out = m_renderer->m_storage->create_clip_element(empty, empty_token, nullptr, m_mask_type, m_clip_out_channel);
      m_clip_in_tile_range = m_clip_out_tile_range = vecN<range_type<unsigned int>, 2>(range_type<unsigned int>(0, 0));

      m_raw_fill.m_mask = nullptr;
      m_raw_fill.m_min_corner = vec2(0.0f, 0.0f);
      m_raw_fill.m_size = vec2(0.0f, 0.0f);
      m_raw_fill.m_mask_channel = Filler::mask_channel_clip_in(m_mask_type);
      m_raw_fill.m_mask_type = m_mask_type;
      m_raw_fill.m_mask_transformation_pixel = ScaleTranslate();

      m_tile_count = uvec2(0, 0);
      m_tile_properties.clear();
      m_mask_tiles_before_combine.clear();

      return;
    }

  mip = clip_element->mip_front();
  ASTRALassert(mip);

  mask_transformation_pixel = clip_element->clip_geometry().image_transformation_pixel();
  mask_transformation_logical = Transformation(mask_transformation_pixel) * pixel_transformation_logical;

  /* used the -CombinePath- bounding box, not the clip-geometry bounding box,
   * to decide if a mask shader should be used
   */
  use_mask_shader = path.paths<AnimatedPath>().empty() && params.m_path_shader.get() != nullptr;
  if (use_mask_shader)
    {
      BoundingBox<float> bb;
      ivec2 sz;

      bb = mask_transformation_logical.apply_to_bb(path.compute_bounding_box());
      bb.intersect_against(clip_element->clip_geometry().pixel_rect());
      sz = ivec2(bb.size());

      use_mask_shader = params.use_mask_shader(sz);
    }

  if (use_mask_shader)
    {
      Renderer::Implement::Filler::create_mask_via_item_path_shader(renderer, params.m_path_shader,
                                                         render_tol, params.m_fill_rule,
                                                         path, clip_element->clip_geometry(),
                                                         mask_transformation_logical,
                                                         clip_element,
                                                         &m_mask_tiles_before_combine,
                                                         &m_raw_fill);
    }
  else
    {
      m_renderer->m_filler[params.m_sparse]->create_mask(render_tol, params.m_fill_rule, params.m_aa_mode,
                                                         path,
                                                         clip_element->clip_geometry(),
                                                         clip_element->clip_geometry_token().sub_rects(*m_renderer->m_storage),
                                                         mask_transformation_logical,
                                                         clip_element,
                                                         clip_combine_mode,
                                                         &m_mask_tiles_before_combine,
                                                         &m_raw_fill);
    }

  ASTRALassert(m_raw_fill.m_mask);
  ASTRALassert(!m_raw_fill.m_mask->mip_chain().empty());

  /* TODO: examine raw.m_mask->mip().front() to see if we should trim tiles from
   *       the left, right, top or bottom sides.
   */
  mip_result = m_raw_fill.m_mask->mip_chain().front().get();
  m_tile_count = m_mask_tiles_before_combine.tile_count();
  ASTRALassert(m_tile_count == mip_result->tile_count());

  /* We know the tile-counts and so resize m_tile_properties */
  m_tile_properties.resize(m_tile_count.x() * m_tile_count.y());

  /* walk the tiles in mip_result against the tiles of mip to classify
   * the tiles for implementation of clip_node_pixel() that takes
   * a const-reference to a RenderClipCombineResult to describe the mask.
   */
  for (unsigned int y = 0; y < m_tile_count.y(); ++y)
    {
      for (unsigned int x = 0; x < m_tile_count.x(); ++x)
        {
          uvec2 tile(x, y);
          TileProperties &tp(tile_property(tile));
          enum ImageMipElement::element_type_t clip_tile, mask_tile;

          clip_tile = mip->tile_type(tile);
          mask_tile = m_mask_tiles_before_combine.fill_tile_type(tile);

          tp.m_clip_in_tile_type = ClipInTileTypeTable::value(clip_tile, mask_tile, mip_result->tile_type(tile));
          tp.m_clip_out_tile_type = ClipOutTileTypeTable::value(clip_tile, mask_tile, mip_result->tile_type(tile));
          tp.m_classification = ClassificationTable::value(tp.m_clip_in_tile_type, tp.m_clip_out_tile_type);
          ASTRALassert(tp.m_classification != invalid_combine_element);

          if (tp.m_clip_in_tile_type != ImageMipElement::empty_element)
            {
              clip_in_range.add(x, y);
            }

          if (tp.m_clip_out_tile_type != ImageMipElement::empty_element)
            {
              clip_out_range.add(x, y);
            }
        }
    }

  m_clip_in_tile_range = clip_in_range.value();
  m_clip_out_tile_range = clip_out_range.value();

  if (clip_in_range.empty())
    {
      m_clip_in = m_renderer->m_storage->create_empty_clip_element(m_mask_type);
    }
  else
    {
      /* ClipIn is written to red and green channels, see \ref image_blit_stc_mask_processing
       * and \ref image_blit_direct_mask_processing
       */
      mask_channels[mask_type_coverage] = clip_element->supports_mask_type(mask_type_coverage) ?
        Filler::mask_channel_clip_in(mask_type_coverage) :
        number_mask_channel;

      mask_channels[mask_type_distance_field] = clip_element->supports_mask_type(mask_type_distance_field) ?
        Filler::mask_channel_clip_in(mask_type_distance_field) :
        number_mask_channel;

      m_clip_in = create_clip(mask_channels, *m_raw_fill.m_mask, clip_in_range.value(),
                              clip_element->clip_geometry(),
                              clip_element->clip_geometry_token(),
                              &TileProperties::m_clip_in_tile_type,
                              clip_element->preferred_mask_type());
    }

  if (clip_out_range.empty() || clip_combine_mode == Filler::clip_combine_intersect_only)
    {
      m_clip_out = m_renderer->m_storage->create_empty_clip_element(m_mask_type);
    }
  else
    {
      /* ClipOut is written to blue and alpha channels, see \ref image_blit_stc_mask_processing
       * and \ref image_blit_direct_mask_processing
       */
      mask_channels[mask_type_coverage] = clip_element->supports_mask_type(mask_type_coverage) ?
        Filler::mask_channel_clip_out(mask_type_coverage) :
        number_mask_channel;

      mask_channels[mask_type_distance_field] = clip_element->supports_mask_type(mask_type_distance_field) ?
        Filler::mask_channel_clip_out(mask_type_distance_field) :
        number_mask_channel;

      m_clip_out = create_clip(mask_channels, *m_raw_fill.m_mask, clip_out_range.value(),
                               clip_element->clip_geometry(),
                               clip_element->clip_geometry_token(),
                               &TileProperties::m_clip_out_tile_type,
                               clip_element->preferred_mask_type());
    }
}

///////////////////////////////////////////
// astral::RenderClipCombineResult methods
const astral::reference_counted_ptr<const astral::RenderClipElement>&
astral::RenderClipCombineResult::
clip_in(void) const
{
  const Renderer::Implement::ClipCombineResult *p;

  p = static_cast<const Renderer::Implement::ClipCombineResult*>(this);
  return p->m_clip_in;
}

const astral::reference_counted_ptr<const astral::RenderClipElement>&
astral::RenderClipCombineResult::
clip_out(void) const
{
  const Renderer::Implement::ClipCombineResult *p;

  p = static_cast<const Renderer::Implement::ClipCombineResult*>(this);
  return p->m_clip_out;
}

enum astral::mask_type_t
astral::RenderClipCombineResult::
mask_type(void) const
{
  const Renderer::Implement::ClipCombineResult *p;

  p = static_cast<const Renderer::Implement::ClipCombineResult*>(this);
  return p->m_mask_type;
}

astral::reference_counted_ptr<const astral::RenderClipCombineResult>
astral::RenderClipCombineResult::
as_mask_type(enum mask_type_t M) const
{
  const Renderer::Implement::ClipCombineResult *p;

  p = static_cast<const Renderer::Implement::ClipCombineResult*>(this);
  if (p->m_mask_type == M)
    {
      return this;
    }

  if (p->m_clip_in->mask_channel(M) == number_mask_channel
      || p->m_clip_out->mask_channel(M) == number_mask_channel)
    {
      return nullptr;
    }

  return p->m_renderer->m_storage->create_clip_combine_result(*p, M);
}

void
astral::RenderClipCombineResult::
delete_object(RenderClipCombineResult *in_p)
{
  Renderer::Implement::ClipCombineResult *p;
  Renderer::Implement *r;

  p = static_cast<Renderer::Implement::ClipCombineResult*>(in_p);

  r = p->m_renderer;
  ASTRALassert(r);

  p->m_renderer = nullptr;
  p->m_clip_in = nullptr;
  p->m_clip_out = nullptr;
  p->m_raw_fill.m_mask = nullptr;
  p->m_tile_properties.clear();
  p->m_mask_tiles_before_combine.clear();

  r->m_storage->reclaim_clip_combine_result(p);
}

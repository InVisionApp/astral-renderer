/*!
 * \file renderer_filler_common_clipping.hpp
 * \brief file renderer_filler_common_clipping.hpp
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

#ifndef ASTRAL_RENDERER_FILLER_COMMON_CLIPPER_HPP
#define ASTRAL_RENDERER_FILLER_COMMON_CLIPPER_HPP

#include <astral/util/memory_pool.hpp>
#include <astral/renderer/renderer.hpp>
#include <astral/renderer/shader/fill_stc_shader.hpp>
#include "renderer_shared_util.hpp"
#include "renderer_cached_combined_path.hpp"
#include "renderer_virtual_buffer.hpp"
#include "renderer_filler.hpp"

class astral::Renderer::Implement::Filler::CommonClipper:public Filler
{
public:
  explicit
  CommonClipper(Renderer::Implement &renderer):
    Filler(renderer)
  {}

protected:
  enum line_t:uint32_t
    {
      x_fixed = 0,
      y_fixed,
    };

  enum side_t:uint32_t
    {
      minx_side = x_fixed,
      maxx_side = x_fixed | 2u,
      miny_side = y_fixed,
      maxy_side = y_fixed | 2u
    };

  static
  enum line_t
  line_t_from_side_t(enum side_t s)
  {
    return static_cast<enum line_t>(s & 1u);
  }

  static
  bool
  is_max_side(enum side_t s)
  {
    return (s & 2u) == 2u;
  }

  static
  int
  fixed_coordinate(enum line_t l)
  {
    return l;
  }

  static
  c_string
  label(enum side_t side);

  bool
  valid_subrect(int X, int Y) const
  {
    return 0 <= X && X < m_number_elementary_rects.x()
      && 0 <= Y && Y < m_number_elementary_rects.y();
  }

  bool
  valid_subrect(ivec2 v) const
  {
    return valid_subrect(v.x(), v.y());
  }

  unsigned int
  number_subrects(void) const
  {
    return m_number_elementary_rects.x() * m_number_elementary_rects.y();
  }

  int
  subrect_id(int X, int Y)
  {
    ASTRALassert(valid_subrect(X, Y));
    return X + Y * m_number_elementary_rects.x();
  }

  int
  subrect_id(const ivec2 &R)
  {
    return subrect_id(R.x(), R.y());
  }

  ivec2
  subrect_from_id(unsigned int ID)
  {
    ivec2 R;

    ASTRALassert(ID < number_subrects());

    R.y() = ID / m_number_elementary_rects.x();
    R.x() = ID - m_number_elementary_rects.x() * R.y();
    ASTRALassert(valid_subrect(R));

    return R;
  }

  /* Returns the range of boxes that include the point p;
   * the range is clamped in both coordinate to
   * [0, m_number_elementary_rects)
   */
  vecN<range_type<int>, 2>
  subrect_from_coordinate(vec2 p)
  {
    return subrect_range_from_coordinate(p, p);
  }

  /* returns the box-range for a coordinate value but does NOT
   * clamp the return value to be within [0, m_number_elementary_rects)
   */
  range_type<int>
  subrect_from_coordinate(float v)
  {
    float min_v, max_v;
    range_type<int> return_value;

    min_v = v - ImageAtlas::tile_padding;
    max_v = v + ImageAtlas::tile_padding;

    min_v *= m_reciprocal_elementary_rect_size;
    max_v *= m_reciprocal_elementary_rect_size;

    return_value.m_begin = static_cast<int>(min_v);
    return_value.m_end = 1 + static_cast<int>(max_v);

    return return_value;
  }

  /* returns the box-range for a coordinate value clamped
   * to [0, m_number_elementary_rects[fc])
   */
  range_type<int>
  subrect_from_coordinate(float v, int fc)
  {
    range_type<int> return_value;

    return_value = subrect_from_coordinate(v);
    return_value.m_begin = t_max(0, t_min(m_number_elementary_rects[fc], return_value.m_begin));
    return_value.m_end = t_max(0, t_min(m_number_elementary_rects[fc], return_value.m_end));

    return return_value;
  }

  /* computes the sub-rect range of a bounding box;
   * the range is clamped in both coordinate to
   * [0, m_number_elementary_rects)
   */
  vecN<range_type<int>, 2>
  subrect_range_from_coordinate(vec2 min_pt, vec2 max_pt)
  {
    vecN<range_type<int>, 2> return_value;

    // add padding
    min_pt -= vec2(ImageAtlas::tile_padding, ImageAtlas::tile_padding);
    max_pt += vec2(ImageAtlas::tile_padding, ImageAtlas::tile_padding);

    // covert to "sub-rectangle"
    min_pt *= m_reciprocal_elementary_rect_size;
    max_pt *= m_reciprocal_elementary_rect_size;

    // clamp
    return_value.x().m_begin = t_max(0, t_min(m_number_elementary_rects.x(), static_cast<int>(min_pt.x())));
    return_value.x().m_end = t_max(0, t_min(m_number_elementary_rects.x(), 1 + static_cast<int>(max_pt.x())));

    return_value.y().m_begin = t_max(0, t_min(m_number_elementary_rects.y(), static_cast<int>(min_pt.y())));
    return_value.y().m_end = t_max(0, t_min(m_number_elementary_rects.y(), 1 + static_cast<int>(max_pt.y())));

    return return_value;
  }

  /* Returns the x-min side for rects in the X'th column
   * with 0 <= X < m_number_elementary_rects.x(). Value
   * includes the padding.
   */
  int
  minx_side_value(int X)
  {
    ASTRALassert(0 <= X && X < m_number_elementary_rects.x());
    return -ImageAtlas::tile_padding + X * ImageAtlas::tile_size_without_padding;
  }

  /* Returns the x-max side for rects in the X'th column
   * with 0 <= X < m_number_elementary_rects.x(). Value
   * includes the padding.
   */
  int
  maxx_side_value(int X)
  {
    ASTRALassert(0 <= X && X < m_number_elementary_rects.x());
    return ImageAtlas::tile_padding + (X + 1) * ImageAtlas::tile_size_without_padding;
  }

  /* Returns the y-min side for rects in the Y'th row
   * with 0 <= Y < m_number_elementary_rects.y(). Value
   * includes the padding.
   */
  int
  miny_side_value(int Y)
  {
    ASTRALassert(0 <= Y && Y < m_number_elementary_rects.y());
    return -ImageAtlas::tile_padding + Y * ImageAtlas::tile_size_without_padding;
  }

  /* Returns the y-max side for rects in the Y'th row
   * with 0 <= Y < m_number_elementary_rects.y(). Value
   * includes the padding.
   */
  int
  maxy_side_value(int Y)
  {
    ASTRALassert(0 <= Y && Y < m_number_elementary_rects.y());
    return ImageAtlas::tile_padding + (Y + 1) * ImageAtlas::tile_size_without_padding;
  }

  bool
  inside_of_box(vec2 p, int V, enum side_t s)
  {
    int c;
    float fv;

    c = fixed_coordinate(line_t_from_side_t(s));
    fv = side_value(V, s);

    return is_max_side(s) ?
      p[c] < fv :
      p[c] > fv;
  }

  /* Calls the appropriate FOO_side_value() from
   * an enum side_t. Value includes the padding.
   */
  int
  side_value(int V, enum side_t s);

  /* Set the sub-rect size and count, i.e. the values of
   * m_number_elementary_rects, ImageAtlas::tile_size_without_padding
   * \param mask_size size of the area covered by the fill
   */
  void
  set_subrect_values(ivec2 mask_size, const ClipElement *clip_element);

  /* From the current state of m_builder, create the streaming blocks
   * for the attribute and static data for it and return the vertex
   * streaming blocks for that data.
   */
  void
  create_blocks_from_builder(FillSTCShader::PassSet pass_set,
                             vecN<range_type<unsigned int>, FillSTCShader::pass_count> *out_vert_blocks);

  /* Clears data handled by Filler::CommonClipper */
  void
  cleanup_common(void);

  //////////////////////////////////////////////
  // virtual methods to build a sparse Image

  /* Get the encoder that renders to the named elementary
   * rect; if the rect is completely full or empty, return
   * an invalid encoder. If an encoder is returned, the
   * backing image of it must be the same size as
   * rect_size_without_padding(X, Y) and represent rendering
   * to [minx_side_value(X), maxx_side_value(X)]x[miny_side_value(Y), maxy_side_value(Y)]
   */
  virtual
  RenderEncoderImage
  rect_encoder(int X, int Y) = 0;

  /* Get the addition to the winding number for
   * the named elementar rect.
   */
  virtual
  int
  rect_winding_offset(int X, int Y) = 0;

  /////////////////////////////////////////////////////
  // member variables and functions for a derived class
  // to use to gets its job done.

  /* Returns the STCData::BuilderSet for the named rect */
  STCData::BuilderSet*
  stc_builder_for_rect(int X, int Y);

  /* routine to handle the logic of building a sparse Image
   * using the virtual methods rect_encoder() and rect_winding_offset().
   * \param item_data ItemData object filled by FillSTCShader with
   *                  scale factor as (1.0, 1.0) and time as 0.0
   * \param clip_element if non-null, its empty tiles are skipped
   * \param out_clip_combine_tile_data if non-null, location to which to write
   *                                   the tile-types before the clip-combining
   */
  reference_counted_ptr<const Image>
  create_sparse_image_from_rects(ItemData item_data,
                                 const ClipElement *clip_element,
                                 enum clip_combine_mode_t clip_combine_mode,
                                 TileTypeTable *out_clip_combine_tile_data);

  /* the total size of the mask */
  ivec2 m_total_size;

  /* the number of rects B. The size of each elementary
   * WITH padding is gauranteed to be nore more than
   * the size of a tile in a Image; indeed for all
   * except the last row and last column, that is the
   * the width and height of each.
   */
  ivec2 m_number_elementary_rects;

  /* reciprocal of ImageAtlas::tile_size_without_padding */
  float m_reciprocal_elementary_rect_size;

  /* the attribute generator */
  FillSTCShader::Data m_builder;
  std::vector<std::pair<ContourCurve, bool>> m_builder_helper;

private:
  /* Under nonzero_fill_rule or complement_nonzero_fill_rule, when
   * a SubRect has m_winding_offset as non-zero, we need to add
   * rects winding around the boundary. A WindingRect has three
   * properties:
   *   - is it the last rect in a column ?
   *   - is it the last rect in a row?
   *   - do we orient to increase or decrease the winding number?
   */
  class WindingRect
  {
  public:
    WindingRect(void):
      m_ready(false)
    {}

    void
    reset(void)
    {
      m_ready = false;
    }

    c_array<const VertexStreamerBlock>
    blocks(Filler::CommonClipper &filler, bool last_rect_X, bool last_rect_Y, bool increase_winding);

  private:
    /* if it is ready or not */
    bool m_ready;

    /* properties */
    bool m_last_rect_X, m_last_rect_Y, m_increase_winding;

    /* the range of blocks from VertexStreamer::request_blocks() */
    vecN<range_type<unsigned int>, FillSTCShader::pass_count> m_vert_blocks;
  };

  /* Fetch (and make ready) the requested WindingRect */
  c_array<const VertexStreamerBlock>
  winding_rect(bool last_rectX, bool last_rectY, bool increase_winding)
  {
    int X(last_rectX), Y(last_rectY), W(increase_winding);
    int idx(X + 2 * Y + 4 * W);

    return m_winding_rects[idx].blocks(*this, last_rectX, last_rectY, increase_winding);
  }

  /* Cache of WindingRect values needed for when filling with
   * a non_zero fill rule and the winding offset of a SubRect
   * is nonzero
   */
  vecN<WindingRect, 8> m_winding_rects;

  /* Our pool of STCData::BuilderSet objects;
   * a derived class is expected to get a pool
   * via allocate_stc_builder() and return those
   * objects in stc_builder_for_rect().
   */
  ObjectPoolClear<STCData::BuilderSet> m_stc_builder_pool;

  /* the 1-dimensional array that gives a pointer to
   * an STCData::BuilderSet; the objects are taken
   * from the pool on demand.
   */
  std::vector<STCData::BuilderSet*> m_stc_builders_for_rects;

  /* our finalized data for building a sparse Image */
  std::vector<uvec2> m_empty_tiles;
  std::vector<uvec2> m_fully_covered_tiles;
  std::vector<std::pair<uvec2, VirtualBuffer::TileSource>> m_element_tiles;
  std::vector<std::pair<uvec2, VirtualBuffer::TileSourceImage>> m_image_tiles;
};

#endif

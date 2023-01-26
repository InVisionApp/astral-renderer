/*!
 * \file renderer_tile_hit_detection.hpp
 * \brief file renderer_tile_hit_detection.hpp
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

#ifndef ASTRAL_RENDERER_TILE_HIT_DETECTION_HPP
#define ASTRAL_RENDERER_TILE_HIT_DETECTION_HPP

#include <astral/util/memory_pool.hpp>
#include "renderer_implement.hpp"
#include "renderer_draw_command.hpp"

/* TileHitDetection computes the tiles that do not have a draw
 * intersect them. The purpose is to allow for when VirtualBuffer
 * creates its backing astral::Image, that image can be sparse.
 */
class astral::Renderer::Implement::TileHitDetection:astral::noncopyable
{
public:
  ~TileHitDetection()
  {
    /* issue clear on the pool because MemoryPool expects that
     * the pool is empty at dtor.
     */
    m_pool.clear();
  }

  /* Computes what tiles are empty, the returned array is
   * valid until compute_empty_tiles() is called again.
   * \param storage Storage object holding the backing for the CullGeometryGroup
   * \param geometry CullGeometryGroup from which to derive the
   *                 transformation between the image and drawing
   *                 commands and to also specify the regions that
   *                 can be backed
   * \param cmds command list to walk, tiles that are not hit
   *             by any command are regarded as empty tiles
   * \param use_pixel_rect_tile_culling if true instead of using
   *                                    the clip-equations of the
   *                                    CullGeometry values, just use
   *                                    the pixel rects instead.
   * \param out_image_bounding_box location to which to write the
   *                               bounding box of the portion of
   *                               the image hit.
   */
  c_array<const uvec2>
  compute_empty_tiles(Storage &storage, const CullGeometryGroup &geometry,
                      const DrawCommandList &cmds,
                      bool use_pixel_rect_tile_culling,
                      BoundingBox<int> *out_image_bounding_box)
  {
    return compute_empty_tiles_implement(storage, geometry, &cmds,
                                         use_pixel_rect_tile_culling,
                                         out_image_bounding_box);
  }

  /* Computes what tiles are empty, the returned array is
   * valid until compute_empty_tiles() is called again.
   * \param storage Storage object holding the backing for the CullGeometryGroup
   * \param geometry CullGeometryGroup specifying the regions
   *                 that can be backed; tiles outside of the regions
   *                 are regarded as empty tiles.
   * \param use_pixel_rect_tile_culling if true instead of using
   *                                    the clip-equations of the
   *                                    CullGeometry values, just use
   *                                    the pixel rects instead.
   * \param out_image_bounding_box location to which to write the
   *                               bounding box of the portion of
   *                               the image hit.
   */
  c_array<const uvec2>
  compute_empty_tiles(Storage &storage, const CullGeometryGroup &geometry,
                      bool use_pixel_rect_tile_culling,
                      BoundingBox<int> *out_image_bounding_box)
  {
    return compute_empty_tiles_implement(storage, geometry, nullptr,
                                         use_pixel_rect_tile_culling,
                                         out_image_bounding_box);
  }

private:
  class Base;
  class Leaf;
  class Node;
  class Pool;
  class RectAdder;
  class ClipEqStack;

  class Base
  {
  public:
    enum backing_status_t
      {
        /* entire region is completely backed by pixels */
        is_completely_backed,

        /* entire region has no backing */
        is_completely_unbacked,

        /* region has portions that are backed an portions that are unbacked */
        mixed_backing,
      };

    Base(void):
      m_backing_status(is_completely_unbacked),
      m_has_unhit_tiles(true)
    {}

    bool
    has_unhit_tiles(void) const
    {
      return m_has_unhit_tiles;
    }

    enum backing_status_t
    backing_status(void) const
    {
      return m_backing_status;
    }

    const BoundingBox<float>&
    padded_box(void) const
    {
      return m_padded_box;
    }

    virtual
    void
    mark_all_tiles_hit(void) = 0;

    virtual
    void
    mark_is_completely_backed(void) = 0;

    /* box is in "pixel" coordinates coming from CullGeometry */
    virtual
    void
    add_hits(const TransformedBoundingBox &pixel_rect) = 0;

    void
    add_backed_region(Storage &storage, ClipEqStack &eq_stack,
                      bool use_pixel_rect_tile_culling,
                      const CullGeometry &geometry);

    virtual
    void
    add_backed_region_rect(const BoundingBox<float> &pixel_rect) = 0;

    virtual
    void
    add_backed_region_clip_eq(const BoundingBox<float> &pixel_rect, ClipEqStack &eq_stack) = 0;

  protected:
    /* padded box in "pixel" coordinates coming from CullGeometry */
    BoundingBox<float> m_padded_box;

    enum backing_status_t m_backing_status;

    /* true if this Base or a descendant of it has any backed tiles
     * that have not been hit by a draw. This is used to early out
     * from add_hits().
     */
    bool m_has_unhit_tiles;
  };

  class Leaf:public Base
  {
  public:
    Leaf(const ScaleTranslate &pixel_transformation_image,
         uvec2 tile_id, const BoundingBox<float> &parent_bb);

    virtual
    void
    mark_all_tiles_hit(void) override final;

    virtual
    void
    mark_is_completely_backed(void) override final;

    virtual
    void
    add_hits(const TransformedBoundingBox &pixel_rect) override final;

    virtual
    void
    add_backed_region_rect(const BoundingBox<float> &pixel_rect) override final;

    virtual
    void
    add_backed_region_clip_eq(const BoundingBox<float> &pixel_rect, ClipEqStack &eq_stack) override final;

    uvec2
    tile_id(void) const
    {
      return m_tile_id;
    }

  private:
    uvec2 m_tile_id;
  };

  class Node:public Base
  {
  public:
    Node(Pool &pool,
         const ScaleTranslate &pixel_transformation_image,
         vecN<range_type<unsigned int>, 2> tile_range,
         const BoundingBox<float> &parent_bb);

    virtual
    void
    mark_all_tiles_hit(void) override final;

    virtual
    void
    mark_is_completely_backed(void) override final;

    virtual
    void
    add_hits(const TransformedBoundingBox &pixel_rect) override final;

    virtual
    void
    add_backed_region_rect(const BoundingBox<float> &pixel_rect) override final;

    virtual
    void
    add_backed_region_clip_eq(const BoundingBox<float> &pixel_rect, ClipEqStack &eq_stack) override final;

  private:
    vecN<Base*, 2> m_children;
  };

  class Pool
  {
  public:
    void
    clear(void)
    {
      m_leaf_pool.clear();
      m_node_pool.clear();
    }

    c_array<const pointer<Leaf>>
    created_leaves(void)
    {
      return m_leaf_pool.created_objects();
    }

    Base*
    create(const ScaleTranslate &pixel_transformation_image,
           vecN<range_type<unsigned int>, 2> tile_range,
           const BoundingBox<float> &parent_bb);

  private:
    MemoryPoolTracked<Leaf, 4096> m_leaf_pool;
    MemoryPoolTracked<Node, 4096> m_node_pool;
  };

  /* Represents a stack of clipping equations */
  class ClipEqStack:astral::noncopyable
  {
  public:
    /* initialize the stack with the passed clipping equations */
    void
    init(c_array<const vec3> eqs)
    {
      m_stack.clear();

      m_backing.resize(eqs.size());
      std::copy(eqs.begin(), eqs.end(), m_backing.begin());

      m_stack.push_back(range_type<unsigned int>(0, eqs.size()));
    }

    /* push onto the stack the intersection of the clipping
     * equations at the top of the stack against a rect.
     * Returns true if the rect is unclipped against the clipping
     * equations.
     */
    bool
    push_intersect(const BoundingBox<float> &rect);

    /* Returns the current clipping equations; if empty
     * that represents that the region of clipping is
     * empty.
     */
    c_array<const vec3>
    current_clipping(void) const
    {
      ASTRALassert(!m_stack.empty());
      return make_c_array(m_backing).sub_array(m_stack.back());
    }

    /* This performs an intersection of the given rect
     * against the top of the clip equationn stack and
     * returns true if there is an intersection.
     *
     * NOTE: do NOT use this to avoid calling push_intersect()
     *       as it still performs a clipping computation.
     */
    bool
    intersects(const BoundingBox<float> &rect);

    /* Pop the top of the stack */
    void
    pop(void)
    {
      ASTRALassert(!m_stack.empty());
      m_backing.resize(m_stack.back().m_begin);
      m_stack.pop_back();
    }

  private:
    std::vector<range_type<unsigned int>> m_stack;
    std::vector<vec3> m_backing;
    vecN<std::vector<vec2>, 2> m_clip_workroom;
  };

  c_array<const uvec2>
  compute_empty_tiles_implement(Storage &storage, const CullGeometryGroup &geometry,
                                const DrawCommandList *cmds,
                                bool use_pixel_rect_tile_culling,
                                BoundingBox<int> *out_image_bounding_box);

  std::vector<uvec2> m_empty_rects;
  ClipEqStack m_eq_stack;
  Base *m_root;
  Pool m_pool;
};

#endif

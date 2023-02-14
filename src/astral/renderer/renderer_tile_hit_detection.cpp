/*!
 * \file renderer_tile_hit_detection.cpp
 * \brief file renderer_tile_hit_detection.cpp
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

#include <astral/util/clip_util.hpp>
#include "renderer_tile_hit_detection.hpp"
#include "renderer_cull_geometry.hpp"
#include "renderer_draw_command.hpp"
#include "renderer_storage.hpp"

class astral::Renderer::Implement::TileHitDetection::RectAdder:public DrawCommandList::RectWalker
{
public:
  explicit
  RectAdder(Base *p):
    m_p(p)
  {}

  virtual
  void
  operator()(enum DrawCommandList::command_list_t C, const TransformedBoundingBox &B) override final
  {
    if (C == DrawCommandList::opaque_command_list || C == DrawCommandList::typical_command_list)
      {
        m_p->add_hits(B);
      }
  }

  virtual
  bool
  early_out(void) override final
  {
    return !m_p->has_unhit_tiles();
  }

private:
  Base *m_p;
};

////////////////////////////////////////////////
// astral::Renderer::Implement::TileHitDetection::ClipEqStack methods
bool
astral::Renderer::Implement::TileHitDetection::ClipEqStack::
intersects(const BoundingBox<float> &rect)
{
  /* TODO: make a faster test than clipping against the given rect */
  c_array<const vec2> clipped_rect;
  vecN<vec2, 4> pts;

  pts[0] = vec2(rect.as_rect().min_x(), rect.as_rect().min_y());
  pts[1] = vec2(rect.as_rect().min_x(), rect.as_rect().max_y());
  pts[2] = vec2(rect.as_rect().max_x(), rect.as_rect().max_y());
  pts[3] = vec2(rect.as_rect().max_x(), rect.as_rect().min_y());

  clip_against_planes(current_clipping(), pts, &clipped_rect, m_clip_workroom);

  return !clipped_rect.empty();
}

bool
astral::Renderer::Implement::TileHitDetection::ClipEqStack::
push_intersect(const BoundingBox<float> &rect)
{
  bool return_value;
  vecN<vec2, 4> pts;
  c_array<const vec2> clipped_rect;

  pts[0] = vec2(rect.as_rect().min_x(), rect.as_rect().min_y());
  pts[1] = vec2(rect.as_rect().min_x(), rect.as_rect().max_y());
  pts[2] = vec2(rect.as_rect().max_x(), rect.as_rect().max_y());
  pts[3] = vec2(rect.as_rect().max_x(), rect.as_rect().min_y());

  return_value = clip_against_planes(current_clipping(), pts, &clipped_rect, m_clip_workroom);

  if (!clipped_rect.empty())
    {
      /* from the points in clipped_rect, derive new clip-equations */
      range_type<unsigned int> R;
      vec2 center(0.0f, 0.0f);

      /* compute the center */
      for (const vec2 &p : clipped_rect)
        {
          center += p;
        }
      center /= static_cast<float>(clipped_rect.size());

      /* now add the clipping equations made from the sides of the polygon */
      R.m_begin = m_backing.size();
      for (unsigned int i = 0, endi = clipped_rect.size(); i < endi; ++i)
        {
          unsigned int next_i(i + 1u);
          vec2 v, n;
          vec3 cl;

          next_i = (next_i == endi) ? 0u : next_i;

          /* compute the normal vector from the side */
          v = clipped_rect[next_i] - clipped_rect[i];
          n = vec2(-v.y(), v.x());

          /* make sure it points to the center of the polygon */
          if (dot(center - clipped_rect[i], n) < 0.0f)
            {
              n = -n;
            }

          /* normalize the vector to avoid getting terribly large values
           * when the sides of the polygon are long.
           */
          n.normalize();

          /* save the equations */
          m_backing.push_back(vec3(n.x(), n.y(), -dot(n, clipped_rect[i])));
        }

      R.m_end = m_backing.size();

      m_stack.push_back(R);
    }
  else
    {
      /* empty intersection, push back an empty range */
      m_stack.push_back(range_type<unsigned int>(m_backing.size(), m_backing.size()));
    }

  return return_value;
}

/////////////////////////////////////////////////
// astral::Renderer::Implement::TileHitDetection::Base methods
void
astral::Renderer::Implement::TileHitDetection::Base::
add_backed_region(Storage &storage, ClipEqStack &eq_stack,
                  bool use_pixel_rect_tile_culling,
                  const CullGeometry &geometry)
{
  if (use_pixel_rect_tile_culling || geometry.is_screen_aligned_rect())
    {
      add_backed_region_rect(geometry.pixel_rect());
    }
  else
    {
      CullGeometry::Backing *backing(&storage.cull_geometry_backing());

      eq_stack.init(geometry.equations(backing));
      if (!eq_stack.current_clipping().empty())
        {
          add_backed_region_clip_eq(geometry.pixel_rect(), eq_stack);
        }
    }
}

//////////////////////////////////////////////////////////////
// astral::Renderer::Implement::TileHitDetection::Leaf methods
astral::Renderer::Implement::TileHitDetection::Leaf::
Leaf(const ScaleTranslate &pixel_transformation_image,
     uvec2 tile_id, const BoundingBox<float> &parent_bb):
  m_tile_id(tile_id)
{
  vec2 min_pt, max_pt;

  min_pt.x() = ImageAtlas::tile_start(tile_id.x(), 0);
  min_pt.y() = ImageAtlas::tile_start(tile_id.y(), 0);
  min_pt = pixel_transformation_image.apply_to_point(min_pt);

  max_pt.x() = ImageAtlas::tile_end(tile_id.x(), 0);
  max_pt.y() = ImageAtlas::tile_end(tile_id.y(), 0);
  max_pt = pixel_transformation_image.apply_to_point(max_pt);

  m_padded_box = BoundingBox<float>(min_pt, max_pt);
  m_padded_box.intersect_against(parent_bb);
}

void
astral::Renderer::Implement::TileHitDetection::Leaf::
mark_all_tiles_hit(void)
{
  m_has_unhit_tiles = false;
}

void
astral::Renderer::Implement::TileHitDetection::Leaf::
mark_is_completely_backed(void)
{
  ASTRALassert(m_backing_status == is_completely_backed || m_backing_status == is_completely_unbacked);
  m_backing_status = is_completely_backed;
}

void
astral::Renderer::Implement::TileHitDetection::Leaf::
add_hits(const TransformedBoundingBox &pixel_rect)
{
  if (!m_has_unhit_tiles || m_backing_status != is_completely_backed)
    {
      return;
    }

  m_has_unhit_tiles = !pixel_rect.intersects(m_padded_box);
}

void
astral::Renderer::Implement::TileHitDetection::Leaf::
add_backed_region_rect(const BoundingBox<float> &pixel_rect)
{
  ASTRALassert(m_backing_status == is_completely_backed || m_backing_status == is_completely_unbacked);

  if (m_backing_status != is_completely_backed && pixel_rect.intersects(m_padded_box))
    {
      m_backing_status = is_completely_backed;
    }
}

void
astral::Renderer::Implement::TileHitDetection::Leaf::
add_backed_region_clip_eq(const BoundingBox<float> &pixel_rect, ClipEqStack &eq_stack)
{
  ASTRALassert(!eq_stack.current_clipping().empty());

  if (m_backing_status != is_completely_backed && pixel_rect.intersects(m_padded_box))
    {
      if (eq_stack.intersects(m_padded_box))
        {
          m_backing_status = is_completely_backed;
        }
    }
}

//////////////////////////////////////////////////////////////
// astral::Renderer::Implement::TileHitDetection::Node methods
astral::Renderer::Implement::TileHitDetection::Node::
Node(Pool &pool, const ScaleTranslate &pixel_transformation_image,
     vecN<range_type<unsigned int>, 2> tile_range,
     const BoundingBox<float> &parent_bb)
{
  /* split the largest side */
  int split_coordinate;
  vec2 min_pt, max_pt;
  vecN<range_type<unsigned int>, 2> r0, r1;

  ASTRALassert(tile_range.x().difference() >= 1u);
  ASTRALassert(tile_range.y().difference() >= 1u);

  min_pt.x() = ImageAtlas::tile_start(tile_range.x().m_begin, 0);
  min_pt.y() = ImageAtlas::tile_start(tile_range.y().m_begin, 0);
  min_pt = pixel_transformation_image.apply_to_point(min_pt);

  max_pt.x() = ImageAtlas::tile_end(tile_range.x().m_end - 1u, 0);
  max_pt.y() = ImageAtlas::tile_end(tile_range.y().m_end - 1u, 0);
  max_pt = pixel_transformation_image.apply_to_point(max_pt);

  m_padded_box = BoundingBox<float>(min_pt, max_pt);
  m_padded_box.intersect_against(parent_bb);

  split_coordinate = (tile_range.x().difference() >= tile_range.y().difference()) ? 0 : 1;
  ASTRALassert(tile_range[split_coordinate].difference() >= 2u);

  r0[1 - split_coordinate] = r1[1 - split_coordinate] = tile_range[1 - split_coordinate];

  r0[split_coordinate].m_begin = tile_range[split_coordinate].m_begin;
  r1[split_coordinate].m_end = tile_range[split_coordinate].m_end;
  r0[split_coordinate].m_end = r1[split_coordinate].m_begin = (r0[split_coordinate].m_begin + r1[split_coordinate].m_end) / 2u;

  m_children[0] = pool.create(pixel_transformation_image, r0, m_padded_box);
  m_children[1] = pool.create(pixel_transformation_image, r1, m_padded_box);
}

void
astral::Renderer::Implement::TileHitDetection::Node::
mark_all_tiles_hit(void)
{
  if (m_has_unhit_tiles)
    {
      m_has_unhit_tiles = false;
      m_children[0]->mark_all_tiles_hit();
      m_children[1]->mark_all_tiles_hit();
    }
}

void
astral::Renderer::Implement::TileHitDetection::Node::
mark_is_completely_backed(void)
{
  if (m_backing_status != is_completely_backed)
    {
      m_backing_status = is_completely_backed;
      m_children[0]->mark_is_completely_backed();
      m_children[1]->mark_is_completely_backed();
    }
}

void
astral::Renderer::Implement::TileHitDetection::Node::
add_hits(const TransformedBoundingBox &pixel_rect)
{
  if (!m_has_unhit_tiles || m_backing_status == is_completely_unbacked)
    {
      return;
    }

  if (pixel_rect.contains(m_padded_box))
    {
      mark_all_tiles_hit();
    }
  else if (pixel_rect.intersects(m_padded_box))
    {
      m_children[0]->add_hits(pixel_rect);
      m_children[1]->add_hits(pixel_rect);
      if (!m_children[0]->has_unhit_tiles() && !m_children[1]->has_unhit_tiles())
        {
          m_has_unhit_tiles = false;
        }
    }
}

void
astral::Renderer::Implement::TileHitDetection::Node::
add_backed_region_rect(const BoundingBox<float> &pixel_rect)
{
  if (m_backing_status == is_completely_backed)
    {
      return;
    }

  if (pixel_rect.contains(m_padded_box))
    {
      mark_is_completely_backed();
      return;
    }

  if (pixel_rect.intersects(m_padded_box))
    {
      m_children[0]->add_backed_region_rect(pixel_rect);
      m_children[1]->add_backed_region_rect(pixel_rect);
      if (m_children[0]->backing_status() == m_children[1]->backing_status())
        {
          m_backing_status = m_children[0]->backing_status();
        }
      else
        {
          m_backing_status = mixed_backing;
        }
    }
}

void
astral::Renderer::Implement::TileHitDetection::Node::
add_backed_region_clip_eq(const BoundingBox<float> &pixel_rect, ClipEqStack &eq_stack)
{
  ASTRALassert(!eq_stack.current_clipping().empty());

  if (m_backing_status == is_completely_backed)
    {
      return;
    }

  if (!pixel_rect.intersects(m_padded_box))
    {
      return;
    }

  bool unclipped;

  unclipped = eq_stack.push_intersect(m_padded_box);
  if (!eq_stack.current_clipping().empty())
    {
      if (unclipped)
        {
          /* the padding rect was completely unclipped, thus
           * the entirity of the contents are covered.
           */
          mark_is_completely_backed();
        }
      else
        {
          m_children[0]->add_backed_region_clip_eq(pixel_rect, eq_stack);
          m_children[1]->add_backed_region_clip_eq(pixel_rect, eq_stack);
          if (m_children[0]->backing_status() == m_children[1]->backing_status())
            {
              m_backing_status = m_children[0]->backing_status();
            }
          else
            {
              m_backing_status = mixed_backing;
            }
        }
    }
  eq_stack.pop();
}

///////////////////////////////////////////////////////
// astral::Renderer::Implement::TileHitDetection::Pool methods
astral::Renderer::Implement::TileHitDetection::Base*
astral::Renderer::Implement::TileHitDetection::Pool::
create(const ScaleTranslate &pixel_transformation_image,
       vecN<range_type<unsigned int>, 2> tile_range,
       const BoundingBox<float> &parent_bb)
{
  Base *return_value;

  if (tile_range.x().difference() >= 2 || tile_range.y().difference() >= 2)
    {
      return_value = m_node_pool.create(*this, pixel_transformation_image, tile_range, parent_bb);
    }
  else
    {
      uvec2 tile_id;

      tile_id.x() = tile_range.x().m_begin;
      tile_id.y() = tile_range.y().m_begin;
      return_value = m_leaf_pool.create(pixel_transformation_image, tile_id, parent_bb);
    }

  return return_value;
}

//////////////////////////////////////////////
// astral::Renderer::Implement::TileHitDetection methods
astral::c_array<const astral::uvec2>
astral::Renderer::Implement::TileHitDetection::
compute_empty_tiles_implement(Storage &storage,
                              const CullGeometryGroup &geometry,
                              const DrawCommandList *cmds,
                              bool use_pixel_rect_tile_culling,
                              BoundingBox<int> *out_image_bounding_box)
{
  vecN<range_type<unsigned int>, 2> tile_range;
  ivec2 cnt;

  m_pool.clear();
  m_empty_rects.clear();

  cnt = ImageAtlas::tile_count(geometry.bounding_geometry().image_size());

  tile_range.x().m_begin = 0;
  tile_range.y().m_begin = 0;

  tile_range.x().m_end = static_cast<unsigned int>(cnt.x());
  tile_range.y().m_end = static_cast<unsigned int>(cnt.y());

  m_root = m_pool.create(geometry.bounding_geometry().image_transformation_pixel().inverse(),
                         tile_range, geometry.bounding_geometry().pixel_rect());

  /* Maybe: Instead "rasterize" the convex regions of each
   *        CullGeometry directly to the tiles and then do
   *        the walk up the heierarchy.
   */
  for (const CullGeometry &C : geometry.sub_clip_geometries(storage))
    {
      m_root->add_backed_region(storage, m_eq_stack, use_pixel_rect_tile_culling, C);
    }

  if (cmds)
    {
      RectAdder walker(m_root);
      cmds->walk_rects_of_draws(walker);
    }

  out_image_bounding_box->clear();

  for (const Leaf *leaf : m_pool.created_leaves())
    {
      enum Base::backing_status_t status;

      status = leaf->backing_status();
      if ((cmds && leaf->has_unhit_tiles()) || status == Base::is_completely_unbacked)
        {
          m_empty_rects.push_back(leaf->tile_id());
        }
      else
        {
          ivec2 min_pt, max_pt;

          min_pt.x() = ImageAtlas::tile_start(leaf->tile_id().x(), 0);
          min_pt.y() = ImageAtlas::tile_start(leaf->tile_id().y(), 0);
          max_pt.x() = ImageAtlas::tile_end(leaf->tile_id().x(), 0);
          max_pt.y() = ImageAtlas::tile_end(leaf->tile_id().y(), 0);

          out_image_bounding_box->union_box(BoundingBox<int>(min_pt, max_pt));
        }
    }

  out_image_bounding_box->intersect_against(BoundingBox<int>(ivec2(0, 0), geometry.bounding_geometry().image_size()));

  return make_c_array(m_empty_rects);
}

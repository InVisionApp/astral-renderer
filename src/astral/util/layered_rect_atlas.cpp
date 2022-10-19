/*!
 * \file rect_atlas.cpp
 * \brief file rect_atlas.cpp
 *
 * Adapted from: WRATHAtlasBase.cpp and WRATHAtlas.cpp of WRATH:
 *
 * Copyright 2013 by Nomovok Ltd.
 * Contact: info@nomovok.com
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#include <limits>
#include <vector>
#include <utility>
#include <astral/util/memory_pool.hpp>
#include <astral/util/layered_rect_atlas.hpp>

class astral::LayeredRectAtlas::Implement:public astral::LayeredRectAtlas
{
public:
  class RectAtlas;
  class NodeBase;
  class NodeWithoutChildren;
  class NodeWithChildren;
  class MemoryPool;
  class Bucket;
  class BucketEntry;

  Implement(void);
  ~Implement();

  Bucket*
  get_bucket(unsigned int I, unsigned int J)
  {
    unsigned int L;

    ASTRALassert(I < 1u * m_log2_dimensions.x());
    ASTRALassert(J < 1u * m_log2_dimensions.y());

    L = m_log2_dimensions.x() * J + I;
    ASTRALassert(L < m_bucket_backings.size());

    return m_bucket_backings[L];
  }

  void
  ready_buckets(bool clear_buckets);

  const BucketEntry&
  add_bucket_entry(RectAtlas *atlas, int which, NodeWithoutChildren *p, ivec2 size);

  void
  remove_bucket_entry(BucketEntry &entry);

  const BucketEntry*
  choose_bucket_entry(const ivec2 &dimensions);

  void
  clear_implement(ivec2 dimensions, unsigned int num_layers);

  void
  add_user_region_implement(ivec2 location, ivec2 dimensions, unsigned int layer);

  void
  number_layers_implement(unsigned int N);

  Entry
  allocate_rectangle_implement(const ivec2 &dimensions);

  ivec2 m_dimensions;
  ivec2 m_log2_dimensions;
  MemoryPool *m_pool;

  /* RectAtlas objects for the layers */
  unsigned int m_num_layers;
  std::vector<reference_counted_ptr<RectAtlas>> m_layer_atlases;

  /* RectAtlas objects for user defined regions */
  unsigned int m_num_user_regions;
  std::vector<reference_counted_ptr<RectAtlas>> m_user_region;

  std::vector<Bucket*> m_bucket_backings;
  std::vector<std::pair<RectAtlas*, NodeBase*>> m_tmp;
};

class astral::LayeredRectAtlas::Entry::Rectangle:public astral::noncopyable
{
public:
  Rectangle(const ivec2 &psize, Implement::RectAtlas *p):
    m_minX_minY(0, 0),
    m_size(psize),
    m_parent(nullptr),
    m_rect_atlas(p),
    m_reclaimed(false)
  {}

  const astral::ivec2&
  minX_minY(void) const
  {
    ASTRALassert(!reclaimed());
    return m_minX_minY;
  }

  int
  area(void) const
  {
    ASTRALassert(!reclaimed());
    return m_size.x() * m_size.y();
  }

  const ivec2&
  size(void) const
  {
    ASTRALassert(!reclaimed());
    return m_size;
  }

  void
  move(const ivec2 &moveby)
  {
    ASTRALassert(!reclaimed());
    m_minX_minY += moveby;
  }

  Implement::NodeWithoutChildren*
  parent(void)
  {
    ASTRALassert(!reclaimed());
    return m_parent;
  }

  void
  parent(Implement::NodeWithoutChildren *p)
  {
    ASTRALassert(!reclaimed());
    m_parent = p;
  }

  Implement::RectAtlas*
  rect_atlas(void)
  {
    ASTRALassert(!reclaimed());
    return m_rect_atlas;
  }

  void
  reclaim(Implement::MemoryPool &pool);

  bool
  reclaimed(void) const
  {
    return m_reclaimed;
  }

private:
  ivec2 m_minX_minY, m_size;
  Implement::NodeWithoutChildren *m_parent;
  Implement::RectAtlas *m_rect_atlas;
  bool m_reclaimed;
};

class astral::LayeredRectAtlas::Implement::BucketEntry
{
public:
  BucketEntry(void):
    m_rect_atlas(nullptr),
    m_bucket(nullptr),
    m_idx(~0u),
    m_node(nullptr),
    m_idx_in_node(~0u),
    m_size(-1, -1)
  {}

  bool
  valid(void) const
  {
    return m_bucket != nullptr;
  }

private:
  friend class LayeredRectAtlas;
  friend class Bucket;

  RectAtlas *m_rect_atlas;

  Bucket *m_bucket; //which Bucket
  unsigned int m_idx; //index into Bucket::m_elements

  NodeWithoutChildren *m_node; // which node
  int m_idx_in_node; //which element of the node.
  ivec2 m_size;
};

class astral::LayeredRectAtlas::Implement::Bucket
{
public:
  void
  clear(void)
  {
    m_elements.clear();
  }

  const BucketEntry*
  choose_element(ivec2 dimensions, int *out_extra_room);

  const BucketEntry*
  choose_element(ivec2 dimensions);

private:
  friend class LayeredRectAtlas;
  std::vector<BucketEntry> m_elements;
};

class astral::LayeredRectAtlas::Implement::NodeBase
{
public:
  NodeBase(NodeWithChildren *parent, unsigned int child_id,
           const ivec2 &bl, const ivec2 &sz);

  virtual
  ~NodeBase()
  {}

  const ivec2&
  size(void) const
  {
    ASTRALassert(!reclaimed());
    return m_size;
  }

  int
  area(void) const
  {
    ASTRALassert(!reclaimed());
    return m_size.x() * m_size.y();
  }

  const ivec2&
  minX_minY(void) const
  {
    ASTRALassert(!reclaimed());
    return m_minX_minY;
  }

  NodeWithChildren*
  parent(void)
  {
    ASTRALassert(!reclaimed());
    return m_parent;
  }

  unsigned int
  child_id(void)
  {
    ASTRALassert(!reclaimed());
    return m_child_id;
  }

  virtual
  bool
  is_empty_node_without_children(void) const = 0;

  void
  reclaim(MemoryPool &pool)
  {
    ASTRALassert(!reclaimed());

    m_reclaimed = true;
    reclaim_implement(pool);
  }

  bool
  reclaimed(void) const
  {
    return m_reclaimed;
  }

  bool
  is_hard_empty(void) const
  {
    return m_is_hard_empty;
  }

  bool
  is_logically_empty(void) const
  {
    return m_is_hard_empty || is_empty_node_without_children();
  }

  static
  void
  free_rectangles(Implement &atlas, c_array<const Entry> entries,
                  std::vector<std::pair<RectAtlas*, NodeBase*>> &tmp);

  /* Clear the buckets of this node and all the descendants */
  virtual
  void
  clear_buckets(Implement&) = 0;

private:
  virtual
  void
  reclaim_implement(MemoryPool &pool) = 0;

  static
  void
  free_rectangles_build_list(c_array<const Entry> entries,
                             std::vector<std::pair<RectAtlas*, NodeBase*>> &tmp);

  static
  void
  free_rectangles_handle_hard_empty(std::vector<std::pair<RectAtlas*, NodeBase*>> &tmp);

  static
  void
  free_rectangles_reclaim(Implement &atlas,
                          std::vector<std::pair<RectAtlas*, NodeBase*>> &tmp);

  NodeWithChildren *m_parent;
  unsigned int m_child_id;
  ivec2 m_minX_minY, m_size;
  bool m_reclaimed, m_is_hard_empty;
};

class astral::LayeredRectAtlas::Implement::NodeWithoutChildren:public astral::LayeredRectAtlas::Implement::NodeBase
{
public:
  NodeWithoutChildren(RectAtlas &atlas,
                      NodeWithChildren *parent,
                      unsigned int child_id,
                      const ivec2 &bl, const ivec2 &sz,
                      Entry::Rectangle *rect);
  virtual
  ~NodeWithoutChildren()
  {
    ASTRALassert(!m_rectangle);
  }

  Entry::Rectangle*
  data(void)
  {
    return m_rectangle;
  }

  NodeBase*
  add(RectAtlas&, Entry::Rectangle*);

  virtual
  bool
  is_empty_node_without_children(void) const override final
  {
    return m_rectangle == nullptr;
  }

  /*
   * Called by Bucket::remove_entry() when where an entry is located
   * within a bucket changes.
   */
  void
  update_entry(const BucketEntry&, int);

  virtual
  void
  clear_buckets(Implement&) override final;

private:
  virtual
  void
  reclaim_implement(MemoryPool &pool) override final;

  Entry::Rectangle *m_rectangle;
  vecN<BucketEntry, 2> m_bucket_entry;
};

class astral::LayeredRectAtlas::Implement::NodeWithChildren:public astral::LayeredRectAtlas::Implement::NodeBase
{
public:
  NodeWithChildren(RectAtlas &atlas,
                   NodeWithChildren *parent,
                   unsigned int child_id,
                   MemoryPool &pool,
                   NodeWithoutChildren *src,
                   Entry::Rectangle *rect_to_add);
  virtual
  ~NodeWithChildren()
  {
    ASTRALassert(!m_children[0]);
    ASTRALassert(!m_children[1]);
    ASTRALassert(!m_children[2]);
  }

  virtual
  bool
  is_empty_node_without_children(void) const override final
  {
    return false;
  }

  NodeBase*
  child(unsigned int id)
  {
    return m_children[id];
  }

  void
  add_from_child(RectAtlas &atlas, NodeWithoutChildren *child, Entry::Rectangle *rect);

  void
  handle_hard_empty_children(RectAtlas &a);

  virtual
  void
  clear_buckets(Implement &a) override final
  {
    ASTRALassert(is_hard_empty());
    m_children[0]->clear_buckets(a);
    m_children[1]->clear_buckets(a);
    m_children[2]->clear_buckets(a);
  }

  bool
  each_child_is_logically_empty(void) const
  {
    return m_children[0]->is_logically_empty()
      && m_children[1]->is_logically_empty()
      && m_children[2]->is_logically_empty();
  }

private:
  virtual
  void
  reclaim_implement(MemoryPool &pool) override final;

  vecN<NodeBase*, 3> m_children;
};

class astral::LayeredRectAtlas::Implement::MemoryPool:astral::noncopyable
{
public:
  MemoryPool(void)
  {
  }

  Entry::Rectangle*
  create_rectangle(const ivec2 &psize, RectAtlas *p)
  {
    return m_rect_allocator.create(psize, p);
  }

  NodeWithoutChildren*
  create_node_without_children(RectAtlas &atlas,
                               NodeWithChildren *parent,
                               unsigned int child_id_in_parent,
                               const ivec2 &bl, const ivec2 &sz,
                               Entry::Rectangle *rect = nullptr)
  {
    return m_node_without_children_allocator.create(atlas, parent, child_id_in_parent, bl, sz, rect);
  }

  NodeWithChildren*
  create_node_with_children(RectAtlas &atlas,
                            NodeWithChildren *p,
                            unsigned int child_id,
                            NodeWithoutChildren *src,
                            Entry::Rectangle *rect_to_add)
  {
    return m_node_with_children_allocator.create(atlas, p, child_id, *this, src, rect_to_add);
  }

  void
  reclaim(Entry::Rectangle *p)
  {
    m_rect_allocator.reclaim(p);
  }

  void
  reclaim(NodeWithoutChildren *p)
  {
    m_node_without_children_allocator.reclaim(p);
  }

  void
  reclaim(NodeWithChildren *p)
  {
    m_node_with_children_allocator.reclaim(p);
  }

  void
  clear(void)
  {
    m_rect_allocator.clear();
    m_node_without_children_allocator.clear();
    m_node_with_children_allocator.clear();
  }

private:
  astral::MemoryPool<Entry::Rectangle, 512> m_rect_allocator;
  astral::MemoryPool<NodeWithoutChildren, 512> m_node_without_children_allocator;
  astral::MemoryPool<NodeWithChildren, 512> m_node_with_children_allocator;
};

class astral::LayeredRectAtlas::Implement::RectAtlas:public reference_counted<astral::LayeredRectAtlas::Implement::RectAtlas>::non_concurrent
{
public:
  RectAtlas(Implement *p);

  virtual
  ~RectAtlas();

  void
  init(ivec2 bottom_left, ivec2 new_dimensions, unsigned int layer);

  void
  init(ivec2 new_dimensions, unsigned int layer);

  Entry::Rectangle*
  allocate_rectangle_from_node(const ivec2 &dimensions,
                               NodeWithoutChildren *node);

  MemoryPool&
  pool(void)
  {
    return *m_layered_atlas->m_pool;
  }

  Implement&
  layered_atlas(void)
  {
    return *m_layered_atlas;
  }

  unsigned int
  layer(void)
  {
    return m_layer;
  }

  void
  handle_hard_empty_root(void);

private:
  NodeBase *m_root;
  Implement *m_layered_atlas;
  unsigned int m_layer;
};

////////////////////////////////
// astral::LayeredRectAtlas::Entry methods
astral::ivec2
astral::LayeredRectAtlas::Entry::
location(void) const
{
  ASTRALassert(m_rectangle);
  ASTRALassert(m_rectangle->rect_atlas());
  return m_rectangle->minX_minY();
}

astral::ivec2
astral::LayeredRectAtlas::Entry::
dimensions(void) const
{
  ASTRALassert(m_rectangle);
  ASTRALassert(m_rectangle->rect_atlas());
  return m_rectangle->size();
}

unsigned int
astral::LayeredRectAtlas::Entry::
layer(void) const
{
  ASTRALassert(m_rectangle);
  ASTRALassert(m_rectangle->rect_atlas());
  return m_rectangle->rect_atlas()->layer();
}


/////////////////////////////////////
// astral::LayeredRectAtlas::Entry::Rectangle methods
void
astral::LayeredRectAtlas::Entry::Rectangle::
reclaim(Implement::MemoryPool &pool)
{
  ASTRALassert(!reclaimed());

  pool.reclaim(this);
  m_reclaimed = true;
}

////////////////////////////////
// astral::LayeredRectAtlas::Implement::Bucket methods
const astral::LayeredRectAtlas::Implement::BucketEntry*
astral::LayeredRectAtlas::Implement::Bucket::
choose_element(ivec2 dimensions, int *out_extra_room)
{
  const BucketEntry *p(nullptr);
  int &min_delta(*out_extra_room);

  min_delta = std::numeric_limits<int>::max();

  /* Idea: maybe should we also make a list of possible
   * choices and sort them by lexigraphically in
   * increasing order (A, B) where A is the smaller
   * of the differences between the dimensions and
   * the rect canidate and then take the first element
   * on the list. Should we also kill the early out too?
   */
  for (const auto &entry : m_elements)
    {
      ASTRALassert(entry.m_bucket == this);
      if (entry.m_size.x() >= dimensions.x()
          && entry.m_size.y() >= dimensions.y())
        {
          ivec2 delta;

          delta = entry.m_size - dimensions;
          if (!p || delta.x() < min_delta || delta.y() < min_delta)
            {
              p = &entry;
              min_delta = t_min(delta.x(), delta.y());

              if (min_delta == 0)
                {
                  return p;
                }
            }
        }
    }

  return p;
}

const astral::LayeredRectAtlas::Implement::BucketEntry*
astral::LayeredRectAtlas::Implement::Bucket::
choose_element(ivec2 dimensions)
{
  for (const auto &entry : m_elements)
    {
      ASTRALassert(entry.m_bucket == this);
      if (entry.m_size.x() >= dimensions.x()
          && entry.m_size.y() >= dimensions.y())
        {
          return &entry;
        }
    }

  return nullptr;
}

////////////////////////////////
// astral::LayeredRectAtlas::Implement::NodeBase methods
astral::LayeredRectAtlas::Implement::NodeBase::
NodeBase(NodeWithChildren *parent, unsigned int child_id,
         const ivec2 &bl, const ivec2 &sz):
  m_parent(parent),
  m_child_id(child_id),
  m_minX_minY(bl),
  m_size(sz),
  m_reclaimed(false),
  m_is_hard_empty(false)
{
  ASTRALassert(!m_parent || !m_parent->reclaimed());
}

void
astral::LayeredRectAtlas::Implement::NodeBase::
free_rectangles(Implement &atlas, c_array<const Entry> entries,
                std::vector<std::pair<RectAtlas*, NodeBase*>> &tmp)
{
  tmp.clear();

  free_rectangles_build_list(entries, tmp);
  free_rectangles_handle_hard_empty(tmp);
  free_rectangles_reclaim(atlas, tmp);
}

void
astral::LayeredRectAtlas::Implement::NodeBase::
free_rectangles_build_list(c_array<const Entry> entries,
                           std::vector<std::pair<RectAtlas*, NodeBase*>> &tmp)
{
  for (const Entry &e : entries)
    {
      RectAtlas *rect_atlas(e.m_rectangle->rect_atlas());
      NodeBase *N(e.m_rectangle->parent());

      ASTRALassert(!N->m_is_hard_empty);

      N->m_is_hard_empty = true;
      tmp.push_back(std::make_pair(rect_atlas, N));

      for (NodeWithChildren *p = N->parent();
           p && p->each_child_is_logically_empty();
           p = p->parent())
        {
          ASTRALassert(!p->m_is_hard_empty);
          p->m_is_hard_empty = true;
          tmp.push_back(std::make_pair(rect_atlas, p));
        }
    }
}

void
astral::LayeredRectAtlas::Implement::NodeBase::
free_rectangles_handle_hard_empty(std::vector<std::pair<RectAtlas*, NodeBase*>> &tmp)
{
  for (const auto &E : tmp)
    {
      RectAtlas *rect_atlas(E.first);
      NodeBase *N(E.second);
      NodeWithChildren *p(N->parent());

      if (p && !p->m_is_hard_empty)
        {
          p->handle_hard_empty_children(*rect_atlas);
        }
      else if (!p)
        {
          rect_atlas->handle_hard_empty_root();
        }
    }
}

void
astral::LayeredRectAtlas::Implement::NodeBase::
free_rectangles_reclaim(Implement &atlas,
                        std::vector<std::pair<RectAtlas*, NodeBase*>> &tmp)
{
  for (const auto &p : tmp)
    {
      p.second->reclaim(*atlas.m_pool);
    }
}

//////////////////////////////////////
// astral::LayeredRectAtlas::Implement::NodeWithoutChildren methods
astral::LayeredRectAtlas::Implement::NodeWithoutChildren::
NodeWithoutChildren(RectAtlas &atlas,
                    NodeWithChildren *parent,
                    unsigned int child_id,
                    const ivec2 &bl, const ivec2 &sz,
                    Entry::Rectangle *rect):
  NodeBase(parent, child_id, bl, sz),
  m_rectangle(rect)
{
  if (m_rectangle)
    {
      m_rectangle->parent(this);
      ASTRALassert(m_rectangle->size() == sz);
    }
  else if (sz.x() > 0 && sz.y() > 0)
    {
      m_bucket_entry[0] = atlas.layered_atlas().add_bucket_entry(&atlas, 0, this, sz);
    }
}

void
astral::LayeredRectAtlas::Implement::NodeWithoutChildren::
update_entry(const BucketEntry &entry, int which)
{
  m_bucket_entry[which] = entry;
}

void
astral::LayeredRectAtlas::Implement::NodeWithoutChildren::
clear_buckets(Implement &layered_atlas)
{
  layered_atlas.remove_bucket_entry(m_bucket_entry[0]);
  layered_atlas.remove_bucket_entry(m_bucket_entry[1]);
}

void
astral::LayeredRectAtlas::Implement::NodeWithoutChildren::
reclaim_implement(MemoryPool &pool)
{
  if (m_rectangle)
    {
      pool.reclaim(m_rectangle);
      m_rectangle = nullptr;
    }
  pool.reclaim(this);
}

astral::LayeredRectAtlas::Implement::NodeBase*
astral::LayeredRectAtlas::Implement::NodeWithoutChildren::
add(RectAtlas &atlas, Entry::Rectangle *im)
{
  ASTRALassert(!reclaimed());
  ASTRALassert(im->size().x() <= size().x());
  ASTRALassert(im->size().y() <= size().y());

  if (m_rectangle == nullptr)
    {
      //do not have a rect so we take it (and move it).
      m_rectangle = im;
      m_rectangle->move(minX_minY());
      m_rectangle->parent(this);

      // add the bucket entry values for the two different
      // ways to split the remaining space.
      ivec2 sz0, sz1;

      ASTRALassert(m_bucket_entry[0].valid());
      ASTRALassert(!m_bucket_entry[1].valid());
      atlas.layered_atlas().remove_bucket_entry(m_bucket_entry[0]);

      sz0.x() = size().x();
      sz0.y() = size().y() - m_rectangle->size().y();
      if (sz0.y() > 0)
        {
          m_bucket_entry[0] = atlas.layered_atlas().add_bucket_entry(&atlas, 0, this, sz0);
        }

      sz1.x() = size().x() - m_rectangle->size().x();
      sz1.y() = size().y();
      if (sz1.x() > 0)
        {
          m_bucket_entry[1] = atlas.layered_atlas().add_bucket_entry(&atlas, 1, this, sz1);
        }

      return this;
    }

  ASTRALassert((size().x() - m_rectangle->size().x() >= im->size().x())
               || (size().y() - m_rectangle->size().y() >= im->size().y()));

  NodeBase *new_node;
  new_node = atlas.pool().create_node_with_children(atlas, parent(), child_id(), this, im);

  /* new_node will hold this->m_rectange,
   * thus we set m_rectangle to nullptr
   * before recycling outselves
   */
  m_rectangle = nullptr;

  /* Remove all the BucketEntry values */
  atlas.layered_atlas().remove_bucket_entry(m_bucket_entry[0]);
  atlas.layered_atlas().remove_bucket_entry(m_bucket_entry[1]);

  /* reclaim ourselves because the caller won't */
  reclaim(atlas.pool());

  return new_node;
}

/* If the width or height is negative, that indicates
 * that the rect cannot fit, so the area is negative.
 */
inline
int
compute_area(int w, int h)
{
  return (w < 0 || h < 0) ? -1 : w * h;
}

/* If one of the areas is negative, then that indicates
 * that the rect cannot fit in that position.
 */
inline
int
compute_max_area(int a0, int a1, int a2)
{
  return (a0 >= 0 && a1 >= 0 && a2 >= 0) ? astral::t_max(a0, astral::t_max(a1, a2)) : -1;
}

////////////////////////////////////
// NodeWithChildren methods
astral::LayeredRectAtlas::Implement::NodeWithChildren::
NodeWithChildren(RectAtlas &atlas,
                 NodeWithChildren *parent,
                 unsigned int child_id,
                 MemoryPool &pool,
                 NodeWithoutChildren *src,
                 Entry::Rectangle *rect_to_add):
  NodeBase(parent, child_id, src->minX_minY(), src->size()),
  m_children(nullptr, nullptr, nullptr)
{
  ASTRALassert(src);
  ASTRALassert(src->data());

  enum
    {
      split_mask    = 1,
      split_x_fixed = 0,
      split_y_fixed = 1,

      place_mask  = 2,
      place_below = 0,
      place_right = 2,
    };

  vecN<int, 4> largest_rect_area(-1);
  vecN<ivec2, 2> corner, sz;
  int dx, dy, choice, best, child_add;
  int a0, a1, a2;
  Entry::Rectangle *R(src->data());

  dx = size().x() - R->size().x();
  dy = size().y() - R->size().y();

  /* choose the configuration that leaves behind the largest possible
   * rectangle in terms of area. The functions compute_area() and
   * compute_max_area() return -1 if any of the arguments is negative;
   * this way we automatically prevent using an invalid choice.
   */
  a0 = compute_area(R->size().x(), dy);
  a1 = compute_area(dx - rect_to_add->size().x(), size().y());
  a2 = compute_area(dx, size().y() - rect_to_add->size().y());
  largest_rect_area[split_x_fixed | place_right] = compute_max_area(a0, a1, a2);

  a0 = compute_area(dx, size().y());
  a1 = compute_area(R->size().x(), dy - rect_to_add->size().y());
  a2 = compute_area(R->size().x() - rect_to_add->size().x(), dy);
  largest_rect_area[split_x_fixed | place_below] = compute_max_area(a0, a1, a2);

  a0 = compute_area(size().x(), dy);
  a1 = compute_area(dx, R->size().y() - rect_to_add->size().y());
  a2 = compute_area(dx - rect_to_add->size().x(), R->size().y());
  largest_rect_area[split_y_fixed | place_right] = compute_max_area(a0, a1, a2);

  a0 = compute_area(dx, R->size().y());
  a1 = compute_area(size().x() - rect_to_add->size().x(), dy);
  a2 = compute_area(size().x(), dy - rect_to_add->size().y());
  largest_rect_area[split_y_fixed | place_below] = compute_max_area(a0, a1, a2);

  choice = 0;
  best = largest_rect_area[0];
  for (int i = 1; i < 4; ++i)
    {
      if (best < largest_rect_area[i])
        {
          choice = i;
          best = largest_rect_area[i];
        }
    }

  ASTRALassert(best >= 0);
  if ((choice & split_mask) == split_x_fixed)
    {
      corner[0] = ivec2(minX_minY().x(), minX_minY().y() + R->size().y());
      sz[0] = ivec2(R->size().x(), dy);

      corner[1] = ivec2(minX_minY().x() + R->size().x(), minX_minY().y());
      sz[1] = ivec2(dx, size().y());

      child_add = ((choice & place_mask) == place_below) ? 0 : 1;
    }
  else
    {
      ASTRALassert((choice & split_mask) == split_y_fixed);

      corner[0] = ivec2(minX_minY().x() + R->size().x(), minX_minY().y());
      sz[0] = ivec2(dx, R->size().y());

      corner[1] = ivec2(minX_minY().x(), minX_minY().y() + R->size().y());
      sz[1] = ivec2(size().x(), dy);

      child_add = ((choice & place_mask) == place_right) ? 0 : 1;
    }

  vecN<NodeWithoutChildren*, 2> children;
  for (int i = 0; i < 2; ++i)
    {
      m_children[i] = children[i] = pool.create_node_without_children(atlas, this, i, corner[i], sz[i]);
    }
  m_children[2] = pool.create_node_without_children(atlas, this, 2u, R->minX_minY(), R->size(), R);

  m_children[child_add] = children[child_add]->add(atlas, rect_to_add);
  ASTRALassert(m_children[child_add] == children[child_add]);
}

void
astral::LayeredRectAtlas::Implement::NodeWithChildren::
reclaim_implement(MemoryPool &pool)
{
  for (unsigned int i = 0; i < 3u; ++i)
    {
      ASTRALassert(m_children[i]->is_logically_empty());
      if (!m_children[i]->is_hard_empty())
        {
          ASTRALassert(m_children[i]->is_empty_node_without_children());
          m_children[i]->reclaim(pool);
        }
      m_children[i] = nullptr;
    }
  pool.reclaim(this);
}

void
astral::LayeredRectAtlas::Implement::NodeWithChildren::
add_from_child(RectAtlas &atlas, NodeWithoutChildren *child, Entry::Rectangle *rect)
{
  unsigned int which_child;

  ASTRALassert(!reclaimed());
  ASTRALassert(child);
  ASTRALassert(!child->reclaimed());

  which_child = child->child_id();
  ASTRALassert(m_children[which_child] == child);

  m_children[which_child] = child->add(atlas, rect);
}

void
astral::LayeredRectAtlas::Implement::NodeWithChildren::
handle_hard_empty_children(RectAtlas &atlas)
{
  vecN<unsigned int, 2> empty_children;
  unsigned int cnt(0);

  ASTRALassert(!is_hard_empty());
  for (unsigned int i = 0; i < 3; ++i)
    {
      if (m_children[i]->is_logically_empty())
        {
          empty_children[cnt] = i;
          ++cnt;
        }
    }

  ASTRALassert(cnt < 3u);
  ASTRALassert(cnt > 0u);
  for (unsigned int i = 0; i < cnt; ++i)
    {
      unsigned int child_idx;

      child_idx = empty_children[i];
      m_children[child_idx]->clear_buckets(atlas.layered_atlas());
      m_children[child_idx] = atlas.pool().create_node_without_children(atlas, this, child_idx,
                                                                        m_children[child_idx]->minX_minY(),
                                                                        m_children[child_idx]->size(),
                                                                        nullptr);
    }
}

////////////////////////////////////
// astral::LayeredRectAtlas::Implement::RectAtlas methods
astral::LayeredRectAtlas::Implement::RectAtlas::
RectAtlas(Implement *p):
  m_root(nullptr),
  m_layered_atlas(p),
  m_layer(~0u)
{
}

astral::LayeredRectAtlas::Implement::RectAtlas::
~RectAtlas()
{
}

void
astral::LayeredRectAtlas::Implement::RectAtlas::
init(ivec2 location, ivec2 dimensions, unsigned int layer)
{
  m_layer = layer;
  m_root = pool().create_node_without_children(*this, nullptr, -1, location, dimensions, nullptr);
}

void
astral::LayeredRectAtlas::Implement::RectAtlas::
init(ivec2 dimensions, unsigned int layer)
{
  m_layer = layer;
  m_root = pool().create_node_without_children(*this, nullptr, -1, ivec2(0,0), dimensions, nullptr);
}

void
astral::LayeredRectAtlas::Implement::RectAtlas::
handle_hard_empty_root(void)
{
  ivec2 dimensions(m_root->size());

  m_root->clear_buckets(layered_atlas());
  m_root = pool().create_node_without_children(*this, nullptr, -1, ivec2(0,0), dimensions, nullptr);
}

astral::LayeredRectAtlas::Implement::Entry::Rectangle*
astral::LayeredRectAtlas::Implement::RectAtlas::
allocate_rectangle_from_node(const ivec2 &dimensions, NodeWithoutChildren *node)
{
  Entry::Rectangle *return_value(nullptr);
  return_value = pool().create_rectangle(dimensions, this);

  if (node == m_root)
    {
      NodeBase *R;

      R = node->add(*this, return_value);
      ASTRALassert(R);
      m_root = R;
    }
  else
    {
      NodeWithChildren *p;

      p = node->parent();
      ASTRALassert(p);
      ASTRALassert(!p->reclaimed());
      ASTRALassert(!node->reclaimed());

      p->add_from_child(*this, node, return_value);
    }

  return return_value;
}

///////////////////////////////////
// astral::LayeredRectAtlas methods
astral::reference_counted_ptr<astral::LayeredRectAtlas>
astral::LayeredRectAtlas::
create(void)
{
  return ASTRALnew Implement();
}

astral::LayeredRectAtlas::
LayeredRectAtlas(void)
{
}

astral::LayeredRectAtlas::
~LayeredRectAtlas(void)
{
}

void
astral::LayeredRectAtlas::
clear(ivec2 dimensions, unsigned int num_layers)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  p->clear_implement(dimensions, num_layers);
}

void
astral::LayeredRectAtlas::
add_user_region(ivec2 location, ivec2 dimensions, unsigned int layer)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  p->add_user_region_implement(location, dimensions, layer);
}

void
astral::LayeredRectAtlas::
number_layers(unsigned int N)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  p->number_layers_implement(N);
}

unsigned int
astral::LayeredRectAtlas::
number_layers(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_num_layers;
}

astral::ivec2
astral::LayeredRectAtlas::
dimensions(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_dimensions;
}

astral::LayeredRectAtlas::Entry
astral::LayeredRectAtlas::
allocate_rectangle(const ivec2 &dimensions)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  return p->allocate_rectangle_implement(dimensions);
}

void
astral::LayeredRectAtlas::
free_rectangles(c_array<const Entry> entries)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  Implement::NodeBase::free_rectangles(*p, entries, p->m_tmp);
}

void
astral::LayeredRectAtlas::
free_rectangle(Entry entry)
{
  c_array<const Entry> E(&entry, 1);
  Implement *p;

  p = static_cast<Implement*>(this);
  Implement::NodeBase::free_rectangles(*p, E, p->m_tmp);
}

/////////////////////////////////////////////
// astral::LayeredRectAtlas::Implement methods
astral::LayeredRectAtlas::Implement::
Implement(void):
  m_dimensions(0, 0),
  m_num_layers(0),
  m_num_user_regions(0)
{
  m_pool = ASTRALnew MemoryPool();
}

astral::LayeredRectAtlas::Implement::
~Implement()
{
  for (Bucket *p : m_bucket_backings)
    {
      ASTRALdelete(p);
    }
  m_pool->clear();
  ASTRALdelete(m_pool);
}

void
astral::LayeredRectAtlas::Implement::
clear_implement(ivec2 dimensions, unsigned int num_layers)
{
  m_pool->clear();
  m_dimensions = dimensions;
  ready_buckets(true);

  m_num_layers = num_layers;
  if (m_num_layers > m_layer_atlases.size())
    {
      m_layer_atlases.resize(m_num_layers);
    }

  for (unsigned int i = 0; i < m_num_layers; ++i)
    {
      if (!m_layer_atlases[i])
        {
          m_layer_atlases[i] = ASTRALnew RectAtlas(this);
        }
      m_layer_atlases[i]->init(m_dimensions, i);
    }

  m_num_user_regions = 0;
}

void
astral::LayeredRectAtlas::Implement::
number_layers_implement(unsigned int N)
{
  unsigned int oldN(m_num_layers);

  ASTRALassert(N >= m_num_layers);
  m_num_layers = N;

  if (m_num_layers > m_layer_atlases.size())
    {
      m_layer_atlases.resize(m_num_layers);
    }

  for (unsigned int i = oldN; i < m_num_layers; ++i)
    {
      if (!m_layer_atlases[i])
        {
          m_layer_atlases[i] = ASTRALnew RectAtlas(this);
        }
      m_layer_atlases[i]->init(m_dimensions, i);
    }
}


void
astral::LayeredRectAtlas::Implement::
add_user_region_implement(ivec2 location, ivec2 dimensions, unsigned int layer)
{
  if (m_num_user_regions >= m_user_region.size())
    {
      m_user_region.push_back(ASTRALnew RectAtlas(this));
    }

  ASTRALassert(m_num_user_regions < m_user_region.size());
  m_user_region[m_num_user_regions]->init(location, dimensions, layer);
  ++m_num_user_regions;
}

const astral::LayeredRectAtlas::Implement::BucketEntry*
astral::LayeredRectAtlas::Implement::
choose_bucket_entry(const ivec2 &dimensions)
{
  ASTRALassert(dimensions.x() > 0 && dimensions.y() > 0);

  int nI, nJ;
  nI = uint32_log2_floor(dimensions.x());
  nJ = uint32_log2_floor(dimensions.y());

  ASTRALassert(1u * dimensions.x() >= (1u << nI));
  ASTRALassert(1u * dimensions.y() >= (1u << nJ));
  ASTRALassert(1u * dimensions.x() <= (1u << (1u + nI)));
  ASTRALassert(1u * dimensions.y() <= (1u << (1u + nJ)));
  ASTRALassert(nI < m_log2_dimensions.x());
  ASTRALassert(nJ < m_log2_dimensions.y());

  /* TODO:
   *   Rather than checking all possible buckets, it would
   *   be ideal that we maintained somekind of list of
   *   non-empty buckets for each of the three access.
   */

  /* The best rect to take is a rect whose width or height
   * match exactly with the desired width or height. So, we
   * first check the buckets:
   *  - (nI, k) with k >= nJ
   *  - (1 + nI, k) with k >= nJ
   *  - (k, nJ) with k >= nI
   *  - (k, 1 + nJ) with k >= nI
   *
   * Tracking the tighest one horizontally or vertically.
   */
  const BucketEntry* p(nullptr);
  int min_diff(std::numeric_limits<int>::max());

  /* Idea: maybe should we also make a list of possible
   * choices and sort them by lexigraphically in
   * increasing order (A, B) where A is the smaller
   * of the differences between the dimensions and
   * the rect canidate and then take the first element
   * on the list. Should we also kill the early out too?
   */
  for (int i = nI; i < m_log2_dimensions.x() && i < nI + 2; ++i)
    {
      for (int k = nJ; k < m_log2_dimensions.y(); ++k)
        {
          Bucket *bucket;
          const BucketEntry *q;
          int q_diff;

          bucket = get_bucket(i, k);
          if ((q = bucket->choose_element(dimensions, &q_diff)))
            {
              if (!p || q_diff < min_diff)
                {
                  p = q;
                  min_diff = q_diff;
                }

              if (min_diff == 0)
                {
                  return p;
                }
            }
        }
    }

  for (int j = nJ; j < m_log2_dimensions.y() && j < nJ + 2; ++j)
    {
      for (int k = nI + 2; k < m_log2_dimensions.x(); ++k)
        {
          Bucket *bucket;
          const BucketEntry *q;
          int q_diff;

          bucket = get_bucket(k, j);
          if ((q = bucket->choose_element(dimensions, &q_diff)))
            {
              if (!p || q_diff < min_diff)
                {
                  p = q;
                  min_diff = q_diff;
                }

              if (min_diff == 0)
                {
                  return p;
                }
            }
        }
    }

  if (p)
    {
      return p;
    }

  /* We have tested all buckets which could match within the size
   * of dimensions the passed rectangle. Now, we just take the
   * first smallest bucket we can get our hands on. Note that
   * we have already tested all buckets (nI, k), (nI + 1, k),
   * (k, nJ) and (k, nJ + 1).
   */
  for (int n = nI + nJ, endn = m_log2_dimensions.x() + m_log2_dimensions.y(); n <= endn; ++n)
    {
      int begin_i, end_i;

      begin_i = t_max(nI + 2, n + 1 - m_log2_dimensions.x());
      end_i = t_min(n - nJ - 1, m_log2_dimensions.x());
      for (int i = begin_i; i < end_i; ++i)
        {
          Bucket *bucket;
          const BucketEntry *p;
          int j;

          ASTRALassert(i < m_log2_dimensions.x());
          ASTRALassert(i >= nI);

          j = n - i;
          if (j < m_log2_dimensions.y())
            {
              ASTRALassert(j >= nJ);

              bucket = get_bucket(i, j);
              p = bucket->choose_element(dimensions);
              if (p)
                {
                  return p;
                }
            }
        }
    }

  return nullptr;
}

astral::LayeredRectAtlas::Entry
astral::LayeredRectAtlas::Implement::
allocate_rectangle_implement(const ivec2 &dimensions)
{
  const BucketEntry *e;
  Entry return_value;

  e = choose_bucket_entry(dimensions);
  if (e)
    {
      NodeWithoutChildren *p;

      ASTRALassert(e->m_node);
      ASTRALassert(e->m_rect_atlas);

      p = e->m_node;
      return_value.m_rectangle = e->m_rect_atlas->allocate_rectangle_from_node(dimensions, p);
      ASTRALassert(return_value.valid());
    }

  return return_value;
}

void
astral::LayeredRectAtlas::Implement::
ready_buckets(bool clear_buckets)
{
  /* The entries at get_bucket(I, J) are so that the:
   *   2^I <= Width  < 2^(I + 1)
   *   2^J <= Height < 2^(J + 1)
   */
  unsigned int nI, nJ, total, current_size;

  nI = 1u + uint32_log2_floor(m_dimensions.x());
  nJ = 1u + uint32_log2_floor(m_dimensions.y());

  total = nI * nJ;
  current_size = m_bucket_backings.size();

  m_log2_dimensions.x() = nI;
  m_log2_dimensions.y() = nJ;

  // resize only grows.
  total = t_max(current_size, total);
  m_bucket_backings.resize(total, nullptr);
  for (unsigned int k = 0; k < total; ++k)
    {
      if (!m_bucket_backings[k])
        {
          m_bucket_backings[k] = ASTRALnew Bucket();
        }
      else if(clear_buckets)
        {
          m_bucket_backings[k]->clear();
        }
    }
}

const astral::LayeredRectAtlas::Implement::BucketEntry&
astral::LayeredRectAtlas::Implement::
add_bucket_entry(RectAtlas *atlas, int which, NodeWithoutChildren *p, ivec2 size)
{
  Bucket *dst;
  unsigned int I, J;

  ASTRALassert(size.x() > 0);
  ASTRALassert(size.y() > 0);
  ASTRALassert(atlas);
  ASTRALassert(&atlas->layered_atlas() == this);

  I = uint32_log2_floor(size.x());
  J = uint32_log2_floor(size.y());
  dst = get_bucket(I, J);

  ASTRALassert(1u * size.x() >= (1u << I));
  ASTRALassert(1u * size.y() >= (1u << J));
  ASTRALassert(1u * size.x() <= (1u << (1u + I)));
  ASTRALassert(1u * size.y() <= (1u << (1u + J)));

  dst->m_elements.push_back(BucketEntry());
  dst->m_elements.back().m_bucket = dst;
  dst->m_elements.back().m_idx = dst->m_elements.size() - 1u;
  dst->m_elements.back().m_node = p;
  dst->m_elements.back().m_idx_in_node = which;
  dst->m_elements.back().m_rect_atlas = atlas;
  dst->m_elements.back().m_size = size;

  return dst->m_elements.back();
}

void
astral::LayeredRectAtlas::Implement::
remove_bucket_entry(BucketEntry &entry)
{
  if (!entry.valid())
    {
      return;
    }

  unsigned int idx(entry.m_idx);
  Bucket *b(entry.m_bucket);

  ASTRALassert(idx < b->m_elements.size());
  if (idx != b->m_elements.size() - 1u)
    {
      std::swap(b->m_elements.back(),  b->m_elements[idx]);
      b->m_elements[idx].m_idx = idx;
      b->m_elements[idx].m_node->update_entry(b->m_elements[idx],
                                              b->m_elements[idx].m_idx_in_node);
    }
  b->m_elements.pop_back();
  entry = BucketEntry();
}

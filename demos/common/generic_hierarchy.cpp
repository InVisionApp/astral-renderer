/*!
 * \file generic_hierarchy.cpp
 * \brief generic_hierarchy.cpp
 *
 * Adapted from: generic_hierarchy.cpp of FastUIDraw (https://github.com/intel/fastuidraw):
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#include <astral/util/astral_memory.hpp>
#include "generic_hierarchy.hpp"

namespace
{
enum
  {
    splitting_size = 20,
  };

class Element
{
public:
  Element(const astral::BoundingBox<float> &b, unsigned int r):
    m_box(b),
    m_reference(r)
  {}

  astral::BoundingBox<float> m_box;
  unsigned int m_reference;
};

class TreeBase:astral::noncopyable
{
public:
  explicit
  TreeBase(const astral::BoundingBox<float> &bbox):
    m_bbox(bbox)
  {}

  virtual
  ~TreeBase()
  {}

  TreeBase*
  add(const astral::BoundingBox<float> &bbox, unsigned int reference)
  {
    if (bbox.intersects(m_bbox))
      {
        return add_implement(bbox, reference);
      }
    else
      {
        return this;
      }
  }

  void
  query(const astral::BoundingBox<float> &bbox,
        std::vector<unsigned int> *output)
  {
    if(bbox.intersects(m_bbox))
      {
        query_implement(bbox, output);
      }
  }

  void
  query(const astral::vec2 pt,
        std::vector<unsigned int> *output)
  {
    if(m_bbox.contains(pt))
      {
        query_implement(pt, output);
      }
  }

  unsigned int
  query(const astral::vec2 &p,
        astral::BoundingBox<float> *out_bb)
  {
    if (m_bbox.contains(p))
      {
        return query_implement(p, out_bb);
      }
    else
      {
        return GenericHierarchy::not_found;
      }
  }

  const astral::BoundingBox<float>&
  bounding_box(void)
  {
    return m_bbox;
  }

private:
  virtual
  TreeBase*
  add_implement(const astral::BoundingBox<float> &bbox,
                unsigned int reference) = 0;

  virtual
  void
  query_implement(const astral::BoundingBox<float> &bbox,
                  std::vector<unsigned int> *output) = 0;

  virtual
  void
  query_implement(const astral::vec2 &p,
                  std::vector<unsigned int> *output) = 0;

  virtual
  unsigned int
  query_implement(const astral::vec2 &p,
                  astral::BoundingBox<float> *out_bb) = 0;

  astral::BoundingBox<float> m_bbox;
};

class Node:public TreeBase
{
public:
  Node(const astral::BoundingBox<float> &bbox,
       const astral::BoundingBox<float> &bbox0,
       const std::vector<Element> &elements0,
       const astral::BoundingBox<float> &bbox1,
       const std::vector<Element> &elements1);

  ~Node();

private:
  virtual
  TreeBase*
  add_implement(const astral::BoundingBox<float> &bbox,
                unsigned int reference);

  virtual
  void
  query_implement(const astral::BoundingBox<float> &bbox,
                  std::vector<unsigned int> *output);

  virtual
  void
  query_implement(const astral::vec2 &p,
                  std::vector<unsigned int> *output);

  virtual
  unsigned int
  query_implement(const astral::vec2 &p,
                  astral::BoundingBox<float> *out_bb);

  astral::vecN<TreeBase*, 2> m_children;
  std::vector<Element> m_elements;
};

class Leaf:public TreeBase
{
public:
  explicit
  Leaf(const astral::BoundingBox<float> &bbox);

  Leaf(const astral::BoundingBox<float> &bbox,
       const std::vector<Element> &elements);

  ~Leaf();

private:
  virtual
  TreeBase*
  add_implement(const astral::BoundingBox<float> &bbox,
                unsigned int reference);

  virtual
  void
  query_implement(const astral::BoundingBox<float> &bbox,
                  std::vector<unsigned int> *output);

  virtual
  void
  query_implement(const astral::vec2 &p,
                  std::vector<unsigned int> *output);

  virtual
  unsigned int
  query_implement(const astral::vec2 &p,
                  astral::BoundingBox<float> *out_bb);

  std::vector<Element> m_elements;
};

}

///////////////////////////////////
// Node methods
Node::
Node(const astral::BoundingBox<float> &bbox,
     const astral::BoundingBox<float> &bbox0,
     const std::vector<Element> &elements0,
     const astral::BoundingBox<float> &bbox1,
     const std::vector<Element> &elements1):
  TreeBase(bbox)
{
  m_children[0] = ASTRALnew Leaf(bbox0, elements0);
  m_children[1] = ASTRALnew Leaf(bbox1, elements1);
}

Node::
~Node()
{
  ASTRALdelete(m_children[0]);
  ASTRALdelete(m_children[1]);
}

TreeBase*
Node::
add_implement(const astral::BoundingBox<float> &bbox, unsigned int reference)
{
  astral::vecN<bool, 2> child_takes;

  for (unsigned int i = 0; i < 2; ++i)
    {
      child_takes[i] = m_children[i]->bounding_box().intersects(bbox);
    }

  if (child_takes[0] && child_takes[1])
    {
      Element E(bbox, reference);
      m_elements.push_back(E);
    }
  else
    {
      unsigned int IDX;
      TreeBase *p;

      IDX = child_takes[0] ? 0u : 1u;
      p = m_children[IDX]->add(bbox, reference);
      if (p != m_children[IDX])
        {
          ASTRALdelete(m_children[IDX]);
          m_children[IDX] = p;
        }
    }

  return this;
}

void
Node::
query_implement(const astral::BoundingBox<float> &bbox,
                std::vector<unsigned int> *output)
{
  m_children[0]->query(bbox, output);
  m_children[1]->query(bbox, output);
  for (const auto &E : m_elements)
    {
      if (E.m_box.intersects(bbox))
        {
          output->push_back(E.m_reference);
        }
    }
}

void
Node::
query_implement(const astral::vec2 &pt,
                std::vector<unsigned int> *output)
{
  m_children[0]->query(pt, output);
  m_children[1]->query(pt, output);
  for (const auto &E : m_elements)
    {
      if (E.m_box.contains(pt))
        {
          output->push_back(E.m_reference);
        }
    }
}

unsigned int
Node::
query_implement(const astral::vec2 &p,
                astral::BoundingBox<float> *out_bb)
{
  unsigned int R;

  R = m_children[0]->query(p, out_bb);
  if (R != GenericHierarchy::not_found)
    {
      return R;
    }
  R = m_children[1]->query(p, out_bb);
  if (R != GenericHierarchy::not_found)
    {
      return R;
    }

  for (const auto &E : m_elements)
    {
      if (E.m_box.contains(p))
        {
          *out_bb = E.m_box;
          return E.m_reference;
        }
    }
  return GenericHierarchy::not_found;
}

///////////////////////////////////////////
// Leaf methods
Leaf::
Leaf(const astral::BoundingBox<float> &bbox, const std::vector<Element> &elements):
  TreeBase(bbox),
  m_elements(elements)
{}

Leaf::
Leaf(const astral::BoundingBox<float> &bbox):
  TreeBase(bbox)
{}

Leaf::
~Leaf()
{}

void
Leaf::
query_implement(const astral::BoundingBox<float> &bbox,
                std::vector<unsigned int> *output)
{
  for (const auto &E : m_elements)
    {
      if (E.m_box.intersects(bbox))
        {
          output->push_back(E.m_reference);
        }
    }
}

void
Leaf::
query_implement(const astral::vec2 &pt,
                std::vector<unsigned int> *output)
{
  for (const auto &E : m_elements)
    {
      if (E.m_box.contains(pt))
        {
          output->push_back(E.m_reference);
        }
    }
}

unsigned int
Leaf::
query_implement(const astral::vec2 &p,
                astral::BoundingBox<float> *out_bb)
{
  for (const auto &E : m_elements)
    {
      if (E.m_box.contains(p))
        {
          *out_bb = E.m_box;
          return E.m_reference;
        }
    }
  return GenericHierarchy::not_found;
}

TreeBase*
Leaf::
add_implement(const astral::BoundingBox<float> &bbox, unsigned int reference)
{
  m_elements.push_back(Element(bbox, reference));
  if (m_elements.size() > splitting_size)
    {
      astral::vecN<std::vector<Element>, 2> split_x, split_y;
      astral::vecN<astral::BoundingBox<float>, 2> split_x_bb, split_y_bb;
      unsigned int split_x_size(0), split_y_size(0);

      split_x_bb = bounding_box().split_x();
      split_y_bb = bounding_box().split_y();

      for (const Element &E : m_elements)
        {
          for (unsigned int i = 0; i < 2; ++i)
            {
              if (split_x_bb[i].intersects(E.m_box))
                {
                  split_x[i].push_back(E);
                  ++split_x_size;
                }
              if (split_y_bb[i].intersects(E.m_box))
                {
                  split_y[i].push_back(E);
                  ++split_y_size;
                }
            }

          ASTRALassert(split_x_bb[0].intersects(E.m_box) || split_x_bb[1].intersects(E.m_box));
          ASTRALassert(split_y_bb[0].intersects(E.m_box) || split_y_bb[1].intersects(E.m_box));
        }

      unsigned int allow_split;
      const float max_overlap(1.5f);
      allow_split = static_cast<unsigned int>(max_overlap * static_cast<float>(m_elements.size()));

      if (astral::t_min(split_x_size, split_y_size) <= allow_split)
        {
          if (split_x_size < split_y_size)
            {
              return ASTRALnew Node(bounding_box(),
                                        split_x_bb[0], split_x[0],
                                        split_x_bb[1], split_x[1]);
            }
          else
            {
              return ASTRALnew Node(bounding_box(),
                                        split_y_bb[0], split_y[0],
                                        split_y_bb[1], split_y[1]);
            }
        }
    }
  return this;
}

///////////////////////////////////
// GenericHierarchy methods
GenericHierarchy::
GenericHierarchy(const astral::BoundingBox<float> &bbox)
{
  m_root = ASTRALnew Leaf(bbox);
}

GenericHierarchy::
~GenericHierarchy()
{
  TreeBase *d;

  d = static_cast<TreeBase*>(m_root);
  ASTRALdelete(d);
}

void
GenericHierarchy::
add(const astral::BoundingBox<float> &bbox, unsigned int reference)
{
  TreeBase *d, *p;

  d = static_cast<TreeBase*>(m_root);
  p = d->add(bbox, reference);
  if (p != d)
    {
      ASTRALdelete(d);
      m_root = p;
    }
}

void
GenericHierarchy::
query(const astral::BoundingBox<float> &bbox, std::vector<unsigned int> *output)
{
  TreeBase *d;

  d = static_cast<TreeBase*>(m_root);
  d->query(bbox, output);
}

void
GenericHierarchy::
query(const astral::vec2 &pt, std::vector<unsigned int> *output)
{
  TreeBase *d;

  d = static_cast<TreeBase*>(m_root);
  d->query(pt, output);
}

unsigned int
GenericHierarchy::
query(const astral::vec2 &p, astral::BoundingBox<float> *out_bb)
{
  TreeBase *d;

  d = static_cast<TreeBase*>(m_root);
  return d->query(p, out_bb);
}

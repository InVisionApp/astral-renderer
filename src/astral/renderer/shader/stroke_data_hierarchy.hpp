/*!
 * \file stroke_data_hierarchy.hpp
 * \brief file stroke_data_hierarchy.hpp
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

#ifndef ASTRAL_STROKE_DATA_HEIRARCHY_HPP
#define ASTRAL_STROKE_DATA_HEIRARCHY_HPP

#include <astral/renderer/shader/stroke_shader.hpp>
#include <astral/renderer/shader/stroke_query.hpp>

/* Oversimplified overview of data construction.
 *  1. Create the hierarchy tree for sparse stroking from
 *     the input data
 *    a. Create list of leaves. StaticLeafBuilder for non-animated
 *       paths and  AnimatedLeafBuilder for animated paths
 *    b. Insert the leaves into a hierarchy. StaticHierarchy for
 *       StaticLeafBuilder and AnimatedHierarchy for
 *       AnimatedLeafBuilder
 *      i. The hierarchy classes do NOT represent children
 *         as raw elements. Instead all Nodes and Leaf values
 *         are backed by continuous arrays in the classes
 *         StaticHierarchy and AnimatedLeafBuilder. A child
 *         node and leaf list instead are represented as a pair
 *         ranges into those arrays that are accessed by the
 *         virtaul methods get_leaf() and get_node(). The
 *         creation of the hierarchies has the downside that
 *         creation has more malloc/free noise coming from the
 *         temporary arrays of Nodes.
 *    c. The creation of the hierarchy will also procude an ordering
 *       on the input data so that leaves and nodes index/vertex data
 *       is also a range into the vertex/index data of the entire
 *       stroked path.
 *  2. With the ordering made from creating the hierarchy, create
 *     the VertexData objects, one object for each primitive_type_t
 */
class astral::detail::StrokeDataHierarchy:
  public reference_counted<StrokeDataHierarchy>::non_concurrent
{
public:
  typedef StrokeShader::Ordering Ordering;

  typedef StrokeShader::LineSegment LineSegment;
  typedef StrokeShader::Quadratic Quadratic;
  typedef StrokeShader::Join Join;
  typedef StrokeShader::Cap Cap;

  typedef StrokeShader::RawData RawData;
  typedef StrokeShader::RawAnimatedData RawAnimatedData;

  class AABB;
  class Base;
  class StaticLeafData;
  class StaticLeafBuilder;
  class AnimatedLeafBuilder;
  class StaticHierarchy;
  class AnimatedHierarchy;

  class Split
  {
  public:
    std::vector<StaticLeafData> m_before, m_after, m_both;
    BoundingBox<float> m_bb_before, m_bb_after, m_bb_both;
  };

  class AnimatedSplit
  {
  public:
    Split m_s0, m_s1;
  };

  virtual
  ~StrokeDataHierarchy()
  {}

  virtual
  const Base&
  root(void) const = 0;

  virtual
  const Base&
  get_leaf(unsigned int I) const = 0;

  virtual
  const Base&
  get_node(unsigned int I) const = 0;
};

class astral::detail::StrokeDataHierarchy::AABB
{
public:
  enum
    {
      edge_point = 0,
      join_point = 1,
      cap_point = 2
    };

  BoundingBox<float>
  compute(const Transformation &pixel_transformation_logical,
          const Transformation &logical_transformation_path,
          const StrokeQuery::StrokeRadii &stroke_params) const;

  void
  union_point(vec2 p, uint32_t flags)
  {
    m_flags |= flags;
    m_bb.union_point(p);
  }

  void
  init(c_array<const vec2> pts);

  void
  init_as_empty(void)
  {
    m_bb.clear();
    m_flags = 0u;
  }

private:
  uint32_t m_flags;
  BoundingBox<float> m_bb;
};

class astral::detail::StrokeDataHierarchy::StaticLeafData
{
public:
  /* axis aligned bounding box containing the leaf */
  AABB m_aabb;

  /* range into astral::StrokeShader::RawData or
   * astral::StrokeShader::RawAnimatedData
   * elements that this leaf takes.
   */
  vecN<range_type<unsigned int>, StrokeShader::number_primitive_types> m_elements;

  /* axis-aligned BB containing the leaf */
  BoundingBox<float> m_containing_bb;
};


class astral::detail::StrokeDataHierarchy::StaticLeafBuilder
{
public:
  static
  void
  create_leaves(std::vector<StaticLeafData> *out_leaves,
                BoundingBox<float> *out_bb,
                const RawData &input);

  explicit
  StaticLeafBuilder(const RawData &input):
    m_input(input),
    m_currentR(0),
    m_prevR(0),
    m_aabb_inited(false)
  {}

  /* Returns true if the element at info should trigger starting a new leaf;
   */
  bool
  should_emit_data(const RawData::Info *prev_info, const RawData::Info &info);

  /* add to the current state absorbing the element,
   * absorbed elements will be placed into the leaf
   * emitted at emit_leaf(). The value of pts comes
   * from extract_pts()
   */
  void
  absorb_element(const RawData::Info &info, const std::vector<Join> *inner_glue = nullptr);

  /* Create and return a Leaf object holding the values
   * passed to absorb_element() since the last call to
   * emit_leaf().
   */
  void
  emit_data(std::vector<StaticLeafData> *dst);

  /* Returns true if an element should be regarded as a point,
   * if the element is not regarded as a point, set pts to
   * be a c_array<> containing the two or three points of the
   * primitive
   */
  bool
  element_is_degenerate(const RawData::Info &info);

private:
  const RawData &m_input;
  vecN<unsigned int, StrokeShader::number_primitive_types> m_currentR, m_prevR;
  bool m_aabb_inited;
  AABB m_aabb;
  vecN<vec2, 3> m_pts;
  std::vector<vec2> m_waiting_pts;
  uint32_t m_waiting_flags;
};

class astral::detail::StrokeDataHierarchy::AnimatedLeafBuilder
{
public:
  static
  void
  create_leaves(std::vector<StaticLeafData> *out_leaves0,
                std::vector<StaticLeafData> *out_leaves1,
                BoundingBox<float> *out_bb0,
                BoundingBox<float> *out_bb1,
                const RawAnimatedData &input);

  explicit
  AnimatedLeafBuilder(const RawAnimatedData &input):
    m_input(input),
    m_start(input.m_start),
    m_end(input.m_end)
  {}

  bool
  should_emit_data(const RawData::Info *prev_info0, const RawData::Info &info0,
                   const RawData::Info *prev_info1, const RawData::Info &info1)
  {
    return m_start.should_emit_data(prev_info0, info0)
      || m_end.should_emit_data(prev_info1, info1);
  }

  void
  absorb_element(const RawData::Info &info0, const RawData::Info &info1)
  {
    m_start.absorb_element(info0, &m_input.m_start_inner_glue);
    m_end.absorb_element(info1, &m_input.m_end_inner_glue);
  }

  void
  emit_data(std::vector<StaticLeafData> *dst0,
            std::vector<StaticLeafData> *dst1)
  {
    m_start.emit_data(dst0);
    m_end.emit_data(dst1);
  }

private:
  const RawAnimatedData &m_input;
  StaticLeafBuilder m_start, m_end;
};

class astral::detail::StrokeDataHierarchy::Base
{
public:
  enum node_type_t:uint32_t
    {
      node_of_nodes,
      node_of_leaves,
    };

  /* Constructor for Leaf
   * \param ranges for each primitive_type_t, a range into
   *               StrokedShader::Data what is in the leaf
   * \param id pointer to ID value, value at pointer is used
   *           for ID, and then value pointed to be is incremented
   * \parm element_ordering blah-blah-blah
   */
  Base(const vecN<range_type<unsigned int>, StrokeShader::number_primitive_types> &ranges,
       unsigned int *id, Ordering *element_ordering);

  /* Constructor for Node
   * \param src DataHierarchy on which the object will reside
   * \param node_type node type determines how to interpret change_range
   * \param child_range if node_type is node_of_nodes, then range of
   *                    values to pass to DataHierarchy::get_node(),
   *                    otherwise, a range of value to pass to
   *                    DataHierarchy::get_leaf()
   * \param id pointer to ID value, value at pointer is used
   *           for ID, and then value pointed to be is incremented
   */
  Base(const StrokeDataHierarchy &src, enum node_type_t node_type,
       range_type<unsigned int> child_range, unsigned int *id);

  virtual
  ~Base(void)
  {}

  virtual
  BoundingBox<float>
  bounding_box(const Transformation &pixel_transformation_logical,
               const Transformation &logical_transformation_path,
               float t, const StrokeQuery::StrokeRadii &stroke_params) const = 0;

  void
  add_elements(const StrokeQuery::ActivePrimitives active_primitives,
               vecN<std::vector<range_type<int>>, StrokeShader::number_primitive_types> *ptr_dst) const;

  void
  add_elements(const StrokeQuery::ActivePrimitives active_primitives,
               const vecN<std::vector<range_type<int>>*, StrokeShader::number_primitive_types> &dst) const;

  unsigned int
  id(void) const
  {
    return m_id;
  }

  int
  number_vertices(unsigned int primitive_type) const
  {
    return m_num_verts[primitive_type];
  }

  unsigned int
  number_child_leaves(void) const
  {
    return m_child_leaves_range.difference();
  }

  unsigned int
  number_child_nodes(void) const
  {
    return m_child_nodes_range.difference();
  }

  bool
  is_node(void) const
  {
    return m_is_node;
  }

  const Base&
  child_leaf(unsigned int I, const StrokeDataHierarchy &src) const;

  const Base&
  child_node(unsigned int I, const StrokeDataHierarchy &src) const;

protected:
  /* Enlarges this->m_vertex_ranges to contain
   * base.m_vertex_ranges and returns the size
   * of base.m_vertex_ranges
   */
  vecN<int, StrokeShader::number_primitive_types>
  absorb_ranges(const Base &base);

  unsigned int m_id;
  vecN<int, StrokeShader::number_primitive_types> m_num_verts;
  range_type<unsigned int> m_child_nodes_range, m_child_leaves_range;
  bool m_is_node;

  /* records actual range of vertices of
   * CookedData::m_vertex_data[primitive_type_t][0] to use
   */
  vecN<range_type<int>, StrokeShader::number_primitive_types> m_vertex_ranges;
};

class astral::detail::StrokeDataHierarchy::StaticHierarchy:
  public astral::detail::StrokeDataHierarchy
{
public:
  static
  reference_counted_ptr<StrokeDataHierarchy>
  create(const RawData &input, unsigned int *out_hierarchy_size, Ordering *out_element_ordering);

  virtual
  const Base&
  root(void) const override final
  {
    return m_nodes.back();
  }

  virtual
  const Base&
  get_leaf(unsigned int I) const override final
  {
    ASTRALassert(I < m_leaves.size());
    return m_leaves[I];
  }

  virtual
  const Base&
  get_node(unsigned int I) const override final
  {
    ASTRALassert(I < m_nodes.size());
    return m_nodes[I];
  }

private:
  class Leaf:public Base
  {
  public:
    explicit
    Leaf(const AABB &aabb,
         const vecN<range_type<unsigned int>, StrokeShader::number_primitive_types> &ranges,
         unsigned int *id, Ordering *element_ordering):
      Base(ranges, id, element_ordering),
      m_aabb(aabb)
    {}

    BoundingBox<float>
    bounding_box(const Transformation &pixel_transformation_logical,
                 const Transformation &logical_transformation_path,
                 float t, const StrokeQuery::StrokeRadii &stroke_params) const override final
    {
      ASTRALunused(t);
      return m_aabb.compute(pixel_transformation_logical,
                           logical_transformation_path,
                           stroke_params);
    }

  private:
    AABB m_aabb;
  };

  class Node:public Base
  {
  public:
    explicit
    Node(const StaticHierarchy &src,
         enum node_type_t node_type,
         range_type<unsigned int> child_range,
         unsigned int *id):
      Base(src, node_type, child_range, id)
    {
      StrokeQuery::StrokeRadii S;
      Transformation tr;
      Transformation sc;

      if (node_type == node_of_leaves)
        {
          for (unsigned int leaf = child_range.m_begin; leaf < child_range.m_end; ++leaf)
            {
              m_bb.union_box(src.m_leaves[leaf].bounding_box(tr, sc, 0.0f, S));
            }
        }
      else
        {
          for (unsigned int node = child_range.m_begin; node < child_range.m_end; ++node)
            {
              m_bb.union_box(src.m_nodes[node].bounding_box(tr, sc, 0.0f, S));
            }
        }
    }

    BoundingBox<float>
    bounding_box(const Transformation &pixel_transformation_logical,
                 const Transformation &logical_transformation_path,
                 float t, const StrokeQuery::StrokeRadii &stroke_params) const
    {
      BoundingBox<float> return_value;
      float r(stroke_params.max_radius());

      ASTRALunused(t);
      return_value = logical_transformation_path.apply_to_bb(m_bb);
      return_value.enlarge(vec2(r, r));
      return_value = pixel_transformation_logical.apply_to_bb(return_value);

      return return_value;
    }

  private:
    BoundingBox<float> m_bb;
  };

  static
  void
  compute_split(unsigned int coord, const BoundingBox<float> &bb,
                c_array<const StaticLeafData> leaves,
                Split *out_value);

  Node
  create_node(c_array<const StaticLeafData> leaf_data,
              unsigned int *id, Ordering *element_ordering);

  Node
  create_hierarchy(unsigned int max_depth, unsigned int split_threshhold,
                   const BoundingBox<float> &bb, c_array<const StaticLeafData> leaf_data,
                   unsigned int *id, Ordering *element_ordering);

  std::vector<Leaf> m_leaves;
  std::vector<Node> m_nodes;
};

class astral::detail::StrokeDataHierarchy::AnimatedHierarchy:
  public astral::detail::StrokeDataHierarchy
{
public:
  static
  reference_counted_ptr<StrokeDataHierarchy>
  create(const RawAnimatedData &input,
         unsigned int *out_hierarchy_size,
         Ordering *out_element_ordering);

  virtual
  const Base&
  root(void) const override final
  {
    return m_nodes.back();
  }

  virtual
  const Base&
  get_leaf(unsigned int I) const override final
  {
    ASTRALassert(I < m_leaves.size());
    return m_leaves[I];
  }

  virtual
  const Base&
  get_node(unsigned int I) const override final
  {
    ASTRALassert(I < m_nodes.size());
    return m_nodes[I];
  }

private:

  class Leaf:public Base
  {
  public:
    Leaf(const AABB &aabb0, const AABB &aabb1,
         const vecN<range_type<unsigned int>, StrokeShader::number_primitive_types> &ranges,
         unsigned int *id, Ordering *element_ordering):
      Base(ranges, id, element_ordering),
      m_aabb0(aabb0),
      m_aabb1(aabb1)
    {
    }

    virtual
    BoundingBox<float>
    bounding_box(const Transformation &pixel_transformation_logical,
                 const Transformation &logical_transformation_path,
                 float t, const StrokeQuery::StrokeRadii &stroke_params) const override final
    {
      BoundingBox<float> b0(m_aabb0.compute(pixel_transformation_logical, logical_transformation_path, stroke_params));
      BoundingBox<float> b1(m_aabb1.compute(pixel_transformation_logical, logical_transformation_path, stroke_params));
      BoundingBox<float> return_value;

      BoundingBox<float>::interpolate(b0, b1, t, &return_value);
      return return_value;
    }

  private:
    AABB m_aabb0, m_aabb1;
  };

  class Node:public Base
  {
  public:
    Node(const AnimatedHierarchy &src,
         enum node_type_t node_type,
         range_type<unsigned int> child_range,
         unsigned int *id):
      Base(src, node_type, child_range, id)
    {
      StrokeQuery::StrokeRadii S;
      Transformation tr;
      Transformation sc;

      if (node_type == node_of_leaves)
        {
          for (unsigned int leaf = child_range.m_begin; leaf < child_range.m_end; ++leaf)
            {
              m_bb0.union_box(src.m_leaves[leaf].bounding_box(tr, sc, 0.0f, S));
              m_bb1.union_box(src.m_leaves[leaf].bounding_box(tr, sc, 1.0f, S));
            }
        }
      else
        {
          for (unsigned int node = child_range.m_begin; node < child_range.m_end; ++node)
            {
              m_bb0.union_box(src.m_nodes[node].bounding_box(tr, sc, 0.0f, S));
              m_bb1.union_box(src.m_nodes[node].bounding_box(tr, sc, 1.0f, S));
            }
        }
    }

    virtual
    BoundingBox<float>
    bounding_box(const Transformation &pixel_transformation_logical,
                 const Transformation &logical_transformation_path,
                 float t, const StrokeQuery::StrokeRadii &stroke_params) const override final
    {
      BoundingBox<float> return_value;
      float r(stroke_params.max_radius());

      BoundingBox<float>::interpolate(m_bb0, m_bb1, t, &return_value);
      return_value = logical_transformation_path.apply_to_bb(return_value);
      return_value.enlarge(vec2(r, r));
      return_value = pixel_transformation_logical.apply_to_bb(return_value);

      return return_value;
    }

  private:
    BoundingBox<float> m_bb0, m_bb1;
  };

  Node
  create_node(c_array<const StaticLeafData> leaf_data0,
              c_array<const StaticLeafData> leaf_data1,
              unsigned int *id, Ordering *element_ordering);

  Node
  create_hierarchy(unsigned int max_depth, unsigned int split_threshhold,
                   const BoundingBox<float> &bb0, c_array<const StaticLeafData> leaf_data0,
                   const BoundingBox<float> &bb1, c_array<const StaticLeafData> leaf_data1,
                   unsigned int *id, Ordering *element_ordering);

  static
  void
  compute_split(unsigned int coord0, unsigned int coord1,
                const BoundingBox<float> &bb0,
                c_array<const StaticLeafData> leaves0,
                const BoundingBox<float> &bb1,
                c_array<const StaticLeafData> leaves1,
                AnimatedSplit *out_value);

  std::vector<Leaf> m_leaves;
  std::vector<Node> m_nodes;
};

#endif

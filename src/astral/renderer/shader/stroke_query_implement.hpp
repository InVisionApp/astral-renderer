/*!
 * \file stroke_query_implement.hpp
 * \brief file stroke_query_implement.hpp
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

#ifndef ASTRAL_STROKE_QUERY_IMPLEMENT_HPP
#define ASTRAL_STROKE_QUERY_IMPLEMENT_HPP

#include <astral/renderer/shader/stroke_query.hpp>
#include <astral/util/memory_pool.hpp>
#include <astral/util/object_pool.hpp>

#include "stroke_data_hierarchy.hpp"

class astral::StrokeQuery::Implement:public astral::StrokeQuery
{
public:
  Implement(void);
  ~Implement();

  void
  begin_query_implement(const ScaleTranslate &rect_transformation_elements,
                        const ivec2 &rect_size, bool sparse_query,
                        c_array<const BoundingBox<float>> restrict_rects);

  void
  end_query_implement(unsigned int max_rect_size);

  void
  add_element_implement(unsigned int ID,
                        const Transformation &element_transformation_stroking,
                        const Transformation &stroking_transformation_path,
                        const StrokeShader::CookedData &path,
                        float animation_t,
                        ActivePrimitives active_primitives,
                        StrokeRadii stroke_radii);

  c_array<const ResultRect>
  elements_implement(void) const
  {
    ASTRALassert(m_mode == mode_query_ended);
    return make_c_array(m_result_elements);
  }

  c_array<const uvec2>
  empty_tiles_implement(void) const
  {
    ASTRALassert(m_mode == mode_query_ended);
    return make_c_array(m_empty_tiles);
  }

  bool
  is_sparse_implement(void) const
  {
    ASTRALassert(m_mode == mode_query_ended);
    return elements().size() + empty_tiles().size() > 1u;
  }

  ivec2
  end_elementary_rect_size_implement(void) const
  {
    return m_end_elementary_rect_size;
  }

  ivec2
  number_elementary_rects_implement(void) const
  {
    return m_number_elementary_rects;
  }

  void
  clear_implement(void);

private:
  friend class StrokeQuery;

  class QueryElement;
  class QueryElementDetailed;
  class RectHierarchy;
  class RectHierarchyPool;

  enum mode_t
    {
      mode_no_query,
      mode_adding_elements,
      mode_empty_rect_adding_element,
      mode_query_ended
    };

  enum elementary_rect_lit_t
    {
       elementary_rect_unlit, /* elementary rect is not yet lit */
       elementary_rect_cannot_be_lit, /* elementary rect cannot be lit because it is not in the restricted list */
       elementary_rect_lit /* elementary rect is lit, i.e. hit by a curve */
    };

  /* Computes the bounding boxes of the elements hit
   * lights what boxes that are hit; to be called on
   * each element of the query
   */
  void
  light_elementary_rects_of_query_element(const detail::StrokeDataHierarchy::Base &base,
                                          const QueryElementDetailed &q);

  /* Creates a rect-heirarchy whose leaves are merged
   * rectangle lit by compute_lit_elementary_rects().
   * \param max_size maximum size allowed for each resulting QueryElement
   */
  void
  create_rect_hierarchy(unsigned int max_size);

  ResultRect*
  create_query_result(ivec2 pixel_rect);

  /* compute the ranges into [0, m_number_full_pixel.x())x[0, m_number_full_pixel.y())
   * that a rect hits.
   */
  void
  compute_elementary_rect_hits(Rect rect,
                               range_type<int> *out_x_range,
                               range_type<int> *out_y_range);

  int
  compute_elementary_rect_id(ivec2 elementary_rect)
  {
    ASTRALassert(elementary_rect.x() >= 0);
    ASTRALassert(elementary_rect.y() >= 0);
    ASTRALassert(elementary_rect.x() < m_number_elementary_rects.x());
    ASTRALassert(elementary_rect.y() < m_number_elementary_rects.y());
    return elementary_rect.x() + elementary_rect.y() * m_number_elementary_rects.x();
  }

  ivec2
  compute_rect_from_id(int rect_id)
  {
    ivec2 return_value;

    ASTRALassert(rect_id >= 0);
    ASTRALassert(rect_id < m_number_elementary_rects.x() * m_number_elementary_rects.y());
    return_value.x() = rect_id % m_number_elementary_rects.x();
    return_value.y() = rect_id / m_number_elementary_rects.x();

    return return_value;
  }

  void
  mark_rects_as_unlit(c_array<const BoundingBox<float>> restrict_rects);

  enum mode_t m_mode;
  Transformation m_rect_transformation_elements;
  bool m_sparse_query;
  std::vector<QueryElement> m_query_elements;

  /* the results of the last query */
  std::vector<ResultRect> m_result_elements;
  std::vector<uvec2> m_empty_tiles;

  /* Query is a three phase algorithm.
   *
   * We have the elementary rects, these rects break
   * the region into (roughly) equally sized rectangles.
   * The first phase determines which of these elementary
   * rects are hit by the stroke, implemented by
   * light_elementary_rects() which calls for each query
   * element light_elementary_rects_of_query_element().
   *
   * The second phase then merges these elementary rects
   * together into larger rects in a hierarchical fashion.
   * The basic gist is that the region is divided into halves
   * recursively both horizontally and vertically. If two
   * halves are lit, then rather than having those two rects,
   * we have a single rect of them merged. Then this proceeds
   * recursively up the hierarchy. At the end, each merged
   * generates a unique ResultRect value which are
   * backed by m_result_elements. This is implemented by
   * create_rect_hierarchy().
   *
   * The third phase is to compute, in an efficient manner
   * what from the stroke lands in each of the merged-rects,
   * the hits for a merged rect are ResultRect::m_sources,
   * implemented by add_hits_of_query_element().
   */

  /* the number of pixel rects of the last query */
  ivec2 m_number_elementary_rects;

  float m_reciprocal_elementary_rect_size;

  /* the width/height of the pixel rects in the
   * last row/column
   */
  ivec2 m_end_elementary_rect_size;

  /* The output for phase 1; an elementary rect R = (x,y)
   * is in the list m_lit_elementary_rect_list exactly once
   * and the array m_elementary_rects_is_lit is the tracking
   * to make sure a rect is not added more than once.
   */
  std::vector<enum elementary_rect_lit_t> m_elementary_rects_is_lit;
  std::vector<ivec2> m_lit_elementary_rect_list;

  /* the rect hierarchy built from m_lit_elementary_rect_list,
   * the backing memory is allocated by m_pool.
   */
  RectHierarchy *m_rect_hierarchy;

  /* this is work room needed for the third phase so
   * that attribute and index data is added only once
   * to each merged-rectangle.
   */
  std::vector<bool> m_workroom_is_lit;
  std::vector<unsigned int> m_workroom_lit_list;

  /* work room to cache the bounding box values */
  std::vector<vecN<range_type<int>, 2>> m_workroom_boxes;

  /* backing for Source array values to avoid malloc/free noise */
  ObjectPoolClear<std::vector<Source>> m_query_src_pool;

  /* arrays for Source::m_idx */
  ObjectPoolClear<std::vector<range_type<int>>> m_ids_backing;

  /* pool for rect-hierarchy objects, needed for phase 2 */
  RectHierarchyPool *m_pool;
};

class astral::StrokeQuery::Implement::QueryElement
{
public:
  unsigned int m_client_id;
  unsigned int m_box_location;
  StrokeQuery::ActivePrimitives m_active_primitives;
  reference_counted_ptr<detail::StrokeDataHierarchy> m_hierarchy;
};

class astral::StrokeQuery::Implement::QueryElementDetailed:public QueryElement
{
public:
  StrokeRadii m_stroke_radii;
  float m_animation_t;
  const Transformation *m_rect_transformation_stroking;
  const Transformation *m_stroking_transformation_path;
};

class astral::StrokeQuery::Implement::RectHierarchy
{
public:
  RectHierarchy(range_type<int> x, range_type<int> y);

  void
  insert(RectHierarchyPool &pool, ivec2 R);

  void
  merge(unsigned int max_rect_size);

  /* may only be called after merge() is called */
  unsigned int
  count(void) const;

  bool
  is_lit_leaf(void) const
  {
    return m_splitting_coordinate == lit_leaf;
  }

  bool
  is_node(void) const
  {
    return m_splitting_coordinate < 2;
  }

  void
  create_result_elements(StrokeQuery::Implement &qr, std::vector<ResultRect> *dst);

  void
  add_sources(StrokeQuery::Implement &caller,
              const QueryElement &q,
              const detail::StrokeDataHierarchy::Base &base);

private:
  enum
    {
      node_x_splits = 0,
      node_y_splits = 1,
      unlit_leaf = 2,
      lit_leaf = 3,
    };

  bool
  can_merge_children(unsigned int max_rect_size);

  void
  add_sources_lit_leaf(StrokeQuery::Implement &caller,
                       const QueryElement &q,
                       const detail::StrokeDataHierarchy::Base &base);

  vecN<range_type<int>, 2> m_range;
  vecN<RectHierarchy*, 2> m_children;

  /* a m_splitting_coordinate >= 2 means that
   * the element is a leaf and describes if the
   * leaf is lit or not.
   */
  int m_splitting_coordinate, m_splitting_value;
  bool m_has_content;

  /* */
  ResultRect *m_query_result;
};

class astral::StrokeQuery::Implement::RectHierarchyPool:public MemoryPool<RectHierarchy, 512>
{
public:
  ~RectHierarchyPool()
  {
    clear();
  }
};

#endif

/*!
 * \file renderer_stc_data.hpp
 * \brief file renderer_stc_data.hpp
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

#ifndef ASTRAL_RENDERER_STC_DATA_HPP
#define ASTRAL_RENDERER_STC_DATA_HPP

#include <astral/renderer/renderer.hpp>
#include "renderer_implement.hpp"
#include "renderer_cached_transformation.hpp"

/* Represents the data to feed to a single pass
 * of STC filling
 */
class astral::Renderer::Implement::STCData
{
public:
  /* Each range added to an STCData can have an optional
   * bounding box speciyign what region in pixel coordinates
   * the draw of that range covers.
   */
  class OptionalBoundingBox
  {
  public:
    OptionalBoundingBox(const BoundingBox<float> *bb):
      m_provided(bb != nullptr)
    {
      if (bb)
        {
          m_bb = *bb;
        }
    }

    BoundingBox<float> m_bb;
    bool m_provided;
  };

  /* Backing data for sub-elements of STCData; a subelement is
   * essentially a range of vertices and an optional bounding
   * box. An SubSTCDataBacking is to be shared across multiple
   * STCData objects; the motivation is to prevent having many
   * tiny arrays that are created for each STCData.
   */
  class SubSTCDataBacking
  {
  public:
    void
    clear(void)
    {
      m_ranges.clear();
      m_optional_bbs.clear();
    }

    bool
    empty(void) const
    {
      ASTRALassert(m_ranges.size() == m_optional_bbs.size());
      return m_ranges.empty();
    }

    void
    add_range(const range_type<int> &R, const OptionalBoundingBox &bb)
    {
      ASTRALassert(m_ranges.size() == m_optional_bbs.size());

      m_ranges.push_back(R);
      m_optional_bbs.push_back(bb);
    }

    unsigned int
    size(void) const
    {
      ASTRALassert(m_ranges.size() == m_optional_bbs.size());
      return m_ranges.size();
    }

    c_array<const range_type<int>>
    fetch(range_type<unsigned int> sub) const
    {
      c_array<const range_type<int>> R;

      R = make_c_array(m_ranges);
      return R.sub_array(sub);
    }

    const range_type<int>&
    range(unsigned int I) const
    {
      ASTRALassert(I < size());
      return m_ranges[I];
    }

    const OptionalBoundingBox&
    optional_bb(unsigned int I) const
    {
      ASTRALassert(I < size());
      return m_optional_bbs[I];
    }

    range_type<int>&
    range(unsigned int I)
    {
      ASTRALassert(I < size());
      return m_ranges[I];
    }

    OptionalBoundingBox&
    optional_bb(unsigned int I)
    {
      ASTRALassert(I < size());
      return m_optional_bbs[I];
    }

  private:
    /* ranges into VertexDataBacking of vertices that are used */
    std::vector<range_type<int>> m_ranges;

    /* for each entry in m_ranges, an optional bounding box */
    std::vector<OptionalBoundingBox> m_optional_bbs;
  };

  /* Class that builds STCData, essentially a staging buffer */
  class Builder;

  /* A set of Builder objects, one for each pass in FillShader */
  class BuilderSet;

  /* Class that wraps over a range of an std::vector<STCData>.
   * One KEY point is that the range of a VirtualArray can only
   * be shrunk AND and that a given range is referenced by
   * exactly one VirtualArray
   */
  class VirtualArray
  {
  public:
    VirtualArray(unsigned int b, unsigned int e):
      m_range(b, e)
    {
      ASTRALassert(b <= e);
    }

    VirtualArray(void):
      m_range(0, 0)
    {}

    unsigned int
    size(void) const
    {
      ASTRALassert(m_range.m_end >= m_range.m_begin);
      return m_range.m_end - m_range.m_begin;
    }

    STCData&
    element(unsigned int I, std::vector<STCData> &backing) const
    {
      ASTRALassert(m_range.m_end <= backing.size());
      ASTRALassert(m_range.m_begin < m_range.m_end);

      I += m_range.m_begin;
      ASTRALassert(I < m_range.m_end);

      return backing[I];
    }

    const STCData&
    element(unsigned int I, const std::vector<STCData> &backing) const
    {
      ASTRALassert(m_range.m_end <= backing.size());
      ASTRALassert(m_range.m_begin < m_range.m_end);

      I += m_range.m_begin;
      ASTRALassert(I < m_range.m_end);

      return backing[I];
    }

    STCData&
    front(std::vector<STCData> &backing) const
    {
      ASTRALassert(m_range.m_end <= backing.size());
      ASTRALassert(m_range.m_begin < m_range.m_end);

      return backing[m_range.m_begin];
    }

    const STCData&
    front(const std::vector<STCData> &backing) const
    {
      ASTRALassert(m_range.m_end <= backing.size());
      ASTRALassert(m_range.m_begin < m_range.m_end);

      return backing[m_range.m_begin];
    }

    STCData&
    back(std::vector<STCData> &backing) const
    {
      ASTRALassert(m_range.m_end <= backing.size());
      ASTRALassert(m_range.m_begin < m_range.m_end);

      return backing[m_range.m_end - 1u];
    }

    const STCData&
    back(const std::vector<STCData> &backing) const
    {
      ASTRALassert(m_range.m_end <= backing.size());
      ASTRALassert(m_range.m_begin < m_range.m_end);

      return backing[m_range.m_end - 1u];
    }

    void
    pop_back(void)
    {
      ASTRALassert(m_range.m_end > m_range.m_begin);
      --m_range.m_end;
    }

    void
    pop_front(void)
    {
      ASTRALassert(m_range.m_end > m_range.m_begin);
      ++m_range.m_begin;
    }

    bool
    empty(void) const
    {
      return m_range.m_begin == m_range.m_end;
    }

    c_array<const STCData>
    values(const std::vector<STCData> &backing) const
    {
      c_array<const STCData> R;

      R = make_c_array(backing);
      return R.sub_array(m_range);
    }

  private:
    range_type<unsigned int> m_range;
  };

  /* Provides a std::vector<STCData> and SubSTCDataBacking for each
   * pass in FillSTCShader.
   */
  class DataSet:astral::noncopyable
  {
  public:
    void
    clear(void)
    {
      for (unsigned int i = 0; i < FillSTCShader::pass_count; ++i)
        {
          m_stc_data[i].clear();
          m_stc_subelement_backing[i].clear();
        }
    }

    vecN<std::vector<STCData>, FillSTCShader::pass_count> m_stc_data;
    vecN<SubSTCDataBacking, FillSTCShader::pass_count> m_stc_subelement_backing;
  };

  /* \param tr What transformation applied to all items drawn through this STCData
   * \param im what ItemData applied to all items drawn through this STCData
   * \param subelement_backing backing of the sub-element of this STCData object. It is
   *                           required that this STCData is then the only object that
   *                           is writing this array until no more ranges are added
   *                           to this STCData. STCData assumes that the values it adds
   *                           are [A, B) where A is the size of the array at STCData
   *                           ctor time and B is the size after the last call to
   *                           add_range() to this STCData.
   */
  STCData(RenderValue<Transformation> tr, ItemData im, SubSTCDataBacking *subelement_backing):
    m_subelement_backing(subelement_backing),
    m_transformation(tr),
    m_item_data(im)
  {
    ASTRALassert(tr.valid());
    ASTRALassert(im.valid());
    ASTRALassert(m_subelement_backing);

    m_sub.m_begin = m_sub.m_end = m_subelement_backing->size();
  }

  /* Add a range of vertice to draw with transformation() and
   * item_data() where the range is a range into the VertexBacking
   */
  void
  add_range(const range_type<int> &R, const OptionalBoundingBox &bb)
  {
    ASTRALassert(m_subelement_backing->size() == m_sub.m_end);
    m_subelement_backing->add_range(R, bb);
    ++m_sub.m_end;
  }

  void
  add_range(const range_type<int> &R)
  {
    add_range(R, OptionalBoundingBox(nullptr));
  }

  c_array<const range_type<int>>
  ranges(void) const
  {
    return m_subelement_backing->fetch(m_sub);
  }

  RenderValue<Transformation>
  transformation(void) const
  {
    return m_transformation;
  }

  ItemData
  item_data(void) const
  {
    return m_item_data;
  }

  bool
  empty(void) const
  {
    ASTRALassert(m_sub.m_begin <= m_sub.m_end);
    return m_sub.m_begin == m_sub.m_end;
  }

  /* Add commands that intersect a bounding box from another STCData.
   * Commands are always added via push_back(), so to get the start
   * of a command stream added is done by querying the size() before
   * adding and to get the end, query the size() after the command.
   * \param stc_data_backing array to which to add STCData; this array
   *                         also backs src_stc
   * \param stc_backing the STCData::Backing backing object to use for
   *                    the STCData added to stc_data_backing
   * \param src_stc range into stc_data_backing from which to copy
   * \param bb query region
   * \param delete_contained if true, delete from src_stc any commands
   *                         completely contained by bb
   */
  static
  void
  copy_stc(std::vector<STCData> *stc_data_backing, SubSTCDataBacking *stc_backing,
           VirtualArray &src_stc, const BoundingBox<float> &bb, bool delete_contained);

  /* Copy commands that intersect a bounding box from another STCData
   * \param dst array of STCData to whcih to add; an entry is only added
   *            there were commands that intersected
   * \param dst_backing the STCData::Backing backing object to use for
   *                    the STCData added to dst
   * \param src_stc STCData from which to copy
   * \param bb query region
   * \param delete_contained if true, delete from src_stc any commands
   *                         completely contained by bb
   */
  static
  void
  copy_stc(std::vector<STCData> *dst, SubSTCDataBacking *dst_backing,
           STCData &src_stc, const BoundingBox<float> &bb, bool delete_contained);

private:
  /* backing to store ranges into VertexDataAllocator of vertices that
   * are used along with the bounding boxes of each range.
   */
  SubSTCDataBacking *m_subelement_backing;

  /* range into m_range_backings that this STCData is */
  range_type<unsigned int> m_sub;

  /* transformation from coordinates of path to pixel coordinates */
  RenderValue<Transformation> m_transformation;

  /* item data for the shader */
  ItemData m_item_data;
};

/* The interface for VirtualBuffer for STCData is that
 * instead of having its own std::vector<STCData>, it
 * stores a range into a common std::vector<STCData> which
 * is managed by Renderer::Implement::Storage. The upshot is that
 * the STCData of a VirtualBuffer can only be set, not
 * appended. However, we wish to have the ability to
 * append (needed strongly for the various sparse filling
 * implementation). To that end, a Builder servers as a
 * scratch space to append STCData. When all that is
 * needed is added, then Builder::end() is to be issued
 * that copies the data inside of the Builder to a backing.
 */
class astral::Renderer::Implement::STCData::Builder:astral::noncopyable
{
public:
  /*!
   * Beging a session for the staging buffer
   */
  void
  start(void)
  {
    ASTRALassert(m_stc_data.empty());
    ASTRALassert(m_subelement_backing.empty());
  }

  /*!
   * Add a new STCData object to the staging buffer
   */
  void
  add_stc(RenderValue<Transformation> tr, ItemData im)
  {
    m_stc_data.push_back(STCData(tr, im, &m_subelement_backing));
  }

  /*!
   * Add a range to the last STCData object added via add_stc().
   */
  void
  add_range(const range_type<int> &R, const BoundingBox<float> *bb = nullptr)
  {
    ASTRALassert(!m_stc_data.empty());
    m_stc_data.back().add_range(R, bb);
  }

  /*!
   * If add_stc() has not been called or if the transformation or
   * item_data do not match with the last add_stc(), calls add_stc()
   * then calls add_range().
   */
  void
  add_range(const VertexData *vertex_data,
            range_type<int> range,
            RenderValue<Transformation> tr,
            ItemData item_data,
            const BoundingBox<float> *bb = nullptr)
  {
    if (vertex_data && range.m_begin < range.m_end)
      {
        range_type<int> R(vertex_data->vertex_range().m_begin + range.m_begin,
                          vertex_data->vertex_range().m_begin + range.m_end);

        if (m_stc_data.empty()
            || m_stc_data.back().transformation() != tr
            || m_stc_data.back().item_data() != item_data)
          {
            add_stc(tr, item_data);
          }

        add_range(R, bb);
      }
  }

  /*!
   * End the current session of the staging buffer and copy the values
   * out. Afterwards, clear the staging buffer.
   * \param dst location to which to copy values
   * \param dst_backing the SubSTCDataBacking used by elements of dst
   */
  VirtualArray
  end(std::vector<STCData> *dst, SubSTCDataBacking *dst_backing)
  {
    unsigned int begin, end;

    begin = dst->size();
    for (const STCData &stc : m_stc_data)
      {
        ASTRALassert(stc.m_subelement_backing == &m_subelement_backing);

        if (stc.m_sub.m_begin < stc.m_sub.m_end)
          {
            dst->push_back(STCData(stc.transformation(), stc.item_data(), dst_backing));
            for (unsigned int b = stc.m_sub.m_begin; b < stc.m_sub.m_end; ++b)
              {
                dst->back().add_range(m_subelement_backing.range(b), m_subelement_backing.optional_bb(b));
              }
          }
      }
    end = dst->size();

    m_stc_data.clear();
    m_subelement_backing.clear();

    return VirtualArray(begin, end);
  }

  /* Abort the current session without copying the data to a backer
   * for STCData; i.e. just clear the staging buffer.
   */
  void
  clear(void)
  {
    m_stc_data.clear();
    m_subelement_backing.clear();
  }

private:
  std::vector<STCData> m_stc_data;
  SubSTCDataBacking m_subelement_backing;
};

/* Essentially, one Builder per pass in a FillSTCShader.
 * Allows for higher level operations such as adding
 * a path or contour.
 */
class astral::Renderer::Implement::STCData::BuilderSet:astral::noncopyable
{
public:
  class Helper;

  /* Call Builder::start() for each pass */
  void
  start(void)
  {
    for (unsigned int i = 0; i < FillSTCShader::pass_count; ++i)
      {
        m_builders[i].start();
      }
  }

  /* Add STCData for the named pass */
  void
  add_stc_pass(enum FillSTCShader::pass_t pass,
               const VertexData *vertex_data,
               range_type<int> range,
               RenderValue<Transformation> tr,
               ItemData item_data,
               const BoundingBox<float> *bb = nullptr)
  {
    m_builders[pass].add_range(vertex_data, range, tr, item_data, bb);
  }

  /* calls add_stc_pass() from a FillSTCShader::CookedData
   * for each pass. Anti-aliasing passes are skipped if
   * the anti-aliasing mode indicates to not have
   * anti-aliasing
   */
  void
  add_stc(const FillSTCShader::CookedData &stc_data,
          enum anti_alias_t aa_mode,
          RenderValue<Transformation> transformation,
          ItemData item_data,
          const BoundingBox<float> *bb = nullptr)
  {
    if (aa_mode == with_anti_aliasing)
      {
        add_stc_pass(FillSTCShader::pass_contour_fuzz,
                     stc_data.m_vertex_data.get(),
                     stc_data.m_pass_range[FillSTCShader::pass_contour_fuzz],
                     transformation,
                     item_data, bb);

        add_stc_pass(FillSTCShader::pass_conic_triangle_fuzz,
                     stc_data.m_vertex_data.get(),
                     stc_data.m_pass_range[FillSTCShader::pass_conic_triangle_fuzz],
                     transformation,
                     item_data, bb);
      }

    add_stc_pass(FillSTCShader::pass_contour_stencil,
                 stc_data.m_vertex_data.get(),
                 stc_data.m_pass_range[FillSTCShader::pass_contour_stencil],
                 transformation,
                 item_data, bb);

    add_stc_pass(FillSTCShader::pass_conic_triangles_stencil,
                 stc_data.m_vertex_data.get(),
                 stc_data.m_pass_range[FillSTCShader::pass_conic_triangles_stencil],
                 transformation,
                 item_data, bb);
  }

  /* Call Builder::end() on each pass and return their results
   * into a single array, i.e. copy the data from the staging
   * buffers to the backing storage and clear the staging buffer.
   */
  vecN<VirtualArray, FillSTCShader::pass_count>
  end(DataSet *dst)
  {
    vecN<VirtualArray, FillSTCShader::pass_count> return_value;
    for (unsigned int i = 0; i < FillSTCShader::pass_count; ++i)
      {
        return_value[i] = m_builders[i].end(&dst->m_stc_data[i], &dst->m_stc_subelement_backing[i]);
      }
    return return_value;
  }

  /* Issue clear for each Builder, i.e. clear the staging buffer. */
  void
  clear(void)
  {
    for (unsigned int i = 0; i < FillSTCShader::pass_count; ++i)
      {
        m_builders[i].clear();
      }
  }

private:
  vecN<Builder, FillSTCShader::pass_count> m_builders;
};

#endif

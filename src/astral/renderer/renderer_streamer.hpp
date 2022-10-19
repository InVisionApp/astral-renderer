/*!
 * \file renderer_streamer.hpp
 * \brief file renderer_streamer.hpp
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

#ifndef ASTRAL_RENDERER_STREAMER_HPP
#define ASTRAL_RENDERER_STREAMER_HPP

#include <astral/renderer/renderer.hpp>

/* Some C++ silliness to avoid foward declaring a template
 * member type in a public header.
 */
class astral::Renderer::Implement::Streamer
{
public:
  template<typename T> class Type;
};

/* Template class that implements VertexStreamer and StaticStreamer32;
 * the use case is for streaming VertexData and static data values
 * per-frame.
 */
template<typename StreamerBlockType> // VertexStreamerBlock or StaticDataStreamerBlock
class astral::Renderer::Implement::Streamer::Type
{
public:
  typedef typename StreamerBlockType::ValueType ValueType;
  typedef typename StreamerBlockType::ObjectType ObjectType;
  typedef typename StreamerBlockType::StreamerSizeType StreamerSizeType;
  typedef typename StreamerBlockType::StreamerValuesType StreamerValuesType;

  explicit
  Type(RenderEngine &engine, unsigned int number_values_per_object):
    m_number_values_per_object(number_values_per_object)
  {
    m_pool.push_back(ASTRALnew PerObject(engine, m_number_values_per_object));
  }

  /*!
   * Begin the streaming
   */
  void
  begin(void)
  {
    m_current_object = 0u;
    m_number_streamed = 0u;
    m_tmp.clear();
    m_pool[0]->begin();
  }

  /*!
   * Request to stream a given number of ValueType
   * values potentially across multiple ObjectType
   * objects. Return a range that is to be fed to
   * blocks() to get the array of StreamerBlockType
   * to write to.
   * \param engine RenderEngine used to create additional
   *               ObjectType objects if necessary
   * \param number_values number of ValueType values to
   *                      stream
   * \param block_size for each StreamerBlockType E returned
   *                   by blocks() when passed the returned
   *                   range, it is guaranteed that
   *                   E.m_dst.size() % block_size is 0.
   */
  range_type<unsigned int>
  request_blocks(RenderEngine &engine,
                 unsigned int number_values,
                 unsigned int block_size)
  {
    unsigned int start(m_tmp.size());

    m_number_streamed += number_values;
    while (number_values > 0)
      {
        StreamerBlockType E;

        ASTRALassert(m_current_object < m_pool.size());
        E = m_pool[m_current_object]->allocate(number_values, block_size);

        ASTRALassert(number_values >= E.m_dst.size());
        number_values -= E.m_dst.size();

        if (!E.m_dst.empty())
          {
            m_tmp.push_back(E);
            ASTRALassert(E.m_dst.size() % block_size == 0u);
          }

        if (number_values > 0u)
          {
            ++m_current_object;
            if (m_current_object >= m_pool.size())
              {
                m_pool.push_back(ASTRALnew PerObject(engine, m_number_values_per_object));
              }
            ASTRALassert(m_current_object < m_pool.size());
            m_pool[m_current_object]->begin();
          }
      }

    return range_type<unsigned int>(start, m_tmp.size());
  }

  /*!
   * Given a return value from request_block(), get the
   * c_array of the blocks. Note: the returned c_array
   * is only guaranteed to be valid until the next call
   * to request_blocks().
   */
  c_array<const StreamerBlockType>
  blocks(range_type<unsigned int> R)
  {
    return make_c_array(m_tmp).sub_array(R);
  }

  /*!
   * Template friendly version of blocks() handling
   * a sequence of ranges.
   */
  template<size_t N>
  vecN<c_array<const StreamerBlockType>, N>
  blocks(const vecN<range_type<unsigned int>, N> &R)
  {
    vecN<c_array<const StreamerBlockType>, N> return_value;
    for (unsigned int i = 0; i < N; ++i)
      {
        return_value[i] = blocks(R[i]);
      }
    return return_value;
  }

  /*!
   * Signals that all data has been written and ready
   * for streaming.
   */
  unsigned int
  end(void)
  {
    for (unsigned int i = 0; i <= m_current_object; ++i)
      {
        m_pool[i]->end();
      }
    return m_number_streamed;
  }

  /*!
   * Abort any data to be streamed.
   */
  void
  end_abort(void)
  {
    /* nothing to really do, just good sanitation to have this method */
  }

private:
  class PerObject:
    public reference_counted<PerObject>::non_concurrent
  {
  public:
    PerObject(RenderEngine &engine,
              unsigned int number_values_per_object):
      m_cpu_backing(number_values_per_object)
    {
      StreamerSizeType sz(number_values_per_object);
      m_object = engine.create_streamer(sz);
    }

    void
    begin(void)
    {
      m_cpu_data = make_c_array(m_cpu_backing);
      m_allocated = 0u;
    }

    StreamerBlockType
    allocate(unsigned int cnt, unsigned int block_size)
    {
      StreamerBlockType return_value;
      unsigned int max_size(m_cpu_data.size());

      ASTRALassert(cnt % block_size == 0u);
      max_size -= max_size % block_size;
      cnt = t_min(cnt, max_size);

      return_value.m_dst = m_cpu_data.sub_array(0, cnt);
      return_value.m_object = m_object.get();
      return_value.m_offset = m_allocated;

      m_cpu_data = m_cpu_data.sub_array(cnt);
      m_allocated += cnt;

      return return_value;
    }

    void
    end(void)
    {
      StreamerValuesType S(make_c_array(m_cpu_backing).sub_array(0, m_allocated));
      m_object->set_values_for_streaming(S);
    }

  private:
    reference_counted_ptr<ObjectType> m_object;
    std::vector<ValueType> m_cpu_backing;
    c_array<ValueType> m_cpu_data;
    unsigned int m_allocated;
  };

  /* pool of mutable objects made with
   * RenderEngine::create_streamer()
   */
  std::vector<reference_counted_ptr<PerObject>> m_pool;

  /* the number of values in each Object of m_pool */
  unsigned int m_number_values_per_object;

  /* Index into m_pool of the current object to add add to. */
  unsigned int m_current_object;

  /* The total number of vertices streamed since begin() */
  unsigned int m_number_streamed;

  /* the backing for the arrays returned by request_blocks() */
  std::vector<StreamerBlockType> m_tmp;
};

class astral::Renderer::Implement::VertexStreamer:
  public reference_counted<VertexStreamer>::non_concurrent
{
public:
  explicit
  VertexStreamer(RenderEngine &engine, unsigned int number_verts_per_object):
    m_streamer(engine, number_verts_per_object)
  {
    ASTRALassert(number_verts_per_object % 3u == 0u);
  }

  void
  begin(void)
  {
    m_streamer.begin();
  }

  unsigned int
  end(void)
  {
    return m_streamer.end();
  }

  /*!
   * Abort any data to be streamed.
   */
  void
  end_abort(void)
  {
    m_streamer.end_abort();
  }

  range_type<unsigned int>
  request_blocks(RenderEngine &engine,
                 unsigned int number_values)
  {
    /* It is -critical- that vertex data is in blocks of
     * size of multiple of 3 because a single triangle must
     * have all of its vertices from a single VertexData,
     * this is why the block_size is 3.
     */
    ASTRALassert(number_values % 3u == 0u);
    return m_streamer.request_blocks(engine, number_values, 3u);
  }

  c_array<const VertexStreamerBlock>
  blocks(range_type<unsigned int> R)
  {
    return m_streamer.blocks(R);
  }

  template<size_t N>
  vecN<c_array<const VertexStreamerBlock>, N>
  blocks(const vecN<range_type<unsigned int>, N> &R)
  {
    return m_streamer.blocks(R);
  }

private:
  Streamer::Type<VertexStreamerBlock> m_streamer;
};

class astral::Renderer::Implement::StaticStreamer32:
  public reference_counted<StaticStreamer32>::non_concurrent,
  public Streamer::Type<StaticDataStreamerBlock<StaticDataBacking::type32, gvec4>>
{
public:
  explicit
  StaticStreamer32(RenderEngine &engine, unsigned int number_gvec4_per_object):
    Streamer::Type<StaticDataStreamerBlock<StaticDataBacking::type32, gvec4>>(engine, number_gvec4_per_object)
  {
  }
};

class astral::Renderer::Implement::StaticStreamer16:
  public reference_counted<StaticStreamer32>::non_concurrent,
  public Streamer::Type<StaticDataStreamerBlock<StaticDataBacking::type16, u16vec4>>
{
public:
  explicit
  StaticStreamer16(RenderEngine &engine, unsigned int number_gvec4_per_object):
    Streamer::Type<StaticDataStreamerBlock<StaticDataBacking::type16, u16vec4>>(engine, number_gvec4_per_object)
  {
  }
};

#endif

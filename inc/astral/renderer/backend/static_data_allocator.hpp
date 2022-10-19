/*!
 * \file static_data_allocator.hpp
 * \brief file static_data_allocator.hpp
 *
 * Copyright 2019 by InvisionApp.
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

#ifndef ASTRAL_STATIC_DATA_ALLOCATOR_HPP
#define ASTRAL_STATIC_DATA_ALLOCATOR_HPP

#include <astral/renderer/backend/static_data_backing.hpp>
#include <astral/renderer/backend/static_data_details.hpp>

namespace astral
{
/*!\addtogroup RendererBackend
 * @{
 */
  class StaticDataAllocator16;
  class StaticDataAllocator32;

  /*!
   * \brief
   * An astral::StaticDataAllocatorCommon is the a common base class for
   * astral::StaticDataAllocator16 and astral::StaticDataAllocator32
   */
  class StaticDataAllocatorCommon:public reference_counted<StaticDataAllocatorCommon>::non_concurrent
  {
  public:
    ~StaticDataAllocatorCommon();

    /*!
     * Returns the astral::StaticDataBacking used by this astral::StaticDataAllocator
     */
    const StaticDataBacking&
    backing(void) const
    {
      return *m_backing;
    }

    /*!
     * During a lock_resources()/unlock_resources() pair,
     * rather than freeing the regions of deleted
     * astral::StaticData objects directly, the regions
     * are only marked to be free and will be released on
     * unlock_resources(). The use case is that during a
     * Renderer::begin() / Renderer::end() pair, an
     * astral::StaticData that was used gets deleted
     * before Renderer::end() is called. Such deletion
     * would then free the area it occupies in the atlas.
     * By calling lock_resources(), the area used by a deleted
     * astral::StaticData is not marked for reuse until
     * unlock_resources() is called.
     *
     * Nesting is supported (though ill-advised) and resources
     * are cleared on the call to the outer-most unlock_resources().
     */
    void
    lock_resources(void);

    /*!
     * Release the regions marked for deletion since
     * lock_resources() was called.
     */
    void
    unlock_resources(void);

    /*!
     * Returns how many gvec4 values are allocated.
     */
    unsigned int
    amount_allocated(void)
    {
      return m_amount_allocated;
    }

    ///@cond
    template<enum astral::StaticDataBacking::type_t type>
    reference_counted_ptr<StaticData>
    create_streamer(detail::StaticDataStreamerSize<type> V)
    {
      ASTRALassert(type == m_backing->type());
      return create_streamer_implement(V.m_size);
    }
    ///@endcond

  private:
    friend class StaticData;
    friend class StaticDataAllocator16;
    friend class StaticDataAllocator32;

    class MemoryPool;

    explicit
    StaticDataAllocatorCommon(StaticDataBacking &backing);

    reference_counted_ptr<const StaticData>
    create_implement(c_array<const u32vec4> data);

    reference_counted_ptr<const StaticData>
    create_implement(c_array<const u16vec4> data);

    const IntervalAllocator::Interval*
    allocate_data(c_array<const u32vec4> data);

    const IntervalAllocator::Interval*
    allocate_data(c_array<const u16vec4> data);

    const IntervalAllocator::Interval*
    allocate_data(int size);

    void
    free_data(const IntervalAllocator::Interval*);

    reference_counted_ptr<StaticData>
    create_streamer_implement(int size);

    reference_counted_ptr<StaticDataBacking> m_backing;
    IntervalAllocator m_interval_allocator;
    std::vector<const IntervalAllocator::Interval*> m_delayed_frees;
    int m_resources_locked;
    unsigned int m_amount_allocated;
    MemoryPool *m_pool;
  };

  /*!
   * An astral::StaticDataAllocator32 is used to create
   * astral::StaticData objects that hold four tuples
   * of 32-bit data.
   */
  class StaticDataAllocator32:public StaticDataAllocatorCommon
  {
  public:
    /*!
     * Ctor.
     * \param backing of the created astral::StaticDataBacking32 that will back the data on the GPU
     */
    static
    reference_counted_ptr<StaticDataAllocator32>
    create(StaticDataBacking &backing)
    {
      ASTRALassert(backing.type() == StaticDataBacking::type32);
      return ASTRALnew StaticDataAllocator32(backing);
    }

    /*!
     * Create a \ref StaticData that holds four tuples of 32-bit data
     *
     * NOTE: it is unsafe to call this outside of a Renderer::begin()/Renderer::end()
     *       pair in environments where the 3D API state is affected by anything
     *       else besides Astral.
     *
     * \param data the data of the created \ref StaticData
     */
    reference_counted_ptr<const StaticData>
    create(c_array<const u32vec4> data)
    {
      return create_implement(data);
    }

    /*!
     * Create a \ref StaticData that holds four tuples of 32-bit data
     *
     * NOTE: it is unsafe to call this outside of a Renderer::begin()/Renderer::end()
     *       pair in environments where the 3D API state is affected by anything
     *       else besides Astral.
     *
     * \param data the data of the created \ref StaticData
     */
    reference_counted_ptr<const StaticData>
    create(c_array<const gvec4> data)
    {
      c_array<const u32vec4> q;

      q = data.reinterpret_pointer<const u32vec4>();
      return create_implement(q);
    }

  private:
    explicit
    StaticDataAllocator32(StaticDataBacking &backing):
      StaticDataAllocatorCommon(backing)
    {}
  };

  /*!
   * An astral::StaticDataAllocator16 is used to create
   * astral::StaticData objects that hold four tuples
   * of 16-bit data, the data can also be viewed as
   * to hold two tuples of 32-bit unsigend integer values.
   */
  class StaticDataAllocator16:public StaticDataAllocatorCommon
  {
  public:
    /*!
     * Ctor.
     * \param backing of the created astral::StaticDataAllocator16 that will back the data on the GPU
     */
    static
    reference_counted_ptr<StaticDataAllocator16>
    create(StaticDataBacking &backing)
    {
      ASTRALassert(backing.type() == StaticDataBacking::type16);
      return ASTRALnew StaticDataAllocator16(backing);
    }

    /*!
     * Create a \ref StaticData that holds four tuples of 16-bit data
     *
     * NOTE: it is unsafe to call this outside of a Renderer::begin()/Renderer::end()
     *       pair in environments where the 3D API state is affected by anything
     *       else besides Astral.
     *
     * \param data the data of the created \ref StaticData
     */
    reference_counted_ptr<const StaticData>
    create(c_array<const u16vec4> data)
    {
      return create_implement(data);
    }

    /*!
     * Create a \ref StaticData that holds two tuples of 32-bit data
     *
     * NOTE: it is unsafe to call this outside of a Renderer::begin()/Renderer::end()
     *       pair in environments where the 3D API state is affected by anything
     *       else besides Astral.
     *
     * \param data the data of the created \ref StaticData
     */
    reference_counted_ptr<const StaticData>
    create(c_array<const u32vec2> data)
    {
      c_array<const u16vec4> q;

      q = data.reinterpret_pointer<const u16vec4>();
      return create_implement(q);
    }

    /*!
     * Create a \ref StaticData that holds four tuples of 16-bit data;
     * the 32-bit floating point values passed are converted to 16-bit
     * floating point values.
     *
     * NOTE: it is unsafe to call this outside of a Renderer::begin()/Renderer::end()
     *       pair in environments where the 3D API state is affected by anything
     *       else besides Astral.
     *
     * \param data the data of the created \ref StaticData
     */
    reference_counted_ptr<const StaticData>
    create(c_array<const vec4> data);

  private:
    explicit
    StaticDataAllocator16(StaticDataBacking &backing):
      StaticDataAllocatorCommon(backing)
    {}

    std::vector<uint16_t> m_workroom;
  };

/*! @} */
}

#endif

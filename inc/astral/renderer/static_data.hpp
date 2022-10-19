/*!
 * \file static_data.hpp
 * \brief file static_data.hpp
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

#ifndef ASTRAL_STATIC_DATA_HPP
#define ASTRAL_STATIC_DATA_HPP

#include <vector>
#include <astral/util/vecN.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/reference_counted.hpp>
#include <astral/util/interval_allocator.hpp>
#include <astral/renderer/backend/static_data_backing.hpp>
#include <astral/renderer/backend/static_data_allocator.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * \brief
   * An astral::StaticData represents data that is immutable
   * and readable by shaders across multiple vertices.
   *
   * The use case is to stash data that is shared by multiple vertices that
   * is statuc and should not be uploaded to the GPU every frame whereas
   * astral::ItemData changes every frame is uploaded every frame. An
   * astral::StaticData is created with StaticDataAllocator::create()
   * where the backing of the astral::StaticDataAllocator determines if
   * the data is generic four channel 32-bit data or four channel fp16 data.
   */
  class StaticData:public reference_counted<StaticData>::non_concurrent_custom_delete
  {
  public:
    /*!
     * The location in the \ref StaticDataBacking where the data
     * resides
     */
    int
    location(void) const
    {
      return m_location ? m_location->range().m_begin : 0;
    }

    /*!
     * how much data this \ref StaticData has
     */
    int
    size(void) const
    {
      return m_location ? m_location->range().difference() : 0;
    }

    /*!
     * Returns the type of data.
     */
    enum StaticDataBacking::type_t
    type(void) const
    {
      return m_allocator->backing().type();
    }

    ///@cond
    static
    void
    delete_object(StaticData *p);

    template<enum astral::StaticDataBacking::type_t ptype, typename T>
    void
    set_values_for_streaming(const detail::StaticDataStreamerValues<ptype, T> &values)
    {
      ASTRALunused(m_for_streaming);
      ASTRALassert(m_for_streaming);
      ASTRALassert(type() == ptype);
      ASTRALassert(values.m_values.size() <= static_cast<unsigned int>(size()));

      m_allocator->m_backing->set_data(location(), values.m_values);
    }
    ///@endcond

  private:
    friend class StaticDataAllocatorCommon;

    StaticData(StaticDataAllocatorCommon &allocator,
               const IntervalAllocator::Interval *location,
               bool for_streaming);
    ~StaticData();

    reference_counted_ptr<StaticDataAllocatorCommon> m_allocator;
    const IntervalAllocator::Interval* m_location;
    bool m_for_streaming;
  };

/*! @} */
}

#endif

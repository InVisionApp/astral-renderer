/*!
 * \file colorstop_sequence.hpp
 * \brief file colorstop_sequence.hpp
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

#ifndef ASTRAL_COLOR_STOP_SEQUENCE_HPP
#define ASTRAL_COLOR_STOP_SEQUENCE_HPP

#include <vector>
#include <map>
#include <algorithm>

#include <astral/util/reference_counted.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/vecN.hpp>
#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/colorstop.hpp>
#include <astral/renderer/backend/colorstop_sequence_atlas.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * \brief
   * A ColorStopSequence represents a sequence of \ref ColorStop
   * values ready for rendering. astral::ColorStopSequence does
   * not HAVE a ctor. To create an astral::ColorStopSequence,
   * use one of the astral::ColorStopSequenceAtlas::create()
   * methods from the class astral::ColorStopSequenceAtlas.
   */
  class ColorStopSequence:public reference_counted<ColorStopSequence>::non_concurrent_custom_delete
  {
  public:
    ~ColorStopSequence();

    /*!
     * Return where in the backing \ref ColorStopSequenceAtlas the
     * color data of this \ref ColorStopSequence is located.
     */
    range_type<int>
    location(void) const
    {
      return m_interval->range();
    }

    /*!
     * Return what layer in the backing \ref ColorStopSequenceAtlas
     * the color data of this \ref ColorStopSequence is located.
     */
    int
    layer(void) const
    {
      return m_interval->layer();
    }

    /*!
     * Returns true if the color stop values are
     * all opaque.
     */
    bool
    opaque(void) const
    {
      return m_opaque;
    }

    /*!
     * Returns the color space of the sequence, interpolation
     * of the gradient is done in this color space.
     */
    enum colorspace_t
    colorspace(void) const
    {
      return m_colorspace;
    }

    ///@cond
    static
    void
    delete_object(ColorStopSequence *p);
    ///@endcond

  private:
    friend class ColorStopSequenceAtlas;

    ColorStopSequence(void)
    {}

    reference_counted_ptr<ColorStopSequenceAtlas> m_atlas;
    const IntervalAllocator::Interval *m_interval;
    enum colorspace_t m_colorspace;
    bool m_opaque;
  };

/*! @} */

}

#endif

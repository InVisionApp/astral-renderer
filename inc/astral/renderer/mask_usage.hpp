/*!
 * \file mask_usage.hpp
 * \brief file mask_usage.hpp
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

#ifndef ASTRAL_MASK_USAGE_HPP
#define ASTRAL_MASK_USAGE_HPP

#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/mask_details.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * Class to specify how to use the mask generated
   * for stroking or filling. The masks generated
   * by filling and stroking have data to support
   * both distance field and coverage sampling.
   */
  class MaskUsage
  {
  public:
    /*!
     * Ctor
     * \param m value with which to initialize \ref m_mask_type
     * \param f value with which to initialize \ref m_filter
     */
    MaskUsage(enum mask_type_t m = mask_type_coverage,
              enum filter_t f = filter_linear):
      m_mask_type(m),
      m_filter(f)
    {}

    /*!
     * Ctor
     * \param m value with which to initialize \ref m_mask_type
     * \param f value with which to initialize \ref m_filter
     */
    MaskUsage(enum filter_t f, enum mask_type_t m = mask_type_coverage):
      m_mask_type(m),
      m_filter(f)
    {}

    /*!
     * Set \ref m_mask_type
     */
    MaskUsage&
    mask_type(enum mask_type_t v)
    {
      m_mask_type = v;
      return *this;
    }

    /*!
     * Set \ref m_filter
     */
    MaskUsage&
    filter(enum filter_t v)
    {
      m_filter = v;
      return *this;
    }

    /*!
     * Given a \ref MaskDetails for stroking or filling, change
     * its parameters to change the mask mode to a specified mode.
     * It is an error to pass a \ref MaskDetails that was not
     * generated from filling or stroking.
     */
    static
    void
    change_mask_mode(MaskDetails *mask, enum mask_type_t type)
    {
      mask->m_mask_channel = mask_channel(type);
      mask->m_mask_type = type;
    }

    /*!
     * The astral::Image held in the MaskDetails::m_mask for
     * stroking and filling holds both a distance field and
     * coverage value. This function returns on what channel
     * each of these are held.
     */
    static
    enum mask_channel_t
    mask_channel(enum mask_type_t v)
    {
      return (v == mask_type_distance_field) ?
        mask_channel_green :
        mask_channel_red;
    }

    /*!
     * Sspecifies how to sample from the mask to compute or
     * fetch a coverage value.
     */
    enum mask_type_t m_mask_type;

    /*!
     * Specifies the filtering to apply to sampling of the
     * mask.
     */
    enum filter_t m_filter;
  };

/*! @} */
}


#endif

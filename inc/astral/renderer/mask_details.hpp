/*!
 * \file mask_details.hpp
 * \brief file mask_details.hpp
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

#ifndef ASTRAL_MASK_DETAILS_HPP
#define ASTRAL_MASK_DETAILS_HPP

#include <astral/util/c_array.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/bounding_box.hpp>
#include <astral/util/scale_translate.hpp>
#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/image.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * \brief
   * Provides detailed information about a mask generated
   * by astral::RenderEncoderBase::generate_mask(). These
   * details tell what channel and how to use that channel
   * of the astral::Image, what portion of the astral::Image
   * to use along with the mapping from pixel coordinates
   * to that portion.
   */
  class MaskDetails
  {
  public:
    MaskDetails(void):
      m_min_corner(0.0f, 0.0f),
      m_size(0.0f, 0.0f),
      m_mask_channel(mask_channel_green),
      m_mask_type(mask_type_distance_field)
    {}

    /*!
     * Intersect against a rectangle in pixel coordinate, this
     * changes the values of \ref m_min_corner, \ref m_size
     */
    void
    instersect_against_pixel_rect(const BoundingBox<float> &pixel_rect);

    /*!
     * Compute and return the region the mask occupies in pixel coordinates.
     */
    BoundingBox<float>
    pixel_rect(void) const;

    /*!
     * astral::Image holding the mask of
     * the stroked or filled path
     */
    reference_counted_ptr<const Image> m_mask;

    /*!
     * The min-corner of the subimage of \ref m_mask
     * from which to sample.
     */
    vec2 m_min_corner;

    /*!
     * The size of the sub-image of \ref m_mask
     * from which to sample.
     */
    vec2 m_size;

    /*!
     * Specifies which channel from \ref m_mask to sample
     */
    enum mask_channel_t m_mask_channel;

    /*!
     * Specifies how to interpret the sample values
     */
    enum mask_type_t m_mask_type;

    /*!
     * The transformation from pixel coordinates to
     * coordinates of the sub-image of \ref m_mask
     * specified by \ref m_min_corner and \ref m_size.
     */
    ScaleTranslate m_mask_transformation_pixel;
  };

/*! @} */
}

#endif

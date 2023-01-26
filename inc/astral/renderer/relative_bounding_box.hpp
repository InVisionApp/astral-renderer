/*!
 * \file relative_bounding_box.hpp
 * \brief file relative_bounding_box.hpp
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

#ifndef ASTRAL_BOUNDING_BOX_WITH_PADDING
#define ASTRAL_BOUNDING_BOX_WITH_PADDING

#include <astral/util/bounding_box.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * Class to specify a region for offscreen rendering
   */
  class RelativeBoundingBox
  {
  public:
    /*!
     * Ctor to initialize as empty with no padding
     */
    RelativeBoundingBox(void):
      m_padding(0.0f),
      m_pixel_bb(nullptr),
      m_inherit_culling_of_parent(true)
    {}

    /*!
     * Ctor
     * \param bb initial value for \ref m_bb
     * \param padding initial value for \ref m_padding
     * \param pixel_bb initial value for \ref m_pixel_bb
     */
    RelativeBoundingBox(const BoundingBox<float> &bb,
                        float padding = 0.0f,
                        const BoundingBox<float> *pixel_bb = nullptr):
      m_bb(bb),
      m_padding(padding),
      m_pixel_bb(pixel_bb),
      m_inherit_culling_of_parent(true)
    {}

    /*!
     * Ctor
     * \param bb initial value for \ref m_bb
     * \param padding initial value for \ref m_padding
     * \param pixel_bb initial value for \ref m_pixel_bb
     */
    RelativeBoundingBox(const BoundingBox<float> &bb,
                        const BoundingBox<float> *pixel_bb,
                        float padding = 0.0f):
      m_bb(bb),
      m_padding(padding),
      m_pixel_bb(pixel_bb),
      m_inherit_culling_of_parent(true)
    {}

    /*!
     * Return an astral::BoundingBox<float> which is the
     * value of \ref m_bb padded by \ref m_padding
     */
    BoundingBox<float>
    bb_with_padding(void) const
    {
      BoundingBox<float> return_value(m_bb);

      return_value.enlarge(vec2(m_padding, m_padding));
      return return_value;
    }

    /*!
     * The bounding box in logical coordinate without the
     * padding.
     */
    BoundingBox<float> m_bb;

    /*!
     * The amount of padding in logical coordinates around
     * \ref m_bb. This padding is applied AFTER clipping
     * the box against both \ref m_pixel_bb and the clipping
     * inherited. The final clipping created by this box
     * includes the padding, i.e. having \ref m_padding > 0
     * can make the clipping region larger than the region
     * inherited from the parent or the region specified
     * by \ref m_pixel_bb.
     */
    float m_padding;

    /*!
     * If non-null, a bounding box in -pixel- coordinate
     * which also clips the region specified by \ref m_bb
     */
    const BoundingBox<float> *m_pixel_bb;

    /*!
     * If true (the default) when computing the region, inherit
     * the culling from the parent.
     */
    bool m_inherit_culling_of_parent;
  };
/*! @} */

}

#endif

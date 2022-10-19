/*!
 * \file render_scale_factor.hpp
 * \brief file render_scale_factor.hpp
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

#ifndef ASTRAL_RENDER_SCALE_FACTOR_HPP
#define ASTRAL_RENDER_SCALE_FACTOR_HPP

#include <astral/util/vecN.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  class RenderUniformScaleFactor;

  /*!
   * \brief
   * When rendering to a relative offscreen buffer (via
   * astral::RenderEncoderBase::encoder_mask_relative(),
   * astral::RenderEncoderBase::encoder_image_relative()),
   * the produced astral::Image can be at a lower resolution
   * than the area it covers in the astral::RenderEncoderBase
   * whose method is called. This class represents the ratio
   * in sizes.
   */
  class RenderScaleFactor
  {
  public:
    /*!
     * Ctor
     * \param v value with which to initialize \ref m_scale_factor
     * \param r value with which to initialize \ref m_relative
     */
    RenderScaleFactor(vec2 v = vec2(1.0f, 1.0f), bool r = true):
      m_scale_factor(v),
      m_relative(r)
    {}

    /*!
     * Ctor
     * \param v value with which to initialize \ref m_scale_factor
     * \param r value with which to initialize \ref m_relative
     */
    RenderScaleFactor(float v, bool r = true):
      m_scale_factor(v, v),
      m_relative(r)
    {}

    /*!
     * Implicit ctor from an astral::RenderUniformScaleFactor
     */
    RenderScaleFactor(const RenderUniformScaleFactor&);

    /*!
     * Scaling factor from pixel coordinates of rendering to
     * the astral::Image, i.e. a value of less than one indicates
     * that the resolution of the astral::Image is lower than
     * the pixel coordinate of rendering.
     */
    vec2 m_scale_factor;

    /*!
     * If true, the scaling factor applied is relative to the
     * astral::RenderEncoderBase objects which generates the
     * astral::RenderEncoderBase which is used to render, i.e.
     * the final scaling factor is the product of the values
     * of \ref astral::RenderEncoderBase::render_scale_factor()
     * of the generating astral::RenderEncoderBase and \ref
     * m_scale_factor
     */
    bool m_relative;
  };

  /*!
   * Analogous to \ref RenderScaleFactor, but the scaling
   * factor is the same in each dimension.
   */
  class RenderUniformScaleFactor
  {
  public:
    /*!
     * Ctor
     * \param v value with which to initialize \ref m_scale_factor
     * \param r value with which to initialize \ref m_relative
     */
    RenderUniformScaleFactor(float v = 1.0f, bool r = true):
      m_scale_factor(v),
      m_relative(r)
    {}

    /*!
     * Scaling factor from pixel coordinates of rendering to
     * the astral::Image, i.e. a value of less than one indicates
     * that the resolution of the astral::Image is lower than
     * the pixel coordinate of rendering.
     */
    float m_scale_factor;

    /*!
     * If true, the scaling factor applied is relative to the
     * astral::RenderEncoderBase objects which generates the
     * astral::RenderEncoderBase which is used to render, i.e.
     * the final scaling factor is the product of the values
     * of \ref astral::RenderEncoderBase::render_scale_factor()
     * of the generating astral::RenderEncoderBase and \ref
     * m_scale_factor
     */
    bool m_relative;
  };

  inline
  RenderScaleFactor::
  RenderScaleFactor(const RenderUniformScaleFactor &obj):
    m_scale_factor(obj.m_scale_factor, obj.m_scale_factor),
    m_relative(obj.m_relative)
  {}

/*! @} */

}

#endif

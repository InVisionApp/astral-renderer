/*!
 * \file brush.hpp
 * \brief file brush.hpp
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

#ifndef ASTRAL_BRUSH_HPP
#define ASTRAL_BRUSH_HPP

#include <astral/util/color.hpp>
#include <astral/renderer/render_value.hpp>
#include <astral/renderer/image_sampler.hpp>
#include <astral/renderer/gradient.hpp>
#include <astral/renderer/gradient_transformation.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * \brief
   * Encapsulates a brush that produces the pixel color values.
   * The starting color for the brush is given by the value of
   * Brush::m_base_color. If an image is present, then the color
   * is then modulated by the image. If a gradient is present,
   * then the color is modulated by the gradient. A brush can
   * have both an image and gradient and in which case they both
   * modulate the color.
   */
  class Brush
  {
  public:
    /*!
     * Ctor. Initialize to having no image, no mask
     * and the initial color as solid white
     */
    Brush(void):
      m_base_color(1.0f, 1.0f, 1.0f, 1.0f),
      m_colorspace(false, colorspace_srgb),
      m_opaque(false)
    {}

    /*!
     * Set the value of \ref m_base_color without affecting
     * in which colorspace the modulation occurs in.
     */
    Brush&
    base_color(const vec4 &v)
    {
      m_base_color = v;
      return *this;
    }

    /*!
     * Set the value of \ref m_base_color specifying
     * at which color space the modulation occurs in.
     */
    Brush&
    base_color(const vec4 &v, enum colorspace_t tcolorspace)
    {
      m_base_color = v;
      m_colorspace.first = true;
      m_colorspace.second = tcolorspace;
      return *this;
    }

    /*!
     * Set the value of \ref m_base_color specifying
     * at which color space it is in.
     */
    template<enum colorspace_t tcolorspace>
    Brush&
    base_color(FixedPointColor<tcolorspace> v)
    {
      return base_color(v.normalized_value(), tcolorspace);
    }

    /*!
     * set the value of \ref m_image
     */
    Brush&
    image(RenderValue<ImageSampler> v)
    {
      m_image = v;
      return *this;
    }

    /*!
     * set the value of \ref m_image_transformation
     */
    Brush&
    image_transformation(RenderValue<Transformation> v)
    {
      m_image_transformation = v;
      return *this;
    }

    /*!
     * set the value of \ref m_gradient
     */
    Brush&
    gradient(RenderValue<Gradient> v)
    {
      m_gradient = v;
      return *this;
    }

    /*!
     * Set the value of \ref m_gradient_transformation
     */
    Brush&
    gradient_transformation(RenderValue<GradientTransformation> v)
    {
      m_gradient_transformation = v;
      return *this;
    }

    /*!
     * Set the colorspace at which the modulation takes place,
     * i.e. set the value of \ref m_colorspace .first as true
     * and \ref m_colorspace .second as the passed value.
     */
    Brush&
    colorspace(enum colorspace_t v)
    {
      m_colorspace.first = true;
      m_colorspace.second = v;
      return *this;
    }

    /*!
     * Set the colorspace  at which the modulation takes place
     * to match the color space at which the rendering takes
     * place, i.e. set the value of \ref m_colorspace .first as
     * false.
     */
    Brush&
    colorspace_rendering(void)
    {
      m_colorspace.first = false;
      return *this;
    }

    /*!
     * set the value of \ref m_opaque
     */
    Brush&
    opaque(bool v)
    {
      m_opaque = v;
      return *this;
    }

    /*!
     * If valid, indicates to module with sampled
     * value from image data.
     */
    RenderValue<ImageSampler> m_image;

    /*!
     * If valid, provides a mapping from material
     * coordinates to image coordinates of \ref m_image.
     * An invalid handle value indicates that material
     * and image coordinates are the same.
     */
    RenderValue<Transformation> m_image_transformation;

    /*!
     * if valid, indicates that the brush has a gradient.
     */
    RenderValue<Gradient> m_gradient;

    /*!
     * If valid, provides the transformation from material
     * coordinate to gradient coordinates AND the tiling
     * mode to apply to the gradient.
     */
    RenderValue<GradientTransformation> m_gradient_transformation;

    /*!
     * Provides the starting base color of the Brush
     * The color space of the base-color is the color
     * space that the modulation is taking place. The
     * value in \ref m_base_color is WITHOUT having
     * alpha pre-multiplied.
     */
    vec4 m_base_color;

    /*!
     * If .first is true, have all color modulation take
     * place in the colorspace named by .second. Otherwise
     * have the color modulation take place in whatever
     * color space one is currently rendering it.
     */
    std::pair<bool, enum colorspace_t> m_colorspace;

    /*!
     * Initial value is false. When a astral::RenderValue<astral::Brush>
     * is created from an astral::Brush, the value of \ref m_opaque
     * is computed as follows:
     *  - if the input Brush::m_opaque is true, then the output
     *    Brush::m_opaque is true
     *  - otherwise, the color, image and gradient of the astral::Brush
     *    are checked and if each of those (that are present) is opaque,
     *    then the output Brush::m_opaque is true. If any of those (that
     *    are present) is not opaqee, then the output Brush::m_opaque is
     *    false.
     *  .
     *
     * The upshop being that a brush can be forced by a caller to be
     * viewed opaque (even if it is not) by setting \ref m_opaque as
     * true.
     */
    bool m_opaque;
  };

/*! @} */
}

#endif

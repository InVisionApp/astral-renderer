/*!
 * \file color.hpp
 * \brief file color.hpp
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

#ifndef ASTRAL_COLOR_HPP
#define ASTRAL_COLOR_HPP

#include <astral/util/vecN.hpp>
#include <astral/util/util.hpp>

namespace astral
{

/*!\addtogroup Utility
 * @{
 */

  /*!
   * Enumeration to specify if an image is sRGB encoded or not.
   * The conversion from linear to sRGB is given by
   * \code
   * if (linear < 0.0031308)
   *   {
   *     sRGB = 12.92 * linear;
   *   }
   * else
   *   {
   *     sRGB = 1.055 * pow(linear, 0.41666) - 0.055;
   *   }
   * \endcode
   * and the conversion from sRGB to linear is given by
   * \code
   * if (sRGB < 0.004045)
   *   {
   *     linear = sRGB / 12.92;
   *   }
   * else
   *   {
   *     linear = pow((sRGB + 0.055) / 1.055, 2.4);
   *   }
   * \endcode
   *
   * Only the red, green and blue channels can be in sRGB encoding.
   * The alpha channel is ALWAYS in linear encoding.
   */
  enum colorspace_t:uint32_t
    {
      /*!
       * color values are linearly encoded.
       */
      colorspace_linear = 0,

      /*!
       * color value are sRGB encoded
       */
      colorspace_srgb,
    };

  /*!
   * Compute a linear value from an sRGB value.
   */
  inline
  float
  linear_from_srgb(float in_srgb)
  {
    float R;

    R = (in_srgb < 0.004045f) ?
      in_srgb * 0.0773993808f:
      ::powf((in_srgb + 0.055f) * 0.94786729857f, 2.4f);

    return t_clamp(R, 0.0f, 1.0f);
  }

  /*!
   * Compute a linear value from an sRGB value.
   */
  inline
  vec4
  linear_from_srgb(const vec4 &in_srgb)
  {
    return vec4(linear_from_srgb(in_srgb.x()),
                linear_from_srgb(in_srgb.y()),
                linear_from_srgb(in_srgb.z()),
                in_srgb.w());
  }

  /*!
   * Compute an sRGB value from a linear value.
   */
  inline
  float
  srgb_from_linear(float in_linear)
  {
    float R;

    R = (in_linear < 0.0031308f) ?
      12.92f * in_linear :
      1.055f * ::powf(in_linear, 0.4166666f) - 0.055f;

    return t_clamp(R, 0.0f, 1.0f);
  }

  /*!
   * Compute an sRGB value from a linear value.
   */
  inline
  vec4
  srgb_from_linear(const vec4 &in_linear)
  {
    return vec4(srgb_from_linear(in_linear.x()),
                srgb_from_linear(in_linear.y()),
                srgb_from_linear(in_linear.z()),
                in_linear.w());
  }

  /*!
   * From a normalized value in [0, 1], compute
   * the unnormalized value in [0, 255] clamped.
   */
  inline
  uint8_t
  uint8_from_normalized(float v)
  {
    int32_t r;

    r = static_cast<int32_t>(255.0f * v);
    return t_clamp(r, 0, 255);
  }

  /*!
   * From a normalized value in [0, 1], compute
   * the unnormalized value in [0, 255] clamped.
   */
  inline
  u8vec4
  uint8_from_normalized(const vec4 &v)
  {
    return u8vec4(uint8_from_normalized(v.x()),
                  uint8_from_normalized(v.y()),
                  uint8_from_normalized(v.z()),
                  uint8_from_normalized(v.w()));
  }

  /*!
   * From an unnormalized value in [0, 255], compute
   * the normalized value in [0, 1]
   */
  inline
  float
  normalized_from_uint8(uint8_t v)
  {
    float r(v);

    r /= 255.0f;
    return r;
  }

  /*!
   * From an unnormalized value in [0, 255], compute
   * the normalized value in [0, 1]
   */
  inline
  vec4
  normalized_from_uint8(u8vec4 v)
  {
    return vec4(normalized_from_uint8(v.x()),
                normalized_from_uint8(v.y()),
                normalized_from_uint8(v.z()),
                normalized_from_uint8(v.w()));
  }

  /*!
   * Represents a color value in 8-bit fixed point.
   * \tparam colorspace specifies if the red, green and blue
   *                    channels are sRGB or linearly encoded.
   */
  template<enum colorspace_t tcolorspace>
  class FixedPointColor
  {
  public:
    /*!
     * Ctor. Initailize all channels as 255u, i.e.
     * solid white
     */
    FixedPointColor(void):
      m_value(255u, 255u, 255u, 255u)
    {}

    /*!
     * Ctor
     * \param v initial value for \ref m_value
     */
    explicit
    FixedPointColor(u8vec4 v):
      m_value(v)
    {}

    /*!
     * Ctor
     * \param r value for red channel
     * \param g value for green channel
     * \param b value for blue channel
     * \param a value for alpha channel
     */
    FixedPointColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255u):
      m_value(r, g, b, a)
    {}

    /*!
     * Equality operator
     */
    bool
    operator==(FixedPointColor v) const
    {
      return m_value == v.m_value;
    }

    /*!
     * Inequality operator
     */
    bool
    operator!=(FixedPointColor v) const
    {
      return m_value != v.m_value;
    }

    /*!
     * Returns true if and only if the
     * alpha channel is full.
     */
    bool
    is_opaque(void) const
    {
      return alpha() == 255u;
    }

    /*!
     * Return the red channel value
     */
    uint8_t
    red(void) const
    {
      return m_value.x();
    }

    /*!
     * Return a reference to the red channel value
     */
    uint8_t&
    red(void)
    {
      return m_value.x();
    }

    /*!
     * Return the green channel value
     */
    uint8_t
    green(void) const
    {
      return m_value.y();
    }

    /*!
     * Return a reference to the green channel value
     */
    uint8_t&
    green(void)
    {
      return m_value.y();
    }

    /*!
     * Return the blue channel value
     */
    uint8_t
    blue(void) const
    {
      return m_value.z();
    }

    /*!
     * Return a reference to the blue channel value
     */
    uint8_t&
    blue(void)
    {
      return m_value.z();
    }

    /*!
     * Return the alpha channel value
     */
    uint8_t
    alpha(void) const
    {
      return m_value.w();
    }

    /*!
     * Return a reference to the alpha channel value
     */
    uint8_t&
    alpha(void)
    {
      return m_value.w();
    }

    /*!
     * Return \ref m_value as an astral::vec4
     * with each value normalized to [0, 1].
     */
    vec4
    normalized_value(void) const
    {
      return vec4(m_value) / 255.0f;
    }

    /*!
     * Returns the colorspace of the type
     */
    static
    colorspace_t
    colorspace(void)
    {
      return tcolorspace;
    }

    /*!
     * Color value where the value is in the range [0, 255];
     * the red channel is in .x(), the green channel is in
     * .y(), the blue channel is in .z() and the alpha channel
     * is in .a().
     */
    u8vec4 m_value;
  };

  /*!
   * Typedef to make typing less.
   */
  typedef FixedPointColor<colorspace_linear> FixedPointColorLinear;

  /*!
   * Typedef to make typing less.
   */
  typedef FixedPointColor<colorspace_srgb> FixedPointColor_sRGB;

  /*!
   * Compute an astral::FixedPointColorLinear value from an
   * astral::FixedPointColor_sRGB value
   */
  inline
  FixedPointColorLinear
  linear_from_srgb(FixedPointColor_sRGB v)
  {
    vec4 raw;

    raw = linear_from_srgb(v.normalized_value());
    return FixedPointColorLinear(uint8_from_normalized(raw));
  }

  /*!
   * Compute an astral::FixedPointColor_sRGB value from an
   * astral::FixedPointColorLinear value
   */
  inline
  FixedPointColor_sRGB
  srgb_from_linear(FixedPointColorLinear v)
  {
    vec4 raw;

    raw = srgb_from_linear(v.normalized_value());
    return FixedPointColor_sRGB(uint8_from_normalized(raw));
  }

  /*!
   * astral::Image color pixels are always with alpha pre-multiplied.
   * This routine multiplies the red, green and blue channels by the
   * alpha channel value.
   */
  inline
  void
  covert_to_premultiplied_alpha(vec4 &data)
  {
    data.x() *= data.w();
    data.y() *= data.w();
    data.z() *= data.w();
  }

  /*!
   * astral::Image color pixels are always with alpha pre-multiplied.
   * This routine multiplies the red, green and blue channels by the
   * alpha channel value.
   */
  inline
  void
  covert_to_premultiplied_alpha(u8vec4 &data)
  {
    if (data.w() != 0xFF)
      {
        vec4 fdata(normalized_from_uint8(data));

        covert_to_premultiplied_alpha(fdata);
        data = uint8_from_normalized(fdata);
      }
  }

  /*!
   * Call covert_to_premultiplied_alpha() on data in an
   * iterator range.
   * \tparam iterator that when derefences gives either
   *                  vec4& or u8vec4&
   * \param begin iterator to 1st element to convert
   * \param end iterator past the last element to convert
   */
  template<typename iterator>
  void
  covert_to_premultiplied_alpha(iterator begin, iterator end)
  {
    for (; begin != end; ++begin)
      {
        covert_to_premultiplied_alpha(*begin);
      }
  }

/*! @} */
}

#endif

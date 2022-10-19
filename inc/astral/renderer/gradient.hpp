/*!
 * \file gradient.hpp
 * \brief file gradient.hpp
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

#ifndef ASTRAL_GRADIENT_HPP
#define ASTRAL_GRADIENT_HPP

#include <astral/util/vecN.hpp>
#include <astral/renderer/colorstop_sequence.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * \brief
   * astral::Gradient encapsulates a gradient on a brush. All values
   * of a gradient are in its own coordiante space. For the conversion
   * from material coordiantes to gradient coordiantes, see the class
   * astral::GradientTransformation. For the conversion from local
   * coordinates to material coordinates see the member variable
   * astral::ItemMaterial::m_material_transformation_logical
   */
  class Gradient
  {
  public:
    /*!
     * Enumeration to describe the interpolate value to give a
     * radial gradient outside of the cone defined by the two
     * circles of it.
     */
    enum radial_gradient_extension_type_t:uint32_t
      {
        /*!
         * Indicates that gradient is a radial gradient
         * where the interpolate value is extended past
         * the domain where the radial gradient is defined
         * so that its extension is continuous on all of
         * the plane.
         */
        radial_gradient_extended,

        /*!
         * Indicates that gradient is a radial gradient
         * where when the interpolate value is not defined,
         * opaque black is emitted
         */
        radial_gradient_unextended_opaque,

        /*!
         * Indicates that gradient is a radial gradient
         * where when the interpolate value is not defined,
         * clear black is emitted
         */
        radial_gradient_unextended_clear,
      };

    /*!
     * \brief
     * Enumeration to specify the geometry type of a gradient
     */
    enum type_t:uint32_t
      {
        /*!
         * Indicates that gradient is a linear gradient.
         */
        linear = 0u,

        /*!
         * Indicates that gradient is a sweep gradient.
         */
        sweep,

        /*!
         * Indicates that gradient is a radial gradient with
         * extension value \ref radial_gradient_extended
         */
        radial_extended,

        /*!
         * Indicates that gradient is a radial gradient with
         * extension value \ref radial_gradient_unextended_opaque
         */
        radial_unextended_opaque,

        /*!
         * Indicates that gradient is a radial gradient with
         * extension value \ref radial_gradient_unextended_clear
         */
        radial_unextended_clear,

        number_types,
      };

    /*!
     * enumeration type to encapsulate \ref invalid_gradient
     */
    enum invalid_gradient_t:uint32_t
      {
        /*!
         * Indicates to create an invalid Gradient value
         */
        invalid_gradient = 0u
      };

    /*!
     * Conveniance function to compute \ref type_t value from
     * a \ref radial_gradient_extension_type_t value.
     */
    static
    enum type_t
    gradient_type(enum radial_gradient_extension_type_t v)
    {
      return static_cast<enum type_t>(radial_extended + v);
    }

    /*!
     * Conveniance function to compute a \ref type_t value
     * from a \ref radial_gradient_extension_type_t value.
     */
    static
    enum radial_gradient_extension_type_t
    gradient_extension_type(enum type_t v)
    {
      unsigned int r(v);
      r = t_max(r, static_cast<unsigned int>(radial_extended));
      return static_cast<enum radial_gradient_extension_type_t>(r - radial_extended);
    }

    /*!
     * Conveniance function that returns true if and only
     * if the gradient type is a radial gradient type.
     */
    static
    bool
    is_radial_gradient(enum type_t v)
    {
      return v >= radial_extended;
    }

    /*!
     * Ctor that creates an invalid gradient value because
     * \ref m_colorstops is left as nullptr. However, the
     * remaining fields are initialized as for a linear
     * gradient from (0, 0) to (1, 0) with the tile mode
     * as astral::tile_clamp.
     */
    explicit
    Gradient(enum invalid_gradient_t):
      m_type(linear),
      m_data(0.0f, 0.0f, 1.0f, 0.0f),
      m_r0(0.0f),
      m_r1(0.0f),
      m_interpolate_tile_mode(tile_mode_clamp)
    {}

    /*!
     * Ctor; initialize as a linear gradient
     * \param cs color stop squence of the gradient
     * \param start_p start point of gradient
     * \param end_p end point of gradient
     * \param tile tile mode of gradient
     */
    Gradient(const reference_counted_ptr<const ColorStopSequence> &cs,
             const vec2 &start_p, const vec2 &end_p,
             enum tile_mode_t tile):
      m_type(linear),
      m_r0(0.0f),
      m_r1(0.0f),
      m_colorstops(cs),
      m_interpolate_tile_mode(tile)
    {
      vec2 d;

      d = end_p - start_p;
      d /= dot(d, d);

      m_data = vec4(start_p.x(), start_p.y(), d.x(), d.y());
      ASTRALassert(m_interpolate_tile_mode < tile_mode_number_modes);
    }

    /*!
     * Ctor; initialize as a radial gradient
     * \param cs color stop squence of the gradient
     * \param start_p start point of gradient
     * \param start_r start radius of gradient
     * \param end_p end point of gradient
     * \param end_r end radius of gradient
     * \param tile tile mode of gradient
     * \param ext how the radial gradient is extended past its
     *            cone of definition
     */
    Gradient(const reference_counted_ptr<const ColorStopSequence> &cs,
             const vec2 &start_p, float start_r,
             const vec2 &end_p, float end_r,
             enum tile_mode_t tile,
             enum radial_gradient_extension_type_t ext = radial_gradient_extended):
      m_type(gradient_type(ext)),
      m_data(start_p.x(), start_p.y(), end_p.x(), end_p.y()),
      m_r0(start_r),
      m_r1(end_r),
      m_colorstops(cs),
      m_interpolate_tile_mode(tile)
    {
      ASTRALassert(m_interpolate_tile_mode < tile_mode_number_modes);
    }

    /*!
     * Ctor; initialize as a radial gradient with the start
     * and end points the same and the start radius as 0
     * \param cs color stop squence of the gradient
     * \param p start and end point of gradient
     * \param r end radius of gradient
     * \param tile tile mode of gradient
     * \param ext how the radial gradient is extended past its
     *            cone of definition
     */
    Gradient(const reference_counted_ptr<const ColorStopSequence> &cs,
             const vec2 &p, float r,
             enum tile_mode_t tile,
             enum radial_gradient_extension_type_t ext = radial_gradient_extended):
      m_type(gradient_type(ext)),
      m_data(p.x(), p.y(), p.x(), p.y()),
      m_r0(0.0f),
      m_r1(r),
      m_colorstops(cs),
      m_interpolate_tile_mode(tile)
    {
      ASTRALassert(m_interpolate_tile_mode < tile_mode_number_modes);
    }

    /*!
     * Ctor; initialize as a sweep gradient
     * \param cs color stop squence of the gradient
     * \param p center of the sweep
     * \param theta the starting angle of the sweep in radians
     * \param F sweep angle multiplier, the angle computed from the
     *          positon relative to p is mutliplied by this value.
     *          This enables for a gradient to be repeated.
     * \param tile tile mode of gradient
     */
    Gradient(const reference_counted_ptr<const ColorStopSequence> &cs,
             const vec2 &p, float theta, float F,
             enum tile_mode_t tile):
      m_type(sweep),
      m_data(p.x(), p.y(), theta, F),
      m_r0(0.0f),
      m_r1(0.0f),
      m_colorstops(cs),
      m_interpolate_tile_mode(tile)
    {
      ASTRALassert(m_interpolate_tile_mode < tile_mode_number_modes);
    }

    /*!
     * Gives the gradient type.
     */
    enum type_t m_type;

    /*!
     * Linear Gradient:
     *  - m_data.xy : start position
     *  - m_data.zw : vector from start to end with the correct
     *                length so that interpolate at p is given
     *                by dot(p - m_data.xy, m_data.zw)
     *
     * Radial gradient:
     *  - m_data.xy : start position
     *  - m_data.zw : end positition
     *
     * Sweep Gradient:
     *  - m_data.xy : sweep center
     *  - m_data.z : sweep start angle in radians
     *  - m_data.w : sweep angle multiplier
     */
    vec4 m_data;

    /*!
     * Only for radial gradients; gives the
     * starting radius.
     */
    float m_r0;

    /*!
     * Only for radial gradients; gives the
     * ending radius.
     */
    float m_r1;

    /*!
     * ColorStopSequence to use to get the RGBA values from the
     * gradient interpolant.
     */
    reference_counted_ptr<const ColorStopSequence> m_colorstops;

    /*!
     * How to interpret the interpolate outside of [0, 1].
     */
    enum tile_mode_t m_interpolate_tile_mode;
  };

  /*!
   * Returns a string corresponing to the enum-value.
   */
  c_string
  label(enum Gradient::type_t);

/*! @} */
}

#endif

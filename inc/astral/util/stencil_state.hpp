/*!
 * \file stencil_state.hpp
 * \brief file stencil_state.hpp
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

#ifndef ASTRAL_STENCIL_STATE_HPP
#define ASTRAL_STENCIL_STATE_HPP

namespace astral
{
/*!\addtogroup Utility
 * @{
 */

  /*!
   * \brief
   * StencilState encapsulates the stecil test, stencil op
   * and stencil write mask.
   */
  class StencilState
  {
  public:
    /*!
     * \brief
     * Enumeration to specify a face orientation
     */
    enum face_t:uint32_t
      {
        /*!
         * Indicates those tirangles that clockwise oriented
         */
        face_cw,

        /*!
         * Indicates those tirangles that counter-clockwise oriented
         */
        face_ccw
      };

    /*!
     * \brief
     * Enumeration to describe a stencil operation.
     */
    enum op_t:uint32_t
      {
        /*!
         * Indicates that the stencil op is to keep the current stencil value.
         */
        op_keep,

        /*!
         * Indicates that the stencil op is to write zero.
         */
        op_zero,

        /*!
         * Indicates that the stencil op is to write the stencil reference
         * value to the current stencil value.
         */
        op_replace,

        /*!
         * Indicates that the stencil op is to increment-clamp
         * the current stencil value, i.e. if incrementing from
         * the maximum stencil value, then to not change the
         * stencil value.
         */
        op_incr_clamp,

        /*!
         * Indicates that the stencil op is to increment-wrap
         * the current stencil value, i.e. if incrementing from
         * the maximum stencil value, then to write 0.
         */
        op_incr_wrap,

        /*!
         * Indicates that the stencil op is to decrement-clamp
         * the current stencil value, i.e. if decrementing from
         * the zero stencil value, then to not change the
         * stencil value.
         */
        op_decr_clamp,

        /*!
         * Indicates that the stencil op is to to decrement-wrap
         * the current stencil value, i.e. if decrementing from
         * the zero stencil value, then to write the maximum
         * stencil value.
         */
        op_decr_wrap,

        /*!
         * Indicates that the stencil op is to flip the bits of
         * the stencil value.
         */
        op_invert,

        op_count
      };

    /*!
     * \brief
     * Enumeration to describe a stencil test.
     */
    enum test_t:uint32_t
      {
        /*!
         * Indicates that the stencil never passes
         */
        test_never,

        /*!
         * Indicates that the stencil always passes
         */
        test_always,

        /*!
         * Indicates that the stencil test passed if
         * (\ref m_reference & \ref m_reference_mask) < (stencil & \ref m_reference_mask)
         * where stencil is the current stencil value.
         */
        test_less,

        /*!
         * Indicates that the stencil test passed if
         * (\ref m_reference & \ref m_reference_mask) <= (stencil & \ref m_reference_mask)
         * where stencil is the current stencil value.
         */
        test_less_equal,

        /*!
         * Indicates that the stencil test passed if
         * (\ref m_reference & \ref m_reference_mask) > (stencil & \ref m_reference_mask)
         * where stencil is the current stencil value.
         */
        test_greater,

        /*!
         * Indicates that the stencil test passed if
         * (\ref m_reference & \ref m_reference_mask) >= (stencil & \ref m_reference_mask)
         * where stencil is the current stencil value.
         */
        test_greater_equal,

        /*!
         * Indicates that the stencil test passed if
         * (\ref m_reference & \ref m_reference_mask) != (stencil & \ref m_reference_mask)
         * where stencil is the current stencil value.
         */
        test_not_equal,

        /*!
         * Indicates that the stencil test passed if
         * (\ref m_reference & \ref m_reference_mask) == (stencil & \ref m_reference_mask)
         * where stencil is the current stencil value.
         */
        test_equal,

        test_count,
      };

    StencilState(void):
      m_stencil_fail_op(op_keep),
      m_stencil_pass_depth_fail_op(op_keep),
      m_stencil_pass_depth_pass_op(op_keep),
      m_func(test_always),
      m_reference_mask(~0u),
      m_reference(0u),
      m_write_mask(~0u),
      m_enabled(false)
    {}

    /*!
     * Comparison test that operates at the effective
     * stencil-test, i.e. two \ref StencilState objects
     * for which both \ref m_enabled is false are equal
     * regardless of the field values.
     */
    bool
    operator==(const StencilState &rhs) const
    {
      /* Note: there are other flavors of equivalent
       * beyond if both are disabled. However, the logic
       * for that is much trickier.
       */
      return (!m_enabled && !rhs.m_enabled)
        || (m_enabled == rhs.m_enabled
            && m_stencil_fail_op == rhs.m_stencil_fail_op
            && m_stencil_pass_depth_fail_op == rhs.m_stencil_pass_depth_fail_op
            && m_stencil_pass_depth_pass_op == rhs.m_stencil_pass_depth_pass_op
            && m_func == rhs.m_func
            && m_reference_mask == rhs.m_reference_mask
            && m_reference == rhs.m_reference
            && m_write_mask == rhs.m_write_mask);
    }

    /*!
     * comparison operator mapping to operator==
     */
    bool
    operator!=(const StencilState &rhs) const
    {
      return !operator==(rhs);
    }

    /*!
     * Set \ref m_stencil_fail_op
     */
    StencilState&
    stencil_fail_op(op_t v, face_t f)
    {
      m_stencil_fail_op[f] = v;
      return *this;
    }

    /*!
     * Set \ref m_stencil_fail_op
     */
    StencilState&
    stencil_fail_op(op_t v)
    {
      m_stencil_fail_op[face_cw] = v;
      m_stencil_fail_op[face_ccw] = v;
      return *this;
    }

    /*!
     * Set \ref m_stencil_pass_depth_fail_op
     */
    StencilState&
    stencil_pass_depth_fail_op(op_t v, face_t f)
    {
      m_stencil_pass_depth_fail_op[f] = v;
      return *this;
    }

    /*!
     * Set \ref m_stencil_pass_depth_fail_op
     */
    StencilState&
    stencil_pass_depth_fail_op(op_t v)
    {
      m_stencil_pass_depth_fail_op[face_cw] = v;
      m_stencil_pass_depth_fail_op[face_ccw] = v;
      return *this;
    }

    /*!
     * Set \ref m_stencil_pass_depth_pass_op
     */
    StencilState&
    stencil_pass_depth_pass_op(op_t v, face_t f)
    {
      m_stencil_pass_depth_pass_op[f] = v;
      return *this;
    }

    /*!
     * Set \ref m_stencil_pass_depth_pass_op
     */
    StencilState&
    stencil_pass_depth_pass_op(op_t v)
    {
      m_stencil_pass_depth_pass_op[face_cw] = v;
      m_stencil_pass_depth_pass_op[face_ccw] = v;
      return *this;
    }

    /*!
     * Set \ref m_func
     */
    StencilState&
    func(test_t v, face_t f)
    {
      m_func[f] = v;
      return *this;
    }

    /*!
     * Set \ref m_func
     */
    StencilState&
    func(test_t v)
    {
      m_func[face_cw] = v;
      m_func[face_ccw] = v;
      return *this;
    }

    /*!
     * Set \ref m_reference
     */
    StencilState&
    reference(uint32_t v, face_t f)
    {
      m_reference[f] = v;
      return *this;
    }

    /*!
     * Set \ref m_reference
     */
    StencilState&
    reference(uint32_t v)
    {
      m_reference[face_cw] = v;
      m_reference[face_ccw] = v;
      return *this;
    }

    /*!
     * Set \ref m_reference_mask
     */
    StencilState&
    reference_mask(uint32_t v, face_t f)
    {
      m_reference_mask[f] = v;
      return *this;
    }

    /*!
     * Set \ref m_reference_mask
     */
    StencilState&
    reference_mask(uint32_t v)
    {
      m_reference_mask[face_cw] = v;
      m_reference_mask[face_ccw] = v;
      return *this;
    }

    /*!
     * Set \ref m_write_mask
     */
    StencilState&
    write_mask(uint32_t v)
    {
      m_write_mask = v;
      return *this;
    }

    /*!
     * Set \ref m_enabled
     */
    StencilState&
    enabled(bool v)
    {
      m_enabled = v;
      return *this;
    }

    /*!
     * Stencil operation to execute if the stencil test
     * fails.
     */
    vecN<op_t, 2> m_stencil_fail_op;

    /*!
     * Stencil operation to execute if the stencil test
     * passes but depth test fails.
     */
    vecN<op_t, 2> m_stencil_pass_depth_fail_op;

    /*!
     * Stencil operation to execute if the stencil test
     * passes and depth test passes.
     */
    vecN<op_t, 2> m_stencil_pass_depth_pass_op;

    /*!
     * Stencil test to exexture.
     */
    vecN<test_t, 2> m_func;

    /*!
     * Provides the bit-mask applied against the reference
     * value and the current stencil value when determining
     * if the stencil test passes.
     */
    vecN<uint32_t, 2> m_reference_mask;

    /*!
     * Provides the reference value used in the stencil test.
     */
    vecN<uint32_t, 2> m_reference;

    /*!
     * The write mask applied when writing values to the
     * stencil buffer.
     */
    uint32_t m_write_mask;

    /*!
     * If false, stencil test is disabled. If true, stencil test
     * is enabled.
     */
    bool m_enabled;
  };
/*! @} */
}

#endif

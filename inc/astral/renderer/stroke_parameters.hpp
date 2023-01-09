/*!
 * \file stroke_parameters.hpp
 * \brief file stroke_parameters.hpp
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

#ifndef ASTRAL_STROKE_PARAMETERS_HPP
#define ASTRAL_STROKE_PARAMETERS_HPP

#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/render_scale_factor.hpp>
#include <astral/util/bounding_box.hpp>
#include <astral/util/matrix.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * \brief
   * Class to specify stroking parameters
   */
  class StrokeParameters
  {
  public:
    StrokeParameters(void):
      m_width(10.0f),
      m_join(join_rounded),
      m_cap(cap_rounded),
      m_glue_join(join_rounded),
      m_glue_cusp_join(join_bevel),
      m_miter_limit(5.0f),
      m_miter_clip(true),
      m_draw_edges(true),
      m_graceful_thin_stroking(false)
    {}

    /*!
     * Set \ref m_draw_edges
     */
    StrokeParameters&
    draw_edges(bool v)
    {
      m_draw_edges = v;
      return *this;
    }

    /*!
     * Set \ref m_join
     */
    StrokeParameters&
    join(enum join_t v)
    {
      m_join = v;
      return *this;
    }

    /*!
     * Set \ref m_cap
     */
    StrokeParameters&
    cap(enum cap_t v)
    {
      m_cap = v;
      return *this;
    }

    /*!
     * Set \ref m_width
     */
    StrokeParameters&
    width(float v)
    {
      m_width = v;
      return *this;
    }

    /*!
     * Equivalent to
     * \code
     * miter_limit(v, true);
     * \endcode
     */
    StrokeParameters&
    miter_limit_clip(float v)
    {
      return miter_limit(v, true);
    }

    /*!
     * Equivalent to
     * \code
     * miter_limit(v, false);
     * \endcode
     */
    StrokeParameters&
    miter_limit_cull(float v)
    {
      return miter_limit(v, false);
    }

    /*!
     * Set \ref m_miter_limit and \ref m_miter_clip
     * \param v miter limit value
     * \param miter_clip if true clip those miter-joins to the miter limit. If
     *                   false, then draw those miter-joins that exceed the
     *                   miter-limit are drawn as bevel joins.
     */
    StrokeParameters&
    miter_limit(float v, bool miter_clip)
    {
      m_miter_limit = t_max(0.0f, v);
      m_miter_clip = miter_clip;
      return *this;
    }

    /*!
     * Set \ref m_graceful_thin_stroking
     */
    StrokeParameters&
    graceful_thin_stroking(bool v)
    {
      m_graceful_thin_stroking = v;
      return *this;
    }

    /*!
     * The stroking *radius* in pixels of hairline strokes.
     */
    static
    constexpr float
    hairline_pixel_radius(void)
    {
      return 1.2f;
    }

    /*!
     * Specifies the stroking width, a value of
     * 0.0 indicates hairline stroking where the
     * hairline is the surface to which a stroke
     * is rendered; for generating a mask, that
     * means it is a harline in the mask (which
     * is thicker when the rendering scale factor
     * from used to generate the mask is less than
     * 1.0, see RenderEncoderBase::render_scale_factor(),
     * StrokeMaskProperties::m_render_scale_factor).
     */
    float m_width;

    /*!
     * Specifies how to draw joins
     */
    enum join_t m_join;

    /*!
     * Specifies how to draw caps
     */
    enum cap_t m_cap;

    /*!
     * Specifies how to draw regular glue joins, it is
     * undefined behavior for this to be astral::join_miter
     */
    enum join_t m_glue_join;

    /*!
     * Specifies how to draw glue cusp join, it is
     * undefined behavior for this to be astral::join_miter
     */
    enum join_t m_glue_cusp_join;

    /*!
     * Specifies the miter-limit to apply if \ref m_join is astral::join_miter
     */
    float m_miter_limit;

    /*!
     * - A true value indicates that the miter join is to be clipped
     *   to the miter-limit
     * - A false value indciates that if a miter-join exceeds the
     *   miter-limit, then it is to be drawn as a bevel join.
     */
    bool m_miter_clip;

    /*!
     * If false, only caps and joins are drawn
     */
    bool m_draw_edges;

    /*!
     * If true, instructs the stroking shader to realize
     * very thin strokes as a hairline stroke but with a
     * lower coverage value; such thin stroking should
     * only use coverage masks though.
     */
    bool m_graceful_thin_stroking;
  };

  /*!
   * Class to specify how a mask for a stroke is to be generated
   */
  class StrokeMaskProperties
  {
  public:
    StrokeMaskProperties(void):
      m_restrict_bb(nullptr),
      m_sparse_mask(true),
      m_apply_clip_equations_clipping(true)
    {}

    /*!
     * Set \ref m_render_scale_factor
     */
    StrokeMaskProperties&
    render_scale_factor(RenderScaleFactor v)
    {
      m_render_scale_factor = v;
      return *this;
    }

    /*!
     * Set \ref m_restrict_bb
     * \param v value
     */
    StrokeMaskProperties&
    restrict_bb(const BoundingBox<float> *v)
    {
      m_restrict_bb = v;
      return *this;
    }

    /*!
     * Set \ref m_sparse_mask
     */
    StrokeMaskProperties&
    sparse_mask(bool v)
    {
      m_sparse_mask = v;
      return *this;
    }

    /*!
     * Set \ref m_apply_clip_equations_clipping
     */
    StrokeMaskProperties&
    apply_clip_equations_clipping(bool v)
    {
      m_apply_clip_equations_clipping = v;
      return *this;
    }

    /*!
     * amount by which to scale rendering of the mask generation;
     * a value greater than one indicates to generate at a higher
     * resolution than the area where as a value less than one
     * indicates to generate at a lower resolution. One should
     * never set this value to greater than one. When using the
     * mask's distance field, this can be signficantly less than
     * 1.0 with the general rule of thumb the thickness of the
     * stroke would be atleast 2-4 pixels when realized in the
     * mask. If generating the mask as just a coverage, a value
     * of less than 1.0 will result in blurring.
     */
    RenderScaleFactor m_render_scale_factor;

    /*!
     * if non-null, provides a bounding box in -pixel- coordinates
     * which the mask bounds are intersected against.
     */
    const BoundingBox<float> *m_restrict_bb;

    /*!
     * If false and if the stroke shader generates a
     * mask, then all tiles of the astral::Image
     * of astral::MaskDetails::m_mask will
     * be populated. Otherwise, only those tiles hit by
     * the stroking will be populated and the remainder
     * will be ImageMipElement::empty_element tiles
     * By being true, astral::Renderer can significanly
     * reduce bandwidth and memory consumption.
     */
    bool m_sparse_mask;

    /*!
     * If true (the default) apply the clipping coming from the
     * clipping equations of the encoder that generates the mask,
     * this clipping includes viewport clipping.
     *
     * If one wishes to reuse a mask across frames where clipping
     * is varies across frames, this should be set to false. However,
     * set to this to false with exteme caution as it can result in
     * very large masks when the path is zoomed in.
     */
    bool m_apply_clip_equations_clipping;
  };

/*! @} */
}

#endif

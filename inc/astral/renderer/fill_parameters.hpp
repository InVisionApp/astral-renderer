/*!
 * \file fill_parameters.hpp
 * \brief file fill_parameters.hpp
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

#ifndef ASTRAL_FILL_PARAMETERS_HPP
#define ASTRAL_FILL_PARAMETERS_HPP

#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/render_scale_factor.hpp>
#include <astral/renderer/shader/item_path_shader.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */
  /*!
   * \brief
   * Class to specify filling parameters
   */
  class FillParameters
  {
  public:
    FillParameters(void):
      m_fill_rule(nonzero_fill_rule),
      m_aa_mode(with_anti_aliasing)
    {}

    /*!
     * Set \ref m_fill_rule
     * \param v value
     */
    FillParameters&
    fill_rule(enum fill_rule_t v)
    {
      m_fill_rule = v;
      return *this;
    }

    /*!
     * Set \ref m_aa_mode
     * \param v value
     */
    FillParameters&
    aa_mode(enum anti_alias_t v)
    {
      m_aa_mode = v;
      return *this;
    }

    /*!
     * The fill rule with which to fill the paths
     */
    enum fill_rule_t m_fill_rule;

    /*!
     * Specifies if/how to apply anti-aliasing via shaders
     * to fill the paths
     */
    enum anti_alias_t m_aa_mode;
  };

  /*!
   * \brief
   * Class to specify how a mask for a fill is to be generated
   */
  class FillMaskProperties
  {
  public:
    FillMaskProperties(void):
      m_complement_bbox(nullptr),
      m_restrict_bb(nullptr),
      m_sparse_mask(fill_method_sparse_line_clipping),
      m_pixel_threshhold_for_path_shader(0)
    {}

    /*!
     * Set \ref m_render_scale_factor
     * \param v value
     */
    FillMaskProperties&
    render_scale_factor(RenderUniformScaleFactor v)
    {
      m_render_scale_factor = v;
      return *this;
    }

    /*!
     * Set \ref m_complement_bbox
     * \param v value
     */
    FillMaskProperties&
    complement_bbox(const BoundingBox<float> *v)
    {
      m_complement_bbox = v;
      return *this;
    }

    /*!
     * Set \ref m_restrict_bb
     * \param v value
     */
    FillMaskProperties&
    restrict_bb(const BoundingBox<float> *v)
    {
      m_restrict_bb = v;
      return *this;
    }

    /*!
     * Set \ref m_sparse_mask
     */
    FillMaskProperties&
    sparse_mask(enum fill_method_t v)
    {
      m_sparse_mask = v;
      return *this;
    }

    /*!
     * Set \ref m_pixel_threshhold_for_path_shader
     */
    FillMaskProperties&
    pixel_threshhold_for_path_shader(int v)
    {
      m_pixel_threshhold_for_path_shader = v;
      return *this;
    }

    /*!
     * Set \ref m_path_shader
     */
    FillMaskProperties&
    path_shader(const MaskItemPathShader &v)
    {
      m_path_shader = v;
      return *this;
    }

    /*!
     * Returns true if a given size in pixels of the mask
     * should be generated with \ref m_path_shader
     * \param size size in pixels of the mask to generate
     */
    bool
    use_mask_shader(ivec2 size) const
    {
      return m_path_shader.get()
        && (size.x() < m_pixel_threshhold_for_path_shader
            || size.y() < m_pixel_threshhold_for_path_shader);
    }

    /*!
     * amount by which to scale the generation of the mask;
     * if the scale factor is less than 1.0 and the mask
     * type if coverage, blurring will result.
     */
    RenderUniformScaleFactor m_render_scale_factor;

    /*!
     * If non-null, gives the bounding box of the path to use
     * when filling with one of the complement fill rules;
     * a null value indicates to use the tight bounding box
     * of the path geometries.
     */
    const BoundingBox<float> *m_complement_bbox;

    /*!
     * if non-null, provides a bounding box in -pixel- coordinates
     * which the mask bounds are intersected against.
     */
    const BoundingBox<float> *m_restrict_bb;

    /*!
     * Specifies if and how the mask is generated
     * sparsely. If is is astral::fill_method_no_sparse,
     * then all tiles of the astral::Image
     * astral::MaskDetails::m_mask will
     * be populated. Otherwise, only those tiles
     * that intersect the edges of the input path
     * will be populated and the remainder will be
     * populated as ImageMipElement::empty_element
     * and ImageMipElement::white_element tiles.
     * By not having value astral::fill_method_no_sparse,
     * astral::Renderer can significanly reduce
     * bandwidth and memory consumption.
     */
    enum fill_method_t m_sparse_mask;

    /*!
     * When \ref m_path_shader is non-null and when the width
     * or height of the bounding region of the fill mask to be
     * rendered is less than this value, instead of using
     * stencil-then-cover to generate the mask, use the shader
     * \ref m_path_shader to generate the mask. Default value
     * is 0, i.e. a never.
     */
    int m_pixel_threshhold_for_path_shader;

    /*!
     * The astral::MaskItemPathShader to use when the width
     * or height of the mask is small, see also \ref
     * m_pixel_threshhold_for_path_shader
     */
    MaskItemPathShader m_path_shader;
  };

/*! @} */
}

#endif

/*!
 * \file render_clip.hpp
 * \brief file render_clip.hpp
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

#ifndef ASTRAL_RENDER_CLIP_HPP
#define ASTRAL_RENDER_CLIP_HPP

#include <astral/util/reference_counted.hpp>
#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/mask_details.hpp>
#include <astral/renderer/shader/item_path_shader.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  class Renderer;
  class RenderEncoderBase;
  class RenderEncoderImage;

  /*!
   * \brief
   * An astral::RenderClipElement embodies a region in space
   * to which to clip against. This object is re-usable across
   * frames subject to that it is used by the same astral::Renderer
   * object.
   */
  class RenderClipElement:public reference_counted<RenderClipElement>::non_concurrent_custom_delete
  {
  public:
    virtual
    ~RenderClipElement()
    {}

    /*!
     * Returns the mask details of this astral::RenderClipElement.
     * If this astral::RenderClipElement referes to an empty region,
     * returns nullptr.
     *
     * NOTE: the astral::MaskDetails of an astral::RenderClipElement
     *       does not necessarily obey the conventions as those found
     *       in astral::MaskUsage. To get different astral::mask_type_t
     *       mask usages, use as_mask_type().
     */
    const MaskDetails*
    mask_details(void) const;

    /*!
     * Given an astral::mask_type_t return from what
     * channel to sample the value. A return value of
     * astral::number_mask_channel indicates that the
     * image data does not support that mask type.
     */
    enum mask_channel_t
    mask_channel(enum mask_type_t) const;

    /*!
     * Returns an astral::mask_type_t that this supports,
     * i.e. when fed to mask_channel() is guarnateed to not generate
     * astral::number_mask_channel. In addition, the mask
     * type value from RenderClipCombineResult::mask_type()
     * of any astral::RenderClipCombineResult objects made with
     * this RenderClipElement will also be this value.
     */
    enum mask_type_t
    preferred_mask_type(void) const;

    /*!
     * Create a RenderClipElement that corresponds to intersecting
     * this RenderClipElement against a rectangle in pixel coordinates
     */
    reference_counted_ptr<const RenderClipElement>
    intersect(BoundingBox<float> pixel_rect) const;

    /*!
     * Return a astral::RenderClipElement that corresponds to the same
     * mask as this astral::RenderClipElement with the specified mask
     * type. If the underlying mask does not support the specified
     * mask type, returns nullptr.
     */
    reference_counted_ptr<const RenderClipElement>
    as_mask_type(enum mask_type_t) const;

    /*!
     * Similair to as_mask_type(), but instead of returning null
     * if the mask type is not available, returns this instead.
     */
    reference_counted_ptr<const RenderClipElement>
    as_mask_type_fall_back_this(enum mask_type_t) const;

    ///@cond
    static
    void
    delete_object(RenderClipElement *p);
    ///@endcond

  private:
    friend class Renderer;
    friend class RenderEncoderBase;
    friend class RenderEncoderImage;

    RenderClipElement(void)
    {}
  };

  /*!
   * Class to specify how to perform computation of
   * combining a path-fill against an astral::RenderClipElement
   */
  class RenderClipCombineParams
  {
  public:
    /*!
     * Ctor
     * \param fill_rule fill rule to apply to fill
     * \param aa_mode weather or not to apply anti-aliasing to the fill
     * \param sparse specify if and how to perform the operation sparsely
     */
    RenderClipCombineParams(enum fill_rule_t fill_rule,
                            enum anti_alias_t aa_mode = with_anti_aliasing,
                            enum fill_method_t sparse = fill_method_sparse_line_clipping):
      m_fill_rule(fill_rule),
      m_aa_mode(aa_mode),
      m_sparse(sparse),
      m_pixel_threshhold_for_path_shader(0)
    {}

    /*!
     * Ctor
     * \param fill_rule fill rule to apply to fill
     * \param aa_mode weather or not to apply anti-aliasing to the fill
     * \param sparse specify if and how to perform the operation sparsely
     */
    RenderClipCombineParams(enum fill_rule_t fill_rule,
                            enum fill_method_t sparse,
                            enum anti_alias_t aa_mode = with_anti_aliasing):
      m_fill_rule(fill_rule),
      m_aa_mode(aa_mode),
      m_sparse(sparse),
      m_pixel_threshhold_for_path_shader(0)
    {}

    /*!
     * Set \ref m_fill_rule
     * \param v value
     */
    RenderClipCombineParams&
    fill_rule(enum fill_rule_t v)
    {
      m_fill_rule = v;
      return *this;
    }

    /*!
     * Set \ref m_aa_mode
     * \param v value
     */
    RenderClipCombineParams&
    aa_mode(enum anti_alias_t v)
    {
      m_aa_mode = v;
      return *this;
    }

    /*!
     * Set \ref m_sparse
     */
    RenderClipCombineParams&
    sparse(enum fill_method_t v)
    {
      m_sparse = v;
      return *this;
    }

    /*!
     * Set \ref m_pixel_threshhold_for_path_shader
     */
    RenderClipCombineParams&
    pixel_threshhold_for_path_shader(int v)
    {
      m_pixel_threshhold_for_path_shader = v;
      return *this;
    }

    /*!
     * Set \ref m_path_shader
     */
    RenderClipCombineParams&
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
     * The fill rule with which to fill the astral::CombinedPath
     */
    enum fill_rule_t m_fill_rule;

    /*!
     * Specifies if/how to apply anti-aliasing via shaders
     * to fill the paths
     */
    enum anti_alias_t m_aa_mode;

    /*!
     * Specifies if and how the mask is generated
     * sparsely.
     */
    enum fill_method_t m_sparse;

    /*!
     * When \ref m_path_shader is non-null and when the width
     * or height of the bounding region of the fill mask to be
     * rendered is less than this value, instead of using
     * stencil-then-cover to generate the mask, use the shader
     * \ref m_path_shader to generate the mask. Default value
     * is 0u, i.e. a never.
     */
    int m_pixel_threshhold_for_path_shader;

    /*!
     * The astral::MaskItemPathShader to use when the width
     * or height of the mask is small, see also \ref
     * m_pixel_threshhold_for_path_shader
     */
    MaskItemPathShader m_path_shader;
  };

  /*!
   * Class that describes the combination of an astral::RenderClipElement C
   * and a path fill F
   */
  class RenderClipCombineResult:public reference_counted<RenderClipCombineResult>::non_concurrent_custom_delete
  {
  public:
    virtual
    ~RenderClipCombineResult()
    {}

    /*!
     * astral::RenderClipElement representing the intersection of
     * the astral::RenderClipElement C and path fill F. Never
     * returns null, but the returned region can represent an
     * empty region; i.e. RenderClipElement::mask_details().
     * will return nullptr if the region is empty.
     */
    const reference_counted_ptr<const RenderClipElement>&
    clip_in(void) const;

    /*!
     * astral::RenderClipElement representing the intersection of
     * the astral::RenderClipElement C and the complement of a
     * path fill F. Never returns null, but the returned can
     * represent an empty region; i.e. nullptr is returned
     * by RenderClipElement::mask_details() when the region
     * is empty.
     */
    const reference_counted_ptr<const RenderClipElement>&
    clip_out(void) const;

    /*!
     * Returns the value for RenderClipElement::preferred_mask_type()
     * of both clip_in() and clip_out().
     */
    enum mask_type_t
    mask_type(void) const;

    /*!
     * Return a astral::RenderClipCombineResult that corresponds to the same
     * mask as this astral::RenderClipCombineResult with the specified mask
     * type. If the masks in this cannot support the specified mask type,
     * returns nullptr.
     */
    reference_counted_ptr<const RenderClipCombineResult>
    as_mask_type(enum mask_type_t) const;

    ///@cond
    static
    void
    delete_object(RenderClipCombineResult *p);
    ///@endcond

  private:
    friend class Renderer;
    friend class RenderEncoderBase;
    friend class RenderEncoderImage;

    RenderClipCombineResult(void)
    {}
  };
/*! @} */
}

#endif

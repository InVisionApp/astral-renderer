/*!
 * \file render_values.hpp
 * \brief file render_values.hpp
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

#ifndef ASTRAL_RENDER_VALUES_HPP
#define ASTRAL_RENDER_VALUES_HPP

#include <astral/util/transformation.hpp>
#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/render_value.hpp>
#include <astral/util/scale_translate.hpp>
#include <astral/renderer/brush.hpp>
#include <astral/renderer/material.hpp>
#include <astral/renderer/shader/item_data.hpp>
#include <astral/renderer/shader/item_shader.hpp>
#include <astral/renderer/backend/blend_mode.hpp>


namespace astral
{
  class RenderClipElement;

/*!\addtogroup RendererBackend
 * @{
 */

  /*!
   * Class to represent a copy of the framebuffer pixels
   * for when a draw's blend mode requires them.
   *
   * NOTE: that using a RenderValue<EmulateFramebufferFetch>
   *       inside an ItemData is not SUPPORTED because
   *       there is not enumeration for it in the class
   *       ItemDataValueMapping. This is deliberate; the
   *       goal of this class is essentially to allow
   *       the GL3 backend to piggy back the location
   *       of the ImageSampler into the padding needed
   *       for an Transformation.
   *
   * MAYBE: maybe we can tweak Transformation to allow
   *        for setting the padding values; perhaps at
   *        the create_value() method for Transformation,
   *        perhaps allowing up to two scalar values to
   *        specify the padding values. However, adding
   *        support that the padding refers to data will
   *        complicate the ItemDataValueMapping class a
   *        great deal.
   */
  class EmulateFramebufferFetch
  {
  public:
    /*!
     * An ImageSampler holding a copy of the pixels
     * over the region of the framebuffer of a draw.
     */
    RenderValue<ImageSampler> m_image;

    /*!
     * The transformation from framebuffer pixel coordinates
     * to coordinates of \ref m_image.
     */
    Transformation m_image_transformation_pixels;
  };

  /*!
   * \brief
   * State to emcompass the entire render state for drawing data.
   */
  class RenderValues
  {
  public:
    /*!
     * Ctor
     */
    explicit
    RenderValues(void):
      m_clip_mask_filter(filter_linear),
      m_clip_out(false),
      m_mask_shader_clip_mode(mask_item_shader_clip_cutoff)
    {}

    /*!
     * The \ref Transformation to apply to the geometry;
     * an invalid value indicates that the transformation
     * applied to the geometry is the identity.
     */
    RenderValue<Transformation> m_transformation;

    /*!
     * Transformation from logical coordinates to material
     * coordinates
     */
    RenderValue<Transformation> m_material_transformation;

    /*!
     * The material applied
     */
    Material m_material;

    /*!
     * The \ref ItemData that the active shader
     * uses. An invalid value indicates that the active
     * shader does not require any item data.
     */
    ItemData m_item_data;

    /*!
     * if valid, provides the mask to clip the item
     * against.
     *
     * NOTE: it is an error if \ref m_clip_mask is valid
     *       when the surface being drawn to will have
     *       the astral::image_blit_stc_mask_processing
     *       applied to it; i.e. this must be invalid for
     *       STC mask rendering.
     */
    RenderValue<const RenderClipElement*> m_clip_mask;

    /*!
     * if \ref m_clip_mask is valid, specifies what
     * filter to apply
     */
    enum filter_t m_clip_mask_filter;

    /*!
     * if m_clip_mask is valid, indicates to clip-out
     * (instead of clip-in) against the clip-mask.
     */
    bool m_clip_out;

    /*!
     * Only applies if both \ref m_clip_mask is valid
     * and ItemShader::type() of the shader used to draw
     * is ItemShader::mask_item_shader. Specifies how the
     * \ref m_clip_mask is combined with the draws emit of
     * converage and distance field values.
     */
    enum mask_item_shader_clip_mode_t m_mask_shader_clip_mode;

    /*!
     * The blend mode to apply. A backend can assume that the value of
     * ItemShader::type() of shader used to draw and the value of
     * BackendBlendMode::item_shader_type() are the same value.
     */
    BackendBlendMode m_blend_mode;

    /*!
     * If the blend mode requires a copy of the pixels in
     * the framebuffer, provides the surface for those copy
     * pixels and the transformation to that surface.
     */
    RenderValue<EmulateFramebufferFetch> m_framebuffer_copy;
  };

/*! @} */
}

#endif

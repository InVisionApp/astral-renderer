/*!
 * \file item_material.hpp
 * \brief file item_material.hpp
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

#ifndef ASTRAL_ITEM_MATERIAL_HPP
#define ASTRAL_ITEM_MATERIAL_HPP

#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/render_clip.hpp>
#include <astral/renderer/material.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * Class to specify per-pixel clipping to apply to a draw
   */
  class ItemMask
  {
  public:
    /*!
     * Ctor.
     * \param clip value for \ref m_clip_element
     * \param filter value for \ref m_filter
     * \param clip_out value for \ref m_clip_out
     */
    ItemMask(const reference_counted_ptr<const RenderClipElement> &clip = reference_counted_ptr<const RenderClipElement>(),
             enum filter_t filter = filter_linear, bool clip_out = false):
      m_clip_element(clip),
      m_filter(filter),
      m_clip_out(clip_out)
    {}

    /*!
     * If valid, this astral::ItemMask applies clipping
     */
    reference_counted_ptr<const RenderClipElement> m_clip_element;

    /*!
     * The filter to apply to the mask pixels
     */
    enum filter_t m_filter;

    /*!
     * If true, the item clipping is clipped agains the
     * complement of \ref m_clip_element
     */
    bool m_clip_out;
  };

  /*!
   * Class to specify the material and clipping applied to a
   * RenderSupportTypes::ColorItem and RenderSupportTypes::RectItem
   */
  class ItemMaterial
  {
  public:
    /*!
     * Ctor. Initializes with no mask or mask image_transformation and
     * the material as emitting white, i.e. vec4(1, 1, 1, 1).
     */
    ItemMaterial(void)
    {}

    /*!
     * Ctor to specify \ref m_material and \ref m_clip
     * \param material initial value for \ref m_material
     * \param clip specifies what, if any, clipping to apply
     */
    ItemMaterial(const Material &material, const ItemMask &clip = ItemMask()):
      m_clip(clip),
      m_material(material)
    {}

    /*!
     * Ctor to specify \ref m_material and \ref m_clip
     * \param material initial value for \ref m_material
     * \param clip specifies what, if any, clipping to apply
     */
    ItemMaterial(RenderValue<Brush> material, const ItemMask &clip = ItemMask()):
      m_clip(clip),
      m_material(material)
    {}

    /*!
     * Ctor to copy material from another ItemMaterial but to use
     * a different clipping mask.
     * \param material ItemMaterial from which to take the values of \ref
     *                 m_material_transformation_logical and \ref m_material
     * \param clip specifies what, if any, clipping to apply
     */
    ItemMaterial(const ItemMaterial &material, const ItemMask &clip):
      m_clip(clip),
      m_material(material.m_material),
      m_material_transformation_logical(material.m_material_transformation_logical)
    {}

    /*!
     * Returns true if the material emits partial
     * coverage either by the Material or
     * having a mask.
     */
    bool
    emits_partial_coverage(void) const
    {
      return m_clip.m_clip_element || m_material.emits_partial_coverage();
    }

    /*!
     * Returns true if the material uses pixels from the framebuffer
     */
    bool
    uses_framebuffer_pixels(void) const
    {
      return m_material.uses_framebuffer_pixels();
    }

    /*!
     * What, if any, clipping to apply
     */
    ItemMask m_clip;

    /*!
     * The material of the item, essentially a brush or
     * an astral::MaterialShader with data.
     */
    Material m_material;

    /*!
     * If valid, gives the transformation from logical to material
     * coordinates. If not valid, then material and logical coordinates
     * are the same.
     */
    RenderValue<Transformation> m_material_transformation_logical;
  };
/*! @} */

}


#endif

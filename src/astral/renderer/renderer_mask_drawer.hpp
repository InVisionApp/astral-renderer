/*!
 * \file renderer_mask_drawer.hpp
 * \brief file renderer_mask_drawer.hpp
 *
 * Copyright 2020 by InvisionApp.
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

#ifndef ASTRAL_RENDERER_MASK_DRAWER_HPP
#define ASTRAL_RENDERER_MASK_DRAWER_HPP

#include <astral/renderer/renderer.hpp>
#include "renderer_implement.hpp"

class astral::Renderer::Implement::MaskDrawerImage
{
public:
  MaskDrawerImage(void):
    m_mip(nullptr)
  {}

  void
  draw_mask(RenderEncoderBase encoder,
            const SubImageT<float> &mask,
            const Transformation &mask_transformation_logical,
            enum filter_t filter,
            enum mask_post_sampling_mode_t post_sampling_mode,
            enum mask_type_t mask_type,
            enum mask_channel_t mask_channel,
            const ItemMaterial &material,
            enum blend_mode_t blend_mode);

private:
  unsigned int
  number_fully_covered_elements(void) const
  {
    return m_mip->number_elements(ImageMipElement::white_element);
  }

  RectT<int>
  fully_covered_element(unsigned int I) const
  {
    return make_rect(m_mip->element_location(ImageMipElement::white_element, I),
                     m_mip->element_size(ImageMipElement::white_element, I));
  }

  unsigned int
  number_empty_elements(void) const
  {
    return m_mip->number_elements(ImageMipElement::empty_element);
  }

  RectT<int>
  empty_element(unsigned int I) const
  {
    return make_rect(m_mip->element_location(ImageMipElement::empty_element, I),
                     m_mip->element_size(ImageMipElement::empty_element, I));
  }

  virtual
  unsigned int
  number_image_elements(void) const
  {
    return m_mip->number_elements(ImageMipElement::color_element);
  }

  virtual
  RectT<int>
  image_element(unsigned int I) const
  {
    return make_rect(m_mip->element_location(ImageMipElement::color_element, I),
                     m_mip->element_size(ImageMipElement::color_element, I));
  }

  void
  draw_image_element(RenderEncoderBase encoder, unsigned int I,
                     const Material &material,
                     enum blend_mode_t blend_mode) const;

  static
  RectT<int>
  make_rect(uvec2 location, uvec2 size)
  {
    RectT<int> R;

    R.m_min_point = ivec2(location);
    R.m_max_point = R.m_min_point + ivec2(size);
    return R;
  }

  const ImageMipElement *m_mip;
};

#endif

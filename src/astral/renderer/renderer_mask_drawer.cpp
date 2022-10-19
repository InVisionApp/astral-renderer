/*!
 * \file renderer_mask_drawer.cpp
 * \brief file renderer_mask_drawer.cpp
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

#include "renderer_mask_drawer.hpp"
#include "renderer_virtual_buffer.hpp"

///////////////////////////////////////////////////
// astral::Renderer::Implement::MaskDrawerImage methods
void
astral::Renderer::Implement::MaskDrawerImage::
draw_mask(RenderEncoderBase encoder,
          const SubImageT<float> &mask,
          const Transformation &in_mask_transformation_logical,
          enum filter_t filter,
          enum mask_post_sampling_mode_t post_sampling_mode,
          enum mask_type_t mask_type,
          enum mask_channel_t mask_channel,
          const ItemMaterial &material,
          enum blend_mode_t blend_mode)
{
  VirtualBuffer &vb(encoder.virtual_buffer());
  Renderer::Implement::DrawCommandList *cmd_list(vb.command_list());

  if (!cmd_list)
    {
      return;
    }

  RenderEncoderBase::AutoRestore restorer(encoder);

  /* the transformation provided is the transformation to the sub-image
   * specified by mask.m_min_corner and mask.m_size. The draw below is
   * executed in the coordinate system of the entirity of mask.m_image;
   * thus we need to move (0, 0) to mask.m_min_corner.
   */
  Transformation mask_transformation_logical(Transformation(mask.m_min_corner) * in_mask_transformation_logical);
  Transformation logical_transformation_mask(mask_transformation_logical.inverse());
  Rect bounds;
  ItemMaterial xform_material(material);

  encoder.begin_pause_snapshot();

  /* We need to set xform_material.m_material_transformation_logical
   * needs to map from mask coordinate to material coordinates.
   */
  if (material.m_material_transformation_logical.valid())
    {
      Transformation tmp;

      tmp = material.m_material_transformation_logical.value() * logical_transformation_mask;
      xform_material.m_material_transformation_logical = encoder.create_value(tmp);
    }
  else
    {
      xform_material.m_material_transformation_logical = encoder.create_value(logical_transformation_mask);
    }

  /* We must explicitely add the dependency to mask.m_mask
   * because MaskedRectShader does not use the ID
   * of the Image in its shader data, instead it uses
   * the values of the return values of various methods of
   * ImageMipElement directly.
   */
  vecN<ImageID, 1> dep_mask(mask.m_image.ID());

  m_mip = mask.m_image.mip_chain().front().get();

  bounds.m_min_point = mask.m_min_corner;
  bounds.m_max_point = mask.m_min_corner + mask.m_size;

  /* the entire draw takes place in mask coordinates */
  encoder.concat(logical_transformation_mask);

  for (unsigned int i = 0, endi = number_image_elements(); i < endi; ++i)
    {
      vecN<gvec4, MaskedRectShader::item_data_size> item_data;
      uvec2 tile_id(m_mip->element_tile_id(ImageMipElement::color_element, i));
      Rect rgn;

      rgn = MaskedRectShader::pack_item_data(*m_mip, tile_id, bounds,
                                             post_sampling_mode, mask_type,
                                             mask_channel, filter, item_data);

      if (rgn.width() > 0.0f && rgn.height() > 0.0f)
        {
          RenderSupportTypes::RectItem item(*encoder.default_shaders().m_masked_rect_shader,
                                            encoder.create_item_data(item_data, no_item_data_value_mapping, dep_mask));
          encoder.draw_custom_rect(rgn, item, xform_material, blend_mode);
        }
    }

  if (post_sampling_mode == mask_post_sampling_mode_direct)
    {
      for (unsigned int i = 0, endi = number_fully_covered_elements(); i < endi; ++i)
        {
          Rect Rf;

          Rect::compute_intersection(Rect(fully_covered_element(i)), bounds, &Rf);
          if (Rf.width() > 0.0f && Rf.height() > 0.0f)
            {
              encoder.draw_rect(Rf, false, xform_material, blend_mode);
            }
        }
    }
  else
    {
      for (unsigned int i = 0, endi = number_empty_elements(); i < endi; ++i)
        {
          Rect Rf;

          Rect::compute_intersection(Rect(fully_covered_element(i)), bounds, &Rf);
          if (Rf.width() > 0.0f && Rf.height() > 0.0f)
            {
              encoder.draw_rect(Rf, false, xform_material, blend_mode);
            }
        }
    }

  #if 0 //change to one to draw the region that the mask buffer covers
    {
      RenderValue<Brush> br[4], wh, em, bl, gr;

      br[0] = encoder.create_value(Brush().base_color(vec4(1.0f, 0.0f, 0.0f, 0.25f)));
      br[1] = encoder.create_value(Brush().base_color(vec4(0.0f, 1.0f, 0.0f, 0.25f)));
      br[2] = encoder.create_value(Brush().base_color(vec4(0.0f, 0.0f, 1.0f, 0.25f)));
      br[3] = encoder.create_value(Brush().base_color(vec4(1.0f, 1.0f, 0.0f, 0.25f)));
      wh = encoder.create_value(Brush().base_color(vec4(1.0f, 1.0f, 1.0f, 1.0f)));
      em = encoder.create_value(Brush().base_color(vec4(1.0f, 0.0f, 1.0f, 0.25f)));
      bl = encoder.create_value(Brush().base_color(vec4(0.0f, 0.0f, 0.0f, 1.0f)));
      gr = encoder.create_value(Brush().base_color(vec4(0.5f, 0.5f, 0.5f, 0.25f)));

      for (unsigned int i = 0, endi = number_image_elements(); i < endi; ++i)
        {
          Rect R(image_element(i));
          uvec2 tile_id(m_mip->element_tile_id(ImageMipElement::color_element, i));

          encoder.draw_rect(R, false, br[(tile_id.x() + tile_id.y()) & 3]);
          encoder.draw_rect(Rect(R).size(R.width(), 2.0f), false, wh);
          encoder.draw_rect(Rect(R).size(2.0f, R.height()), false, wh);
        }

      for (unsigned int i = 0, endi = number_fully_covered_elements(); i < endi; ++i)
        {
          Rect R(fully_covered_element(i));

          encoder.draw_rect(R, false, gr);
          encoder.draw_rect(Rect(R).size(R.width(), 2.0f), false, wh);
          encoder.draw_rect(Rect(R).size(2.0f, R.height()), false, wh);
        }

      for (unsigned int i = 0, endi = number_empty_elements(); i < endi; ++i)
        {
          Rect R(empty_element(i));

          encoder.draw_rect(R, false, em);
          encoder.draw_rect(Rect(R).size(R.width(), 2.0f), false, bl);
          encoder.draw_rect(Rect(R).size(2.0, R.height()), false, bl);
        }
    }
  #endif

  encoder.end_pause_snapshot();
}

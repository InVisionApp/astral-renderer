/*!
 * \file font.cpp
 * \brief file font.cpp
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

#include <astral/util/math.hpp>
#include <astral/text/font.hpp>
#include <astral/renderer/static_data.hpp>

astral::Font::
Font(Typeface &typeface, float pixel_size):
  m_typeface(&typeface),
  m_pixel_size(pixel_size)
{
  if (m_typeface->is_scalable())
    {
      const auto &M(m_typeface->scalable_metrics());

      m_fixed_size_index = -1;
      m_scaling_factor = m_pixel_size / M.m_units_per_EM;
      m_metrics = M;
    }
  else
    {
      c_array<const TypefaceMetricsFixedSize> sizes;
      float diff;

      sizes = m_typeface->fixed_metrics();
      ASTRALassert(!sizes.empty());

      m_fixed_size_index = 0;
      diff = t_abs(sizes[0].m_pixel_size - m_pixel_size);
      for (unsigned int i = 1; i < sizes.size(); ++i)
        {
          float d;

          d = t_abs(sizes[i].m_pixel_size - m_pixel_size);
          if (d < diff)
            {
              diff = d;
              m_fixed_size_index = i;
            }
        }
      m_scaling_factor = m_pixel_size / sizes[m_fixed_size_index].m_pixel_size;
      m_metrics = sizes[m_fixed_size_index];
    }

  m_metrics.m_height *= m_scaling_factor;
}

const astral::GlyphMetrics&
astral::Font::
glyph_metrics(Glyph glyph, SkewParameters skew,
              GlyphMetrics *scale_metrics) const
{
  const astral::GlyphMetrics *return_value;

  ASTRALassert(&glyph.typeface() == &typeface());
  if (m_fixed_size_index == -1)
    {
      return_value = &glyph.scalable_metrics();
    }
  else
    {
      return_value = &glyph.fixed_metrics(m_fixed_size_index);
    }

  if (scale_metrics)
    {
      scale_metrics->m_horizontal_layout_offset = m_scaling_factor * return_value->m_horizontal_layout_offset;
      scale_metrics->m_vertical_layout_offset = m_scaling_factor * return_value->m_vertical_layout_offset;
      scale_metrics->m_size = m_scaling_factor * return_value->m_size;
      scale_metrics->m_advance = m_scaling_factor * return_value->m_advance;

      scale_metrics->m_horizontal_layout_offset.x() *= skew.m_scale_x;
      scale_metrics->m_vertical_layout_offset.x() *= skew.m_scale_x;
      scale_metrics->m_size.x() *= skew.m_scale_x;
      scale_metrics->m_advance.x() *= skew.m_scale_x;
    }

  return *return_value;
}

astral::reference_counted_ptr<const astral::StaticData>
astral::Font::
image_render_data(RenderEngine &engine, Glyph glyph,
                  reference_counted_ptr<const Image> *out_image) const
{
  reference_counted_ptr<const StaticData> return_value;

  ASTRALassert(&glyph.typeface() == &typeface());
  if (m_fixed_size_index != -1)
    {
      return_value = glyph.image_render_data(engine, m_fixed_size_index, out_image);
    }
  else if (out_image)
    {
      *out_image = nullptr;
    }

  return return_value;
}

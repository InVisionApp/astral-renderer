/*!
 * \file ImageCompare.cpp
 * \brief ImageCompare.cpp
 *
 * Copyright 2022 by InvisionApp.
 *
 * Contact kevinrogovin@invisionapp.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 */

#include "ImageCompare.hpp"

/* Comparison algorithm lifted from https://github.com/mapbox/pixelmatch */

static
astral::vec3
yiq_from_rgb(astral::vec3 rgb)
{
  astral::vec3 yiq;
  float r(rgb[0]), g(rgb[1]), b(rgb[2]);

  yiq[0] = r * 0.29889531f + g * 0.58662247f + b * 0.11448223f;
  yiq[1] = r * 0.59597799f - g * 0.27417610f - b * 0.32180189f;
  yiq[2] = r * 0.21147017f - g * 0.52261711f + b * 0.31114694f;

  return yiq;
}

static
astral::vec3
yiq_from_rgba(astral::u8vec4 rgba)
{
  astral::vec3 rgb(rgba.x(), rgba.y(), rgba.z());

  if (rgba.w() < 255)
    {
      float f;
      const astral::vec3 white(255.0f);

      f = static_cast<float>(rgba.w()) / 255.0f;
      rgb = white + (rgb - white) * f;
    }

  return yiq_from_rgb(rgb);
}

static
bool
pixel_colors_different(float thresh_sq, astral::u8vec4 a, astral::u8vec4 b, astral::vec3* out_yiq_a, astral::vec3* out_yiq_b)
{
  *out_yiq_a = yiq_from_rgba(a);
  if (a == b)
    {
      return false;
    }

  astral::vec3 raw_delta;
  float delta_sq;

  *out_yiq_b = yiq_from_rgba(b);

  raw_delta = *out_yiq_a - *out_yiq_b;
  delta_sq = 0.5053f * raw_delta[0] * raw_delta[0] + 0.299f * raw_delta[1] * raw_delta[1] + 0.1957f * raw_delta[2] * raw_delta[2];

  return thresh_sq <= delta_sq;
}

ImageCompare::
ImageCompare(Options options,
             astral::ivec2 sizeA, astral::c_array<astral::u8vec4> dataA,
             astral::ivec2 sizeB, astral::c_array<astral::u8vec4> dataB):
  m_difference_count(0),
  m_size(astral::t_max(sizeA.x(), sizeB.x()), astral::t_max(sizeA.y(), sizeB.y())),
  m_diff_image(m_size.x() * m_size.y())
{
  float thresh_sq;

  thresh_sq = 35215.0f * options.m_thresh * options.m_thresh;
  for (int y = 0; y < m_size.y(); ++y)
    {
      int yA, yB;

      yA = astral::t_min(y, sizeA.y() - 1);
      yB = astral::t_min(y, sizeB.y() - 1);

      for (int x = 0; x < m_size.x(); ++x)
        {
          int xA, xB, lA, lB, l;
          astral::u8vec4 cD;
          astral::vec3 yiqA, yiqB;

          xA = astral::t_min(x, sizeA.x() - 1);
          xB = astral::t_min(x, sizeB.x() - 1);

          l = x + y * m_size.x();
          lA = xA + yA * sizeA.x();
          lB = xB + yB * sizeB.x();

          if (pixel_colors_different(thresh_sq, dataA[lA], dataB[lB], &yiqA, &yiqB))
            {
              ++m_difference_count;
              m_diff_image[l] = astral::u8vec4(255, 0, 0, 255);
            }
          else
            {
              uint8_t lum;

              lum = static_cast<uint8_t>(astral::t_min(255.0f, astral::t_max(0.0f, yiqA[0])));
              m_diff_image[l] = astral::u8vec4(lum, lum, lum, 255);
            }
        }
    }
}

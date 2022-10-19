/*!
 * \file ImageCompare.hpp
 * \brief ImageCompare.hpp
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


#ifndef ASTRAL_DEMO_IMAGE_COMPARE_HPP
#define ASTRAL_DEMO_IMAGE_COMPARE_HPP

#include <astral/util/vecN.hpp>
#include <astral/util/c_array.hpp>

class ImageCompare
{
public:
  class Options
  {
  public:
    Options(float thresh = 0.1f):
      m_thresh(thresh)
    {}

    float m_thresh;
  };

  ImageCompare(Options options,
               astral::ivec2 sizeA, astral::c_array<astral::u8vec4> dataA,
               astral::ivec2 sizeB, astral::c_array<astral::u8vec4> dataB);

  /* number of pixels different */
  unsigned int m_difference_count;

  /* size of image */
  astral::ivec2 m_size;

  /* image showing different in red and rest with luminance value of input images */
  std::vector<astral::u8vec4> m_diff_image;
};

#endif

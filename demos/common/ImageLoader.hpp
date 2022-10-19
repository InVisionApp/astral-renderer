/*!
 * \file ImageLoader.hpp
 * \brief ImageLoader.hpp
 *
 * Adapted from: ImageLoader.hpp of FastUIDraw (https://github.com/intel/fastuidraw):
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */
#ifndef ASTRAL_DEMO_IMAGELOADER_HPP
#define ASTRAL_DEMO_IMAGELOADER_HPP

#include <SDL_image.h>
#include <vector>
#include <stdint.h>
#include <astral/util/vecN.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/reference_counted.hpp>
#include <astral/renderer/image.hpp>

astral::ivec2
load_image_to_array(const SDL_Surface *img,
                    std::vector<astral::u8vec4> &out_bytes,
                    bool flip = false);

astral::ivec2
load_image_to_array(const std::string &pfilename,
                    std::vector<astral::u8vec4> &out_bytes,
                    bool flip = false);

void
create_mipmap_level(astral::ivec2 sz,
                    astral::c_array<const astral::u8vec4> in_data,
                    std::vector<astral::u8vec4> &out_data);

class ImageLoaderData
{
public:
  ImageLoaderData(const std::string &pfilename, bool flip = false);
  ImageLoaderData(astral::ivec2 sz, std::vector<astral::u8vec4> &steal_pixels);

  astral::uvec2
  dimensions(void) const
  {
    return m_dimensions;
  }

  int
  width(void) const
  {
    return m_dimensions.x();
  }

  int
  height(void) const
  {
    return m_dimensions.y();
  }

  bool
  non_empty(void)
  {
    return m_dimensions.x() > 0u
      && m_dimensions.y() > 0u;
  }

  astral::c_array<const astral::u8vec4>
  mipmap_pixels(unsigned int M) const
  {
    return (M < m_mipmap_levels.size()) ?
      make_c_array(m_mipmap_levels[M]):
      astral::c_array<const astral::u8vec4>();
  }

  astral::reference_counted_ptr<astral::Image>
  create_image(astral::ImageAtlas &atlas);

private:
  void
  generate_mipmap_pixels(void);

  astral::uvec2 m_dimensions;
  std::vector<std::vector<astral::u8vec4>> m_mipmap_levels;
};

typedef astral::reference_counted<ImageLoaderData>::NonConcurrent ImageLoader;

#endif

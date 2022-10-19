/*!
 * Adapted from: WRATHSDLImageSupport.cpp of WRATH:
 *
 * Copyright 2013 by Nomovok Ltd.
 * Contact: info@nomovok.com
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 */

#include <iostream>
#include <cstring>
#include "ImageLoader.hpp"

namespace
{
  class SDL_Image
  {
  public:
    static
    SDL_Image&
    library(void)
    {
      static SDL_Image R;
      return R;
    }

    SDL_Surface*
    Load(const char *filename)
    {
      return IMG_Load(filename);
    }

    SDL_Surface*
    Load_RW(SDL_RWops *src, int freesrc)
    {
      return IMG_Load_RW(src, freesrc);
    }

    SDL_Surface*
    LoadTyped_RW(SDL_RWops *src, int freesrc, const char *type)
    {
      return IMG_LoadTyped_RW(src, freesrc, type);
    }

  private:
    SDL_Image(void)
    {
      IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_TIF);
    }

    ~SDL_Image()
    {
      IMG_Quit();
    }
  };

  astral::ivec2
  load_image_worker(SDL_Surface *img, std::vector<astral::u8vec4> &bits_data, bool flip)
  {
    SDL_PixelFormat *fmt;

    fmt = img->format;
    // Store some useful variables
    SDL_LockSurface(img);

    int w(img->w), h(img->h);
    int pitch(img->pitch);
    int bytes_per_pixel(img->format->BytesPerPixel);

    const unsigned char *surface_data;
    surface_data = reinterpret_cast<const unsigned char*>(img->pixels);

    // Resize the vector holding the bit data
    bits_data.resize(w * h);

    for(int y=0; y<h; ++y)
      {
        int source_y = (flip) ? h-1-y : y;

        for(int x =0 ; x < w; ++x)
          {
            int src_L, dest_L;
            Uint8 red, green, blue, alpha;
            Uint32 temp, pixel;

            src_L = source_y * pitch + x * bytes_per_pixel;
            dest_L = y * w + x;

            pixel = *reinterpret_cast<const Uint32*>(surface_data + src_L);

            /* Get Alpha component */
            temp = pixel & fmt->Amask;
            temp = temp >> fmt->Ashift;
            temp = temp << fmt->Aloss;
            alpha = (Uint8)temp;

            temp = pixel & fmt->Rmask;
            temp = temp >> fmt->Rshift;
            temp = temp << fmt->Rloss;
            red = temp;

            temp = pixel & fmt->Gmask;
            temp = temp >> fmt->Gshift;
            temp = temp << fmt->Gloss;
            green = temp;

            temp = pixel & fmt->Bmask;
            temp = temp >> fmt->Bshift;
            temp = temp << fmt->Bloss;
            blue = temp;

            bits_data[dest_L].x() = red;
            bits_data[dest_L].y() = green;
            bits_data[dest_L].z() = blue;
            bits_data[dest_L].w() = alpha;
          }
      }
    SDL_UnlockSurface(img);

    astral::covert_to_premultiplied_alpha(bits_data.begin(), bits_data.end());

    return astral::ivec2(w, h);
  }
}

astral::ivec2
load_image_to_array(const SDL_Surface *img,
                    std::vector<astral::u8vec4> &out_bytes,
                    bool flip)
{
  SDL_Surface *q;
  astral::ivec2 R;

  if (!img)
    {
      return astral::ivec2(0,0);
    }
  q = SDL_ConvertSurfaceFormat(const_cast<SDL_Surface*>(img), SDL_PIXELFORMAT_RGBA8888, 0);
  R = load_image_worker(q, out_bytes, flip);
  SDL_FreeSurface(q);
  return R;
}


astral::ivec2
load_image_to_array(const std::string &pfilename,
                    std::vector<astral::u8vec4> &out_bytes,
                    bool flip)
{
  astral::ivec2 R;

  SDL_Surface *img;
  img = SDL_Image::library().Load(pfilename.c_str());
  R = load_image_to_array(img, out_bytes, flip);
  SDL_FreeSurface(img);
  return R;
}

void
create_mipmap_level(astral::ivec2 sz,
                    astral::c_array<const astral::u8vec4> in_data,
                    std::vector<astral::u8vec4> &out_data)
{
  int w, h;

  w = astral::t_max(1, sz.x() / 2);
  h = astral::t_max(1, sz.y() / 2);
  out_data.resize(w * h);

  for (int dst_y = 0; dst_y < h; ++dst_y)
    {
      int sy0, sy1;

      sy0 = astral::t_min(2 * dst_y, sz.y() - 1);
      sy1 = astral::t_min(2 * dst_y + 1, sz.y() - 1);

      for (int dst_x = 0; dst_x < w; ++dst_x)
        {
          int sx0, sx1;
          astral::vec4 p00, p01, p10, p11, p;

          sx0 = astral::t_min(2 * dst_x, sz.x() - 1);
          sx1 = astral::t_min(2 * dst_x + 1, sz.x() - 1);

          p00 = astral::vec4(in_data[sx0 + sy0 * sz.x()]);
          p01 = astral::vec4(in_data[sx0 + sy1 * sz.x()]);
          p10 = astral::vec4(in_data[sx1 + sy0 * sz.x()]);
          p11 = astral::vec4(in_data[sx1 + sy1 * sz.x()]);

          p = 0.25f * (p00 + p01 + p10 + p11);

          for (unsigned int c = 0; c < 4; ++c)
            {
              float q;
              q = astral::t_min(p[c], 255.0f);
              q = astral::t_max(q, 0.0f);
              out_data[dst_x + dst_y * w][c] = static_cast<unsigned int>(q);
            }
        }
    }
}

ImageLoaderData::
ImageLoaderData(const std::string &pfilename, bool flip):
  m_dimensions(0, 0)
{
  astral::ivec2 dims;
  std::vector<astral::u8vec4> data;

  dims = load_image_to_array(pfilename, data, flip);
  if (dims.x() <= 0 || dims.y() <= 0)
    {
      return;
    }

  m_dimensions = astral::uvec2(dims);
  m_mipmap_levels.push_back(std::vector<astral::u8vec4>());
  m_mipmap_levels.back().swap(data);

  generate_mipmap_pixels();
}

ImageLoaderData::
ImageLoaderData(astral::ivec2 sz, std::vector<astral::u8vec4> &data):
  m_dimensions(sz)
{
  m_mipmap_levels.push_back(std::vector<astral::u8vec4>());
  m_mipmap_levels.back().swap(data);

  generate_mipmap_pixels();
}

void
ImageLoaderData::
generate_mipmap_pixels(void)
{
  astral::ivec2 dims(m_dimensions);
  while(dims.x() >= 2 || dims.y() >= 2)
    {
      astral::ivec2 wh;
      std::vector<astral::u8vec4> data;

      wh.x() = astral::t_max(dims.x(), 1);
      wh.y() = astral::t_max(dims.y(), 1);
      create_mipmap_level(wh,
                          make_c_array(m_mipmap_levels.back()),
                          data);
      m_mipmap_levels.push_back(std::vector<astral::u8vec4>());
      m_mipmap_levels.back().swap(data);
      dims /= 2;
    }
}

astral::reference_counted_ptr<astral::Image>
ImageLoaderData::
create_image(astral::ImageAtlas &atlas)
{
  astral::reference_counted_ptr<astral::Image> image;
  unsigned int w(m_dimensions.x());
  unsigned int h(m_dimensions.y());

  if (w == 0 || h == 0)
    {
      return image;
    }

  image = atlas.create_image(astral::uvec2(w, h));
  for (unsigned int LOD = 0;
       w > 0u && h > 0u && LOD < image->number_mipmap_levels();
       w >>= 1u, h >>= 1u, ++LOD)
    {
      image->set_pixels(LOD,
                        astral::ivec2(0, 0),
                        astral::ivec2(w, h),
                        w,
                        make_c_array(m_mipmap_levels[LOD]));
    }

  image->colorspace(astral::colorspace_srgb);
  return image;
}

/*!
 * \file ImageSaver.cpp
 * \brief ImageSaver.cpp
 *
 * Copyright 2022 by InvisionApp.
 *
 * Contact itaidanan@invisionapp.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 */

#include <fstream>
#include <vector>

#include "png.h"

#include "ImageSaver.hpp"

namespace
{
  void
  png_write_callback(png_structp png_ptr, png_bytep src, png_size_t size)
  {
    std::ostream *output = reinterpret_cast<std::ostream*>(png_get_io_ptr(png_ptr));
    output->write(reinterpret_cast<const char*>(src), size);
  }

  void
  png_flush_callback(png_structp png_ptr)
  {
    std::ostream *output = reinterpret_cast<std::ostream*>(png_get_io_ptr(png_ptr));
    output->flush();
  }
}

enum astral::return_code
save_png(bool input_is_with_alpha_premultiplied, astral::ivec2 size, astral::c_array<astral::u8vec4> pixels, std::ostream &os, bool flip_y)
{
  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  astral::u8vec4 *tmp = nullptr;

  if (!png)
    {
      return astral::routine_fail;
    }

  png_infop info = png_create_info_struct(png);
  if (!info)
    {
        return astral::routine_fail;
    }

  if (setjmp(png_jmpbuf(png)))
    {
        png_destroy_write_struct(&png, &info);
        if (tmp)
          {
            ASTRALfree(tmp);
          }
        return astral::routine_fail;
    }

  // Given that read_color_buffer above always returns 8-bit RGBA for which we had to allocate exactly the right about of space,
  // the only choice now is to create an 8-bit RGBA PNG which for lack of more specific information is assumed to be in sRGB format.
  png_set_IHDR(png, info, size.x(), size.y(), 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_set_sRGB(png, info, PNG_sRGB_INTENT_PERCEPTUAL);
  png_set_write_fn(png, &os, png_write_callback, png_flush_callback);
  png_write_info(png, info);

  png_byte* bytes = reinterpret_cast<png_byte*>(pixels.c_ptr());
  if (input_is_with_alpha_premultiplied)
    {
      tmp = static_cast<astral::u8vec4*>(ASTRALmalloc(sizeof(astral::u8vec4) * size.x()));
    }

  for (int y = 0; y < size.y(); ++y)
    {
      int yy;

      yy = (flip_y) ? (size.y() - 1 - y) : y;
      if (tmp)
        {
          for (int src = yy * size.x(), dst = 0; dst < size.x(); ++src, ++dst)
            {
              astral::u8vec4 v;

              v = pixels[src];
              if (v.w() != 0u && v.w() != 255u)
                {
                  float fr, fg, fb, fa, recip_fa;

                  fr = static_cast<float>(v.x());
                  fg = static_cast<float>(v.y());
                  fb = static_cast<float>(v.z());
                  fa = static_cast<float>(v.w()) / 255.0f;
                  recip_fa = 1.0f / fa;

                  fr *= recip_fa;
                  fg *= recip_fa;
                  fb *= recip_fa;

                  v.x() = astral::t_min(255, static_cast<int>(fr));
                  v.y() = astral::t_min(255, static_cast<int>(fg));
                  v.z() = astral::t_min(255, static_cast<int>(fb));
                }
              tmp[dst] = v;
            }
          png_write_row(png, reinterpret_cast<png_byte*>(tmp));
        }
      else
        {
          png_write_row(png, bytes + (yy * 4 * size.x()));
        }
    }

  png_write_end(png, info);
  png_destroy_write_struct(&png, &info);
  ASTRALfree(tmp);

  return astral::routine_success;
}

enum astral::return_code
save_png(bool input_is_with_alpha_premultiplied, astral::ivec2 size, astral::c_array<astral::u8vec4> pixels, const std::string &filepath, bool flip_y)
{
  if (filepath.empty())
    {
      return astral::routine_fail;
    }

  std::ofstream file(filepath, std::ios::out | std::ios::binary);
  if (!file)
    {
      return astral::routine_fail;
    }

  return save_png(input_is_with_alpha_premultiplied, size, pixels, file, flip_y);
}

enum astral::return_code
save_png(bool input_is_with_alpha_premultiplied, const astral::RenderTarget &target, std::ostream &os)
{
  astral::ivec2 size(target.size());
  std::vector<astral::u8vec4> pixels(size.x() * size.y());
  target.read_color_buffer(astral::ivec2(0, 0), size, astral::make_c_array(pixels));

  return save_png(input_is_with_alpha_premultiplied, size, astral::make_c_array(pixels), os);
}

enum astral::return_code
save_png(bool input_is_with_alpha_premultiplied, const astral::RenderTarget &target, const std::string &filepath)
{
  if (filepath.empty())
    {
      return astral::routine_fail;
    }

  std::ofstream file(filepath, std::ios::out | std::ios::binary);
  if (!file)
    {
      return astral::routine_fail;
    }

  return save_png(input_is_with_alpha_premultiplied, target, file);
}

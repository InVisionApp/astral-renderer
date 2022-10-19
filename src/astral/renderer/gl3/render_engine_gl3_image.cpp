/*!
 * \file render_engine_gl3_image.cpp
 * \brief file render_engine_gl3_image.cpp
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

#include <astral/util/ostream_utility.hpp>

#include <astral/renderer/gl3/render_target_gl3.hpp>
#include "render_engine_gl3_image.hpp"

///////////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::ImageBacking methods
astral::gl::RenderEngineGL3::Implement::ImageBacking::
ImageBacking(AtlasBlitter &blitter, enum backing_type_t tp,
             unsigned int width_height, unsigned int num_layers):
  m_width_height(width_height),
  m_number_layers(0),
  m_texture(0u),
  m_tp(tp),
  m_blitter(&blitter),
  m_staging_width_height(512),
  m_staging_texture(0u),
  m_staging_atlas(ImageAtlas::log2_tile_size,
                  uvec2(m_staging_width_height >> ImageAtlas::log2_tile_size),
                  1u),
  m_staging_range_uploaded(range_type<int>(m_staging_width_height, 0),
                           range_type<int>(m_staging_width_height, 0)),
  m_staging_pixels(m_staging_width_height * m_staging_width_height, 0u),
  m_cpu_uploads_pending(false)
{
  ASTRALassert((width_height & (ImageAtlas::tile_size - 1u)) == 0u);
  ASTRALassert(width_height <= max_width_height);
  ASTRALassert(can_support_number_layers(m_tp, num_layers));

  if (tp == index_backing)
    {
      m_internal_format = ASTRAL_GL_R32UI;
      m_external_format = ASTRAL_GL_RED_INTEGER;
      m_external_type = ASTRAL_GL_UNSIGNED_INT;
      m_min_filter = ASTRAL_GL_NEAREST;
      m_mag_filter = ASTRAL_GL_NEAREST;
      m_number_lod = 1;
    }
  else
    {
      ASTRALassert(tp == color_backing);

      m_internal_format = ASTRAL_GL_RGBA8;
      m_external_format = ASTRAL_GL_RGBA;
      m_external_type = ASTRAL_GL_UNSIGNED_BYTE;
      m_min_filter = ASTRAL_GL_LINEAR_MIPMAP_NEAREST;
      m_mag_filter = ASTRAL_GL_LINEAR;
      m_number_lod = ImageMipElement::maximum_number_of_mipmaps;
    }

  /* create the staging texture */
  astral_glGenTextures(1, &m_staging_texture);
  ASTRALassert(m_staging_texture != 0);

  astral_glBindTexture(ASTRAL_GL_TEXTURE_2D, m_staging_texture);
  astral_glTexStorage2D(ASTRAL_GL_TEXTURE_2D, m_number_lod,
                        m_internal_format,
                        m_staging_width_height,
                        m_staging_width_height);
  astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D, ASTRAL_GL_TEXTURE_MIN_FILTER, ASTRAL_GL_NEAREST);
  astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D, ASTRAL_GL_TEXTURE_MAG_FILTER, ASTRAL_GL_NEAREST);
  astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D, ASTRAL_GL_TEXTURE_WRAP_S, ASTRAL_GL_CLAMP_TO_EDGE);
  astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D, ASTRAL_GL_TEXTURE_WRAP_T, ASTRAL_GL_CLAMP_TO_EDGE);
  astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D, ASTRAL_GL_TEXTURE_MIN_LOD, 0);
  astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D, ASTRAL_GL_TEXTURE_MAX_LOD, m_number_lod - 1);

  m_src_rects.resize(m_number_lod);
  m_dst_rects.resize(m_number_lod);
  m_dst_fmts.resize(m_number_lod);
  m_post_process_windows.resize(m_number_lod);
  on_resize(num_layers);
}

astral::gl::RenderEngineGL3::Implement::ImageBacking::
~ImageBacking(void)
{
  astral_glDeleteTextures(1, &m_texture);

  /* if there is pending work to flush, there is no point to
   * flush it, so clear the atlas to make sure it does not
   * assert
   */
  if (m_cpu_uploads_pending || m_gpu_texture.m_texture != 0)
    {
      m_staging_atlas.release_all();
    }
}

void
astral::gl::RenderEngineGL3::Implement::ImageBacking::
create_texture(void)
{
  ASTRALassert(m_texture == 0u);
  if (m_number_layers > 0u)
    {
      astral_glGenTextures(1, &m_texture);
      ASTRALassert(m_texture != 0u);

      astral_glBindTexture(ASTRAL_GL_TEXTURE_2D_ARRAY, m_texture);
      astral_glTexStorage3D(ASTRAL_GL_TEXTURE_2D_ARRAY,
                            m_number_lod,
                            m_internal_format,
                            m_width_height,
                            m_width_height,
                            m_number_layers);
      astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D_ARRAY, ASTRAL_GL_TEXTURE_MIN_FILTER, m_min_filter);
      astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D_ARRAY, ASTRAL_GL_TEXTURE_MAG_FILTER, m_mag_filter);
      astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D_ARRAY, ASTRAL_GL_TEXTURE_WRAP_S, ASTRAL_GL_CLAMP_TO_EDGE);
      astral_glTexParameteri(ASTRAL_GL_TEXTURE_2D_ARRAY, ASTRAL_GL_TEXTURE_WRAP_T, ASTRAL_GL_CLAMP_TO_EDGE);
    }
}

void
astral::gl::RenderEngineGL3::Implement::ImageBacking::
flush_cpu(void)
{
  int offset;

  m_cpu_uploads_pending = false;
  if (m_staging_range_uploaded.x().difference() <= 0
      || m_staging_range_uploaded.y().difference() <= 0)
    {
      return;
    }

  /* upload the texels to the staging buffer; note that
   * ASTRAL_GL_UNPACK_ROW_LENGTH is set as the width of the
   * staging texture since that is the distance between
   * each row of texels of the staging buffer.
   */
  offset = m_staging_range_uploaded.x().m_begin + m_staging_width_height * m_staging_range_uploaded.y().m_begin;
  astral_glPixelStorei(ASTRAL_GL_UNPACK_ROW_LENGTH, m_staging_width_height);
  astral_glBindTexture(ASTRAL_GL_TEXTURE_2D, m_staging_texture);
  astral_glTexSubImage2D(ASTRAL_GL_TEXTURE_2D, 0,
                         m_staging_range_uploaded.x().m_begin,
                         m_staging_range_uploaded.y().m_begin,
                         m_staging_range_uploaded.x().difference(),
                         m_staging_range_uploaded.y().difference(),
                         m_external_format, m_external_type, &m_staging_pixels[offset]);
  astral_glPixelStorei(ASTRAL_GL_UNPACK_ROW_LENGTH, 0);

  /* we are ALWAYS sourcing from LOD = 0 from the staging texture */
  flush_implement(AtlasBlitter::Texture().texture(m_staging_texture).lod(0).layer(-1));

  /* the funny range where begin is the m_staging_width_height
   * and the end is 0 is so that the range is actually empty.
   */
  m_staging_range_uploaded.x() = range_type<int>(m_staging_width_height, 0);
  m_staging_range_uploaded.y() = range_type<int>(m_staging_width_height, 0);

  /* clear the staging atlas since we've uploaded all */
  m_staging_atlas.release_all();
}

void
astral::gl::RenderEngineGL3::Implement::ImageBacking::
flush_gpu(void)
{
  if (m_gpu_texture.m_texture != 0u)
    {
      flush_implement(m_gpu_texture);
      m_gpu_texture.m_texture = 0u;
    }
}

void
astral::gl::RenderEngineGL3::Implement::ImageBacking::
flush_implement(const AtlasBlitter::Texture &src_texture)
{
  uvec2 dims(m_width_height, m_width_height);
  AtlasBlitter::Texture dst_texture;

  dst_texture.texture(m_texture);
  for (unsigned int lod = 0; lod < m_number_lod; ++lod)
    {
      dst_texture.lod(lod);
      for (unsigned int L = 0; L < m_number_layers; ++L)
        {
          ASTRALassert(m_src_rects[lod][L].size() == m_dst_rects[lod][L].size());
          if (!m_src_rects[lod][L].empty())
            {
              dst_texture.layer(L);
              if (m_tp == index_backing)
                {
                  ASTRALassert(lod == 0u);
                  ASTRALassert(m_dst_fmts[lod][L].empty());
                  m_blitter->blit_pixels_uint(src_texture, make_c_array(m_src_rects[lod][L]),
                                              dst_texture, dims, make_c_array(m_dst_rects[lod][L]));
                }
              else
                {
                  ASTRALassert(m_dst_fmts[lod][L].empty() || m_dst_fmts[lod][L].size() == m_dst_rects[lod][L].size());
                  m_blitter->blit_pixels(src_texture, make_c_array(m_src_rects[lod][L]),
                                         dst_texture, dims, make_c_array(m_dst_rects[lod][L]),
                                         make_c_array(m_dst_fmts[lod][L]),
                                         make_c_array(m_post_process_windows[lod][L]));
                }
              m_src_rects[lod][L].clear();
              m_dst_rects[lod][L].clear();
              m_dst_fmts[lod][L].clear();
              m_post_process_windows[lod][L].clear();
            }
          ASTRALassert(m_src_rects[lod][L].empty());
          ASTRALassert(m_dst_rects[lod][L].empty());
          ASTRALassert(m_dst_fmts[lod][L].empty());
          ASTRALassert(m_post_process_windows[lod][L].empty());
        }
    }
}

void
astral::gl::RenderEngineGL3::Implement::ImageBacking::
upload_texels(unsigned int lod, uvec3 location, uvec2 size, unsigned int row_width, const void *ptexels)
{
  const TileAllocator::Tile *tile;
  unsigned int log2_width, log2_height;
  const uint32_t *texels;
  Rect srcR, dstR;

  if (lod >= m_number_lod)
    {
      return;
    }

  if (m_gpu_texture.m_texture != 0u)
    {
      flush_gpu();
    }

  ASTRALassert(m_gpu_texture.m_texture == 0u);

  log2_width = uint32_log2_floor(next_power_of_2(size.x()));
  log2_height = uint32_log2_floor(next_power_of_2(size.y()));
  texels = static_cast<const uint32_t*>(ptexels);

  ASTRALassert(log2_width <= m_staging_atlas.log2_max_tile_size().x());
  ASTRALassert(log2_height <= m_staging_atlas.log2_max_tile_size().y());
  ASTRALassert(1u == m_staging_atlas.number_layers());
  ASTRALassert(m_src_rects.size() == m_number_lod);
  ASTRALassert(m_dst_rects.size() == m_number_lod);
  ASTRALassert(m_post_process_windows.size() == m_number_lod);
  ASTRALassert(m_src_rects[lod].size() == m_number_layers);
  ASTRALassert(m_dst_rects[lod].size() == m_number_layers);
  ASTRALassert(m_post_process_windows[lod].size() == m_number_layers);
  ASTRALassert(location.z() < m_number_layers);

  tile = m_staging_atlas.allocate_tile(log2_width, log2_height);
  if (!tile)
    {
      flush_cpu();
      tile = m_staging_atlas.allocate_tile(log2_width, log2_height);
    }

  ASTRALassert(tile);
  ASTRALassert(tile->location().z() == 0u);

  /* copy the pixels to m_staging_pixels */
  for (unsigned int y = 0,
         src_offset = 0,
         dst_offset = tile->location().x() + tile->location().y() * m_staging_width_height;
       y < size.y();
       ++y, src_offset += row_width, dst_offset += m_staging_width_height)
    {
      ASTRALassert(dst_offset + size.x() <= m_staging_pixels.size());
      std::copy(texels + src_offset, texels + src_offset + size.x(), &m_staging_pixels[dst_offset]);
    }

  srcR.m_min_point = vec2(tile->location().x(), tile->location().y());
  srcR.m_max_point = srcR.m_min_point + vec2(size);

  dstR.m_min_point = vec2(location.x(), location.y());
  dstR.m_max_point = dstR.m_min_point + vec2(size);

  m_src_rects[lod][location.z()].push_back(srcR);
  m_dst_rects[lod][location.z()].push_back(dstR);
  ASTRALassert(m_dst_fmts[lod][location.z()].empty());
  ASTRALassert(m_post_process_windows[lod][location.z()].empty());

  m_staging_range_uploaded.x().absorb(range_type<int>(tile->location().x(), tile->location().x() + size.x()));
  m_staging_range_uploaded.y().absorb(range_type<int>(tile->location().y(), tile->location().y() + size.y()));

  m_cpu_uploads_pending = true;
}

void
astral::gl::RenderEngineGL3::Implement::ImageBacking::
copy_pixels(unsigned int dst_lod, uvec3 dst_location, uvec2 size,
            const AtlasBlitter::Texture &src_texture,
            uvec2 src_location, RectT<int> post_process_window,
            enum image_blit_processing_t fmt,
            bool permute_src_x_y_coordinates)
{
  RectT<int> srcR, dstR;

  if (m_cpu_uploads_pending)
    {
      flush_cpu();
    }

  ASTRALassert(!m_cpu_uploads_pending);
  if (m_gpu_texture.m_texture != src_texture.m_texture
      || m_gpu_texture.m_lod != src_texture.m_lod
      || m_gpu_texture.m_layer != src_texture.m_layer)
    {
      flush_gpu();
      m_gpu_texture = src_texture;
    }

  ASTRALassert(m_src_rects.size() == m_number_lod);
  ASTRALassert(m_dst_rects.size() == m_number_lod);
  ASTRALassert(m_dst_fmts.size() == m_number_lod);
  ASTRALassert(m_src_rects[dst_lod].size() == m_number_layers);
  ASTRALassert(m_dst_rects[dst_lod].size() == m_number_layers);
  ASTRALassert(m_dst_fmts[dst_lod].size() == m_number_layers);
  ASTRALassert(dst_location.z() < m_number_layers);

  srcR.m_min_point = ivec2(src_location);
  srcR.m_max_point = srcR.m_min_point + ivec2(size);

  dstR.m_min_point = ivec2(dst_location);
  dstR.m_max_point = dstR.m_min_point + ivec2(size);

  m_src_rects[dst_lod][dst_location.z()].push_back(srcR);
  m_post_process_windows[dst_lod][dst_location.z()].push_back(post_process_window);
  m_dst_rects[dst_lod][dst_location.z()].push_back(dstR);
  m_dst_fmts[dst_lod][dst_location.z()].push_back(fmt);

  if (permute_src_x_y_coordinates)
    {
      m_src_rects[dst_lod][dst_location.z()].back().permute_xy();
      m_post_process_windows[dst_lod][dst_location.z()].back().permute_xy();
    }
}

void
astral::gl::RenderEngineGL3::Implement::ImageBacking::
downsample_pixels(unsigned int dst_lod, uvec3 dst_location, uvec2 dst_size,
                  const AtlasBlitter::Texture &src_texture,
                  uvec2 src_location, enum downsampling_processing_t downsamping_processing,
                  bool permute_src_x_y_coordinates)
{
  RectT<int> srcR, dstR;

  if (m_cpu_uploads_pending)
    {
      flush_cpu();
    }

  ASTRALassert(!m_cpu_uploads_pending);
  if (m_gpu_texture.m_texture != src_texture.m_texture
      || m_gpu_texture.m_lod != src_texture.m_lod
      || m_gpu_texture.m_layer != src_texture.m_layer)
    {
      flush_gpu();
      m_gpu_texture = src_texture;
    }

  ASTRALassert(m_src_rects.size() == m_number_lod);
  ASTRALassert(m_dst_rects.size() == m_number_lod);
  ASTRALassert(m_dst_fmts.size() == m_number_lod);
  ASTRALassert(m_src_rects[dst_lod].size() == m_number_layers);
  ASTRALassert(m_dst_rects[dst_lod].size() == m_number_layers);
  ASTRALassert(m_dst_fmts[dst_lod].size() == m_number_layers);
  ASTRALassert(dst_location.z() < m_number_layers);

  srcR.m_min_point = ivec2(src_location);
  srcR.m_max_point = srcR.m_min_point + ivec2(dst_size * 2);

  dstR.m_min_point = ivec2(dst_location);
  dstR.m_max_point = dstR.m_min_point + ivec2(dst_size);

  m_src_rects[dst_lod][dst_location.z()].push_back(srcR);
  m_dst_rects[dst_lod][dst_location.z()].push_back(dstR);
  m_dst_fmts[dst_lod][dst_location.z()].push_back(downsamping_processing);

  if (permute_src_x_y_coordinates)
    {
      m_src_rects[dst_lod][dst_location.z()].back().permute_xy();
    }

  /* downsampling does not use the post-processing window, so it does not matter what we put */
  m_post_process_windows[dst_lod][dst_location.z()].push_back(AtlasBlitter::PostProcessWindow());
}

void
astral::gl::RenderEngineGL3::Implement::ImageBacking::
on_resize(unsigned int new_size)
{
  ASTRALassert(can_support_number_layers(m_tp, new_size));
  if (new_size <= m_number_layers)
    {
      return;
    }

  for (unsigned int lod = 0; lod < m_number_lod; ++lod)
    {
      m_src_rects[lod].resize(new_size);
      m_dst_rects[lod].resize(new_size);
      m_dst_fmts[lod].resize(new_size);
      m_post_process_windows[lod].resize(new_size);
    }

  astral_GLuint old_texture(m_texture);
  unsigned int old_size(m_number_layers);

  m_number_layers = new_size;
  m_texture = 0u;
  create_texture();

  if (old_texture != 0u)
    {
      uvec2 dims(m_width_height, m_width_height);
      AtlasBlitter::Texture src_texture, dst_texture;
      Rect rect;

      src_texture.texture(old_texture);
      dst_texture.texture(m_texture);

      rect.m_min_point = vec2(0, 0);

      for (unsigned int L = 0; L < old_size; ++L)
        {
          src_texture.layer(L);
          dst_texture.layer(L);

          for (unsigned int lod = 0; lod < m_number_lod; ++lod)
            {
              src_texture.lod(lod);
              dst_texture.lod(lod);

              rect.m_max_point = vec2(dims.x() >> lod, dims.y() >> lod);
              if (m_tp == index_backing)
                {
                  ASTRALassert(lod == 0u);
                  m_blitter->blit_pixels_uint(src_texture, rect,
                                              dst_texture, dims, rect);
                }
              else
                {
                  m_blitter->blit_pixels(src_texture, rect,
                                         dst_texture, dims, rect);
                }
            }
        }

      /* delete the old texture */
      astral_glDeleteTextures(1, &old_texture);
    }
}

//////////////////////////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::ImageColorBacking methods
void
astral::gl::RenderEngineGL3::Implement::ImageColorBacking::
copy_pixels(unsigned int lod, uvec3 location, uvec2 size,
            ColorBuffer &src_buffer, uvec2 src_location,
            const RectT<int> &post_process_window,
            enum image_blit_processing_t fmt,
            bool permute_src_x_y_coordinates)
{
  ColorBufferGL *gl_src_buffer;
  AtlasBlitter::Texture src_texture;

  ASTRALassert(dynamic_cast<ColorBufferGL*>(&src_buffer));
  gl_src_buffer = static_cast<ColorBufferGL*>(&src_buffer);

  src_texture
    .texture(gl_src_buffer->texture()->texture())
    .layer(gl_src_buffer->layer())
    .lod(0);

  m_backing.copy_pixels(lod, location, size, src_texture, src_location,
                        post_process_window, fmt, permute_src_x_y_coordinates);
}

void
astral::gl::RenderEngineGL3::Implement::ImageColorBacking::
downsample_pixels(unsigned int lod, uvec3 location, uvec2 size,
                  ColorBuffer &src_buffer, uvec2 src_location,
                  enum downsampling_processing_t downsamping_processing,
                  bool permute_src_x_y_coordinates)
{
  ColorBufferGL *gl_src_buffer;
  AtlasBlitter::Texture src_texture;

  ASTRALassert(dynamic_cast<ColorBufferGL*>(&src_buffer));
  gl_src_buffer = static_cast<ColorBufferGL*>(&src_buffer);

  src_texture
    .texture(gl_src_buffer->texture()->texture())
    .layer(gl_src_buffer->layer())
    .lod(0);

  m_backing.downsample_pixels(lod, location, size,
                              src_texture, src_location,
                              downsamping_processing,
                              permute_src_x_y_coordinates);
}

///////////////////////////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::ImageIndexBacking methods
void
astral::gl::RenderEngineGL3::Implement::ImageIndexBacking::
upload_texels(uvec3 location, uvec2 size, c_array<const uvec3> texels)
{
  m_workroom.resize(size.x() * size.y());

  for (unsigned int y = 0; y < size.y(); ++y)
    {
      for (unsigned int x = 0; x < size.x(); ++x)
        {
          unsigned int dst, src;
          uint32_t value;

          src = dst = x + y * size.x();

          /* pack the location of a tile into 32-bits */
          value = texel_value_from_location(texels[src]);

          m_workroom[dst] = value;
        }
    }

  ASTRALassert(!m_workroom.empty());
  m_backing.upload_texels(0, location, size, size.x(), &m_workroom[0]);
}

/*!
 * \file render_engine_gl3_image.hpp
 * \brief file render_engine_gl3_image.hpp
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

#ifndef ASTRAL_RENDER_ENGINE_GL3_TILED_IMAGE_HPP
#define ASTRAL_RENDER_ENGINE_GL3_TILED_IMAGE_HPP

#include <astral/util/tile_allocator.hpp>
#include <astral/util/gl/astral_gl.hpp>
#include <astral/renderer/image.hpp>
#include <astral/renderer/gl3/render_engine_gl3.hpp>

#include "render_engine_gl3_implement.hpp"
#include "render_engine_gl3_atlas_blitter.hpp"

/* Class to:
 *  - allocate and resize a ASTRAL_GL_TEXTURE_2D_ARRAY texture
 *  - upload texels to a ASTRAL_GL_TEXTURE_2D_ARRAY
 */
class astral::gl::RenderEngineGL3::Implement::ImageBacking:astral::noncopyable
{
public:
  /* Common bit counts */
  enum common_bits:uint32_t
    {
      /* Number of bits to encode an x-coordinate or y-coordinate;
       * the value of 11 is chosen because 2^11 is 2048 which is
       * maximum texture size that WebGL2 gauarantees.
       */
      coord_num_bits = 11u,

      x_bit0 = 0u,
      y_bit0 = x_bit0 + coord_num_bits,
      layer_bit0 = y_bit0 + coord_num_bits,
    };

  /* Enumeration to describe how to pack the location of a
   * root index tile; this includes the location of tile and
   * the number of index levels. Note that the layer is 8-bits,
   * thus ImageAtlasIndexBacking can have at most only 256 layers
   */
  enum root_index_tile_location_t:uint32_t
    {
      root_index_tile_layer_num_bits = 8u,
      root_index_tile_number_levels_num_bits = 2u,

      root_index_tile_number_levels_bit0 = layer_bit0 + root_index_tile_layer_num_bits
    };
  ASTRALstatic_assert(root_index_tile_number_levels_bit0 + root_index_tile_number_levels_num_bits == 32u);

  /* Enumeration to describe how to pack the location of any tile
   * except a root-index tile.
   */
  enum generic_tile_location_t:uint32_t
    {
      generic_tile_layer_num_bits = 10u,
    };
  ASTRALstatic_assert(layer_bit0 + generic_tile_layer_num_bits == 32u);

  enum
    {
      /* Maximum allowed width and height for tiled
       * image atlas (be it color or index backing).
       */
      max_width_height = (1u << coord_num_bits),

      /* Maximum allowed number of layers for tiled
       * image atlas index backing.
       */
      max_layers_index_texture = (1u << root_index_tile_layer_num_bits),

      /* Maximum allowed number of layers for tiled
       * image atlas color backing
       */
      max_layers_color_texture = (1u << generic_tile_layer_num_bits),

      /* maximum value allowed for number of levels for an image */
      max_number_levels = (1u << root_index_tile_number_levels_num_bits) - 1u,
    };

  enum backing_type_t:uint32_t
    {
      /* Backing is for holding index values */
      index_backing,

      /* Backing is for holding color values */
      color_backing
    };

  ImageBacking(AtlasBlitter &blitter,
               enum backing_type_t tp,
               unsigned int width_height,
               unsigned int number_layers);

  ~ImageBacking();

  /* Upload texels to the texture; it is assumed that each
   * texel is 32-bits in size. The actual copy is delayed
   * until flush_cpu() is called
   */
  void
  upload_texels(unsigned int lod, uvec3 location, uvec2 size, unsigned int row_width, const void *texels);

  /* copy pixels from a texture. The actual copy is delayed
   * until flush_gpu() is called.
   * \param dst_lod destination LOD to which to blit
   * \param dst_location destination location to which to blit
   * \param size size (in pixels) of blit
   * \param src_texture source texture from which to blit
   * \param src_location source location from which to blit
   * \param post_process_window window to which to restrict
   *                                   post-processing texel
   *                                   fetches
   * \param blit_processing specifies if and how the pixels are
     *                        processed in the blit.
   */
  void
  copy_pixels(unsigned int dst_lod, uvec3 dst_location, uvec2 size,
              const AtlasBlitter::Texture &src_texture,
              uvec2 src_location,
              RectT<int> post_process_window,
              enum image_blit_processing_t blit_processing,
              bool permute_src_x_y_coordinates);

  void
  downsample_pixels(unsigned int dst_lod, uvec3 dst_location, uvec2 dst_size,
                    const AtlasBlitter::Texture &src_texture,
                    uvec2 src_location,
                    enum downsampling_processing_t downsamping_processing,
                    bool permute_src_x_y_coordinates);

  /* Resize the underlying GL texture */
  void
  on_resize(unsigned int new_size);

  void
  flush(void)
  {
    flush_cpu();
    flush_gpu();
  }

  /* get the GL name of the texture */
  astral_GLuint
  texture(void) const
  {
    return m_texture;
  }

private:
  void
  create_texture(void);

  void
  flush_implement(const AtlasBlitter::Texture &src);

  static
  bool
  can_support_number_layers(enum backing_type_t tp, unsigned int num_layers)
  {
    return (tp == color_backing) ?
      num_layers <= max_layers_color_texture:
      num_layers <= max_layers_index_texture;
  }

  void
  flush_cpu(void);

  void
  flush_gpu(void);

  unsigned int m_width_height;
  unsigned int m_number_layers;
  unsigned int m_number_lod;
  astral_GLuint m_texture;

  enum backing_type_t m_tp;
  astral_GLenum m_internal_format;
  astral_GLenum m_external_format;
  astral_GLenum m_external_type;
  astral_GLenum m_min_filter, m_mag_filter;

  /* for resize and flushing */
  reference_counted_ptr<AtlasBlitter> m_blitter;

  /* the staging texture and its size */
  unsigned int m_staging_width_height;
  astral_GLuint m_staging_texture;

  /* Bookkeeping for staging uploaded. We use a TileAllocator to track
   * what needs to be uploaded because a TiledAllocator allocates along
   * rows of the maximum size that a set_pixels() will be passed. Thus
   * we can dramatially reduce the glTexSubImage2D call buffer size by
   * tracking the the range of x and y coordinates of the source rects.
   * In contrast, a RectAtlas fills the left side and then the top
   * which would means a single upload doing such simple tracking would
   * likely upload the entire staging texture although most of it is
   * empty.
   */
  TileAllocator m_staging_atlas;
  std::vector<std::vector<std::vector<AtlasBlitter::BlitRect>>> m_src_rects;
  std::vector<std::vector<std::vector<AtlasBlitter::BlitRect>>> m_dst_rects;
  std::vector<std::vector<std::vector<AtlasBlitter::process_pixel_t>>> m_dst_fmts;
  std::vector<std::vector<std::vector<AtlasBlitter::PostProcessWindow>>> m_post_process_windows;
  vecN<range_type<int>, 2> m_staging_range_uploaded;
  std::vector<uint32_t> m_staging_pixels;

  /* the bookkeeping for m_src_rect and m_dst_pts
   * is also reused for blitting from a color buffer;
   * however we may only have one type accumulated:
   * from CPU or from GPU, never both at the same
   * time. This value is used to indicate from what
   * GL texture and layer of it the GPU commands are
   * sourced; if its texture value is 0 that means no
   * GPU commands are queued. The GPU commands are
   * flushed whenever the texture, layer, LOD or flip-y
   * parameter changes.
   *
   * NOTE: it is not necessary for a gpu flush if the
   *       layer or LOD change from the point of view
   *       of GL, however the use case for GPU blits
   *       is for blitting from rendered pixels to a
   *       Image. Additionally, the AtlasBlitter
   *       interface does makes LOD or layer specified
   *       per-call instead of per-element.
   *
   * TODO: consider avoiding flush for the case where
   *       a user wishes to copy from GL texture into
   *       a Image.
   */
  AtlasBlitter::Texture m_gpu_texture;

  bool m_cpu_uploads_pending;
};

class astral::gl::RenderEngineGL3::Implement::ImageColorBacking:public ImageAtlasColorBacking
{
public:
  ImageColorBacking(AtlasBlitter &blitter, unsigned int width_height, unsigned int number_layers,
                    unsigned int max_number_layers):
    ImageAtlasColorBacking(width_height, number_layers, max_number_layers),
    m_backing(blitter, ImageBacking::color_backing, width_height, number_layers)
  {
  }

  virtual
  void
  flush(void) override
  {
    m_backing.flush();
  }

  virtual
  void
  copy_pixels(unsigned int lod, uvec3 location, uvec2 size,
              ColorBuffer &src, uvec2 src_location,
              const RectT<int> &post_process_window,
              enum image_blit_processing_t blit_processing,
              bool permute_src_x_y_coordinates) override;

  astral_GLuint
  texture(void) const
  {
    return m_backing.texture();
  }

  virtual
  void
  upload_texels(unsigned int lod, uvec3 location, uvec2 size,
                c_array<const u8vec4> texels) override
  {
    m_backing.upload_texels(lod, location, size, size.x(), texels.c_ptr());
  }

  virtual
  void
  downsample_pixels(unsigned int lod, uvec3 location, uvec2 size,
                    ColorBuffer &src, uvec2 src_location,
                    enum downsampling_processing_t downsamping_processing,
                    bool permute_src_x_y_coordinates) override;

protected:
  virtual
  void
  on_resize(unsigned int new_number_layers) override
  {
    m_backing.on_resize(new_number_layers);
  }

private:
  ImageBacking m_backing;
};

class astral::gl::RenderEngineGL3::Implement::ImageIndexBacking:public ImageAtlasIndexBacking
{
public:
  ImageIndexBacking(AtlasBlitter &blitter, unsigned int width_height, unsigned int number_layers,
                    unsigned int max_number_layers):
    ImageAtlasIndexBacking(width_height, number_layers, max_number_layers),
    m_backing(blitter, ImageBacking::index_backing, width_height, number_layers)
  {
  }

  virtual
  void
  flush(void) override
  {
    m_backing.flush();
  }

  astral_GLuint
  texture(void) const
  {
    return m_backing.texture();
  }

  virtual
  void
  upload_texels(uvec3 location, uvec2 size, c_array<const uvec3> texels) override;

  static
  uint32_t
  texel_value_from_location(const uvec3 &location)
  {
    return pack_bits(ImageBacking::x_bit0, ImageBacking::coord_num_bits, location.x())
      | pack_bits(ImageBacking::y_bit0, ImageBacking::coord_num_bits, location.y())
      | pack_bits(ImageBacking::layer_bit0, ImageBacking::generic_tile_layer_num_bits, location.z());
  }

protected:
  virtual
  void
  on_resize(unsigned int new_number_layers) override
  {
    m_backing.on_resize(new_number_layers);
  }

private:
  ImageBacking m_backing;
  std::vector<uint32_t> m_workroom;
};

#endif

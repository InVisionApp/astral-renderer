/*!
 * \file image.cpp
 * \brief file image.cpp
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

#include <cstring>
#include <astral/renderer/image.hpp>
#include <astral/util/memory_pool.hpp>
#include <astral/util/object_pool.hpp>
#include <astral/util/ostream_utility.hpp>

/* Image/ImageMipElement Overview
 * ------------------------------
 *
 * The basic gist is that image data is broken into tiles with padding, the
 * padding allows one to use the GPU's bilinear sampling but at the cost that
 * we lose efficiency because those padding pixels are in mutiple tiles. Another
 * important issue is to delay allocating the space on ImageAtlas::color_backing()
 * as long as possible for content rendered by Renderer. Doing so allows one
 * to reuse color tiles for multiple scratch images (a scratch image is essentially
 * any image that is not used directly to render the content to a RenderEncoderSurface).
 * The main catch is the complexity in delaying allocating the color tiles which
 * is embodied by ImageAtlas::Implement::allocate_color_tile_backings(). Another
 * important point is that one cannot query the location of a color tile in the atlas
 * either (because it does not have a location).
 */

class astral::ImageAtlas::Implement:public astral::ImageAtlas
{
public:
  class MemoryPool;
  class ColorTile;
  class ColorUpload;
  class IndexUpload;
  typedef const TileAllocator::Tile IndexTile;

  class Counts
  {
  public:
    /* enumeration to specify to ctor a Counts that
     * specifies an empty region
     */
    enum empty_region_t:uint32_t
      {
        empty_region
      };

    explicit
    Counts(enum empty_region_t):
      m_remainder_size(0, 0),
      m_remainder_size_exact(0, 0),
      m_log2_remainder_size(0, 0),
      m_count(0, 0)
    {}

    Counts(uvec2 sz, bool with_padding);

    Counts(const Counts &src_counts,
           vecN<range_type<unsigned int>, 2> tile_range);

    /* The size of the last row and last column
     * of index/color tiles, this will be rounded up
     * to a power of 2. For color tiles this INCLUDES
     * the padding. Note that the last tile does NOT
     * have post-padding.
     */
    uvec2 m_remainder_size;

    /* the size of the last row and last column
     * BEFORE being rounded up to a power of 2.
     * For color tiles this INCLUDES the padding.
     * Note that the last tile does NOT have post-padding
     */
    uvec2 m_remainder_size_exact;

    /* The log2 of the m_remainder_size */
    uvec2 m_log2_remainder_size;

    /* the number of index tiles in each dimension */
    uvec2 m_count;
  };

  /* An IndexImage ...
   */
  class IndexImage
  {
  public:
    IndexImage(uvec2 sz, unsigned int first_tile):
      m_size(sz),
      m_cnt(m_size, false),
      m_first_tile(first_tile)
    {}

    /* Upload index texels, size of the array must be the same
     * as pitch * m_size.y() with pitch >= m_size.x
     */
    void
    upload_texels(Implement &atlas,
                  c_array<const pointer<IndexTile>> index_tile_array_backing,
                  unsigned int pitch, c_array<const uvec3> index_texels) const;

    uvec2
    tile_size(unsigned int tile_x, unsigned int tile_y) const
    {
      uvec2 sz;

      sz.x() = (tile_x + 1u == m_cnt.m_count.x()) ?
        m_cnt.m_remainder_size_exact.x() :
        static_cast<unsigned int>(ImageAtlas::tile_size);

      sz.y() = (tile_y + 1u == m_cnt.m_count.y()) ?
        m_cnt.m_remainder_size_exact.y() :
        static_cast<unsigned int>(ImageAtlas::tile_size);

      return sz;
    }

    uvec2
    tile_log2_size(unsigned int tile_x, unsigned int tile_y) const
    {
      uvec2 log2_sz;

      log2_sz.x() = (tile_x + 1u == m_cnt.m_count.x()) ?
        m_cnt.m_log2_remainder_size.x() :
        static_cast<unsigned int>(ImageAtlas::log2_tile_size);

      log2_sz.y() = (tile_y + 1u == m_cnt.m_count.y()) ?
        m_cnt.m_log2_remainder_size.y() :
        static_cast<unsigned int>(ImageAtlas::log2_tile_size);

      return log2_sz;
    }

    unsigned int
    tile_index(unsigned int tile_x, unsigned int tile_y) const
    {
      ASTRALassert(tile_x < m_cnt.m_count.x());
      ASTRALassert(tile_y < m_cnt.m_count.y());

      return m_first_tile + tile_x + m_cnt.m_count.x() * tile_y;
    }

    unsigned int
    num_tiles(void) const
    {
      return m_cnt.m_count.x() * m_cnt.m_count.y();
    }

    /* given a texel coordinate of the IndexImage, return the texel in the IndexAtlas where it is backed */
    uvec3
    index_texel_location(c_array<const pointer<IndexTile>> index_tile_array_backing, uvec2 coordinate) const;

    /* number of index texels of the index image */
    uvec2 m_size;

    /* describes the number of tiles in each dimension */
    Counts m_cnt;

    /* Index into an std::vector<IndexTile*> of the
     * first tile of the tiles that back the IndexImage
     */
    unsigned int m_first_tile;
  };

  Implement(ImageAtlasColorBacking &color_backing,
            ImageAtlasIndexBacking &index_backing);

  ~Implement();

  void
  create_shared_common(ImageMipElement *return_value,
                       c_array<const uvec2> empty_tiles,
                       c_array<const uvec2> fully_covered_tiles,
                       c_array<const std::pair<uvec2, TileElement>> shared_tiles);

  void
  release_tile(ColorTile *tile);

  void
  release_tile(IndexTile *tile);

  static
  const TileAllocator::Tile*
  allocate_tile(unsigned int max_number_layers, TileAllocator &allocator,
                unsigned int log2_width, unsigned int log2_height);

  ColorTile*
  allocate_color_tile(unsigned int log2_width, unsigned int log2_height, uvec2 actual_size, bool allocate_backing = false);

  IndexTile*
  allocate_index_tile(unsigned int log2_width, unsigned int log2_height);

  void
  free_image_id(Image *image);

  ImageID
  allocate_image_id(Image *image);

  /* Does:
   *  - resizes ImageMipElement::m_tiles
   *  - sets ImageMipElement::m_size
   *  - sets ImageMipElement::m_color_counts
   */
  void
  reserve_color_tiles(ImageMipElement *image, uvec2 sz);

  void
  reserve_color_tiles(ImageMipElement *image,
                      const ImageMipElement &src_image,
                      vecN<range_type<unsigned int>, 2> tile_range);

  /* Does:
   *  - sets ImageMipElement::m_ratio
   *  - sets ImageMipElement::m_index_images
   *  - sets ImageMipElement::m_index_tiles
   *
   * In addition, it also fills the index tile
   * texel values.
   */
  void
  create_index_tiles(ImageMipElement *image);

  enum return_code
  create_index_tiles_implement(uvec2 tile_count, std::vector<IndexImage> *dst_index_images,
                               std::vector<IndexTile*> *dst_index_tiles,
                               bool upload_index_workroom);

  void
  internal_upload_color_texels(unsigned int lod, uvec3 location, uvec2 size,
                               unsigned int row_width, c_array<const u8vec4> texels);

  void
  internal_upload_index_texels(uvec3 location, uvec2 size,
                               unsigned int row_width, c_array<const uvec3> texels);

  void
  internal_copy_color_pixels(unsigned int lod, uvec3 location, uvec2 size,
                             ColorBuffer &src, uvec2 src_location,
                             const RectT<int> &post_process_window,
                             enum image_blit_processing_t blit_processing,
                             bool permute_src_x_y_coordinates);

  void
  internal_downsample_color_texels(unsigned int lod, uvec3 location, uvec2 size,
                                   ColorBuffer &src, uvec2 src_location,
                                   enum downsampling_processing_t downsamping_processing,
                                   bool permute_src_x_y_coordinates);

  void
  flush_implement(void);

  reference_counted_ptr<Image>
  create_image_implement(detail::RenderedImageTag tag,
                         c_array<const reference_counted_ptr<const ImageMipElement>> mip_chain,
                         enum colorspace_t colorspace);

  reference_counted_ptr<Image>
  create_image_implement(detail::RenderedImageTag tag,
                         unsigned int num_mip_levels, uvec2 sz, enum colorspace_t colorspace);

  reference_counted_ptr<Image>
  create_image_implement(detail::RenderedImageTag tag,
                         uvec2 sz, enum colorspace_t colorspace);

  reference_counted_ptr<const ImageMipElement>
  create_mip_element_implement(const ImageMipElement &in_src_mip,
                               vecN<range_type<unsigned int>, 2> tile_range,
                               c_array<const uvec2> empty_tiles,
                               c_array<const uvec2> full_tiles,
                               c_array<const uvec2> shared_tiles);

  reference_counted_ptr<ImageMipElement>
  create_mip_element_implement(uvec2 sz, unsigned int number_mipmap_levels,
                               c_array<const uvec2> empty_tiles,
                               c_array<const uvec2> fully_covered_tiles,
                               c_array<const std::pair<uvec2, TileElement>> shared_tiles);

  astral::reference_counted_ptr<ImageMipElement>
  create_mip_element_implement(uvec2 sz, unsigned int number_mipmap_levels,
                               c_array<const vecN<range_type<int>, 2>> tile_regions);

  /* The backing of the ColorTile objects on the color atlas is NOT performed
   * when the color tiles are created but instead on demand. This function
   * forces the ColorTile objects to back their color tiles.
   */
  void
  allocate_color_tile_backings(ImageMipElement &image);

  void
  upload_index_values_for_single_tile_image(ImageMipElement::Implement &image);

  void
  on_tile_allocation_failed(ImageMipElement::Implement &image);

  TileAllocator m_color_tile_allocator;
  TileAllocator m_index_tile_allocator;

  /* A resused tile for a color tile where all
   * texels are (0, 0, 0, 0).
   */
  ColorTile *m_empty_tile;

  /* A resused tile for a color tile where all
   * texels are (255, 255, 255, 255).
   */
  ColorTile *m_white_tile;

  /* A reused tile for when allocation of tile backing
   * fails on a ColorTile, texels are (255, 127, 255, 255)
   */
  const TileAllocator::Tile *m_failed_tile;

  /* A tile used when one allocation of a color
   * tile fails; this happens when the maximum
   * size of the ImageAtlasIndexBacking is reached.
   */
  IndexTile *m_scratch_index_tile;

  /* incremented on lock_resources() and decremented on unlock_resources() */
  int m_resources_locked;

  /* incremented each time m_resources_locked is decremented to zero */
  uint64_t m_resources_unlock_count;

  /* the buggers that actually hold the texels */
  reference_counted_ptr<ImageAtlasColorBacking> m_color_backing;
  reference_counted_ptr<ImageAtlasIndexBacking> m_index_backing;

  /* list of free ImageID available */
  std::vector<ImageID> m_free_ids;

  /* get an image from ImageID by grabbing
   * m_image_fetcher[ImageID::m_slot]
   */
  std::vector<Image*> m_image_fetcher;

  /* work room for filling in the index tile values */
  std::vector<uvec3> m_index_workroom;

  /* work room for creating a mip-chain */
  std::vector<reference_counted_ptr<const ImageMipElement>> m_workroom;

  /* This list is added to whenever an image is marked as in
   * use for the first time within in lock_resources()/unlock_resources()
   * pair. The list is cleared when the last matching unlock_resources()
   * is called.
   */
  std::vector<reference_counted_ptr<const Image>> m_in_use_images;

  /* ColorUpload values in order of request */
  std::vector<ColorUpload> m_color_uploads;

  /* uploads for indices */
  std::vector<IndexUpload> m_index_uploads;

  /* texel backing for index uploads */
  std::vector<uvec3> m_index_upload_texel_backing;

  /* additional number of layers for the color backing on flush() */
  unsigned int m_extra_color_backing_layers;

  /* pool to allocate Image objects */
  MemoryPool *m_pool;
};

class astral::ImageMipElement::Implement:public astral::ImageMipElement
{
public:
  typedef ImageAtlas::Implement::ColorTile ColorTile;
  typedef ImageAtlas::Implement::IndexTile IndexTile;
  typedef ImageAtlas::Implement::Counts Counts;

  class SubRange;

  /* ImageAtlas will initialize fields
   * of Implement directly
   */
  Implement(void);

  unsigned int
  color_tile_index(unsigned int tile_x, unsigned int tile_y) const
  {
    unsigned int idx;

    idx = tile_x + tile_y * m_color_counts.m_count.x();
    ASTRALassert(idx < m_tiles.size());
    return idx;
  }

  ColorTile*
  fetch_tile(unsigned int tile_x, unsigned int tile_y) const
  {
    unsigned int idx;

    idx = color_tile_index(tile_x, tile_y);
    return m_tiles[idx];
  }

  ColorTile*&
  fetch_tile(unsigned int tile_x, unsigned int tile_y)
  {
    unsigned int idx;

    idx = color_tile_index(tile_x, tile_y);
    return m_tiles[idx];
  }

  /* when the image is a single tile, returns the location
   * within the atls given a location within the image
   */
  uvec3
  translate_location(int lod, ivec2 location) const;

  /* computes the range of tiles hit by range in the image */
  void
  compute_tile_range(int lod, ivec2 location, ivec2 size,
                     ivec2 *out_min_tile, ivec2 *out_max_tile) const;

  /* Returns the texel lcoation into the ImageTiledAtlasBacking
   * that stores the reference to a given color_tile.
   */
  uvec3
  index_tile_texel_for_color_tile(uvec2 tile_xy);

  unsigned int
  copy_pixels_implement(int lod, ivec2 location, ivec2 size,
                        ColorBuffer &src, ivec2 src_location,
                        enum image_blit_processing_t blit_processing,
                        bool permute_src_x_y_coordinates);

  unsigned int
  downsample_pixels_implement(int lod, ivec2 location, ivec2 size,
                              ColorBuffer &src, ivec2 src_location,
                              enum downsampling_processing_t downsamping_processing,
                              bool permute_src_x_y_coordinates);

  void
  set_pixels_implement(int lod, ivec2 location, ivec2 size, unsigned int row_width, c_array<const u8vec4> pixels);

  /* If true, then the ColorTile objects are all allocated
   * AND the first index layer is also uploaded.
   */
  bool m_allocate_color_tile_backings_called;

  /* The number of tile texels walked per index tile texel;
   *
   * When the image occupies more than a single tile, this is given by
   *
   *   m_ratio = (T - 2 * P) * T^(N - 1)
   *
   * where
   *
   *  T = ImageAtlas::tile_size
   *  P = padding size (should be 2 to enable bicubic filtering)
   *  N = number_index_levels()
   *
   * Thus it has the value
   *
   *        D = 1u << (ImageAtlas::log2_tile_size * (number_index_levels() - 1u))
   *  m_ratio = (ImageAtlas::tile_size - 2u * ImageAtlas::tile_padding) * D;
   *
   * When the image is contained in a single tile it is simply ImageAtlas::tile_size
   */
  unsigned int m_ratio;

  /* Indicates that if the image lies on a single UNPADDED tile */
  bool m_on_single_unpadded_tile;

  /* the size of the image */
  uvec2 m_size;

  /* the color tiles, some of these may point to
   * ImageAtlas::m_black_tile or m_white_tile
   */
  std::vector<ColorTile*> m_tiles;

  /* The counts for color tiles */
  Counts m_color_counts;

  /* access to the index tiles and texels */
  std::vector<ImageAtlas::Implement::IndexImage> m_index_images;

  /* the index tiles; all levels. The index tiles are
   * store with the index tiles just above the color
   * tiles first, then the index tiles above them
   * and so on until the last element is the
   * same value as m_root_tile. In addition, the tiles
   * are ordered via the indexing of ImageAtlas::IndexImage
   */
  std::vector<IndexTile*> m_index_tiles;

  /* an array of (tile_x, tile_y) for each element_type_t */
  vecN<std::vector<uvec2>, number_element_type> m_element_tiles;

  /* number of mipmap levels, this value is restricted by the color tile size */
  unsigned int m_number_mipmap_levels;

  /* parent atlas */
  reference_counted_ptr<ImageAtlas> m_atlas;

  /* true if allocation of color or index tiles failed */
  bool m_tile_allocation_failed;
};

class astral::Image::Implement:public astral::Image
{
public:
  Implement(void)
  {}

  ImageMipElement&
  mip_chain(unsigned int I)
  {
    ASTRALassert(I < m_mip_chain.size());
    return const_cast<ImageMipElement&>(*m_mip_chain[I]);
  }

  void
  mark_as_rendered_image(detail::RenderedImageTag v);

  void
  allocate_color_tile_backings(void);

  /* parent atlas */
  reference_counted_ptr<ImageAtlas> m_atlas;

  /* Specifies the default way to interpret the color values
   * when the image is sampled for color values
   */
  enum colorspace_t m_colorspace;

  bool m_opaque, m_default_use_prepadding;

  /* the mipmap chain; likely making this an std::vector<> is
   * overkill because the only present backend, the GL3 backend,
   * only support a mipmap chain up to 5 in length.
   */
  std::vector<reference_counted_ptr<const ImageMipElement>> m_mip_chain;

  /* way to see if image is in use */
  uint64_t m_in_use_marker;

  /* ID of the image */
  ImageID m_image_id;

  /* This value is an index into an array of render-order bookkeeping
   * data maintained by Renderer within a begin()/end() pair.
   */
  uint32_t m_offscreen_render_index;
};

/* A ColorTile can be used multiple times,
 * as such it carries a reference count as
 * well. The ctor initializes the reference
 * count as one, thus when creating a ColorTile,
 * the caller does NOT call acquire(). The
 * method acquire() is only called if a tile
 * is already in use and to be used by another
 * user.
 */
class astral::ImageAtlas::Implement::ColorTile
{
public:
  ColorTile(unsigned int log2_width, unsigned int log2_height, Implement &atlas, uvec2 actual_size):
    m_atlas(atlas),
    m_location(nullptr),
    m_size(actual_size),
    m_log2_size(log2_width, log2_height),
    m_reference_count(1)
  {
  }

  ColorTile(const TileAllocator::Tile *L, Implement &atlas, uvec2 actual_size):
    m_atlas(atlas),
    m_location(L),
    m_size(actual_size),
    m_reference_count(1)
  {
    ASTRALassert(L);
    ASTRALassert(L->location().x() + actual_size.x() <= m_atlas.m_color_tile_allocator.required_backing_size().x());
    ASTRALassert(L->location().y() + actual_size.y() <= m_atlas.m_color_tile_allocator.required_backing_size().y());

    m_log2_size = L->log2_size();
  }

  uvec3
  location(void)
  {
    return tile()->location();
  }

  uvec2
  size(void) const
  {
    return m_size;
  }

  bool
  unique(void) const
  {
    ASTRALassert(m_reference_count > 0);
    return m_reference_count == 1u;
  }

  bool
  release(void)
  {
    ASTRALassert(m_reference_count > 0u);
    --m_reference_count;

    return m_reference_count == 0u;
  }

  void
  acquire(void)
  {
    ++m_reference_count;
  }

  const TileAllocator::Tile*
  tile(void)
  {
    allocate_backing_implement();
    return m_location;
  }

  enum return_code
  allocate_backing(void)
  {
    allocate_backing_implement();

    ASTRALassert(m_location != nullptr);
    return (m_location != m_atlas.m_failed_tile) ? routine_success : routine_fail;
  }

  bool
  backing_allocated(void) const
  {
    return m_location != nullptr && m_location != m_atlas.m_failed_tile;
  }

  unsigned int
  reference_count(void) const
  {
    return m_reference_count;
  }

private:
  void
  allocate_backing_implement(void)
  {
    if (!m_location)
      {
        m_location = allocate_tile(m_atlas.m_color_backing->max_number_layers(),
                                   m_atlas.m_color_tile_allocator,
                                   m_log2_size.x(), m_log2_size.y());

        ASTRALassert(!m_location || m_location->location().x() + m_size.x() <= m_atlas.m_color_tile_allocator.required_backing_size().x());
        ASTRALassert(!m_location || m_location->location().y() + m_size.y() <= m_atlas.m_color_tile_allocator.required_backing_size().y());

        if (!m_location)
          {
            m_location = m_atlas.m_failed_tile;
          }
      }
  }

  Implement &m_atlas;
  const TileAllocator::Tile *m_location;
  uvec2 m_size, m_log2_size;
  unsigned int m_reference_count;
};

/* ImageMipElement has member fields with
 * whose ctor's and dtor's do memory allocation
 * and dealloactiomn, (the std::vector's). So,
 * ImageAtlas::MemoryPool will track the
 * reclaimed object directly itself. At dtor,
 * it will call the reclaimed object's dtor
 * manually.
 */
class astral::ImageAtlas::Implement::MemoryPool
{
public:
  MemoryPool(void)
  {}

  ~MemoryPool()
  {}

  Image::Implement*
  create_image(void)
  {
    return m_image_pool.allocate();
  }

  ImageMipElement::Implement*
  create_mip_element(ImageAtlas::Implement *atlas)
  {
    ImageMipElement::Implement *return_value;

    return_value = m_mip_element_pool.allocate();
    return_value->m_atlas = atlas;
    return_value->m_tile_allocation_failed = false;
    return_value->m_allocate_color_tile_backings_called = false;

    return return_value;
  }

  template<typename ...Args>
  ColorTile*
  create_color_tile(Args&&... args)
  {
    ColorTile *return_value;
    void *vptr;

    vptr = m_color_tile_pool.allocate();
    return_value = new(vptr) ColorTile(std::forward<Args>(args)...);

    return return_value;
  }

  void
  reclaim(Image::Implement *p)
  {
    m_image_pool.reclaim(p);
  }

  void
  reclaim(ImageMipElement::Implement *p)
  {
    m_mip_element_pool.reclaim(p);
  }

  void
  reclaim(ColorTile *p)
  {
    ASTRALassert(p->reference_count() == 0u);
    m_color_tile_pool.reclaim(p);
  }

  unsigned int
  total_images_allocated(void) const
  {
    return m_image_pool.live_count();
  }

  unsigned int
  total_image_mip_elements_allocated(void) const
  {
    return m_mip_element_pool.live_count();
  }

  /* workroom for ImageAtlas::create_mip_element() */
  std::vector<std::pair<uvec2, TileElement>> m_create_sub_mip_workroom;

private:
  Image::Pool m_image_pool;
  ImageMipElement::Pool m_mip_element_pool;
  astral::MemoryPool<ColorTile, 1024> m_color_tile_pool;
};

class astral::ImageAtlas::Implement::ColorUpload
{
public:
  /* A CPUUpload object is NOT meant to be reused; the reason being that the
   * CPU upload is usually a one time-ish coming from first uploading an
   * image.
   */
  class CPUUpload:public reference_counted<CPUUpload>::non_concurrent
  {
  public:
    CPUUpload(unsigned int lod, uvec3 location, uvec2 size, unsigned int row_width, c_array<const u8vec4> texels);

    void
    upload_texels(ImageAtlasColorBacking &dst) const
    {
      dst.upload_texels(m_lod, m_location, m_size, make_c_array(m_texels));
    }

    /* Taken from the ctor, except that m_texels extracts the values from texels taking
     * into account row_width
     */
    unsigned int m_lod;
    uvec3 m_location;
    uvec2 m_size;
    std::vector<u8vec4> m_texels;
  };

  class GPUUpload
  {
  public:
    GPUUpload(unsigned int lod, uvec3 location, uvec2 size,
              ColorBuffer &src, uvec2 src_location,
              const RectT<int> &post_process_window,
              enum image_blit_processing_t blit_processing,
              bool permute_src_x_y_coordinates):
      m_lod(lod),
      m_location(location),
      m_size(size),
      m_src(&src),
      m_src_location(src_location),
      m_permute_src_x_y_coordinates(permute_src_x_y_coordinates),
      m_downsample_pixels(false),
      m_post_process_window(post_process_window),
      m_blit_processing(blit_processing)
    {}

    GPUUpload(unsigned int lod, uvec3 location, uvec2 size,
              ColorBuffer &src, uvec2 src_location,
              enum downsampling_processing_t downsamping_processing,
              bool permute_src_x_y_coordinates):
      m_lod(lod),
      m_location(location),
      m_size(size),
      m_src(&src),
      m_src_location(src_location),
      m_permute_src_x_y_coordinates(permute_src_x_y_coordinates),
      m_downsample_pixels(true),
      m_downsamping_processing(downsamping_processing)
    {}

    GPUUpload(void)
    {}

    void
    upload_texels(ImageAtlasColorBacking &dst) const;

    unsigned int m_lod;
    uvec3 m_location;
    uvec2 m_size;

    reference_counted_ptr<ColorBuffer> m_src;
    uvec2 m_src_location;

    bool m_permute_src_x_y_coordinates;
    bool m_downsample_pixels;

    /* only matters if m_downsample_pixels is false */
    RectT<int> m_post_process_window;
    enum image_blit_processing_t m_blit_processing;

    /* only matters if m_downsample_pixels is true */
    enum downsampling_processing_t m_downsamping_processing;
  };

  ColorUpload(reference_counted_ptr<const CPUUpload> &c):
    m_cpu_upload(c)
  {}

  ColorUpload(const GPUUpload &g):
    m_gpu_upload(g)
  {}

  void
  upload_texels(ImageAtlasColorBacking &dst) const;

  /* if non-null indicates to upload from CPU */
  reference_counted_ptr<const CPUUpload> m_cpu_upload;

  /* if m_cpu_upload is nullptr, provides the
   * the GPU upload information
   */
  GPUUpload m_gpu_upload;
};

class astral::ImageAtlas::Implement::IndexUpload
{
public:
  /* In contrast to color texel upload, one can anticipate a great
   * of index texel uploading happening every frame (because of the
   * need to create Image objects to hold GPU rendered data).
   */
  IndexUpload(uvec3 location, uvec2 size, unsigned int row_width,
              Implement &atlas, c_array<const uvec3> texels);

  void
  upload_texels(Implement &atlas) const
  {
    c_array<uvec3> v;

    v = make_c_array(atlas.m_index_upload_texel_backing);
    atlas.m_index_backing->upload_texels(m_location, m_size, v.sub_array(m_texels));
  }

  /* Taken from the ctor, except that m_texels is a range
   * into Implement::m_index_upload_texel_backing and the
   * data extracted is made tighty packed.
   */
  uvec3 m_location;
  uvec2 m_size;
  range_type<unsigned int> m_texels;
};

class astral::ImageMipElement::Implement::SubRange
{
public:
  SubRange(int location, int size, int tile, int lod)
  {
    range_type<int> tile_range;
    range_type<int> intersect_range;

    tile_range.m_begin = ImageAtlas::tile_start(tile, lod);
    tile_range.m_end = ImageAtlas::tile_end(tile, lod);

    intersect_range.m_begin = t_max(location, tile_range.m_begin);
    intersect_range.m_end = t_min(location + size, tile_range.m_end);

    m_upload_size = intersect_range.m_end - intersect_range.m_begin;
    m_upload_dst = intersect_range.m_begin - tile_range.m_begin;
    m_upload_src = intersect_range.m_begin - location;
  }

  /* number of texels to upload */
  int m_upload_size;

  /* min-side in destination tile */
  int m_upload_dst;

  /* min-side in source texels to upload */
  int m_upload_src;
};

////////////////////////////////////////////////////////////////////////
// astral::ImageMipElement and astral::ImageMipElement::Implement methods
astral::ImageMipElement::
ImageMipElement(void)
{
}

astral::ImageMipElement::Implement::
Implement(void):
  m_color_counts(ImageAtlas::Implement::Counts::empty_region)
{
}

astral::uvec2
astral::ImageMipElement::
size(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_size;
}

int
astral::ImageMipElement::
tile_padding(int lod) const
{
  int return_value;
  const Implement *p;

  p = static_cast<const Implement*>(this);

  ASTRALassert(lod >= 0);
  ASTRALassert(lod < static_cast<int>(number_mipmap_levels()));
  return_value = (p->m_on_single_unpadded_tile) ?
    static_cast<int>(0) :
    static_cast<int>(ImageAtlas::tile_padding >> lod);

  return return_value;
}

unsigned int
astral::ImageMipElement::
number_mipmap_levels(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  ASTRALassert(p->m_atlas);
  return p->m_number_mipmap_levels;
}

void
astral::ImageMipElement::
number_mipmap_levels(unsigned int v)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  ASTRALassert(p->m_atlas);
  ASTRALassert(v <= maximum_number_of_mipmaps);
  ASTRALassert(v > 0);
  ASTRALassert(v == 1 || (p->m_size.x() > 1 && p->m_size.y() > 1));
  p->m_number_mipmap_levels = v;
}

unsigned int
astral::ImageMipElement::
number_index_levels(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_index_images.size();
}

unsigned int
astral::ImageMipElement::
ratio(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_ratio;
}

astral::uvec3
astral::ImageMipElement::
root_tile_location(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  if (p->m_index_images.empty())
    {
      return tile_index_atlas_location(uvec2(0, 0));
    }
  else
    {
      return p->m_index_tiles.back()->location();
    }
}

unsigned int
astral::ImageMipElement::
number_elements(enum element_type_t tp) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  ASTRALassert(tp < number_element_type);
  return p->m_element_tiles[tp].size();
}

astral::uvec2
astral::ImageMipElement::
element_tile_id(enum element_type_t tp, unsigned int I) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  ASTRALassert(I < p->m_element_tiles[tp].size());
  return p->m_element_tiles[tp][I];
}

astral::uvec2
astral::ImageMipElement::
element_location(enum element_type_t tp, unsigned int I) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  ASTRALassert(I < p->m_element_tiles[tp].size());
  return tile_location(p->m_element_tiles[tp][I]);
}

astral::uvec2
astral::ImageMipElement::
element_size(enum element_type_t tp, unsigned int I) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  ASTRALassert(I < p->m_element_tiles[tp].size());
  return tile_size(p->m_element_tiles[tp][I]);
}

astral::uvec2
astral::ImageMipElement::
tile_count(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_color_counts.m_count;
}

astral::uvec2
astral::ImageMipElement::
tile_location(uvec2 tile_xy) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);

  ASTRALassert(tile_xy.x() < p->m_color_counts.m_count.x());
  ASTRALassert(tile_xy.y() < p->m_color_counts.m_count.y());
  ASTRALunused(p);

  return (ImageAtlas::tile_size - 2u * ImageAtlas::tile_padding) * tile_xy;
}

astral::uvec2
astral::ImageMipElement::
tile_size(uvec2 tile_xy, bool include_padding) const
{
  uvec2 R, Q;
  unsigned int P;
  const Implement *p;

  p = static_cast<const Implement*>(this);
  ASTRALassert(tile_xy.x() < p->m_color_counts.m_count.x());
  ASTRALassert(tile_xy.y() < p->m_color_counts.m_count.y());

  R.x() = (tile_xy.x() + 1u == p->m_color_counts.m_count.x()) ?
    p->m_color_counts.m_remainder_size_exact.x() :
    static_cast<unsigned int>(ImageAtlas::tile_size);

  R.y() = (tile_xy.y() + 1u == p->m_color_counts.m_count.y()) ?
    p->m_color_counts.m_remainder_size_exact.y() :
    static_cast<unsigned int>(ImageAtlas::tile_size);

  /* the last tile only has padding in the front */
  Q.x() = (tile_xy.x() + 1u == p->m_color_counts.m_count.x()) ? 1u : 2u;
  Q.y() = (tile_xy.y() + 1u == p->m_color_counts.m_count.y()) ? 1u : 2u;

  P = (p->m_on_single_unpadded_tile || include_padding) ?
    0u :
    static_cast<unsigned int>(ImageAtlas::tile_padding);

  return R - P * Q;
}

bool
astral::ImageMipElement::
on_single_unpadded_tile(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_on_single_unpadded_tile;
}

bool
astral::ImageMipElement::
tile_allocation_failed(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_tile_allocation_failed;
}

enum astral::ImageMipElement::element_type_t
astral::ImageMipElement::
tile_type(uvec2 tile_xy) const
{
  Implement::ColorTile *t;
  const Implement *p;
  const ImageAtlas::Implement *atlas;

  p = static_cast<const Implement*>(this);
  atlas = static_cast<const ImageAtlas::Implement*>(p->m_atlas.get());

  ASTRALassert(tile_xy.x() < p->m_color_counts.m_count.x());
  ASTRALassert(tile_xy.y() < p->m_color_counts.m_count.y());
  t = p->fetch_tile(tile_xy.x(), tile_xy.y());

  if (t == atlas->m_white_tile)
    {
      return white_element;
    }

  if (t == atlas->m_empty_tile)
    {
      return empty_element;
    }

  return color_element;
}

astral::EnumFlags<enum astral::RectEnums::side_t, 4>
astral::ImageMipElement::
tile_boundary(uvec2 tile_xy) const
{
  EnumFlags<enum RectEnums::side_t, 4> return_value;
  const Implement *p;

  p = static_cast<const Implement*>(this);

  ASTRALassert(tile_xy.x() < p->m_color_counts.m_count.x());
  ASTRALassert(tile_xy.y() < p->m_color_counts.m_count.y());

  return_value.value(RectEnums::minx_side, tile_xy.x() == 0);
  return_value.value(RectEnums::miny_side, tile_xy.y() == 0);
  return_value.value(RectEnums::maxx_side, tile_xy.x() + 1u == p->m_color_counts.m_count.x());
  return_value.value(RectEnums::maxy_side, tile_xy.y() + 1u == p->m_color_counts.m_count.y());

  return return_value;
}

bool
astral::ImageMipElement::
color_tile_is_shared(unsigned int I) const
{
  uvec2 T;
  Implement::ColorTile *tile;
  const Implement *p;
  const ImageAtlas::Implement *atlas;

  p = static_cast<const Implement*>(this);
  atlas = static_cast<const ImageAtlas::Implement*>(p->m_atlas.get());

  ASTRALassert(I < p->m_element_tiles[color_element].size());
  T = p->m_element_tiles[color_element][I];
  tile = p->fetch_tile(T.x(), T.y());

  ASTRALassert(tile != atlas->m_white_tile);
  ASTRALassert(tile != atlas->m_empty_tile);
  ASTRALunused(atlas);

  return !tile->unique();
}

bool
astral::ImageMipElement::
tile_is_shared(uvec2 tile_xy) const
{
  Implement::ColorTile *t;
  const Implement *p;

  p = static_cast<const Implement*>(this);

  ASTRALassert(tile_xy.x() < p->m_color_counts.m_count.x());
  ASTRALassert(tile_xy.y() < p->m_color_counts.m_count.y());
  t = p->fetch_tile(tile_xy.x(), tile_xy.y());

  return !t->unique();
}

astral::uvec3
astral::ImageMipElement::
tile_index_atlas_location(uvec2 tile_xy) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  if (!p->m_index_images.empty())
    {
      return p->m_index_images.front().index_texel_location(make_c_array(p->m_index_tiles), tile_xy);
    }
  else
    {
      ASTRALassert(tile_xy == uvec2(0, 0));
      if (p->m_index_tiles.empty())
        {
          /* It might be tempting to make the fake index tile when the
           * ImageMipElement is first created, but that will incur ALOT
           * of such index tiles made that are never used. The use case
           * is for stroking and filling where Renderer makes a set of
           * Image objects whose size is exactly one tile and then several
           * of those Image objects are then used to be assembled into a
           * single Image; in these cases those single tile Image objects
           * are never used directly, and so making the IndexTile for those
           * is quite wasteful.
           */
          uvec2 log2_sz(0, 0);
          Implement *q;
          ImageAtlas::Implement *atlas;
          ImageAtlas::Implement::IndexTile *I;

          q = const_cast<Implement*>(p);
          atlas = static_cast<ImageAtlas::Implement*>(p->m_atlas.get());
          I = atlas->allocate_index_tile(log2_sz.x(), log2_sz.y());

          if (I)
            {
              q->m_index_tiles.push_back(I);
              if (q->m_allocate_color_tile_backings_called)
                {
                  /* the color backing was already called which means
                   * that we need to upload the index texel value
                   * now. If it has not been called, that we added
                   * the index tile to m_index_tiles means that when
                   * it is called it will perform the index texel
                   * upload.
                   */
                  atlas->upload_index_values_for_single_tile_image(*q);
                }
            }
          else
            {
              /* Failing to allocate a single index tile of size 1x1
               * requires unbelievably exceptional circumstances; We
               * will issue a warning and punt to the scratch index
               * tile.
               */
              std::cerr << "[" << __FILE__ << ", " << __LINE__ << "] Astral: Failed to allocate an index tile of size 1x1, wow\n";
              I = atlas->m_scratch_index_tile;
            }
        }

      ASTRALassert(p->m_index_tiles.size() == 1u);
      return p->m_index_tiles.front()->location();
    }
}

void
astral::ImageMipElement::Implement::
compute_tile_range(int lod, ivec2 location, ivec2 size,
                   ivec2 *out_min_tile, ivec2 *out_max_tile) const
{
  ivec2 &min_tile(*out_min_tile);
  ivec2 &max_tile(*out_max_tile);

  ASTRALassert(!m_on_single_unpadded_tile);
  ASTRALassert(lod >= 0);
  ASTRALassert(static_cast<unsigned int>(lod) < m_number_mipmap_levels);
  ASTRALassert(location.x() >= -tile_padding(lod));
  ASTRALassert(location.y() >= -tile_padding(lod));
  ASTRALassert(size.x() >= 1);
  ASTRALassert(size.y() >= 1);
  ASTRALassert(size.x() + location.x() <= static_cast<int>(m_size.x() >> lod));
  ASTRALassert(size.y() + location.y() <= static_cast<int>(m_size.y() >> lod));

  for (unsigned int c = 0; c < 2; ++c)
    {
      int last(location[c] + size[c] - 1);

      /* min_tile is THE tile that contains the texel location inside its interior */
      min_tile[c] = ImageAtlas::tile_from_texel(location[c], lod);
      ASTRALassert(min_tile[c] >= 0);

      /* we need to decrement min_tile if it is in the padding of the previous tile */
      if (min_tile[c] > 0 && location[c] < ImageAtlas::tile_end(min_tile[c] - 1, lod))
        {
          --min_tile[c];
        }

      /* max_tile is THE tile that contains the last texel hit in its interior */
      max_tile[c] = ImageAtlas::tile_from_texel(last, lod);
      ASTRALassert(max_tile[c] >= 0);

      max_tile[c] = t_min(max_tile[c], static_cast<int>(m_color_counts.m_count[c]) - 1);

      /* we need to increment max_tile if it is in the padding of the next tile */
      if (max_tile[c] + 1 < static_cast<int>(m_color_counts.m_count[c])
          && last >= ImageAtlas::tile_start(max_tile[c] + 1, lod))
        {
          ++max_tile[c];
        }

      ASTRALassert(min_tile[c] <= max_tile[c]);
      ASTRALassert(max_tile[c] < static_cast<int>(m_color_counts.m_count[c]));
      ASTRALassert(ImageAtlas::tile_start(min_tile[c], lod) <= location[c]);
      ASTRALassert(location[c] + size[c] <= ImageAtlas::tile_end(max_tile[c], lod));
    }
}

astral::uvec3
astral::ImageMipElement::Implement::
translate_location(int lod, ivec2 location) const
{
  uvec3 atlas_location;

  ASTRALassert(m_on_single_unpadded_tile);
  ASTRALassert(static_cast<unsigned int>(lod) < m_number_mipmap_levels);
  ASTRALassert(location.x() >= 0);
  ASTRALassert(location.y() >= 0);

  atlas_location = fetch_tile(0, 0)->location();
  atlas_location.x() >>= lod;
  atlas_location.y() >>= lod;
  atlas_location.x() += location.x();
  atlas_location.y() += location.y();

  return atlas_location;
}

unsigned int
astral::ImageMipElement::
copy_pixels(int lod, ivec2 location, ivec2 size,
            ColorBuffer &src, ivec2 src_location,
            enum image_blit_processing_t blit_processing,
            bool permute_src_x_y_coordinates)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  return p->copy_pixels_implement(lod, location, size,
                                  src, src_location,
                                  blit_processing,
                                  permute_src_x_y_coordinates);
}

unsigned int
astral::ImageMipElement::Implement::
copy_pixels_implement(int lod, ivec2 location, ivec2 size,
                      ColorBuffer &src, ivec2 src_location,
                      enum image_blit_processing_t blit_processing,
                      bool permute_src_x_y_coordinates)
{
  ASTRALassert(lod >= 0);
  ASTRALassert(size.x() >= 1);
  ASTRALassert(size.y() >= 1);
  ASTRALassert(size.x() + location.x() <= static_cast<int>(m_size.x() >> lod));
  ASTRALassert(size.y() + location.y() <= static_cast<int>(m_size.y() >> lod));

  if (static_cast<unsigned int>(lod) >= m_number_mipmap_levels)
    {
      return 0u;
    }

  ImageAtlas::Implement *atlas;
  atlas = static_cast<ImageAtlas::Implement*>(m_atlas.get());

  RectT<int> post_process_window;

  post_process_window.m_min_point = ivec2(src_location.x(), src_location.y());
  post_process_window.m_max_point = post_process_window.m_min_point + ivec2(size.x(), size.y());

  /* step 0: special case of a single tile means that data is directly stored */
  if (m_on_single_unpadded_tile)
    {
      if (!m_tiles.empty()
          && m_tiles.front() != atlas->m_white_tile
          && m_tiles.front() != atlas->m_empty_tile
          && m_tiles.front()->tile() != atlas->m_failed_tile)
        {
          uvec3 atlas_location;

          ASTRALassert(location.x() >= 0);
          ASTRALassert(location.y() >= 0);
          atlas_location = translate_location(lod, location);
          atlas->internal_copy_color_pixels(lod, atlas_location, uvec2(size),
                                            src, uvec2(src_location),
                                            post_process_window, blit_processing,
                                            permute_src_x_y_coordinates);
          return size.x() * size.y();
        }
      else
        {
          return 0;
        }
    }

  /* step 1: find the range of tiles affected */
  unsigned int return_value(0u);
  ivec2 min_tile, max_tile;
  compute_tile_range(lod, location, size, &min_tile, &max_tile);

  /* step 2: walk the affected tiles */
  for (int tile_y = min_tile.y(); tile_y <= max_tile.y(); ++tile_y)
    {
      SubRange Ry(location.y(), size.y(), tile_y, lod);

      for (int tile_x = min_tile.x(); tile_x <= max_tile.x(); ++tile_x)
        {
          SubRange Rx(location.x(), size.x(), tile_x, lod);
          ColorTile *tile(fetch_tile(tile_x, tile_y));
          uvec3 atlas_location;

          if (tile != atlas->m_white_tile
              && tile != atlas->m_empty_tile
              && tile->tile() != atlas->m_failed_tile
              && Rx.m_upload_size > 0
              && Ry.m_upload_size > 0)
            {
              atlas_location = tile->location();
              atlas_location.x() >>= lod;
              atlas_location.y() >>= lod;
              atlas_location.x() += Rx.m_upload_dst;
              atlas_location.y() += Ry.m_upload_dst;

              atlas->internal_copy_color_pixels(lod, atlas_location,
                                                uvec2(Rx.m_upload_size, Ry.m_upload_size),
                                                src,
                                                uvec2(Rx.m_upload_src + src_location.x(),
                                                      Ry.m_upload_src + src_location.y()),
                                                post_process_window, blit_processing,
                                                permute_src_x_y_coordinates);

              return_value += Rx.m_upload_size * Ry.m_upload_size;
            }
        }
    }

  return return_value;
}

unsigned int
astral::ImageMipElement::
downsample_pixels(int lod, ivec2 location, ivec2 size,
                  ColorBuffer &src, ivec2 src_location,
                  enum downsampling_processing_t downsamping_processing,
                  bool permute_src_x_y_coordinates)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  return p->downsample_pixels_implement(lod, location, size,
                                        src, src_location,
                                        downsamping_processing,
                                        permute_src_x_y_coordinates);
}

unsigned int
astral::ImageMipElement::Implement::
downsample_pixels_implement(int lod, ivec2 location, ivec2 size,
                            ColorBuffer &src, ivec2 src_location,
                            enum downsampling_processing_t downsamping_processing,
                            bool permute_src_x_y_coordinates)
{
  ASTRALassert(lod >= 0);
  ASTRALassert(size.x() >= 1);
  ASTRALassert(size.y() >= 1);
  ASTRALassert(size.x() + location.x() <= static_cast<int>(m_size.x() >> lod));
  ASTRALassert(size.y() + location.y() <= static_cast<int>(m_size.y() >> lod));

  if (static_cast<unsigned int>(lod) >= m_number_mipmap_levels)
    {
      return 0u;
    }

  ImageAtlas::Implement *atlas;
  atlas = static_cast<ImageAtlas::Implement*>(m_atlas.get());

  /* step 0: special case of a single tile means that data is directly stored */
  if (m_on_single_unpadded_tile)
    {
      if (!m_tiles.empty()
          && m_tiles.front() != atlas->m_white_tile
          && m_tiles.front() != atlas->m_empty_tile
          && m_tiles.front()->tile() != atlas->m_failed_tile)
        {
          uvec3 atlas_location;

          ASTRALassert(location.x() >= 0);
          ASTRALassert(location.y() >= 0);
          atlas_location = translate_location(lod, location);
          atlas->internal_downsample_color_texels(lod, atlas_location, uvec2(size),
                                                  src, uvec2(src_location),
                                                  downsamping_processing,
                                                  permute_src_x_y_coordinates);
          return size.x() * size.y();
        }
      else
        {
          return 0;
        }
    }

  /* step 1: find the range of tiles affected */
  unsigned int return_value(0u);
  ivec2 min_tile, max_tile;
  compute_tile_range(lod, location, size, &min_tile, &max_tile);

  /* step 2: walk the affected tiles */
  for (int tile_y = min_tile.y(); tile_y <= max_tile.y(); ++tile_y)
    {
      SubRange Ry(location.y(), size.y(), tile_y, lod);

      for (int tile_x = min_tile.x(); tile_x <= max_tile.x(); ++tile_x)
        {
          SubRange Rx(location.x(), size.x(), tile_x, lod);
          ColorTile *tile(fetch_tile(tile_x, tile_y));
          uvec3 atlas_location;

          if (tile != atlas->m_white_tile
              && tile != atlas->m_empty_tile
              && tile->tile() != atlas->m_failed_tile
              && Rx.m_upload_size > 0
              && Ry.m_upload_size > 0)
            {
              atlas_location = tile->location();
              atlas_location.x() >>= lod;
              atlas_location.y() >>= lod;
              atlas_location.x() += Rx.m_upload_dst;
              atlas_location.y() += Ry.m_upload_dst;

              /* Note that moving 1 pixel in the destination is moving
               * 2 pixels in the source.
               */
              atlas->internal_downsample_color_texels(lod, atlas_location,
                                                      uvec2(Rx.m_upload_size, Ry.m_upload_size),
                                                      src,
                                                      uvec2(2 * Rx.m_upload_src + src_location.x(),
                                                            2 * Ry.m_upload_src + src_location.y()),
                                                      downsamping_processing,
                                                      permute_src_x_y_coordinates);

              return_value += Rx.m_upload_size * Ry.m_upload_size;
            }
        }
    }

  return return_value;
}

void
astral::ImageMipElement::
set_pixels(int lod, ivec2 location, ivec2 size, unsigned int row_width, c_array<const u8vec4> pixels)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  p->set_pixels_implement(lod, location, size, row_width, pixels);
}

void
astral::ImageMipElement::Implement::
set_pixels_implement(int lod, ivec2 location, ivec2 size, unsigned int row_width, c_array<const u8vec4> pixels)
{
  ASTRALassert(lod >= 0);
  ASTRALassert(size.x() >= 1);
  ASTRALassert(size.y() >= 1);
  ASTRALassert(size.x() + location.x() <= static_cast<int>(m_size.x() >> lod));
  ASTRALassert(size.y() + location.y() <= static_cast<int>(m_size.y() >> lod));

  if (static_cast<unsigned int>(lod) >= m_number_mipmap_levels)
    {
      return;
    }

  ImageAtlas::Implement *atlas;
  atlas = static_cast<ImageAtlas::Implement*>(m_atlas.get());

  /* step 0: special case of a single tile means that data is directly stored */
  if (m_on_single_unpadded_tile)
    {
      if (!m_tiles.empty()
          && m_tiles.front() != atlas->m_white_tile
          && m_tiles.front() != atlas->m_empty_tile
          && m_tiles.front()->tile() != atlas->m_failed_tile)
        {
          uvec3 atlas_location;

          ASTRALassert(size.x() + location.x() <= static_cast<int>(m_size.x() >> lod));
          ASTRALassert(size.y() + location.y() <= static_cast<int>(m_size.y() >> lod));
          atlas_location = translate_location(lod, location);
          atlas->internal_upload_color_texels(lod, atlas_location, uvec2(size), row_width, pixels);
        }
      return;
    }

  /* step 1: find the range of tiles affected */
  ivec2 min_tile, max_tile;
  compute_tile_range(lod, location, size, &min_tile, &max_tile);

  /* step 2: walk the affected tiles */
  for (int tile_y = min_tile.y(); tile_y <= max_tile.y(); ++tile_y)
    {
      SubRange Ry(location.y(), size.y(), tile_y, lod);

      for (int tile_x = min_tile.x(); tile_x <= max_tile.x(); ++tile_x)
        {
          SubRange Rx(location.x(), size.x(), tile_x, lod);
          ColorTile *tile(fetch_tile(tile_x, tile_y));
          uvec3 atlas_location;
          int src_offset;

          if (tile != atlas->m_white_tile
              && tile != atlas->m_empty_tile
              && tile->tile() != atlas->m_failed_tile
              && Rx.m_upload_size > 0
              && Ry.m_upload_size > 0)
            {
              src_offset = Rx.m_upload_src + row_width * Ry.m_upload_src;
              atlas_location = tile->location();
              atlas_location.x() >>= lod;
              atlas_location.y() >>= lod;
              atlas_location.x() += Rx.m_upload_dst;
              atlas_location.y() += Ry.m_upload_dst;

              atlas->internal_upload_color_texels(lod, atlas_location,
                                                  uvec2(Rx.m_upload_size, Ry.m_upload_size),
                                                  row_width, pixels.sub_array(src_offset));
            }
        }
    }
}

astral::reference_counted_ptr<const astral::ImageMipElement>
astral::ImageMipElement::
create_sub_mip(vecN<range_type<unsigned int>, 2> tile_range,
               c_array<const uvec2> empty_tiles,
               c_array<const uvec2> full_tiles,
               c_array<const uvec2> shared_tiles) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_atlas->create_mip_element(*this, tile_range, empty_tiles, full_tiles, shared_tiles);
}

void
astral::ImageMipElement::
delete_object(ImageMipElement *in_image)
{
  Implement *image;

  image = static_cast<Implement*>(in_image);

  /* we want to avoid the memory allocation noise
   * of deleting the arrays, thus we return the
   * object to ImageAtlas::m_memory_pool, but
   * first clear the contents (by hand).
   */
  reference_counted_ptr<ImageAtlas> ref_atlas(image->m_atlas);
  ImageAtlas::Implement *atlas = static_cast<ImageAtlas::Implement*>(ref_atlas.get());

  /* remove the reference since we just took it; this
   * is critical to remove because if we don't, there
   * will be circle of references coming from that the
   * MemoryPool has the ImageMipElement object.
   */
  image->m_atlas = nullptr;

  /* return each color tile to the atlas */
  for (Implement::ColorTile *p : image->m_tiles)
    {
      atlas->release_tile(p);
    }

  /* return each index tile to the atlas */
  for (Implement::IndexTile *p : image->m_index_tiles)
    {
      if (p != atlas->m_scratch_index_tile)
        {
          atlas->release_tile(p);
        }
    }

  /* clear all vectors */
  image->m_tiles.clear();
  image->m_index_images.clear();
  image->m_index_tiles.clear();
  for (auto &v : image->m_element_tiles)
    {
      v.clear();
    }

  /* return the the object to ImageAtlas::m_pool */
  atlas->m_pool->reclaim(image);
}

unsigned int
astral::ImageMipElement::
compute_ratio(unsigned int num_index_levels)
{
  if (num_index_levels == 0u)
    {
      return ImageAtlas::tile_size;
    }
  else
    {
      unsigned int D;

      D = 1u << (ImageAtlas::log2_tile_size * (num_index_levels - 1u));
      return (ImageAtlas::tile_size_without_padding) * D;
    }
}

astral::c_string
astral::
label(enum ImageMipElement::element_type_t v)
{
#define LABEL_MACRO(X) [X] = #X
  const static c_string values[ImageMipElement::number_element_type] =
    {
      LABEL_MACRO(ImageMipElement::empty_element),
      LABEL_MACRO(ImageMipElement::white_element),
      LABEL_MACRO(ImageMipElement::color_element),
    };
#undef LABEL_MACRO

  ASTRALassert(v < ImageMipElement::number_element_type);
  return values[v];
}

///////////////////////////
// astral::Image methods
astral::Image::
Image(void)
{
}

enum astral::colorspace_t
astral::Image::
colorspace(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_colorspace;
}

void
astral::Image::
colorspace(enum colorspace_t v)
{
  Implement *p;

  p = static_cast<Implement*>(this);

  ASTRALassert(!in_use());
  p->m_colorspace = v;
}

bool
astral::Image::
opaque(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_opaque;
}

void
astral::Image::
override_to_opaque(void)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  p->m_opaque = true;
}

bool
astral::Image::
tile_allocation_failed(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  for (const reference_counted_ptr<const ImageMipElement> &mip : p->m_mip_chain)
    {
      if (mip->tile_allocation_failed())
        {
          return true;
        }
    }
  return false;
}

void
astral::Image::
delete_object(Image *in_image)
{
  Image::Implement *image = static_cast<Implement*>(in_image);

  reference_counted_ptr<ImageAtlas> ref_atlas(image->m_atlas);
  ImageAtlas::Implement *atlas = static_cast<ImageAtlas::Implement*>(ref_atlas.get());

  image->m_atlas = nullptr;

  /* clear the mipmap chain of mip-elements, this typically
   * triggers deletion of ImageMipElement objects.
   */
  image->m_mip_chain.clear();

  /* return the image ID */
  atlas->free_image_id(image);

  /* return the the object to ImageAtlas::m_pool */
  atlas->m_pool->reclaim(image);
}

int
astral::Image::
tile_padding(int lod) const
{
  ASTRALassert(lod >= 0);
  unsigned int ulod(lod);

  ASTRALassert(ulod < number_mipmap_levels());
  ASTRALstatic_assert(ImageMipElement::maximum_number_of_mipmaps == 2u);

  /* m_mip_chain[n] has LODS 2n, and 2n + 1 */
  unsigned int idx, rel_lod;

  idx = ulod >> 1u;
  rel_lod = (ulod & 1u);
  return mip_chain()[idx]->tile_padding(rel_lod);
}

void
astral::Image::
set_pixels(int lod, ivec2 location, ivec2 size, unsigned int row_width, c_array<const u8vec4> pixels)
{
  ASTRALassert(lod >= 0);
  unsigned int ulod(lod);
  Implement *p;

  p = static_cast<Implement*>(this);

  ASTRALassert(ulod < number_mipmap_levels());
  ASTRALassert(!in_use());
  ASTRALassert(p->m_offscreen_render_index == InvalidRenderValue);

  /* code assumes that the maximum number of mipmap levels
   * per ImageMipElement is 2 and that the miplevels
   * of m_mip_chain are disjoint.
   *
   * To support tri-linear filtering means that the levels
   * cannot be disjoint. There are several ways to do this:
   *  1) m_mip_chain[n] holds mips {2n, 2n + 1, 2n + 2} and
   *     then backend can do tri-linear filtering directly.
   *     The bad part is that there is a risk of leakage and
   *     there is a mismatch between the number of texels
   *     walked per index texel walked at native backend
   *     texture LOD = 2.
   * OR
   *  2) m_mip_chain[n] holds mips {n, n + 1}. This is kind
   *     of awful since it basically increases the mipmap
   *     room consumed from the usual 33% (for squares) to
   *     a much larger 66%. Eww.
   */
  ASTRALstatic_assert(ImageMipElement::maximum_number_of_mipmaps == 2u);

  /* m_mip_chain[n] has LODS 2n, and 2n + 1 */
  unsigned int idx, rel_lod;

  idx = ulod >> 1u;
  rel_lod = (ulod & 1u);

  ASTRALassert(idx < p->m_mip_chain.size());
  p->mip_chain(idx).set_pixels(rel_lod, location, size, row_width, pixels);
}

unsigned int
astral::Image::
copy_pixels(int lod, ivec2 location, ivec2 size,
            ColorBuffer &src, ivec2 src_location,
            enum image_blit_processing_t blit_processing,
            bool permute_src_x_y_coordinates)
{
  ASTRALassert(lod >= 0);
  unsigned int ulod(lod);
  Implement *p;

  p = static_cast<Implement*>(this);

  ASTRALassert(ulod < number_mipmap_levels());
  ASTRALassert(!in_use());
  ASTRALassert(p->m_offscreen_render_index == InvalidRenderValue);
  ASTRALstatic_assert(ImageMipElement::maximum_number_of_mipmaps == 2u);

  /* m_mip_chain[n] has LODS 2n, and 2n + 1 */
  unsigned int idx, rel_lod;

  idx = ulod >> 1u;
  rel_lod = (ulod & 1u);

  ASTRALassert(idx < p->m_mip_chain.size());
  return p->mip_chain(idx).copy_pixels(rel_lod, location, size, src, src_location,
                                       blit_processing, permute_src_x_y_coordinates);
}

unsigned int
astral::Image::
downsample_pixels(int lod, ivec2 location, ivec2 size,
                  ColorBuffer &src, ivec2 src_location,
                  enum downsampling_processing_t downsamping_processing,
                  bool permute_src_x_y_coordinates)
{
  ASTRALassert(lod >= 0);
  unsigned int ulod(lod);
  Implement *p;

  p = static_cast<Implement*>(this);

  ASTRALassert(ulod < number_mipmap_levels());
  ASTRALassert(!in_use());
  ASTRALassert(p->m_offscreen_render_index == InvalidRenderValue);
  ASTRALstatic_assert(ImageMipElement::maximum_number_of_mipmaps == 2u);

  /* m_mip_chain[n] has LODS 2n, and 2n + 1 */
  unsigned int idx, rel_lod;

  idx = ulod >> 1u;
  rel_lod = (ulod & 1u);

  ASTRALassert(idx < p->m_mip_chain.size());
  return p->mip_chain(idx).downsample_pixels(rel_lod, location, size, src, src_location,
                                             downsamping_processing, permute_src_x_y_coordinates);
}

astral::c_array<const astral::reference_counted_ptr<const astral::ImageMipElement>>
astral::Image::
mip_chain(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return make_c_array(p->m_mip_chain);
}

void
astral::Image::
mark_in_use(void)
{
  Implement *p;
  ImageAtlas::Implement *atlas;

  p = static_cast<Implement*>(this);
  atlas = static_cast<ImageAtlas::Implement*>(p->m_atlas.get());

  if (p->m_in_use_marker != atlas->m_resources_unlock_count + 1u)
    {
      p->m_in_use_marker = atlas->m_resources_unlock_count + 1u;

      /* We only add an image to in-use images if it is not
       * rendered by a Renderer; the internals of Renderer
       * will make sure that that a reference of the image
       * will stay alive until the image is no longer needed.
       */
      if (p->m_offscreen_render_index == InvalidRenderValue)
        {
          atlas->m_in_use_images.push_back(this);
        }
    }
}

bool
astral::Image::
in_use(void) const
{
  const Implement *p;
  ImageAtlas::Implement *atlas;

  p = static_cast<const Implement*>(this);
  atlas = static_cast<ImageAtlas::Implement*>(p->m_atlas.get());
  return p->m_in_use_marker > atlas->m_resources_unlock_count;
}

astral::ImageID
astral::Image::
ID(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_image_id;
}

bool
astral::Image::
default_use_prepadding(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_default_use_prepadding;
}

void
astral::Image::
default_use_prepadding(bool v)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  p->m_default_use_prepadding = v;
}

uint32_t
astral::Image::
offscreen_render_index(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_offscreen_render_index;
}

void
astral::Image::
mark_as_usual_image(detail::RenderedImageTag v)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  ASTRALassert(v.m_offscreen_render_index == InvalidRenderValue);
  ASTRALassert(in_use());
  ASTRALassert(offscreen_render_index() != InvalidRenderValue);

  ImageAtlas::Implement *atlas;
  atlas = static_cast<ImageAtlas::Implement*>(p->m_atlas.get());

  p->m_offscreen_render_index = v.m_offscreen_render_index;
  p->m_in_use_marker = atlas->m_resources_unlock_count;
  p->allocate_color_tile_backings();
}

void
astral::Image::Implement::
mark_as_rendered_image(detail::RenderedImageTag v)
{
  ASTRALassert(v.m_offscreen_render_index != InvalidRenderValue);
  ASTRALassert(!in_use());
  ASTRALassert(offscreen_render_index() == InvalidRenderValue);

  m_opaque = false;
  m_offscreen_render_index = v.m_offscreen_render_index;
  mark_in_use();
}

void
astral::Image::Implement::
allocate_color_tile_backings(void)
{
  ImageAtlas::Implement *atlas;

  atlas = static_cast<ImageAtlas::Implement*>(m_atlas.get());
  for (const auto &p : m_mip_chain)
    {
      ImageMipElement *q = const_cast<ImageMipElement*>(p.get());
      atlas->allocate_color_tile_backings(*q);
    }
}

///////////////////////////////////////
// astral::ImageAtlas::Implement::Counts methods
astral::ImageAtlas::Implement::Counts::
Counts(uvec2 sz, bool with_padding)
{
  if (sz.x() <= ImageAtlas::tile_size
      && sz.y() <= ImageAtlas::tile_size)
    {
      m_count = uvec2(1, 1);
      m_remainder_size = sz;
      m_remainder_size_exact = sz;
      m_log2_remainder_size = uvec2(0, 0);
    }
  else
    {
      unsigned int padding(with_padding ? static_cast<unsigned int>(ImageAtlas::tile_padding) : 0u);
      unsigned int dv(ImageAtlas::tile_size - 2u * padding);

      for (unsigned int c = 0; c < 2u; ++c)
        {
          unsigned int A, B;

          A = sz[c] / dv;

          ASTRALassert(A * dv <= sz[c]);
          B = sz[c] - A * dv;

          /* this is a little bit of a hassle. We require that the last
           * tile only has padding in the front, not the end. Let us write
           *
           *    sz[c] = dv * A + B
           *
           * with A >= 0 and 0 <= B < dv.
           *
           * Case 1: A = 0, then the last tile is size B + padding and we have only
           *         one tile
           * Case 2: A > 0 and B <= padding. Then we have A tiles and the last tile
           *         is size dv + B + padding.
           * Case 3: A > 0 and B > padding. Then we have (A + 1) tiles with the last
           *         tile having size B + padding
           */
          if (A == 0u)
            {
              m_count[c] = 1u;
              m_remainder_size_exact[c] = B + padding;
            }
          else if (B <= padding)
            {
              m_count[c] = A;
              m_remainder_size_exact[c] = dv + B + padding;
            }
          else
            {
              m_count[c] = A + 1;
              m_remainder_size_exact[c] = B + padding;
            }
        }
    }

  for (unsigned int c = 0; c < 2u; ++c)
    {
      m_remainder_size[c] = next_power_of_2(m_remainder_size_exact[c]);
      m_log2_remainder_size[c] = uint32_log2_floor(m_remainder_size[c]);
      ASTRALassert((1u << m_log2_remainder_size[c]) >= m_remainder_size[c]);
    }
}

astral::ImageAtlas::Implement::Counts::
Counts(const Counts &src_counts,
       vecN<range_type<unsigned int>, 2> tile_range)
{
  m_count.x() = tile_range.x().difference();
  m_count.y() = tile_range.y().difference();

  m_remainder_size_exact.x() = (tile_range.x().m_end == src_counts.m_count.x()) ?
    src_counts.m_remainder_size_exact.x() :
    static_cast<unsigned int>(tile_size);

  m_remainder_size_exact.y() = (tile_range.y().m_end == src_counts.m_count.y()) ?
    src_counts.m_remainder_size_exact.y() :
    static_cast<unsigned int>(tile_size);

  for (unsigned int c = 0; c < 2u; ++c)
    {
      m_remainder_size[c] = next_power_of_2(m_remainder_size_exact[c]);
      m_log2_remainder_size[c] = uint32_log2_floor(m_remainder_size[c]);
      ASTRALassert((1u << m_log2_remainder_size[c]) >= m_remainder_size[c]);
    }
}

/////////////////////////////////////////////////////
// astral::ImageAtlas::Implement::IndexImage methods
void
astral::ImageAtlas::Implement::IndexImage::
upload_texels(Implement &atlas, c_array<const pointer<IndexTile>> index_tile_array_backing,
              unsigned int pitch, c_array<const uvec3> index_texels) const
{
  ASTRALassert(!index_texels.empty());
  ASTRALassert(pitch >= m_size.x());
  ASTRALassert(index_texels.size() == pitch * m_size.y());
  ASTRALassert(index_tile_array_backing.size() >= m_first_tile + m_cnt.m_count.x() * m_cnt.m_count.y());

  for (unsigned int y = 0, index_tile = m_first_tile; y < m_cnt.m_count.y(); ++y)
    {
      for (unsigned int x = 0; x < m_cnt.m_count.x(); ++x, ++index_tile)
        {
          unsigned int src_offset;
          uvec2 sz;
          uvec3 dst_location;

          ASTRALassert(index_tile == tile_index(x, y));

          sz = tile_size(x, y);
          src_offset = x * ImageAtlas::tile_size + y * pitch * ImageAtlas::tile_size;
          dst_location = index_tile_array_backing[index_tile]->location();
          atlas.internal_upload_index_texels(dst_location, sz, pitch, index_texels.sub_array(src_offset));
        }
    }
}

astral::uvec3
astral::ImageAtlas::Implement::IndexImage::
index_texel_location(c_array<const pointer<IndexTile>> index_tile_array_backing, uvec2 coordinate) const
{
  ASTRALassert(index_tile_array_backing.size() >= m_first_tile + m_cnt.m_count.x() * m_cnt.m_count.y());
  ASTRALassert(coordinate.x() < m_size.x());
  ASTRALassert(coordinate.y() < m_size.y());

  uvec2 tile, tile_offset;
  uvec3 return_value;

  /* get which tile */
  tile.x() = coordinate.x() >> ImageAtlas::log2_tile_size;
  tile.y() = coordinate.y() >> ImageAtlas::log2_tile_size;

  /* get the offset into the tile */
  tile_offset = coordinate - tile * ImageAtlas::tile_size;

  /* the backing index tile is then given by the location of the tile plus the tile offset */
  return_value = index_tile_array_backing[tile_index(tile.x(), tile.y())]->location();
  return_value.x() += tile_offset.x();
  return_value.y() += tile_offset.y();

  return return_value;
}

//////////////////////////////////////////////
// astral::ImageAtlas::Implement::ColorUpload::GPUUpload methods
void
astral::ImageAtlas::Implement::ColorUpload::GPUUpload::
upload_texels(ImageAtlasColorBacking &dst) const
{
  ASTRALassert(m_src);
  if (m_downsample_pixels)
    {
      dst.downsample_pixels(m_lod, m_location, m_size, *m_src,
                            m_src_location, m_downsamping_processing,
                            m_permute_src_x_y_coordinates);
    }
  else
    {
      dst.copy_pixels(m_lod, m_location, m_size, *m_src,
                      m_src_location, m_post_process_window,
                      m_blit_processing, m_permute_src_x_y_coordinates);
    }
}

//////////////////////////////////////////////
// astral::ImageAtlas::Implement::ColorUpload::CPUUpload methods
astral::ImageAtlas::Implement::ColorUpload::CPUUpload::
CPUUpload(unsigned int lod, uvec3 location, uvec2 size, unsigned int row_width, c_array<const u8vec4> src_texels):
  m_lod(lod),
  m_location(location),
  m_size(size),
  m_texels(m_size.x() * m_size.y())
{
  for (unsigned int y = 0, src_loc = 0u, dst_loc = 0u; y < m_size.y(); ++y, src_loc += row_width, dst_loc += m_size.x())
    {
      ASTRALassert(src_texels.size() >= src_loc + m_size.x());
      ASTRALassert(m_texels.size() >= dst_loc + m_size.x());
      std::memcpy(&m_texels[dst_loc], &src_texels[src_loc], m_size.x() * sizeof(u8vec4));
    }
}

//////////////////////////////////////////////
// astral::ImageAtlas::Implement::ColorUpload methods
void
astral::ImageAtlas::Implement::ColorUpload::
upload_texels(ImageAtlasColorBacking &dst) const
{
  if (m_cpu_upload)
    {
      m_cpu_upload->upload_texels(dst);
    }
  else
    {
      m_gpu_upload.upload_texels(dst);
    }
}

//////////////////////////////////////////////
// astral::ImageAtlas::Implement::IndexUpload methods
astral::ImageAtlas::Implement::IndexUpload::
IndexUpload(uvec3 location, uvec2 size, unsigned int row_width,
            Implement &atlas, c_array<const uvec3> src_texels):
  m_location(location),
  m_size(size)
{
  unsigned int num_texels;

  num_texels = m_size.x() * m_size.y();
  m_texels.m_begin = atlas.m_index_upload_texel_backing.size();
  m_texels.m_end = m_texels.m_begin + num_texels;

  atlas.m_index_upload_texel_backing.resize(m_texels.m_end);

  for (unsigned int y = 0, src_loc = 0u, dst_loc = m_texels.m_begin; y < m_size.y(); ++y, src_loc += row_width, dst_loc += m_size.x())
    {
      ASTRALassert(src_texels.size() >= src_loc + m_size.x());
      ASTRALassert(atlas.m_index_upload_texel_backing.size() >= dst_loc + m_size.x());
      std::memcpy(&atlas.m_index_upload_texel_backing[dst_loc], &src_texels[src_loc], m_size.x() * sizeof(uvec3));
    }
}

///////////////////////////////////////////////////////////////
// astral::ImageAtlas and astral::ImageAtlas::Implement methods
astral::reference_counted_ptr<astral::ImageAtlas>
astral::ImageAtlas::
create(ImageAtlasColorBacking &color_backing,
       ImageAtlasIndexBacking &index_backing)
{
  return ASTRALnew Implement(color_backing, index_backing);
}

astral::ImageAtlas::Implement::
Implement(ImageAtlasColorBacking &color_backing,
          ImageAtlasIndexBacking &index_backing):
  m_color_tile_allocator(ImageAtlas::log2_tile_size,
                         uvec2(color_backing.width_height() >> ImageAtlas::log2_tile_size),
                         color_backing.number_layers()),
  m_index_tile_allocator(ImageAtlas::log2_tile_size,
                         uvec2(index_backing.width_height() >> ImageAtlas::log2_tile_size),
                         index_backing.number_layers()),
  m_resources_locked(0u),
  m_resources_unlock_count(0u),
  m_color_backing(&color_backing),
  m_index_backing(&index_backing),
  m_extra_color_backing_layers(0)
{
  const unsigned int log2_sz(ImageAtlas::log2_tile_size);
  const unsigned int sz(ImageAtlas::tile_size);

  m_pool = ASTRALnew MemoryPool();

  m_empty_tile = allocate_color_tile(log2_sz, log2_sz, uvec2(sz), true);
  m_white_tile = allocate_color_tile(log2_sz, log2_sz, uvec2(sz), true);
  m_scratch_index_tile = allocate_index_tile(log2_sz, log2_sz);

  m_failed_tile = allocate_tile(m_color_backing->max_number_layers(),
                                m_color_tile_allocator,
                                log2_sz, log2_sz);

  ASTRALassert(m_empty_tile);
  ASTRALassert(m_white_tile);
  ASTRALassert(m_scratch_index_tile);
  ASTRALassert(m_failed_tile);

  /* These must be true in order for images that failed allocation
   * to be able to use m_scratch_index_tile to map to m_empty_tile
   */
  ASTRALassert(m_scratch_index_tile->location() == uvec3(0, 0, 0));
  ASTRALassert(m_empty_tile->location() == uvec3(0, 0, 0));

  std::vector<u8vec4> tmp(sz * sz, u8vec4(0u, 0u, 0u, 0u));
  for (unsigned int lod = 0; lod < ImageMipElement::maximum_number_of_mipmaps; ++lod)
    {
      uvec3 L(m_empty_tile->location());
      L.x() >>= lod;
      L.y() >>= lod;
      internal_upload_color_texels(lod, L, uvec2(sz >> lod), sz, make_c_array(tmp));
    }

  std::fill(tmp.begin(), tmp.end(), u8vec4(255u, 255u, 255u, 255u));
  for (unsigned int lod = 0; lod < ImageMipElement::maximum_number_of_mipmaps; ++lod)
    {
      uvec3 L(m_white_tile->location());
      L.x() >>= lod;
      L.y() >>= lod;
      internal_upload_color_texels(lod, L, uvec2(sz >> lod), sz, make_c_array(tmp));
    }

  std::fill(tmp.begin(), tmp.end(), u8vec4(255u, 127, 255u, 255u));
  for (unsigned int lod = 0; lod < ImageMipElement::maximum_number_of_mipmaps; ++lod)
    {
      uvec3 L(m_failed_tile->location());
      L.x() >>= lod;
      L.y() >>= lod;
      internal_upload_color_texels(lod, L, uvec2(sz >> lod), sz, make_c_array(tmp));
    }

  m_index_workroom.resize(sz * sz, uvec3(0, 0, 0));
  internal_upload_index_texels(m_scratch_index_tile->location(), uvec2(sz, sz), sz,
                               make_c_array(m_index_workroom));

  m_index_workroom.clear();
}

astral::ImageAtlas::Implement::
~Implement()
{
  m_index_tile_allocator.release_tile(m_scratch_index_tile);
  release_tile(m_empty_tile);
  release_tile(m_white_tile);
  m_color_tile_allocator.release_tile(m_failed_tile);
  ASTRALdelete(m_pool);
}

int
astral::ImageAtlas::
tile_start(int tile, int lod)
{
  int return_value, effective_padding;

  ASTRALassert(lod >= 0);
  ASTRALassert(tile >= 0);

  return_value = tile * tile_size_without_padding;
  return_value >>= lod;

  effective_padding = tile_padding >> lod;
  return_value -= effective_padding;

  return return_value;
}

int
astral::ImageAtlas::
tile_end(int tile, int lod)
{
  int return_value;

  ASTRALassert(lod >= 0);
  ASTRALassert(tile >= 0);

  return_value = tile_start(tile, lod) + (tile_size >> lod);
  return return_value;
}

int
astral::ImageAtlas::
tile_from_texel(int texel, int lod)
{
  int return_value, effective_padding;

  ASTRALassert(lod >= 0);
  if (texel < 0)
    {
      return 0;
    }

  effective_padding = tile_padding >> lod;
  return_value = (texel - effective_padding) << lod;
  return_value += effective_padding;
  return_value /= tile_size_without_padding;

  return return_value;
}

astral::uvec2
astral::ImageAtlas::
tile_count(uvec2 sz, uvec2 *out_remainder_size)
{
  Implement::Counts C(sz, true);
  if (out_remainder_size)
    {
      /* should we remove the pre-padding from the size? */
      *out_remainder_size = C.m_remainder_size_exact;
    }
  return C.m_count;
}

astral::ivec2
astral::ImageAtlas::
tile_count(ivec2 sz, ivec2 *out_remainder_size)
{
  uvec2 uc, ur;

  uc = tile_count(uvec2(sz), &ur);
  if (out_remainder_size)
    {
      *out_remainder_size = ivec2(ur);
    }
  return ivec2(uc);
}

astral::uvec3
astral::ImageAtlas::
empty_tile_atlas_location(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_empty_tile->location();
}

astral::uvec3
astral::ImageAtlas::
white_tile_atlas_location(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_white_tile->location();
}

void
astral::ImageAtlas::Implement::
internal_upload_color_texels(unsigned int lod, uvec3 location, uvec2 size,
                             unsigned int row_width, c_array<const u8vec4> texels)
{
  reference_counted_ptr<const ColorUpload::CPUUpload> p;

  p = ASTRALnew ColorUpload::CPUUpload(lod, location, size, row_width, texels);
  m_color_uploads.push_back(p);
}

void
astral::ImageAtlas::Implement::
internal_upload_index_texels(uvec3 location, uvec2 size,
                             unsigned int row_width, c_array<const uvec3> texels)
{
  m_index_uploads.push_back(IndexUpload(location, size, row_width, *this, texels));
}

void
astral::ImageAtlas::Implement::
internal_copy_color_pixels(unsigned int lod, uvec3 location, uvec2 size,
                           ColorBuffer &src, uvec2 src_location,
                           const RectT<int> &post_process_window,
                           enum image_blit_processing_t blit_processing,
                           bool permute_src_x_y_coordinates)
{
  ColorUpload::GPUUpload G(lod, location, size, src, src_location,
                           post_process_window, blit_processing,
                           permute_src_x_y_coordinates);
  m_color_uploads.push_back(G);
}

void
astral::ImageAtlas::Implement::
internal_downsample_color_texels(unsigned int lod, uvec3 location, uvec2 size,
                                 ColorBuffer &src, uvec2 src_location,
                                 enum downsampling_processing_t downsamping_processing,
                                 bool permute_src_x_y_coordinates)
{
  ColorUpload::GPUUpload G(lod, location, size, src, src_location,
                           downsamping_processing,
                           permute_src_x_y_coordinates);
  m_color_uploads.push_back(G);
}

void
astral::ImageAtlas::
flush(void)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  p->flush_implement();
}

void
astral::ImageAtlas::Implement::
flush_implement(void)
{
  /* before processing the uploads, first resize the backings
   * as needed;
   */
  if (m_index_backing->number_layers() < m_index_tile_allocator.number_layers())
    {
      m_index_backing->number_layers(m_index_tile_allocator.number_layers());
    }

  if (m_color_backing->number_layers() < m_color_tile_allocator.number_layers())
    {
      unsigned int num_layers;

      num_layers = m_color_tile_allocator.number_layers() + m_extra_color_backing_layers;
      num_layers = t_min(num_layers, m_color_backing->max_number_layers());
      m_color_backing->number_layers(num_layers);
    }

  for (const ColorUpload &upload : m_color_uploads)
    {
      upload.upload_texels(*m_color_backing);
    }

  for (const IndexUpload &upload : m_index_uploads)
    {
      upload.upload_texels(*this);
    }

  m_color_uploads.clear();
  m_index_uploads.clear();
  m_index_upload_texel_backing.clear();

  m_color_backing->flush();
  m_index_backing->flush();
}

void
astral::ImageAtlas::
extra_color_backing_texels(unsigned int texels)
{
  unsigned int wh, texels_per_layer;
  Implement *p;

  p = static_cast<Implement*>(this);

  wh = p->m_color_backing->width_height();
  texels_per_layer = wh * wh;
  p->m_extra_color_backing_layers = texels / texels_per_layer;

  if (p->m_extra_color_backing_layers * texels_per_layer < texels)
    {
      ++p->m_extra_color_backing_layers;
    }
}

astral::reference_counted_ptr<astral::Image>
astral::ImageAtlas::
create_rendered_image(detail::RenderedImageTag tag,
                      c_array<const reference_counted_ptr<const ImageMipElement>> mip_chain,
                      enum colorspace_t colorspace)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  return p->create_image_implement(tag, mip_chain, colorspace);
}

astral::reference_counted_ptr<astral::Image>
astral::ImageAtlas::
create_image(c_array<const reference_counted_ptr<const ImageMipElement>> mip_chain, enum colorspace_t colorspace)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  return p->create_image_implement(detail::RenderedImageTag(InvalidRenderValue), mip_chain, colorspace);
}

astral::reference_counted_ptr<astral::Image>
astral::ImageAtlas::Implement::
create_image_implement(detail::RenderedImageTag tag,
                       c_array<const reference_counted_ptr<const ImageMipElement>> mip_chain,
                       enum colorspace_t colorspace)
{
  Image::Implement *return_value;

  return_value = m_pool->create_image();
  return_value->m_atlas = this;
  return_value->m_colorspace = colorspace;
  return_value->m_opaque = false;
  return_value->m_default_use_prepadding = false;
  return_value->m_in_use_marker = 0u;
  return_value->m_image_id = allocate_image_id(return_value);
  return_value->m_offscreen_render_index = InvalidRenderValue;

  ASTRALassert(!mip_chain.empty());
  ASTRALassert(return_value->m_mip_chain.empty());

  return_value->m_mip_chain.resize(mip_chain.size());
  std::copy(mip_chain.begin(), mip_chain.end(), return_value->m_mip_chain.begin());

  if (tag.m_offscreen_render_index != InvalidRenderValue)
    {
      return_value->mark_as_rendered_image(tag);
    }
  else
    {
      return_value->allocate_color_tile_backings();
    }

  return return_value;
}

astral::reference_counted_ptr<astral::Image>
astral::ImageAtlas::
create_rendered_image(detail::RenderedImageTag tag,
                      uvec2 sz, enum colorspace_t colorspace)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  return p->create_image_implement(tag, sz, colorspace);
}

astral::reference_counted_ptr<astral::Image>
astral::ImageAtlas::
create_image(uvec2 sz, enum colorspace_t colorspace)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  return p->create_image_implement(detail::RenderedImageTag(InvalidRenderValue), sz, colorspace);
}

astral::reference_counted_ptr<astral::Image>
astral::ImageAtlas::Implement::
create_image_implement(detail::RenderedImageTag tag,
                       uvec2 sz, enum colorspace_t colorspace)
{
  reference_counted_ptr<Image> return_value;

  ASTRALassert(m_workroom.empty());
  while (sz.x() > 0u && sz.y() > 0u)
    {
      reference_counted_ptr<const ImageMipElement> m;

      m = create_mip_element(sz, c_array<const uvec2>(), c_array<const uvec2>());
      m_workroom.push_back(m);

      sz.x() >>= ImageMipElement::maximum_number_of_mipmaps;
      sz.y() >>= ImageMipElement::maximum_number_of_mipmaps;
    }

  return_value = create_image_implement(tag, make_c_array(m_workroom), colorspace);
  m_workroom.clear();

  return return_value;
}

astral::reference_counted_ptr<astral::Image>
astral::ImageAtlas::
create_rendered_image(detail::RenderedImageTag tag, unsigned int num_mip_levels,
                      uvec2 sz, enum colorspace_t colorspace)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  return p->create_image_implement(tag, num_mip_levels, sz, colorspace);
}

astral::reference_counted_ptr<astral::Image>
astral::ImageAtlas::
create_image(unsigned int num_mip_levels, uvec2 sz, enum colorspace_t colorspace)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  return p->create_image_implement(detail::RenderedImageTag(InvalidRenderValue), num_mip_levels, sz, colorspace);
}

astral::reference_counted_ptr<astral::Image>
astral::ImageAtlas::Implement::
create_image_implement(detail::RenderedImageTag tag,
                       unsigned int num_mip_levels, uvec2 sz, enum colorspace_t colorspace)
{
  reference_counted_ptr<Image> return_value;

  ASTRALassert(m_workroom.empty());
  ASTRALassert(num_mip_levels > 0u);
  while (sz.x() > 0u && sz.y() > 0u && ImageMipElement::maximum_number_of_mipmaps * m_workroom.size() < num_mip_levels)
    {
      reference_counted_ptr<const ImageMipElement> m;

      m = create_mip_element(sz, c_array<const uvec2>(), c_array<const uvec2>());
      m_workroom.push_back(m);

      sz.x() >>= ImageMipElement::maximum_number_of_mipmaps;
      sz.y() >>= ImageMipElement::maximum_number_of_mipmaps;
    }

  return_value = create_image_implement(tag, make_c_array(m_workroom), colorspace);

  if (return_value->number_mipmap_levels() > num_mip_levels)
    {
      ASTRALassert(return_value->number_mipmap_levels() == num_mip_levels + 1u);
      ASTRALassert(!return_value->mip_chain().empty());
      ASTRALassert(return_value->mip_chain().back()->number_mipmap_levels() == 2u);

      ImageMipElement::Implement *p;
      p = const_cast<ImageMipElement::Implement*>(static_cast<const ImageMipElement::Implement*>(return_value->mip_chain().back().get()));
      --p->m_number_mipmap_levels;
    }

  ASTRALassert(return_value->number_mipmap_levels() == num_mip_levels);

  m_workroom.clear();
  return return_value;
}

astral::reference_counted_ptr<astral::ImageMipElement>
astral::ImageAtlas::
create_mip_element(uvec2 sz,
                   c_array<const uvec2> empty_tiles,
                   c_array<const uvec2> fully_covered_tiles)
{
  return create_mip_element(sz, ImageMipElement::maximum_number_of_mipmaps,
                            empty_tiles, fully_covered_tiles,
                            c_array<const std::pair<uvec2, TileElement>>());
}

astral::reference_counted_ptr<const astral::ImageMipElement>
astral::ImageAtlas::
create_mip_element(const ImageMipElement &in_src_mip,
                   vecN<range_type<unsigned int>, 2> tile_range,
                   c_array<const uvec2> empty_tiles,
                   c_array<const uvec2> full_tiles,
                   c_array<const uvec2> shared_tiles)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  return p->create_mip_element_implement(in_src_mip, tile_range, empty_tiles, full_tiles, shared_tiles);
}

astral::reference_counted_ptr<const astral::ImageMipElement>
astral::ImageAtlas::Implement::
create_mip_element_implement(const ImageMipElement &in_src_mip,
                             vecN<range_type<unsigned int>, 2> tile_range,
                             c_array<const uvec2> empty_tiles,
                             c_array<const uvec2> full_tiles,
                             c_array<const uvec2> shared_tiles)
{
  ImageMipElement::Implement *return_value;
  const ImageMipElement::Implement *psrc_mip = static_cast<const ImageMipElement::Implement*>(&in_src_mip);
  const ImageMipElement::Implement &src_mip(*psrc_mip);

  ASTRALassert(m_pool->m_create_sub_mip_workroom.empty());
  ASTRALassert(tile_range.x().m_begin < tile_range.x().m_end);
  ASTRALassert(tile_range.y().m_begin < tile_range.y().m_end);
  ASTRALassert(tile_range.x().m_end <= src_mip.m_color_counts.m_count.x());
  ASTRALassert(tile_range.y().m_end <= src_mip.m_color_counts.m_count.y());

  for (const uvec2 &src_tile : shared_tiles)
    {
      uvec2 dst_tile;
      std::pair<uvec2, TileElement> E;

      dst_tile.x() = src_tile.x() - tile_range.x().m_begin;
      dst_tile.y() = src_tile.y() - tile_range.y().m_begin;

      E.first = dst_tile;
      E.second.m_src = &src_mip;
      E.second.m_tile = src_tile;

      m_pool->m_create_sub_mip_workroom.push_back(E);
    }

  /* we are going to set m_counts outselves so it matches with what we expect */
  return_value = m_pool->create_mip_element(this);

  reserve_color_tiles(return_value, src_mip, tile_range);
  ASTRALassert(return_value->m_color_counts.m_count.x() == tile_range.x().m_end - tile_range.x().m_begin);
  ASTRALassert(return_value->m_color_counts.m_count.y() == tile_range.y().m_end - tile_range.y().m_begin);

  return_value->m_number_mipmap_levels = src_mip.m_number_mipmap_levels;
  create_shared_common(return_value, empty_tiles, full_tiles,
                       make_c_array(m_pool->m_create_sub_mip_workroom));

  m_pool->m_create_sub_mip_workroom.clear();

  return return_value;
}

void
astral::ImageAtlas::Implement::
reserve_color_tiles(ImageMipElement *in_dst_image,
                    const ImageMipElement &in_src_mip,
                    vecN<range_type<unsigned int>, 2> tile_range)
{
  ImageMipElement::Implement *pdst_image = static_cast<ImageMipElement::Implement*>(in_dst_image);
  ImageMipElement::Implement &dst_image(*pdst_image);

  const ImageMipElement::Implement *psrc_mip = static_cast<const ImageMipElement::Implement*>(&in_src_mip);
  const ImageMipElement::Implement &src_mip(*psrc_mip);

  Counts counts(src_mip.m_color_counts, tile_range);
  uvec2 min_tile, max_tile, image_size, image_start, image_end;

  min_tile = uvec2(tile_range.x().m_begin, tile_range.y().m_begin);
  max_tile = uvec2(tile_range.x().m_end - 1, tile_range.y().m_end - 1);
  image_start = src_mip.tile_location(min_tile);
  image_end = src_mip.tile_location(max_tile) + src_mip.tile_size(max_tile);
  image_size = image_end - image_start;

  dst_image.m_size = image_size;
  dst_image.m_color_counts = counts;
  dst_image.m_number_mipmap_levels = src_mip.m_number_mipmap_levels;

  /* this will only occur if the src-image is that */
  dst_image.m_on_single_unpadded_tile = src_mip.m_on_single_unpadded_tile;

  ASTRALassert(dst_image.m_tiles.empty());
  dst_image.m_tiles.resize(counts.m_count.x() * counts.m_count.y(), nullptr);
}

astral::reference_counted_ptr<astral::ImageMipElement>
astral::ImageAtlas::
create_mip_element(uvec2 sz, unsigned int number_mipmap_levels,
                   c_array<const uvec2> empty_tiles,
                   c_array<const uvec2> fully_covered_tiles,
                   c_array<const std::pair<uvec2, TileElement>> shared_tiles)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  return p->create_mip_element_implement(sz, number_mipmap_levels, empty_tiles, fully_covered_tiles, shared_tiles);
}

astral::reference_counted_ptr<astral::ImageMipElement>
astral::ImageAtlas::Implement::
create_mip_element_implement(uvec2 sz, unsigned int number_mipmap_levels,
                             c_array<const uvec2> empty_tiles,
                             c_array<const uvec2> fully_covered_tiles,
                             c_array<const std::pair<uvec2, TileElement>> shared_tiles)
{
  ImageMipElement::Implement *return_value;

  /* create the object and fill th fields */
  return_value = m_pool->create_mip_element(this);

  reserve_color_tiles(return_value, sz);
  return_value->m_number_mipmap_levels = t_min(return_value->m_number_mipmap_levels, number_mipmap_levels);

  create_shared_common(return_value, empty_tiles, fully_covered_tiles, shared_tiles);

  return return_value;
}

astral::reference_counted_ptr<astral::ImageMipElement>
astral::ImageAtlas::
create_mip_element(uvec2 sz, unsigned int number_mipmap_levels,
                   c_array<const vecN<range_type<int>, 2>> tile_regions)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  return p->create_mip_element_implement(sz, number_mipmap_levels, tile_regions);
}

astral::reference_counted_ptr<astral::ImageMipElement>
astral::ImageAtlas::Implement::
create_mip_element_implement(uvec2 sz, unsigned int number_mipmap_levels,
                             c_array<const vecN<range_type<int>, 2>> tile_regions)
{
  ImageMipElement::Implement *return_value;

  /* create the object and fill th fields */
  return_value = m_pool->create_mip_element(this);

  reserve_color_tiles(return_value, sz);
  return_value->m_number_mipmap_levels = t_min(return_value->m_number_mipmap_levels, number_mipmap_levels);

  /* walk the tiles named in tile_regions and allocate a color tile */
  ivec2 tile_count(return_value->m_color_counts.m_count);
  for (const vecN<range_type<int>, 2> &R : tile_regions)
    {
      for (int tile_y = R.y().m_begin; tile_y < R.y().m_end; ++tile_y)
        {
          for (int tile_x = R.x().m_begin; tile_x < R.x().m_end; ++tile_x)
            {
              uvec2 log2_sz, sz;

              log2_sz.x() = (tile_x + 1 == tile_count.x()) ?
                return_value->m_color_counts.m_log2_remainder_size.x() :
                static_cast<unsigned int>(ImageAtlas::log2_tile_size);

              log2_sz.y() = (tile_y + 1 == tile_count.y()) ?
                return_value->m_color_counts.m_log2_remainder_size.y() :
                static_cast<unsigned int>(ImageAtlas::log2_tile_size);

              sz.x() = (tile_x + 1 == tile_count.x()) ?
                return_value->m_color_counts.m_remainder_size_exact.x() :
                static_cast<unsigned int>(ImageAtlas::tile_size);

              sz.y() = (tile_y + 1 == tile_count.y()) ?
                return_value->m_color_counts.m_remainder_size_exact.y() :
                static_cast<unsigned int>(ImageAtlas::tile_size);

              ASTRALassert(!return_value->fetch_tile(tile_x, tile_y));

              return_value->fetch_tile(tile_x, tile_y) = allocate_color_tile(log2_sz.x(), log2_sz.y(), sz);
              return_value->m_element_tiles[ImageMipElement::color_element].push_back(uvec2(tile_x, tile_y));

              ASTRALassert(return_value->fetch_tile(tile_x, tile_y));
            }
        }
    }

  /* now walk all tiles, any tile not given a color tile is made an empty tile */
  uvec2 utile_count(return_value->m_color_counts.m_count);
  for (unsigned int tile_y = 0; tile_y < utile_count.y(); ++tile_y)
    {
      for (unsigned int tile_x = 0; tile_x < utile_count.x(); ++tile_x)
        {
          ColorTile* &tile(return_value->fetch_tile(tile_x, tile_y));
          if (!tile)
            {
              return_value->m_element_tiles[ImageMipElement::empty_element].push_back(uvec2(tile_x, tile_y));
              tile = m_empty_tile;
              tile->acquire();
            }
        }
    }

  ASTRALassert(return_value->m_element_tiles[ImageMipElement::empty_element].size()
               + return_value->m_element_tiles[ImageMipElement::white_element].size()
               + return_value->m_element_tiles[ImageMipElement::color_element].size()
               == utile_count.x() * utile_count.y());

  create_index_tiles(return_value);

  return return_value;
}

void
astral::ImageAtlas::Implement::
create_shared_common(ImageMipElement *in_return_value,
                     c_array<const uvec2> empty_tiles,
                     c_array<const uvec2> fully_covered_tiles,
                     c_array<const std::pair<uvec2, TileElement>> shared_tiles)
{
  ImageMipElement::Implement *return_value = static_cast<ImageMipElement::Implement*>(in_return_value);

  ASTRALassert(return_value->m_index_images.empty());
  ASTRALassert(return_value->m_element_tiles[ImageMipElement::empty_element].empty());
  ASTRALassert(return_value->m_element_tiles[ImageMipElement::white_element].empty());
  ASTRALassert(return_value->m_element_tiles[ImageMipElement::color_element].empty());

  for (const uvec2 &v : empty_tiles)
    {
      ASTRALassert(!return_value->fetch_tile(v.x(), v.y()));
      return_value->fetch_tile(v.x(), v.y()) = m_empty_tile;
      return_value->fetch_tile(v.x(), v.y())->acquire();
      return_value->m_element_tiles[ImageMipElement::empty_element].push_back(v);
    }

  for (const uvec2 &v : fully_covered_tiles)
    {
      ASTRALassert(!return_value->fetch_tile(v.x(), v.y()));
      return_value->fetch_tile(v.x(), v.y()) = m_white_tile;
      return_value->fetch_tile(v.x(), v.y())->acquire();
      return_value->m_element_tiles[ImageMipElement::white_element].push_back(v);
    }

  for (const std::pair<uvec2, TileElement> &v : shared_tiles)
    {
      ASTRALassert(!return_value->fetch_tile(v.first.x(), v.first.y()));
      ASTRALassert(v.second.m_src);

      const ImageMipElement::Implement *src = static_cast<const ImageMipElement::Implement*>(v.second.m_src.get());

      return_value->fetch_tile(v.first.x(), v.first.y())
        = src->fetch_tile(v.second.m_tile.x(), v.second.m_tile.y());

      /* aquire the tile */
      return_value->fetch_tile(v.first.x(), v.first.y())->acquire();

      return_value->m_element_tiles[ImageMipElement::color_element].push_back(v.first);
    }

  uvec2 tile_count(return_value->m_color_counts.m_count);
  for (unsigned int tile_y = 0; tile_y < tile_count.y(); ++tile_y)
    {
      for (unsigned int tile_x = 0; tile_x < tile_count.x(); ++tile_x)
        {
          uvec2 log2_sz, sz;

          log2_sz.x() = (tile_x + 1u == tile_count.x()) ?
            return_value->m_color_counts.m_log2_remainder_size.x() :
            static_cast<unsigned int>(ImageAtlas::log2_tile_size);

          log2_sz.y() = (tile_y + 1u == tile_count.y()) ?
            return_value->m_color_counts.m_log2_remainder_size.y() :
            static_cast<unsigned int>(ImageAtlas::log2_tile_size);

          sz.x() = (tile_x + 1u == tile_count.x()) ?
            return_value->m_color_counts.m_remainder_size_exact.x() :
            static_cast<unsigned int>(ImageAtlas::tile_size);

          sz.y() = (tile_y + 1u == tile_count.y()) ?
            return_value->m_color_counts.m_remainder_size_exact.y() :
            static_cast<unsigned int>(ImageAtlas::tile_size);

          ColorTile* &tile(return_value->fetch_tile(tile_x, tile_y));
          if (!tile)
            {
              tile = allocate_color_tile(log2_sz.x(), log2_sz.y(), sz);
              return_value->m_element_tiles[ImageMipElement::color_element].push_back(uvec2(tile_x, tile_y));
            }
        }
    }

  ASTRALassert(return_value->m_element_tiles[ImageMipElement::empty_element].size()
               + return_value->m_element_tiles[ImageMipElement::white_element].size()
               + return_value->m_element_tiles[ImageMipElement::color_element].size()
               == tile_count.x() * tile_count.y());

  create_index_tiles(return_value);
}

enum astral::return_code
astral::ImageAtlas::Implement::
create_index_tiles_implement(uvec2 tile_count, std::vector<IndexImage> *dst_cnts,
                             std::vector<IndexTile*> *dst_index_tiles,
                             bool upload_index_workroom)
{
  ASTRALassert(!upload_index_workroom || tile_count.x() * tile_count.y() == m_index_workroom.size());
  if (tile_count.x() == 1u && tile_count.y() == 1u)
    {
      /* There should be no IndexImage only when there are no IndexTiles */
      ASTRALassert(dst_cnts->empty() == dst_index_tiles->empty());
      return routine_success;
    }

  IndexImage index_image(tile_count, dst_index_tiles->size());

  /* Save the value of index_image and allocate the index tiles */
  dst_cnts->push_back(index_image);
  dst_index_tiles->resize(dst_index_tiles->size() + index_image.num_tiles());
  for (unsigned int y = 0; y < index_image.m_cnt.m_count.y(); ++y)
    {
      for (unsigned int x = 0; x < index_image.m_cnt.m_count.x(); ++x)
        {
          uvec2 log2_sz;
          unsigned int tile_idx;
          IndexTile *I;

          log2_sz = index_image.tile_log2_size(x, y);
          I = allocate_index_tile(log2_sz.x(), log2_sz.y());

          if (!I)
            {
              return routine_fail;
            }

          tile_idx = index_image.tile_index(x, y);
          (*dst_index_tiles)[tile_idx] = I;
        }
    }

  if (upload_index_workroom)
    {
      /* upload the index texels */
      index_image.upload_texels(*this, make_c_array(*dst_index_tiles), tile_count.x(), make_c_array(m_index_workroom));
    }

  /* ready m_index_workroom for the next level up */
  m_index_workroom.clear();
  m_index_workroom.resize(index_image.m_cnt.m_count.x() * index_image.m_cnt.m_count.y());
  for (unsigned int y = 0; y < index_image.m_cnt.m_count.y(); ++y)
    {
      for (unsigned int x = 0; x < index_image.m_cnt.m_count.x(); ++x)
        {
          uvec3 v;
          unsigned int idx;

          idx = index_image.tile_index(x, y);
          v = (*dst_index_tiles)[idx]->location();
          m_index_workroom[x + y * index_image.m_cnt.m_count.x()] = v;
        }
    }

  return create_index_tiles_implement(index_image.m_cnt.m_count, dst_cnts, dst_index_tiles, true);
}

void
astral::ImageAtlas::Implement::
create_index_tiles(ImageMipElement *in_image)
{
  ImageMipElement::Implement *pimage = static_cast<ImageMipElement::Implement*>(in_image);
  ImageMipElement::Implement &image(*pimage);

  ASTRALassert(image.m_index_tiles.empty());
  ASTRALassert(image.m_index_images.empty());

  if (image.m_tile_allocation_failed)
    {
      image.m_index_tiles.push_back(m_scratch_index_tile);
      return;
    }

  uvec2 index_texel_count(image.m_color_counts.m_count);

  if (index_texel_count.x() != 1u
      || index_texel_count.y() != 1u)
    {
      /* If the Image is not a lone color tile, then we pad
       * the index tiles by one on each side so that a fragment
       * shader that performs the tiled image lookup can safely
       * assume that a single index texel at the level above the
       * color tiles corresponds to tile_size_without_padding
       * texels in each dimension. The padding of a single index
       * texel points to the location -within- the color tiles
       * along the last row or column that another color tile
       * would start at. Note that padding the index tiles if
       * there is only a single color tile would be incorrect
       * because if there is a single tile, it embodies an area
       * of TileSize X TileSize and the logic of Image
       * counting the number of indirect levels would be
       * incorrect.
       */
      index_texel_count += uvec2(1u, 1u);
    }

  enum return_code R;

  R = create_index_tiles_implement(index_texel_count, &image.m_index_images, &image.m_index_tiles, false);
  if (R == routine_fail)
    {
      on_tile_allocation_failed(image);
    }

  image.m_ratio = ImageMipElement::compute_ratio(image.m_index_images.size());
}

void
astral::ImageAtlas::Implement::
upload_index_values_for_single_tile_image(ImageMipElement::Implement &image)
{
  ASTRALassert(image.m_index_images.empty());
  ASTRALassert(image.m_index_tiles.size() == 1u);
  ASTRALassert(image.m_tiles.size() == 1u);
  ASTRALassert(image.m_index_tiles.front()->log2_size() == uvec2(0, 0));

  unsigned int pitch(1);
  uvec2 sz(1, 1);

  vecN<uvec3, 1> texel_value(image.fetch_tile(0, 0)->location());
  internal_upload_index_texels(image.m_index_tiles.front()->location(), sz, pitch, texel_value);
}

void
astral::ImageAtlas::Implement::
allocate_color_tile_backings(ImageMipElement &in_image)
{
  ImageMipElement::Implement *p = static_cast<ImageMipElement::Implement*>(&in_image);
  ImageMipElement::Implement &image(*p);
  unsigned int cnt(0), total_cnt(0);

  if (image.m_allocate_color_tile_backings_called)
    {
      return;
    }

  image.m_allocate_color_tile_backings_called = true;
  for (ColorTile *p : image.m_tiles)
    {
      if (!p->backing_allocated())
        {
          ++cnt;
        }

      if (p != m_empty_tile && p != m_white_tile)
        {
          ++total_cnt;
        }

      if (routine_fail == p->allocate_backing())
        {
          on_tile_allocation_failed(image);
          return;
        }
    }

  if (false)
    {
      std::cout << "Allocate " << cnt << "/" << total_cnt << " color tiles\n";
    }

  /* When image.m_index_images is empty, that indicates
   * that the image is a single tile. However, it can
   * still have an index tile if tile_index_atlas_location()
   * was called on; in that case image.m_index_tiles
   * will be non-empty but not initialized yet.
   */
  if (image.m_index_images.empty())
    {
      if (image.m_index_tiles.empty())
        {
          /* Having no index tiles means that tile_index_atlas_location()
           * was not called, thus we do not need to have that index tile
           * at all.
           */
          return;
        }

      upload_index_values_for_single_tile_image(image);
      return;
    }

  /* initialize m_index_workroom to hold the first level of
   * index tiles above the color tiles.
   */
  m_index_workroom.resize(image.m_index_images.front().m_size.x() * image.m_index_images.front().m_size.y());
  for (unsigned int y = 0, idx = 0; y < image.m_index_images.front().m_size.y(); ++y)
    {
      for (unsigned int x = 0; x < image.m_index_images.front().m_size.x(); ++x, ++idx)
        {
          unsigned int xx, yy;
          uvec2 sz;
          ColorTile *color_tile;

          xx = t_min(x, image.m_color_counts.m_count.x() - 1u);
          yy = t_min(y, image.m_color_counts.m_count.y() - 1u);

          color_tile = image.fetch_tile(xx, yy);
          m_index_workroom[idx] = color_tile->location();
          sz = color_tile->size();

          /* if we are on the last index texel horizontally or
           * vertically, adjust the value so that it points
           * to the texels at the end of the last tile.
           */
          if (xx != x)
            {
              unsigned int padding(2u);
              m_index_workroom[idx].x() = m_index_workroom[idx].x() + sz.x() - t_min(padding, sz.x());
            }

          if (yy != y)
            {
              unsigned int padding(2u);
              m_index_workroom[idx].y() = m_index_workroom[idx].y() + sz.y() - t_min(padding, sz.y());
            }
        }
    }

  image.m_index_images.front().upload_texels(*this, make_c_array(image.m_index_tiles),
                                             image.m_index_images.front().m_size.x(),
                                             make_c_array(m_index_workroom));
}

void
astral::ImageAtlas::Implement::
on_tile_allocation_failed(ImageMipElement::Implement &image)
{
  image.m_allocate_color_tile_backings_called = true;
  image.m_tile_allocation_failed = false;

  /* clear all of the color tiles and set the index tiles to just the scratch clear tile */
  for (IndexTile *p : image.m_index_tiles)
    {
      release_tile(p);
    }

  std::fill(image.m_index_tiles.begin(), image.m_index_tiles.end(), m_scratch_index_tile);
  for (auto &tile_ptr : image.m_tiles)
    {
      release_tile(tile_ptr);
      tile_ptr = m_empty_tile;
      tile_ptr->acquire();
    }
}

void
astral::ImageAtlas::Implement::
reserve_color_tiles(ImageMipElement *in_image, uvec2 sz)
{
  ImageMipElement::Implement *pimage = static_cast<ImageMipElement::Implement*>(in_image);
  ImageMipElement::Implement &image(*pimage);

  const unsigned int max_num_mips(ImageMipElement::maximum_number_of_mipmaps);
  Counts counts(sz, true);

  /* TODO: should we have special case code for when a dimension is one?
   *       It seems horribly silly to give a two pixel padding for that
   *       case.
   */
  ASTRALassert(image.m_index_images.empty());

  image.m_on_single_unpadded_tile = (counts.m_count == uvec2(1, 1));

  image.m_size = sz;
  image.m_color_counts = counts;

  /* compute the number of supported mipmaps, no greater than
   * ImageMipElement::maximum_number_of_mipmaps
   */
  image.m_number_mipmap_levels = 1u + uint32_log2_floor(t_min(sz.x(), sz.y()));
  image.m_number_mipmap_levels = t_min(image.m_number_mipmap_levels, max_num_mips);

  ASTRALassert(image.m_tiles.empty());
  image.m_tiles.resize(counts.m_count.x() * counts.m_count.y(), nullptr);
}

const astral::TileAllocator::Tile*
astral::ImageAtlas::Implement::
allocate_tile(unsigned int max_number_layers, TileAllocator &allocator,
              unsigned int log2_width, unsigned int log2_height)
{
  const TileAllocator::Tile *return_value;

  return_value = allocator.allocate_tile(log2_width, log2_height);
  if (return_value == nullptr && allocator.number_layers() < max_number_layers)
    {
      /* resize */
      unsigned int L;

      L = allocator.number_layers();
      allocator.number_layers(L + 1u);

      return_value = allocator.allocate_tile(log2_width, log2_height);
    }

  return return_value;
}

astral::ImageAtlas::Implement::IndexTile*
astral::ImageAtlas::Implement::
allocate_index_tile(unsigned int log2_width, unsigned int log2_height)
{
  return allocate_tile(m_index_backing->max_number_layers(), m_index_tile_allocator, log2_width, log2_height);
}

astral::ImageAtlas::Implement::ColorTile*
astral::ImageAtlas::Implement::
allocate_color_tile(unsigned int log2_width, unsigned int log2_height, uvec2 actual_size, bool allocate_backing)
{
  const TileAllocator::Tile *location;

  if (allocate_backing)
    {
      location = allocate_tile(m_color_backing->max_number_layers(), m_color_tile_allocator, log2_width, log2_height);
      if (location)
        {
          return m_pool->create_color_tile(location, *this, actual_size);
        }
      else
        {
          return nullptr;
        }
    }

  return m_pool->create_color_tile(log2_width, log2_height, *this, actual_size);
}

astral::ImageID
astral::ImageAtlas::Implement::
allocate_image_id(Image *image)
{
  ImageID ID;
  if (!m_free_ids.empty())
    {
      ID = m_free_ids.back();
      m_free_ids.pop_back();
    }
  else
    {
      ID.m_slot = m_image_fetcher.size();
      ID.m_uniqueness = 0u;
      m_image_fetcher.push_back(nullptr);
    }

  ASTRALassert(ID.m_slot < m_image_fetcher.size());
  ASTRALassert(m_image_fetcher[ID.m_slot] == nullptr);
  m_image_fetcher[ID.m_slot] = image;
  return ID;
}

void
astral::ImageAtlas::Implement::
free_image_id(Image *in_image)
{
  Image::Implement *image = static_cast<Image::Implement*>(in_image);
  ImageID ID;

  ASTRALassert(image);
  ID = image->m_image_id;

  ASTRALassert(m_image_fetcher.size() > ID.m_slot);
  ASTRALassert(m_image_fetcher[ID.m_slot] == image);

  m_image_fetcher[ID.m_slot] = nullptr;

  /* when retiring an ID, increment the uniqueness
   * so that when the slot is reused, it gives a
   * unique ImageID value still.
   */
  ++ID.m_uniqueness;
  m_free_ids.push_back(ID);
}

void
astral::ImageAtlas::Implement::
release_tile(ColorTile *tile)
{
  if (tile->release())
    {
      if (tile->backing_allocated())
        {
          m_color_tile_allocator.release_tile(tile->tile());
        }
      m_pool->reclaim(tile);
    }
}

void
astral::ImageAtlas::Implement::
release_tile(IndexTile *tile)
{
  m_index_tile_allocator.release_tile(tile);
}

void
astral::ImageAtlas::
lock_resources(void)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  ++p->m_resources_locked;
}

void
astral::ImageAtlas::
unlock_resources(void)
{
  Implement *p;

  p = static_cast<Implement*>(this);
  ASTRALassert(p->m_resources_locked > 0);

  --p->m_resources_locked;
  if (p->m_resources_locked == 0)
    {
      p->m_in_use_images.clear();
      ++p->m_resources_unlock_count;
    }
}

const astral::ImageAtlasColorBacking&
astral::ImageAtlas::
color_backing(void)
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return *p->m_color_backing;
}

const astral::ImageAtlasIndexBacking&
astral::ImageAtlas::
index_backing(void)
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return *p->m_index_backing;
}

unsigned int
astral::ImageAtlas::
total_color_pixels_allocated(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_color_tile_allocator.space_allocated();
}

unsigned int
astral::ImageAtlas::
total_index_pixels_allocated(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_index_tile_allocator.space_allocated();
}

unsigned int
astral::ImageAtlas::
total_images_allocated(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_pool->total_images_allocated();
}

unsigned int
astral::ImageAtlas::
total_image_mip_elements_allocated(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->m_pool->total_image_mip_elements_allocated();
}

astral::Image*
astral::ImageAtlas::
fetch_image(ImageID ID)
{
  Image::Implement *im;
  Implement *p;

  p = static_cast<Implement*>(this);

  if (!ID.valid() || ID.m_slot >= p->m_image_fetcher.size())
    {
      return nullptr;
    }

  im = static_cast<Image::Implement*>(p->m_image_fetcher[ID.m_slot]);
  ASTRALassert(!im || im->m_image_id.m_slot == ID.m_slot);
  if (!im || im->m_image_id.m_uniqueness != ID.m_uniqueness)
    {
      return nullptr;
    }

  return im;
}

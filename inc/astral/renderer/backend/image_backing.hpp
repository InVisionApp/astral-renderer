/*!
 * \file image_backing.hpp
 * \brief file image_backing.hpp
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

#ifndef ASTRAL_IMAGE_BACKING_HPP
#define ASTRAL_IMAGE_BACKING_HPP

#include <astral/util/reference_counted.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/util.hpp>
#include <astral/util/rect.hpp>
#include <astral/util/color.hpp>
#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/render_value.hpp>
#include <astral/renderer/render_target.hpp>
#include <astral/renderer/image_sampler_bits.hpp>
#include <astral/renderer/image_id.hpp>
#include <astral/util/tile_allocator.hpp>

namespace astral
{
  class Renderer;
  class ImageID;
  class ImageAtlasBacking;
  class ImageAtlasColorBacking;
  class ImageAtlasIndexBacking;
  class ImageAtlas;
  class ImageMipElement;
  class Image;

  namespace detail
  {
    ///@cond

    /* The only purpose of this class is to allow
     * for Renderer to specify the value of
     * Imae::offscreen_render_index() of Image
     * without being a friend of Image.
     */
    class RenderedImageTag
    {
    private:
      friend class astral::Image;
      friend class astral::ImageAtlas;
      friend class astral::Renderer;

      explicit
      RenderedImageTag(uint32_t idx):
        m_offscreen_render_index(idx)
      {}

      RenderedImageTag(void):
        m_offscreen_render_index(InvalidRenderValue)
      {}

      uint32_t m_offscreen_render_index;
    };

    ///@endcond
  }

/*!\addtogroup RendererBackend
 * @{
 */

  /*!
   * An astral::ImageAtlas backs the image data.
   * The image data is broken into tiles that have padding;
   * this padding gives the ability for shaders to easily perform
   * bilinear and bicubic filtering on the tiled images.
   *
   * Let the padding be P and the tile size be T, let Z = T - 2P.
   * P is given by ImageAtlas::tile_padding, Z is given by
   * ImageAtlas::tile_size_without_padding and T is given by
   * ImageAtlas::tile_size. The one-dimension picture is the
   * following:
   *
   *  Tile{n} <-----> [-P + n * Z, P + (n + 1) * Z)
   *
   * When sampling from a tile, it will be that sampling point
   * is always within the INTERIOR of the tile, i.e. the sampling
   * when sampling Tile{n} at point S one has that
   *
   *   n * Z <= S < (n + 1) * Z.
   *
   * With this guarantee, one can move (up to) P texels from S;
   * this enables to use HW bilinear filtering when P >= 1 and
   * if samping bicubic, to stay within the tile if P >= 2. Since
   * P = ImageAtlas::tile_padding, sampling bilinear and
   * bicubic "just works" once the texel coordinate in the color
   * backing store is known.
   *
   * An image broken into tiles has an index tile where each
   * texel of the index tile gives a min-min corner of a color
   * tile. For each texel moved in the index tile, one moves
   * Z (not T) texels in the color tile. This is because the
   * effective size of each color tile when sampling is Z.
   * If the size of the image is so great that a single index
   * tile cannot address all of the color tiles, then there are
   * multiple layers of index tiles above the color tiles. When
   * going from index layer to index layer, because an index layer
   * does not have padding, then moving one texel in a parent index
   * tile moves T texels in a child INDEX layer. Image data will
   * have as many index layers as necessary so that the top is
   * one index tile, called the root index tile, whose lcoation is
   * given by ImageMipElement::root_tile_location().
   *
   * Moving a single texel in the root index tile then moves
   * R = Z * T^(N - 1) texels in the color tiles where N is the
   * number of index layers, given by  ImageMipElement::number_index_levels()).
   * This value is available in the method ratio(). Note that since T is
   * ImageAtlas::tile_size which is perfect power of 2 with log2
   * having value ImageAtlas::log2_tile_size, then the value
   * of ImageMipElement::ratio() is given by:
   * \code
   *        Z = ImageAtlas::tile_size_without_padding;
   *        N = number_index_levels();
   *        T = ImageAtlas::tile_size;
   *   log2_T = ImageAtlas::log2_tile_size;
   *  ratio() = Z * (1 << ((N - 1) * log2_T));
   * \endcode
   *
   * In the case the size of the image is less than equal to
   * ImageAtlas::tile_size, there are no index tiles and
   * there is no need for padding either and sampling at S is
   * accomplished by directly samling from the color backing
   * store at uvec3(S.x, S.y, 0) + root_tile_location().
   *
   * The next issue for tiled image is handling mipmaps and
   * the filtering of mipmaps. The mipmap LOD = 1 of an image
   * is an image of half the resolution in each dimension.
   * We can place that image as a tiled image in the LOD = 1
   * of the astral::ImageAtlasColorBacking with a padding
   * of 1 as well. Because a texel at LOD = 1 is two texels
   * at LOD = 0, the arithematic for getting the color tile
   * from an index texel works correctly for LOD = 1. In addition.
   * LOD = 1 can be linear (but not cubically) filtered because
   * it has one real texel padding. However, this breaks down
   * for LOD >= 2 not just for filtering but also for the index
   * tile look up because moving a single texel in the root
   * index tile moves ImageAtlas::tile_size / 2^LOD texels
   * in the LOD color tile for LOD >= 2, thus when doing the
   * fetching the ratio should be T * (1 << ((N - 1) * log2_T))
   * instead of Z * (1 << ((N - 1) * log2_T)). The solution is
   * that to support mipmapping beyond LOD = 1, a tiled image
   * has a mipmap chain C[] where C[n] stores image data for
   * LOD = {2n, 2n + 1}. This enables, through a shader to
   * perform mipmap filtering as GL's ASTRAL_GL_LINEAR_MIPMAP_NEAREST.
   * The chain is accessed by Image::mip_chain().
   *
   * Supporting tri-linear filtering means that a shader would
   * need the logic to check if the two LOD's to sample from
   * are in the same ImageMipElement. If they are not, then
   * it would need to sample from both and perform the mixing
   * itself. The other way is to have a pixel padding of *four*
   * pixels and have a Image::mip_chain()[n] store the LOD's
   * {2n, 2n + 1, 2n + 2}. This solution is too wasteful because
   * not only does it mean more padding pixels, it also means
   * that the non-zero even mip-levels must be stored twice. Eww.
   */
  class ImageAtlas:public reference_counted<ImageAtlas>::non_concurrent
  {
  public:
    enum
      {
        /*!
         * log2 of the tile size
         */
        log2_tile_size = 6u,

        /*!
         * The size of a full color tile including padding
         */
        tile_size = (1u << log2_tile_size),

        /*!
         * The padding given to each tile; this padding
         * ensures that sampling from a tile does not
         * leak outside of it.
         */
        tile_padding = 2u,

        /*!
         * the size of a full color tile without padding
         */
        tile_size_without_padding = tile_size - 2 * tile_padding
      };

    /*!
     * Class to specify a tile from an astral::ImageMipElement.
     * This is used to allow for the backing stores of tiles of
     * different astral::ImageMipElement objects to be shared.
     * The color tile is backed only once, thus if any of the
     * astral::ImageMipElement change its texels, that change
     * is reflected also in the other astral::ImageMipElement
     * objects.
     */
    class TileElement
    {
    public:
      /*!
       * The astral::ImageMipElement from which
       * to take the tile
       */
      reference_counted_ptr<const ImageMipElement> m_src;

      /*!
       * Which tile from \ref m_src. A tile with coordinate
       * (x, y) covers the region gvein by
       *
       * [-P + (x - 1) * Z, P + x * Z) x [-P + (y - 1) * Z, P + y * Z)
       *
       * where
       *
       *   - P = padding size = ImageAtlas::padding_size
       *   - T = tile size = ImageAtlas::tile_size
       *   - Z = T - 2 * P
       */
      uvec2 m_tile;
    };

    /*!
     * Ctor.
     * \param color_backing backing for the color tiles
     * \param index_backing backing for the index tiles
     */
    static
    reference_counted_ptr<ImageAtlas>
    create(ImageAtlasColorBacking &color_backing,
           ImageAtlasIndexBacking &index_backing);

    virtual
    ~ImageAtlas()
    {}

    /*!
     * Create a Image backed by the backing of this
     * astral::ImageAtlas.
     *
     * \param sz the dimensions of the image
     * \param colorspace initial value for the colorspace of the
     *                   created image.
     */
    reference_counted_ptr<Image>
    create_image(uvec2 sz, enum colorspace_t colorspace = colorspace_srgb);

    /*!
     * Create a Image backed by the backing of this
     * astral::ImageAtlas.
     *
     * \param num_mip_levels number of mipmap levels the returned image shall have
     * \param sz the dimensions of the image
     * \param colorspace initial value for the colorspace of the
     *                   created image.
     */
    reference_counted_ptr<Image>
    create_image(unsigned int num_mip_levels, uvec2 sz,
                 enum colorspace_t colorspace = colorspace_srgb);

    /*!
     * Create a astral::Image by explicitely providing its
     * mipmap chain. Note that if multiple astral::Image
     * share astral::ImageMipElement, then for those mip-levels
     * the backing is shared.
     *
     * \param mip_chain mipmap chain; the top element's size is
     *                  the size of the return value. For correct
     *                  support of mipmapping, all but the last
     *                  element of the chain must be that the value
     *                  ImageMipElement::number_mipmap_levels()
     *                  is ImageMipElement::maximum_number_of_mipmaps
     *                  and the sizes of successive ImageMipElement
     *                  reduces by a factor 2 ^ N where N is the value
     *                  of astral::ImageMipElement::maximum_number_of_mipmaps
     * \param colorspace initial value for the colorspace of the
     *                   created image.
     */
    reference_counted_ptr<Image>
    create_image(c_array<const reference_counted_ptr<const ImageMipElement>> mip_chain,
                 enum colorspace_t colorspace = colorspace_srgb);

    /*!
     * Create an astral::ImageMipElement backed by the backings
     * of this astral::ImageAtlas. Tiles not listed in empty_tiles
     * of fully_covered_tiles will be backed by color tiles.
     *
     * \param sz the dimensions of the image
     * \param empty_tiles rects that are to be completely (0, 0, 0, 0)
     * \param fully_covered_tiles rects that are to be completely (255, 255, 255, 255)
     *
     * Note: a tile with coordinate (x, y) covers the region
     * [-P + (x - 1) * Z, P + x * Z) x [-P + (y - 1) * Z, P + y * Z)
     * where
     *   - P = padding size = ImageAtlas::padding_size
     *   - T = tile size = ImageAtlas::tile_size
     *   - Z = T - 2 * P
     */
    reference_counted_ptr<ImageMipElement>
    create_mip_element(uvec2 sz,
                       c_array<const uvec2> empty_tiles,
                       c_array<const uvec2> fully_covered_tiles);

    /*!
     * Create an astral::ImageMipElement backed by the backings
     * of this astral::ImageAtlas. Tiles not listed in empty_tiles,
     * fully_covered_tiles or shared_tiles will be backed by color tiles
     * allocated for the created astral::ImageMipElement object
     *
     * \param sz the dimensions of the image
     * \param number_mipmap_levels the number of mipmap levels the
     *                             created objet will support
     * \param empty_tiles rects that are to be completely (0, 0, 0, 0)
     * \param fully_covered_tiles rects that are to be completely (255, 255, 255, 255)
     * \param shared_tiles rects whose tile backing is shared with
     *                     another astral::ImageMipElement;
     *                     .first provides the location within the
     *                     created astral::ImageMipElement
     *
     * Note: a tile with coordinate (x, y) covers the region
     * [-P + (x - 1) * Z, P + x * Z) x [-P + (y - 1) * Z, P + y * Z)
     * where
     *   - P = padding size = ImageAtlas::padding_size
     *   - T = tile size = ImageAtlas::tile_size
     *   - Z = T - 2 * P
     */
    reference_counted_ptr<ImageMipElement>
    create_mip_element(uvec2 sz, unsigned int number_mipmap_levels,
                       c_array<const uvec2> empty_tiles,
                       c_array<const uvec2> fully_covered_tiles,
                       c_array<const std::pair<uvec2, TileElement>> shared_tiles);

    /*!
     * Create an astral::ImageMipElement backed by the backings
     * of this astral::ImageAtlas. Tiles not listed will be backed
     * by empty empty_tiles
     * \param sz the dimensions of the image
     * \param number_mipmap_levels the number of mipmap levels the
     *                             created objet will support
     * \param tile_ranges array of disjoint tile ranges listing what
     *                    tiles of image are to be backed
     */
    reference_counted_ptr<ImageMipElement>
    create_mip_element(uvec2 sz, unsigned int number_mipmap_levels,
                       c_array<const vecN<range_type<int>, 2>> tile_ranges);

    /*!
     * Create an astral::ImageMipElement that corresponds to a
     * sub-range of tiles of a starting astral::ImageMipElement
     * where in addition some tiles can also be overridden to be
     * empty or color tiles.
     * \param src_mip source astral::ImageMipElement with which to share
     *                tiles
     * \param tile_range gives the range of tiles into the image
     *                   the are listed; the created sub-image's
     *                   tile(0, 0) corresponds to src_image's
     *                   (RX, RY) where RX = tile_range.x().m_begin and
     *                   RY = tile_range.y().m_begin.
     * \param empty_tiles list of tiles that will be empty tiles in
     *                    the created image. These value are relative
     *                    to the created image, i.e. if (TX, TY) is in
     *                    the list, then the tile (TX, TY)
     *                    of the created image is an empty tile
     * \param full_tiles list of tiles that will be full tiles in
     *                   the created image. These value are relative
     *                   to the created image, i.e. if (TX, TY) is in
     *                   the list, then the tile (TX, TY)
     *                   of the created image is a full tile
     * \param shared_tiles list of tiles that will be taken from the
     *                     source image, i.e. if  (TX, TY) is in the
     *                     list then the tile (TX - RX, TY - RY) of
     *                     the created image will be same exact tile
     *                     as (TX, TY) of the source image
     */
    reference_counted_ptr<const ImageMipElement>
    create_mip_element(const ImageMipElement &src_mip,
                       vecN<range_type<unsigned int>, 2> tile_range,
                       c_array<const uvec2> empty_tiles,
                       c_array<const uvec2> full_tiles,
                       c_array<const uvec2> shared_tiles);

    /*!
     * During a lock_resources()/unlock_resources() pair,
     * rather than freeing the regions of release tile
     * objects directly, the regions are only marked to be
     * free and will be released on unlock_resources().
     * The use case is that during a Renderer::begin()/end()
     * pair, an astral::Image is used and its last reference
     * goes out of scope triggering its deletion, but because
     * the GPU still has not been sent the GPU commands to
     * draw, the pixels are needed until Renderer::end().
     * Thus, Renderer uses lock_resources() to keep the pixels
     * "alive" until it sends them to the GPU.
     *
     * Nesting is supported (though ill-advised) and resources
     * are cleared on the call to the outer-most unlock_resources().
     */
    void
    lock_resources(void);

    /*!
     * Release the regions marked for deletion since
     * lock_resources() was called.
     */
    void
    unlock_resources(void);

    /*!
     * Return the astral::ImageAtlasColorBacking
     */
    const ImageAtlasColorBacking&
    color_backing(void);

    /*!
     * Return the astral::ImageAtlasColorBacking
     */
    const ImageAtlasIndexBacking&
    index_backing(void);

    /*!
     * Flush all CPU texel uploads, GPU uploads and resizes
     *
     * NOTE: it is unsafe to call this outside of a Renderer::begin()/Renderer::end()
     *       pair in environments where the 3D API state is affected by anything
     *       else besides Astral.
     *
     */
    void
    flush(void);

    /*!
     * When resizing the color backing to hold the images,
     * one can specify to resize the color backing to be able
     * to hold additional texel beyond that what is needed
     * for a flush command; the usual case is when the render
     * expects to need many offscreen pixels.
     */
    void
    extra_color_backing_texels(unsigned int);

    /*!
     * Returns the location in color_backing() where
     * the (shared) empty tile is located. This value
     * is guarnteed to be the same for the lifetime
     * of this astral::ImageAtlas object.
     */
    uvec3
    empty_tile_atlas_location(void) const;

    /*!
     * Returns the location in color_backing() where
     * the (shared) white tile is located. This value
     * is guarnteed to be the same for the lifetime
     * of this astral::ImageAtlas object.
     */
    uvec3
    white_tile_atlas_location(void) const;

    /*!
     * Returns the number of pixels that all color tiles that are
     * live consume.
     */
    unsigned int
    total_color_pixels_allocated(void) const;

    /*!
     * Returns the number of pixels that all index tiles that are
     * live consume.
     */
    unsigned int
    total_index_pixels_allocated(void) const;

    /*!
     * Returns the number of astral::Image objects that are live.
     */
    unsigned int
    total_images_allocated(void) const;

    /*!
     * Returns the number of astral::ImageMipElement objects that are live.
     */
    unsigned int
    total_image_mip_elements_allocated(void) const;

    /*!
     * Return an astral::Image from a unique Image ID; if the
     * astral::Image is not alive with the passed ID, or if the
     * ID is not valid, will return nullptr.
     */
    Image*
    fetch_image(ImageID ID);

    /*!
     * Returns the start of the tile including the padding
     * for an LOD level
     */
    static
    int
    tile_start(int tile, int lod);

    /*!
     * Returns the end of the tile including the padding.
     * for an LOD level.
     */
    static
    int
    tile_end(int tile, int lod);

    /*!
     * Returns THE tile whose interior contains the named texel
     */
    static
    int
    tile_from_texel(int texel, int lod);

    /*!
     * Given a size, computes the number of image tiles needed
     * to hold an image of that size where the tiles are of size
     * ImageAtlas::tile_size where the contents of each
     * tile have ImageAtlas::padding_size on the boundary
     * of each tile.
     * \param sz size of image query
     * \param out_remainder_size if non-null, location to which
     *                           to write the width and height
     *                           of the tiles in the last row
     *                           and column; these tiles will
     *                           only have padding in the front,
     *                           but not in the back
     * \returns the number of tiles to fully contain an image of
     *          the passed size, sz.
     */
    static
    uvec2
    tile_count(uvec2 sz, uvec2 *out_remainder_size = nullptr);

    /*!
     * Overload to tile_count(uvec2, uvec2*) that operates on
     * astral::ivec2 values instead of astral::uvec2 vales.
     */
    static
    ivec2
    tile_count(ivec2 sz, ivec2 *out_remainder_size = nullptr);

    ///@cond

    /* The below create_rendered_image() routines are used exclusively
     * by Renderer to create Image objects whose content are rendered
     * to a scratch RenderTarget and then blitted to the Image. The
     * main need for the tagging is to facilitate dependency tracking
     * for rendered Images.
     */

    reference_counted_ptr<Image>
    create_rendered_image(detail::RenderedImageTag tag,
                          uvec2 sz, enum colorspace_t colorspace);

    reference_counted_ptr<Image>
    create_rendered_image(detail::RenderedImageTag tag, unsigned int num_mip_levels,
                          uvec2 sz, enum colorspace_t colorspace);

    reference_counted_ptr<Image>
    create_rendered_image(detail::RenderedImageTag tag,
                          c_array<const reference_counted_ptr<const ImageMipElement>> mip_chain,
                          enum colorspace_t colorspace);

    ///@endcond

  private:
    friend class Image;
    friend class ImageMipElement;

    class Implement;

    ImageAtlas(void)
    {}
  };

  /*!
   * \brief
   * Common base clase for the backing stores of a \ref ImageAtlas
   */
  class ImageAtlasBacking:public reference_counted<ImageAtlasBacking>::non_concurrent
  {
  public:
    /*!
     * Ctor.
     * \param width_height the width and height of each layer. The value must be
     *                     of the form V = ImageAtlas::tile_size * N
     * \param number_layers the number of layers.
     * \param max_number_layers the maximum number of layers the atlas can
     *                          be resized to.
     */
    ImageAtlasBacking(unsigned int width_height, unsigned int number_layers,
                      unsigned int max_number_layers):
      m_width_height(width_height),
      m_number_layers(number_layers),
      m_max_number_layers(max_number_layers)
    {
      ASTRALassert((m_width_height & (ImageAtlas::tile_size - 1u)) == 0u);
    }

    virtual
    ~ImageAtlasBacking()
    {}

    /*!
     * Resize the atlas
     */
    void
    number_layers(unsigned int L)
    {
      ASTRALassert(L >= m_number_layers);
      on_resize(L);
      m_number_layers = L;
    }

    /*!
     * Returns the number of layer of the atlas.
     */
    unsigned int
    number_layers(void) const
    {
      return m_number_layers;
    }

    /*!
     * Returns the maximum number of layers the backing
     * can ever be
     */
    unsigned int
    max_number_layers(void) const
    {
      return m_max_number_layers;
    }

    /*!
     * Returns the width and height of each layer of the atlas.
     */
    unsigned int
    width_height(void) const
    {
      return m_width_height;
    }

  protected:
    /*!
     * To be implemented by a derived class to resize the atlas.
     * The value of number_layers() is the value -before- the
     * resize is to occur.
     */
    virtual
    void
    on_resize(unsigned int new_number_layers) = 0;

  private:
    unsigned int m_width_height, m_number_layers, m_max_number_layers;
  };

  /*!
   * Backing store for the color texels of a \ref ImageAtlas
   */
  class ImageAtlasColorBacking:public ImageAtlasBacking
  {
  public:
    /*!
     * Ctor.
     * \param width_height the width and height of each layer. The value must be
     *                     of the form V = ImageAtlas::tile_size * N
     * \param number_layers the number of layers.
     * \param max_number_layers the maximum number of layers the atlas can
     *                          be resized to.
     */
    ImageAtlasColorBacking(unsigned int width_height,
                           unsigned int number_layers,
                           unsigned int max_number_layers):
      ImageAtlasBacking(width_height, number_layers, max_number_layers)
    {}

    /*!
     * To be implemented by a derived class to flush changes
     * accumulated by upload_texels(), copy_pixels() and
     * downsample_pixels().
     */
    virtual
    void
    flush(void) = 0;

    /*!
     * To be implemented by a derived class to upload texels that
     * are used for color tiles. It is an error for upload_texels()
     * to be called after copy_pixels() without having a call
     * flush_gpu() between them.
     *
     * \param lod mipmap level
     * \param location location within in atlas
     * \param size size of rect to affect
     * \param texels values to upload
     */
    virtual
    void
    upload_texels(unsigned int lod, uvec3 location, uvec2 size, c_array<const u8vec4> texels) = 0;

    /*!
     * To be implemented by a derived class to copy pixels from a
     * single astral::ColorBuffer to the color backing. It is an
     * error for copy_pixels() to be called after upload_texels()
     * without having a call flush_cpu() between them. In addition,
     * the pixels taken from ColorBuffer taken are those when
     * flush_gpu() is issued.
     * \param lod mipmap level
     * \param location location within in atlas
     * \param size size of rect to affect
     * \param src ColorBuffer from which to copy pixels
     * \param src_location location in ColorBuffer from which to copy
     * \param post_process_window the region within src the blit is
     *                            allowed to read from when performing
     *                            post processing for fill masks.
     * \param blit_processing specifies if and how the pixels are
     *                        processed in the blit.
     * \param permute_src_x_y_coordinates if true, instead of taking the
     *                                    pixels from src from the rect
     *                                    whose min-corner is src_location
     *                                    whose size is size, take the pixels
     *                                    from the rect whose min-corner
     *                                    (src_location.y, src_location.x)
     *                                    and whose size is size.y, size.x)
     */
    virtual
    void
    copy_pixels(unsigned int lod, uvec3 location, uvec2 size,
                ColorBuffer &src, uvec2 src_location,
                const RectT<int> &post_process_window,
                enum image_blit_processing_t blit_processing,
                bool permute_src_x_y_coordinates) = 0;

    /*!
     * Downsample pixels from a single astral::ColorBuffer to the color
     * backing. The pixel foot print from the src color buffer is twice
     * in both dimensions that its foot print in the destinaction. It is
     * an error for downsample_pixels() to be called after set_pixels()
     * without having a call to ImageAtlasColorBacking::flush_cpu() between
     * them.  In addition, the pixels taken from astral::ColorBuffer taken
     * are those when flush_gpu() is issued.
     *
     * \param lod mipmap level
     * \param location location of min-min corner of pixels to which to copy
     * \param size size of region of pixels to set
     * \param src astral::ColorBuffer from which to take pixels
     * \param src_location min-min corner of pixels from which to downsample
     * \param downsamping_processing specifies the downsampling processing to perform
     * \param permute_src_x_y_coordinates if true, instead of taking the
     *                                    pixels from src from the rect
     *                                    whose min-corner is src_location
     *                                    whose size is size, take the pixels
     *                                    from the rect whose min-corner
     *                                    (src_location.y, src_location.x)
     *                                    and whose size is size.y, size.x)
     */
    virtual
    void
    downsample_pixels(unsigned int lod, uvec3 location, uvec2 size,
                      ColorBuffer &src, uvec2 src_location,
                      enum downsampling_processing_t downsamping_processing,
                      bool permute_src_x_y_coordinates) = 0;
  };

  /*!
   * Backing store for the index texels of a \ref ImageAtlas
   */
  class ImageAtlasIndexBacking:public ImageAtlasBacking
  {
  public:
    /*!
     * Ctor.
     * \param width_height the width and height of each layer. The value must be
     *                     of the form V = ImageAtlas::tile_size * N
     * \param number_layers the number of layers.
     * \param max_number_layers the maximum number of layers the atlas can
     *                          be resized to.
     */
    ImageAtlasIndexBacking(unsigned int width_height, unsigned int number_layers,
                           unsigned int max_number_layers):
      ImageAtlasBacking(width_height, number_layers, max_number_layers)
    {}

    /*!
     * To be implemented by a derived class to flush changes
     * accumulated by upload_texels().
     */
    virtual
    void
    flush(void) = 0;

    /*!
     * To be implemented by a derived class to upload texels that
     * are used for color tiles.
     * \param location location within in atlas
     * \param size size of rect to affect
     * \param texels values to upload
     */
    virtual
    void
    upload_texels(uvec3 location, uvec2 size, c_array<const uvec3> texels) = 0;
  };

/*! @} */
}


#endif

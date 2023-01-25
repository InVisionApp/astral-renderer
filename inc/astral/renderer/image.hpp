/*!
 * \file image.hpp
 * \brief file image.hpp
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

#ifndef ASTRAL_IMAGE_HPP
#define ASTRAL_IMAGE_HPP

#include <type_traits>
#include <astral/util/enum_flags.hpp>
#include <astral/util/object_pool.hpp>
#include <astral/util/rect.hpp>
#include <astral/renderer/backend/image_backing.hpp>
#include <astral/renderer/mipmap_level.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * A \ref ImageMipElement represents up to two mipmaps
   * of an astral::Image. An astral::ImageMipElemenet can be
   * created by one of the overloads of ImageAtlas::create_mip_element().
   */
  class ImageMipElement:public reference_counted<ImageMipElement>::non_concurrent_custom_delete
  {
  public:
    /*!
     * Enumerationt tol hold the value ImageMipElement::maximum_number_of_mipmaps
     */
    enum maximum_number_of_mipmaps_t:uint32_t
      {
        /*!
         * The maximum number of mipmaps a single
         * astral::ImageMipElement may posess
         */
        maximum_number_of_mipmaps = 2,
      };

    /*!
     * Enumeration to describe the kinds of elements
     * that a \ref Image is broken into.
     */
    enum element_type_t:uint32_t
      {
        /*!
         * An element represents a region that all
         * texels are (0, 0, 0, 0).
         */
        empty_element,

        /*!
         * An element represents a region that all
         * texels are (255, 255, 255, 255).
         */
        white_element,

        /*!
         * An element represents a region that texels
         * may vary per pixel
         */
        color_element,

        number_element_type
      };

    /*!
     * Returns the size of the object
     */
    uvec2
    size(void) const;

    /*!
     * Returns the number of padding pixels each tile of
     * the object has for a given LOD. A tile covers a
     * region R and has this amount of padding past that
     * region R in each dimension with the exception that
     * the tiles in the last row do not have that padding
     * at the end in y-direction and the the tiles in the
     * last column do not have padding at the end in the
     * x-direction. The negative of this value is the
     * minumum value that can be passed for the location
     * parameter in set_pixels() and copy_pixels(). The
     * purpose of the padding is to allow for bilinear and
     * bi-cubic filtering to be possible and remain in the
     * same tile to get the neighboring texels.
     */
    int
    tile_padding(int lod) const;

    /*!
     * Returns the number of mipmaps the object supports.
     */
    unsigned int
    number_mipmap_levels(void) const;

    /*!
     * Set the number of mipmaps the object supports.
     */
    void
    number_mipmap_levels(unsigned int v);

    /*!
     * Number of index lookups to go from the root tile to a color tile
     */
    unsigned int
    number_index_levels(void) const;

    /*!
     * The number of color tile texels per root index tile texel; this
     * value is purely a function of number_index_levels(). Note that
     * this gives the same value as compute_ratio(number_index_levels()).
     */
    unsigned int
    ratio(void) const;

    /*!
     * The location within the ImageAtlasBacking of the
     * root index tile; if number_index_levels() is 0, then
     * this is the location in ImageAtlas::index_backing()
     * of a texel that stores the location of the single
     * color tile that backs this ImageMipElement.
     */
    uvec3
    root_tile_location(void) const;

    /*!
     * Set pixel color data of portion of the image.
     *
     * NOTE: the pixels backed by \ref white_element and \ref
     *       empty_element tiles are NOT changed.
     *
     * \param lod mipmap level
     * \param location location of min-min corner of pixels to set;
     *                 each coordinate of location must be atleast
     *                 -1 * tile_padding(lod). A negative value indicates
     *                 to set the texel values in the padding before
     *                 the image.
     * \param size size of region of pixels to set
     * \param row_width the number of texels between the start
     *                  of successive rows in the paramater
     *                  texels;
     * \param pixels pixel values to upload; these are the raw pixel
     *               values to upload to the GPU. The interpretation of
     *               those pixel values is determined by how the image
     *               data is sampled, i.e. astral::ImageSampler. However,
     *               it is critical to note that when the pixels are used
     *               as color values, then the color values are *with*
     *               alpha premulyiplied; thus when uploading color values,
     *               the pixels must be with alpha premultiplied.
     */
    void
    set_pixels(int lod, ivec2 location, ivec2 size, unsigned int row_width,
               c_array<const u8vec4> pixels);

    /*!
     * Copy pixels from a single astral::ColorBuffer to the color backing.
     * The pixels taken from astral::ColorBuffer taken are those when
     * ImageAtlas::flush() is issued.
     *
     * NOTE: the pixels backed by \ref white_element and \ref
     *       empty_element tiles are NOT changed.
     *
     * \param lod mipmap level
     * \param location location of min-min corner of pixels to which to copy
     * \param size size of region of pixels to set
     * \param src astral::ColorBuffer from which to take pixels
     * \param src_location min-min corner of pixels from which to copy
     * \param blit_processing specifies if and how the pixels are
     *                        processed in the blit.
     * \param permute_src_x_y_coordinates if true, instead of taking the
     *                                    pixels from src from the rect
     *                                    whose min-corner is src_location
     *                                    whose size is size, take the pixels
     *                                    from the rect whose min-corner
     *                                    (src_location.y, src_location.x)
     *                                    and whose size is size.y, size.x)
     * \returns the number of pixels affected on the image atlas
     */
    unsigned int
    copy_pixels(int lod, ivec2 location, ivec2 size,
                ColorBuffer &src, ivec2 src_location,
                enum image_blit_processing_t blit_processing,
                bool permute_src_x_y_coordinates);

    /*!
     * Downsample pixels from a single astral::ColorBuffer to the color
     * backing. The pixel foot print from the src color buffer is twice
     * in both dimensions that its foot print in the destinaction. The
     * pixels taken from astral::ColorBuffer taken are those when
     * ImageAtlas::flush() is issued.
     *
     * NOTE: the pixels backed by ImageMipElement::white_element
     *       and ImageMipElement::empty_element tiles are NOT changed.
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
     * \returns the number of pixels affected on the image atlas
     */
    unsigned int
    downsample_pixels(int lod, ivec2 location, ivec2 size,
                      ColorBuffer &src, ivec2 src_location,
                      enum downsampling_processing_t downsamping_processing,
                      bool permute_src_x_y_coordinates);

    /*!
     * Returns the number of regions of the Image that
     * are of a specified \ref element_type_t
     * \param tp which element type
     */
    unsigned int
    number_elements(enum element_type_t tp) const;

    /*!
     * Equivalent to
     * \code
     * number_elements(empty_element) > 0u || number_elements(white_element) > 0u
     * \endcode
     */
    bool
    has_white_or_empty_elements(void) const
    {
      return number_elements(empty_element) > 0u || number_elements(white_element) > 0u;
    }

    /*!
     * Returns the tile ID for an element
     * \param tp which element type
     * \param I which region with 0 <= I < number_elements(tp)
     */
    uvec2
    element_tile_id(enum element_type_t tp, unsigned int I) const;

    /*!
     * Return the location of an element's region, does NOT
     * include the padding. Equivalent to
     * \code
     * tile_location(element_tile_id(tp, I))
     * \endcode
     * \param tp which element type
     * \param I which region with 0 <= I < number_elements(tp)
     */
    uvec2
    element_location(enum element_type_t tp, unsigned int I) const;

    /*!
     * Return the size of an element's region, does NOT
     * include the padding. Equivalent to
     * \code
     * tile_size(element_tile_id(tp, I))
     * \endcode
     * \param tp which element type
     * \param I which region with 0 <= I < number_elements(tp)
     */
    uvec2
    element_size(enum element_type_t tp, unsigned int I) const;

    /*!
     * Returns the boundary flags for a named tile.
     * \code
     * tile_boundary(element_tile_id(tp, I))
     * \endcode
     * \param tp which element type
     * \param I which region with 0 <= I < number_elements(tp)
     */
    EnumFlags<enum RectEnums::side_t, 4>
    element_boundary(enum element_type_t tp, unsigned int I) const
    {
      return tile_boundary(element_tile_id(tp, I));
    }

    /*!
     * Returns true if a \ref color_element tile is shared
     * with another \ref ImageMipElement.
     * \param I which \ref color_element with 0 <= I < number_elements(\ref color_element)
     */
    bool
    color_tile_is_shared(unsigned int I) const;

    /*!
     * Returns the number of tiles in each dimension
     */
    uvec2
    tile_count(void) const;

    /*!
     * Provided as a conveniance; returns a
     * 2-vector of range type values where
     * the x-component range is given by
     * [0, tile_count().x()) and the y-component
     * by [0, tile_count().y())
     */
    vecN<range_type<unsigned int>, 2>
    tile_range(void) const
    {
      vecN<range_type<unsigned int>, 2> R;
      uvec2 end(tile_count());

      R.x().m_begin = 0;
      R.x().m_end = end.x();

      R.y().m_begin = 0;
      R.y().m_end = end.y();

      return R;
    }

    /*!
     * Given a tile ID, returns the location of the min-min
     * corner of the tile NOT including its padding.
     * \param tile_xy tile ID with tile_xy.x() < tile_count().x()
     *                and tile_xy.y() < tile_count().y()
     */
    uvec2
    tile_location(uvec2 tile_xy) const;

    /*!
     * Given a tile ID, returns the size of the tile
     * \param tile_xy tile ID with tile_xy.x() < tile_count().x()
     *                and tile_xy.y() < tile_count().y()
     * \param include_padding if true include the padding in the tile size
     */
    uvec2
    tile_size(uvec2 tile_xy, bool include_padding = false) const;

    /*!
     * Returns true if the entire ImageMipElement lies in a single
     * UNPADDED tile.
     */
    bool
    on_single_unpadded_tile(void) const;

    /*!
     * Given a tile ID, returns the \ref element_type_t
     * of the named tile.
     * \param tile_xy tile ID with tile_xy.x() < tile_count().x()
     *                and tile_xy.y() < tile_count().y()
     */
    enum element_type_t
    tile_type(uvec2 tile_xy) const;

    /*!
     * Given a tile ID, returns what sides of the tile
     * share a boundary with the boundary of this
     * astral::ImageMipElement
     * \param tile_xy tile ID with tile_xy.x() < tile_count().x()
     *                and tile_xy.y() < tile_count().y()
     */
    EnumFlags<enum RectEnums::side_t, 4>
    tile_boundary(uvec2 tile_xy) const;

    /*!
     * Given a tile ID, returns true if the named tile is
     * shared with another \ref ImageMipElement.
     * \param tile_xy tile ID with tile_xy.x() < tile_count().x()
     *                and tile_xy.y() < tile_count().y()
     */
    bool
    tile_is_shared(uvec2 tile_xy) const;

    /*!
     * Given a tile ID, returns the location in the INDEX atlas of the
     * texel that stores the min-min corner of the tile.
     * \param tile_xy tile ID with tile_xy.x() < tile_count().x()
     *                and tile_xy.y() < tile_count().y()
     */
    uvec3
    tile_index_atlas_location(uvec2 tile_xy) const;

    /*!
     * Create an astral::ImageMipElement that corresponds to a
     * sub-range of tiles of this astral::ImageMipElement where
     * in addition some tiles can also be overridden to be empty
     * or color tiles.
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
    create_sub_mip(vecN<range_type<unsigned int>, 2> tile_range,
                   c_array<const uvec2> empty_tiles,
                   c_array<const uvec2> full_tiles,
                   c_array<const uvec2> shared_tiles) const;

    /*!
     * Overload of conveniance, equivalent to
     * \code
     * create_sub_mip(tile_range(), empty_tiles, full_tiles, shared_tiles)
     * \endcode
     */
    reference_counted_ptr<const ImageMipElement>
    create_sub_mip(c_array<const uvec2> empty_tiles,
                   c_array<const uvec2> full_tiles,
                   c_array<const uvec2> shared_tiles)
    {
      return create_sub_mip(tile_range(), empty_tiles, full_tiles, shared_tiles);
    }

    /*!
     * Returns true if the astral::ImageAtlas failed to allocate
     * a color or index tile; this happens when too much color data is
     * allocated. In this case, all of the tiles that are specified
     * to not be shared are backed by the empty tile (i.e. clear
     * black).
     */
    bool
    tile_allocation_failed(void) const;

    ///@cond
    static
    void
    delete_object(ImageMipElement *p);
    ///@endcond

    /*!
     * Given the number of index levels, compute
     * the number of color texels per root index
     * tile texel.
     * \param num_index_levels number of index levels
     */
    static
    unsigned int
    compute_ratio(unsigned int num_index_levels);

  private:
    class Implement;
    typedef ObjectPoolDirect<Implement, 512> Pool;

    friend class ImageAtlas;
    friend ObjectPoolDirect<Implement, 512>;

    ImageMipElement(void);
  };

  /*!
   * Returns a string corresponing to the enum-value.
   */
  c_string
  label(enum ImageMipElement::element_type_t);

  /*!
   * \brief
   * A \ref Image is an image broken into tiles; it is the class
   * in Astral to represent image data. All color image data is
   * *with* alpha pre-multiplied. In addiiton, when astral::Renderer
   * draws color data to an astral::Image, the final write to the
   * astal::Image will be with alpha pre-multiplied. An astral::Image
   * can be created by one of the overloads of ImageAtlas::create_image().
   */
  class Image:public reference_counted<Image>::non_concurrent_custom_delete
  {
  public:
    /*!
     * Return the default sRGB encoding for the image
     * when the image is part of an astral::ImageSampler
     * as a color source.
     */
    enum colorspace_t
    colorspace(void) const;

    /*!
     * Set the value returned by colorspace(void) const
     */
    void
    colorspace(enum colorspace_t v);

    /*!
     * Returns true if and only if the astral::Image
     * is regarded as opaque, i.e. all values for the
     * alpha channel are 1.0.
     */
    bool
    opaque(void) const;

    /*!
     * Declare that all image data is known to be
     * opaque; for example via image update.
     */
    void
    override_to_opaque(void);

    /*!
     * Returns the size of the Image
     */
    uvec2
    size(void) const
    {
      ASTRALassert(!mip_chain().empty());
      return mip_chain().front()->size();
    }

    /*!
     * Gives the tile padding for the tiles at a given
     * LOD, see ImageMipElement::tile_padding()
     * for details.
     */
    int
    tile_padding(int lod) const;

    /*!
     * Set pixel color data of portion of the image. NOTE:
     * it is illegal to call set_pixels() if in_use()
     * returns true.
     *
     * NOTE: the pixels backed by ImageMipElement::white_element
     *       and ImageMipElement::empty_element tiles are NOT changed.
     *
     * \param lod mipmap level
     * \param location location of min-min corner of pixels to set;
     *                 each coordinate of location must be atleast
     *                 -1 * tile_padding(lod). A negative value indicates
     *                 to set the texel values in the padding before
     *                 the image.
     * \param size size of region of pixels to set
     * \param row_width the number of texels between the start
     *                  of successive rows in the paramater
     *                  texels;
     * \param pixels pixel values to upload; these are the raw pixel
     *               values to upload to the GPU. The interpretation of
     *               those pixel values is determined by how the image
     *               data is sampled, i.e. astral::ImageSampler. However,
     *               it is critical to note that when the pixels are used
     *               as color values, then the color values are *with*
     *               alpha premultiplied; thus when uploading color values,
     *               the pixels must be with alpha premultiplied.
     */
    void
    set_pixels(int lod, ivec2 location, ivec2 size, unsigned int row_width,
               c_array<const u8vec4> pixels);

    /*!
     * Copy pixels from a single astral::ColorBuffer to the color backing.
     * The pixels taken from astral::ColorBuffer taken are those when
     * ImageAtlas::flush() is issued.
     *
     * NOTE: the pixels backed by ImageMipElement::white_element
     *       and ImageMipElement::empty_element tiles are NOT changed.
     *
     * \param lod mipmap level
     * \param location location of min-min corner of pixels to which to copy
     * \param size size of region of pixels to set
     * \param src astral::ColorBuffer from which to take pixels
     * \param src_location min-min corner of pixels from which to copy
     * \param blit_processing specifies if and how the pixels are
     *                        processed in the blit.
     * \param permute_src_x_y_coordinates if true, instead of taking the
     *                                    pixels from src from the rect
     *                                    whose min-corner is src_location
     *                                    whose size is size, take the pixels
     *                                    from the rect whose min-corner
     *                                    (src_location.y, src_location.x)
     *                                    and whose size is size.y, size.x)
     * \returns the number of pixels affected on the image atlas
     */
    unsigned int
    copy_pixels(int lod, ivec2 location, ivec2 size,
                ColorBuffer &src, ivec2 src_location,
                enum image_blit_processing_t blit_processing,
                bool permute_src_x_y_coordinates);

    /*!
     * Downsample pixels from a single astral::ColorBuffer to the color
     * backing. The pixel foot print from the src color buffer is twice
     * in both dimensions that its foot print in the destinaction. The
     * pixels taken from astral::ColorBuffer taken are those when
     * ImageAtlas::flush() is issued.
     *
     * NOTE: the pixels backed by ImageMipElement::white_element
     *       and ImageMipElement::empty_element tiles are NOT changed.
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
     * \returns the number of pixels affected on the image atlas
     */
    unsigned int
    downsample_pixels(int lod, ivec2 location, ivec2 size,
                      ColorBuffer &src, ivec2 src_location,
                      enum downsampling_processing_t downsamping_processing,
                      bool permute_src_x_y_coordinates);

    /*!
     * Returns the number of mipmaps the Image supports.
     */
    unsigned int
    number_mipmap_levels(void) const
    {
      c_array<const reference_counted_ptr<const ImageMipElement>> C(mip_chain());
      ASTRALassert(!C.empty());
      return ImageMipElement::maximum_number_of_mipmaps * (C.size() - 1u) + C.back()->number_mipmap_levels();
    }

    /*!
     * Returns the mip-chain of the image; these are the real
     * backings for the image. The interface is designed so
     * that multiple images can share ImageMipElement
     * objects.
     */
    c_array<const reference_counted_ptr<const ImageMipElement>>
    mip_chain(void) const;

    /*!
     * Given an astral::MipmapLevel value, return the astral::ImageMipElement
     * and which level of it to use to access the named mipmap level.
     */
    std::pair<const ImageMipElement*, unsigned int>
    mip(MipmapLevel L) const
    {
      c_array<const reference_counted_ptr<const ImageMipElement>> C(mip_chain());
      std::pair<const ImageMipElement*, unsigned int> return_value;
      unsigned int m;

      ASTRALstatic_assert(ImageMipElement::maximum_number_of_mipmaps == 2u);
      m = L.m_value >> 1u;
      return_value.second = L.m_value & 1u;
      return_value.first = (m < C.size()) ? C[m].get() : nullptr;

      return return_value;
    }

    /*!
     * Mark this astral::Image as in use until resources are
     * unlocked by ImageAtlas::unlock_resources(). When an
     * image is marked as in use, it is illegal to change its
     * content.
     */
    void
    mark_in_use(void);

    /*!
     * Returns true if the astral::Image is marked as in use,
     * see also mark_in_use(). An astral::Image is marked as
     * in use by astral::RenderEncoderBase (and its related
     * classes) if the astral::Image is used as a source
     * for color or mask value of it the astral::Image is
     * rendered to. The astral::Image will cease to be marked
     * as in use as soon as the Renderer::end() is issued
     * to send the draw commands to the GPU backend.
     */
    bool
    in_use(void) const;

    /*!
     * Returns the unique ID for this astral::Image. ID values
     * are unique for the lifetime of the astral::Image, i.e.
     * no two alive different astral::Image objects will have
     * the same ID, but a created ID can have the ID of a previously
     * deleted astral::Image.
     */
    ImageID
    ID(void) const;

    /*!
     * Returns the default value if to use the pre-padding of the
     * image data. The ctor of astral::ImageSampler will
     * initialize its bits to respect this value. Default value
     * is false.
     */
    bool
    default_use_prepadding(void) const;

    /*!
     * Set the value returned by default_use_prepadding(void) const
     */
    void
    default_use_prepadding(bool v);

    /*!
     * Returns true if any astral::ImageMipElement of this
     * astral::Image has ImageMipElement::tile_allocation_failed()
     * as true.
     */
    bool
    tile_allocation_failed(void) const;

    /*!
     * Given an astral::MipmapLevel, return index into mip_chain()
     * to use to access the astral::ImageMipElement that holds the
     * named mipmap level.
     */
    static
    unsigned int
    mip_element_index(MipmapLevel L)
    {
      ASTRALstatic_assert(ImageMipElement::maximum_number_of_mipmaps == 2u);
      return L.m_value >> 1u;
    }

    ///@cond

    /* When Renderer constucts an Image to which it renders,
     * it uses one of the create_rendered_image() of ImageAtlas
     * so that it also specifies the offscreen_render_index().
     * That value is InvalidRenderValue for usual images. For
     * rendere images it is set to InvalidRenderValue by
     * mark_as_usual_image() which Renderer calls just before
     * the first blits from a scratch RenderTarget to the tiles
     * of the Image are performed.
     */
    void
    mark_as_usual_image(detail::RenderedImageTag v);

    uint32_t
    offscreen_render_index(void) const;

    static
    void
    delete_object(Image *p);
    ///@endcond

  private:
    class Implement;
    friend ObjectPoolDirect<Implement, 512>;
    friend class ImageAtlas;

    typedef ObjectPoolDirect<Implement, 512> Pool;

    Image(void);
  };

  /*!
   * \brief
   * Class to define a sub-image of an astral::Image
   * \param T scalar type to specify the corners of the sub-image
   */
  template<typename T>
  class SubImageT
  {
  public:
    /*!
     * Ctor that sets the astral::SubImage to use an entire astral::Image
     * \param im astral::Image to use
     * \param override_opaque if true, mark the content of the SubImage as
     *                        opaque even if Image::opaque() returns false.
     */
    SubImageT(const Image &im, bool override_opaque = false):
      m_image(im),
      m_mip_range(0, im.mip_chain().size()),
      m_min_corner(0, 0),
      m_size(im.size()),
      m_opaque(override_opaque || im.opaque())
    {}

    /*!
     * Ctor that sets the astral::SubImage to use the entire region of
     * a specified mip-chain of an astral::Image
     * \param im astral::Image to use
     * \param mip_range range into astral::Image::mip_chain() of the astral::Image to use
     * \param override_opaque if true, mark the content of the SubImage as
     *                        opaque even if Image::opaque() returns false.
     */
    SubImageT(const Image &im,
              range_type<unsigned int> mip_range,
              bool override_opaque = false):
      m_image(im),
      m_mip_range(mip_range),
      m_min_corner(0, 0),
      m_size(im.mip_chain()[mip_range.m_begin]->size()),
      m_opaque(override_opaque || im.opaque())
    {}

    /*!
     * Ctor that sets the astral::SubImage to use a portion of an astral::Image
     * \param im astral::Image to use
     * \param min_corner min-corner of im to use
     * \param size size from im to use
     * \param override_opaque if true, mark the content of the SubImage as
     *                        opaque even if Image::opaque() returns false.
     */
    SubImageT(const Image &im,
              vecN<T, 2> min_corner, vecN<T, 2> size,
              bool override_opaque = false):
      m_image(im),
      m_mip_range(0, im.mip_chain().size()),
      m_min_corner(min_corner),
      m_size(size),
      m_opaque(override_opaque || im.opaque())
    {
      bound_values_to_image();
    }

    /*!
     * Ctor that sets the astral::SubImage to use the entire region of
     * a specified mip-chain of an astral::Image
     * \param im astral::Image to use
     * \param mip_range range of mip elements of the astral::Image to use
     * \param min_corner min-corner of im to use in LOD = mip_range.m_begin coordinates
     * \param size size from im to use in LOD = mip_range.m_begin coordinates
     * \param override_opaque if true, mark the content of the SubImage as
     *                        opaque even if Image::opaque() returns false.
     */
    SubImageT(const Image &im,
              range_type<unsigned int> mip_range,
              vecN<T, 2> min_corner, vecN<T, 2> size,
              bool override_opaque = false):
      m_image(im),
      m_mip_range(mip_range),
      m_min_corner(min_corner),
      m_size(size),
      m_opaque(override_opaque || im.opaque())
    {
      bound_values_to_image();
    }

    /*!
     * Set \ref m_min_corner
     * \param m value to use
     */
    SubImageT&
    min_corner(vecN<T, 2> m)
    {
      m_min_corner = m;
      return *this;
    }

    /*!
     * Set \ref m_size
     * \param s value to use
     */
    SubImageT&
    size(vecN<T, 2> s)
    {
      m_size = s;
      return *this;
    }

    /*!
     * Return a astral::SubImageT value whose \ref m_mip_range
     * is the tail of this astral::SubImageT \ref m_mip_range value.
     * In addition, the return value will have \ref m_min_corner
     * and \ref m_size adjusted.
     * \param v offset from m_mip_range.m_begin, i.e. a value of 1
     *          means drop the first ImageMipElement value that this
     *          astral::SubImageT referes to.
     */
    SubImageT
    mip_tail(unsigned int v) const
    {
      SubImageT return_value(mip_tail_implement(v));
      return_value.m_opaque = m_opaque;

      return return_value;
    }

    /*!
     * Create a sub-image of this astral::SubImageT
     * \param rel_min_corner min-min corner relative to \ref m_min_corner
     * \param size size of the new sub-image
     */
    SubImageT
    sub_image(const vecN<T, 2> &rel_min_corner, const vecN<T, 2> &size) const
    {
      SubImageT return_value(*this);

      return_value.m_min_corner += rel_min_corner;
      return_value.m_size = size;

      return return_value;
    }

    /*!
     * Returns the mip-chain that this astral::SubImage refers to,
     * i.e. the sub-array of Image::mip_chain() specified by \ref
     * m_mip_range
     */
    c_array<const reference_counted_ptr<const ImageMipElement>>
    mip_chain(void) const
    {
      return m_image.mip_chain().sub_array(m_mip_range);
    }

    /*!
     * Realize value as a different scalar type.
     * Beware that going from float to uint will
     * truncate the values \ref m_min_corner and
     * \ref m_size.
     */
    template<typename S>
    SubImageT<S>
    type_cast(void) const
    {
      SubImageT<S> return_value(m_image,
                                vecN<S, 2>(m_min_corner),
                                vecN<S, 2>(m_size));

      return_value.m_opaque = m_opaque;

      return return_value;
    }

    /*!
     * Realize value as a different scalar type, potentially
     * with the return value a larger area if the conversion
     * from T to S truncates values.
     */
    template<typename S>
    SubImageT<S>
    type_cast_enlarge(void) const
    {
      vecN<T, 2> t_max_point(m_min_corner + m_size);
      vecN<S, 2> s_max_point(t_max_point);
      vecN<S, 2> s_min_point(m_min_corner);

      if (T(s_max_point.x()) < t_max_point.x())
        {
          ++s_max_point.x();
        }

      if (T(s_max_point.y()) < t_max_point.y())
        {
          ++s_max_point.y();
        }

      SubImageT<S> return_value(m_image,
                                s_min_point,
                                s_max_point - s_min_point);

      return_value.m_opaque = m_opaque;

      return return_value;
    }

    /*!
     * Bound the values of \ref m_min_corner, \ref m_size
     * and \ref m_mip_range so that they are legal values
     * for \ref m_image.
     */
    void
    bound_values_to_image(void)
    {
      unsigned int M(m_image.mip_chain().size());

      m_mip_range.m_begin = t_min(m_mip_range.m_begin, M - 1u);
      m_mip_range.m_end = t_min(m_mip_range.m_end, M);

      vecN<T, 2> sz(m_image.mip_chain()[m_mip_range.m_begin]->size());
      vecN<T, 2> max_pt(m_min_corner + m_size);

      m_min_corner.x() = t_max(m_min_corner.x(), T(0));
      m_min_corner.y() = t_max(m_min_corner.y(), T(0));

      max_pt.x() = t_min(max_pt.x(), sz.x());
      max_pt.y() = t_min(max_pt.y(), sz.y());

      m_size = max_pt - m_min_corner;
    }

    /*!
     * Returns the number of mipmap levels this SubImage
     * effectively has assuming that \ref m_mip_range is
     * valid for \ref m_image
     */
    unsigned int
    number_mipmap_levels(void) const
    {
      unsigned int return_value;

      ASTRALassert(m_mip_range.m_begin < m_mip_range.m_end);
      ASTRALassert(m_mip_range.m_end <= m_image.mip_chain().size());

      return_value = (m_mip_range.m_end - m_mip_range.m_begin - 1u) * ImageMipElement::maximum_number_of_mipmaps;
      return_value += m_image.mip_chain()[m_mip_range.m_end - 1u]->number_mipmap_levels();

      return return_value;
    }

    /*!
     * The underlying image
     */
    const Image &m_image;

    /*!
     * The range of mipmaps into Image::mip_chain() to use
     */
    range_type<unsigned int> m_mip_range;

    /*!
     * The min-corner relative to m_image.sub_array(\ref m_mip_range).front()
     * to use
     */
    vecN<T, 2> m_min_corner;

    /*!
     * The size of this sub-image relative to m_image.sub_array(\ref m_mip_range).front().
     */
    vecN<T, 2> m_size;

    /*!
     * Regard the pixels referenced by this
     * astral::SubImage as fully opaque.
     */
    bool m_opaque;

  private:
    template<typename U = T,
             enable_if_t<std::is_integral<U>::value, bool> = true>
    SubImageT
    mip_tail_implement(unsigned int v) const
    {
      vecN<T, 2> min_corner, max_corner;
      unsigned int vv;

      vv = v * ImageMipElement::maximum_number_of_mipmaps;

      min_corner.x() = m_min_corner.x() >> vv;
      min_corner.y() = m_min_corner.y() >> vv;

      max_corner.x() = (m_min_corner.x() + m_size.x()) >> vv;
      max_corner.y() = (m_min_corner.y() + m_size.y()) >> vv;

      return SubImageT(m_image,
                       range_type<unsigned int>(v + m_mip_range.m_begin, m_mip_range.m_end),
                       min_corner, max_corner - min_corner);
    }

    template<typename U = T,
             enable_if_t<std::is_floating_point<U>::value, bool> = true>
    SubImageT
    mip_tail_implement(unsigned int v) const
    {
      vecN<T, 2> min_corner, max_corner;
      int vv;

      /* note that vv is an integer, this way negating it is safe */
      vv = v * ImageMipElement::maximum_number_of_mipmaps;

      min_corner.x() = t_ldexp(m_min_corner.x(), -vv);
      min_corner.y() = t_ldexp(m_min_corner.y(), -vv);

      max_corner.x() = t_ldexp(m_min_corner.x() + m_size.x(), -vv);
      max_corner.y() = t_ldexp(m_min_corner.y() + m_size.y(), -vv);

      return SubImageT(m_image,
                       range_type<unsigned int>(v + m_mip_range.m_begin, m_mip_range.m_end),
                       min_corner, max_corner - min_corner);
    }
  };

  /*!
   * Classical sub-images are given with integer
   * coordinates.
   */
  typedef SubImageT<uint32_t> SubImage;

  /*!
   * Class that holds a single astral::ImageMipElement
   * for easier packing later; using this indicates to only
   * access mip level 0 of the object.
   */
  class PackedImageMipElement
  {
  public:
    enum
      {
        /*!
         * Number of bits that \ref ImageSamplerBits value occupies
         * in \ref m_packed_data
         */
        image_bits_num_bits = ImageSamplerBits::number_bits,

        /*!
         * Number of bits that ImageMipElement::root_tile_location().z()
         * value occupies in \ref m_packed_data
         */
        root_tile_z_num_bits = 8,

        /*!
         * Number of bits that ImageMipElement::number_index_levels()
         * value occupies in \ref m_packed_data
         */
        number_index_levels_num_bits = 2,

        /*!
         * First bit that \ref ImageSamplerBits value occupies
         * in \ref m_packed_data
         */
        image_bits_bit0 = 0,

        /*!
         * First bit that ImageMipElement::root_tile_location().z()
         * value occupies in \ref m_packed_data
         */
        root_tile_z_bit0 = image_bits_bit0 + image_bits_num_bits,

        /*!
         * First bit that ImageMipElement::number_index_levels()
         * value occupies in \ref m_packed_data
         */
        number_index_levels_bit0 = root_tile_z_bit0 + root_tile_z_num_bits,
      };
    ///@cond
    ASTRALstatic_assert(uint32_t(number_index_levels_num_bits + number_index_levels_bit0) <= 32u);
    ///@endcond

    /*!
     * Ctor
     * \param im astral::ImageMipElement from which to take values
     * \param bits value as encoded by astral::ImageSamplerBits that conrol
     *             how to sample the image data
     */
    PackedImageMipElement(const ImageMipElement &im, uint32_t bits)
    {
      uvec3 loc;

      loc = im.root_tile_location();
      m_root_min_corner = pack_pair(loc.x(), loc.y());
      m_subimage_min_corner = pack_pair(0u, 0u);
      m_subimage_size = pack_pair(im.size());
      m_packed_data = pack_bits(image_bits_bit0, image_bits_num_bits, bits)
        | pack_bits(root_tile_z_bit0, root_tile_z_num_bits, loc.z())
        | pack_bits(number_index_levels_bit0, number_index_levels_num_bits, im.number_index_levels());

      ASTRALstatic_assert(image_bits_num_bits + root_tile_z_num_bits + number_index_levels_num_bits <= 32u);
    }

    /*!
     * Ctor
     * \param padding amount of padding to remove from all sides.
     * \param im astral::ImageMipElement from which to take values
     * \param bits value as encoded by astral::ImageSamplerBits that conrol
     *             how to sample the image data
     */
    PackedImageMipElement(unsigned int padding, const ImageMipElement &im, uint32_t bits):
      PackedImageMipElement(im, bits)
    {
      uvec2 sz(unpack_pair(m_subimage_size)), corner;

      sz.x() -= t_min(2u * padding, sz.x());
      sz.y() -= t_min(2u * padding, sz.y());
      corner.x() = t_min(padding, sz.x());
      corner.y() = t_min(padding, sz.y());

      m_subimage_min_corner = pack_pair(corner);
      m_subimage_size = pack_pair(sz);

      ASTRALstatic_assert(image_bits_num_bits + root_tile_z_num_bits + number_index_levels_num_bits <= 32u);
    }

    /*!
     * Ctor
     * \param im astral::ImageMipElement from which to take values
     * \param filter astral::filter_t applied to image
     * \param encoding color space of image data
     */
    PackedImageMipElement(const ImageMipElement &im, enum filter_t filter,
                          enum colorspace_t encoding):
      PackedImageMipElement(im, ImageSamplerBits::value(filter, mipmap_none, 0, encoding))
    {}

    /*!
     * Ctor
     * \param padding amount of padding to remove from all sides.
     * \param im astral::ImageMipElement from which to take values
     * \param filter astral::filter_t applied to image
     * \param encoding color space of image data
     */
    PackedImageMipElement(unsigned int padding, const ImageMipElement &im,
                          enum filter_t filter, enum colorspace_t encoding):
      PackedImageMipElement(padding, im, ImageSamplerBits::value(filter, mipmap_none, 0, encoding))
    {}

    /*!
     * Empty ctor
     */
    PackedImageMipElement(void):
      m_root_min_corner(0),
      m_subimage_min_corner(0),
      m_subimage_size(0),
      m_packed_data(0)
    {}

    /*!
     * Pack this PackedImageMipElement into a single
     * astral::gvec4 value
     */
    void
    pack_item_data(gvec4 *dst) const
    {
      dst->x().u = m_root_min_corner;
      dst->y().u = m_subimage_min_corner;
      dst->z().u = m_subimage_size;
      dst->w().u = m_packed_data;
    }

    /*!
     * Returns true if the PackedImageMipElement refers
     * to non-empty image data.
     */
    bool
    non_empty(void) const
    {
      return m_subimage_size != 0u;
    }

    /*!
     * Change the sub-image to take from the originating
     * \ref ImageMipElement object
     */
    PackedImageMipElement&
    sub_image(uvec2 min_corner, uvec2 size)
    {
      m_subimage_min_corner = pack_pair(min_corner);
      m_subimage_size = pack_pair(size);
      return *this;
    }

    /*!
     * Change the image bits that control how to sample
     * from the \ref ImageMipElement object
     */
    PackedImageMipElement&
    image_bits(uint32_t bits)
    {
      m_packed_data &= ~ASTRAL_MASK(image_bits_bit0, image_bits_num_bits);
      m_packed_data |= pack_bits(image_bits_bit0, image_bits_num_bits, bits);
      return *this;
    }

    /*!
     * (x, y) of ImageMipElement::root_tile_location()
     * packed via pack_pair()
     */
    uint32_t m_root_min_corner;

    /*!
     * The min-corner of the sub-image into
     * the ImageMipElement packed via
     * pack_pair()
     */
    uint32_t m_subimage_min_corner;

    /*!
     * The size of the sub-image into the
     * ImageMipElement packed via
     * pack_pair()
     */
    uint32_t m_subimage_size;

    /*!
     * Pack into a single 32-bit integer:
     *  - Applicable bits as found in astral::ImageSamplerBits
     *    packed at bit \ref image_bits_bit0 taking \ref
     *    image_bits_num_bits
     *  - ImageMipElement::root_tile_location().z()
     *    packed at bit \ref root_tile_z_bit0 taking \ref
     *    root_tile_z_num_bits
     *  - ImageMipElement::number_index_levels()
     *    packed at bit \ref number_index_levels_bit0
     *    taking \ref number_index_levels_num_bits
     */
    uint32_t m_packed_data;
  };

/*! @} */
}

#endif

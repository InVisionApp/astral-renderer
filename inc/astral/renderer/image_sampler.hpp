/*!
 * \file image_sampler.hpp
 * \brief file image_sampler.hpp
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

#ifndef ASTRAL_IMAGE_SAMPLER_HPP
#define ASTRAL_IMAGE_SAMPLER_HPP

#include <astral/renderer/image.hpp>
#include <astral/renderer/image_sampler_bits.hpp>
#include <astral/renderer/render_value.hpp>
#include <astral/renderer/mipmap_level.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * \brief
   * An astral::ImageSampler image encapsulates how to sample and
   * filter from an astral::SubImage
   */
  class ImageSampler
  {
  public:
    /*!
     * Ctor to that make the astral::ImageSampler as NOT
     * sourcing from an astral::Image but instead
     * the raw value is (0, 0, 0, 0)
     */
    explicit
    ImageSampler(void):
      m_min_corner(0, 0),
      m_size(0, 0),
      m_bits(0u),
      m_image_id(),
      m_image_opaque(true)
    {
    }

    /*!
     * Ctor
     * \param image specifies the astral::Image and what portion of it from which to sample
     * \param f the filter to apply to sampling
     * \param mip the mipmapping to apply to sampling
     * \param post_sampling_mode the post-processing to apply to sampling
     * \param x_tile_mode the tiling mode to apply in the x-direction
     * \param y_tile_mode the tiling mode to apply in the y-direction
     */
    ImageSampler(const SubImage &image,
                 enum filter_t f = filter_linear,
                 enum mipmap_t mip = mipmap_ceiling,
                 enum color_post_sampling_mode_t post_sampling_mode = color_post_sampling_mode_direct,
                 enum tile_mode_t x_tile_mode = tile_mode_clamp,
                 enum tile_mode_t y_tile_mode = tile_mode_clamp):
      ImageSampler(image, image.m_image.colorspace(),
                   f, mip, post_sampling_mode,
                   x_tile_mode, y_tile_mode)
    {
    }

    /*!
     * Ctor
     * \param image specifies the astral::Image and what portion of it from which to sample
     * \param colorspace colorspace in which to interpret the pixels of the astral::Image,
     *                   this does not need to be same value as astral::Image::colorspace()
     * \param f the filter to apply to sampling
     * \param mip the mipmapping to apply to sampling
     * \param post_sampling_mode the post-processing to apply to sampling
     * \param x_tile_mode the tiling mode to apply in the x-direction
     * \param y_tile_mode the tiling mode to apply in the y-direction
     */
    ImageSampler(const SubImage &image,
                 enum colorspace_t colorspace,
                 enum filter_t f = filter_linear,
                 enum mipmap_t mip = mipmap_ceiling,
                 enum color_post_sampling_mode_t post_sampling_mode = color_post_sampling_mode_direct,
                 enum tile_mode_t x_tile_mode = tile_mode_clamp,
                 enum tile_mode_t y_tile_mode = tile_mode_clamp):
      m_mip_range(image.m_mip_range),
      m_min_corner(image.m_min_corner),
      m_size(image.m_size),
      m_image_id(image.m_image.ID()),
      m_image_opaque(image.m_opaque)
    {
      unsigned int max_lod, pre_padding;

      pre_padding = (image.m_image.default_use_prepadding()) ? image.m_image.tile_padding(0) : 0u;
      max_lod = (mip == mipmap_none || mip == mipmap_chosen) ? 0u : image.m_image.number_mipmap_levels() - 1u;

      m_bits = ImageSamplerBits::value(f, mip, max_lod, colorspace, post_sampling_mode,
                                       pre_padding, x_tile_mode, y_tile_mode);
    }

    /*!
     * Ctor
     * \param image specifies the astral::Image and what portion of it from which to sample
     * \param mask_type how to interpret the mask values
     * \param mask_channel what channel of the image to sample
     * \param f the filter to apply to sampling
     * \param mip the mipmapping to apply to sampling
     * \param post_sampling_mode the post-processing to apply to sampling
     * \param x_tile_mode the tiling mode to apply in the x-direction
     * \param y_tile_mode the tiling mode to apply in the y-direction
     */
    ImageSampler(const SubImage &image,
                 enum mask_type_t mask_type,
                 enum mask_channel_t mask_channel,
                 enum filter_t f = filter_linear,
                 enum mipmap_t mip = mipmap_ceiling,
                 enum mask_post_sampling_mode_t post_sampling_mode = mask_post_sampling_mode_direct,
                 enum tile_mode_t x_tile_mode = tile_mode_decal,
                 enum tile_mode_t y_tile_mode = tile_mode_decal):
      m_mip_range(image.m_mip_range),
      m_min_corner(image.m_min_corner),
      m_size(image.m_size),
      m_image_id(image.m_image.ID()),
      m_image_opaque(image.m_opaque)
    {
      unsigned int max_lod, pre_padding;

      pre_padding = (image.m_image.default_use_prepadding()) ? image.m_image.tile_padding(0) : 0u;
      max_lod = (mip == mipmap_none || mip == mipmap_chosen) ? 0u : image.m_image.number_mipmap_levels() - 1u;

      m_bits = ImageSamplerBits::value(mask_type, mask_channel, f, mip, max_lod, post_sampling_mode,
                                       pre_padding, x_tile_mode, y_tile_mode);
    }

    /*!
     * Ctor to sample from a specific mipmap level
     * \param image specifies the astral::Image and what portion of it from which to sample
     * \param mipmap_level what mipmap level from which to sample
     * \param f the filter to apply to sampling
     * \param post_sampling_mode the post-processing to apply to sampling
     * \param x_tile_mode the tiling mode to apply in the x-direction
     * \param y_tile_mode the tiling mode to apply in the y-direction
     */
    ImageSampler(const SubImage &image,
                 MipmapLevel mipmap_level,
                 enum filter_t f = filter_linear,
                 enum color_post_sampling_mode_t post_sampling_mode = color_post_sampling_mode_direct,
                 enum tile_mode_t x_tile_mode = tile_mode_clamp,
                 enum tile_mode_t y_tile_mode = tile_mode_clamp):
      ImageSampler(image, mipmap_level, image.m_image.colorspace(),
                   f, post_sampling_mode, x_tile_mode, y_tile_mode)
    {
    }

    /*!
     * Ctor to sample from a specific mipmap level
     * \param image specifies the astral::Image and what portion of it from which to sample
     * \param mipmap_level what mipmap level from which to sample
     * \param colorspace colorspace in which to interpret the pixels of the astral::Image,
     *                   this does not need to be same value as astral::Image::colorspace()
     * \param f the filter to apply to sampling
     * \param post_sampling_mode the post-processing to apply to sampling
     * \param x_tile_mode the tiling mode to apply in the x-direction
     * \param y_tile_mode the tiling mode to apply in the y-direction
     */
    ImageSampler(const SubImage &image,
                 MipmapLevel mipmap_level,
                 enum colorspace_t colorspace,
                 enum filter_t f = filter_linear,
                 enum color_post_sampling_mode_t post_sampling_mode = color_post_sampling_mode_direct,
                 enum tile_mode_t x_tile_mode = tile_mode_clamp,
                 enum tile_mode_t y_tile_mode = tile_mode_clamp):
      m_mip_range(image.m_mip_range),
      m_min_corner(image.m_min_corner),
      m_size(image.m_size),
      m_image_id(image.m_image.ID()),
      m_image_opaque(image.m_opaque)
    {
      unsigned int pre_padding;

      pre_padding = (image.m_image.default_use_prepadding()) ? image.m_image.tile_padding(0) : 0u;
      m_bits = ImageSamplerBits::value(f, mipmap_chosen, mipmap_level.m_value,
                                       colorspace, post_sampling_mode,
                                       pre_padding, x_tile_mode, y_tile_mode);
    }

    /*!
     * Ctor to sample from a specific mipmap level as a mask.
     * \param image specifies the astral::Image and what portion of it from which to sample
     * \param mipmap_level what mipmap level from which to sample
     * \param mask_type how to interpret the mask values
     * \param mask_channel what channel of the image to sample
     * \param f the filter to apply to sampling
     * \param post_sampling_mode the post-processing to apply to sampling
     * \param x_tile_mode the tiling mode to apply in the x-direction
     * \param y_tile_mode the tiling mode to apply in the y-direction
     */
    ImageSampler(const SubImage &image,
                 MipmapLevel mipmap_level,
                 enum mask_type_t mask_type,
                 enum mask_channel_t mask_channel,
                 enum filter_t f = filter_linear,
                 enum mask_post_sampling_mode_t post_sampling_mode = mask_post_sampling_mode_direct,
                 enum tile_mode_t x_tile_mode = tile_mode_decal,
                 enum tile_mode_t y_tile_mode = tile_mode_decal):
      m_mip_range(image.m_mip_range),
      m_min_corner(image.m_min_corner),
      m_size(image.m_size),
      m_image_id(image.m_image.ID()),
      m_image_opaque(image.m_opaque)
    {
      unsigned int pre_padding;

      pre_padding = (image.m_image.default_use_prepadding()) ? image.m_image.tile_padding(0) : 0u;
      m_bits = ImageSamplerBits::value(mask_type, mask_channel, f, mipmap_chosen,
                                       mipmap_level.m_value, post_sampling_mode,
                                       pre_padding, x_tile_mode, y_tile_mode);
    }

    /*!
     * Set \ref bits() to use the named astral::post_sampling_mode value.
     */
    ImageSampler&
    mask_post_sampling_mode(enum mask_post_sampling_mode_t v)
    {
      m_bits = ImageSamplerBits::mask_post_sampling_mode(m_bits, v);
      return *this;
    }

    /*!
     * Set \ref bits() to use the named astral::post_sampling_mode value.
     */
    ImageSampler&
    color_post_sampling_mode(enum color_post_sampling_mode_t v)
    {
      m_bits = ImageSamplerBits::color_post_sampling_mode(m_bits, v);
      return *this;
    }

    /*!
     * Set \ref bits() to use the named astral::mask_type_t value.
     */
    ImageSampler&
    mask_type(enum mask_type_t v)
    {
      m_bits = ImageSamplerBits::mask_type(m_bits, v);
      return *this;
    }

    /*!
     * Set \ref bits() to use the named astral::mask_channel_t value.
     */
    ImageSampler&
    mask_channel(enum mask_channel_t v)
    {
      m_bits = ImageSamplerBits::mask_channel(m_bits, v);
      return *this;
    }

    /*!
     * Set \ref bits() to use the named astral::filter_t value.
     */
    ImageSampler&
    filter(enum filter_t v)
    {
      m_bits = ImageSamplerBits::filter(m_bits, v);
      return *this;
    }

    /*!
     * Set \ref bits() to use the named astral::minification_mipmap_t.
     */
    ImageSampler&
    mipmap(enum mipmap_t v)
    {
      m_bits = ImageSamplerBits::mipmap(m_bits, v);
      return *this;
    }

    /*!
     * Set \ref bits() maximum LOD value.
     */
    ImageSampler&
    maximum_lod(unsigned int v)
    {
      m_bits = ImageSamplerBits::maximum_lod(m_bits, v);
      return *this;
    }

    /*!
     * Set \ref bits() x-tile mode value.
     */
    ImageSampler&
    x_tile_mode(enum tile_mode_t v)
    {
      m_bits = ImageSamplerBits::x_tile_mode(m_bits, v);
      return *this;
    }

    /*!
     * Set \ref bits() y-tile mode value.
     */
    ImageSampler&
    y_tile_mode(enum tile_mode_t v)
    {
      m_bits = ImageSamplerBits::y_tile_mode(m_bits, v);
      return *this;
    }

    /*!
     * Set \ref bits() to store the number of texels before the start
     * of an astral::TiledSubImage that can be used when sampling.
     * The simplest use case is for when the it is a proper sub-image
     * and one wants the texel outside of the sub-image to contribute
     * to filtering. The second use case if for when the sub-image
     * starts at the start of the astral::Image and one wants
     * the texels of its pre-padding to affect filtering.
     */
    ImageSampler&
    numbers_texels_pre_padding(unsigned int v)
    {
      m_bits = ImageSamplerBits::numbers_texels_pre_padding(m_bits, v);
      return *this;
    }

    /*!
     * Get from \ref bits() the astral::color_post_sampling_mode_t value.
     */
    enum color_post_sampling_mode_t
    color_post_sampling_mode(void) const
    {
      return ImageSamplerBits::color_post_sampling_mode(m_bits);
    }

    /*!
     * Get from \ref bits() the astral::mask_post_sampling_mode_t value.
     */
    enum mask_post_sampling_mode_t
    mask_post_sampling_mode(void) const
    {
      return ImageSamplerBits::mask_post_sampling_mode(m_bits);
    }

    /*!
     * Get from \ref bits() the astral::mask_type_t value.
     */
    enum mask_type_t
    mask_type(void) const
    {
      return ImageSamplerBits::mask_type(m_bits);
    }

    /*!
     * Get from \ref bits() the astral::mask_channel_t value.
     */
    enum mask_channel_t
    mask_channel(void) const
    {
      return ImageSamplerBits::mask_channel(m_bits);
    }

    /*!
     * Get from \ref bits() the astral::magnification_filter_t value.
     */
    enum filter_t
    filter(void) const
    {
      return ImageSamplerBits::filter(m_bits);
    }

    /*!
     * Get from \ref bits() the astral::minification_mipmap_t value.
     */
    enum mipmap_t
    mipmap(void) const
    {
      return ImageSamplerBits::mipmap(m_bits);
    }

    /*!
     * Get from \ref bits() maximum LOD value.
     */
    unsigned int
    maximum_lod(void) const
    {
      return ImageSamplerBits::maximum_lod(m_bits);
    }

    /*!
     * Get from \ref bits() x-tile mode value.
     */
    enum tile_mode_t
    x_tile_mode(void) const
    {
      return ImageSamplerBits::x_tile_mode(m_bits);
    }

    /*!
     * Get from \ref bits() y-tile mode value.
     */
    enum tile_mode_t
    y_tile_mode(void) const
    {
      return ImageSamplerBits::y_tile_mode(m_bits);
    }

    /*!
     * Get from \ref bits() the number of pre-padding texels
     * allowed for sampled filtering
     */
    unsigned int
    numbers_texels_pre_padding(void) const
    {
      return ImageSamplerBits::numbers_texels_pre_padding(m_bits);
    }

    /*!
     * The min-corner into the astral::Image used.
     */
    const uvec2&
    min_corner(void) const
    {
      return m_min_corner;
    }

    /*!
     * The size into the source from which to sample pixels.
     */
    const uvec2&
    size(void) const
    {
      return m_size;
    }

    /*!
     * Image properties packed into bits via astral::ImageSamplerBits.
     */
    uint32_t
    bits(void) const
    {
      return m_bits;
    }

    /*!
     * Returns true if the pixels referenced by this
     * astral::ImageSampler are regarded as all opaque.
     */
    bool
    image_opaque(void) const
    {
      return m_image_opaque;
    }

    /*!
     * Override the opacity of the image
     */
    ImageSampler&
    override_image_opacity(bool v)
    {
      m_image_opaque = v;
      return *this;
    }

    ///@cond
    ImageID
    image_id(void) const
    {
      return m_image_id;
    }
    ///@endcond

    /*!
     * Returns the chain of mip-maps this astral::ImageSampler
     * uses. Warning: if the astral::Image that was used to create
     * this astral::ImageSampler has been deleted, then this
     * will return an empty array even if \ref m_mip_range is
     * not empty.
     */
    c_array<const reference_counted_ptr<const ImageMipElement>>
    mip_chain(ImageAtlas &atlas) const
    {
      const Image *im;

      im = atlas.fetch_image(m_image_id);
      return (im) ?
        im->mip_chain().sub_array(m_mip_range) :
        c_array<const reference_counted_ptr<const ImageMipElement>>();
    }

    /*!
     * The range of mipmaps into Image::mip_chain(); empty indicates no
     * image data and that the raw value is (0, 0, 0, 0)
     */
    range_type<unsigned int> m_mip_range;

    /*!
     * min-corner of the portion of mip_chain().front() to access
     */
    uvec2 m_min_corner;

    /*!
     * size of the portion of mip_chain().front() to access
     */
    uvec2 m_size;

    /*!
     * bits encoding filter, mipmap mode, color transfer mode
     */
    uint32_t m_bits;

    /*!
     * image id for book keeping, this value is used internally
     * by Renderer to make sure that the image data is ready
     * before it is used for those images that are rendered.
     */
    ImageID m_image_id;

    /*!
     * If the image data is regarded as opaque
     */
    bool m_image_opaque;
  };

/*! @} */
}

#endif

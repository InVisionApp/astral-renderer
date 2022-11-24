/*!
 * \file renderer_virtual_buffer.hpp
 * \brief file renderer_virtual_buffer.hpp
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

#ifndef ASTRAL_RENDERER_VIRTUAL_BUFFER_HPP
#define ASTRAL_RENDERER_VIRTUAL_BUFFER_HPP

#include <astral/util/layered_rect_atlas.hpp>
#include <astral/util/interval_allocator.hpp>
#include <astral/renderer/renderer.hpp>

#include "renderer_draw_command.hpp"
#include "renderer_cached_transformation.hpp"
#include "renderer_clip_geometry.hpp"
#include "renderer_stc_data.hpp"
#include "renderer_cached_combined_path.hpp"
#include "renderer_workroom.hpp"

/*!
 * A VirtualBuffer is the actual backing to a RenderEncoderBase
 * derived object. It encapsulates:
 *  - list of drawing command (embodied by DrawCommandList)
 *  - list of STCData for stencil-then-cover-fills
 *  - if it renders to a portion of a Image, what portion
 */
class astral::Renderer::VirtualBuffer:astral::noncopyable
{
public:
  /* sorting classes */
  class SorterCommon;
  class AreaSorter;
  class ShadowSizeSorter;
  class FormatSorter;
  class FirstShaderUsedSorter;
  class IsMaskFormat;

  enum type_t:uint32_t
    {
      /* Represents that the virtual buffer renders to a
       * RenderTarget
       */
      render_target_buffer,

      /* Represents that the virtual buffer renders to an image */
      image_buffer,

      /* Represents that the virtual buffer renders to a portion
       * of an image of another VirtualBuffer
       */
      sub_image_buffer,

      /* Represents that the virtual buffer renders to an image
       * of size (0, 0).
       */
      degenerate_buffer,

      /* represents that the virtual buffer is not rendered to
       * directly, instead it is essentially a Image whose
       * tiles come from other virtual buffer renders.
       */
      assembled_buffer,

      /* represents that the virtual buffer is used to generate
       * a shadow map
       */
      shadowmap_buffer,
    };

  enum sub_buffer_type_t:uint32_t
    {
      /* These buffers are made during Renderer::Implement::on_renderer_end()
       * when a VirtualBuffer V size exceeds max_renderable_buffer_size. In
       * this case, the buffer V spawns child buffers C[] which correspond
       * to sub-rects of the image of V. Note that no additional commands
       * will be added to V either
       */
      sub_image_buffer_copy_commands_from_parent,

      /* Indicates that the sub-buffer does NOT copy commands
       * from the parent VirtualBuffer and that it renders commands
       * directly added to it.
       */
      sub_image_buffer_renderer,
    };

  enum buffer_sizes_t:int32_t //int32_t chosen to silence sign compare warnings.
    {
      /* The size of the offscreen scratch buffer. The
       * value 2048 is chosen for the following reasons:
       *  1) that is the size that WebGL2/GLES3 can guarantee
       *  2) there have been cases where some WebGL2 implementations
       *     act wierd with sizes past 2048 although they reported
       *     a max texture size of 4096.
       */
      render_scratch_buffer_size = 2048,

      /* the maximum size for a VirtualBuffer; any VirtualBuffer
       * bigger than this value is broken into smaller render
       * jobs.
       */
      max_renderable_buffer_size = render_scratch_buffer_size
    };
  ASTRALstatic_assert(max_renderable_buffer_size <= render_scratch_buffer_size);

  class TileSource
  {
  public:
    /* VirtualBuffer::render_index() of the image that has the tile */
    unsigned int m_src_render_index;

    /* which tile from the source image */
    uvec2 m_src_tile;
  };

  class TileSourceImage
  {
  public:
    /* what image to take the tile from, assuming taking from mip level 0 */
    reference_counted_ptr<const Image> m_src_image;

    /* which tile from the source image */
    uvec2 m_src_tile;
  };

  /* Tag of where the in the coder base the VirtualBuffer was created */
  class CreationTag
  {
  public:
    CreationTag(const char *f, int l):
      m_file(f),
      m_line(l)
    {}

    const char *m_file;
    int m_line;
  };

  /* Describes when and how the backing astral::Image of a
   * VirtualBuffer is created; default is to create the image
   * when issue_finish() is called.
   */
  class ImageCreationSpec
  {
  public:
    ImageCreationSpec(void):
      m_create_immediately(false),
      m_default_use_prepadding_true(false)
    {}

    ImageCreationSpec&
    create_immediately(bool v)
    {
      m_create_immediately = v;
      return *this;
    }

    ImageCreationSpec&
    default_use_prepadding_true(bool v)
    {
      m_default_use_prepadding_true = v;
      return *this;
    }

    /*!
     * If true, create the astral::Image at ctor instead of
     * waiting unti issue_finish().
     */
    bool m_create_immediately;

    /*!
     * If true, call astral::Image::default_use_prepadding(true)
     * when the image is created
     */
    bool m_default_use_prepadding_true;
  };

  /* Ctor to create a VirtualBuffer to render to an offscreen
   * image with clipping and transformation specified. Used by
   * generate_child_buffer()
   * \param C where the VirtaulBuffer was created
   * \param render_index if index into Storage that gives the VirtualBuffer
   * \param renderer the Renderer doing the rendering
   * \param initial_transformation initial transformation
   * \param geometry clipping to use
   * \param render_type the nature of the rendering: color, mask or shadowmap
   * \param blit_processing post-processing to perform on blit to atlas
   * \param colorspace colorspace at which to render
   * \param stc_fill_rule fill rule to apply in stencil-then-cover,
   *                      a value of number_fill_rule indicates that
   *                      it is not a mask for filling
   * \param image_create_spec when and how to create the backing astral::Image
   */
  VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
                const Transformation &initial_transformation,
                const Implement::ClipGeometryGroup &geometry,
                enum Implement::DrawCommandList::render_type_t render_type,
                enum image_blit_processing_t blit_processing,
                enum colorspace_t colorspace,
                enum fill_rule_t stc_fill_rule,
                ImageCreationSpec image_create_spec);

  /* Ctor to create a VirtualBuffer to render to an offscreen
   * color image passing the clipping and transformation. Used by
   * generate_child_buffer()
   * \param C where the VirtaulBuffer was created
   * \param render_index if index into Storage that gives the VirtualBuffer
   * \param renderer the Renderer doing the rendering
   * \param initial_transformation initial transformation
   * \param geometry clipping to use
   * \param colorspace colorspace at which to render
   * \param image_create_spec when and how to create the backing astral::Image
   */
  VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
                const Transformation &initial_transformation,
                const Implement::ClipGeometryGroup &geometry, enum colorspace_t colorspace,
                ImageCreationSpec image_create_spec):
    VirtualBuffer(C, render_index, renderer, initial_transformation,
                  geometry, Implement::DrawCommandList::render_color_image,
                  image_processing_none, colorspace,
                  number_fill_rule, image_create_spec)
  {}

  /* Ctor to create a VirtualBuffer to render to an offscreen
   * mask image passing clipping and transformation. Used by
   * generate_child_buffer()
   * \param C where the VirtaulBuffer was created
   * \param render_index if index into Storage that gives the VirtualBuffer
   * \param renderer the Renderer doing the rendering
   * \param initial_transformation initial transformation
   * \param geometry clipping to use
   * \param stc_fill_rule fill rule to apply in stencil-then-cover,
   *                      a value of number_fill_rule indicates that
   *                      mask is for stroking and thus no post-processing
   *                      is to be performed
   * \param image_create_spec when and how to create the backing astral::Image
   */
  VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
                const Transformation &initial_transformation,
                const Implement::ClipGeometryGroup &geometry, enum fill_rule_t stc_fill_rule,
                ImageCreationSpec image_create_spec):
    VirtualBuffer(C, render_index, renderer, initial_transformation,
                  geometry, Implement::DrawCommandList::render_mask_image,
                  image_blit_processing_for_mask(stc_fill_rule),
                  colorspace_linear, stc_fill_rule, image_create_spec)
  {}

  /* Ctor to create a VirtualBuffer to render to an offscreen
   * image that does not inherit any clipping or transformation
   * \param C where the VirtaulBuffer was created
   * \param render_index if index into Storage that gives the VirtualBuffer
   * \param renderer the Renderer doing the rendering
   * \param image_size size of the image
   * \param render_type the nature of the rendering: color, mask or shadowmap
   * \param blit_processing post-processing to perform on blit to atlas
   * \param colorspace colorspace at which to render
   * \param stc_fill_rule fill rule to apply in stencil-then-cover,
   *                      a value of number_fill_rule indicates that
   *                      it is not a mask for filling
   * \param image_create_spec when and how to create the backing astral::Image
   */
  VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
                ivec2 image_size,
                enum Implement::DrawCommandList::render_type_t render_type,
                enum image_blit_processing_t blit_processing,
                enum colorspace_t colorspace,
                enum fill_rule_t stc_fill_rule,
                ImageCreationSpec image_create_spec);

  /* Ctor to create a VirtualBuffer to render to an offscreen
   * color image that does not inherit any clipping or transformation
   * \param C where the VirtaulBuffer was created
   * \param render_index if index into Storage that gives the VirtualBuffer
   * \param renderer the Renderer doing the rendering
   * \param image_size size of backing image
   * \param colorspace colorspace at which to render
   * \param image_create_spec when and how to create the backing astral::Image
   */
  VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
                ivec2 image_size, enum colorspace_t colorspace,
                ImageCreationSpec image_create_spec):
    VirtualBuffer(C, render_index, renderer, image_size,
                  Implement::DrawCommandList::render_color_image,
                  image_processing_none,
                  colorspace, number_fill_rule, image_create_spec)
  {}

  /* Ctor to create a VirtualBuffer to render to an offscreen
   * mask image that does not inherit any clipping or transformation
   *
   * \param C where the VirtaulBuffer was created
   * \param render_index if index into Storage that gives the VirtualBuffer
   * \param renderer the Renderer doing the rendering
   * \param image_size size of image to render
   * \param stc_fill_rule fill rule to apply in stencil-then-cover,
   *                      a value of number_fill_rule indicates that
   *                      mask is for stroking and thus no post-processing
   *                      is to be performed
   * \param image_create_spec when and how to create the backing astral::Image
   */
  VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
                ivec2 image_size, enum fill_rule_t stc_fill_rule,
                ImageCreationSpec image_create_spec):
    VirtualBuffer(C, render_index, renderer, image_size,
                  Implement::DrawCommandList::render_mask_image,
                  image_blit_processing_for_mask(stc_fill_rule),
                  colorspace_linear, stc_fill_rule, image_create_spec)
  {}

  /* Ctor for creating a VirtualBuffer where only some of the tiles are backed
   * \param C where the VirtaulBuffer was created
   * \param render_index if index into Storage that gives the VirtualBuffer
   * \param renderer the Renderer doing the rendering
   * \param initial_transformation initial transformation
   * \param geometry clipping to use
   * \param render_type the nature of the rendering: color, mask or shadowmap
   * \param blit_processing post-processing to perform on blit to atlas
   * \param colorspace colorspace at which to render
   * \param stc_fill_rule fill rule to apply in stencil-then-cover,
   *                      a value of number_fill_rule indicates that
   *                      it is not a mask for filling
   * \param tile_regions array of disjoint tile ranges listing what
   *                     tiles of image are to be backed
   * \param out_virtual_buffers location to which to write the VirtualBuffer
   *                            pointers for the purpose of rendering to the
   *                            individual tiles; The size of the aray must
   *                            be the exact same size as tile_regions. The
   *                            value of out_virtual_buffers[i] is a pointer
   *                            to the virtual buffer that draws to the region
   *                            given by tile_regions[i].
   *
   * NOTE: m_image WILL be created at ctor and it is partially backed.
   */
  VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
                const Transformation &initial_transformation,
                Implement::ClipGeometryGroup &geometry,
                enum Implement::DrawCommandList::render_type_t render_type,
                enum image_blit_processing_t blit_processing,
                enum colorspace_t colorspace,
                enum fill_rule_t stc_fill_rule,
                c_array<const vecN<range_type<int>, 2>> tile_regions,
                c_array<VirtualBuffer*> out_virtual_buffers);

  VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
                const Transformation &initial_transformation,
                Implement::ClipGeometryGroup &geometry,
                enum fill_rule_t stc_fill_rule,
                c_array<const vecN<range_type<int>, 2>> tile_regions,
                c_array<VirtualBuffer*> out_virtual_buffers):
    VirtualBuffer(C, render_index, renderer, initial_transformation,
                  geometry, Implement::DrawCommandList::render_mask_image,
                  image_blit_processing_for_mask(stc_fill_rule),
                  colorspace_linear, stc_fill_rule,
                  tile_regions, out_virtual_buffers)
  {}

  /* A VirtualBuffer that represents a single Image,
   * but with the tiles of that Image coming from
   * other VirtualBuffer render jobs to be assembled into
   * a single Image. Such a VirtualBuffer is never for rendering,
   * it is only for the purpose of making sure all the renders of
   * the tiles on which it depends are ready. Used by sparse fill
   * mask generation and indirectly by sparse stroke mask generation
   * via the method create_assembled_image().
   *
   * \param C where the VirtaulBuffer was created
   * \param render_index if index into Storage that gives the VirtualBuffer
   * \param renderer the Renderer doing the rendering
   * \param colorspace colorspace at which to render
   * \param empty_tiles as found in ImageMipElement::create_sub_mip()
   * \param fully_covered_tiles as found in ImageMipElement::create_sub_mip()
   * \param encoder_shared_tiles list of tiles and VirtualBuffers to share from other VirtualBuffers
   * \param image_shared_tiles list of tiles and image to share from other images
   */
  VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
                uvec2 sz, enum colorspace_t colorspace,
                c_array<const uvec2> empty_tiles,
                c_array<const uvec2> fully_covered_tiles,
                c_array<const std::pair<uvec2, TileSource>> encoder_shared_tiles,
                c_array<const std::pair<uvec2, TileSourceImage>> image_shared_tiles);

  /* A VirtualBuffer that represents an Image, but some of the
   * tiles of the Image are replaced by empty or full tiles.
   * Such a VirtualBuffer is never for rendering, it is only for
   * the purpose of making sure all the renders of the tiles on
   * which it depends are ready. Used by sparse fill mask generation
   * and indirectly by sparse stroke mask generation via the method
   * create_assembled_image().
   *
   * \param C where the VirtaulBuffer was created
   * \param render_index if index into Storage that gives the VirtualBuffer
   * \param renderer the Renderer doing the rendering
   * \param src_image image with which to share tiles
   * \param tile_range as found in ImageMipElement::create_sub_mip(),
   *                   i.e. what subrange of tiles from src_image
   *                   the image of the created VirtaulBuffer has
   * \param empty_tiles as found in ImageMipElement::create_sub_mip()
   * \param full_tiles as found in ImageMipElement::create_sub_mip()
   * \param shared_tiles as found in ImageMipElement::create_sub_mip()
   */
  VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
                const astral::Image &src_image,
                vecN<range_type<unsigned int>, 2> tile_range,
                c_array<const uvec2> empty_tiles,
                c_array<const uvec2> full_tiles,
                c_array<const uvec2> shared_tiles);

  VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
                const astral::Image &src_image,
                c_array<const uvec2> empty_tiles,
                c_array<const uvec2> full_tiles,
                c_array<const uvec2> shared_tiles):
    VirtualBuffer(C, render_index, renderer, src_image,
                  src_image.mip_chain().front()->tile_range(),
                  empty_tiles, full_tiles, shared_tiles)
  {}

  /* Ctor to create an Image whose mipchain consists of
   * the mip-chain of the Image of a first VirtualBuffer
   * followed by mip-chain of the Image of another VirtualBuffer.
   * Such a VirtualBuffer is never for rendering, it only exists to
   * make sure that dependencies are met.
   *
   * \param render_index if index into Storage that gives the VirtualBuffer
   * \param renderer the Renderer doing the rendering
   * \param C where the VirtaulBuffer was created
   * \param mip_chain VirtualBuffer whose image gives the first portion
   *                  of the mip-chain
   * \param mip_chain_tail VirtualBuffer whose image gives the remaining
   *                  portion of the mip-chain
   */
  VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
                VirtualBuffer &mip_chain,
                VirtualBuffer &mip_chain_tail);

  /* Ctor to create a VirtualBuffer to render to a RenderTarget
   *
   * \param C where the VirtaulBuffer was created
   * \param render_index if index into Storage that gives the VirtualBuffer
   * \param renderer the Renderer doing the rendering
   * \param rt RenderTarget to which to render
   * \param clear_color clear color to use upon clearing the render target
   *                    represented as an u8vec4.
   * \param colorspace color space of rendering
   * \param brush if valid(), means that clear_color is not (0, 0, 0, 0)
   *              and gives the RenderValue<Brush> to use when drawing a
   *              rect of the clear color
   * \param region if non-null, the virtual buffer renders to the named
   *               sub-region of rt.
   */
  VirtualBuffer(CreationTag C, unsigned int render_index,
                Renderer::Implement &renderer, RenderTarget &rt,
                u8vec4 clear_color, enum colorspace_t colorspace,
                RenderValue<Brush> clear_brush,
                const SubViewport *region = nullptr);

  /* Ctor for a VirtualBuffer that generates pixels for a ShadowMap
   *
   * \param C where the VirtaulBuffer was created
   * \param render_index if index into Storage that gives the VirtualBuffer
   * \param renderer the Renderer doing the rendering
   * \param shadow_map the ShadowMap to which to render
   * \param light_p position of the light
   */
  VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
                reference_counted_ptr<ShadowMap> shadow_map,
                vec2 light_p);

  /* Ctor for a VirtualBuffer that renders the contents of a portion of
   * another VirtualBuffer that is too large for the real render target.
   * The caller must mark that the src_buffer is dependent on the created
   * buffer manually.
   *
   * \param C where the VirtaulBuffer was created
   * \param render_index if index into Storage that gives the VirtualBuffer
   * \param renderer the Renderer doing the rendering
   * \param src_buffer VirtualBuffer holding the astral::Image to which to render
   * \param image_region region into src_buffer.m_image to whcih to render
   * \param tp specifies the sub-buffer type
   */
  VirtualBuffer(CreationTag C, unsigned int render_index, Renderer::Implement &renderer,
                VirtualBuffer &src_buffer, const RectT<int> image_region,
                enum sub_buffer_type_t tp);

  ~VirtualBuffer();

  /* Add the shader used to draw the depth-rect to the uber
   * shader getting built on the RenderEngine
   */
  static
  void
  add_depth_rect_shader_to_uber(Renderer::Implement &renderer, RenderBackend::UberShadingKey &uber_key);

  /* If image is not backed by a VirtualBuffer or if shared_tiles
   * is empty, creates the Image directly with no VirtualBuffer
   * intermediate. If the image is backed by a VirtualBuffer and
   * if shared_tiles is non-empty, then generates a VirtaulBuffer
   * so that dependency tracking occurs.
   */
  static
  reference_counted_ptr<const Image>
  create_assembled_image(CreationTag C, Renderer::Implement &renderer,
                         const astral::Image &image,
                         vecN<range_type<unsigned int>, 2> tile_range,
                         c_array<const uvec2> empty_tiles,
                         c_array<const uvec2> fully_covered_tiles,
                         c_array<const uvec2> shared_tiles);
  static
  reference_counted_ptr<const Image>
  create_assembled_image(CreationTag C, Renderer::Implement &renderer,
                         const astral::Image &image,
                         c_array<const uvec2> empty_tiles,
                         c_array<const uvec2> fully_covered_tiles,
                         c_array<const uvec2> shared_tiles)
  {
    return create_assembled_image(C, renderer, image,
                                  image.mip_chain().front()->tile_range(),
                                  empty_tiles, fully_covered_tiles,
                                  shared_tiles);
  }

  static
  reference_counted_ptr<const Image>
  create_assembled_image(CreationTag C, Renderer::Implement &renderer,
                         uvec2 sz, enum colorspace_t colorspace,
                         c_array<const uvec2> empty_tiles,
                         c_array<const uvec2> fully_covered_tiles,
                         c_array<const std::pair<uvec2, TileSource>> encoder_shared_tiles,
                         c_array<const std::pair<uvec2, TileSourceImage>> image_shared_tiles);

  /* returns where the VirtualBuffer was created */
  const CreationTag&
  creation_tag(void) const
  {
    return m_creation_tag;
  }

  /* Returns the VirtualBuffer type */
  enum type_t
  type(void) const
  {
    return m_type;
  }

  /* Returns the count of VirtualBuffers on which this
   * VirtualBuffer depends that have not yet rendered
   * their pixels
   */
  unsigned int
  remaining_dependencies(void) const
  {
    return m_remaining_dependencies;
  }

  /* Once the rendering of this VirtualBuffer are completed,
   * this function is called to send a message to all the
   * VirtualBuffer that depend on this VirtualBuffer that
   * this VirtualBuffer's pixels are ready to be blitted
   * from the passed ColorBuffer
   */
  void
  render_performed(ColorBuffer *src) const
  {
    render_performed_implement(src, nullptr);
  }

  /* Returns the DrawCommandList of this VirtualBuffer;
   * the value is NULL if the VirtualBuffer should not
   * accept commands (for example if the VirtualBuffer
   * is a degenerate_buffer or a node_dependency_buffer).
   */
  Implement::DrawCommandList*
  command_list(void)
  {
    return m_command_list;
  }

  const Implement::DrawCommandList*
  command_list(void) const
  {
    return m_command_list;
  }

  /* Returns the RenderBackend::ClipWindowValue to be used when
   * rendering the VirtualBuffer along with others to the
   * scratch RenderTarget
   */
  const RenderBackend::ClipWindowValue&
  clip_window(void) const
  {
    return m_clip_window;
  }

  /* Returns the ImageCreationSpec of the VirtualBuffer */
  const ImageCreationSpec&
  image_create_spec(void) const
  {
    return m_image_create_spec;
  }

  /* Returns the astral::Image to which the pixel of the
   * VirtualBuffer's render are blitted.
   */
  const reference_counted_ptr<Image>&
  fetch_image(void) const
  {
    ASTRALhard_assert(type() == image_buffer || type() == sub_image_buffer
                        || type() == degenerate_buffer || type() == assembled_buffer);
    ASTRALhard_assert(m_image_create_spec.m_create_immediately || finish_issued());

    return m_image;
  }

  /* Returns the astral::RenderTarget to which the VirtualBuffer
   * is rendering. A null return value indicates that it is NOT
   * rendering to an astral::RenderTarget.
   */
  const reference_counted_ptr<RenderTarget>&
  render_target(void) const
  {
    return m_render_target;
  }

  /* Returns the clear color to issue to render_target() when
   * rendering render_target() is non-null.
   */
  u8vec4
  render_target_clear_color(void) const
  {
    return m_render_target_clear_color;
  }

  /* Returns an astral::Image of this object with the named number
   * of mipmaps. It is an error to call this before issue_finish()
   * has been called.
   */
  const reference_counted_ptr<Image>&
  image_with_mips(unsigned int max_lod);

  /* Returns an astral::Image of this object consisting of a single
   * ImageMipElement that hold the named LOD level. If the requested
   * LOD level exceeds the supported LOD, will return a null reference
   * instead. It is an error to call this before issue_finish() has been
   * called.
   */
  const reference_counted_ptr<Image>&
  image_last_mip_only(unsigned int lod, unsigned int *actualLOD);

  /* Returns the colorspace of the VirtualBuffer */
  enum colorspace_t
  colorspace(void) const
  {
    ASTRALassert(!m_image || m_image->colorspace() == m_colorspace);
    return m_colorspace;
  }

  /* Returns the pixel-rect which is the rect in the root
   * VirtualBuffer that spawns this VirtualBuffer. The returned
   * value includes the padding around the image when rendering
   * to an image. The root of a VirtualBuffer is defined as follows:
   *   - if VirtualBuffer was created via generate_child_buffer()
   *     then the created VirtualBuffer shares the same root as
   *     the object on which generate_child_buffer() was called
   *   - in all other cases, the root of a VirtualBuffer is itself
   *
   * When rendering to an image, the conversion from units of
   * pixel_rect() to the image target is to multiply by scale_factor().
   * In particular, a scale_factor() < 1.0, represents that a
   * single pixel of the image covers multiple pixels in the root.
   */
  const BoundingBox<float>&
  pixel_rect(void) const
  {
    return clip_geometry().bounding_geometry().pixel_rect();
  }

  /*!
   * Gives the transformation from coordinates
   * of pixel_rect() to coordinate of the
   * astral::Image returned by image().
   */
  const ScaleTranslate&
  image_transformation_pixel(void) const
  {
    return clip_geometry().bounding_geometry().image_transformation_pixel();
  }

  /* image-processing to perform on blit to image atlas */
  enum image_blit_processing_t
  blit_processing(void) const
  {
    return m_command_list->blit_processing();
  }

  /* what kind of rendering: color, mask or shadowmap */
  enum Implement::DrawCommandList::render_type_t
  render_type(void) const
  {
    return m_command_list->render_type();
  }

  /* Gives the scaling factor from coordinates of pixel_rect()
   * to actaul pixels renderer.
   */
  float
  scale_factor(void) const
  {
    return clip_geometry().bounding_geometry().scale_factor();
  }

  /* Gives the area of the actual render target covered
   * by the VirtualBuffer; for all but the VirtualBuffer
   * spanwed from the RenderTarget passed to Renderer::Implement::begin()
   * this is the area of image(). For the VirtualBuffer
   * rendering to the RenderTarget passed to Renderer::Implement::begin(),
   * this is the area of the RenderTarget
   */
  int
  area(void) const
  {
    ivec2 sz(offscreen_render_size());
    return sz.x() * sz.y();
  }

  /* Returns the fill_rule_t to apply to the STCData.
   * Renderer uses this function to places mask virtual
   * buffers into four buckets, keyed by fill rule for
   * the purpose of the cover pass. If the fill rule is
   * a complement fill rule, then the cover pass still
   * needs to be drawn. However, if it is not a complement
   * fill rule, then the cover pass can be skipped.
   */
  enum fill_rule_t
  stc_fill_rule(void) const
  {
    bool can_skip_if_empty, empty;

    can_skip_if_empty = (m_stc_fill_rule == number_fill_rule)
      || !fill_rule_is_complement_rule(m_stc_fill_rule);

    empty = m_stc[FillSTCShader::pass_contour_stencil].empty()
      && m_stc[FillSTCShader::pass_conic_triangles_stencil].empty();

    return (empty && can_skip_if_empty) ?
      number_fill_rule:
      m_stc_fill_rule;
  }

  /* Invert the value of stc_fill_rule(), i.e. afterwards
   * the value of stc_fill_rule() will be
   * invert_fill_rule(stc_fill_rule())
   */
  void
  invert_stc_fill_rule(void)
  {
    ASTRALassert(m_stc_fill_rule != number_fill_rule);
    m_stc_fill_rule = invert_fill_rule(m_stc_fill_rule);
  }

  /* The location within the scatch render target where
   * this VirtualBuffer is to be rendered; the value is
   * set by location_in_color_buffer(LayeredRectAtlas::Entry).
   * The buffer must be of type image_buffer or sub_image_buffer.
   */
  const Implement::WorkRoom::ImageBufferLocation&
  location_in_color_buffer(void) const
  {
    ASTRALassert(type() == image_buffer || type() == sub_image_buffer);
    return m_location_in_color_buffer;
  }

  /* Set the location within the scatch render target where
   * this VirtualBuffer is to be rendered. Called by Renderer.
   */
  void
  location_in_color_buffer(Implement::WorkRoom::ImageBufferLocation E);

  /*!
   * Returns the size needed to render the contents
   * this VirtualBuffer. The buffer must be of type
   * image_buffer or sub_image_buffer.
   */
  ivec2
  offscreen_render_size(void) const
  {
    ASTRALassert(type() == image_buffer || type() == sub_image_buffer);
    return m_render_rect.size();
  }

  /* Used by Renderer to enable clipping via z-buffer occlusion;
   * the start_z() value is passed to DrawCommand::send_to_backend()
   * as the add_z argument; this makes the z-occlusion value of
   * each draw command incremented by start_z().
   */
  void
  start_z(unsigned int v)
  {
    /* only image buffers or render target buffers with a clip window should ever set start_z() */
    ASTRALassert(type() == image_buffer || type() == sub_image_buffer
                 || (type() == render_target_buffer && m_clip_window.clip_window_value_type() != clip_window_not_present));

    /* start_z() should only set it once */
    ASTRALassert(m_start_z == 0u);

    m_start_z = v;
  }

  /* fetch the start_z() value */
  unsigned int
  start_z(void) const
  {
    ASTRALassert(type() == image_buffer || type() == sub_image_buffer || m_start_z == 0
                 || (type() == render_target_buffer && m_clip_window.clip_window_value_type() != clip_window_not_present));
    return m_start_z;
  }

  /*!
   * Returns if to permute the x and y coordinates when
   * rendering the contents of this VirtualBuffer to the
   * scratch render target.
   */
  bool
  permute_xy_when_rendering(void) const
  {
    /* only image and sub-image buffers should ever have the (x, y) permuted;
     * the empty ctor of m_location_in_color_buffer sets it as false.
     */
    ASTRALassert(type() == image_buffer || type() == sub_image_buffer || !m_location_in_color_buffer.m_permute_xy);
    return m_location_in_color_buffer.m_permute_xy;
  }

  /* Add to the passed bounding box the region of the
   * scratch render target that this VirtualBuffer
   * wil be rendered to.
   */
  void
  add_scratch_area(BoundingBox<int> *dst) const;

  /*!
   * Gives the RenderValue of the transformation from virtual
   * pixel coordinates COMPOSED with the translation to where
   * VirtualBuffer is rendered in the scratch buffer render
   * target
   */
  RenderValue<ScaleTranslate>
  render_scale_translate(void) const
  {
    return m_render_scale_translate;
  }

  /* Returns true if this VirtualBuffer is ended and will
   * no longer accept rendering commands.
   */
  bool
  finish_issued(void) const
  {
    return m_finish_issued;
  }

  /* Mark that this VirtualBuffer is to no longer
   * accept new rendering commands. However, this command
   * is IGNORED if the VirtualBuffer is inside a
   * being_render_effect()/end_render_effect() pair.
   */
  void
  issue_finish(void);

  /* Returns the array of STCData objects that the VirtualBuffer
   * is to act of for stencil-then-cover mask fills.
   */
  Implement::STCData::VirtualArray
  stc_data(enum FillSTCShader::pass_t p) const
  {
    return m_stc[p];
  }

  /* Set the STCData */
  void
  stc_data(const vecN<Implement::STCData::VirtualArray, FillSTCShader::pass_count> &v)
  {
    ASTRALassert(!finish_issued());
    m_stc = v;
  }

  /* conveniance function, just issues values() from the correct
   * data from m_storage, i.e.
   * \code
   * m_stc[p].values(m_renderer.m_storage->stc_data_set().m_stc_data[pass]);
   * \endcode
   */
  c_array<const Implement::STCData>
  stc_data_values(enum FillSTCShader::pass_t p) const;

  /* Returns ClipGeometry of this VirtualBuffer which specifies
   * the clip-planes of the VirtualBuffer.
   */
  const Implement::ClipGeometryGroup&
  clip_geometry(void) const
  {
    return m_clip_geometry;
  }

  /* Returns a ClipElement corresponding to using a channel
   * of the image to render to for clipping. May only be called
   * if type() is image_buffer
   */
  reference_counted_ptr<const RenderClipElement>
  clip_element(enum mask_type_t mask_type,
               enum mask_channel_t mask_channel);

  /* Helper to create a transformation from the current transformation
   * that may or may not modified by a translation and/or scale
   */
  RenderValue<Transformation>
  create_transformation(const vec2 *ptranslate, const float2x2 *pmatrix)
  {
    return m_transformation_stack.back().create_transformation(m_renderer, ptranslate, pmatrix, &m_pre_transformation);
  }

  /* Compute the accuracy tolerance from the current
   * transformation concated with an optional scale.
   * A nullptr value on scale means to not concat with
   * a scale.
   */
  float
  compute_tol(const vec2 *scale) const
  {
    return m_transformation_stack.back().compute_tol(m_render_accuracy, scale);
  }

  /* Compute the accuracy tolerance from the current
   * transformation concated with an optional matrix.
   * A nullptr value on scale matrix to not concat with
   * a matrix.
   */
  float
  compute_tol(const float2x2 *matrix) const
  {
    return m_transformation_stack.back().compute_tol(m_render_accuracy, matrix);
  }

  /* Compute the accuracy tolerance from the current
   * transformation
   */
  float
  logical_rendering_accuracy(void) const
  {
    return m_transformation_stack.back().logical_rendering_accuracy(m_render_accuracy);
  }

  /* Conveniance method to create and return a
   * RenderValue<ImageSampler> derived from
   * image() with the passed filter and
   * color transfer mode.
   */
  RenderValue<ImageSampler>
  create_image_sampler(enum filter_t filter) const;

  /* Return (and create on demand) a RenderValue<Transformation>
   * of the current transformation
   */
  RenderValue<Transformation>
  render_value_transformation(void) const
  {
    return m_transformation_stack.back().render_value(m_renderer, &m_pre_transformation);
  }

  /* Return the current transformation */
  const Transformation&
  transformation(void) const
  {
    return m_transformation_stack.back().transformation();
  }

  /* Add a draw command to this VirtualBuffer
   * \param tr transformation applied to draw
   * \param region pointer to region that covers the draw
   * \param item shader, vertex data and item data
   * \param material material and mask applied to draw
   * \param blend_mode blend mode to apply
   * \param framebuffer_copy if blend mode requires framebuffer-fetch
   *                         emulation, handle to the object holding
   *                         image data of pixel behind.
   */
  void
  draw_generic(RenderValue<Transformation> tr,
               const RenderSupportTypes::RectRegion *region,
               const RenderSupportTypes::Item<ColorItemShader> &item,
               const ItemMaterial &material,
               enum blend_mode_t blend_mode,
               RenderValue<EmulateFramebufferFetch> framebuffer_copy);

  void
  draw_generic(RenderValue<Transformation> tr,
               const RenderSupportTypes::RectRegion *region,
               const RenderSupportTypes::ColorItem &item,
               const ItemMaterial &material,
               enum blend_mode_t blend_mode,
               RenderValue<EmulateFramebufferFetch> framebuffer_copy);

  void
  draw_generic(RenderValue<Transformation> tr,
               const RenderSupportTypes::Item<MaskItemShader> &item,
               const ItemMask &clip,
               enum mask_item_shader_clip_mode_t clip_mode);

  void
  draw_generic(RenderValue<Transformation> tr,
               const RenderSupportTypes::Item<MaskItemShader> &item)
  {
    draw_generic(tr, item, ItemMask(), mask_item_shader_clip_cutoff);
  }

  void
  draw_generic(RenderValue<Transformation> tr,
               const RenderSupportTypes::Item<ShadowMapItemShader> &item);

  /* Copies commands from a src VirtualBuffer to this VirtualBuffer
   * \param src VirtualBuffer from which to copy commands
   * \param pixel_transformation_bb transformation from coordinates
   *                                of the bounding box bb to pixel
   *                                coordinates
   * \param bb bounding box query region
   * \param bb_pad amount by which to bb when computing what commands
   *               to take
   * \param delete_contained_cmds if true, delete those commands from
   *                              *this* VirtualBuffer that are completely
   *                              contained in bb (however, commands that
   *                              completely contained in bb with padding
   *                              but not by bb itself are not deleted).
   */
  void
  copy_commands(VirtualBuffer &src,
                RenderValue<Transformation> pixel_transformation_bb,
                const BoundingBox<float> &bb,
                float bb_pad,
                bool delete_contained_cmds);

  void
  copy_commands(VirtualBuffer &src,
                const BoundingBox<float> &pixel_bb,
                bool delete_contained_cmds)
  {
    copy_commands(src, RenderValue<Transformation>(), pixel_bb, 0.0f, delete_contained_cmds);
  }

  void
  add_occluder(RenderValue<Transformation> tr, const Rect &rect);

  /* Instead of blitting to the entire area of the backing image,
   * instead only blit to the named rects. The pointer of the
   * array is copied, so the object must stay alive until
   * Renderer::Implement::end(). This is accomplished by having the array
   * allocated/managed by Renderer::Implement::Storage.
   *
   * DANGER: these rects are in coordinates of the astral::Image(),
   *         thus one must take into accout the value of
   *         image_transformation_pixel() to get it to do the right
   *         thing.
   */
  void
  specify_blit_rects(const std::vector<RectT<int>> *rects)
  {
    ASTRALassert(!m_blit_rects);
    m_blit_rects = rects;
  }

  /* Generate a VirtualBufferProxy that corresponds to a child
   * buffer.
   */
  RenderSupportTypes::Proxy
  generate_child_proxy(const RelativeBoundingBox &logical_rect,
                       unsigned int pixel_slack,
                       RenderScaleFactor scale_factor);

  /* Generate a child buffer for mask rendering
   * \param logical_rect rect in logical coordinate for region
   * \param fill_rule fill rule of the mask rendering. If has the valuee
   *                  number_fill_rule, then is for generating a mask from
   *                  stroking and no image processing will be performed to
   *                  remove false edges
   * \param pixel_slack number of pixels by which to pad logical_rect
   * \param scale_factor scaling factor for rendering
   */
  RenderEncoderBase
  generate_child_buffer(const RelativeBoundingBox &logical_rect,
                        enum fill_rule_t fill_rule,
                        unsigned int pixel_slack,
                        RenderScaleFactor scale_factor,
                        ImageCreationSpec image_create_spec)
  {
    return generate_child_buffer(logical_rect,
                                 Implement::DrawCommandList::render_mask_image,
                                 image_blit_processing_for_mask(fill_rule),
                                 colorspace_linear, fill_rule,
                                 pixel_slack, scale_factor, image_create_spec);
  }

  /* Generate a child buffer for mask rendering
   * \param proxy specifies the geometry of the VirtualBuffer
   * \param fill_rule fill rule of the mask rendering. If has the valuee
   *                  number_fill_rule, then is for generating a mask from
   *                  stroking and no image processing will be performed to
   *                  remove false edges
   */
  RenderEncoderBase
  generate_buffer_from_proxy(RenderSupportTypes::Proxy proxy,
                             enum fill_rule_t fill_rule,
                             ImageCreationSpec image_create_spec)
  {
    return generate_buffer_from_proxy(proxy, Implement::DrawCommandList::render_mask_image,
                                      image_blit_processing_for_mask(fill_rule),
                                      colorspace_linear, fill_rule, image_create_spec);
  }

  /* Generate a child buffer for color rendering
   * \param logical_rect rect in logical coordinate for region
   * \param colorspace colorspace in which to render
   * \param pixel_slack number of pixels by which to pad logical_rect
   * \param scale_factor scaling factor for rendering
   */
  RenderEncoderBase
  generate_child_buffer(const RelativeBoundingBox &logical_rect,
                        enum colorspace_t colorspace,
                        unsigned int pixel_slack,
                        RenderScaleFactor scale_factor,
                        ImageCreationSpec image_create_spec)
  {
    return generate_child_buffer(logical_rect,
                                 Implement::DrawCommandList::render_color_image,
                                 image_processing_none,
                                 colorspace, number_fill_rule,
                                 pixel_slack, scale_factor,
                                 image_create_spec);
  }

  /* Generate a child buffer for color rendering
   * \param proxy specifies the geometry of the VirtualBuffer
   * \param colorspace colorspace in which to render
   */
  RenderEncoderBase
  generate_buffer_from_proxy(RenderSupportTypes::Proxy proxy,
                             enum colorspace_t colorspace,
                             ImageCreationSpec image_create_spec)
  {
    return generate_buffer_from_proxy(proxy, Implement::DrawCommandList::render_color_image,
                                      image_processing_none,
                                      colorspace, number_fill_rule,
                                      image_create_spec);
  }

  /* Returns a ClipGeoemetry allocated by Renderer::Implement::m_storage
   * for the purpose of a child
   * \param scale_factor scale factor of the child rendering
   * \param resrict_pixel_bb if non-null, futher intersect the clip geometry,
   *                         values are in pixel coordinates
   * \param logical_rect bounding box of region in current LOGICAL coordinates
   * \param pixel_slack number of pixels of padding of the output ClipGeoemtry's
   *                    pixel_rect()
   */
  Implement::ClipGeometryGroup
  child_clip_geometry(RenderScaleFactor scale_factor,
                      const RelativeBoundingBox &logical_rect, unsigned int pixel_slack);

  /* "Real" implementation of RenderEncoderBase::begin_pause_snapshot() */
  void
  begin_pause_snapshot(void);

  /* "Real" implementation of RenderEncoderBase::end_pause_snapshot() */
  void
  end_pause_snapshot(void);

  /* "Real" implementation of RenderEncoderBase::snapshot_paused() */
  int
  pause_snapshot_counter(void)
  {
    return m_pause_snapshot_counter;
  }

  /* "Real" implementation of RenderEncoderBase::snapshot_paused() */
  void
  pause_snapshot_counter(int v);

  /* Issue RenderBackend::draw_depth_rect() on the location of the
   * VirtualBuffer in the backing color buffer.
   */
  void
  draw_depth_rect(RenderBackend::UberShadingKey::Cookie uber_key_cookie, unsigned int f);

  unsigned int
  render_index(void)
  {
    return m_render_index;
  }

  ///////////////////////////////////////////////////////////////
  // only to be used when the VirtualBuffer generates a shadowmap
  const reference_counted_ptr<ShadowMap>&
  shadow_map(void)
  {
    ASTRALassert(type() == shadowmap_buffer);
    return m_shadow_map;
  }

  /* Once the rendering of this VirtualBuffer are completed,
   * this function is called to send a message to all the
   * VirtualBuffer that depend on this VirtualBuffer that
   * this VirtualBuffer's pixels are ready to be blitted
   * from the passed DepthBuffer; the location of where
   * in the depth buffer was already specified when rendering
   * started (held by m_location_in_depth_buffer). If the passed
   * DepthBuffer is nullptr, that indicates that the ShadowMap
   * was rendered directly to the ShadowMapAtlasBacking and
   * blit is not needed.
   */
  void
  render_performed_shadow_map(DepthStencilBuffer *src) const
  {
    render_performed_implement(nullptr, src);
  }

  /* to be only issue for rendering a shadowmap; specifies the location
   * within the depth buffer where it is to be rendered.
   */
  void
  location_in_depth_buffer(uvec2 location)
  {
    ScaleTranslate tr;

    ASTRALassert(type() == shadowmap_buffer);
    ASTRALassert(!m_render_scale_translate.valid());

    tr.m_scale.x() = static_cast<float>(m_shadow_map->dimensions());
    tr.m_scale.y() = 1.0f;
    tr.m_translate = vec2(location);

    m_location_in_depth_buffer = location;
    m_render_scale_translate = m_renderer.create_value(tr);
  }

  /* Returns true if and only if the render of this VirtualBuffer
   * reads from the ShadowMapAtlas
   */
  bool
  uses_shadow_map(void) const
  {
    return m_uses_shadow_map;
  }

  void
  add_dependency(const Image &image);

  /*!
   * Called by Renderer::Implement::end() on each VirtualBuffer.
   */
  void
  on_renderer_end(void);

  /*!
   * Called by Renderer::Implement::end_abort() on each VirtualBuffer.
   */
  void
  on_renderer_end_abort(void);

  /*!
   * Called by Renderer just before the VirtualBuffer is about to get rendered;
   * return routine_fail if the rendering is to be aborted.
   */
  enum return_code
  about_to_render_content(void);

  /* the Renderer that spawned this */
  Renderer::Implement &m_renderer;

  /* configuration */
  bool m_use_pixel_rect_tile_culling;
  float m_render_accuracy;
  bool m_use_sub_ubers;

  /* the transformation stack */
  std::vector<Renderer::Implement::CachedTransformation> &m_transformation_stack;

  /* value from Renderer::Implement::m_begin_cnt at time of "creation" */
  unsigned int m_renderer_begin_cnt;

private:
  /* implementation of DrawCommandList::OnAddDependency
   * to call VirtualBuffer::add_dependency()
   */
  class OnAddDependencyImpl:public Implement::DrawCommandList::OnAddDependency
  {
  public:
    OnAddDependencyImpl(VirtualBuffer *pthis):
      m_this(pthis)
    {}

    virtual
    void
    operator()(VirtualBuffer *b) const
    {
      ASTRALassert(b);
      ASTRALassert(m_this);
      m_this->add_dependency(*b);
    }

  private:
    VirtualBuffer *m_this;
  };

  static
  reference_counted_ptr<Image>
  make_partially_backed_image(Renderer::Implement &renderer,
                              unsigned int render_index,
                              ivec2 size, enum colorspace_t colorspace,
                              c_array<const vecN<range_type<int>, 2>> tile_regions)
  {
    ImageAtlas &image_atlas(renderer.m_engine->image_atlas());
    vecN<reference_counted_ptr<const ImageMipElement>, 1> mip;

    mip[0] = image_atlas.create_mip_element(uvec2(size), 1, tile_regions);
    return image_atlas.create_rendered_image(detail::RenderedImageTag(render_index), mip, colorspace);
  }

  /* Create a VirtualBuffer that inherits the clipping and
   * transformation of this VirtualBuffer
   */
  RenderEncoderBase
  generate_child_buffer(const RelativeBoundingBox &logical_rect,
                        enum Implement::DrawCommandList::render_type_t render_type,
                        enum image_blit_processing_t blit_processing,
                        enum colorspace_t colorspace,
                        enum fill_rule_t fill_rule,
                        unsigned int pixel_slack,
                        RenderScaleFactor scale_factor,
                        ImageCreationSpec image_create_spec);

  RenderEncoderBase
  generate_buffer_from_proxy(RenderSupportTypes::Proxy proxy,
                             enum Implement::DrawCommandList::render_type_t render_type,
                             enum image_blit_processing_t blit_processing,
                             enum colorspace_t colorspace,
                             enum fill_rule_t fill_rule,
                             ImageCreationSpec image_create_spec);

  static
  enum image_blit_processing_t
  image_blit_processing_for_mask(enum fill_rule_t stc_fill_rule)
  {
    return (stc_fill_rule == number_fill_rule) ?
      image_blit_direct_mask_processing :
      image_blit_stc_mask_processing;
  }

  enum downsampling_processing_t
  downsampling_processing(void) const
  {
    return downsampling_simple;
  }

  /* request to generate the next mipmap-level. */
  void
  generate_next_mipmap_level(void);

  /* Each of add_dependency() methods return a pointer
   * the the VirtualBuffer added to the dependency list;
   * a nullptr return value indicates that VirtualBuffer
   * was added to the list.
   */

  /* Mark that this VirtualBuffer requires the pixels of a
   * Image; this checks if the image is to be rendered
   * by a VirtualBuffer. It also marks regardless that the
   * Image is in use.
   */
  VirtualBuffer*
  add_dependency(const ImageID &ID);

  /* Mark that this VirtualBuffer requires the pixels of a
   * ShadowMap; this checks if the image is to be rendered by
   * a VirtualBuffer. It also marks regardless that the
   * ShadowMap is in use.
   */
  VirtualBuffer*
  add_dependency(const ShadowMapID &ID);

  /* Mark that this VirtualBuffer requires the pixels rendered
   * by another Virtualbuffer b.
   */
  VirtualBuffer*
  add_dependency(VirtualBuffer &b, bool allow_unfinised = false);

  /* Mark that this VirtualBuffer requires the pixels rendered
   * by the Virtualbuffer backing the RenderEncoderBase b.
   */
  VirtualBuffer*
  add_dependency(RenderEncoderBase b);

  /* Mark that this VirtualBuffer requires the pixels rendered
   * by the Virtualbuffer whose (unique) render_index is the
   * value passed
   */
  VirtualBuffer*
  add_dependency(unsigned int render_index);

  void
  render_performed_implement(ColorBuffer *color_src, DepthStencilBuffer *depth_src) const;

  void
  draw_generic_implement(RenderValue<Transformation> tr,
                         const RenderSupportTypes::RectRegion *region,
                         const Implement::DrawCommandVerticesShaders &item,
                         ItemData item_data,
                         const ItemMaterial &material,
                         BackendBlendMode blend_mode,
                         RenderValue<EmulateFramebufferFetch> framebuffer_copy,
                         enum mask_item_shader_clip_mode_t clip_mode = mask_item_shader_clip_cutoff);

  /* Break this VirtualBuffer into multiple sub-buffers for rendering.
   * \param region region over which to render
   */
  void
  realize_as_sub_buffers(RectT<int> region);

  /* Create the astral::Image that backs this VirtualBuffer */
  void
  create_backing_image(void);

  ///////////////////////////////////////////////
  // Member fields for all modes of VirtualBuffer
  ///////////////////////////////////////////////
  CreationTag m_creation_tag;

  enum type_t m_type;

  /* colorspace of rendering */
  enum colorspace_t m_colorspace;

  /* If valid indicates that rendering to the surface
   * started with a clear command of color not (0, 0, 0, 0)
   */
  RenderValue<Brush> m_clear_brush;

  /* true if issue_finish() was called */
  bool m_finish_issued;

  /* the index to feed Storage::virtual_buffer() to get this VirtualBuffer */
  unsigned int m_render_index;

  /* list of all VirtualBuffer  with multiplicity that depend on this VirtualBuffer */
  std::vector<VirtualBuffer*> *m_uses_this_buffer_list;

  /* list of all VirtualBuffers with multiplicity this VirtualBuffer depends on */
  std::vector<VirtualBuffer*> *m_dependency_list;

  /* the number of elements in m_dependency_list that need to be rendered to still */
  unsigned int m_remaining_dependencies;

  /* the number of elements in m_uses_this_buffer_list that have completed rendering */
  unsigned int m_users_that_completed_rendering;

  /* list of commands */
  Implement::DrawCommandList *m_command_list;

  /* ScaleTranslate that maps from pixel coordinates to where the VirtualBuffer
   * is rendered in either the scratch offscreen render target or a user passed
   * render target when used in Renderer::encoders_surface() where multiple encoders
   * are returned to render to a single surface.
   */
  RenderValue<ScaleTranslate> m_render_scale_translate;

  /* The geometry of the clipping region */
  Implement::ClipGeometryGroup m_clip_geometry;

  /* counter for begin_pause_snapshot() / end_pause_snapshot() */
  int m_pause_snapshot_counter;

  ////////////////////////////////////////////////////////////
  // These apply only for when the VirtualBuffer is rendering
  // to a RenderTarget
  astral::reference_counted_ptr<RenderTarget> m_render_target;
  u8vec4 m_render_target_clear_color;

  // only present if rendering to a sub-region of a RenderTarget
  SubViewport m_region;

  /////////////////////////////////////////////////////////
  // These apply only for when the VirtualBuffer is rendering
  // to an image

  /* Value to which to add to the z's of the drawing commands.
   * Used by Renderer to implement clipping via depth buffer.
   */
  unsigned int m_start_z;

  /* fill rule to apply to stc rendered content */
  enum fill_rule_t m_stc_fill_rule;

  /* m_clip_elements created lazily stored in a one-dimensional
   * array to make initialization simpler.
   */
  enum
    {
      num_clip_elements = number_mask_channel * number_mask_type
    };

  vecN<reference_counted_ptr<const RenderClipElement>, num_clip_elements> m_clip_elements;

  /* If non-null, indicates that when blitting from the
   * ColorBuffer to the ImageAtlas, only these rects are
   * to be blitted.
   */
  const std::vector<RectT<int>> *m_blit_rects;

  /* The sub-rect of m_image to which this VirtualBuffer renders,
   * this value is set in on_renderer_end() and its size is used
   * by renderer to determine the size of area needed for to render
   * the contents to the offscreen buffer.
   */
  RectT<int> m_render_rect;

  /* ClipWindow on the virtual buffer used to restrict its
   * render content to the area of m_image
   */
  RenderBackend::ClipWindowValue m_clip_window;

  /* Specification of when and how to create m_image; the size of
   * the image to create is specified by m_clip_geoemetry.bounding_geometry().image_size().
   */
  ImageCreationSpec m_image_create_spec;

  /* The eventual place where the rendered pixels of the
   * VirtualBuffer land
   */
  reference_counted_ptr<Image> m_image;

  /* max LOD level supported */
  unsigned int m_max_lod_supported;

  /* Array of VirtualBuffers whose m_image includes mipmaps
   * that is made on demand; to make life simple, element [I]
   * gives the image whose I'th LOD is defined. In particular,
   * [0] and [1] have the value of this->m_render_index
   */
  std::vector<VirtualBuffer*> *m_images_with_mips;

  /* Array of VirtualBuffers whose m_image is just the last
   * mipmap of m_images_with_mips
   */
  std::vector<VirtualBuffer*> *m_last_mip_only;

  /* is non-NULL only when m_images_with_mips->size() is odd;
   * it gives the VirtualBuffer that holds an astral::Image
   * whose mipmap chain length is exactly 1 and the image is
   * the pixels of the mipmap L in m_images_with_mips[L] where
   * L is m_images_with_mips->size() - 1. It is used to tell
   * the VirtualBuffer to do downsampling on its finish.
   */
  VirtualBuffer *m_dangling_mip_chain;

  /* the location and size within the ColorBuffer
   * of the pixel data.
   */
  Implement::WorkRoom::ImageBufferLocation m_location_in_color_buffer;

  /* STC data */
  vecN<Implement::STCData::VirtualArray, FillSTCShader::pass_count> m_stc;

  /////////////////////////////////////////////////////////////
  // These apply only when VirtualBuffer renders to a ShadowMap
  /////////////////////////////////////////////////////////////

  /* ShadowMap to which to render */
  reference_counted_ptr<ShadowMap> m_shadow_map;

  /* transformation that maps the light to (0, 0) */
  Transformation m_pre_transformation;

  /* Location where in depth buffer shadowmap was rendererd */
  uvec2 m_location_in_depth_buffer;

  /* in an ideal world, we render shadow maps directly to the
   * surface of the shadow map atlas. We cannot do this in GLES3
   * (and thus WebGL2) if the shadow map render uses another
   * shadow map because this would create a feedback loop. Thus,
   * we need to record if a shadow map render uses another shadow
   * map. In desktop GL, feedback loops are allowed as long as
   * the pixels read do not intersect with the pixel written until
   * glTextureBarrier() is called.
   */
  bool m_uses_shadow_map;
};

class astral::Renderer::VirtualBuffer::SorterCommon
{
public:
  typedef VirtualBuffer *VirtualBufferPtr;

  explicit
  SorterCommon(Renderer::Implement &renderer);

  c_array<const VirtualBufferPtr> m_buffers;
};

/*!
 * Used to sort VirtualBuffer objects by their area;
 * this is useful when assigning them a place in a real
 * render target where they are rendered because the
 * atlas allocation operates much better when allcoation
 * of regions is with the largest allocated first.
 */
class astral::Renderer::VirtualBuffer::AreaSorter:private astral::Renderer::VirtualBuffer::SorterCommon
{
public:
  explicit
  AreaSorter(Renderer::Implement &renderer):
    SorterCommon(renderer)
  {}

  bool
  operator()(unsigned int lhs, unsigned int rhs) const
  {
    int lhs_area(m_buffers[lhs]->area());
    int rhs_area(m_buffers[rhs]->area());

    ASTRALassert(m_buffers[lhs]->type() == VirtualBuffer::image_buffer || m_buffers[lhs]->type() == VirtualBuffer::sub_image_buffer);
    ASTRALassert(m_buffers[rhs]->type() == VirtualBuffer::image_buffer || m_buffers[rhs]->type() == VirtualBuffer::sub_image_buffer);

    /* We wish to also keep the VirtualBuffer objects in
     * the same order that they were created to improve
     * cache hit rate, this is why we also compare the
     * VirtualBuffer::m_render_index values.
     *
     * we want largest to come first, so reverse
     */
    return lhs_area > rhs_area
      || (lhs_area == rhs_area && m_buffers[lhs]->m_render_index < m_buffers[rhs]->m_render_index);
  }
};

/*!
 * Used to sort VirtualBuffer objects that render shadows
 * by their length.
 */
class astral::Renderer::VirtualBuffer::ShadowSizeSorter:private astral::Renderer::VirtualBuffer::SorterCommon
{
public:
  explicit
  ShadowSizeSorter(Renderer::Implement &renderer):
    SorterCommon(renderer)
  {}

  bool
  operator()(unsigned int lhs, unsigned int rhs) const
  {
    ASTRALassert(m_buffers[lhs]->type() == VirtualBuffer::shadowmap_buffer);
    ASTRALassert(m_buffers[lhs]->shadow_map());

    ASTRALassert(m_buffers[rhs]->type() == VirtualBuffer::shadowmap_buffer);
    ASTRALassert(m_buffers[rhs]->shadow_map());

    int lhs_size(m_buffers[lhs]->shadow_map()->dimensions());
    int rhs_size(m_buffers[rhs]->shadow_map()->dimensions());

    /* We wish to also keep the VirtualBuffer objects in
     * the same order that they were created to improve
     * cache hit rate, this is why we also compare the
     * VirtualBuffer::m_render_index values.
     *
     * we want largest to come first, so reverse
     */
    return lhs_size > rhs_size
      || (lhs_size == rhs_size && m_buffers[lhs]->m_render_index < m_buffers[rhs]->m_render_index);
  }
};

/*!
 * Sorter for sorting VirtualBuffers by format with
 * color rendering buffers coming first and mask
 * rendering buffers coming last.
 */
class astral::Renderer::VirtualBuffer::FormatSorter:private astral::Renderer::VirtualBuffer::SorterCommon
{
public:
  explicit
  FormatSorter(Renderer::Implement &renderer):
    SorterCommon(renderer)
  {}

  bool
  operator()(unsigned int lhs, unsigned int rhs) const
  {
    /* we want mask rendering to come last, so we sort
     * by is_mask_format().
     */
    ASTRALassert(is_rgba_format(lhs) || is_mask_format(lhs));
    ASTRALassert(is_rgba_format(rhs) || is_mask_format(rhs));

    /* We wish to also keep the VirtualBuffer objects in
     * the same order that they were created to improve
     * cache hit rate, this is why we also compare the
     * VirtualBuffer::m_render_index values;
     */
    return is_mask_format(lhs) < is_mask_format(rhs)
      || (is_mask_format(lhs) == is_mask_format(rhs) && m_buffers[lhs]->m_render_index < m_buffers[rhs]->m_render_index);
  }

  bool
  is_rgba_format(unsigned int idx) const
  {
    const Implement::DrawCommandList *p;

    p = m_buffers[idx]->command_list();
    ASTRALassert(p);
    return p->renders_to_color_buffer();
  }

  bool
  is_mask_format(unsigned int idx) const
  {
    const Implement::DrawCommandList *p;

    p = m_buffers[idx]->command_list();
    ASTRALassert(p);
    return p->renders_to_mask_buffer();
  }
};

class astral::Renderer::VirtualBuffer::IsMaskFormat:private astral::Renderer::VirtualBuffer::SorterCommon
{
public:
  explicit
  IsMaskFormat(Renderer::Implement &renderer):
    SorterCommon(renderer)
  {}

  bool
  operator()(unsigned int idx) const
  {
    const Implement::DrawCommandList *p;

    p = m_buffers[idx]->command_list();
    ASTRALassert(p);
    return p->renders_to_mask_buffer();
  }
};

/*!
 * Sorter for sorting VirtualBuffers by first shader used,
 * should only operate on color virtual buffers
 */
class astral::Renderer::VirtualBuffer::FirstShaderUsedSorter:private astral::Renderer::VirtualBuffer::SorterCommon
{
public:
  explicit
  FirstShaderUsedSorter(Renderer::Implement &renderer):
    SorterCommon(renderer)
  {}

  bool
  operator()(unsigned int lhs, unsigned int rhs) const
  {
    const Implement::DrawCommandList *lhs_list, *rhs_list;

    lhs_list = m_buffers[lhs]->command_list();
    rhs_list = m_buffers[rhs]->command_list();

    ASTRALassert(lhs_list && lhs_list->renders_to_color_buffer());
    ASTRALassert(rhs_list && rhs_list->renders_to_color_buffer());

    return lhs_list->first_shader_used() < rhs_list->first_shader_used();
  }
};

#endif

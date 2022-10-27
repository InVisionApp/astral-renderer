/*!
 * \file renderer.hpp
 * \brief file renderer.hpp
 *
 * Copyright 2019 by InvisionApp.
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

#ifndef ASTRAL_RENDERER_HPP
#define ASTRAL_RENDERER_HPP

#include <astral/util/memory_pool.hpp>
#include <astral/util/rect.hpp>
#include <astral/util/bounding_box.hpp>
#include <astral/path.hpp>
#include <astral/animated_path.hpp>
#include <astral/renderer/backend/render_backend.hpp>
#include <astral/renderer/relative_bounding_box.hpp>
#include <astral/renderer/combined_path.hpp>
#include <astral/renderer/mask_details.hpp>
#include <astral/renderer/item_material.hpp>
#include <astral/renderer/render_clip.hpp>
#include <astral/renderer/render_scale_factor.hpp>
#include <astral/renderer/render_target.hpp>
#include <astral/renderer/render_value.hpp>
#include <astral/renderer/brush.hpp>
#include <astral/renderer/render_engine.hpp>
#include <astral/renderer/item_path.hpp>
#include <astral/renderer/mask_usage.hpp>
#include <astral/renderer/fill_parameters.hpp>
#include <astral/renderer/stroke_parameters.hpp>
#include <astral/renderer/effect/effect.hpp>
#include <astral/text/text_item.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  class Renderer;
  class RenderClipElement;
  class RenderClipCombineResult;
  class RenderSupportTypes;
  class RenderEncoderBase;
  class RenderEncoderImage;
  class RenderEncoderMask;
  class RenderEncoderSurface;
  class RenderEncoderShadowMap;
  class RenderEncoderStrokeMask;
  class RenderClipNode;
  class RenderEncoderLayer;

  /*!
   * \brief
   * astral::Renderer represents Astral's interface to draw content. Internally
   * it relies on astral::RenderBackend and other objects to realize the
   * rendering in fashion that reduces GPU state thrashing.
   *
   * There are three main coordinate systems active at any time:
   *  - PixelCoordinates are the raw pixel coordinates relateive
   *    to RenderTarget::viewport_xy() of the surface passed to
   *    Renderer::encoder_surface() or the raw pixel coordinates
   *    of the astral::Image rendered to from any of
   *    the overloads of RenderEncoderBase::encoder_image(),
   *    RenderEncoderBase::encoder_mask() or
   *    RenderEncoderBase::encoder_shadow_map()
   *  - SurfaceCoordinates are the raw pixel coordinates of the
   *    astral::Image rendered to that are spawned by any of
   *    the overloads of RenderEncoderBase::encoder_mask_relative(),
   *    or RenderEncoderBase::encoder_image_relative(). These
   *    coordinates in addition to being a translation of
   *    PixelCoordinates may also have a scaling factor applied
   *    because the render result of them does not need to be
   *    at the same resolution as the final surface render.
   *    The method RenderEncoderBase::render_scale_factor() gives
   *    the scaling factor between SurfaceCoordinates and PixelCoordinates
   *  - LogicalCoordinates are the coordinates in which the items
   *    to draw are in. The transformation from logical coordinates
   *    to PixelCoordinates is provided by RenderEncoderBase::transformation()
   *    and manipulated by RenderEncoderBase::save_transformation(),
   *    RenderEncoderBase::translate(), RenderEncoderBase::scale(),
   *    RenderEncoderBase::rotate(), RenderEncoderBase::concat()
   *    and RenderEncoderBase::restore_transformation().
   *  .
   *
   * The purpose of PixelCoordinate is that it is the coordinate system
   * "anchor" and corresponds to the presented content. The purpose of
   * SurfaceCoordinates is to allow for content that needs to be rendered
   * to an offscreen buffer to be rendered at a lower resolution.
   *
   * The pixel "pipeline" or Renderer is as follows:
   *  - The vertex shader of the astral::ItemShader emits logical coordinates.
   *    If ItemMaterial::m_material_transformation_logical is valid, then
   *    that transformation is applied to the logical coordinate to produces
   *    material coordinates; otherwise, material and logical coordinates are
   *    the same. These material coordinates are fed to the vertex shader of
   *    the material shader.
   *  - The fragment shader of the astral::ItemShader computes a pre-multiplied
   *    by alpha color value and a coverage value. The coverage and color value
   *    are passed the astral::Material which will emit a new color
   *    and coverage value; for example an astral::Brush leaves the coverage
   *    value as is, but modulates the color by the color of the brush.
   *  - The coverage and color value computed above are sent blending
   *  .
   *
   * The clipping in Renderer is as follows:
   *  - The first stage of clipping is clip-equations clipping. For encoders
   *    returned by Renderer (via Renderer::encoder_image() or encoder_surface()),
   *    this clipping is the rectangle realized by the surface. For encoders
   *    coming from RenderEncoderBase::encoder_image_relative() (and similair
   *    methods) the clip-equations clipping is the intersection of the parent
   *    encoder and the bounding box passed. The tiles of the backing image
   *    outside of that region may not even be backed and the contents of such
   *    tiles is undefined.
   *  - The second stage of clipping comes from ItemMaterial::m_clip which can
   *    vary draw to draw. The clipping is applied to the item drawn and pixels
   *    that are clipped are unaffected by the draw.
   *  .
   *
   * All color content is realized as pre-multiplied by alpha, i.e. the
   * render to an astral::RenderTarget via an astral::RenderEncoderSurface
   * will be with alpha pre-multiplied as will the pixels in each of the
   * astral::Image objects renderer to by an astral::RenderEncoderImage.
   *
   * How blending and modulation occurs is controllable via the colorspace
   * argument for those methods that produce an astral::RenderEncoderSurface
   * or astral::RenderEncoderImage. When the value of the colorspace argument
   * is a valie C, then all color values from images and gradients are converted
   * to that space C and then blending and modulation are done in the space
   * C. For classic 2D rendering (as found on the Web and other UI renderers),
   * the value of C is astral::colorspace_srgb. For 3D rendering the value of
   * C should be astral::colorspace_linear.
   */
  class Renderer:public reference_counted<Renderer>::non_concurrent
  {
  public:
    /*!
     * Enumeration of render statistics of Renderer
     */
    enum renderer_stats_t:uint32_t
      {
        /*!
         * The number of virtual buffers issued;
         * virtual buffers are rendered together
         * so that fewer render target changes are
         * needed.
         */
        number_virtual_buffers,

        /*!
         * The number of virtual buffers issued
         * that actually had pixels
         */
        number_non_degenerate_virtual_buffers,

        /*!
         * The number of virtual buffers for color
         * rendering issued that actually had pixels
         */
        number_non_degenerate_color_virtual_buffers,

        /*!
         * The number of virtual buffers for mask
         * rendering issued that actually had pixels
         */
        number_non_degenerate_mask_virtual_buffers,

        /*!
         * The number of virtual buffers for shadowmap
         * rendering issued that actually had pixels
         */
        number_non_degenerate_shadowmap_virtual_buffers,

        /*!
         * Number of surfaces spawned for emulating framebuffer fetch.
         */
        number_emulate_framebuffer_fetches,

        /*!
         * The number of pixels used across color
         * virtual buffers; this also includes the
         * pixel skipped for empty tiles.
         */
        number_color_virtual_buffer_pixels,

        /*!
         * Number of pixels not realized from sparse color
         * buffer rendering because Renderer detected that
         * no draws intersect a set of tiles.
         */
        number_skipped_color_buffer_pixels,

        /*!
         * The number of pixels used across mask
         * virtual buffers
         */
        number_mask_virtual_buffer_pixels,

        /*!
         * The number of pixels used across all
         * virtual buffers
         */
        number_virtual_buffer_pixels,

        /*!
         * The number of virtual buffers whose backing color images
         * could not be allocated
         */
        number_virtual_buffer_backing_allocation_failed,

        /*!
         * The number of pixels skipped using sparse
         * filling
         */
        number_tiles_skipped_from_sparse_filling,

        /*!
         * Number of pixels blitted from virtual buffers
         * to the image atlas.
         */
        number_pixels_blitted,

        /*!
         * The number of *OFFSCREEN* render targets
         * used to render the virtual buffers
         */
        number_offscreen_render_targets,

        /*!
         * The number of astral::Vertex values streamed
         * as vertices
         */
        number_vertices_streamed,

        /*!
         * The number of four tuples of 32-bit values streamed
         * as static data
         */
        number_static_u32vec4_streamed,

        /*!
         * The number of four tuples of 16-bit values streamed
         * as fp16 static data
         */
        number_static_u16vec4_streamed,

        /*!
         * Total number of commands copied.
         */
        number_commands_copied,

        /*!
         * The number of curves mapped on CPU for sparse filling
         */
        number_sparse_fill_curves_mapped,

        /*!
         * The number of contours mapped on CPU for sparse filling
         */
        number_sparse_fill_contours_mapped,

        /*!
         * The number of curves clipped on CPU for sparse filling
         */
        number_sparse_fill_curves_clipped,

        /*!
         * The number of contours clipped on CPU for sparse filling
         */
        number_sparse_fill_contours_clipped,

        /*!
         * The number of contours that were mapped but after mapping
         * were culled during sparse filling
         */
        number_sparse_fill_late_culled_contours,

        /*!
         * The sum over all contour C from sparse filling
         * of the number of subrects affected by C
         */
        number_sparse_fill_subrects_clipping,

        /*!
         * The sum over all contour C from sparse filling
         * of the number of subrects that are affected by
         * C but had their winding offsets computed together
         * because there was no curves of C hitting the
         * continuous sub-block of rects
         */
        number_sparse_fill_subrect_skip_clipping,

        /*!
         * The sum over all contour C from sparse filling
         * that after clipping the virtual mask buffer, had
         * all sub-rects not hit by any curve of C.
         */
        number_sparse_fill_contour_skip_clipping,

        /*!
         */
        number_sparse_fill_awkward_fully_clipped_or_unclipped,

        /*!
         * Number of mapped contours that encountered
         * an error in clipping.
         */
        number_sparse_fill_clipping_errors,

        number_renderer_stats
      };

    /*!
     * Class to save how Renderer used the offsceen buffer
     */
    class OffscreenBufferAllocInfo
    {
    public:
      /*!
       * Number of total offscreen sessions
       */
      unsigned int
      number_offscreen_sessions(void) const
      {
        return m_sessions.size();
      }

      /*!
       * Dimensions of the named session
       */
      ivec2
      session_size(unsigned int session) const
      {
        ASTRALassert(session < m_sessions.size());
        return m_sessions[session].m_session_size;
      }

      /*!
       * Rects for a given session
       */
      c_array<const RectT<int>>
      sessions_rects(unsigned int session) const
      {
        c_array<const RectT<int>> all_rects;

        all_rects = make_c_array(m_rects);
        ASTRALassert(session < m_sessions.size());
        return all_rects.sub_array(m_sessions[session].m_begin, m_sessions[session].m_count);
      }

      /*!
       * Returns the smallest size possible for the value
       * returned by session_size().
       */
      static
      ivec2
      session_smallest_size(void);

      /*!
       * Returns the largest size possible for the value
       * returned by session_size().
       */
      static
      ivec2
      session_largest_size(void);

    private:
      friend class Renderer;

      class Session
      {
      public:
        explicit
        Session(unsigned int begin, ivec2 sz):
          m_begin(begin),
          m_count(0),
          m_session_size(sz)
        {}

        unsigned int m_begin, m_count;
        ivec2 m_session_size;
      };

      void
      clear(void)
      {
        m_sessions.clear();
        m_rects.clear();
      }

      void
      begin_offscreen_session(ivec2 offscreen_size)
      {
        m_sessions.push_back(Session(m_rects.size(), offscreen_size));
      }

      void
      add_rect(const RectT<int> &R)
      {
        ASTRALassert(!m_sessions.empty());
        m_rects.push_back(R);
        ++m_sessions.back().m_count;
      }

      std::vector<Session> m_sessions;
      std::vector<RectT<int>> m_rects;
    };

    /*!
     * Create an astral::Renderer object
     */
    static
    reference_counted_ptr<Renderer>
    create(RenderEngine &engine);

    virtual
    ~Renderer()
    {}

    /*!
     * Returne the astral::RenderEngine passed at ctor
     */
    RenderEngine&
    render_engine(void) const;

    /*!
     * Returns the stats returned on the last call to end().
     * Gives undefined results if within a begin()/end() pair.
     */
    c_array<const unsigned int>
    last_stats(void) const;

    /*!
     * Returns the value that astral::Renderer will initialize
     * RenderEncoderBase::render_accuracy() for the encoder
     * returned by begin(), encoder_image() and encoder_surface().
     * Initial value is 0.5.
     */
    float
    default_render_accuracy(void) const;

    /*!
     * Set the value returned by default_render_accuracy(void) const
     */
    void
    default_render_accuracy(float v);

    /*!
     * Returns the value that astral::Renderer will initialize
     * RenderEncoderImage::use_pixel_rect_tile_culling()
     * for the encoders rencoder_image(). Initial value is false.
     */
    bool
    default_use_pixel_rect_tile_culling(void) const;

    /*!
     * Set the value returned by default_use_pixel_rect_tile_culling(void) const
     */
    void
    default_use_pixel_rect_tile_culling(bool v);

    /*!
     * Create a handle for a "compiled" astral::Transformation
     * that can be used only by this astral::Renderer within
     * the current begin() / end()
     * pair.
     */
    RenderValue<Transformation>
    create_value(const Transformation &tr)
    {
      return backend().create_value(tr);
    }

    /*!
     * Create a handle for a "compiled" astral::ScaleTranslate
     * that can be used only by this astral::Renderer within
     * the current begin() / end()
     * pair.
     */
    RenderValue<ScaleTranslate>
    create_value(const ScaleTranslate &tr)
    {
      return backend().create_value(tr);
    }

    /*!
     * Create a handle for a "compiled" astral::GradientTransformation
     * that can be used only by this astral::Renderer within
     * the current begin() / end()
     * pair.
     */
    RenderValue<GradientTransformation>
    create_value(const GradientTransformation &image_transformation)
    {
      return backend().create_value(image_transformation);
    }

    /*!
     * Create a handle for a "compiled" astral::ImageSampler
     * that can be used only by this astral::Renderer within
     * the current begin() / end() pair.
     */
    RenderValue<ImageSampler>
    create_value(const ImageSampler &image)
    {
      return backend().create_value(image);
    }

    /*!
     * Create a handle for a "compiled" astral::Gradient
     * that can be used only by this astral::Renderer within
     * the current begin() / end() pair.
     */
    RenderValue<Gradient>
    create_value(const Gradient &gradient)
    {
      return backend().create_value(gradient);
    }

    /*!
     * Create a handle for a "compiled" astral::Brush
     * that can be used only by this astral::Renderer within
     * the current begin() / end() pair.
     */
    RenderValue<Brush>
    create_value(const Brush &brush)
    {
      return backend().create_value(brush);
    }

    /*!
     * Create a handle for a "compiled" astral::ShadowMap
     * that can be used only by this astral::Renderer within
     * the current begin() / end() pair.
     */
    RenderValue<const ShadowMap&>
    create_value(const ShadowMap &shadow_map)
    {
      return backend().create_value(shadow_map);
    }

    /*!
     * Recreate a RenderValue from the value returned
     * by RenderValue::cookie(). NOTE: it is an error
     * if the the cookie value did not come from a
     * RenderValue made within the current begin()/end()
     * frame.
     */
    template<typename T>
    RenderValue<T>
    render_value_from_cookie(uint32_t cookie)
    {
      return backend().render_value_from_cookie<T>(cookie);
    }

    /*!
     * Create a handle for a "compiled" astral::ItemData
     * that can be used only by this astral::Renderer within
     * the current begin() / end() pair, passing an array
     * of astral::ItemDataValueMapping::entry values that
     * describe if/how values within the item data are
     * interpreted as astral::RenderValue references.
     */
    ItemData
    create_item_data(c_array<const gvec4> value,
                     c_array<const ItemDataValueMapping::entry> item_data_value_map,
                     const ItemDataDependencies &dependencies = ItemDataDependencies())
    {
      return backend().create_item_data(value, item_data_value_map, dependencies);
    }

    /*!
     * Equivalent to
     * \code
     * create_item_data(value, map.data(), dependencies)
     * \endcode
     */
    ItemData
    create_item_data(c_array<const gvec4> value,
                     const ItemDataValueMapping &item_data_value_map,
                     const ItemDataDependencies &dependencies = ItemDataDependencies())
    {
      return create_item_data(value, item_data_value_map.data(), dependencies);
    }

    /*!
     * Equivalent to
     * \code
     * create_item_data(value, c_array<const ItemDataValueMapping::entry>(), dependencies)
     * \endcode
     */
    ItemData
    create_item_data(c_array<const gvec4> value, enum no_item_data_value_mapping_t,
                     const ItemDataDependencies &dependencies = ItemDataDependencies())
    {
      return create_item_data(value, c_array<const ItemDataValueMapping::entry>(),
                              dependencies);
    }

    /*!
     * Overload of create_item_data() passing an astral::vecN of astral::ImageID
     * values. Equivalent to
     * \code
     * c_array<const ImageID> ii(dependencies);
     * c_array<const ShadowMapID> ss;
     * create_item_data(value, v, ItemDataDependencies(ii, ss));
     * \endcode
     */
    template<size_t N, typename V>
    ItemData
    create_item_data(c_array<const gvec4> value, const V &v, const vecN<ImageID, N> &dependencies)
    {
      c_array<const ImageID> ii(dependencies);
      c_array<const ShadowMapID> ss;
      return create_item_data(value, v, ItemDataDependencies(ii, ss));
    }

    /*!
     * Overload of create_item_data() passing an astral::vecN of astral::ShadowMapID
     * values. Equivalent to
     * \code
     * c_array<const ImageID> ii;
     * c_array<const ShadowMapID> ss(dependencies);
     * create_item_data(value, v, ItemDataDependencies(ii, ss));
     * \endcode
     */
    template<size_t N, typename V>
    ItemData
    create_item_data(c_array<const gvec4> value, const V &v, const vecN<ShadowMapID, N> &dependencies)
    {
      c_array<const ImageID> ii;
      c_array<const ShadowMapID> ss(dependencies);
      return create_item_data(value, v, ItemDataDependencies(ii, ss));
    }

    /*!
     * Overload of create_item_data() passing an astral::vecN of astral::ImageID
     * and an astral::vecN of astral::ShadowMapID values. Equivalent to
     * \code
     * c_array<const ImageID> ii(image_dependencies);
     * c_array<const ShadowMapID> ss(shadow_dependencies);
     * create_item_data(value, v, ItemDataDependencies(ii, ss));
     * \endcode
     */
    template<size_t N, size_t M, typename V>
    ItemData
    create_item_data(c_array<const gvec4> value, const V &v,
                     const vecN<ImageID, N> &image_dependencies,
                     const vecN<ShadowMapID, M> &shadow_dependencies)
    {
      c_array<const ImageID> ii(image_dependencies);
      c_array<const ShadowMapID> ss(shadow_dependencies);
      return create_item_data(value, v, ItemDataDependencies(ii, ss));
    }

    /*!
     * Begin rendering to an offscreen buffer that is used as a mask.
     * May only be called inside of a begin() / end() pair.
     * \param size size of the mask to render.
     * \returns astral::RenderEncoderMask to render the offscreen buffer
     */
    RenderEncoderMask
    encoder_mask(ivec2 size);

    /*!
     * Begin rendering to an offscreen buffer.
     * May only be called inside of a begin() / end() pair.
     * \param size size of the mask to render.
     * \param colorspace the colorspace in which to render the content
     * \returns astral::RenderEncoderImage to render the offscreen buffer
     */
    RenderEncoderImage
    encoder_image(ivec2 size, enum colorspace_t colorspace);

    /*!
     * Begin rendering to an offscreen buffer in the same colorspace as
     * passed to begin().  May only be called inside of a begin() / end()
     * pair.
     * \param size size of the mask to render.
     * \returns astral::RenderEncoderImage to render the offscreen buffer
     */
    RenderEncoderImage
    encoder_image(ivec2 size);

    /*!
     * Begin rendering to a -surface-. Contents are not realized
     * on the surface until end() is called. The render target is
     * always cleared over the area of RenderTarget::viewport()
     * before rendering.
     * \param rt what astral::RenderTarget to which to render
     * \param colorspace the colorspace at which to render content
     * \param clear_color color to which to clear rt in the color
     *                    space named by colorspace
     * \returns an astral::RenderEncoderSurface which draws to
     *          the astral::RenderTarget rt
     */
    RenderEncoderSurface
    encoder_surface(RenderTarget &rt, enum colorspace_t colorspace = colorspace_srgb,
                    u8vec4 clear_color = u8vec4(0u, 0u, 0u, 0u));

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * begin(rt, colorspace, clear_color.m_value);
     * \endcode
     */
    template<enum colorspace_t colorspace>
    RenderEncoderSurface
    encoder_surface(RenderTarget &rt, FixedPointColor<colorspace> clear_color);

    /*!
     * Begin a rendering session. Doing so also guarantees that the
     * 3D API state is set correctly for use by Astral. It is an error
     * to modify the 3D API state within a begin() / end() pair.
     * \param default_image_encoder_colorspace colorspace to use
     *                                         for RenderEncoderImage
     *                                         values returned by
     *                                         encoder_image(ivec2).
     */
    void
    begin(enum colorspace_t default_image_encoder_colorspace);

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * begin(colorspace);
     * encoder_surface(rt, colorspace, clear_color);
     * \endcode
     * \param rt what astral::RenderTarget to which to render
     * \param colorspace the colorspace at which to render content
     * \param clear_color color to which to clear rt in the color
     *                    space named by colorspace
     * \returns an astral::RenderEncoderSurface which draws to
     *          the astral::RenderTarget rt
     */
    RenderEncoderSurface
    begin(RenderTarget &rt, enum colorspace_t colorspace = colorspace_srgb,
          u8vec4 clear_color = u8vec4(0u, 0u, 0u, 0u));

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * begin(rt, colorspace, clear_color.m_value);
     * \endcode
     */
    template<enum colorspace_t colorspace>
    RenderEncoderSurface
    begin(RenderTarget &rt, FixedPointColor<colorspace> clear_color);

    /*!
     * Indicates to end drawing from the matching begin()
     * call. Actual drawing does not commence until end()
     * is called. The retur array will keep its values
     * until the next call to begin() upon which the values
     * are no longer valid.
     * \param out_alloc_info if non-null, location to which
     *                       to  record the offscreen buffer
     *                       allocation
     * \returns an astral::c_array of the rendering stats
     *          of the begin()/end() pair ended. The function
     *          stats_labels() gives a C-string for the
     *          meaning of each of the rendering stats
     */
    c_array<const unsigned int>
    end(OffscreenBufferAllocInfo *out_alloc_info = nullptr);

    /*!
     * Indicates to end drawing from the matching begin()
     * call but to NOT draw ANY content specified from the
     * matching begin(). Any astral::Image objects made
     * during an aborted session will have undefined pixel
     * color values and each astral::RenderTarget will not
     * be rendered to or even cleared.
     */
    c_array<const unsigned int>
    end_abort(void);

    /*!
     * Returns the labels of the render stats returned by end().
     */
    c_array<const c_string>
    stats_labels(void) const;

    /*!
     * Given a \ref renderer_stats_t, value return
     * an index into teh arrays returned by end()
     * and stats_labels() of where the stat is
     * located.
     */
    unsigned int
    stat_index(enum renderer_stats_t st) const;

    /*!
     * Given a RenderBackend::render_backend_stats_t
     * value, return an index into the arrays returned
     * by end() and stats_labels() of where the stat
     * is located.
     */
    unsigned int
    stat_index(enum RenderBackend::render_backend_stats_t st) const;

    /*!
     * Given a RenderBackend::DerivedStat value,
     * return an index into the arrays returned
     * by end() and stats_labels() of where the stat
     * is located.
     */
    unsigned int
    stat_index(RenderBackend::DerivedStat st) const;

    /*!
     * Override rendering properties, it is an error to
     * call within a begin()/end() pair.
     */
    void
    overridable_properties(const RenderEngine::OverridableProperties &props);

    /*!
     * Return current value of overridable rendering properties
     */
    const RenderEngine::OverridableProperties&
    overridable_properties(void) const;

    /*!
     * Query if a custom draw will be blended correctly if it overdraws itself.
     * \param emits_partially_covered_fragments if the shader combined with the material
     *                                          and clipping mask emit partially covered
     *                                          pixels
     * \param blend_mode blend mode to query
     */
    bool
    custom_draw_can_overdraw_itself(bool emits_partially_covered_fragments, enum blend_mode_t blend_mode) const;

    /*!
     * Overload of custom_draw_can_overdraw_itself() taking a material and item shader
     * \param material ItemMaterial of the query
     * \param shader astral::ColorItemShader of the query
     * \param blend_mode blend mode to query
     */
    bool
    custom_draw_can_overdraw_itself(const ItemMaterial &material,
                                    const ColorItemShader &shader,
                                    enum blend_mode_t blend_mode) const
    {
      bool e;

      e = shader.properties().m_emits_partially_covered_fragments
        || material.emits_partial_coverage();
      return custom_draw_can_overdraw_itself(e, blend_mode);
    }

  private:
    friend class RenderClipElement;
    friend class RenderClipCombineResult;
    friend class RenderSupportTypes;
    friend class RenderEncoderBase;
    friend class RenderEncoderImage;
    friend class RenderEncoderMask;
    friend class RenderEncoderShadowMap;
    friend class RenderEncoderStrokeMask;
    friend class RenderEncoderLayer;
    friend class RenderClipNode;

    class Implement;
    class VirtualBuffer;

    RenderBackend&
    backend(void);

    Implement&
    implement(void);

    const Implement&
    implement(void) const;
  };

  /*!
   * Class to encapsulate various types and enums
   * used by astral::RenderEncoderBase and
   * astral::Painter.
   */
  class RenderSupportTypes
  {
  public:
    /*!
     * Flags values to pass to clip_node_pixel() and
     * clip_node_logical().
     */
    enum clip_node_flags_t:uint32_t
      {
        /*!
         * Inidicates no content to be rendered
         */
        clip_node_none = 0u,

        /*!
         * Indicates that clipped-in content is to be rendered
         */
        clip_node_clip_in = 1u,

        /*!
         * Indicates that clipped-out content is to be rendered
         */
        clip_node_clip_out = 2u,

        /*!
         * Indicates that both clip-in and clip-out content is to be rendered
         */
        clip_node_both = 3u,
      };

    /*!
     * Class to specify the vertices and shader to apply
     * to those vertices to draw an item.
     * \tparam T one of astral::ItemShader, astral::ColorItemShader,
     *           astral::MaskItemShader or astral::ShadowMapShader
     */
    template<typename T>
    class Item
    {
    public:
      /*!
       * Ctor.
       * \param shader shader with which
       * \param vertex_data vertex data to draw
       * \param ranges array of ranges into the vertics of VertexData,
       *               i.e. a set of ranges within the range
       *               [0, vertex_data.number_vertices()); the contents
       *               are NOT copied, thus the caller must make sure
       *               that the backing is valid (and unchanging) for
       *               the lifetime of the DrawGenericValues object
       * \param item_data ItemData passed to the shader
       */
      Item(const T &shader,
           const VertexData &vertex_data,
           c_array<const range_type<int>> ranges,
           ItemData item_data = ItemData()):
        m_shader(shader),
        m_vertex_data(vertex_data),
        m_draw_all(false),
        m_vertex_data_ranges(ranges),
        m_item_data(item_data)
      {}

      /*!
       * Ctor.
       * \param shader shader with which
       * \param vertex_data vertex data to draw
       * \param ranges array of ranges into the vertics of VertexData,
       *               i.e. a set of ranges within the range
       *               [0, vertex_data.number_vertices()); the contents
       *               are NOT copied, thus the caller must make sure
       *               that the backing is valid (and unchanging) for
       *               the lifetime of the DrawGenericValues object
       * \param item_data ItemData passed to the shader
       */
      Item(const T &shader,
           ItemData item_data,
           const VertexData &vertex_data,
           c_array<const range_type<int>> ranges):
        m_shader(shader),
        m_vertex_data(vertex_data),
        m_draw_all(false),
        m_vertex_data_ranges(ranges),
        m_item_data(item_data)
      {}

      /*!
       * Ctor.
       * \param shader shader with which
       * \param vertex_data vertex data to draw; all vertices in the
       *                    passed astral::VertexData will be sent
       *                    to the GPU.
       * \param item_data ItemData passed to the shader
       */
      Item(const T &shader,
           const VertexData &vertex_data,
           ItemData item_data = ItemData()):
        m_shader(shader),
        m_vertex_data(vertex_data),
        m_draw_all(true),
        m_item_data(item_data)
      {}

      /*!
       * Ctor.
       * \param shader shader with which
       * \param vertex_data vertex data to draw; all vertices in the
       *                    passed astral::VertexData will be sent
       *                    to the GPU.
       * \param item_data ItemData passed to the shader
       */
      Item(const T &shader,
           ItemData item_data,
           const VertexData &vertex_data):
        m_shader(shader),
        m_vertex_data(vertex_data),
        m_draw_all(true),
        m_item_data(item_data)
      {}

      /*!
       * Returns true if this Item does not process any vertices
       */
      bool
      empty(void) const
      {
        /* TODO: this is not actually tight since it does not
         *       handle the cases where m_vertex_data has zero
         *       vertices or if all entries of m_vertex_data_ranges
         *       are empty.
         */
        return !m_draw_all && m_vertex_data_ranges.empty();
      }

      /*!
       * Shader with which to draw
       */
      const T &m_shader;

      /*!
       * Vertex data to draw
       */
      const VertexData &m_vertex_data;

      /*!
       * If true, the all data of \ref m_vertex_data is sent
       * to the GPU. If false, the ranges specified by \ref
       * m_vertex_data_ranges is sent to the GPU.
       */
      bool m_draw_all;

      /*!
       * If \ref m_draw_all is false, then provides an array
       * of ranges into [0, m_vertex_data.number_vertices())
       * to be drawn. If \ref m_draw_all is true, this field
       * is ignored. It is an error if any elements of the
       * passed array are not contained by the range of
       * vertices of \ref m_vertex_data.
       */
      c_array<const range_type<int>> m_vertex_data_ranges;

      /*!
       * Optional astral::ItemData to pass to the shader
       */
      ItemData m_item_data;
    };

    /*!
     * Analogous to \ref Item, but the vertex data
     * is exactly the vertex data for rectangle shading.
     * The vertex data fed is the following:
     *  - Vertex::m_data[0].f --> x-relative position, value is 0 or 1
     *  - Vertex::m_data[1].f --> y-relative position, value is 0 or 1
     */
    class RectItem
    {
    public:
      /*!
       * Ctor.
       */
      RectItem(const ColorItemShader &shader,
               ItemData item_data = ItemData()):
        m_shader(shader),
        m_item_data(item_data)
      {}

      /*!
       * shader with which to draw
       */
      const ColorItemShader &m_shader;

      /*!
       * Optional astral::ItemData to pass to the shader
       */
      ItemData m_item_data;
    };

    /*!
     * A ColorItem is similiar to an Item<ColorItemShader>
     * except that it allows to use multiple \ref
     * ColorItemShader objects and multiple \ref VertexData
     * objects
     */
    class ColorItem
    {
    public:
      /*!
       * A SubItem represents a choice of shader and vertex data
       */
      class SubItem
      {
      public:
        /*!
         * Index into ColorItem::m_shaders of which shader
         * to apply to the vertices
         */
        unsigned int m_shader;

        /*!
         * Index into ColorItem::m_vertex_datas of the source
         * of the vertex data
         */
        unsigned int m_vertex_data;

        /*!
         * Range into [0, VertexData::number_vertices())
         * giving the vertices to process.
         */
        range_type<int> m_vertices;
      };

      /*!
       * Returns true if one of the astral::ColorItemShader of \ref m_shaders has
       * that ColorItemShader::Properties::m_emits_partially_covered_fragments
       * is true.
       */
      bool
      emits_partially_covered_fragments(void) const;

      /*!
       * Returns true if one of the astral::ColorItemShader of \ref m_shaders
       * has that ColorItemShader::Properties::m_emits_transparent_fragments
       * is true.
       */
      bool
      emits_transparent_fragments(void) const;

      /*!
       * List of shaders used by the draw; cannot be
       * an empty array and each element of the array
       * must be non-null. It is strongly advised that
       * each element is unique.
       */
      c_array<const pointer<const ColorItemShader>> m_shaders;

      /*!
       * List of vertex data objects used by the draw;
       * cannot be an empty array and each element of
       * the array must be non-null.
       */
      c_array<const pointer<const VertexData>> m_vertex_datas;

      /*!
       * List of sub-items of this \ref ColorItem.
       */
      c_array<const SubItem> m_sub_items;

      /*!
       * astral::ItemData to pass to each shader
       */
      ItemData m_item_data;
    };

    /*!
     * A RectRegion specifies a region that tightly
     * bounds the region covered by a \ref RectItem
     * or an \ref Item<\ref ColorItemShader>
     */
    class RectRegion
    {
    public:
      /*!
       * The rectangular region that tighly bounds
       * the area covered by the RectItem. The
       * rect is in currenty logical coordinates of
       * the draw.
       */
      BoundingBox<float> m_rect;
    };

    /*!
     * A Proxy represents the computation to generate the
     * backing for an astral::RenderEncoderImage for the
     * region specified by an astral::RelativeBoundingBox.
     * Through it, one can query what is the size of the
     * backing image, the region that it covers and the
     * transformation from pixel coordinates to image
     * coordinates.
     */
    class Proxy
    {
    public:
      Proxy(void):
        m_data(nullptr)
      {}

      /*!
       * Returns the bounding box in pixel-coordinates
       * of the rendering region.
       */
      BoundingBox<float>
      pixel_bounding_box(void) const;

      /*!
       * Gives the astral::ScaleTranslate mapping from pixel_bounding_box()
       * to the image whose size is given by image_size().
       */
      ScaleTranslate
      image_transformation_pixel(void) const;

      /*!
       * Returns the size of the image needed to back an astral::RenderEncoderImage
       * specified by this RenderEncoderBase::Proxy.
       */
      ivec2
      image_size(void) const;

    private:
      friend class RenderEncoderBase;
      friend class Renderer;

      class Backing;

      Proxy(Backing *p):
        m_data(p)
      {}

      Backing *m_data;
    };

    /*!
     * \brief
     * An \ref ImageDraw represents the additional paremeters
     * beyond the image data for drawing an image with
     * RenderEncoderBase::draw_image().
     */
    class ImageDraw
    {
    public:
      /*!
       * Ctor. Initialize to having no mask,
       * no gradient and the base color as
       * solid white
       */
      ImageDraw(enum filter_t f = filter_linear):
        m_base_color(1.0f, 1.0f, 1.0f, 1.0f),
        m_colorspace(false, colorspace_srgb),
        m_filter(f),
        m_post_sampling_mode(color_post_sampling_mode_direct),
        m_with_aa(true)
      {}

      /*!
       * Ctor. Initialize to having the passed
       * clipping mask, no gradient and the base
       * color as solid white
       */
      ImageDraw(const ItemMask &clip, enum filter_t f = filter_linear):
        m_base_color(1.0f, 1.0f, 1.0f, 1.0f),
        m_colorspace(false, colorspace_srgb),
        m_filter(f),
        m_post_sampling_mode(color_post_sampling_mode_direct),
        m_with_aa(true),
        m_clip(clip)
      {}

      /*!
       * Set the value of \ref m_base_color without affecting
       * in which colorspace the modulation occurs in.
       */
      ImageDraw&
      base_color(const vec4 &v)
      {
        m_base_color = v;
        return *this;
      }

      /*!
       * Set the value of \ref m_base_color specifying
       * at which color space the modulation occurs in.
       */
      ImageDraw&
      base_color(const vec4 &v, enum colorspace_t tcolorspace)
      {
        m_base_color = v;
        m_colorspace.first = true;
        m_colorspace.second = tcolorspace;
        return *this;
      }

      /*!
       * Set the value of \ref m_base_color specifying
       * at which color space it is in.
       */
      template<enum colorspace_t tcolorspace>
      ImageDraw&
      base_color(FixedPointColor<tcolorspace> v)
      {
        return base_color(v.normalized_value(), tcolorspace);
      }

      /*!
       * set the value of \ref m_gradient
       */
      ImageDraw&
      gradient(RenderValue<Gradient> v)
      {
        m_gradient = v;
        return *this;
      }

      /*!
       * Set the value of \ref m_gradient_transformation
       */
      ImageDraw&
      gradient_transformation(RenderValue<GradientTransformation> v)
      {
        m_gradient_transformation = v;
        return *this;
      }

      /*!
       * Set the colorspace at which the modulation takes place,
       * i.e. set the value of \ref m_colorspace .first as true
       * and \ref m_colorspace .second as the passed value.
       */
      ImageDraw&
      colorspace(enum colorspace_t v)
      {
        m_colorspace.first = true;
        m_colorspace.second = v;
        return *this;
      }

      /*!
       * Set the colorspace at which the modulation takes place
       * to match the color space at which the rendering takes
       * place, i.e. set the value of \ref m_colorspace .first as
       * false.
       */
      ImageDraw&
      colorspace_rendering(void)
      {
        m_colorspace.first = false;
        return *this;
      }

      /*!
       * Set the value of \ref m_filter
       */
      ImageDraw&
      filter(enum filter_t v)
      {
        m_filter = v;
        return *this;
      }

      /*!
       * Set the value of \ref m_post_sampling_mode
       */
      ImageDraw&
      post_sampling_mode(enum color_post_sampling_mode_t v)
      {
        m_post_sampling_mode = v;
        return *this;
      }

      /*!
       * Set the value of \ref m_with_aa
       */
      ImageDraw&
      with_aa(bool v)
      {
        m_with_aa = v;
        return *this;
      }

      /*!
       * Set the value of \ref m_clip
       */
      ImageDraw&
      clip(const ItemMask &v)
      {
        m_clip = v;
        return *this;
      }

      /*!
       * if valid, indicates that the brush has a gradient.
       */
      RenderValue<Gradient> m_gradient;

      /*!
       * If valid, provides the astral::GradientTransformation
       * to apply to \ref m_gradient. An invalid handle value
       * indicates that no transformation or repeat-winding
       * occur.
       */
      RenderValue<GradientTransformation> m_gradient_transformation;

      /*!
       * Provides the starting base color of the ImageDraw
       * The color space of the base-color is the color
       * space that the modulation is taking place. The
       * value in \ref m_base_color is WITHOUT having
       * alpha pre-multiplied.
       */
      vec4 m_base_color;

      /*!
       * If .first is true, have all color modulation take
       * place in the colorspace named by .second. Otherwise
       * have the color modulation take place in whatever
       * color space one is currently rendering it.
       */
      std::pair<bool, enum colorspace_t> m_colorspace;

      /*!
       * Filtering to apply to the image's mipmap level chosen
       */
      enum filter_t m_filter;

      /*!
       * Post sampling option to apply to the image
       */
      enum color_post_sampling_mode_t m_post_sampling_mode;

      /*!
       * If true, anti-alias the edges of the rectangle if the rectangle
       * is not screen aligned
       */
      bool m_with_aa;

      /*!
       * What, if any, clipping to apply
       */
      ItemMask m_clip;
    };
  };

  /*!
   * \brief
   * A astral::RenderEncoderBase specifies the common drawing interface
   * of astral::RenderEncoderImage and astral::RenderEncoderSurface.
   */
  class RenderEncoderBase:public RenderSupportTypes
  {
  public:
    /*!
     * Executes RenderEncoderBase::save_transformation() at ctor
     * and RenderEncoderBase::restore_transformation() on dtor.
     * In addition, it also restores the snapshot pause state
     * (including depth) at dtor. Do not use save_transformation()
     * and restore_transformation(), use AutoRestore to make sure.
     */
    class AutoRestore
    {
    public:
      /*!
       * Ctor.
       * \param b on dtor, will restore the astral::RenderEncoderBase
       *        to its state it was in at ctor.
       */
      explicit
      AutoRestore(const RenderEncoderBase &b);

      /*!
       * Ctor.
       * \param b on dtor, will restore the astral::RenderEncoderMask
       *        to its state it was in at ctor.
       */
      explicit
      AutoRestore(const RenderEncoderMask &b);

      /*!
       * Ctor.
       * \param b on dtor, will restore the astral::RenderEncoderShadowMap
       *        to its state it was in at ctor.
       */
      explicit
      AutoRestore(const RenderEncoderShadowMap &b);

      ~AutoRestore(void);

    private:
      AutoRestore(Renderer::VirtualBuffer *p);

      unsigned int m_transformation_stack;
      int m_pause_snapshot;
      Renderer::VirtualBuffer *m_buffer;
    };

    /*!
     * Default ctor creates an astral::RenderEncoderBase
     * whose valid() method returns false.
     */
    RenderEncoderBase(void):
      m_virtual_buffer(nullptr)
    {}

    /*!
     * Construct an astral::RenderEncoderBase from an
     * astral::RenderEncoderSurface
     */
    RenderEncoderBase(RenderEncoderSurface);

    /*!
     * Construct an astral::RenderEncoderBase from an
     * astral::RenderEncoderImage
     */
    RenderEncoderBase(RenderEncoderImage);

    /*!
     * Equality operator
     */
    bool
    operator==(RenderEncoderBase rhs) const
    {
      return m_virtual_buffer == rhs.m_virtual_buffer
        || (!valid() && !rhs.valid());
    }

    /*!
     * Inequality operator
     */
    bool
    operator!=(RenderEncoderBase rhs) const
    {
      return !operator==(rhs);
    }

    /*!
     * Returns true if and only if this astral::RenderEncoderBase
     * is valid, i.e. it can be used for rendering. If valid()
     * returns false, then it is illegal to call any method
     * except valid(). This method does NOT take into account
     * if Renderer::finish() was called.
     */
    bool
    valid(void) const;

    /*!
     * Returns true if and only if this astral::RenderEncoderBase
     * is effectivly degenerate.
     */
    bool
    degenerate(void) const;

    /*!
     * Returns the astral::Renderer ancestor of this
     * astral::RenderEncoderBase
     */
    Renderer&
    renderer(void) const;

    /*!
     * Returne the astral::RenderEngine of renderer().
     */
    RenderEngine&
    render_engine(void) const;

    /*!
     * Query if a custom draw will be blended correctly if it overdraws itself.
     * \param emits_partially_covered_fragments if the shader combined with the material
     *                                          and clipping mask emit partially covered
     *                                          pixels
     * \param blend_mode blend mode to query
     */
    bool
    custom_draw_can_overdraw_itself(bool emits_partially_covered_fragments, enum blend_mode_t blend_mode) const
    {
      return renderer().custom_draw_can_overdraw_itself(emits_partially_covered_fragments, blend_mode);
    }

    /*!
     * Overload of custom_draw_can_overdraw_itself() taking a material and item shader
     * \param material astral::ItemMaterial of the query
     * \param shader astral::ColorItemShader of the query
     * \param blend_mode blend mode to query
     */
    bool
    custom_draw_can_overdraw_itself(const ItemMaterial &material,
                                    const ColorItemShader &shader,
                                    enum blend_mode_t blend_mode) const
    {
      return renderer().custom_draw_can_overdraw_itself(material, shader, blend_mode);
    }

    /*!
     * Returns true if and only if this astral::RenderEncoderBase
     * has been finished. An indirect call can happens on an encoder
     * Foo if the results of Foo are used by an encoder Bar when
     * astral::RenderEncoderBase::finish() is called on Bar. This
     * indirect call is to guarnantee that Foo cannot use the image
     * of Bar in its rendering which would result in a circular
     * dependency.
     */
    bool
    finished(void) const;

    /*!
     * Returns the scaling factor from pixel coordinates to
     * the the render destination (be it a astral::RenderTarget
     * or astral::Image). A value of 1.0 means that a pixel in
     * RenderEncoderBase is a pixel in the render destination.
     * A value of less than 1.0 means that a single pixel in the
     * render destination is multiple pixels in astral::RenderEncoderBase.
     */
    vec2
    render_scale_factor(void) const;

    /*!
     * The colorspace at which the contents of the layer or surface
     * of this encoder are rendered
     */
    enum colorspace_t
    colorspace(void) const;

    /*!
     * Returns true only when the encoder is rendering to an astral::Image.
     */
    bool
    rendering_to_image(void) const;

    /*!
     * Equivalent to
     * \code
     * renderer().create_value(v);
     * \endcode
     */
    RenderValue<Transformation>
    create_value(const Transformation &v) const
    {
      return renderer().create_value(v);
    }

    /*!
     * Equivalent to
     * \code
     * renderer().create_value(v);
     * \endcode
     */
    RenderValue<ScaleTranslate>
    create_value(const ScaleTranslate &v) const
    {
      return renderer().create_value(v);
    }

    /*!
     * Equivalent to
     * \code
     * renderer().create_value(v);
     * \endcode
     */
    RenderValue<GradientTransformation>
    create_value(const GradientTransformation &v) const
    {
      return renderer().create_value(v);
    }

    /*!
     * Equivalent to
     * \code
     * renderer().create_value(v);
     * \endcode
     */
    RenderValue<ImageSampler>
    create_value(const ImageSampler &v) const
    {
      return renderer().create_value(v);
    }

    /*!
     * Equivalent to
     * \code
     * renderer().create_value(v);
     * \endcode
     */
    RenderValue<Gradient>
    create_value(const Gradient &v) const
    {
      return renderer().create_value(v);
    }

    /*!
     * Equivalent to
     * \code
     * renderer().create_value(v);
     * \endcode
     */
    RenderValue<Brush>
    create_value(const Brush &v) const
    {
      return renderer().create_value(v);
    }

    /*!
     * Equivalent to
     * \code
     * renderer().create_value(v);
     * \endcode
     */
    RenderValue<const ShadowMap&>
    create_value(const ShadowMap &v) const
    {
      return renderer().create_value(v);
    }

    /*!
     * Equivalent to
     * \code
     * renderer().create_value(v);
     * \endcode
     */
    template<typename T>
    RenderValue<T>
    render_value_from_cookie(uint32_t cookie) const
    {
      return renderer().render_value_from_cookie<T>(cookie);
    }

    /*!
     * Equivalent to
     * \code
     * renderer().create_item_data(v, item_data_value_map, dependencies);
     * \endcode
     */
    ItemData
    create_item_data(c_array<const gvec4> value,
                     c_array<const ItemDataValueMapping::entry> item_data_value_map,
                     const ItemDataDependencies &dependencies = ItemDataDependencies()) const
    {
      return renderer().create_item_data(value, item_data_value_map, dependencies);
    }

    /*!
     * Equivalent to
     * \code
     * create_item_data(value, map.data(), dependencies)
     * \endcode
     */
    ItemData
    create_item_data(c_array<const gvec4> value,
                     const ItemDataValueMapping &item_data_value_map,
                     const ItemDataDependencies &dependencies = ItemDataDependencies()) const
    {
      return create_item_data(value, item_data_value_map.data(), dependencies);
    }

    /*!
     * Equivalent to
     * \code
     * create_item_data(value, c_array<const ItemDataValueMapping::entry>(), dependencies)
     * \endcode
     */
    ItemData
    create_item_data(c_array<const gvec4> value, enum no_item_data_value_mapping_t,
                     const ItemDataDependencies &dependencies = ItemDataDependencies()) const
    {
      return create_item_data(value, c_array<const ItemDataValueMapping::entry>(), dependencies);
    }

    /*!
     * Overload of create_item_data() passing an astral::vecN of astral::ImageID
     * values. Equivalent to
     * \code
     * c_array<const ImageID> ii(dependencies);
     * c_array<const ShadowMapID> ss;
     * create_item_data(value, v, ItemDataDependencies(ii, ss));
     * \endcode
     */
    template<size_t N, typename V>
    ItemData
    create_item_data(c_array<const gvec4> value, const V &v, const vecN<ImageID, N> &dependencies) const
    {
      c_array<const ImageID> ii(dependencies);
      c_array<const ShadowMapID> ss;
      return create_item_data(value, v, ItemDataDependencies(ii, ss));
    }

    /*!
     * Overload of create_item_data() passing an astral::vecN of astral::ShadowMapID
     * values. Equivalent to
     * \code
     * c_array<const ImageID> ii;
     * c_array<const ShadowMapID> ss(dependencies);
     * create_item_data(value, v, ItemDataDependencies(ii, ss));
     * \endcode
     */
    template<size_t N, typename V>
    ItemData
    create_item_data(c_array<const gvec4> value, const V &v, const vecN<ShadowMapID, N> &dependencies) const
    {
      c_array<const ImageID> ii;
      c_array<const ShadowMapID> ss(dependencies);
      return create_item_data(value, v, ItemDataDependencies(ii, ss));
    }

    /*!
     * Overload of create_item_data() passing an astral::vecN of astral::ImageID
     * and an astral::vecN of astral::ShadowMapID values. Equivalent to
     * \code
     * c_array<const ImageID> ii(image_dependencies);
     * c_array<const ShadowMapID> ss(shadow_dependencies);
     * create_item_data(value, v, ItemDataDependencies(ii, ss));
     * \endcode
     */
    template<size_t N, size_t M, typename V>
    ItemData
    create_item_data(c_array<const gvec4> value, const V &v,
                     const vecN<ImageID, N> &image_dependencies,
                     const vecN<ShadowMapID, M> &shadow_dependencies) const
    {
      c_array<const ImageID> ii(image_dependencies);
      c_array<const ShadowMapID> ss(shadow_dependencies);
      return create_item_data(value, v, ItemDataDependencies(ii, ss));
    }

    /*!
     * Returns true if the passed astral::RenderValue
     * is valid (i.e. can be used) with this
     * astral::RenderEncoderBase.
     */
    template<typename T>
    bool
    valid(const RenderValue<T> &v) const;

    /*!
     * Returns true if the passed astral::ItemData
     * is valid (i.e. can be used) with this
     * astral::RenderEncoderBase.
     */
    bool
    valid(const ItemData &v) const;

    /*!
     * Returns the accuracy, in pixels, that astral::RenderEncoder will
     * use when approximating curves by lines and/or other curves.
     */
    float
    render_accuracy(void) const;

    /*!
     * Sets the value returned by render_accuracy(void) const.
     */
    void
    render_accuracy(float v) const;

    /*!
     * Only has effect if RenderEngine::OverridableProperties::m_uber_shader_method
     * is astral::uber_shader_none. When true, specifies to use mini uber-shaders;
     * currently only has impact for direct stroking and drawing masks. Default value
     * is true.
     */
    bool
    use_sub_ubers(void) const;

    /*!
     * Set the value returned by use_sub_ubers(void) const
     */
    void
    use_sub_ubers(bool) const;

    /*!
     * Compute the tolerance value to use for path approximations
     * based off of the current value for render_accuracy(void) const
     * and the current transformation(void) const.
     */
    float
    compute_tolerance(void) const;

    /*!
     * Compute the tolerance value to use for path approximations
     * based off of the current value for render_accuracy(void) const,
     * the current transformation(void) const and an optional float2x2
     * matrix.
     * \param matrix if non-null, compute the tolerance needed
     *               as if the current concact() was called passing
     *               *matrix.
     */
    float
    compute_tolerance(const float2x2 *matrix) const;

    /*!
     * Returns the bounding box in pixel-coordinates
     * of the rendering region of the encoder.
     */
    BoundingBox<float>
    pixel_bounding_box(void) const;

    /*!
     * Returns the bounding box in pixel-coordinates
     * of the rendering region intersected against a
     * passed bounding box that is in logical coordinates
     */
    BoundingBox<float>
    pixel_bounding_box(const BoundingBox<float> &logical_bb) const;

    /*!
     * Returns the current transformation from logical to pixel coordinates.
     */
    const Transformation&
    transformation(void) const;

    /*!
     * Returns the current transformation realized
     * in an astral::RenderValue<astral::Transformation>.
     * Value is heavily cached.
     */
    RenderValue<Transformation>
    transformation_value(void) const;

    /*!
     * Returns the singular values of transformation().
     */
    vec2
    singular_values(void) const;

    /*!
     * Returns an upper bound for the size of one pixel
     * of the surface to which the encoder renders in
     * logical coordinates.
     */
    float
    surface_pixel_size_in_logical_coordinates(void) const;

    /*!
     * Returns the inverse of the current transformation value.
     */
    const Transformation&
    inverse_transformation(void) const;

    /*!
     * Set the current transformation value.
     */
    void
    transformation(const Transformation &v) const;

    /*!
     * Set the current transformation value; if v is
     * not valid, sets the transformation to the identity
     */
    void
    transformation(RenderValue<Transformation> v) const;

    /*!
     * *Set* the translation of the transformation
     */
    void
    transformation_translate(float x, float y) const;

    /*!
     * *Set* the translation of the transformation
     */
    void
    transformation_translate(vec2 v)
    {
      transformation_translate(v.x(), v.y());
    }

    /*!
     * *Set* the translation of the transformation
     */
    void
    transformation_matrix(const float2x2 &v) const;

    /*!
     * Concat the current transformation value.
     */
    void
    concat(const Transformation &v) const;

    /*!
     * Concat the current transformation value.
     */
    void
    concat(const float2x2 &v) const;

    /*!
     * Translate the current transformation.
     */
    void
    translate(float x, float y) const;

    /*!
     * Translate the current transformation.
     */
    void
    translate(vec2 v) const
    {
      translate(v.x(), v.y());
    }

    /*!
     * Scale the current transformation.
     */
    void
    scale(float sx, float sy) const;

    /*!
     * Scale the current transformation.
     */
    void
    scale(vec2 s) const
    {
      scale(s.x(), s.y());
    }

    /*!
     * Scale the current transformation.
     */
    void
    scale(float s) const
    {
      scale(s, s);
    }

    /*!
     * Rotate the current transformation value.
     */
    void
    rotate(float radians) const;

    /*!
     * Pushes the transformation-stack
     */
    void
    save_transformation(void) const;

    /*!
     * Returns the current save transformation stack size
     */
    unsigned int
    save_transformation_count(void) const;

    /*!
     * Restores the transformation stack to what it was
     * at the last call to save_transformation(); only
     * affects tranformation stack
     */
    void
    restore_transformation(void) const;

    /*!
     * Restores the transformation stack to a specified value of
     * the stack size; only affects tranformation stack
     */
    void
    restore_transformation(unsigned int cnt) const;

    /*!
     * returns the default shaders used
     */
    const ShaderSet&
    default_shaders(void) const;

    /*!
     * returns the default effects used
     */
    const EffectSet&
    default_effects(void) const;

    /*!
     * Draw a rectangle
     * \param shader to use to draw the rect
     * \param rect rectangle to draw
     * \param material the material and mask to apply to the draw
     * \param blend_mode blend mode to apply to the rect
     */
    void
    draw_rect(const ColorItemShader &shader, const Rect &rect,
              const ItemMaterial &material = ItemMaterial(),
              enum blend_mode_t blend_mode = blend_porter_duff_src_over) const;

    /*!
     * Draw a rectangle
     * \param rect rectangle to draw
     * \param with_aa use a shader that will apply anti-aliasing to the rect
     * \param material the material and mask to apply to the draw
     * \param blend_mode blend mode to apply to the rect
     */
    void
    draw_rect(const Rect &rect, bool with_aa,
              const ItemMaterial &material = ItemMaterial(),
              enum blend_mode_t blend_mode = blend_porter_duff_src_over) const;

    /*!
     * Draw a rectangle with anti-aliasing, equivalent to
     * \code
     * draw_rect(rect, true, material, blend_mode);
     * \endcode
     * \param rect rectangle to draw
     * \param material the material and mask to apply to the draw
     * \param blend_mode blend mode to apply to the rect
     */
    void
    draw_rect(const Rect &rect, const ItemMaterial &material = ItemMaterial(),
              enum blend_mode_t blend_mode = blend_porter_duff_src_over) const
    {
      draw_rect(rect, true, material, blend_mode);
    }

    /*!
     * Draw the region of a mask with a material.
     * \param mask the mask to apply to the draw
     * \param mask_transformation_logical trasformation from logical
     *                                    to mask coordinates; providing
     *                                    this value overrides the value
     *                                    of MaskDetails::m_mask_transformation_pixel
     * \param filter filter to apply when sampling from the mask
     * \param material the material to apply to the draw
     * \param blend_mode blend mode to apply to the rect
     */
    void
    draw_mask(const MaskDetails &mask,
              const Transformation &mask_transformation_logical,
              enum filter_t filter, const ItemMaterial &material,
              enum blend_mode_t blend_mode = blend_porter_duff_src_over) const;

    /*!
     * Draw the region of a mask with a material, using te value
     * of transformation() and MaskDetails::m_mask_transformation_pixel
     * to compute the transformation from logical to mask coordinates
     * \param mask the mask to apply to the draw
     * \param filter filter to apply when sampling from the mask
     * \param material the material to apply to the draw
     * \param blend_mode blend mode to apply to the rect
     */
    void
    draw_mask(const MaskDetails &mask,
              enum filter_t filter, const ItemMaterial &material,
              enum blend_mode_t blend_mode = blend_porter_duff_src_over) const
    {
      draw_mask(mask,
                Transformation(mask.m_mask_transformation_pixel) * transformation(),
                filter, material, blend_mode);
    }

    /*!
     * Draw the region of Image mask with a material.
     * \param mask the mask to apply to the draw
     * \param mask_transformation_logical trasformation from logical
     *                                    to mask coordinates
     * \param filter filter to apply when sampling from the mask
     * \param post_sampling_mode specifies if to invert the mask
     * \param mask_type how to interpet the sampled value from the mask
     * \param mask_channel specifies which channel from the mask to sample
     * \param material the material to apply to the draw
     * \param blend_mode blend mode to apply to the rect
     */
    void
    draw_mask(const SubImageT<float> &mask,
              const Transformation &mask_transformation_logical,
              enum filter_t filter,
              enum mask_post_sampling_mode_t post_sampling_mode,
              enum mask_type_t mask_type,
              enum mask_channel_t mask_channel,
              const ItemMaterial &material,
              enum blend_mode_t blend_mode = blend_porter_duff_src_over) const;

    /*!
     * Draw the region of Image mask with a material.
     * \param mask the mask to apply to the draw
     * \param mask_transformation_logical trasformation from logical
     *                                    to mask coordinates
     * \param filter filter to apply when sampling from the mask
     * \param post_sampling_mode specifies if to invert the mask
     * \param mask_type how to interpet the sampled value from the mask
     * \param mask_channel specifies which channel from the mask to sample
     * \param material the material to apply to the draw
     * \param blend_mode blend mode to apply to the rect
     */
    template<typename T>
    void
    draw_mask(const SubImageT<T> &mask,
              const Transformation &mask_transformation_logical,
              enum filter_t filter,
              enum mask_post_sampling_mode_t post_sampling_mode,
              enum mask_type_t mask_type,
              enum mask_channel_t mask_channel,
              const ItemMaterial &material,
              enum blend_mode_t blend_mode = blend_porter_duff_src_over) const
    {
      draw_mask(mask.template type_cast<float>(),
                mask_transformation_logical, filter,
                post_sampling_mode, mask_type,
                mask_channel, material, blend_mode);
    }

    /*!
     * Draw the region of Image mask with a material.
     * \param mask the mask to apply to the draw
     * \param mask_transformation_logical trasformation from logical
     *                                    to mask coordinates
     * \param material the material to apply to the draw
     * \param blend_mode blend mode to apply to the rect
     */
    void
    draw_mask(const ImageSampler &mask,
              const Transformation &mask_transformation_logical,
              const ItemMaterial &material,
              enum blend_mode_t blend_mode = blend_porter_duff_src_over) const;

    /*!
     * Draw an image; the main use of draw_image() over draw_rect() is
     * that the empty and full tiles of the image will be handled
     * more efficiently. The rect drawn is the size of the image.
     * \param image image to draw
     * \param mip how to chose what mipmap level to draw
     * \param draw additional parameters for drawing the image
     * \param blend_mode blend mode to use
     */
    void
    draw_image(const SubImage &image, enum mipmap_t mip = mipmap_ceiling,
               const ImageDraw &draw = ImageDraw(),
               enum blend_mode_t blend_mode = blend_porter_duff_src_over)
    {
      draw_image(image, MipmapLevel(mip, transformation().m_matrix), draw, blend_mode);
    }

    /*!
     * Draw an image; the main use of draw_image() over draw_rect() is
     * that the empty and full tiles of the image will be handled
     * more efficiently. The rect drawn is the size of the image.
     * \param image image to draw
     * \param mipmap_level what mipmap level of the image to draw
     * \param draw additional parameters for drawing the image
     * \param blend_mode blend mode to use
     */
    void
    draw_image(const SubImage &image, MipmapLevel mipmap_level,
               const ImageDraw &draw = ImageDraw(),
               enum blend_mode_t blend_mode = blend_porter_duff_src_over);

    /*!
     * Draw layers of an astral::ItemPath with a specified shader
     * \param shader astral::ColorItemPathShader to use to draw
     * \param layers layers to draw
     * \param material the material and mask to apply to the draw
     * \param blend_mode blend mode to apply
     */
    void
    draw_item_path(const ColorItemPathShader &shader,
                   c_array<const ItemPath::Layer> layers,
                   const ItemMaterial &material = ItemMaterial(),
                   enum blend_mode_t blend_mode = blend_porter_duff_src_over) const;

    /*!
     * Draw astral::ItemPath layers
     * \param layers layers to draw
     * \param material the material and mask to apply to the draw
     * \param blend_mode blend mode to apply
     */
    void
    draw_item_path(c_array<const ItemPath::Layer> layers,
                   const ItemMaterial &material = ItemMaterial(),
                   enum blend_mode_t blend_mode = blend_porter_duff_src_over) const
    {
      draw_item_path(default_shaders().m_color_item_path_shader,
                     layers, material, blend_mode);
    }

    /*!
     * Draw a singlelayer of an astral::ItemPath with a specified shader
     * \param shader astral::ColorItemPathShader to use to draw
     * \param layer layer to draw
     * \param material the material and mask to apply to the draw
     * \param blend_mode blend mode to apply
     */
    void
    draw_item_path(const ColorItemPathShader &shader,
                   const ItemPath::Layer &layer,
                   const ItemMaterial &material = ItemMaterial(),
                   enum blend_mode_t blend_mode = blend_porter_duff_src_over) const
    {
      draw_item_path(shader, c_array<const ItemPath::Layer>(&layer, 1),
                     material, blend_mode);
    }

    /*!
     * Draw a singlelayer of an astral::ItemPath
     * \param layer layer to draw
     * \param material the material and mask to apply to the draw
     * \param blend_mode blend mode to apply
     */
    void
    draw_item_path(const ItemPath::Layer &layer,
                   const ItemMaterial &material = ItemMaterial(),
                   enum blend_mode_t blend_mode = blend_porter_duff_src_over) const
    {
      draw_item_path(default_shaders().m_color_item_path_shader,
                     c_array<const ItemPath::Layer>(&layer, 1),
                     material, blend_mode);
    }

    /*!
     * Stroke an astral::CombinedPath using a default stroke shader
     * \param paths paths to stroke
     * \param stroke_params how to stroke
     * \param material material to apply to the stroke
     * \param blend_mode blend mode to apply to stroke
     * \param mask_usage how to use the mask generated
     * \param mask_properties how to generate the mask
     * \param out_data if non-null, location to which to write the
     *                 the path mask and the transformations to use it.
     *                 RenderEncoderStrokeMask::change_mask_mode() is
     *                 can be used with the value written to out_data.
     *
     * TODO: make a form that does not take a StrokeMaskProperties but
     *       instead generates good values from the other arguments.
     */
    void
    stroke_paths(const CombinedPath &paths,
                 const StrokeParameters &stroke_params,
                 const ItemMaterial &material = ItemMaterial(),
                 enum blend_mode_t blend_mode = blend_porter_duff_src_over,
                 MaskUsage mask_usage = MaskUsage(),
                 const StrokeMaskProperties &mask_properties = StrokeMaskProperties(),
                 MaskDetails *out_data = nullptr) const
    {
      stroke_paths(*default_shaders().m_mask_stroke_shader,
                   paths, stroke_params,
                   StrokeShader::ItemDataPacker(),
                   material, blend_mode, mask_usage,
                   mask_properties, out_data);
    }

    /*!
     * Stroke an astral::CombinedPath dashed using a default stroke shader.
     * \param paths paths to stroke
     * \param stroke_params how to stroke
     * \param material material to apply to the stroke
     * \param dash_pattern dash pattern to apply
     * \param blend_mode blend mode to apply to stroke
     * \param mask_usage how to use the mask generated
     * \param mask_properties how to generate the mask
     * \param out_data if non-null, location to which to write the
     *                 the path mask and the transformations to use it.
     *                 RenderEncoderStrokeMask::change_mask_mode() is
     *                 can be used with the value written to out_data.
     *
     * TODO: make a form that does not take a StrokeMaskProperties but
     *       instead generates good values from the other arguments.
     */
    void
    stroke_paths(const CombinedPath &paths,
                 const StrokeParameters &stroke_params,
                 const StrokeShader::DashPattern &dash_pattern,
                 const ItemMaterial &material = ItemMaterial(),
                 enum blend_mode_t blend_mode = blend_porter_duff_src_over,
                 MaskUsage mask_usage = MaskUsage(),
                 const StrokeMaskProperties &mask_properties = StrokeMaskProperties(),
                 MaskDetails *out_data = nullptr) const
    {
      stroke_paths(*default_shaders().m_mask_dashed_stroke_shader,
                   paths, stroke_params, dash_pattern, material, blend_mode,
                   mask_usage, mask_properties, out_data);
    }

    /*!
     * Stroke an astral::CombinedPath using a custom shader and data packer.
     * \param shader shader with which to stroke
     * \param paths paths to stroke
     * \param stroke_params how to stroke
     * \param packer generator of item data
     * \param material material to apply to the stroke
     * \param blend_mode blend mode to apply to stroke
     * \param mask_usage how to use the mask generated
     * \param mask_properties how to generate the mask
     * \param out_data if non-null, location to which to write the
     *                 the path mask and the transformations to use it.
     *                 RenderEncoderStrokeMask::change_mask_mode() is
     *                 can be used with the value written to out_data.
     */
    void
    stroke_paths(const MaskStrokeShader &shader,
                 const CombinedPath &paths,
                 const StrokeParameters &stroke_params,
                 const StrokeShader::ItemDataPackerBase &packer,
                 const ItemMaterial &material = ItemMaterial(),
                 enum blend_mode_t blend_mode = blend_porter_duff_src_over,
                 MaskUsage mask_usage = MaskUsage(),
                 const StrokeMaskProperties &mask_properties = StrokeMaskProperties(),
                 MaskDetails *out_data = nullptr) const;

    /*!
     * Stroke a path directly without generating a mask using the
     * default stroke shader. This results in overdraw where the
     * stroked path self-intersects which also includes the inside
     * of every join. Do not use this if the material is transparent.
     * \param paths paths to stroke
     * \param stroke_params how to stroke
     * \param material material to apply to the stroke
     * \param blend_mode blend mode to apply to stroke
     */
    void
    direct_stroke_paths(const CombinedPath &paths,
                        const StrokeParameters &stroke_params,
                        const ItemMaterial &material = ItemMaterial(),
                        enum blend_mode_t blend_mode = blend_porter_duff_src_over) const
    {
      direct_stroke_paths(*default_shaders().m_direct_stroke_shader,
                          paths, stroke_params,
                          StrokeShader::ItemDataPacker(),
                          material, blend_mode);
    }

    /*!
     * Stroke a path dahsed directly without generating a mask using
     * the default stroke shader. This results in overdraw where the
     * stroked path self-intersects which also includes the inside
     * of every join. Do not use this if the material is transparent.
     * \param paths paths to stroke
     * \param stroke_params how to stroke
     * \param dash_pattern dash pattern to apply to the stroke
     * \param material material to apply to the stroke
     * \param blend_mode blend mode to apply to stroke
     */
    void
    direct_stroke_paths(const CombinedPath &paths,
                        const StrokeParameters &stroke_params,
                        const StrokeShader::DashPattern &dash_pattern,
                        const ItemMaterial &material = ItemMaterial(),
                        enum blend_mode_t blend_mode = blend_porter_duff_src_over) const
    {
      direct_stroke_paths(*default_shaders().m_direct_dashed_stroke_shader,
                          paths, stroke_params, dash_pattern,
                          material, blend_mode);
    }

    /*!
     * Stroke a path directly without generating a mask. This
     * results in overdraw where the stroked path self-intersects
     * which also includes the inside of every join. Do not use
     * this if the material is transparent.
     * \param shader shader with which to stroke
     * \param paths paths to stroke
     * \param stroke_params how to stroke
     * \param packer generator of item data
     * \param material material to apply to the stroke
     * \param blend_mode blend mode to apply to stroke
     */
    void
    direct_stroke_paths(const DirectStrokeShader &shader,
                        const CombinedPath &paths,
                        const StrokeParameters &stroke_params,
                        const StrokeShader::ItemDataPackerBase &packer,
                        const ItemMaterial &material = ItemMaterial(),
                        enum blend_mode_t blend_mode = blend_porter_duff_src_over) const;

    /*!
     * Fill an astral::CombinedPath.
     * \param paths paths to be filled
     * \param fill_params how to fill
     * \param material material to apply to the stroke
     * \param blend_mode blend mode to apply to stroke
     * \param mask_usage how to use the mask generated
     * \param mask_properties how to generate the mask
     * \param out_data if non-null, location to which to write the
     *                 the path mask and the transformations to use it
     * \param out_clip_element if non-null, location to which to write
     *                         the astral::RenderClipElement corresponding
     *                         to the path fills
     */
    void
    fill_paths(const CombinedPath &paths,
               const FillParameters &fill_params,
               const ItemMaterial &material = ItemMaterial(),
               enum blend_mode_t blend_mode = blend_porter_duff_src_over,
               MaskUsage mask_usage = MaskUsage(),
               const FillMaskProperties &mask_properties = FillMaskProperties(),
               MaskDetails *out_data = nullptr,
               reference_counted_ptr<const RenderClipElement> *out_clip_element = nullptr) const;

    /*!
     * Fill an astral::CombinedPath.
     * \param paths paths to be filled
     * \param fill_params how to fill
     * \param material material to apply to the stroke
     * \param blend_mode blend mode to apply to stroke
     * \param mask_usage how to use the mask generated
     * \param mask_properties how to generate the mask
     * \param out_clip_element if non-null, location to which to write
     *                         the astral::RenderClipElement corresponding
     *                         to the path fills
     */
    void
    fill_paths(const CombinedPath &paths,
               const FillParameters &fill_params,
               const ItemMaterial &material,
               enum blend_mode_t blend_mode,
               MaskUsage mask_usage,
               const FillMaskProperties &mask_properties,
               reference_counted_ptr<const RenderClipElement> *out_clip_element) const
    {
      fill_paths(paths, fill_params, material, blend_mode, mask_usage, mask_properties, nullptr, out_clip_element);
    }

    /*!
     * Draw some text.
     * \param shader shader to use for shading
     * \param text text to draw
     * \param packer GlyphShader::ItemDataPackerBase to generate shader data
     *               for shader to consume
     * \param material the material and mask to apply to the draw
     * \param blend_mode blend mode to apply to the rect
     * \returns which strike index used from the astral::TextItem
     */
    int
    draw_text(const GlyphShader &shader,
              const TextItem &text,
              const GlyphShader::ItemDataPackerBase &packer,
              const ItemMaterial &material = ItemMaterial(),
              enum blend_mode_t blend_mode = blend_porter_duff_src_over) const;

    /*!
     * Draw some text. Equivalent to
     * \code
     * draw_text(default_shaders().m_glyph_shader, text,
     *           packer, material, blend_mode);
     * \endcode
     * i.e. draw glyphs with the default glyph shader.
     *
     * \param text text to draw
     * \param packer synthetic font data
     * \param material the material and mask to apply to the draw
     * \param blend_mode blend mode to apply to the rect
     * \returns which strike index used from the astral::TextItem
     */
    int
    draw_text(const TextItem &text,
              const GlyphShader::SyntheticData &packer,
              const ItemMaterial &material = ItemMaterial(),
              enum blend_mode_t blend_mode = blend_porter_duff_src_over) const
    {
      return draw_text(default_shaders().m_glyph_shader, text,
                       packer, material, blend_mode);
    }

    /*!
     * Draw some text. Equivalent to
     * \code
     * draw_text(default_shaders().m_glyph_shader, text,
     *           GlyphShader::EmptyPacker(), material, blend_mode);
     * \endcode
     * i.e. draw glyphs with the default glyph shader.
     *
     * \param text text to draw
     * \param material the material and mask to apply to the draw
     * \param blend_mode blend mode to apply to the rect
     * \returns which strike index used from the astral::TextItem
     */
    int
    draw_text(const TextItem &text,
              const ItemMaterial &material = ItemMaterial(),
              enum blend_mode_t blend_mode = blend_porter_duff_src_over) const
    {
      return draw_text(default_shaders().m_glyph_shader, text,
                       GlyphShader::EmptyPacker(), material, blend_mode);
    }

    /*!
     * Draw some text as a PATH. Requires that the value of
     * TextItem::font().typeface().is_scalable() is true.
     *
     * WARNING: if the geometries of the glyphs overlap, this
     *          will render differently than rendering the glyphs
     *          with draw_text().
     *
     * \param text text to draw
     * \param material the material and mask to apply to the draw
     * \param blend_mode blend mode to apply to the rect
     * \param mask_usage how to use the mask generated
     * \param mask_properties how to generate the mask
     * \returns astral::routine_fail if the underlying typeface is
     *          not scalable (and thus will draw nothing).
     */
    enum return_code
    draw_text_as_path(const TextItem &text,
                      const ItemMaterial &material = ItemMaterial(),
                      enum blend_mode_t blend_mode = blend_porter_duff_src_over,
                      MaskUsage mask_usage = MaskUsage(),
                      const FillMaskProperties &mask_properties = FillMaskProperties()) const;

    /*!
     * Begin a clipping node defined by the pixels of an astral::MaskDetails. The
     * actual blit of the clipped contents is issued on end_clip_node().
     * \param blend_mode the blend mode to apply when blitting the offscreen buffers
     *                   to this encoder's surface
     * \param flags specifies which (or both or none) of clip-in and clip-out to do
     * \param mask description of mask by which to clip-in and clip-out against
     * \param clip_in_bbox clipping region in PIXEL coordinates that clip-in
     *                     is to be restricted to
     * \param clip_out_bbox clipping region in PIXEL coordinates that clip-out
     *                     is to be restricted to
     * \param mask_filter filter to apply to pixel of the MaskDetails mask
     * \param clip additional clipping (if any) to apply to both the clipped-in and clipped-out content.
     */
    RenderClipNode
    begin_clip_node_pixel(enum blend_mode_t blend_mode,
                          enum clip_node_flags_t flags,
                          const MaskDetails &mask,
                          const BoundingBox<float> &clip_in_bbox,
                          const BoundingBox<float> &clip_out_bbox,
                          enum filter_t mask_filter = filter_linear,
                          const ItemMask &clip = ItemMask()) const;

    /*!
     * Begin a clipping node defined by the results of combine_clipping().
     * Both the clip-in and clip-out data is clipped by the original
     * astral::RenderClipElement; the clip-in content will be clipped-in
     * against the path-fill and the original astral::RenderClipElement;
     * the clip-out content will be clipped-out against the path-fill and
     * clipped-in against the original astral::RenderClipElement. The
     * actual blit of the clipped contents is issued on end_clip_node().
     * \param blend_mode the blend mode to apply when blitting the offscreen buffers
     *                   to this encoder's surface
     * \param flags specifies which (or both or none) of clip-in and clip-out to do
     * \param mask_buffer astral::RenderClipCombineResult that provides the mask
     * \param clip_in_bbox clipping region in PIXEL coordinates that clip-in
     *                     is to be restricted to
     * \param clip_out_bbox clipping region in PIXEL coordinates that clip-out
     *                      is to be restricted to
     * \param mask_filter filter to apply to pixel of the MaskDetails mask
     * \param clip additional clipping (if any) to apply to both the clipped-in and clipped-out content.
     */
    RenderClipNode
    begin_clip_node_pixel(enum blend_mode_t blend_mode,
                          enum clip_node_flags_t flags,
                          const RenderClipCombineResult &mask_buffer,
                          const BoundingBox<float> &clip_in_bbox,
                          const BoundingBox<float> &clip_out_bbox,
                          enum filter_t mask_filter = filter_linear,
                          const ItemMask &clip = ItemMask());

    /*!
     * Begin a clipping node defined by the pixels of an astral::ImageSampler.
     * In contrast to clip_node_logical(), clip_node_pixel() works in pixel
     * coordinates. Generally speaking, clip_node_pixel() should be used instead
     * of clip_node_logical() when issuing a clip node with raw image data. The
     * actual blit of the clipped contents is issued on end_clip_node().
     * \param flags specifies which (or both or none) of clip-in and clip-out to do
     * \param mask description of mask by which to clip-in and clip-out against
     * \param clip_in_bbox clipping region in PIXEL coordinates that clip-in
     *                     is to be restricted to
     * \param clip_out_bbox clipping region in PIXEL coordinates that clip-out
     *                     is to be restricted to
     * \param mask_filter filter to apply to pixel of the MaskDetails mask
     * \param clip additional clipping (if any) to apply to both the clipped-in and clipped-out content.
     */
    RenderClipNode
    begin_clip_node_pixel(enum clip_node_flags_t flags,
                          const MaskDetails &mask,
                          const BoundingBox<float> &clip_in_bbox,
                          const BoundingBox<float> &clip_out_bbox,
                          enum filter_t mask_filter = filter_linear,
                          const ItemMask &clip = ItemMask()) const;

    /*!
     * Begin a clipping node defined by the results of combine_clipping().
     * Both the clip-in and clip-out data is clipped by the original
     * astral::RenderClipElement; the clip-in content will be clipped-in
     * against the path-fill and the original astral::RenderClipElement;
     * the clip-out content will be clipped-out against the path-fill and
     * clipped-int against the original astral::RenderClipElement. The
     * actual blit of the clipped contents is issued on end_clip_node().
     * \param flags specifies which (or both or none) of clip-in and clip-out to do
     * \param mask_buffer astral::RenderClipCombineResult the mask
     * \param clip_in_bbox clipping region in PIXEL coordinates that clip-in
     *                     is to be restricted to
     * \param clip_out_bbox clipping region in PIXEL coordinates that clip-out
     *                     is to be restricted to
     * \param mask_filter filter to apply to pixel of the MaskDetails mask
     * \param clip additional clipping (if any) to apply to both the clipped-in and clipped-out content.
     */
    RenderClipNode
    begin_clip_node_pixel(enum clip_node_flags_t flags,
                          const RenderClipCombineResult &mask_buffer,
                          const BoundingBox<float> &clip_in_bbox,
                          const BoundingBox<float> &clip_out_bbox,
                          enum filter_t mask_filter = filter_linear,
                          const ItemMask &clip = ItemMask());

    /*!
     * Begin an offscreen clipping node defined by the fill of a astral::CombinedPath.
     * The actual blit of the clipped contents is issued on end_clip_node().
     * \param blend_mode the blend mode to apply when blitting the offscreen buffers
     *                   to this encoder's surface
     * \param flags specifies which (or both or none) of clip-in and clip-out to do
     * \param paths paths to clip against
     * \param params fill path parameters
     * \param mask_properties how to generate the mask
     * \param mask_usage how to use the mask generated from the fill to perform clipping
     * \param out_data if non-null, location to which to write the
     *                 the path mask and the transformations to use it
     * \param clip additional clipping (if any) to apply to both the clipped-in and clipped-out content.
     */
    RenderClipNode
    begin_clip_node_logical(enum blend_mode_t blend_mode,
                            enum clip_node_flags_t flags,
                            const CombinedPath &paths,
                            const FillParameters &params,
                            const FillMaskProperties &mask_properties,
                            MaskUsage mask_usage = MaskUsage(),
                            MaskDetails *out_data = nullptr,
                            const ItemMask &clip = ItemMask()) const;

    /*!
     * Begin an offscreen clipping node defined by the fill of a astral::CombinedPath.
     * The actual blit of the clipped contents is issued on end_clip_node().
     * \param flags specifies which (or both or none) of clip-in and clip-out to do
     * \param paths paths to clip against
     * \param params fill path parameters
     * \param mask_properties how to generate the mask
     * \param mask_usage how to use the mask generated from the fill to perform clipping
     * \param out_data if non-null, location to which to write the
     *                 the path mask and the transformations to use it
     * \param clip additional clipping (if any) to apply to both the clipped-in and clipped-out content.
     */
    RenderClipNode
    begin_clip_node_logical(enum clip_node_flags_t flags,
                            const CombinedPath &paths,
                            const FillParameters &params,
                            const FillMaskProperties &mask_properties,
                            MaskUsage mask_usage = MaskUsage(),
                            MaskDetails *out_data = nullptr,
                            const ItemMask &clip = ItemMask()) const;

    /*!
     * Issue the blit of clipped contents, i.e. issue the maching end
     * to begin_clip_node_logical() or begin_clip_node_pixel(). Calling
     * end_clip_node() with the same astral::RenderEncoderLayer value is
     * an error; in debug build will assert and in release build will be
     * ignored.
     * \param content RenderClipNode returned by begin_clip_node_logical()
     *                or begin_clip_node_pixel() to blit correctly clipped.
     */
    void
    end_clip_node(RenderClipNode content) const;

    /*!
     * Generate a mask-buffer from filling a path; the mask generated
     * comes from rendering the paths in this encoder's rendering region
     * with the current transformation applied.
     * \param paths paths to clip against
     * \param params fill path parameters
     * \param mask_properties how to generate the mask
     * \param mask_type specifies mask value type in the returned
     *                  MaskDetails out_data
     * \param out_data cannot be null, location to which to write the
     *                 the path mask and the transformations to use it
     * \param out_clip_element if non-null, location to which to write
     *                         the astral::RenderClipElement corresponding
     *                         to the mask described in out_data
     */
    void
    generate_mask(const CombinedPath &paths,
                  const FillParameters &params,
                  const FillMaskProperties &mask_properties,
                  enum mask_type_t mask_type,
                  MaskDetails *out_data,
                  reference_counted_ptr<const RenderClipElement> *out_clip_element = nullptr) const;

    /*!
     * Combine an astral::RenderClipElement with an astral::CombinedPath filled
     * \param clip_element RenderClipElement with which to combine
     * \param path astral::CombinedPath to fill
     * \param params specifies fill rule and how to perfrom the computation
     * \returns the astral::RenderClipCombineResult which gives both C \ F and
     *          C intersect F where C is the region represented by clip_element
     *          and F is the region of the path fill
     */
    reference_counted_ptr<const RenderClipCombineResult>
    combine_clipping(const RenderClipElement &clip_element,
                     const CombinedPath &path,
                     const RenderClipCombineParams &params) const;

    /*!
     * Combine an astral::RenderClipElement with an astral::CombinedPath
     * filled. In contrast to combine_clipping(), this only computes the
     * intersection of the input astral::RenderClipElement with a path
     * fill
     * \param clip_element RenderClipElement with which to combine
     * \param path astral::CombinedPath to fill
     * \param params specifies fill rule and how to perfrom the computation
     * \returns the astral::RenderClipElement which gives only the region
     *          C intersect F where C is the region represented by clip_element
     *          and F is the region of the path fill
     */
    reference_counted_ptr<const RenderClipElement>
    intersect_clipping(const RenderClipElement &clip_element,
                       const CombinedPath &path,
                       const RenderClipCombineParams &params) const;

    /*!
     * Returns true if pixel_rect() or the passed astral::RenderClipElement
     * clips a box
     * \param box bounding box to test
     * \param pixel_transformation_box transformation from box coordinates to
     *                                 pixel coordinates
     * \param clip if non-null, test also if clip clips box
     */
    bool
    clips_box(BoundingBox<float> box,
              const astral::Transformation &pixel_transformation_box,
              const RenderClipElement *clip = nullptr) const;

    /*!
     * Provded as an overload of conveniance, equivalent to
     * \code
     * clips_box(box, transformation(), clip);
     * \endcode
     */
    bool
    clips_box(const BoundingBox<float> &box,
               const RenderClipElement *clip = nullptr) const
    {
      return clips_box(box, transformation(), clip);
    }

    /*!
     * Generate a mask-buffer from stroking a path using a custom
     * shader with custom item data; the mask generated comes
     * from rendering the paths in this encoder's rendering region
     * with the current transformation applied.
     * \param shader how to stroke the paths
     * \param paths paths to clip against
     * \param params stroke path parameters
     * \param packer generator of item data
     * \param mask_properties how to generate the mask
     * \param mask_type specifies mask value type
     * \param out_data cannot be null, location to which to write the
     *                 the path mask and the transformations to use it.
     *                 RenderEncoderStrokeMask::change_mask_mode() is
     *                 can be used with the value written to out_data.
     *
     * TODO: make a form that does not take a StrokeMaskProperties but
     *       instead generates good values from the other arguments.
     */
    void
    generate_mask(const MaskStrokeShader &shader,
                  const CombinedPath &paths,
                  const StrokeParameters &params,
                  const StrokeShader::ItemDataPackerBase &packer,
                  const StrokeMaskProperties &mask_properties,
                  enum mask_type_t mask_type,
                  MaskDetails *out_data) const;

    /*!
     * Generate a mask-buffer from stroking a path using a stroke
     * shader from the default shader set; the mask generated
     * comes from rendering the paths in this encoder's rendering region
     * with the current transformation applied.
     * \param paths paths to clip against
     * \param params stroke path parameters
     * \param mask_properties how to generate the mask
     * \param mask_type specifies mask value type
     * \param out_data cannot be null, location to which to write the
     *                 the path mask and the transformations to use it.
     *                 RenderEncoderStrokeMask::change_mask_mode() is
     *                 can be used with the value written to out_data.
     *
     * TODO: make a form that does not take a StrokeMaskProperties but
     *       instead generates good values from the other arguments.
     */
    void
    generate_mask(const CombinedPath &paths,
                  const StrokeParameters &params,
                  const StrokeMaskProperties &mask_properties,
                  enum mask_type_t mask_type,
                  MaskDetails *out_data) const
    {
      generate_mask(*default_shaders().m_mask_stroke_shader,
                    paths, params, StrokeShader::ItemDataPacker(),
                    mask_properties, mask_type, out_data);
    }

    /*!
     * Generate a mask-buffer for dashed stroking using a shader
     * from the default shader set.
     * \param paths paths to clip against
     * \param params stroke path parameters
     * \param dash_pattern dash pattern to apply
     * \param mask_properties how to generate the mask
     * \param mask_type specifies mask value type
     * \param out_data cannot be null, location to which to write the
     *                 the path mask and the transformations to use it.
     *                 RenderEncoderStrokeMask::change_mask_mode() is
     *                 can be used with the value written to out_data.
     *
     * TODO: make a form that does not take a StrokeMaskProperties but
     *       instead generates good values from the other arguments.
     */
    void
    generate_mask(const CombinedPath &paths,
                  const StrokeParameters &params,
                  const StrokeShader::DashPattern &dash_pattern,
                  const StrokeMaskProperties &mask_properties,
                  enum mask_type_t mask_type,
                  MaskDetails *out_data) const
    {
      generate_mask(*default_shaders().m_mask_dashed_stroke_shader,
                    paths, params, dash_pattern,
                    mask_properties, mask_type, out_data);
    }

    /*!
     * Draw a rect with a custom shader.
     * \param region specifies the area covered by the draw
     * \param rect_item specifies shader and shader data for the draw
     * \param material specifies the material and mask to apply to the draw
     * \param blend_mode blend mode to apply to the draw
     */
    void
    draw_custom_rect(const RectRegion &region, const RectItem &rect_item,
                     const ItemMaterial &material = ItemMaterial(),
                     enum blend_mode_t blend_mode = blend_porter_duff_src_over) const;

    /*!
     * Draw a rect with a custom shader.
     * \param rect specifies the region covered by the draw in logical coordinates
     * \param rect_item specifies shader and shader data for the draw
     * \param material specifies the material and mask to apply to the draw
     * \param blend_mode blend mode to apply to the draw
     */
    void
    draw_custom_rect(const Rect &rect, const RectItem &rect_item,
                     const ItemMaterial &material = ItemMaterial(),
                     enum blend_mode_t blend_mode = blend_porter_duff_src_over) const
    {
      RectRegion R;

      R.m_rect = BoundingBox<float>(rect);
      draw_custom_rect(R, rect_item, material, blend_mode);
    }

    /*!
     * Draw generic attribute data with an astral::ColorItemShader
     * \param rect rectangle covering the entire are of the draw; the
     *             rectangle is in logical coordinates
     * \param item specifies shader, attribute data, and shader data
     * \param material specifies the material and mask to apply to the draw
     * \param blend_mode blend mode to apply to the draw
     */
    void
    draw_custom(const Rect &rect, const Item<ColorItemShader> &item,
                const ItemMaterial &material = ItemMaterial(),
                enum blend_mode_t blend_mode = blend_porter_duff_src_over) const
    {
      RectRegion R;

      R.m_rect = BoundingBox<float>(rect);
      draw_custom(R, item, material, blend_mode);
    }

    /*!
     * Draw generic attribute data with an astral::ColorItemShader
     * \param region rectangle covering the entire are of the draw; the
     *               rectangle is in logical coordinates
     * \param item specifies shader, attribute data, and shader data
     * \param material specifies the material and mask to apply to the draw
     * \param blend_mode blend mode to apply to the draw
     */
    void
    draw_custom(const RectRegion &region, const Item<ColorItemShader> &item,
                const ItemMaterial &material = ItemMaterial(),
                enum blend_mode_t blend_mode = blend_porter_duff_src_over) const;

    /*!
     * Draw generic attribute data with multiple shaders
     * \param region rectangle covering the entire are of the draw; the
     *               rectangle is in logical coordinates
     * \param item specifies shader, attribute data, and shader data
     * \param material specifies the material and mask to apply to the draw
     * \param blend_mode blend mode to apply to the draw
     */
    void
    draw_custom(const RectRegion &region, const ColorItem &item,
                const ItemMaterial &material = ItemMaterial(),
                enum blend_mode_t blend_mode = blend_porter_duff_src_over) const;

    /*!
     * Compute (and return) a Proxy value representing the rendering
     * to a region specified by an astral::RelativeBoundingBox.
     * \param bb the bounding box of the region
     * \param scale_rendering amount by which to scale rendering; a value greater
     *                        than one indicates to render at a higher resolution
     *                        than the area where as a value less than one indicates
     *                        to render at a lower resolution.
     * \param pixel_slack amount of pixels for the image specified specified
     *                    by Proxy::image_size() by which to pad the rendering
     */
    Proxy
    proxy_relative(const RelativeBoundingBox &bb,
                   RenderScaleFactor scale_rendering,
                   unsigned int pixel_slack) const;

    /*!
     * Conveniance overload, equivalent to
     * \code
     * proxy_relative(bb, RenderScaleFactor(), pixel_slack);
     * \endcode
     */
    Proxy
    proxy_relative(const RelativeBoundingBox &bb, unsigned int pixel_slack) const;

    /*!
     * Begin rendering to an offscreen buffer that is used as a mask,
     * the returned astral::RenderEncoderMask will inherit the transformation
     * of the astral::RenderEncoderBase at the time of the Proxy creation.
     * \param proxy Proxy value specifying the region.
     */
    RenderEncoderMask
    encoder_mask(Proxy proxy) const;

    /*!
     * Begin rendering to an offscreen buffer that is used as a mask,
     * the returned astral::RenderEncoderMask will inherit the
     * current transformation and its rendering region will be the
     * intersection of the passed bounding box with this encoder's
     * rendering region. Returned encoder inherits the rendering
     * accuracy and the value of use_sub_ubers() of this encoder.
     * \param bb the bounding box of the mask
     * \param pixel_slack amount of pixels for RenderEncoderMask::image()
     *                    by which to pad the rendering
     * \returns astral::RenderEncoderMask to render the offscreen buffer
     */
    RenderEncoderMask
    encoder_mask_relative(const RelativeBoundingBox &bb,
                          unsigned int pixel_slack = 1u) const;

    /*!
     * Begin rendering to an offscreen buffer that is used as a mask,
     * the returned astral::RenderEncoderMask will inherit the
     * current transformation and its rendering region will be the
     * intersection of the passed bounding box with this encoder's
     * rendering region. Returned encoder inherits the rendering
     * accuracy and the value of use_sub_ubers() of this encoder.
     * \param bb the bounding box, in logical coordinates of the mask
     * \param scale_rendering amount by which to scale rendering; a value greater
     *                        than one indicates to render at a higher resolution
     *                        than the area where as a value less than one indicates
     *                        to render at a lower resolution.
     * \param pixel_slack amount of pixels for RenderEncoderMask::image()
     *                    by which to pad the rendering
     * \returns astral::RenderEncoderMask to render the offscreen buffer
     */
    RenderEncoderMask
    encoder_mask_relative(const RelativeBoundingBox &bb,
                          RenderScaleFactor scale_rendering,
                          unsigned int pixel_slack = 1u) const;

    /*!
     * Begin rendering to an offscreen buffer that is used as a mask,
     * the returned astral::RenderEncoderMask does not inherit
     * the current transformation and its rendering region is set
     * as the rect at (0, 0) with the passed size. Returned encoder
     * inherits the rendering accuracy and the value of use_sub_ubers()
     * of this encoder.
     * \param size size of the mask to render.
     * \returns astral::RenderEncoderMask to render the offscreen buffer
     */
    RenderEncoderMask
    encoder_mask(ivec2 size) const;

    /*!
     * Begin rendering to an offscreen buffer that is used as an image,
     * the returned astral::RenderEncoderImage will inherit the transformation
     * of the astral::RenderEncoderBase at the time of the Proxy creation.
     * \param proxy Proxy value specifying the region.
     * \param colorspace the colorspace in which to render the content
     */
    RenderEncoderImage
    encoder_image(Proxy proxy, enum colorspace_t colorspace) const;

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * encoder_image(proxy, colorspace())
     * \endcode
     */
    RenderEncoderImage
    encoder_image(Proxy proxy) const;

    /*!
     * Begin rendering to an offscreen buffer astral::RenderEncoderImage
     * will inherit the current transformation and its rendering region
     * will be the intersection of the passed bounding box with this
     * encoder's rendering region. Returned encoder inherits the rendering
     * accuracy and the value of use_sub_ubers() of this encoder.
     * \param bb the bounding box, in logical coordinates, of the buffer.
     * \param colorspace the colorspace in which to render the content
     * \param pixel_slack amount of pixels for RenderEncoderImage::image()
     *                    by which to pad the rendering
     * \returns astral::RenderEncoderImage to render the offscreen buffer
     */
    RenderEncoderImage
    encoder_image_relative(const RelativeBoundingBox &bb,
                           enum colorspace_t colorspace,
                           unsigned int pixel_slack = 0u) const;

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * encoder_image_relative(bb, colorspace(), pixel_slack);
     * \endcode
     */
    RenderEncoderImage
    encoder_image_relative(const RelativeBoundingBox &bb,
                           unsigned int pixel_slack = 0u) const;

    /*!
     * Begin rendering to an offscreen buffer astral::RenderEncoderImage
     * will inherit the current transformation and its rendering region
     * will be the intersection of the passed bounding box with this
     * encoder's rendering region. Returned encoder inherits the rendering
     * accuracy and the value of use_sub_ubers() of this encoder.
     * \param bb the bounding box, in logical coordinates of the buffer.
     * \param scale_rendering amount by which to scale rendering; a value greater
     *                        than one indicates to render at a higher resolution
     *                        than the area where as a value less than one indicates
     *                        to render at a lower resolution.
     * \param colorspace the colorspace in which to render the content
     * \param pixel_slack amount of pixels for RenderEncoderImage::image()
     *                    by which to pad the rendering
     * \returns astral::RenderEncoderImage to render the offscreen buffer
     */
    RenderEncoderImage
    encoder_image_relative(const RelativeBoundingBox &bb,
                           RenderScaleFactor scale_rendering,
                           enum colorspace_t colorspace,
                           unsigned int pixel_slack = 0u) const;

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * encoder_image_relative(bb, scale_rendering, colorspace(), pixel_slack);
     * \endcode
     */
    RenderEncoderImage
    encoder_image_relative(const RelativeBoundingBox &bb,
                           RenderScaleFactor scale_rendering,
                           unsigned int pixel_slack = 0u) const;

    /*!
     * Begin rendering to an offscreen buffer the returned
     * astral::RenderEncoderImage does not inherit the current
     * transformation and its rendering region is set as the rect
     * at (0, 0) with the passed size. Returned encoder
     * inherits the rendering accuracy and the value of use_sub_ubers()
     * of this encoder.
     * \param size size of the mask to render.
     * \param colorspace the colorspace in which to render the content
     * \returns astral::RenderEncoderImage to render the offscreen buffer
     */
    RenderEncoderImage
    encoder_image(ivec2 size, enum colorspace_t colorspace) const;

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * encoder_image(size, colorspace());
     * \endcode
     */
    RenderEncoderImage
    encoder_image(ivec2 size) const;

    /*!
     * Render to an offcreen layer that is blitted on end_layer() with the
     * specified astral::blend_mode_t to this astral::RenderEncoderBase
     * modulated by the specified color. The encoder of the returned value
     * will inherit the current transformation and its rendering region
     * will be the intersection of the passed bounding box with this
     * encoder's rendering region. Returned encoder also inherits the
     * rendering accuracy and the value of use_sub_ubers() of this encoder.
     * \param bb the bounding box in logical coordinate of the layer
     * \param colorspace the colorspace in which to render the content
     * \param scale_rendering amount by which to scale rendering; a value greater
     *                        than one indicates to render at a higher resolution
     *                        than the area where as a value less than one indicates
     *                        to render at a lower resolution.
     * \param color color by which to modulate the rendered content
     * \param blend_mode blend mode to apply
     * \param filter_mode filter to apply to image
     * \param clip optional clipping to apply the layer
     * \returns astral::RenderEncoderImage to render the offscreen buffer
     */
    RenderEncoderLayer
    begin_layer(const BoundingBox<float> &bb, RenderScaleFactor scale_rendering,
                enum colorspace_t colorspace, const vec4 &color,
                enum blend_mode_t blend_mode = blend_porter_duff_src_over,
                enum filter_t filter_mode = filter_linear,
                const ItemMask &clip = ItemMask()) const;

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * begin_layer(bb, scale_rendering, colorspace(),
     *             color, blend_mode, filter_mode, clip);
     * \endcode
     */
    RenderEncoderLayer
    begin_layer(const BoundingBox<float> &bb,
                RenderScaleFactor scale_rendering, const vec4 &color,
                enum blend_mode_t blend_mode = blend_porter_duff_src_over,
                enum filter_t filter_mode = filter_linear,
                const ItemMask &clip = ItemMask()) const;

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * begin_layer(bb, vec2(1.0f, 1.0f), colorspace,
     *             color, blend_mode, filter_mode, clip);
     * \endcode
     */
    RenderEncoderLayer
    begin_layer(const BoundingBox<float> &bb,
                enum colorspace_t colorspace, const vec4 &color,
                enum blend_mode_t blend_mode = blend_porter_duff_src_over,
                enum filter_t filter_mode = filter_linear,
                const ItemMask &clip = ItemMask()) const;

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * begin_layer(bb, vec2(1.0f, 1.0f), colorspace(),
     *             color, blend_mode, filter_mode, clip);
     * \endcode
     */
    RenderEncoderLayer
    begin_layer(const BoundingBox<float> &bb,
                const vec4 &color,
                enum blend_mode_t blend_mode = blend_porter_duff_src_over,
                enum filter_t filter_mode = filter_linear,
                const ItemMask &clip = ItemMask()) const;

    /*!
     * Equivalent to
     * \code
     * begin_layer(bb, colorspace, vec4(1.0f, 1.0f, 1.0f, alpha), blend_mode, clip);
     * \endcode
     */
    RenderEncoderLayer
    begin_layer(const BoundingBox<float> &bb,
                enum colorspace_t colorspace, float alpha,
                enum blend_mode_t blend_mode = blend_porter_duff_src_over,
                const ItemMask &clip = ItemMask()) const;

    /*!
     * Equivalent to
     * \code
     * begin_layer(bb, colorspace(), vec4(1.0f, 1.0f, 1.0f, alpha), blend_mode, clip);
     * \endcode
     */
    RenderEncoderLayer
    begin_layer(const BoundingBox<float> &bb, float alpha,
                enum blend_mode_t blend_mode = blend_porter_duff_src_over,
                const ItemMask &clip = ItemMask()) const;

    /*!
     * Render to an offcreen layer that is blitted on end_layer() with
     * the specified astral::Effect. The encoder of the returned value
     * will inherit the current transformation and its rendering region
     * will be the intersection of the passed bounding box with this
     * encoder's rendering region. Returned encoder also inherits the
     * rendering accuracy and the value of use_sub_ubers() of this encoder.
     * \param effect astral::Effect to apply
     * \param effect_parameters parameters for the astral::Effect
     * \param bb the bounding box in effect coordinate of the layer
     * \param colorspace the colorspace in which to render the content
     *                   to which the effect applies
     * \param blend_mode blend mode to apply when blittinf the astral::Effect
     * \param clip the clipping to apply to the effect
     * \returns astral::RenderEncoderImage to render the offscreen buffer
     *          to which the effect is applied
     */
    RenderEncoderLayer
    begin_layer(const Effect &effect,
                const EffectParameters &effect_parameters,
                const BoundingBox<float> &bb,
                enum colorspace_t colorspace,
                enum blend_mode_t blend_mode = blend_porter_duff_src_over,
                const ItemMask &clip = ItemMask()) const;

    /*!
     * Equivalent to
     * \code
     * begin_layer(effect, effect_parameters, bb, colorspace(), blend_mode, clip);
     * \endcode
     */
    RenderEncoderLayer
    begin_layer(const Effect &effect,
                const EffectParameters &effect_parameters,
                const BoundingBox<float> &bb,
                enum blend_mode_t blend_mode = blend_porter_duff_src_over,
                const ItemMask &clip = ItemMask()) const;

    /*!
     * Render to an offcreen layer that is blitted on end_layer() with
     * the specified astral::EffectCollectionBase. The encoder of the
     * returned value will inherit the current transformation and its
     * rendering region will be the intersection of the passed bounding
     * box with this encoder's rendering region. Returned encoder also
     * inherits the rendering accuracy and the value of use_sub_ubers()
     * of this encoder.
     * \param effects the collection of effects, the object is not saved.
     *                It is examined when begin_layer() is called.
     * \param bb the bounding box in effect coordinate of the layer
     * \param colorspace the colorspace in which to render the content
     *                   to which the effect applies
     * \param clip the clipping to apply to the effect
     * \returns astral::RenderEncoderImage to render the offscreen buffer
     *          to which the effect is applied
     */
    RenderEncoderLayer
    begin_layer(const EffectCollectionBase &effects,
                const BoundingBox<float> &bb,
                enum colorspace_t colorspace,
                const ItemMask &clip = ItemMask()) const;
    /*!
     * Equivalent to
     * \code
     * begin_layer(effects, bb, colorspace(), clip);
     * \endcode
     */
    RenderEncoderLayer
    begin_layer(const EffectCollectionBase &effects,
                const BoundingBox<float> &bb,
                const ItemMask &clip = ItemMask()) const;

    /*!
     * Finish the passed encoder (if not already finished) and
     * blit its contents. Calling end_layer() with the same
     * astral::RenderEncoderLayer valiue is an error; in debug
     * build will assert and in release build will be ignored.
     * \param layer RenderEncoderLayer as returned by begin_layer().
     */
    void
    end_layer(RenderEncoderLayer layer) const;

    /*!
     * Begin an offscreen render of a shadow map. Returned encoder
     * inherits the rendering accuracy and the value of use_sub_ubers()
     * of this encoder.
     * \param dimensions provides the width and height of the shadow map,
     *                   recall that an astral::RenderEncoderShadowMap
     *                   produces an astral::ShadowMap and that the
     *                   number of pixels consumed is O(dimensions)
     * \param light_p position of light
     */
    RenderEncoderShadowMap
    encoder_shadow_map(int dimensions, vec2 light_p) const;

    /*!
     * Begin an offscreen render of a shadow map where the returned
     * encoder's logical coordinates are the same as the current
     * logical coordinates. Returned encoder inherits the rendering
     * accuracy and the value of use_sub_ubers() of this encoder.
     * The astral::ShadowMap coordinates made by the returned
     * astral::RenderEncoderShadowMap are the same as pixel coordinates.
     * \param dimensions provides the width and height of the shadow map,
     *                   recall that an astral::RenderEncoderShadowMap
     *                   produces an astral::ShadowMap and that the
     *                   number of pixels consumed is O(dimensions)
     * \param light_p position of light in -logical- coordinates
     */
    RenderEncoderShadowMap
    encoder_shadow_map_relative(int dimensions, vec2 light_p) const;

    /*!
     * Create an astral::RenderEncoderStrokeMask used to generate a mask
     * from stroking paths. The returned object inherits the current value
     * of transformation().
     */
    RenderEncoderStrokeMask
    encoder_stroke(const StrokeMaskProperties &mask_properties) const;

    /*!
     * Returns an image whose contents are the result of all rendering
     * commands currently in the command time line over the specified
     * area.
     * \param src_encoder encoder from which to take a snapshot; it
     *                    is assumed that the PixelCoordinates of src_encoder
     *                    is the same as the PixelCoordinates of this encoder
     * \param scale_rendering amount by which to scale rendering; a value greater
     *                        than one indicates to realized the image at a higher
     *                        resolution than the area where as a value less than
     *                        one indicates to realize at a lower resolution.
     * \param logical_bb bounding box in current *logical* coordinates
     * \param out_image_transformation_logical if non-null, location to which to
     *                                         write the transformation from
     *                                         current logical coordinates to the
     *                                         image coordiates of the image
     *                                         returned by RenderEncoderImage::image()
     * \param pixel_slack amount of pixels for RenderEncoderImage::image()
     *                    by which to pad the rendering
     * \param lod_requirement the highest level LOD needed for the returned image
     */
    reference_counted_ptr<Image>
    snapshot_logical(RenderEncoderBase src_encoder,
                     const RelativeBoundingBox &logical_bb,
                     RenderScaleFactor scale_rendering,
                     Transformation *out_image_transformation_logical = nullptr,
                     unsigned int pixel_slack = 0u,
                     unsigned int lod_requirement = 0) const;

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * snapshot_logical(src_encoder, true, RenderScaleFactor(),
     *                  logical_bb,
     *                  out_image_transformation_logical,
     *                  pixel_slack,
     *                  lod_requirement);
     * \endcode
     */
    reference_counted_ptr<Image>
    snapshot_logical(RenderEncoderBase src_encoder,
                     const RelativeBoundingBox &logical_bb,
                     Transformation *out_image_transformation_logical = nullptr,
                     unsigned int pixel_slack = 0u,
                     unsigned int lod_requirement = 0) const
    {
      return snapshot_logical(src_encoder,
                              logical_bb, RenderScaleFactor(),
                              out_image_transformation_logical,
                              pixel_slack,
                              lod_requirement);
    }

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * snapshot_logical(*this, true, scale_rendering,
     *                  logical_bb,
     *                  out_image_transformation_logical,
     *                  pixel_slack,
     *                  lod_requirement);
     * \endcode
     */
    reference_counted_ptr<Image>
    snapshot_logical(const RelativeBoundingBox &logical_bb,
                     RenderScaleFactor scale_rendering,
                     Transformation *out_image_transformation_logical = nullptr,
                     unsigned int pixel_slack = 0u,
                     unsigned int lod_requirement = 0) const
    {
      return snapshot_logical(*this, logical_bb, scale_rendering,
                              out_image_transformation_logical,
                              pixel_slack,
                              lod_requirement);
    }

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * snapshot_logical(*this, true, RenderScaleFactor(),
     *                  logical_bb,
     *                  out_image_transformation_logical,
     *                  pixel_slack,
     *                  lod_requirement);
     * \endcode
     */
    reference_counted_ptr<Image>
    snapshot_logical(const RelativeBoundingBox &logical_bb,
                     Transformation *out_image_transformation_logical = nullptr,
                     unsigned int pixel_slack = 0u,
                     unsigned int lod_requirement = 0) const
    {
      return snapshot_logical(*this, logical_bb, RenderScaleFactor(),
                              out_image_transformation_logical,
                              pixel_slack,
                              lod_requirement);
    }

    /*!
     * Returns a material whose contents are the result of those rendering
     * commands over the specified area with an effect applied.
     *
     * \param src_encoder encoder from which to take a snapshot; it
     *                    is assumed that the PixelCoordinates of src_encoder
     *                    is the same as the PixelCoordinates of this encoder
     * \param effect astral::Effect to apply
     * \param effect_parameters parameters of the astral::Effect
     * \param logical_bb bounding box in current *logical* coordinates
     * \param out_material location to which to write the resulting EffectMaterial value
     */
    void
    snapshot_effect(RenderEncoderBase src_encoder,
                    const Effect &effect,
                    c_array<const generic_data> effect_parameters,
                    const RelativeBoundingBox &logical_bb,
                    EffectMaterial *out_material) const;

    /*!
     * Returns a material whose contents are the result of those rendering
     * commands over the specified area with an effect applied.
     * \param effect astral::Effect to apply
     * \param effect_parameters parameters of the astral::Effect
     * \param logical_bb bounding box in current *logical* coordinates
     * \param out_material location to which to write the resulting EffectMaterial value
     */
    void
    snapshot_effect(const Effect &effect,
                    c_array<const generic_data> effect_parameters,
                    const RelativeBoundingBox &logical_bb,
                    EffectMaterial *out_material) const
    {
      snapshot_effect(*this, effect, effect_parameters, logical_bb, out_material);
    }

    /*!
     * Indicates to the renderer that the caller is entering a section
     * of drawing where the draws are guaranteed to have no overlap.
     */
    void
    begin_pause_snapshot(void) const;

    /*!
     * Indicates to the renderer that the caller is leaving a section
     * of drawing where the draws are guaranteed to have no overlap.
     */
    void
    end_pause_snapshot(void) const;

    /*!
     * Returns true exactly when inside a pause snapshot sessions.
     */
    bool
    snapshot_paused(void) const
    {
      return pause_snapshot_depth() != 0;
    }

    /*!
     * Explicitely add an astral::Image dependency to the contents
     * rendered by the encoder.
     */
    void
    add_dependency(const Image &image) const;

    /*!
     * Indicates that no additional drawing commands are to be added
     * to this astral::RenderEncoderBase. In addition, it will also issue
     * finish() on any encoder objects that generate pixels on which this
     * encoder depends.
     */
    void
    finish(void) const;

  private:
    friend class Renderer;
    friend class RenderEncoderMask;
    friend class RenderEncoderImage;
    friend class RenderEncoderSurface;
    friend class RenderEncoderShadowMap;
    friend class RenderClipNode;
    friend class RenderEncoderStrokeMask;
    friend class RenderEncoderLayer;

    class Details;
    typedef Renderer::VirtualBuffer VirtualBuffer;

    RenderEncoderBase(VirtualBuffer *p):
      m_virtual_buffer(p)
    {}

    VirtualBuffer&
    virtual_buffer(void) const;

    Renderer::Implement&
    renderer_implement(void) const;

    /* Draw custom vertex data with a custom shader and instead
     * of using the current transformation, use the passed one;
     * used only by derived classes to submit custom attribute data.
     * \param transformation transformation from item coordinates to pixels
     * \param item specifies shader, shader data and vertex data for the draw
     */
    void
    draw_generic_private(RenderValue<Transformation> transformation,
                         const Item<MaskItemShader> &item,
                         const ItemMask &clip,
                         enum mask_item_shader_clip_mode_t clip_mode) const;

    void
    draw_generic_private(RenderValue<Transformation> transformation, const Item<ShadowMapItemShader> &item) const;

    /* Returns the depth of the pause snapshot sessions,
     * a non-zero value indicates inside a pause snapshot
     * session.
     */
    int
    pause_snapshot_depth(void) const;

    void
    pause_snapshot_depth(int v) const;

    VirtualBuffer *m_virtual_buffer;
  };

  /*!
   * \brief
   * A astral::RenderEncoderImage represents a handle to
   * drawing to an offscreen buffer. The final image data
   * returned by astral::RenderEncoderImage::image() is
   * with alpha pre-multiplied.
   */
  class RenderEncoderImage:public RenderEncoderBase
  {
  public:
    RenderEncoderImage(void):
      RenderEncoderBase()
    {}

    /*!
     * If true, when performing determining what tiles are
     * to be backed, use the pixel rects instead of clip
     * equations. This can be faster at the potential cost
     * of more tiles getting backed that have no content.
     */
    bool
    use_pixel_rect_tile_culling(void) const;

    /*!
     * Sets the value returned by use_pixel_rect_tile_culling(void) const.
     */
    void
    use_pixel_rect_tile_culling(bool v) const;

    /*!
     * Returns a reference to the astral::Image generated from the
     * commands added to this astral::RenderEncoderImage if finish()
     * has been called. Otherwise returns a null-pointer.
     */
    reference_counted_ptr<Image>
    image(void) const;

    /*!
     * Returns the astral::Image generated from the commands
     * added to this astral::RenderEncoderImage with the specified
     * number of mipmap levels created if finish() has been called.
     * Otherwise returns a null-pointer.
     */
    reference_counted_ptr<Image>
    image_with_mips(unsigned int max_lod) const;

    /*!
     * Returns the astral::Image that contains a single
     * astral::ImageMipElement element in its Image::mip_chain()
     * that corresponds that is the same as the last element
     * of image_with_mips().
     * \param LOD LOD requested
     * \param actualLOD the LOD that the returned image's
     *                  LOD = 0 corresponds to.
     */
    reference_counted_ptr<Image>
    image_last_mip_only(unsigned int LOD, unsigned int *actualLOD) const;

    /*!
     * Returns an astral::RenderClipElement corresponding to
     * using a channel of the image for clipping/coverage.
     * If finish() has not been called, returns a nullptr.
     */
    reference_counted_ptr<const RenderClipElement>
    clip_element(enum mask_type_t mask_type,
                 enum mask_channel_t mask_channel) const;

    /*!
     * Gives the astral::ScaleTranslate mapping from pixel_bounding_box()
     * to astral::image().
     */
    const ScaleTranslate&
    image_transformation_pixel(void) const;

  private:
    friend class Renderer;
    friend class RenderEncoderBase;
    friend class RenderEncoderMask;
    friend class RenderClipNode;
    friend class RenderEncoderLayer;

    RenderEncoderImage(RenderEncoderBase base):
      RenderEncoderBase(base)
    {}
  };

  /*!
   * An astral::RenderEncoderLayer represents rendering
   * to an astral::Image that is blitted.
   */
  class RenderEncoderLayer
  {
  public:
    /*!
     * Default ctor that initializes this astral::RenderEncoderLayer
     * as a nullptr where it has backing.
     */
    RenderEncoderLayer(void):
      m_backing(nullptr)
    {}

    /*!
     * Returns true if and only if this astral::RenderEncoderLayer
     * is valid, i.e. it can be used for rendering. If valid()
     * returns false, then it is illegal to call any method
     * except valid(). This method does NOT take into account
     * if Renderer::finish() was called.
     */
    bool
    valid(void) const
    {
      return m_backing != nullptr;
    }

    /*!
     * Returns the encoder to which to add rendering commands
     * for the content that it is to be blitted.
     */
    RenderEncoderImage
    encoder(void) const;

    /*!
     * Returns the encoder that created this RenderEncoderLayer
     * an invalid value indicates this RenderEncoderLayer is
     * essentially a nullptr.
     */
    RenderEncoderBase
    parent_encoder(void) const;

    /*!
     * Returns true if RenderEncoderBase::end_layer() has been
     * called on this astral::RenderEncoderLayer; also returns
     * true if this astral::RenderEncoderLayer has no backing.
     */
    bool
    ended(void) const;

  private:
    friend class Renderer;
    friend class RenderEncoderBase;

    class Backing;

    explicit
    RenderEncoderLayer(Backing *p):
      m_backing(p)
    {}

    Backing *m_backing;
  };

  /*!
   * An astral::RenderClipNode is a container for
   * two encoders: one for content to be clipped in and
   * another for content to be clipped out.
   */
  class RenderClipNode
  {
  public:
    RenderClipNode(void):
      m_backing(nullptr)
    {}

    /*!
     * Returns the encoder that created this RenderClipNode;
     * an invalid value indicates this RenderClipNode is
     * essentially a nullptr.
     */
    RenderEncoderBase
    parent_encoder(void) const;

    /*!
     * The encoder for clipped-in content. An
     * invalid or degenerate value indicates that
     * all clipped-in content is not drawn.
     */
    RenderEncoderImage
    clip_in(void) const;

    /*!
     * The encoder for clipped-out content An
     * invalid or degenerate value indicates that
     * all clipped-out content is not drawn.
     */
    RenderEncoderImage
    clip_out(void) const;

    /*!
     * Returns true if RenderEncoderBase::end_clip_node() has
     * been called on this astral::RenderClipNode; also returns
     * true if this astral::RenderClipNode has no backing.
     */
    bool
    ended(void) const;

  private:
    friend class Renderer;
    friend class RenderEncoderBase;
    class Backing;

    explicit
    RenderClipNode(Backing *p):
      m_backing(p)
    {}

    Backing *m_backing;
  };

  /*!
   * \brief
   * A astral::RenderEncoderMask represents rendering to an
   * offscreen image that will be used as a mask. When rendering
   * via a astral::RenderEncoderMask, the blend mode is always
   * astral::blend_mode_max (which implies rendering ordering
   * does not matter). The format of both the return value of
   * astral::RenderEncoderMask::image() and the pixels rendered
   * by astral::RenderEncoderMask are the output format of
   * astral::image_blit_stc_mask_processing
   *
   * In addition, one can add direct renders to an
   * astral::RenderEncoderMask to generate custom mask values.
   */
  class RenderEncoderMask:private RenderEncoderImage
  {
  public:
    RenderEncoderMask(void):
      RenderEncoderImage()
    {}

    /*!
     * Typedef to define "what" to draw
     */
    typedef RenderEncoderImage::Item<MaskItemShader> Item;

    /*!
     * Conveniance typedef
     */
    typedef RenderEncoderImage::AutoRestore AutoRestore;

    using RenderEncoderImage::renderer;
    using RenderEncoderImage::render_engine;
    using RenderEncoderImage::finished;
    using RenderEncoderImage::render_scale_factor;
    using RenderEncoderImage::valid;
    using RenderEncoderImage::create_value;
    using RenderEncoderImage::create_item_data;
    using RenderEncoderImage::render_accuracy;
    using RenderEncoderImage::compute_tolerance;
    using RenderEncoderImage::transformation;
    using RenderEncoderImage::singular_values;
    using RenderEncoderImage::surface_pixel_size_in_logical_coordinates;
    using RenderEncoderImage::inverse_transformation;
    using RenderEncoderImage::concat;
    using RenderEncoderImage::translate;
    using RenderEncoderImage::scale;
    using RenderEncoderImage::rotate;
    using RenderEncoderImage::save_transformation;
    using RenderEncoderImage::save_transformation_count;
    using RenderEncoderImage::restore_transformation;
    using RenderEncoderImage::default_shaders;
    using RenderEncoderImage::default_effects;
    using RenderEncoderImage::encoder_mask_relative;
    using RenderEncoderImage::encoder_mask;
    using RenderEncoderImage::encoder_image_relative;
    using RenderEncoderImage::encoder_image;
    using RenderEncoderImage::finish;
    using RenderEncoderImage::image;
    using RenderEncoderImage::pixel_bounding_box;
    using RenderEncoderImage::image_transformation_pixel;
    using RenderEncoderImage::encoder_shadow_map;
    using RenderEncoderImage::encoder_shadow_map_relative;

    /*!
     * Add strokes of a path to the mask using the defualt shader
     * for astral::with_anti_aliasing
     * \param paths paths to stroke
     * \param stroke_params how to stroke
     * \param clip clipping, if any to apply
     * \param clip_mode specifies how to apply clipping if clipping is present
     */
    void
    add_path_strokes(const CombinedPath &paths,
                     const StrokeParameters &stroke_params,
                     const ItemMask &clip = ItemMask(),
                     enum mask_item_shader_clip_mode_t clip_mode = mask_item_shader_clip_cutoff) const
    {
      add_path_strokes(*default_shaders().m_mask_stroke_shader,
                       paths, stroke_params,
                       StrokeShader::ItemDataPacker(),
                       clip, clip_mode);
    }

    /*!
     * Add strokes of a path dashed to the mask using a shader
     * from the default shader set
     * \param paths paths to stroke
     * \param stroke_params how to stroke
     * \param dash_pattern dash pattern to apply
     * \param clip clipping, if any to apply
     * \param clip_mode specifies how to apply clipping if clipping is present
     */
    void
    add_path_strokes(const CombinedPath &paths,
                     const StrokeParameters &stroke_params,
                     const StrokeShader::DashPattern &dash_pattern,
                     const ItemMask &clip = ItemMask(),
                     enum mask_item_shader_clip_mode_t clip_mode = mask_item_shader_clip_cutoff) const
    {
      add_path_strokes(*default_shaders().m_mask_dashed_stroke_shader,
                       paths, stroke_params, dash_pattern, clip, clip_mode);
    }

    /*!
     * Add strokes of a path to the mask
     * \param shader shader with which to stroke
     * \param paths paths to stroke
     * \param stroke_params how to stroke
     * \param packer packer of the item data for the shader
     * \param clip clipping, if any to apply
     * \param clip_mode specifies how to apply clipping if clipping is present
     */
    void
    add_path_strokes(const MaskStrokeShader &shader,
                     const CombinedPath &paths,
                     const StrokeParameters &stroke_params,
                     const StrokeShader::ItemDataPackerBase &packer,
                     const ItemMask &clip = ItemMask(),
                     enum mask_item_shader_clip_mode_t clip_mode = mask_item_shader_clip_cutoff) const;

    /*!
     * Draw custom vertex data with a custom shader
     * \param item specifies shader, shader data and vertex data for the draw
     * \param clip clipping to apply to mask shading
     * \param clip_mode specifies how to apply clipping if clipping is present
     */
    void
    draw_generic(const Item &item, const ItemMask &clip = ItemMask(),
                 enum mask_item_shader_clip_mode_t clip_mode = mask_item_shader_clip_cutoff) const
    {
      RenderEncoderBase::draw_generic_private(transformation_value(), item, clip, clip_mode);
    }

    /*!
     * Draw custom vertex data with a custom shader
     * \param transformation instead of using the current value of transformation()
     *                       for the mapping from item coordiantes, use this passed
     *                       value instead.
     * \param item specifies shader, shader data and vertex data for the draw
     * \param clip clipping to apply to mask shading
     * \param clip_mode specifies how to apply clipping if clipping is present
     */
    void
    draw_generic(RenderValue<Transformation> transformation, const Item &item,
                 const ItemMask &clip = ItemMask(),
                 enum mask_item_shader_clip_mode_t clip_mode = mask_item_shader_clip_cutoff) const
    {
      RenderEncoderBase::draw_generic_private(transformation, item, clip, clip_mode);
    }

  private:
    friend class Renderer;
    friend class RenderEncoderBase;
    friend class RenderEncoderStrokeMask;

    RenderEncoderMask(RenderEncoderBase base):
      RenderEncoderImage(base)
    {}
  };

  /*!
   * \brief
   * A astral::RenderEncoderSurface represents drawing to an
   * astral::RenderTarget. The writes to the astral::RenerTarget
   * performed by astral::RenderEncoderSurface are witg alpha
   * pre-multiplied, this is the same as the final output of
   * astral::RenderEncoderImage.
   */
  class RenderEncoderSurface:public RenderEncoderBase
  {
  public:
    RenderEncoderSurface(void):
      RenderEncoderBase()
    {}

  private:
    friend class Renderer;

    RenderEncoderSurface(VirtualBuffer *r):
      RenderEncoderBase(r)
    {}
  };

  /*!
   * \brief
   * A RenderEncoderShadowMap represents a handle drawing
   * content to an astral::ShadowMap
   */
  class RenderEncoderShadowMap:private RenderEncoderBase
  {
  public:
    RenderEncoderShadowMap(void):
      RenderEncoderBase()
    {}

    /*!
     * Typedef to define "what" to draw
     */
    typedef RenderEncoderBase::Item<ShadowMapItemShader> Item;

    /*!
     * Conveniance typedef
     */
    typedef RenderEncoderBase::AutoRestore AutoRestore;

    using RenderEncoderBase::valid;
    using RenderEncoderBase::renderer;
    using RenderEncoderBase::render_engine;
    using RenderEncoderBase::finished;
    using RenderEncoderBase::render_scale_factor;
    using RenderEncoderBase::create_value;
    using RenderEncoderBase::create_item_data;
    using RenderEncoderBase::render_accuracy;
    using RenderEncoderBase::compute_tolerance;
    using RenderEncoderBase::transformation;
    using RenderEncoderBase::singular_values;
    using RenderEncoderBase::inverse_transformation;
    using RenderEncoderBase::concat;
    using RenderEncoderBase::translate;
    using RenderEncoderBase::scale;
    using RenderEncoderBase::rotate;
    using RenderEncoderBase::save_transformation;
    using RenderEncoderBase::save_transformation_count;
    using RenderEncoderBase::restore_transformation;
    using RenderEncoderBase::default_shaders;
    using RenderEncoderBase::default_effects;

    /*!
     * Indicates to end drawing and return a reference to the
     * astral::ShadowMap
     */
    reference_counted_ptr<ShadowMap>
    finish(void) const;

    /*!
     * Draw custom vertex data with a custom shader
     * \param item item to draw
     */
    void
    draw_generic(const Item &item) const
    {
      RenderEncoderBase::draw_generic_private(transformation_value(), item);
    }

    /*!
     * Draw custom vertex data with a custom shader
     * \param transformation instead of using the current value of transformation()
     *                       for the mapping from item coordiantes, use this passed
     *                       value instead.
     * \param item specifies shader, shader data and vertex data for the draw
     */
    void
    draw_generic(RenderValue<Transformation> transformation, const Item &item) const
    {
      RenderEncoderBase::draw_generic_private(transformation, item);
    }

    /*!
     * Add additional path geometry to the shadow map;
     * the path geometry itself (not the boundary of
     * its fill) are added as occluders.
     * \param paths CombinedPath to add
     * \param include_implicit_closing_edge if false, do not have the
     *                                      implicit closing edges of any
     *                                      open contour cast a shadow
     */
    void
    add_path(const CombinedPath &paths, bool include_implicit_closing_edge = true) const;

  private:
    friend class RenderEncoderBase;

    explicit
    RenderEncoderShadowMap(RenderEncoderBase p):
      RenderEncoderBase(p)
    {}
  };

  /*!
   * An astral::RenderEncoderStrokeMask is used to specify a
   * set of astral::Path and astral::AnimatedPath along with
   * how to render them to generate a mask.
   */
  class RenderEncoderStrokeMask
  {
  public:
    /*!
     * When stroking, for any geometry added, there are three
     * coordinate systems:
     *  - pixel coordinates which are the pixel coordinates
     *    of the astral::RenderEncoderBase that generated the
     *    astral::RenderEncoderStrokeMask
     *  - logical coordinates are the coordinates in which the
     *    stroking takes places
     *  - path coordinates are the coordinate of the path's
     *    geometry
     *  .
     */
    enum transformation_type_t
      {
        /*!
         * Transformation from logical coordinates to pixel coordinates
         */
        pixel_transformation_logical,

        /*!
         * Transformation from path coordinates to logical coordinates
         */
        logical_transformation_path,
      };

    RenderEncoderStrokeMask(void):
      m_builder(nullptr)
    {}

    /*!
     * Returns true if and only if this astral::RenderEncoderStrokeMask
     * is valid, i.e. it can be used for rendering. If valid() returns
     * false, then it is illegal to call any method except valid(). This
     * method does NOT take into account if Renderer::finish() was called.
     */
    bool
    valid(void) const
    {
      return m_builder != nullptr;
    }

    /*!
     * Returns the astral::Renderer ancestor of this
     * astral::RenderEncoderStrokeMask
     */
    Renderer&
    renderer(void) const;

    /*!
     * Returne the astral::RenderEngine of renderer().
     */
    RenderEngine&
    render_engine(void) const;

    /*!
     * Returns the astral::StrokeMaskProperties value used for
     * creating this astral::RenderEncoderStrokeMask
     */
    const StrokeMaskProperties&
    mask_params(void) const;

    /*!
     * Returns the scaling factor from pixel coordinates to
     * the the render destination (be it a astral::RenderTarget
     * or astral::Image). A value of 1.0 means that a pixel in
     * RenderEncoderBase is a pixel in the render destination.
     * A value of less than 1.0 means that a single pixel in the
     * render destination is multiple pixels in astral::RenderEncoderBase.
     */
    vec2
    render_scale_factor(void) const;

    /*!
     * Returns true if and only if this astral::RenderEncoderStrokeMask
     * has been finished.
     */
    bool
    finished(void) const;

    /*!
     * Returns the accuracy, in pixels, used when approximating curves
     * by lines and/or other curves.
     */
    float
    render_accuracy(void) const;

    /*!
     * Set the accuracy, in pixels, used when approximating curves
     * by lines and/or other curves.
     */
    void
    render_accuracy(float v) const;

    /*!
     * Returns the current transformation value for the
     * named transformation
     */
    const Transformation&
    transformation(enum transformation_type_t) const;

    /*!
     * Returns the singular values of transformation().
     */
    vec2
    singular_values(enum transformation_type_t) const;

    /*!
     * Returns the inverse of the current transformation value.
     */
    const Transformation&
    inverse_transformation(enum transformation_type_t) const;

    /*!
     * Set the current transformation value.
     */
    void
    transformation(enum transformation_type_t, const Transformation&) const;

    /*!
     * Set the current transformation value; if v is
     * not valid, sets the transformation to the identity
     */
    void
    transformation(enum transformation_type_t,
                   RenderValue<Transformation> v) const;

    /*!
     * *Set* the translation of the transformation
     */
    void
    transformation_translate(enum transformation_type_t, float x, float y) const;

    /*!
     * *Set* the translation of the transformation
     */
    void
    transformation_translate(enum transformation_type_t tp, vec2 v)
    {
      transformation_translate(tp, v.x(), v.y());
    }

    /*!
     * *Set* the translation of the transformation
     */
    void
    transformation_matrix(enum transformation_type_t, const float2x2 &v) const;

    /*!
     * Returns the current transformation realized
     * in an astral::RenderValue<astral::Transformation>.
     * Value is heavily cached.
     */
    RenderValue<Transformation>
    transformation_value(enum transformation_type_t) const;

    /*!
     * Concat the current transformation value.
     */
    void
    concat(enum transformation_type_t, const Transformation&) const;

    /*!
     * Concat the current transformation value.
     */
    void
    concat(enum transformation_type_t, const float2x2&) const;

    /*!
     * Translate the current transformation.
     */
    void
    translate(enum transformation_type_t, float x, float y) const;

    /*!
     * Translate the current transformation.
     */
    void
    translate(enum transformation_type_t tp, vec2 v) const
    {
      translate(tp, v.x(), v.y());
    }

    /*!
     * Scale the current transformation.
     */
    void
    scale(enum transformation_type_t, float sx, float sy) const;

    /*!
     * Scale the current transformation.
     */
    void
    scale(enum transformation_type_t tp, vec2 s) const
    {
      scale(tp, s.x(), s.y());
    }

    /*!
     * Scale the current transformation.
     */
    void
    scale(enum transformation_type_t tp, float s) const
    {
      scale(tp, s, s);
    }

    /*!
     * Rotate the current transformation value.
     */
    void
    rotate(enum transformation_type_t tp, float radians) const;

    /*!
     * Pushes the transformation-stack
     */
    void
    save_transformation(enum transformation_type_t) const;

    /*!
     * Returns the current save transformation stack size
     */
    unsigned int
    save_transformation_count(enum transformation_type_t) const;

    /*!
     * Restores the transformation stack to what it was
     * at the last call to save_transformation()
     */
    void
    restore_transformation(enum transformation_type_t) const;

    /*!
     * Restores the transformation stack to a specified value of
     * the stack size
     */
    void
    restore_transformation(enum transformation_type_t, unsigned int cnt) const;

    /*!
     * Set the active astral::StrokeShader. Subsequence paths added will use
     * the passed shader.
     */
    void
    set_shader(const MaskStrokeShader &shader) const;

    /*!
     * Set a clip-mask to be active for subsequence draws added.
     */
    void
    set_item_clip(const ItemMask &clip,
                  enum mask_item_shader_clip_mode_t clip_mode = mask_item_shader_clip_cutoff) const;

    /*!
     * Set the active StrokeShader::ItemDataPackerBase to use; the object
     * must stay alive and remain immutable until set_item_packer() is called
     * again. Passing a nullptr value indicates that data packing will be done
     * with a StrokeShader::ItemDataPacker.
     */
    void
    set_item_packer(const StrokeShader::ItemDataPackerBase *packer) const;

    /*!
     * Set the active stroking parameters and time of animation to use.
     * \param stroke_params stroking parameter for subsequently added paths and contours
     * \param t interpolate for subsequently added animated paths and contours
     */
    void
    set_stroke_params(const StrokeParameters &stroke_params, float t) const;

    /*!
     * Add a path to the mask. The transformations of the astral::CombinedPath
     * will be combined with the current \ref logical_transformation_path value.
     */
    void
    add_path(const CombinedPath &path) const;

    /*!
     * Add a path to the mask.
     * \param path path to stroke
     */
    void
    add_path(const Path &path) const;

    /*!
     * Add an animated path to the mask.
     * \param path path to stroke
     */
    void
    add_path(const AnimatedPath &path) const;

    /*!
     * Add a contour to the mask.
     * \param contour contour to stroke
     */
    void
    add_contour(const Contour &contour) const;

    /*!
     * Add a contour to the mask.
     * \param contour contour to stroke
     */
    void
    add_contour(const AnimatedContour &contour) const;

    /*!
     * Indicates that no additional paths are to be added
     */
    void
    finish(void) const;

    /*!
     * May only be called after finish() is called. Returns
     * the astral::MaskDetails describing the mask generated
     * from the strokes of the added paths.
     */
    const MaskDetails&
    mask_details(enum mask_type_t) const;

    /*!
     * May only be called after finish() is called. Returns
     * the astral::RenderClipElement of the mask used for
     * clipping.
     */
    const reference_counted_ptr<const RenderClipElement>&
    clip_element(enum mask_type_t) const;

    /*!
     * Given a \ref MaskDetails for stroking, change its parameters
     * to change the mask mode to a specified mode. It is an error
     * to pass a \ref MaskDetails that was not returned by
     * RenderEncoderStrokeMask::mask_details().
     */
    static
    void
    change_mask_mode(MaskDetails *mask, enum mask_type_t mode)
    {
      MaskUsage::change_mask_mode(mask, mode);
    }

    /*!
     * The astral::Image held in the MaskDetails::m_mask returned
     * by \ref mask_details() holds both a distance field and
     * coverage value. This function returns on what channel
     * each of these are held.
     */
    static
    enum mask_channel_t
    mask_channel(enum mask_type_t v)
    {
      return MaskUsage::mask_channel(v);
    }

  private:
    friend class Renderer;
    friend class RenderEncoderBase;

    class Backing;

    RenderEncoderStrokeMask(Backing *p):
      m_builder(p)
    {}

    Backing&
    builder(void) const
    {
      ASTRALassert(valid());
      return *m_builder;
    }

    Backing *m_builder;
  };

///@cond
  template<enum colorspace_t colorspace>
  RenderEncoderSurface
  Renderer::
  encoder_surface(RenderTarget &rt, FixedPointColor<colorspace> clear_color)
  {
    return encoder_surface(rt, colorspace, clear_color.m_value);
  }

  inline
  RenderEncoderSurface
  Renderer::
  begin(RenderTarget &rt, enum colorspace_t colorspace, u8vec4 clear_color)
  {
    begin(colorspace);
    return encoder_surface(rt, colorspace, clear_color);
  }

  template<enum colorspace_t colorspace>
  RenderEncoderSurface
  Renderer::
  begin(RenderTarget &rt, FixedPointColor<colorspace> clear_color)
  {
    return begin(rt, colorspace, clear_color.m_value);
  }

  inline
  RenderEncoderBase::
  RenderEncoderBase(RenderEncoderSurface S):
    m_virtual_buffer(S.m_virtual_buffer)
  {}

  inline
  RenderEncoderBase::
  RenderEncoderBase(RenderEncoderImage S):
    m_virtual_buffer(S.m_virtual_buffer)
  {}

  inline
  RenderEncoderBase::Proxy
  RenderEncoderBase::
  proxy_relative(const RelativeBoundingBox &bb, unsigned int pixel_slack) const
  {
    return proxy_relative(bb, RenderScaleFactor(), pixel_slack);
  }

  inline
  RenderEncoderMask
  RenderEncoderBase::
  encoder_mask_relative(const RelativeBoundingBox &bb,
                        unsigned int pixel_slack) const
  {
    return encoder_mask_relative(bb, RenderScaleFactor(), pixel_slack);
  }

  inline
  RenderEncoderImage
  RenderEncoderBase::
  encoder_image(Proxy proxy) const
  {
    return encoder_image(proxy, colorspace());
  }

  inline
  RenderEncoderImage
  RenderEncoderBase::
  encoder_image_relative(const RelativeBoundingBox &bb,
                         enum colorspace_t colorspace,
                         unsigned int pixel_slack) const
  {
    return encoder_image_relative(bb, RenderScaleFactor(), colorspace, pixel_slack);
  }

  inline
  RenderEncoderImage
  RenderEncoderBase::
  encoder_image_relative(const RelativeBoundingBox &bb,
                         unsigned int pixel_slack) const
  {
    return encoder_image_relative(bb, RenderScaleFactor(), colorspace(), pixel_slack);
  }

  inline
  RenderEncoderImage
  RenderEncoderBase::
  encoder_image_relative(const RelativeBoundingBox &bb,
                         RenderScaleFactor scale_rendering,
                         unsigned int pixel_slack) const
  {
    return encoder_image_relative(bb, scale_rendering, colorspace(), pixel_slack);
  }

  inline
  RenderEncoderImage
  RenderEncoderBase::
  encoder_image(ivec2 size) const
  {
    return encoder_image(size, colorspace());
  }

  inline
  RenderEncoderLayer
  RenderEncoderBase::
  begin_layer(const BoundingBox<float> &bb,
              RenderScaleFactor scale_rendering, const vec4 &color,
              enum blend_mode_t blend_mode,
              enum filter_t filter_mode,
              const ItemMask &clip) const
  {
    return begin_layer(bb, scale_rendering, colorspace(), color,
                       blend_mode, filter_mode, clip);
  }

  inline
  RenderEncoderLayer
  RenderEncoderBase::
  begin_layer(const BoundingBox<float> &bb, enum colorspace_t colorspace,
              const vec4 &color, enum blend_mode_t blend_mode,
              enum filter_t filter_mode,
              const ItemMask &clip) const
  {
    return begin_layer(bb, RenderScaleFactor(), colorspace, color, blend_mode, filter_mode, clip);
  }

  inline
  RenderEncoderLayer
  RenderEncoderBase::
  begin_layer(const BoundingBox<float> &bb, const vec4 &color,
              enum blend_mode_t blend_mode, enum filter_t filter_mode,
              const ItemMask &clip) const
  {
    return begin_layer(bb, RenderScaleFactor(), colorspace(), color, blend_mode, filter_mode, clip);
  }

  inline
  RenderEncoderLayer
  RenderEncoderBase::
  begin_layer(const BoundingBox<float> &bb, enum colorspace_t colorspace,
              float alpha, enum blend_mode_t blend_mode,
              const ItemMask &clip) const
  {
    return begin_layer(bb, colorspace, vec4(1.0f, 1.0f, 1.0f, alpha), blend_mode, filter_linear, clip);
  }

  inline
  RenderEncoderLayer
  RenderEncoderBase::
  begin_layer(const BoundingBox<float> &bb, float alpha,
                enum blend_mode_t blend_mode,
                const ItemMask &clip) const
  {
    return begin_layer(bb, colorspace(), vec4(1.0f, 1.0f, 1.0f, alpha), blend_mode, filter_linear, clip);
  }

  inline
  RenderEncoderLayer
  RenderEncoderBase::
  begin_layer(const Effect &effect,
              const EffectParameters &effect_parameters,
              const BoundingBox<float> &bb,
              enum blend_mode_t blend_mode,
              const ItemMask &clip) const
  {
    return begin_layer(effect, effect_parameters, bb, colorspace(), blend_mode, clip);
  }

  inline
  RenderEncoderLayer
  RenderEncoderBase::
  begin_layer(const EffectCollectionBase &effects,
              const BoundingBox<float> &bb,
              const ItemMask &clip) const
  {
    return begin_layer(effects, bb, colorspace(), clip);
  }

  inline
  RenderClipNode
  RenderEncoderBase::
  begin_clip_node_pixel(enum clip_node_flags_t flags,
                        const MaskDetails &mask,
                        const BoundingBox<float> &clip_in_bbox,
                        const BoundingBox<float> &clip_out_bbox,
                        enum filter_t mask_filter,
                        const ItemMask &clip) const
  {
    return begin_clip_node_pixel(number_blend_modes,
                                 flags, mask,
                                 clip_in_bbox, clip_out_bbox,
                                 mask_filter,
                                 clip);
  }

  inline
  RenderClipNode
  RenderEncoderBase::
  begin_clip_node_pixel(enum clip_node_flags_t flags,
                        const RenderClipCombineResult &mask_buffer,
                        const BoundingBox<float> &clip_in_bbox,
                        const BoundingBox<float> &clip_out_bbox,
                        enum filter_t mask_filter,
                        const ItemMask &clip)
  {
    return begin_clip_node_pixel(number_blend_modes,
                                 flags, mask_buffer,
                                 clip_in_bbox, clip_out_bbox,
                                 mask_filter, clip);
  }

  inline
  RenderClipNode
  RenderEncoderBase::
  begin_clip_node_logical(enum clip_node_flags_t flags,
                          const CombinedPath &paths,
                          const FillParameters &params,
                          const FillMaskProperties &mask_properties,
                          MaskUsage mask_usage,
                          MaskDetails *out_data,
                          const ItemMask &clip) const
  {
    return begin_clip_node_logical(number_blend_modes, flags,
                                   paths, params, mask_properties,
                                   mask_usage, out_data, clip);
  }

  template<typename T>
  inline
  bool
  RenderEncoderBase::
  valid(const RenderValue<T> &v) const
  {
    return valid()
      && v.valid()
      && v.m_backend == &renderer().backend();
  }

  template<typename T>
  inline
  bool
  RenderValue<T>::
  valid_for(const RenderEncoderBase &p) const
  {
    return p.valid(*this);
  }

  template<typename T>
  inline
  bool
  RenderValue<T>::
  valid_for(const RenderEncoderMask &p) const
  {
    return p.valid(*this);
  }

  inline
  bool
  RenderEncoderBase::
  valid(const ItemData &v) const
  {
    return valid()
      && v.valid()
      && v.m_backend == &renderer().backend();
  }

  inline
  bool
  ItemData::
  valid_for(const RenderEncoderBase &p) const
  {
    return p.valid(*this);
  }

  inline
  bool
  ItemData::
  valid_for(const RenderEncoderMask &p) const
  {
    return p.valid(*this);
  }

  inline
  RenderEncoderBase::AutoRestore::
  AutoRestore(Renderer::VirtualBuffer *p):
    m_buffer(p)
  {
    RenderEncoderBase encoder(m_buffer);

    ASTRALassert(encoder.valid());
    m_transformation_stack = encoder.save_transformation_count();
    m_pause_snapshot = encoder.pause_snapshot_depth();
    encoder.save_transformation();
  }

  inline
  RenderEncoderBase::AutoRestore::
  AutoRestore(const RenderEncoderBase &b):
    AutoRestore(b.m_virtual_buffer)
  {
  }

  inline
  RenderEncoderBase::AutoRestore::
  AutoRestore(const RenderEncoderMask &b):
    AutoRestore(b.m_virtual_buffer)
  {}

  inline
  RenderEncoderBase::AutoRestore::
  AutoRestore(const RenderEncoderShadowMap &b):
    AutoRestore(b.m_virtual_buffer)
  {}

  inline
  RenderEncoderBase::AutoRestore::
  ~AutoRestore()
  {
    RenderEncoderBase encoder(m_buffer);
    encoder.restore_transformation(m_transformation_stack);
    encoder.pause_snapshot_depth(m_pause_snapshot);
  }

///@endcond
/*! @} */

}

/*!\addtogroup Renderer
 * @{
 */

/*!
 * Overload for | operator for astral::RenderEncoderBase::clip_node_flags_t
 */
inline
enum astral::RenderEncoderBase::clip_node_flags_t
operator|(enum astral::RenderEncoderBase::clip_node_flags_t a,
          enum astral::RenderEncoderBase::clip_node_flags_t b)
{
  uint32_t ua(a), ub(b);
  return static_cast<enum astral::RenderEncoderBase::clip_node_flags_t>(ua | ub);
}


/*!
 * Overload for & operator for astral::RenderEncoderBase::clip_node_flags_t
 */
inline
enum astral::RenderEncoderBase::clip_node_flags_t
operator&(enum astral::RenderEncoderBase::clip_node_flags_t a,
          enum astral::RenderEncoderBase::clip_node_flags_t b)
{
  uint32_t ua(a), ub(b);
  return static_cast<enum astral::RenderEncoderBase::clip_node_flags_t>(ua & ub);
}
/*! @} */

#endif

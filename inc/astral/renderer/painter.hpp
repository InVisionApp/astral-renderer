/*!
 * \file painter.hpp
 * \brief file painter.hpp
 *
 * Copyright 2022 by InvisionApp.
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

#ifndef ASTRAL_PAINTER_HPP
#define ASTRAL_PAINTER_HPP

#include <astral/renderer/renderer.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * An astral::Painter is a wrapper over astral::RenderEncoderBase
   * that gives a more traditional interface for 2D rendering. The
   * main advantage of astral::Painter over astral::RenderEncoderBase
   * is that Painter::begin_layer() / Painter::end_layer() take care
   * of calling RenderEncoderBase::end_layer() for the user.
   */
  class Painter:
    public RenderSupportTypes,
    private noncopyable
  {
  public:
    /*!
     * Value to indicate which layer of the layer stack.
     */
    class LayerIndex
    {
    public:
      /*!
       * Default ctor is so that refers ALWAYS to the current
       * layer of ANY Painter.
       */
      LayerIndex(void):
        m_value(~0u)
      {}

      /*!
       * Returns a LayerIndex value that is ALWAYS the bottom layer
       */
      static
      LayerIndex
      bottom_layer(void)
      {
        return LayerIndex(0);
      }

      /*!
       * Returns a LayerIndex value that is ALWAYS the top layer
       */
      static
      LayerIndex
      top_layer(void)
      {
        return LayerIndex(~0u);
      }

    private:
      friend class Painter;

      LayerIndex(unsigned int v):
        m_value(v)
      {}

      unsigned int m_value;
    };

    /*!
     * At ctor, saves the current transformation state, pausing snapshot
     * state and current layer of an astral::Painter and at dtor restores
     * these states.
     */
    class AutoRestore:noncopyable
    {
    public:
      /*!
       * Ctor, takes snaspshot state of the passed astral::Painter.
       * On dtor, the astral::Painter state is restored
       * \param p astral::Painter to restore state on dtor of created
       *          AutoRestore object
       */
      AutoRestore(Painter &p):
        m_p(p),
        m_layer(p.current_layer()),
        m_sub_auto_restore(p.m_encoder)
      {}

      ~AutoRestore()
      {
        m_p.end_layer(m_layer);
      }

    private:
      Painter &m_p;
      LayerIndex m_layer;
      RenderEncoderBase::AutoRestore m_sub_auto_restore;
    };

    /*!
     * Ctor that initializes the Painter with active()
     * as false, i.e. no encoder to which to add commands.
     */
    Painter(void);

    /*!
     * Ctor that initializes the Painter with active()
     * as true, sending commands to the passed encoder.
     *
     * NOTE: it is NOT necessary to called end() before
     *       dtor; the dtor will call end() if the Painter
     *       is active.
     */
    Painter(RenderEncoderBase encoder);

    ~Painter();

    /*!
     * Start a painter that renders to content via an encoder
     */
    void
    begin(RenderEncoderBase encoder);

    /*!
     * Finish rendering with the Painter, pops all layers and
     * Painter will not accept any additional rendering
     * commands until begin() is called again. Returns the
     * astral::RenderEncoderBase passed to begin().
     *
     * NOTE: it is NOT necessary to called end() before
     *       dtor; the dtor will call end() if the Painter
     *       is active.
     */
    RenderEncoderBase
    end(void);

    /*!
     * Returns true if this astral::Painter is within a
     * begin()/end() pair.
     */
    bool
    active(void)
    {
      return m_start_encoder.valid();
    }

    /*!
     * The colorspace at which the contents of the layer or surface
     * of this encoder are rendered
     */
    enum colorspace_t
    colorspace(void)
    {
      return m_encoder.colorspace();
    }

    /*!
     * Creates an astral::RenderValue from a value;
     * only guaranteed to be valid within the current
     * begin()/end() pair.
     */
    template<typename T>
    RenderValue<T>
    create_value(const T &v)
    {
      return m_encoder.create_value(v);
    }

    /*!
     * Creates an astral::RenderValue from a value;
     * only guaranteed to be valid within the current
     * begin()/end() pair.
     */
    RenderValue<const ShadowMap&>
    create_value(const ShadowMap &v)
    {
      return m_encoder.create_value(v);
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
    render_value_from_cookie(uint32_t cookie) const
    {
      return m_encoder.render_value_from_cookie<T>(cookie);
    }

    /*!
     * Creates an astral:ItemData; only guaranteed to be valid
     * within the current begin()/end() pair.
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::create_item_data()
     */
    template<typename ...Args>
    ItemData
    create_item_data(Args&&... args)
    {
      return m_encoder.create_item_data(std::forward<Args>(args)...);
    }

    /*!
     * Compute the tolerance value to use for path approximations.
     */
    float
    compute_tolerance(void)
    {
      return m_encoder.compute_tolerance();
    }

    /*!
     * Compute the tolerance value to use for path approximations
     * based off of the current value for render_accuracy(void),
     * the current transformation(void) and an optional float2x2
     * matrix.
     * \param matrix if non-null, compute the tolerance needed
     *               as if the current concact() was called passing
     *               *matrix.
     */
    float
    compute_tolerance(const float2x2 *matrix)
    {
      return m_encoder.compute_tolerance(matrix);
    }

    /*!
     * Returns the current transformation from logical to pixel coordinates.
     */
    const Transformation&
    transformation(void)
    {
      return m_encoder.transformation();
    }

    /*!
     * Returns the current transformation realized
     * in an astral::RenderValue<astral::Transformation>.
     * Value is heavily cached.
     */
    RenderValue<Transformation>
    transformation_value(void)
    {
      return m_encoder.transformation_value();
    }

    /*!
     * Returns the singular values of transformation().
     */
    vec2
    singular_values(void)
    {
      return m_encoder.singular_values();
    }

    /*!
     * Returns an upper bound for the size of one pixel in
     * the current surface in logical coordinates.
     */
    float
    surface_pixel_size_in_logical_coordinates(void) const
    {
      return m_encoder.surface_pixel_size_in_logical_coordinates();
    }

    /*!
     * Returns the inverse of the current transformation value.
     */
    const Transformation&
    inverse_transformation(void)
    {
      return m_encoder.inverse_transformation();
    }

    /*!
     * Set the current transformation value.
     */
    void
    transformation(const Transformation &v)
    {
      return m_encoder.transformation(v);
    }

    /*!
     * Set the current transformation value; if v is
     * not valid, sets the transformation to the identity
     */
    void
    transformation(RenderValue<Transformation> v)
    {
      return m_encoder.transformation(v);
    }

    /*!
     * *Set* the translation of the transformation
     */
    void
    transformation_translate(float x, float y)
    {
      return m_encoder.transformation_translate(x, y);
    }

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
    transformation_matrix(const float2x2 &v)
    {
      return m_encoder.transformation_matrix(v);
    }

    /*!
     * Concat the current transformation value.
     */
    void
    concat(const Transformation &v)
    {
      return m_encoder.concat(v);
    }

    /*!
     * Concat the current transformation value.
     */
    void
    concat(const float2x2 &v)
    {
      return m_encoder.concat(v);
    }

    /*!
     * Translate the current transformation.
     */
    void
    translate(float x, float y)
    {
      return m_encoder.translate(x, y);
    }

    /*!
     * Translate the current transformation.
     */
    void
    translate(vec2 v)
    {
      translate(v.x(), v.y());
    }

    /*!
     * Scale the current transformation.
     */
    void
    scale(float sx, float sy)
    {
      return m_encoder.scale(sx, sy);
    }

    /*!
     * Scale the current transformation.
     */
    void
    scale(vec2 s)
    {
      scale(s.x(), s.y());
    }

    /*!
     * Scale the current transformation.
     */
    void
    scale(float s)
    {
      scale(s, s);
    }

    /*!
     * Rotate the current transformation value.
     */
    void
    rotate(float radians)
    {
      return m_encoder.rotate(radians);
    }

    /*!
     * Pushes the transformation-stack
     */
    void
    save_transformation(void)
    {
      m_encoder.save_transformation();
    }

    /*!
     * Restores the transformation stack to what it was
     * at the last call to save_transformation(); only
     * affects tranformation stack
     */
    void
    restore_transformation(void)
    {
      m_encoder.restore_transformation();
    }

    /*!
     * returns the default shaders used
     */
    const ShaderSet&
    default_shaders(void)
    {
      return m_encoder.default_shaders();
    }

    /*!
     * returns the default effects used
     */
    const EffectSet&
    default_effects(void)
    {
      return m_encoder.default_effects();
    }

    /*!
     * Draw a rect using an overload of RenderEncoderBase::draw_rect().
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::draw_rect()
     */
    template<typename ...Args>
    void
    draw_rect(Args&&... args)
    {
      m_encoder.draw_rect(std::forward<Args>(args)...);
    }

    /*!
     * Draw a rect using an overload of RenderEncoderBase::draw_image().
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::draw_image()
     */
    template<typename ...Args>
    void
    draw_image(Args&&... args)
    {
      m_encoder.draw_image(std::forward<Args>(args)...);
    }

    /*!
     * Draw a rect using an overload of RenderEncoderBase::draw_custom_rect().
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::draw_custom_rect()
     */
    template<typename ...Args>
    void
    draw_custom_rect(Args&&... args)
    {
      m_encoder.draw_custom_rect(std::forward<Args>(args)...);
    }

    /*!
     * Draw a using an overload of RenderEncoderBase::draw_custom().
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::draw_custom()
     */
    template<typename ...Args>
    void
    draw_custom(Args&&... args)
    {
      m_encoder.draw_custom(std::forward<Args>(args)...);
    }

    /*!
     * Draw a mask using an overload of RenderEncoderBase::draw_mask().
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::draw_mask()
     */
    template<typename ...Args>
    void
    draw_mask(Args&&... args)
    {
      m_encoder.draw_mask(std::forward<Args>(args)...);
    }

    /*!
     * Draw layers of an astral::ItemPath using an overload of RenderEncoderBase::draw_item_path().
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::draw_item_path()
     */
    template<typename ...Args>
    void
    draw_item_path(Args&&... args)
    {
      m_encoder.draw_item_path(std::forward<Args>(args)...);
    }

    /*!
     * Stroke using an overload of RenderEncoderBase::stroke_paths().
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::stroke_paths()
     */
    template<typename ...Args>
    void
    stroke_paths(Args&&... args)
    {
      m_encoder.stroke_paths(std::forward<Args>(args)...);
    }

    /*!
     * Directly stroke using an overload of RenderEncoderBase::direct_stroke_paths().
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::direct_stroke_paths()
     */
    template<typename ...Args>
    void
    direct_stroke_paths(Args&&... args)
    {
      m_encoder.direct_stroke_paths(std::forward<Args>(args)...);
    }

    /*!
     * Fill using an overload of RenderEncoderBase::fill_paths().
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::fill_paths()
     */
    template<typename ...Args>
    void
    fill_paths(Args&&... args)
    {
      m_encoder.fill_paths(std::forward<Args>(args)...);
    }

    /*!
     * Draw text using an overload of RenderEncoderBase::draw_text().
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::draw_text()
     */
    template<typename ...Args>
    void
    draw_text(Args&&... args)
    {
      m_encoder.draw_text(std::forward<Args>(args)...);
    }

    /*!
     * Draw text using an overload of RenderEncoderBase::draw_text_as_path().
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::draw_text_as_path()
     */
    template<typename ...Args>
    void
    draw_text_as_path(Args&&... args)
    {
      m_encoder.draw_text_as_path(std::forward<Args>(args)...);
    }

    /*!
     * Returns true if a path is clipped by the name layer's viewport
     * or the optional astral::RenderClipElement
     */
    template<typename ...Args>
    bool
    clips_box(LayerIndex layer, Args&&... args)
    {
      return encoder(layer).clips_box(std::forward<Args>(args)...);
    }

    /*!
     * Generate a mask using an overload of RenderEncoderBase::generate_mask().
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::generate_mask()
     */
    template<typename ...Args>
    void
    generate_mask(Args&&... args)
    {
      m_encoder.generate_mask(std::forward<Args>(args)...);
    }

    /*!
     * Create an astral::RenderClipCombineResult using an overload of RenderEncoderBase::combine_clipping().
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::combine_clipping()
     */
    template<typename ...Args>
    reference_counted_ptr<const RenderClipCombineResult>
    combine_clipping(Args&&... args)
    {
      return m_encoder.combine_clipping(std::forward<Args>(args)...);
    }

    /*!
     * Call an overload of RenderEncoderBase::intersect_clipping().
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::intersect_clipping()
     */
    template<typename ...Args>
    reference_counted_ptr<const RenderClipElement>
    intersect_clipping(Args&&... args)
    {
      return m_encoder.intersect_clipping(std::forward<Args>(args)...);
    }

    /*!
     * Draw clipped-in and clipped-out content using an overload of
     * RenderEncoderBase::begin_clip_node_pixel()
     * \tparam F functor that provides operator(Painter*, Painter*) to draw
     *           where first argument if non-null is Painter to draw clipped-in
     *           content and second argument if non-null is Painter to draw
     *           clipped-out content
     * \param clip_drawer functor to draw clipped-in and clipped-out content
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::begin_clip_node_pixel()
     */
    template<typename F, typename ...Args>
    void
    clip_node_pixel(const F &clip_drawer, Args&&... args)
    {
      RenderClipNode C;
      Painter Pin, Pout;
      Painter *pin(nullptr), *pout(nullptr);

      C = m_encoder.begin_clip_node_pixel(std::forward<Args>(args)...);
      if (C.clip_in().valid())
        {
          pin = &Pin;
          pin->begin(C.clip_in());
        }

      if (C.clip_out().valid())
        {
          pout = &Pout;
          pout->begin(C.clip_out());
        }

      clip_drawer(pin, pout);

      if (pin)
        {
          pin->end();
        }
      if (pout)
        {
          pout->end();
        }

      m_encoder.end_clip_node(C);
    }

    /*!
     * Draw clipped-in and clipped-out content using an overload of
     * RenderEncoderBase::begin_clip_node_logical()
     * \tparam F functor that provides operator(Painter*, Painter*) to draw
     *           where first argument if non-null is Painter to draw clipped-in
     *           content and second argument if non-null is Painter to draw
     *           clipped-out content
     * \param clip_drawer functor to draw clipped-in and clipped-out content
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::begin_clip_node_logical()
     */
    template<typename F, typename ...Args>
    void
    clip_node_logical(const F &clip_drawer, Args&&... args)
    {
      RenderClipNode C;
      Painter Pin, Pout;
      Painter *pin(nullptr), *pout(nullptr);

      C = m_encoder.begin_clip_node_logical(std::forward<Args>(args)...);
      if (C.clip_in().valid())
        {
          pin = &Pin;
          pin->begin(C.clip_in());
        }

      if (C.clip_out().valid())
        {
          pout = &Pout;
          pout->begin(C.clip_out());
        }

      clip_drawer(pin, pout);

      if (pin)
        {
          pin->end();
        }
      if (pout)
        {
          pout->end();
        }

      m_encoder.end_clip_node(C);
    }

    /*!
     * Begin a new layer, actual layer contents are blitted
     * on end_layer(). In addition, on end_layer(),
     * the transformation value is also restored.
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::begin_layer()
     */
    template<typename ...Args>
    void
    begin_layer(Args&&... args)
    {
      RenderEncoderLayer L;

      L = m_encoder.begin_layer(std::forward<Args>(args)...);
      begin_layer_implement(L);
    }

    /*!
     * Matching call to begin_layer() to blit the contents
     * rendered. Also restores the transformation value
     * to what it was when the matching begin_layer() was
     * called.
     * \param out_image if non-null, location to which to write
     *                  a reference of the astral::Image representing
     *                  the pixels rendered for the layer
     * \param out_image_transformation_pixel if non-null, location
     *                                       to which to write the
     *                                       the transformation from
     *                                       pixel coordiantes to
     *                                       image coordiantes.
     */
    void
    end_layer(reference_counted_ptr<Image> *out_image = nullptr,
                  ScaleTranslate *out_image_transformation_pixel = nullptr);

    /*!
     * Returns the \ref LayerIndex value for the current layer.
     */
    LayerIndex
    current_layer(void)
    {
      return LayerIndex(m_layers.size());
    }

    /*!
     * Pop layers from the save layer stack until the
     * value of current_layer() is the same as the
     * passed argument.
     * \param layer LayerIndex to what layer to restore
     */
    void
    end_layer(LayerIndex layer);

    /*!
     * Call an overload of RenderEncoderBase::proxy_relative()
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::proxy_relative()
     */
    template<typename ...Args>
    RenderEncoderBase::Proxy
    proxy_relative(Args&&... args)
    {
      return m_encoder.proxy_relative(std::forward<Args>(args)...);
    }

    /*!
     * Call an overload of RenderEncoderBase::encoder_mask()
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::encoder_mask()
     */
    template<typename ...Args>
    RenderEncoderMask
    encoder_mask(Args&&... args)
    {
      return m_encoder.encoder_mask(std::forward<Args>(args)...);
    }

    /*!
     * Call an overload of RenderEncoderBase::encoder_mask_relative()
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::encoder_mask_relative()
     */
    template<typename ...Args>
    RenderEncoderMask
    encoder_mask_relative(Args&&... args)
    {
      return m_encoder.encoder_mask_relative(std::forward<Args>(args)...);
    }

    /*!
     * Call an overload of RenderEncoderBase::encoder_image()
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::encoder_image()
     */
    template<typename ...Args>
    RenderEncoderImage
    encoder_image(Args&&... args)
    {
      return m_encoder.encoder_image(std::forward<Args>(args)...);
    }

    /*!
     * Call an overload of RenderEncoderBase::encoder_image_relative()
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::encoder_image_relative()
     */
    template<typename ...Args>
    RenderEncoderImage
    encoder_image_relative(Args&&... args)
    {
      return m_encoder.encoder_image_relative(std::forward<Args>(args)...);
    }

    /*!
     * Call an overload of RenderEncoderBase::encoder_shadow_map()
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::encoder_shadow_map()
     */
    template<typename ...Args>
    RenderEncoderShadowMap
    encoder_shadow_map(Args&&... args)
    {
      return m_encoder.encoder_shadow_map(std::forward<Args>(args)...);
    }

    /*!
     * Call an overload of RenderEncoderBase::encoder_shadow_map_relative()
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::encoder_shadow_map_relative()
     */
    template<typename ...Args>
    RenderEncoderShadowMap
    encoder_shadow_map_relative(Args&&... args)
    {
      return m_encoder.encoder_shadow_map_relative(std::forward<Args>(args)...);
    }

    /*!
     * Call an overload of RenderEncoderBase::encoder_stroke()
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::encoder_stroke()
     */
    template<typename ...Args>
    RenderEncoderStrokeMask
    encoder_stroke(Args&&... args)
    {
      return m_encoder.encoder_stroke(std::forward<Args>(args)...);
    }

    /*!
     * Indicates to the renderer that the caller is entering a section
     * of drawing where the draws are guaranteed to have no overlap.
     */
    void
    begin_pause_snapshot(void)
    {
      m_encoder.begin_pause_snapshot();
    }

    /*!
     * Indicates to the renderer that the caller is leaving a section
     * of drawing where the draws are guaranteed to have no overlap.
     */
    void
    end_pause_snapshot(void)
    {
      m_encoder.end_pause_snapshot();
    }

    /*!
     * Returns true exactly when inside a pause snapshot sessions.
     */
    bool
    snapshot_paused(void)
    {
      return m_encoder.snapshot_paused();
    }

    /*!
     * Take an image snapshot of the current layer, returning
     * an astral::Image of the pixels. see the overloads of
     * RenderEncoderBase::snapshot_logical()
     * \param layer which LayerIndex from which to take the snapsot
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::snapshot_logical()
     */
    template<typename ...Args>
    reference_counted_ptr<Image>
    snapshot_logical(LayerIndex layer, Args&&... args)
    {
      RenderEncoderBase src;

      layer.m_value = t_min(layer.m_value, current_layer().m_value);
      src = (layer.m_value == 0) ?
        m_start_encoder :
        m_layers[layer.m_value - 1].encoder();

      return m_encoder.snapshot_logical(src, std::forward<Args>(args)...);
    }

    /*!
     * Take an image snapshot of the current layer with an effect applied,
     * see the overloads of RenderEncoderBase::snapshot_effect().
     * \param layer which LayerIndex from which to take the snapsot
     * \param args arguments directly passed to the correct overload of
     *             RenderEncoderBase::snapshot_effect()
     */
    template<typename ...Args>
    void
    snapshot_effect(LayerIndex layer, Args&&... args)
    {
      RenderEncoderBase src;

      layer.m_value = t_min(layer.m_value, current_layer().m_value);
      src = (layer.m_value == 0) ?
        m_start_encoder :
        m_layers[layer.m_value - 1].encoder();

      return m_encoder.snapshot_effect(src, std::forward<Args>(args)...);
    }

    /*!
     * Returns the bounding box in pixel-coordinates
     * of the rendering region of the named layer.
     * \param layer which LayerIndex to query for the pixel counding box
     */
    BoundingBox<float>
    pixel_bounding_box(LayerIndex layer = LayerIndex())
    {
      return encoder(layer).pixel_bounding_box();
    }

    /*!
     * Returns the bounding box in pixel-coordinates
     * of the rendering region  of the named layer
     * intersected against a passed bounding box that
     * is in logical coordinates
     * \param logical_bb lbox in logical coordinate to query
     * \param layer which LayerIndex to query for the pixel counding box
     */
    BoundingBox<float>
    pixel_bounding_box(const BoundingBox<float> &logical_bb,
                       LayerIndex layer = LayerIndex())
    {
      return encoder(layer).pixel_bounding_box(logical_bb);
    }

    /*!
     * Returns the scaling factor from pixel coordinates to
     * the render destination of the named layer (be it an
     * astral::RenderTarget or an astral::Image).
     * \param layer which LayerIndex to query
     */
    vec2
    render_scale_factor(LayerIndex layer = LayerIndex())
    {
      return encoder(layer).render_scale_factor();
    }

    /*!
     * Returns true if the named layer is degenerate, i.e.
     * covers no pixels.
     * \param layer which LayerIndex to query
     */
    bool
    degenerate(LayerIndex layer = LayerIndex())
    {
      return encoder(layer).degenerate();
    }

    /* TODO: add a method to take an image snapshot that also includes
     *       all of the previous layers as well.
     */

  private:
    /*!
     * Escape hatch to access the encoder for the current layer.
     *
     * WARNING: when begin_layer() is called, the blit for the content
     *          renderer offscreen does not occur until end_layer()
     *          is called. In particular, adding drawing commands to
     *          anything except the encoder of the top layer will make
     *          those drawing commands appear before those blits.
     */
    RenderEncoderBase
    encoder(LayerIndex layer = LayerIndex())
    {
      layer.m_value = t_min(layer.m_value, current_layer().m_value);
      return (layer.m_value == 0) ?
        m_start_encoder :
        m_layers[layer.m_value - 1].encoder();
    }

    void
    begin_layer_implement(RenderEncoderLayer);

    RenderEncoderBase m_start_encoder, m_encoder;
    std::vector<RenderEncoderLayer> m_layers;
  };

/*! @} */
} //namespace astral

#endif

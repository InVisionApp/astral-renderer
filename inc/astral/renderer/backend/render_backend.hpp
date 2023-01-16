/*!
 * \file render_backend.hpp
 * \brief file render_backend.hpp
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

#ifndef ASTRAL_RENDER_BACKEND_HPP
#define ASTRAL_RENDER_BACKEND_HPP

#include <vector>
#include <astral/util/reference_counted.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/rect.hpp>
#include <astral/util/gpu_dirty_state.hpp>
#include <astral/util/stencil_state.hpp>
#include <astral/renderer/vertex_data.hpp>
#include <astral/renderer/backend/clip_window.hpp>
#include <astral/renderer/backend/render_values.hpp>
#include <astral/renderer/shader/item_data.hpp>
#include <astral/renderer/brush.hpp>
#include <astral/renderer/image_sampler.hpp>
#include <astral/renderer/shadow_map.hpp>
#include <astral/renderer/colorstop_sequence.hpp>
#include <astral/util/transformation.hpp>
#include <astral/renderer/render_target.hpp>
#include <astral/renderer/render_value.hpp>
#include <astral/renderer/render_engine.hpp>

namespace astral
{
  class RenderClipElemenet;

/*!\addtogroup RendererBackend
 * @{
 */

  /*!
   * \brief
   * An astral::RenderBackend provides the abstraction between
   * astral::Renderer and an underlying 3D API. Of critical
   * importance is that the underlying 3D API must have that
   * the provoking vertex for flat varyings is the LAST vertex
   * of each triangle.
   *
   * In addition it provides an interface to reuse values via the
   * astral::RenderValue.
   */
  class RenderBackend:public reference_counted<RenderBackend>::non_concurrent
  {
  public:
    /*!
     * Enumeration of render statistics of RenderBackend
     */
    enum render_backend_stats_t:uint32_t
      {
        /*!
         * The number of RenderBackend draw calls issued;
         * this is usually a much larger value than the
         * number of actual 3D API draw calls.
         */
        stats_number_draws,

        /*!
         * The number of vertices sent down the pipeline
         */
        stats_vertices,

        /*!
         * The total number of render targets
         */
        stats_render_targets,

        /*!
         * The size of the vertex backing store.
         */
        stats_vertex_backing_size,

        /*!
         * The actual number of vertices on the store
         */
        stats_vertics_on_store,

        /*!
         * The size of the static gvec4 data backing store.
         */
        stats_static_data32_backing_size,

        /*!
         * The actual number of gvec4 on the static data store
         */
        stats_static_data32_on_store,

        /*!
         * The size of the static fp16 data backing store.
         */
        stats_static_data16_backing_size,

        /*!
         * The actual number of u16vec4 on the fp16 static data store
         */
        stats_static_data16_on_store,

        number_render_stats,
      };

    /*!
     * Class to specify one wants a statistic coming from
     * a -derived- class of \ref RenderBackend, i.e.
     * essentially a wrapper over the index fed to
     * render_stats_label_derived().
     */
    class DerivedStat
    {
    public:
      /*!
       * Ctor
       * \param v value with which to initialize \ref m_value
       */
      explicit
      DerivedStat(unsigned int v):
        m_value(v)
      {}

      /*!
       * The Value that is wrapped, i.e. an index fed
       * to render_stats_label_derived()
       */
      unsigned int m_value;
    };

    /*!
     * Enumeration to contain the bit masks to specify what
     * buffers to clear.
     */
    enum clear_bits:uint32_t
      {
        /*!
         * Bitmask to indicate to clear the color buffer
         */
        ClearColorBuffer = 1,

        /*!
         * Bitmask to indicate to clear the depth buffer
         */
        ClearDepthBuffer = 2,

        /*!
         * Bitmask to indicate to clear the stencil buffer
         */
        ClearStencilBuffer = 4,

        /*!
         */
        ClearDepthStencilBuffer = ClearDepthBuffer | ClearStencilBuffer,

        /*!
         * bit mask to indicates to clear all buffers.
         */
        ClearAllBuffers = ~0u
      };

    /*!
     * Enumeration to describe if and how the depth
     * buffer is used and/or obeyed
     */
    enum depth_buffer_mode_t:uint32_t
      {
        /*!
         * Depth buffer is set to be used for occluding
         * when rendering to a color buffer. Depth buffer
         * is written to and tested against. The depth test
         * in conjuction with the shader is expected to be a
         * strict depth test that passes on monotonically
         * increasing values of the z argument to draw_render_data().
         */
        depth_buffer_occlude,

        /*!
         * Depth buffer is set to be used where the depth test
         * passes only if the depth value emitted is the same
         * value as that alrady present in the depth buffer.
         */
        depth_buffer_equal,

        /*!
         * Depth buffer is inactive, i.e. depth testing is
         * off and depth writes are also off.
         */
        depth_buffer_off,

        /*!
         * Depth buffer is set to be used for generating
         * a shadow map.
         */
        depth_buffer_shadow_map,

        /*!
         * Depth writes are on but depth test always passes.
         */
        depth_buffer_always,
      };

    /*!
     * Enumeration value specifying depth value to either
     * clear the buffer to allow drawing or to occlude all.
     */
    enum depth_buffer_value:uint32_t
      {
        /*!
         * Indicates to clear the depth buffer with a value
         * that does not occlude any fragments.
         */
        depth_buffer_value_clear = 0,

        /*!
         * Indicates to clear the depth buffer with a value
         * that does occludes all fragments.
         */
        depth_buffer_value_occlude = 0xFFFFFFFFu,
      };

    /*!
     * Class to specify what and how to clear the color, depth
     * and stencil buffers when starting a render target with
     * begin_render_target().
     */
    class ClearParams
    {
    public:
      /*!
       * Ctor
       */
      ClearParams(void):
        m_clear_mask(0u),
        m_clear_depth(depth_buffer_value_clear),
        m_clear_stencil(0),
        m_clear_color(0.0f, 0.0f, 0.0f, 0.0f)
      {}

      /*!
       * Set \ref m_clear_depth and sets to clear the depth buffer.
       */
      ClearParams&
      clear_depth(enum depth_buffer_value v)
      {
        m_clear_mask |= ClearDepthBuffer;
        m_clear_depth = v;
        return *this;
      }

      /*!
       * Set \ref m_clear_stencil and sets to clear the stencil buffer
       */
      ClearParams&
      clear_stencil(int v)
      {
        m_clear_mask |= ClearStencilBuffer;
        m_clear_stencil = v;
        return *this;
      }

      /*!
       * Set \ref m_clear_color and sets to clear the color buffer
       */
      ClearParams&
      clear_color(const vec4 &v)
      {
        m_clear_mask |= ClearColorBuffer;
        m_clear_color = v;
        return *this;
      }

      /*!
       * Sets to clear all buffers with:
       *  - clear color as (0, 0, 0, 0)
       *  - clear depth as RenderBackend::depth_buffer_clear
       *  - clear stencil as 0
       */
      ClearParams&
      clear_all(void)
      {
        m_clear_mask = ClearAllBuffers;
        m_clear_color = vec4(0.0f, 0.0f, 0.0f, 0.0f);
        m_clear_depth = depth_buffer_value_clear;
        m_clear_stencil = 0;
        return *this;
      }

      /*!
       * Bitmask of which buffers, if any, to clear.
       * The enumeration \ref clear_bits
       */
      uint32_t m_clear_mask;

      /*!
       * If the depth buffer is to be cleared, this
       * specifies the clear value for the depth buffer.
       */
      enum depth_buffer_value m_clear_depth;

      /*!
       * If the stencil buffer is to be cleared, this
       * specifies the clear value for the stencil buffer.
       */
      int m_clear_stencil;

      /*!
       * If the color buffer is to be cleared, this
       * specifies the clear value for the color buffer.
       */
      vec4 m_clear_color;
    };

    /*!
     * Embodies a key for an uber-shader; to be used to allow
     * for an implementation to avoid 3D API shader changes at
     * the expense of the overhead of an uber-shader.
     */
    class UberShadingKey:public reference_counted<UberShadingKey>::non_concurrent
    {
    public:
      /*!
       * Class to wrap the value returned by UberShadingKey::on_end_accumulate().
       */
      class Cookie
      {
      public:
        /*!
         * Ctor
         * \param v raw value from UberShadingKey::on_end_accumulate().
         */
        explicit
        Cookie(uint32_t v = InvalidRenderValue):
          m_value(v)
        {}

        /*!
         * Returns true exactly when the cookie value is valid
         * value from UberShadingKey::on_end_accumulate().
         */
        bool
        valid(void) const
        {
          return m_value != InvalidRenderValue;
        }

        /*!
         * Value returned by UberShadingKey::on_end_accumulate(), if this value
         * is \ref InvalidRenderValue, then indicates no uber-shading.
         */
        uint32_t m_value;
      };

      UberShadingKey(void):
        m_shader_clipping(clip_window_not_present),
        m_accumulating(false)
      {}

      virtual
      ~UberShadingKey()
      {}

      /*!
       * Set the uber-shading key to have that all color item
       * shaders, all material shaders and all blend modes
       * are added.
       */
      void
      uber_shader_of_all(enum clip_window_value_type_t shader_clipping)
      {
        ASTRALassert(!m_accumulating);
        m_shader_clipping = shader_clipping;
        m_cookie = Cookie(on_uber_shader_of_all(shader_clipping));
      }

      /*!
       * Begin accumulating the shaders that are part of this uber-shader.
       * Any shaders that were part of it are also removed.
       * \param shader_clipping specifies if a clip window is present and
       *                                  how to use it
       * \param uber_method uber shading method, must not be
       *                    uber_shader_none or uber_shader_all
       */
      void
      begin_accumulate(enum clip_window_value_type_t shader_clipping,
                       enum uber_shader_method_t uber_method)
      {
        ASTRALassert(!m_accumulating);
        ASTRALassert(uber_method != uber_shader_none);
        ASTRALassert(uber_method != uber_shader_all);

        m_cookie = Cookie();
        m_accumulating = true;
        m_shader_clipping = shader_clipping;
        on_begin_accumulate(m_shader_clipping, uber_method);
      }

      /*!
       * Adds to the uber-shader the code needed to execute a rendering
       * command as specified by an astral::RenderValues value. The shader
       * in the passed astral::RenderValues must be an astral::ColorItemShader,
       * i.e. uber shading is only for color rendering. This function may
       * only be called in a begin_accumulate() / end_accumulate() pair.
       * \param shader what shader to use
       * \param material_shader if non-null, what material shader to use.
       *                        If nullptr, then material shader is the
       *                        default brush shader
       * \param blend_mode what blend mode
       */
      void
      add_shader(const ItemShader &shader,
                 const MaterialShader *material_shader,
                 BackendBlendMode blend_mode)
      {
        ASTRALassert(m_accumulating);
        ASTRALassert(shader.type() == ItemShader::color_item_shader);
        ASTRALassert(blend_mode.item_shader_type() == ItemShader::color_item_shader);

        on_add_shader(shader, material_shader, blend_mode);
      }

      /*!
       * Provided as a conveniance, equivalent to
       * \code
       * add_shader(shader, cmd.m_material.material_shader(), cmd.m_blend_mode);
       * \endcode
       */
      void
      add_shader(const ItemShader &shader, const RenderValues &cmd)
      {
        add_shader(shader, cmd.m_material.material_shader(), cmd.m_blend_mode);
      }

      /*!
       * Mark sthe end of adding shaders to an uber-shader.
       */
      void
      end_accumulate(void)
      {
        ASTRALassert(m_accumulating);

        m_accumulating = false;
        m_cookie = Cookie(on_end_accumulate());
      }

      /*!
       * Returns if a clip window is present and how to use it;
       * this is specified by the call to begin_accumulate().
       */
      enum clip_window_value_type_t
      shader_clipping(void) const
      {
        return m_shader_clipping;
      }

      /*!
       * Returns true if inside of a begin_accumulate() / end_accumulate()
       * pair, i.e. it is legal to call add_shader().
       */
      bool
      accumulating(void) const
      {
        return m_accumulating;
      }

      /*!
       * Returns the cookie value for the uber-key, i.e. the return
       * value of on_end_accumulate().
       */
      Cookie
      cookie(void) const
      {
        ASTRALassert(!m_accumulating);
        return m_cookie;
      }

    protected:
      /*!
       * To be implemented by a derived class to begin accumulating the
       * shaders that are part of this uber-shader. Any shaders that were
       * part of it are also removed.
       * \param shader_clipping specifies if a clip window is present and
       *                        how to use it
       * \param uber_method uber shading method, must not be
       *                    uber_shader_none or
       *                    uber_shader_all
       */
      virtual
      void
      on_begin_accumulate(enum clip_window_value_type_t shader_clipping,
                          enum uber_shader_method_t uber_method) = 0;

      /*!
       * To be implemented by a derived class to add to the uber-shader
       * the shader needed to execute a rendering command as specified
       * by an astral::RenderValues value. The shader in the passed
       * astral::RenderValues must be an astral::ColorItemShader, i.e.
       * uber shading is only for color rendering. This function may
       * only be called in a begin_accumulate() / end_accumulate() pair.
       */
      virtual
      void
      on_add_shader(const ItemShader &shader,
                    const MaterialShader *material_shader,
                    BackendBlendMode blend_mode) = 0;

      /*!
       * To be implemented by a derived class to mark the end of adding
       * shaders to an uber-shader; returns a cookie to identify the
       * uber-shader. Returning the value astral::InvalidRenderValue
       * indicates the the backend dictates to not use uber-shading
       * for this key value.
       */
      virtual
      uint32_t
      on_end_accumulate(void) = 0;

      /*!
       * To be implemented by a derived class to mark that the
       * uber-shader key is to represent an uber-shader that has
       * all color-item shaders, all material shader and code
       * to handle all blend modes; functionally this is to be
       * equivalent to:
       * \code
       * on_begin_accumulate(shader_clipping, uber_shader_active);
       * .. add all color-item shaders acting on all material-item shader with any blend mode
       * on_end_accumulate()
       * \endcode
       */
      virtual
      uint32_t
      on_uber_shader_of_all(enum clip_window_value_type_t shader_clipping) = 0;

    private:
      friend class RenderBackend;

      enum clip_window_value_type_t m_shader_clipping;
      bool m_accumulating;
      Cookie m_cookie;
    };

    /*!
     * Conveniant class the emcompass an \ref astral::clip_window_value_type_t
     * and a RenderValue<ClipWindow> value.
     */
    class ClipWindowValue
    {
    public:
      /*!
       * Ctor that leaves \ref m_clip_window as invalid and sets
       * \ref m_enforce as true.
       */
      ClipWindowValue(void):
        m_enforce(true)
      {}

      /*!
       * Ctor
       * \param c initial value for \ref m_clip_window
       * \param e initial value for \ref m_enforce
       */
      ClipWindowValue(RenderValue<ClipWindow> c, bool e):
        m_enforce(e),
        m_clip_window(c)
      {}

      /*!
       * Returns the \ref astral::clip_window_value_type_t value
       * for this \ref ClipWindowValue
       */
      enum clip_window_value_type_t
      clip_window_value_type(void) const
      {
        return (m_clip_window.valid()) ?
          ((m_enforce) ? clip_window_present_enforce : clip_window_present_optional) :
          clip_window_not_present;
      }

      /*!
       * If true, then if a m_clip_window is valid, then
       * the backend will enforce that pixels outside of
       * the clip window are not drawn to the framebuffer.
       * If false, then if m_clip_window is valid, then
       * a backend can assume that another means (namely
       * depth testing) will clip the pixels outside of
       * clip window, but the backend can take an early
       * out in fragment shading to improve performance.
       */
      bool m_enforce;

      /*!
       * If valid, provides a clip window. Depending on the
       * value of \ref m_enforce, a backend must enforce
       * the clip window or can take jut an early out
       * in fragment shading for pixel outside of the
       * clip window.
       */
      RenderValue<ClipWindow> m_clip_window;
    };

    /*!
     * Ctor.
     * \param engine the astral::RenderEngine that creates this
     *               astral::RenderBackend; ths cerated backend
     *               is gauranteed to use the resource atlases
     *               and shaders of the passed astral::RenderEngine
     *               only.
     */
    explicit
    RenderBackend(RenderEngine &engine);

    virtual
    ~RenderBackend();

    /*!
     * returns the size of the array that should be passed
     * to end() to get all the rendering statistics.
     */
    unsigned int
    render_stats_size(void) const;

    /*!
     * Returns the label for the indexed render stat.
     */
    c_string
    render_stats_label(unsigned int idx) const;

    /*!
     * Begin rendering. It is illegal to nest begin() calls.
     */
    void
    begin(void);

    /*!
     * Finish the current DataBuffer and send all accumulated
     * DataBuffer data to the GPU for processing.
     */
    void
    end(c_array<unsigned int> out_stats);

    /*!
     * Returns true if inside a begin() / end()
     * pair.
     */
    bool
    rendering(void)
    {
      return m_rendering;
    }

    /*!
     * Set the current render-target. May only be called
     * within a begin()/end() pair. Calls to begin_render_target()
     * CANNOT be nested. An implementation should assume that
     * the 3D API state is completely dirty and initialize
     * 3D API state on begin_render_target(). In addition a caller
     * guarantees that it will not pollute the 3D API state
     * within a begin_render_target()/end_render_target() pair.
     * \param clear_params specifies what buffer, if any, and how to clear
     * \param rt render target to which to render
     */
    void
    begin_render_target(const ClearParams &clear_params, RenderTarget &rt);

    /*!
     * Call to indicate that rendering to the astral::RenderTarget
     * from the last call to begin_render_target() has ended. After
     * this call, reading from that astral::RenderTarget shall reflect
     * the commands sent to it.
     */
    void
    end_render_target(void);

    /*!
     * Returns the current render target
     */
    const reference_counted_ptr<RenderTarget>&
    current_render_target(void)
    {
      return m_current_rt;
    }

    /*!
     * Returns a astral::RenderValue that can be reused within
     * the current begin()/end() pair to indicate to use
     * the passed astral::Transformation as the transformation
     * value. Do NOT save any handle values once end() is called.
     */
    RenderValue<Transformation>
    create_value(const Transformation &value)
    {
      RenderValue<Transformation> R;

      ASTRALassert(m_rendering);
      R.init(allocate_transformation(value), *this);
      return R;
    }

    /*!
     * Returns the astral::Transformation value that generated the
     * astral::RenderValue<astral::Transformation>
     */
    const Transformation&
    fetch(RenderValue<Transformation> v)
    {
      return fetch_transformation(v.m_cookie);
    }

    /*!
     * Returns a astral::RenderValue that can be reused within
     * the current begin()/end() pair to indicate to use
     * the passed astral::RenderBackendTranslate as the translate
     * to be applied after the current astral::Transformation value.
     * Do NOT save any handle values once end() is called.
     */
    RenderValue<ScaleTranslate>
    create_value(const ScaleTranslate &value)
    {
      RenderValue<ScaleTranslate> R;

      ASTRALassert(m_rendering);
      R.init(allocate_translate(value), *this);
      return R;
    }

    /*!
     * Returns the astral::ScaleTranslate value that generated the
     * astral::RenderValue<astral::ScaleTranslate>
     */
    const ScaleTranslate&
    fetch(RenderValue<ScaleTranslate> v)
    {
      return fetch_translate(v.m_cookie);
    }

    /*!
     * Returns a astral::RenderValue that can be reused within
     * the current begin()/end() pair to indicate to use
     * the passed astral::ClipWindow for GPU enforced clipping.
     * Do NOT save any handle values once end() is called.
     */
    RenderValue<ClipWindow>
    create_value(const ClipWindow &value)
    {
      RenderValue<ClipWindow> R;

      ASTRALassert(m_rendering);
      R.init(allocate_clip_window(value), *this);
      return R;
    }

    /*!
     * Returns the astral::ClipWindow value that generated the
     * astral::RenderValue<astral::ClipWindow>
     */
    const ClipWindow&
    fetch(RenderValue<ClipWindow> v)
    {
      return fetch_clip_window(v.m_cookie);
    }

    /*!
     * Returns a astral::RenderValue that can be reused within
     * the current begin()/end() pair to indicate to use
     * the passed astral::Brush for the brush.
     * Do NOT save any handle values once end() is called.
     */
    RenderValue<Brush>
    create_value(Brush value);

    /*!
     * Returns the astral::Brush value that generated the
     * astral::RenderValue<astral::Brush>
     */
    const Brush&
    fetch(RenderValue<Brush> v)
    {
      return fetch_render_brush(v.m_cookie);
    }

    /*!
     * Returns a astral::RenderValue that can be reused within
     * the current begin()/end() pair to indicate to use
     * the passed astral::ImageSampler for a brush.
     * Do NOT save any handle values once end() is called.
     */
    RenderValue<ImageSampler>
    create_value(const ImageSampler &value)
    {
      RenderValue<ImageSampler> R;

      ASTRALassert(m_rendering);
      R.init(allocate_image_sampler(value), *this);
      return R;
    }

    /*!
     * Returns the astral::ImageSampler value that generated the
     * astral::RenderValue<astral::ImageSampler>
     */
    const ImageSampler&
    fetch(RenderValue<ImageSampler> v)
    {
      return fetch_image_sampler(v.m_cookie);
    }

    /*!
     * Returns a astral::RenderValue that can be reused within
     * the current begin()/end() pair to indicate to use
     * the passed astral::Gradient for a brush.
     * Do NOT save any handle values once end() is called.
     */
    RenderValue<Gradient>
    create_value(const Gradient &value)
    {
      RenderValue<Gradient> R;

      ASTRALassert(m_rendering);
      R.init(allocate_gradient(value), *this);
      return R;
    }

    /*!
     * Returns the astral::Gradient value that generated the
     * astral::RenderValue<astral::Gradient>
     */
    const Gradient&
    fetch(RenderValue<Gradient> v)
    {
      return fetch_gradient(v.m_cookie);
    }

    /*!
     * Returns a astral::RenderValue that can be reused within
     * the current begin()/end() pair to indicate to use
     * the passed astral::GradientTransformation for a brush.
     * Do NOT save any handle values once end() is called.
     */
    RenderValue<GradientTransformation>
    create_value(const GradientTransformation &value)
    {
      RenderValue<GradientTransformation> R;

      ASTRALassert(m_rendering);
      R.init(allocate_image_transformation(value), *this);
      return R;
    }

    /*!
     * Returns the astral::GradientTransformation value that generated the
     * astral::RenderValue<astral::GradientTransformation>
     */
    const GradientTransformation&
    fetch(RenderValue<GradientTransformation> v)
    {
      return fetch_image_transformation(v.m_cookie);
    }

    /*!
     * Returns a astral::RenderValue that can be reused within
     * the current begin()/end() pair to indicate to use
     * the passed astral::ShadowMap. Do NOT save any handle
     * values once end() is called.
     */
    RenderValue<const ShadowMap&>
    create_value(const ShadowMap &shadow_map)
    {
      RenderValue<const ShadowMap&> R;

      ASTRALassert(m_rendering);
      R.init(allocate_shadow_map(shadow_map), *this);
      return R;
    }

    /*!
     * Returns the astral::ShadowMap value that generated the
     * astral::RenderValue<astral::ShadowMap>
     */
    const ShadowMap&
    fetch(RenderValue<const ShadowMap&> v)
    {
      return fetch_shadow_map(v.m_cookie);
    }

    /*!
     * Returns an astral::RenderValue that can be reused within
     * the current begin()/end() pair to indicate to use the
     * passed astral::EmulateFramebufferFetch. Do NOT save any
     * handle values once end() is called.
     */
    RenderValue<EmulateFramebufferFetch>
    create_value(const EmulateFramebufferFetch &value)
    {
      RenderValue<EmulateFramebufferFetch> R;

      ASTRALassert(m_rendering);
      R.init(allocate_framebuffer_pixels(value), *this);
      return R;
    }

    /*!
     * Returns the astral::EmulateFramebufferFetch value that generated
     * the astral::RenderValue<astral::EmulateFramebufferFetch>
     */
    const EmulateFramebufferFetch&
    fetch(RenderValue<EmulateFramebufferFetch> v)
    {
      return fetch_framebuffer_pixels(v.m_cookie);
    }

    /*!
     * Create an astral::RenderValue that can be reused within
     * the current begin()/end() pair to indicate to use the
     * passed astral::RenderClipElement object value. Do NOT
     * save any handle values once end() is called.
     */
    RenderValue<const RenderClipElement*>
    create_value(const RenderClipElement *p)
    {
      RenderValue<const RenderClipElement*> R;

      ASTRALassert(p);
      ASTRALassert(m_rendering);
      R.init(allocate_render_clip_element(p), *this);
      return R;
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
      RenderValue<T> R;

      R.init(cookie, *this);
      return R;
    }

    /*!
     * Request a astral::ItemData that can be reused within
     * the current begin()/end() pair to indicate to use
     * the passed data array for the value of the shader-data.
     * Do NOT save any handle values once end() is called.
     */
    ItemData
    create_item_data(c_array<const gvec4> value,
                     c_array<const ItemDataValueMapping::entry> item_data_value_map,
                     const ItemDataDependencies &dependencies)
    {
      ItemData R;

      ASTRALassert(m_rendering);
      R.init(allocate_item_data(value, item_data_value_map, dependencies), *this);
      return R;
    }

    /*!
     * Returns the -raw- values of a astral::ItemData
     */
    c_array<const gvec4>
    fetch(ItemData v)
    {
      return fetch_item_data(v.m_cookie);
    }

    /*!
     * Add a command to mask or unmask the color writes
     * \param b for each channel of b, if the value is true,
     *          permit color writes to that channel, otherwise
     *          prevent color writes to that channel
     */
    virtual
    void
    color_write_mask(bvec4 b) = 0;

    /*!
     * Add a command to enable or disable depth testing.
     * The depth testing, in conjuction with the shader
     * is expected to be a -strict- depth test that passes
     * on strictly increasing values of the z argument to
     * draw_render_data().
     * \param b if true, enable depth testing. If false,
     *          disable depth testing.
     */
    virtual
    void
    depth_buffer_mode(enum depth_buffer_mode_t b) = 0;

    /*!
     * Add a command to set the stencil test and ops.
     */
    virtual
    void
    set_stencil_state(const StencilState &st) = 0;

    /*!
     * Add a command to specify that the fragment shader
     * is to emit values in a specified color space.
     */
    virtual
    void
    set_fragment_shader_emit(enum colorspace_t encoding) = 0;

    /*!
     * Add the data to the current command buffer
     * to draw. The other of the vertices and the
     * grouping of the vertices is EXACTLY the
     * other provided to draw_render_data().
     * \param z the z-value which is used in the depth test; the
     *          depth test is -strict- i.e. a fragment passes if
     *          the z-value passed is greater than or equal to that
     *          which is in the framebuffer. The z-value takes on
     *          the special meaning if its value matches the value
     *          of a value in \ref depth_buffer_value. In that case
     *          the item drawn is for setting the depth buffer to
     *          the value as indicated by the enumeration.
     * \param shaders shaders to use to render
     * \param st the astral::RenderValues that specify the how to draw:
     *           brush, transformation, etc.
     * \param uber_shader_cookie if value is not \ref InvalidRenderValue,
     *                           provides the value of UberShadingKey::cookie()
     *                           of an uber-shader that can perform the draw
     * \param tr the scale-translate to apply
     * \param cl the clipping window to apply
     * \param permute_xy AFTER clipping and applying the scale translate tr,
     *                         permute the x and y coordinates of the render
     *                         target writes
     * \param R array of pairs where .first is an index into shaders
     *          on what shader to use and .second is arange into the
     *          vertices of the astral::VertexDataAllocator used by this
     *          astral::RenderBackend.
     */
    void
    draw_render_data(unsigned int z,
                     c_array<const pointer<const ItemShader>> shaders,
                     const RenderValues &st,
                     UberShadingKey::Cookie uber_shader_cookie,
                     RenderValue<ScaleTranslate> tr,
                     ClipWindowValue cl,
                     bool permute_xy,
                     c_array<const std::pair<unsigned int, range_type<int>>> R);

    /*!
     * Add the data to the current command buffer
     * to draw. The other of the vertices and the
     * grouping of the vertices is EXACTLY the
     * other provided to draw_render_data().
     * \param z the z-value which is used in the depth test; the
     *          depth test is -strict- i.e. a fragment passes if
     *          the z-value passed is greater than or equal to that
     *          which is in the framebuffer. The z-value takes on
     *          the special meaning if its value matches the value
     *          of a value in \ref depth_buffer_value. In that case
     *          the item drawn is for setting the depth buffer to
     *          the value as indicated by the enumeration.
     * \param shader shaders to use to render
     * \param st the astral::RenderValues that specify the how to draw:
     *           brush, transformation, etc.
     * \param uber_shader_cookie if value is not \ref InvalidRenderValue,
     *                           provides the value of UberShadingKey::cookie()
     *                           of an uber-shader that can perform the draw
     * \param tr the scale-translate to apply
     * \param cl the clipping window to apply
     * \param permute_xy AFTER clipping and applying the scale translate tr,
     *                         permute the x and y coordinates of the render
     *                         target writes
     * \param R array of ranges into the vertices of the
     *          astral::VertexDataAllocator used by this
     *          astral::RenderBackend.
     */
    void
    draw_render_data(unsigned int z,
                     const ItemShader &shader,
                     const RenderValues &st,
                     UberShadingKey::Cookie uber_shader_cookie,
                     RenderValue<ScaleTranslate> tr,
                     ClipWindowValue cl,
                     bool permute_xy,
                     c_array<const range_type<int>> R);

    /*!
     * Add the data to the current command buffer
     * to draw the passed astral::VertexData.
     * It is the caller's responsibility that the passed
     * astral::VertexData stays alive until
     * end_render_target() is called.
     * \param z the z-value which is used in the depth test; the
     *          depth test is -strict- i.e. a fragment passes if
     *          the z-value passed is greater than or equal to that
     *          which is in the framebuffer. The z-value takes on
     *          the special meaning if its value matches the value
     *          of a value in \ref depth_buffer_value. In that case
     *          the item drawn is for setting the depth buffer to
     *          the value as indicated by the enumeration.
     * \param shader shader to use to render
     * \param st the astral::RenderValues that specify the how to draw:
     *           brush, transformation, etc.
     * \param uber_shader_cookie if value is not \ref InvalidRenderValue,
     *                           provides the value of UberShadingKey::cookie()
     *                           of an uber-shader that can perform the draw
     * \param tr the scale-translate to apply
     * \param cl the clipping window to apply
     * \param permute_xy AFTER clipping and applying the scale translate tr,
     *                         permute the x and y coordinates of the render
     *                         target writes
     * \param R of range into the vertices of the
     *          astral::VertexDataAllocator used by this
     *          astral::RenderBackend.
     */
    void
    draw_render_data(unsigned int z,
                     const ItemShader &shader,
                     const RenderValues &st,
                     UberShadingKey::Cookie uber_shader_cookie,
                     RenderValue<ScaleTranslate> tr,
                     ClipWindowValue cl,
                     bool permute_xy,
                     range_type<int> R)
    {
      std::pair<unsigned int, range_type<int>> RR(0, R);
      pointer<const ItemShader> shader_ptr(&shader);
      c_array<const pointer<const ItemShader>> shaders(&shader_ptr, 1);

      draw_render_data(z, shaders, st, uber_shader_cookie, tr, cl, permute_xy,
                       c_array<const std::pair<unsigned int, range_type<int>>>(&RR, 1));
    }

    /*!
     * Used by astral::Renderer to prevent modifying a Image
     * once it is in use.
     */
    ImageID
    image_id(RenderValue<ImageSampler> v)
    {
      return (v.valid()) ?
        fetch(v).image_id() :
        ImageID();
    }

    /*!
     * Used by astral::Renderer to prevent modifying a Image
     * once it is in use.
     */
    ShadowMapID
    shadow_map_id(RenderValue<const ShadowMap&> v)
    {
      return (v.valid()) ?
        fetch(v).ID() :
        ShadowMapID();
    }

    /*!
     * Used by astral::Renderer to prevent modifying a
     * Image once it is in use.
     */
    c_array<const ShadowMapID>
    shadow_map_id(ItemData v)
    {
      return (v.valid()) ?
        shadow_map_id_of_item_data(v.m_cookie):
        c_array<const ShadowMapID>();
    }

    /*!
     * Used by astral::Renderer to prevent modifying an Image
     * once it is in use.
     */
    ImageID
    image_id(RenderValue<Brush> v)
    {
      return (v.valid()) ?
        image_id(fetch(v).m_image) :
        ImageID();
    }

    /*!
     * Used by astral::Renderer to prevent modifying a
     * Image once it is in use.
     */
    c_array<const ImageID>
    image_id(ItemData v)
    {
      return (v.valid()) ?
        image_id_of_item_data(v.m_cookie):
        c_array<const ImageID>();
    }

    /*!
     * Returns the number of begin()/end() pairs that the RenderBackend
     * has experienced.
     */
    unsigned int
    number_renders(void) const
    {
      return m_number_renders;
    }

    /*!
     * To be implemented by a derived class to create an
     * UberShadingKey object.
     */
    virtual
    reference_counted_ptr<UberShadingKey>
    create_uber_shading_key(void) = 0;

    /*!
     * Given a \ref render_backend_stats_t value,
     * return an index into the arrays returned
     * by end() and render_stats_labels() of
     * where the stat is located.
     */
    unsigned int
    stat_index(enum render_backend_stats_t st) const;

    /*!
     * Given a \ref DerivedStat value, return an
     * index into the arrays returned by end()
     * and render_stats_labels() of where the stat
     * is located.
     */
    unsigned int
    stat_index(DerivedStat st) const;

  protected:
    /*!
     * To be implemented by a derived class to add the data
     * to the current command buffer to draw a astral::VertexData
     * \param z the z-value to use for occlusion by the depth buffer
     * \param shaders shaders to use to render
     * \param st the astral::RenderValues that specify the how to draw:
     *           brush, transformation, etc.
     * \param uber_shader_cookie if value is not \ref InvalidRenderValue,
     *                           provides the value of UberShadingKey::cookie()
     *                           of an uber-shader that can perform the draw
     * \param tr the astral::ScaleTranslate, if any, to apply
     * \param cl the astral::ClipWindow, if any, to apply and how
     * \param permute_xy AFTER clipping and applying the scale translate tr,
     *                         permute the x and y coordinates of the render
     *                         target writes
     * \param R array of pairs where .first is an index into shaders
     *          on what shader to use and .second is arange into the
     *          vertices of the astral::VertexDataAllocator used by this
     *          astral::RenderBackend.
     */
    virtual
    void
    on_draw_render_data(unsigned int z,
                        c_array<const pointer<const ItemShader>> shaders,
                        const RenderValues &st,
                        UberShadingKey::Cookie uber_shader_cookie,
                        RenderValue<ScaleTranslate> tr,
                        ClipWindowValue cl,
                        bool permute_xy,
                        c_array<const std::pair<unsigned int, range_type<int>>> R) = 0;

    /*!
     * To be implemented by a derived class to store into a buffer
     * the values of a astral::Transformation. Returns a 32-bit cookie
     * value to identify the value. Routine cannot return the value
     * astral::InvalidRenderValue as that value is reserved.
     */
    virtual
    uint32_t
    allocate_transformation(const Transformation &value) = 0;

    /*!
     * To be implemented by a derived class to fetch the
     * value that was passed to \ref allocate_transformation().
     */
    virtual
    const Transformation&
    fetch_transformation(uint32_t cookie) = 0;

    /*!
     * To be implemented by a derived class to store into a buffer
     * the values of a astral::RenderBackendTranslate. Returns a 32-bit
     * cookie value to identify the value. Routine cannot return the
     * value astral::InvalidRenderValue as that value is reserved.
     */
    virtual
    uint32_t
    allocate_translate(const ScaleTranslate &value) = 0;

    /*!
     * To be implemented by a derived class to fetch the
     * value that was passed to \ref allocate_translate().
     */
    virtual
    const ScaleTranslate&
    fetch_translate(uint32_t cookie) = 0;

    /*!
     * To be implemented by a derived class to store into a buffer
     * the values of a astral::ClipWindow. Returns a 32-bit cookie
     * value to identify the value. Routine cannot return the value
     * astral::InvalidRenderValue as that value is reserved.
     */
    virtual
    uint32_t
    allocate_clip_window(const ClipWindow &value) = 0;

    /*!
     * To be implemented by a derived class to fetch the
     * value that was passed to \ref allocate_clip_window().
     */
    virtual
    const ClipWindow&
    fetch_clip_window(uint32_t cookie) = 0;

    /*!
     * To be implemented by a derived class to store into a buffer
     * the values of a astral::Brush. Returns a 32-bit
     * cookie value to identify the value. NOTE: an implementation
     * will need to check the fields of the passed value if
     * they are valid. When they are not valid, that indicates
     * the the brush does not have that feature active. Routine
     * cannot return the value astral::InvalidRenderValue as that value
     * is reserved.
     */
    virtual
    uint32_t
    allocate_render_brush(const Brush &value) = 0;

    /*!
     * To be implemented by a derived class to fetch the
     * value that was passed to \ref allocate_render_brush().
     */
    virtual
    const Brush&
    fetch_render_brush(uint32_t cookie) = 0;

    /*!
     * To be implemented by a derived class to store into a buffer
     * the values of a astral::ImageSampler. Returns a 32-bit cookie
     * value to identify the value. Routine cannot return the value
     * astral::InvalidRenderValue as that value is reserved. An
     * implementation must NEVER save a reference to the astral::Image
     * object the passed astral::ImageSampler references; instead an
     * implementation should extract the needed values from the
     * RenderClipElement for the purpose of packing data.
     */
    virtual
    uint32_t
    allocate_image_sampler(const ImageSampler &value) = 0;

    /*!
     * To be implemented by a derived class to fetch the
     * value that was passed to \ref allocate_image_sampler().
     */
    virtual
    const ImageSampler&
    fetch_image_sampler(uint32_t cookie) = 0;

    /*!
     * To be implemented by a derived class to store into a buffer
     * the values of a astral::Gradient. Returns a 32-bit cookie
     * value to identify the value. Routine cannot return the value
     * astral::InvalidRenderValue as that value is reserved.
     */
    virtual
    uint32_t
    allocate_gradient(const Gradient &value) = 0;

    /*!
     * To be implemented by a derived class to fetch the
     * value that was passed to \ref allocate_gradient().
     */
    virtual
    const Gradient&
    fetch_gradient(uint32_t cookie) = 0;

    /*!
     * To be implemented by a derived class to store into a buffer
     * the values of a astral::GradientTransformation. Returns a 32-bit cookie
     * value to identify the value. Routine cannot return the value
     * astral::InvalidRenderValue as that value is reserved.
     */
    virtual
    uint32_t
    allocate_image_transformation(const GradientTransformation &value) = 0;

    /*!
     * To be implemented by a derived class to fetch the
     * value that was passed to \ref allocate_image_transformation().
     */
    virtual
    const GradientTransformation&
    fetch_image_transformation(uint32_t cookie) = 0;

    /*!
     * To be implemented by a derived class to store into a buffer
     * the values of a const  astral::ShadowMap&. Returns a 32-bit cookie
     * value to identify the value. Routine cannot return the value
     * astral::InvalidRenderValue as that value is reserved. An
     * implementation must save a reference to the object passed.
     */
    virtual
    uint32_t
    allocate_shadow_map(const ShadowMap &value) = 0;

    /*!
     * To be implemented by a derived class to fetch the
     * value that was passed to \ref allocate_shadow_map().
     */
    virtual
    const ShadowMap&
    fetch_shadow_map(uint32_t cookie) = 0;

    /*!
     * To be implemented by a derived class to store into a buffer
     * the value of a astral::EmulateFramebufferFetch. Returns a 32-bit
     * cookie value to identify the value. Routine cannot return
     * the value astral::InvalidRenderValue as that value is
     * reserved.
     */
    virtual
    uint32_t
    allocate_framebuffer_pixels(const EmulateFramebufferFetch &value) = 0;

    /*!
     * To be implemented by a derived class to fetch the
     * value that was passed to \ref allocate_framebuffer_pixels().
     */
    virtual
    const EmulateFramebufferFetch&
    fetch_framebuffer_pixels(uint32_t cookie) = 0;

    /*!
     * To be implemented by a derived class to store into a buffer
     * the value of a astral::EmulateFramebufferFetch. Returns a 32-bit
     * cookie value to identify the value. Routine cannot return
     * the value astral::InvalidRenderValue as that value is
     * reserved. An implementation must NEVER save a reference to
     * the passed object; instead an implementation should extract
     * the needed values from the RenderClipElement for the purpose
     * of packing data.
     */
    virtual
    uint32_t
    allocate_render_clip_element(const RenderClipElement *value) = 0;

    /*!
     * To be implemented by a derived class to make room into a buffer
     * for value used by a ItemShader that are shared across vertices
     * and fragments. Returns a 32-bit cookie value to identify the value.
     * Routine cannot return the value astral::InvalidRenderValue as that
     * value is reserved.
     */
    virtual
    uint32_t
    allocate_item_data(c_array<const gvec4> value,
                       c_array<const ItemDataValueMapping::entry> item_data_value_map,
                       const ItemDataDependencies &dependencies) = 0;

    /*!
     * To be implemented by a derived class to fetch the
     * value that was passed to \ref allocate_item_data()
     */
    virtual
    c_array<const gvec4>
    fetch_item_data(uint32_t cookie) = 0;

    /*!
     * To be implemented by a derived class to return an array
     * (that is possibly empty) of the the values of
     * ImageSampler::m_image_id of any astral::ImageSampler
     * referenced by item data as allocated by allocate_item_data().
     * The array should NOT include any of those values for which
     * ImageSampler::m_image_id is not valid.
     */
    virtual
    c_array<const ImageID>
    image_id_of_item_data(uint32_t cookie) = 0;

    /*!
     * To be implemented by a derived class to return an array
     * (that is possibly empty) of the the values of
     * ShadowMap::ID() of any astral::ShadowMap referenced
     * by item data as allocated by allocate_item_data().
     */
    virtual
    c_array<const ShadowMapID>
    shadow_map_id_of_item_data(uint32_t cookie) = 0;

    /*!
     * Called by RenderBackend whenever the render target changes
     * and rendering to color pixels.
     * \param clear_params specifies what buffer, if any, and how to clear
     * \param rt render target to which to render
     */
    virtual
    void
    on_begin_render_target(const ClearParams &clear_params, RenderTarget &rt) = 0;

    /*!
     * Called by RenderBackend whenever rendering to the render-target
     * named in the last call to on_begin_render_target() has ended.
     * After this call, reading from that astral::RenderTarget shall
     * reflect the commands sent that affected it.
     */
    virtual
    void
    on_end_render_target(RenderTarget &rt) = 0;

    /*!
     * To be implemented by a derived class to initialize the GPU state
     * for rendering.
     */
    virtual
    void
    on_begin(void) = 0;

    /*!
     * To be implemented by a derived class to send any remaining GPU
     * commands to the 3D API.
     */
    virtual
    void
    on_end(c_array<unsigned int> out_stats) = 0;

    /*!
     * To be optionally impemented by a derived class
     * to return how many entries for render statistics
     * the derived backend has private to it.
     */
    virtual
    unsigned int
    render_stats_size_derived(void) const
    {
      return 0;
    }

    /*!
     * To be optionally impemented by a derived class
     * to return the label of the indexed render stat.
     */
    virtual
    c_string
    render_stats_label_derived(unsigned int) const
    {
      return "";
    }

  private:
    std::vector<std::pair<unsigned int, range_type<int>>> m_tmp_R;
    reference_counted_ptr<RenderEngine> m_engine;
    bool m_rendering;
    unsigned int m_number_renders;
    reference_counted_ptr<RenderTarget> m_current_rt;
    vecN<unsigned int, number_render_stats> m_base_stats;

    reference_counted_ptr<const MaterialShader> m_brush_shader;
  };

  ///@cond

  template<typename T>
  inline
  void
  RenderValue<T>::
  init(uint32_t v, RenderBackend &r)
  {
    m_cookie = v;
    m_begin_cnt = r.number_renders();
    m_backend = &r;
  }

  template<typename T>
  inline
  bool
  RenderValue<T>::
  valid(void) const
  {
    return m_cookie != InvalidRenderValue
      && m_backend != nullptr
      && m_backend->rendering()
      && m_backend->number_renders() == m_begin_cnt;
  }

  template<typename T>
  inline
  const T&
  RenderValue<T>::
  value(void) const
  {
    ASTRALassert(valid());
    return m_backend->fetch(*this);
  }

  inline
  void
  ItemData::
  init(uint32_t v, RenderBackend &r)
  {
    m_cookie = v;
    m_begin_cnt = r.number_renders();
    m_backend = &r;
  }

  inline
  bool
  ItemData::
  valid(void) const
  {
    return m_cookie != InvalidRenderValue
      && m_backend != nullptr
      && m_backend->rendering()
      && m_backend->number_renders() == m_begin_cnt;
  }
  ///@endcond

/*! @} */
}

#endif

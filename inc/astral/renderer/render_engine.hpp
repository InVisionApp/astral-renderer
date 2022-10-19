/*!
 * \file render_engine.hpp
 * \brief file render_engine.hpp
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

#ifndef ASTRAL_RENDER_ENGINE_HPP
#define ASTRAL_RENDER_ENGINE_HPP

#include <astral/renderer/image.hpp>
#include <astral/renderer/static_data.hpp>
#include <astral/renderer/shadow_map.hpp>
#include <astral/renderer/backend/blend_mode_information.hpp>
#include <astral/renderer/shader/shader_set.hpp>
#include <astral/renderer/shader/shader_detail.hpp>
#include <astral/renderer/effect/effect_shader_set.hpp>
#include <astral/renderer/effect/effect_set.hpp>

namespace astral
{
  class ColorStopSequenceAtlasBacking;
  class VertexDataBacking;
  class StaticDataBacking;
  class ImageAtlasIndexBacking;
  class ImageAtlasColorBacking;
  class ShadowMapAtlasBacking;
  class ColorStopSequenceAtlas;
  class VertexDataAllocator;
  class StaticDataAllocator16;
  class StaticDataAllocator32;
  class ImageAtlas;
  class ShadowMapAtlas;
  class RenderBackend;

/*!\addtogroup Renderer
 * @{
 */

  /*!
   * \brief
   * An astral::RenderEngine represents essentially a factory for
   * astral::RenderBackend objects; In addition, astral::ItemShader
   * objects are created by an astral::RenderEngine and only those
   * asral::ItemShader objects created by the astral::RenderEngine
   * can be used by the astral::RenderBackend objects from the creating
   * astral::RenderEngine.
   */
  class RenderEngine:public reference_counted<RenderEngine>::non_concurrent
  {
  public:
    /*!
     * A RenderEngine::OverridableProperties represents the properties
     * that can be overridden by astral::RenderEngine that can impact
     * rendering strategies.
     */
    class OverridableProperties
    {
    public:
      OverridableProperties(void):
        m_clip_window_strategy(clip_window_strategy_shader),
        m_uber_shader_method(uber_shader_none)
      {}

      /*!
       * Set \ref m_clip_window_strategy
       * \param v new value
       */
      OverridableProperties&
      clip_window_strategy(enum clip_window_strategy_t v)
      {
        m_clip_window_strategy = v;
        return *this;
      }

      /*!
       * Set \ref m_uber_shader_method
       * \param v new value
       */
      OverridableProperties&
      uber_shader_method(enum uber_shader_method_t v)
      {
        m_uber_shader_method = v;
        return *this;
      }

      /*!
       * Specifies the clip window implementation strategy
       * Default value is \ref clip_window_strategy_shader.
       */
      enum clip_window_strategy_t m_clip_window_strategy;

      /*!
       * Specifies if and how to use uber-shading when rendering
       */
      enum uber_shader_method_t m_uber_shader_method;
    };

    /*!
     * A RenderEngine::Properties represents the properties of an
     * astral::RenderEngine that can impact rendering strategies.
     */
    class Properties
    {
    public:
      /*!
       * Specifies default values for the properties that
       * astral::Renderer can override.
       */
      OverridableProperties m_overridable_properties;

      /*!
       * Specifies how blending is accomplished
       */
      BlendModeInformation m_blend_mode_information;
    };

    virtual
    ~RenderEngine();

    /*!
     * Returns the engine properties that can impact rendering strategies.
     */
    const Properties&
    properties(void) const
    {
      return m_properties;
    }

    /*!
     * To be implemented by a derived class to create and return
     * a astral::RenderBackend object that uses the resource objects
     * and shaders of this astral::RenderEngine.
     */
    virtual
    reference_counted_ptr<RenderBackend>
    create_backend(void) = 0;

    /*!
     * To be implemented by a derived class to produce a \ref
     * RenderTarget derived object suitable for offscreen rendering
     * by astral::RenderBackend objects returned by create_backend().
     * \param dims dimensions of rendering area
     * \param out_color_buffer location to which to write the color buffer
     *                         of the returned \ref RenderTarget, can be nullptr.
     * \param out_ds_buffer location to which to write the depth-stencil buffer
     *                      of the returned \ref RenderTarget, can be nullptr.
     */
    virtual
    reference_counted_ptr<RenderTarget>
    create_render_target(ivec2 dims,
                         reference_counted_ptr<ColorBuffer> *out_color_buffer = nullptr,
                         reference_counted_ptr<DepthStencilBuffer> *out_ds_buffer = nullptr) = 0;

    /*!
     * To be implemented by a derived class to return the default shaders
     * of the astral::RenderEngine that the astral::RenderBackend objects
     * from create_backend() may use.
     */
    virtual
    const ShaderSet&
    default_shaders(void) = 0;

    /*!
     * To be implemented by a derived class to return the shaders of
     * default_effects().
     */
    virtual
    const EffectShaderSet&
    default_effect_shaders(void) = 0;

    /*!
     * To be implemented by a derived class to return the default effects
     * of the astral::RenderEngine that the astral::RenderBackend objects
     * from astral::create_backend() may use.
     */
    virtual
    const EffectSet&
    default_effects(void) = 0;

    /*!
     * Returns the astral::ImageAtlas to use for creating/using tiled
     * image data for any astral::RenderBackend returned by create_backend().
     */
    ImageAtlas&
    image_atlas(void) const
    {
      return *m_image_atlas;
    }

    /*!
     * Returns the astral::ColorStopSequenceAtlas to use for creating/using
     * color-array data for any astral::RenderBackend returned by
     * create_backend().
     */
    ColorStopSequenceAtlas&
    colorstop_sequence_atlas(void) const
    {
      return *m_colorstop_sequence_atlas;
    }

    /*!
     * Returns the astral::VertexDataAllocator to use for creating/using
     * any astral::VertexData objects for any astral::RenderBackend returned by
     * create_backend().
     */
    VertexDataAllocator&
    vertex_data_allocator(void) const
    {
      return *m_vertex_data_allocator;
    }

    /*!
     * Returns the astral::StaticDataAllocator32 to use for creating/using
     * any astral::StaticData objects holding four tuples of 32-bit data
     * for any astral::RenderBackend returned by create_backend().
     */
    StaticDataAllocator32&
    static_data_allocator32(void) const
    {
      return *m_static_data_allocator32;
    }

    /*!
     * Returns the astral::StaticDataAllocator16 to use for creating/using
     * any astral::StaticData objects holding four tuples of 16-bit data
     * for any astral::RenderBackend returned by create_backend().
     */
    StaticDataAllocator16&
    static_data_allocator16(void) const
    {
      return *m_static_data_allocator16;
    }

    /*!
     * Returns the astral::ShadowMapAtlas to use for creating/using
     * any astral::ShadowMap objects for any astral::RenderBackend
     * returned by create_backend().
     */
    ShadowMapAtlas&
    shadow_map_atlas(void) const
    {
      return *m_shadow_map_atlas;
    }

    /*!
     * To be implemented by a derived class to pack a ImageSampler
     * value as static data.
     */
    virtual
    reference_counted_ptr<const StaticData>
    pack_image_sampler_as_static_data(const ImageSampler &image) = 0;

    ///@cond

    /* Used by astral::ItemShader ctor to allocate a unique
     * shader id.
     */
    unsigned int
    allocate_shader_id(detail::SubShaderCount num_sub_shaders)
    {
      unsigned int return_value(m_ID_count);

      m_ID_count += num_sub_shaders.m_v;
      return return_value;
    }

    /* Used by astral::Material ctor to allocate a unique
     * shader id.
     */
    unsigned int
    allocate_material_id(detail::SubShaderCount num_sub_shaders)
    {
      unsigned int return_value(m_material_ID_count);

      m_material_ID_count += num_sub_shaders.m_v;
      return return_value;
    }

    reference_counted_ptr<VertexData>
    create_streamer(detail::VertexDataStreamerSize sz)
    {
      return vertex_data_allocator().create_streamer(sz);
    }

    reference_counted_ptr<StaticData>
    create_streamer(detail::StaticDataStreamerSize<StaticDataBacking::type32> sz)
    {
      return static_data_allocator32().create_streamer(sz);
    }

    reference_counted_ptr<StaticData>
    create_streamer(detail::StaticDataStreamerSize<StaticDataBacking::type16> sz)
    {
      return static_data_allocator16().create_streamer(sz);
    }

    ///@endcond

  protected:
    /*!
     * Ctor.
     * \param P RenderEngine::Properties of the astral::RenderEngine
     * \param colorstop_sequence_backing GPU resource to back the
     *                                   color stops of the constructed
     *                                   astral::RenderEngine
     * \param vertex_data_backing GPU resource to back the vertices
     *                            of the constructed astral::VertexData
     * \param data_backing32 GPU resource to back the astral::StaticDataAllocator
     *                           for StaticDataBacking::type32 data
     * \param data_backing16 GPU resource to back the astral::StaticDataAllocator
     *                           for StaticDataBacking::type16 data
     * \param image_index_backing backing for index tiles of tiled images
     * \param image_color_backing backing for color tiles of tiled images
     * \param shadow_map_backing backing for rect shadow maps
     */
    RenderEngine(const Properties &P,
                 const reference_counted_ptr<ColorStopSequenceAtlasBacking> &colorstop_sequence_backing,
                 const reference_counted_ptr<VertexDataBacking> &vertex_data_backing,
                 const reference_counted_ptr<StaticDataBacking> &data_backing32,
                 const reference_counted_ptr<StaticDataBacking> &data_backing16,
                 const reference_counted_ptr<ImageAtlasIndexBacking> &image_index_backing,
                 const reference_counted_ptr<ImageAtlasColorBacking> &image_color_backing,
                 const reference_counted_ptr<ShadowMapAtlasBacking> &shadow_map_backing);

    /*!
     * To be used by a derived class to know how many shaders
     * are associated to this astral::RenderEngine.
     */
    unsigned int
    shader_count(void) const
    {
      return m_ID_count;
    }

  private:

  private:
    Properties m_properties;
    reference_counted_ptr<ColorStopSequenceAtlas> m_colorstop_sequence_atlas;
    reference_counted_ptr<VertexDataAllocator> m_vertex_data_allocator;
    reference_counted_ptr<StaticDataAllocator32> m_static_data_allocator32;
    reference_counted_ptr<StaticDataAllocator16> m_static_data_allocator16;
    reference_counted_ptr<ImageAtlas> m_image_atlas;
    reference_counted_ptr<ShadowMapAtlas> m_shadow_map_atlas;
    unsigned int m_ID_count, m_material_ID_count;

    reference_counted_ptr<MaterialShader> m_brush_shader;
  };

/*! @} */
}

#endif

/*!
 * \file render_engine_gl3.hpp
 * \brief file render_engine_gl3.hpp
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

#ifndef ASTRAL_RENDER_ENGINE_GL3_HPP
#define ASTRAL_RENDER_ENGINE_GL3_HPP

#include <astral/util/gl/gl_program.hpp>
#include <astral/util/gl/gl_context_properties.hpp>
#include <astral/renderer/render_engine.hpp>
#include <astral/renderer/backend/render_backend.hpp>
#include <astral/renderer/gl3/item_shader_gl3.hpp>
#include <astral/renderer/gl3/material_shader_gl3.hpp>
#include <astral/renderer/gl3/shader_set_gl3.hpp>

namespace astral
{
  namespace gl
  {
/*!\addtogroup gl
 * @{
 */

    /*!
     * \brief
     * An astral::gl::RenderEngineGL3 is an implementation of
     * astral::RenderEngine.
     *
     * It uses various GL3/GLES3/WebGL2 features, including
     * - native integer support
     * - texture arrays (i.e. ASTRAL_GL_TEXTURE_2D_ARRAY)
     * - UBO's
     * .
     */
    class RenderEngineGL3:public RenderEngine
    {
    public:
      /*!
       * \brief
       * Enumeration to choose what shader to use in uber-shading
       * if the uber-shader of the key is not yet ready (for example
       * if the backend support parallel shader compiling and the
       * shader is not ready).
       */
      enum uber_shader_fallback_t
        {
           /*!
            * If the the uber-shader requested it not yet ready,
            * use the shader to the requested item, material and
            * blend.
            */
           uber_shader_fallback_separate,

           /*!
            * If the uber-shader requested it not yet ready,
            * use the uber-shader that contains all item-color
            * shaders, all material shaders and all blend modes
            */
           uber_shader_fallback_uber_all,

           /*!
            * No fall back at all, force the requested uber-shader
            * to be used.
            */
           uber_shader_fallback_none,

           uber_shader_fallback_last = uber_shader_fallback_none,
        };

      /*!
       * \brief
       * To perform batching, a RenderEngineGL3's backend will stash
       * many data values into a few buffers. This enumeration defines
       * the maximum number of data values for each type.
       */
      enum data_t:uint32_t
        {
          /*!
           * Buffer that stores the header for each item.
           */
          data_header,

          /*!
           * Buffer that holds the \ref Transformation values
           * for how items are trasformed.
           */
          data_item_transformation,

          /*!
           * Buffer that holds the \ref ScaleTranslate values
           * for how items are scaled-translated post \ref Transformation
           */
          data_item_scale_translate,

          /*!
           * Buffer that holds the \ref ClipWindow values
           * for how items are are clipped against the
           * clipping rect
           */
          data_clip_window,

          /*!
           * Buffer that holds the \ref Brush values
           * for what brush is applied to items.
           */
          data_brush,

          /*!
           * Buffer that holds the \ref Gradient values
           * of gradients that are part of a brush
           */
          data_gradient,

          /*!
           * Buffer that holds the \ref GradientTransformation values
           * that are how gradients and images are sampled
           */
          data_gradient_transformation,

          /*!
           * Buffer that holds the \ref ItemData values
           * that are the custom paremeters for a \ref ItemShader
           */
          data_item_data,

          /*!
           * Buffer that holds the \ref ImageSampler values
           */
          data_image,

          /*!
           * Buffer that holds \ref ShadowMap property values
           */
          data_shadow_map,

          /*!
           * Buffer that holds \ref RenderClipElement property values
           */
          data_clip_mask,

          number_data_types,
        };

      /*!
       * \brief
       * Enumeration to specify how a static and vertex
       * is realized: as a linear array or a 2D texture.
       */
      enum layout_t:uint32_t
        {
          /*!
           * Indicates that the data store backing is realized
           * as a linear buffer
           */
          linear_array,

          /*!
           * indicates that the data store backing is realized
           * as a texture 2D array
           */
          texture_2d_array,

	  layout_last = texture_2d_array
        };

      /*!
       * \brief
       * Enumeration to specify how to data is streamed to GL
       */
      enum data_streaming_t:uint32_t
        {
          /*!
           * Buffer data are streamed by buffer object
           * orphaning, i.e. calling glBufferData
           */
          data_streaming_bo_orphaning,

          /*!
           * Buffer data are streamed by mapping the
           * buffers to CPU address space
           */
          data_streaming_bo_mapping,

          /*!
           * Buffer data are streamed by calling
           * glBufferSubData
           */
          data_streaming_bo_subdata,

          data_streaming_bo_last = data_streaming_bo_subdata
        };

      /*!
       * Enumeration to encapsulate stat index of RenderEngineGL3
       */
      enum derived_stats_t:uint32_t
        {
          /*!
           * Value to feed to RenderBackend::stat_index(DerivedStat) const
           * to get the number of GL program binds.
           */
          number_program_binds = 0,

          /*!
           *  Value to feed to RenderBackend::stat_index(DerivedStat) const
           * to get the number of GL blend state changes
           */
          number_blend_state_changes,

          /*!
           * Value to feed to RenderBackend::stat_index(DerivedStat) const
           * to get the number of draw-groups.
           */
          number_item_groups,

          /*!
           * Value to feed to RenderBackend::stat_index(DerivedStat) const
           * to get the number of item buffers sent to GL. An item buffer
           * consists of all the data from the different values of \ref
           * data_t packed into a UBO.
           */
          number_item_buffers,

          /*!
           * Value to feed to RenderBackend::stat_index(DerivedStat) const
           * to get the number of unwritten bytes in the item buffers
           */
          unwritten_ubo_bytes,

          /*!
           * Value to feed to RenderBackend::stat_index(DerivedStat) const
           * to get the number of written bytes in the item buffers
           */
          written_ubo_bytes,

          /*!
           * Value to feed to RenderBackend::stat_index(DerivedStat) const
           * to get the percentage of \ref written_ubo_bytes against
           * \ref written_ubo_bytes + \ref unwritten_ubo_bytes
           */
          percentage_ubo_written,

          /*!
           * Value to feed to RenderBackend::stat_index(DerivedStat) const
           * to get the number of bytes across item buffers used by padding.
           */
          padded_ubo_bytes,

          /*!
           * Value to feed to RenderBackend::stat_index(DerivedStat) const
           * to get the actual number of GL draw calls issued to GL
           */
          number_draws,

          /*!
           * Value to feed to RenderBackend::stat_index(DerivedStat) const
           * to get the number of staging buffers used; a staging buffer
           * is a buffer generated by GL packing vertex and header ID's
           * into a single surface.
           */
          number_staging_buffers,

          /*!
           * Value to feed to RenderBackend::stat_index(DerivedStat) const
           * to get the number of blit entries used to fill all the staging
           * buffers
           */
          number_blit_entries,

          /*!
           * Value to feed to RenderBackend::stat_index(DerivedStat) const
           * to get the number of vertices used to create all of the
           * staging buffers
           */
          number_blit_rect_vertices,

          /*!
           * Value to feed to RenderBackend::stat_index(DerivedStat) const
           * to get the number of texels used by all of the staging buffers
           */
          number_vertex_surface_pixels,

          /*!
           * Value to feed to RenderBackend::stat_index(DerivedStat) const
           * to get the number of times that a super-uber shader was used
           * to prevent hitching from waiting for a small uber-shader to
           * compile.
           */
          number_times_super_uber_used,

          /*!
           * Value to feed to RenderBackend::stat_index(DerivedStat) const
           * to get the number of times that a shader with the specified
           * item, material and blend was used to prevent hitching from
           * waiting for an uber-shader to compile.
           */
          number_times_separate_used,

          /*!
           * Feed this value + the value of an enumeration from \ref data_t
           * to RenderBackend::stat_index(DerivedStat) const to get the stat
           * of how many of the given item values were uploaded to GL
           */
          number_items_bufferX,

          /*!
           * Feed this value + the value of an enumeration from \ref data_t
           * to RenderBackend::stat_index(DerivedStat) const to get the stat
           * of how many of the given item values were reused
           */
          number_reuses_bufferX = number_items_bufferX + number_data_types,

          /*!
           * Feed this value + the value of an enumeration from \ref data_t
           * to RenderBackend::stat_index(DerivedStat) const to get the stat
           * of how many of the given item value buffer became full, triggering
           * a draw.
           */
          number_times_bufferX_full = number_reuses_bufferX + number_data_types,

          number_total_stats = number_times_bufferX_full + number_data_types,
        };

      /*!
       * \brief
       * Config determines the limits and some of the how
       * that an astral::gl::RenderEngineGL3 will process
       * and handle data.
       */
      class Config
      {
      public:
        Config(void):
          m_initial_num_colorstop_atlas_layers(0),
          m_log2_dims_colorstop_atlas(12),
          m_max_per_draw_call(1024u),
          m_vertex_buffer_size(65536),
          m_uniform_buffer_size(3u * compute_min_ubo_size(m_max_per_draw_call)),
          m_use_texture_for_uniform_buffer(false),
          m_use_hw_clip_window(true),
          m_data_streaming(data_streaming_bo_orphaning),
          m_buffer_reuse_period(1u),
          m_log2_gpu_stream_surface_width(12),
          m_initial_static_data_size(256 * 1024),
          m_static_data_layout(linear_array),
          m_static_data_log2_width(10u),
          m_static_data_log2_height(10u),
          m_vertex_buffer_layout(linear_array),
          m_vertex_buffer_log2_width(10u),
          m_vertex_buffer_log2_height(10u),
          m_use_glsl_unpack_fp16(true),
          m_image_color_atlas_width_height(2048u),
          m_image_color_atlas_number_layers(1u),
          m_image_index_atlas_width_height(1024u),
          m_image_index_atlas_number_layers(1u),
          m_use_attributes(false),
          m_use_indices(false),
          m_shadow_map_atlas_width(8192),
          m_shadow_map_atlas_initial_height(4),
          m_inflate_degenerate_glue_joins(false),
          m_uber_shader_max_if_depth(2),
          m_uber_shader_max_if_length(4),
          m_uber_shader_fallback(uber_shader_fallback_none),
          m_force_shader_log_generation_before_use(false),
          m_max_number_color_backing_layers(128),
          m_max_number_index_backing_layers(128)
        {}

        /*!
         * Sets \ref m_initial_num_colorstop_atlas_layers
         * \param v value to use
         */
        Config&
        initial_num_colorstop_atlas_layers(unsigned int v)
        {
          m_initial_num_colorstop_atlas_layers = v;
          return *this;
        }

        /*!
         * Sets \ref m_image_color_atlas_width_height
         * \param v value to use
         */
        Config&
        image_color_atlas_width_height(unsigned int v)
        {
          m_image_color_atlas_width_height = v;
          return *this;
        }

        /*!
         * Sets \ref m_image_color_atlas_number_layers
         * \param v value to use
         */
        Config&
        image_color_atlas_number_layers(unsigned int v)
        {
          m_image_color_atlas_number_layers = v;
          return *this;
        }

        /*!
         * Sets \ref m_image_index_atlas_width_height
         * \param v value to use
         */
        Config&
        image_index_atlas_width_height(unsigned int v)
        {
          m_image_index_atlas_width_height = v;
          return *this;
        }

        /*!
         * Sets \ref m_image_index_atlas_number_layers
         * \param v value to use
         */
        Config&
        image_index_atlas_number_layers(unsigned int v)
        {
          m_image_index_atlas_number_layers = v;
          return *this;
        }

        /*!
         * Sets \ref m_log2_dims_colorstop_atlas
         * \param v value to use
         */
        Config&
        log2_dims_colorstop_atlas(uint32_t v)
        {
          m_log2_dims_colorstop_atlas = v;
          return *this;
        }

        /*!
         * Sets a specific element of \ref m_max_per_draw_call
         * \param v value to use
         * \param tp which element of \ref m_max_per_draw_call
         */
        Config&
        max_per_draw_call(enum data_t tp, unsigned int v)
        {
          m_max_per_draw_call[tp] = v;
          return *this;
        }

        /*!
         * Sets each element of \ref m_max_per_draw_call
         * \param v value to use
         */
        Config&
        max_per_draw_call(unsigned int v)
        {
          m_max_per_draw_call = vecN<unsigned int, number_data_types>(v);
          return *this;
        }

        /*!
         * Sets \ref m_vertex_buffer_size
         * \param v value to use
         */
        Config&
        vertex_buffer_size(unsigned int v)
        {
          m_vertex_buffer_size = v;
          return *this;
        }

        /*!
         * Sets \ref m_uniform_buffer_size
         * \param v value to use
         */
        Config&
        uniform_buffer_size(unsigned int v)
        {
          m_uniform_buffer_size = v;
          return *this;
        }

        /*!
         * Sets \ref m_use_texture_for_uniform_buffer
         * \param v value to use
         */
        Config&
        use_texture_for_uniform_buffer(bool v)
        {
          m_use_texture_for_uniform_buffer = v;
          return *this;
        }

        /*!
         * Sets \ref m_use_hw_clip_window
         * \param v value to use
         */
        Config&
        use_hw_clip_window(bool v)
        {
          m_use_hw_clip_window = v;
          return *this;
        }

        /*!
         * Sets \ref m_data_streaming
         * \param v value to use
         */
        Config&
        data_streaming(enum data_streaming_t v)
        {
          m_data_streaming = v;
          return *this;
        }

        /*!
         * Sets \ref m_buffer_reuse_period
         * \param v value to use
         */
        Config&
        buffer_reuse_period(unsigned int v)
        {
          m_buffer_reuse_period = v;
          return *this;
        }

        /*!
         * Sets \ref m_log2_gpu_stream_surface_width
         * \param v value to use
         */
        Config&
        log2_gpu_stream_surface_width(unsigned int v)
        {
          m_log2_gpu_stream_surface_width = v;
          return *this;
        }

        /*!
         * Sets \ref m_initial_static_data_size
         * \param v value to use
         */
        Config&
        initial_static_data_size(unsigned int v)
        {
          m_initial_static_data_size = v;
          return *this;
        }

        /*!
         * Sets \ref m_static_data_layout
         * \param v value to use
         */
        Config&
        static_data_layout(enum layout_t v)
        {
          m_static_data_layout = v;
          return *this;
        }

        /*!
         * Sets \ref m_static_data_log2_width
         * \param v value to use
         */
        Config&
        static_data_log2_width(unsigned int v)
        {
          m_static_data_log2_width = v;
          return *this;
        }

        /*!
         * Sets \ref m_static_data_log2_width
         * \param v value to use
         */
        Config&
        static_data_log2_height(unsigned int v)
        {
          m_static_data_log2_height = v;
          return *this;
        }

        /*!
         * Sets \ref m_vertex_buffer_layout
         * \param v value to use
         */
        Config&
        vertex_buffer_layout(enum layout_t v)
        {
          m_vertex_buffer_layout = v;
          return *this;
        }

        /*!
         * Sets \ref m_vertex_buffer_log2_width
         * \param v value to use
         */
        Config&
        vertex_buffer_log2_width(unsigned int v)
        {
          m_vertex_buffer_log2_width = v;
          return *this;
        }

        /*!
         * Sets \ref m_vertex_buffer_log2_height
         * \param v value to use
         */
        Config&
        vertex_buffer_log2_height(unsigned int v)
        {
          m_vertex_buffer_log2_height = v;
          return *this;
        }

        /*!
         * Sets \ref m_use_glsl_unpack_fp16
         * \param v value to use
         */
        Config&
        use_glsl_unpack_fp16(bool v)
        {
          m_use_glsl_unpack_fp16 = v;
          return *this;
        }

        /*!
         * Sets \ref m_use_attributes
         * \param v value to use
         */
        Config&
        use_attributes(bool v)
        {
          m_use_attributes = v;
          return *this;
        }

        /*!
         * Sets \ref m_use_indices
         * \param v value to use
         */
        Config&
        use_indices(bool v)
        {
          m_use_indices = v;
          return *this;
        }

        /*!
         * Sets \ref m_shadow_map_atlas_width
         * \param v value to use
         */
        Config&
        shadow_map_atlas_width(unsigned int v)
        {
          m_shadow_map_atlas_width = v;
          return *this;
        }

        /*!
         * Sets \ref m_shadow_map_atlas_initial_height
         * \param v value to use
         */
        Config&
        shadow_map_atlas_initial_height(unsigned int v)
        {
          m_shadow_map_atlas_initial_height = v;
          return *this;
        }

        /*!
         * Sets \ref m_inflate_degenerate_glue_joins
         * \param v value to use
         */
        Config&
        inflate_degenerate_glue_joins(bool v)
        {
          m_inflate_degenerate_glue_joins = v;
          return *this;
        }

        /*!
         * Sets \ref m_uber_shader_max_if_depth
         * \param v value to use
         */
        Config&
        uber_shader_max_if_depth(unsigned int v)
        {
          m_uber_shader_max_if_depth = v;
          return *this;
        }

        /*!
         * Sets \ref m_uber_shader_max_if_length
         * \param v value to use
         */
        Config&
        uber_shader_max_if_length(unsigned int v)
        {
          m_uber_shader_max_if_length = v;
          return *this;
        }

        /*!
         * Sets \ref m_uber_shader_fallback
         * \param v value to use
         */
        Config&
        uber_shader_fallback(enum uber_shader_fallback_t v)
        {
          m_uber_shader_fallback = v;
          return *this;
        }

        /*!
         * Sets \ref m_force_shader_log_generation_before_use
         * \param v value to use
         */
        Config&
        force_shader_log_generation_before_use(bool v)
        {
          m_force_shader_log_generation_before_use = v;
          return *this;
        }

        /*!
         * Sets \ref m_max_number_color_backing_layers
         * \param v value to use
         */
        Config&
        max_number_color_backing_layers(unsigned int v)
        {
          m_max_number_color_backing_layers = v;
          return *this;
        }

        /*!
         * Sets \ref m_max_number_index_backing_layers
         * \param v value to use
         */
        Config&
        max_number_index_backing_layers(unsigned int v)
        {
          m_max_number_index_backing_layers = v;
          return *this;
        }

        /*!
         * The initial number of layers for the
         * astral::ColorStopSequenceAtlasBacking
         * of the astral::gl::RenderEngineGL3.
         */
        unsigned int m_initial_num_colorstop_atlas_layers;

        /*!
         * The log2 of the width and height for the layers
         * of the \ref ColorStopSequenceAtlas
         */
        uint32_t m_log2_dims_colorstop_atlas;

        /*!
         * For each \ref data_t the maximum number of such
         * items supported per GL draw call.
         */
        vecN<unsigned int, number_data_types> m_max_per_draw_call;

        /*!
         * Gives the initial size of the astral::VertexDataBacking.
         */
        unsigned int m_vertex_buffer_size;

        /*!
         * Size in bytes for each buffer that backs all of the
         * the per-item data (but not vertex data) packed.
         */
        unsigned int m_uniform_buffer_size;

        /*!
         * If true, instead of using multiple UBO's backed by a single
         * uniform buffer, use a single texture. This is for compatibility
         * with platform where dynamic UBO access is problematic. This
         * is required to be true for Safari as of March 31, 2022.
         */
        bool m_use_texture_for_uniform_buffer;

        /*!
         * If true, use HW clip-planes (ala gl_ClipDistance), otherwise
         * use discard to enforce \ref ClipWindow
         */
        bool m_use_hw_clip_window;

        /*!
         * Specifies how data is streamed to GL
         */
        enum data_streaming_t m_data_streaming;

        /*!
         * Specifies the number of times Renderer::end() is to be called
         * before reusing buffers. A value of one or smaller indicates
         * to reset every time Renderer::end() is called.
         */
        unsigned int m_buffer_reuse_period;

        /*!
         * Specifies the log2 width of an offscreen surface used for
         * GPU vertex streaming.
         */
        unsigned int m_log2_gpu_stream_surface_width;

        /*!
         * The initial size of the \ref StaticDataBacking used
         * by the RenderEngineGL3
         */
        unsigned int m_initial_static_data_size;

        /*!
         * Specifies if the \ref StaticDataBacking will be
         * a 2D-texture-array or a linear buffer
         */
        enum layout_t m_static_data_layout;

        /*!
         * Only has effect if \ref m_static_data_layout is \ref
         * texture_2d_array. Specifies the log2 of the width
         * of the texture that backs the \ref StaticData created
         * by the created \ref RenderEngineGL3
         */
        unsigned int m_static_data_log2_width;

        /*!
         * Only has effect if \ref m_static_data_layout is \ref
         * texture_2d_array. Specifies the log2 of the height
         * of the texture that backs the \ref StaticData created
         * by the created \ref RenderEngineGL3
         */
        unsigned int m_static_data_log2_height;

        /*!
         * Determines if the backing is realized as a single linear
         * buffer or as texture array. The latter is required for
         * GLES3/WebGL2.
         */
        enum layout_t m_vertex_buffer_layout;

        /*!
         * Only has effect if \ref m_vertex_buffer_layout is
         * \ref texture_2d_array. Specifies the log2 of the
         * width of the texture that backs the vertex data
         */
        unsigned int m_vertex_buffer_log2_width;

        /*!
         * Only has effect if \ref m_vertex_buffer_layout is
         * \ref texture_2d_array. Specifies the log2 of the
         * height of the texture that backs the vertex data
         */
        unsigned int m_vertex_buffer_log2_height;

        /*!
         * If true, rely on GLSL's unpackHalf2x16 built-in to
         * unpack fp16 data pairs.
         */
        bool m_use_glsl_unpack_fp16;

        /*!
         * The width and height of the texture to hold the
         * color tiles for tiled images
         */
        unsigned int m_image_color_atlas_width_height;

        /*!
         * The initial number of layers of the texture to hold
         * the color tiles for tiled images
         */
        unsigned int m_image_color_atlas_number_layers;

        /*!
         * The width and height of the texture to hold the
         * index tiles for tiled images
         */
        unsigned int m_image_index_atlas_width_height;

        /*!
         * The initial number of layers of the texture to hold
         * the index tiles for tiled images
         */
        unsigned int m_image_index_atlas_number_layers;

        /*!
         * If true, use attributes in rendering.
         * NOTE: Versions of Safari from Dec 2020 would render
         *       incorrectly if attributes are not enabled. This
         *       is caused by that gl_VertexID is incorrect
         *       (various WebGL2 conformance tests on gl_VertexID
         *       fail). Versions of Safari atleast starting in
         *       April 2022 do not require this workaround.
         */
        bool m_use_attributes;

        /*!
         * If true, use indices in rendering.
         */
        bool m_use_indices;

        /*!
         * The width of the texture that backs all of the
         * astral::ShadowMap objects
         */
        unsigned int m_shadow_map_atlas_width;

        /*!
         * The initial height of the texture that backs all
         * of the astral::ShadowMap objects
         */
        unsigned int m_shadow_map_atlas_initial_height;

        /*!
         * If true, when rendering glue joins, inflate the joins
         * by a pixel if they are degenerate. The M1 GPU (as of
         * March 2022) has exhibited a rendering crack on a
         * very specific path (see demos/path_test/main.cpp)
         * when two quadratic curves meet tangentially with the
         * direction of the stroke being nearly exactly 45-degrees.
         */
        bool m_inflate_degenerate_glue_joins;

        /*!
         * When consructing the root of an uber-shader, this
         * values gives the maximum depth of if blocks to
         * realized the uber-shader.
         */
        unsigned int m_uber_shader_max_if_depth;

        /*!
         * When consructing the root of an uber-shader, this
         * values gives the maximum number of elements in
         * a single if block
         */
        unsigned int m_uber_shader_max_if_length;

        /*!
         * If a requested uber-shader is not yet ready, specifies
         * what shader to use instead. Checking for it is linked is
         * done via gl::Program::program_linked(). The main purpose
         * of this is to prevent hitching from compiling different
         * shaders as they are needed in a scene.
         */
        enum uber_shader_fallback_t m_uber_shader_fallback;

        /*!
         * Setting this to true essentially disables the created
         * astral::gl::RenderEngineGL3 from using the GL extension
         * GL_KHR_parallel_shader_compile; its main use case is
         * to generate logs for bad shaders at creation rather than
         * delayed for later.
         */
        bool m_force_shader_log_generation_before_use;

        /*!
         * The maximum number of layers the astral::ImageAtlasColorBacking
         * can ever have.
         */
        unsigned int m_max_number_color_backing_layers;

        /*!
         * The maximum number of layers the astral::ImageAtlasIndexBacking
         * can ever have.
         */
        unsigned int m_max_number_index_backing_layers;
      };

      /*!
       * Computes the minimum size necessary for a buffer to back
       * all of the UBO's as enumerated by data_t.
       */
      static
      unsigned int
      compute_min_ubo_size(const vecN<unsigned int, number_data_types> &max_per_draw_call);

      /*!
       * Create and return a \ref RenderEngineGL3 object. The
       * configuration will be adjusted to not use/specify
       * features that the GL context does not support
       * \param config \ref Config parameters
       */
      static
      reference_counted_ptr<RenderEngineGL3>
      create(const Config &config);

      /*!
       * Returns the configuration of this \ref RenderEngineGL3.
       */
      const Config&
      config(void) const;

      /*!
       * Returns the GL3 shaders from which one can build new
       * shaders.
       */
      const ShaderSetGL3&
      gl3_shaders(void) const;

      /*!
       * Return the \ref Program to use to draw items with
       * the named \ref ItemShader.
       * \param shader what item shader
       * \param material if non-null, add code for the material
       *                 shader
       * \param mode what blend mode applies
       * \param shader_clipping specifies if a clip window is present and
       *                                  how to use it
       */
      Program*
      gl_program(const ItemShader &shader,
                 const MaterialShader *material,
                 BackendBlendMode mode,
                 enum clip_window_value_type_t shader_clipping);

      /*!
       * Returns the \ref Program to use for a given uber-shader key.
       */
      Program*
      gl_program(const RenderBackend::UberShadingKey &key);

      /*!
       * When called, it forces the uber-shaders consisting of
       * all color-item shader, all material shaders working
       * with all blend modes to be linked.
       */
      void
      force_uber_shader_program_link(void);

      ///@cond
      unsigned int
      allocate_item_shader_index(detail::ShaderIndexArgument, const ItemShaderBackendGL3 *pshader, enum ItemShader::type_t);

      unsigned int
      allocate_material_shader_index(detail::ShaderIndexArgument, const MaterialShaderGL3 *pshader);
      ///@endcond

    private:
      class Implement;

      RenderEngineGL3(const Properties &P,
                      const reference_counted_ptr<ColorStopSequenceAtlasBacking> &colorstop_sequence_backing,
                      const reference_counted_ptr<VertexDataBacking> &vertex_data_backing,
                      const reference_counted_ptr<StaticDataBacking> &data_backing32,
                      const reference_counted_ptr<StaticDataBacking> &data_backing16,
                      const reference_counted_ptr<ImageAtlasIndexBacking> &image_index_backing,
                      const reference_counted_ptr<ImageAtlasColorBacking> &image_color_backing,
                      const reference_counted_ptr<ShadowMapAtlasBacking> &shadow_map_backing):
        RenderEngine(P, colorstop_sequence_backing, vertex_data_backing,
                     data_backing32, data_backing16, image_index_backing,
                     image_color_backing, shadow_map_backing)
      {}
    };

/*! @} */
  }

/*!\addtogroup gl
 * @{
 */

  /*!
   * Returns a string for a data_t value for printing
   */
  c_string
  label(enum gl::RenderEngineGL3::data_t);

  /*!
   * Returns a string for a layout_t value for printing
   */
  c_string
  label(enum gl::RenderEngineGL3::layout_t);

  /*!
   * Returns a string for a data_streaming_t value for printing
   */
  c_string
  label(enum gl::RenderEngineGL3::data_streaming_t);

  /*!
   * Returns a string for a uber_shader_fallback_t value for printing
   */
  c_string
  label(enum gl::RenderEngineGL3::uber_shader_fallback_t);
/*! @} */
}

#endif

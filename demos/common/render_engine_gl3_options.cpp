/*!
 * \file render_engine_gl3_options.cpp
 * \brief render_engine_gl3_options.cpp
 *
 * Copyright 2020 by InvisionApp.
 *
 * Contact kevinrogovin@invisionapp.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 */
#include "render_engine_gl3_options.hpp"

render_engine_gl3_options::
render_engine_gl3_options(command_line_register &reg):
  m_options_applied(false),
  m_label("RenderEngineGL3 options", reg),
  m_image_color_atlas_width_height(m_config.m_image_color_atlas_width_height,
                                         "image_color_atlas_width_height",
                                         "Width and height for the color backing of the tiled image atlas",
                                         reg),
  m_image_color_atlas_number_layers(m_config.m_image_color_atlas_number_layers,
                                          "initial_image_color_atlas_number_layers",
                                          "Initial number of layers for the color backing of the tiled image atlas",
                                          reg),
  m_image_index_atlas_width_height(m_config.m_image_index_atlas_width_height,
                                         "image_index_atlas_width_height",
                                         "Width and height for the index backing of the tiled image atlas",
                                         reg),
  m_image_index_atlas_number_layers(m_config.m_image_index_atlas_number_layers,
                                          "initial_image_index_atlas_number_layers",
                                          "Initial number of layers for the index backing of the tiled image atlas",
                                          reg),
  m_initial_num_colorstop_atlas_layers(m_config.m_initial_num_colorstop_atlas_layers,
                                       "initial_num_colorstop_atlas_layers",
                                       "Initial number of layers for the color-stop atlas",
                                       reg),
  m_log2_dims_colorstop_atlas(m_config.m_log2_dims_colorstop_atlas,
                              "log2_dims_colorstop_atlas",
                              "The log2 of the width of each layer of the colorstop atlas",
                              reg),
  m_use_texture_for_uniform_buffer(m_config.m_use_texture_for_uniform_buffer,
                                   "use_texture_for_uniform_buffer",
                                   "If true, instead of using a battery of UBO's to "
                                   "access the various per-item data, use a single "
                                   "texture; for Safari as of March 20, 2022 this is "
                                   "required",
                                   reg),
  m_max_per_draw_call_header(m_config.m_max_per_draw_call[astral::gl::RenderEngineGL3::data_header],
                             "max_per_draw_call_header",
                             "Buffer size for header buffer paremeter in number of elements (not bytes);"
                             "higher values means more elements possible per draw",
                             reg),
  m_max_per_draw_call_item_transformation(m_config.m_max_per_draw_call[astral::gl::RenderEngineGL3::data_item_transformation],
                                          "max_per_draw_call_item_transformation",
                                          "Buffer size for item_transformation buffer in number of elements (not bytes);"
                                          "higher values means more elements possible per draw",
                                          reg),
  m_max_per_draw_call_item_scale_translate(m_config.m_max_per_draw_call[astral::gl::RenderEngineGL3::data_item_scale_translate],
                                     "max_per_draw_call_item_scale_translate",
                                     "Buffer size for item_scale_translate buffer in number of elements (not bytes);"
                                     "higher values means more elements possible per draw",
                                     reg),
  m_max_per_draw_call_clip(m_config.m_max_per_draw_call[astral::gl::RenderEngineGL3::data_clip_window],
                           "max_per_draw_call_clip",
                           "Buffer size for clip buffer in number of elements (not bytes);"
                           "higher values means more elements possible per draw",
                           reg),
  m_max_per_draw_call_brush(m_config.m_max_per_draw_call[astral::gl::RenderEngineGL3::data_brush],
                            "max_per_draw_call_brush",
                            "Buffer size for brush buffer in number of elements (not bytes);"
                            "higher values means more elements possible per draw",
                            reg),
  m_max_per_draw_call_image(m_config.m_max_per_draw_call[astral::gl::RenderEngineGL3::data_image],
                                  "max_per_draw_call_image",
                                  "Buffer size for image buffer in number of elements (not bytes);"
                                  "higher values means more elements possible per draw",
                                  reg),
  m_max_per_draw_call_gradient(m_config.m_max_per_draw_call[astral::gl::RenderEngineGL3::data_gradient],
                               "max_per_draw_call_gradient",
                               "Buffer size for gradient buffer in number of elements (not bytes);"
                               "higher values means more elements possible per draw",
                               reg),
  m_max_per_draw_call_image_transformation(m_config.m_max_per_draw_call[astral::gl::RenderEngineGL3::data_gradient_transformation],
                              "max_per_draw_call_image_transformation", "Buffer size for image_transformation buffer in number of elements (not bytes);"
                              "higher values means more elements possible per draw",
                              reg),
  m_max_per_draw_call_item_data(m_config.m_max_per_draw_call[astral::gl::RenderEngineGL3::data_item_data],
                                "max_per_draw_call_item_data",
                                "Buffer size for  buffer in number of elements (not bytes);"
                                "higher values means more elements possible per draw",
                                reg),
  m_max_per_draw_call(0, "max_per_draw_call",
                      "If set, set all the buffer sizes value to this value and then apply "
                      "the buffer size values set by command line",
                      reg),
  m_vertex_buffer_size(m_config.m_vertex_buffer_size,
                       "vertex_buffer_size",
                       "Specifies the initial size of the buffer that backs vertices",
                       reg),
  m_use_hw_clip_window(m_config.m_use_hw_clip_window,
                       "use_hw_clip_window",
                       "Specifies if to use HW clip planes",
                       reg),
  m_initial_static_data_size(m_config.m_initial_static_data_size,
                             "initial_static_data_size",
                             "Initial size of the StaticDataBacking (in units of vec4s)",
                             reg),
  m_static_data_log2_width(m_config.m_static_data_log2_width,
                           "static_data_log2_width",
                           "Only has effect if static_data_layout is texture_2d_array; "
                           "specifies the log2 of the width of the texture that "
                           "backs the StaticDataBacking",
                           reg),
  m_static_data_log2_height(m_config.m_static_data_log2_height,
                           "static_data_log2_height",
                           "Only has effect if static_data_layout is texture_2d_array; "
                           "specifies the log2 of the height of the texture that "
                           "backs the StaticDataBacking",
                           reg),
  m_vertex_buffer_log2_width(m_config.m_vertex_buffer_log2_width,
                             "vertex_buffer_log2_width",
                             "Only has effect if vertex_buffer_layout is texture_2d_array; "
                             "specifies the log2 of the width of the texture that "
                             "backs the VertexDataBacking",
                             reg),
  m_vertex_buffer_log2_height(m_config.m_vertex_buffer_log2_height,
                              "vertex_buffer_log2_height",
                              "Only has effect if vertex_buffer_layout is texture_2d_array; "
                              "specifies the log2 of the width of the texture that "
                              "backs the VertexDataBacking",
                              reg),
  m_use_attributes(m_config.m_use_attributes,
                   "use_attributes",
                   "If disabled, use attributeless rendering; "
                   "NOTE: Safari's WebGL2 implementation incorrectly implement "
                   "gl_VertexID value which makes rendering fail; Safari fails "
                   "https://www.khronos.org/registry/webgl/sdk/tests/conformance2/rendering/vertex-id.html "
                   "of the Khronos comformance test suite",
                   reg),
  m_use_indices(m_config.m_use_indices,
                "use_indices",
                "Use an index buffer when rendering",
                reg),
  m_inflate_degenerate_glue_joins(m_config.m_inflate_degenerate_glue_joins,
                                  "inflate_degenerate_glue_joins",
                                  "If enabled, when rendering glue joins, inflate the joins "
                                  "by a pixel if they are degenerate. The M1 GPU (as of "
                                  "March 2022) has exhibited a rendering crack on a "
                                  "very specific path (see demos/path_test/main.cpp) "
                                  "when two quadratic curves meet tangentially with the "
                                  "direction of the stroke being nearly exactly 45-degrees.",
                                  reg),
  m_uber_shader_max_if_depth(m_config.m_uber_shader_max_if_depth,
                             "uber_shader_max_if_depth",
                             "Maximum depth of if-blocks in root of uber shaders",
                             reg),
  m_uber_shader_max_if_length(m_config.m_uber_shader_max_if_length,
                              "uber_shader_max_if_length",
                              "Maximum length of and if-block in root of uber shaders",
                              reg),
  m_max_number_color_backing_layers(m_config.m_max_number_color_backing_layers,
                                    "max_number_color_backing_layers",
                                    "Maximum number of color layers allowed in image atlas",
                                    reg),
  m_max_number_index_backing_layers(m_config.m_max_number_index_backing_layers,
                                    "max_number_index_backing_layers",
                                    "Maximum number of index layers allowed in image atlas",
                                    reg),
  m_buffer_reuse_period(m_config.m_buffer_reuse_period,
                        "buffer_reuse_period",
                        "number of frames to draw before reusing buffer pools",
                        reg),
  m_emit_file_on_link_error(true, "emit_file_on_link_error",
                            "if true, emit a file when a program fails to link",
                            reg),
  m_emit_file_on_compile_error(true, "emit_file_on_compile_error",
                            "if true, emit a file when a shader fails to compile",
                            reg),
  m_uber_shader_fallback(m_config.m_uber_shader_fallback,
                         enumerated_string_type<enum astral::gl::RenderEngineGL3::uber_shader_fallback_t>()
                         .add_entry("uber_shader_fallback_separate", astral::gl::RenderEngineGL3::uber_shader_fallback_separate, "")
                         .add_entry("uber_shader_fallback_uber_all", astral::gl::RenderEngineGL3::uber_shader_fallback_uber_all, "")
                         .add_entry("uber_shader_fallback_none", astral::gl::RenderEngineGL3::uber_shader_fallback_none, ""),
                         "uber_shader_fallback",
                         "Specifies how the engine will fall back to a different shader if a shader is not available",
                         reg),
  m_data_streaming(m_config.m_data_streaming,
                   enumerated_string_type<enum astral::gl::RenderEngineGL3::data_streaming_t>()
                   .add_entry("data_streaming_bo_orphaning", astral::gl::RenderEngineGL3::data_streaming_bo_orphaning, "")
                   .add_entry("data_streaming_bo_mapping", astral::gl::RenderEngineGL3::data_streaming_bo_mapping, "")
                   .add_entry("data_streaming_bo_subdata", astral::gl::RenderEngineGL3::data_streaming_bo_subdata, ""),
                   "data_streaming",
                   "Specifies how the engine will stream data via buffer objects to GL",
                   reg),
  m_static_data_layout(m_config.m_static_data_layout,
                       enumerated_string_type<enum astral::gl::RenderEngineGL3::layout_t>()
                       .add_entry("texture_2d_array", astral::gl::RenderEngineGL3::texture_2d_array, "")
                       .add_entry("linear_array", astral::gl::RenderEngineGL3::linear_array, ""),
                       "static_data_layout",
                       "Specifies how the StaticDataBacking is backed",
                       reg),
  m_vertex_buffer_layout(m_config.m_vertex_buffer_layout,
                         enumerated_string_type<enum astral::gl::RenderEngineGL3::layout_t>()
                         .add_entry("texture_2d_array", astral::gl::RenderEngineGL3::texture_2d_array, "")
                         .add_entry("linear_array", astral::gl::RenderEngineGL3::linear_array, ""),
                         "vertex_buffer_layout",
                         "Specifies how the VertexDataBacking is backed",
                         reg),
  m_clip_window_strategy(astral::clip_window_strategy_shader,
                         enumerated_string_type<enum astral::clip_window_strategy_t>()
                         .add_entry("clip_window_strategy_shader", astral::clip_window_strategy_shader, "")
                         .add_entry("clip_window_strategy_depth_occlude", astral::clip_window_strategy_depth_occlude, "")
                         .add_entry("clip_window_strategy_depth_occlude_hinted", astral::clip_window_strategy_depth_occlude_hinted, ""),
                         "clip_window_strategy",
                         "If set, override how clip windows are enforced for virtual buffers",
                         reg),
  m_uber_shader_method(astral::uber_shader_active,
                       enumerated_string_type<enum astral::uber_shader_method_t>(&astral::label, astral::number_uber_shader_method),
                       "uber_shader_method",
                       "what uber-shader, if any, to use when drawing",
                       reg),
  m_item_path_max_recursion(m_item_path_params.m_max_recursion,
                            "glyph_max_recursion",
                            "When generating scalable glyph data, "
                            "specifies the maximum number of levels "
                            "of recursion to employ when breaking a "
                            "glyph's path into bands",
                            reg),
  m_item_path_cost(m_item_path_params.m_cost,
                   "glyph_cost",
                   "When generating scalable glyph data, "
                   "specifies the the average pixel cost at "
                   "which to stop dividing the glyph's path "
                   "into bands",
                   reg)
{
}

void
render_engine_gl3_options::
apply_options(void)
{
  if (!m_options_applied)
    {
      m_options_applied = true;
      if (m_max_per_draw_call.set_by_command_line())
        {
          for (unsigned int i = 0; i < astral::gl::RenderEngineGL3::number_data_types; ++i)
            {
              m_config.m_max_per_draw_call[i] = m_max_per_draw_call.value();
            }
        }

      if (m_max_per_draw_call_header.set_by_command_line())
        {
          m_config.max_per_draw_call(astral::gl::RenderEngineGL3::data_header, m_max_per_draw_call_header.value());
        }

      if (m_max_per_draw_call_item_transformation.set_by_command_line())
        {
          m_config.max_per_draw_call(astral::gl::RenderEngineGL3::data_item_transformation, m_max_per_draw_call_item_transformation.value());
        }

      if (m_max_per_draw_call_item_scale_translate.set_by_command_line())
        {
          m_config.max_per_draw_call(astral::gl::RenderEngineGL3::data_item_scale_translate, m_max_per_draw_call_item_scale_translate.value());
        }

      if (m_max_per_draw_call_clip.set_by_command_line())
        {
          m_config.max_per_draw_call(astral::gl::RenderEngineGL3::data_clip_window, m_max_per_draw_call_clip.value());
        }

      if (m_max_per_draw_call_brush.set_by_command_line())
        {
          m_config.max_per_draw_call(astral::gl::RenderEngineGL3::data_brush, m_max_per_draw_call_brush.value());
        }

      if (m_max_per_draw_call_image.set_by_command_line())
        {
          m_config.max_per_draw_call(astral::gl::RenderEngineGL3::data_image, m_max_per_draw_call_image.value());
        }

      if (m_max_per_draw_call_gradient.set_by_command_line())
        {
          m_config.max_per_draw_call(astral::gl::RenderEngineGL3::data_gradient, m_max_per_draw_call_gradient.value());
        }

      if (m_max_per_draw_call_image_transformation.set_by_command_line())
        {
          m_config.max_per_draw_call(astral::gl::RenderEngineGL3::data_gradient_transformation, m_max_per_draw_call_image_transformation.value());
        }

      if (m_max_per_draw_call_item_data.set_by_command_line())
        {
          m_config.max_per_draw_call(astral::gl::RenderEngineGL3::data_item_data, m_max_per_draw_call_item_data.value());
        }

      m_config
        .image_color_atlas_width_height(m_image_color_atlas_width_height.value())
        .image_color_atlas_number_layers(m_image_color_atlas_number_layers.value())
        .image_index_atlas_width_height(m_image_index_atlas_width_height.value())
        .image_index_atlas_number_layers(m_image_index_atlas_number_layers.value())
        .initial_num_colorstop_atlas_layers(m_initial_num_colorstop_atlas_layers.value())
        .log2_dims_colorstop_atlas(m_log2_dims_colorstop_atlas.value())
        .use_texture_for_uniform_buffer(m_use_texture_for_uniform_buffer.value())
        .vertex_buffer_size(m_vertex_buffer_size.value())
        .use_hw_clip_window(m_use_hw_clip_window.value())
        .uber_shader_fallback(m_uber_shader_fallback.value())
        .data_streaming(m_data_streaming.value())
        .initial_static_data_size(m_initial_static_data_size.value())
        .static_data_log2_width(m_static_data_log2_width.value())
        .static_data_log2_height(m_static_data_log2_height.value())
        .static_data_layout(m_static_data_layout.value())
        .vertex_buffer_log2_width(m_vertex_buffer_log2_width.value())
        .vertex_buffer_log2_height(m_vertex_buffer_log2_height.value())
        .vertex_buffer_layout(m_vertex_buffer_layout.value())
        .use_attributes(m_use_attributes.value())
        .use_indices(m_use_indices.value())
        .inflate_degenerate_glue_joins(m_inflate_degenerate_glue_joins.value())
        .uber_shader_max_if_depth(m_uber_shader_max_if_depth.value())
        .uber_shader_max_if_length(m_uber_shader_max_if_length.value())
        .max_number_color_backing_layers(m_max_number_color_backing_layers.value())
        .max_number_index_backing_layers(m_max_number_index_backing_layers.value())
        .buffer_reuse_period(m_buffer_reuse_period.value())
        ;

      m_item_path_params.m_max_recursion = m_item_path_max_recursion.value();
      m_item_path_params.m_cost = m_item_path_cost.value();

      astral::gl::Program::emit_file_on_link_error(m_emit_file_on_link_error.value());
      astral::gl::Shader::emit_file_on_compile_error(m_emit_file_on_compile_error.value());
    }
}

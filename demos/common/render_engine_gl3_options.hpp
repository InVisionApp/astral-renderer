/*!
 * \file render_engine_gl3_options.hpp
 * \brief render_engine_gl3_options.hpp
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
#ifndef ASTRAL_DEMO_RENDER_ENGINE_GL3_OPTIONS_HPP
#define ASTRAL_DEMO_RENDER_ENGINE_GL3_OPTIONS_HPP

#include <astral/renderer/gl3/render_engine_gl3.hpp>
#include <astral/renderer/item_path.hpp>
#include "generic_command_line.hpp"

class render_engine_gl3_options
{
public:
  render_engine_gl3_options(command_line_register &reg);

  const astral::gl::RenderEngineGL3::Config&
  config(void)
  {
    apply_options();
    return m_config;
  }

  const astral::ItemPath::GenerationParams&
  item_path_params(void)
  {
    apply_options();
    return m_item_path_params;
  }

  enumerated_command_line_argument_value<enum astral::clip_window_strategy_t>&
  clip_window_strategy(void)
  {
    return m_clip_window_strategy;
  }

  enumerated_command_line_argument_value<enum astral::uber_shader_method_t>&
  uber_shader_method(void)
  {
    return m_uber_shader_method;
  }

private:
  void
  apply_options(void);

  bool m_options_applied;
  astral::gl::RenderEngineGL3::Config m_config;
  astral::ItemPath::GenerationParams m_item_path_params;

  command_separator m_label;
  command_line_argument_value<unsigned int> m_image_color_atlas_width_height;
  command_line_argument_value<unsigned int> m_image_color_atlas_number_layers;
  command_line_argument_value<unsigned int> m_image_index_atlas_width_height;
  command_line_argument_value<unsigned int> m_image_index_atlas_number_layers;
  command_line_argument_value<unsigned int> m_initial_num_colorstop_atlas_layers;
  command_line_argument_value<uint32_t> m_log2_dims_colorstop_atlas;
  command_line_argument_value<bool> m_use_texture_for_uniform_buffer;
  command_line_argument_value<uint32_t> m_max_per_draw_call_header;
  command_line_argument_value<uint32_t> m_max_per_draw_call_item_transformation;
  command_line_argument_value<uint32_t> m_max_per_draw_call_item_scale_translate;
  command_line_argument_value<uint32_t> m_max_per_draw_call_clip;
  command_line_argument_value<uint32_t> m_max_per_draw_call_brush;
  command_line_argument_value<uint32_t> m_max_per_draw_call_image;
  command_line_argument_value<uint32_t> m_max_per_draw_call_gradient;
  command_line_argument_value<uint32_t> m_max_per_draw_call_image_transformation;
  command_line_argument_value<uint32_t> m_max_per_draw_call_item_data;
  command_line_argument_value<uint32_t> m_max_per_draw_call;
  command_line_argument_value<unsigned int> m_vertex_buffer_size;
  command_line_argument_value<bool> m_use_hw_clip_window;
  command_line_argument_value<unsigned int> m_initial_static_data_size;
  command_line_argument_value<unsigned int> m_static_data_log2_width;
  command_line_argument_value<unsigned int> m_static_data_log2_height;
  command_line_argument_value<unsigned int> m_vertex_buffer_log2_width;
  command_line_argument_value<unsigned int> m_vertex_buffer_log2_height;
  command_line_argument_value<bool> m_use_attributes;
  command_line_argument_value<bool> m_use_indices;
  command_line_argument_value<bool> m_inflate_degenerate_glue_joins;
  command_line_argument_value<unsigned int> m_uber_shader_max_if_depth;
  command_line_argument_value<unsigned int> m_uber_shader_max_if_length;
  command_line_argument_value<unsigned int> m_max_number_color_backing_layers;
  command_line_argument_value<unsigned int> m_max_number_index_backing_layers;
  command_line_argument_value<unsigned int> m_buffer_reuse_period;
  command_line_argument_value<bool> m_emit_file_on_link_error;
  command_line_argument_value<bool> m_emit_file_on_compile_error;

  enumerated_command_line_argument_value<enum astral::gl::RenderEngineGL3::uber_shader_fallback_t> m_uber_shader_fallback;
  enumerated_command_line_argument_value<enum astral::gl::RenderEngineGL3::data_streaming_t> m_data_streaming;
  enumerated_command_line_argument_value<enum astral::gl::RenderEngineGL3::layout_t> m_static_data_layout;
  enumerated_command_line_argument_value<enum astral::gl::RenderEngineGL3::layout_t> m_vertex_buffer_layout;
  enumerated_command_line_argument_value<enum astral::clip_window_strategy_t> m_clip_window_strategy;
  enumerated_command_line_argument_value<enum astral::uber_shader_method_t> m_uber_shader_method;

  command_line_argument_value<unsigned int> m_item_path_max_recursion;
  command_line_argument_value<float> m_item_path_cost;
};

#endif

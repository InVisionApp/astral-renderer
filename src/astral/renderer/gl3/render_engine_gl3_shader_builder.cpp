/*!
 * \file render_engine_gl3_shader_builder.cpp
 * \brief file render_engine_gl3_shader_builder.cpp
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

#include <sstream>
#include <astral/util/ostream_utility.hpp>
#include <astral/util/gl/gl_get.hpp>
#include <astral/renderer/shader/fill_stc_shader.hpp>
#include <astral/renderer/gl3/shader_set_gl3.hpp>
#include <astral/renderer/stroke_parameters.hpp>
#include "render_engine_gl3_shader_builder.hpp"
#include "render_engine_gl3_packing.hpp"
#include "render_engine_gl3_image.hpp"
#include "../renderer_shared_util.hpp"
#include "stroke_shader_gl3_enums.hpp"
#include "shader_implement_gl3.hpp"

namespace
{
  template<typename ...Args>
  inline
  astral::reference_counted_ptr<const astral::ColorItemShader>
  create_color_item_shader(const astral::ColorItemShader::Properties &props,
                           astral::gl::RenderEngineGL3 &engine, Args&&... args)
  {
    astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> p;

    p = astral::gl::ItemShaderBackendGL3::create(engine, astral::ItemShader::color_item_shader, std::forward<Args>(args)...);
    return p->create_color_item_shader(props);
  }

  template<typename ...Args>
  inline
  astral::reference_counted_ptr<const astral::MaskItemShader>
  create_mask_shader(astral::gl::RenderEngineGL3 &engine, Args&&... args)
  {
    astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> p;

    p = astral::gl::ItemShaderBackendGL3::create(engine, astral::ItemShader::mask_item_shader, std::forward<Args>(args)...);
    return p->create_mask_shader();
  }

  template<typename ...Args>
  inline
  astral::reference_counted_ptr<const astral::ShadowMapItemShader>
  create_shadow_map_shader(astral::gl::RenderEngineGL3 &engine, Args&&... args)
  {
    astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> p;

    p = astral::gl::ItemShaderBackendGL3::create(engine, astral::ItemShader::shadow_map_item_shader, std::forward<Args>(args)...);
    return p->create_shadow_map_shader();
  }
}

class astral::gl::RenderEngineGL3::Implement::ShaderBuilder::StrokeShaderBuilder
{
public:
  /*
   * Stroke shaders have three types of BASE stroke shaders:
   *   (sh1) Vanilla stroking, no dashing and no computation of distance along the stroke [DONE]
   *   (sh2) Emit distance values, no dashing but emit distance along edge and contour;
   *         made with same shader sourced as (sh1) but with additional macros and varyings
   *   (sh3) Dash stroking against distance along contour or edge.
   *
   * TODO: Shader (sh3) will have a mode (specified by sub-shader ID, or shader data tag):
   *   A. Dashed using contour length [ONLY MODE currently]
   *   B. Dashed using edge length with no adjustment
   *   C. Dashed using edge length with Stretch Adjustment: Joins always drawn with a
   *      length R around them and where the entire dash pattern is stretched to be a
   *      multiple of the length of each edge minus 2 * R.
   *      i. OR only draws stretched
   *      ii. OR only skips stretched
   *   D. Dashed using edge length with Compressed Adjustment: Joins always drawn with a
   *      length R around them and where the entire dash pattern is compressed to be a
   *      multiple of the length of each edge minus 2 * R.
   *      i. OR only draws stretched
   *      ii. OR only skips stretched
   *
   * Both C. and D. will induce two flat varyings, one to specify the R and the other
   * to specify the stretch/compress factor to apply.
   *
   */
  static
  void
  build_shaders(enum ShaderSetGL3::stroke_shader_type_t type,
                const ShaderLibraryGL3 &libs, RenderEngineGL3 &engine,
                StrokeShaderGL3 *out_shaders)
  {
    StrokeShaderBuilder ctor_builds_it(type, libs, engine, out_shaders);
  }

private:
  StrokeShaderBuilder(enum ShaderSetGL3::stroke_shader_type_t type,
                      const ShaderLibraryGL3 &libs,
                      RenderEngineGL3 &engine,
                      StrokeShaderGL3 *out_shaders);

  void
  build_capper_shaders(StrokeShaderGL3 *out_shaders);

  enum ShaderSetGL3::stroke_shader_type_t m_type;
  ShaderLibraryGL3 m_libs;
  RenderEngineGL3 &m_engine;
  unsigned int m_number_sub_shaders; //not for Cap shaders or Capper shaders
  ShaderSource::MacroSet m_macros;
  ShaderSymbolList m_linear_curve_dash_symbols;
  ShaderSymbolList m_linear_distance_symbols, m_curve_distance_symbols;
  ShaderSymbolList m_join_distance_symbols, m_cap_distance_symbols;
  ShaderSymbolList m_additional_symbols;
};

class astral::gl::RenderEngineGL3::Implement::ShaderBuilder::SourceBuilder
{
public:
  explicit
  SourceBuilder(const ShaderBuilder &shader_builder):
    m_shader_builder(shader_builder)
  {}

  /*!
   * Create the shader source code for building a
   * \ref Program that handles processing via a single
   * \ref ItemShaderGL3 (which may or may not have
   * sub-shaders). Returns the number of sampler2D
   * needed by the shader.
   */
  void
  create_item_shader_src(ShaderSource *out_vert,
                         ShaderSource *out_frag,
                         const ItemShaderBackendGL3::Implement &gl_shader,
                         const MaterialShaderGL3::Implement *gl_material,
                         enum ItemShader::type_t shader_type,
                         const std::string &blend_shader_src,
                         bool requires_framebuffer_pixels,
                         enum clip_window_value_type_t shader_clipping);

  void
  create_uber_shader_src(ShaderSource *out_vert,
                         ShaderSource *out_frag,
                         const UberShaderKey &key,
                         unsigned int *out_number_varyings);

private:
  enum stream_define_t:uint32_t
    {
      add_defines,
      add_undefs,
    };

  static
  c_string
  mode_label(enum stream_define_t v)
  {
    return (v == add_defines) ?
      "ADD-DEFINES" :
      "REMOVE-DEFINES";
  }

  static
  unsigned int
  shader_id(const ItemShaderBackendGL3 &shader)
  {
    return shader.begin_shaderID();
  }

  static
  unsigned int
  num_sub_shaders(const ItemShaderBackendGL3 &shader)
  {
    return shader.num_sub_shaders();
  }

  static
  std::string
  shader_prefix(const ItemShaderBackendGL3 &shader)
  {
    std::ostringstream str;
    str << "astral_item_shader" << shader.begin_shaderID();
    return str.str();
  }

  static
  unsigned int
  shader_id(const MaterialShaderGL3 &shader)
  {
    return shader.ID();
  }

  static
  unsigned int
  num_sub_shaders(const MaterialShaderGL3 &shader)
  {
    return shader.num_sub_shaders();
  }

  static
  std::string
  shader_prefix(const MaterialShaderGL3 &shader)
  {
    std::ostringstream str;
    str << "astral_material_shader" << shader.ID();
    return str.str();
  }

  template<typename T>
  static
  bool
  compare_shader_id(const T *lhs, const T *rhs)
  {
    return shader_id(*lhs) < shader_id(*rhs);
  }

  /* Stream the uber-root function, i.e. the function that examines
   * the shader ID and calls the correct shader.
   * \param tag string to identify shader type (item, material or shadow)
   * \param stage what shader stage
   * \param function_signature singatures of functions
   * \param uber_root_function_name name to give the uber-root function
   * \param shader_list list of shaders; note that it is copy by value;
   *                    this is so that it can sort the shaders by shader_id()
   * \param dst ShaderSource to which to rende
   */
  template<typename T>
  void
  stream_uber_root_function(const std::string &tag,
                            enum detail::shader_stage_t stage,
                            const PreAndActualFunctionSignature &function_signature,
                            const std::string &uber_root_function_name,
                            std::vector<const T*> shader_list,
                            ShaderSource &dst)
  {
    std::sort(shader_list.begin(), shader_list.end(), compare_shader_id<T>);
    stream_uber_root_function_implement<T>(tag, stage, function_signature,
                                           uber_root_function_name,
                                           make_c_array(shader_list),
                                           dst);
  }

  /* Stream the uber-root function, i.e. the function that examines
   * the shader ID and calls the correct shader.
   * \param tag string to identify shader type (item, material or shadow)
   * \param stage what shader stage
   * \param function_signature singatures of functions
   * \param uber_root_function_name name to give the uber-root function
   * \param shader_list list of shaders, mus be sorted by shader_id(const T&)
   * \param dst ShaderSource to which to rende
   */
  template<typename T>
  void
  stream_uber_root_function_implement(const std::string &tag,
                                      enum detail::shader_stage_t stage,
                                      const PreAndActualFunctionSignature &function_signature,
                                      const std::string &uber_root_function_name,
                                      c_array<const pointer<const T>> shader_list,
                                      ShaderSource &dst);

  template<typename T>
  void
  stream_uber_root_function_subcontents(const std::string &tag, unsigned int max_depth, const std::string &tabs,
                                        enum detail::shader_stage_t stage,
                                        const PreAndActualFunctionSignature &function_signature,
                                        c_array<const pointer<const T>> shader_list,
                                        ShaderSource &dst);

  bool //return true if framebuffer fetch emulate is required
  stream_uber_blend_shader(ShaderSource &stream, const std::vector<unsigned int> &blend_shaders);

  void
  create_shader_src(ShaderSource *out_vert,
                    ShaderSource *out_frag,
                    const ShaderSource &varyings,
                    const ShaderSource &vert_item_material,
                    const ShaderSource &frag_item_material,
                    enum ItemShader::type_t shader_type,
                    bool requires_framebuffer_pixels,
                    enum clip_window_value_type_t shader_clipping);

  const ShaderBuilder &m_shader_builder;
};

class astral::gl::RenderEngineGL3::Implement::ShaderBuilder::ShaderListForUberShading
{
public:
  void
  begin(enum clip_window_value_type_t shader_clipping,
        enum uber_shader_method_t uber_method);

  void
  add_shader(const ItemShaderBackendGL3 &item_shader,
             const MaterialShaderGL3 &material_shader,
             unsigned int blend_shader);

  void
  end(UberShaderKey *out_key);

  bool
  has_program(const ItemShaderBackendGL3 &item_shader,
              const MaterialShaderGL3 &material_shader,
              unsigned int blend_shader) const;

private:
  template<typename T>
  class SimpleSet
  {
  public:
    void
    clear(void)
    {
      m_values.clear();
      m_in_values.clear();
    }

    void
    add(T value, unsigned int idx)
    {
      if (idx >= m_in_values.size())
        {
          m_in_values.resize(idx + 1u, false);
        }

      if (!m_in_values[idx])
        {
          m_in_values[idx] = true;
          m_values.push_back(value);
        }
    }

    bool
    has_element(unsigned int V) const
    {
      return V < m_in_values.size() && m_in_values[V];
    }

    const std::vector<T>&
    values(void) const
    {
      return m_values;
    }

  private:
    std::vector<T> m_values;
    std::vector<bool> m_in_values;
  };

  /* list of ItemShaderBackendGL3 objects added */
  SimpleSet<const ItemShaderBackendGL3*> m_item_shaders;

  /* list of MaterialShaderGL3 objects added */
  SimpleSet<const MaterialShaderGL3*> m_material_shaders;

  /* list of BlendBuilder::PerBlendMode::shader_id() values added */
  SimpleSet<unsigned int> m_blend_shaders;

  enum clip_window_value_type_t m_shader_clipping;
};

class astral::gl::RenderEngineGL3::Implement::ShaderBuilder::UberShadingKeyImplement:
  public astral::RenderBackend::UberShadingKey
{
public:
  explicit
  UberShadingKeyImplement(ShaderBuilder &b, const MaterialShader *default_brush):
    m_default_brush(default_brush),
    m_builder(&b)
  {}

protected:
  virtual
  void
  on_begin_accumulate(enum clip_window_value_type_t shader_clipping,
                      enum uber_shader_method_t uber_method) override final;

  virtual
  void
  on_add_shader(const ItemShader &shader,
                const MaterialShader *material_shader,
                BackendBlendMode blend_mode) override final;

  virtual
  uint32_t
  on_end_accumulate(void) override final;

  virtual
  uint32_t
  on_uber_shader_of_all(enum clip_window_value_type_t shader_clipping) final;

private:
  UberShaderKey m_key;
  ShaderListForUberShading m_shader_list;
  const MaterialShader *m_default_brush;
  reference_counted_ptr<ShaderBuilder> m_builder;
};

///////////////////////////////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::ShaderBuilder::StrokeShaderBuilder methods
void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::StrokeShaderBuilder::
build_capper_shaders(StrokeShaderGL3 *out_shaders)
{
  static const c_string capper_primitive_macro[StrokeShaderGL3::capper_primitive_count] =
    {
      [StrokeShaderGL3::line_segment_capper] = "ASTRAL_STROKE_CAPPER_LINE_SEGMENT",
      [StrokeShaderGL3::quadratic_capper] = "ASTRAL_STROKE_CAPPER_QUADRATIC",
    };

  static const c_string capper_style_macro[number_cap_t] =
    {
      [cap_flat] = "ASTRAL_STROKE_CAPPER_FLAT",
      [cap_rounded] = "ASTRAL_STROKE_CAPPER_ROUNDED",
      [cap_square] = "ASTRAL_STROKE_CAPPER_SQUARE",
    };

  for (unsigned int capper_primitive = 0; capper_primitive < StrokeShaderGL3::capper_primitive_count; ++capper_primitive)
    {
      for (unsigned int style = 0; style < number_cap_t; ++style)
        {
          if (style != cap_flat)
            {
              out_shaders->m_cappers[capper_primitive][style]
                = ItemShaderBackendGL3::create(m_engine,
                                               ItemShader::mask_item_shader,
                                               ShaderSource()
                                               .add_library(m_libs.m_stroke_lib)
                                               .add_macros(m_macros)
                                               .add_macro(capper_primitive_macro[capper_primitive])
                                               .add_macro(capper_style_macro[style])
                                               .add_source("astral_chain_stroke_capper.vert.glsl.resource_string", ShaderSource::from_resource)
                                               .remove_macro(capper_primitive_macro[capper_primitive])
                                               .remove_macro(capper_style_macro[style])
                                               .remove_macros(m_macros),
                                               ShaderSource()
                                               .add_library(m_libs.m_stroke_lib)
                                               .add_macros(m_macros)
                                               .add_macro(capper_primitive_macro[capper_primitive])
                                               .add_macro(capper_style_macro[style])
                                               .add_source("astral_chain_stroke_capper.frag.glsl.resource_string", ShaderSource::from_resource)
                                               .remove_macro(capper_primitive_macro[capper_primitive])
                                               .remove_macro(capper_style_macro[style])
                                               .remove_macros(m_macros),
                                               ShaderSymbolList()
                                               .add_varying("astral_chain_stroke_pt_x", ShaderVaryings::interpolator_smooth)
                                               .add_varying("astral_chain_stroke_pt_y", ShaderVaryings::interpolator_smooth)
                                               .add_varying("astral_chain_stroke_radius", ShaderVaryings::interpolator_flat)
                                               .add_varying("astral_chain_stroke_coverage_multiplier", ShaderVaryings::interpolator_flat)
                                               .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_distance_along_contour"),
                                               m_number_sub_shaders);
            }
        }
    }
}

astral::gl::RenderEngineGL3::Implement::ShaderBuilder::StrokeShaderBuilder::
StrokeShaderBuilder(enum ShaderSetGL3::stroke_shader_type_t type,
                    const ShaderLibraryGL3 &libs,
                    RenderEngineGL3 &engine, StrokeShaderGL3 *out_shaders):
  m_type(type),
  m_libs(libs),
  m_engine(engine)
{
  m_macros
    .add_macro("ASTRAL_STROKING_SECONDARY_AA_FUZZ_DISTANCE", StrokeParameters::hairline_pixel_radius())
    .add_macro("ASTRAL_STROKING_SECONDARY_AA_ACTIVE_THRESH", -1000.0f)
    .add_macro("ASTRAL_STROKING_SECONDARY_AA_DISABLED", -2000.0f);

  if (m_type == ShaderSetGL3::dashed_stroking)
    {
      m_number_sub_shaders = StrokeShaderGL3Enums::stroke_shader_number_with_cap_style;
      m_macros.add_macro("ASTRAL_STROKING_WITH_DASHING");
      out_shaders->m_per_cap_shading = true;

      m_linear_curve_dash_symbols
        .add_varying("astral_chain_stroke_dash_xz_draw_factor", ShaderVaryings::interpolator_flat)
        .add_varying("astral_chain_stroke_dash_yw_draw_factor", ShaderVaryings::interpolator_flat)
        .add_varying("astral_chain_stroke_dash_corner", ShaderVaryings::interpolator_flat)
        .add_varying("astral_chain_stroke_dash_end_length", ShaderVaryings::interpolator_flat)
        .add_varying("astral_chain_stroke_dash_total_length", ShaderVaryings::interpolator_flat);
    }
  else
    {
      m_number_sub_shaders = StrokeShaderGL3Enums::stroke_shader_number_without_cap_style;
      out_shaders->m_per_cap_shading = false;
    }

  if (m_type == ShaderSetGL3::only_stroking)
    {
      out_shaders->m_type = StrokeShaderGL3::stroking_only;
    }
  else
    {
      out_shaders->m_type = StrokeShaderGL3::emit_distances;

      m_macros
        .add_macro("ASTRAL_STROKING_EMIT_DISTANCE_VALUES");

      m_linear_distance_symbols
        .add_varying("astral_chain_stroke_boundary_flags", ShaderVaryings::interpolator_uint)
        .add_varying("astral_chain_stroke_distance_along_contour_start", ShaderVaryings::interpolator_flat)
        .add_varying("astral_chain_stroke_distance_along_contour_end", ShaderVaryings::interpolator_flat)
        .add_varying("astral_chain_stroke_distance_along_contour", ShaderVaryings::interpolator_smooth)
        .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_distance_from_start_contour_minus_from_start_edge")
        .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_contour_length")
        .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_edge_length")
        .add_fragment_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_distance_along_contour_gradient_x")
        .add_fragment_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_distance_along_contour_gradient_y");

      m_curve_distance_symbols
        .add_varying("astral_chain_stroke_distance_along_contour_start", ShaderVaryings::interpolator_flat)
        .add_varying("astral_chain_stroke_distance_along_contour_end", ShaderVaryings::interpolator_flat)
        .add_varying("astral_chain_stroke_distance_along_contour_multiplier", ShaderVaryings::interpolator_flat)
        .add_varying("astral_chain_stroke_distance_along_contour_pre_offset", ShaderVaryings::interpolator_flat)
        .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_distance_from_start_contour_minus_from_start_edge")
        .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_contour_length")
        .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_edge_length")
        .add_fragment_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_distance_along_contour")
        .add_fragment_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_distance_along_contour_gradient_x")
        .add_fragment_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_distance_along_contour_gradient_y");

      m_join_distance_symbols
        .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_distance_along_contour")
        .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_contour_length")
        .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_edge_into_join_length")
        .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_edge_leaving_join_length")
        .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_distance_from_start_edge");

      m_cap_distance_symbols
        .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_distance_along_contour")
        .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_contour_length")
        .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_edge_length");
    }

  /* NOTE: if an edge connects from the start to the end
   *       of an open contour, we should actually have
   *       two varyings; we will assume/require that it
   *       is never the case that an open contour consists
   *       of a single line or quadratic curve.
   */

  out_shaders->m_line =
    ItemShaderBackendGL3::create(m_engine,
                                 ItemShader::mask_item_shader,
                                 ShaderSource()
                                 .add_library(m_libs.m_stroke_lib)
                                 .add_macros(m_macros)
                                 .add_source("astral_chain_stroke_line.vert.glsl.resource_string", ShaderSource::from_resource)
                                 .remove_macros(m_macros),
                                 ShaderSource()
                                 .add_library(m_libs.m_stroke_lib)
                                 .add_macros(m_macros)
                                 .add_source("astral_chain_stroke_line_biarc_common.frag.glsl.resource_string", ShaderSource::from_resource)
                                 .add_source("astral_chain_stroke_line.frag.glsl.resource_string", ShaderSource::from_resource)
                                 .remove_macros(m_macros),
                                 ShaderSymbolList()
                                 .add_symbols(m_additional_symbols)
                                 .add_symbols(m_linear_distance_symbols)
                                 .add_symbols(m_linear_curve_dash_symbols)
                                 .add_varying("astral_chain_stroke_coverage_multiplier", ShaderVaryings::interpolator_flat)
                                 .add_varying("astral_chain_stroke_radius", ShaderVaryings::interpolator_flat)
                                 .add_varying("astral_chain_stroke_perp_distance_to_curve", ShaderVaryings::interpolator_smooth)
                                 .add_varying("astral_chain_stroke_pixel_distance_from_end", ShaderVaryings::interpolator_smooth)
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_edge_start_x")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_edge_start_y")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_edge_end_x")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_edge_end_y")
                                 .add_fragment_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_perp_distance_to_curve_gradient_x")
                                 .add_fragment_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_perp_distance_to_curve_gradient_y")
                                 .add_fragment_shader_local("astral_line_biarc_frag_shader"),
                                 StrokeShaderGL3Enums::stroke_shader_number_with_cap_style);

  out_shaders->m_biarc_curve =
    ItemShaderBackendGL3::create(m_engine,
                                 ItemShader::mask_item_shader,
                                 ShaderSource()
                                 .add_library(m_libs.m_stroke_lib)
                                 .add_macros(m_macros)
                                 .add_source("astral_chain_stroke_biarc.vert.glsl.resource_string", ShaderSource::from_resource)
                                 .remove_macros(m_macros),
                                 ShaderSource()
                                 .add_library(m_libs.m_stroke_lib)
                                 .add_macros(m_macros)
                                 .add_source("astral_chain_stroke_line_biarc_common.frag.glsl.resource_string", ShaderSource::from_resource)
                                 .add_source("astral_chain_stroke_biarc.frag.glsl.resource_string", ShaderSource::from_resource)
                                 .remove_macros(m_macros),
                                 ShaderSymbolList()
                                 .add_symbols(m_additional_symbols)
                                 .add_symbols(m_curve_distance_symbols)
                                 .add_symbols(m_linear_curve_dash_symbols)
                                 .add_varying("astral_chain_stroke_coverage_multiplier", ShaderVaryings::interpolator_flat)
                                 .add_varying("astral_chain_stroke_boundary_flags", ShaderVaryings::interpolator_uint)
                                 .add_varying("astral_chain_stroke_radius", ShaderVaryings::interpolator_flat)
                                 .add_varying("astral_chain_stroke_arc_radius", ShaderVaryings::interpolator_flat)
                                 .add_varying("astral_chain_stroke_pt_x", ShaderVaryings::interpolator_smooth)
                                 .add_varying("astral_chain_stroke_pt_y", ShaderVaryings::interpolator_smooth)
                                 .add_varying("astral_chain_stroke_pixel_distance_from_end", ShaderVaryings::interpolator_smooth)
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_start_pt_x")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_start_pt_y")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_end_pt_x")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_end_pt_y")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_control_pt_x")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_control_pt_y")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_arc_center_x")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_arc_center_y")
                                 .add_fragment_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_perp_distance_to_curve")
                                 .add_fragment_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_perp_distance_to_curve_gradient_x")
                                 .add_fragment_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_perp_distance_to_curve_gradient_y")
                                 .add_fragment_shader_local("astral_line_biarc_frag_shader"),
                                 StrokeShaderGL3Enums::stroke_shader_number_with_cap_style);

  out_shaders->m_join[join_rounded] =
    ItemShaderBackendGL3::create(m_engine,
                                 ItemShader::mask_item_shader,
                                 ShaderSource()
                                 .add_library(m_libs.m_stroke_lib)
                                 .add_macros(m_macros)
                                 .add_source("astral_chain_stroke_join.vert.glsl.resource_string", ShaderSource::from_resource)
                                 .add_source("astral_chain_stroke_rounded_join.vert.glsl.resource_string", ShaderSource::from_resource)
                                 .remove_macros(m_macros),
                                 ShaderSource()
                                 .add_library(m_libs.m_stroke_lib)
                                 .add_macros(m_macros)
                                 .add_source("astral_chain_stroke_rounded.frag.glsl.resource_string", ShaderSource::from_resource)
                                 .remove_macros(m_macros),
                                 ShaderSymbolList()
                                 .add_symbols(m_additional_symbols)
                                 .add_symbols(m_join_distance_symbols)
                                 .add_varying("astral_chain_stroke_coverage_multiplier", ShaderVaryings::interpolator_flat)
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_position_x")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_position_y")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_radius")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_vertex_to_fragment_radius_ratio")
                                 .add_varying("astral_chain_stroke_offset_vector_x", ShaderVaryings::interpolator_smooth)
                                 .add_varying("astral_chain_stroke_offset_vector_y", ShaderVaryings::interpolator_smooth),
                                 m_number_sub_shaders);

  out_shaders->m_join[join_bevel] =
    ItemShaderBackendGL3::create(m_engine,
                                 ItemShader::mask_item_shader,
                                 ShaderSource()
                                 .add_library(m_libs.m_stroke_lib)
                                 .add_macros(m_macros)
                                 .add_source("astral_chain_stroke_join.vert.glsl.resource_string", ShaderSource::from_resource)
                                 .add_source("astral_chain_stroke_bevel_join.vert.glsl.resource_string", ShaderSource::from_resource)
                                 .remove_macros(m_macros),
                                 ShaderSource()
                                 .add_library(m_libs.m_stroke_lib)
                                 .add_macros(m_macros)
                                 .add_source("astral_chain_stroke_bevel_miter_join.frag.glsl.resource_string", ShaderSource::from_resource)
                                 .remove_macros(m_macros),
                                 ShaderSymbolList()
                                 .add_symbols(m_additional_symbols)
                                 .add_symbols(m_join_distance_symbols)
                                 .add_varying("astral_chain_stroke_coverage_multiplier", ShaderVaryings::interpolator_flat)
                                 .add_varying("astral_chain_stroke_secondary_aa", ShaderVaryings::interpolator_smooth)
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_position_x")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_position_y")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_offset_vector_x")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_offset_vector_y")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_radius")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_vertex_to_fragment_radius_ratio")
                                 .add_varying("astral_chain_stroke_normalized_distance_to_path", ShaderVaryings::interpolator_smooth),
                                 m_number_sub_shaders);

  out_shaders->m_join[join_miter] =
    ItemShaderBackendGL3::create(m_engine,
                                 ItemShader::mask_item_shader,
                                 ShaderSource()
                                 .add_library(m_libs.m_stroke_lib)
                                 .add_macros(m_macros)
                                 .add_source("astral_chain_stroke_join.vert.glsl.resource_string", ShaderSource::from_resource)
                                 .add_source("astral_chain_stroke_miter_join.vert.glsl.resource_string", ShaderSource::from_resource)
                                 .remove_macros(m_macros),
                                 ShaderSource()
                                 .add_library(m_libs.m_stroke_lib)
                                 .add_macros(m_macros)
                                 .add_source("astral_chain_stroke_bevel_miter_join.frag.glsl.resource_string", ShaderSource::from_resource)
                                 .remove_macros(m_macros),
                                 ShaderSymbolList()
                                 .add_symbols(m_additional_symbols)
                                 .add_symbols(m_join_distance_symbols)
                                 .add_varying("astral_chain_stroke_coverage_multiplier", ShaderVaryings::interpolator_flat)
                                 .add_varying("astral_chain_stroke_secondary_aa", ShaderVaryings::interpolator_smooth)
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_position_x")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_position_y")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_offset_vector_x")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_offset_vector_y")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_radius")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_vertex_to_fragment_radius_ratio")
                                 .add_varying("astral_chain_stroke_normalized_distance_to_path", ShaderVaryings::interpolator_smooth),
                                 m_number_sub_shaders);

  out_shaders->m_square_cap =
    ItemShaderBackendGL3::create(m_engine,
                                 ItemShader::mask_item_shader,
                                 ShaderSource()
                                 .add_library(m_libs.m_stroke_lib)
                                 .add_macros(m_macros)
                                 .add_source("astral_chain_stroke_cap.vert.glsl.resource_string", ShaderSource::from_resource)
                                 .add_source("astral_chain_stroke_square_cap.vert.glsl.resource_string", ShaderSource::from_resource)
                                 .remove_macros(m_macros),
                                 ShaderSource()
                                 .add_library(m_libs.m_stroke_lib)
                                 .add_macros(m_macros)
                                 .add_source("astral_chain_stroke_square_cap.frag.glsl.resource_string", ShaderSource::from_resource)
                                 .remove_macros(m_macros),
                                 ShaderSymbolList()
                                 .add_symbols(m_additional_symbols)
                                 .add_symbols(m_cap_distance_symbols)
                                 .add_varying("astral_chain_stroke_coverage_multiplier", ShaderVaryings::interpolator_flat)
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_position_x")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_position_y")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_radius")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_vertex_to_fragment_radius_ratio")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_offset_vector_x")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_offset_vector_y")
                                 .add_varying("astral_chain_stroke_normalized_distance_to_path", ShaderVaryings::interpolator_smooth),
                                 StrokeShaderGL3Enums::stroke_shader_number_without_cap_style);

  out_shaders->m_rounded_cap =
    ItemShaderBackendGL3::create(m_engine,
                                 ItemShader::mask_item_shader,
                                 ShaderSource()
                                 .add_library(m_libs.m_stroke_lib)
                                 .add_macros(m_macros)
                                 .add_source("astral_chain_stroke_cap.vert.glsl.resource_string", ShaderSource::from_resource)
                                 .add_source("astral_chain_stroke_rounded_cap.vert.glsl.resource_string", ShaderSource::from_resource)
                                   .remove_macros(m_macros),
                                 ShaderSource()
                                 .add_library(m_libs.m_stroke_lib)
                                 .add_macros(m_macros)
                                 .add_source("astral_chain_stroke_rounded.frag.glsl.resource_string", ShaderSource::from_resource)
                                 .remove_macros(m_macros),
                                 ShaderSymbolList()
                                 .add_varying("astral_chain_stroke_coverage_multiplier", ShaderVaryings::interpolator_flat)
                                 .add_symbols(m_additional_symbols)
                                 .add_symbols(m_cap_distance_symbols)
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_position_x")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_position_y")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_radius")
                                 .add_vertex_shader_symbol(ShaderSymbolList::symbol_type_float, "astral_chain_stroke_vertex_to_fragment_radius_ratio")
                                 .add_varying("astral_chain_stroke_offset_vector_x", ShaderVaryings::interpolator_smooth)
                                 .add_varying("astral_chain_stroke_offset_vector_y", ShaderVaryings::interpolator_smooth)
                                 .add_vertex_shader_local("astral_chain_stroke_radius_info"),
                                 2u); //only matters if animated or not animated

  if (type == ShaderSetGL3::dashed_stroking)
    {
      build_capper_shaders(out_shaders);
    }
}

////////////////////////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::ShaderBuilder::UberShaderKey
bool
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::UberShaderKey::
requires_framebuffer_pixels(void) const
{
  for (const MaterialShaderGL3 *shader : std::get<2>(m_values))
    {
      if (shader->properties().m_uses_framebuffer_pixels)
        {
          return true;
        }
    }

  return false;
}

////////////////////////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::ShaderBuilder::SourceBuilder methods
void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::SourceBuilder::
create_shader_src(ShaderSource *out_vert,
                  ShaderSource *out_frag,
                  const ShaderSource &varyings,
                  const ShaderSource &vert_item_material,
                  const ShaderSource &frag_item_material,
                  enum ItemShader::type_t shader_type,
                  bool requires_framebuffer_pixels,
                  enum clip_window_value_type_t shader_clipping)
{
  ShaderSource &vert(*out_vert), &frag(*out_frag);
  c_string clip_window_macro, shader_type_macro;
  const PreAndActualFunctionSignature *vert_sig, *frag_sig;

  switch (shader_type)
    {
    case ItemShader::color_item_shader:
      shader_type_macro = "ASTRAL_COLOR_ITEM_SHADER";
      vert_sig = &m_shader_builder.m_rect_vert_sigs;
      frag_sig = &m_shader_builder.m_rect_frag_sigs;
      break;

    case ItemShader::mask_item_shader:
      shader_type_macro = "ASTRAL_MASK_ITEM_SHADER";
      vert_sig = &m_shader_builder.m_mask_vert_sigs;
      frag_sig = &m_shader_builder.m_mask_frag_sigs;
      break;

    case ItemShader::shadow_map_item_shader:
      shader_type_macro = "ASTRAL_SHADOW_GENERATOR_SHADER";
      vert_sig = &m_shader_builder.m_shadow_vert_sigs;
      frag_sig = &m_shader_builder.m_shadow_frag_sigs;
      ASTRALassert(shader_clipping == clip_window_not_present);
      break;

    default:
      vert_sig = &m_shader_builder.m_rect_vert_sigs;
      frag_sig = &m_shader_builder.m_rect_frag_sigs;
      ASTRALfailure("Invalid shader type for building a shader");
      break;
    }

  if (!m_shader_builder.m_config.m_use_attributes)
    {
      vert.add_macro("ASTRAL_ATTRIBUTELESS_RENDERING");
    }

  if (m_shader_builder.m_config.m_inflate_degenerate_glue_joins)
    {
      vert.add_macro("ASTRAL_INFLATE_DEGENERATE_GLUE_JOINS");
      frag.add_macro("ASTRAL_INFLATE_DEGENERATE_GLUE_JOINS");
    }

  if (requires_framebuffer_pixels)
    {
      ASTRALassert(shader_type != ItemShader::shadow_map_item_shader);
      frag.add_macro("ASTRAL_EMULATE_FRAMEBUFFER_FETCH", "", ShaderSource::push_front);
      vert.add_macro("ASTRAL_EMULATE_FRAMEBUFFER_FETCH", "", ShaderSource::push_front);
    }

  if (shader_type != ItemShader::shadow_map_item_shader)
    {
      frag.add_library(m_shader_builder.m_shader_libs.m_image_lib);
      vert.add_library(m_shader_builder.m_shader_libs.m_image_lib);
    }

  if (shader_clipping == clip_window_present_enforce)
    {
      clip_window_macro = "ASTRAL_ENFORCE_CLIP_WINDOW";
    }
  else if (shader_clipping == clip_window_present_optional)
    {
      clip_window_macro = "ASTRAL_CLIP_WINDOW_PRESENT_DO_NOT_ENFORCE";
    }
  else
    {
      clip_window_macro = "ASTRAL_DOES_NOT_HAVE_CLIP_WINDOW";
    }

  /* Browser's WebGL2 implementation are unable to handle
   * the shader if the varying declarations come after
   * including astral_main_bo.vert.glsl.resource_string;
   * The fed shader is valid GLSL, but the way they handle
   * the shader, it barfs unless we put the varying
   * declarations first. The errors are of the form "Use
   * of undeclared identifier webgl_SOMEHEXVALUE", which
   * clearly indicate a browser WebGL2 bug.
   */
  vert
    .shader_type(ASTRAL_GL_VERTEX_SHADER)
    .add_macro(shader_type_macro)
    .add_macro(clip_window_macro)
    .add_library(m_shader_builder.m_base_lib)
    .add_source(varyings)
    .add_source("astral_main_clip_window.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source("astral_main_packing_bo.glsl.resource_string", ShaderSource::from_resource);

  vert_sig->stream_runner_declaration("astral_run_vert_shader", true, &vert);

  vert
    .add_source("astral_main_bo.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source(vert_item_material);

  frag
    .shader_type(ASTRAL_GL_FRAGMENT_SHADER)
    .add_macro(shader_type_macro);

  if (shader_type == ItemShader::mask_item_shader)
    {
      frag
        .add_macro_u32("ASTRAL_MASK_ITEM_SHADER_CLIP_CUTOFF", mask_item_shader_clip_cutoff)
        .add_macro_u32("ASTRAL_MASK_ITEM_SHADER_CLIP_COMBINE", mask_item_shader_clip_combine);
    }

  frag
    .add_macro(clip_window_macro)
    .add_library(m_shader_builder.m_base_lib)
    .add_source(varyings)
    .add_source("astral_main_packing_bo.glsl.resource_string", ShaderSource::from_resource);

  frag_sig->stream_runner_declaration("astral_run_frag_shader", true, &frag);

  frag
    .add_source("astral_main_bo.frag.glsl.resource_string", ShaderSource::from_resource)
    .add_source(frag_item_material);
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::SourceBuilder::
create_item_shader_src(ShaderSource *out_vert,
                       ShaderSource *out_frag,
                       const ItemShaderBackendGL3::Implement &gl_shader,
                       const MaterialShaderGL3::Implement *gl_material,
                       enum ItemShader::type_t shader_type,
                       const std::string &blend_shader_src,
                       bool requires_framebuffer_pixels,
                       enum clip_window_value_type_t shader_clipping)
{
  ShaderSource vert_str, frag_str, varyings_str;

  if (shader_type == ItemShader::color_item_shader)
    {
      ASTRALassert(gl_material);

      detail::ShaderImplementBase::stream_varying_backings("material", gl_material->distilled_symbols().m_varying_counts, varyings_str);
      detail::ShaderImplementBase::stream_symbol_backings("material", gl_material->distilled_symbols().m_symbol_counts[detail::vertex_shader_stage], vert_str);
      detail::ShaderImplementBase::stream_symbol_backings("material", gl_material->distilled_symbols().m_symbol_counts[detail::fragment_shader_stage], frag_str);

      gl_material->stream_shader("material", detail::vertex_shader_stage, "astral_only::",
                                 gl_material->distilled_symbols().m_varying_counts,
                                 gl_material->distilled_symbols().m_symbol_counts[detail::vertex_shader_stage],
                                 vecN<c_string, 2>("astral_material_vert_shader", "astral_material_pre_vert_shader"),
                                 vert_str);

      gl_material->stream_shader("material", detail::fragment_shader_stage, "astral_only::",
                                 gl_material->distilled_symbols().m_varying_counts,
                                 gl_material->distilled_symbols().m_symbol_counts[detail::fragment_shader_stage],
                                 vecN<c_string, 2>("astral_material_frag_shader", "astral_material_pre_frag_shader"),
                                 frag_str);
    }
  else
    {
      ASTRALassert(!gl_material);
    }

  /* create the varying delcarations from gl_shader.varyings() */
  detail::ShaderImplementBase::stream_varying_backings("item", gl_shader.distilled_symbols().m_varying_counts, varyings_str);
  detail::ShaderImplementBase::stream_symbol_backings("item", gl_shader.distilled_symbols().m_symbol_counts[detail::vertex_shader_stage], vert_str);
  detail::ShaderImplementBase::stream_symbol_backings("item", gl_shader.distilled_symbols().m_symbol_counts[detail::fragment_shader_stage], frag_str);

  gl_shader.stream_shader("item", detail::vertex_shader_stage, "astral_only::",
                          gl_shader.distilled_symbols().m_varying_counts,
                          gl_shader.distilled_symbols().m_symbol_counts[detail::vertex_shader_stage],
                          vecN<c_string, 2>("astral_vert_shader", "astral_pre_vert_shader"),
                          vert_str);

  gl_shader.stream_shader("item", detail::fragment_shader_stage, "astral_only::",
                          gl_shader.distilled_symbols().m_varying_counts,
                          gl_shader.distilled_symbols().m_symbol_counts[detail::fragment_shader_stage],
                          vecN<c_string, 2>("astral_frag_shader", "astral_pre_frag_shader"),
                          frag_str);


  const PreAndActualFunctionSignature *vert_sig, *frag_sig;
  switch (shader_type)
    {
    case ItemShader::color_item_shader:
      vert_sig = &m_shader_builder.m_rect_vert_sigs;
      frag_sig = &m_shader_builder.m_rect_frag_sigs;
      break;

    case ItemShader::mask_item_shader:
      vert_sig = &m_shader_builder.m_mask_vert_sigs;
      frag_sig = &m_shader_builder.m_mask_frag_sigs;
      break;

    case ItemShader::shadow_map_item_shader:
      vert_sig = &m_shader_builder.m_shadow_vert_sigs;
      frag_sig = &m_shader_builder.m_shadow_frag_sigs;
      break;

    default:
      vert_sig = &m_shader_builder.m_rect_vert_sigs;
      frag_sig = &m_shader_builder.m_rect_frag_sigs;
      ASTRALfailure("Invalid shader type");
      break;
    }

  /* this block of code assumes
   *   - function signature for astral_run_vert_shader() is same as vert_sig->m_function
   *   - function signature for astral_run_frag_shader() is same as frag_sig->m_function
   */
  vert_sig->stream_runner_declaration("astral_run_vert_shader", false, &vert_str);
  vert_str << "{\n"
           << "    astral_only::astral_pre_vert_shader(shader - uint(" << gl_shader.begin_shaderID() << ")";
  for (const auto &p : vert_sig->m_pre_function.m_argument_list)
    {
      vert_str << ", " << p.m_name;
    }
  vert_str << ");\n"
           << "    " << vert_sig->m_function.m_return_type
           << " return_value = astral_only::astral_vert_shader(shader - uint(" << gl_shader.begin_shaderID() << ")";
  for (const auto &p : vert_sig->m_function.m_argument_list)
    {
      vert_str << ", " << p.m_name;
    }
  vert_str << ");\n"
           << "    astral_only::astral_item_write_varyings();\n"
           << "    return return_value;\n"
           << "}\n";

  frag_sig->stream_runner_declaration("astral_run_frag_shader", false, &frag_str);
  frag_str << "{\n"
           << "    astral_only::astral_item_load_varyings();\n"
           << "    astral_only::astral_pre_frag_shader(shader - uint(" << gl_shader.begin_shaderID() << ")";
  for (const auto &p : frag_sig->m_pre_function.m_argument_list)
    {
      frag_str << ", " << p.m_name;
    }
  frag_str << ");\n"
           << "    astral_only::astral_frag_shader(shader - uint(" << gl_shader.begin_shaderID() << ")";
  for (const auto &p : frag_sig->m_function.m_argument_list)
    {
      frag_str << ", " << p.m_name;
    }
  frag_str << ");\n"
           << "}\n";

  if (shader_type == ItemShader::color_item_shader)
    {
      unsigned int material_ID;

      material_ID = gl_material->ID();

      vert_str << "\n\n"
               << "void\n"
               << "astral_run_material_vert_shader(in uint material_shader,\n"
               << "                                in uint material_data_location,\n"
               << "                                in uint material_brush,\n"
               << "                                in vec2 item_p,\n"
               << "                                in AstralTransformation tr)\n"
               << "{\n"
               << "    astral_only::astral_material_pre_vert_shader(material_shader - uint(" << material_ID << "),\n"
               << "                                                 material_data_location,\n"
               << "                                                 material_brush, item_p, tr);\n"
               << "    astral_only::astral_material_vert_shader(material_shader - uint(" << material_ID << "),\n"
               << "                                             material_data_location,\n"
               << "                                             material_brush, item_p, tr);\n"
               << "    astral_only::astral_material_write_varyings();\n"
               << "}\n";

      frag_str << "\n\n"
               << "void\n"
               << "astral_run_material_frag_shader(in uint material_shader, in uint color_space, inout vec4 color, inout float coverage)\n"
               << "{\n"
               << "    astral_only::astral_material_load_varyings();\n"
               << "    astral_only::astral_material_pre_frag_shader(material_shader - uint(" << material_ID << "), color_space);\n"
               << "    astral_only::astral_material_frag_shader(material_shader - uint(" << material_ID << "), color_space, color, coverage);\n"
               << "}\n";
    }

  if (shader_type != ItemShader::shadow_map_item_shader)
    {
      frag_str.add_source(blend_shader_src.c_str(), ShaderSource::from_resource);
      frag_str << "\n\n"
               << "vec4\n"
               << "astral_run_apply_blending(in uint shader, in float coverage, in vec4 color)\n"
               << "{\n"
               << "    return astral_apply_blending(coverage, color);\n"
               << "}\n\n";
    }

  create_shader_src(out_vert, out_frag, varyings_str,
                    vert_str, frag_str, shader_type,
                    requires_framebuffer_pixels,
                    shader_clipping);
}

template<typename T>
void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::SourceBuilder::
stream_uber_root_function_subcontents(const std::string &tag,
                                      unsigned int max_depth, const std::string &tabs,
                                      enum detail::shader_stage_t stage,
                                      const PreAndActualFunctionSignature &function_signature,
                                      c_array<const pointer<const T>> shader_list,
                                      ShaderSource &stream)
{
  if (max_depth > 0 && shader_list.size() >= m_shader_builder.m_config.m_uber_shader_max_if_length)
    {
      unsigned int half;

      half = shader_list.size() / 2;
      stream << tabs << "if (shader_id < uint(" << shader_id(*shader_list[half]) << "))\n"
             << tabs << "{\n"
             << tabs << "\t// ids = [ " << shader_id(*shader_list[0]) << ", " << shader_id(*shader_list[half]) << ")\n";
      stream_uber_root_function_subcontents<T>(tag, max_depth - 1, tabs + "\t", stage,
                                               function_signature, shader_list.sub_array(0, half),
                                               stream);
      stream << tabs << "}\n"
             << tabs << "else\n"
             << tabs << "{\n"
             << tabs << "\t// ids = [ " << shader_id(*shader_list[half]) << ", " << shader_id(*shader_list.back()) << "]\n";
      stream_uber_root_function_subcontents<T>(tag, max_depth - 1, tabs + "\t", stage,
                                               function_signature, shader_list.sub_array(half),
                                               stream);
      stream << tabs << "}\n";
      return;
    }

  for (unsigned int i = 0, endi = shader_list.size(); i < endi; ++i)
    {
      const T &shader(*shader_list[i]);
      const std::string &prefix(shader_prefix(shader));
      unsigned int ID, num;

      ID = shader_id(shader);
      num = num_sub_shaders(shader);

      stream << tabs;
      if (i != 0u)
        {
          stream << "else ";
        }

      if (i + 1u != endi)
        {
          if (num == 1u)
            {
              stream << "if (shader_id == uint(" << ID << "))\n";
            }
          else
            {
              stream << "if (shader_id >= uint(" << ID << ") && shader_id < uint(" << ID + num << "))\n";
            }
        }
      else
        {
          stream << "// (shader_id >= uint(" << ID << ") && shader_id < uint(" << ID + num << "))\n";
        }

      stream << tabs << "{\n";
      if (stage == detail::fragment_shader_stage)
        {
          stream << tabs << "\t" << prefix << "::astral_" << tag << "_load_varyings();\n";
        }

      stream << tabs << "\t" << prefix << "::" << function_signature.m_pre_function.m_name
             << "(shader_id - uint(" << ID << ")";
      for (const auto &argument : function_signature.m_pre_function.m_argument_list)
        {
          stream << ", " << argument.m_name;
        }
      stream << ");\n" << tabs << "\t";
      if (!function_signature.m_function.m_return_type.empty())
        {
          stream << "return_value = ";
        }

      stream << prefix << "::" << function_signature.m_function.m_name
             << "(shader_id - uint(" << ID << ")";
      for (const auto &argument : function_signature.m_function.m_argument_list)
        {
          stream << ", " << argument.m_name;
        }
      stream << ");\n";

      if (stage == detail::vertex_shader_stage)
        {
          stream << tabs << "\t" << prefix << "::astral_" << tag << "_write_varyings();\n";
        }

      stream << tabs << "}\n";
    }
}

template<typename T>
void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::SourceBuilder::
stream_uber_root_function_implement(const std::string &tag,
                                    enum detail::shader_stage_t stage,
                                    const PreAndActualFunctionSignature &function_signature,
                                    const std::string &uber_root_function_name,
                                    c_array<const pointer<const T>> shader_list,
                                    ShaderSource &stream)
{
  stream << "\n\n// Stream " << tag << " uber shader\n";
  for (const T *shader : shader_list)
    {
      stream << "//\tIDs : [" << shader_id(*shader)
             << ", " << shader_id(*shader) + num_sub_shaders(*shader)
             << ")\n";
    }

  if (function_signature.m_function.m_return_type.empty())
    {
      stream << "void\n";
    }
  else
    {
      stream << function_signature.m_function.m_return_type << "\n";
    }

  stream << uber_root_function_name << "(in uint shader_id";
  for (const auto &argument : function_signature.m_function.m_argument_list)
    {
      stream << ", " << argument.m_type << " " << argument.m_name;
    }
  stream << ")\n"
         << "{\n";

  ASTRALassert(!shader_list.empty());
  if (shader_list.size() == 1u)
    {
      const T &shader(*shader_list[0]);
      const std::string &prefix(shader_prefix(shader));
      unsigned int ID(shader_id(shader));

      if (stage == detail::fragment_shader_stage)
        {
          stream << "\t" << prefix << "::astral_" << tag << "_load_varyings();\n";
        }

      stream << "\t" << prefix << "::" << function_signature.m_pre_function.m_name
             << "(shader_id - uint(" << ID << ")";
      for (const auto &argument : function_signature.m_pre_function.m_argument_list)
        {
          stream << ", " << argument.m_name;
        }
      stream << ");\n\t\t";
      if (!function_signature.m_function.m_return_type.empty())
        {
          stream << function_signature.m_function.m_return_type << " return_value = ";
        }
      stream << prefix << "::" << function_signature.m_function.m_name
             << "(shader_id - uint(" << ID << ")";
      for (const auto &argument : function_signature.m_function.m_argument_list)
        {
          stream << ", " << argument.m_name;
        }
      stream << ");\n";

      if (stage == detail::vertex_shader_stage)
        {
          stream << "\t" << prefix << "::astral_" << tag << "_write_varyings();\n";
        }

      if (!function_signature.m_function.m_return_type.empty())
        {
          stream << "return return_value;\n";
        }

      stream << "}\n";
      return;
    }

  if (!function_signature.m_function.m_return_type.empty())
    {
      stream << "\t" << function_signature.m_function.m_return_type << " return_value;\n";
    }

  stream_uber_root_function_subcontents<T>(tag, m_shader_builder.m_config.m_uber_shader_max_if_depth,
                                           "\t", stage, function_signature, shader_list, stream);

  if (!function_signature.m_function.m_return_type.empty())
    {
      stream << "\treturn return_value;\n";
    }
  stream << "}\n";
}

bool //return true if framebuffer fetch emulate is required
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::SourceBuilder::
stream_uber_blend_shader(ShaderSource &stream, const std::vector<unsigned int> &blend_shaders)
{
  bool return_value(false);
  for (unsigned int blend_shader : blend_shaders)
    {
      const CommonBlendEpilogue &epi(m_shader_builder.m_blend_epilogue[blend_shader]);

      return_value = return_value || epi.m_requires_framebuffer_pixels;
      stream << "\n#define astral_apply_blending astral_apply_blending" << blend_shader << "\n";
      stream.add_source(epi.m_shader_epilogue.c_str(), ShaderSource::from_resource);
      stream << "#undef astral_apply_blending\n";
    }

  stream << "vec4\n"
         << "astral_run_apply_blending(in uint shader, in float coverage, in vec4 color)\n"
         << "{\n";

  ASTRALassert(!blend_shaders.empty());
  if (blend_shaders.size() == 1u)
    {
      stream << "    return astral_apply_blending" << blend_shaders.front()
             << "(coverage, color);\n";
    }
  else
    {
      for (unsigned int i = 0, endi = blend_shaders.size(); i < endi; ++i)
        {
          stream << "    ";
          if (i != 0)
            {
              stream << "else ";
            }

          if (i + 1u != endi)
            {
              stream << "if (shader == uint(" << blend_shaders[i] << "))";
            }

          stream << "\n    {\n"
                 << "        return astral_apply_blending" << blend_shaders[i]
                 << "(coverage, color);\n"
                 << "    }\n";

        }
    }
  stream << "}\n";

  return return_value;
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::SourceBuilder::
create_uber_shader_src(ShaderSource *out_vert,
                       ShaderSource *out_frag,
                       const UberShaderKey &key,
                       unsigned int *out_number_varyings)
{
  ShaderSource vert, frag, varyings_str;
  bool requires_framebuffer_pixels;
  detail::BackingVaryingCount item_varying_counts;
  vecN<unsigned int, ShaderSymbolList::number_symbol_type> item_vertex_symbol_counts(0);
  vecN<unsigned int, ShaderSymbolList::number_symbol_type> item_fragment_symbol_counts(0);

  detail::BackingVaryingCount material_varying_counts;
  vecN<unsigned int, ShaderSymbolList::number_symbol_type> material_vertex_symbol_counts(0);
  vecN<unsigned int, ShaderSymbolList::number_symbol_type> material_fragment_symbol_counts(0);
  enum clip_window_value_type_t shader_clipping;

  shader_clipping = key.shader_clipping();

  /* Step 1: compute how many varyings and symbols are needed;
   *         the value is the max, not the sum across all included
   *         shaders
   */
  for (const ItemShaderBackendGL3 *item_shader : *key.item_shaders())
    {
      const ItemShaderBackendGL3::Implement *p;

      p = static_cast<const ItemShaderBackendGL3::Implement*>(item_shader);
      item_varying_counts.max_against(p->distilled_symbols().m_varying_counts);
      item_vertex_symbol_counts = component_max(item_vertex_symbol_counts, p->distilled_symbols().m_symbol_counts[detail::vertex_shader_stage]);
      item_fragment_symbol_counts = component_max(item_fragment_symbol_counts, p->distilled_symbols().m_symbol_counts[detail::fragment_shader_stage]);
    }

  for (const MaterialShaderGL3 *material_shader : *key.material_shaders())
    {
      const MaterialShaderGL3::Implement *p;

      p = static_cast<const MaterialShaderGL3::Implement*>(material_shader);
      material_varying_counts.max_against(p->distilled_symbols().m_varying_counts);
      material_vertex_symbol_counts = component_max(material_vertex_symbol_counts, p->distilled_symbols().m_symbol_counts[detail::vertex_shader_stage]);
      material_fragment_symbol_counts = component_max(material_fragment_symbol_counts, p->distilled_symbols().m_symbol_counts[detail::fragment_shader_stage]);
    }

  // stream the varying backings
  detail::ShaderImplementBase::stream_varying_backings("item", item_varying_counts, varyings_str);
  detail::ShaderImplementBase::stream_varying_backings("material", material_varying_counts, varyings_str);

  // stream the symbol backings
  detail::ShaderImplementBase::stream_symbol_backings("item", item_vertex_symbol_counts, vert);
  detail::ShaderImplementBase::stream_symbol_backings("item", item_fragment_symbol_counts, frag);

  detail::ShaderImplementBase::stream_symbol_backings("material", material_vertex_symbol_counts, vert);
  detail::ShaderImplementBase::stream_symbol_backings("material", material_fragment_symbol_counts, frag);

  // stream each of the item shaders
  for (const ItemShaderBackendGL3 *item_shader : *key.item_shaders())
    {
      const ItemShaderBackendGL3::Implement *p;
      std::string prefix;

      prefix = shader_prefix(*item_shader) + "::";
      p = static_cast<const ItemShaderBackendGL3::Implement*>(item_shader);

      p->stream_shader("item", detail::vertex_shader_stage, prefix,
                       item_varying_counts,
                       item_vertex_symbol_counts,
                       vecN<c_string, 2>("astral_vert_shader", "astral_pre_vert_shader"),
                       vert);
      p->stream_shader("item", detail::fragment_shader_stage, prefix,
                       item_varying_counts,
                       item_fragment_symbol_counts,
                       vecN<c_string, 2>("astral_frag_shader", "astral_pre_frag_shader"),
                       frag);
    }

  // stream each of the material shaders
  for (const MaterialShaderGL3 *material_shader : *key.material_shaders())
    {
      const MaterialShaderGL3::Implement *p;
      std::string prefix;

      prefix = shader_prefix(*material_shader) + "::";
      p = static_cast<const MaterialShaderGL3::Implement*>(material_shader);

      p->stream_shader("material", detail::vertex_shader_stage, prefix,
                       material_varying_counts,
                       material_vertex_symbol_counts,
                       vecN<c_string, 2>("astral_material_vert_shader", "astral_material_pre_vert_shader"),
                       vert);
      p->stream_shader("material", detail::fragment_shader_stage, prefix,
                       material_varying_counts,
                       material_fragment_symbol_counts,
                       vecN<c_string, 2>("astral_material_frag_shader", "astral_material_pre_frag_shader"),
                       frag);
    }

  // stream the functions that does the act of uber-ing
  stream_uber_root_function("item", detail::vertex_shader_stage,
                            m_shader_builder.m_rect_vert_sigs,
                            "astral_run_vert_shader",
                            *key.item_shaders(),
                            vert);

  stream_uber_root_function("material", detail::vertex_shader_stage,
                            m_shader_builder.m_material_vert_sigs,
                            "astral_run_material_vert_shader",
                            *key.material_shaders(),
                            vert);

  stream_uber_root_function("item", detail::fragment_shader_stage,
                            m_shader_builder.m_rect_frag_sigs,
                            "astral_run_frag_shader",
                            *key.item_shaders(),
                            frag);

  stream_uber_root_function("material", detail::fragment_shader_stage,
                            m_shader_builder.m_material_frag_sigs,
                            "astral_run_material_frag_shader",
                            *key.material_shaders(),
                            frag);

  /* remember the evil of C short-circuting, it is critical that the stream_uber_blend_shader()
   * is called, where as UberShaderKey::requires_framebuffer_pixels() has no side-effects.
   */
  requires_framebuffer_pixels = stream_uber_blend_shader(frag, *key.blend_shaders())
    || key.requires_framebuffer_pixels();
  create_shader_src(out_vert, out_frag, varyings_str,
                    vert, frag, ItemShader::color_item_shader,
                    requires_framebuffer_pixels,
                    shader_clipping);

  *out_number_varyings = item_varying_counts.total() + material_varying_counts.total();

  /* we use 4 more varyings when we emulate clip-planes which happens
   * exactly whenever m_use_hw_clip_window is false when passed a
   * ClipWindow.
   */
  if (!m_shader_builder.m_config.m_use_hw_clip_window && shader_clipping != clip_window_not_present)
    {
      *out_number_varyings += 4u;
    }

  if (m_shader_builder.m_max_item_material_varying_count < *out_number_varyings)
    {
      std::cerr << "---> Uber shader warning: uber has too many varyings ("
                << *out_number_varyings << ") where only "
                << m_shader_builder.m_max_item_material_varying_count
                << " varyings are allowed\n";
    }
}

///////////////////////////////////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::ShaderBuilder::ShaderListForUberShading methods
void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::ShaderListForUberShading::
begin(enum clip_window_value_type_t shader_clipping,
      enum uber_shader_method_t uber_method)
{
  if (uber_method != uber_shader_cumulative)
    {
      m_item_shaders.clear();
      m_material_shaders.clear();
    }

  if (uber_method != uber_shader_active_blend_cumulative)
    {
      m_blend_shaders.clear();
    }

  m_shader_clipping = shader_clipping;
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::ShaderListForUberShading::
add_shader(const ItemShaderBackendGL3 &item_shader,
           const MaterialShaderGL3 &material_shader,
           unsigned int blend_shader)
{
  m_item_shaders.add(&item_shader, item_shader.shader_builder_index(detail::ShaderIndexArgument()));
  m_material_shaders.add(&material_shader, material_shader.shader_builder_index(detail::ShaderIndexArgument()));
  m_blend_shaders.add(blend_shader, blend_shader);
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::ShaderListForUberShading::
end(UberShaderKey *out_key)
{
  out_key->item_shaders(m_item_shaders.values().begin(), m_item_shaders.values().end());
  out_key->material_shaders(m_material_shaders.values().begin(), m_material_shaders.values().end());
  out_key->blend_shaders(m_blend_shaders.values().begin(), m_blend_shaders.values().end());

  out_key->shader_clipping(m_shader_clipping);
}

bool
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::ShaderListForUberShading::
has_program(const ItemShaderBackendGL3 &item_shader,
            const MaterialShaderGL3 &material_shader,
            unsigned int blend_shader) const
{
  return m_item_shaders.has_element(item_shader.shader_builder_index(detail::ShaderIndexArgument()))
    && m_material_shaders.has_element(material_shader.shader_builder_index(detail::ShaderIndexArgument()))
    && m_blend_shaders.has_element(blend_shader);
}

//////////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::ShaderBuilder::PreAndActualFunctionSignature methods
void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::PreAndActualFunctionSignature::
stream_runner_declaration(c_string name, bool add_semi_colon, ShaderSource *pdst) const
{
  ASTRALassert(pdst);

  ShaderSource &dst(*pdst);

  dst << "\n\n";
  if (m_function.m_return_type.empty())
    {
      dst << "void";
    }
  else
    {
      dst << m_function.m_return_type;
    }
  dst << "\n" << name << "(in uint shader";
  for (const auto &p : m_function.m_argument_list)
    {
      dst << ", " << p.m_type << " " << p.m_name;
    }
  dst << ")";

  if (add_semi_colon)
    {
      dst << ";";
    }
  dst << "\n";
}


//////////////////////////////////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::ShaderBuilder::UberShadingKeyImplement methods
void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::UberShadingKeyImplement::
on_begin_accumulate(enum clip_window_value_type_t shader_clipping, enum uber_shader_method_t uber_method)
{
  m_shader_list.begin(shader_clipping, uber_method);
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::UberShadingKeyImplement::
on_add_shader(const ItemShader &shader,
              const MaterialShader *material_shader,
              BackendBlendMode blend_mode)
{
  const ItemShaderBackendGL3 *gl_shader;
  const MaterialShaderGL3 *gl_material;
  unsigned int blend_id;

  if (!material_shader)
    {
      material_shader = m_default_brush;
    }

  ASTRALassert(dynamic_cast<const ItemShaderBackendGL3*>(&shader.backend()));
  gl_shader = static_cast<const ItemShaderBackendGL3*>(&shader.backend());

  ASTRALassert(dynamic_cast<const MaterialShaderGL3*>(&material_shader->root()));
  gl_material = static_cast<const MaterialShaderGL3*>(&material_shader->root());

  blend_id = m_builder->blend_mode_shader_epilogue(blend_mode);

  m_shader_list.add_shader(*gl_shader, *gl_material, blend_id);
}

uint32_t
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::UberShadingKeyImplement::
on_end_accumulate(void)
{
  uint32_t return_value;
  std::map<UberShaderKey, uint32_t>::iterator iter;

  m_shader_list.end(&m_key);

  if (m_key.item_shaders()->size() <= 1u
      && m_key.material_shaders()->size() <= 1u
      && m_key.blend_shaders()->size() <= 1u)
    {
      /* there is 0 point in doing uber shader if there is
       * only one of everything; this also protects against
       * if no shaders were ever added as well.
       */
      return InvalidRenderValue;
    }

  iter = m_builder->m_uber_shader_lookup.find(m_key);
  if (iter != m_builder->m_uber_shader_lookup.end())
    {
      return_value = iter->second;
    }
  else
    {
      SourceBuilder src_builder(*m_builder);
      ShaderSource vert, frag;
      PerUberShader pr;

      src_builder.create_uber_shader_src(&vert, &frag, m_key, &pr.m_num_varyings);
      if (pr.m_num_varyings < m_builder->m_max_item_material_varying_count)
        {
          pr.m_program = m_builder->create_program(vert, frag);
        }
      pr.m_key = m_key;

      /* It might seem odd that we do not here assert
       * on pr.m_program->link_success(). However, doing
       * so triggers the program to be completely linked
       * which would effectively disable the option to
       * fallback to an uber-shader of all to avoid hitching.
       */

      //std::cout << "RendererAstral: generate uber from key:"
      //        << pr.m_program->name() << "\n";

      return_value = m_builder->m_uber_shaders.size();
      m_builder->m_uber_shaders.push_back(pr);
      m_builder->m_uber_shader_lookup[m_key] = return_value;
    }

  return return_value;
}

uint32_t
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::UberShadingKeyImplement::
on_uber_shader_of_all(enum clip_window_value_type_t shader_clipping)
{
  ASTRALassert(shader_clipping < clip_window_value_type_count);
  return uber_shader_cookie(shader_clipping);
}

//////////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::ShaderBuilder methods
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
ShaderBuilder(RenderEngineGL3 &engine, const BlendBuilder &blend_builder, const Config &config):
  m_item_shader_index_count(0u),
  m_material_shader_index_count(1u), //0 is reserved for no material
  m_uber_shaders(clip_window_value_type_count),
  m_blend_builder(blend_builder),
  m_config(config),
  m_engine(engine)
{
  c_string version;
  ShaderSource vert, frag;

  if (ContextProperties::is_es())
    {
      version = "300 es";

      vert.add_source("astral_gles_precisions.glsl.resource_string",  ShaderSource::from_resource);
      frag.add_source("astral_gles_precisions.glsl.resource_string",  ShaderSource::from_resource);
    }
  else
    {
      version = "330 core";
    }

  vert
    .specify_version(version)
    .add_source("astral_gpu_vertex_streaming_blitter.vert.glsl.resource_string", ShaderSource::from_resource);

  frag
    .specify_version(version)
    .add_source("astral_gpu_vertex_streaming_blitter.frag.glsl.resource_string", ShaderSource::from_resource);

  m_gpu_streaming_blitter = Program::create(vert, frag, PreLinkActionArray().add_binding("in_data", 0));
  ASTRALassert(m_gpu_streaming_blitter->link_success());

  m_recip_half_viewport_width_height_location = m_gpu_streaming_blitter->uniform_location("recip_half_viewport_width_height");
  ASTRALassert(m_recip_half_viewport_width_height_location != -1);

  create_libs();
  create_blend_epilogue_chooser();
  create_shader_singnatures();

  for (unsigned int s = 0; s < clip_window_value_type_count; ++s)
    {
      enum clip_window_value_type_t c;

      c = static_cast<enum clip_window_value_type_t>(s);
      m_uber_shaders[s].m_key.shader_clipping(c);
      m_uber_shaders[s].m_key.blend_shaders_direct(0u, m_blend_epilogue.size());
    }

  /* It would be natural to use ASTRAL_GL_MAX_VARYING_COMPONENTS,
   * but that was deprecated for desktop GL (no clue to be honest why),
   * so we use the awkward ASTRAL_GL_MAX_VARYING_VECTORS which is also
   * present in GLES3.
   */
  unsigned int gl_max_varying_count;
  gl_max_varying_count = 4u * context_get<astral_GLuint>(ASTRAL_GL_MAX_VARYING_VECTORS);

  /* This values comes from the number of varying components in
   * astral_main_packing_bo.glsl.resource_string. If that file is
   * modified, then this decrement needs to be modified.
   */
  const unsigned int number_main_packing_bo_varyings(16);
  m_max_item_material_varying_count = gl_max_varying_count - t_min(number_main_packing_bo_varyings, gl_max_varying_count);
}

unsigned int
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
allocate_item_shader_index(const ItemShaderBackendGL3 *shader, enum ItemShader::type_t type)
{
  unsigned int return_value(m_item_shader_index_count);

  ++m_item_shader_index_count;

  /* if type is color_item_shader, add this shader
   * to the super uber-shader keys
   */
  if (type == ItemShader::color_item_shader)
    {
      //std::cout << "Add: " << shader << ":" << return_value << "\n";

      m_all_color_item_shaders.push_back(shader);
      for (unsigned int s = 0; s < clip_window_value_type_count; ++s)
        {
          m_uber_shaders[s].m_num_varyings = 0;
          m_uber_shaders[s].m_program = nullptr;
        }
    }

  return return_value;
}

unsigned int
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
allocate_material_shader_index(const MaterialShaderGL3 *shader)
{
  unsigned int return_value(m_material_shader_index_count);

  /* add the material shader to the super-uber keys */
  m_all_material_shaders.push_back(shader);
  for (unsigned int s = 0; s < clip_window_value_type_count; ++s)
    {
      m_uber_shaders[s].m_program = nullptr;
    }

  ++m_material_shader_index_count;
  return return_value;
}

astral::reference_counted_ptr<astral::RenderBackend::UberShadingKey>
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
create_uber_shading_key(const MaterialShader *default_brush)
{
  return ASTRALnew UberShadingKeyImplement(*this, default_brush);
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
create_shader_singnatures(void)
{
  ////////////////////////////////////////////
  // Rect shader signatures
  m_rect_vert_sigs.m_pre_function
    .name("astral_pre_vert_shader")
    .add_argument("in uint", "item_data_location")
    .add_argument("in vec4", "a0")
    .add_argument("in AstralTransformation", "item_transformation");

  m_rect_vert_sigs.m_function
    .name("astral_vert_shader")
    .return_type("vec2")
    .add_argument("in uint", "item_data_location")
    .add_argument("in vec4", "a0")
    .add_argument("in AstralTransformation", "item_transformation")
    .add_argument("out vec2", "item_p");

  m_rect_frag_sigs.m_pre_function
    .name("astral_pre_frag_shader")
    .add_argument("in uint", "item_data_location");

  m_rect_frag_sigs.m_function
    .name("astral_frag_shader")
    .add_argument("in uint", "item_data_location")
    .add_argument("in uint", "color_space")
    .add_argument("out float", "coverage")
    .add_argument("out vec4", "base_color");

  ////////////////////////////////////////////
  // Mask shader signatures
  m_mask_vert_sigs.m_pre_function
    .name("astral_pre_vert_shader")
    .add_argument("in uint", "item_data_location")
    .add_argument("in vec4", "a0")
    .add_argument("in AstralTransformation", "item_transformation");

  m_mask_vert_sigs.m_function
    .name("astral_vert_shader")
    .return_type("vec2")
    .add_argument("in uint", "item_data_location")
    .add_argument("in vec4", "a0")
    .add_argument("in AstralTransformation", "item_transformation");

  m_mask_frag_sigs.m_pre_function
    .name("astral_pre_frag_shader")
    .add_argument("in uint", "item_data_location");

  m_mask_frag_sigs.m_function
    .name("astral_frag_shader")
    .add_argument("in uint", "item_data_location")
    .add_argument("out vec4", "base_color");

  /////////////////////////////////////////
  // Shadow shader signatures
  m_shadow_vert_sigs.m_pre_function
    .name("astral_pre_vert_shader")
    .add_argument("in uint", "item_data_location")
    .add_argument("in vec4", "a0")
    .add_argument("in AstralTransformation", "item_transformation");

  m_shadow_vert_sigs.m_function
    .name("astral_vert_shader")
    .return_type("vec2")
    .add_argument("in uint", "item_data_location")
    .add_argument("in vec4", "a0")
    .add_argument("in AstralTransformation", "item_transformation");

  m_shadow_frag_sigs.m_pre_function
    .name("astral_pre_frag_shader")
    .add_argument("in uint", "item_data_location");

  m_shadow_frag_sigs.m_function
    .name("astral_frag_shader")
    .add_argument("in uint", "item_data_location")
    .add_argument("out float", "depth_value");

  ////////////////////////////////////////////
  // Material shader signatures
  m_material_vert_sigs.m_pre_function
    .name("astral_material_pre_vert_shader")
    .add_argument("in uint", "material_data_location")
    .add_argument("in uint", "material_brush_location")
    .add_argument("in vec2", "material_p")
    .add_argument("in AstralTransformation", "pixel_transformation_item");

  m_material_vert_sigs.m_function
    .name("astral_material_vert_shader")
    .add_argument("in uint", "material_data_location")
    .add_argument("in uint", "material_brush_location")
    .add_argument("in vec2", "material_p")
    .add_argument("in AstralTransformation", "pixel_transformation_item");

  m_material_frag_sigs.m_pre_function
    .name("astral_material_pre_frag_shader")
    .add_argument("in uint", "color_space");

  m_material_frag_sigs.m_function
    .name("astral_material_frag_shader")
    .add_argument("in uint", "color_space")
    .add_argument("inout vec4", "item_color")
    .add_argument("inout float", "coverage");
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
create_blend_epilogue_chooser(void)
{
  std::unordered_map<std::string, CommonBlendEpilogue> epilogue_map_shader;
  for (uint32_t v = 0u; v < BackendBlendMode::number_packed_values; ++v)
    {
      BackendBlendMode bb(BackendBlendMode::from_packed_value(v));
      const BlendBuilder::PerBlendMode &info(m_blend_builder.info(bb));
      std::unordered_map<std::string, CommonBlendEpilogue>::iterator iter(epilogue_map_shader.find(info.shader(m_blend_builder)));
      CommonBlendEpilogue *dst;

      if (iter == epilogue_map_shader.end())
        {
          CommonBlendEpilogue &ref(epilogue_map_shader[info.shader(m_blend_builder)]);

          dst = &ref;
          dst->m_shader_epilogue = info.shader(m_blend_builder);
        }
      else
        {
          dst = &iter->second;
        }

      ASTRALassert(dst->m_shader_epilogue == info.shader(m_blend_builder));
      dst->m_elements.push_back(bb);
      dst->m_requires_framebuffer_pixels = dst->m_requires_framebuffer_pixels
        || info.pixels_needed() != BlendModeInformation::does_not_need_framebuffer_pixels;
    }

  m_blend_epilogue.reserve(epilogue_map_shader.size());
  for (auto &v : epilogue_map_shader)
    {
      CommonBlendEpilogue &dsc(v.second);

      for (BackendBlendMode e : dsc.m_elements)
        {
          m_blend_epilogue_chooser[e.packed_value()] = m_blend_epilogue.size();
        }
      m_blend_epilogue.push_back(CommonBlendEpilogue());
      m_blend_epilogue.back().swap(dsc);
    }
}

astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
~ShaderBuilder()
{
}

bool
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
uber_has_shader(RenderBackend::UberShadingKey::Cookie key,
                const ItemShader &shader,
                const MaterialShader &material,
                BackendBlendMode blend_mode) const
{
  const ItemShaderBackendGL3 *gl_shader;
  const MaterialShaderGL3 *gl_material;
  unsigned int blend_id;

  /* uber shading is only valid for color rendering */
  ASTRALassert(shader.type() == ItemShader::color_item_shader);

  ASTRALassert(dynamic_cast<const ItemShaderBackendGL3*>(&shader.backend()));
  gl_shader = static_cast<const ItemShaderBackendGL3*>(&shader.backend());

  ASTRALassert(dynamic_cast<const MaterialShaderGL3*>(&material.root()));
  gl_material = static_cast<const MaterialShaderGL3*>(&material.root());

  blend_id = blend_mode_shader_epilogue(blend_mode);

  ASTRALassert(key.m_value < m_uber_shaders.size());
  return uber_shader_cookie_is_all_uber_shader(key)
    || m_uber_shaders[key.m_value].m_key.has_program(*gl_shader, *gl_material, blend_id);
}

bool
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
uber_has_shaders(RenderBackend::UberShadingKey::Cookie key,
                 c_array<const pointer<const ItemShader>> shaders,
                 const MaterialShader &material_shader,
                 BackendBlendMode blend_mode) const
{
  for (unsigned int i = 0; i < shaders.size(); ++i)
    {
      ASTRALassert(shaders[i]);
      if (!uber_has_shader(key, *shaders[i], material_shader, blend_mode))
        {
          return false;
        }
    }
  return true;
}

enum astral::clip_window_value_type_t
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
uber_shader_clipping(RenderBackend::UberShadingKey::Cookie key) const
{
  ASTRALassert(key.m_value < m_uber_shaders.size());
  return m_uber_shaders[key.m_value].m_key.shader_clipping();
}

astral::gl::Program*
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
uber_program(RenderBackend::UberShadingKey::Cookie v)
{
  ASTRALassert(v.m_value < m_uber_shaders.size());
  if (!m_uber_shaders[v.m_value].m_program && m_uber_shaders[v.m_value].m_num_varyings == 0)
    {
      /* only the uber-of-all can have its program be null */
      ASTRALassert(uber_shader_cookie_is_all_uber_shader(v));

      SourceBuilder src_builder(*this);
      ShaderSource vert, frag;

      m_uber_shaders[v.m_value].m_key.item_shaders(m_all_color_item_shaders.begin(), m_all_color_item_shaders.end());
      m_uber_shaders[v.m_value].m_key.material_shaders(m_all_material_shaders.begin(), m_all_material_shaders.end());

      src_builder.create_uber_shader_src(&vert, &frag, m_uber_shaders[v.m_value].m_key, &m_uber_shaders[v.m_value].m_num_varyings);
      if (m_uber_shaders[v.m_value].m_num_varyings < m_max_item_material_varying_count)
        {
          m_uber_shaders[v.m_value].m_program = create_program(vert, frag);
        }
    }

  return m_uber_shaders[v.m_value].m_program.get();
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
force_uber_shader_program_link(void)
{
  for (unsigned int i = 0; i < clip_window_value_type_count; ++i)
    {
      /* calling link success forces the uber to get linked */
      uber_program(RenderBackend::UberShadingKey::Cookie(i))->link_success();
    }
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
create_libs(void)
{
  create_base_lib();
  create_stroke_lib();
  create_image_lib();
  create_gradient_lib();
  create_item_path_lib();
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
create_stroke_lib(void)
{
  ShaderSource src;

  src
    .add_macro_u32("ASTRAL_STROKE_SHADER_STATIC", StrokeShader::static_path_shader)
    .add_macro_u32("ASTRAL_STROKE_SHADER_ANIMATED", StrokeShader::animated_path_shader)
    .add_macro_u32("ASTRAL_STROKE_SHADER_ANIMATED_MASK", StrokeShaderGL3Enums::stroker_shader_animation_mask)
    .add_macro_u32("ASTRAL_STROKE_SHADER_ANIMATED_BIT0", StrokeShaderGL3Enums::stroker_shader_animation_bit0)
    .add_macro_u32("ASTRAL_STROKE_SUB_SHADER_MASK", StrokeShaderGL3Enums::stroker_shader_subtype_mask)
    .add_macro_u32("ASTRAL_STROKE_SUB_SHADER_BIT0", StrokeShaderGL3Enums::stroker_shader_subtype_bit0)
    .add_macro_u32("ASTRAL_STROKE_SUB_SHADER_OUTER_JOIN", StrokeShaderGL3Enums::outer_join_sub_shader)
    .add_macro_u32("ASTRAL_STROKE_SUB_SHADER_INNER_JOIN", StrokeShaderGL3Enums::inner_join_sub_shader)
    .add_macro_u32("ASTRAL_STROKE_CAP_STYLE_MASK", StrokeShaderGL3Enums::stroker_shader_cap_style_mask)
    .add_macro_u32("ASTRAL_STROKE_CAP_STYLE_BIT0", StrokeShaderGL3Enums::stroker_shader_cap_style_bit0)
    .add_macro_u32("ASTRAL_STROKE_CAP_STYLE_FLAT_CAP", cap_flat)
    .add_macro_u32("ASTRAL_STROKE_CAP_STYLE_ROUNDED_CAP", cap_rounded)
    .add_macro_u32("ASTRAL_STROKE_CAP_STYLE_SQUARE_CAP", cap_square)
    .add_macro_u32("ASTRAL_STROKE_CAPPER_START", StrokeShader::capper_shader_start)
    .add_macro_u32("ASTRAL_STROKE_CAPPER_END", StrokeShader::capper_shader_end)
    .add_macro_u32("ASTRAL_STROKE_START_EDGE_MASK", StrokeShader::start_edge_mask)
    .add_macro_u32("ASTRAL_STROKE_END_EDGE_MASK", StrokeShader::end_edge_mask)
    .add_macro_u32("ASTRAL_STROKE_START_CONTOUR_MASK", StrokeShader::start_contour_mask)
    .add_macro_u32("ASTRAL_STROKE_END_CONTOUR_MASK", StrokeShader::end_contour_mask)
    .add_macro_u32("ASTRAL_STROKE_CLOSED_CONTOUR_MASK", StrokeShader::contour_closed_mask)
    .add_macro_u32("ASTRAL_STROKE_END_CAP_MASK", StrokeShader::cap_end_mask)

    .add_macro_u32("ASTRAL_STROKE_SHADER_LINE_OFFSET_TYPE_MASK", StrokeShader::line_offset_type_mask)
    .add_macro_u32("ASTRAL_STROKE_SHADER_LINE_END_POINT_MASK", StrokeShader::line_is_end_point_mask)
    .add_macro_u32("ASTRAL_STROKE_SHADER_LINE_NEGATE_NORMAL", StrokeShader::line_offset_negate_normal)
    .add_macro_u32("ASTRAL_STROKE_SHADER_LINE_BASE_POINT", StrokeShader::line_offset_base_point)
    .add_macro_u32("ASTRAL_STROKE_SHADER_LINE_ADD_NORMAL", StrokeShader::line_offset_normal)

    .add_macro_u32("ASTRAL_STROKE_SHADER_ARC_OFFSET_TYPE_MASK", StrokeShader::biarc_offset_type_mask)
    .add_macro_u32("ASTRAL_STROKE_SHADER_ARC_END_POINT_MASK", StrokeShader::biarc_is_end_point_mask)
    .add_macro_u32("ASTRAL_STROKE_SHADER_ARC_OMEGA", StrokeShader::biarc_offset_omega)
    .add_macro_u32("ASTRAL_STROKE_SHADER_ARC_ZETA", StrokeShader::biarc_offset_zeta)
    .add_macro_u32("ASTRAL_STROKE_SHADER_ARC_TOWARDS_CENTER", StrokeShader::biarc_offset_towards_center)
    .add_macro_u32("ASTRAL_STROKE_SHADER_ARC_BASE_POINT", StrokeShader::biarc_offset_base_point)
    .add_macro_u32("ASTRAL_STROKE_SHADER_ARC_AWAY_FROM_CENTER", StrokeShader::biarc_offset_away_from_center)
    .add_macro_u32("ASTRAL_STROKE_SHADER_ARC_TOP", StrokeShader::biarc_offset_top)
    .add_macro_u32("ASTRAL_STROKE_SHADER_BIARC_SECOND_ARC_MASK", StrokeShader::biarc_is_second_arc_mask)

    .add_macro_u32("ASTRAL_STROKE_SHADER_JOIN_OFFSET_TYPE_MASK", StrokeShader::join_offset_type_mask)
    .add_macro_u32("ASTRAL_STROKE_SHADER_JOIN_END_POINT_MASK", StrokeShader::join_point_leave_mask)
    .add_macro_u32("ASTRAL_STROKE_SHADER_JOIN_POINT_ON_PATH", StrokeShader::join_point_on_path)
    .add_macro_u32("ASTRAL_STROKE_SHADER_JOIN_POINT_EDGE_BOUNDARY", StrokeShader::join_point_edge_boundary)
    .add_macro_u32("ASTRAL_STROKE_SHADER_JOIN_POINT_BEYOND_EDGE_BOUNDARY", StrokeShader::join_point_beyond_boundary)
    .add_macro_u32("ASTRAL_STROKE_SHADER_CAP_OFFSET_TYPE_MASK", StrokeShader::cap_offset_type_mask)
    .add_macro_u32("ASTRAL_STROKE_SHADER_CAP_SIDE_MASK", StrokeShader::cap_point_side_mask)
    .add_macro_u32("ASTRAL_STROKE_SHADER_CAP_POINT_ON_PATH", StrokeShader::cap_point_path)
    .add_macro_u32("ASTRAL_STROKE_SHADER_CAP_POINT_EDGE_BOUNDARY", StrokeShader::cap_point_edge_boundary)
    .add_macro_u32("ASTRAL_STROKE_SHADER_CAP_POINT_BEYOND_EDGE_BOUNDARY", StrokeShader::cap_point_beyond_boundary)

    .add_macro_u32("ASTRAL_STROKE_SHADER_LINE_SEGMENT_SIZE", StrokeShader::line_segment_static_data_size)
    .add_macro_u32("ASTRAL_STROKE_SHADER_QUADRATIC_CURVE_SIZE", StrokeShader::quadratic_curve_data_data_size)
    .add_macro_u32("ASTRAL_STROKE_SHADER_JOIN_SIZE", StrokeShader::join_static_data_size)
    .add_macro_u32("ASTRAL_STROKE_SHADER_CAP_SIZE", StrokeShader::cap_static_data_size)
    .add_macro_u32("ASTRAL_STROKE_SHADER_LINE_SEGMENT_PAIR_SIZE", StrokeShader::line_segment_pair_static_data_size)
    .add_macro_u32("ASTRAL_STROKE_SHADER_QUADRATIC_CURVE_PAIR_SIZE", StrokeShader::quadratic_curve_pair_static_data_size)
    .add_macro_u32("ASTRAL_STROKE_SHADER_JOIN_PAIR_SIZE", StrokeShader::join_pair_static_data_size)
    .add_macro_u32("ASTRAL_STROKE_SHADER_CAP_PAIR_SIZE", StrokeShader::cap_pair_static_data_size)

    .add_macro_u32("ASTRAL_STROKE_ROLE_BIT0", StrokeShader::role_bit0)
    .add_macro_u32("ASTRAL_STROKE_ROLE_NUM_BITS", StrokeShader::role_number_bits)
    .add_macro_u32("ASTRAL_STROKE_FLAGS_BIT0", StrokeShader::flags_bit0)
    .add_macro_u32("ASTRAL_STROKE_FLAGS_NUM_BITS", StrokeShader::flags_number_bits)

    .add_macro_u32("ASTRAL_STROKE_DASH_ADJUST_COMPRESS", StrokeShader::DashPattern::length_adjust_compress)
    .add_macro_u32("ASTRAL_STROKE_DASH_ADJUST_STRETCH", StrokeShader::DashPattern::length_adjust_stretch)
    .add_macro_u32("ASTRAL_STROKE_DASH_STARTS_PER_EDGE", StrokeShader::DashPattern::stroke_starts_at_edge)
    .add_macro_u32("ASTRAL_STROKE_DASH_ADJUST_XZ", StrokeShader::DashPattern::adjust_xz_lengths)
    .add_macro_u32("ASTRAL_STROKE_DASH_ADJUST_YW", StrokeShader::DashPattern::adjust_yw_lengths)
    .add_macro_u32("ASTRAL_STROKE_DASH_ADJUST_XZ_AND_YW", StrokeShader::DashPattern::adjust_xz_and_yw_lengths)

    .add_library(m_base_lib)
    .add_macro_u32("ASTRAL_STROKING_ARC_INVERTED_PORTION_MASK", 1u << 31u)
    .add_macro_u32("ASTRAL_STROKING_DASH_DATA_START", StrokeShader::ItemDataPacker::item_data_count)
    .add_source("astral_stroke_common.glsl.resource_string", ShaderSource::from_resource)
    .add_source("astral_stroke_common_join.glsl.resource_string", ShaderSource::from_resource)
    .add_source("astral_stroke_common_cap.glsl.resource_string", ShaderSource::from_resource)
    .add_source("astral_stroke_capper_util.glsl.resource_string", ShaderSource::from_resource)
    .add_source("astral_stroke_biarc_util.glsl.resource_string", ShaderSource::from_resource);

  m_shader_libs.m_stroke_lib = ShaderLibrary::create(src);
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
create_image_lib(void)
{
  /* macro-constants for packing/unpacking AstralImageLOD */
  const uint32_t ASTRAL_IMAGE_LOD_PACKING_ABSOLUTE_LOD_NUMBITS = 4u; //enough to specify any value in [0, 10)
  const uint32_t ASTRAL_IMAGE_LOD_PACKING_NUMBER_LEVELS_NUMBITS = PackedImageMipElement::number_index_levels_num_bits;
  const uint32_t ASTRAL_IMAGE_LOD_PACKING_ROOT_TILE_Z_NUMBITS = PackedImageMipElement::root_tile_z_num_bits;

  const uint32_t ASTRAL_IMAGE_LOD_PACKING_ABSOLUTE_LOD_BIT0 = 0u;
  const uint32_t ASTRAL_IMAGE_LOD_PACKING_NUMBER_LEVELS_BIT0 = ASTRAL_IMAGE_LOD_PACKING_ABSOLUTE_LOD_BIT0 + ASTRAL_IMAGE_LOD_PACKING_ABSOLUTE_LOD_NUMBITS;
  const uint32_t ASTRAL_IMAGE_LOD_PACKING_ROOT_TILE_Z_BIT0 = ASTRAL_IMAGE_LOD_PACKING_NUMBER_LEVELS_BIT0 + ASTRAL_IMAGE_LOD_PACKING_NUMBER_LEVELS_NUMBITS;

  m_shader_libs.m_image_lib =
    ShaderLibrary::create(ShaderSource()
                          .add_library(m_base_lib)
                          .add_macro_u32("ASTRAL_IMAGE_LOD_PACKING_ABSOLUTE_LOD_NUMBITS", ASTRAL_IMAGE_LOD_PACKING_ABSOLUTE_LOD_NUMBITS)
                          .add_macro_u32("ASTRAL_IMAGE_LOD_PACKING_NUMBER_LEVELS_NUMBITS", ASTRAL_IMAGE_LOD_PACKING_NUMBER_LEVELS_NUMBITS)
                          .add_macro_u32("ASTRAL_IMAGE_LOD_PACKING_ROOT_TILE_Z_NUMBITS", ASTRAL_IMAGE_LOD_PACKING_ROOT_TILE_Z_NUMBITS)
                          .add_macro_u32("ASTRAL_IMAGE_LOD_PACKING_ABSOLUTE_LOD_BIT0", ASTRAL_IMAGE_LOD_PACKING_ABSOLUTE_LOD_BIT0)
                          .add_macro_u32("ASTRAL_IMAGE_LOD_PACKING_NUMBER_LEVELS_BIT0", ASTRAL_IMAGE_LOD_PACKING_NUMBER_LEVELS_BIT0)
                          .add_macro_u32("ASTRAL_IMAGE_LOD_PACKING_ROOT_TILE_Z_BIT0", ASTRAL_IMAGE_LOD_PACKING_ROOT_TILE_Z_BIT0)
                          .add_source("astral_image_util.glsl.resource_string", ShaderSource::from_resource)
                          .add_source("astral_image.glsl.resource_string", ShaderSource::from_resource));
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
create_gradient_lib(void)
{
  m_shader_libs.m_gradient_lib =
    ShaderLibrary::create(ShaderSource()
                          .add_library(m_base_lib)
                          .add_source("astral_gradient_bo.glsl.resource_string", ShaderSource::from_resource));
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
create_item_path_lib(void)
{
  m_shader_libs.m_item_path_lib =
    ShaderLibrary::create(ShaderSource()
                          .add_library(m_base_lib)
                          .add_source("astral_banded_rays_common.glsl.resource_string", ShaderSource::from_resource)
                          .add_source("astral_banded_rays_nearest_curve.glsl.resource_string", ShaderSource::from_resource)
                          .add_source("astral_banded_rays_neighbor_pixel.glsl.resource_string", ShaderSource::from_resource));
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
create_base_lib(void)
{
  ShaderSource dst;
  const float pi = 3.14159265358979323846f;

  /* depth buffer's are typically 24-bits;
   * We will (somewhat arbitrarily) trust 21
   * bits of depth. The value is 20 because
   * the GL's normalized depth value is in
   * the range [-1, 1] and the shader code
   * to produce it is given by the formula
   * ASTRAL_Z_COEFF * float(z) - 1.0
   */
  const uint32_t z_max = 1u << t_min(20u, (unsigned int)Packing::header_z_num_bits);
  const float z_coeff = 1.0f / static_cast<float>(z_max);
  c_string version;
  uint32_t white_tile_id, empty_tile_id;

  white_tile_id = ImageIndexBacking::texel_value_from_location(m_engine.image_atlas().white_tile_atlas_location());
  empty_tile_id = ImageIndexBacking::texel_value_from_location(m_engine.image_atlas().empty_tile_atlas_location());

  if (ContextProperties::is_es())
    {
      version = "300 es";
      if (m_config.m_use_hw_clip_window)
        {
          dst
            .specify_extension("GL_EXT_clip_cull_distance", ShaderSource::enable_extension)
            .specify_extension("GL_APPLE_clip_distance", ShaderSource::enable_extension)
            .specify_extension("GL_ANGLE_clip_cull_distance", ShaderSource::enable_extension);
        }
      dst.add_source("astral_gles_precisions.glsl.resource_string",  ShaderSource::from_resource);
    }
  else
    {
      version = "330 core";
    }

  /* define astral-varying */
  dst << "\n#ifdef ASTRAL_VERTEX_SHADER\n"
      << "#define astral_varying out\n"
      << "#else\n"
      << "#define astral_varying in\n"
      << "#endif\n\n";

  dst
    // constants
    .add_macro("ASTRAL_PI", pi)
    .add_macro("ASTRAL_2PI", 2.0f * pi)
    .add_macro("ASTRAL_RECIP_PI", 1.0f / pi)
    .add_macro("ASTRAL_RECIP_2PI", 0.5f / pi)
    .add_macro("ASTRAL_SQRT2", ASTRAL_SQRT2)
    .add_macro("ASTRAL_HALF_SQRT2", ASTRAL_HALF_SQRT2)
    .add_macro_u32("ASTRAL_INVALID_INDEX", Packing::invalid_render_index)
    .add_macro("ASTRAL_Z_COEFF", z_coeff)
    .add_macro_u32("ASTRAL_DEPTH_CLEAR", RenderBackend::depth_buffer_value_clear)
    .add_macro_u32("ASTRAL_DEPTH_OCCLUDE", RenderBackend::depth_buffer_value_occlude)
    .add_macro("ASTRAL_STROKE_HARILINE_PIXEL_RADIUS", StrokeParameters::hairline_pixel_radius())
    .add_macro_u32("ASTRAL_RENDER_PATH_STC_MAX_MAJOR_MASK", FillSTCShader::ConicTriangle::max_major)
    .add_macro_u32("ASTRAL_RENDER_PATH_STC_MAX_MINOR_MASK", FillSTCShader::ConicTriangle::max_minor)
    .add_macro("astral_colorstop_inverse_width", 1.0f / float(1u << m_config.m_log2_dims_colorstop_atlas))
    .add_macro("astral_shadow_map_inverse_width", 1.0f / float(m_config.m_shadow_map_atlas_width))
    .add_macro_u32("astral_white_tile_id", white_tile_id)
    .add_macro_u32("astral_empty_tile_id", empty_tile_id)
    // configuration
    .add_macro("ASTRAL_NUMBER_HEADERS", m_config.m_max_per_draw_call[data_header])
    .add_macro("ASTRAL_NUMBER_ITEM_TRANSFORMATIONS", m_config.m_max_per_draw_call[data_item_transformation])
    .add_macro("ASTRAL_NUMBER_ITEM_SCALE_TRANSLATES", m_config.m_max_per_draw_call[data_item_scale_translate])
    .add_macro("ASTRAL_NUMBER_CLIP_WINDOWS", m_config.m_max_per_draw_call[data_clip_window])
    .add_macro("ASTRAL_NUMBER_BRUSHES", m_config.m_max_per_draw_call[data_brush])
    .add_macro("ASTRAL_NUMBER_GRADIENTS", m_config.m_max_per_draw_call[data_gradient])
    .add_macro("ASTRAL_NUMBER_IMAGE_TRANSFORMATIONS", m_config.m_max_per_draw_call[data_gradient_transformation])
    .add_macro("ASTRAL_NUMBER_SHADER_DATA", m_config.m_max_per_draw_call[data_item_data])
    .add_macro("ASTRAL_NUMBER_TILED_IMAGES", m_config.m_max_per_draw_call[data_image])
    .add_macro("ASTRAL_NUMBER_SHADOW_MAPS", m_config.m_max_per_draw_call[data_shadow_map])
    .add_macro("ASTRAL_NUMBER_CLIP_ELEMENTS", m_config.m_max_per_draw_call[data_clip_mask])
    .add_macro("ASTRAL_HEADER_SIZE", Packing::element_size_blocks(data_header))
    .add_macro("ASTRAL_ITEM_TRANSFORMATION_SIZE", Packing::element_size_blocks(data_item_transformation))
    .add_macro("ASTRAL_ITEM_SCALE_TRANSLATE_SIZE", Packing::element_size_blocks(data_item_scale_translate))
    .add_macro("ASTRAL_CLIP_WINDOW_SIZE", Packing::element_size_blocks(data_clip_window))
    .add_macro("ASTRAL_BRUSH_SIZE", Packing::element_size_blocks(data_brush))
    .add_macro("ASTRAL_GRADIENT_SIZE", Packing::element_size_blocks(data_gradient))
    .add_macro("ASTRAL_IMAGE_TRANSFORMATION_SIZE", Packing::element_size_blocks(data_gradient_transformation))
    .add_macro("ASTRAL_TILED_IMAGE_SIZE", Packing::element_size_blocks(data_image))
    .add_macro("ASTRAL_SHADOW_MAP_SIZE", Packing::element_size_blocks(data_shadow_map))
    .add_macro("ASTRAL_CLIP_ELEMENT_SIZE", Packing::element_size_blocks(data_clip_mask))
    // header unpack enums
    .add_macro_u32("ASTRAL_HEADER_Z_BIT0", Packing::header_z_bit0)
    .add_macro_u32("ASTRAL_HEADER_Z_NUM_BITS", Packing::header_z_num_bits)
    .add_macro_u32("ASTRAL_HEADER_BLEND_SHADER_ID_BIT0", Packing::header_blend_shader_id_bit0)
    .add_macro_u32("ASTRAL_HEADER_BLEND_SHADER_ID_NUM_BITS", Packing::header_blend_shader_id_num_bits)
    .add_macro_u32("ASTRAL_HEADER_LOCATION_ID_BIT0", ShaderBuilder::header_location_id_bit0)
    .add_macro_u32("ASTRAL_HEADER_LOCATION_ID_NUM_BITS", ShaderBuilder::header_location_id_num_bits)
    .add_macro_u32("ASTRAL_HEADER_LOCATION_COLOR_SPACE_BIT0", ShaderBuilder::header_location_color_space_bit0)
    .add_macro_u32("ASTRAL_HEADER_LOCATION_COLOR_SPACE_NUM_BITS", ShaderBuilder::header_location_color_space_num_bits)
    .add_macro_u32("ASTRAL_HEADER_LOCATION_PERMUTE_XY_BIT", ShaderBuilder::header_location_permute_xy_bit)
    .add_macro_u32("ASTRAL_HEADER_LOCATION_PERMUTE_XY_MASK", ASTRAL_MASK(ShaderBuilder::header_location_permute_xy_bit, 1u))
    // fill rule enums
    .add_macro_u32("ASTRAL_ODD_EVEN_FILL_RULE", odd_even_fill_rule)
    .add_macro_u32("ASTRAL_NON_ZERO_FILL_RULE", nonzero_fill_rule)
    .add_macro_u32("ASTRAL_COMPLEMENT_ODD_EVEN_FILL_RULE", complement_odd_even_fill_rule)
    .add_macro_u32("ASTRAL_COMPLEMENT_NON_ZERO_FILL_RULE", complement_nonzero_fill_rule)
    // tile mode enums
    .add_macro_u32("ASTRAL_TILE_MODE_CLAMP", tile_mode_clamp)
    .add_macro_u32("ASTRAL_TILE_MODE_MIRROR", tile_mode_mirror)
    .add_macro_u32("ASTRAL_TILE_MODE_REPEAT", tile_mode_repeat)
    .add_macro_u32("ASTRAL_TILE_MODE_MIRROR_REPEAT", tile_mode_mirror_repeat)
    .add_macro_u32("ASTRAL_TILE_MODE_DECAL", tile_mode_decal)
    .add_macro_u32("ASTRAL_TILE_MODE_NUMBER_BITS", ImageSamplerBits::tile_mode_num_bits) //a tile mode requires 3 bits to pack
    .add_macro_u32("ASTRAL_X_TILE_MODE_BIT0", ImageSamplerBits::x_tile_mode_bit0)
    .add_macro_u32("ASTRAL_Y_TILE_MODE_BIT0", ImageSamplerBits::y_tile_mode_bit0)
    .add_macro_u32("ASTRAL_WINDOW_TILE_MODE_NUM_BITS", 2u * ImageSamplerBits::tile_mode_num_bits)
    // sRGB or linear encoding
    .add_macro_u32("ASTRAL_COLORSPACE_LINEAR", colorspace_linear)
    .add_macro_u32("ASTRAL_COLORSPACE_SRGB", colorspace_srgb)
    .add_macro_u32("ASTRAL_IMAGE_COLORSPACE_BIT0", ImageSamplerBits::colorspace_bit0)
    .add_macro_u32("ASTRAL_IMAGE_COLORSPACE_NUMBER_BITS", ImageSamplerBits::colorspace_num_bits)
    // mask-channel enums
    .add_macro_u32("ASTRAL_MASK_CHANNEL_RED", mask_channel_red)
    .add_macro_u32("ASTRAL_MASK_CHANNEL_GREEN", mask_channel_green)
    .add_macro_u32("ASTRAL_MASK_CHANNEL_BLUE", mask_channel_blue)
    .add_macro_u32("ASTRAL_MASK_CHANNEL_ALPHA", mask_channel_alpha)
    .add_macro_u32("ASTRAL_MASK_CHANNEL_INVALID", number_mask_channel)
    .add_macro_u32("ASTRAL_MASK_CHANNEL_BIT0", ImageSamplerBits::mask_channel_bit0)
    .add_macro_u32("ASTRAL_MASK_CHANNEL_NUM_BITS", ImageSamplerBits::mask_channel_num_bits)
    .add_macro_u32("ASTRAL_MASK_CHANNEL_MASK", ImageSamplerBits::mask_channel_mask)
    // color post-sampling mode enums
    .add_macro_u32("ASTRAL_COLOR_POST_SAMPLING_MODE_BITS_ALPHA_INVERT", color_post_sampling_mode_bits_alpha_invert)
    .add_macro_u32("ASTRAL_COLOR_POST_SAMPLING_MODE_BITS_RGB_ZERO", color_post_sampling_mode_bits_rgb_zero)
    .add_macro_u32("ASTRAL_COLOR_POST_SAMPLING_MODE_BITS_RGB_INVERT", color_post_sampling_mode_bits_rgb_invert)
    .add_macro_u32("ASTRAL_COLOR_POST_SAMPLING_MODE_BITS_ALPHA_ONE", color_post_sampling_mode_bits_alpha_one)
    .add_macro_u32("ASTRAL_COLOR_POST_SAMPLING_MODE_DIRECT", color_post_sampling_mode_direct)
    .add_macro_u32("ASTRAL_COLOR_POST_SAMPLING_MODE_BLACK", color_post_sampling_mode_black)
    .add_macro_u32("ASTRAL_COLOR_POST_SAMPLING_MODE_WHITE", color_post_sampling_mode_white)
    .add_macro_u32("ASTRAL_COLOR_POST_SAMPLING_MODE_BLACK_ALPHA_INVERT", color_post_sampling_mode_black_alpha_invert)
    .add_macro_u32("ASTRAL_COLOR_POST_SAMPLING_MODE_WHITE_ALPHA_INVERT", color_post_sampling_mode_white_alpha_invert)
    .add_macro_u32("ASTRAL_COLOR_POST_SAMPLING_MODE_RGB_INVERT", color_post_sampling_mode_rgb_invert)
    .add_macro_u32("ASTRAL_COLOR_POST_SAMPLING_MODE_RGB_DIRECT_ALPHA_ONE", color_post_sampling_mode_rgb_direct_alpha_one)
    .add_macro_u32("ASTRAL_COLOR_POST_SAMPLING_MODE_OPAQUE_BLACK", color_post_sampling_mode_opaque_black)
    .add_macro_u32("ASTRAL_COLOR_POST_SAMPLING_MODE_OPAQUE_WHITE", color_post_sampling_mode_opaque_white)
    .add_macro_u32("ASTRAL_COLOR_POST_SAMPLING_MODE_OPAQUE_RGB_INVERT", color_post_sampling_mode_opaque_rgb_invert)
    .add_macro_u32("ASTRAL_COLOR_POST_SAMPLING_MODE_BIT0", ImageSamplerBits::color_post_sampling_mode_bit0)
    .add_macro_u32("ASTRAL_COLOR_POST_SAMPLING_MODE_NUM_BITS", ImageSamplerBits::color_post_sampling_mode_num_bits)
    .add_macro_u32("ASTRAL_COLOR_POST_SAMPLING_MODE_MASK", ImageSamplerBits::color_post_sampling_mode_mask)

    // mask post-sampling mode enums
    .add_macro_u32("ASTRAL_MASK_POST_SAMPLING_MODE_DIRECT", mask_post_sampling_mode_direct)
    .add_macro_u32("ASTRAL_MASK_POST_SAMPLING_MODE_INVERT", mask_post_sampling_mode_invert)
    .add_macro_u32("ASTRAL_MASK_POST_SAMPLING_MODE_BIT0", ImageSamplerBits::mask_post_sampling_mode_bit0)
    .add_macro_u32("ASTRAL_MASK_POST_SAMPLING_MODE_NUM_BITS", ImageSamplerBits::mask_post_sampling_mode_num_bits)
    .add_macro_u32("ASTRAL_MASK_POST_SAMPLING_MODE_MASK", ImageSamplerBits::mask_post_sampling_mode_mask)

    // mask_type enums
    .add_macro_u32("ASTRAL_COVERAGE_MASK", mask_type_coverage)
    .add_macro_u32("ASTRAL_DISTANCE_FIELD_MASK", mask_type_distance_field)
    .add_macro_u32("ASTRAL_MASK_TYPE_BIT0", ImageSamplerBits::mask_type_bit0)
    .add_macro_u32("ASTRAL_MASK_TYPE_NUM_BITS", ImageSamplerBits::mask_type_num_bits)
    .add_macro_u32("ASTRAL_MASK_TYPE_MASK", ImageSamplerBits::mask_type_mask)
    // filter enums
    .add_macro_u32("ASTRAL_FILTER_NEAREST", filter_nearest)
    .add_macro_u32("ASTRAL_FILTER_LINEAR", filter_linear)
    .add_macro_u32("ASTRAL_FILTER_CUBIC", filter_cubic)
    .add_macro_u32("ASTRAL_FILTER_BIT0", ImageSamplerBits::filter_bit0)
    .add_macro_u32("ASTRAL_FILTER_NUM_BITS", ImageSamplerBits::filter_num_bits)
    // minification mipmap mode enums
    .add_macro_u32("ASTRAL_MIPMAP_NONE", mipmap_none)
    .add_macro_u32("ASTRAL_MIPMAP_CEILING", mipmap_ceiling)
    .add_macro_u32("ASTRAL_MIPMAP_FLOOR", mipmap_floor)
    .add_macro_u32("ASTRAL_MIPMAP_CHOSEN", mipmap_chosen)
    .add_macro_u32("ASTRAL_MIPMAP_BIT0", ImageSamplerBits::mipmap_bit0)
    .add_macro_u32("ASTRAL_MIPMAP_NUM_BITS", ImageSamplerBits::mipmap_num_bits)
    // unpacking max-LOD
    .add_macro_u32("ASTRAL_MAX_LOD_BIT0", ImageSamplerBits::maximum_lod_bit0)
    .add_macro_u32("ASTRAL_MAX_LOD_NUM_BITS", ImageSamplerBits::maximum_lod_num_bits)
    // use texel padding for ImageSampler
    .add_macro_u32("ASTRAL_NUMBER_PRE_PADDING_TEXELS_BIT0", ImageSamplerBits::numbers_texels_pre_padding_bit0)
    .add_macro_u32("ASTRAL_NUMBER_PRE_PADDING_TEXELS_NUM_BITS", ImageSamplerBits::numbers_texels_pre_padding_num_bits)
    // packing for extracting colorspace of a brush
    .add_macro_u32("ASTRAL_PACKED_BRUSH_COLORSPACE_SPECIFIED_MASK", ASTRAL_BIT_MASK(Packing::brush_colorspace_specified_bit))
    .add_macro_u32("ASTRAL_PACKED_BRUSH_COLORSPACE_BIT", Packing::brush_colorspace_bit)
    // unpack gradient type and spread
    .add_macro_u32("ASTRAL_GRADIENT_TYPE_BIT0", Packing::gradient_type_bit0)
    .add_macro_u32("ASTRAL_GRADIENT_TYPE_NUM_BITS", Packing::gradient_type_num_bits)
    .add_macro_u32("ASTRAL_GRADIENT_INTERPOLATE_TILE_MODE_BIT0", Packing::gradient_interpolate_tile_mode_bit0)
    .add_macro_u32("ASTRAL_GRADIENT_INTERPOLATE_TILE_MODE_NUM_BITS", Packing::gradient_interpolate_tile_mode_num_bits)
    .add_macro_u32("ASTRAL_GRADIENT_COLORSPACE_BIT0", Packing::gradient_colorspace_bit0)
    .add_macro_u32("ASTRAL_GRADIENT_COLORSPACE_NUM_BITS", Packing::gradient_colorspace_num_bits)
    .add_macro_u32("ASTRAL_GRADIENT_LINEAR", Gradient::linear)
    .add_macro_u32("ASTRAL_GRADIENT_SWEEP", Gradient::sweep)
    .add_macro_u32("ASTRAL_GRADIENT_RADIAL_UNEXTENDED_OPAQUE", Gradient::radial_unextended_opaque)
    .add_macro_u32("ASTRAL_GRADIENT_RADIAL_UNEXTENDED_CLEAR", Gradient::radial_unextended_clear)
    .add_macro_u32("ASTRAL_GRADIENT_RADIAL_EXTENDED", Gradient::radial_extended)
    // macro values for handling tiled images
    .add_macro_u32("ASTRAL_TILED_IMAGE_X_BIT0", ImageBacking::x_bit0)
    .add_macro_u32("ASTRAL_TILED_IMAGE_X_NUMBITS", ImageBacking::coord_num_bits)
    .add_macro_u32("ASTRAL_TILED_IMAGE_Y_BIT0", ImageBacking::y_bit0)
    .add_macro_u32("ASTRAL_TILED_IMAGE_Y_NUMBITS", ImageBacking::coord_num_bits)
    .add_macro_u32("ASTRAL_TILED_IMAGE_LAYER_BIT0", ImageBacking::layer_bit0)
    .add_macro_u32("ASTRAL_TILED_IMAGE_LAYER_GENERIC_TILE_NUMBITS", ImageBacking::generic_tile_layer_num_bits)
    .add_macro_u32("ASTRAL_TILED_IMAGE_LAYER_ROOT_TILE_NUMBITS", ImageBacking::root_index_tile_layer_num_bits)
    .add_macro_u32("ASTRAL_TILED_IMAGE_NUM_LEVELS_BIT0", ImageBacking::root_index_tile_number_levels_bit0)
    .add_macro_u32("ASTRAL_TILED_IMAGE_NUM_LEVELS_NUMBITS", ImageBacking::root_index_tile_number_levels_num_bits)
    .add_macro_u32("ASTRAL_LOG2_TILE_SIZE", ImageAtlas::log2_tile_size)
    .add_macro_u32("ASTRAL_TILE_SIZE", ImageAtlas::tile_size)
    .add_macro_u32("ASTRAL_TILE_PADDING", ImageAtlas::tile_padding)
    .add_macro_u32("ASTRAL_TILED_IMAGE_MIP_X_HIGH_BITS_BIT0", Packing::image_root_high_x_bit0)
    .add_macro_u32("ASTRAL_TILED_IMAGE_MIP_Y_HIGH_BITS_BIT0", Packing::image_root_high_y_bit0)
    .add_macro_u32("ASTRAL_TILED_IMAGE_MIP_NUM_LEVELS_BIT0", Packing::image_root_num_index_levels_bit0)
    .add_macro_u32("ASTRAL_TILED_IMAGE_MIP_NUM_HIGH_NUMBITS", Packing::image_root_num_high_bits)
    .add_macro_u32("ASTRAL_TILED_IMAGE_MIP_NUM_LEVELS_NUMBITS", Packing::image_root_num_index_levels_bits)
    .add_macro_u32("ASTRAL_PACKED_TILED_IMAGE_MIP_ROOT_TILE_Z_BIT0", PackedImageMipElement::root_tile_z_bit0)
    .add_macro_u32("ASTRAL_PACKED_TILED_IMAGE_MIP_ROOT_TILE_Z_NUMBITS", PackedImageMipElement::root_tile_z_num_bits)
    .add_macro_u32("ASTRAL_PACKED_TILED_IMAGE_MIP_ROOT_TILE_NUM_LEVELS_BIT0", PackedImageMipElement::number_index_levels_bit0)
    .add_macro_u32("ASTRAL_PACKED_TILED_IMAGE_MIP_ROOT_TILE_NUM_LEVELS_NUMBITS", PackedImageMipElement::number_index_levels_num_bits)
    .add_macro("astral_image_color_atlas_inverse_size", 1.0f / static_cast<float>(m_config.m_image_color_atlas_width_height))
    // macros for bits of clip-mask
    .add_macro_u32("ASTRAL_CLIP_MASK_LAYER_BIT0", Packing::ProcessedRenderClipElement::layer_bit0)
    .add_macro_u32("ASTRAL_CLIP_MASK_LAYER_NUM_BITS", Packing::ProcessedRenderClipElement::layer_num_bits)
    .add_macro_u32("ASTRAL_CLIP_MASK_NUM_LEVELS_BIT0", Packing::ProcessedRenderClipElement::num_index_levels_bit0)
    .add_macro_u32("ASTRAL_CLIP_MASK_NUM_LEVELS_NUM_BITS", Packing::ProcessedRenderClipElement::num_index_levels_bits)
    .add_macro_u32("ASTRAL_CLIP_MASK_CHANNEL_BIT0", Packing::ProcessedRenderClipElement::mask_channel_bit0)
    .add_macro_u32("ASTRAL_CLIP_MASK_CHANNEL_NUM_BITS", Packing::ProcessedRenderClipElement::mask_channel_num_bits)
    .add_macro_u32("ASTRAL_CLIP_MASK_TYPE_BIT0", Packing::ProcessedRenderClipElement::mask_type_bit0)
    .add_macro_u32("ASTRAL_CLIP_MASK_TYPE_NUM_BITS", Packing::ProcessedRenderClipElement::mask_type_num_bits)
    .add_macro_u32("ASTRAL_CLIP_MASK_FILTER_BIT0", Packing::ProcessedRenderClipElement::filter_bit0)
    .add_macro_u32("ASTRAL_CLIP_MASK_FILTER_NUM_BITS", Packing::ProcessedRenderClipElement::filter_num_bits)
    .add_macro_u32("ASTRAL_CLIP_MASK_CLIP_OUT_MASK", 1u << Packing::ProcessedRenderClipElement::clip_out_bit)
    // macros from GlyphShader::flags_t
    .add_macro_u32("ASTRAL_GLYPH_SHADER_IS_COLORED_GLYPH", GlyphShader::is_colored_glyph);

  for (unsigned int bit = 0; bit < 32u; ++bit)
    {
      std::ostringstream label;

      label << "ASTRAL_BIT_MASK" << bit;
      dst.add_macro_u32(label.str().c_str(), ASTRAL_BIT_MASK(bit));
    }

  dst
    .add_macro_u32("ASTRAL_X_COMPONENT_FP16_SIGN_BIT_MASK", ASTRAL_BIT_MASK(15u))
    .add_macro_u32("ASTRAL_Y_COMPONENT_FP16_SIGN_BIT_MASK", ASTRAL_BIT_MASK(31u))
    .add_macro_u32("ASTRAL_FP16_SIGN_BIT_MASK", ASTRAL_BIT_MASK(15u) | ASTRAL_BIT_MASK(31u));

  /* macros for the size of a color texel in a root index tile;
   * The purpose of having the macros is that since the number of
   * levels is so small (0, 1, 2, 3), the functin to compute the
   * ratio can do conditional assign instead of the computation
   * which invokes a divide.
   */
  for (unsigned int number_levels = 0; number_levels <= ImageBacking::max_number_levels; ++number_levels)
    {
      std::ostringstream macro_name_u, macro_name_f;
      generic_data value;

      macro_name_u << "ASTRAL_COLOR_TEXEL_SIZE_IN_ROOT_TILE" << number_levels << "_RAW_BITS";
      macro_name_f << "ASTRAL_COLOR_TEXEL_SIZE_IN_ROOT_TILE" << number_levels;

      value.f = 1.0f / static_cast<float>(ImageMipElement::compute_ratio(number_levels));

      dst.add_macro_u32(macro_name_u.str().c_str(), value.u);
      dst.add_macro(macro_name_f.str().c_str(), value.f);
    }

  if (m_config.m_use_hw_clip_window)
    {
      dst.add_macro("ASTRAL_USE_HW_CLIP_PLANES");
    }

  /* vertex streaming surface props */
  uint32_t x_mask, y_shift;
  y_shift = m_config.m_log2_gpu_stream_surface_width;
  x_mask = (1u << y_shift) - 1u;

  dst
    .add_macro("ASTRAL_GPU_VERTEX_STREAMING")
    .add_macro_u32("ASTRAL_GPU_VERTEX_SURFACE_X_MASK", x_mask)
    .add_macro_u32("ASTRAL_GPU_VERTEX_SURFACE_Y_SHIFT", y_shift);

  if (m_config.m_static_data_layout == linear_array)
    {
      dst.add_macro("ASTRAL_STATIC_DATA_TBO");
    }
  else
    {
      dst
        .add_macro("ASTRAL_STATIC_DATA_TEXTURE_2D")
        .add_macro_u32("ASTRAL_SHARED_DATA_X_MASK", ASTRAL_MASK(0u, m_config.m_static_data_log2_width))
        .add_macro_u32("ASTRAL_SHARED_DATA_Y_MASK", ASTRAL_MASK(0u, m_config.m_static_data_log2_height))
        .add_macro_u32("ASTRAL_SHARED_DATA_Y_SHIFT", m_config.m_static_data_log2_width)
        .add_macro_u32("ASTRAL_SHARED_DATA_Z_SHIFT", m_config.m_static_data_log2_width + m_config.m_static_data_log2_height);
    }

  if (m_config.m_vertex_buffer_layout == linear_array)
    {
      dst
        .add_macro("ASTRAL_VERTEX_BACKING_TBO");
    }
  else
    {
      dst
        .add_macro("ASTRAL_VERTEX_BACKING_TEXTURE_2D_ARRAY")
        .add_macro_u32("ASTRAL_VERTEX_BACKING_X_MASK", ASTRAL_MASK(0u, m_config.m_vertex_buffer_log2_width))
        .add_macro_u32("ASTRAL_VERTEX_BACKING_Y_MASK", ASTRAL_MASK(0u, m_config.m_vertex_buffer_log2_height))
        .add_macro_u32("ASTRAL_VERTEX_BACKING_Y_SHIFT", m_config.m_vertex_buffer_log2_width)
        .add_macro_u32("ASTRAL_VERTEX_BACKING_Z_SHIFT", m_config.m_vertex_buffer_log2_width + m_config.m_vertex_buffer_log2_height);
    }

  if (m_config.m_use_glsl_unpack_fp16)
    {
      dst.add_macro("ASTRAL_GLSL_HAS_UNPACK_HALF_2x16");
      if (!ContextProperties::is_es())
        {
          dst.specify_extension("GL_ARB_shading_language_packing", ShaderSource::enable_extension);
        }
    }

  /* macros describing shadow mapping */
  float max_unnormalized_depth_value(1u << 22u);
  dst
    .add_macro("ASTRAL_SHADOW_MAP_MAX_DEPTH_VALUE", max_unnormalized_depth_value)
    .add_macro("ASTRAL_SHADOW_MAP_RECIRPOCAL_MAX_DEPTH_VALUE", 1.0f / max_unnormalized_depth_value)
    .add_macro("ASTRAL_SHADOW_MAP_NORMALIZED_DEPTH_VALUE");

  dst
    .specify_version(version)
    .add_source("\nvoid astral_do_nothing(void) {}\n", ShaderSource::from_string)
    .add_source("astral_unpackHalf2x16.glsl.resource_string", ShaderSource::from_resource)
    .add_source("astral_unpack.glsl.resource_string", ShaderSource::from_resource)
    .add_source("astral_types_bo.glsl.resource_string", ShaderSource::from_resource)
    .add_source("astral_uniforms_common.glsl.resource_string", ShaderSource::from_resource);

  if (!m_config.m_use_texture_for_uniform_buffer)
    {
      Packing::emit_unpack_code_ubo(dst);
    }
  else
    {
      Packing::emit_unpack_code_texture(dst);
    }

  dst
    .add_source("astral_utils.glsl.resource_string", ShaderSource::from_resource)
    .add_source("astral_compute_shadow_map_depth.glsl.resource_string", ShaderSource::from_resource);

  m_base_lib = ShaderLibrary::create(dst);
}

astral::reference_counted_ptr<astral::gl::Program>
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
create_program(const ShaderSource &vert,
               const ShaderSource &frag)
{
  PreLinkActionArray prelink_actions;
  ProgramInitializerArray uniform_initers;
  reference_counted_ptr<Program> pr;

  if (!m_config.m_use_attributes)
    {
      prelink_actions
        .add_binding("astral_vertex_id", 0);
    }

  uniform_initers
    .add_uniform_block_binding("AstralPackedHeadersUBO", data_binding_point_index(data_header))
    .add_uniform_block_binding("AstralTransformationsUBO", data_binding_point_index(data_item_transformation))
    .add_uniform_block_binding("AstralScaleTranslatesUBO", data_binding_point_index(data_item_scale_translate))
    .add_uniform_block_binding("AstralClipWindowUBO", data_binding_point_index(data_clip_window))
    .add_uniform_block_binding("AstralPackedBrushesUBO", data_binding_point_index(data_brush))
    .add_uniform_block_binding("AstralPackedGradientsUBO", data_binding_point_index(data_gradient))
    .add_uniform_block_binding("AstralGradientTransformationsUBO", data_binding_point_index(data_gradient_transformation))
    .add_uniform_block_binding("AstralItemDataUBO", data_binding_point_index(data_item_data))
    .add_uniform_block_binding("AstralPackedImagesUBO", data_binding_point_index(data_image))
    .add_uniform_block_binding("AstralShadowMapUBO", data_binding_point_index(data_shadow_map))
    .add_uniform_block_binding("AstralClipElementsUBO", data_binding_point_index(data_clip_mask))
    .add_uniform_block_binding("AstralMiscUBO", misc_data_binding_point_index())
    .add_sampler_initializer("astral_colorstop_atlas", colorstop_atlas_binding_point_index)
    .add_sampler_initializer("astral_static_data32", static_data32_texture_binding_point_index)
    .add_sampler_initializer("astral_static_data16", static_data16_texture_binding_point_index)
    .add_sampler_initializer("astral_vertex_backing", vertex_backing_texture_binding_point_index)
    .add_sampler_initializer("astral_vertex_surface", vertex_surface_texture_binding_point_index)
    .add_sampler_initializer("astral_image_color_atlas", color_tile_image_atlas_binding_point_index)
    .add_sampler_initializer("astral_image_index_atlas", index_tile_image_atlas_binding_point_index)
    .add_sampler_initializer("astral_shadow_map_atlas", shadow_map_atlas_binding_point_index)
    .add_sampler_initializer("astral_data_texture", data_buffer_texture_binding_point_index)
    .add_uniform_block_binding("AstralDataTextureOffsetUBO", data_texture_offset_ubo_binding_point_index());

  pr = Program::create(vert, frag, prelink_actions, uniform_initers);
  if (m_config.m_force_shader_log_generation_before_use)
    {
      pr->generate_logs();
    }

  return pr;
}

astral::gl::Program*
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
gl_program(const ItemShader &shader,
           const MaterialShader *material,
           BackendBlendMode mode,
           enum clip_window_value_type_t shader_clipping)
{
  const ItemShaderBackendGL3::Implement *gl_shader;
  const MaterialShaderGL3::Implement *gl_material;
  unsigned int IDX, material_IDX;
  astral::gl::Program *return_value;
  enum ItemShader::type_t shader_type(shader.type());

  ASTRALassert(shader.type() == mode.item_shader_type());

  ASTRALassert(dynamic_cast<const ItemShaderBackendGL3::Implement*>(&shader.backend()));
  gl_shader = static_cast<const ItemShaderBackendGL3::Implement*>(&shader.backend());
  IDX = gl_shader->shader_builder_index(detail::ShaderIndexArgument());

  /* material shader is not present in mask shading */
  ASTRALassert((material != nullptr) == (shader.type() == ItemShader::color_item_shader));
  ASTRALassert(!material || dynamic_cast<const MaterialShaderGL3::Implement*>(&material->root()));

  gl_material = material ? static_cast<const MaterialShaderGL3::Implement*>(&material->root()) : nullptr;
  material_IDX = gl_material ?
    gl_material->shader_builder_index(detail::ShaderIndexArgument()) :
    0;

  /* only null-material gets material_IDX as 0 */
  ASTRALassert((material_IDX == 0) == (gl_material == nullptr));

  if (IDX >= m_non_uber_programs[shader_type].size())
    {
      m_non_uber_programs[shader_type].resize(IDX + 1);
    }

  if (material_IDX >= m_non_uber_programs[shader_type][IDX].size())
    {
      m_non_uber_programs[shader_type][IDX].resize(material_IDX + 1u);
    }

  if (!m_non_uber_programs[shader_type][IDX][material_IDX].program(shader_clipping, mode))
    {
      unsigned int epilogue_choice(m_blend_epilogue_chooser[mode.packed_value()]);
      const CommonBlendEpilogue &epilogue(m_blend_epilogue[epilogue_choice]);
      SourceBuilder src_builder(*this);
      ShaderSource vert, frag;
      reference_counted_ptr<Program> pr;
      bool requires_framebuffer_pixels;

      requires_framebuffer_pixels = epilogue.m_requires_framebuffer_pixels
        || (gl_material && gl_material->properties().m_uses_framebuffer_pixels);

      src_builder.create_item_shader_src(&vert, &frag, *gl_shader, gl_material,
                                         shader_type,
                                         epilogue.m_shader_epilogue,
                                         requires_framebuffer_pixels,
                                         shader_clipping);
      pr = create_program(vert, frag);
      for (BackendBlendMode e : epilogue.m_elements)
        {
          m_non_uber_programs[shader_type][IDX][material_IDX].program(shader_clipping, e, pr);
        }

      //std::cout << "RendererAstral: create non-uber-shader: " << pr->name()
      //    << ", type = " << gl_shader->type()
      //    << "[should be same as " << shader.type() << "], material = "
      //    << gl_material << "\n";
    }

  return_value = m_non_uber_programs[shader_type][IDX][material_IDX].program(shader_clipping, mode).get();

  return return_value;
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
create_misc_gl3_shaders(ShaderSetGL3 *out_shaders)
{
  /* MAYBE:
   *  - Merge m_glyph_shader.m_scalable_shader and m_glyph_shader.m_image_shader and they become subshaders??
   */

  /* TODO: Color glyph shading currently only work with porter_duff_src_over
   *       because the shaders emit coverage as 1.0 always. We need a way
   *       to compute what the partial coverage for the scalable color glyphs
   *       shader should be; the image glyphs are even tricker because
   *       those do not have any notion of coverage as they are just images.
   */

  ShaderSource::MacroSet glyph_macros;

  glyph_macros
    .add_macro_u32("ASTRAL_SUB_SHADER_COLOR_GLYPHS_IGNORE_MATERIAL_RGB", ShaderSetGL3::glyph_sub_shader_preserve_color_glyphs)
    .add_macro_u32("ASTRAL_SUB_SHADER_COLOR_GLYPHS_OBSERVE_MATERIAL_RGB", ShaderSetGL3::glyph_sub_shader_color_glyphs_obey_material)
    .add_macro_u32("ASTRAL_GLYPH_MAX_X_MASK", RectEnums::maxx_mask)
    .add_macro_u32("ASTRAL_GLYPH_MAX_Y_MASK", RectEnums::maxy_mask);

  out_shaders->m_scalable_glyph_shader =
    ItemShaderBackendGL3::create(m_engine, ItemShader::color_item_shader,
                                 ShaderSource()
                                 .add_macros(glyph_macros)
                                 .add_source("astral_glyph.vert.glsl.resource_string", ShaderSource::from_resource)
                                 .remove_macros(glyph_macros),
                                 ShaderSource()
                                 .add_library(m_shader_libs.m_item_path_lib)
                                 .add_macros(glyph_macros)
                                 .add_source("astral_glyph.frag.glsl.resource_string", ShaderSource::from_resource)
                                 .remove_macros(glyph_macros),
                                 ShaderVaryings()
                                 .add_varying("astral_glyph_data", ShaderVaryings::interpolator_uint)
                                 .add_varying("astral_glyph_flags", ShaderVaryings::interpolator_uint)
                                 .add_varying("astral_glyph_widen", ShaderVaryings::interpolator_flat)
                                 .add_varying("astral_glyph_x", ShaderVaryings::interpolator_smooth)
                                 .add_varying("astral_glyph_y", ShaderVaryings::interpolator_smooth),
                                 ShaderSetGL3::glyph_sub_shader_count);

  out_shaders->m_image_glyph_shader =
    ItemShaderBackendGL3::create(m_engine, ItemShader::color_item_shader,
                                 ShaderSource()
                                 .add_macros(glyph_macros)
                                 .add_source("astral_image_glyph.vert.glsl.resource_string", ShaderSource::from_resource)
                                 .remove_macros(glyph_macros),
                                 ShaderSource()
                                 .add_macros(glyph_macros)
                                 .add_source("astral_image_glyph.frag.glsl.resource_string", ShaderSource::from_resource)
                                 .remove_macros(glyph_macros),
                                 ShaderVaryings()
                                 .add_varying("astral_glyph_packed_image0_x", ShaderVaryings::interpolator_uint)
                                 .add_varying("astral_glyph_packed_image0_y", ShaderVaryings::interpolator_uint)
                                 .add_varying("astral_glyph_packed_image0_z", ShaderVaryings::interpolator_uint)
                                 .add_varying("astral_glyph_packed_image0_w", ShaderVaryings::interpolator_uint)
                                 .add_varying("astral_glyph_packed_image1_x", ShaderVaryings::interpolator_uint)
                                 .add_varying("astral_glyph_packed_image1_y", ShaderVaryings::interpolator_uint)
                                 .add_varying("astral_glyph_packed_image1_z", ShaderVaryings::interpolator_uint)
                                 .add_varying("astral_glyph_packed_image1_w", ShaderVaryings::interpolator_uint)
                                 .add_varying("astral_glyph_flags", ShaderVaryings::interpolator_uint)
                                 .add_varying("astral_glyph_x", ShaderVaryings::interpolator_smooth)
                                 .add_varying("astral_glyph_y", ShaderVaryings::interpolator_smooth),
                                 ShaderSetGL3::glyph_sub_shader_count);
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
create_misc_shaders(ShaderSet *out_shaders)
{
  ShaderSource::MacroSet blit_mask_tile_shader_macros;
  enum blit_mask_tile_shader_internal_bits
    {
     /* these values are used internally by the blit-mask shader code to pass
      * bits from vertex shader to fragment shader
      */
      ASTRAL_BLIT_MASK_TILE_PACK_ATLAS_Z_BIT0 = 0u,
      ASTRAL_BLIT_MASK_TILE_PACK_MASK_VALUE_TYPE_BIT0 = ASTRAL_BLIT_MASK_TILE_PACK_ATLAS_Z_BIT0 + BlitMaskTileShader::tile_layer_num_bits,
      ASTRAL_BLIT_MASK_TILE_PACK_MASK_CHANNEL_BIT0 = ASTRAL_BLIT_MASK_TILE_PACK_MASK_VALUE_TYPE_BIT0 + 1u,
      ASTRAL_BLIT_MASK_TILE_PACK_OPTIONAL_MASK_CHANNEL_BIT0 = ASTRAL_BLIT_MASK_TILE_PACK_MASK_CHANNEL_BIT0 + ImageSamplerBits::mask_channel_num_bits,
      ASTRAL_BLIT_MASK_TILE_PACK_FILTER_BIT0 = ASTRAL_BLIT_MASK_TILE_PACK_OPTIONAL_MASK_CHANNEL_BIT0 + ImageSamplerBits::mask_channel_num_bits,
    };

  blit_mask_tile_shader_macros
    .add_macro_u32("ASTRAL_BLIT_MASK_TILE_MASK_DETAILS_VARIANT", BlitMaskTileShader::mask_details_variant)
    .add_macro_u32("ASTRAL_BLIT_MASK_TILE_CLIP_COMBINE_VARIANT", BlitMaskTileShader::clip_combine_variant)

    .add_macro_u32("ASTRAL_BLIT_MASK_TILE_LAYER_NUM_BITS", BlitMaskTileShader::tile_layer_num_bits)
    .add_macro_u32("ASTRAL_BLIT_MASK_TILE_LAYER_BIT0", BlitMaskTileShader::tile_layer_bit0)

    .add_macro_u32("ASTRAL_BLIT_MASK_TILE_PADDING_NUM_BITS", BlitMaskTileShader::tile_padding_num_bits)
    .add_macro_u32("ASTRAL_BLIT_MASK_TILE_PADDING_BIT0", BlitMaskTileShader::tile_padding_bit0)

    .add_macro_u32("ASTRAL_BLIT_MASK_TILE_MASK_VALUE_TYPE_BIT0", BlitMaskTileShader::mask_type_bit0)
    .add_macro_u32("ASTRAL_BLIT_MASK_TILE_MASK_CHANNEL_BIT0", BlitMaskTileShader::mask_channel_bit0)

    .add_macro_u32("ASTRAL_BLIT_MASK_TILE_OPTIONAL_MASK_CHANNEL_BIT0", BlitMaskTileShader::optional_mask_channel_bit0)
    .add_macro_u32("ASTRAL_BLIT_MASK_TILE_FILTER_BIT0", BlitMaskTileShader::filter_bit0)

    .add_macro_u32("ASTRAL_BLIT_MASK_TILE_PACK_ATLAS_Z_BIT0", ASTRAL_BLIT_MASK_TILE_PACK_ATLAS_Z_BIT0)
    .add_macro_u32("ASTRAL_BLIT_MASK_TILE_PACK_MASK_VALUE_TYPE_BIT0", ASTRAL_BLIT_MASK_TILE_PACK_MASK_VALUE_TYPE_BIT0)
    .add_macro_u32("ASTRAL_BLIT_MASK_TILE_PACK_MASK_CHANNEL_BIT0", ASTRAL_BLIT_MASK_TILE_PACK_MASK_CHANNEL_BIT0)
    .add_macro_u32("ASTRAL_BLIT_MASK_TILE_PACK_OPTIONAL_MASK_CHANNEL_BIT0", ASTRAL_BLIT_MASK_TILE_PACK_OPTIONAL_MASK_CHANNEL_BIT0)
    .add_macro_u32("ASTRAL_BLIT_MASK_TILE_PACK_FILTER_BIT0", ASTRAL_BLIT_MASK_TILE_PACK_FILTER_BIT0);

  reference_counted_ptr<const MaterialShaderGL3> blit_mask_tile_shader;

  blit_mask_tile_shader =
    MaterialShaderGL3::create(m_engine,
                              ShaderSource()
                              .add_macros(blit_mask_tile_shader_macros)
                              .add_library(m_shader_libs.m_image_lib)
                              .add_source("astral_blit_mask_tile.vert.glsl.resource_string", ShaderSource::from_resource)
                              .remove_macros(blit_mask_tile_shader_macros),
                              ShaderSource()
                              .add_library(m_shader_libs.m_image_lib)
                              .add_macros(blit_mask_tile_shader_macros)
                              .add_source("astral_blit_mask_tile.frag.glsl.resource_string", ShaderSource::from_resource)
                              .remove_macros(blit_mask_tile_shader_macros),
                              ShaderVaryings()
                              .add_varying("astral_clip_in_texel_x", ShaderVaryings::interpolator_smooth)
                              .add_varying("astral_clip_in_texel_y", ShaderVaryings::interpolator_smooth)
                              .add_varying("astral_clip_in_image_x", ShaderVaryings::interpolator_uint)
                              .add_varying("astral_clip_in_image_y", ShaderVaryings::interpolator_uint)
                              .add_varying("astral_clip_in_image_z", ShaderVaryings::interpolator_uint)
                              .add_varying("astral_clip_in_image_w", ShaderVaryings::interpolator_uint)
                              .add_varying("astral_clip_out_texel_x", ShaderVaryings::interpolator_smooth)
                              .add_varying("astral_clip_out_texel_y", ShaderVaryings::interpolator_smooth)
                              .add_varying("astral_clip_out_image_x", ShaderVaryings::interpolator_uint)
                              .add_varying("astral_clip_out_image_y", ShaderVaryings::interpolator_uint)
                              .add_varying("astral_clip_out_image_z", ShaderVaryings::interpolator_uint)
                              .add_varying("astral_clip_out_image_w", ShaderVaryings::interpolator_uint)
                              .add_varying("astral_mask_texel_x", ShaderVaryings::interpolator_smooth)
                              .add_varying("astral_mask_texel_y", ShaderVaryings::interpolator_smooth)
                              .add_varying("astral_mask_texel_z_xfer_filter", ShaderVaryings::interpolator_uint),
                              MaterialShader::Properties(), 2u);

  vecN<reference_counted_ptr<const MaterialShader>, 2> blit_mask_tile_sub_shaders;

  blit_mask_tile_sub_shaders[BlitMaskTileShader::mask_details_variant]
    = ASTRALnew MaterialShader(*blit_mask_tile_shader,
                               BlitMaskTileShader::mask_details_variant,
                               MaterialShader::Properties()
                               .reduces_coverage(false)
                               .emits_transparent_fragments(true));

  blit_mask_tile_sub_shaders[BlitMaskTileShader::clip_combine_variant]
    = ASTRALnew MaterialShader(*blit_mask_tile_shader,
                               BlitMaskTileShader::clip_combine_variant,
                               MaterialShader::Properties()
                               .reduces_coverage(true)
                               .emits_transparent_fragments(true));

  out_shaders->m_blit_mask_tile_shader = blit_mask_tile_sub_shaders;
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
create_rect_shaders(ShaderSet *out_shaders)
{
  reference_counted_ptr<const ItemShaderBackendGL3> rect_shader;
  ShaderSource::MacroSet rect_shader_macros;
  uint32_t number_dynamic_rect_sub_shaders;

  number_dynamic_rect_sub_shaders = ASTRAL_MAX_VALUE_FROM_NUM_BITS(ShaderSet::RectSideAAList::number_bits_used_in_last_element) + 1u;

  rect_shader_macros
    .add_macro_u32("ASTRAL_MASKED_RECT_SHADER_SAMPLING_BIT0", MaskedRectShader::sampling_bits_bit0)
    .add_macro_u32("ASTRAL_MASKED_RECT_SHADER_SAMPLING_NUM_BITS", MaskedRectShader::sampling_bits_num_bits)
    .add_macro_u32("ASTRAL_MASKED_RECT_SHADER_Z_BIT0", MaskedRectShader::tile_z_bit0)
    .add_macro_u32("ASTRAL_MASKED_RECT_SHADER_Z_NUM_BITS", MaskedRectShader::tile_z_num_bits)
    .add_macro_u32("ASTRAL_MASKED_RECT_SHADER_PADDING_BIT0", MaskedRectShader::tile_padding_bit0)
    .add_macro_u32("ASTRAL_MASKED_RECT_SHADER_PADDING_NUM_BITS", MaskedRectShader::tile_padding_num_bits)
    .add_macro_u32("ASTRAL_MASKED_RECT_SHADER", number_dynamic_rect_sub_shaders)
    .add_macro_u32("ASTRAL_MINY_AA", ShaderSet::RectSideAAList().value(RectEnums::miny_side, true).m_backing[0])
    .add_macro_u32("ASTRAL_MAXX_AA", ShaderSet::RectSideAAList().value(RectEnums::maxx_side, true).m_backing[0])
    .add_macro_u32("ASTRAL_MAXY_AA", ShaderSet::RectSideAAList().value(RectEnums::maxy_side, true).m_backing[0])
    .add_macro_u32("ASTRAL_MINX_AA", ShaderSet::RectSideAAList().value(RectEnums::minx_side, true).m_backing[0]);

  rect_shader =
    ItemShaderBackendGL3::create(m_engine,
                                 ItemShader::color_item_shader,
                                 ShaderSource()
                                 .add_macros(rect_shader_macros)
                                 .add_source("astral_rect_shader.vert.glsl.resource_string", ShaderSource::from_resource)
                                 .remove_macros(rect_shader_macros),
                                 ShaderSource()
                                 .add_library(m_shader_libs.m_image_lib)
                                 .add_macros(rect_shader_macros)
                                 .add_source("astral_rect_shader.frag.glsl.resource_string", ShaderSource::from_resource)
                                 .remove_macros(rect_shader_macros),
                                 ShaderVaryings()
                                 .add_varying("astral_rect_texel_x", ShaderVaryings::interpolator_smooth)
                                 .add_varying("astral_rect_texel_y", ShaderVaryings::interpolator_smooth)
                                 .add_varying("astral_mask_sampling_and_layer", ShaderVaryings::interpolator_uint),
                                 number_dynamic_rect_sub_shaders + 1u);

  out_shaders->m_masked_rect_shader =
    ColorItemShader::create(*rect_shader, number_dynamic_rect_sub_shaders,
                            ColorItemShader::Properties().emits_partially_covered_fragments(true));

  for (unsigned int mask = 0; mask < number_dynamic_rect_sub_shaders; ++mask)
    {
      ShaderSet::RectSideAAList v;

      v.m_backing[0] = mask;
      out_shaders->dynamic_rect_shader(v) =
        ColorItemShader::create(*rect_shader, mask,
                                ColorItemShader::Properties()
                                .emits_partially_covered_fragments(mask != 0u));
    }

  ShaderSet::RectSideAAList all_sides;

  all_sides
    .value(RectEnums::miny_side, true)
    .value(RectEnums::maxx_side, true)
    .value(RectEnums::maxy_side, true)
    .value(RectEnums::minx_side, true);

  out_shaders->m_dynamic_rect_aa_shader = out_shaders->dynamic_rect_shader(all_sides);
  out_shaders->m_dynamic_rect_shader = out_shaders->dynamic_rect_shader(ShaderSet::RectSideAAList());
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
create_fill_stc_shader(FillSTCShader *dst)
{
  dst->m_shaders[FillSTCShader::pass_contour_stencil] =
    create_mask_shader(m_engine,
                       ShaderSource()
                       .add_source("astral_stc_line.vert.glsl.resource_string", ShaderSource::from_resource),
                       ShaderSource()
                       .add_source("astral_stc_line.frag.glsl.resource_string", ShaderSource::from_resource),
                       ShaderVaryings());

  dst->m_shaders[FillSTCShader::pass_conic_triangles_stencil] =
    create_mask_shader(m_engine,
                       ShaderSource()
                       .add_source("astral_stc_quad_tri.vert.glsl.resource_string", ShaderSource::from_resource),
                       ShaderSource()
                       .add_source("astral_stc_quad_tri.frag.glsl.resource_string", ShaderSource::from_resource),
                       ShaderVaryings()
                       .add_varying("astral_stc_quad_tri_tex_x", ShaderVaryings::interpolator_smooth)
                       .add_varying("astral_stc_quad_tri_tex_y", ShaderVaryings::interpolator_smooth));

  ShaderSource::MacroSet fuzz_shader_macros;

  fuzz_shader_macros
    .add_macro_float("ASTRAL_STC_MAX_DISTANCE", ASTRAL_SQRT2);

  dst->m_shaders[FillSTCShader::pass_contour_fuzz] =
    create_mask_shader(m_engine,
                       ShaderSource()
                       .add_macros(fuzz_shader_macros)
                       .add_source("astral_stc_line_fuzz.vert.glsl.resource_string", ShaderSource::from_resource)
                       .remove_macros(fuzz_shader_macros),
                       ShaderSource()
                       .add_macros(fuzz_shader_macros)
                       .add_source("astral_stc_line_fuzz.frag.glsl.resource_string", ShaderSource::from_resource)
                       .remove_macros(fuzz_shader_macros),
                       ShaderVaryings()
                       .add_varying("astral_stc_contour_fuzz_aa_perp", ShaderVaryings::interpolator_smooth));

  dst->m_shaders[FillSTCShader::pass_conic_triangle_fuzz] =
    create_mask_shader(m_engine,
                       ShaderSource()
                       .add_macros(fuzz_shader_macros)
                       .add_source("astral_stc_quad_tri_util.glsl.resource_string", ShaderSource::from_resource)
                       .add_source("astral_stc_quad_tri_fuzz.vert.glsl.resource_string", ShaderSource::from_resource)
                       .remove_macros(fuzz_shader_macros),
                       ShaderSource()
                       .add_macros(fuzz_shader_macros)
                       .add_source("astral_stc_quad_tri_util.glsl.resource_string", ShaderSource::from_resource)
                       .add_source("astral_stc_quad_tri_fuzz.frag.glsl.resource_string", ShaderSource::from_resource)
                       .remove_macros(fuzz_shader_macros),
                       ShaderVaryings()
                       .add_varying("astral_stc_quad_tri_fuzz_type", ShaderVaryings::interpolator_uint)
                       .add_varying("astral_stc_quad_tri_fuzz_tex_x", ShaderVaryings::interpolator_smooth)
                       .add_varying("astral_stc_quad_tri_fuzz_tex_y", ShaderVaryings::interpolator_smooth));

  dst->m_cover_shader =
    create_mask_shader(m_engine,
                       ShaderSource().add_source("astral_cover_rect.vert.glsl.resource_string", ShaderSource::from_resource),
                       ShaderSource().add_source("astral_cover_rect.frag.glsl.resource_string", ShaderSource::from_resource),
                       ShaderVaryings());
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
create_material_gl3_shaders(ShaderSetGL3 *out_shaders)
{
  //stadard brush */
    {
      ShaderSource::MacroSet brush_macros;
      ShaderSource vert_shader, frag_shader;
      ShaderVaryings varyings;
      MaterialShader::Properties props;

      const uint32_t ASTRAL_BRUSH_ACTIVE = (1u << ShaderSetGL3::brush_active_bit);
      const uint32_t ASTRAL_BRUSH_HAS_IMAGE = (1u << ShaderSetGL3::brush_has_image_bit);
      const uint32_t ASTRAL_BRUSH_HAS_GRADIENT = (1u << ShaderSetGL3::brush_has_gradient_bit);
      const uint32_t ASTRAL_BRUSH_HAS_GRADIENT_TRANSFORMATION = (1u << ShaderSetGL3::brush_has_gradient_transformation_bit);
      const uint32_t ASTRAL_BRUSH_SPECIFIED_COLORSPACE = (1u << ShaderSetGL3::brush_specified_colorspace_bit);

      brush_macros
        .add_macro_u32("ASTRAL_BRUSH_ACTIVE_BIT", ShaderSetGL3::brush_active_bit)
        .add_macro_u32("ASTRAL_BRUSH_HAS_IMAGE_BIT", ShaderSetGL3::brush_has_image_bit)
        .add_macro_u32("ASTRAL_BRUSH_HAS_GRADIENT_BIT", ShaderSetGL3::brush_has_gradient_bit)
        .add_macro_u32("ASTRAL_BRUSH_HAS_GRADIENT_TRANSFORMATION_BIT", ShaderSetGL3::brush_has_gradient_transformation_bit)
        .add_macro_u32("ASTRAL_BRUSH_COLORSPACE_BIT", ShaderSetGL3::brush_colorspace_bit)
        .add_macro_u32("ASTRAL_BRUSH_ACTIVE", ASTRAL_BRUSH_ACTIVE)
        .add_macro_u32("ASTRAL_BRUSH_HAS_IMAGE", ASTRAL_BRUSH_HAS_IMAGE)
        .add_macro_u32("ASTRAL_BRUSH_HAS_GRADIENT", ASTRAL_BRUSH_HAS_GRADIENT)
        .add_macro_u32("ASTRAL_BRUSH_HAS_GRADIENT_TRANSFORMATION", ASTRAL_BRUSH_HAS_GRADIENT_TRANSFORMATION)
        .add_macro_u32("ASTRAL_BRUSH_SPECIFIED_COLORSPACE", ASTRAL_BRUSH_SPECIFIED_COLORSPACE);

      vert_shader
        .add_macros(brush_macros)
        .add_source("astral_brush_bo.vert.glsl.resource_string", ShaderSource::from_resource)
        .remove_macros(brush_macros);

      frag_shader
        .add_library(m_shader_libs.m_image_lib)
        .add_library(m_shader_libs.m_gradient_lib)
        .add_macros(brush_macros)
        .add_source("astral_brush_bo.frag.glsl.resource_string", ShaderSource::from_resource)
        .remove_macros(brush_macros);

      varyings
        .add_varying("astral_brush_image_p_x", ShaderVaryings::interpolator_smooth)
        .add_varying("astral_brush_image_p_y", ShaderVaryings::interpolator_smooth)
        .add_varying("astral_brush_gradient_p_x", ShaderVaryings::interpolator_smooth)
        .add_varying("astral_brush_gradient_p_y", ShaderVaryings::interpolator_smooth)
        .add_varying("astral_brush_base_color_rg", ShaderVaryings::interpolator_uint)
        .add_varying("astral_brush_base_color_ba", ShaderVaryings::interpolator_uint)
        .add_varying("astral_brush_flags", ShaderVaryings::interpolator_uint)
        .add_varying("astral_brush_image0_x", ShaderVaryings::interpolator_uint)
        .add_varying("astral_brush_image0_y", ShaderVaryings::interpolator_uint)
        .add_varying("astral_brush_image0_z", ShaderVaryings::interpolator_uint)
        .add_varying("astral_brush_image0_w", ShaderVaryings::interpolator_uint)
        .add_varying("astral_brush_image1_x", ShaderVaryings::interpolator_uint)
        .add_varying("astral_brush_image1_y", ShaderVaryings::interpolator_uint)
        .add_varying("astral_brush_gradient0_x", ShaderVaryings::interpolator_uint)
        .add_varying("astral_brush_gradient0_y", ShaderVaryings::interpolator_uint)
        .add_varying("astral_brush_gradient0_z", ShaderVaryings::interpolator_uint)
        .add_varying("astral_brush_gradient0_w", ShaderVaryings::interpolator_uint)
        .add_varying("astral_brush_gradient1_x", ShaderVaryings::interpolator_uint)
        .add_varying("astral_brush_gradient1_y", ShaderVaryings::interpolator_uint)
        .add_varying("astral_brush_gradient1_z", ShaderVaryings::interpolator_uint)
        .add_varying("astral_brush_gradient1_w", ShaderVaryings::interpolator_uint)
        .add_varying("astral_brush_gradient_transformation_x", ShaderVaryings::interpolator_uint)
        .add_varying("astral_brush_gradient_transformation_y", ShaderVaryings::interpolator_uint)
        .add_varying("astral_brush_gradient_transformation_z", ShaderVaryings::interpolator_uint)
        .add_varying("astral_brush_gradient_transformation_w", ShaderVaryings::interpolator_uint);

      props
        .emits_transparent_fragments(true);

      out_shaders->m_brush_shader = MaterialShaderGL3::create(m_engine, vert_shader, frag_shader, varyings, props);
    }

  // lighting shader
    {
      ShaderSource vert_shader, frag_shader;
      ShaderVaryings varyings;
      ShaderSource::MacroSet light_macros;
      MaterialShader::Properties props;

      light_macros
        .add_macro_u32("ASTRAL_LIGHT_MAP_WITHOUT_AA", ShaderSetGL3::lighting_sub_shader_without_aa)
        .add_macro_u32("ASTRAL_LIGHT_MAP_WITH_AA4", ShaderSetGL3::lighting_sub_shader_aa4)
        .add_macro_u32("ASTRAL_LIGHT_MAP_WITH_AA8", ShaderSetGL3::lighting_sub_shader_aa8)
        .add_macro_u32("ASTRAL_LIGHT_MAP_WITH_AA16", ShaderSetGL3::lighting_sub_shader_aa16);

      vert_shader.add_source("astral_light_material.vert.glsl.resource_string", ShaderSource::from_resource);
      frag_shader
        .add_source("astral_light_util.glsl.resource_string", ShaderSource::from_resource)
        .add_macros(light_macros)
        .add_source("astral_light_material.frag.glsl.resource_string", ShaderSource::from_resource)
        .remove_macros(light_macros);

      varyings
        .add_varying("astral_light_color", ShaderVaryings::interpolator_uint)
        .add_varying("astral_light_shadow_color", ShaderVaryings::interpolator_uint)
        .add_varying("astral_light_shadow_map_atlas_location_x", ShaderVaryings::interpolator_flat)
        .add_varying("astral_light_shadow_map_atlas_location_y", ShaderVaryings::interpolator_flat)
        .add_varying("astral_light_directional_cos_thresh", ShaderVaryings::interpolator_flat)
        .add_varying("astral_light_z", ShaderVaryings::interpolator_flat)
        .add_varying("astral_light_shadow_map_size", ShaderVaryings::interpolator_flat)
        .add_varying("astral_light_vector_x", ShaderVaryings::interpolator_smooth)
        .add_varying("astral_light_vector_y", ShaderVaryings::interpolator_smooth)
        .add_varying("astral_light_vector_shadow_map_x", ShaderVaryings::interpolator_smooth)
        .add_varying("astral_light_vector_shadow_map_y", ShaderVaryings::interpolator_smooth)
        .add_varying("astral_light_material_dot", ShaderVaryings::interpolator_smooth)
        .add_varying("astral_light_shadow_fall_off", ShaderVaryings::interpolator_flat)
        .add_varying("astral_light_shadow_fall_off_length_sq", ShaderVaryings::interpolator_flat);

      props
        .emits_transparent_fragments(true);

      out_shaders->m_lighting_shader = MaterialShaderGL3::create(m_engine, vert_shader, frag_shader, varyings, props, ShaderSetGL3::lighting_sub_shader_count);
    }
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
create_stroke_gl3_shaders(ShaderSetGL3 *out_shaders)
{
  for (unsigned int s = 0; s < ShaderSetGL3::stroke_shader_type_count; ++s)
    {
      StrokeShaderBuilder::build_shaders(static_cast<enum ShaderSetGL3::stroke_shader_type_t>(s),
                                         m_shader_libs, m_engine,
                                         &out_shaders->m_mask_stroke_shaders[s]);

      out_shaders->m_direct_stroke_shaders[s] = out_shaders->m_mask_stroke_shaders[s].color_shader_from_mask_shader();
    }
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
create_shadow_generator_shaders(ShadowMapGeneratorShader *out_shaders)
{
  reference_counted_ptr<const ShadowMapItemShader> edge_generator, curve_generator;
  ShaderSource::MacroSet macros;
  const uint32_t ASTRAL_SHADOW_X_SIDE = 0u;
  const uint32_t ASTRAL_SHADOW_Y_SIDE = 1u;

  macros
    .add_macro_u32("ASTRAL_SHADOW_X_SIDE", ASTRAL_SHADOW_X_SIDE)
    .add_macro_u32("ASTRAL_SHADOW_Y_SIDE", ASTRAL_SHADOW_Y_SIDE);

  edge_generator = create_shadow_map_shader(m_engine,
                                            ShaderSource()
                                            .add_macros(macros)
                                            .add_source("astral_shadow_util.glsl.resource_string", ShaderSource::from_resource)
                                            .add_source("astral_edge_shadow.vert.glsl.resource_string", ShaderSource::from_resource)
                                            .remove_macros(macros),
                                            ShaderSource()
                                            .add_macros(macros)
                                            .add_source("astral_light_util.glsl.resource_string", ShaderSource::from_resource)
                                            .add_source("astral_edge_shadow.frag.glsl.resource_string", ShaderSource::from_resource)
                                            .remove_macros(macros),
                                            ShaderSymbolList()
                                            .add_varying("astral_edge_p_x", ShaderVaryings::interpolator_flat)
                                            .add_varying("astral_edge_p_y", ShaderVaryings::interpolator_flat)
                                            .add_varying("astral_edge_q_x", ShaderVaryings::interpolator_flat)
                                            .add_varying("astral_edge_q_y", ShaderVaryings::interpolator_flat)
                                            .add_varying("astral_R_value", ShaderVaryings::interpolator_smooth)
                                            .add_varying("astral_y_coord", ShaderVaryings::interpolator_smooth),
                                            2);

  curve_generator = create_shadow_map_shader(m_engine,
                                             ShaderSource()
                                             .add_macros(macros)
                                             .add_source("astral_shadow_util.glsl.resource_string", ShaderSource::from_resource)
                                             .add_source("astral_conic_shadow.vert.glsl.resource_string", ShaderSource::from_resource)
                                             .remove_macros(macros),
                                             ShaderSource()
                                             .add_macros(macros)
                                             .add_source("astral_light_util.glsl.resource_string", ShaderSource::from_resource)
                                             .add_source("astral_conic_shadow.frag.glsl.resource_string", ShaderSource::from_resource)
                                             .remove_macros(macros),
                                             ShaderSymbolList()
                                             .add_varying("astral_conic_p0_x", ShaderVaryings::interpolator_flat)
                                             .add_varying("astral_conic_p0_y", ShaderVaryings::interpolator_flat)
                                             .add_varying("astral_conic_wp1_x", ShaderVaryings::interpolator_flat)
                                             .add_varying("astral_conic_wp1_y", ShaderVaryings::interpolator_flat)
                                             .add_varying("astral_conic_p2_x", ShaderVaryings::interpolator_flat)
                                             .add_varying("astral_conic_p2_y", ShaderVaryings::interpolator_flat)
                                             .add_varying("astral_R_value", ShaderVaryings::interpolator_smooth)
                                             .add_varying("astral_y_coord", ShaderVaryings::interpolator_smooth),
                                             2);

  out_shaders->shader(ShadowMapGeneratorShader::line_segment_primitive, ShadowMapGeneratorShader::x_sides) =
    ShadowMapItemShader::create(*edge_generator, ASTRAL_SHADOW_X_SIDE);

  out_shaders->shader(ShadowMapGeneratorShader::line_segment_primitive, ShadowMapGeneratorShader::y_sides) =
    ShadowMapItemShader::create(*edge_generator, ASTRAL_SHADOW_Y_SIDE);

  out_shaders->shader(ShadowMapGeneratorShader::conic_triangle_primitive, ShadowMapGeneratorShader::x_sides) =
    ShadowMapItemShader::create(*curve_generator, ASTRAL_SHADOW_X_SIDE);

  out_shaders->shader(ShadowMapGeneratorShader::conic_triangle_primitive, ShadowMapGeneratorShader::y_sides) =
    ShadowMapItemShader::create(*curve_generator, ASTRAL_SHADOW_Y_SIDE);

  out_shaders->m_clear_shader = create_shadow_map_shader(m_engine,
                                                         ShaderSource()
                                                         .add_macros(macros)
                                                         .add_source("astral_clear_shadow.vert.glsl.resource_string", ShaderSource::from_resource)
                                                         .remove_macros(macros),
                                                         ShaderSource()
                                                         .add_macros(macros)
                                                         .add_source("astral_clear_shadow.frag.glsl.resource_string", ShaderSource::from_resource)
                                                         .remove_macros(macros),
                                                         ShaderSymbolList(), 1);
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
create_gl3_shaders(ShaderSetGL3 *out_shaders)
{
  create_material_gl3_shaders(out_shaders);
  create_stroke_gl3_shaders(out_shaders);
  create_misc_gl3_shaders(out_shaders);
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
create_default_shaders(const ShaderSetGL3 &gl3_shaders,
                       ShaderSet *out_shaders)
{
  create_rect_shaders(out_shaders);
  create_misc_shaders(out_shaders);
  create_fill_stc_shader(&out_shaders->m_stc_shader);
  create_shadow_generator_shaders(&out_shaders->m_shadow_map_generator_shader);

  out_shaders->m_mask_stroke_shader = gl3_shaders.m_mask_stroke_shaders[ShaderSetGL3::only_stroking].create_mask_stroke_shader(StrokeShaderGL3::include_cap_shaders);
  out_shaders->m_mask_dashed_stroke_shader = gl3_shaders.m_mask_stroke_shaders[ShaderSetGL3::dashed_stroking].create_mask_stroke_shader(StrokeShaderGL3::include_capper_shaders);

  out_shaders->m_direct_stroke_shader = gl3_shaders.m_direct_stroke_shaders[ShaderSetGL3::only_stroking].create_direct_stroke_shader(StrokeShaderGL3::include_cap_shaders);
  out_shaders->m_direct_dashed_stroke_shader = gl3_shaders.m_direct_stroke_shaders[ShaderSetGL3::dashed_stroking].create_direct_stroke_shader(StrokeShaderGL3::include_capper_shaders);

  out_shaders->m_brush_shader = gl3_shaders.m_brush_shader;
  out_shaders->m_light_material_shader = ASTRALnew MaterialShader(*gl3_shaders.m_lighting_shader, ShaderSetGL3::lighting_sub_shader_without_aa);
  out_shaders->m_light_material_shader_aa4_shadow = ASTRALnew MaterialShader(*gl3_shaders.m_lighting_shader, ShaderSetGL3::lighting_sub_shader_aa4);
  out_shaders->m_light_material_shader_aa8_shadow = ASTRALnew MaterialShader(*gl3_shaders.m_lighting_shader, ShaderSetGL3::lighting_sub_shader_aa8);
  out_shaders->m_light_material_shader_aa16_shadow = ASTRALnew MaterialShader(*gl3_shaders.m_lighting_shader, ShaderSetGL3::lighting_sub_shader_aa16);

  ShaderSource::MacroSet clip_combine_macros;

  clip_combine_macros
    .add_macro_u32("ASTRAL_CLIP_COMBINE_CHANNEL_NUMBER_BITS", ClipCombineShader::channel_num_bits)
    .add_macro_u32("ASTRAL_CLIP_COMBINE_LAYER_NUMBER_BITS", ClipCombineShader::tile_layer_num_bits)
    .add_macro_u32("ASTRAL_CLIP_COMBINE_MODE_NUM_BITS", ClipCombineShader::mode_num_bits)
    .add_macro_u32("ASTRAL_CLIP_COMBINE_LAYER_BIT0", ClipCombineShader::tile_layer_bit0)
    .add_macro_u32("ASTRAL_CLIP_COMBINE_COVERAGE_CHANNEL_BIT0", ClipCombineShader::coverage_channel_bit0)
    .add_macro_u32("ASTRAL_CLIP_COMBINE_DISTANCE_CHANNEL_BIT0", ClipCombineShader::distance_channel_bit0)
    .add_macro_u32("ASTRAL_CLIP_COMBINE_MODE_BIT0", ClipCombineShader::mode_bit0)
    .add_macro_u32("ASTRAL_CLIP_COMBINE_ADD_PADDING_BIT", ClipCombineShader::add_padding_bit)
    .add_macro_u32("ASTRAL_CLIP_COMBINE_ADD_PADDING_MASK", (1u << ClipCombineShader::add_padding_bit))
    .add_macro_u32("ASTRAL_CLIP_COMBINE_EMIT_COMPLEMENT_BLUE_ALPHA", ClipCombineShader::emit_complement_values_to_blue_alpha)
    .add_macro_u32("ASTRAL_CLIP_COMBINE_EMIT_RAW_BLUE_ALPHA", ClipCombineShader::emit_direct_values_to_blue_alpha)
    .add_macro_u32("ASTRAL_CLIP_COMBINE_EMIT_RAW_RED_GREEN", ClipCombineShader::emit_direct_values_to_red_green);

  out_shaders->m_clip_combine_shader =
    create_mask_shader(m_engine,
                       ShaderSource()
                       .add_macros(clip_combine_macros)
                       .add_source("astral_combine_clip.vert.glsl.resource_string", ShaderSource::from_resource)
                       .remove_macros(clip_combine_macros),
                       ShaderSource()
                       .add_macros(clip_combine_macros)
                       .add_source("astral_combine_clip.frag.glsl.resource_string", ShaderSource::from_resource)
                       .remove_macros(clip_combine_macros),
                       ShaderVaryings()
                       .add_varying("astral_combine_texel_x", ShaderVaryings::interpolator_smooth)
                       .add_varying("astral_combine_texel_y", ShaderVaryings::interpolator_smooth)
                       .add_varying("astral_combine_bits", ShaderVaryings::interpolator_uint));

  out_shaders->m_color_item_path_shader =
    create_color_item_shader(ColorItemShader::Properties()
                             .emits_partially_covered_fragments(true),
                             m_engine,
                             ShaderSource()
                             .add_source("astral_item_path_common.vert.glsl.resource_string", ShaderSource::from_resource)
                             .add_source("astral_color_item_path.vert.glsl.resource_string", ShaderSource::from_resource),
                             ShaderSource()
                             .add_library(m_shader_libs.m_item_path_lib)
                             .add_source("astral_color_item_path.frag.glsl.resource_string", ShaderSource::from_resource),
                             ShaderSymbolList()
                             .add_vertex_shader_local("astral_item_path_vert_shader_common")
                             .add_varying("astral_path_coord_x", ShaderVaryings::interpolator_smooth)
                             .add_varying("astral_path_coord_y", ShaderVaryings::interpolator_smooth));

  out_shaders->m_mask_item_path_shader =
    create_mask_shader(m_engine,
                       ShaderSource()
                       .add_source("astral_item_path_common.vert.glsl.resource_string", ShaderSource::from_resource)
                       .add_source("astral_mask_item_path.vert.glsl.resource_string", ShaderSource::from_resource),
                       ShaderSource()
                       .add_library(m_shader_libs.m_item_path_lib)
                       .add_source("astral_mask_item_path.frag.glsl.resource_string", ShaderSource::from_resource),
                       ShaderSymbolList()
                       .add_vertex_shader_local("astral_item_path_vert_shader_common")
                       .add_varying("astral_path_coord_x", ShaderVaryings::interpolator_smooth)
                       .add_varying("astral_path_coord_y", ShaderVaryings::interpolator_smooth));

  out_shaders->m_glyph_shader.m_scalable_shader = ColorItemShader::create(*gl3_shaders.m_scalable_glyph_shader,
                                                                          ColorItemShader::Properties()
                                                                          .emits_transparent_fragments(true)
                                                                          .emits_partially_covered_fragments(true),
                                                                          ShaderSetGL3::glyph_sub_shader_preserve_color_glyphs);

  out_shaders->m_glyph_shader.m_image_shader = ColorItemShader::create(*gl3_shaders.m_image_glyph_shader,
                                                                       ColorItemShader::Properties()
                                                                       .emits_transparent_fragments(true),
                                                                       ShaderSetGL3::glyph_sub_shader_preserve_color_glyphs);

  out_shaders->m_glyph_shader_observe_material_always.m_scalable_shader = ColorItemShader::create(*gl3_shaders.m_scalable_glyph_shader,
                                                                                                  ColorItemShader::Properties()
                                                                                                  .emits_transparent_fragments(true)
                                                                                                  .emits_partially_covered_fragments(true),
                                                                                                  ShaderSetGL3::glyph_sub_shader_color_glyphs_obey_material);

  out_shaders->m_glyph_shader_observe_material_always.m_image_shader = ColorItemShader::create(*gl3_shaders.m_image_glyph_shader,
                                                                                               ColorItemShader::Properties()
                                                                                               .emits_transparent_fragments(true),
                                                                                               ShaderSetGL3::glyph_sub_shader_color_glyphs_obey_material);
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
create_default_effect_shaders(const ShaderSetGL3 &gl3_shaders,
                              EffectShaderSet *out_shaders)
{
  reference_counted_ptr<const MaterialShader> gaussian_blur_shader;
  ShaderSource::MacroSet gaussian_blur_macros;
  MaterialShaderGL3::Properties properties;

  ASTRALunused(gl3_shaders);

  gaussian_blur_macros
    .add_macro_u32("ASTRAL_GAUSSIAN_BLUR_HORIZONTAL_PASS", 0u)
    .add_macro_u32("ASTRAL_GAUSSIAN_BLUR_VERTICAL_PASS", 1u)
    .add_macro_u32("ASTRAL_GAUSSIAN_BLUR_SAMPLE_FROM_LOD1_MASK", GaussianBlurEffectShader::sample_from_lod1_mask);

  properties
    .emits_transparent_fragments(true);

  gaussian_blur_shader =
    MaterialShaderGL3::create(m_engine,
                              ShaderSource()
                              .add_macros(gaussian_blur_macros)
                              .add_source("astral_gaussian_blur.vert.glsl.resource_string", ShaderSource::from_resource)
                              .remove_macros(gaussian_blur_macros),
                              ShaderSource()
                              .add_library(m_shader_libs.m_image_lib)
                              .add_macros(gaussian_blur_macros)
                              .add_source("astral_gaussian_blur.frag.glsl.resource_string", ShaderSource::from_resource)
                              .remove_macros(gaussian_blur_macros),
                              ShaderSymbolList()
                              .add_varying("astral_gaussian_blur_x", ShaderVaryings::interpolator_smooth)
                              .add_varying("astral_gaussian_blur_y", ShaderVaryings::interpolator_smooth)
                              .add_varying("astral_gaussian_blur_image_x", ShaderVaryings::interpolator_uint)
                              .add_varying("astral_gaussian_blur_image_y", ShaderVaryings::interpolator_uint)
                              .add_varying("astral_gaussian_blur_image_z", ShaderVaryings::interpolator_uint)
                              .add_varying("astral_gaussian_blur_image_w", ShaderVaryings::interpolator_uint)
                              .add_varying("astral_gaussian_data_location", ShaderVaryings::interpolator_uint)
                              .add_fragment_shader_local("astral_gaussian_filter_coordinate")
                              .add_fragment_shader_local("astral_init_gaussian_filter_coordinate")
                              .add_fragment_shader_local("astral_update_gaussian_filter_coordinate"),
                              properties,
                              2 /* number sub-shaders */);

  out_shaders->m_gaussian_blur_shader
    .horizontal_blur(ASTRALnew MaterialShader(*gaussian_blur_shader, 0u))
    .vertical_blur(ASTRALnew MaterialShader(*gaussian_blur_shader, 1u));
}

void
astral::gl::RenderEngineGL3::Implement::ShaderBuilder::
create_shaders(ShaderSet *out_shaders,
               EffectShaderSet *out_effect_shaders,
               ShaderSetGL3 *out_gl3_shaders)
{
  ASTRALassert(out_shaders);
  ASTRALassert(out_effect_shaders);
  ASTRALassert(out_gl3_shaders);

  out_gl3_shaders->m_common_libs = m_shader_libs;
  create_gl3_shaders(out_gl3_shaders);
  create_default_shaders(*out_gl3_shaders, out_shaders);
  create_default_effect_shaders(*out_gl3_shaders, out_effect_shaders);

  out_gl3_shaders->m_brush_shader = out_shaders->m_brush_shader.dynamic_cast_ptr<const MaterialShaderGL3>();
  ASTRALassert(out_gl3_shaders->m_brush_shader);
}

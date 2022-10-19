/*!
 * \file render_engine_gl3_shader_builder.hpp
 * \brief file render_engine_gl3_shader_builder.hpp
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

#ifndef ASTRAL_RENDER_ENGINE_GL3_SHADER_BUILDER_HPP
#define ASTRAL_RENDER_ENGINE_GL3_SHADER_BUILDER_HPP

#include <iostream>
#include <tuple>
#include <numeric>
#include <astral/renderer/backend/render_backend.hpp>
#include <astral/renderer/gl3/render_engine_gl3.hpp>
#include <astral/renderer/gl3/item_shader_gl3.hpp>
#include <astral/renderer/gl3/shader_set_gl3.hpp>
#include "render_engine_gl3_implement.hpp"
#include "render_engine_gl3_blend_builder.hpp"

class astral::gl::RenderEngineGL3::Implement::ShaderBuilder:public reference_counted<astral::gl::RenderEngineGL3::Implement::ShaderBuilder>::non_concurrent
{
public:
  /* The header ID is a full 32-bit value. However,
   * the header ID itself is only 16-bits. We use
   * the other 16-bits for additional purposes.
   */
  enum header_location_packing_t:uint32_t
    {
      header_location_id_num_bits = 16u,
      header_location_color_space_num_bits = 8u,

      header_location_id_bit0 = 0,
      header_location_color_space_bit0 = header_location_id_bit0 + header_location_id_num_bits,
      header_location_permute_xy_bit = header_location_color_space_bit0 + header_location_color_space_num_bits
    };

  explicit
  ShaderBuilder(RenderEngineGL3 &engine, const BlendBuilder &blend_builder, const Config &config);

  ~ShaderBuilder();

  unsigned int
  allocate_item_shader_index(const ItemShaderBackendGL3 *pshader, enum ItemShader::type_t type);

  unsigned int
  allocate_material_shader_index(const MaterialShaderGL3 *pshader);

  unsigned int
  blend_mode_shader_epilogue(BackendBlendMode blend_mode) const
  {
    return m_blend_epilogue_chooser[blend_mode.packed_value()];
  }

  /*!
   * Return the \ref Program to use to draw items with
   * the named \ref ItemShader.
   * \param shader what ItemShader
   * \param material what, if any, material shader
   * \param mode what blend mode
   * \param shader_clipping specifies if a clip window is present
   *                                  and how to use it
   */
  Program*
  gl_program(const ItemShader &shader,
             const MaterialShader *material,
             BackendBlendMode mode,
             enum clip_window_value_type_t shader_clipping);

  /*!
   * Return the \ref Program for uber-shading given a value from
   * UberShadingKey::cookie(), i.e. from UberShadingKey::uber_shader_of_all();
   * or from UberShadingKey::on_end_accumulate()
   *
   * Can return nullptr if the number of varyings required by the uber
   * exceeds the number of varying the GL implementation supports.
   *
   * \param v uber-shader cookie value, i.e. from UberShadingKey::on_end_accumulate();
   *          or from UberShadingKey::uber_shader_of_all()
   */
  Program*
  uber_program(RenderBackend::UberShadingKey::Cookie cookie);

  /*!
   * Given a value for clip_window_value_type_t return the uber-of-all program
   */
  Program*
  uber_of_all_program(enum clip_window_value_type_t shader_clipping)
  {
    RenderBackend::UberShadingKey::Cookie cookie(shader_clipping);
    return uber_program(cookie);
  }

  /*!
   * Force an Program returned by uber_of_all_program() to be linked
   * immediately.
   */
  void
  force_uber_shader_program_link(void);

  /*!
   * Given a uber-shader cookie, value, returns true if and only
   * if the cookied refers to a shader as returned by
   * uber_of_all_program().
   */
  static
  bool
  uber_shader_cookie_is_all_uber_shader(RenderBackend::UberShadingKey::Cookie cookie)
  {
    return cookie.m_value < clip_window_value_type_count;
  }

  const BlendBuilder&
  blend_builder(void) const
  {
    return m_blend_builder;
  }

  /*!
   * Returns the \ref Program to perform the blitting
   * needed for GPU vertex streaming. The input is
   *   - .xy --> gives the location in pixels to blit to (floats)
   *   - .z  --> gives the header ID (uint)
   *   - .w   --> gives the index into the TBO of VertexDataBacking for that side (float)
   * and the output is
   *   - .x --> 32-bit uint giving the index into the TBO of VertexDataBacking
   *   - .y --> 32-bit uint giving the header index/location
   *
   * \param recip_half_viewport_width_height_location destination to which to write
   *                                                  the locaiton of the uniform
   *                                                  storing vec2(2.0) / dimensions
   */
  Program*
  gpu_streaming_blitter(int *recip_half_viewport_width_height_location)
  {
    *recip_half_viewport_width_height_location = m_recip_half_viewport_width_height_location;
    return m_gpu_streaming_blitter.get();
  }

  /*
   * Called by RenderEngineGL3 to build/create the default shader set
   */
  void
  create_shaders(ShaderSet *out_shaders,
                 EffectShaderSet *out_effect_shaders,
                 ShaderSetGL3 *out_gl3_shaders);

  /* Impements RenderBackend::create_uber_shading_key() for
   * RenderEngineGL3::Implement::Backend.
   */
  reference_counted_ptr<RenderBackend::UberShadingKey>
  create_uber_shading_key(const MaterialShader *default_brush);

  /* Given a key value returned by UberShadingKey::on_end_accumulate(),
   * returns true exactly when the uber-shader handles the given
   * shader.
   */
  bool
  uber_has_shader(RenderBackend::UberShadingKey::Cookie key,
                  const ItemShader &shader,
                  const MaterialShader &material_shader,
                  BackendBlendMode blend_mode) const;

  /* Given a key value returned by UberShadingKey::on_end_accumulate(),
   * returns true exactly when the uber-shader handles the given
   * shaders.
   */
  bool
  uber_has_shaders(RenderBackend::UberShadingKey::Cookie key,
                   c_array<const pointer<const ItemShader>> shaders,
                   const MaterialShader &material_shader,
                   BackendBlendMode blend_mode) const;

  /* Given a key value returned by UberShadingKey::on_end_accumulate(),
   * returns if a clip window is present and how it is used.
   */
  enum clip_window_value_type_t
  uber_shader_clipping(RenderBackend::UberShadingKey::Cookie key) const;

private:
  class StrokeShaderBuilder;
  class SourceBuilder;

  /* when building what shaders are to be used by an uber-shader,
   * we maintain a list of what ItemShader, MaterialShader and
   * blend shader epilogues are needed for the uber shader; this
   * class represents that single list; the lists are sorted so
   * that we can also quickly tell if an entry is present.
   */
  class UberShaderKey
  {
  public:
    bool
    operator<(const UberShaderKey &rhs) const
    {
      return m_values < rhs.m_values;
    }

    /* Set the item shaders (stored value will sort the shaders by address
     * when copying them).
     * \tparam iterator can be an iterator to a const ItemShaderBackendGL3* or
     *                  an iterator to a reference_counted_ptr<const ItemShaderBackendGL3>
     */
    template<typename iterator>
    void
    item_shaders(iterator begin, iterator end)
    {
      set_values(begin, end, &std::get<1>(m_values));
    }

    /* Set the material shaders (stored value will sort the shaders by address
     * when copying them).
     * \tparam iterator can be an iterator to a const MaterialShaderGL3* or
     *                  an iterator to a reference_counted_ptr<const MaterialShaderGL3>
     */
    template<typename iterator>
    void
    material_shaders(iterator begin, iterator end)
    {
      set_values(begin, end, &std::get<2>(m_values));
    }

    /* Set the blend shaders as all blend shaders in [begin, end) */
    void
    blend_shaders_direct(unsigned int begin, unsigned int end)
    {
      ASTRALassert(end >= begin);
      std::get<3>(m_values).resize(end - begin);
      std::iota(std::get<3>(m_values).begin(), std::get<3>(m_values).end(), begin);
    }

    /* Set the blend shaders (stored value will sort the shaders by ID
     * when copying them).
     * \tparam iterator iterator to an unsigned int
     */
    template<typename iterator>
    void
    blend_shaders(iterator begin, iterator end)
    {
      set_values(begin, end, &std::get<3>(m_values));
    }

    const std::vector<const ItemShaderBackendGL3*>*
    item_shaders(void) const
    {
      return &std::get<1>(m_values);
    }

    const std::vector<const MaterialShaderGL3*>*
    material_shaders(void) const
    {
      return &std::get<2>(m_values);
    }

    const std::vector<unsigned int>*
    blend_shaders(void) const
    {
      return &std::get<3>(m_values);
    }

    enum clip_window_value_type_t
    shader_clipping(void) const
    {
      return std::get<0>(m_values);
    }

    void
    shader_clipping(enum clip_window_value_type_t v)
    {
      std::get<0>(m_values) = v;
    }

    bool
    has_program(const ItemShaderBackendGL3 &item_shader,
                const MaterialShaderGL3 &material_shader,
                unsigned int blend_shader) const
    {
      return std::binary_search(item_shaders()->begin(), item_shaders()->end(), &item_shader)
        && std::binary_search(material_shaders()->begin(), material_shaders()->end(), &material_shader)
        && std::binary_search(blend_shaders()->begin(), blend_shaders()->end(), blend_shader);
    }

    bool
    requires_framebuffer_pixels(void) const;

  private:
    static
    const ItemShaderBackendGL3*
    get_value(const ItemShaderBackendGL3 *p) { return p; }

    static
    const ItemShaderBackendGL3*
    get_value(const reference_counted_ptr<const ItemShaderBackendGL3> &p) { return p.get(); }

    static
    const MaterialShaderGL3*
    get_value(const MaterialShaderGL3 *p) { return p; }

    static
    const MaterialShaderGL3*
    get_value(const reference_counted_ptr<const MaterialShaderGL3> &p) { return p.get(); }

    static
    unsigned int
    get_value(unsigned int p) { return p; }

    template<typename T, typename iterator>
    static
    void
    set_values(iterator begin, iterator end, std::vector<T> *out_values)
    {
      out_values->clear();
      for (; begin != end; ++begin)
        {
          out_values->push_back(get_value(*begin));
        }
      std::sort(out_values->begin(), out_values->end());
    }

    std::tuple<enum clip_window_value_type_t,
               std::vector<const ItemShaderBackendGL3*>,
               std::vector<const MaterialShaderGL3*>,
               std::vector<unsigned int>> m_values;
  };

  class ShaderListForUberShading;
  class UberShadingKeyImplement;

  class PerUberShader
  {
  public:
    PerUberShader(void):
      m_num_varyings(0)
    {}

    UberShaderKey m_key;
    unsigned int m_num_varyings;
    reference_counted_ptr<Program> m_program;
  };

  class ProgramSet
  {
  public:
    void
    clear_all(void)
    {
      for (unsigned int t = 0; t < clip_window_value_type_count; ++t)
        {
          for (unsigned int i = 0; i < BackendBlendMode::number_packed_values; ++i)
            {
              m_programs[t][i] = nullptr;
            }
        }
    }

    const reference_counted_ptr<Program>&
    program(enum clip_window_value_type_t shader_clipping, BackendBlendMode mode) const
    {
      return m_programs[shader_clipping][mode.packed_value()];
    }

    void
    program(enum clip_window_value_type_t shader_clipping, BackendBlendMode mode,
            const reference_counted_ptr<Program> &v)
    {
      m_programs[shader_clipping][mode.packed_value()] = v;
    }

  private:
    vecN<vecN<reference_counted_ptr<Program>, BackendBlendMode::number_packed_values>, clip_window_value_type_count> m_programs;
  };

  class ShaderFunctionSignature
  {
  public:
    class Argument
    {
    public:
      Argument(const std::string &type,
               const std::string &name):
        m_type(type),
        m_name(name)
      {}

      std::string m_type;
      std::string m_name;
    };

    ShaderFunctionSignature&
    add_argument(const std::string &type, const std::string &name)
    {
      m_argument_list.push_back(Argument(type, name));
      return *this;
    }

    ShaderFunctionSignature&
    name(const std::string &v)
    {
      m_name = v;
      return *this;
    }

    ShaderFunctionSignature&
    return_type(const std::string &v)
    {
      m_return_type = v;
      return *this;
    }

    /* includes both the type and the in, out or inout decoration */
    std::vector<Argument> m_argument_list;

    /* return type; an empty string indicates no return type */
    std::string m_return_type;

    /* name of function */
    std::string m_name;
  };

  class PreAndActualFunctionSignature
  {
  public:
    void
    stream_runner_declaration(c_string name, bool add_semi_colon, ShaderSource *dst) const;

    ShaderFunctionSignature m_pre_function, m_function;
  };

  /* Different blend modes can share the same shader epilogue;
   * this class lists the BackendBlendMode values that use the
   * same shader epilogue.
   */
  class CommonBlendEpilogue
  {
  public:
    void
    swap(CommonBlendEpilogue &obj)
    {
      std::swap(m_requires_framebuffer_pixels, obj.m_requires_framebuffer_pixels);
      m_shader_epilogue.swap(obj.m_shader_epilogue);
      m_elements.swap(obj.m_elements);
    }

    bool m_requires_framebuffer_pixels;
    std::string m_shader_epilogue;
    std::vector<BackendBlendMode> m_elements;
  };

  reference_counted_ptr<Program>
  create_program(const ShaderSource &vert,
                 const ShaderSource &frag);

  void
  create_blend_epilogue_chooser(void);

  void
  create_shader_singnatures(void);

  void
  create_libs(void);

  void
  create_base_lib(void);

  void
  create_stroke_lib(void);

  void
  create_image_lib(void);

  void
  create_gradient_lib(void);

  void
  create_item_path_lib(void);

  void
  create_misc_shaders(ShaderSet *out_shaders);

  void
  create_rect_shaders(ShaderSet *out_shaders);

  void
  create_fill_stc_shader(FillSTCShader *dst);

  void
  create_shadow_generator_shaders(ShadowMapGeneratorShader *out_shaders);

  void
  create_material_gl3_shaders(ShaderSetGL3 *out_shaders);

  void
  create_stroke_gl3_shaders(ShaderSetGL3 *out_shaders);

  void
  create_misc_gl3_shaders(ShaderSetGL3 *out_shaders);

  void
  create_gl3_library(ShaderLibraryGL3 *out_lib);

  void
  create_gl3_shaders(ShaderSetGL3 *out_shaders);

  void
  create_default_shaders(const ShaderSetGL3 &gl3_shaders, ShaderSet *out_shaders);

  void
  create_default_effect_shaders(const ShaderSetGL3 &gl3_shaders,
                                EffectShaderSet *out_shaders);

  static
  uint32_t
  uber_shader_cookie(enum clip_window_value_type_t shader_clipping)
  {
    ASTRALassert(shader_clipping < clip_window_value_type_count);
    return shader_clipping;
  }

  unsigned int m_item_shader_index_count;
  unsigned int m_material_shader_index_count;

  /* Different blend modes can share the same shader epilogue.
   * Each array of the array of arrays share the same shader
   * epilogue.
   */
  std::vector<CommonBlendEpilogue> m_blend_epilogue;

  /* m_blend_epilogue_chooser[b.packed_value()] gives the index
   * into m_blend_epilogue for a blend mode b
   */
  vecN<unsigned int, BackendBlendMode::number_packed_values> m_blend_epilogue_chooser;

  reference_counted_ptr<const ShaderLibrary> m_base_lib;
  ShaderLibraryGL3 m_shader_libs;

  /* for each (ItemShader::shader_type, ItemShaderBackend, MaterialShader)
   * where ItemShaderBackend and MaterialShader are not a sub-shader, a program
   */
  vecN<std::vector<std::vector<ProgramSet>>, ItemShader::number_item_shader_types> m_non_uber_programs;

  reference_counted_ptr<Program> m_gpu_streaming_blitter;
  int m_recip_half_viewport_width_height_location;

  /* I hate this; the values in PerUberShader::m_key are just
   * pointers not strong references. We need to hold onto the
   * references to prevent the unlikley case that a shader
   * is deleted.
   */
  std::vector<reference_counted_ptr<const ItemShaderBackendGL3>> m_all_color_item_shaders;
  std::vector<reference_counted_ptr<const MaterialShaderGL3>> m_all_material_shaders;

  /* Give the index into m_uber_shaders[] for a given UberShaderKey.
   */
  std::map<UberShaderKey, uint32_t> m_uber_shader_lookup;

  /* Objects holding the gl::Program made from an UberShaderKey
   * m_uber_shaders[F] for 0 <= F < clip_window_value_type_count is the
   * uber-shade where the shader-clipping is given by the value of F.
   */
  std::vector<PerUberShader> m_uber_shaders;

  PreAndActualFunctionSignature m_rect_vert_sigs, m_rect_frag_sigs;
  PreAndActualFunctionSignature m_mask_vert_sigs, m_mask_frag_sigs;
  PreAndActualFunctionSignature m_shadow_vert_sigs, m_shadow_frag_sigs;
  PreAndActualFunctionSignature m_material_vert_sigs, m_material_frag_sigs;

  BlendBuilder m_blend_builder;
  Config m_config;
  unsigned int m_max_item_material_varying_count;

  /* the RenderEngine to which owns this ShaderBuilder;
   * note that this cannot be a reference_counted_ptr<>
   * because the RenderEngineGL3 that made this has
   * a reference_counted_ptr<> to it; so we just make
   * it a reference. We need this reference to access
   * values derived from various atlas values
   */
  RenderEngineGL3 &m_engine;
};

#endif

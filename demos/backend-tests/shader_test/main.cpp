/*!
 * \file main.cpp
 * \brief main.cpp
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
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <dirent.h>
#include <stdlib.h>
#include <set>

#include <astral/util/ostream_utility.hpp>
#include <astral/util/gl/gl_program.hpp>
#include <astral/util/gl/gl_get.hpp>

#include "render_engine_gl3_demo.hpp"
#include "sdl_demo.hpp"

class ShaderTest:public render_engine_gl3_demo
{
public:
  ShaderTest(void);

protected:
  virtual
  void
  init_gl(int, int) override;

private:
  void
  check_item_shaders(void);

  void
  check_uber_shaders(void);

  void
  check_shader(astral::gl::Program *p, const std::string &label);

  void
  check_shader(const astral::ItemShader *shader, const std::string &label);

  void
  check_shader(const astral::reference_counted_ptr<const astral::ItemShader> &shader,
               const std::string &label)
  {
    check_shader(shader.get(), label);
  }

  template<typename T>
  void
  check_stroke_capper_shader(const astral::vecN<astral::reference_counted_ptr<const T>, astral::StrokeShader::number_capper_shader> &shader,
                             const std::string &label);

  template<typename T>
  void
  check_stroke_shader_set(const typename astral::StrokeShaderT<T>::ShaderSet &shader, const std::string &label);

  template<typename T>
  void
  check_shader(const astral::StrokeShaderT<T> &shader, const std::string &label);

  command_separator m_demo_options;
  command_line_argument_value<bool> m_test_ubers;

  std::map<astral::gl::Program*, std::string> m_tested_programs;
};

//////////////////////////////////
// ShaderTest methods
ShaderTest::
ShaderTest(void):
  m_demo_options("Demo Options", *this),
  m_test_ubers(false, "test_ubers",
               "if true instead of testing various item shaders, "
               "just test the uber shaders",
               *this)
{
}

void
ShaderTest::
check_shader(astral::gl::Program *p, const std::string &label)
{
  if (demo_over())
    {
      return;
    }

  if (!p)
    {
      std::cout << "Program generation failed for " << label << "\n";
      return;
    }

  std::cout << "Shader: " << label;
  if (m_tested_programs.find(p) == m_tested_programs.end())
    {
      std::ostringstream pr, vs, fs;

      pr << "program_" << p->name() << ".log";
      m_tested_programs[p] = pr.str();

      std::ofstream pr_file(m_tested_programs[p]);
      pr_file << p->log() << "\n";

      const std::pair<astral_GLenum, const char*> shaders[2] = {
        {ASTRAL_GL_FRAGMENT_SHADER, "frag"},
        {ASTRAL_GL_VERTEX_SHADER, "vert"},
      };

      for (auto sh : shaders)
        {
          for (unsigned int i = 0, endi = p->num_shaders(sh.first); i < endi; ++i)
            {
              std::ostringstream nm;

              nm << m_tested_programs[p] << "." << i << "." << sh.second;
              std::ofstream sh_file(nm.str());
              sh_file << p->shader_src_code(sh.first, i) << "\n";
            }
        }
      std::cout << " logged to " << m_tested_programs[p] << "\n";
      if (!p->link_success())
        {
          std::cout << "!!!!!!" << label << " shader failed to link\n";
          end_demo(0);
        }
    }
  else
    {
      std::cout << " already generated, logged to " << m_tested_programs[p] << "\n";
    }
}

void
ShaderTest::
check_shader(const astral::ItemShader *shader, const std::string &label)
{
  if (!shader)
    {
      return;
    }

  const astral::MaterialShader *material(nullptr);

  struct sub_shader
  {
  public:
    sub_shader(enum astral::clip_window_value_type_t c, enum astral::blend_mode_t b, bool p, const char *l):
      m_c(c),
      m_blend_mode(b, p),
      m_sub_label(l)
    {}

    sub_shader(enum astral::clip_window_value_type_t c, astral::BackendBlendMode b, const char *l):
      m_c(c),
      m_blend_mode(b),
      m_sub_label(l)
    {}

    enum astral::clip_window_value_type_t m_c;
    astral::BackendBlendMode m_blend_mode;
    const char *m_sub_label;
  };

  if (shader->type() == astral::ItemShader::color_item_shader)
    {
      static const struct sub_shader sub_shaders[] =
        {
          sub_shader(astral::clip_window_present_enforce, astral::blend_porter_duff_src_over, false, ".shader_clip"),
          sub_shader(astral::clip_window_not_present, astral::blend_porter_duff_src_over, false, ".depth_occlude_clip"),
          sub_shader(astral::clip_window_present_optional, astral::blend_porter_duff_src_over, false, ".depth_occlude_clip_hinted"),

          sub_shader(astral::clip_window_present_enforce, astral::blend_porter_duff_src_over, true, ".shader_clip.partial_coverage"),
          sub_shader(astral::clip_window_not_present, astral::blend_porter_duff_src_over, true, ".depth_occlude_clip.partial_coverage"),
          sub_shader(astral::clip_window_present_optional, astral::blend_porter_duff_src_over, true, ".depth_occlude_clip_hinted"),
        };

      material = engine().default_shaders().m_brush_shader.get();

      for (unsigned int i = 0; i < 6; ++i)
        {
          astral::gl::Program *p;

          /* only rect item shaders should support mask coverage */
          p = engine().gl_program(*shader, material, sub_shaders[i].m_blend_mode, sub_shaders[i].m_c);
          check_shader(p, label + sub_shaders[i].m_sub_label);
        }
    }
  else if (shader->type() == astral::ItemShader::mask_item_shader)
    {
      static const struct sub_shader sub_shaders[] =
        {
          sub_shader(astral::clip_window_present_enforce, astral::BackendBlendMode(astral::BackendBlendMode::mask_mode_rendering), ".shader_clip"),
          sub_shader(astral::clip_window_not_present, astral::BackendBlendMode(astral::BackendBlendMode::mask_mode_rendering), ".depth_occlude_clip"),
          sub_shader(astral::clip_window_present_optional, astral::BackendBlendMode(astral::BackendBlendMode::mask_mode_rendering), ".depth_occlude_clip_hinted"),
        };

      for (unsigned int i = 0; i < 3; ++i)
        {
          astral::gl::Program *p;
          p = engine().gl_program(*shader, nullptr, sub_shaders[i].m_blend_mode, sub_shaders[i].m_c);
          check_shader(p, label + sub_shaders[i].m_sub_label);
        }
    }
  else
    {
      astral::gl::Program *p;

      ASTRALassert(shader->type() == astral::ItemShader::shadow_map_item_shader);
      p = engine().gl_program(*shader, nullptr, astral::BackendBlendMode(astral::BackendBlendMode::shadowmap_mode_rendering), astral::clip_window_not_present);
      check_shader(p, label);
    }
}

template<typename T>
void
ShaderTest::
check_shader(const astral::StrokeShaderT<T> &shader, const std::string &label)
{
  for (unsigned int i = 0; i < astral::number_cap_t; ++i)
    {
      enum astral::cap_t c;

      c = static_cast<enum astral::cap_t>(i);
      check_stroke_shader_set<T>(shader.shader_set(c), label + "." + astral::label(c));
    }
}

template<typename T>
void
ShaderTest::
check_stroke_capper_shader(const astral::vecN<astral::reference_counted_ptr<const T>, astral::StrokeShader::number_capper_shader> &shader,
                           const std::string &label)
{
  std::string sh[astral::StrokeShader::number_capper_shader] =
    {
      [astral::StrokeShader::capper_shader_start] = ".capper_start",
      [astral::StrokeShader::capper_shader_end] = ".capper_end",
    };

  for (unsigned int i = 0; i < astral::StrokeShader::number_capper_shader; ++i)
    {
      check_shader(shader[i], label + sh[i]);
    }
}

template<typename T>
void
ShaderTest::
check_stroke_shader_set(const typename astral::StrokeShaderT<T>::ShaderSet &shader, const std::string &label)
{
  std::string sh[astral::StrokeShader::path_shader_count] =
    {
      [astral::StrokeShader::static_path_shader] = ".static.",
      [astral::StrokeShader::animated_path_shader] = ".animated.",
    };

  for (unsigned int P = 0; P < astral::StrokeShader::path_shader_count; ++P)
    {
      check_shader(shader.m_subset[P].m_line_segment_shader, label + sh[P] + "line_segment");
      check_shader(shader.m_subset[P].m_biarc_curve_shader, label + sh[P] + "biarc");
      check_shader(shader.m_subset[P].m_cap_shader, label + sh[P] + "cap");

      for (unsigned int i = 0; i < astral::number_join_t; ++i)
        {
          check_shader(shader.m_subset[P].m_join_shaders[i], label + sh[P] + astral::label(static_cast<enum astral::join_t>(i)));
        }

      check_stroke_capper_shader(shader.m_subset[P].m_line_capper_shaders, label + sh[P] + "line_capper");
      check_stroke_capper_shader(shader.m_subset[P].m_quadratic_capper_shaders,  label + sh[P] + "quadratic_capper");
    }
}

void
ShaderTest::
init_gl(int, int)
{
  if (m_test_ubers.value())
    {
      check_uber_shaders();
    }
  else
    {
      check_item_shaders();
    }

  std::cout << "\n\nDone\n";
  end_demo(0);
}

void
ShaderTest::
check_uber_shaders(void)
{
  using namespace astral;

  reference_counted_ptr<RenderBackend> bk;
  reference_counted_ptr<RenderBackend::UberShadingKey> key;
  gl::Program *p;

  bk = engine().create_backend();
  key = bk->create_uber_shading_key();

  key->uber_shader_of_all(clip_window_present_enforce);
  p = engine().gl_program(*key);
  check_shader(p, "uber.shader_clip");

  key->uber_shader_of_all(clip_window_not_present);
  p = engine().gl_program(*key);
  check_shader(p, "uber.depth_occlude_clip");

  key->uber_shader_of_all(clip_window_present_optional);
  p = engine().gl_program(*key);
  check_shader(p, "uber.depth_occlude_clip_hint");
}

void
ShaderTest::
check_item_shaders(void)
{
  using namespace astral;
  const ShaderSet &shaders(engine().default_shaders());

  check_shader(shaders.m_shadow_map_generator_shader.m_clear_shader.get(), "clear_shadow_map");
  check_shader(shaders.m_shadow_map_generator_shader.shader(astral::ShadowMapGeneratorShader::line_segment_primitive,
                                                            astral::ShadowMapGeneratorShader::x_sides).get(),
               "shadow_map_x_sides_line");
  check_shader(shaders.m_shadow_map_generator_shader.shader(astral::ShadowMapGeneratorShader::conic_triangle_primitive,
                                                            astral::ShadowMapGeneratorShader::x_sides).get(),
               "shadow_map_x_sides_conic");
  check_shader(shaders.m_shadow_map_generator_shader.shader(astral::ShadowMapGeneratorShader::line_segment_primitive,
                                                            astral::ShadowMapGeneratorShader::y_sides).get(),
               "shadow_map_y_sides_line");
  check_shader(shaders.m_shadow_map_generator_shader.shader(astral::ShadowMapGeneratorShader::conic_triangle_primitive,
                                                            astral::ShadowMapGeneratorShader::y_sides).get(),
               "shadow_map_y_sides_conic");

  check_shader(shaders.m_masked_rect_shader.get(), "masked_mapped_rect");
  check_shader(shaders.m_glyph_shader.m_scalable_shader.get(), "scalable_glyph");
  check_shader(shaders.m_glyph_shader.m_image_shader.get(), "image_glyph");
  check_shader(shaders.m_color_item_path_shader.get(), "item_path");
  check_shader(shaders.m_dynamic_rect_shader.get(), "dynamic_rect");

  check_shader(shaders.m_stc_shader.m_shaders[FillSTCShader::pass_contour_stencil].get(), "stc_line");
  check_shader(shaders.m_stc_shader.m_shaders[FillSTCShader::pass_conic_triangles_stencil].get(), "stc_line_fuzz");
  check_shader(shaders.m_stc_shader.m_shaders[FillSTCShader::pass_contour_fuzz].get(), "stc_curve");
  check_shader(shaders.m_stc_shader.m_shaders[FillSTCShader::pass_conic_triangle_fuzz].get(), "stc_curve_fuzz");

  check_shader(*shaders.m_mask_stroke_shader, "stroking_mask");
  check_shader(*shaders.m_mask_dashed_stroke_shader, "dashed_stroking_mask");

  check_shader(*shaders.m_direct_stroke_shader, "stroking_direct");

}

int
main(int argc, char **argv)
{
  ShaderTest M;
  return M.main(argc, argv);
}

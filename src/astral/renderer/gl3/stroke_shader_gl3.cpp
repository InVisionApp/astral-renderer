/*!
 * \file stroke_shader_gl3.cpp
 * \brief file stroke_shader_gl3.cpp
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

#include <astral/renderer/gl3/stroke_shader_gl3.hpp>
#include "stroke_shader_gl3_enums.hpp"

namespace
{
  class ColorItemShaderCreator
  {
  public:
    explicit
    ColorItemShaderCreator(astral::ColorItemShader::Properties props):
      m_props(props)
    {
    }

    astral::reference_counted_ptr<const astral::ColorItemShader>
    create_shader(const astral::reference_counted_ptr<const astral::ItemShaderBackend> &backend, unsigned int sub_shader_id) const
    {
      if (backend)
        {
          return astral::ColorItemShader::create(*backend, m_props, sub_shader_id);
        }
      else
        {
          return nullptr;
        }
    }

  private:
    astral::ColorItemShader::Properties m_props;
  };

  class MaskItemShaderCreator
  {
  public:
    astral::reference_counted_ptr<const astral::MaskItemShader>
    create_shader(const astral::reference_counted_ptr<const astral::ItemShaderBackend> &backend, unsigned int sub_shader_id) const
    {
      if (backend)
        {
          return astral::MaskItemShader::create(*backend, sub_shader_id);
        }
      else
        {
          return nullptr;
        }
    }
  };

  astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3>
  color_shader_from_mask_shader_implement(const astral::reference_counted_ptr<const astral::gl::ItemShaderBackendGL3> &V)
  {
    if (V)
      {
        return V->color_shader_from_mask_shader();
      }
    else
      {
        return nullptr;
      }
  }

  template<typename T>
  class ShaderSetFamiliyContainer
  {
  public:
    typename astral::StrokeShaderT<T>::ShaderSetFamily m_shaders;
  };
}

template<typename T, size_t N>
astral::vecN<T, N>
color_shader_from_mask_shader_implement(const astral::vecN<T, N> &V)
{
  astral::vecN<T, N> return_value;

  for (unsigned int i = 0; i < N; ++i)
    {
      return_value[i] = color_shader_from_mask_shader_implement(V[i]);
    }

  return return_value;
}

template<typename T, typename F>
void
compute_stroke_shader_vanilla(astral::type_tag<T>, F f, const astral::gl::StrokeShaderGL3 &src,
                              ShaderSetFamiliyContainer<T> *dst)
{
  using namespace astral;
  using namespace astral::gl;
  using namespace StrokeShaderGL3Enums;

  for (unsigned int ip = 0; ip < StrokeShader::path_shader_count; ++ip)
    {
      enum StrokeShader::path_shader_t p;

      p = static_cast<enum StrokeShader::path_shader_t>(ip);

      /* join shading is not impacted by caps */
      for (unsigned int i = 0; i < number_join_t; ++i)
        {
          dst->m_shaders[0].m_subset[p].m_join_shaders[i]
            = f.create_shader(src.m_join[i], sub_shader_id(p, outer_join_sub_shader));
        }

      dst->m_shaders[0].m_subset[p].m_inner_glue_shader
        = f.create_shader(src.m_join[join_rounded], sub_shader_id(p, inner_join_sub_shader));

      for (unsigned int ic = 1; ic < number_cap_t; ++ic)
        {
          dst->m_shaders[ic].m_subset[p] = dst->m_shaders[0].m_subset[p];
        }

      /* but line and shading are impacted by caps */
      for (unsigned int ic = 0; ic < number_cap_t; ++ic)
        {
          enum cap_t c;

          c = static_cast<enum cap_t>(ic);
          dst->m_shaders[c].m_subset[p].m_line_segment_shader
            = f.create_shader(src.m_line, sub_shader_id(p, c));

          dst->m_shaders[c].m_subset[p].m_biarc_curve_shader
            = f.create_shader(src.m_biarc_curve, sub_shader_id(p, c));
        }
    }
}

template<typename T, typename F>
void
compute_cap_shaders(astral::type_tag<T>, F f, const astral::gl::StrokeShaderGL3 &src, ShaderSetFamiliyContainer<T> *dst)
{
  using namespace astral;
  using namespace astral::gl;
  using namespace StrokeShaderGL3Enums;

  for (unsigned int ip = 0; ip < StrokeShader::path_shader_count; ++ip)
    {
      enum StrokeShader::path_shader_t p;

      p = static_cast<enum StrokeShader::path_shader_t>(ip);

      dst->m_shaders[cap_rounded].m_subset[p].m_cap_shader
        = f.create_shader(src.m_rounded_cap, sub_shader_id(p));

      dst->m_shaders[cap_square].m_subset[p].m_cap_shader
        = f.create_shader(src.m_square_cap, sub_shader_id(p));
    }
}

template<typename T, typename F>
void
compute_capper_shaders(astral::type_tag<T>, F f, const astral::gl::StrokeShaderGL3 &src, ShaderSetFamiliyContainer<T> *dst)
{
  using namespace astral;
  using namespace astral::gl;
  using namespace StrokeShaderGL3Enums;

  for (unsigned int ip = 0; ip < StrokeShader::path_shader_count; ++ip)
    {
      enum StrokeShader::path_shader_t p;

      p = static_cast<enum StrokeShader::path_shader_t>(ip);
      for (unsigned int ic = 0; ic < number_cap_t; ++ic)
        {
          enum cap_t c;

          c = static_cast<enum cap_t>(ic);
          if (c != cap_flat)
            {
              /* m_cappers[capper_primitive_t][cap_t] where
               *   capper_primitive_t is one of line_segment_capper, quadratic_capper
               *   cap_t is one of cap_rounded, cap_square
               */
              for (unsigned int s = 0; s < StrokeShader::number_capper_shader; ++s)
                {
                  enum StrokeShader::capper_shader_t es;

                  es = static_cast<enum StrokeShader::capper_shader_t>(s);

                  dst->m_shaders[c].m_subset[p].m_line_capper_shaders[s]
                    = f.create_shader(src.m_cappers[StrokeShaderGL3::line_segment_capper][c],
                                      sub_shader_id(p, es));

                  dst->m_shaders[c].m_subset[p].m_quadratic_capper_shaders[s]
                    = f.create_shader(src.m_cappers[StrokeShaderGL3::quadratic_capper][c],
                                      sub_shader_id(p, es));
                }
            }
        }
    }
}

template<typename T, typename F>
void
compute_stroke_shader_per_cap(astral::type_tag<T>, F f, const astral::gl::StrokeShaderGL3 &src, ShaderSetFamiliyContainer<T> *dashed_dst)
{
  using namespace astral;
  using namespace astral::gl;
  using namespace StrokeShaderGL3Enums;

  for (unsigned int ip = 0; ip < StrokeShader::path_shader_count; ++ip)
    {
      enum StrokeShader::path_shader_t p;

      p = static_cast<enum StrokeShader::path_shader_t>(ip);
      for (unsigned int ic = 0; ic < number_cap_t; ++ic)
        {
          enum cap_t c;

          c = static_cast<enum cap_t>(ic);

          dashed_dst->m_shaders[c].m_subset[p].m_line_segment_shader
            = f.create_shader(src.m_line, sub_shader_id(p, c));

          dashed_dst->m_shaders[c].m_subset[p].m_biarc_curve_shader
            = f.create_shader(src.m_biarc_curve, sub_shader_id(p, c));
        }

      for (unsigned int i = 0; i < number_join_t; ++i)
        {
          unsigned int sub_shader;

          /* join shaders only enforce the dash pattern, it is the job of the
           * capper shaders to extend a line segment or biarc to get a cap.
           */
          sub_shader = sub_shader_id(p, outer_join_sub_shader, cap_flat);
          dashed_dst->m_shaders[0].m_subset[p].m_join_shaders[i]
            = f.create_shader(src.m_join[i], sub_shader);

          for (unsigned int ic = 1; ic < number_cap_t; ++ic)
            {
              dashed_dst->m_shaders[ic].m_subset[p].m_join_shaders[i] =
                dashed_dst->m_shaders[0].m_subset[p].m_join_shaders[i];
            }
        }

      dashed_dst->m_shaders[cap_flat].m_subset[p].m_inner_glue_shader
        = dashed_dst->m_shaders[cap_rounded].m_subset[p].m_inner_glue_shader
        = f.create_shader(src.m_join[join_rounded],
                          sub_shader_id(p, inner_join_sub_shader, cap_flat));

      dashed_dst->m_shaders[cap_square].m_subset[p].m_inner_glue_shader
        = f.create_shader(src.m_join[join_rounded],
                          sub_shader_id(p, inner_join_sub_shader, cap_square));
    }
}

template<typename T, typename F>
astral::reference_counted_ptr<astral::StrokeShaderT<T>>
create_stroke_shader_implement(astral::type_tag<T> t, F f, const astral::gl::StrokeShaderGL3 &src, uint32_t flags)
{
  using namespace astral;
  using namespace astral::gl;
  using namespace StrokeShaderGL3Enums;

  ShaderSetFamiliyContainer<T> dst;

  if (src.m_per_cap_shading)
    {
      compute_stroke_shader_per_cap(t, f, src, &dst);
    }
  else
    {
      compute_stroke_shader_vanilla(t, f, src, &dst);
    }

  if ((flags & StrokeShaderGL3::include_capper_shaders) != 0u)
    {
      compute_capper_shaders(t, f, src, &dst);
    }

  if ((flags & StrokeShaderGL3::include_cap_shaders) != 0u)
    {
      compute_cap_shaders(t, f, src, &dst);
    }

  return StrokeShaderT<T>::create(dst.m_shaders);
}

////////////////////////////////////////////////
// astral::gl::StrokeShaderGL3 methods
astral::reference_counted_ptr<astral::DirectStrokeShader>
astral::gl::StrokeShaderGL3::
create_direct_stroke_shader(uint32_t flags,
                            bool emits_transparent_fragments) const
{
  ColorItemShader::Properties props;

  props
    .emits_partially_covered_fragments(true)
    .emits_transparent_fragments(emits_transparent_fragments);

  ColorItemShaderCreator R(props);
  return create_stroke_shader_implement(astral::type_tag<ColorItemShader>(), R, *this, flags);
}

astral::reference_counted_ptr<astral::MaskStrokeShader>
astral::gl::StrokeShaderGL3::
create_mask_stroke_shader(uint32_t flags) const
{
  MaskItemShaderCreator M;
  return create_stroke_shader_implement(astral::type_tag<MaskItemShader>(), M, *this, flags);
}

astral::gl::StrokeShaderGL3
astral::gl::StrokeShaderGL3::
color_shader_from_mask_shader(void) const
{
  StrokeShaderGL3 return_value;

  return_value.m_type = m_type;
  return_value.m_per_cap_shading = m_per_cap_shading;

  return_value.m_line = color_shader_from_mask_shader_implement(m_line);
  return_value.m_biarc_curve = color_shader_from_mask_shader_implement(m_biarc_curve);
  return_value.m_join = color_shader_from_mask_shader_implement(m_join);
  return_value.m_square_cap = color_shader_from_mask_shader_implement(m_square_cap);
  return_value.m_rounded_cap = color_shader_from_mask_shader_implement(m_rounded_cap);
  return_value.m_cappers = color_shader_from_mask_shader_implement(m_cappers);

  return return_value;
}

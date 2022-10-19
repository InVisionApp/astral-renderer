/*!
 * \file render_engine_gl3_atlas_blitter.cpp
 * \brief file render_engine_gl3_atlas_blitter.cpp
 *
 * Copyright 2020 by InvisionApp.
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

#include <type_traits>
#include <astral/util/gl/gl_get.hpp>
#include <astral/util/gl/gl_vertex_attrib.hpp>
#include <astral/util/gl/gl_context_properties.hpp>
#include <astral/util/ostream_utility.hpp>

#include "render_engine_gl3_atlas_blitter.hpp"

/////////////////////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::AtlasBlitter::PerBlitter methods
void
astral::gl::RenderEngineGL3::Implement::AtlasBlitter::PerBlitter::
init(enum blitter_t tp, enum blitter_fmt_t fmt)
{
  c_string macro_sampler_type, macro_fmt;
  ShaderSource gles_prec;
  ProgramInitializerArray sampler_bindings;
  ShaderSource::MacroSet macros;

  macro_sampler_type = (tp == blitter_texture_2d_src) ?
    "BLITTER_SRC_SAMPLER2D" :
    "BLITTER_SRC_SAMPLER2D_ARRAY";

  sampler_bindings.add_sampler_initializer("astral_surface_src", 0u);

  switch (fmt)
    {
    case blitter_fmt_uint:
      macro_fmt = "BLITTER_RGBA_UINT";
      break;

    case blitter_fmt_non_integer:
      macro_fmt = "BLITTER_RGBA_FLOAT";
      sampler_bindings.add_sampler_initializer("astral_surface_src_filtered", 1u);
      break;

    case blitter_fmt_depth:
      macro_fmt = "BLITTER_DEPTH";
      break;

    default:
      ASTRALfailure("Invalid blitter_fmt");
    }

  if (ContextProperties::is_es())
    {
      gles_prec.add_source("astral_gles_precisions.glsl.resource_string",  ShaderSource::from_resource);
    }

  macros
    .add_macro(macro_sampler_type)
    .add_macro(macro_fmt)
    .add_macro_u32("ASTRAL_BLIT_STC_MASK_PROCESSING", image_blit_stc_mask_processing)
    .add_macro_u32("ASTRAL_BLIT_DIRECT_MASK_PROCESSING", image_blit_direct_mask_processing)
    .add_macro_u32("ASTRAL_BLIT_BIT_COPY", image_processing_none)
    .add_macro_u32("ASTRAL_BLIT_BIT_COPY_ALIAS", image_processing_count)
    .add_macro_u32("ASTRAL_BLIT_DOWNSAMPLE_SIMPLE", image_processing_count + 1u + downsampling_simple);

  m_program = Program::create(ShaderSource()
                              .specify_version(Shader::default_shader_version())
                              .add_source(gles_prec)
                              .add_macros(macros)
                              .add_source("astral_image_atlas_blitter.vert.glsl.resource_string", ShaderSource::from_resource),
                              ShaderSource()
                              .specify_version(Shader::default_shader_version())
                              .add_source(gles_prec)
                              .add_macros(macros)
                              .add_source("astral_image_atlas_blitter.frag.glsl.resource_string", ShaderSource::from_resource),
                              PreLinkActionArray()
                              .add_binding("astral_src", 0)
                              .add_binding("astral_dst", 1)
                              .add_binding("astral_mode", 2)
                              .add_binding("astral_post_process_window", 3),
                              sampler_bindings);
  ASTRALassert(m_program->link_success());

  m_coeff_x_loc = m_program->uniform_location("ASTRAL_PROJ_COEFF_X");
  m_coeff_y_loc = m_program->uniform_location("ASTRAL_PROJ_COEFF_Y");
  m_lod_loc = m_program->uniform_location("LOD");
  ASTRALassert(m_coeff_x_loc != -1 && m_coeff_y_loc != -1 && m_lod_loc != -1);

  if (tp == blitter_texture_2d_array_src)
    {
      m_src_layer_loc = m_program->uniform_location("SRC_LAYER");
      ASTRALassert(m_src_layer_loc != -1);
    }
}

////////////////////////////////////////////////////
// astral::gl::RenderEngineGL3::Implement::AtlasBlitter methods
astral::gl::RenderEngineGL3::Implement::AtlasBlitter::
AtlasBlitter(int number_clip_planes):
  m_number_clip_planes(number_clip_planes),
  m_fbo(0u),
  m_vao(0u),
  m_vbo(0u),
  m_filter_sampler(0u)
{
  /* Create the program to do the blitting */
  get_blitter(blitter_texture_2d_src, blitter_fmt_non_integer).init(blitter_texture_2d_src, blitter_fmt_non_integer);
  get_blitter(blitter_texture_2d_array_src, blitter_fmt_non_integer).init(blitter_texture_2d_array_src, blitter_fmt_non_integer);

  get_blitter(blitter_texture_2d_src, blitter_fmt_uint).init(blitter_texture_2d_src, blitter_fmt_uint);
  get_blitter(blitter_texture_2d_array_src, blitter_fmt_uint).init(blitter_texture_2d_array_src, blitter_fmt_uint);

  get_blitter(blitter_texture_2d_src, blitter_fmt_depth).init(blitter_texture_2d_src, blitter_fmt_depth);
  get_blitter(blitter_texture_2d_array_src, blitter_fmt_depth).init(blitter_texture_2d_array_src, blitter_fmt_depth);

  astral_glGenFramebuffers(1, &m_fbo);
  ASTRALassert(m_fbo != 0u);

  astral_glGenVertexArrays(1, &m_vao);
  ASTRALassert(m_vao != 0u);
  astral_glBindVertexArray(m_vao);

  astral_glGenBuffers(1, &m_vbo);
  ASTRALassert(m_vbo != 0u);
  astral_glBindBuffer(ASTRAL_GL_ARRAY_BUFFER, m_vbo);

  /* starting in C++11, offsetof() macro is guaranteed
   * to work if the struct/class is standard layout
   */
  ASTRALstatic_assert(std::is_standard_layout<BlitVert>::value);
  VertexAttribPointer(0, gl_vertex_attrib_value<vec2>(sizeof(BlitVert), offsetof(BlitVert, m_src)));
  VertexAttribPointer(1, gl_vertex_attrib_value<vec2>(sizeof(BlitVert), offsetof(BlitVert, m_dst)));
  VertexAttribIPointer(2, gl_vertex_attrib_value<unsigned int>(sizeof(BlitVert), offsetof(BlitVert, m_mode)));
  VertexAttribIPointer(3, gl_vertex_attrib_value<ivec4>(sizeof(BlitVert), offsetof(BlitVert, m_post_process_window)));
  astral_glBindVertexArray(0);

  astral_glGenSamplers(1, &m_filter_sampler);
  ASTRALassert(m_filter_sampler != 0u);
  astral_glSamplerParameteri(m_filter_sampler, ASTRAL_GL_TEXTURE_MAG_FILTER, ASTRAL_GL_LINEAR);
  astral_glSamplerParameteri(m_filter_sampler, ASTRAL_GL_TEXTURE_MIN_FILTER, ASTRAL_GL_LINEAR);
}

astral::gl::RenderEngineGL3::Implement::AtlasBlitter::
~AtlasBlitter()
{
  astral_glDeleteFramebuffers(1, &m_fbo);
  astral_glDeleteVertexArrays(1, &m_vao);
  astral_glDeleteBuffers(1, &m_vbo);
  astral_glDeleteSamplers(1, &m_filter_sampler);
}

void
astral::gl::RenderEngineGL3::Implement::AtlasBlitter::
clear_pixels_begin(Texture dst, ivec2 min_corner, ivec2 size)
{
  ASTRALassert(dst.m_texture != 0);

  astral_glBindFramebuffer(ASTRAL_GL_DRAW_FRAMEBUFFER, m_fbo);
  astral_glColorMask(ASTRAL_GL_TRUE, ASTRAL_GL_TRUE, ASTRAL_GL_TRUE, ASTRAL_GL_TRUE);
  astral_glEnable(ASTRAL_GL_SCISSOR_TEST);
  astral_glScissor(min_corner.x(), min_corner.y(), size.x(), size.y());

  if (dst.m_layer >= 0)
    {
      astral_glFramebufferTextureLayer(ASTRAL_GL_DRAW_FRAMEBUFFER, ASTRAL_GL_COLOR_ATTACHMENT0,
                                       dst.m_texture, dst.m_lod, dst.m_layer);
    }
  else
    {
      astral_glFramebufferTexture2D(ASTRAL_GL_DRAW_FRAMEBUFFER, ASTRAL_GL_COLOR_ATTACHMENT0, ASTRAL_GL_TEXTURE_2D, dst.m_texture, dst.m_lod);
    }
}

void
astral::gl::RenderEngineGL3::Implement::AtlasBlitter::
clear_pixels_end(void)
{
  astral_glFramebufferTexture2D(ASTRAL_GL_DRAW_FRAMEBUFFER, ASTRAL_GL_COLOR_ATTACHMENT0, ASTRAL_GL_TEXTURE_2D, 0, 0);
  astral_glFramebufferTextureLayer(ASTRAL_GL_DRAW_FRAMEBUFFER, ASTRAL_GL_COLOR_ATTACHMENT0, 0, 0, 0);
}

void
astral::gl::RenderEngineGL3::Implement::AtlasBlitter::
blit_pixels_implement(enum blitter_fmt_t blit_fmt,
                      Texture src, c_array<const BlitRect> src_rects,
                      Texture dst, uvec2 dst_dims, c_array<const BlitRect> dst_rects,
                      c_array<const process_pixel_t> blit_processings,
                      c_array<const PostProcessWindow> post_process_windows)
{
  ASTRALassert(dst.m_texture != 0);
  ASTRALassert(src.m_texture != 0);

  enum blitter_t tp;
  astral_GLenum attchment_pt;

  dst_dims.x() >>= dst.m_lod;
  dst_dims.y() >>= dst.m_lod;
  tp = (src.m_layer < 0) ?
    blitter_texture_2d_src:
    blitter_texture_2d_array_src;

  ASTRALassert(src_rects.size() == dst_rects.size());
  ASTRALassert(blit_processings.empty() || blit_processings.size() == dst_rects.size());
  ASTRALassert(post_process_windows.empty() || post_process_windows.size() == dst_rects.size());
  ASTRALassert(blit_processings.empty() || blit_fmt == blitter_fmt_non_integer || blit_fmt == blitter_fmt_depth);

  m_tmp_verts.resize(dst_rects.size());
  for (unsigned int r = 0; r < src_rects.size(); ++r)
    {
      ivec4 post_process_window_value;
      process_pixel_t mode(image_processing_none);

      if (!blit_processings.empty())
        {
          mode = blit_processings[r];
        }

      if (post_process_windows.empty())
        {
          BoundingBox<float> bb;

          bb.union_point(src_rects[r].m_pts[0]);
          bb.union_point(src_rects[r].m_pts[1]);
          bb.union_point(src_rects[r].m_pts[2]);
          bb.union_point(src_rects[r].m_pts[3]);

          post_process_window_value.x() = int(bb.as_rect().m_min_point.x()) - 1;
          post_process_window_value.y() = int(bb.as_rect().m_min_point.y()) - 1;
          post_process_window_value.z() = int(bb.as_rect().m_max_point.x()) + 1;
          post_process_window_value.w() = int(bb.as_rect().m_max_point.y()) + 1;
        }
      else
        {
          post_process_window_value.x() = post_process_windows[r].m_min_point.x();
          post_process_window_value.y() = post_process_windows[r].m_min_point.y();
          post_process_window_value.z() = post_process_windows[r].m_max_point.x();
          post_process_window_value.w() = post_process_windows[r].m_max_point.y();
        }

      static const unsigned int corners[] =
        {
          0, 1, 2,
          0, 2, 3
        };

      //std::cout << "\tRect #" << r << ":\n";
      for (unsigned int i = 0; i < 6; ++i)
        {
          m_tmp_verts[r][i].m_src = src_rects[r].m_pts[corners[i]];
          m_tmp_verts[r][i].m_dst = dst_rects[r].m_pts[corners[i]];
          m_tmp_verts[r][i].m_post_process_window = post_process_window_value;
          m_tmp_verts[r][i].m_mode = mode.m_value;

          //std::cout << "\t\tsrc[" << i << "] = " << m_tmp_verts[r][i].m_src
          //        << ", dst[" << i << "] = " << m_tmp_verts[r][i].m_dst
          //        << "\n";
        }
    }

  astral_glBindBuffer(ASTRAL_GL_ARRAY_BUFFER, m_vbo);
  BufferData(ASTRAL_GL_ARRAY_BUFFER, make_c_array(m_tmp_verts), ASTRAL_GL_STREAM_DRAW);

  /* Set the uniforms for the blitter */
  PerBlitter &blitter(get_blitter(tp, blit_fmt));

  blitter.m_program->use_program();
  astral_glUniform1f(blitter.m_coeff_x_loc, 2.0f / static_cast<float>(dst_dims.x()));
  astral_glUniform1f(blitter.m_coeff_y_loc, 2.0f / static_cast<float>(dst_dims.y()));
  astral_glUniform1i(blitter.m_lod_loc, src.m_lod);

  astral_glActiveTexture(ASTRAL_GL_TEXTURE0);
  astral_glBindSampler(0, 0);
  if (tp == blitter_texture_2d_array_src)
    {
      astral_glBindTexture(ASTRAL_GL_TEXTURE_2D_ARRAY, src.m_texture);
      astral_glUniform1i(blitter.m_src_layer_loc, src.m_layer);
    }
  else
    {
      astral_glBindTexture(ASTRAL_GL_TEXTURE_2D, src.m_texture);
    }

  if (blit_fmt == blitter_fmt_non_integer)
    {
      astral_glActiveTexture(ASTRAL_GL_TEXTURE1);
      astral_glBindSampler(1, m_filter_sampler);
      if (tp == blitter_texture_2d_array_src)
        {
          astral_glBindTexture(ASTRAL_GL_TEXTURE_2D_ARRAY, src.m_texture);
        }
      else
        {
          astral_glBindTexture(ASTRAL_GL_TEXTURE_2D, src.m_texture);
        }
      astral_glActiveTexture(ASTRAL_GL_TEXTURE0);
    }

  ASTRALassert(dst.m_texture != src.m_texture);

  for (int i = 0; i < m_number_clip_planes; ++i)
    {
      astral_glDisable(ASTRAL_GL_CLIP_DISTANCE0 + i);
    }

  /* Set the render target */
  astral_glBindFramebuffer(ASTRAL_GL_DRAW_FRAMEBUFFER, m_fbo);
  astral_glViewport(0, 0, dst_dims.x(), dst_dims.y());
  astral_glDisable(ASTRAL_GL_SCISSOR_TEST);
  astral_glDisable(ASTRAL_GL_STENCIL_TEST);
  astral_glDisable(ASTRAL_GL_BLEND);

  if (blit_fmt == blitter_fmt_depth)
    {
      astral_glEnable(ASTRAL_GL_DEPTH_TEST);
      astral_glDepthFunc(ASTRAL_GL_ALWAYS);
      attchment_pt = ASTRAL_GL_DEPTH_STENCIL_ATTACHMENT;
    }
  else
    {
      astral_glColorMask(ASTRAL_GL_TRUE, ASTRAL_GL_TRUE, ASTRAL_GL_TRUE, ASTRAL_GL_TRUE);
      astral_glDisable(ASTRAL_GL_DEPTH_TEST);
      attchment_pt = ASTRAL_GL_COLOR_ATTACHMENT0;
    }

  if (dst.m_layer >= 0)
    {
      astral_glFramebufferTextureLayer(ASTRAL_GL_DRAW_FRAMEBUFFER, attchment_pt,
                                       dst.m_texture, dst.m_lod, dst.m_layer);
    }
  else
    {
      astral_glFramebufferTexture2D(ASTRAL_GL_DRAW_FRAMEBUFFER, attchment_pt,
                                    ASTRAL_GL_TEXTURE_2D, dst.m_texture, dst.m_lod);
    }

  /* draw all those rects */
  astral_glBindVertexArray(m_vao);
  astral_glDrawArrays(ASTRAL_GL_TRIANGLES, 0, 6u * dst_rects.size());
  astral_glBindVertexArray(0);

  /* unbind the dst texture from the fbo so it can be released */
  astral_glFramebufferTexture2D(ASTRAL_GL_DRAW_FRAMEBUFFER, attchment_pt, ASTRAL_GL_TEXTURE_2D, 0, 0);
  astral_glFramebufferTextureLayer(ASTRAL_GL_DRAW_FRAMEBUFFER, attchment_pt, 0, 0, 0);

  /* unbind src texture from GL context as well */
  if (tp == blitter_texture_2d_array_src)
    {
      astral_glBindTexture(ASTRAL_GL_TEXTURE_2D_ARRAY, 0u);
    }
  else
    {
      astral_glBindTexture(ASTRAL_GL_TEXTURE_2D, 0u);
    }

  if (blit_fmt == blitter_fmt_non_integer)
    {
      astral_glActiveTexture(ASTRAL_GL_TEXTURE1);
      astral_glBindSampler(0, m_filter_sampler);
      if (tp == blitter_texture_2d_array_src)
        {
          astral_glBindTexture(ASTRAL_GL_TEXTURE_2D_ARRAY, 0u);
        }
      else
        {
          astral_glBindTexture(ASTRAL_GL_TEXTURE_2D, 0u);
        }
      astral_glActiveTexture(ASTRAL_GL_TEXTURE0);
    }
}

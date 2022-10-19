/*!
 * \file render_engine_gl3_shadow_map.cpp
 * \brief file render_engine_gl3_shadow_map.cpp
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

#include <astral/util/gl/gl_context_properties.hpp>
#include <astral/util/gl/gl_get.hpp>
#include <astral/util/gl/astral_gl.hpp>
#include "render_engine_gl3_shadow_map.hpp"

astral::gl::RenderEngineGL3::Implement::ShadowMapBacking::
ShadowMapBacking(unsigned int width, unsigned int initial_height,
                 FBOBlitter &fbo_blitter, AtlasBlitter &atlas_blitter):
  ShadowMapAtlasBacking(width, initial_height),
  m_fbo_blitter(&fbo_blitter),
  m_atlas_blitter(&atlas_blitter),
  m_shadow_sampler(0u),
  m_linear_sampler(0u)
{
  m_max_texture_size = context_get<astral_GLint>(ASTRAL_GL_MAX_TEXTURE_SIZE);
  create_storage(width, initial_height);

  astral_glGenSamplers(1, &m_shadow_sampler);
  ASTRALassert(m_shadow_sampler != 0u);

  astral_glSamplerParameteri(m_shadow_sampler, ASTRAL_GL_TEXTURE_MIN_FILTER, ASTRAL_GL_LINEAR);
  astral_glSamplerParameteri(m_shadow_sampler, ASTRAL_GL_TEXTURE_MAG_FILTER, ASTRAL_GL_LINEAR);
  astral_glSamplerParameteri(m_shadow_sampler, ASTRAL_GL_TEXTURE_COMPARE_MODE, ASTRAL_GL_COMPARE_REF_TO_TEXTURE);
  astral_glSamplerParameteri(m_shadow_sampler, ASTRAL_GL_TEXTURE_COMPARE_FUNC, ASTRAL_GL_LESS);

  astral_glGenSamplers(1, &m_linear_sampler);
  ASTRALassert(m_linear_sampler != 0u);

  astral_glSamplerParameteri(m_linear_sampler, ASTRAL_GL_TEXTURE_MIN_FILTER, ASTRAL_GL_LINEAR);
  astral_glSamplerParameteri(m_linear_sampler, ASTRAL_GL_TEXTURE_MAG_FILTER, ASTRAL_GL_LINEAR);
}

astral::gl::RenderEngineGL3::Implement::ShadowMapBacking::
~ShadowMapBacking()
{
  astral_glDeleteSamplers(1, &m_shadow_sampler);
  astral_glDeleteSamplers(1, &m_linear_sampler);
}

void
astral::gl::RenderEngineGL3::Implement::ShadowMapBacking::
create_storage(unsigned int width, unsigned int height)
{
  reference_counted_ptr<TextureHolder> texture;
  reference_counted_ptr<DepthStencilBufferGL> buffer;
  ivec2 sz(width, height);

  ASTRALassert(height <= m_max_texture_size);
  ASTRALassert(!m_render_target);

  texture = TextureHolder::create(ASTRAL_GL_DEPTH24_STENCIL8, sz, ASTRAL_GL_NEAREST, ASTRAL_GL_NEAREST, 1);
  m_texture = texture->texture();

  buffer = DepthStencilBufferGL::create(texture, sz);
  m_render_target = RenderTargetGL_Texture::create(nullptr, buffer);
}

void
astral::gl::RenderEngineGL3::Implement::ShadowMapBacking::
flush_gpu(void)
{
  ASTRALassert(m_src_rects.size() == m_dst_rects.size());
  if (!m_src_rects.empty())
    {
      AtlasBlitter::Texture dst_texture;
      uvec2 sz(width(), height());

      ASTRALassert(m_gpu_texture.m_texture != 0u);

      dst_texture
        .lod(0)
        .texture(m_texture)
        .layer(-1);

      m_atlas_blitter->blit_pixels_depth(m_gpu_texture, make_c_array(m_src_rects),
                                         dst_texture, sz, make_c_array(m_dst_rects));

      m_gpu_texture = AtlasBlitter::Texture();
      m_src_rects.clear();
      m_dst_rects.clear();
    }

  ASTRALassert(m_gpu_texture.m_texture == 0u);
  ASTRALassert(m_src_rects.empty());
  ASTRALassert(m_dst_rects.empty());
}

void
astral::gl::RenderEngineGL3::Implement::ShadowMapBacking::
copy_pixels(uvec2 dst_location, uvec2 size,
            DepthStencilBuffer &src, uvec2 src_location)
{
  DepthStencilBufferGL *gl_src;
  astral_GLuint src_texture;
  int src_layer;

  ASTRALassert(dynamic_cast<DepthStencilBufferGL*>(&src));
  gl_src = static_cast<DepthStencilBufferGL*>(&src);
  src_texture = gl_src->texture()->texture();
  src_layer = gl_src->layer();

  if (m_gpu_texture.m_texture != src_texture
      || m_gpu_texture.m_layer != src_layer)
    {
      flush_gpu();
      m_gpu_texture
        .texture(src_texture)
        .layer(src_layer)
        .lod(0);
    }

  Rect srcR, dstR;

  srcR.m_min_point = vec2(src_location);
  srcR.m_max_point = srcR.m_min_point + vec2(size);

  dstR.m_min_point = vec2(dst_location);
  dstR.m_max_point = dstR.m_min_point + vec2(size);

  m_src_rects.push_back(srcR);
  m_dst_rects.push_back(dstR);
}

unsigned int
astral::gl::RenderEngineGL3::Implement::ShadowMapBacking::
on_resize(unsigned int new_height)
{
  reference_counted_ptr<RenderTargetGL_Texture> prev_rt;
  astral_GLbitfield blit_mask;
  astral_GLuint prev_texture;
  ivec2 sz(width(), height());

  prev_rt.swap(m_render_target);
  create_storage(width(), new_height);

  blit_mask = ASTRAL_GL_DEPTH_BUFFER_BIT;
  prev_texture = prev_rt->depth_texture()->texture();

  m_fbo_blitter->blit(blit_mask, prev_texture, m_texture, sz);

  return new_height;
}

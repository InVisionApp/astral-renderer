/*!
 * \file render_engine_gl3_atlas_blitter.hpp
 * \brief file render_engine_gl3_atlas_blitter.hpp
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

#ifndef ASTRAL_RENDER_ENGINE_GL3_ATLAS_BLITTER_HPP
#define ASTRAL_RENDER_ENGINE_GL3_ATLAS_BLITTER_HPP

#include <astral/util/c_array.hpp>
#include <astral/util/rect.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/gl/gl_program.hpp>
#include <astral/util/gl/astral_gl.hpp>
#include <astral/renderer/gl3/render_engine_gl3.hpp>

#include "render_engine_gl3_implement.hpp"

/* Class to handle blitting for both RenderEngineGL3::Implement::ImageBacking
 * and RenderEngineGL3::Implement::ImageBacking
 */
class astral::gl::RenderEngineGL3::Implement::AtlasBlitter:public reference_counted<AtlasBlitter>::non_concurrent
{
public:
  /* Class to specify a texture, a layer and mimpmap levels */
  class Texture
  {
  public:
    Texture(void):
      m_texture(0u),
      m_layer(-1),
      m_lod(0)
    {}

    Texture&
    texture(astral_GLuint v)
    {
      m_texture = v;
      return *this;
    }

    Texture&
    layer(int v)
    {
      m_layer = v;
      return *this;
    }

    Texture&
    lod(int v)
    {
      m_lod = v;
      return *this;
    }

    /* GL name of the texture; texture bind target
     * must be either ASTRAL_GL_TEXTURE_2D or ASTRAL_GL_TEXTURE_2D_ARRAY
     */
    astral_GLuint m_texture;

    /* What layer of the texure; a negative value inidcates
     * that the texture bind target is ASTRAL_GL_TEXTURE_2D
     */
    int m_layer;

    /* what LOD, i.e. mipmap levels */
    int m_lod;
  };

  /* A BlitRect is different than a source rect
   * in that is just specifies the 4 vertices of
   * the rect; a caller must make sure that the
   * 4 vertices match in meaning from the src to
   * dst. In addition, the triangles used to blit
   * the rect are [0, 1, 2] and [0, 2, 3].
   */
  class BlitRect
  {
  public:
    template<typename T>
    BlitRect(const RectT<T> &rect)
    {
      m_pts[0] = vec2(rect.min_x(), rect.min_y());
      m_pts[1] = vec2(rect.max_x(), rect.min_y());
      m_pts[2] = vec2(rect.max_x(), rect.max_y());
      m_pts[3] = vec2(rect.min_x(), rect.max_y());
    }

    void
    permute_xy(void)
    {
      for (unsigned int i = 0; i < 4; ++i)
        {
          std::swap(m_pts[i].x(), m_pts[i].y());
        }
    }

    vecN<vec2, 4> m_pts;
  };

  /* Class to specify the window in which to restrict sampling
   * neighboring texels when performing edge detection.
   */
  class PostProcessWindow
  {
  public:
    /* Default ctor where PostProcessWindow's
     * range is large negative to large positive
     * to effectively say, ignore.
     */
    PostProcessWindow(void):
      m_min_point(-0xFFFF, 0xFFFF),
      m_max_point(-0xFFFF, 0xFFFF)
    {}

    PostProcessWindow(const RectT<int> &rect):
      m_min_point(rect.m_min_point),
      m_max_point(rect.m_max_point)
    {
      ASTRALassert(m_min_point.x() <= m_max_point.x());
      ASTRALassert(m_min_point.y() <= m_max_point.y());
    }

    void
    permute_xy(void)
    {
      std::swap(m_min_point.x(), m_min_point.y());
      std::swap(m_max_point.x(), m_max_point.y());
    }

    ivec2 m_min_point, m_max_point;
  };

  /*!
   * Class to describe the nature of a pixel copy or downsample
   */
  class process_pixel_t
  {
  public:
    process_pixel_t(enum image_blit_processing_t I):
      m_value(I)
    {}

    process_pixel_t(enum downsampling_processing_t I):
      m_value(image_processing_count + 1 + I)
    {
    }

    bool
    operator==(process_pixel_t rhs) const
    {
      return m_value == rhs.m_value;
    }

    /* Meaning:
     *  1. if m_value <= image_processing_count then m_value
     *     is an enumeration from image_blit_processing_t
     *  2. otherwise, m_value - (1 + image_processing_count)
     *     gives a value of the enumeration downsampling_processing_t
     */
    uint32_t m_value;
  };

  explicit
  AtlasBlitter(int number_clip_planes);

  ~AtlasBlitter();

  /* Clear the named pixels from the named texture at the named
   * location using FBO's and glClearBufferfv(). Does NOT restore GL
   * state on exit.
   * \param dst texture and layer of it to clear pixels
   * \param size size of the clear
   * \param clear_color color to clear to
   */
  void
  clear_pixels(Texture dst, ivec2 min_corner, ivec2 size, vec4 clear_color)
  {
    clear_pixels_begin(dst, min_corner, size);
    astral_glClearBufferfv(ASTRAL_GL_COLOR, 0, clear_color.c_ptr());
    clear_pixels_end();
  }

  /* Clear the named pixels from the named texture at the named
   * location using FBO's and glClearBufferuiv(). Does NOT restore GL
   * state on exit.
   * \param dst texture and layer of it to clear pixels
   * \param size size of the clear
   * \param clear_color color to clear to
   */
  void
  clear_pixels(Texture dst, ivec2 min_corner, ivec2 size, vecN<astral_GLuint, 4> clear_color)
  {
    clear_pixels_begin(dst, min_corner, size);
    astral_glClearBufferuiv(ASTRAL_GL_COLOR, 0, clear_color.c_ptr());
    clear_pixels_end();
  }

  /* Clear the named pixels from the named texture at the named
   * location using FBO's and glClearBufferiv(). Does NOT restore GL
   * state on exit.
   * \param dst texture and layer of it to clear pixels
   * \param size size of the clear
   * \param clear_color color to clear to
   */
  void
  clear_pixels(Texture dst, ivec2 min_corner, ivec2 size, vecN<astral_GLint, 4> clear_color)
  {
    clear_pixels_begin(dst, min_corner, size);
    astral_glClearBufferiv(ASTRAL_GL_COLOR, 0, clear_color.c_ptr());
    clear_pixels_end();
  }

  /* Copy pixels from one texture to another texture where the format
   * of both the source and destination are non-integer formats. Does
   * NOT restore GL state on exit.
   * \param src source texture, layer and LOD of it
   * \param src_rects list of rects from which to blit
   * \param dst destination texture, layer and LOD of it
   * \param dst_dims the dimensions of LOD 0 of the destination texture
   * \param dst_rects destination rects to blit to
   * \param blit_processing specifies if and how the pixels are
   *                        processed in the blit. An empty array
   *                        indicates that pixels are to be bits-wise
   *                        copied.
   * \param post_process_windows if non-empty, when doing edge detection,
   *                             do not sample outside of this window
   */
  void
  blit_pixels(Texture src, c_array<const BlitRect> src_rects,
              Texture dst, uvec2 dst_dims, c_array<const BlitRect> dst_rects,
              c_array<const process_pixel_t> blit_processings,
              c_array<const PostProcessWindow> post_process_windows)
  {
    blit_pixels_implement(blitter_fmt_non_integer,
                          src, src_rects, dst, dst_dims, dst_rects,
                          blit_processings, post_process_windows);
  }

  void
  blit_pixels(Texture src, const BlitRect &src_rect,
              Texture dst, uvec2 dst_dims, const BlitRect &dst_rect,
              process_pixel_t blit_processing,
              const PostProcessWindow &post_process_window)
  {
    blit_pixels(src, c_array<const BlitRect>(&src_rect, 1),
                dst, dst_dims, c_array<const BlitRect>(&dst_rect, 1),
                c_array<const process_pixel_t>(&blit_processing, 1),
                c_array<const PostProcessWindow>(&post_process_window, 1));
  }

  void
  blit_pixels(Texture src, const BlitRect &src_rect,
              Texture dst, uvec2 dst_dims, const BlitRect &dst_rect)
  {
    blit_pixels(src, c_array<const BlitRect>(&src_rect, 1),
                dst, dst_dims, c_array<const BlitRect>(&dst_rect, 1),
                c_array<const process_pixel_t>(),
                c_array<const PostProcessWindow>());
  }

  /* Copy pixels from one texture to another texture where the format
   * of both the source and destination are unsigned integer formats.
   * Does NOT restore GL state on exit.
   * \param src source texture, layer and LOD of it
   * \param src_rects list of rects from which to blit
   * \param dst destination texture, layer and LOD of it
   * \param dst_dims the dimensions of LOD 0 of the destination texture
   * \param dst_rects destination rects to blit to
   */
  void
  blit_pixels_uint(Texture src, c_array<const BlitRect> src_rects,
                   Texture dst, uvec2 dst_dims, c_array<const BlitRect> dst_rects)
  {
    blit_pixels_implement(blitter_fmt_uint,
                          src, src_rects, dst, dst_dims, dst_rects,
                          c_array<const process_pixel_t>(),
                          c_array<const PostProcessWindow>());
  }

  void
  blit_pixels_uint(Texture src, const BlitRect &src_rect,
                   Texture dst, uvec2 dst_dims, const BlitRect &dst_rect)
  {
    blit_pixels_uint(src, c_array<const BlitRect>(&src_rect, 1),
                     dst, dst_dims, c_array<const BlitRect>(&dst_rect, 1));
  }

  /* Copy pixels from one texture to another texture where the format
   * of both the source and destination are a ASTRAL_GL_DEPTH format. Note
   * that does not copy stencil pixels even if both have a format that
   * include stencil pixels. Does NOT restore GL state on exit.
   * \param src source texture, layer and LOD of it
   * \param src_rects list of rects from which to blit
   * \param dst destination texture, layer and LOD of it
   * \param dst_dims the dimensions of LOD 0 of the destination texture
   * \param dst_rects destination rects to blit to
   */
  void
  blit_pixels_depth(Texture src, c_array<const BlitRect> src_rects,
                    Texture dst, uvec2 dst_dims, c_array<const BlitRect> dst_rects)
  {
    blit_pixels_implement(blitter_fmt_depth,
                          src, src_rects, dst, dst_dims, dst_rects,
                          c_array<const process_pixel_t>(),
                          c_array<const PostProcessWindow>());
  }

  void
  blit_pixels_depth(Texture src, const BlitRect &src_rect,
                   Texture dst, uvec2 dst_dims, const BlitRect &dst_rect)
  {
    blit_pixels_depth(src, c_array<const BlitRect>(&src_rect, 1),
                      dst, dst_dims, c_array<const BlitRect>(&dst_rect, 1));
  }

private:
  enum blitter_t:uint32_t
    {
      blitter_texture_2d_src,
      blitter_texture_2d_array_src,
    };

  enum blitter_fmt_t:uint32_t
    {
      blitter_fmt_non_integer,
      blitter_fmt_uint,
      blitter_fmt_depth,
    };

  class PerBlitter
  {
  public:
    void
    init(enum blitter_t, enum blitter_fmt_t);

    reference_counted_ptr<Program> m_program;
    astral_GLint m_coeff_x_loc, m_coeff_y_loc, m_lod_loc, m_src_layer_loc;
  };

  class BlitVert
  {
  public:
    vec2 m_src, m_dst;
    ivec4 m_post_process_window;
    uint32_t m_mode;
  };

  void
  clear_pixels_begin(Texture dst, ivec2 min_corner, ivec2 size);

  void
  clear_pixels_end(void);

  void
  blit_pixels_implement(enum blitter_fmt_t blit_fmt,
                        Texture src, c_array<const BlitRect> src_rects,
                        Texture dst, uvec2 dst_dims, c_array<const BlitRect> dst_rects,
                        c_array<const process_pixel_t> blit_processings,
                        c_array<const PostProcessWindow> post_process_windows);

  PerBlitter&
  get_blitter(enum blitter_t b, enum blitter_fmt_t f)
  {
    return m_blitters[b][f];
  }

  int m_number_clip_planes;
  astral_GLuint m_fbo, m_vao, m_vbo, m_filter_sampler;
  vecN<vecN<PerBlitter, 3>, 2> m_blitters;
  std::vector<vecN<BlitVert, 6>> m_tmp_verts;
};

#endif

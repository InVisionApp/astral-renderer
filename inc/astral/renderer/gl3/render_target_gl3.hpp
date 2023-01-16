/*!
 * \file render_target_gl3.hpp
 * \brief file render_target_gl3.hpp
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

#ifndef ASTRAL_RENDER_TARGET_GL3_HPP
#define ASTRAL_RENDER_TARGET_GL3_HPP

#include <astral/renderer/render_target.hpp>
#include <astral/util/gl/astral_gl.hpp>

namespace astral
{
  namespace gl
  {
/*!\addtogroup gl
 * @{
 */

    /*!
     * \brief
     * A TextureHolder is a reference counted object
     * that holds a GL texture name.
     */
    class TextureHolder:public reference_counted<TextureHolder>::non_concurrent
    {
    public:
      /*!
       * enumeratiomn to specify to delete or not delete the
       * backing texture when the dtor of TextureHolder executes.
       */
      enum dtor_behaviour_t:uint32_t
        {
          /*!
           * On dtor of TextureHolder, the texture it holds
           * is not deleted
           */
          dont_delete_texture_on_dtor = 0,

          /*!
           * On dtor of TextureHolder, the texture it holds
           * is deleted
           */
          delete_texture_on_dtor,
        };

      /*!
       * Ctor
       * \param tex GL name of pre-existing GL-texture; the texture
       *            must not be deleted for the lifetime of the
       *            created TextureHolder.
       * \param dtor_behaviour specifies behaviour of dtor on the texture
       */
      static
      reference_counted_ptr<TextureHolder>
      create(astral_GLuint tex, enum dtor_behaviour_t dtor_behaviour)
      {
        return ASTRALnew TextureHolder(tex, dtor_behaviour);
      }

      /*!
       * Ctor. Creates the held texture as a ASTRAL_GL_TEXTURE_2D
       * \param internal_format internal format of the texture
       * \param sz size of the texture
       * \param min_filter minificaiton filter
       * \param mag_filter magnification filter
       * \param number_lod number of level of detail mipmaps, a
       *                   value of 1 indicates no mipmapping support.
       * \param dtor_behaviour specifies behaviour of dtor on the texture
       */
      static
      reference_counted_ptr<TextureHolder>
      create(astral_GLenum internal_format, ivec2 sz,
             astral_GLenum min_filter, astral_GLenum mag_filter,
             unsigned int number_lod,
             enum dtor_behaviour_t dtor_behaviour = delete_texture_on_dtor)
      {
        return ASTRALnew TextureHolder(internal_format, sz, min_filter, mag_filter, number_lod, dtor_behaviour);
      }

      /*!
       * Ctor. Creates the held texture as a ASTRAL_GL_TEXTURE_2D_ARRAY
       * \param internal_format internal format of the texture
       * \param sz size of the texture
       * \param min_filter minificaiton filter
       * \param mag_filter magnification filter
       * \param number_lod number of level of detail mipmaps, a
       *                   value of 1 indicates no mipmapping support.
       * \param dtor_behaviour specifies behaviour of dtor on the texture
       */
      static
      reference_counted_ptr<TextureHolder>
      create(astral_GLenum internal_format, ivec3 sz,
             astral_GLenum min_filter, astral_GLenum mag_filter,
             unsigned int number_lod,
             enum dtor_behaviour_t dtor_behaviour = delete_texture_on_dtor)
      {
        return ASTRALnew TextureHolder(internal_format, sz, min_filter, mag_filter, number_lod, dtor_behaviour);
      }

      ~TextureHolder();

      /*!
       * Returns the GL texture.
       */
      astral_GLuint
      texture(void) const
      {
        return m_texture;
      }

    private:
      TextureHolder(astral_GLuint tex, enum dtor_behaviour_t dtor_behaviour):
        m_texture(tex),
        m_dtor_behaviour(dtor_behaviour)
      {
        ASTRALassert(m_texture != 0);
      }

      TextureHolder(astral_GLenum internal_format,
                    ivec2 sz,
                    astral_GLenum min_filter,
                    astral_GLenum mag_filter,
                    unsigned int number_lod,
                    enum dtor_behaviour_t dtor_behaviour);

      TextureHolder(astral_GLenum internal_format,
                    ivec3 sz,
                    astral_GLenum min_filter,
                    astral_GLenum mag_filter,
                    unsigned int number_lod,
                    enum dtor_behaviour_t dtor_behaviour);

      astral_GLuint m_texture;
      enum dtor_behaviour_t m_dtor_behaviour;
    };

    /*!
     * \brief
     * A ColorBufferGL represents astral::ColorBuffer implemented for
     * the OpenGL API family. The buffer's backing store is a GL
     * texture.
     */
    class ColorBufferGL:public ColorBuffer
    {
    public:
      /*!
       * Create a ColorBufferGL backed by a pre-existing GL texture,
       * whose texture bind target is ASTRAL_GL_TEXTURE_2D. The backing texture
       * should be an RGBA texture.
       * \param tex Holder of the pre-exising texture
       * \param sz dimensions of the pre-existing texture
       */
      static
      reference_counted_ptr<ColorBufferGL>
      create(const reference_counted_ptr<TextureHolder> &tex, ivec2 sz)
      {
        return ASTRALnew ColorBufferGL(sz, tex, -1);
      }

      /*!
       * Create a ColorBufferGL backed by a layer of a pre-existing
       * texture whose bind target is ASTRAL_GL_TEXTURE_2D_ARRAY.
       * \param tex Holder of the pre-exising texture
       * \param layer what layer backs the ColorBufferGL
       * \param sz dimensions of the pre-existing texture
       */
      static
      reference_counted_ptr<ColorBufferGL>
      create(const reference_counted_ptr<TextureHolder> &tex,
             int layer, ivec2 sz)
      {
        return ASTRALnew ColorBufferGL(sz, tex, layer);
      }

      /*!
       * Create a depth ASTRAL_GL_TEXTURE_2D and ColorBufferGL backed
       * by it; the created texture will not have mipmaps.
       * \param sz size of texture
       * \param min_filter minificaiton filter, must be ASTRAL_GL_LINEAR or ASTRAL_GL_NEAREST
       * \param mag_filter magnification filter, must be ASTRAL_GL_LINEAR or ASTRAL_GL_NEAREST
       */
      static
      reference_counted_ptr<ColorBufferGL>
      create(ivec2 sz, astral_GLenum min_filter, astral_GLenum mag_filter);

      /*!
       * Create a depth ASTRAL_GL_TEXTURE_2D and ColorBufferGL backed
       * by it; the created texture will not have mipmaps and filter
       * with linear filtering.
       * \param sz size of texture
       */
      static
      reference_counted_ptr<ColorBufferGL>
      create(ivec2 sz)
      {
        return create(sz, ASTRAL_GL_LINEAR, ASTRAL_GL_LINEAR);
      }

      ~ColorBufferGL()
      {}

      /*!
       * Returns the binding target for the backing texture.
       */
      astral_GLenum
      bind_target(void) const
      {
        return (m_layer == -1) ?
          ASTRAL_GL_TEXTURE_2D :
          ASTRAL_GL_TEXTURE_2D_ARRAY;
      }

      /*!
       * If bind_target() returns ASTRAL_GL_TEXTURE_2D_ARRAY, returns
       * what layer of the texture array that the color buffer
       * is backed by. A return value of -1 means that the
       * bind target is ASTRAL_GL_TEXTURE_2D.
       */
      astral_GLint
      layer(void) const
      {
        return m_layer;
      }

      /*!
       * Returns the \ref TextureHolder of the backing texture.
       */
      const reference_counted_ptr<TextureHolder>&
      texture(void) const
      {
        return m_texture;
      }

    private:
      ColorBufferGL(ivec2 sz, reference_counted_ptr<TextureHolder> ptexture,
                    int player):
        ColorBuffer(sz),
        m_texture(ptexture),
        m_layer(player)
      {}

      reference_counted_ptr<TextureHolder> m_texture;
      int m_layer;
    };

    /*!
     * \brief
     * A DepthStencilBufferGL represents astral::DepthStencilBuffer
     * implemented for the OpenGL API family. The buffer's backing
     * store is a GL texture which has a depth-stencil format.
     */
    class DepthStencilBufferGL:public DepthStencilBuffer
    {
    public:
      /*!
       * Create a DepthStencilBufferGL backed by a pre-existing GL texture,
       * whose texture bind target is ASTRAL_GL_TEXTURE_2D.
       * \param tex Holder of the pre-exising texture
       * \param sz dimensions of the pre-existing texture
       */
      static
      reference_counted_ptr<DepthStencilBufferGL>
      create(const reference_counted_ptr<TextureHolder> &tex, ivec2 sz)
      {
        return ASTRALnew DepthStencilBufferGL(sz, tex, -1);
      }

      /*!
       * Create a DepthStencilBufferGL backed by a layer of a pre-existing
       * texture whose bind target is ASTRAL_GL_TEXTURE_2D_ARRAY.
       * \param tex Holder of the pre-exising texture
       * \param layer what layer backs the DepthStencilBufferGL
       * \param sz dimensions of the pre-existing texture
       */
      static
      reference_counted_ptr<DepthStencilBufferGL>
      create(const reference_counted_ptr<TextureHolder> &tex,
             int layer, ivec2 sz)
      {
        return ASTRALnew DepthStencilBufferGL(sz, tex, layer);
      }

      /*!
       * Create a depth ASTRAL_GL_TEXTURE_2D and DepthStencilBufferGL backed
       * by it; the created texture will not have mipmaps.
       * \param sz size of texture
       * \param min_filter minificaiton filter, must be ASTRAL_GL_LINEAR or ASTRAL_GL_NEAREST
       * \param mag_filter magnification filter, must be ASTRAL_GL_LINEAR or ASTRAL_GL_NEAREST
       */
      static
      reference_counted_ptr<DepthStencilBufferGL>
      create(ivec2 sz, astral_GLenum min_filter, astral_GLenum mag_filter);

      /*!
       * Create a depth ASTRAL_GL_TEXTURE_2D and DepthStencilBufferGL backed
       * by it; the created texture will not have mipmaps and use nearest
       * filtering for sampling.
       * \param sz size of texture
       */
      static
      reference_counted_ptr<DepthStencilBufferGL>
      create(ivec2 sz)
      {
        return create(sz, ASTRAL_GL_NEAREST, ASTRAL_GL_NEAREST);
      }

      ~DepthStencilBufferGL()
      {}

      /*!
       * Returns the binding target for the backing texture.
       */
      astral_GLenum
      bind_target(void) const
      {
        return (m_layer == -1) ?
          ASTRAL_GL_TEXTURE_2D :
          ASTRAL_GL_TEXTURE_2D_ARRAY;
      }

      /*!
       * If bind_target() returns ASTRAL_GL_TEXTURE_2D_ARRAY, returns
       * what layer of the texture array that the color buffer
       * is backed by. A return value of -1 means that the
       * bind target is ASTRAL_GL_TEXTURE_2D.
       */
      astral_GLint
      layer(void) const
      {
        return m_layer;
      }

      /*!
       * Returns the \ref TextureHolder of the backing texture.
       */
      const reference_counted_ptr<TextureHolder>&
      texture(void) const
      {
        return m_texture;
      }

    private:
      DepthStencilBufferGL(ivec2 sz, reference_counted_ptr<TextureHolder> ptexture,
                           int player):
        DepthStencilBuffer(sz),
        m_texture(ptexture),
        m_layer(player)
      {}

      reference_counted_ptr<TextureHolder> m_texture;
      int m_layer;
    };

    /*!
     * \brief
     * Common base class for RenderTarget derived classes used
     * by a GL engine.
     */
    class RenderTargetGL:public RenderTarget
    {
    public:
      /*!
       * Specifies the y-coordinate convention of the pixels
       * rendered.
       */
      enum y_coordinate_convention_t
        {
          /*!
           * Use the convention that pixel y = 0 is the
           * top of the surface when rendering. Recall that
           * the Astral convention is that y = 0 is the
           * top of a surface. Thus, if this surface is
           * to be consumed by Astral, for example via
           * Image::copy_pixels(), this convention should
           * be used.
           */
          pixel_y_zero_is_top,

          /*!
           * Use the convention that pixel y = 0 is the
           * bottom of the surface when rendering. Recall
           * that in GL the bottom pixel of a framebuffer
           * is y = 0. This convention should be used when
           * rendering to a window system surface or to a
           * texture to be consumed by something else that
           * expects the y = 0 to be the bottom.
           */
          pixel_y_zero_is_bottom,
        };

      /*!
       * Ctor
       * \param cb color buffer of the render target
       * \param ds depth buffer of the render target
       */
      RenderTargetGL(const reference_counted_ptr<ColorBuffer> &cb,
                     const reference_counted_ptr<DepthStencilBuffer> &ds):
        RenderTarget(cb, ds),
        m_y_coordinate_convention(pixel_y_zero_is_top)
      {}

      /*!
       * Imlemented by a derived class to return the name of the
       * GL Framebuffer object; this is the FBO that is used by
       * the backend for rendering.
       */
      virtual
      astral_GLuint
      fbo(void) const = 0;

      /*!
       * Convention to follow for the y-coordinate
       */
      enum y_coordinate_convention_t m_y_coordinate_convention;

    private:
      virtual
      void
      read_color_buffer_implement(ivec2 location, ivec2 size, c_array<u8vec4> dst) const override;
    };

    /*!
     * \brief
     * astral::gl::RenderTargetGL_Texture is the implementation for
     * astral::gl::RenderTargetGL to render to a texture.
     */
    class RenderTargetGL_Texture:public RenderTargetGL
    {
    public:
      /*!
       * Ctor. Atleast one of cb or ds must not be null.
       * \param cb color buffer to render to; if non-null, size must match size of ds.
       * \param ds depth buffer to render to; if non-null, size must match size of cb.
       */
      static
      reference_counted_ptr<RenderTargetGL_Texture>
      create(const reference_counted_ptr<ColorBufferGL> &cb,
             const reference_counted_ptr<DepthStencilBufferGL> &ds);

      ~RenderTargetGL_Texture();

      virtual
      astral_GLuint
      fbo(void) const override final
      {
        return m_fbo;
      }

      /*!
       * Returns the texture that backs the depth buffer;
       * if there is no such texture, then returns nullptr.
       */
      reference_counted_ptr<TextureHolder>
      depth_texture(void) const
      {
        return m_depth_texture;
      }

      /*!
       * Returns the texture that backs the color buffer;
       * if there is no such texture, then returns nullptr.
       */
      reference_counted_ptr<TextureHolder>
      color_texture(void) const
      {
        return m_color_texture;
      }

    private:
      RenderTargetGL_Texture(astral_GLuint fbo,
                             const reference_counted_ptr<ColorBufferGL> &cb,
                             const reference_counted_ptr<DepthStencilBufferGL> &ds);

      astral_GLuint m_fbo;
      reference_counted_ptr<TextureHolder> m_color_texture, m_depth_texture;
    };

    /*!
     * \brief
     * astral::gl::RenderTargetGL_DefaultFBO is the implementation for
     * astral::gl::RenderTargetGL to render to the default framebuffer
     */
    class RenderTargetGL_DefaultFBO:public RenderTargetGL
    {
    public:
      /*!
       * Ctor.
       * \param sz dimensions of the default framebuffer
       */
      static
      reference_counted_ptr<RenderTargetGL_DefaultFBO>
      create(ivec2 sz);

      virtual
      astral_GLuint
      fbo(void) const override final
      {
        return 0u;
      }

    private:
      explicit
      RenderTargetGL_DefaultFBO(const reference_counted_ptr<ColorBuffer> &cb,
                                const reference_counted_ptr<DepthStencilBuffer> &ds):
        RenderTargetGL(cb, ds)
      {
        m_y_coordinate_convention = pixel_y_zero_is_bottom;
      }
    };

/*! @} */
  }
}

#endif

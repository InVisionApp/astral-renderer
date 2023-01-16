/*!
 * \file render_target.hpp
 * \brief file render_target.hpp
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

#ifndef ASTRAL_RENDER_TARGET_HPP
#define ASTRAL_RENDER_TARGET_HPP

#include <astral/util/vecN.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/reference_counted.hpp>
#include <astral/renderer/render_enums.hpp>

namespace astral
{
  class Renderer;
  class RenderBackend;
  class RenderTarget;

  namespace detail
  {
    ///@cond

    /* The only purpose of this class it to allow
     * for a RenderBackend or Renderer to mark that
     * the RenderTarget is being rendered to.
     * \tparam T must be Renderer or RenderBackend
     */
    template<typename T>
    class RenderTargetStatus
    {
    private:
      friend class astral::Renderer;
      friend class astral::RenderBackend;
      friend class astral::RenderTarget;

      explicit
      RenderTargetStatus(T *active):
        m_active(active)
      {}

      /* If m_active_renderer is non-null, then it is illegal to call
       * RenderTarget::read_color_buffer().
       */
       T *m_active;
    };

    template<typename T>
    class RenderTargetStatusQuery
    {
    private:
      friend class astral::Renderer;
      friend class astral::RenderBackend;
      friend class astral::RenderTarget;

      RenderTargetStatusQuery(void)
      {}
    };

    typedef RenderTargetStatus<RenderBackend> RenderTargetRenderBackendStatus;
    typedef RenderTargetStatusQuery<RenderBackend> RenderTargetRenderBackendStatusQuery;

    typedef RenderTargetStatus<Renderer> RenderTargetRendererStatus;
    typedef RenderTargetStatusQuery<Renderer> RenderTargetRendererStatusQuery;
    ///@endcond
  }

/*!\addtogroup Renderer
 * @{
 */

  /*!
   * \brief
   * Represents an offscreen color buffer to which to render.
   * Recall that rendering results are with alpha pre-multiplied.
   */
  class ColorBuffer:public reference_counted<ColorBuffer>::non_concurrent
  {
  public:
    virtual
    ~ColorBuffer()
    {}

    /*!
     * Returns the size of the \ref ColorBuffer
     */
    ivec2
    size(void) const
    {
      return m_size;
    }

  protected:
    /*!
     * Ctor.
     * \param sz the size of the buffer
     */
    explicit
    ColorBuffer(ivec2 sz):
      m_size(sz)
    {}

  private:
    ivec2 m_size;
  };

  /*!
   * \brief
   * Represents an offscreen depth-stencil buffer to which to render.
   */
  class DepthStencilBuffer:public reference_counted<DepthStencilBuffer>::non_concurrent
  {
  public:
    virtual
    ~DepthStencilBuffer()
    {}

    /*!
     * Returns the size of the \ref ColorBuffer
     */
    ivec2
    size(void) const
    {
      return m_size;
    }

  protected:
    /*!
     * Ctor.
     * \param sz the size of the buffer
     */
    explicit
    DepthStencilBuffer(ivec2 sz):
      m_size(sz)
    {}

  private:
    ivec2 m_size;
  };

  /*!
   * \brief
   * An astral:: RenderTarget represents a render target consisting of a
   *   - an optional depth-stencil buffer
   *   - an optianal color buffer
   *   - 3D API-specifc object
   *
   * If both a color and depth-stencil buffer are present, their
   * dimensions must match. It is an error for neither to be present.
   *
   * The convention in Astral is that y = 0 is the top of the surface.
   */
  class RenderTarget:public reference_counted<RenderTarget>::non_concurrent
  {
  public:
    virtual
    ~RenderTarget()
    {}

    /*!
     * Returns the render-target size.
     */
    ivec2
    size(void) const
    {
      return m_size;
    }

    /*!
     * Returns the location of the viewport used for the \ref
     * RenderTarget with the convention that y = 0 is the top.
     * Value is initialized to (0, 0).
     */
    ivec2
    viewport_xy(void) const
    {
      return m_viewport_xy;
    }

    /*!
     * Set the location of the viewport used for the \ref
     * RenderTarget with the convention that y = 0 is the top.
     * Value is initialized to (0, 0). One cannot change the
     * viewport location while the astral::RenderTarget is
     * active().
     * \param v new value for viewport origin
     */
    void
    viewport_xy(ivec2 v)
    {
      ASTRALassert(!active());
      m_viewport_xy = v;
    }

    /*!
     * Returns the size of the viewport used for the \ref
     * RenderTarget with the convention that y = 0 is the top.
     * Value is initialized to size().
     */
    ivec2
    viewport_size(void) const
    {
      return m_viewport_size;
    }

    /*!
     * Set the size of the viewport used for the \ref
     * RenderTarget with the convention that y = 0 is the top.
     * Value is initialized to size(). One cannot change the
     * viewport size while the astral::RenderTarget is
     * active().
     * \param v new value for viewport size
     */
    void
    viewport_size(ivec2 v)
    {
      ASTRALassert(!active());
      m_viewport_size = v;
    }

    /*!
     * Returns true if and only if a color buffer is attached
     * to this astral::RenderTarget
     */
    bool
    has_color_buffer(void)
    {
      return m_color_buffer;
    }

    /*!
     * Returns true if and only if a depth stencil buffer is attached
     * to this astral::RenderTarget
     */
    bool
    has_depth_stencil_buffer(void)
    {
      return m_depth_buffer;
    }

    /*!
     * Read pixels from the astral::ColorBuffer of this astral::RenderTarget.
     * It is illegal to call read_color_buffer() while the astral::RenderTarget
     * is active().
     *
     * \param read_location location in pixels within color buffer from which to read
     * \param read_size size in pixels of region to read
     * \param dst location to which to write the pixels; the size of dst
     *            must be atleast size.x() * size.y().
     */
    void
    read_color_buffer(ivec2 read_location, ivec2 read_size, c_array<u8vec4> dst) const
    {
      ASTRALassert(!active());
      ASTRALassert(read_location.x() >= 0);
      ASTRALassert(read_location.y() >= 0);
      ASTRALassert(read_size.x() >= 0);
      ASTRALassert(read_size.y() >= 0);
      ASTRALassert(read_location.x() + read_size.x() <= size().x());
      ASTRALassert(read_location.y() + read_size.y() <= size().y());
      ASTRALassert(static_cast<int>(dst.size()) >= read_size.x() * read_size.y());

      read_color_buffer_implement(read_location, read_size, dst);
    }

    /*!
     * Returns true if this astral::RenderTarget is currently being rendered
     * to.
     */
    bool
    active(void) const
    {
      return m_render_backend_status.m_active != nullptr
        || m_renderer_status.m_active != nullptr;
    }

    ///@cond
    void
    active_status(detail::RenderTargetRenderBackendStatus V)
    {
      ASTRALassert(V.m_active == nullptr || m_render_backend_status.m_active == nullptr);
      m_render_backend_status = V;
    }

    RenderBackend*
    active_status(detail::RenderTargetRenderBackendStatusQuery)
    {
      return m_render_backend_status.m_active;
    }

    void
    active_status(detail::RenderTargetRendererStatus V)
    {
      ASTRALassert(V.m_active == nullptr || m_renderer_status.m_active == nullptr);
      m_renderer_status = V;
    }

    Renderer*
    active_status(detail::RenderTargetRendererStatusQuery)
    {
      return m_renderer_status.m_active;
    }
    ///@endcond

  protected:
    /*!
     * Protected ctor that the buffers that back the created \ref RenderTarget.
     * If both buffers are not null, they must have identical dimensions.
     * Atleast one of the buffers must not be null.
     * \param cb color buffer to use
     * \param ds depth-stencil buffer to use
     */
    RenderTarget(const reference_counted_ptr<ColorBuffer> &cb,
                 const reference_counted_ptr<DepthStencilBuffer> &ds):
      m_color_buffer(cb),
      m_depth_buffer(ds),
      m_render_backend_status(nullptr),
      m_renderer_status(nullptr)
    {
      ASTRALassert(m_color_buffer || m_depth_buffer);
      ASTRALassert(!m_color_buffer || !m_depth_buffer || m_depth_buffer->size() == m_color_buffer->size());

      m_size = m_color_buffer ?
        m_color_buffer->size() :
        m_depth_buffer->size();

      m_viewport_xy = ivec2(0, 0);
      m_viewport_size = m_size;
    }

  private:
    /*!
     * To be implemented by a derived class to read pixels from the
     * astral::ColorBuffer of this astral::RenderTarget. The implementation
     * can assume that the RenderTarget does have a color buffer and
     * that the arguments are legal.
     *
     * \param location location in pixels within color buffer from which to read
     * \param size size in pixels of region to read
     * \param dst location to which to write the pixels; the size of dst
     *            is atleast size.x() * size.y().
     */
    virtual
    void
    read_color_buffer_implement(ivec2 location, ivec2 size, c_array<u8vec4> dst) const = 0;

    reference_counted_ptr<ColorBuffer> m_color_buffer;
    reference_counted_ptr<DepthStencilBuffer> m_depth_buffer;
    ivec2 m_viewport_xy, m_viewport_size, m_size;

    detail::RenderTargetRenderBackendStatus m_render_backend_status;
    detail::RenderTargetRendererStatus m_renderer_status;
  };

/*! @} */
}

#endif

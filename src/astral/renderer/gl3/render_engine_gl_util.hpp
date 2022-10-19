/*!
 * \file render_engine_gl_util.hpp
 * \brief file render_engine_gl_util.hpp
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

#ifndef ASTRAL_RENDER_ENGINE_GL_UTIL_HPP
#define ASTRAL_RENDER_ENGINE_GL_UTIL_HPP

#include <astral/util/gl/astral_gl.hpp>
#include <astral/renderer/backend/render_backend.hpp>

namespace astral
{
  namespace gl
  {
    /*!
     * Emits the GL calls to change the color write mask
     */
    void
    emit_gl_color_write_mask(bvec4 b);

    /*!
     * Emits the GL calls to enable or disable the depth-test
     */
    void
    emit_gl_depth_buffer_mode(enum RenderBackend::depth_buffer_mode_t b);

    /*!
     * Emits the GL calls to change the stencil test
     */
    void
    emit_gl_set_stencil_state(const StencilState &st, astral_GLenum front_face = ASTRAL_GL_CW);

    /*!
     * Emits the GL calls to start a new render target
     */
    void
    emit_gl_begin_render_target(const RenderBackend::ClearParams &clear_params,
                                RenderTarget &rt, astral_GLenum front_face = ASTRAL_GL_CW);
  };
}

#endif

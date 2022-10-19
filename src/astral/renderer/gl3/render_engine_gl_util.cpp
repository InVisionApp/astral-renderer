/*!
 * \file render_engine_gl_util.cpp
 * \brief file render_engine_gl_util.cpp
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

#include <astral/util/gl/gl_context_properties.hpp>
#include <astral/renderer/gl3/render_target_gl3.hpp>
#include "render_engine_gl_util.hpp"

namespace
{
  inline
  astral_GLboolean
  gl_bool_from_bool(bool v)
  {
    return (v) ? ASTRAL_GL_TRUE : ASTRAL_GL_FALSE;
  }
}

void
astral::gl::
emit_gl_color_write_mask(bvec4 b)
{
  astral_glColorMask(gl_bool_from_bool(b[0]),
                     gl_bool_from_bool(b[1]),
                     gl_bool_from_bool(b[2]),
                     gl_bool_from_bool(b[3]));
}

void
astral::gl::
emit_gl_depth_buffer_mode(enum RenderBackend::depth_buffer_mode_t b)
{
  switch (b)
    {
    case RenderBackend::depth_buffer_occlude:
    case RenderBackend::depth_buffer_shadow_map:
      astral_glDepthMask(ASTRAL_GL_TRUE);
      astral_glDepthFunc(ASTRAL_GL_LEQUAL);
      astral_glEnable(ASTRAL_GL_DEPTH_TEST);
      break;

    case RenderBackend::depth_buffer_always:
      astral_glDepthMask(ASTRAL_GL_TRUE);
      astral_glDepthFunc(ASTRAL_GL_ALWAYS);
      astral_glEnable(ASTRAL_GL_DEPTH_TEST);
      break;

    case RenderBackend::depth_buffer_equal:
      astral_glDepthMask(ASTRAL_GL_FALSE);
      astral_glDepthFunc(ASTRAL_GL_EQUAL);
      astral_glEnable(ASTRAL_GL_DEPTH_TEST);
      break;

    case RenderBackend::depth_buffer_off:
      astral_glDepthMask(ASTRAL_GL_FALSE);
      astral_glDisable(ASTRAL_GL_DEPTH_TEST);
      break;
    }
}

void
astral::gl::
emit_gl_set_stencil_state(const StencilState &st, astral_GLenum front_face)
{
  if (st.m_enabled)
    {
      const astral_GLenum gl_op[StencilState::op_count] =
        {
          [StencilState::op_keep]       = ASTRAL_GL_KEEP,
          [StencilState::op_zero]       = ASTRAL_GL_ZERO,
          [StencilState::op_replace]    = ASTRAL_GL_REPLACE,
          [StencilState::op_incr_clamp] = ASTRAL_GL_INCR,
          [StencilState::op_incr_wrap]  = ASTRAL_GL_INCR_WRAP,
          [StencilState::op_decr_clamp] = ASTRAL_GL_DECR,
          [StencilState::op_decr_wrap]  = ASTRAL_GL_DECR_WRAP,
          [StencilState::op_invert]     = ASTRAL_GL_INVERT,
        };

      const astral_GLenum gl_test[StencilState::test_count] =
        {
          [StencilState::test_never]         = ASTRAL_GL_NEVER,
          [StencilState::test_always]        = ASTRAL_GL_ALWAYS,
          [StencilState::test_less]          = ASTRAL_GL_LESS,
          [StencilState::test_less_equal]    = ASTRAL_GL_LEQUAL,
          [StencilState::test_greater]       = ASTRAL_GL_GREATER,
          [StencilState::test_greater_equal] = ASTRAL_GL_GEQUAL,
          [StencilState::test_not_equal]     = ASTRAL_GL_NOTEQUAL,
          [StencilState::test_equal]         = ASTRAL_GL_EQUAL,
        };

      astral_GLenum cw, ccw;

      ASTRALassert(front_face == ASTRAL_GL_CW || front_face == ASTRAL_GL_CCW);
      cw = (front_face == ASTRAL_GL_CW) ? ASTRAL_GL_FRONT : ASTRAL_GL_BACK;
      ccw = (front_face == ASTRAL_GL_CCW) ? ASTRAL_GL_FRONT : ASTRAL_GL_BACK;

      astral_glEnable(ASTRAL_GL_STENCIL_TEST);
      astral_glStencilOpSeparate(cw,
                                 gl_op[st.m_stencil_fail_op[StencilState::face_cw]],
                                 gl_op[st.m_stencil_pass_depth_fail_op[StencilState::face_cw]],
                                 gl_op[st.m_stencil_pass_depth_pass_op[StencilState::face_cw]]);
      astral_glStencilFuncSeparate(cw,
                                   gl_test[st.m_func[StencilState::face_cw]],
                                   st.m_reference[StencilState::face_cw],
                                   st.m_reference_mask[StencilState::face_cw]);
      astral_glStencilOpSeparate(ccw,
                                 gl_op[st.m_stencil_fail_op[StencilState::face_ccw]],
                                 gl_op[st.m_stencil_pass_depth_fail_op[StencilState::face_ccw]],
                                 gl_op[st.m_stencil_pass_depth_pass_op[StencilState::face_ccw]]);
      astral_glStencilFuncSeparate(ccw,
                                   gl_test[st.m_func[StencilState::face_ccw]],
                                   st.m_reference[StencilState::face_ccw],
                                   st.m_reference_mask[StencilState::face_ccw]);
      astral_glStencilMask(st.m_write_mask);
    }
  else
    {
      astral_glDisable(ASTRAL_GL_STENCIL_TEST);
      astral_glStencilMask(0u);
    }
}

void
astral::gl::
emit_gl_begin_render_target(const RenderBackend::ClearParams &clear_params,
                            RenderTarget &rt, astral_GLenum front_face)
{
  RenderTargetGL *rt_gl;
  ivec2 sz(rt.size()), vwp_xy(rt.viewport_xy()), vwp_dims(rt.viewport_size());

  ASTRALassert(dynamic_cast<RenderTargetGL*>(&rt));
  rt_gl = static_cast<RenderTargetGL*>(&rt);

  astral_glBindFramebuffer(ASTRAL_GL_DRAW_FRAMEBUFFER, rt_gl->fbo());
  astral_glBindFramebuffer(ASTRAL_GL_READ_FRAMEBUFFER, rt_gl->fbo());

  if (rt_gl->m_y_coordinate_convention == RenderTargetGL::pixel_y_zero_is_bottom)
    {
      int gl_y;

      /* meh, GL's convention is y = 0 is the bottom */
      gl_y = sz.y() - vwp_xy.y() - vwp_dims.y();
      astral_glViewport(vwp_xy.x(), gl_y, vwp_dims.x(), vwp_dims.y());
      if (vwp_xy != ivec2(0, 0) || vwp_dims != sz)
        {
          astral_glEnable(ASTRAL_GL_SCISSOR_TEST);
          astral_glScissor(vwp_xy.x(), gl_y, vwp_dims.x(), vwp_dims.y());
        }
      else
        {
          astral_glDisable(ASTRAL_GL_SCISSOR_TEST);
        }
    }
  else
    {
      astral_glViewport(vwp_xy.x(), vwp_xy.y(), vwp_dims.x(), vwp_dims.y());
      if (vwp_xy != ivec2(0, 0) || vwp_dims != sz)
        {
          astral_glEnable(ASTRAL_GL_SCISSOR_TEST);
          astral_glScissor(vwp_xy.x(), vwp_xy.y(), vwp_dims.x(), vwp_dims.y());
        }
      else
        {
          astral_glDisable(ASTRAL_GL_SCISSOR_TEST);
        }
    }

  astral_glColorMask(ASTRAL_GL_TRUE, ASTRAL_GL_TRUE, ASTRAL_GL_TRUE, ASTRAL_GL_TRUE);
  astral_glDepthMask(ASTRAL_GL_TRUE);
  astral_glStencilMask(~0u);

  ASTRALassert(front_face == ASTRAL_GL_CW || front_face == ASTRAL_GL_CCW);
  astral_glFrontFace(front_face);
  astral_glDisable(ASTRAL_GL_CULL_FACE);

  #if defined(astral_glProvokingVertex) && defined(ASTRAL_GL_LAST_VERTEX_CONVENTION)
    {
      if (!ContextProperties::is_es())
        {
          astral_glProvokingVertex(ASTRAL_GL_LAST_VERTEX_CONVENTION);
        }
    }
  #endif

  astral_GLbitfield clear_mask(0u);

  if (clear_params.m_clear_mask & RenderBackend::ClearColorBuffer)
    {
      astral_glClearColor(clear_params.m_clear_color.x(),
                          clear_params.m_clear_color.y(),
                          clear_params.m_clear_color.z(),
                          clear_params.m_clear_color.w());
      clear_mask |= ASTRAL_GL_COLOR_BUFFER_BIT;
    }

  if (clear_params.m_clear_mask & RenderBackend::ClearDepthBuffer)
    {
      float v;

      v = (clear_params.m_clear_depth == RenderBackend::depth_buffer_value_clear) ? 1.0f : 0.0f;
      astral_glClearDepthf(v);
      clear_mask |= ASTRAL_GL_DEPTH_BUFFER_BIT;
    }

  if (clear_params.m_clear_mask & RenderBackend::ClearStencilBuffer)
    {
      astral_glClearStencil(clear_params.m_clear_stencil);
      clear_mask |= ASTRAL_GL_STENCIL_BUFFER_BIT;
    }

  astral_glClear(clear_mask);
}

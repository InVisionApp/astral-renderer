/*!
 * \file gpu_dirty_state.hpp
 * \brief file gpu_dirty_state.hpp
 *
 * Copyright 2018 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#ifndef ASTRAL_GPU_DIRTY_STATE_HPP
#define ASTRAL_GPU_DIRTY_STATE_HPP

#include <stdint.h>

namespace astral
{
/*!\addtogroup Utility
 * @{
 */

  /*!
   * \brief
   * Object to encapsulate GPU dirty state via bit-flags.
   */
  class gpu_dirty_state
  {
  public:
    /*!
     * \brief
     * Enumeration to specify the bit-flags to specify
     * dirty GPU state.
     */
    enum bit_flags:uint32_t
      {
        /*!
         * If this bit is up, indicates that the shaders used
         * by the GPU has changed.
         */
        shader = 1 << 0,

        /*!
         * If this bit is up, indicates that  the binding
         * of the texture binding slots used by the shader
         * of a PainterBackend derived class have changed.
         * For example, in GL these changes are accompished
         * by calling glBindTexture().
         */
        textures = 1 << 1,

        /*!
         * If this bit is up, indicates that the binding
         * of the image binding slots used by the shader
         * of a PainterBackend derived class have changed.
         * For example, in GL these changes are accompished
         * by calling glBindImageTexture().
         */
        images = 1 << 2,

        /*!
         * If this bit is up, indicates that the blend
         * state of the GPU has changed.
         */
        blend_mode = 1 << 3,

        /*!
         * If this bit is up, indicates that the bound
         * render-target of the GPU has changed. For example,
         * in GL changing the render-target is accomplished
         * by glBindFramgebuffer() and/or glDrawBuffers().
         */
        render_target = 1 << 4,

        /*!
         * If this bit is up, indicates that the viewport
         * or scissor values have changed. For example, in GL
         * this is accomplished by calling glViewport(),
         * glScissor() or glEnable/glDisable() passing
         * ASTRAL_GL_SCISSOR_TEST.
         */
        viewport_scissor = 1 << 5,

        /*!
         * If this bit is up, indicates that the source or format
         * for index or vertex buffers has changed. For example,
         * in GL, this can be accomplished by calling
         * glBindVertexArray() or modifying the currently bound
         * vertex array object.
         */
        vertex_index_source = 1 << 6,

        /*!
         * If this bit is up, indicates that a constant buffer
         * source has changed. For GL, these are UBOs. For example,
         * in GL, this can be accomplished by calling glBindBuffer()
         * with the binding target as ASTRAL_GL_UNIFORM_BUFFER.
         */
        constant_buffers = 1 << 7,

        /*!
         * If this bit is up, indicates that a storage buffer
         * source has changed. For GL, these are SSBOs. For example,
         * in GL, this can be accomplished by calling glBindBuffer()
         * with the binding target as ASTRAL_GL_SHADER_STORAGE_BUFFER.
         */
        storage_buffers = 1 << 8,

        /*!
         * If this bit is up, the depth test or depth mask
         * has been modified.
         */
        depth = 1 << 9,

        /*!
         * If this bit is up, the stencil test or write mask
         * has been modified.
         */
        stencil = 1 << 10,

        /*!
         * Equivalent to
         * \code
         * depth | stencil
         * \endcode
         */
        depth_stencil = depth | stencil,

        /*!
         * If this bit is up, the color write mask has been modified
         */
        color_mask = 1 << 11,

        /*!
         * If this bit is up, indicates that the HW clip planes
         * has changed.
         */
        hw_clip = 1 << 12,

        /*!
         * Specify that all state is dirty.
         */
        all = ~0u,
      };

    /*!
     * Ctor.
     * \param flags bit field using \ref bit_flags to specify
     *              what portion of GPU state is firty
     */
    gpu_dirty_state(uint32_t flags = 0u):
      m_flags(flags)
    {}

    /*!
     * Implicit cast-operator to uint32_t.
     */
    operator uint32_t() const { return m_flags; }

    /*!
     * Implicit cast-operator to uint32_t.
     */
    operator uint32_t&() { return m_flags; }

  private:
    uint32_t m_flags;
  };
/*! @} */
}

#endif

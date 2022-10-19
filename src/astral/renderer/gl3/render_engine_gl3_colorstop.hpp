/*!
 * \file render_engine_gl3_colorstop.hpp
 * \brief file render_engine_gl3_colorstop.hpp
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

#ifndef ASTRAL_RENDER_ENGINE_GL3_COLORSTOP_HPP
#define ASTRAL_RENDER_ENGINE_GL3_COLORSTOP_HPP

#include <astral/renderer/colorstop_sequence.hpp>
#include <astral/util/gl/astral_gl.hpp>
#include <astral/util/gl/gl_program.hpp>
#include <astral/renderer/gl3/render_engine_gl3.hpp>

#include "render_engine_gl3_implement.hpp"
#include "render_engine_gl3_fbo_blitter.hpp"

/*!
 * \brief
 * A ColorStopSequenceAtlasBackingGL3 represents an
 * implementation of \ref ColorStopSequenceAtlasBacking
 * by a single ASTRAL_GL_TEXTURE_2D.
 *
 * TODO: use a ASTRAL_GL_TEXTURE_2D_ARRAY for the backing to allow
 *       for large number of color stops; the .y() and .z()
 *       can be extracted via bit shits and masks if the height
 *       is a power of 2.
 */
class astral::gl::RenderEngineGL3::Implement::ColorStopSequenceBacking:
  public astral::ColorStopSequenceAtlasBacking
{
public:
  /*!
   * Ctor
   * \param blitter blitter used on resize
   * \param log2_per_layer_dims log2 of the width of the backing texture, this maps to the
   *                            value of \ref ColorStopSequenceAtlasBacking::layer_dimensions()
   * \param initial_number_layers initial number of layers for the backing, this maps to the
   *                              value of \ref ColorStopSequenceAtlasBacking::number_layers()
   */
  ColorStopSequenceBacking(FBOBlitter &blitter,
                           unsigned int log2_per_layer_dims,
                           unsigned int initial_number_layers);
  ~ColorStopSequenceBacking();

  virtual
  void
  load_pixels(int layer, int start, c_array<const u8vec4> pixels) override final;

  /*!
   * The GL texture. NOTE! this value changes if the
   * ColorStopSequenceAtlasBackingGL3 is resized.
   */
  astral_GLuint
  texture(void) const
  {
    return m_texture;
  }

protected:
  virtual
  unsigned int
  on_resize(unsigned int L) override final;

private:
  void
  create_storage(unsigned int width, unsigned int height);

  astral_GLuint m_texture;
  reference_counted_ptr<FBOBlitter> m_blitter;
};


#endif

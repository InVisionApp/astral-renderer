/*!
 * \file glyph_colors.hpp
 * \brief file glyph_colors.hpp
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

#ifndef ASTRAL_GLYPH_COLORS_HPP
#define ASTRAL_GLYPH_COLORS_HPP

#include <astral/util/util.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/vecN.hpp>
#include <astral/text/glyph_palette_id.hpp>

namespace astral
{
  ///@cond
  class Path;
  ///@endcond

/*!\addtogroup Text
 * @{
 */
  /*!
   * \brief
   * Class to embody the colors for the layers of all palettes
   * of a scalable glyph.
   */
  class GlyphColors
  {
  public:
    /*!
     * Set the size: number of layers and number of palettes.
     * \param num_layers number of layers the glyph has
     * \param num_palettes number of palettes the glyph has
     */
    void
    resize(unsigned int num_layers, unsigned int num_palettes)
    {
      m_num_layers = num_layers;
      m_num_palettes = num_palettes;
      m_color_backing.resize(m_num_layers * m_num_palettes);
    }

    /*!
     * Returns the number of layers
     */
    unsigned int
    number_layers(void) const
    {
      return m_num_layers;
    }

    /*!
     * Returns the number of palettes
     */
    unsigned int
    number_palettes(void) const
    {
      return m_num_palettes;
    }

    /*!
     * Returns true if the number of palettes or number
     * of layers is zero.
     */
    bool
    empty(void) const
    {
      return m_color_backing.empty();
    }

    /*!
     * clear this GlyphColors object.
     */
    void
    clear(void)
    {
      m_color_backing.clear();
      m_num_layers = 0u;
      m_num_palettes = 0u;
    }

    /*!
     * Returns a writeable array for the colors
     * of the named palette. The colors folow
     * the convenion of the COLR truetype table,
     * i.e. the color values are NOT pre-multiplied
     * by alpha.
     * \param P which color palette
     */
    c_array<vec4>
    layer_colors(GlyphPaletteID P)
    {
      c_array<vec4> R(make_c_array(m_color_backing));

      ASTRALassert(P.m_value < m_num_palettes);
      R = R.sub_array(P.m_value * m_num_layers, m_num_layers);
      return R;
    }

    /*!
     * Returns a read only array for the colors
     * of the named palette. The colors folow
     * the convenion of the COLR truetype table,
     * i.e. the color values are NOT pre-multiplied
     * by alpha.
     * \param P which color palette
     */
    c_array<const vec4>
    layer_colors(GlyphPaletteID P) const
    {
      c_array<const vec4> R(make_c_array(m_color_backing));

      ASTRALassert(P.m_value < m_num_palettes);
      R = R.sub_array(P.m_value * m_num_layers, m_num_layers);
      return R;
    }

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * layer_colors(P)[layer]
     * \endcode
     * \param P which color palette
     * \param layer which layer
     */
    vec4&
    color(GlyphPaletteID P, unsigned int layer)
    {
      return layer_colors(P)[layer];
    }

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * layer_colors(P)[layer]
     * \endcode
     * \param P which color palette
     * \param layer which layer
     */
    const vec4&
    color(GlyphPaletteID P, unsigned int layer) const
    {
      return layer_colors(P)[layer];
    }

  private:
    unsigned int m_num_layers, m_num_palettes;
    std::vector<vec4> m_color_backing;
  };

/*! @} */
}

#endif

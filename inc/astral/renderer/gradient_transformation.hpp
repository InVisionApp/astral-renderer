/*!
 * \file gradient_transformation.hpp
 * \brief file gradient_transformation.hpp
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

#ifndef ASTRAL_GRADIENT_TRANSFORMATION_HPP
#define ASTRAL_GRADIENT_TRANSFORMATION_HPP

#include <astral/renderer/render_enums.hpp>
#include <astral/util/transformation.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  /*!
   * \brief
   * Specifies if a coordinate has a tile range
   * applied to it.
   */
  class TileRange
  {
  public:
    /*!
     * Default ctor that initializes to not apply
     * a tile mode, i.e. \ref m_mode has the value
     * astral::tile_mode_decal with both \ref m_begin
     * and \ref m_end left unintialized
     */
    TileRange(void):
      m_mode(tile_mode_decal)
    {}

    /*!
     * Ctor to initialize each field of astral::TileRange
     * \param b initial value for \ref m_begin
     * \param e initial value for \ref m_end
     * \param m initial value for \ref m_mode
     */
    TileRange(float b, float e, enum tile_mode_t m):
      m_begin(b),
      m_end(e),
      m_mode(m)
    {}

    /*!
     * Set \ref m_begin
     */
    TileRange&
    begin(float v)
    {
      m_begin = v;
      return *this;
    }

    /*!
     * Set \ref m_end
     */
    TileRange&
    end(float v)
    {
      m_end = v;
      return *this;
    }

    /*!
     * Set \ref m_mode
     */
    TileRange&
    mode(enum tile_mode_t v)
    {
      m_mode = v;
      return *this;
    }

    /*!
     * Gives the start of the tile range
     * (only has effect if \ref m_mode
     * is not astral::tile_mode_decal).
     */
    float m_begin;

    /*!
     * Gives the end of the tile range
     * (only has effect if \ref m_mode
     * is not astral::tile_mode_decal).
     */
    float m_end;

    /*!
     * Gives the tile mode; if this value is
     * not \ref astral::tile_mode_decal then
     * it is an error for \ref m_begin to
     * have the same value as \ref m_end.
     */
    enum tile_mode_t m_mode;
  };

  /*!
   * \brief
   * An astral::GradientTransformation specifies
   * a transformation (which maps from material
   * coordiante to gradient coordinates) and a
   * window tile pattern to apply to a 2D point.
   */
  class GradientTransformation
  {
  public:
    /*!
     * Ctor, initializes \ref m_transformation as the
     * identity and leaves \ref m_x_tile and \ref m_y_tile
     * as not applying a tile range (i.e. no range specified
     * and tile mode is astral::tile_mode_decal).
     */
    GradientTransformation(void)
    {}

    /*!
     * Ctor.
     * \param t initial value for \ref m_transformation
     * \param x initial value for \ref m_x_tile
     * \param y initial value for \ref m_y_tile
     */
    explicit
    GradientTransformation(const Transformation &t,
                           const TileRange &x = TileRange(),
                           const TileRange &y = TileRange()):
      m_transformation(t),
      m_x_tile(x),
      m_y_tile(y)
    {}

    /*!
     * Ctor; initializes \ref m_transformation as identity
     * \param x initial value for \ref m_x_tile
     * \param y initial value for \ref m_y_tile
     */
    explicit
    GradientTransformation(const TileRange &x,
                           const TileRange &y = TileRange()):
      m_x_tile(x),
      m_y_tile(y)
    {}

    /*!
     * Set \ref m_transformation
     */
    GradientTransformation&
    transformation(const Transformation &v)
    {
      m_transformation = v;
      return *this;
    }

    /*!
     * Set \ref m_x_tile
     */
    GradientTransformation&
    x_tile(const TileRange &v)
    {
      m_x_tile = v;
      return *this;
    }

    /*!
     * Set \ref m_y_tile
     */
    GradientTransformation&
    y_tile(const TileRange &v)
    {
      m_y_tile = v;
      return *this;
    }

    /*!
     * The transformation that maps from material coordinates
     * to gradient coordinates
     * \code
     * G = m_transformation.apply_to_point(M)
     * \endcode
     * where M is material coordinates and G is gradient
     * coordinates
     */
    Transformation m_transformation;

    /*!
     * Gives if and how a tile range is applied
     * to the gradient x-coordinate
     */
    TileRange m_x_tile;

    /*!
     * Gives if and how a tile range is applied
     * to the gradient y-coordinate
     */
    TileRange m_y_tile;
  };

/*! @} */
}

#endif

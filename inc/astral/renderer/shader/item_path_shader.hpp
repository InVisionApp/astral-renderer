/*!
 * \file item_path_shader.hpp
 * \brief file item_path_shader.hpp
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

#ifndef ASTRAL_ITEM_PATH_SHADER_HPP
#define ASTRAL_ITEM_PATH_SHADER_HPP

#include <astral/renderer/item_path.hpp>
#include <astral/renderer/shader/item_data.hpp>
#include <astral/renderer/shader/item_shader.hpp>

namespace astral
{
  /*!
   * \brief
   * As astral::ItemPathShader is for drawing astral::ItemPath
   * objects. To render using an astral::ItemPathShader means to
   * pack the astral::ItemData via the method ItemPathShader::pack_data()
   * and using the same vertex-index data as astral::DynamicRectShader.
   * \tparam T shader type which must be astral::ColorItemShader or
   *           astral::MaskItemShader
   */
  template<typename T>
  class ItemPathShader
  {
  public:
    /*!
     * Empty ctor, leaving the object wihout a shader
     */
    ItemPathShader(void)
    {}

    /*!
     * Ctor giving it a shader.
     * \param sh shader of the created object
     */
    ItemPathShader(const reference_counted_ptr<const T> &sh):
      m_shader(sh)
    {}

    /*!
     * Ctor giving it a shader.
     * \param sh shader of the created object
     */
    ItemPathShader(const T *sh):
      m_shader(sh)
    {}

    /*!
     * Cast operator
     */
    operator const reference_counted_ptr<const T>&() const { return m_shader; }

    /*!
     * Cast operator
     */
    operator reference_counted_ptr<const T>&() { return m_shader; }

    /*!
     * overload operator*
     */
    const T& operator*(void) const { return *m_shader; }

    /*!
     * overload operator->
     */
    const T* operator->(void) const { ASTRALassert(m_shader); return m_shader.get(); }

    /*!
     * Return the pointer to the underlying shader
     */
    const T* get(void) const { return m_shader.get(); }

    /*!
     * Returns the size of array that should be passed
     * to pack_item_data()
     * \param number_layers number of layers to draw
     */
    static
    unsigned int
    item_data_size(unsigned int number_layers)
    {
      return ItemPath::data_size(number_layers) + 1u;
    }

    /*!
     * Pack item data that is accepted by an astral::ItemPathShader
     * \param engine RenderEngine
     * \param layers layers of astral::ItemPath to draw
     * \param dst location to which to write the item data
     * \returns the rect covered by the layers as a bounding box
     */
    static
    BoundingBox<float>
    pack_item_data(RenderEngine &engine,
                   c_array<const ItemPath::Layer> layers,
                   c_array<gvec4> dst)
    {
      return pack_item_data(engine, layers, nullptr, dst);
    }

    /*!
     * Pack item data that is accepted by an astral::ItemPathShader
     * \param engine RenderEngine
     * \param layers layers of astral::ItemPath to draw
     * \param restrict_bb if non null, restrict the drawing to this
     *                    bounding box
     * \param dst location to which to write the item data
     * \returns the rect covered by the layers as a bounding box
     *          if restrict_bb is non-null, intersect the return
     *          value against restrict_bb
     */
    static
    BoundingBox<float>
    pack_item_data(RenderEngine &engine,
                   c_array<const ItemPath::Layer> layers,
                   const BoundingBox<float> *restrict_bb,
                   c_array<gvec4> dst)
    {
      c_array<gvec4> item_path_dst;
      BoundingBox<float> bb;

      /* the first gvec4 value is for the rect to contain
       * the ItemPath's layers.
       */
      item_path_dst = dst.sub_array(1);
      bb = ItemPath::pack_data(engine, layers, item_path_dst);

      if (restrict_bb)
        {
          bb.intersect_against(*restrict_bb);
        }

      dst[0].x().f = bb.min_point().x();
      dst[0].y().f = bb.min_point().y();
      dst[0].z().f = bb.max_point().x();
      dst[0].w().f = bb.max_point().y();

      return bb;
    }

  private:
    reference_counted_ptr<const T> m_shader;
  };

  /*!
   * Typedef for a astral::ColorItemShader that renders
   * an astral::ItemPath directly.
   */
  typedef ItemPathShader<ColorItemShader> ColorItemPathShader;

  /*!
   * Typedef for a astral::ItemPathShader that renders
   * an astral::ItemPath to a mask without invoking
   * the stencil-then-cover algorithm. A shader of this
   * type will only process those astral::ItemPath values
   * that have a single layer and will also ignore the
   * color value of the layer. The shader is to to emit
   * the values as follows:
   *  - .r is the coverage by the mask
   *  - .g is the signed distance value normalized to [0, 1] for the mask
   *  - .b is 0
   *  - .a is 0
   */
  typedef ItemPathShader<MaskItemShader> MaskItemPathShader;
}

#endif

/*!
 * \file dynamic_rect_shader.hpp
 * \brief file dynamic_rect_shader.hpp
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

#ifndef ASTRAL_DYNAMIC_RECT_SHADER_HPP
#define ASTRAL_DYNAMIC_RECT_SHADER_HPP

#include <astral/util/rect.hpp>
#include <astral/renderer/shader/item_shader.hpp>
#include <astral/renderer/shader/item_data.hpp>
#include <astral/renderer/vertex_index.hpp>
#include <astral/renderer/vertex_data.hpp>

namespace astral
{
  class RenderEngine;

  /*!
   * \brief
   * An astral::DynamicRectShader is for drawing rectangles
   * whose locaion and size is determined by item data.
   *
   * The packing of vertices is as follows:
   *  - Vertex::m_data[0].f --> x-relative position, value is 0 or 1
   *  - Vertex::m_data[1].f --> y-relative position, value is 0 or 1
   *
   * The ItemData is packed as:
   *  - [0].x().f --> min-x corner
   *  - [0].y().f --> min-y corner
   *  - [0].z().f --> max-x corner
   *  - [0].w().f --> max-y corner
   */
  class DynamicRectShader
  {
  public:
    /*!
     * \brief
     * Enumeration describing how item data is packed for
     * an astral::DynamicRectShader
     */
    enum item_data_packing:uint32_t
      {
        /*!
         * Offset at which to pack the geometry of the rect
         *  - .x().f --> min-x corner
         *  - .y().f --> min-y corner
         *  - .z().f --> max-x corner
         *  - .w().f --> max-y corner
         */
        coordinate_offset = 0,

        /*!
         * Number of item datas consumed.
         */
        item_data_size
      };

    /*!
     * Empty ctor, leaving the object wihout a shader
     */
    DynamicRectShader(void)
    {}

    /*!
     * Ctor giving it a shader.
     * \param sh shader of the created object
     */
    DynamicRectShader(const reference_counted_ptr<const ColorItemShader> &sh):
      m_shader(sh)
    {}

    /*!
     * Ctor giving it a shader.
     * \param sh shader of the created object
     */
    DynamicRectShader(const ColorItemShader *sh):
      m_shader(sh)
    {}

    /*!
     * Cast operator
     */
    operator const reference_counted_ptr<const ColorItemShader>&() const { return m_shader; }

    /*!
     * Cast operator
     */
    operator reference_counted_ptr<const ColorItemShader>&() { return m_shader; }

    /*!
     * overload operator*
     */
    const ColorItemShader& operator*(void) const { return *m_shader; }

    /*!
     * overload operator->
     */
    const ColorItemShader* operator->(void) const { ASTRALassert(m_shader); return m_shader.get(); }

    /*!
     * Return the pointer to the underlying shader
     */
    const ColorItemShader* get(void) const { return m_shader.get(); }

    /*!
     * Pack item data that the \ref ColorItemShader of a \ref
     * DynamicRectShader accepts
     */
    template<typename T>
    static
    void
    pack_item_data(const RectT<T> &rect, c_array<gvec4> dst)
    {
      ASTRALassert(dst.size() == item_data_size);
      dst[coordinate_offset].x().f = rect.m_min_point.x();
      dst[coordinate_offset].y().f = rect.m_min_point.y();
      dst[coordinate_offset].z().f = rect.m_max_point.x();
      dst[coordinate_offset].w().f = rect.m_max_point.y();
    }

    /*!
     * Pack item data that the \ref ColorItemShader of a \ref
     * DynamicRectShader accepts
     */
    template<typename T>
    static
    void
    pack_item_data(const vecN<T, 2> &size, c_array<gvec4> dst)
    {
      ASTRALassert(dst.size() == item_data_size);
      dst[coordinate_offset].x().f = 0.0f;
      dst[coordinate_offset].y().f = 0.0f;
      dst[coordinate_offset].z().f = size.x();
      dst[coordinate_offset].w().f = size.y();
    }

    /*!
     * Pack vertex data the \ref ColorItemShader of a \ref
     * DynamicRectShader accepts
     */
    static
    reference_counted_ptr<const VertexData>
    create_rect(RenderEngine &engine);

  private:
    reference_counted_ptr<const ColorItemShader> m_shader;
  };
}

#endif

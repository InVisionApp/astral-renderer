/*!
 * \file glyph_shader.hpp
 * \brief file glyph_shader.hpp
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

#ifndef ASTRAL_GLYPH_SHADER_HPP
#define ASTRAL_GLYPH_SHADER_HPP

#include <astral/util/rect.hpp>
#include <astral/util/skew_parameters.hpp>
#include <astral/renderer/render_data.hpp>
#include <astral/renderer/shader/item_data.hpp>
#include <astral/renderer/shader/item_shader.hpp>
#include <astral/text/typeface_metrics.hpp>

namespace astral
{
  ///@cond
  class RenderEngine;
  class TextItem;
  ///@endcond

  /*!
   * \brief
   * An astral::GlyphShader is for drawing glyphs
   * each realized as a rect.
   *
   * The packing of vertices is as follows:
   *  - Vertex::m_data[0].f --> x-coordinate relative to pen-position
   *  - Vertex::m_data[1].f --> y-coordinate relative to pen-position
   *  - Vertex::m_data[2].u --> which corner enumerated by astral::RectEnums::corner_t
   *  - Vertex::m_data[3].u --> astral::StaticData::location()
   *
   * The static data of Vertex::m_data[3].u is packed as follows
   *  - [0].x().f --> x-pen position of glyph
   *  - [0].y().f --> y-pen position of glyph
   *  - [0].z().f --> width of glyph
   *  - [0].w().f --> height of glyph
   *  - [1].x().u --> astral::StaticData::location() of Glyph::render_data()
   *                  or Glyph::image_render_data()
   *  - [2].y.u() --> flags, see GlyphShader::flags_t
   *  - [2].zw   --> free
   */
  class GlyphShader
  {
  public:
    /*!
     * Additional information on a glyph
     */
    enum flags_t:uint32_t
      {
        /*!
         * If bit is up, then glyph is a colored glyph
         */
        is_colored_glyph = 1u,
      };

    /*!
     * \brief
     * Class to abstract fetching the position data
     * and where to read from the static data of a
     * glyph. This class is utilized by pack_glyph_data()
     * to make the packing of attribute data for glyphs
     * suitable for an astral::GlyphShader.
     */
    class Elements
    {
    public:
      virtual
      ~Elements(void)
      {}

      /*!
       * To be implemented by a derived class to return
       * the position in logical coordinates and the
       * the value of astral::StaticData::location()
       * for the named element.
       * \param idx which element with 0 <= idx < number_elements()
       * \param out_rect the rect relative to the pen position to draw
       *                 the idx'th element
       * \param out_pen_position location to which to write the position
       *                         of the pen of the idx'th element.
       * \param out_shared_data_location value of astral::StaticData::location()
       *                                 of Glyph::image_render_data() or
       *                                 Glyph::render_data()
       * \returns returns the flags of the glyph enumerated by \ref flags_t
       */
      virtual
      uint32_t
      element(unsigned int idx,
              Rect *out_rect,
              vec2 *out_pen_position,
              uint32_t *out_shared_data_location) const = 0;

      /*!
       * To be implemented by a derived class to return the
       * the number of elements.
       */
      virtual
      unsigned int
      number_elements(void) const = 0;
    };

    /*!
     * A \ref ItemDataPackerBase represents how item data
     * is packed for the shaders of a astral::GlyphShader
     * to consume.
     */
    class ItemDataPackerBase
    {
    public:
      virtual
      ~ItemDataPackerBase()
      {}

      /*!
       * To be implemented by a derived class to return
       * the required size to pack the shader data that
       * this packer packs.
       */
      virtual
      unsigned int
      item_data_size(void) const = 0;

      /*!
       * To be implemnted by a derived class to pack the
       * item data
       * \param dst location to which to pack the values
       */
      virtual
      void
      pack_item_data(c_array<gvec4> dst) const = 0;

      /*!
       * To be optionally implemented by a derived class
       * to return the bounding box of the data acting
       * on the glyphs of an astral::TextItem. Default
       * implementation returns the value of
       * TextItem::bounding_box(void) const
       * \param text_item text item to compute the bounding box
       */
      virtual
      BoundingBox<float>
      bounding_box(const TextItem &text_item) const;

      /*!
       * To be optionally implemented by a derived class to return
       * the astral::ItemDataValueMapping associated to data packed
       * via pack_item_data(). Default implementation returns an
       * empty astral::ItemDataValueMapping.
       */
      virtual
      const ItemDataValueMapping&
      intrepreted_value_map(void) const;
    };

    /*!
     * Implementation of \ref ItemDataPackerBase that has no data
     */
    class EmptyPacker:public ItemDataPackerBase
    {
    public:
      virtual
      unsigned int
      item_data_size(void) const override
      {
        return 0u;
      }

      virtual
      void
      pack_item_data(c_array<gvec4> dst) const override
      {
        ASTRALunused(dst);
        ASTRALassert(dst.empty());
      }
    };

    /*!
     * \brief
     * Class to embody properties of text that are dynamic
     * and interpreted by shaders to synthesize fonts
     */
    class SyntheticData:public ItemDataPackerBase
    {
    public:
      enum
        {
          /*!
           * Size of item data. Data is packed as follows:
           * - [0].x().f = \ref m_line_start_x
           * - [0].y().f = SkewParameters::m_scale_x of \ref m_skew
           * - [0].z().f = SkewParameters::m_skew_x of \ref m_skew
           */
          data_size = 1
        };

      /*!
       * Default ctor. Initializes with no glyph distortion.
       */
      SyntheticData(void):
        m_line_start_x(0.0f),
        m_thicken(0.0f)
      {}

      /*!
       * Initialize from an astral::SkewParameters value
       */
      SyntheticData(const SkewParameters v):
        m_line_start_x(0.0f),
        m_skew(v),
        m_thicken(0.0f)
      {}

      virtual
      unsigned int
      item_data_size(void) const override
      {
        return data_size;
      }

      virtual
      void
      pack_item_data(c_array<gvec4> dst) const override
      {
        ASTRALassert(dst.size() == data_size);
        dst[0].x().f = m_line_start_x;
        dst[0].y().f = m_skew.m_scale_x;
        dst[0].z().f = m_skew.m_skew_x;
        dst[0].w().f = t_max(0.0f, t_min(1.0f, m_thicken));
      }

      virtual
      BoundingBox<float>
      bounding_box(const TextItem &text_item) const override;

      /*!
       * Set \ref m_line_start_x
       */
      SyntheticData&
      line_start_x(float v)
      {
        m_line_start_x = v;
        return *this;
      }

      /*!
       * Set \ref m_skew
       */
      SyntheticData&
      skew(SkewParameters v)
      {
        m_skew = v;
        return *this;
      }

      /*!
       * Given a bounding box of glyphs, compute the bounding box
       * of the same glyphs with this SyntheticData applied to it.
       */
      BoundingBox<float>
      bounding_box(const BoundingBox<float> &bb,
                   const TypefaceMetricsBase &metrics) const;

      /*!
       * x-coordinate for the start of each line. This
       * value is used to account for when the value of
       * SkewParameters::m_scale_x is greater than one.
       */
      float m_line_start_x;

      /*!
       * Skew parameters
       */
      SkewParameters m_skew;

      /*!
       * How much thickness to add to the glyphs, with
       * 1.0 representing that the entire quad of the
       * glyph is solid and 0.0 indicating adding no
       * additional thickness.
       */
      float m_thicken;
    };

    /*!
     * Realize vertex index data that the shaders accepts
     * \param engine astral::RenderEngine from which to allocate
     *               vertex-index and static data
     * \param elements provides positions and locations
     * \param vert_storage std::vector<Vertex> storage to place vertex data,
     *                     the passed std::vector must be alive as long
     *                     as RenderData::m_vertex_data of the returned
     *                     astral::RenderData is alive.
     * \param index_storage std::vector<Index> storage to place index data,
     *                      the passed std::vector must be alive as long
     *                      as RenderData::m_vertex_data of the returned
     *                      astral::RenderData is alive.
     * \param static_values std::vector<gvec4> storage to place static data values,
     *                      the expected use case is to reuse the std::vector to
     *                      prevent allocation noise for dynamic text.
     */
    static
    RenderData
    pack_glyph_data(RenderEngine &engine,
                    const Elements &elements,
                    std::vector<Vertex> *vert_storage,
                    std::vector<Index> *index_storage,
                    std::vector<gvec4> *static_values);

    /*!
     * The astral::ColorItemShader to use for scalable glyphs
     */
    reference_counted_ptr<const ColorItemShader> m_scalable_shader;

    /*!
     * The astral::ColorItemShader to use for image glyphs
     */
    reference_counted_ptr<const ColorItemShader> m_image_shader;
  };
}

#endif

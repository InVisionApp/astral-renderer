/*!
 * \file glyph_shader.cpp
 * \brief file glyph_shader.cpp
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

#include <astral/renderer/render_engine.hpp>
#include <astral/renderer/shader/glyph_shader.hpp>
#include <astral/text/text_item.hpp>

///////////////////////////////////////////////////
// astral::GlyphShader::ItemDataPackerBase methods
const astral::ItemDataValueMapping&
astral::GlyphShader::ItemDataPackerBase::
intrepreted_value_map(void) const
{
  static ItemDataValueMapping V;
  return V;
}

astral::BoundingBox<float>
astral::GlyphShader::ItemDataPackerBase::
bounding_box(const TextItem &text_item) const
{
  return text_item.bounding_box();
}

///////////////////////////////////////////////////
// astral::GlyphShader::SyntheticData methods
astral::BoundingBox<float>
astral::GlyphShader::SyntheticData::
bounding_box(const BoundingBox<float> &bb,
             const TypefaceMetricsBase &metrics) const
{
  BoundingBox<float> return_value;

  if (bb.empty())
    {
      return return_value;
    }

  vec2 m(bb.min_point()), M(bb.max_point());

  M.x() = m_skew.m_scale_x * (M.x() - m_line_start_x) + m_line_start_x;
  if (m_skew.m_skew_x > 0.0f)
    {
      /* leans forward */
      M.x() += metrics.m_height * t_abs(m_skew.m_skew_x);
    }
  else
    {
      /* leans backwards */
      m.x() -= metrics.m_height * t_abs(m_skew.m_skew_x);
    }
  return_value.union_point(m);
  return_value.union_point(M);

  return return_value;
}

astral::BoundingBox<float>
astral::GlyphShader::SyntheticData::
bounding_box(const TextItem &text_item) const
{
  return bounding_box(text_item.bounding_box(),
                      text_item.font().base_metrics());
}

/////////////////////////////////////
// astral::GlyphShader methods
astral::RenderData
astral::GlyphShader::
pack_glyph_data(RenderEngine &engine,
                const Elements &elements,
                std::vector<Vertex> *pvertices,
                std::vector<Index> *pindices,
                std::vector<gvec4> *pstatic_values)
{
  const RectEnums::corner_t rect_corners[] =
    {
      RectEnums::minx_miny_corner,
      RectEnums::minx_maxy_corner,
      RectEnums::maxx_maxy_corner,
      RectEnums::maxx_miny_corner
    };

  const static Index quad[] =
    {
      0, 1, 2,
      0, 2, 3
    };

  RenderData return_value;
  unsigned int N(elements.number_elements());
  Rect R;
  uint32_t location;
  std::vector<Vertex> &vertices(*pvertices);
  std::vector<Index> &indices(*pindices);
  std::vector<gvec4> &static_values(*pstatic_values);

  vertices.resize(4u * N);
  indices.resize(6u * N);
  static_values.resize(2u * N);
  for (unsigned int idx = 0, v_offset = 0, i_offset = 0; idx < N; ++idx)
    {
      vec2 pen_position, glyph_size;
      uint32_t flags;

      flags = elements.element(idx, &R, &pen_position, &location);

      static_values[2u * idx].x().f = pen_position.x();
      static_values[2u * idx].y().f = pen_position.y();
      static_values[2u * idx].z().f = R.width();
      static_values[2u * idx].w().f = R.height();

      static_values[2u * idx + 1u].x().u = location;
      static_values[2u * idx + 1u].y().u = flags;
      static_values[2u * idx + 1u].z().u = 0u;
      static_values[2u * idx + 1u].w().u = 0u;

      for (unsigned int k = 0; k < 6; ++k, ++i_offset)
        {
          indices[i_offset] = v_offset + quad[k];
        }

      for (unsigned int c = 0; c < 4; ++c, ++v_offset)
        {
          vec2 p;

          p = R.point(rect_corners[c]);
          vertices[v_offset].m_data[0].f = p.x();
          vertices[v_offset].m_data[1].f = p.y();
          vertices[v_offset].m_data[2].u = rect_corners[c];
        }
    }

  return_value.m_static_data = engine.static_data_allocator32().create(make_const_c_array(static_values));
  for (unsigned int idx = 0, v_offset = 0; idx < N; ++idx)
    {
      for (unsigned int c = 0; c < 4; ++c, ++v_offset)
        {
          vertices[v_offset].m_data[3].u = 2u * idx + return_value.m_static_data->location();
        }
    }

  return_value.m_vertex_data = engine.vertex_data_allocator().create(make_c_array(vertices), make_c_array(indices));

  return return_value;
}

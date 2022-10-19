/*!
 * \file text_item.cpp
 * \brief file text_item.cpp
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

#include <astral/path.hpp>
#include <astral/renderer/item_path.hpp>
#include <astral/renderer/render_engine.hpp>
#include <astral/renderer/shader/glyph_shader.hpp>
#include <astral/text/glyph.hpp>
#include <astral/text/text_item.hpp>

class astral::TextItem::PerGlyph
{
public:
  Glyph m_glyph;
  GlyphPaletteID m_palette;
  vec2 m_position;

  void
  compute_translated_rect(const Font &font,
                          Rect *out_rect)
  {
    vec2 p;

    compute_positions(font, out_rect, &p);
    out_rect->translate(p);
  }

  void
  compute_positions(const Font &font,
                    Rect *out_rect,
                    vec2 *out_pen_position) const
  {
    float scale_factor(font.scaling_factor());
    const GlyphMetrics &metrics(font.glyph_metrics(m_glyph));
    vec2 layout_offset(scale_factor * metrics.m_horizontal_layout_offset);
    vec2 glyph_size(scale_factor * metrics.m_size);

    out_pen_position->x() = m_position.x();
    out_pen_position->y() = m_position.y();

    out_rect->m_min_point.x() = layout_offset.x();
    out_rect->m_max_point.x() = out_rect->m_min_point.x() + glyph_size.x();

    /* the base line is at max_point.y() because y-increases downwards
     * in astral rendering
     */
    out_rect->m_max_point.y() = layout_offset.y();
    out_rect->m_min_point.y() = out_rect->m_max_point.y() - glyph_size.y();
  }
};

class astral::TextItem::PerRenderSize
{
public:
  /* ctor taking blindly from the astal::Font */
  explicit
  PerRenderSize(const astral::Font &font):
    m_strike(font.fixed_size_index()),
    m_pixel_size(font.pixel_size())
  {
  }

  /* ctor for non-scalable typeface */
  PerRenderSize(const astral::Typeface &face, int strike):
    m_strike(strike),
    m_pixel_size(face.fixed_metrics()[strike].m_pixel_size)
  {}

  uint32_t
  render_data_location(RenderEngine &engine, const PerGlyph &glyph) const
  {
    if (m_strike == -1)
      {
        return glyph.m_glyph.render_data(engine, glyph.m_palette)->location();
      }
    else
      {
        return glyph.m_glyph.image_render_data(engine, m_strike)->location();
      }
  }

  /* Comparison operator so that we can sort
   * PerRenderSize by pixel size.
   */
  bool
  operator<(const PerRenderSize &rhs) const
  {
    return m_pixel_size < rhs.m_pixel_size;
  }

  int m_strike;
  float m_pixel_size;

  RenderData m_render_data;
  std::vector<gvec4> m_static_values;
  std::vector<Vertex> m_verts;
  std::vector<Index> m_indices;
};

class astral::TextItem::GlyphElements:public GlyphShader::Elements
{
public:
  explicit
  GlyphElements(const TextItem &src, RenderEngine &engine, PerRenderSize &dst):
    m_src(src),
    m_engine(engine),
    m_dst(dst)
  {}

  virtual
  unsigned int
  number_elements(void) const override final
  {
    return m_src.m_glyphs.size();
  }

  virtual
  uint32_t
  element(unsigned int idx,
          Rect *out_position,
          vec2 *out_pen_position,
          uint32_t *out_shared_data_location) const override final
  {
    m_src.m_glyphs[idx].compute_positions(m_src.m_font, out_position, out_pen_position);
    *out_shared_data_location = m_dst.render_data_location(m_engine, m_src.m_glyphs[idx]);

    return (m_src.m_glyphs[idx].m_glyph.is_colored()) ?
      astral::GlyphShader::is_colored_glyph :
      0u;
  }

private:
  const TextItem &m_src;
  RenderEngine &m_engine;
  const PerRenderSize &m_dst;
};

class astral::TextItem::Helper
{
public:
  explicit
  Helper(TextItem &dst):
    m_dst(dst)
  {}

  static
  const vec2&
  position_value(const vec2 &p) { return p; }

  static
  vec2
  position_value(float p) { return vec2(p, 0.0f); }

  template<typename T>
  void
  add_glyphs(c_array<const GlyphIndex> glyph_indices,
             c_array<const T> glyph_positions,
             GlyphPaletteID P)
  {
    ASTRALassert(glyph_indices.size() == glyph_positions.size());

    if (glyph_indices.empty())
      {
        return;
      }

    for (PerRenderSize &prs : m_dst.m_per_render_size)
      {
        prs.m_render_data.clear();
      }

    float f(1.0f);

    if (m_dst.font().typeface().is_scalable())
      {
        f = 1.0f / m_dst.font().scaling_factor();
      }

    for (unsigned int i = 0; i < glyph_indices.size(); ++i)
      {
        Glyph g(m_dst.m_font.typeface().fetch_glyph(glyph_indices[i]));
        const GlyphMetrics &metrics(m_dst.m_font.glyph_metrics(g));

        if (metrics.m_size.x() > 0.0f && metrics.m_size.y() > 0.0f)
          {
            Rect pts;
            vec2 pos;

            pos = position_value(glyph_positions[i]);

            m_dst.m_glyphs.push_back(PerGlyph());

            m_dst.m_glyphs.back().m_glyph = g;
            m_dst.m_glyphs.back().m_position = pos;
            m_dst.m_glyphs.back().m_palette = P;

            m_dst.m_glyphs.back().compute_translated_rect(m_dst.m_font, &pts);
            m_dst.m_bb.union_box(pts);

            if (m_dst.font().typeface().is_scalable())
              {
                if (g.is_colored())
                  {
                    m_dst.m_color_glyphs.push_back(m_dst.m_glyphs.size() - 1u);
                  }
                else
                  {
                    const Path *path;
                    enum fill_rule_t fill_rule;
                    unsigned int layer(0);

                    path = g.path(layer, &fill_rule);
                    if (path)
                      {
                        /* The position is multiplied by the reciprocal of the position
                         * because the caller is expected to scale the rendering of the
                         * paths BEFORE drawing the CombinedPath values.
                         */
                        m_dst.m_combined_path_backings[fill_rule].push_back(path);
                        m_dst.m_combined_path_translate_backings[fill_rule].push_back(pos * f);
                      }
                  }
              }
          }
      }
  }

private:
  TextItem &m_dst;
};

/////////////////////////////
// astral::TextItem methods
astral::TextItem::
TextItem(const Font &font, enum image_glyph_handing_t handling):
  m_font(font)
{
  Typeface &typeface(font.typeface());
  if (typeface.is_scalable() || handling == use_strike_as_indicated_by_font)
    {
      m_per_render_size.push_back(PerRenderSize(m_font));
    }
  else
    {
      c_array<const TypefaceMetricsFixedSize> fixed_metrics(typeface.fixed_metrics());

      for (unsigned int i = 0; i < fixed_metrics.size(); ++i)
        {
          m_per_render_size.push_back(PerRenderSize(typeface, i));
        }
      std::sort(m_per_render_size.begin(), m_per_render_size.end());
    }
}

astral::TextItem::
TextItem(const Font &font, float max_pixel_size):
  TextItem(font, use_nearest_strike)
{
  /* m_per_render_size is sorted by pixel_size in
   * increasing order; all we do is just keep
   * popping the back.
   */
  while (m_per_render_size.size() > 1u
         && m_per_render_size.back().m_pixel_size > max_pixel_size)
    {
      m_per_render_size.pop_back();
    }
}

astral::TextItem::
~TextItem()
{
  clear();
}

void
astral::TextItem::
clear(void)
{
  m_glyphs.clear();
  m_bb.clear();
  m_color_glyphs.clear();
  for (unsigned int i = 0; i < number_fill_rule; ++i)
    {
      m_combined_path_backings[i].clear();
      m_combined_path_translate_backings[i].clear();
    }

  for (PerRenderSize &prs : m_per_render_size)
    {
      prs.m_render_data.clear();
    }
}

void
astral::TextItem::
clear(const Font &font)
{
  clear();
  m_font = font;
}

void
astral::TextItem::
add_glyphs(c_array<const GlyphIndex> glyph_indices,
           c_array<const vec2> glyph_positions,
           GlyphPaletteID P)
{
  Helper(*this).add_glyphs(glyph_indices, glyph_positions, P);
}

void
astral::TextItem::
add_glyphs(c_array<const GlyphIndex> glyph_indices,
           c_array<const float> glyph_positions,
           GlyphPaletteID P)
{
  Helper(*this).add_glyphs(glyph_indices, glyph_positions, P);
}

int
astral::TextItem::
compute_render_size_index(float zoom_factor) const
{
  int data_index;

  ASTRALassert(!m_per_render_size.empty());
  if (m_per_render_size.size() == 1)
    {
      data_index = 0;
    }
  else
    {
      float effective_pixel_size;

      effective_pixel_size = zoom_factor * m_font.pixel_size();
      for (data_index = m_per_render_size.size() - 1;
           data_index > 0 && effective_pixel_size < m_per_render_size[data_index].m_pixel_size;
           --data_index)
        {}
    }

  return data_index;
}

int
astral::TextItem::
strike_index(float zoom_factor) const
{
  int data_index;

  data_index = compute_render_size_index(zoom_factor);
  return m_per_render_size[data_index].m_strike;
}

const astral::RenderData&
astral::TextItem::
render_data(float zoom_factor, RenderEngine &engine, int *out_strike_index) const
{
  int data_index;

  data_index = compute_render_size_index(zoom_factor);
  if (!m_per_render_size[data_index].m_render_data.m_vertex_data)
    {
      GlyphElements packer(*this, engine, m_per_render_size[data_index]);

      ASTRALassert(!m_per_render_size[data_index].m_render_data.m_static_data);
      m_per_render_size[data_index].m_render_data = GlyphShader::pack_glyph_data(engine, packer,
                                                                                 &m_per_render_size[data_index].m_verts,
                                                                                 &m_per_render_size[data_index].m_indices,
                                                                                 &m_per_render_size[data_index].m_static_values);
    }

  if (out_strike_index)
    {
      *out_strike_index = m_per_render_size[data_index].m_strike;
    }

  return m_per_render_size[data_index].m_render_data;
}

unsigned int
astral::TextItem::
number_glyphs(void) const
{
  return m_glyphs.size();
}

void
astral::TextItem::
glyph(unsigned int idx, Glyph *out_glyph,
      vec2 *out_position, GlyphPaletteID *out_palette) const
{
  ASTRALassert(idx < m_glyphs.size());

  if (out_glyph)
    {
      *out_glyph = m_glyphs[idx].m_glyph;
    }

  if (out_position)
    {
      *out_position = m_glyphs[idx].m_position;
    }

  if (out_palette)
    {
      *out_palette = m_glyphs[idx].m_palette;
    }
}

enum astral::return_code
astral::TextItem::
combined_paths(vecN<CombinedPath, number_fill_rule> *out_paths,
               c_array<const unsigned int> *out_color_glyph_indices,
               float *out_scale_factor) const
{
  if (!m_font.typeface().is_scalable())
    {
      out_paths->x() = out_paths->y() = out_paths->z() = out_paths->w() = CombinedPath();
      *out_color_glyph_indices = c_array<const unsigned int>();
      *out_scale_factor = 1.0f;
      return routine_fail;
    }

  *out_scale_factor = m_font.scaling_factor();
  *out_color_glyph_indices = make_c_array(m_color_glyphs);
  for (unsigned int i = 0; i < number_fill_rule; ++i)
    {
      out_paths->operator[](i) = CombinedPath(make_c_array(m_combined_path_backings[i]),
                                              make_c_array(m_combined_path_translate_backings[i]));
    }

  return routine_success;
}

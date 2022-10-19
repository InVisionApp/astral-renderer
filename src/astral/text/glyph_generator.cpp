/*!
 * \file glyph_generator.cpp
 * \brief file glyph_generator.cpp
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

#include <astral/text/glyph_generator.hpp>
#include <astral/path.hpp>

namespace
{
  class TofuGlyphGenerator:public astral::GlyphGenerator
  {
  public:
    virtual
    uint32_t
    number_glyphs(void) const override final
    {
      return 1;
    }

    virtual
    void
    fill_character_mapping(unsigned int, astral::CharacterMapping&) const override final
    {
    }

    virtual
    unsigned int
    number_threads(void) const override final
    {
      return 1;
    }

    virtual
    const astral::TypefaceMetricsScalable*
    scalable_metrics(void) const override final
    {
      return &m_typeface_metrics;
    }

    virtual
    astral::c_array<const astral::TypefaceMetricsFixedSize>
    fixed_metrics(void) const override final
    {
      return astral::c_array<const astral::TypefaceMetricsFixedSize>();
    }

    virtual
    void
    scalable_glyph_info(unsigned int thread_slot,
                        astral::GlyphIndex glyph_index,
                        astral::GlyphMetrics *out_metrics,
                        astral::GlyphColors *out_layer_colors,
                        std::vector<astral::Path> *out_paths,
                        std::vector<enum astral::fill_rule_t> *out_fill_rules) const override final
    {
      ASTRALassert(glyph_index.m_value == 0);
      ASTRALunused(glyph_index);
      ASTRALunused(out_layer_colors);
      ASTRALunused(thread_slot);

      *out_metrics = m_glyph_metrics;
      out_paths->push_back(m_path);
      out_fill_rules->push_back(astral::odd_even_fill_rule);
    }

    virtual
    bool
    fixed_glyph_info(unsigned int,
                     astral::GlyphIndex, unsigned int,
                     astral::GlyphMetrics*, astral::ivec2*,
                     std::vector<astral::FixedPointColor_sRGB>*) const override final
    {
      return false;
    }

    static
    astral::reference_counted_ptr<astral::GlyphGenerator>
    singleton(void);

  private:
    class Singleton
    {
    public:
      Singleton(void)
      {
        m_v = ASTRALnew TofuGlyphGenerator();
      }

      astral::reference_counted_ptr<astral::GlyphGenerator> m_v;
    };

    TofuGlyphGenerator(void);

    astral::TypefaceMetricsScalable m_typeface_metrics;
    astral::GlyphMetrics m_glyph_metrics;
    astral::Path m_path;
  };
}

////////////////////////////
// TofuGlyphGenerator methods
TofuGlyphGenerator::
TofuGlyphGenerator(void)
{
  m_glyph_metrics.m_horizontal_layout_offset = astral::vec2(0.0f, 0.0f);
  m_glyph_metrics.m_vertical_layout_offset = astral::vec2(0.0f, 0.0f);
  m_glyph_metrics.m_size = astral::vec2(500.0, 1000.0f);
  m_glyph_metrics.m_advance = astral::vec2(1.1f * m_glyph_metrics.m_size.x(), 0.0f);

  m_typeface_metrics.m_height = m_glyph_metrics.m_size.y();
  m_typeface_metrics.m_ascender = 1.2f * m_glyph_metrics.m_size.y();
  m_typeface_metrics.m_descender = 0.0f;
  m_typeface_metrics.m_strikeout_position = 0.5f * m_glyph_metrics.m_size.y();
  m_typeface_metrics.m_underline_position = 0.0f;
  m_typeface_metrics.m_strikeout_thickness = 0.05f * m_glyph_metrics.m_size.y();
  m_typeface_metrics.m_underline_thickness = 0.05f * m_glyph_metrics.m_size.y();
  m_typeface_metrics.m_units_per_EM = m_glyph_metrics.m_size.y();

  m_path
    .move(astral::vec2(0.0f, 0.0f))
    .line_to(astral::vec2(m_glyph_metrics.m_size.x(), 0.0f))
    .line_to(m_glyph_metrics.m_size)
    .line_to(astral::vec2(0.0f, m_glyph_metrics.m_size.y()))
    .line_close()
    .move(0.1f * m_glyph_metrics.m_size)
    .line_to(astral::vec2(0.9f * m_glyph_metrics.m_size.x(), 0.1f * m_glyph_metrics.m_size.y()))
    .line_to(astral::vec2(0.9f * m_glyph_metrics.m_size.x(), 0.9f * m_glyph_metrics.m_size.y()))
    .line_to(astral::vec2(0.1f * m_glyph_metrics.m_size.x(), 0.9f * m_glyph_metrics.m_size.y()))
    .line_close();
}

astral::reference_counted_ptr<astral::GlyphGenerator>
TofuGlyphGenerator::
singleton(void)
{
  static Singleton S;
  return S.m_v;
}

//////////////////////////////////
// astral::GlyphGenerator methods
astral::reference_counted_ptr<astral::GlyphGenerator>
astral::GlyphGenerator::
tofu_generator(void)
{
  return TofuGlyphGenerator::singleton();
}

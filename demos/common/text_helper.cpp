/*!
 * \file text_helper.cpp
 * \brief text_helper.cpp
 *
 * Copyright 2020 by InvisionApp.
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

#include <iomanip>
#include "text_helper.hpp"

static
std::string
substitute_tabs(const std::string &v)
{
  std::string return_value;

  for (char ch : v)
    {
      if (ch != '\t')
        {
          return_value.push_back(ch);
        }
      else
        {
          return_value.push_back(' ');
          return_value.push_back(' ');
          return_value.push_back(' ');
          return_value.push_back(' ');
        }
    }
  return return_value;
}

static
void
add_line_text(astral::vec2 &pen, const std::string &in_line,
              astral::TextItem &text_item)
{
  float scaling_factor(text_item.font().scaling_factor());
  astral::Typeface &typeface(text_item.font().typeface());
  std::string line(substitute_tabs(in_line));

  for (char ch : line)
    {
      astral::GlyphIndex glyph_index;
      astral::Glyph glyph;

      glyph_index = typeface.glyph_index(ch);
      glyph = typeface.fetch_glyph(glyph_index);
      ASTRALassert(glyph.valid());

      text_item.add_glyph(glyph_index, pen);
      pen.x() += scaling_factor * text_item.font().glyph_metrics(glyph).m_advance.x();
    }
}

float
add_text(float y, std::istream &stream,
         astral::TextItem &text_item)
{
  const astral::Font &font(text_item.font());
  float height(font.base_metrics().m_height);
  astral::vec2 pen(0.0f, y);
  std::string line;

  while (std::getline(stream, line))
    {
      pen.x() = 0.0f;
      add_line_text(pen, line, text_item);
      pen.y() += height;
    }

  return pen.y();
}

void
set_and_draw_hud(astral::RenderEncoderBase encoder, float frame_ms,
                 astral::c_array<const unsigned int> stat_values,
                 astral::TextItem &text_item, const std::string &additional_text,
                 astral::c_array<const astral::Renderer::renderer_stats_t> render_stats,
                 astral::c_array<const enum astral::RenderBackend::render_backend_stats_t> backend_stats,
                 astral::c_array<const unsigned int> gl3_backend_stats)
{
  astral::Renderer &renderer(encoder.renderer());
  astral::c_array<const astral::c_string> stats_labels(renderer.stats_labels());
  std::ostringstream hud_text;

  hud_text << "\n";
  if (frame_ms > 0.0f)
    {
      hud_text << "FPS = " << std::setw(8)
               << std::setprecision(2) << std::fixed
               << 1000.0f / frame_ms
               << "\n(" << std::setw(6)
               << frame_ms << " ms)\n"
	       << "clip-strategy: "
	       << astral::label(renderer.overridable_properties().m_clip_window_strategy)
	       << "\n";
    }

  for (unsigned int i = 0; i < render_stats.size(); ++i)
    {
      unsigned int idx;

      idx = renderer.stat_index(render_stats[i]);
      hud_text << stats_labels[idx] << " = " << stat_values[idx] << "\n";
    }

  for (unsigned int i = 0; i < backend_stats.size(); ++i)
    {
      unsigned int idx;

      idx = renderer.stat_index(backend_stats[i]);
      hud_text << stats_labels[idx] << " = " << stat_values[idx] << "\n";
    }

  for (unsigned int i = 0; i < gl3_backend_stats.size(); ++i)
    {
      astral::RenderBackend::DerivedStat st(gl3_backend_stats[i]);
      unsigned int idx;

      idx = renderer.stat_index(st);
      hud_text << stats_labels[idx] << " = " << stat_values[idx] << "\n";
    }

  hud_text << additional_text;

  text_item.clear();
  add_text(0.0f, hud_text.str(), text_item);

  encoder.draw_rect(text_item.bounding_box().as_rect(),
                    encoder.create_value(astral::Brush().base_color(astral::vec4(0.0f, 0.0f, 0.0f, 0.50f))));

  encoder.draw_text(text_item,
                    encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 1.0f, 0.85f))));
}

void
set_and_draw_hud_all_stats(astral::RenderEncoderBase encoder, float frame_ms,
                           astral::c_array<const unsigned int> stat_values,
                           astral::TextItem &text_item, const std::string &additional_text)
{
  astral::Renderer &renderer(encoder.renderer());
  astral::c_array<const astral::c_string> stats_labels(renderer.stats_labels());
  std::ostringstream hud_text;

  hud_text << "\n";
  if (frame_ms > 0.0f)
    {
      hud_text << "FPS = " << std::setw(8)
               << std::setprecision(2) << std::fixed
               << 1000.0f / frame_ms
               << "\n(" << std::setw(6)
               << frame_ms << " ms)\n";
    }

  for (unsigned int idx = 0; idx < stats_labels.size(); ++idx)
    {
      hud_text << stats_labels[idx] << " = " << stat_values[idx] << "\n";
    }

  hud_text << additional_text;

  text_item.clear();
  add_text(0.0f, hud_text.str(), text_item);

  encoder.draw_rect(text_item.bounding_box().as_rect(),
                    encoder.create_value(astral::Brush().base_color(astral::vec4(0.0f, 0.0f, 0.0f, 0.50f))));

  encoder.draw_text(text_item,
                    encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 1.0f, 0.85f))));
}

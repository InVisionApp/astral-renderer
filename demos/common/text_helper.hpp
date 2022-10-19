/*!
 * \file text_helper.hpp
 * \brief text_helper.hpp
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

#ifndef ASTRA_DEMO_TEXT_HELPER_HPP
#define ASTRA_DEMO_TEXT_HELPER_HPP

#include <istream>
#include <sstream>
#include <astral/text/text_item.hpp>
#include <astral/renderer/render_engine.hpp>
#include <astral/renderer/renderer.hpp>

/*!
 * Add the text of an istream to an astral::TextItem accepting
 * end-of-lines.
 */
float
add_text(float y, std::istream &str,
         astral::TextItem &text_item);

inline
float
add_text(std::istream &str,
         astral::TextItem &text_item)
{
  return add_text(0.0f, str, text_item);
}

inline
float
add_text(float y, const std::string &str,
         astral::TextItem &text_item)
{
  std::istringstream i(str);
  return add_text(y, i, text_item);
}

inline
float
add_text(const std::string &str,
         astral::TextItem &text_item)
{
  return add_text(0.0f, str, text_item);
}

void
set_and_draw_hud(astral::RenderEncoderBase encoder, float frame_ms,
                 astral::c_array<const unsigned int> stat_values,
                 astral::TextItem &text_item, const std::string &additional_text = "",
                 astral::c_array<const astral::Renderer::renderer_stats_t> render_stats
                 = astral::c_array<const astral::Renderer::renderer_stats_t>(),
                 astral::c_array<const enum astral::RenderBackend::render_backend_stats_t> backend_stats
                 = astral::c_array<const enum astral::RenderBackend::render_backend_stats_t>(),
                 astral::c_array<const unsigned int> gl3_backend_stats
                 = astral::c_array<const unsigned int>());

inline
void
set_and_draw_hud(astral::RenderEncoderBase encoder, float frame_ms,
                 astral::TextItem &text_item, const std::string &additional_text = "")
{
  set_and_draw_hud(encoder, frame_ms,
                   astral::c_array<unsigned int>(),
                   text_item, additional_text);
}

inline
void
set_and_draw_hud(astral::RenderEncoderBase encoder,
                 astral::TextItem &text_item, const std::string &additional_text = "")
{
  set_and_draw_hud(encoder, -1.0f,
                   astral::c_array<unsigned int>(),
                   text_item, additional_text);
}

inline
void
set_and_draw_hud(astral::RenderEncoderBase encoder, float frame_ms,
                 astral::c_array<const unsigned int> stat_values,
                 astral::TextItem &text_item, const std::string &additional_text,
                 astral::c_array<const enum astral::RenderBackend::render_backend_stats_t> backend_stats,
                 astral::c_array<const unsigned int> gl3_backend_stats
                 = astral::c_array<const unsigned int>())
{
  set_and_draw_hud(encoder, frame_ms, stat_values, text_item, additional_text,
                   astral::c_array<const astral::Renderer::renderer_stats_t>(),
                   backend_stats, gl3_backend_stats);
}

inline
void
set_and_draw_hud(astral::RenderEncoderBase encoder, float frame_ms,
                 astral::c_array<const unsigned int> stat_values,
                 astral::TextItem &text_item, const std::string &additional_text,
                 astral::c_array<const unsigned int> gl3_backend_stats)
{
  set_and_draw_hud(encoder, frame_ms, stat_values, text_item, additional_text,
                   astral::c_array<const astral::Renderer::renderer_stats_t>(),
                   astral::c_array<const enum astral::RenderBackend::render_backend_stats_t>(),
                   gl3_backend_stats);
}

void
set_and_draw_hud_all_stats(astral::RenderEncoderBase encoder, float frame_ms,
                           astral::c_array<const unsigned int> stat_values,
                           astral::TextItem &text_item,
                           const std::string &additional_text= "");

#endif

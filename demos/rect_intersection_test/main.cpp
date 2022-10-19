/*!
 * \file main.cpp
 * \brief main.cpp
 *
 * Copyright 2020 by InvisionApp.
 *
 * Contact kevinrogovin@invisionapp.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 */
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <dirent.h>
#include <stdlib.h>

#include <astral/util/ostream_utility.hpp>
#include <astral/renderer/gl3/render_target_gl3.hpp>
#include <astral/renderer/renderer.hpp>

#include "render_engine_gl3_demo.hpp"
#include "sdl_demo.hpp"
#include "read_path.hpp"
#include "command_line_list.hpp"
#include "ImageLoader.hpp"
#include "PanZoomTracker.hpp"
#include "cycle_value.hpp"
#include "text_helper.hpp"

class RectDifferenceTest:public render_engine_gl3_demo
{
public:
  RectDifferenceTest(void);

protected:
  virtual
  void
  init_gl(int, int) override;

  virtual
  void
  draw_frame(void) override;

  virtual
  void
  handle_event(const SDL_Event &ev) override;

private:
  void
  draw_outlined_rect(astral::RenderEncoderSurface encoder,
                     const astral::Rect &rect,
                     astral::RenderValue<astral::Brush> border,
                     astral::RenderValue<astral::Brush> interior);

  astral::vecN<PanZoomTrackerSDLEvent, 2> m_rects;
  astral::reference_counted_ptr<astral::TextItem> m_text_item;
};

//////////////////////////////////
//RectDifferenceTest  methods
RectDifferenceTest::
RectDifferenceTest(void)
{
  std::cout << "\tLeft-button + Mouse : move and zoom red rect\n"
            << "\tRight-button + Mouse : move and zoom green rect\n";
}

void
RectDifferenceTest::
init_gl(int w, int h)
{
  UniformScaleTranslate<float> tr;

  tr.m_translation = 0.5f * astral::vec2(w, h);
  m_rects[0].transformation(tr);

  float pixel_size(32.0f);
  astral::Font font(default_typeface(), pixel_size);
  m_text_item = astral::TextItem::create(font);
}

void
RectDifferenceTest::
handle_event(const SDL_Event &ev)
{
  m_rects[0].handle_event(ev, SDL_BUTTON_LEFT);
  m_rects[1].handle_event(ev, SDL_BUTTON_RIGHT);
  render_engine_gl3_demo::handle_event(ev);
}

void
RectDifferenceTest::
draw_outlined_rect(astral::RenderEncoderSurface encoder,
                   const astral::Rect &inner_rect,
                   astral::RenderValue<astral::Brush> border,
                   astral::RenderValue<astral::Brush> interior)
{
  astral::Rect outer_rect;
  float thickness;

  thickness = 2.0f;
  outer_rect.m_min_point = outer_rect.m_min_point - astral::vec2(thickness);
  outer_rect.m_max_point = outer_rect.m_max_point + astral::vec2(thickness);

  encoder.draw_rect(outer_rect, border);
  encoder.draw_rect(inner_rect, interior);
}

void
RectDifferenceTest::
draw_frame(void)
{
  astral::vecN<astral::Rect, 2> rects;
  astral::Rect rect_intersection;
  bool intersects;
  astral::RenderEncoderSurface encoder;
  std::ostringstream txt;

  encoder = renderer().begin( render_target());

  for (unsigned int i = 0; i < 2; ++i)
    {
      rects[i].m_min_point = m_rects[i].transformation().apply_to_point(astral::vec2(0.0f, 0.0f));
      rects[i].m_max_point = m_rects[i].transformation().apply_to_point(astral::vec2(100.0f, 100.0f));
    }

  intersects = astral::Rect::compute_intersection(rects[0], rects[1], &rect_intersection);
  if (intersects)
    {
      astral::vecN<astral::Rect, 8> diff;
      unsigned int cnt;
      astral::RenderValue<astral::Brush> black;
      static const astral::vec4 colors[] =
        {
          astral::vec4(1.0f, 0.0f, 0.0f, 0.5f),
          astral::vec4(0.0f, 1.0f, 0.0f, 0.5f),
          astral::vec4(0.0f, 0.0f, 1.0f, 0.5f),
          astral::vec4(0.0f, 1.0f, 1.0f, 0.5f),
          astral::vec4(1.0f, 0.0f, 1.0f, 0.5f),
          astral::vec4(1.0f, 1.0f, 0.0f, 0.5f),
          astral::vec4(0.5f, 0.5f, 1.0f, 0.5f),
          astral::vec4(0.5f, 1.0f, 0.5f, 0.5f),
        };

      black = encoder.create_value(astral::Brush().base_color(astral::vec4(0.0f, 0.0f, 0.0f, 1.0f)));

      cnt = rects[1].compute_difference_for_rasterization(rects[0], diff);
      for (unsigned int i = 0; i < cnt; ++i)
        {
          draw_outlined_rect(encoder, diff[i], black,
                             encoder.create_value(astral::Brush().base_color(colors[i])));
        }
      draw_outlined_rect(encoder, rect_intersection, black,
                         encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 1.0f, 1.0f))));

      txt << "\nIntersects, cnt = " << cnt << "\n";
    }
  else
    {
      encoder.draw_rect(rects[0], encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 0.0f, 0.0f, 0.5f))));
      encoder.draw_rect(rects[1], encoder.create_value(astral::Brush().base_color(astral::vec4(0.0f, 1.0f, 0.0f, 0.5f))));
      txt << "\nNo intersection\n";
    }

  m_text_item->clear();
  add_text(0.0f, txt.str(), *m_text_item);
  encoder.draw_rect(m_text_item->bounding_box().as_rect(),
                    encoder.create_value(astral::Brush().base_color(astral::vec4(0.0f, 0.0f, 0.0f, 0.50f))));

  encoder.draw_text(*m_text_item,
                    encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 1.0f, 0.85f))));

  renderer().end();
}

int
main(int argc, char **argv)
{
  RectDifferenceTest M;
  return M.main(argc, argv);
}

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
#include <fstream>
#include <random>
#include <dirent.h>
#include <stdlib.h>

#include <astral/util/ostream_utility.hpp>
#include <astral/renderer/renderer.hpp>
#include <astral/renderer/gl3/render_target_gl3.hpp>

#include "command_line_list.hpp"
#include "sdl_demo.hpp"
#include "render_engine_gl3_demo.hpp"
#include "PanZoomTracker.hpp"
#include "read_path.hpp"
#include "cycle_value.hpp"
#include "text_helper.hpp"

class command_line_list_paths:public command_line_list_loader<astral::Path>
{
public:
  command_line_list_paths(const std::string &nm,
                           const std::string &desc,
                           command_line_register &p):
    command_line_list_loader<astral::Path>(nm, desc, p)
  {}

private:
  virtual
  enum astral::return_code
  load_element(const std::string &filename, astral::Path *out_value)
  {
    std::ifstream path_file(filename.c_str());
    if (path_file)
      {
        read_path(out_value, path_file);
        return (out_value->number_contours() == 0u) ?
          astral::routine_fail:
          astral::routine_success;
      }
    return astral::routine_fail;
  }
};

class RandomColorGenerator
{
public:
  RandomColorGenerator(void):
    m_dist(0, 255)
  {}

  astral::vec4
  operator()(void)
  {
    float r, g, b;

    r = static_cast<float>(m_dist(m_generator));
    g = static_cast<float>(m_dist(m_generator));
    b = static_cast<float>(m_dist(m_generator));

    return astral::vec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
  }

private:
  std::mt19937 m_generator;
  std::uniform_int_distribution<> m_dist;
};

class EncodersSurface:public render_engine_gl3_demo
{
public:
  EncodersSurface(void);

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
  class PerScene
  {
  public:
    PanZoomTrackerSDLEvent m_zoom;
    const astral::Path *m_path;
    astral::vec4 m_fill_color, m_stroke_color;

    void
    draw_scene(astral::RenderEncoderBase encoder);
  };

  void
  add_viewport(astral::RectT<int> r, RandomColorGenerator &G);

  void
  resize_viewports(int w, int h);

  void
  compute_viewport_rects(int w, int h, std::vector<astral::RectT<int>> *out_rects);

  command_separator m_demo_options;
  command_line_list_paths m_add_paths;

  astral::Path m_rect_path;
  std::vector<astral::Path> m_paths;
  std::vector<astral::Renderer::SubViewport> m_viewports;
  std::vector<PerScene> m_scenes;
  std::vector<astral::RenderEncoderSurface> m_encoders;
  astral::reference_counted_ptr<astral::TextItem> m_text_item;

  unsigned int m_current_viewport;
  simple_time m_draw_timer;
};

//////////////////////////////////
// EncodersSurface methods
EncodersSurface::
EncodersSurface(void):
  m_demo_options("Demo Options", *this),
  m_add_paths("add_path", "add a path file from which to read a path", *this),
  m_current_viewport(0)
{
  std::cout << "Controls:"
            << "\n\tv: change active viewport"
            << "\n";
}

void
EncodersSurface::
add_viewport(astral::RectT<int> rect, RandomColorGenerator &G)
{
  astral::Renderer::SubViewport V;
  unsigned int path_idx(m_scenes.size() % m_paths.size());
  PerScene P;

  V.m_xy = rect.m_min_point;
  V.m_size = rect.size();

  P.m_path = &m_paths[path_idx];
  P.m_zoom.m_translate_event = -astral::vec2(V.m_xy);
  P.m_stroke_color = G();
  P.m_fill_color = G();

  astral::BoundingBox<float> bb;
  bb = P.m_path->bounding_box();
  if (!bb.empty())
    {
      astral::vec2 screen_pt(V.m_size.x() / 2, V.m_size.y() / 2), path_pt;
      UniformScaleTranslate<float> tr;

      path_pt = (bb.min_point() + bb.max_point()) * 0.5f;
      tr.m_translation = screen_pt - path_pt;
      P.m_zoom.transformation(tr);
    }

  m_scenes.push_back(P);
  m_viewports.push_back(V);
  m_encoders.push_back(astral::RenderEncoderSurface());
}

void
EncodersSurface::
compute_viewport_rects(int w, int h, std::vector<astral::RectT<int>> *out_rects)
{
  out_rects->clear();
  out_rects->push_back(astral::RectT<int>().min_point(0, 0).max_point(w / 2, h / 3));
  out_rects->push_back(astral::RectT<int>().min_point(w / 2, 0).max_point((3 * w) / 4, h / 3));
  out_rects->push_back(astral::RectT<int>().min_point((3 * w) / 4, 0).max_point(w, h / 3));
  out_rects->push_back(astral::RectT<int>().min_point(0, h / 3).max_point(w, (2 * h) / 3));
  out_rects->push_back(astral::RectT<int>().min_point(0, (2 * h) / 3).max_point(w / 3, h));
  out_rects->push_back(astral::RectT<int>().min_point(w / 3, (2 * h) / 3).max_point(w, h));
}

void
EncodersSurface::
resize_viewports(int w, int h)
{
  std::vector<astral::RectT<int>> rects;

  compute_viewport_rects(w, h, &rects);

  ASTRALassert(rects.size() == m_viewports.size());
  ASTRALassert(rects.size() == m_scenes.size());
  ASTRALassert(rects.size() == m_encoders.size());

  for (unsigned int i = 0, endi = rects.size(); i < endi; ++i)
    {
      astral::vec2 old_size(m_viewports[i].m_size);
      astral::vec2 new_size(rects[i].size());
      astral::vec2 sc(new_size / old_size);
      UniformScaleTranslate<float> tr(astral::t_min(sc.x(), sc.y()));

      m_viewports[i].m_xy = rects[i].m_min_point;
      m_viewports[i].m_size = rects[i].size();

      m_scenes[i].m_zoom.m_translate_event = -astral::vec2(rects[i].m_min_point);
      m_scenes[i].m_zoom.transformation(tr * m_scenes[i].m_zoom.transformation());
    }
}

void
EncodersSurface::
init_gl(int w, int h)
{
  for (const auto &v : m_add_paths.elements())
    {
      m_paths.push_back(v.m_loaded_value);
    }

  if (m_paths.empty())
    {
      const char *default_path =
        "[ (50.0, 35.0) [[(60.0, 50.0) ]] (70.0, 35.0)\n"
        "arc 180 (70.0, -100.0)\n"
        "[[ (60.0, -150.0) (30.0, -50.0) ]]\n"
        "(0.0, -100.0) arc 90 ]\n"
        "{ (200, 200) (400, 200) (400, 400) (200, 400) }\n"
        "[ (-50, 100) (0, 200) (100, 300) (150, 325) (150, 100) ]\n"
        "{ (300 300) }\n"
        ;

      astral::Path P;

      read_path(&P, default_path);
      m_paths.push_back(P);
    }

  m_rect_path
    .move(astral::vec2(0.0f, 0.0f))
    .line_to(astral::vec2(0.0f, 1.0f))
    .line_to(astral::vec2(1.0f, 1.0f))
    .line_to(astral::vec2(1.0f, 0.0f))
    .line_close();

  std::vector<astral::RectT<int>> rects;
  RandomColorGenerator G;

  compute_viewport_rects(w, h, &rects);
  for (const astral::RectT<int> &r : rects)
    {
      add_viewport(r, G);
    }

  float pixel_size(32.0f);
  astral::Font font(default_typeface(), pixel_size);
  m_text_item = astral::TextItem::create(font);
}

void
EncodersSurface::
handle_event(const SDL_Event &ev)
{
  m_scenes[m_current_viewport].m_zoom.handle_event(ev);
  switch (ev.type)
    {
    case SDL_KEYDOWN:
      {
        switch (ev.key.keysym.sym)
          {
          case SDLK_v:
            {
              cycle_value(m_current_viewport,
                          ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                          m_viewports.size());
            }
            break;
          }
      }
      break;

    case SDL_WINDOWEVENT:
      {
        if (ev.window.event == SDL_WINDOWEVENT_RESIZED)
          {
            resize_viewports(ev.window.data1, ev.window.data2);
          }
      }
      break;
    }
  render_engine_gl3_demo::handle_event(ev);
}

void
EncodersSurface::PerScene::
draw_scene(astral::RenderEncoderBase encoder)
{
  encoder.transformation(m_zoom.transformation().astral_transformation());

  encoder.fill_paths(*m_path,
                     astral::FillParameters(),
                     encoder.create_value(astral::Brush().base_color(m_fill_color)));

  encoder.stroke_paths(*m_path,
                       astral::StrokeParameters(),
                       encoder.create_value(astral::Brush().base_color(m_stroke_color)));
}

void
EncodersSurface::
draw_frame(void)
{
  float frame_ms;

  frame_ms = static_cast<float>(m_draw_timer.restart_us()) * 0.001f;
  renderer().begin(astral::colorspace_srgb);
  renderer().encoders_surface(render_target(),
                              astral::make_c_array(m_viewports),
                              astral::make_c_array(m_encoders));

  for (unsigned int i = 0, endi = m_encoders.size(); i < endi; ++i)
    {
      m_scenes[i].draw_scene(m_encoders[i]);
      if (i == m_current_viewport)
        {
          /* map [0, 1]x[0, 1] to [0, X]x[0, Y]
           * where (X, Y) = m_viewports[i].m_size
           */
          astral::vec2 scale(m_viewports[i].m_size), translate(0.0f, 0.0f);

          m_encoders[i].transformation(astral::Transformation());
          m_encoders[i].direct_stroke_paths(astral::CombinedPath(m_rect_path, translate, scale),
                                            astral::StrokeParameters());

          set_and_draw_hud(m_encoders[i], frame_ms, *m_text_item);
        }
    }

  renderer().end();
}

int
main(int argc, char **argv)
{
  EncodersSurface M;
  return M.main(argc, argv);
}

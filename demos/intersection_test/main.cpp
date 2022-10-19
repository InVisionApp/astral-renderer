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
#include <astral/util/transformed_bounding_box.hpp>
#include <astral/renderer/gl3/render_target_gl3.hpp>
#include <astral/renderer/renderer.hpp>

#include "render_engine_gl3_demo.hpp"
#include "sdl_demo.hpp"
#include "text_helper.hpp"
#include "command_line_list.hpp"
#include "ImageLoader.hpp"
#include "PanZoomTracker.hpp"
#include "cycle_value.hpp"

class IntersectionTest:public render_engine_gl3_demo
{
public:
  IntersectionTest(void);

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
  class PerBox
  {
  public:
    PerBox(void):
      m_scale_pre_rotate(1.0f, 1.0f),
      m_scale_post_rotate(1.0f, 1.0f),
      m_rotate_angle(0.0f),
      m_flip(false),
      m_draw_ui_rects(false)
    {}

    PanZoomTrackerSDLEvent m_zoom;
    astral::vec2 m_scale_pre_rotate, m_scale_post_rotate;
    float m_rotate_angle;
    bool m_flip, m_draw_ui_rects;
    astral::BoundingBox<float> m_bb;
  };

  float
  update_smooth_values(void);

  void
  draw_ui_rect(astral::RenderEncoderSurface dst,
               astral::vec2 position,
               astral::vec4 color);

  astral::vecN<PerBox, 2> m_boxes;
  simple_time m_draw_timer;
  astral::reference_counted_ptr<astral::TextItem> m_text_item;
  bool m_test_normalized;
};

//////////////////////////////////
// IntersectionTest methods
IntersectionTest::
IntersectionTest(void):
  m_test_normalized(true)
{
  std::cout << "Controls:"
            << "\n\t6: increase horizontal pre-rotate scale (hold alt to effect 2nd rectangle) (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\tctrl-6: decrease horizontal pre-rotate scale (hold alt to effect 2nd rectangle) (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\t7: increase vertical pre-rotate scale (hold alt to effect 2nd rectangle) (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\tctrl-7: decrease vertical pre-rotate scale (hold alt to effect 2nd rectangle) (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + 6: increase horizontal post-rotate scale (hold alt to effect 2nd rectangle) (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + ctrl-6: decrease horizontal post-rotate scale (hold alt to effect 2nd rectangle)  (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + 7: increase vertical post-rotate scale (hold alt to effect 2nd rectangle) (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + ctrl-7: decrease vertical post-rotate scale (hold alt to effect 2nd rectangle) (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\t9/0 increase/decrease angle of rotation (hold alt to effect 2nd rectangle) (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\tr: reset rotation to 0 degrees (hold alt to effect 2nd rectangle)"
            << "\n\ts: reset pre-scale to (1, 1) (hold alt to effect 2nd rectangle)"
            << "\n\tshift-s: reset post-scale to (1, 1) (hold alt to effect 2nd rectangle)"
            << "\n\tf: toggle flip (hold alt to effect 2nd rectangle)"
            << "\n\tp: toggle drawing squares at corners (hold alt to effect 2nd rectangle)"
            << "\t\nspace: toggle testing with normalized boxes\n"
            << "\n";
}

void
IntersectionTest::
init_gl(int w, int h)
{
  float pixel_size(32.0f);
  astral::Font font(default_typeface(), pixel_size);
  m_text_item = astral::TextItem::create(font);

  astral::vec2 wh(w, h);
  m_boxes[0].m_bb = astral::BoundingBox<float>(0.1f * wh, 0.4f * wh);
  m_boxes[1].m_bb = astral::BoundingBox<float>(0.6f * wh, 0.9f * wh);
}

void
IntersectionTest::
handle_event(const SDL_Event &ev)
{
  m_boxes[0].m_zoom.handle_event(ev, SDL_BUTTON_LEFT);
  m_boxes[1].m_zoom.handle_event(ev, SDL_BUTTON_RIGHT);
  switch (ev.type)
    {
    case SDL_KEYDOWN:
      {
        unsigned int which;

        which = (ev.key.keysym.mod & KMOD_ALT) ? 1 : 0;
        switch (ev.key.keysym.sym)
          {
          case SDLK_r:
            m_boxes[which].m_rotate_angle = 0.0f;
            std::cout << "Rotation of box # " << which << " reset to 0.\n";
            break;

          case SDLK_s:
            if (ev.key.keysym.mod & (KMOD_SHIFT | KMOD_CTRL))
              {
                m_boxes[which].m_scale_post_rotate = astral::vec2(1.0f, 1.0f);
                std::cout << "Post-scale of box # " << which << " reset to (1, 1).\n";
              }
            else
              {
                m_boxes[which].m_scale_pre_rotate = astral::vec2(1.0f, 1.0f);
                std::cout << "Pre-scale of box # " << which << " reset to (1, 1).\n";
              }
            break;

          case SDLK_f:
            m_boxes[which].m_flip = !m_boxes[which].m_flip;
            std::cout << "Flip of box # " << which << " set to " << std::boolalpha << m_boxes[which].m_flip << ".\n";
            break;

          case SDLK_p:
            m_boxes[which].m_draw_ui_rects = !m_boxes[which].m_draw_ui_rects;
            break;

          case SDLK_SPACE:
            m_test_normalized = !m_test_normalized;
            std::cout << "Testing with normalized boxes set to " << std::boolalpha << m_test_normalized << ".\n";
            break;
          }
      };
    }
  render_engine_gl3_demo::handle_event(ev);
}

float
IntersectionTest::
update_smooth_values(void)
{
  const Uint8 *keyboard_state = SDL_GetKeyboardState(nullptr);
  float return_value, delta, scale_delta, angle_delta;
  astral::vec2 *scale_ptr(nullptr);
  astral::c_string scale_txt(nullptr);
  unsigned int which;

  ASTRALassert(keyboard_state != nullptr);
  return_value = delta = static_cast<float>(m_draw_timer.restart_us()) * 0.001f;

  which = (keyboard_state[SDL_SCANCODE_LALT] || keyboard_state[SDL_SCANCODE_RALT]) ? 1u : 0u;
  if (keyboard_state[SDL_SCANCODE_LSHIFT])
    {
      delta *= 0.1f;
    }
  if (keyboard_state[SDL_SCANCODE_RSHIFT])
    {
      delta *= 10.0f;
    }

  scale_delta = 0.01f * delta;
  angle_delta = 0.0025f * delta;
  if (keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
    {
      scale_delta = -scale_delta;
    }

  if (keyboard_state[SDL_SCANCODE_RETURN])
    {
      scale_ptr = &m_boxes[which].m_scale_post_rotate;
      scale_txt = "post-rotate-scale";
    }
  else
    {
      scale_ptr = &m_boxes[which].m_scale_pre_rotate;
      scale_txt = "pre-rotate-scale";
    }

  if (keyboard_state[SDL_SCANCODE_6])
    {
      scale_ptr->x() += scale_delta;
      std::cout << scale_txt << " of box # " << which << " set to: " << *scale_ptr << "\n";
    }
  if (keyboard_state[SDL_SCANCODE_7])
    {
      scale_ptr->y() += scale_delta;
      std::cout << scale_txt << " of box # " << which << " set to: " << *scale_ptr << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_9])
    {
      m_boxes[which].m_rotate_angle += angle_delta;
      if (angle_delta > 2.0f * ASTRAL_PI)
        {
          m_boxes[which].m_rotate_angle -= 2.0f * ASTRAL_PI;
        }
      std::cout << "Angle of box # " << which << " set to: "
                << m_boxes[which].m_rotate_angle * (180.0f / ASTRAL_PI)
                << " degrees\n";
    }

  if (keyboard_state[SDL_SCANCODE_0])
    {
      m_boxes[which].m_rotate_angle -= angle_delta;
      if (angle_delta < 0.0f)
        {
          m_boxes[which].m_rotate_angle += 2.0f * ASTRAL_PI;
        }
      std::cout << "Angle of box # " << which << " set to: "
                << m_boxes[which].m_rotate_angle * (180.0f / ASTRAL_PI)
                << " degrees\n";
    }

  return return_value;
}

void
IntersectionTest::
draw_ui_rect(astral::RenderEncoderSurface dst,
             astral::vec2 position,
             astral::vec4 color)
{
  dst.save_transformation();
  dst.translate(position.x(), position.y());
  dst.draw_rect(astral::Rect()
                .min_point(-15.0f, -15.0f)
                .max_point(+15.0f, +15.0f),
                false,
                dst.create_value(astral::Brush().base_color(color)));
  dst.restore_transformation();
}

void
IntersectionTest::
draw_frame(void)
{
  astral::vecN<astral::Transformation, 2> tr;
  astral::RenderEncoderSurface render_encoder;
  std::ostringstream txt;
  float frame_ms;

  frame_ms = update_smooth_values();

  render_encoder = renderer().begin( render_target());

  for (unsigned int i = 0; i < 2; ++i)
    {
      tr[i] = m_boxes[i].m_zoom.transformation().astral_transformation();

      tr[i].translate(m_boxes[i].m_bb.as_rect().center_point());
      tr[i].scale(m_boxes[i].m_scale_pre_rotate);
      if (m_boxes[i].m_flip)
        {
          astral::Transformation f;

          f.m_matrix.row_col(0, 0) = 0.0f;
          f.m_matrix.row_col(0, 1) = 1.0f;
          f.m_matrix.row_col(1, 0) = 1.0f;
          f.m_matrix.row_col(1, 1) = 0.0f;
          tr[i] = tr[i] * f;
        }
      tr[i].rotate(m_boxes[i].m_rotate_angle);
      tr[i].scale(m_boxes[i].m_scale_post_rotate);
      tr[i].translate(-m_boxes[i].m_bb.as_rect().center_point());
    }

  astral::TransformedBoundingBox obb0(m_boxes[0].m_bb, tr[0]);
  astral::TransformedBoundingBox obb1(m_boxes[1].m_bb, tr[1]);

  astral::TransformedBoundingBox::Normalized nbb0(obb0);
  astral::TransformedBoundingBox::Normalized nbb1(obb1);

  const astral::TransformedBoundingBox *pbb0, *pbb1;

  if (m_test_normalized)
    {
      pbb0 = &nbb0;
      pbb1 = &nbb1;
    }
  else
    {
      pbb0 = &obb0;
      pbb1 = &obb1;
    }

  astral::vec4 color;

  if (pbb0->contains(*pbb1))
    {
      color = astral::vec4(1.0f, 1.0f, 0.0f, 0.5f);
      txt << "Contains\n";
    }
  else if (pbb0->intersects(*pbb1))
    {
      color = astral::vec4(1.0f, 0.0f, 0.0f, 0.5f);
      txt << "Intersect\n";
    }
  else
    {
      color = astral::vec4(0.0f, 1.0f, 0.0f, 0.5f);
      txt << "Disjoint\n";
    }

  txt << "OBB0 AxisAligned: " << std::boolalpha << pbb0->is_axis_aligned() << "\n"
      << "OBB1 AxisAligned: " << std::boolalpha << pbb1->is_axis_aligned() << "\n";

  for (unsigned int i = 0; i < 2; ++i)
    {
      render_encoder.transformation(tr[i]);
      render_encoder.draw_rect(m_boxes[i].m_bb.as_rect(), false, render_encoder.create_value(astral::Brush().base_color(color)));
    }

  render_encoder.transformation(astral::Transformation());
  if (m_boxes[0].m_draw_ui_rects)
    {
      for (const auto &p : obb0.pts())
        {
          draw_ui_rect(render_encoder, p, astral::vec4(1.0f, 0.0f, 1.0f, 1.0f));
        }
    }

  if (m_boxes[1].m_draw_ui_rects)
    {
      for (const auto &p : obb1.pts())
        {
          draw_ui_rect(render_encoder, p, astral::vec4(0.0f, 1.0f, 1.0f, 1.0f));
        }
    }

  if (m_test_normalized && nbb1.is_axis_aligned())
    {
      astral::BoundingBox<float> R;

      R = nbb0.compute_intersection(nbb1.containing_aabb());
      if (!R.empty())
        {
          astral::vec4 color(0.0f, 0.0f, 1.0f, 0.5f);

          render_encoder.transformation(astral::Transformation());
          render_encoder.draw_rect(R.as_rect(), false,
                                   render_encoder.create_value(astral::Brush().base_color(color)));
        }
    }

  render_encoder.transformation(astral::Transformation());

  if (!pixel_testing())
    {
      set_and_draw_hud(render_encoder, frame_ms, *m_text_item, txt.str());
    }

  renderer().end();
}

int
main(int argc, char **argv)
{
  IntersectionTest M;
  return M.main(argc, argv);
}

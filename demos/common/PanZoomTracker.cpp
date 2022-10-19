/*!
 * \file PanZoomTracker.cpp
 * \brief PanZoomTracker.cpp
 *
 * Adapted from: PanZoomTracker.cpp of FastUIDraw (https://github.com/intel/fastuidraw):
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */
#include <ciso646>
#include "PanZoomTracker.hpp"

/////////////////////////////////
// PanZoomTracker methods
void
PanZoomTracker::
handle_down(const astral::vec2 &pos)
{
  m_zoom_time.restart();
  m_button_down = true;
  m_zoom_pivot = pos;
  m_start_gesture = m_transformation;
}

void
PanZoomTracker::
handle_up(void)
{
  m_is_zooming = false;
  m_button_down = false;
}


void
PanZoomTracker::
handle_motion(const astral::vec2 &pos, const astral::vec2 &delta)
{
  if (!m_button_down)
    {
      return;
    }

  if (m_zoom_time.elapsed() > m_zoom_gesture_begin_time)
    {
      m_is_zooming = true;
    }

  float zdivide(m_scale_zooming * m_zoom_divider);

  if (!m_is_zooming)
    {
      float zdx(pos.x() - m_zoom_pivot.x());
      float zdy(pos.y() - m_zoom_pivot.y());

      m_transformation.m_translation += delta;

      if (astral::t_abs(zdx) > zdivide || astral::t_abs(zdy) > zdivide)
        {
          m_zoom_time.restart();
          m_zoom_pivot = pos;
          m_start_gesture = m_transformation;
        }
    }
  else
    {
      float zoom_factor(pos.y() - m_zoom_pivot.y());
      UniformScaleTranslate<float> R;

      if (m_zoom_direction == zoom_direction_negative_y)
        {
          zoom_factor = -zoom_factor;
        }

      zoom_factor /= zdivide;
      if (zoom_factor < 0.0f)
        {
          zoom_factor = -1.0f/astral::t_min(-1.0f, zoom_factor);
        }
      else
        {
          zoom_factor = astral::t_max(1.0f, zoom_factor);
        }
      R.m_scale = zoom_factor;
      R.m_translation = (1.0f - zoom_factor) * m_zoom_pivot;
      m_transformation = R * m_start_gesture;
    }
}


void
PanZoomTracker::
transformation(const UniformScaleTranslate<float> &v)
{
  m_transformation = v;
  if (m_button_down)
    {
      m_start_gesture = m_transformation;
    }
}

/////////////////////////////////
// PanZoomTrackerSDLEvent methods
void
PanZoomTrackerSDLEvent::
handle_event(const SDL_Event &ev, unsigned int button)
{
  switch(ev.type)
    {
    case SDL_MOUSEBUTTONDOWN:
      if (ev.button.button == button)
        {
          handle_down(m_scale_event * astral::vec2(ev.button.x, ev.button.y) + m_translate_event);
        }
      break;

    case SDL_MOUSEBUTTONUP:
      if (ev.button.button == button)
        {
          handle_up();
        }
      break;

    case SDL_MOUSEMOTION:
      handle_motion(m_scale_event * astral::vec2(ev.motion.x, ev.motion.y) + m_translate_event,
                    m_scale_event * astral::vec2(ev.motion.xrel, ev.motion.yrel));
      break;
    }
}

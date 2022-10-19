/*!
 * \file PanZoomTracker.hpp
 * \brief PanZoomTracker.hpp
 *
 * Adapted from: PanZoomTracker.hpp of FastUIDraw (https://github.com/intel/fastuidraw):
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
#ifndef ASTRAL_DEMO_PANZOOMTRACKER_HPP
#define ASTRAL_DEMO_PANZOOMTRACKER_HPP

#include <SDL.h>
#include "UniformScaleTranslate.hpp"
#include "simple_time.hpp"

/*!\class PanZoomTracker
  A PanZoomTracker implements the following gesture:
    - panning while dragging
    - hold button down (long) time, then up or left zoom out,
      down or right zoom in.
 */
class PanZoomTracker
{
public:
  enum zoom_direction_t
    {
      zoom_direction_negative_y,
      zoom_direction_positive_y
    };

  PanZoomTracker(int32_t zoom_gesture_begin_time_ms=500,
                 float zoom_divider=40.0f):
    m_scale_zooming(1.0f),
    m_zoom_direction(zoom_direction_positive_y),
    m_zoom_gesture_begin_time(zoom_gesture_begin_time_ms),
    m_zoom_divider(zoom_divider),
    m_is_zooming(false),
    m_button_down(false)
  {}


  const UniformScaleTranslate<float>&
  transformation(void) const
  {
    return m_transformation;
  }

  void
  transformation(const UniformScaleTranslate<float> &v);

  /*!\fn
    Tell PanZoomTracker of a motion event
    \param pos position of event
    \param delta amunt of motion of event of event
   */
  void
  handle_motion(const astral::vec2 &pos,
                const astral::vec2 &delta);

  /*!\fn
    Tell PanZoomTracker of down (i.e begin gesture) event
    \param pos position of event
   */
  void
  handle_down(const astral::vec2 &pos);

  /*!\fn
    Tell PanZoomTracker of button up (i.e. end gesture) event
   */
  void
  handle_up(void);

  /*!
    Scale zooming factor
   */
  float m_scale_zooming;

  enum zoom_direction_t m_zoom_direction;

private:

  int32_t m_zoom_gesture_begin_time;
  float m_zoom_divider;

  astral::vec2 m_zoom_pivot;
  simple_time m_zoom_time;
  bool m_is_zooming, m_button_down;

  UniformScaleTranslate<float> m_transformation, m_start_gesture;
};


class PanZoomTrackerSDLEvent:public PanZoomTracker
{
public:
  PanZoomTrackerSDLEvent(int32_t zoom_gesture_begin_time_ms=500,
                         float zoom_divider=40.0f):
    PanZoomTracker(zoom_gesture_begin_time_ms,
                   zoom_divider),
    m_scale_event(1.0f, 1.0f),
    m_translate_event(0.0f, 0.0f)
  {}

  /*!\fn
    Maps to handle_drag, handle_button_down, handle_button_up
    from events on mouse button 0
   */
  void
  handle_event(const SDL_Event &ev, unsigned int button = SDL_BUTTON_LEFT);

  /*!\fn
    Amount by which to scale events
   */
  astral::vec2 m_scale_event, m_translate_event;
};

#endif

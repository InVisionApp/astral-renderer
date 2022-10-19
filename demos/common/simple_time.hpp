/*!
 * \file simple_time.hpp
 * \brief file simple_time.hpp
 *
 * Adapted from: WRATHTime.hpp of WRATH:
 *
 * Copyright 2013 by Nomovok Ltd.
 * Contact: info@nomovok.com
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */



#ifndef ASTRAL_DEMO_SIMPLE_TIME_HPP
#define ASTRAL_DEMO_SIMPLE_TIME_HPP

#include <chrono>
#include <stdint.h>

class simple_time
{
public:
  simple_time(void):
    m_time_fake(0),
    m_paused(false)
  {
    m_start_time = std::chrono::steady_clock::now();
  }

  int32_t
  elapsed(void) const
  {
    return elapsed_us() / 1000;
  }

  int32_t
  restart(void)
  {
    return restart_us() / 1000;
  }

  int64_t
  elapsed_us(void) const
  {
    int64_t R;

    R = (m_paused) ?
      time_difference_us(m_pause_time, m_start_time):
      time_difference_us(std::chrono::steady_clock::now(), m_start_time);

    R += m_time_fake;
    return R;
  }

  int64_t
  restart_us(void)
  {
    int64_t return_value;

    return_value = elapsed_us();
    m_start_time = std::chrono::steady_clock::now();
    m_paused = false;

    return return_value;
  }

  void
  pause(void)
  {
    if (!m_paused)
      {
        m_paused = true;
        m_pause_time = std::chrono::steady_clock::now();
      }
  }

  void
  resume(void)
  {
    if (m_paused)
      {
        auto time_now(std::chrono::steady_clock::now());

        m_paused = false;
        m_start_time += (time_now - m_pause_time);
      }
  }

  void
  pause(bool p)
  {
    if (p)
      {
        pause();
      }
    else
      {
        resume();
      }
  }

  //value in us
  void
  decrement_time(int64_t D)
  {
    m_time_fake -= D;
  }

  //value in us
  void
  increment_time(int64_t D)
  {
    m_time_fake += D;
  }

  void
  set_time(int64_t D)
  {
    m_paused = true;
    m_start_time = m_pause_time = std::chrono::steady_clock::now();
    m_time_fake = D;
  }

  bool
  paused(void) const
  {
    return m_paused;
  }

private:
  static
  int64_t
  time_difference_us(const std::chrono::time_point<std::chrono::steady_clock> &end,
                     const std::chrono::time_point<std::chrono::steady_clock> &begin)
  {
    return std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
  }

  std::chrono::time_point<std::chrono::steady_clock> m_start_time, m_pause_time;
  int64_t m_time_fake;
  bool m_paused;
};

class average_timer
{
public:
  average_timer(uint32_t interval_ms):
    m_interval_ms(interval_ms),
    m_counter(0),
    m_average_elapsed(0.0f),
    m_parity(false)
  {}

  uint32_t
  interval_ms(void) const
  {
    return m_interval_ms;
  }

  void
  interval_ms(uint32_t v)
  {
    m_interval_ms = v;
    m_counter = 0;
    m_average_elapsed = 0.0f;
  }

  void
  increment_counter(void)
  {
    ++m_counter;

    uint32_t e(m_time.elapsed());
    if (e >= m_interval_ms)
      {
        m_average_elapsed = static_cast<float>(e) / static_cast<float>(m_counter);
        m_parity = !m_parity;

        m_counter = 0;
        m_time.restart();
      }
  }

  bool
  parity(void) const
  {
    return m_parity;
  }

  const char*
  parity_string(void) const
  {
    return m_parity ? "*" : "";
  }

  float
  average_elapsed_ms(void) const
  {
    return m_average_elapsed;
  }

private:
  simple_time m_time;
  uint32_t m_interval_ms, m_counter;
  float m_average_elapsed;
  bool m_parity;
};

#endif

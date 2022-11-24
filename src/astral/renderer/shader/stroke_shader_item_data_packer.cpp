/*!
 * \file stroke_shader_item_data_packer.cpp
 * \brief file stroke_shader_item_data_packer.cpp
 *
 * Copyright 2021 by InvisionApp.
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

#include <astral/renderer/shader/stroke_shader_item_data_packer.hpp>


namespace
{
  class StrokeShaderItemDataPackerValueMapping
  {
  public:
    StrokeShaderItemDataPackerValueMapping(void)
    {
      m_value
        .add(astral::ItemDataValueMapping::render_value_transformation,
             astral::ItemDataValueMapping::z_channel, 0);
    }

    astral::ItemDataValueMapping m_value;
  };
}

/////////////////////////////////////////
// astral::StrokeShaderItemDataPacker::ItemDataPacker methods
unsigned int
astral::StrokeShaderItemDataPacker::ItemDataPacker::
item_data_size(const StrokeParameters&) const
{
  return item_data_count;
}

bool
astral::StrokeShaderItemDataPacker::ItemDataPacker::
caps_joins_collapse(const float2x2 &pixel_transformation_logical,
                    float render_scale_factor,
                    const StrokeParameters &stroke_params) const
{
  ASTRALunused(pixel_transformation_logical);
  ASTRALunused(render_scale_factor);

  return stroke_params.m_width <= 0.0f;
}

void
astral::StrokeShaderItemDataPacker::ItemDataPacker::
pack_item_data(RenderValue<Transformation> logical_transformation_path,
               const StrokeParameters &stroke_params,
               float t, c_array<gvec4> dst) const
{
  float m, s, r;

  m = stroke_params.m_miter_limit;
  s = (stroke_params.m_miter_clip) ? 1.0f : -1.0f;
  r = (stroke_params.m_graceful_thin_stroking) ? -1.0f : 1.0f;

  ASTRALassert(dst.size() >= item_data_count);
  dst[base_data_offset].x().f = r * t_max(stroke_params.m_width * 0.5f, 0.0f);
  dst[base_data_offset].y().f = t;
  dst[base_data_offset].z().u = logical_transformation_path.cookie();
  dst[base_data_offset].w().f = s * t_sqrt(t_max(0.0f, m * m - 1.0f));
}

const astral::ItemDataValueMapping&
astral::StrokeShaderItemDataPacker::ItemDataPacker::
intrepreted_value_map(void) const
{
  static StrokeShaderItemDataPackerValueMapping R;
  return R.m_value;
}

////////////////////////////////////////////
// astral::StrokeShaderItemDataPacker::DashPattern methods
astral::StrokeShaderItemDataPacker::DashPattern::
DashPattern(void):
  m_total_length(0.0f),
  m_dash_offset(0.0f),
  m_dash_corner(-1.0f),
  m_adjust(adjust_none),
  m_draw_lengths_adjusted(false),
  m_skip_lengths_adjusted(false),
  m_dash_pattern_per_edge(false),
  m_scale_factor(1.0f)
{}

astral::StrokeShaderItemDataPacker::DashPattern&
astral::StrokeShaderItemDataPacker::DashPattern::
clear(void)
{
  m_elements.clear();
  m_total_length = 0.0f;
  mark_dirty();
  return *this;
}

astral::StrokeShaderItemDataPacker::DashPattern&
astral::StrokeShaderItemDataPacker::DashPattern::
dash_start_offset(float f)
{
  if (f != m_dash_offset)
    {
      m_dash_offset = f;
      mark_dirty();
    }
  return *this;
}

astral::StrokeShaderItemDataPacker::DashPattern&
astral::StrokeShaderItemDataPacker::DashPattern::
add_draw(float e)
{
  /* a zero-length draw means the user wants a cap ball */
  if (e >= 0.0f)
    {
      add_implement(e);
    }
  return *this;
}

astral::StrokeShaderItemDataPacker::DashPattern&
astral::StrokeShaderItemDataPacker::DashPattern::
add_skip(float e)
{
  /* a zero-length skip is meaningless */
  if (e > 0.0f)
    {
      add_implement(-e);
    }
  return *this;
}

void
astral::StrokeShaderItemDataPacker::DashPattern::
add_implement(float length)
{
  m_total_length += t_abs(length);
  if (m_elements.empty() || (length >= 0.0f) != (m_elements.back() >= 0.0f))
    {
      m_elements.push_back(length);
    }
  else
    {
      m_elements.back() += length;
    }
  mark_dirty();
}

uint32_t
astral::StrokeShaderItemDataPacker::DashPattern::
flags(void) const
{
  uint32_t return_value(0u);

  ready_computed_intervals();
  ASTRALassert(!m_computed_intervals.empty());

  if (m_adjust != adjust_none
      && (m_draw_lengths_adjusted || m_skip_lengths_adjusted))
    {
      return_value |= (1u << (m_adjust - 1u));
      if (m_draw_lengths_adjusted)
        {
          uint32_t f;

          f = (m_computed_intervals.front() >= 0.0f) ? adjust_xz_lengths : adjust_yw_lengths;
          return_value |= f;
        }

      if (m_skip_lengths_adjusted)
        {
          uint32_t f;

          f = (m_computed_intervals.front() < 0.0f) ? adjust_xz_lengths : adjust_yw_lengths;
          return_value |= f;
        }
    }

  if (m_dash_pattern_per_edge)
    {
      return_value |= stroke_starts_at_edge;
    }

  return return_value;
}

void
astral::StrokeShaderItemDataPacker::DashPattern::
ready_computed_intervals(void) const
{
  if (!m_computed_intervals.empty())
    {
      return;
    }

  if (m_elements.empty())
    {
      m_computed_intervals.push_back(1.0f);
      m_first_interval = 1.0f;
      m_last_interval = 1.0f;
      m_totals[0] = 1.0f;
      m_totals[1] = 0.0f;
      return;
    }

  /* instead of having the shader handle the dash-offset,
   * the packing will instead pack values reflecting the
   * dash offset; we do this by translating the dash pattern
   * to "start" at the dash offset value. First compute the
   * dash offset bounded to [0, m_total_length):
   */
  unsigned int location;
  float effective_offset, begin_interval, end_interval;

  m_totals.x() = 0.0f;
  m_totals.y() = 0.0f;
  if (m_dash_offset < 0.0f)
    {
      float r, a;

      a = t_abs(m_dash_offset);
      r = t_fmod(a, m_total_length);
      effective_offset = m_total_length - r;
    }
  else
    {
      effective_offset = t_fmod(m_dash_offset, m_total_length);
    }

  /* now find the interval that contains the dash offset */
  for (begin_interval = end_interval = 0.0f, location = 0; location < m_elements.size(); ++location)
    {
      end_interval += t_abs(m_elements[location]);
      if (effective_offset <= end_interval)
        {
          break;
        }
      begin_interval = end_interval;
    }

  /* the dash start is within the location'th interval, we now encode
   * the intervals -starting- at that offset; that means the first
   * interval is pre-truncated.
   */
  for (unsigned int i = 0, endi = m_elements.size(); i < endi; ++i)
    {
      unsigned int src_i;

      src_i = (i + location) % m_elements.size();
      if (m_computed_intervals.empty() || ((m_computed_intervals.back() >= 0.0f) != (m_elements[src_i] >= 0.0f)))
        {
          m_computed_intervals.push_back(m_elements[src_i]);
        }
      else
        {
          m_computed_intervals.back() += m_elements[src_i];
        }

      ASTRALassert(!m_computed_intervals.empty());
      m_totals[(m_computed_intervals.size() - 1u) & 1] += t_abs(m_elements[src_i]);
    }

  /* the effective dash offset is then effective_offset is the distance
   * from begin_interval
   */
  effective_offset -= begin_interval;

  /* now for something amusing: force the dash offset to be
   * zero; we do this by removing from the first interval the
   * value of dash offset and then adding to the last interval
   * the value of dash offset; the main trick we need to handle
   * is that if the first and last interval have the same sign
   * to then just increment the last interval; if they have
   * different sign, then to add the interval
   */
  ASTRALassert(!m_computed_intervals.empty());
  ASTRALassert(effective_offset >= 0.0f);
  if (effective_offset > 0.0f)
    {
      if ((m_computed_intervals.front() >= 0.0f) != (m_computed_intervals.back() >= 0.0f))
        {
          m_computed_intervals.push_back(t_sign_prefer_positive(m_computed_intervals.front()) * effective_offset);
        }
      else
        {
          m_computed_intervals.back() += t_sign_prefer_positive(m_computed_intervals.back()) * effective_offset;
        }
      m_computed_intervals.front() -= t_sign_prefer_positive(m_computed_intervals.front()) * effective_offset;
    }

  m_first_interval = m_computed_intervals.front();
  m_last_interval = m_computed_intervals.back();

  /* make the last interval longer by the first interval if they
   * are the same type of intervals; this prevents getting cracks when
   * drawing flat caps when passing over dash pattern repeat boundary.
   * NOTE: we do this after recoding the length of the last interval
   * because we want that value to be the actual length of the last
   * interval.
   */
  if ((m_computed_intervals.front() >= 0.0f) == (m_computed_intervals.back() >= 0.0f))
    {
      m_computed_intervals.back() += m_computed_intervals.front();
    }
}

void
astral::StrokeShaderItemDataPacker::DashPattern::
ready_computed_intervals_filter_zero(void) const
{
  ASTRALassert(!m_computed_intervals.empty());
  if (!m_computed_intervals_filter_zero.empty())
    {
      return;
    }

  for (float f : m_computed_intervals)
    {
      if (t_abs(f) > 0.0f)
        {
          if (!m_computed_intervals_filter_zero.empty() && (m_computed_intervals_filter_zero.back() >= 0.0f) == (f >= 0.0f))
            {
              m_computed_intervals_filter_zero.back() += f;
            }
          else
            {
              m_computed_intervals_filter_zero.push_back(f);
            }
        }
    }

  if (m_computed_intervals_filter_zero.empty())
    {
      /* I guess something? */
      m_computed_intervals_filter_zero.push_back(0.0f);
    }
}

unsigned int
astral::StrokeShaderItemDataPacker::DashPattern::
item_data_size(const StrokeParameters &stroke_params) const
{
  unsigned int sz;

  ready_computed_intervals();
  if (stroke_params.m_cap != cap_flat)
    {
      sz = m_computed_intervals.size();
    }
  else
    {
      ready_computed_intervals_filter_zero();
      sz = m_computed_intervals_filter_zero.size();
    }

  return ItemDataPacker::item_data_size(stroke_params) + 2u + ASTRAL_NUMBER_BLOCK4_NEEDED(sz);
}

void
astral::StrokeShaderItemDataPacker::DashPattern::
pack_item_data(RenderValue<Transformation> logical_transformation_path,
               const StrokeParameters &stroke_params,
               float t, c_array<gvec4> dst) const
{
  c_array<float> dst_intervals;
  unsigned int base_size(ItemDataPacker::item_data_size(stroke_params));
  c_array<const float> src_intervals;

  ASTRALassert(dst.size() == item_data_size(stroke_params));

  ItemDataPacker::pack_item_data(logical_transformation_path, stroke_params,
                                 t, dst.sub_array(0, base_size));

  ready_computed_intervals();
  if (stroke_params.m_cap != cap_flat)
    {
      src_intervals = make_c_array(m_computed_intervals);
    }
  else
    {
      ready_computed_intervals_filter_zero();
      src_intervals = make_c_array(m_computed_intervals_filter_zero);
    }

  dst = dst.sub_array(base_size);
  dst[0].x().f = t_abs(m_totals[0]) * m_scale_factor;
  dst[0].y().f = t_abs(m_totals[1]) * m_scale_factor;
  dst[0].z().f = dash_corner_radius();
  dst[0].w().u = flags();

  dst[1].x().u = 0u;
  dst[1].y().f = m_last_interval * m_scale_factor;
  dst[1].z().f = m_first_interval * m_scale_factor;
  dst[1].w().u = src_intervals.size();

  dst_intervals = dst.sub_array(2).reinterpret_pointer<float>();
  ASTRALassert(dst_intervals.size() >= src_intervals.size());
  std::copy(src_intervals.begin(), src_intervals.end(), dst_intervals.begin());
  std::fill(dst_intervals.begin() + dst_intervals.size(), dst_intervals.end(), 0.0f);

  if (m_scale_factor != 1.0f)
    {
      for (float &v : dst_intervals)
        {
          v *= m_scale_factor;
        }
    }
}

astral::c_string
astral::
label(enum StrokeShaderItemDataPacker::DashPattern::adjust_t v)
{
  static const c_string labels[StrokeShaderItemDataPacker::DashPattern::number_adjust] =
    {
      [StrokeShaderItemDataPacker::DashPattern::adjust_none] = "adjust_none",
      [StrokeShaderItemDataPacker::DashPattern::adjust_compress] = "adjust_compress",
      [StrokeShaderItemDataPacker::DashPattern::adjust_stretch] = "adjust_stretch"
    };

  ASTRALassert(v < StrokeShaderItemDataPacker::DashPattern::number_adjust);
  return labels[v];
}

/*!
 * \file colorstop_sequence.cpp
 * \brief file colorstop_sequence.cpp
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

#include <astral/util/ostream_utility.hpp>
#include <astral/util/memory_pool.hpp>
#include <astral/renderer/colorstop_sequence.hpp>

class astral::ColorStopSequenceAtlas::MemoryPool
{
public:
  template<typename ...Args>
  ColorStopSequence*
  create(Args&&... args)
  {
    void *data;
    ColorStopSequence *p;

    data = m_pool.allocate();
    p = new(data) ColorStopSequence(std::forward<Args>(args)...);

    return p;
  }

  void
  reclaim(ColorStopSequence *p)
  {
    ASTRALassert(!p->m_atlas);
    m_pool.reclaim(p);
  }

  template<enum colorspace_t colorspace>
  c_array<const ColorStop<vec4>>
  normalized_colorstops(c_array<const ColorStop<FixedPointColor<colorspace>>> colorstops)
  {
    m_colors_converted.resize(colorstops.size());
    for (unsigned int i = 0; i < colorstops.size(); ++i)
      {
        m_colors_converted[i].m_t = colorstops[i].m_t;
        m_colors_converted[i].m_color = colorstops[i].m_color.normalized_value();
      }
    return make_c_array(m_colors_converted);
  }

  /* workroom */
  std::vector<ColorStop<vec4>> m_colorstops_tmp;
  std::vector<u8vec4> m_colors;

private:
  astral::MemoryPool<ColorStopSequence, 512> m_pool;

  /* private workroom for conversion */
  std::vector<ColorStop<vec4>> m_colors_converted;
};

//////////////////////////
// global methods
namespace std
{
  ostream&
  operator<<(ostream &str, const astral::ColorStop<astral::vec4> &C)
  {
    str << "(" << C.m_t
        << ", " << C.m_color
        << ")";
    return str;
  }

  std::ostream&
  operator<<(ostream &str, const astral::ColorStop<astral::FixedPointColorLinear> &C)
  {
    str << "(" << C.m_t
        << ", " << astral::uvec4(C.m_color.m_value)
        << ")";
    return str;
  }

  std::ostream&
  operator<<(ostream &str, const astral::ColorStop<astral::FixedPointColor_sRGB> &C)
  {
    str << "(" << C.m_t
        << ", " << astral::uvec4(C.m_color.m_value)
        << ")";
    return str;
  }
}

//////////////////////////////
// astral::ColorAtlas methods
astral::ColorStopSequenceAtlas::
ColorStopSequenceAtlas(const reference_counted_ptr<ColorStopSequenceAtlasBacking> &backing):
  m_backing(backing),
  m_interval_allocator(backing->layer_dimensions(), backing->number_layers()),
  m_lock_resources(0)
{
  m_pool = ASTRALnew MemoryPool();
}

astral::ColorStopSequenceAtlas::
~ColorStopSequenceAtlas()
{
  ASTRALdelete(m_pool);
}

astral::reference_counted_ptr<astral::ColorStopSequence>
astral::ColorStopSequenceAtlas::
create(c_array<const ColorStop<FixedPointColorLinear>> colorstops, unsigned int num_texels)
{
  return create(m_pool->normalized_colorstops(colorstops), FixedPointColorLinear::colorspace(), num_texels);
}

astral::reference_counted_ptr<astral::ColorStopSequence>
astral::ColorStopSequenceAtlas::
create(c_array<const ColorStop<FixedPointColor_sRGB>> colorstops, unsigned int num_texels)
{
  return create(m_pool->normalized_colorstops(colorstops), FixedPointColor_sRGB::colorspace(), num_texels);
}

astral::reference_counted_ptr<astral::ColorStopSequence>
astral::ColorStopSequenceAtlas::
create(c_array<const ColorStop<vec4>> colorstops, enum colorspace_t colorspace, unsigned int num_texels)
{
  bool opaque(true);
  c_array<ColorStop<vec4>> sorted;
  c_array<u8vec4> colors;

  ASTRALassert(!colorstops.empty());
  m_pool->m_colorstops_tmp.resize(colorstops.size());
  sorted = make_c_array(m_pool->m_colorstops_tmp);

  std::copy(colorstops.begin(), colorstops.end(), sorted.begin());

  /* First sort and clamp the time of all values */
  std::stable_sort(sorted.begin(), sorted.end());
  for (ColorStop<vec4> &v : sorted)
    {
      v.m_t = t_max(0.0f, t_min(1.0f, v.m_t));
      opaque = opaque && v.m_color.w() >= 1.0f;
    }

  if (colorstops.size() >= 2)
    {
      /* num_texels finer than the layer dimensions or our
       * arbitary 1024 value is not allowed
       */
      const unsigned int max_size = t_min(1024u, m_backing->layer_dimensions());
      const float tiny_t = 1.0f / static_cast<float>(max_size);
      unsigned int src, dst;
      float t, dt;

      if (num_texels == 0u)
        {
          /* TODO: We need to compute the number of texels needed so that
           *       each color stop S[i], that the linear interpolation of
           *       the discretization approximxates it well with a special
           *       rule handling hard colorstops. For now, we punt and just
           *       use the max beteween 512 color and the reciprocal of the
           *       two closest color stops.
           */
          float smallest_delta_t = 1.0f;
          for (unsigned int i = 1; i < sorted.size(); ++i)
            {
              dt = sorted[i].m_t - sorted[i - 1].m_t;
              smallest_delta_t = t_min(dt, t_max(tiny_t, smallest_delta_t));
            }

          num_texels = static_cast<unsigned int>(1.0f / smallest_delta_t) + 1u;
          num_texels = t_max(256u, num_texels);
        }

      num_texels = t_max(num_texels, 2u);
      num_texels = t_min(max_size, num_texels);
      dt = 1.0f / static_cast<float>(num_texels - 1u);
      t = 0.0f;

      m_pool->m_colors.resize(num_texels);
      colors = make_c_array(m_pool->m_colors);

      u8vec4 first_color(uint8_from_normalized(sorted.front().m_color));
      for (dst = 0; t <= sorted[0].m_t && dst < num_texels; ++dst, t += dt)
        {
          colors[dst] = first_color;
        }

      for (src = 1u; src < colorstops.size(); ++ src)
        {
          ColorStop<vec4> before(sorted[src - 1u]), after(sorted[src]);
          vec4 before_color(before.m_color);
          vec4 after_color(after.m_color);
          float ds;

          ds = after.m_t - before.m_t;
          if (ds >= tiny_t)
            {
              ds = 1.0f / ds;
              for (;t < after.m_t && dst < num_texels; t += dt, ++dst)
                {
                  vec4 c;
                  float s;

                  s = (t - before.m_t) * ds;
                  c = (1.0f - s) * before_color + s * after_color;

                  colors[dst] = uint8_from_normalized(c);
                }
            }
        }

      ASTRALassert(dst >= 1u);
      u8vec4 last_color(uint8_from_normalized(sorted.back().m_color));
      for (; dst < num_texels; ++dst)
        {
          colors[dst] = last_color;
        }
      colors.back() = last_color;
    }
  else
    {
      m_pool->m_colors.resize(2);
      colors = make_c_array(m_pool->m_colors);

      colors[0] = colors[1] = uint8_from_normalized(sorted.front().m_color);
    }

  reference_counted_ptr<ColorStopSequence> return_value;

  return_value = m_pool->create();
  return_value->m_atlas = this;
  return_value->m_interval = allocate_region(colors);
  return_value->m_opaque = opaque;
  return_value->m_colorspace = colorspace;

  return return_value;
}

void
astral::ColorStopSequenceAtlas::
lock_resources(void)
{
  ++m_lock_resources;
}

void
astral::ColorStopSequenceAtlas::
unlock_resources(void)
{
  --m_lock_resources;
  ASTRALassert(m_lock_resources >= 0);

  if (m_lock_resources == 0)
    {
      for (const IntervalAllocator::Interval *d : m_delayed_frees)
        {
          deallocate_region(d);
        }
      m_delayed_frees.clear();
    }
}

void
astral::ColorStopSequenceAtlas::
deallocate_region(const IntervalAllocator::Interval *d)
{
  if (m_lock_resources > 0)
    {
      m_delayed_frees.push_back(d);
    }
  else
    {
      m_interval_allocator.release(d);
    }
}

const astral::IntervalAllocator::Interval*
astral::ColorStopSequenceAtlas::
allocate_region(c_array<const u8vec4> colors)
{
  const astral::IntervalAllocator::Interval *return_value;

  ASTRALassert(colors.size() <= m_backing->layer_dimensions());
  return_value = m_interval_allocator.allocate(colors.size());
  if (!return_value)
    {
      int oldL, newL;

      oldL = m_backing->number_layers();
      newL = m_backing->resize(t_max(oldL + 1, 2 * oldL));
      m_interval_allocator.number_layers(newL);
      return_value = m_interval_allocator.allocate(colors.size());
    }

  ASTRALassert(return_value);
  m_backing->load_pixels(return_value->layer(), return_value->range().m_begin, colors);

  return return_value;

}

/////////////////////////////
// astral::ColorStopSequence methods
astral::ColorStopSequence::
~ColorStopSequence()
{
  ASTRALassert(!m_atlas);
}

void
astral::ColorStopSequence::
delete_object(ColorStopSequence *p)
{
  reference_counted_ptr<ColorStopSequenceAtlas> atlas;

  atlas.swap(p->m_atlas);
  atlas->deallocate_region(p->m_interval);
  atlas->m_pool->reclaim(p);
}

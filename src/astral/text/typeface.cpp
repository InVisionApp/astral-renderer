/*!
 * \file glyph_cache.cpp
 * \brief file glyph_cache.cpp
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

#include <thread>
#include <iostream>

#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
#include <atomic>
#endif

#include <astral/path.hpp>
#include <astral/util/util.hpp>
#include <astral/text/font.hpp>
#include <astral/text/typeface.hpp>
#include <astral/renderer/combined_path.hpp>
#include <astral/renderer/render_engine.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/threading.h>
#endif

/* TODO: if too many glyphs are realized on GPU is getting too big,
 *       walk the  (not yet made) list of ejectable glyphs and
 *       eject some glyphs.
 */

class astral::Typeface::Implement:public astral::Typeface
{
public:
  explicit
  Implement(const reference_counted_ptr<const GlyphGenerator> &generator,
            const ItemPath::GenerationParams &params);

  ~Implement();

  Glyph::Private&
  fetch_glyph(unsigned int thread_slot, GlyphIndex glyph_code);

  void
  instance_glyph(unsigned int thread_slot, GlyphIndex glyph_code)
  {
    const Glyph::Private &G(fetch_glyph(thread_slot, glyph_code));
    ASTRALunused(G);
  }

  void
  add_to_list(std::vector<GlyphIndex> *dst, GlyphIndex glyph_code);

  void
  remove_from_list(GlyphIndex glyph_code);

  bool
  glyph_inited(GlyphIndex glyph_code);

  unsigned int
  number_threads(void) const
  {
    return m_number_threads;
  }

  unsigned int
  number_glyphs(void) const
  {
    return m_glyphs.size();
  }

  bool
  is_scalable(void) const
  {
    return m_scalable;
  }

  const TypefaceMetricsScalable&
  scalable_metrics(void) const
  {
    ASTRALassert(is_scalable());
    return *m_generator->scalable_metrics();
  }

  c_array<const TypefaceMetricsFixedSize>
  fixed_metrics(void) const
  {
    ASTRALassert(!is_scalable());
    return m_generator->fixed_metrics();
  }

  unsigned int
  smallest_strike(void) const
  {
    ASTRALassert(!is_scalable());
    return m_smallest_strike;
  }

  GlyphIndex
  glyph_index(uint32_t character_code) const
  {
    auto iter = m_mapping.m_values.find(character_code);
    return (iter != m_mapping.m_values.end()) ?
      iter->second :
      GlyphIndex();
  }

  const GlyphGenerator&
  generator(void) const
  {
    return *m_generator;
  }

  void
  parallel_fetch_glyphs(unsigned int number_threads,
                        c_array<const GlyphIndex> glyph_indices,
                        c_array<Glyph> out_glyphs);

  const ItemPath::GenerationParams&
  implement_item_path_params(void) const
  {
    return m_item_path_params;
  }

  static
  ItemPath::GenerationParams&
  default_item_path_params(void)
  {
    static ItemPath::GenerationParams R;
    return R;
  }

private:
  void
  parallel_fetch_glyphs_execute(unsigned int thread_slot);

  reference_counted_ptr<const GlyphGenerator> m_generator;
  CharacterMapping m_mapping;

  bool m_scalable;
  unsigned int m_number_threads;
  std::vector<Glyph::Private> m_glyphs;
  ItemPath::GenerationParams m_item_path_params;

  /* index into fixed_metrics() which strike has the smallest
   * pixel size; for scalable glyphs this value is meaningless
   */
  unsigned int m_smallest_strike;

  /* data for parallel glyph fetching */
  std::vector<GlyphIndex> m_parallel_fetch_glyph_list;
  std::vector<std::thread> m_parallel_fetch_threads;
  std::vector<int> m_parallel_fetch_stats;
#ifdef __EMSCRIPTEN__
  std::vector<range_type<unsigned int>> m_parallel_fetch_ranges;
#else
  std::atomic<int> m_parallel_fetch_current;
#endif
};

class astral::Glyph::Private
{
public:
  Private(void):
    m_init_called(false),
    m_lock_counter(0),
    m_on_list(false),
    m_typeface(nullptr)
  {}

  ~Private()
  {
    ASTRALassert(m_lock_counter == 0u);
  }

  bool
  inited(void)
  {
    return m_init_called;
  }

  void
  init_scalable(Typeface::Implement *typeface,
                const GlyphGenerator &generator,
                unsigned int thread_slot,
                GlyphIndex idx);

  void
  init_fixed_size(Typeface::Implement *typeface,
                  const GlyphGenerator &generator,
                  unsigned int thread_slot,
                  GlyphIndex idx);

  const GlyphMetrics&
  scalable_metrics(void) const
  {
    ASTRALassert(is_scalable());
    return m_metrics[0];
  }

  const GlyphMetrics&
  fixed_metrics(unsigned int strike_index) const
  {
    ASTRALassert(!is_scalable());
    ASTRALassert(strike_index < m_strikes.size());
    return m_metrics[strike_index];
  }

  GlyphIndex
  glyph_index(void) const
  {
    return m_glyph_index;
  }

  const ScaleTranslate&
  scale_translate(void) const
  {
    return m_tr;
  }

  Typeface&
  typeface(void) const
  {
    return *m_typeface;
  }

  bool
  is_colored(void) const
  {
    return m_is_colored;
  }

  bool
  is_scalable(void) const
  {
    return m_typeface->is_scalable();
  }

  const GlyphColors*
  colors(void) const
  {
    return (is_colored() && is_scalable()) ?
      &m_colors:
      nullptr;
  }

  const Path*
  path(unsigned int layer, enum fill_rule_t *fill_rule,
       reference_counted_ptr<const ItemPath> *out_item_path)
  {
    layer = (is_colored()) ? layer : 0;

    ASTRALassert(layer < m_paths.size());
    ASTRALassert(m_fill_rules.size() == m_paths.size());
    ASTRALassert(m_item_paths.size() == m_paths.size());

    if (out_item_path)
      {
        *out_item_path = m_item_paths[layer];
      }

    *fill_rule = m_fill_rules[layer];
    return &m_paths[layer];
  }

  reference_counted_ptr<const StaticData>
  render_data(RenderEngine &engine, GlyphPaletteID palette);

  reference_counted_ptr<const StaticData>
  image_render_data(RenderEngine &engine, unsigned int strike_index,
                    reference_counted_ptr<const Image> *out_image);

  void
  eject(void);

  void
  increment_lock(void)
  {
    /* TODO: if m_lock_counter increments from zero
     *       to non-zero, remove it from the (not yet
     *       made) list of ejectable glyphs.
     */
    ++m_lock_counter;
  }

  void
  decrement_lock(void)
  {
    /* TODO: if m_lock_counter decrements from non-zero
     *       to zero, add it to the (not yet made) list
     *       of ejectable glyphs.
     */
    ASTRALassert(m_lock_counter > 0);
    --m_lock_counter;
  }

  void
  add_to_list(std::vector<GlyphIndex> *dst, GlyphIndex idx)
  {
    if (!m_on_list)
      {
        m_on_list = true;
        dst->push_back(GlyphIndex(idx));
      }
  }

  void
  remove_from_list(void)
  {
    m_on_list = false;
  }

private:
  class PerStrike
  {
  public:
    reference_counted_ptr<const StaticData> m_static_data;
    reference_counted_ptr<const Image> m_image;
    std::vector<FixedPointColor_sRGB> m_image_data;
    ivec2 m_image_size;
  };

  void
  generate_image(RenderEngine &engine, unsigned int strike_index);

  bool m_init_called;
  unsigned int m_lock_counter;

  /* when doing parallel glyph loading, this marks the
   * glyph as already on the list to prevent it being
   * on a list twice.
   */
  bool m_on_list;

  GlyphIndex m_glyph_index;
  std::vector<GlyphMetrics> m_metrics;

  /* note that it is a weak/raw pointer because the
   * Typeface::Implement holds the array that holds
   * the glyph.
   */
  Typeface::Implement *m_typeface;

  bool m_is_colored;

  /* data for scalable glyph rendering */
  GlyphColors m_colors;
  std::vector<Path> m_paths;
  std::vector<enum fill_rule_t> m_fill_rules;
  std::vector<reference_counted_ptr<ItemPath>> m_item_paths;
  std::vector<reference_counted_ptr<const StaticData>> m_render_data;

  /* data for image rendering */
  std::vector<PerStrike> m_strikes;

  /* transformation that maps [0, 1]x[0, 1]
   * to the coordinate system of the input paths
   */
  ScaleTranslate m_tr;
};

///////////////////////////////////////////
// astral::Typeface::Implement methods
astral::Typeface::Implement::
Implement(const reference_counted_ptr<const GlyphGenerator> &generator,
          const ItemPath::GenerationParams &item_path_params):
  m_generator(generator),
  m_scalable(m_generator->scalable_metrics() != nullptr),
  m_number_threads(m_generator->number_threads()),
  m_glyphs(m_generator->number_glyphs()),
  m_item_path_params(item_path_params),
  m_smallest_strike(0)
{
  unsigned int thread_slot(0);
  c_array<const TypefaceMetricsFixedSize> fm(m_generator->fixed_metrics());

  m_generator->fill_character_mapping(thread_slot, m_mapping);
  if (!fm.empty())
    {
      float smallest_size;

      ASTRALassert(!m_scalable);

      smallest_size = fm[0].m_pixel_size;
      for (unsigned int i = 1; i < fm.size(); ++i)
        {
          if (smallest_size > fm[i].m_pixel_size)
            {
              m_smallest_strike = i;
              smallest_size = fm[i].m_pixel_size;
            }
        }
    }
}

astral::Typeface::Implement::
~Implement()
{
}

void
astral::Typeface::Implement::
add_to_list(std::vector<GlyphIndex> *dst, GlyphIndex glyph_code)
{
  ASTRALassert(glyph_code.m_value < m_glyphs.size());

  Glyph::Private &G(m_glyphs[glyph_code.m_value]);
  if (!G.inited())
    {
      G.add_to_list(dst, glyph_code);
    }
}

void
astral::Typeface::Implement::
remove_from_list(GlyphIndex glyph_code)
{
  ASTRALassert(glyph_code.m_value < m_glyphs.size());

  Glyph::Private &G(m_glyphs[glyph_code.m_value]);
  G.remove_from_list();
}

astral::Glyph::Private&
astral::Typeface::Implement::
fetch_glyph(unsigned int thread_slot, GlyphIndex glyph_code)
{
  ASTRALassert(glyph_code.m_value < m_glyphs.size());

  Glyph::Private &G(m_glyphs[glyph_code.m_value]);
  if (!G.inited())
    {
      if (m_scalable)
        {
          G.init_scalable(this, *m_generator, thread_slot, glyph_code);
        }
      else
        {
          G.init_fixed_size(this, *m_generator, thread_slot, glyph_code);
        }
    }
  ASTRALassert(G.inited());
  return G;
}

bool
astral::Typeface::Implement::
glyph_inited(GlyphIndex glyph_code)
{
  ASTRALassert(glyph_code.m_value < m_glyphs.size());
  return m_glyphs[glyph_code.m_value].inited();
}

void
astral::Typeface::Implement::
parallel_fetch_glyphs_execute(unsigned int thread_slot)
{
  unsigned int w;

  m_parallel_fetch_stats[thread_slot] = 0u;
  #ifndef __EMSCRIPTEN__
    {
      while ((w = m_parallel_fetch_current.fetch_add(1u, std::memory_order_relaxed)) < m_parallel_fetch_glyph_list.size())
        {
          instance_glyph(thread_slot, m_parallel_fetch_glyph_list[w]);
          ++m_parallel_fetch_stats[thread_slot];
        }
    }
  #else
    {
      for (w = m_parallel_fetch_ranges[thread_slot].m_begin; w < m_parallel_fetch_ranges[thread_slot].m_end; ++w)
        {
          instance_glyph(thread_slot, m_parallel_fetch_glyph_list[w]);
          ++m_parallel_fetch_stats[thread_slot];
        }
    }
  #endif
}

void
astral::Typeface::Implement::
parallel_fetch_glyphs(unsigned int pnumber_threads,
                      c_array<const GlyphIndex> glyph_indices,
                      c_array<Glyph> out_glyphs)
{
  ASTRALassert(pnumber_threads <= number_threads());

  #if defined(__EMSCRIPTEN__) && defined(EMSCRIPTEN_NUMBER_THREADS)
    {
      const unsigned int v(EMSCRIPTEN_NUMBER_THREADS);
      pnumber_threads = t_min(pnumber_threads, v);
    }
  #endif

  /* For a list of the glyphs needed without repetition */
  ASTRALassert(m_parallel_fetch_threads.empty());
  ASTRALassert(m_parallel_fetch_glyph_list.empty());

  for (GlyphIndex g : glyph_indices)
    {
      add_to_list(&m_parallel_fetch_glyph_list, g);
    }
  m_parallel_fetch_stats.resize(pnumber_threads);

  #ifdef __EMSCRIPTEN__
    {
      /* Relying on an atomic counter fetch_add() to get a
       * glyph to instance under Emscripten starves all of
       * the other threads (unless the main thread calls
       * often emscripten_main_thread_process_queued_calls()
       * which has wierd performance impact). So instead,
       * just say up front what glyphs each thread gets.
       * The only issue with the below simple ranges is that
       * if one thread has a high percentage of heavy glyphs,
       * then the load across threads will be unbalanced.
       * Oh well.
       */
      unsigned int cnt = m_parallel_fetch_glyph_list.size() / pnumber_threads;

      m_parallel_fetch_ranges.clear();
      for (unsigned int i = 0; i < pnumber_threads; ++i)
        {
          range_type<unsigned int> R;

          R.m_begin = i * cnt;
          R.m_end = i * cnt + cnt;
          m_parallel_fetch_ranges.push_back(R);
        }
      m_parallel_fetch_ranges.back().m_end = m_parallel_fetch_glyph_list.size();
    }
  #else
    {
      m_parallel_fetch_current.store(0);
    }
  #endif

  for (unsigned int i = 1; i < pnumber_threads; ++i)
    {
      m_parallel_fetch_threads.push_back(std::thread(&Implement::parallel_fetch_glyphs_execute, this, i));
    }

  parallel_fetch_glyphs_execute(0);
  for (auto &T : m_parallel_fetch_threads)
    {
      T.join();
    }

  for (GlyphIndex g : m_parallel_fetch_glyph_list)
    {
      remove_from_list(g);
    }

  m_parallel_fetch_threads.clear();
  m_parallel_fetch_glyph_list.clear();

  /* now set the glyph output */
  for (unsigned int i = 0; i < glyph_indices.size(); ++i)
    {
      out_glyphs[i] = Glyph(&fetch_glyph(0, glyph_indices[i]));
    }
}

////////////////////////////////////////////////////
// astral::Glyph::Private methods
void
astral::Glyph::Private::
init_scalable(Typeface::Implement *typeface,
              const GlyphGenerator &generator,
              unsigned int thread_slot,
              GlyphIndex idx)
{
  m_init_called = true;
  m_glyph_index = idx;
  m_typeface = typeface;

  m_metrics.resize(1);
  generator.scalable_glyph_info(thread_slot,
                                m_glyph_index,
                                &m_metrics[0],
                                &m_colors,
                                &m_paths,
                                &m_fill_rules);

  ASTRALassert(m_paths.size() == m_fill_rules.size());
  if (m_colors.empty())
    {
      ASTRALassert(m_paths.size() == 1u);
      m_render_data.resize(1);
      m_is_colored = false;
    }
  else
    {
      ASTRALassert(m_paths.size() == m_colors.number_layers());
      m_render_data.resize(m_colors.number_palettes());
      m_is_colored = true;
    }

  const float rel_tol = 1e-2;
  astral::RelativeThreshhold tol(rel_tol);

  m_item_paths.resize(m_paths.size());
  for (unsigned int i = 0, endi = m_paths.size(); i < endi; ++i)
    {
      m_item_paths[i] = ItemPath::create(ItemPath::Geometry().add(m_paths[i], tol),
                                         m_typeface->implement_item_path_params());
    }

  /* map [0, 1]x[0, 1] to bb */
  m_tr.m_scale = m_metrics[0].m_bb.size();
  m_tr.m_translate = m_metrics[0].m_bb.min_point();
}

astral::reference_counted_ptr<const astral::StaticData>
astral::Glyph::Private::
render_data(RenderEngine &engine, GlyphPaletteID palette)
{
  palette = (is_colored()) ? palette : GlyphPaletteID(0);
  ASTRALassert(palette.m_value < m_render_data.size());

  if (!m_render_data[palette.m_value])
    {
      std::vector<ItemPath::Layer> layers;
      std::vector<gvec4> tmp_backing;
      vec4 one_color(1.0f, 1.0f, 1.0f, 1.0f);
      c_array<const vec4> colors;
      c_array<gvec4> tmp;

      if (is_colored())
        {
          colors = m_colors.layer_colors(palette);
        }
      else
        {
          colors = c_array<const vec4>(&one_color, 1);
        }

      for (unsigned int i = 0; i < colors.size(); ++i)
        {
          layers.push_back(ItemPath::Layer(*m_item_paths[i])
                           .color(colors[i])
                           .fill_rule(m_fill_rules[i])
                           .transformation(m_tr));
        }

      tmp_backing.resize(ItemPath::data_size(layers.size()));
      tmp = make_c_array(tmp_backing);
      ItemPath::pack_data(engine, make_const_c_array(layers), tmp);

      m_render_data[palette.m_value] = engine.static_data_allocator32().create(tmp);
    }
  return m_render_data[palette.m_value];
}

void
astral::Glyph::Private::
init_fixed_size(Typeface::Implement *typeface,
                const GlyphGenerator &generator,
                unsigned int thread_slot,
                GlyphIndex idx)
{
  m_init_called = true;
  m_glyph_index = idx;
  m_typeface = typeface;
  m_is_colored = false;

  unsigned int sz(generator.fixed_metrics().size());
  m_metrics.resize(sz);
  m_strikes.resize(sz);

  for (unsigned int strike_index = 0; strike_index < sz; ++strike_index)
    {
      bool b;

      b = generator.fixed_glyph_info(thread_slot, m_glyph_index, strike_index,
                                     &m_metrics[strike_index],
                                     &m_strikes[strike_index].m_image_size,
                                     &m_strikes[strike_index].m_image_data);

      /* it is a wild bug if the generator gives a different
       * answer for being color glyph depending on the strike
       * chosen; we just take it as colored if any strike is
       * colored.
       */
      m_is_colored = m_is_colored || b;
    }
}

void
astral::Glyph::Private::
eject(void)
{
  if (m_lock_counter == 0)
    {
      m_init_called = false;
      m_colors.clear();
      m_paths.clear();
      m_fill_rules.clear();
      m_item_paths.clear();
      m_render_data.clear();
      m_strikes.clear();
    }
}

astral::reference_counted_ptr<const astral::StaticData>
astral::Glyph::Private::
image_render_data(RenderEngine &engine, unsigned int strike_index,
                  reference_counted_ptr<const Image> *out_image)
{
  ASTRALassert(!is_scalable());
  ASTRALassert(strike_index < m_strikes.size());
  if (!m_strikes[strike_index].m_image)
    {
      generate_image(engine, strike_index);
    }
  if (out_image)
    {
      *out_image = m_strikes[strike_index].m_image;
    }

  return m_strikes[strike_index].m_static_data;
}

void
astral::Glyph::Private::
generate_image(RenderEngine &engine, unsigned int strike_index)
{
  uvec2 sz(m_strikes[strike_index].m_image_size);
  std::vector<FixedPointColor_sRGB> pixels, prev_pixels;
  reference_counted_ptr<astral::Image> im;
  ImageSampler image_sampler;

  ASTRALassert(!m_strikes[strike_index].m_image);
  ASTRALassert(!m_strikes[strike_index].m_static_data);

  /* We pad the glyph image by one pixel on each
   * size to make filtering happier.
   */
  sz += uvec2(2, 2);

  if (strike_index == m_typeface->smallest_strike())
    {
      im = engine.image_atlas().create_image(sz);
    }
  else
    {
      /* only the lowest resolution strike gets mipmaps; */
      im = engine.image_atlas().create_image(1, sz);
    }
  im->colorspace(colorspace_srgb);

  /* We cannot just freely swap pixels with
   * m_strikes[strike_index].m_image_size
   * because of the padding; we need to copy
   * the data.
   */
  pixels.resize(sz.x() * sz.y(), FixedPointColor_sRGB(0, 0, 0, 0));
  for (unsigned int y = 1u; y + 1u < sz.y(); ++y)
    {
      for (unsigned int x = 1u; x + 1u < sz.x(); ++x)
        {
          unsigned int src, dst;

          src = (x - 1u) + (y - 1u) * (sz.x() - 2u);
          dst = x + y * sz.x();

          pixels[dst] = m_strikes[strike_index].m_image_data[src];
        }
    }
  m_strikes[strike_index].m_image_data.clear();

  for (unsigned int mipmap = 0, w = sz.x(), h = sz.y();
       h > 0u && w > 0u && mipmap < im->number_mipmap_levels();
       w >>= 1u, h >>= 1u, ++mipmap)
    {
      c_array<const u8vec4> u8pixels;

      u8pixels = make_c_array(pixels).reinterpret_pointer<const u8vec4>();
      im->set_pixels(mipmap, ivec2(0, 0), ivec2(w, h), w, u8pixels);

      /* It is debatable to support mipmaps in the glyph-images;
       * what is even more debatable is that the color values are
       * in sRGB color space and thus we should first convert to
       * linear, then average and then convert back.
       */
      if (w > 1u && h > 1u && mipmap + 1 < im->number_mipmap_levels())
        {
          uvec2 msz(w >> 1u, h >> 1u);

          pixels.swap(prev_pixels);
          pixels.resize(msz.x() * msz.y());
          for (unsigned int y = 0; y < msz.y(); ++y)
            {
              for (unsigned int x = 0; x < msz.x(); ++x)
                {
                  ivec2 src_xy(2u * x, 2u * y);
                  unsigned int dst(x + y * msz.x());
                  unsigned int src00(src_xy.x() + src_xy.y() * w);
                  unsigned int src10(src_xy.x() + 1 + src_xy.y() * w);
                  unsigned int src01(src_xy.x() + w + src_xy.y() * w);
                  unsigned int src11(src_xy.x() + 1 + w + src_xy.y() * w);
                  uvec4 color;

                  color = uvec4(prev_pixels[src00].m_value)
                    + uvec4(prev_pixels[src10].m_value)
                    + uvec4(prev_pixels[src01].m_value)
                    + uvec4(prev_pixels[src11].m_value);

                  color.x() >>= 2u;
                  color.y() >>= 2u;
                  color.z() >>= 2u;
                  color.w() >>= 2u;

                  pixels[dst].m_value = u8vec4(color);
                }
            }
        }
    }

  m_strikes[strike_index].m_image = im;
  image_sampler = ImageSampler(*im, filter_cubic,
                               (strike_index == m_typeface->smallest_strike()) ? mipmap_ceiling : mipmap_none);


  m_strikes[strike_index].m_static_data = engine.pack_image_sampler_as_static_data(image_sampler);
}

/////////////////////////////////////
// astral::Glyph methods
astral::Glyph::
Glyph(const Glyph &glyph):
  m_private(glyph.m_private)
{
  if (m_private)
    {
      m_private->increment_lock();
    }
}

astral::Glyph::
Glyph(Private *p):
  m_private(p)
{
  if (m_private)
    {
      m_private->increment_lock();
    }
}

astral::Glyph&
astral::Glyph::
operator=(Glyph &&glyph)
{
  if (m_private)
    {
      m_private->decrement_lock();
    }
  m_private = glyph.m_private;
  glyph.m_private = nullptr;

  return *this;
}

astral::Glyph::
~Glyph()
{
  if (m_private)
    {
      m_private->decrement_lock();
    }
}

astral::Typeface&
astral::Glyph::
typeface(void) const
{
  ASTRALassert(valid());
  return m_private->typeface();
}

bool
astral::Glyph::
is_scalable(void) const
{
  return typeface().is_scalable();
}

astral::GlyphIndex
astral::Glyph::
glyph_index(void) const
{
  ASTRALassert(valid());
  return m_private->glyph_index();
}

const astral::GlyphMetrics&
astral::Glyph::
scalable_metrics(void) const
{
  ASTRALassert(valid());
  return m_private->scalable_metrics();
}

const astral::GlyphMetrics&
astral::Glyph::
fixed_metrics(unsigned int strike_index) const
{
  ASTRALassert(valid());
  return m_private->fixed_metrics(strike_index);
}

bool
astral::Glyph::
is_colored(void) const
{
  ASTRALassert(valid());
  return m_private->is_colored();
}

const astral::GlyphColors*
astral::Glyph::
colors(void) const
{
  ASTRALassert(valid());
  return m_private->colors();
}

const astral::ScaleTranslate&
astral::Glyph::
scale_translate(void) const
{
  return m_private->scale_translate();
}

const astral::Path*
astral::Glyph::
path(unsigned int layer, enum fill_rule_t *out_fill_rule,
     reference_counted_ptr<const ItemPath> *out_item_path) const
{
  ASTRALassert(valid());
  if (!is_scalable())
    {
      if (out_item_path)
        {
          out_item_path->clear();
        }
      return nullptr;
    }
  return m_private->path(layer, out_fill_rule, out_item_path);
}

astral::reference_counted_ptr<const astral::StaticData>
astral::Glyph::
render_data(RenderEngine &engine, GlyphPaletteID palette) const
{
  ASTRALassert(valid());
  if (!is_scalable())
    {
      return nullptr;
    }

  return m_private->render_data(engine, palette);
}

astral::reference_counted_ptr<const astral::StaticData>
astral::Glyph::
image_render_data(RenderEngine &engine, unsigned int strike_index,
                  reference_counted_ptr<const Image> *out_image) const
{
  ASTRALassert(valid());
  if (is_scalable())
    {
      if (out_image)
        {
          *out_image = nullptr;
        }
      return nullptr;
    }

  return m_private->image_render_data(engine, strike_index, out_image);
}

/////////////////////////////////////
// astral::Typeface methods
astral::reference_counted_ptr<astral::Typeface>
astral::Typeface::
create(reference_counted_ptr<const GlyphGenerator> generator,
       const ItemPath::GenerationParams &params)
{
  return ASTRALnew Typeface::Implement(generator, params);
}

void
astral::Typeface::
default_item_path_params(const ItemPath::GenerationParams &v)
{
  Implement::default_item_path_params() = v;
}

const astral::ItemPath::GenerationParams&
astral::Typeface::
default_item_path_params(void)
{
  return Implement::default_item_path_params();
}

const astral::ItemPath::GenerationParams&
astral::Typeface::
item_path_params(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->implement_item_path_params();
}

bool
astral::Typeface::
is_scalable(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->is_scalable();
}

const astral::TypefaceMetricsScalable&
astral::Typeface::
scalable_metrics(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->scalable_metrics();
}

astral::c_array<const astral::TypefaceMetricsFixedSize>
astral::Typeface::
fixed_metrics(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->fixed_metrics();
}

unsigned int
astral::Typeface::
number_glyphs(void) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->number_glyphs();
}

astral::GlyphIndex
astral::Typeface::
glyph_index(uint32_t character_code) const
{
  const Implement *p;

  p = static_cast<const Implement*>(this);
  return p->glyph_index(character_code);
}

astral::Glyph
astral::Typeface::
fetch_glyph(GlyphIndex glyph_code)
{
  Implement *p;
  Glyph::Private *g;

  p = static_cast<Implement*>(this);
  g = &p->fetch_glyph(0, glyph_code);
  return Glyph(g);
}

void
astral::Typeface::
fetch_glyphs(c_array<const GlyphIndex> glyph_indices,
             c_array<Glyph> out_glyphs)
{
  ASTRALassert(glyph_indices.size() == out_glyphs.size());
  for (unsigned int i = 0; i < glyph_indices.size(); ++i)
    {
      out_glyphs[i] = fetch_glyph(glyph_indices[i]);
    }
}

void
astral::Typeface::
fetch_glyphs_parallel(unsigned int number_threads,
                      c_array<const GlyphIndex> glyph_indices,
                      c_array<Glyph> out_glyphs)
{
  Typeface::Implement *p;

  p = static_cast<Typeface::Implement*>(this);

  #if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
    {
      /* build does not have pthreads, just do unparallel fetching */
      number_threads = 1;
    }
  #else
    {
      number_threads = t_min(number_threads, p->number_threads());
    }
  #endif

  if (number_threads > 1)
    {
      p->parallel_fetch_glyphs(number_threads, glyph_indices, out_glyphs);
    }
  else
    {
      fetch_glyphs(glyph_indices, out_glyphs);
    }
}

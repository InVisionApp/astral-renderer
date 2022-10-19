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

#include <random>
#include <vector>
#include <SDL.h>
#include <astral/util/tile_allocator.hpp>
#include <astral/util/layered_rect_atlas.hpp>

#include "render_engine_gl3_demo.hpp"
#include "PanZoomTracker.hpp"
#include "simple_time.hpp"
#include "cycle_value.hpp"
#include "text_helper.hpp"

class AtlasBase:public astral::reference_counted<AtlasBase>::non_concurrent
{
public:
  virtual
  const void*
  allocate_region(astral::ivec2 size) = 0;

  virtual
  void
  remove_region(const void*) = 0;

  virtual
  astral::uvec3
  location(const void*) = 0;

  virtual
  astral::uvec2
  size(const void*) = 0;

  virtual
  unsigned int
  number_layers(void) = 0;

  virtual
  void
  number_layers(unsigned int L) = 0;

  virtual
  void
  clear(void) = 0;

  virtual
  astral::uvec2
  log2_max_tile_size(void) = 0;

  astral::ivec2
  max_tile_size(void)
  {
    astral::uvec2 tmp(log2_max_tile_size());

    tmp.x() = 1u << tmp.x();
    tmp.y() = 1u << tmp.y();

    return astral::ivec2(tmp);
  }
};

class TiledAtlas:public AtlasBase
{
public:
  TiledAtlas(astral::uvec2 log2_max_tile_size,
             astral::uvec2 number_tiles_per_layer,
             unsigned int number_layers):
    m_atlas(log2_max_tile_size,
            number_tiles_per_layer,
            number_layers)
  {}

  virtual
  const void*
  allocate_region(astral::ivec2 sz) override
  {
    return m_atlas.allocate_region(sz.x(), sz.y());
  }

  virtual
  void
  remove_region(const void *p) override
  {
    const astral::TileAllocator::Region *tile;

    tile = static_cast<const astral::TileAllocator::Region*>(p);
    m_atlas.release_region(tile);
  }

  virtual
  astral::uvec3
  location(const void *p) override
  {
    const astral::TileAllocator::Region *tile;

    tile = static_cast<const astral::TileAllocator::Region*>(p);
    return tile->location();
  }

  virtual
  astral::uvec2
  size(const void *p) override
  {
    astral::uvec2 s;
    const astral::TileAllocator::Region *region;

    region = static_cast<const astral::TileAllocator::Region*>(p);
    return region->size();
  }

  virtual
  unsigned int
  number_layers(void) override
  {
    return m_atlas.number_layers();
  }

  virtual
  void
  number_layers(unsigned int L) override
  {
    m_atlas.number_layers(L);
  }

  virtual
  void
  clear(void) override
  {
    m_atlas.release_all();
  }

  virtual
  astral::uvec2
  log2_max_tile_size(void) override
  {
    return m_atlas.log2_max_tile_size();
  }

private:
  astral::TileAllocator m_atlas;
};

class RectAtlas:public AtlasBase
{
public:
  RectAtlas(astral::uvec2 log2_max_tile_size,
            astral::uvec2 number_tiles_per_layer,
            unsigned int number_layers):
    m_log2_max_tile_size(log2_max_tile_size)
  {
    astral::ivec2 dims;

    dims.x() = (1u << log2_max_tile_size.x()) * number_tiles_per_layer.x();
    dims.y() = (1u << log2_max_tile_size.y()) * number_tiles_per_layer.y();

    m_atlas = astral::LayeredRectAtlas::create();
    m_atlas->clear(dims, number_layers);
  }

  virtual
  const void*
  allocate_region(astral::ivec2 dims) override
  {
    astral::LayeredRectAtlas::Entry E;

    E = m_atlas->allocate_rectangle(dims);
    if (E.valid())
      {
        return m_entry_pool.create(E);
      }
    else
      {
        return nullptr;
      }
  }

  virtual
  void
  remove_region(const void *p) override
  {
    const astral::LayeredRectAtlas::Entry *E;

    E = static_cast<const astral::LayeredRectAtlas::Entry*>(p);
    m_atlas->free_rectangle(*E);
    m_entry_pool.reclaim(const_cast<astral::LayeredRectAtlas::Entry*>(E));
  }

  virtual
  astral::uvec3
  location(const void *p) override
  {
    const astral::LayeredRectAtlas::Entry *E;
    astral::ivec2 L;

    E = static_cast<const astral::LayeredRectAtlas::Entry*>(p);
    L = E->location();
    return astral::uvec3(L.x(), L.y(), E->layer());
  }

  virtual
  astral::uvec2
  size(const void *p) override
  {
    const astral::LayeredRectAtlas::Entry *E;
    astral::ivec2 L;

    E = static_cast<const astral::LayeredRectAtlas::Entry*>(p);
    L = E->dimensions();
    return astral::uvec2(L.x(), L.y());
  }

  virtual
  unsigned int
  number_layers(void) override
  {
    return m_atlas->number_layers();
  }

  virtual
  void
  number_layers(unsigned int L) override
  {
    m_atlas->number_layers(L);
  }

  virtual
  void
  clear(void) override
  {
    m_atlas->clear();
    m_entry_pool.clear();
  }

  virtual
  astral::uvec2
  log2_max_tile_size(void) override
  {
    return m_log2_max_tile_size;
  }

private:
  astral::MemoryPool<astral::LayeredRectAtlas::Entry, 1024 * 1024> m_entry_pool;
  astral::reference_counted_ptr<astral::LayeredRectAtlas> m_atlas;
  astral::uvec2 m_log2_max_tile_size;
};

class RegionAllocatorTest:public render_engine_gl3_demo
{
public:
  RegionAllocatorTest(void);

  ~RegionAllocatorTest();

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
  enum atlas_mode_t
    {
      tiled_atlas,
      rect_atlas,
    };

  void
  add_region(astral::ivec2 sz);

  void
  remove_region(unsigned int layer, unsigned int idx);

  void
  draw_region(astral::RenderEncoderSurface encoder,
              const void *region,
              astral::RenderValue<astral::Brush> bregion,
              astral::RenderValue<astral::Brush> bregion_border);

  void
  draw_rect(astral::RenderEncoderSurface encoder,
            const astral::Rect &outer_rect,
            astral::RenderValue<astral::Brush> interior,
            astral::RenderValue<astral::Brush> border);

  void
  draw_hud(astral::RenderEncoderSurface encoder);

  void
  run_perf_test(void);

  void
  clear(void)
  {
    m_region_allocator->clear();
    m_regions.clear();
    m_regions.resize(m_region_allocator->number_layers());
  }

  command_separator m_demo_options;
  command_line_argument_value<unsigned int> m_log2_tile_size_x;
  command_line_argument_value<unsigned int> m_log2_tile_size_y;
  command_line_argument_value<unsigned int> m_number_tiles_x;
  command_line_argument_value<unsigned int> m_number_tiles_y;
  command_line_argument_value<unsigned int> m_number_layers;
  command_line_argument_value<unsigned int> m_random_seed;
  command_line_argument_value<unsigned int> m_max_random_size_x;
  command_line_argument_value<unsigned int> m_max_random_size_y;
  enumerated_command_line_argument_value<enum atlas_mode_t> m_mode;

  std::mt19937 m_generator;
  astral::vecN<std::uniform_int_distribution<>, 2> m_distribution;
  astral::ivec2 m_current_request_size;
  astral::uvec2 m_current_max_random_size;
  unsigned int m_current_layer;
  astral::vec2 m_layer_size;
  bool m_force_to_power2_size;

  astral::reference_counted_ptr<astral::TextItem> m_text_item;

  PanZoomTrackerSDLEvent m_zoom;

  std::vector<std::vector<const void*>> m_regions;
  astral::reference_counted_ptr<AtlasBase> m_region_allocator;

  simple_time m_draw_timer;
};

RegionAllocatorTest::
RegionAllocatorTest(void):
  m_demo_options("Demo Options", *this),
  m_log2_tile_size_x(12, "log2_tile_size_x", "tile size in tile allocator", *this),
  m_log2_tile_size_y(12, "log2_tile_size_y", "tile size in tile allocator", *this),
  m_number_tiles_x(1u, "number_tiles_x", "number of tiles supported in x-direction", *this),
  m_number_tiles_y(1u, "number_tiles_y", "number of tiles supported in y-direction", *this),
  m_number_layers(1u, "number_layers", "number of layers", *this),
  m_random_seed(std::mt19937::default_seed, "random_seed", "seed for random number generator", *this),
  m_max_random_size_x(1u << 6u, "max_random_size_x", "", *this),
  m_max_random_size_y(1u << 6u, "max_random_size_y", "", *this),
  m_mode(tiled_atlas,
         enumerated_string_type<enum atlas_mode_t>()
         .add_entry("tiled_atlas", tiled_atlas, "")
         .add_entry("rect_atlas", rect_atlas, ""),
         "mode", "", *this),
  m_current_layer(0),
  m_force_to_power2_size(false)
{
  std::cout << "Controls:"
            << "\n\tl/strl-l: change layer"
            << "\n\tb: toggle forcing region size to power of two"
            << "\n\tx/ctrl-x: increase/decrease of x-size allocation request (shift multiplies increase by factor by 10, alt by 100)"
            << "\n\ty/ctrl-y: increase/decrease of y-size allocation request (shift multiplies increase by factor by 10, alt by 100)"
            << "\n\tp: toggle make all allocations power of 2 in size"
            << "\n\ta: allocate a region of the current request size"
            << "\n\t\tshift: multiply request by 10"
            << "\n\t\tctrl: multiply request by 100"
            << "\n\t\talt: multiply request by 1000"
            << "\n\tr: allocate a region of random size"
            << "\n\t\tshift: multiply request by 10"
            << "\n\t\tctrl: multiply request by 100"
            << "\n\t\talt: multiply request by 1000"
            << "\n\td: release the region at the end of the region list for the current displayer layer"
            << "\n\t\tshift: multiply request by 10"
            << "\n\t\tctrl: multiply request by 100"
            << "\n\t\talt: multiply request by 1000"
            << "\n\ts: release the region at the start of the region list for the current displayer layer"
            << "\n\t\tshift: multiply request by 10"
            << "\n\t\tctrl: multiply request by 100"
            << "\n\t\talt: multiply request by 1000"
            << "\n\te: release a random region on a random layer"
            << "\n\t\tshift: multiply request by 10"
            << "\n\t\tctrl: multiply request by 100"
            << "\n\t\talt: multiply request by 1000"
            << "\n\tw: release a random region on the current layer"
            << "\n\t\tshift: multiply request by 10"
            << "\n\t\tctrl: multiply request by 100"
            << "\n\t\talt: multiply request by 1000"
            << "\n\tt: run a perf test of allocating and releasing many regions"
            << "\n\tc: release all regions";
}

RegionAllocatorTest::
~RegionAllocatorTest()
{
  if (m_region_allocator)
    {
      m_region_allocator->clear();
    }
}

void
RegionAllocatorTest::
init_gl(int, int)
{
  m_current_request_size = astral::ivec2(32u, 32u);

  m_max_random_size_x.value() = astral::t_min(m_max_random_size_x.value(),
                                              1u << m_log2_tile_size_x.value());
  m_max_random_size_y.value() = astral::t_min(m_max_random_size_y.value(),
                                              1u << m_log2_tile_size_y.value());

  m_generator = std::mt19937(m_random_seed.value());
  m_distribution.x() = std::uniform_int_distribution<>(1, m_max_random_size_x.value());
  m_distribution.y() = std::uniform_int_distribution<>(1, m_max_random_size_y.value());

  if (m_mode.value() == tiled_atlas)
    {
      m_region_allocator = ASTRALnew TiledAtlas(astral::uvec2(m_log2_tile_size_x.value(), m_log2_tile_size_y.value()),
                                                astral::uvec2(m_number_tiles_x.value(), m_number_tiles_y.value()),
                                                m_number_layers.value());
    }
  else
    {
      m_region_allocator = ASTRALnew RectAtlas(astral::uvec2(m_log2_tile_size_x.value(), m_log2_tile_size_y.value()),
                                               astral::uvec2(m_number_tiles_x.value(), m_number_tiles_y.value()),
                                               m_number_layers.value());
    }

  astral::uvec2 tile_size;

  tile_size.x() = (1u << m_log2_tile_size_x.value());
  tile_size.y() = (1u << m_log2_tile_size_y.value());
  m_layer_size.x() = static_cast<float>(tile_size.x() * m_number_tiles_x.value());
  m_layer_size.y() = static_cast<float>(tile_size.y() * m_number_tiles_y.value());

  float pixel_size(16.0f);
  astral::Font font(default_typeface(), pixel_size);
  m_text_item = astral::TextItem::create(font);

  m_regions.resize(m_number_layers.value());
}

void
RegionAllocatorTest::
remove_region(unsigned int layer, unsigned int idx)
{
  ASTRALassert(layer < m_regions.size());
  ASTRALassert(idx < m_regions[layer].size());

  std::swap(m_regions[layer][idx], m_regions[layer].back());
  m_region_allocator->remove_region(m_regions[layer].back());
  m_regions[layer].pop_back();
}

void
RegionAllocatorTest::
add_region(astral::ivec2 size)
{
  const void *region;
  astral::uvec3 L;

  if (m_force_to_power2_size)
    {
      size.x() = astral::next_power_of_2(size.x());
      size.y() = astral::next_power_of_2(size.y());
    }

  region = m_region_allocator->allocate_region(size);
  if (region == nullptr)
    {
      unsigned int L;

      /* attempt to add a layer */

      L = m_region_allocator->number_layers();
      std::cout << "(Attempt to) increase number of layers from "
                << L << " to " << L + 1 << "\n";
      m_region_allocator->number_layers(L + 1u);
      m_regions.resize(m_region_allocator->number_layers());

      region = m_region_allocator->allocate_region(size);
    }

  if (region)
    {
      L = m_region_allocator->location(region);

      ASTRALassert(L.z() < m_regions.size());
      m_regions[L.z()].push_back(region);
    }
}

void
RegionAllocatorTest::
run_perf_test(void)
{
  unsigned int count(1000u * 1000u), loop_count(10u);
  std::vector<unsigned int> nonempty_layers;
  simple_time tm, em;

  clear();
  for (unsigned int i = 0; i < count; ++i)
    {
      astral::ivec2 sz;

      sz.x() = m_distribution.x()(m_generator);
      sz.y() = m_distribution.y()(m_generator);
      add_region(sz);
    }

  clear();

  tm.restart();
  for (unsigned int i = 0; i < count; ++i)
    {
      astral::ivec2 sz;

      sz.x() = m_distribution.x()(m_generator);
      sz.y() = m_distribution.y()(m_generator);
      add_region(sz);
    }

  for (unsigned int loop = 0; loop < loop_count; ++loop)
    {
      nonempty_layers.clear();
      for (unsigned int i = 0; i < m_regions.size(); ++i)
        {
          if (!m_regions[i].empty())
            {
              nonempty_layers.push_back(i);
            }
        }

      //add 30% and remove 30%
      unsigned int sub_count(count * 3u / 10u);
      for (unsigned int i = 0; i < sub_count; ++i)
        {
          astral::ivec2 sz;

          sz.x() = m_distribution.x()(m_generator);
          sz.y() = m_distribution.y()(m_generator);
          add_region(sz);
        }

      for (unsigned int i = 0; i < sub_count && !nonempty_layers.empty(); ++i)
        {
          unsigned int layer_idx, idx, layer;
          std::uniform_int_distribution<> dist(0, nonempty_layers.size() - 1);

          layer_idx = dist(m_generator);
          layer = nonempty_layers[layer_idx];
          ASTRALassert(!m_regions[layer].empty());

          dist = std::uniform_int_distribution<>(0, m_regions[layer].size() - 1);
          idx = dist(m_generator);
          remove_region(layer, idx);

          if (m_regions[layer].empty())
            {
              std::swap(nonempty_layers[layer_idx], nonempty_layers.back());
              nonempty_layers.pop_back();
            }
        }
    }

  em.restart();
  while (!nonempty_layers.empty())
    {
      unsigned int layer_idx, idx, layer;
      std::uniform_int_distribution<> dist(0, nonempty_layers.size() - 1);

      layer_idx = dist(m_generator);
      layer = nonempty_layers[layer_idx];
      ASTRALassert(!m_regions[layer].empty());

      dist = std::uniform_int_distribution<>(0, m_regions[layer].size() - 1);
      idx = dist(m_generator);
      remove_region(layer, idx);

      if (m_regions[layer].empty())
        {
          std::swap(nonempty_layers[layer_idx], nonempty_layers.back());
          nonempty_layers.pop_back();
        }
    }

  clear();
  std::cout << "Took " << tm.elapsed() << " ms to run perf-test, "
            << em.elapsed() << " ms to do element-by-element clear\n";
}

void
RegionAllocatorTest::
handle_event(const SDL_Event &ev)
{
  m_zoom.handle_event(ev);
  if (ev.type == SDL_KEYDOWN)
    {
      switch(ev.key.keysym.sym)
        {
        case SDLK_l:
          cycle_value(m_current_layer,
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      m_region_allocator->number_layers());
          std::cout << "Show layer #" << m_current_layer << " of "
                    << m_region_allocator->number_layers() << " layers\n";
          break;

        case SDLK_b:
          m_force_to_power2_size = !m_force_to_power2_size;
          std::cout << "Force region size to power of 2 set to " << std::boolalpha
                    << m_force_to_power2_size << "\n";
          break;

        case SDLK_x:
          {
            int sz = 1;

            if (ev.key.keysym.mod & KMOD_SHIFT)
              {
                sz *= 10;
              }

            if (ev.key.keysym.mod & KMOD_ALT)
              {
                sz *= 100;
              }

            if (ev.key.keysym.mod & KMOD_CTRL)
              {
                sz *= -1;
              }

            m_current_request_size.x() += sz;
            m_current_request_size.x() = astral::t_max(0, m_current_request_size.x());
            m_current_request_size.x() = astral::t_min(m_current_request_size.x(), m_region_allocator->max_tile_size().x());
            std::cout << "Add region size set to " << m_current_request_size << "\n";
          }
          break;

        case SDLK_y:
          {
            int sz = 1;

            if (ev.key.keysym.mod & KMOD_SHIFT)
              {
                sz *= 10;
              }

            if (ev.key.keysym.mod & KMOD_ALT)
              {
                sz *= 100;
              }

            if (ev.key.keysym.mod & KMOD_CTRL)
              {
                sz *= -1;
              }

            m_current_request_size.y() += sz;
            m_current_request_size.y() = astral::t_max(0, m_current_request_size.y());
            m_current_request_size.y() = astral::t_min(m_current_request_size.y(), m_region_allocator->max_tile_size().y());
            std::cout << "Add region size set to " << m_current_request_size << "\n";
          }
          break;

        case SDLK_a:
          {
            unsigned int count(1);
            simple_time tm;

            if (ev.key.keysym.mod & KMOD_SHIFT)
              {
                count *= 10u;
              }

            if (ev.key.keysym.mod & KMOD_CTRL)
              {
                count *= 100u;
              }

            if (ev.key.keysym.mod & KMOD_ALT)
              {
                count *= 1000u;
              }

            tm.restart();
            for (unsigned int i = 0; i < count; ++i)
              {
                add_region(m_current_request_size);
              }
            std::cout << "Took " << tm.elapsed() << " ms to allocate "
                      << count << " regions of size " << m_current_request_size
                      << "\n";
          }
          break;

        case SDLK_d:
          {
            unsigned int count(1);
            simple_time tm;

            if (ev.key.keysym.mod & KMOD_SHIFT)
              {
                count *= 10u;
              }

            if (ev.key.keysym.mod & KMOD_CTRL)
              {
                count *= 100u;
              }

            if (ev.key.keysym.mod & KMOD_ALT)
              {
                count *= 1000u;
              }

            tm.restart();
            for (unsigned int i = 0; i < count && !m_regions[m_current_layer].empty(); ++i)
              {
                remove_region(m_current_layer, m_regions[m_current_layer].size() - 1u);
              }
            std::cout << "Took " << tm.elapsed() << " ms to remove "
                      << count << " most recently made regions on layer "
                      << m_current_layer << "\n";
          }
          break;

        case SDLK_s:
          {
            unsigned int count(1);
            simple_time tm;

            if (ev.key.keysym.mod & KMOD_SHIFT)
              {
                count *= 10u;
              }

            if (ev.key.keysym.mod & KMOD_CTRL)
              {
                count *= 100u;
              }

            if (ev.key.keysym.mod & KMOD_ALT)
              {
                count *= 1000u;
              }

            tm.restart();
            for (unsigned int i = 0; i < count && !m_regions[m_current_layer].empty(); ++i)
              {
                remove_region(m_current_layer, 0);
              }
            std::cout << "Took " << tm.elapsed() << " ms to remove "
                      << count << " oldest made regions on layer "
                      << m_current_layer << "\n";
          }
          break;

        case SDLK_r:
          {
            unsigned int count(1);
            simple_time tm;

            if (ev.key.keysym.mod & KMOD_SHIFT)
              {
                count *= 10u;
              }

            if (ev.key.keysym.mod & KMOD_CTRL)
              {
                count *= 100u;
              }

            if (ev.key.keysym.mod & KMOD_ALT)
              {
                count *= 1000u;
              }

            tm.restart();
            for (unsigned int i = 0; i < count; ++i)
              {
                astral::ivec2 sz;

                sz.x() = m_distribution.x()(m_generator);
                sz.y() = m_distribution.y()(m_generator);
                add_region(sz);
              }
            std::cout << "Took " << tm.elapsed() << " ms to allocate "
                      << count << " regions of random size\n";
          }
          break;

        case SDLK_e:
          {
            unsigned int count(1);
            std::uniform_int_distribution<> dist;
            simple_time tm;
            std::vector<unsigned int> nonempty_layers;

            if (ev.key.keysym.mod & KMOD_SHIFT)
              {
                count *= 10u;
              }

            if (ev.key.keysym.mod & KMOD_CTRL)
              {
                count *= 100u;
              }

            if (ev.key.keysym.mod & KMOD_ALT)
              {
                count *= 1000u;
              }

            for (unsigned int i = 0; i < m_regions.size(); ++i)
              {
                if (!m_regions[i].empty())
                  {
                    nonempty_layers.push_back(i);
                  }
              }

            tm.restart();
            for (unsigned int i = 0; i < count && !nonempty_layers.empty(); ++i)
              {
                unsigned int layer_idx, idx, layer;

                layer_idx = dist(m_generator) % nonempty_layers.size();
                layer = nonempty_layers[layer_idx];
                ASTRALassert(!m_regions[layer].empty());

                idx = dist(m_generator) % m_regions[layer].size();
                remove_region(layer, idx);

                if (m_regions[layer].empty())
                  {
                    std::swap(nonempty_layers[layer_idx], nonempty_layers.back());
                    nonempty_layers.pop_back();
                  }
              }
            std::cout << "Took " << tm.elapsed() << " ms to remove "
                      << count << " random regions\n";
          }
          break;

        case SDLK_w:
          {
            unsigned int count(1);
            std::uniform_int_distribution<> dist;
            simple_time tm;

            if (ev.key.keysym.mod & KMOD_SHIFT)
              {
                count *= 10u;
              }

            if (ev.key.keysym.mod & KMOD_CTRL)
              {
                count *= 100u;
              }

            if (ev.key.keysym.mod & KMOD_ALT)
              {
                count *= 1000u;
              }

            tm.restart();
            for (unsigned int i = 0; i < count && !m_regions[m_current_layer].empty(); ++i)
              {
                unsigned int idx;

                idx = dist(m_generator) % m_regions[m_current_layer].size();
                remove_region(m_current_layer, idx);
              }
            std::cout << "Took " << tm.elapsed() << " ms to remove "
                      << count << " random regions\n";
          }
          break;

        case SDLK_t:
          if (ev.key.keysym.mod & KMOD_CTRL)
            {
              for (unsigned int i = 0; i < 10; ++i)
                {
                  run_perf_test();
                }
            }
          else
            {
              run_perf_test();
            }
          break;

        case SDLK_c:
          {
            clear();
            std::cout << "Cleared\n";
          }
          break;

        } //of switch()
    }

  render_engine_gl3_demo::handle_event(ev);
}

void
RegionAllocatorTest::
draw_region(astral::RenderEncoderSurface encoder,
            const void *region,
            astral::RenderValue<astral::Brush> bregion,
            astral::RenderValue<astral::Brush> bregion_border)
{
  astral::vec2 sz;
  astral::uvec3 loc;
  astral::Rect rect;

  loc = m_region_allocator->location(region);
  sz = astral::vec2(m_region_allocator->size(region));

  rect
    .min_point(static_cast<float>(loc.x()),
               static_cast<float>(loc.y()))
    .size(sz.x(), sz.y());

  draw_rect(encoder, rect, bregion, bregion_border);
}

void
RegionAllocatorTest::
draw_rect(astral::RenderEncoderSurface encoder,
          const astral::Rect &outer_rect,
          astral::RenderValue<astral::Brush> interior,
          astral::RenderValue<astral::Brush> border)
{
  if (border.valid())
    {
      float thickness;
      astral::Rect inner_rect;

      /* draw the outline around the rect */
      encoder.draw_rect(outer_rect, false, border);

      thickness = 2.0f / m_zoom.transformation().m_scale;

      inner_rect.m_min_point = outer_rect.m_min_point + astral::vec2(thickness);
      inner_rect.m_max_point = outer_rect.m_max_point - astral::vec2(thickness);
      inner_rect.standardize();

      encoder.draw_rect(inner_rect, false, interior);
    }
  else
    {
      encoder.draw_rect(outer_rect, false, interior);
    }
}

void
RegionAllocatorTest::
draw_frame(void)
{
  astral::RenderEncoderSurface encoder;
  astral::RenderValue<astral::Brush> red, white, black;

  encoder = renderer().begin( render_target());
  encoder.transformation(m_zoom.transformation().astral_transformation());

  /* make our brushes */
  red = encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 0.0f, 0.0f, 0.5f)));
  white = encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 1.0f, 1.0f)));
  black = encoder.create_value(astral::Brush().base_color(astral::vec4(0.0f, 0.0f, 0.0f, 1.0f)));

  /* draw a white rect for the entire atlas-layer being shown */
  encoder.draw_rect(astral::Rect()
                    .min_point(0.0f, 0.0f)
                    .max_point(m_layer_size.x(), m_layer_size.y()),
                    false, // no anti-aliasing please
                    white);

  /* draw the allocations regions as a red rect with a black outline */
  for (const void *region : m_regions[m_current_layer])
    {
      ASTRALassert(m_region_allocator->location(region).z() == m_current_layer);
      draw_region(encoder, region, red, black);
    }

  if (!pixel_testing())
    {
      draw_hud(encoder);
    }

  renderer().end();
}

void
RegionAllocatorTest::
draw_hud(astral::RenderEncoderSurface encoder)
{
  float frame_ms;
  std::ostringstream ostr;
  unsigned int total(0);

  for (const auto &p : m_regions)
    {
      total += p.size();
    }

  frame_ms = static_cast<float>(m_draw_timer.restart_us()) * 0.001f;
  ostr << "Current Size Request: " << m_current_request_size
       << "\n[b] ForcePower2 allocation " << std::boolalpha << m_force_to_power2_size
       << "\n[l/shiftl] Showing layer #" << m_current_layer << " of "
       << m_region_allocator->number_layers() << " layers\n"
       << "Current region request size: " << m_current_request_size << "\n"
       << "\t[x] (shift: x10, alt: x100, ctrl:shring) change region request width\n"
       << "\t[y] (shift: x10, alt: x100, ctrl:shring) change region request height\n"
       << "[a] (shift: x10, ctrl: x100, alt:x1000) allocate region of current size\n"
       << "[a] (shift: x10, ctrl: x100, alt:x1000) allocate region of random size\n"
       << "[d] (shift: x10, ctrl: x100, alt:x1000) remove most recently allocated region on current layer\n"
       << "[s] (shift: x10, ctrl: x100, alt:x1000) remove oldest allocated region on current layer\n"
       << "[w] (shift: x10, ctrl: x100, alt:x1000) remove random allocated region on current layer\n"
       << "[e] (shift: x10, ctrl: x100, alt:x1000) remove random allocated region on random layer\n"
       << "[t] (shift: x10) run perf test of large number of region allocs and frees in a loop\n"
       << "\nShownAllocatedRegionCount = " << m_regions[m_current_layer].size()
       << "\nAllocatedRegionCount = " << total
       << "\n";

  encoder.transformation(astral::Transformation());
  set_and_draw_hud(encoder, frame_ms, *m_text_item, ostr.str());
}

int
main(int argc, char **argv)
{
  RegionAllocatorTest M;
  return M.main(argc, argv);
}

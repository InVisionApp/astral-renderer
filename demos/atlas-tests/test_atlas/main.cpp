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
#include <fstream>

#include <astral/util/ostream_utility.hpp>
#include <astral/util/layered_rect_atlas.hpp>

#include "simple_time.hpp"
#include "command_line_list.hpp"

class TestAtlas:public command_line_register
{
public:
  TestAtlas(void);

  int
  main(int argc, char **argv);

private:
  enum sort_t
    {
      unsorted,
      sort_by_area,
      sort_by_width,
      sort_by_height,
      sort_by_perimiter,
      sort_random
    };

  enum run_t
    {
      run_use_atlas_clear,
      run_delete_individual_rects,
    };

  class PerRect
  {
  public:
    PerRect(astral::ivec2 dims):
      m_dims(dims),
      m_area(dims.x() * dims.y())
    {
      ASTRALassert(dims.x() > 0);
      ASTRALassert(dims.y() > 0);
    }

    astral::LayeredRectAtlas::Entry m_entry;
    astral::ivec2 m_dims;
    int m_area;
  };

  class SortByArea
  {
  public:
    explicit
    SortByArea(bool bad_ordering):
      m_bad_ordering(bad_ordering)
    {}

    bool
    operator()(const PerRect &lhs, const PerRect &rhs) const
    {
      return m_bad_ordering ?
        (lhs.m_area < rhs.m_area):
        (lhs.m_area > rhs.m_area);
    }

  private:
    bool m_bad_ordering;
  };

  class SortByWidth
  {
  public:
    explicit
    SortByWidth(bool bad_ordering):
      m_bad_ordering(bad_ordering)
    {}

    bool
    operator()(const PerRect &lhs, const PerRect &rhs) const
    {
      return m_bad_ordering ?
        (lhs.m_dims.x() > rhs.m_dims.x()):
        (lhs.m_dims.x() > rhs.m_dims.x());
    }

  private:
    bool m_bad_ordering;
  };

  class SortByHeight
  {
  public:
    explicit
    SortByHeight(bool bad_ordering):
      m_bad_ordering(bad_ordering)
    {}

    bool
    operator()(const PerRect &lhs, const PerRect &rhs) const
    {
      return m_bad_ordering ?
        (lhs.m_dims.y() < rhs.m_dims.y()):
        (lhs.m_dims.y() > rhs.m_dims.y());
    }

  private:
    bool m_bad_ordering;
  };

  class SortByPerimiter
  {
  public:
    explicit
    SortByPerimiter(bool bad_ordering):
      m_bad_ordering(bad_ordering)
    {}

    bool
    operator()(const PerRect &lhs, const PerRect &rhs) const
    {
      return m_bad_ordering ?
        (lhs.m_dims.x() + lhs.m_dims.y() < rhs.m_dims.x() + rhs.m_dims.y()):
        (lhs.m_dims.x() + lhs.m_dims.y() > rhs.m_dims.x() + rhs.m_dims.y());
    }

  private:
    bool m_bad_ordering;
  };

  void
  create_rects(void);

  void
  do_run(void);

  astral::ivec2
  random_size(void)
  {
    astral::ivec2 v;

    v.x() = rand() % m_rect_atlas_dims.x() + 1;
    v.y() = rand() % m_rect_atlas_dims.x() + 1;

    return v;
  }

  command_line_argument_value<int> m_init_num_layers;
  command_line_argument_value<unsigned int> m_layer_dims;
  command_line_argument_value<unsigned int> m_number_rects;
  command_line_argument_value<unsigned int> m_number_runs;
  enumerated_command_line_argument_value<enum run_t> m_run_type;
  command_line_argument_value<std::string> m_read_rect_file;
  command_line_argument_value<std::string> m_write_rect_file;
  enumerated_command_line_argument_value<enum sort_t> m_sort_rects;
  command_line_argument_value<bool> m_bad_sort_order;

  astral::ivec2 m_rect_atlas_dims;
  int m_rect_atlas_num_layers;
  astral::reference_counted_ptr<astral::LayeredRectAtlas> m_rect_atlas;
  std::vector<PerRect> m_rects;
};

//////////////////////////////////
// TestImageAtlas methods
TestAtlas::
TestAtlas(void):
  m_init_num_layers(0, "init_num_layers", "Initial number of layers in the atlas", *this),
  m_layer_dims(11, "layer_size", "Width and height of each layer in the atlas", *this),
  m_number_rects(1000, "num_rects", "Number of rects with which to stress the atlas", *this),
  m_number_runs(10, "num_runs", "Number of times to do a run with the rects", *this),
  m_run_type(run_use_atlas_clear,
             enumerated_string_type<enum run_t>()
             .add_entry("use_atlas_clear", run_use_atlas_clear, "Use LayeredRectAtlas::clear() between runs")
             .add_entry("delete_individual_rects", run_delete_individual_rects, "Delete the individual rects between runs"),
             "run_type",
             "Specify how to clear the atlas between runs",
             *this),
  m_read_rect_file("", "read_rect_file", "If a valid filename, read the rect sizes from that file", *this),
  m_write_rect_file("", "write_rect_file", "If non-empty write rect sizes to file", *this),
  m_sort_rects(unsorted,
                enumerated_string_type<enum sort_t>()
                .add_entry("unsorted", unsorted, "do not sort the images")
                .add_entry("area", sort_by_area, "sort images in decreasing order of area")
                .add_entry("width", sort_by_width, "sort images in decreasing order of width")
                .add_entry("height", sort_by_height, "sort images in decreasing order of height")
                .add_entry("perimiter", sort_by_perimiter, "sort images in decreasing order of perimiter")
                .add_entry("random", sort_random, "place images into random order"),
                "sort_rects",
                "Specifies if and how rects are sorted before being added to the atlas",
                *this),
  m_bad_sort_order(false, "bad_sort_order",
                   "If true sort as according to sort_images "
                   "but place in increasing order, gives worse "
                   "packing into atlas", *this)
{
  m_rect_atlas = astral::LayeredRectAtlas::create();
}

void
TestAtlas::
create_rects(void)
{
  std::ifstream read_file;
  std::ofstream write_file;

  if (m_read_rect_file.value() != "")
    {
      read_file.open(m_read_rect_file.value().c_str());
    }

  if (m_write_rect_file.value() != "")
    {
      write_file.open(m_write_rect_file.value().c_str());
    }

  m_rects.reserve(m_number_rects.value());
  if (read_file && m_read_rect_file.value() != "")
    {
      while (read_file && m_rects.size() < m_number_rects.value())
        {
          astral::ivec2 v;
          read_file >> v.x() >> v.y();
          if (!read_file.fail())
            {
              m_rects.push_back(PerRect(v));
            }
        }

      if (m_number_rects.value() > 0 && m_rects.empty())
        {
          m_rects.push_back(PerRect(random_size()));
        }

      for (unsigned int src = 0; m_rects.size() < m_number_rects.value(); ++src)
        {
          m_rects.push_back(m_rects[src]);
        }
    }
  else
    {
      for (unsigned int r = 0; r < m_number_rects.value(); ++r)
        {
          astral::ivec2 v;

          v = random_size();
          m_rects.push_back(PerRect(v));
        }
    }

  switch (m_sort_rects.value())
    {
    case unsorted:
      break;

    case sort_by_area:
      std::sort(m_rects.begin(), m_rects.end(), SortByArea(m_bad_sort_order.value()));
      break;

    case sort_by_width:
      std::sort(m_rects.begin(), m_rects.end(), SortByWidth(m_bad_sort_order.value()));
      break;

    case sort_by_height:
      std::sort(m_rects.begin(), m_rects.end(), SortByHeight(m_bad_sort_order.value()));
      break;

    case sort_by_perimiter:
      std::sort(m_rects.begin(), m_rects.end(), SortByPerimiter(m_bad_sort_order.value()));
      break;

    case sort_random:
      {
        std::vector<PerRect> tmp;

        srand(0);
        tmp.swap(m_rects);
        m_rects.reserve(tmp.size());
        while (!tmp.empty())
          {
            int r;

            r = rand() % tmp.size();
            std::swap(tmp[r], tmp.back());
            m_rects.push_back(tmp.back());
            tmp.pop_back();
          }
      }
      break;
    }

  if (write_file && m_write_rect_file.value() != "")
    {
      for (const auto &r : m_rects)
        {
          write_file << r.m_dims.x() << " " << r.m_dims.y() << "\n";
        }
    }
}

void
TestAtlas::
do_run(void)
{
  m_rect_atlas->clear(m_rect_atlas_dims, m_rect_atlas_num_layers);

  for (auto &r : m_rects)
    {
      r.m_entry = m_rect_atlas->allocate_rectangle(r.m_dims);
      if (!r.m_entry.valid())
        {
          ++m_rect_atlas_num_layers;
          m_rect_atlas->number_layers(m_rect_atlas_num_layers);
          r.m_entry = m_rect_atlas->allocate_rectangle(r.m_dims);
        }
      ASTRALassert(r.m_entry.valid());
    }

  switch (m_run_type.value())
    {
    case run_use_atlas_clear:
      m_rect_atlas->clear(m_rect_atlas_dims, m_rect_atlas_num_layers);
      break;

    case run_delete_individual_rects:
      {
        for (auto &r : m_rects)
          {
            m_rect_atlas->free_rectangle(r.m_entry);
          }
      }
    }
}

int
TestAtlas::
main(int argc, char **argv)
{
  if (argc == 2 && is_help_request(argv[1]))
    {
      std::cout << "\n\nUsage: " << argv[0];
      print_help(std::cout);
      print_detailed_help(std::cout);
      return 0;
    }

  parse_command_line(argc, argv);
  std::cout << "\n\n" << std::flush;

  m_rect_atlas_dims = astral::ivec2(1u << m_layer_dims.value());
  m_rect_atlas_num_layers = m_init_num_layers.value();
  create_rects();

  simple_time timer;
  for (unsigned int i = 0; i < m_number_runs.value(); ++i)
    {
      do_run();
    }

  std::cout << "Took " << timer.elapsed()
            << " ms\n";
  return 0;
}

int
main(int argc, char **argv)
{
  TestAtlas M;
  return M.main(argc, argv);
}

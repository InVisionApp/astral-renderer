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
#include <algorithm>
#include <SDL.h>
#include <astral/util/interval_allocator.hpp>
#include <astral/util/astral_memory.hpp>
#include <astral/util/ostream_utility.hpp>

#include "generic_command_line.hpp"

class Entry
{
public:
  Entry(const astral::IntervalAllocator::Interval *p, unsigned int L):
    m_p(p),
    m_location(L)
  {}

  const astral::IntervalAllocator::Interval *m_p;
  unsigned int m_location;
};

class Test:public command_line_register
{
public:
  Test(void):
    m_layer_length(4096, "layer_length", "", *this),
    m_initial_number_layers(1, "initial_number_layers", "if negative, then only one layer that is dynamically resizeable", *this),
    m_random_seed(std::mt19937::default_seed, "random_seed", "", *this),
    m_interval_allocator(nullptr)
  {}

  ~Test(void)
  {
    for (Entry *p : m_allocations)
      {
        ASTRALdelete(p);
      }
    if (m_interval_allocator)
      {
        ASTRALdelete(m_interval_allocator);
      }
  }

  void
  run_tests(void);

private:
  void
  allocate(int size);

  void
  release(unsigned int entry);

  void
  release_random(void);

  command_line_argument_value<int> m_layer_length;
  command_line_argument_value<int> m_initial_number_layers;
  command_line_argument_value<int> m_random_seed;

  std::mt19937 m_generator;
  astral::IntervalAllocator *m_interval_allocator;
  std::vector<Entry*> m_allocations;
};

void
Test::
allocate(int size)
{
  const astral::IntervalAllocator::Interval *p;

  std::cout << "Allocate " << size << "\n";
  p = m_interval_allocator->allocate(size);
  if (!p)
    {
      if (m_initial_number_layers.value() <= 0)
        {
          int L;

          std::cout << "Lengthen to allociate size " << size << "\n";

          L = m_interval_allocator->layer_length();
          L = astral::t_max(2 * L, L + size);
          m_interval_allocator->layer_length(L);
        }
      else
        {
          std::cout << "Increase number of layers to allociate size " << size << "\n";
          m_interval_allocator->number_layers(m_interval_allocator->number_layers() + 1);
        }
      p = m_interval_allocator->allocate(size);
    }

  ASTRALassert(p);
  m_interval_allocator->check(p->layer());

  Entry *e;

  e = ASTRALnew Entry(p, m_allocations.size());
  m_allocations.push_back(e);
}

void
Test::
release(unsigned int entry)
{
  ASTRALassert(entry < m_allocations.size());
  Entry *e(m_allocations[entry]);

  if (entry + 1u != m_allocations.size())
    {
      Entry *p;

      p = m_allocations.back();
      p->m_location = entry;
      m_allocations[entry] = p;
    }

  m_allocations.pop_back();
  unsigned int layer(e->m_p->layer());

  std::cout << "Release " << e->m_p->range() << "@" << e->m_p->layer() << "\n";
  m_interval_allocator->release(e->m_p);
  ASTRALdelete(e);

  m_interval_allocator->check(layer);
}

void
Test::
release_random(void)
{
  if (!m_allocations.empty())
    {
      std::uniform_int_distribution<> D(0, m_allocations.size() - 1);
      release(D(m_generator));
    }
}

void
Test::
run_tests(void)
{
  int num_layers;

  m_generator = std::mt19937(m_random_seed.value());

  num_layers = astral::t_max(1, m_initial_number_layers.value());
  m_interval_allocator = ASTRALnew astral::IntervalAllocator(m_layer_length.value(), num_layers);

  char choice;
  do
    {
      std::cout << "'a': allocate fixed size\n"
                << "'r': allocate random size\n"
                << "'d': delete random amount\n"
                << "'c': check\n"
                << "'l': check layer\n"
                << "'q': quit\n" << std::flush;
      std::cin >> choice;
      switch (choice)
        {
        case 'a':
          {
            int size, count;

            std::cout << "size? " << std::flush;
            std::cin >> size;
            std::cout << "count ?" << std::flush;
            std::cin >> count;

            for (int i = 0; i < count; ++i)
              {
                allocate(size);
              }
          }
          break;

        case 'r':
          {
            int count, min_size, max_size;

            std::cout << "count ?" << std::flush;
            std::cin >> count;
            std::cout << "min_size?" << std::flush;
            std::cin >> min_size;
            std::cout << "max_size?" << std::flush;
            std::cin >> max_size;

            if (m_initial_number_layers.value() > 0)
              {
                max_size = astral::t_min(max_size, m_layer_length.value());
                min_size = astral::t_min(min_size, max_size);
              }

            std::uniform_int_distribution<> D(min_size, max_size);
            for (int i = 0; i < count; ++i)
              {
                allocate(D(m_generator));
              }
          }
          break;

        case 'd':
          {
            unsigned int count;

            std::cout << "count ?" << std::flush;
            std::cin >> count;

            for (unsigned int i = 0; i < count; ++i)
              {
                release_random();
              }
          }
          break;

        case 'l':
          {
            unsigned int l, cnt;

            std::cout << "layer ?" << std::flush;
            std::cin >> l;

            cnt = m_interval_allocator->check(l);
            std::cout << cnt << " allocated elements on layer " << l << "\n";
          }
          break;

        case 'c':
          {
            unsigned int cnt;

            cnt = m_interval_allocator->check();
            ASTRALassert(cnt == m_allocations.size());
            std::cout << cnt << " allocated elements total\n";
          }
          break;
        }
    }
  while (choice != 'q');
}


int
main(int argc, char **argv)
{
  Test test;

  if (argc == 2 && test.is_help_request(argv[1]))
    {
      std::cout << "\n\nUsage: " << argv[0];
      test.print_help(std::cout);
      test.print_detailed_help(std::cout);
      return 0;
    }

  std::cout << "\n\nRunning: \"";
  for(int i = 0; i < argc; ++i)
    {
      std::cout << argv[i] << " ";
    }

  test.parse_command_line(argc, argv);
  std::cout << "\n\n" << std::flush;

  test.run_tests();
}

/*!
 * \file interval_allocator.cpp
 * \brief file interval_allocator.cpp
 *
 * Copyright 2020 by InvisionApp.
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

#include <astral/util/interval_allocator.hpp>
#include <astral/util/memory_pool.hpp>
#include <astral/util/ostream_utility.hpp>

class astral::IntervalAllocator::Layer
{
public:
  Layer(void):
    m_head(nullptr),
    m_tail(nullptr)
  {}

  IntervalImplement *m_head, *m_tail;
};

class astral::IntervalAllocator::IntervalImplement:public astral::IntervalAllocator::Interval
{
public:
  enum create_tail_t:uint32_t
    {
      create_tail
    };

  /* Splits parent into two pieces, one for the constructed
   * IntervalImplement of size S and another piece that is
   * added to the free list.
   */
  IntervalImplement(IntervalAllocator &allocator,
                    IntervalImplement *parent,
                    int S);

  /* Creates a free interval that is the entire layer */
  IntervalImplement(IntervalAllocator &allocator, int layer);

  /* create an interval for when the layer length is enlarged */
  IntervalImplement(enum create_tail_t,
                    IntervalAllocator &allocator,
                    IntervalImplement *prev,
                    int range_end);

  void
  on_layer_length_enlarge(IntervalAllocator &allocator, int new_length);

  void
  merge_with_next(IntervalAllocator &allocator);

  void
  merge_with_prev(IntervalAllocator &allocator);

  bool
  prev_is_free(void) const
  {
    return m_prev && m_prev->is_free();
  }

  bool
  next_is_free(void) const
  {
    return m_next && m_next->is_free();
  }

  bool
  is_free(void) const
  {
    return m_free_list_location != -1;
  }

  range_type<int>
  range(void) const
  {
    return m_range;
  }

  int
  size(void) const
  {
    return m_range.difference();
  }

  int
  layer(void) const
  {
    return m_layer;
  }

  void
  free_list_location(int V)
  {
    ASTRALassert(V == -1 || V >= 0);
    m_free_list_location = V;
  }

  int
  free_list_location(void) const
  {
    return m_free_list_location;
  }

  void
  add_to_free_list(IntervalAllocator &allocator);

  void
  remove_from_free_list(IntervalAllocator &allocator);

  const IntervalImplement*
  next(void) const
  {
    return m_next;
  }

  const IntervalImplement*
  prev(void) const
  {
    return m_prev;
  }

private:
  range_type<int> m_range;
  int m_layer;

  /* index into Freelist where is located, if free */
  int m_free_list_location;

  /* neighbors in the Layer were located */
  IntervalImplement *m_prev, *m_next;
};

class astral::IntervalAllocator::MemoryPool:public astral::MemoryPool<IntervalImplement, 4096>
{
};

class astral::IntervalAllocator::Freelist
{
public:
  explicit
  Freelist(int size):
    m_size(size)
  {
  }

  void
  add_interval(IntervalAllocator &allocator, IntervalImplement *p)
  {
    ASTRALassert(p);
    ASTRALassert(p->free_list_location() == -1);
    ASTRALassert(p->size() == m_size);

    if (m_free_elements.empty())
      {
        allocator.m_available_sizes.insert(m_size);
      }
    p->free_list_location(m_free_elements.size());
    m_free_elements.push_back(p);
  }

  void
  remove_interval(IntervalAllocator &allocator, IntervalImplement *p)
  {
    ASTRALassert(p);
    ASTRALassert(p->free_list_location() >= 0);
    ASTRALassert(p->free_list_location() < static_cast<int>(m_free_elements.size()));
    ASTRALassert(p->size() == m_size);

    if (p == m_free_elements.back())
      {
        ASTRALassert(p->free_list_location() == static_cast<int>(m_free_elements.size()) - 1);
        p->free_list_location(-1);
        m_free_elements.pop_back();
      }
    else
      {
        IntervalImplement *q;
        int L;

        L = p->free_list_location();
        q = m_free_elements.back();
        m_free_elements[L] = q;
        q->free_list_location(L);

        p->free_list_location(-1);
        m_free_elements.pop_back();
      }

    if (m_free_elements.empty())
      {
        allocator.m_available_sizes.erase(m_size);
      }
  }

  IntervalImplement*
  back(void)
  {
    ASTRALassert(!m_free_elements.empty());
    return m_free_elements.back();
  }

  void
  clear(void)
  {
    m_free_elements.clear();
  }

private:
  int m_size;
  std::vector<IntervalImplement*> m_free_elements;
};

//////////////////////////////////////////////////
// astral::IntervalAllocator::IntervalImplement methods
inline
void
astral::IntervalAllocator::IntervalImplement::
add_to_free_list(IntervalAllocator &allocator)
{
  ASTRALassert(size() < static_cast<int>(allocator.m_free_list.size()));
  allocator.m_free_list[size()].add_interval(allocator, this);
}

inline
void
astral::IntervalAllocator::IntervalImplement::
remove_from_free_list(IntervalAllocator &allocator)
{
  ASTRALassert(size() < static_cast<int>(allocator.m_free_list.size()));
  allocator.m_free_list[size()].remove_interval(allocator, this);
}

astral::IntervalAllocator::IntervalImplement::
IntervalImplement(IntervalAllocator &allocator,
                  IntervalImplement *parent,
                  int S):
  m_range(-1, -1),
  m_layer(-1),
  m_free_list_location(-1),
  m_prev(nullptr),
  m_next(nullptr)
{
  ASTRALassert(parent);
  ASTRALassert(parent->m_free_list_location >= 0);
  ASTRALassert(parent->size() >= S);
  ASTRALassert(!parent->prev_is_free());
  ASTRALassert(!parent->next_is_free());

  parent->remove_from_free_list(allocator);

  m_range.m_begin = parent->m_range.m_begin;
  m_range.m_end = parent->m_range.m_begin + S;
  m_layer = parent->m_layer;
  m_free_list_location = -1;
  m_prev = parent->m_prev;
  if (m_prev)
    {
      m_prev->m_next = this;
    }
  else
    {
      ASTRALassert(parent == allocator.m_layer[m_layer].m_head);
      allocator.m_layer[m_layer].m_head = this;
    }

  if (S == parent->size())
    {
      m_next = parent->m_next;
      if (m_next)
        {
          m_next->m_prev = this;
        }
      else
        {
          ASTRALassert(parent == allocator.m_layer[m_layer].m_tail);
          allocator.m_layer[m_layer].m_tail = this;
        }
      allocator.m_pool->reclaim(parent);
    }
  else
    {
      /* change parent to be the bugger after this */
      parent->m_range.m_begin = m_range.m_end;
      parent->m_prev = this;
      m_next = parent;
      parent->add_to_free_list(allocator);
    }
}

astral::IntervalAllocator::IntervalImplement::
IntervalImplement(IntervalAllocator &allocator, int layer):
  m_range(0, allocator.m_layer_length),
  m_layer(layer),
  m_free_list_location(-1),
  m_prev(nullptr),
  m_next(nullptr)
{
  add_to_free_list(allocator);
  ASTRALassert(is_free());

  ASTRALassert(allocator.m_layer[layer].m_head == nullptr);
  ASTRALassert(allocator.m_layer[layer].m_tail == nullptr);
  allocator.m_layer[layer].m_head = this;
  allocator.m_layer[layer].m_tail = this;
}

astral::IntervalAllocator::IntervalImplement::
IntervalImplement(enum create_tail_t, IntervalAllocator &allocator,
                  IntervalImplement *prev, int range_end):
  m_range(-1, -1),
  m_layer(-1),
  m_free_list_location(-1),
  m_prev(nullptr),
  m_next(nullptr)
{
  ASTRALassert(prev);

  m_range.m_begin = prev->m_range.m_end;
  m_range.m_end = range_end;
  m_layer = prev->m_layer;
  m_free_list_location = -1;

  prev->m_next = this;
  m_prev = prev;
  m_next = nullptr;

  ASTRALassert(allocator.m_layer[m_layer].m_tail == prev);
  allocator.m_layer[m_layer].m_tail = this;
  add_to_free_list(allocator);
}

void
astral::IntervalAllocator::IntervalImplement::
on_layer_length_enlarge(IntervalAllocator &allocator, int new_length)
{
  ASTRALassert(allocator.m_layer[m_layer].m_tail == this);
  if (is_free())
    {
      remove_from_free_list(allocator);
      m_range.m_end = new_length;
      add_to_free_list(allocator);
    }
  else
    {
      IntervalImplement *p;

      p = allocator.m_pool->create(create_tail, allocator, this, new_length);
      ASTRALassert(m_next == p);
      ASTRALassert(p == allocator.m_layer[m_layer].m_tail);
      ASTRALunused(p);
    }
}

void
astral::IntervalAllocator::IntervalImplement::
merge_with_next(IntervalAllocator &allocator)
{
  ASTRALassert(is_free());
  if (!m_next)
    {
      ASTRALassert(this == allocator.m_layer[m_layer].m_tail);
      return;
    }

  ASTRALassert(next_is_free());
  IntervalImplement *old_next(m_next);

  remove_from_free_list(allocator);
  old_next->remove_from_free_list(allocator);

  m_range.m_end = old_next->m_range.m_end;
  m_next = old_next->m_next;
  if (m_next)
    {
      ASTRALassert(m_next->m_prev == old_next);
      m_next->m_prev = this;
    }
  else
    {
      ASTRALassert(old_next == allocator.m_layer[m_layer].m_tail);
      allocator.m_layer[m_layer].m_tail = this;
    }

  add_to_free_list(allocator);
  allocator.m_pool->reclaim(old_next);
}

void
astral::IntervalAllocator::IntervalImplement::
merge_with_prev(IntervalAllocator &allocator)
{
  ASTRALassert(is_free());
  if (!m_prev)
    {
      ASTRALassert(this == allocator.m_layer[m_layer].m_head);
      return;
    }

  ASTRALassert(prev_is_free());
  IntervalImplement *old_prev(m_prev);

  remove_from_free_list(allocator);
  old_prev->remove_from_free_list(allocator);

  m_range.m_begin = old_prev->m_range.m_begin;
  m_prev = old_prev->m_prev;
  if (m_prev)
    {
      ASTRALassert(m_prev->m_next == old_prev);
      m_prev->m_next = this;
    }
  else
    {
      ASTRALassert(old_prev == allocator.m_layer[m_layer].m_head);
      allocator.m_layer[m_layer].m_head = this;
    }

  add_to_free_list(allocator);
  allocator.m_pool->reclaim(old_prev);
}

//////////////////////////////////////////////////
// astral::IntervalAllocator::Interval methods
astral::range_type<int>
astral::IntervalAllocator::Interval::
range(void) const
{
  return static_cast<const IntervalImplement*>(this)->range();
}

int
astral::IntervalAllocator::Interval::
layer(void) const
{
  return static_cast<const IntervalImplement*>(this)->layer();
}

//////////////////////////////////////////////////
// astral::IntervalAllocator methods
astral::IntervalAllocator::
IntervalAllocator(unsigned int length, unsigned int number_layers):
  m_layer_length(0),
  m_number_layers(0),
  m_number_allocated(0)
{
  m_pool = ASTRALnew MemoryPool();
  clear(length, number_layers);
}

astral::IntervalAllocator::
~IntervalAllocator()
{
  m_pool->clear();
  ASTRALdelete(m_pool);
}

void
astral::IntervalAllocator::
clear(unsigned int new_length, unsigned int new_number_layers)
{
  if (!has_live_intervals()
      && new_length + 1u == m_free_list.size()
      && new_number_layers == m_number_layers)
    {
      return;
    }

  for (auto &free_list : m_free_list)
    {
      free_list.clear();
    }

  m_free_list.reserve(new_length + 1);
  for (unsigned int s = m_free_list.size(); s <= new_length; ++s)
    {
      m_free_list.push_back(Freelist(s));
    }
  ASTRALassert(m_free_list.size() == new_length + 1);

  m_number_allocated = 0;
  m_layer_length = new_length;
  m_number_layers = new_number_layers;
  m_layer.clear();
  m_layer.resize(m_number_layers);

  m_pool->clear();
  m_available_sizes.clear();

  for (unsigned int layer = 0; layer < m_number_layers; ++layer)
    {
      IntervalImplement *q;

      q = m_pool->create(*this, layer);
      ASTRALassert(q->is_free());
      ASTRALunused(q);
    }

  ASTRALassert(m_number_layers == 0 || m_available_sizes.size() == 1);
  ASTRALassert(m_number_layers == 0 || *m_available_sizes.begin() == m_layer_length);
}

void
astral::IntervalAllocator::
number_layers(unsigned int L)
{
  ASTRALassert(L > m_number_layers);

  unsigned int old_size(m_number_layers);

  m_layer.resize(L);
  m_number_layers = L;
  for (; old_size < m_number_layers; ++old_size)
    {
      IntervalImplement *q;

      q = m_pool->create(*this, old_size);
      ASTRALassert(q->is_free());
      ASTRALunused(q);
    }
}

void
astral::IntervalAllocator::
layer_length(int L)
{
  ASTRALassert(L > m_layer_length);

  m_free_list.reserve(L + 1);
  for (int i = m_layer_length + 1; i <= L; ++i)
    {
      m_free_list.push_back(Freelist(i));
    }

  m_layer_length = L;
  for (unsigned int layer = 0; layer < m_number_layers; ++layer)
    {
      m_layer[layer].m_tail->on_layer_length_enlarge(*this, L);
    }
}

const astral::IntervalAllocator::Interval*
astral::IntervalAllocator::
allocate(int size)
{
  std::set<int>::iterator iter;

  ASTRALassert(size > 0);
  iter = m_available_sizes.lower_bound(size);
  if (iter == m_available_sizes.end())
    {
      return nullptr;
    }

  ASTRALassert(*iter >= size);
  ASTRALassert(*iter < static_cast<int>(m_free_list.size()));

  IntervalImplement *q, *return_value;

  /* get an interval that can be split to hold it */
  q = m_free_list[*iter].back();

  /* create an interval by splitting q */
  return_value = m_pool->create(*this, q, size);

  ++m_number_allocated;
  return return_value;
}

void
astral::IntervalAllocator::
release(const Interval *p)
{
  IntervalImplement *pp;

  pp = static_cast<IntervalImplement*>(const_cast<Interval*>(p));
  pp->add_to_free_list(*this);

  if (pp->prev_is_free())
    {
      pp->merge_with_prev(*this);
    }

  if (pp->next_is_free())
    {
      pp->merge_with_next(*this);
    }
  --m_number_allocated;
}

unsigned int
astral::IntervalAllocator::
check(unsigned int layer)
{
  unsigned int return_value(0);

  ASTRALassert(layer < m_layer.size());

  const IntervalImplement *p(m_layer[layer].m_head);
  static const c_string labels[2] = { "used", "free" };

  ASTRALassert(p);
  ASTRALassert(p->range().m_begin == 0);

  std::cout << "Layer(" << layer << ") = {"
            << labels[p->is_free()] << p->range() << ", ";

  if (!p->is_free())
    {
      ++return_value;
    }

  for (; p->next(); p = p->next())
    {
      ASTRALassert(p->next()->prev() == p);
      ASTRALassert(p->next()->range().m_begin == p->range().m_end);
      ASTRALassert(!(p->next()->is_free() && p->is_free()));

      std::cout << labels[p->next()->is_free()] << p->next()->range() << ", ";

      if (!p->next()->is_free())
        {
          ++return_value;
        }
    }
  ASTRALassert(p);
  ASTRALassert(p == m_layer[layer].m_tail);
  ASTRALassert(p->range().m_end == m_layer_length);

  std::cout << "}\n";

  return return_value;
}

unsigned int
astral::IntervalAllocator::
check(void)
{
  unsigned int return_value(0);
  for (unsigned int layer = 0; layer < m_number_layers; ++layer)
    {
      return_value += check(layer);
    }
  return return_value;
}

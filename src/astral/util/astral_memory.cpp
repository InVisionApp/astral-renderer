/*!
 * \file astral_memory.cpp
 * \brief file astral_memory.cpp
 *
 * Adapted from: WRATHNew.cpp and WRATHmemory.cpp of WRATH:
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


#include <map>
#include <string>
#include <iostream>
#include <iomanip>
#include <iosfwd>
#include <cstdlib>
#include <sstream>
#include <mutex>

#include <astral/util/util.hpp>
#include <astral/util/astral_memory.hpp>

#ifdef ASTRAL_DEBUG

namespace
{
  void
  bad_malloc_message(size_t size, const char *file, int line)
  {
    std::cerr << "Allocation at [" << file << "," << line << "] of "
              << size << " bytes failed\n";
  }

  class per_address
  {
  public:
    per_address(const char *f, int l):
      m_file(f),
      m_line(l),
      m_tag(nullptr)
    {}

    const char *m_file;
    int m_line;
    const char *m_tag;
  };

  class address_set_type:
    private std::map<const void*, per_address>
  {
  public:
    address_set_type(void)
    {}

    virtual
    ~address_set_type()
    {
      if (!empty())
        {
          print(std::cerr);
        }
    }

    bool
    present(const void *ptr);

    void
    track(const void *ptr, const char *file, int line);

    void
    untrack(const void *ptr, const char *file, int line);

    bool
    exists(const void *ptr);

    void
    tag(const void *ptr, const char *tag);

    void
    print(std::ostream &ostr);

  private:
    std::mutex m_mutex;
  };

  address_set_type&
  address_set(void)
  {
    static address_set_type retval;
    return retval;
  }

}

///////////////////////////////////////////////
// address_set_type methods
bool
address_set_type::
present(const void *ptr)
{
  bool return_value;

  m_mutex.lock();
  return_value = (find(ptr) != end());
  m_mutex.unlock();
  return return_value;
}

void
address_set_type::
track(const void *ptr, const char *file, int line)
{
  m_mutex.lock();
  insert(value_type(ptr, mapped_type(file, line)));
  m_mutex.unlock();
}

void
address_set_type::
tag(const void *ptr, const char *tag_value)
{
  iterator iter;

  m_mutex.lock();
  iter = find(ptr);
  if (iter != end())
    {
      iter->second.m_tag = tag_value;
    }
  m_mutex.unlock();
}

bool
address_set_type::
exists(const void *ptr)
{
  bool found;
  iterator iter;

  m_mutex.lock();
  found = (find(ptr) != end());
  m_mutex.unlock();

  return found;
}

void
address_set_type::
untrack(const void *ptr, const char *file, int line)
{
  bool found;
  iterator iter;

  m_mutex.lock();

  iter = find(ptr);
  if (iter != end())
    {
      found = true;
      erase(iter);
    }
  else
    {
      found = false;
    }

  m_mutex.unlock();

  if (!found)
    {
      std::cerr << "Deletion from [" << file << ", "  << line
                << "] of untracked @" << ptr << "\n"
                << std::flush;
    }
}

void
address_set_type::
print(std::ostream &ostr)
{
  m_mutex.lock();

  for(const_iterator iter = begin(); iter != end(); ++iter)
    {
      ostr << const_cast<void*>(iter->first) << "[" << iter->second.m_file
           << "," << iter->second.m_line << "]";
      if (iter->second.m_tag)
        {
          ostr << "(" << iter->second.m_tag << ")";
        }
      ostr << "\n";
    }

  m_mutex.unlock();
}

#endif

//////////////////////////////////////////////////////
// astral::memory methods
void
astral::memory::
check_object_exists(const void *ptr, const char *file, int line)
{
  #ifdef ASTRAL_DEBUG
    {
      if (!ptr)
        {
          return;
        }

      if (!address_set().exists(ptr))
        {
          std::cerr << "Untracked object from [" << file << ", "
                    << line << "] @" << ptr << "\n" << std::flush;
        }
    }
  #else
    {
      ASTRALunused(file);
      ASTRALunused(line);
      ASTRALunused(ptr);
    }
  #endif
}

void*
astral::memory::
malloc_implement(size_t size, const char *file, int line)
{
  void *return_value;

  if (size == 0)
    {
      return nullptr;
    }

  return_value = std::malloc(size);

  #ifdef ASTRAL_DEBUG
    {
      if (!return_value)
        {
          bad_malloc_message(size, file, line);
        }
      else
        {
          address_set().track(return_value, file, line);
        }
    }
  #else
    {
      ASTRALunused(file);
      ASTRALunused(line);
    }
  #endif

  return return_value;
}

void*
astral::memory::
calloc_implement(size_t nmemb, size_t size, const char *file, int line)
{
  void *return_value;

  if (nmemb == 0 || size == 0)
    {
      return nullptr;
    }

  return_value = std::calloc(nmemb, size);

  #ifdef ASTRAL_DEBUG
    {
      if (!return_value)
        {
          bad_malloc_message(size * nmemb, file, line);
        }
      else
        {
          address_set().track(return_value, file, line);
        }
    }
  #else
    {
      ASTRALunused(file);
      ASTRALunused(line);
    }
  #endif

  return return_value;
}

void*
astral::memory::
realloc_implement(void *ptr, size_t size, const char *file, int line)
{
  void *return_value;

  if (!ptr)
    {
      return malloc_implement(size, file, line);
    }

  if (size == 0)
    {
      free_implement(ptr, file, line);
      return nullptr;
    }

  #ifdef ASTRAL_DEBUG
    {
      if (!address_set().present(ptr))
        {
          std::cerr << "Realloc from [" << file << ", "  << line
                    << "] of untracked @" << ptr << "\n"
                    << std::flush;
        }
    }
  #else
    {
      ASTRALunused(file);
      ASTRALunused(line);
    }
  #endif

  return_value = std::realloc(ptr, size);

  #ifdef ASTRAL_DEBUG
    {
      if (return_value != ptr)
        {
          address_set().untrack(ptr, file, line);
          address_set().track(return_value, file, line);
        }
    }
  #endif

  return return_value;
}

void
astral::memory::
free_implement(void *ptr, const char *file, int line)
{
  #ifdef ASTRAL_DEBUG
    {
      if (ptr)
        {
          address_set().untrack(ptr, file, line);
        }
    }
  #else
    {
      ASTRALunused(file);
      ASTRALunused(line);
    }
  #endif

  std::free(ptr);
}

void
astral::memory::
tag_object(const void *ptr, const char *tag)
{
  #ifdef ASTRAL_DEBUG
    {
      address_set().tag(ptr, tag);
    }
  #else
    {
      ASTRALunused(ptr);
      ASTRALunused(tag);
    }
  #endif
}

void*
operator new(std::size_t n, const char *file, int line) noexcept
{
  return astral::memory::malloc_implement(n, file, line);
}

void*
operator new[](std::size_t n, const char *file, int line) noexcept
{
  return astral::memory::malloc_implement(n, file, line);
}

void
operator delete(void *ptr, const char *file, int line) noexcept
{
  astral::memory::free_implement(ptr, file, line);
}

void
operator delete[](void *ptr, const char *file, int line) noexcept
{
  astral::memory::free_implement(ptr, file, line);
}

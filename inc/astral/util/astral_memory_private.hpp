/*!
 * \file astral_memory_private.hpp
 * \brief file astral_memory_private.hpp
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


/* Private functions needed for macros in astral_memory.hpp
 */

#ifndef ASTRAL_ASTRAL_MEMORY_PRIVATE_HPP
#define ASTRAL_ASTRAL_MEMORY_PRIVATE_HPP

#include <cstddef>

///@cond

/*!
 * Internal routine used by ASTRALnew, do not use directly.
 */
void*
operator new(std::size_t n, const char *file, int line) noexcept;

/*!
 * Internal routine used by ASTRALnew, do not use directly.
 */
void*
operator new[](std::size_t n, const char *file, int line) noexcept;

/*!
 * Internal routine used by ASTRALnew, do not use directly.
 */
void
operator delete(void *ptr, const char *file, int line) noexcept;

/*!
 * Internal routine used by ASTRALnew, do not use directly.
 */
void
operator delete[](void *ptr, const char *file, int line) noexcept;

namespace astral
{

namespace memory
{
  /*!
   * Private function used by macro ASTRALdelete, do NOT call.
   */
  void
  check_object_exists(const void *ptr, const char *file, int line);

  /*!
   * Tag a memory allocation; tag string is -not copied-
   */
  void
  tag_object(const void *ptr, const char *tag);

  /*!
   * Private function used by macro ASTRALdelete, do NOT call.
   */
  template<typename T>
  void
  call_dtor(T *p)
  {
    p->~T();
  }

  /*!
   * Private function used by macro ASTRALmalloc, do NOT call.
   */
  void*
  malloc_implement(size_t size, const char *file, int line);

  /*!
   * Private function used by macro ASTRALcalloc, do NOT call.
   */
  void*
  calloc_implement(size_t nmemb, size_t size, const char *file, int line);

  /*!
   * Private function used by macro ASTRALrealloc, do NOT call.
   */
  void*
  realloc_implement(void *ptr, size_t size, const char *file, int line);

  /*!
   * Private function used by macro ASTRALfree, do NOT call.
   */
  void
  free_implement(void *ptr, const char *file, int line);

} //namespace memory

///@endcond

} //namespace astral

#endif

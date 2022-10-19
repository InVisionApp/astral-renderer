/*!
 * \file astral_memory.hpp
 * \brief file astral_memory.hpp
 *
 * Adapted from: WRATHNew.hpp and WRATHmemory.hpp of WRATH:
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

#ifndef ASTRAL_ASTRAL_MEMORY_HPP
#define ASTRAL_ASTRAL_MEMORY_HPP

#include <cstdlib>
#include <astral/util/astral_memory_private.hpp>

/*!\addtogroup Utility
 * @{
 */

/*!\def ASTRALnew
 * When creating Astral objects, one must use ASTRALnew instead of new
 * to create objects. For debug build of Astral, allocations with ASTRALnew
 * are tracked and at program exit a list of those objects not deleted by
 * ASTRALdelete are printed with the file and line number of the allocation.
 * For release builds of Astral, allocations are not tracked and std::malloc
 * is used to allocate memory. Do NOT use ASTRALnew for creating arrays
 * (i.e. p = new type[N]) as ASTRALdelete does not handle array deletion.
 */
#define ASTRALnew \
  ::new(__FILE__, __LINE__)

/*!\def ASTRALdelete
 * Use \ref ASTRALdelete to delete objects that were allocated with \ref
 * ASTRALnew. For debug builds of Astral, if the memory was not tracked
 * an error message is emitted.
 * \param ptr address of object to delete, value must be a return value
 *            from ASTRALnew
 */
#define ASTRALdelete(ptr) \
  do {                                                                  \
    astral::memory::check_object_exists(ptr, __FILE__, __LINE__);   \
    astral::memory::call_dtor(ptr);                                 \
    astral::memory::free_implement(ptr, __FILE__, __LINE__);        \
  } while(0)

/*!\def ASTRALmalloc
 * For debug build of Astral, allocations with \ref ASTRALmalloc are tracked and
 * at program exit a list of those objects not deleted by \ref ASTRALfree
 * are printed with the file and line number of the allocation. For release builds
 * of Astral, maps to std::malloc.
 */
#define ASTRALmalloc(size) \
  astral::memory::malloc_implement(size, __FILE__, __LINE__)

/*!\def ASTRALcalloc
 * For debug build of Astral, allocations with \ref ASTRALcalloc are tracked and
 * at program exit a list of those objects not deleted by \ref ASTRALfree
 * are printed with the file and line number of the allocation. For release builds
 * of Astral, maps to std::calloc.
 * \param nmemb number of elements to allocate
 * \param size size of each element in bytes
 */
#define ASTRALcalloc(nmemb, size) \
  astral::memory::calloc_implement(nmemb, size, __FILE__, __LINE__)

/*!\def ASTRALrealloc
 * For debug build of Astral, allocations with \ref ASTRALrealloc are tracked and
 * at program exit a list of those objects not deleted by \ref ASTRALfree
 * are printed with the file and line number of the allocation. For release builds
 * of Astral, maps to std::realloc.
 * \param ptr pointer at which to rellocate
 * \param size new size
 */
#define ASTRALrealloc(ptr, size) \
  astral::memory::realloc_implement(ptr, size, __FILE__, __LINE__)

/*!\def ASTRALfree
 * Use ASTRALfree for objects allocated with ASTRALmalloc,
 * ASTRALrealloc and ASTRALcalloc. For release builds of
 * Astral, maps to std::free.
 * \param ptr address of object to free
 */
#define ASTRALfree(ptr) \
  astral::memory::free_implement(ptr, __FILE__, __LINE__)

/*! @} */

#endif

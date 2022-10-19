/*!
 * \file freetype_lib.cpp
 * \brief file freetype_lib.cpp
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

#include <mutex>
#include <astral/text/freetype_lib.hpp>

/////////////////////////////
// astral::FreetypeLib methods
astral::FreetypeLib::
FreetypeLib(void):
  m_lib(nullptr)
{
  int error_code;

  error_code = FT_Init_FreeType(&m_lib);
  ASTRALassert(error_code == 0);
  ASTRALunused(error_code);
}

astral::FreetypeLib::
~FreetypeLib()
{
  if (m_lib != nullptr)
    {
      FT_Done_FreeType(m_lib);
    }
}

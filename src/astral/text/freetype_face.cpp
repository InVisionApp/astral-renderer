/*!
 * \file freetype_face.cpp
 * \brief file freetype_face.cpp
 *
 * Copyright 2017 by Intel.
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
#include <iostream>
#include <astral/text/freetype_face.hpp>
#include <astral/util/static_resource.hpp>

//////////////////////////////////////////////////
// astral::FreetypeFace::GeneratorBase methods
astral::reference_counted_ptr<astral::FreetypeFace>
astral::FreetypeFace::GeneratorBase::
create_face(reference_counted_ptr<FreetypeLib> lib) const
{
  FT_Face face;
  reference_counted_ptr<FreetypeFace> return_value;

  lib->lock();
  face = create_face_implement(lib->lib());
  lib->unlock();

  if (face != nullptr)
    {
      return_value = FreetypeFace::create(face, lib);
    }
  return return_value;
}

enum astral::return_code
astral::FreetypeFace::GeneratorBase::
check_creation(reference_counted_ptr<FreetypeLib> lib) const
{
  enum return_code R;
  FT_Face f;

  lib->lock();
  f = create_face_implement(lib->lib());
  R = (f) ? routine_success : routine_fail;
  if (f)
    {
      FT_Done_Face(f);
    }
  lib->unlock();

  return R;
}

//////////////////////////////////////////////////
// astral::FreetypeFace::GeneratorFile methods
FT_Face
astral::FreetypeFace::GeneratorFile::
create_face_implement(FT_Library lib) const
{
  FT_Error error_code;
  FT_Face face(nullptr);

  error_code = FT_New_Face(lib, m_filename.c_str(), m_face_index, &face);
  if (error_code != 0 && face != nullptr)
    {
      FT_Done_Face(face);
      face = nullptr;
    }
  return face;
}

/////////////////////////////////////////////////
// astral::FreetypeFace::GeneratorMemory methods
astral::FreetypeFace::GeneratorMemory::
GeneratorMemory(const reference_counted_ptr<const DataBufferBase> &src,
                int face_index):
  m_buffer(src),
  m_face_index(face_index)
{
}

astral::FreetypeFace::GeneratorMemory::
GeneratorMemory(c_string filename, int face_index):
  m_face_index(face_index)
{
  m_buffer = DataBuffer::create(filename);
}

FT_Face
astral::FreetypeFace::GeneratorMemory::
create_face_implement(FT_Library lib) const
{
  c_array<const uint8_t> src;
  FT_Error error_code;
  FT_Face face(nullptr);

  src = m_buffer->data_ro();
  error_code = FT_New_Memory_Face(lib,
                                  static_cast<const FT_Byte*>(src.c_ptr()),
                                  src.size(), m_face_index,
                                  &face);
  if (error_code != 0 && face != nullptr)
    {
      FT_Done_Face(face);
      face = nullptr;
    }
  return face;
}

/////////////////////////////////////////////////
// astral::FreetypeFace::GeneratorResource methods
astral::FreetypeFace::GeneratorResource::
GeneratorResource(c_string resource_name, int face_index):
  m_buffer(fetch_static_resource(resource_name)),
  m_face_index(face_index)
{
}

FT_Face
astral::FreetypeFace::GeneratorResource::
create_face_implement(FT_Library lib) const
{
  c_array<const uint8_t> src;
  FT_Error error_code;
  FT_Face face(nullptr);

  error_code = FT_New_Memory_Face(lib,
                                  static_cast<const FT_Byte*>(m_buffer.c_ptr()),
                                  m_buffer.size(), m_face_index,
                                  &face);
  if (error_code != 0 && face != nullptr)
    {
      FT_Done_Face(face);
      face = nullptr;
    }
  return face;
}

/////////////////////////////
// astral::FreetypeFace methods
enum astral::return_code
astral::FreetypeFace::
load_glyph(uint32_t glyph_code)
{
  FT_Int32 load_flags;
  int error;

  load_flags = FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING
    | FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_TRANSFORM
    | FT_LOAD_LINEAR_DESIGN;

  error = FT_Load_Glyph(face(), glyph_code, load_flags);

  return (error == 0) ?
    routine_success:
    routine_fail;
}

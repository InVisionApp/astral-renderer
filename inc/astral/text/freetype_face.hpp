/*!
 * \file freetype_face.hpp
 * \brief file freetype_face.hpp
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


#ifndef ASTRAL_FREETYPE_FACE_HPP
#define ASTRAL_FREETYPE_FACE_HPP

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include <mutex>
#include <utility>
#include <string>
#include <astral/util/data_buffer.hpp>
#include <astral/text/freetype_lib.hpp>
#include <astral/text/glyph_generator.hpp>

namespace astral
{

/*!\addtogroup Text
 * @{
 */

  /*!\brief
   * A FreetypeFace wraps an FT_Face object of the Freetype library
   * together with a mutex in a reference counted object.
   *
   * The threading model for the Freetype appears to be:
   * - Create an FT_Library object
   * - When creating or releasing FT_Face objects, lock a mutex
   *   around the FT_Library when doing so
   * - If an FT_Face is accessed from multiple threads, the FT_Face
   *   (but not the FT_Library) needs to be mutex locked
   */
  class FreetypeFace:public reference_counted<FreetypeFace>::concurrent
  {
  public:
    /*!\brief
     * A Generator provides an interface to create FreetypeFace
     * objects.
     */
    class GeneratorBase:public reference_counted<GeneratorBase>::concurrent
    {
    public:
      virtual
      ~GeneratorBase()
      {}

      /*!
       * Public interface to create a FreetypeFace object.
       * \param lib FreetypeLib with which to create the FT_Face;
       *            lib cannot be nullptr
       */
      virtual
      reference_counted_ptr<FreetypeFace>
      create_face(reference_counted_ptr<FreetypeLib> lib) const;

      /*!
       * Checks if the GeneratorBase object can create a face
       * (by calling create_face_implement()). Returns routine_fail
       * if the object is unable to create a face.
       * \param lib FreetypeLib with which to test face creation;
       *            lib cannot be nullptr
       */
      enum return_code
      check_creation(reference_counted_ptr<FreetypeLib> lib) const;

      /*!
       * Create an astral::GlyphGenerator whose source is
       * the face values from create_face().
       * \param number_threads number of threads that can access
       *                       the created generator simutaneously.
       * \param lib FreetypeLib with which to create face
       *            objects; lib cannot be nullptr
       */
      reference_counted_ptr<const GlyphGenerator>
      create_glyph_generator(unsigned int number_threads,
                             reference_counted_ptr<FreetypeLib> lib) const;

    protected:
      /*!
       * To be implemented by a derived class to create a
       * FT_Face using a given (and locked by the caller)
       * FT_Library object.
       * \param lib FT_Libray with which to create the FT_Face
       */
      virtual
      FT_Face
      create_face_implement(FT_Library lib) const = 0;
    };

    /*!
     * \brief Implementation of GeneratorBase to create a FreetypeFace
     *        from a face index / file pair via lib Freetype's FT_New_Face
     */
    class GeneratorFile:public GeneratorBase
    {
    public:
      /*!
       * Ctor.
       * \param filename name of file from which to source the created
       *                 FT_Face objects
       * \param face_index face index of file
       */
      static
      reference_counted_ptr<GeneratorFile>
      create(c_string filename, int face_index)
      {
        return ASTRALnew GeneratorFile(filename, face_index);
      }

      ~GeneratorFile()
      {}

    protected:
      virtual
      FT_Face
      create_face_implement(FT_Library lib) const override;

    private:
      GeneratorFile(c_string filename, int face_index):
        m_filename(filename),
        m_face_index(face_index)
      {}

      std::string m_filename;
      int m_face_index;
    };

    /*!
     * \brief Implementation of GeneratorBase to create a FreetypeFace
     *        from a face index / file pair via lib Freetype's
     *        FT_New_Memory_Face
     */
    class GeneratorMemory:public GeneratorBase
    {
    public:
      /*!
       * Ctor.
       * \param src holder of data; modifying the data after creating
       *            a GeneratorMemory that uses it is undefined and
       *            crashing behavior.
       * \param face_index face index of data
       */
      static
      reference_counted_ptr<GeneratorMemory>
      create(const reference_counted_ptr<const DataBufferBase> &src,
             int face_index)
      {
        return ASTRALnew GeneratorMemory(src, face_index);
      }

      /*!
       * Ctor. Provided as a convenience, a DataBuffer object is created
       * from the named file and used as the memory source.
       * \param filename name of file from which to source the created
       *                 FT_Face objects
       * \param face_index face index of file
       */
      static
      reference_counted_ptr<GeneratorMemory>
      create(c_string filename, int face_index)
      {
        return ASTRALnew GeneratorMemory(filename, face_index);
      }

      ~GeneratorMemory()
      {}

    protected:
      virtual
      FT_Face
      create_face_implement(FT_Library lib) const override;

    private:
      GeneratorMemory(const reference_counted_ptr<const DataBufferBase> &src,
                      int face_index);

      GeneratorMemory(c_string filename, int face_index);

      reference_counted_ptr<const DataBufferBase> m_buffer;
      int m_face_index;
    };

    /*!
     * \brief Implementation of GeneratorBase to create a FreetypeFace
     *        from a face index / resource pair via lib Freetype's
     *        FT_New_Memory_Face
     */
    class GeneratorResource:public GeneratorBase
    {
    public:
      /*!
       * Ctor.
       * \param resource_name name of the resource fetched by fetch_static_resource()
       * \param face_index face index of data
       */
      GeneratorResource(c_string resource_name, int face_index);

      ~GeneratorResource()
      {}

    protected:
      virtual
      FT_Face
      create_face_implement(FT_Library lib) const override;

    private:
      c_array<const uint8_t> m_buffer;
      int m_face_index;
    };

    /*!
     * Ctor.
     * \param pFace the underlying FT_Face; the created FreetypeFace
     *        takes ownership of pFace and pFace will be deleted when
     *        the created FreetypeFace is deleted.
     * \param pLib the FreetypeLib that was used to create pFace
     */
    static
    reference_counted_ptr<FreetypeFace>
    create(FT_Face pFace,
           const reference_counted_ptr<FreetypeLib> &pLib)
    {
      return ASTRALnew FreetypeFace(pFace, pLib);
    }

    ~FreetypeFace()
    {
      m_lib->lock();
      FT_Done_Face(m_face);
      m_lib->unlock();
    }

    /*!
     * Returns the FT_Face object about which
     * this object wraps.
     */
    FT_Face
    face(void)
    {
      return m_face;
    }

    /*!
     * Function that directs Freetype to load the
     * named glyph so that the glyph is loaded in
     * EM coordinates; this function does NOT lock
     * the face. If a caller needs to access the
     * same face from multiple threads it needs to
     * lock it via lock()/try_lock().
     */
    enum return_code
    load_glyph(uint32_t glyph_code);

    /*!
     * Returns the FreetypeLib that was used to
     * create the FT_Face face().
     */
    const reference_counted_ptr<FreetypeLib>&
    lib(void) const
    {
      return m_lib;
    }

    /*!
     * Aquire the lock of the mutex used to access/use the FT_Face
     * return by face() safely across multiple threads. This method
     * will block until the lock is aquired.
     */
    void
    lock(void)
    {
      m_mutex.lock();
    }

    /*!
     * Release the lock of the mutex used to access/use the FT_Face
     * return by face() safely across multiple threads.
     */
    void
    unlock(void)
    {
      m_mutex.unlock();
    }

    /*!
     * Try to aquire the lock of the mutex. Return true on success.
     * This method only aquires the lock if another thread does not
     * hold it. In particular it will NOT block if the lock is held
     * by another thread.
     */
    bool
    try_lock(void)
    {
      return m_mutex.try_lock();
    }

  private:
    FreetypeFace(FT_Face pFace,
                 const reference_counted_ptr<FreetypeLib> &pLib):
      m_face(pFace),
      m_lib(pLib)
    {
      ASTRALassert(m_face);
      ASTRALassert(m_lib);
    }

    std::mutex m_mutex;
    FT_Face m_face;
    reference_counted_ptr<FreetypeLib> m_lib;
  };

/*! @} */
}

#endif

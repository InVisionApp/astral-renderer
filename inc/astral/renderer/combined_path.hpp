/*!
 * \file combined_path.hpp
 * \brief file combined_path.hpp
 *
 * Copyright 2019 by InvisionApp.
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

#ifndef ASTRAL_COMBINED_PATH_HPP
#define ASTRAL_COMBINED_PATH_HPP

#include <astral/util/c_array.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/bounding_box.hpp>
#include <astral/path.hpp>
#include <astral/animated_path.hpp>
#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/image.hpp>
#include <astral/util/transformation.hpp>
#include <astral/renderer/render_scale_factor.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  class Renderer;

  /*!
   * \brief
   * astral::CombinedPath represents a combination of paths
   * where paths taken together form a virtual path. The
   * source paths can be either astral::AnimatedPath or
   * astral::Path objects where each such object can be scaled
   * and translated. An astral::CombinedPath is a light weight
   * object that does not copy the source the astral::Path,
   * astral::AnimatedPath or even the transformations applied
   * to them.
   */
  class CombinedPath
  {
  public:
    /*!
     * Typedef to make C++ code slightly more readable
     */
    typedef const Path *ConstPathPtr;

    /*!
     * Typedef to make C++ code slightly more readable
     */
    typedef const AnimatedPath *ConstAnimatedPathPtr;

    /*!
     * Ctor for an empty path set.
     */
    CombinedPath(void)
    {}

    /*!
     * Realize to draw a single \ref Path
     * \param path \ref Path to draw. NOTE: the object must
     *             stay alive until the \ref CombinedPath is no
     *             longer used
     */
    CombinedPath(const Path &path):
      m_paths(&path)
    {}

    /*!
     * Realize to draw a single \ref Path translated
     * and optinally scaled and/or rotated by a matrix.
     * \param path \ref Path to draw. NOTE: the object must
     *             stay alive until the \ref CombinedPath is no
     *             longer used
     * \param translate amount by which to translate the path
     * \param matrix matrix to apply to path
     */
    CombinedPath(const Path &path, vec2 translate,
                 const float2x2 &matrix = float2x2()):
      m_paths(&path),
      m_path_translates(translate),
      m_path_matrices(matrix)
    {}

    /*!
     * Realize to draw a single \ref Path translated
     * and scaled.
     * \param path \ref Path to draw. NOTE: the object must
     *             stay alive until the \ref CombinedPath is no
     *             longer used
     * \param translate amount by which to translate the path
     * \param scale amount by which to scale the path
     */
    CombinedPath(const Path &path, vec2 translate, vec2 scale):
      CombinedPath(path, translate, scale_matix(scale))
    {}

    /*!
     * Realize to draw a single \ref AnimatedPath
     * \param path \ref AnimatedPath to draw. NOTE: the object
     *             must stay alive until the \ref CombinedPath is no
     *             longer used
     * \param t time of animation
     */
    CombinedPath(float t, const AnimatedPath &path):
      m_animated_paths(&path),
      m_ts(t)
    {}

    /*!
     * Realize to draw a single \ref AnimatedPath translated
     * and optinally scaled
     * \param path \ref AnimatedPath to draw. NOTE: the object
     *             must stay alive until the \ref CombinedPath is no
     *             longer used
     * \param t time of animation
     * \param translate amount by which to translate the path
     * \param matrix matrix to apply to path
     */
    CombinedPath(float t, const AnimatedPath &path,
                 vec2 translate, const float2x2 &matrix = float2x2()):
      m_animated_paths(&path),
      m_ts(t),
      m_animated_path_translates(translate),
      m_animated_path_matrices(matrix)
    {}

    /*!
     * Realize to draw a single \ref AnimatedPath translated
     * and optinally scaled
     * \param path \ref AnimatedPath to draw. NOTE: the object
     *             must stay alive until the \ref CombinedPath is no
     *             longer used
     * \param t time of animation
     * \param translate amount by which to translate the path
     * \param scale amount by which to scale the path
     */
    CombinedPath(float t, const AnimatedPath &path,
                 vec2 translate, vec2 scale):
      CombinedPath(t, path, translate, scale_matix(scale))
    {}

    /*!
     * Realize to draw several paths
     * \param paths array of path pointser. Note: the backing of
     *              array (in addition to the \ref Path objects
     *              themselves) must stay alive until the \ref
     *              CombinedPath is no longer used
     * \param translates array of translates, if the size of this
     *                   array is smaller, elements in the path
     *                   array past the size of this array are not
     *                   translated.  Note: the backing of array
     *                   must stay alive until the \ref CombinedPath
     *                   is no longer used
     * \param matrices array of matrices, if the size of this
     *                 array is smaller, elements in the path
     *                 array past the size of this array do not
     *                 have a matrix applied to them.  Note: the
     *                 backing of array must stay alive until the
     *                 \ref CombinedPath is no longer used
     */
    CombinedPath(c_array<const ConstPathPtr> paths,
                 c_array<const vec2> translates = c_array<const vec2>(),
                 c_array<const float2x2> matrices = c_array<const float2x2>()):
      m_paths(paths),
      m_path_translates(translates),
      m_path_matrices(matrices)
    {}

    /*!
     * Realize to draw several animated paths
     * \param t time of animation to apply to each animated path
     * \param paths array of path pointers. Note: the backing of
     *              array (in addition to the \ref Path objects
     *              themselves) must stay alive until the \ref
     *              CombinedPath is no longer used
     * \param translates array of translates, if the size of this
     *                   array is smaller, elements in the path
     *                   array past the size of this array are not
     *                   translated.  Note: the backing of array
     *                   must stay alive until the \ref CombinedPath is no
     *                   longer used
     * \param matrices array of matrices, if the size of this
     *                 array is smaller, elements in the path
     *                 array past the size of this array do not
     *                 have a matrix applied to them.  Note: the
     *                 backing of array must stay alive until the
     *                 \ref CombinedPath is no longer used
     */
    CombinedPath(float t,
                 c_array<const ConstAnimatedPathPtr> paths,
                 c_array<const vec2> translates = c_array<const vec2>(),
                 c_array<const float2x2> matrices = c_array<const float2x2>()):
      m_animated_paths(paths),
      m_ts(t),
      m_animated_path_translates(translates),
      m_animated_path_matrices(matrices)
    {}

    /*!
     * Realize to draw several animated paths
     * \param ts time of animation to apply to each animated path,
     *           if this array is shorted than the arary of path
     *           pointers, all paths past that point are animated
     *           at time t = 0.
     * \param paths array of path pointers. Note: the backing of
     *              array (in addition to the \ref Path objects
     *              themselves) must stay alive until the \ref
     *              CombinedPath is no longer used
     * \param translates array of translates, if the size of this
     *                   array is smaller, elements in the path
     *                   array past the size of this array are not
     *                   translated.  Note: the backing of array
     *                   must stay alive until the \ref CombinedPath is no
     *                   longer used
     * \param matrices array of matrices, if the size of this
     *                 array is smaller, elements in the path
     *                 array past the size of this array do not
     *                 have a matrix applied to them.  Note: the
     *                 backing of array must stay alive until the
     *                 \ref CombinedPath is no longer used
     */
    CombinedPath(c_array<const float> ts,
                 c_array<const ConstAnimatedPathPtr> paths,
                 c_array<const vec2> translates = c_array<const vec2>(),
                 c_array<const float2x2> matrices = c_array<const float2x2>()):
      m_animated_paths(paths),
      m_ts(ts),
      m_animated_path_translates(translates),
      m_animated_path_matrices(matrices)
    {}

    /*!
     * Returns the bounding box necessary to stroke
     * the paths referenced by this \ref CombinedPath.
     * \param stroke_inflate the stroking radius
     * \param join_inflate the amount the joins need for
     *                     stroking; this is the same as
     *                     stroke_inflate for non-miter
     *                     join stroking, otherwise it
     *                     should be the miter-limit times
     *                     stroke_inflate.
     * \param cap_style cap style
     */
    BoundingBox<float>
    compute_bounding_box(float stroke_inflate,
                         float join_inflate,
                         enum cap_t cap_style) const;

    /*!
     * Returns the bounding box necessary to fill
     * the paths referenced by this \ref CombinedPath.
     */
    BoundingBox<float>
    compute_bounding_box(void) const
    {
      return compute_bounding_box(0.0f, 0.0f, cap_flat);
    }

    /*!
     * Returns the array of astral::Path or astral::AnimatedPath
     * pointers of the combined path
     * \tparam T which to get, T must be astral::AnimatedPath or
     *           astral::Path
     */
    template<typename T>
    c_array<const T* const>
    paths(void) const
    {
      return paths(type_tag<T>());
    }

    /*!
     * Returns true if the CombinedPath is empty, i.e. contains
     * no geometry.
     */
    bool
    empty(void) const
    {
      return paths<AnimatedPath>().empty() && paths<Path>().empty();
    }

    /*!
     * Returns a pointer to the translation applied to
     * to the I'th astral::Path or I'th astral::AniamtedPath.
     * A return value of nullptr, indicates that no
     * translation is applied
     * \tparam must be astral::Path or astral::AnimatedPath
     * \param I which path or animated path
     */
    template<typename T>
    const vec2*
    get_translate(unsigned int I) const
    {
      c_array<const vec2> trs(translates(type_tag<T>()));
      return (I < trs.size()) ? &trs[I]: nullptr;
    }

    /*!
     * Returns a pointer to the matrix applied to
     * to the I'th astral::Path or I'th astral::AniamtedPath.
     * A return value of nullptr, indicates that no
     * scale is applied
     * \tparam must be astral::Path or astral::AnimatedPath
     * \param I which path or animated path
     */
    template<typename T>
    const float2x2*
    get_matrix(unsigned int I) const
    {
      c_array<const float2x2> s(matrices(type_tag<T>()));
      return (I < s.size()) ? &s[I]: nullptr;
    }

    /*!
     * Get the animation interpolate
     * \tparam T must be Path or AnimatedPath
     * \param I which animated path if T = AnimatedPath,
     * \returns the animation interpolate for the I'th
     *          AniamtedPath if T = AnimatedPath. If
     *          T = Path, then returns 0.
     */
    template<typename T>
    float
    get_t(unsigned int I) const
    {
      return get_t(type_tag<T>(), I);
    }

  private:
    friend class Renderer;

    template<typename T>
    class c_array_with_value:public c_array<const T>
    {
    public:
      c_array_with_value(void):
        m_from_value(false)
      {}

      c_array_with_value(const c_array<const T> &v):
        c_array<const T>(v),
        m_from_value(false)
      {}

      c_array_with_value(const c_array_with_value &obj):
        c_array<const T>(obj),
        m_value(obj.m_value),
        m_from_value(obj.m_from_value)
      {
        if (m_from_value)
          {
            c_array<const T>::set(&m_value, 1);
          }
      }

      c_array_with_value(const T &v):
        m_value(v),
        m_from_value(true)
      {
        c_array<const T>::set(&m_value, 1);
      }

      c_array_with_value&
      operator=(const c_array_with_value &rhs)
      {
        m_from_value = rhs.m_from_value;
        m_value = rhs.m_value;
        if (m_from_value)
          {
            c_array<const T>::set(&m_value, 1);
          }
        else
          {
            c_array<const T>::set(rhs.c_ptr(), rhs.size());
          }

        return *this;
      }

    private:
      T m_value;
      bool m_from_value;
    };

    static
    void
    add_bb(const BoundingBox<float> &path_rect,
           const vec2 *ptranslate,
           const float2x2 *pmatrix,
           BoundingBox<float> *out_bb);

    c_array<const float2x2>
    matrices(type_tag<Path>) const
    {
      return m_path_matrices;
    }

    c_array<const float2x2>
    matrices(type_tag<AnimatedPath>) const
    {
      return m_animated_path_matrices;
    }

    c_array<const vec2>
    translates(type_tag<Path>) const
    {
      return m_path_translates;
    }

    c_array<const vec2>
    translates(type_tag<AnimatedPath>) const
    {
      return m_animated_path_translates;
    }

    c_array<const Path* const>
    paths(type_tag<Path>) const
    {
      return m_paths;
    }

    c_array<const AnimatedPath* const>
    paths(type_tag<AnimatedPath>) const
    {
      return m_animated_paths;
    }

    float
    get_t(type_tag<Path>, unsigned int) const
    {
      return 0.0f;
    }

    float
    get_t(type_tag<AnimatedPath>, unsigned int I) const
    {
      ASTRALassert(!m_ts.empty());
      return (I < m_ts.size()) ? m_ts[I] : m_ts.back();
    }

    c_array_with_value<ConstPathPtr> m_paths;
    c_array_with_value<vec2> m_path_translates;
    c_array_with_value<float2x2> m_path_matrices;

    c_array_with_value<ConstAnimatedPathPtr> m_animated_paths;
    c_array_with_value<float> m_ts;
    c_array_with_value<vec2> m_animated_path_translates;
    c_array_with_value<float2x2> m_animated_path_matrices;
  };

/*! @} */
}

#endif

/*!
 * \file renderer_cached_transformation.hpp
 * \brief file renderer_cached_transformation.hpp
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

#ifndef ASTRAL_RENDERER_CACHED_TRANSFORMATION_HPP
#define ASTRAL_RENDERER_CACHED_TRANSFORMATION_HPP

#include <astral/renderer/renderer.hpp>
#include "renderer_implement.hpp"

/*!
 * Represents a single Transformation value
 * with derived values made (and cached) on
 * request.
 */
class astral::Renderer::Implement::CachedTransformation
{
public:
  CachedTransformation(void):
    m_matrix_type_ready(true),
    m_inverse_ready(true),
    m_singular_values_ready(true),
    m_singular_values(1.0f, 1.0f),
    m_matrix_type(matrix_diagonal),
    m_last_render_scale_factor(1.0f, 1.0f),
    m_pixel_size(1.0f),
    m_pixel_size_ready(true)
  {}

  CachedTransformation(const Transformation &v):
    CachedTransformation()
  {
    transformation(v);
  }

  /* Generated on demand.
   * \param post_transformation transformation applied on the
   *                            left when generating the value;
   *                            it is the callers responsibility
   *                            to make sure the same value is
   *                            passed awlays.
   */
  RenderValue<Transformation>
  render_value(Renderer &renderer,
               const Transformation *post_transformation = nullptr) const;

  /* also generated on demand */
  vecN<float, 2>
  singular_values(void) const;

  /* also generated on demand; a different value for the argument
   * triggers a recomputation.
   */
  float
  surface_pixel_size_in_logical_coordinates(vec2 render_scale_factor) const;

  enum matrix_type_t
  matrix_type(void) const
  {
    if (!m_matrix_type_ready)
      {
        m_matrix_type = compute_matrix_type(m_transformation.m_matrix);
        m_matrix_type_ready = true;
      }
    return m_matrix_type;
  }

  /* also generated on demand */
  const Transformation&
  inverse(void) const;

  const Transformation&
  transformation(void) const
  {
    return m_transformation;
  }

  void
  transformation(const Transformation &tr)
  {
    reset(true, true);
    m_transformation = tr;
  }

  void
  transformation(RenderValue<Transformation> v);

  void
  transformation_translate(float x, float y)
  {
    reset(false, false);
    m_transformation.m_translate.x() = x;
    m_transformation.m_translate.y() = y;
  }

  void
  transformation_matrix(const float2x2 &v)
  {
    reset(true, true);
    m_transformation.m_matrix = v;
  }

  void
  concat(const Transformation &rhs)
  {
    reset(true, true);
    m_transformation.concat(rhs);
  }

  void
  concat(const float2x2 &rhs)
  {
    reset(true, true);
    m_transformation.concat(rhs);
  }

  void
  translate(float x, float y)
  {
    reset(false, false);
    m_transformation.translate(x, y);
  }

  void
  scale(float sx, float sy)
  {
    /* Note that scaleing affects the singular values,
     * but not the matrix type.
     */
    reset(true, false);
    m_transformation.scale(sx, sy);
  }

  void
  rotate(float angle)
  {
    /* Note that rotating does not affect the singular
     * values, but does affect the matrix type
     */
    reset(false, true);
    m_transformation.rotate(angle);
  }

  /* computes the rendering accuracy on input
   * coordinates of the transformation needed
   * to achieve a given output accuracy
   */
  float
  logical_rendering_accuracy(float output_accuracy) const
  {
    const float thresh(1e-6);
    float v;

    v = t_max(thresh, singular_values()[0]);
    return output_accuracy / v;
  }

  /* computes the rendering accuracy on input
   * coordinates of the transformation needed
   * to achieve a given output accuracy when the
   * transformation is concacted with a scaleing.
   */
  float
  compute_tol(float output_accuracy, const vec2 *scale) const
  {
    float v;

    v = logical_rendering_accuracy(output_accuracy);
    if (scale)
      {
        const float tol_scale(1e-6);
        float m;

        m = t_min(t_abs(scale->x()), t_abs(scale->y()));
        m = t_max(tol_scale, m);
        v /= m;
      }
    return v;
  }

  /* computes the rendering accuracy on input
   * coordinates of the transformation needed
   * to achieve a given output accuracy when the
   * transformation is concacted with a matrix.
   */
  float
  compute_tol(float output_accuracy, const float2x2 *matrix) const
  {
    if (matrix)
      {
        const float thresh(1e-6);
        float2x2 tmp(m_transformation.m_matrix * (*matrix));
        float norm, v;

        norm = compute_singular_values(tmp).x();
        v = t_max(thresh, norm);
        return output_accuracy / v;
      }
    else
      {
        return logical_rendering_accuracy(output_accuracy);
      }
  }

  RenderValue<Transformation>
  create_transformation(Renderer &renderer, const vec2 *ptranslate, const float2x2 *pmatrix,
                        const Transformation *post_transformation = nullptr)
  {
    if (!ptranslate && !pmatrix)
      {
        return render_value(renderer, post_transformation);
      }

    Transformation tr(transformation());
    if (ptranslate)
      {
        tr.translate(*ptranslate);
      }

    if (pmatrix)
      {
        tr.m_matrix = tr.m_matrix * (*pmatrix);
      }

    if (post_transformation)
      {
        tr = *post_transformation * tr;
      }

    return renderer.create_value(tr);
  }

private:
  void
  reset(bool reset_svd, bool reset_type)
  {
    m_inverse_ready = false;
    m_singular_values_ready = m_singular_values_ready && !reset_svd;
    m_pixel_size_ready = m_pixel_size_ready && !reset_svd;
    m_matrix_type_ready = m_matrix_type_ready && !reset_type;
    m_cached_transformation = RenderValue<Transformation>();
  }

  Transformation m_transformation;
  mutable Transformation m_inverse;
  mutable RenderValue<Transformation> m_cached_transformation;
  mutable bool m_matrix_type_ready, m_inverse_ready, m_singular_values_ready;
  mutable vecN<float, 2> m_singular_values;
  mutable enum matrix_type_t m_matrix_type;

  mutable vec2 m_last_render_scale_factor;
  mutable float m_pixel_size;
  mutable bool m_pixel_size_ready;
};

#endif

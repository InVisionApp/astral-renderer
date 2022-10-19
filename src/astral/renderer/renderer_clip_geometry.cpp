/*!
 * \file renderer_clip_geometry.cpp
 * \brief file renderer_clip_geometry.cpp
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

#include <cmath>
#include <astral/util/clip_util.hpp>
#include <astral/util/ostream_utility.hpp>
#include "renderer_clip_geometry.hpp"
#include "renderer_storage.hpp"

/////////////////////////////////////////
// astral::Renderer::Implement::ClipGeometrySimple methods
astral::Renderer::Implement::ClipGeometrySimple
astral::Renderer::Implement::ClipGeometrySimple::
sub_geometry(uvec2 begin, uvec2 end) const
{
  ClipGeometrySimple return_value;

  end.x() = t_min(end.x(), static_cast<unsigned int>(m_image_size.x()));
  end.y() = t_min(end.y(), static_cast<unsigned int>(m_image_size.y()));

  ASTRALassert(begin.x() < end.x());
  ASTRALassert(begin.y() < end.y());

  return_value.m_image_size = ivec2(end - begin);

  /* now we need to compute the new pixel-rect from the sub-image
   * pixels specified. First we specify the rect in image coordinates
   */
  vec2 fbegin(begin), fend(end);
  BoundingBox<float> image_rect(fbegin, fend);

  /* now map it to pixel coordinates */
  return_value.m_pixel_rect = m_image_transformation_pixel.inverse().apply_to_bb(image_rect);

  /* We need the transformation to the sub-image */
  return_value.m_image_transformation_pixel = ScaleTranslate(-fbegin) * m_image_transformation_pixel;

  return return_value;
}

//////////////////////////////////////////
// astral::Renderer::Implement::ClipGeometry methods
astral::Renderer::Implement::ClipGeometry::
ClipGeometry(Backing *backing, ivec2 size):
  m_polygon(backing),
  m_equations(backing),
  m_is_screen_aligned_rect(true)
{
  m_image_transformation_pixel = ScaleTranslate();
  m_image_size = size;

  if (size.x() > 0 && size.y() > 0)
    {
      m_polygon.push_back(backing, vec2(0.0f, 0.0f));
      m_polygon.push_back(backing, vec2(0.0f, size.y()));
      m_polygon.push_back(backing, vec2(size.x(), size.y()));
      m_polygon.push_back(backing, vec2(size.x(), 0.0f));

      /* Recall that a vec3 C represents the clip equation:
       *
       *  x * C.x + y * C.y + C.z >= 0
       *
       * So,
       *
       *  x >= 0      <----> C = (1, 0, 0)
       *  y >= 0      <----> C = (0, 1, 0)
       *  x <= size.x <----> C = (-1, 0, size.x)
       *  y <= size.y <----> C = (0, -1, size.y)
       */
      m_equations.push_back(backing, vec3(1.0f, 0.0f, 0.0f));
      m_equations.push_back(backing, vec3(0.0f, 1.0f, 0.0f));
      m_equations.push_back(backing, vec3(-1.0f, 0.0f, size.x()));
      m_equations.push_back(backing, vec3(0.0f, -1.0f, size.y()));

      m_pixel_rect.union_point(vec2(0, 0));
      m_pixel_rect.union_point(vec2(size.x(), size.y()));
    }
}

astral::Renderer::Implement::ClipGeometry::
ClipGeometry(Backing *backing, const BoundingBox<float> &pixel_rect, vec2 scale_factor):
  m_polygon(backing),
  m_equations(backing),
  m_is_screen_aligned_rect(true)
{
  m_polygon.push_back(backing, pixel_rect.as_rect().point(Rect::minx_miny_corner));
  m_polygon.push_back(backing, pixel_rect.as_rect().point(Rect::minx_maxy_corner));
  m_polygon.push_back(backing, pixel_rect.as_rect().point(Rect::maxx_maxy_corner));
  m_polygon.push_back(backing, pixel_rect.as_rect().point(Rect::maxx_miny_corner));

  /* Recall that a vec3 C represents the clip equation:
   *
   *  x * C.x + y * C.y + C.z >= 0
   *
   * So,
   *
   *  x >= min_x <----> C = (1, 0, -min_x)
   *  y >= min_y <----> C = (0, 1, -min_y)
   *  x <= max_x <----> C = (-1, 0, max_x)
   *  y <= max_y <----> C = (0, -1, max_y)
   */
  m_equations.push_back(backing, vec3(1.0f, 0.0f, -pixel_rect.as_rect().min_x()));
  m_equations.push_back(backing, vec3(0.0f, 1.0f, -pixel_rect.as_rect().min_y()));
  m_equations.push_back(backing, vec3(-1.0f, 0.0f, pixel_rect.as_rect().max_x()));
  m_equations.push_back(backing, vec3(0.0f, -1.0f, pixel_rect.as_rect().max_y()));

  int pixel_padding = 0;
  set_image_transformation_and_rects(pixel_rect, scale_factor, pixel_padding);
}

astral::Renderer::Implement::ClipGeometry::
ClipGeometry(Backing *backing, ivec2 size, Renderer::Implement &renderer, RenderBackend::ClipWindowValue *out_clip_window):
  ClipGeometry(backing, size)
{
  *out_clip_window = renderer.create_clip_window(vec2(0.0f), vec2(size));
}

astral::Renderer::Implement::ClipGeometry::
ClipGeometry(Backing *backing, const Transformation &tr,
             float tr_norm, vec2 scale_factor,
             const RelativeBoundingBox &logical_rect,
             const ClipGeometry &geom, int pixel_padding,
             vec2 translate_geom):
  ClipGeometry(backing, scale_factor,
               geom.compute_intersection(backing, tr, tr_norm, logical_rect, translate_geom),
               pixel_padding)
{
}

astral::Renderer::Implement::ClipGeometry::
ClipGeometry(Backing *backing, vec2 scale_factor,
             Intersection intersection, int pixel_padding):
  m_polygon(backing),
  m_equations(backing),
  m_is_screen_aligned_rect(intersection.m_is_screen_aligned_rect)
{
  if (intersection.m_pts.empty())
    {
      m_pixel_rect.clear();
      m_image_transformation_pixel = ScaleTranslate();
      m_image_size = ivec2(0, 0);
      m_is_screen_aligned_rect = true;
      return;
    }

  BoundingBox<float> bb;

  m_polygon.push_back(backing, intersection.m_pts);
  set_equations_and_bb_from_polygon(backing, &bb);

  m_pixel_rect.clear();
  if (!bb.empty() && bb.as_rect().width() > 0.0f && bb.as_rect().height() > 0.0f)
    {
      set_image_transformation_and_rects(bb, scale_factor, pixel_padding);
    }
  else
    {
      m_image_transformation_pixel.m_scale = vec2(0.0f);
      m_image_transformation_pixel.m_translate = vec2(0.0f);
      m_image_size = ivec2(0, 0);
      m_is_screen_aligned_rect = true;
    }

  ASTRALassert(m_pixel_rect.empty() || !std::isnan(m_pixel_rect.as_rect().m_min_point.x()));
  ASTRALassert(m_pixel_rect.empty() || !std::isnan(m_pixel_rect.as_rect().m_min_point.y()));
  ASTRALassert(m_pixel_rect.empty() || !std::isnan(m_pixel_rect.as_rect().m_max_point.x()));
  ASTRALassert(m_pixel_rect.empty() || !std::isnan(m_pixel_rect.as_rect().m_max_point.y()));
}

void
astral::Renderer::Implement::ClipGeometry::
set_image_transformation_and_rects(const BoundingBox<float> &bb, vec2 scale_factor, int pixel_padding)
{
  vec2 sz, scaled_sz;
  ivec2 iscaled_sz, pp;
  Rect mbb;
  RectT<int> ibb;

  mbb = bb.as_rect();

  /* adjust the bounind rect to staert and end on pixel boundaries */
  ibb.m_min_point = ivec2(mbb.m_min_point);
  ibb.m_max_point = ivec2(mbb.m_max_point);

  if (mbb.m_max_point.x() > ibb.m_max_point.x())
    {
      ++ibb.m_max_point.x();
    }

  if (mbb.m_max_point.y() > ibb.m_max_point.y())
    {
      ++ibb.m_max_point.y();
    }

  mbb.m_min_point = vec2(ibb.m_min_point);
  mbb.m_max_point = vec2(ibb.m_max_point);

  /* then compute the needed image size */
  sz = mbb.size();
  sz.x() = t_max(1.0f, sz.x());
  sz.y() = t_max(1.0f, sz.y());

  scaled_sz = scale_factor * sz;
  iscaled_sz = ivec2(scaled_sz);

  /* Guarantee that it contains the target pixel region;
   * it might be tempting to only do this when pixel_padding
   * is zero. However, the value of iscaled_sz must be
   * non-zero in order for the transformation logic below
   * to work. It also might be tempting to say that is
   * iscaled_sz is 0 then there are no pixels, but there
   * still are sub-pixels because of the scale_factor.
   */
  if (scaled_sz.x() > iscaled_sz.x())
    {
      ++iscaled_sz.x();
    }

  if (scaled_sz.y() > iscaled_sz.y())
    {
      ++iscaled_sz.y();
    }

  /* NOTE: To keep the transformation logic simple, when
   *       rendering to an offscreen buffer whose size
   *       is determined by the bounding box of rendering
   *       to another buffer, we do NOT use the pre-padding
   *       of a Image if it has some; the motivation is
   *       that it is just 2-pixels and small offscreen images
   *       (i.e. no more than 64x64 in each dimension) do
   *       not have padding.
   */
  m_image_size = iscaled_sz + ivec2(2 * pixel_padding);

  /* now compute the transformation that maps
   *
   *    mbb.m_min_point --> P
   *    mbb.m_max_point --> scaled_sz + P
   *
   *  where
   *
   *    P = (pixel_padding, pixel_padding)
   *
   * Recall that a ScaleTranslate represents the
   * transformation
   *
   *   q --> m_image_transformation_pixel.m_scale * q + m_image_transformation_pixel.m_translate
   */
  m_image_transformation_pixel.m_scale = scaled_sz / sz;
  m_image_transformation_pixel.m_translate = vec2(pixel_padding)
    - mbb.m_min_point * m_image_transformation_pixel.m_scale;

  ASTRALassert(!std::isnan(m_image_transformation_pixel.m_scale.x()));
  ASTRALassert(!std::isnan(m_image_transformation_pixel.m_scale.y()));
  ASTRALassert(m_image_transformation_pixel.m_scale.x() != 0.0f);
  ASTRALassert(m_image_transformation_pixel.m_scale.y() != 0.0f);

  ScaleTranslate inv(m_image_transformation_pixel.inverse());

  m_pixel_rect.union_point(inv.apply_to_point(vec2(0.0f, 0.0f)));
  m_pixel_rect.union_point(inv.apply_to_point(vec2(m_image_size)));
}

void
astral::Renderer::Implement::ClipGeometry::
set_equations_and_bb_from_polygon(Backing *backing, BoundingBox<float> *bb)
{
  bb->clear();
  ASTRALassert(m_equations.empty());

  if (!m_polygon.empty())
    {
      vec2 center(0.0f, 0.0f);
      float cnt(m_polygon.size());

      for (const vec2 &p : m_polygon.array(backing))
        {
          center += p;
          bb->union_point(p);
        }
      center /= cnt;

      for (unsigned int i = 0, endi = m_polygon.size(); i < endi; ++i)
        {
          unsigned int next_i(i + 1u);
          vec2 v, n;
          vec3 cl;

          /* derive the clip-equations from the edge
           * p[i], p[i + 1] with the requirement that
           * center is on the correct side.
           *
           * TODO: should we filter polygon to make sure
           *       that no two points are close to avoid
           *       degenerate clip-equations?
           */
          next_i = (next_i == endi) ? 0u : next_i;
          v = m_polygon.element(backing, next_i) - m_polygon.element(backing, i);
          n = vec2(-v.y(), v.x());
          if (dot(center - m_polygon.element(backing, i), n) < 0.0f)
            {
              n = -n;
            }

          /* normalize n so that adjusting cl.z()
           * corresponds to pushing in or pushing
           * out in units of pixel coordinates
           */
          n.normalize();

          cl.x() = n.x();
          cl.y() = n.y();
          cl.z() = -dot(n, m_polygon.element(backing, i));
          m_equations.push_back(backing, cl);
        }
    }
}

astral::Renderer::Implement::ClipGeometry::Intersection
astral::Renderer::Implement::ClipGeometry::
compute_intersection(Backing *backing, const Transformation &tr, float tr_norm,
                     const RelativeBoundingBox &logical_rect,
                     vec2 translate_this) const
{
  Intersection polygon;

  if (logical_rect.m_bb.empty()
      || (logical_rect.m_pixel_bb && logical_rect.m_pixel_bb->empty())
      || (m_pixel_rect.empty() && logical_rect.m_inherit_clipping_of_parent))
    {
      return polygon;
    }

  BoundingBox<float> bb;
  float padding_in_pixel_units;

  if (logical_rect.m_padding > 0.0f)
    {
      /* the padding is in logical coordinates, not pixel coordinates,
       * we need to compute the padding from logical to pixel coordinates.
       * This is not quite correct when the matrix has skew, as it enlarges
       * more than necessary.
       */
      padding_in_pixel_units = tr_norm * logical_rect.m_padding;
    }
  else
    {
      padding_in_pixel_units = 0.0f;
    }

  /* Pad the rect before applying clipping */
  bb = logical_rect.bb_with_padding();

  /* it might be tempting to use a local array of 4 points
   * for the polygon from the rect, but if those points are
   * what is returned via polygon, then they will be values
   * on the stack, ready to be changed by a caller (or later).
   * Thus, we need to use backing on the heap, i.e. coming
   * from the scratch space of backing.
   */
  backing->m_scratch_rect_pts[0] = tr.apply_to_point(vec2(bb.as_rect().min_x(), bb.as_rect().min_y()));
  backing->m_scratch_rect_pts[1] = tr.apply_to_point(vec2(bb.as_rect().min_x(), bb.as_rect().max_y()));
  backing->m_scratch_rect_pts[2] = tr.apply_to_point(vec2(bb.as_rect().max_x(), bb.as_rect().max_y()));
  backing->m_scratch_rect_pts[3] = tr.apply_to_point(vec2(bb.as_rect().max_x(), bb.as_rect().min_y()));

  if (logical_rect.m_inherit_clipping_of_parent)
    {
      backing->m_scratch_eqs.clear();
      backing->m_scratch_eqs.reserve(m_equations.size());
      for (vec3 clip_eq : m_equations.array(backing))
        {
          /* pad the clipping equations */
          clip_eq.z() += padding_in_pixel_units;

          /* translate by translate_this, the derivation is as follows
           * The clip equation gives the set S where
           *
           *  S = { p | p.x * clip_eq.x + p.x * clip_eq.y + clip_eq.z >= 0 }
           *
           * We want the set
           *
           *  T = { p - translate_this | p.x * clip_eq.x + p.x * clip_eq.y + clip_eq.z >= 0 }
           *
           * Letting q = p - translate_this gives that p = q + translate_this which gives
           *
           *  T = { q | q.x * clip_eq.x + q.y * clip_eq.y + clip_eq.z + translate_this.x * clip_eq.x + translate_this.y * clip_eq.y >= 0 }
           *    = { q | q.x * Nclip_eq.x + q.y * Nclip_eq.y + Nclip_eq.z >= 0 }
           *
           * So the new clip equation is given by
           *
           *   Nclip_eq.x = clip_eq.x
           *   Nclip_eq.y = clip_eq.y
           *   Nclip_eq.z = clip_eq.z + translate_this.x * clip_eq.x + translate_this.y * clip_eq.y
           */
          clip_eq.z() += translate_this.x() * clip_eq.x() + translate_this.y() * clip_eq.y();

          backing->m_scratch_eqs.push_back(clip_eq);
        }

      clip_against_planes(make_c_array(backing->m_scratch_eqs), backing->m_scratch_rect_pts,
                          &polygon.m_pts,
                          backing->m_scratch_clip_pts);
    }
  else
    {
      polygon.m_pts = backing->m_scratch_rect_pts;
    }

  if (logical_rect.m_pixel_bb)
    {
      vecN<vec3, 4> pp_eqs;
      Rect pixel_rect;

      /* note that we must also modify the clip-equations of pixel_rect too */
      pixel_rect = logical_rect.m_pixel_bb->as_rect();
      pp_eqs[0] = vec3(1.0f, 0.0f, -pixel_rect.min_x() + translate_this.x() + padding_in_pixel_units);
      pp_eqs[1] = vec3(0.0f, 1.0f, -pixel_rect.min_y() + translate_this.y() + padding_in_pixel_units);
      pp_eqs[2] = vec3(-1.0f, 0.0f, pixel_rect.max_x() - translate_this.x() + padding_in_pixel_units);
      pp_eqs[3] = vec3(1.0f, -1.0f, pixel_rect.max_y() - translate_this.y() + padding_in_pixel_units);

      /* We can't use polygon directly as the input to clip_against_planes
       * because it may point to backing->m_scratch_clip_pts; so stash it into
       * m_scratch_aux_pts temporarily.
       */
      backing->m_scratch_aux_pts.resize(polygon.m_pts.size());
      std::copy(polygon.m_pts.begin(), polygon.m_pts.end(), backing->m_scratch_aux_pts.begin());
      clip_against_planes(pp_eqs, make_c_array(backing->m_scratch_aux_pts), &polygon.m_pts, backing->m_scratch_clip_pts);
    }

  polygon.m_is_screen_aligned_rect = polygon.m_pts.empty()
    || (m_is_screen_aligned_rect && compute_matrix_type(tr.m_matrix) != matrix_generic);

  return polygon;
}

///////////////////////////////////////////////////
// astral::Renderer::Implement::ClipGeometryGroup methods
astral::Renderer::Implement::ClipGeometryGroup::
ClipGeometryGroup(Implement &renderer,
                  const Transformation &tr,
                  float tr_norm, vec2 scale_factor,
                  const RelativeBoundingBox &logical_rect,
                  const ClipGeometryGroup &parent_geom, int pixel_padding,
                  c_array<const TranslateAndPadding> translate_and_paddings)
{
  Intersection &tmp(renderer.m_workroom->m_clip_geometry_intersection);

  ASTRALhard_assert(!translate_and_paddings.empty());
  parent_geom.compute_intersection(*renderer.m_storage,
                                   tr, tr_norm, logical_rect,
                                   translate_and_paddings, &tmp);

  init(renderer, scale_factor, tmp, pixel_padding);
}

void
astral::Renderer::Implement::ClipGeometryGroup::
init(Implement &renderer, vec2 scale_factor,
     const Intersection &intersection,
     int pixel_padding)
{
  std::vector<ClipGeometry> &workroom(renderer.m_workroom->m_clip_geometries);
  BoundingBox<float> bb;

  ASTRALassert(workroom.empty());
  for (unsigned int G = 0, endG = intersection.num_polygon_groups(); G < endG; ++G)
    {
      for (unsigned int P = 0, endP = intersection.number_polygons(G); P < endP; ++P)
        {
          ClipGeometry::Intersection polygon;
          ClipGeometry C;

          polygon.m_pts = intersection.polygon(G, P);
          polygon.m_is_screen_aligned_rect = intersection.polygon_is_screen_aligned_rect(G, P);

          C = renderer.m_storage->create_clip(scale_factor, polygon, pixel_padding);

          if (C.image_size().x() > 0 && C.image_size().y() > 0)
            {
              workroom.push_back(C);
              bb.union_box(C.pixel_rect());
            }
        }
    }

  /* MAYBE: it might be a good idea to just change the created
   *        sub-regions to just be the screen aligned rectangles.
   *        When dealing with such rectangles, then some of the
   *        issues we face may simplify.
   */

  /* TODO: we should be willing to combine sub-regions if the
   *       combining does not induce many more pixels; the motivation
   *       is to reduce the combinatoric explosion of several generations
   *       of multiple translate vectors.
   */
#warning "Should combine sub-geometries to reduce the number of sub-geometries"

  if (workroom.empty())
    {
      /* all the resulting clip-geoemetries are empty, so this will be empty too */
      m_bounding_geometry = ClipGeometry();
      m_sub_clips = range_type<unsigned int>(0, 0);
    }
  else if (workroom.size() == 1)
    {
      /* if there is only one sub-geoemetry, then make that
       * the entire geometry.
       */
      m_bounding_geometry = workroom.back();
      m_sub_clips = range_type<unsigned int>(0, 0);
    }
  else
    {
      /* our pixel pixel geometry is very simple, just the bounding box
       * of the clip regions made above.
       */
      m_bounding_geometry = renderer.m_storage->create_clip(bb, scale_factor);

      /* now save our child clip geometries as well */
      m_sub_clips = renderer.m_storage->create_backed_rects_and_clips(make_c_array(workroom), &m_sub_rects);

      /* compute the sub-rects */
      c_array<BoundingBox<float>> subrects;
      c_array<const ClipGeometry> clips;

      clips = renderer.m_storage->backed_clip_geometries(m_sub_clips);
      subrects = renderer.m_storage->backed_clip_geometry_sub_rects(m_sub_rects);

      ASTRALassert(clips.size() == subrects.size());
      for (unsigned int i = 0, endi = clips.size(); i < endi; ++i)
        {
          BoundingBox<float> pixel_rect;

          /* grab the pixel rect */
          pixel_rect = clips[i].pixel_rect();

          /* map the pixel rect to image coordinates */
          subrects[i] = m_bounding_geometry.image_transformation_pixel().apply_to_bb(pixel_rect);
        }
    }

  /* clear the workroom for other users */
  workroom.clear();
}

void
astral::Renderer::Implement::ClipGeometryGroup::
compute_intersection(Storage &storage,
                     const Transformation &tr, float tr_norm,
                     const RelativeBoundingBox &in_logical_rect,
                     c_array<const TranslateAndPadding> translate_and_paddings,
                     Intersection *dst) const
{
  /* The main use case for this method is where an multiple
   * effects with different translates are to be applied
   * to the same rendered content. The simplest way to implement
   * drawing multiple effects for content contained in a local
   * bounding box B is:
   *
   * for (each effect S)
   *   {
   *      R = begin_layer(S.m_effect, S.m_effect_paramters, B);
   *      render_blurred_content(R.encoder());
   *      end_layer(R);
   *   }
   *
   * The problem with the above approach is that it means the content
   * to get blurred is rendered from scratch for each effect S. What
   * should happen instead is that the content is rendered once to
   * an image and then the each effect S is applied to that image.
   * The main issue is that EffectParameters::m_effect_transformation_logical
   * can be a different value for each S. Recall that begin_layer() is
   * essentially:
   *
   * save_transformation()
   * translate(S.m_effect_paramters.m_effect_transformation_logical);
   * R = encoder_image_relative(B);
   * restore_transformation();
   *
   * which the above captures the clipping against the viewport V.
   *
   * Now we need to have the backing surface large enough to capture
   * all of the region that could be rendered to clipped by V. The
   * derivation is as follows:
   *
   *  Q   = pixel_transformation_logical
   *  G   = region defined by parent_geom.m_bounding_geometry
   *  B   = rect in logical coordinates
   *  t_i = translation of i'th effect, viewed as a mapping
   *
   * Then the box in pixel coordinates that has teh i'th effect applied
   * to is is given by
   *
   *  C_i = Q(t_i(B)) interesect G
   *
   * Define L_i as the region of C_i in current logical coordinates
   *
   *  L_i = inverse(Q * t_i)(C_i)
   *      = B intersect inverse(Q * t_i)(G)
   *
   * Now consider where L_i lands in pixel coordinates:
   *
   * Q(L_i) = Q(B) intersect (Q * inverse(Q * t_i))(G)
   *        = Q(B) intersect (Q * inverse(t_i) * inverset(Q))(G)
   *
   * Now, Q is a Transformation value and is given by
   *
   *  Q(p) = A(p) + b
   *
   * where A is a 2x2 matrix and b is a 2-vector. This gives that
   *
   * inverse(Q)(p) = inverse(A)(p) - inverse(A)(b)
   *
   * Letting H = Q * inverse(t_i) * inverset(Q), we have
   *
   * H(p) = Q(inverse(t_i)(inverse(Q)(p)))
   *      = Q(inverse(A)(p) - inverse(A)(b) - t_i)
   *      = A(inverse(A)(p) - inverse(A)(b) - t_i) + b
   *      = p - A(t_i)
   *
   * Thus,
   *
   * Q(L_i) = Q(B) intersect (G - A(t_i))
   *
   * i.e. the pixel region of L_i is just the pixel region of G translated
   * by -A(t_i).
   */

  dst->clear();
  for (unsigned int src = 0, end_src = translate_and_paddings.size(); src < end_src; ++src)
    {
      vec2 offset;
      RelativeBoundingBox logical_rect(in_logical_rect);

      offset = tr.apply_to_direction(translate_and_paddings[src].m_logical_translate);
      logical_rect.m_padding += translate_and_paddings[src].m_logical_padding;

      for (const ClipGeometry &C : sub_clip_geometries(storage))
        {
          ClipGeometry::Intersection pts;

          pts = C.compute_intersection(&storage.clip_geometry_backing(),
                                       tr, tr_norm, logical_rect,
                                       offset);
          if (!pts.m_pts.empty())
            {
              if (dst->m_polygon_groups.empty() || dst->m_polygon_groups.back().m_source != src)
                {
                  Intersection::PolygonGroup G(src, *dst);
                  dst->m_polygon_groups.push_back(G);
                }

              ASTRALassert(!dst->m_polygon_groups.empty() && dst->m_polygon_groups.back().m_source == src);
              dst->m_polygon_groups.back().add_polygon(pts.m_is_screen_aligned_rect, pts.m_pts, *dst);
            }
        }
    }
}

astral::c_array<const astral::Renderer::Implement::ClipGeometry>
astral::Renderer::Implement::ClipGeometryGroup::
sub_clip_geometries(Storage &storage) const
{
  ASTRALassert(m_sub_clips.difference() == m_sub_rects.difference());
  if (m_sub_clips.m_begin == m_sub_clips.m_end)
    {
      ASTRALassert(m_sub_clips.m_begin == 0);
      return c_array<const ClipGeometry>(&m_bounding_geometry, 1);
    }
  else
    {
      return storage.backed_clip_geometries(m_sub_clips);
    }
}

//////////////////////////////////////////
// astral::Renderer::Implement::ClipGeometryGroup::Token methods
astral::c_array<const astral::BoundingBox<float>>
astral::Renderer::Implement::ClipGeometryGroup::Token::
sub_rects(Storage &storage) const
{
  if (m_begin == m_end)
    {
      ASTRALassert(m_begin == 0);
      return c_array<const BoundingBox<float>>();
    }
  else
    {
      return storage.backed_clip_geometry_sub_rects(*this);
    }
}

astral::Renderer::Implement::ClipGeometryGroup::Token
astral::Renderer::Implement::ClipGeometryGroup::Token::
intersect_against(Storage &storage, const BoundingBox<float> &pixel_rect) const
{
  if (m_begin == m_end)
    {
      /* no subrects, so just clear it as empty */
      return *this;
    }

  /* create an array of subrects whose values are the ones
   * we have intersected against the passed pixel rect
   */
  return storage.create_intersected_backed_clip_geometry_rects(*this, pixel_rect);
}

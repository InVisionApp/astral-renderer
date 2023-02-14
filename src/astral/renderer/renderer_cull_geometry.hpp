/*!
 * \file renderer_cull_geometry.hpp
 * \brief file renderer_cull_geometry.hpp
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

#ifndef ASTRAL_RENDERER_CULL_GEOMETRY_HPP
#define ASTRAL_RENDERER_CULL_GEOMETRY_HPP

#include <tuple>
#include <astral/renderer/renderer.hpp>
#include "renderer_implement.hpp"

/*!
 * A CullGeometrySimple represents essentially
 * just the information on the bounding box and
 * transformation values.
 */
class astral::Renderer::Implement::CullGeometrySimple
{
public:
  /* default ctor initializes as CullGeometrySimple
   * representing an empty region.
   */
  CullGeometrySimple(void):
    m_image_size(0, 0)
  {}

  /* Pixel rect of the geometry; includes the padding
   * around specified by pixel_padding().
   */
  const BoundingBox<float>&
  pixel_rect(void) const
  {
    return m_pixel_rect;
  }

  /* returns the smallest RectT<int> that contains pixel_rect() */
  RectT<int>
  ipixel_rect(void) const
  {
    RectT<int> return_value;

    return_value.m_min_point = ivec2(m_pixel_rect.as_rect().m_min_point);
    return_value.m_max_point = ivec2(m_pixel_rect.as_rect().m_max_point);

    if (static_cast<float>(return_value.m_max_point.x()) < m_pixel_rect.as_rect().m_max_point.x())
      {
        ++return_value.m_max_point.x();
      }

    if (static_cast<float>(return_value.m_max_point.y()) < m_pixel_rect.as_rect().m_max_point.y())
      {
        ++return_value.m_max_point.y();
      }

    return return_value;
  }

  /* Size of the offscreen image to which to render,
   * i.e. the same as Image::size().
   */
  ivec2
  image_size(void) const
  {
    return m_image_size;
  }

  /* Scale factor to apply to rendering */
  float
  scale_factor(void) const
  {
    ASTRALassert(m_image_transformation_pixel.m_scale.x() == m_image_transformation_pixel.m_scale.y());
    return m_image_transformation_pixel.m_scale.x();
  }

  /*!
   * Gives the transformation from coordinates
   * of pixel_rect() to coordinate of the
   * astral::Image of size image_size().
   */
  const ScaleTranslate&
  image_transformation_pixel(void) const
  {
    return m_image_transformation_pixel;
  }

  ScaleTranslate
  image_transformation_logical(const ScaleTranslate &pixel_transformation_logical) const
  {
    return m_image_transformation_pixel * pixel_transformation_logical;
  }

  Transformation
  image_transformation_logical(const Transformation &pixel_transformation_logical) const
  {
    return Transformation(m_image_transformation_pixel) * pixel_transformation_logical;
  }

  /* Computes and returns the CullGeometrySimple corresponding to
   * a sub-image of the usual backing image.
   */
  CullGeometrySimple
  sub_geometry(uvec2 begin, uvec2 end) const;

private:
  friend class CullGeometry;

  ivec2 m_image_size;
  BoundingBox<float> m_pixel_rect;
  ScaleTranslate m_image_transformation_pixel;
};

/*!
 * A CullGeometry represents the functionality of
 * tracking the current culling from previous rectangles
 * (including the possibility of rotated rectangles)
 * and clipping a rectangle against that cull to produce
 * smaller region screen-aligned rect that is tighter
 * to the culling polygon.
 */
class astral::Renderer::Implement::CullGeometry:public CullGeometrySimple
{
public:
  /* Backing for the polygon points and the clip
   * equations of a CullGeometry; the expectation
   * is that a single Backing is used; in addition
   * Backing also provides scratch space to compute
   * the clip-planes and points of the polygon.
   */
  class Backing:
    public std::tuple<std::vector<vec2>, std::vector<vec3>>,
    public astral::noncopyable
  {
  public:
    void
    clear(void)
    {
      std::get<0>(*this).clear();
      std::get<1>(*this).clear();
    }

  private:
    friend class CullGeometry;

    vecN<std::vector<vec2>, 2> m_scratch_clip_pts;
    std::vector<vec2> m_scratch_aux_pts;
    std::vector<vec3> m_scratch_eqs;
    vecN<vec2, 4> m_scratch_rect_pts;
  };

  /* Class to codify an intersection of a CullGeometry
   * against a transformed rectangle
   */
  class Intersection
  {
  public:
    Intersection(void):
      m_is_screen_aligned_rect(true)
    {}

    /* Array of points codifying the clipped polygon; the array
     * becomes invalid when CullGeometry::compute_intersection()
     * is called (directly or indirectly).
     */
    c_array<const vec2> m_pts;

    /* If true, the polygon is a screen aligned rect. */
    bool m_is_screen_aligned_rect;
  };

  /* Special ctor to indicate nothing */
  CullGeometry(void):
    m_polygon(nullptr),
    m_equations(nullptr)
  {}

  /* initialize the CullGeometry for rendering to a region
   * of the specified size
   */
  CullGeometry(Backing *backing, ivec2 size);

  /* initialize the CullGeometry for rendering to a region
   * of the specified size. In addition, initalize a
   * RenderBackend::ClipWindowValue for clipping to that region.
   */
  CullGeometry(Backing *backing, ivec2 size, Renderer::Implement &renderer,
               RenderBackend::ClipWindowValue *out_clip_window);

  /* initialize CullGeometry to a specific pixel rect */
  CullGeometry(Backing *backing, const BoundingBox<float> &pixel_rect, float scale_factor);

  /* Initialize the CullGeometry from a given convex polygon,
   * for example as computed by compute_intersection()
   * CullGeometry and a rectangle in logical coordinates.
   * \param backing object that backs the clip-equations and clip-polygon
   * \param scale_factor amount by which to scale rendering
   * \param polygon convex polygon specifying the region
   * \param pixel_padding amount in raw pixels to pad
   */
  CullGeometry(Backing *backing, float scale_factor,
               Intersection intersection, int pixel_padding);

  /* Initialize the CullGeometry as the intersection of another
   * CullGeometry and a rectangle in logical coordinates.
   * \param backing object that backs the clip-equations and clip-polygon
   * \param tr transformation from logical to pixel coordinates
   * \param tr_norm operartor norm of tr which is the largest
   *                singular value of the matrix of tr
   * \param scale_factor amount by which to scale rendering
   * \param logical_rect rect in logical coordinate to clip
   * \param geom CullGeometry from which to inherit clipping
   * \param pixel_padding amount in raw pixels to pad
   * \param translate_geom if non-zero, instead of intersection against
   *                       the region degined by geom, interset against
   *                       the region G = { p - T | p in geom } where
   *                       T is the value of translate_geom
   */
  CullGeometry(Backing *backing, const Transformation &tr,
               float tr_norm, float scale_factor,
               const RelativeBoundingBox &logical_rect,
               const CullGeometry &geom, int pixel_padding,
               vec2 translate_geom = vec2(0.0f, 0.0f));

  /*!
   * Computes the intersection of this CullGeometry against a
   * RelativeBoundingBox.
   * \param backing object that backs the clip-equations and clip-polygon
   * \param tr transformation from logical to pixel coordinates
   * \param tr_norm operator norm of tr
   * \param logical_rect rect to test against in logical coordinates
   * \param translate_geom if non-zero, instead of intersection against
   *                       the region degined by this, interset against
   *                       the region G = { p - T | p in this } where
   *                       T is the value of translate_this
   */
  Intersection
  compute_intersection(Backing *backing, const Transformation &tr, float tr_norm,
                       const RelativeBoundingBox &logical_rect,
                       vec2 translate_this = vec2(0.0f, 0.0f)) const;

  c_array<const vec2>
  polygon(Backing *backing) const
  {
    return m_polygon.array(backing);
  }

  c_array<const vec3>
  equations(Backing *backing) const
  {
    return m_equations.array(backing);
  }

  bool
  is_screen_aligned_rect(void) const
  {
    return m_is_screen_aligned_rect;
  }

private:
  template<std::size_t N, typename T>
  class VirtualArray:public range_type<unsigned int>
  {
  public:
    explicit
    VirtualArray(Backing *backing)
    {
      unsigned int v;

      v = backing ? std::get<N>(*backing).size() : 0u;
      m_begin = m_end = v;
    }

    bool
    empty(void) const
    {
      ASTRALassert(m_begin <= m_end);
      return m_begin == m_end;
    }

    unsigned int
    size(void) const
    {
      ASTRALassert(m_begin <= m_end);
      return m_end - m_begin;
    }

    T&
    element(Backing *backing, unsigned int I)
    {
      ASTRALassert(m_end <= std::get<N>(*backing).size());

      I += m_begin;
      ASTRALassert(I < m_end);

      return std::get<N>(*backing)[I];
    }

    const T&
    element(Backing *backing, unsigned int I) const
    {
      ASTRALassert(m_end <= std::get<N>(*backing).size());

      I += m_begin;
      ASTRALassert(I < m_end);

      return std::get<N>(*backing)[I];
    }

    c_array<T>
    array(Backing *backing)
    {
      c_array<T> R;

      R = make_c_array(std::get<N>(*backing));
      return R.sub_array(*this);
    }

    c_array<const T>
    array(Backing *backing) const
    {
      c_array<T> R;

      R = make_c_array(std::get<N>(*backing));
      return R.sub_array(*this);
    }

    void
    push_back(Backing *backing, const T &value)
    {
      ASTRALassert(m_end == std::get<N>(*backing).size());
      std::get<N>(*backing).push_back(value);
      ++m_end;
    }

    void
    push_back(Backing *backing, c_array<const T> values)
    {
      ASTRALassert(m_end == std::get<N>(*backing).size());
      ASTRALassert(!values.overlapping_memory(make_c_array(std::get<N>(*backing))));

      std::get<N>(*backing).resize(m_end + values.size());
      std::copy(values.begin(), values.end(), std::get<N>(*backing).begin() + m_end);
      m_end = std::get<N>(*backing).size();
    }

    void
    drop_from_backing(Backing *backing)
    {
      ASTRALassert(m_end == std::get<N>(*backing).size());
      std::get<N>(*backing).resize(m_begin);
      m_end = m_begin;
    }
  };

  typedef VirtualArray<0, vec2> VirtualArrayPolygon;
  typedef VirtualArray<1, vec3> VirtualArrayEquation;

  void
  set_equations_and_bb_from_polygon(Backing *backing, BoundingBox<float> *bb);

  void
  set_image_transformation_and_rects(const BoundingBox<float> &in_pixel_rect,
                                     float scale_factor, int pixel_padding);

  VirtualArrayPolygon m_polygon;
  VirtualArrayEquation m_equations;
  bool m_is_screen_aligned_rect;
};

/* A CullGeometryGroup represents an array of CullGeometry values.
 * These are necessary when the VirtualBuffer backing area is a
 * collection of convex regions; this happens when handling an array
 * of Effect objects at once.
 */
class astral::Renderer::Implement::CullGeometryGroup
{
public:
  /* Class to encapsulate a translate and padding.
   * The name is silly.
   */
  class TranslateAndPadding
  {
  public:
    TranslateAndPadding(const vec2 &translate = vec2(0.0f, 0.0f),
                        float padding = 0.0f):
      m_logical_translate(translate),
      m_logical_padding(padding)
    {}

    /* Translate in logical coordinates to apply to the
     * logical rect.
     */
    vec2 m_logical_translate;

    /* Padding in logical coordinate to apply to the
     * logical rect; this value is added to
     * RelativeBoundingBox::m_padding
     */
    float m_logical_padding;
  };

  /* Class to store an Intersection of a CullGeometryGroup
   * against a RelativeBoundingBox. This is an expensive
   * object whose instances should be reused.
   */
  class Intersection:astral::noncopyable
  {
  public:
    /* Returns the number of polygon groups; a single
     * polygon group represents the intersection of
     * a translated RelativeBoundingBox against a
     * CullGeometryGroup
     */
    unsigned int
    num_polygon_groups(void) const
    {
      return m_polygon_groups.size();
    }

    /* Returns the index into the translates argument
     * of CullGeometryGroup::compute_intersection()
     * that the named polygon group sources from.
     */
    unsigned int
    polygon_group_source(unsigned int G) const
    {
      return polygon_group_object(G).m_source;
    }

    /* Returns the number of polygons for a single
     * group; Any polygon group is gauarnteed to
     * be non-empty
     * \param G which polygon group with 0 <= G < num_polygon_groups()
     */
    unsigned int
    number_polygons(unsigned int G) const
    {
      return polygon_group_object(G).m_polygons.difference();
    }

    /* Returns the named polygon of the named group
     * \param G which polygon group with 0 <= G < num_polygon_groups()
     * \param P which polygon with 0 <= P < number_polygons(G)
     */
    c_array<const vec2>
    polygon(unsigned int G, unsigned int P) const
    {
      const Polygon &poly(polygon_object(G, P));
      return make_c_array(m_backing_pts).sub_array(poly.m_pts);
    }

    /* Returns true if the polygon is a screen aligned rect */
    bool
    polygon_is_screen_aligned_rect(unsigned int G, unsigned int P) const
    {
      const Polygon &poly(polygon_object(G, P));
      return poly.m_is_screen_aligned_rect;
    }

    /* Clear the Intersection object */
    void
    clear(void)
    {
      m_backing_pts.clear();
      m_polygons.clear();
      m_polygon_groups.clear();
    }

  private:
    friend class CullGeometryGroup;

    class Polygon
    {
    public:
      /* range into m_backing_pts of points of the polygon */
      range_type<unsigned int> m_pts;

      /* true if the polygon is actually a screen aligned rect */
      bool m_is_screen_aligned_rect;
    };

    class PolygonGroup
    {
    public:
      PolygonGroup(unsigned int src, Intersection &backing):
        m_source(src),
        m_polygons(backing.m_polygons.size(), backing.m_polygons.size())
      {}

      void
      add_polygon(bool is_screen_aligned, c_array<const vec2> polygon, Intersection &backing)
      {
        Polygon R;

        ASTRALassert(!polygon.overlapping_memory(make_c_array(backing.m_backing_pts)));

        R.m_is_screen_aligned_rect = is_screen_aligned;

        /* copy the points into backing.m_pts */
        R.m_pts.m_begin = backing.m_backing_pts.size();
        backing.m_backing_pts.resize(R.m_pts.m_begin + polygon.size());
        std::copy(polygon.begin(), polygon.end(), backing.m_backing_pts.begin() + R.m_pts.m_begin);
        R.m_pts.m_end = backing.m_backing_pts.size();

        /* save the range where the added polygon resides */
        ASTRALassert(m_polygons.m_end == backing.m_polygons.size());
        backing.m_polygons.push_back(R);
        m_polygons.m_end = backing.m_polygons.size();
      }

      unsigned int m_source;

      /* range into m_polygons */
      range_type<unsigned int> m_polygons;
    };

    const PolygonGroup&
    polygon_group_object(unsigned int G) const
    {
      ASTRALassert(G < m_polygon_groups.size());
      return m_polygon_groups[G];
    }

    const Polygon&
    polygon_object(unsigned int G, unsigned int P) const
    {
      const PolygonGroup &group(polygon_group_object(G));

      P += group.m_polygons.m_begin;
      ASTRALassert(P < m_polygons.size());
      ASTRALassert(P < group.m_polygons.m_end);

      return m_polygons[P];
    }

    /* backing for all geometric data */
    std::vector<vec2> m_backing_pts;

    /* each polygon is a range into m_pts */
    std::vector<Polygon> m_polygons;

    /* A PolygronGroup represents the intersection of
     * a single translated RelativeBoundingBox against
     * a CullGeometryGroup
     */
    std::vector<PolygonGroup> m_polygon_groups;
  };

  /* An opaque value that holds a value that can be used
   * to fetch the value of CullGeometryGroup::sub_rects().
   */
  class Token:private range_type<unsigned int>
  {
  public:
    /* Default ctor has that the token has refers to
     * an empty array.
     */
    Token(void):
      range_type<unsigned int>(0, 0)
    {}

    c_array<const BoundingBox<float>>
    sub_rects(Storage &storage) const;

    Token
    intersect_against(Storage &storage, const BoundingBox<float> &pixel_rect) const;

  private:
    friend class CullGeometryGroup;

    Token(const range_type<unsigned int> &R):
      range_type<unsigned int>(R)
    {}

    Token(unsigned int b, unsigned int e):
      range_type<unsigned int>(b, e)
    {}
  };

  /* Special ctor to indicate nothing */
  CullGeometryGroup(void):
    m_bounding_geometry(),
    m_sub_clips(0, 0)
  {}

  /* ctor where CullGeometryGroup does not have sub-regions */
  explicit
  CullGeometryGroup(const CullGeometry &v):
    m_bounding_geometry(v),
    m_sub_clips(0, 0)
  {}

  /* ctor where CullGeometryGroup does not have sub-regions */
  CullGeometryGroup(CullGeometry::Backing *backing, ivec2 size):
    m_bounding_geometry(backing, size),
    m_sub_clips(0, 0)
  {}

  /* ctor where CullGeometryGroup does not have sub-regions */
  CullGeometryGroup(CullGeometry::Backing *backing, ivec2 size, Renderer::Implement &renderer,
                    RenderBackend::ClipWindowValue *out_clip_window):
    m_bounding_geometry(backing, size, renderer, out_clip_window),
    m_sub_clips(0, 0)
  {}

  /* ctor where the CullGeometryGroup region is defined by an Intersection
   * \param renderer Renderer::Implement object that runs the show
   * \param scale_factor amount by which to scale rendering
   * \param intersection define the region that the CullGeometryGroup will cover
   * \param pixel_padding amount in raw pixels to pad
   */
  CullGeometryGroup(Implement &renderer, float scale_factor,
                    const Intersection &intersection,
                    int pixel_padding)
  {
    init(renderer, scale_factor, intersection, pixel_padding);
  }

  /* ctor where CullGeometryGroup as the intersection of another
   * CullGeometryGroup and a rectangle instanced multiple times
   * with different translates
   * \param renderer Renderer::Implement object that runs the show
   * \param backing object that backs the clip-equations and clip-polygon
   * \param tr transformation from logical to pixel coordinates
   * \param tr_norm operartor norm of tr which is the largest
   *                singular value of the matrix of tr
   * \param scale_factor amount by which to scale the resulting rectangle
   * \param logical_rect rect in logical coordinate to clip
   * \param geom CullGeometryGroup from which to inherit clipping
   * \param pixel_padding amount in raw pixels to pad
   * \param translates array of translates and paddings to apply to
   *                   logical_rect; cannot be empty
   */
  CullGeometryGroup(Implement &renderer,
                    const Transformation &tr,
                    float tr_norm, float scale_factor,
                    const RelativeBoundingBox &logical_rect,
                    const CullGeometryGroup &geom, int pixel_padding,
                    c_array<const TranslateAndPadding> translate_and_paddings);

  /* ctor where CullGeometryGroup as the intersection of another
   * CullGeometryGroup and a rectangle instanced once.
   * \param renderer Renderer::Implement object that runs the show
   * \param backing object that backs the clip-equations and clip-polygon
   * \param tr transformation from logical to pixel coordinates
   * \param tr_norm operartor norm of tr which is the largest
   *                singular value of the matrix of tr
   * \param scale_factor amount by which to scale the resulting rectangle
   * \param logical_rect rect in logical coordinate to clip
   * \param geom CullGeometryGroup from which to inherit clipping
   * \param pixel_padding amount in raw pixels to pad
   * \param translate translate and padding to apply to logical_rect
   */
  CullGeometryGroup(Implement &renderer,
                    const Transformation &tr,
                    float tr_norm, float scale_factor,
                    const RelativeBoundingBox &logical_rect,
                    const CullGeometryGroup &geom, int pixel_padding,
                    TranslateAndPadding translate_and_padding = TranslateAndPadding()):
    CullGeometryGroup(renderer, tr, tr_norm, scale_factor, logical_rect,
                      geom, pixel_padding,
                      c_array<const TranslateAndPadding>(&translate_and_padding, 1))
  {}

  /* Computes the interscection of this CullGeometryGroup against
   * a sequence of translates and paddings of a RelativeBoundingBox
   */
  void
  compute_intersection(Storage &storage,
                       const Transformation &tr, float tr_norm,
                       const RelativeBoundingBox &logical_rect,
                       c_array<const TranslateAndPadding> translate_and_paddings,
                       Intersection *dst) const;

  /* The ClipGeomtry values that contains the union of
   * all sub-regions; this can cover much more area than
   * the union indiviual sub-regions.
   */
  const CullGeometry&
  bounding_geometry(void) const
  {
    return m_bounding_geometry;
  }

  /* Returns true if there are sub-regions to this CullGeometryGroup;
   * in this case the actual area covered is given by the union of
   * the elements of sub_clip_geometries(). If false, then the region
   * covered is exactly the region returned by bounding_geometry().
   */
  bool
  has_sub_geometries(void) const
  {
    ASTRALassert(m_sub_clips.difference() == m_sub_rects.difference());
    return m_sub_clips.m_begin < m_sub_clips.m_end;
  }

  /* Array whose union specifying the actual region covered;
   * the elements are not necessarily disjoint. In the case
   * that has_sub_geometries() is false, returns a array
   * of length one whose sole element is bounding_geometry().
   */
  c_array<const CullGeometry>
  sub_clip_geometries(Storage &storage) const;

  /* Array of sub-rects in Image coordinates of the
   * Image specified by bounding_geometry() of the
   * pixels convered by the sub_clip_geometries(). In
   * the case that has_sub_geometries() is false,
   * returns an empty array. Note the difference
   * in behavior compared to sub_clip_geometries().
   */
  c_array<const BoundingBox<float>>
  sub_rects(Storage &storage) const
  {
    ASTRALassert(m_sub_clips.difference() == m_sub_rects.difference());
    return m_sub_rects.sub_rects(storage);
  }

  /*!
   * Returns the token value that can be used to fetch the
   * value of sub_rects() without needing this object.
   */
  Token
  token(void) const
  {
    return m_sub_rects;
  }

private:
  void
  init(Implement &renderer, float scale_factor,
       const Intersection &intersection,
       int pixel_padding);

  CullGeometry m_bounding_geometry;
  range_type<unsigned int> m_sub_clips;
  Token m_sub_rects;
};

#endif

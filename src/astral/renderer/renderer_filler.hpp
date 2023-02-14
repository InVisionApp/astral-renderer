/*!
 * \file renderer_filler.hpp
 * \brief file renderer_filler.hpp
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

#ifndef ASTRAL_RENDERER_FILLER_HPP
#define ASTRAL_RENDERER_FILLER_HPP

#include <astral/util/memory_pool.hpp>
#include <astral/renderer/renderer.hpp>
#include <astral/renderer/shader/fill_stc_shader.hpp>
#include "renderer_implement.hpp"
#include "renderer_shared_util.hpp"
#include "renderer_cached_combined_path.hpp"
#include "renderer_stc_data.hpp"


/* This interface essentially represents allowing to have multiple
 * strategies live in the same build for different ways to build
 * a fill mask, including building a mask non-sparsely.
 */
class astral::Renderer::Implement::Filler:public reference_counted<Filler>::non_concurrent
{
public:
  /*!
   * Enumeration to specify the combination mode with
   * a ClipElement
   */
  enum clip_combine_mode_t
    {
      /*!
       * When combining a fill F with a ClipElement C, compute
       * both C intersect F and C \ F (i.e. both clip-in F and
       * clip-out F).
       */
      clip_combine_both,

      /*!
       * When combining a fill F with a ClipElement C, compute
       * only C inversect F (i.e. clip-in F only)
       */
      clip_combine_intersect_only
    };

  /* A TileTypeTable is filled with the status of each tile
   * of an Image comign directly from the fill without the
   * affect combining with a ClipElement.
   */
  class TileTypeTable:astral::noncopyable
  {
  public:
    TileTypeTable(void):
      m_tile_count(0, 0)
    {}

    void
    copy(TileTypeTable *dst) const
    {
      dst->m_tile_count = m_tile_count;
      dst->m_fill_tile_types.resize(m_fill_tile_types.size());
      std::copy(m_fill_tile_types.begin(), m_fill_tile_types.end(), dst->m_fill_tile_types.begin());
    }

    uvec2
    tile_count(void) const
    {
      return m_tile_count;
    }

    enum ImageMipElement::element_type_t
    fill_tile_type(ivec2 p) const
    {
      unsigned int idx;

      idx = compute_index(p);
      return m_fill_tile_types[idx];
    }

    enum ImageMipElement::element_type_t
    fill_tile_type(uvec2 p) const
    {
      unsigned int idx;

      idx = compute_index(p);
      return m_fill_tile_types[idx];
    }

    void
    clear(void)
    {
      m_tile_count = uvec2(0, 0);
      m_fill_tile_types.clear();
    }

    void
    set_size(ivec2 cnt)
    {
      m_tile_count = uvec2(cnt);
      m_fill_tile_types.resize(cnt.x() * cnt.y(), ImageMipElement::number_element_type);
    }

    void
    set_size(uvec2 cnt)
    {
      m_tile_count = cnt;
      m_fill_tile_types.resize(cnt.x() * cnt.y(), ImageMipElement::number_element_type);
    }

    enum ImageMipElement::element_type_t&
    fill_tile_type(ivec2 p)
    {
      unsigned int idx;

      idx = compute_index(p);
      return m_fill_tile_types[idx];
    }

    enum ImageMipElement::element_type_t&
    fill_tile_type(uvec2 p)
    {
      unsigned int idx;

      idx = compute_index(p);
      return m_fill_tile_types[idx];
    }

    void
    fill_tile_type_all(enum ImageMipElement::element_type_t v)
    {
      std::fill(m_fill_tile_types.begin(), m_fill_tile_types.end(), v);
    }

  private:
    int
    compute_index(ivec2 p) const
    {
      ASTRALassert(p.x() >= 0);
      ASTRALassert(p.y() >= 0);
      ASTRALassert(p.x() < static_cast<int>(m_tile_count.x()));
      ASTRALassert(p.y() < static_cast<int>(m_tile_count.y()));
      return p.x() + p.y() * m_tile_count.x();
    }

    unsigned int
    compute_index(uvec2 p) const
    {
      ASTRALassert(p.x() < m_tile_count.x());
      ASTRALassert(p.y() < m_tile_count.y());
      return p.x() + p.y() * m_tile_count.x();
    }

    uvec2 m_tile_count;
    std::vector<enum ImageMipElement::element_type_t> m_fill_tile_types;
  };

  /* Derived class that implement Filler */
  class CommonClipper;
  class CurveClipper;
  class LineClipper;
  class NonSparse;

  explicit
  Filler(Renderer::Implement &renderer):
    m_renderer(renderer)
  {}

  virtual
  ~Filler()
  {}

  /*!
   * Create a Image representing the fill; the returned
   * image will have Image::default_use_prepadding(v)
   * where if v is true if and only if the padding at the
   * start of the mask is in the pre-padding of the returned
   * image.
   * \param tol accuracy tolerance to use when choosing contour LOD,
   *            this value already takes into account the transformation
   *            encoded in image_transformation_logical, but not the
   *            optional transformations in a CombnedPath
   * \param fill_rule the fill rule to apply
   * \param aa_mode specifies if to generate with anti-aliasing
   * \param path CombinedPath to fill
   * \param cull_geometry CullGeometry specifying the bounding region
   *                      of the mask to generate
   * \param image_transformation_logical transformation from CombinedPath
   *                                     coordinates to image coordinates of the
   *                                     astral::Image to be made
   * \param clip_element if non-null, perform combine shading against it
   * \param clip_combine_mode what clip-combining mode to perform if clip_element
   *                          is non-null.
   * \param out_clip_combine_tile_data if non-null, location to which to write
   *                                   the tile-types before the clip-combining
   * \param restrict_bbs if non-empty, tiles outside of the union of the rects can
   *                     be made to be empty; sub_rects are in image coordiantes
   * \param out_data MaskDetails to populate the fields m_pixel_rect,
   *                 m_mask_transformation_logical, m_mask_transformation_pixel,
   *                 m_mask, m_size and m_min_corner.
   */
  void
  create_mask(float tol, enum fill_rule_t fill_rule, enum anti_alias_t aa_mode,
              const CombinedPath &path, const CullGeometrySimple &cull_geometry,
              c_array<const BoundingBox<float>> restrict_bbs,
              const Transformation &image_transformation_logical,
              const ClipElement *clip_element, enum clip_combine_mode_t clip_combine_mode,
              TileTypeTable *out_clip_combine_tile_data, MaskDetails *out_data);
  void
  create_mask(float tol, enum fill_rule_t fill_rule, enum anti_alias_t aa_mode,
              const CombinedPath &path, const CullGeometrySimple &cull_geometry,
              c_array<const BoundingBox<float>> restrict_bbs,
              const Transformation &image_transformation_logical,
              MaskDetails *out_data)
  {
    const ClipElement *clip_element(nullptr);
    TileTypeTable *out_clip_combine_tile_data(nullptr);
    enum clip_combine_mode_t clip_combine_mode(clip_combine_both); //does not matter since clip_element is null

    create_mask(tol, fill_rule, aa_mode, path, cull_geometry,
                restrict_bbs, image_transformation_logical,
                clip_element, clip_combine_mode,
                out_clip_combine_tile_data, out_data);
  }


  /*!
   * Analogous to create_mask(), but instead use a MaskItemPathShader
   * to generate the pixels.
   */
  static
  void
  create_mask_via_item_path_shader(Renderer::Implement &renderer, const MaskItemPathShader &shader,
                                   float tol, enum fill_rule_t fill_rule,
                                   const CombinedPath &path, const CullGeometrySimple &cull_geometry,
                                   const Transformation &image_transformation_logical,
                                   const ClipElement *clip_element,
                                   TileTypeTable *out_clip_combine_tile_data, MaskDetails *out_data);
  static
  void
  create_mask_via_item_path_shader(Renderer::Implement &renderer, const MaskItemPathShader &shader,
                                   float tol, enum fill_rule_t fill_rule,
                                   const CombinedPath &path, const CullGeometrySimple &cull_geometry,
                                   const Transformation &image_transformation_logical,
                                   MaskDetails *out_data)
  {
    const ClipElement *clip_element(nullptr);
    TileTypeTable *out_clip_combine_tile_data(nullptr);

    create_mask_via_item_path_shader(renderer, shader, tol, fill_rule, path, cull_geometry,
                                     image_transformation_logical,
                                     clip_element, out_clip_combine_tile_data, out_data);
  }

  /* For now, nowhere else to stick these functions;
   * when we make a dedicated encoder type class for
   * generating mask-fills they will be static functions
   * in it.
   */
  static
  enum mask_channel_t
  mask_channel_clip_in(enum mask_type_t v)
  {
    return (v == mask_type_distance_field) ?
      mask_channel_green :
      mask_channel_red;
  }

  /* For now, nowhere else to stick these functions;
   * when we make a dedicated encoder type class for
   * generating mask-fills they will be static functions
   * in it.
   */
  static
  enum mask_channel_t
  mask_channel_clip_out(enum mask_type_t v)
  {
    return (v == mask_type_distance_field) ?
      mask_channel_alpha :
      mask_channel_blue;
  }

protected:
  /* To be implemented by a derived class to do the work of
   * creating the mask sparsely. At entry, the fields
   * - m_cached_combined_path
   * - m_fill_rule
   * - m_aa_mode
   * - m_region
   * are set by the caller.
   * An implementation can return null to indicate that it
   * chooses not create the mask sparsely.
   * \param rect_size size of image to create
   * \param restrict_bbs if non-empty, tiles outside of the union
   *                     of these rects can be empty; these rects
   *                     are in the same coordinates as rect_size
   * \param clip_element optional clipping to combine against
   * \param clip_combine_mode how to combine with clip_element if present
   * \param out_clip_combine_tile_data if non-null, location ot which to
   *                                   wtite the tile table
   */
  virtual
  reference_counted_ptr<const Image>
  create_sparse_mask(ivec2 rect_size,
                     c_array<const BoundingBox<float>> restrict_bbs,
                     const CombinedPath &path,
                     const ClipElement *clip_element,
                     enum clip_combine_mode_t clip_combine_mode,
                     TileTypeTable *out_clip_combine_tile_data) = 0;

  /* the renderer that uses this */
  Renderer::Implement &m_renderer;

  /* holder of cached values derived from CombinedPath */
  CachedCombinedPath m_cached_combined_path;

  /* current fill rule */
  enum fill_rule_t m_fill_rule;

  /* curret aa-mode */
  enum anti_alias_t m_aa_mode;

  /* the value of rect_size passed in create_mask()
   * realized as a BoundingBox<float>
   */
  BoundingBox<float> m_region;

private:
  /*!
   * Create a Image where each tile of it is realized
   * \param renderer calling Renderer
   * \param rect_size size of each sub-rect
   * \param path CombinedPath to fill
   */
  reference_counted_ptr<const Image>
  create_mask_non_sparse(ivec2 rect_size,
                         const CombinedPath &path,
                         const ClipElement *clip_element,
                         TileTypeTable *out_clip_combine_tile_data);

  /* Given a RenderEncoderBase used to render a mask non-sparsely,
   * apply the clipping (if any) to the data.
   */
  static
  void
  non_sparse_handle_clipping(RenderEncoderBase im,
                             const ClipElement *clip_element,
                             TileTypeTable *out_clip_combine_tile_data);

  /*!
   * Giventhe the CullGeometrySimple value passed to create_mask(),
   * and the reference_counted_ptr<Image> holdng the mask, compute the
   * MaskDetails value.
   */
  static
  void
  compute_mask_details(const CullGeometrySimple &cull_geometry,
                       const reference_counted_ptr<const Image> &mask_image,
                       MaskDetails *out_data);

  /* STCData builder for adding a combined path */
  STCData::BuilderSet m_builder;
};

#endif

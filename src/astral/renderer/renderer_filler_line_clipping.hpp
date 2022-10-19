/*!
 * \file renderer_filler_line_clipping.hpp
 * \brief file renderer_filler_line_clipping.hpp
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

#ifndef ASTRAL_RENDERER_FILLER_LINE_CLIPPER_HPP
#define ASTRAL_RENDERER_FILLER_LINE_CLIPPER_HPP

#include <astral/util/memory_pool.hpp>
#include <astral/renderer/renderer.hpp>
#include <astral/renderer/shader/fill_stc_shader.hpp>
#include "renderer_shared_util.hpp"
#include "renderer_cached_combined_path.hpp"
#include "renderer_filler.hpp"
#include "renderer_filler_common_clipper.hpp"

/* This Filler, in contrast to FillerCurveClipping, does not clip against
 * curves thus avoiding the numerical trickiness and expense of clipping
 * conic curves. Instead it relies solely on the line-segment-contour and
 * clips that. The complications arise when it needs to adjust the winding
 * numbers for the SubRect values and in addition, it needs to also add
 * back the stencil conic triangle for all those SubRect objects that
 * intersect the stencil conic-triangle.
 *
 * Algorithm overview:
 *  1. Compute a render-target aligned bounding box B of a CombinedPath filled
 *  2. Break B into a grid of sub-rects of size S with padding P. The value of
 *     S is ImageAtlas::tile_size_without_padding and teh value of P is
 *     ImageAtlas::tile_padding. Let w = number of B horizontal and
 *     h = number of B vertical. If min(w, h) < 3 then do not fill sparsely and
 *     exit.
 *  3. For each contour curve C of each contour of the CombinedPath, compute
 *     what rects B the curve C intersects and "light" them. If the number of
 *     all lit rects B is too great compared to the total number of rects B,
 *     then do not sparsely fill and exit.
 *  4. For each contour C. Compute what SubRects C lights. Those SubRects
 *     which C does not light, simply a compute the winding contribution of
 *     C against those sub-rects and add that to the winding offset. Let
 *     L(C) be the line-segment contour (i.e. all conic curves are viewed as
 *     line segments connecting the start and end). For each curve and line
 *     segment M add the anti-aliasing fuzz to the rects it hits. In addition,
 *     for each curve (but not line segment) M add the STC stencil conic pass
 *     data to those SubRects that are lit by the current contour C. Now,
 *     clip L(C) against each SubRect R that is lit by C (we do this in the
 *     same fashion as Filler::CurveClipper with an early out if all are
 *     edge huggers). Clipping L(C) only changes the contents of those R that
 *     are lit by C. It changes it by adding the stencil line segment pass of
 *     L(C) clipped against R. If all of L(C) is edge huggers, we can skip adding
 *     the data and instead add to the winding offset instead.
 *  5. Once Step 4 is done on all contours C, each subrect R will have
 *     the STC data added to it for those contours that hit and the effect
 *     of the winding number of all those contours that do not have curves
 *     hitting it. Those subrects that have no STC data then are completely
 *     filled or unfilled.
 *  7. If the winding rule is nonzero_fill_rule or complement_nonzero_fill_rule,
 *     for those subrects with STC data add abs(winding offset) winding rects
 *     around the sub-rect. If winding offset is positive orient clockwise,
 *     if winding offset is negative orient counter clockwise.
 *  8. If the winding rule is odd_even_fill_rule or complement_odd_even_fill_rule,
 *     for those SubRect with STC data invert the fill rule in the VirtualBuffer
 *     when then winding offset is odd.
 *  9. Using Filler::CommonClipper::create_sparse_image_from_rects() creates
 *     a Image where the tiles that are completely filled become
 *     ImageMipElement::white_element and those thare are completetly
 *     unfilled become ImageMipElement::empty_element tiles. Of key
 *     importance is that each rect is exactly one tile in size when the
 *     padding is included.
 *
 * UGLY TODO: this class has ALOT of code just copied from Filler::CurveClipper
 *            and it would be better if instead they shared the code instead.
 *
 */
class astral::Renderer::Implement::Filler::LineClipper:public Filler::CommonClipper
{
public:
  explicit
  LineClipper(Renderer::Implement &renderer):
    Filler::CommonClipper(renderer)
  {
    /* make sure all counters are set to 0 */
    cleanup();
  }

protected:
  virtual
  reference_counted_ptr<const Image>
  create_sparse_mask(ivec2 rect_size,
                     c_array<const BoundingBox<float>> restrict_bbs,
                     const CombinedPath &path,
                     const ClipElement *clip_element,
                     enum clip_combine_mode_t clip_combine_mode,
                     TileTypeTable *out_clip_combine_tile_data) override;

private:
  class Helper;
  class ClippedCurve;

  class SubRect
  {
  public:
    enum lit_by_t:uint32_t
      {
        /* m_lit[lit_by_path] is true if one curve of one
         * contour intersect the rect.
         */
        lit_by_path = 0,

        /* m_lit[lit_by_current_contour] is true if one curve
         * of the current contour intersects the rect
         */
        lit_by_current_contour
      };

    SubRect(ivec2 R, Filler::LineClipper &filler, const ClipElement *clip_element);

    const ivec2&
    ID(void) const
    {
      return m_ID;
    }

    /* Set m_lit[v] to true. Returns 1 if the value
     * of m_lit[v] went from false to true and returns
     * 0 otherwise.
     */
    unsigned int
    light_rect(enum lit_by_t v)
    {
      unsigned int return_value((m_lit[v] && !m_skip_rect) ? 0u : 1u);

      m_lit[v] = true;
      return return_value;
    }

    /* If returns true, the rect should be skipped because it is an
     * empty rect of the ClipElement to intersect against.
     */
    bool
    skip_rect(void) const
    {
      return m_skip_rect;
    }

    /* add the passes as specified by PassSet from blocks,
     * usually coming from create_blocks_from_builder()
     * to the STC data
     */
    void
    add_blocks(LineClipper &clipper, FillSTCShader::PassSet pass_set,
               const vecN<range_type<unsigned int>, FillSTCShader::pass_count> &blocks);

    /* Adds to encoder() the data for pass FillSTCShader::pass_contour_stencil
     * from a clipped contour
     */
    void
    add_stc_contour_data(LineClipper &clipper, c_array<const ClippedCurve> curves);

    /* Changes m_winding_offset by the effect of an edge hugging contour on this SubRect */
    void
    add_edge_hugging_contour(c_array<const ClippedCurve> contour);

    RenderEncoderImage
    encoder(void)
    {
      return m_encoder;
    }

    vecN<bool, 2> m_lit;

    /* The winding contribution of the curves that go around the
     * SubRect but do not intersect it; this is only added to
     * if the curve is not lit by the current contour
     */
    int m_winding_offset;

  private:
    void
    ready_encoder(void);

    /* the backing of the work data */
    Filler::LineClipper &m_filler;

    /* the subrect ID */
    ivec2 m_ID;

    /* the coordinate of the center */
    vec2 m_center;

    /* if true, this means the rect should be skipped */
    bool m_skip_rect;

    /* Those that are lit, get a RenderEncoderBase */
    RenderEncoderImage m_encoder;

    /* the transformation that maps to m_encoder coordinates */
    RenderValue<Transformation> m_tr;
  };

  /* An Intersection stores the intersection of a
   * MappedCurve against a horizontal or vertical
   * line. There can be up to two intersections
   * because a fill can be made from line segments,
   * quadratic bezier curves or conics.
   */
  class Intersection
  {
  public:
    class PerPoint
    {
    public:
      float m_t;
      vec2 m_position;
      int m_winding_effect;
    };

    /* Ctor; computes the intersection of the given curve
     * against the line at v oriented as according to tp.
     */
    Intersection(enum line_t tp, float v, const ContourCurve &curve);

    /* Ctor to make an Intersection object with no intersections  */
    Intersection(void):
      m_count(0u)
    {}

    unsigned int
    light_rects(Filler::LineClipper &filler, enum line_t l,
                int subrect_row_or_col,
                enum SubRect::lit_by_t tp) const;

    void
    add_to_lit_by_curves(Filler::LineClipper &filler, enum line_t l,
                         int subrect_row_or_col) const;

    /* compute the effect of the winding number
     * at a point along the line_t that was
     * use to construct the Intersection
     */
    int
    winding_effect(enum line_t tp, float varying_value) const;

    c_array<const PerPoint>
    values(void) const
    {
      return c_array<const PerPoint>(m_values).sub_array(0, m_count);
    }

  private:
    /* number of intersections, can be 0, 1 or 2; a value of zero
     * represents that there is no intersection.
     */
    unsigned int m_count;

    vecN<PerPoint, 2> m_values;
  };

  /* We need to clip against the curves mapped to pixel coordinates,
   * so we need to compute (on CPU) that mapping for each curve.
   */
  class MappedCurve
  {
  public:
    /* Ctor. Computes the curve mapped and all other fields
     * of MappedCurve
     * \param filler the backer
     * \param curve the curve to map by tr
     * \param tr the transforamtion
     * \param prev if non-null, the mapped curve's start point
     *             must match with the end point of prev mapped
     *             by tr
     */
    MappedCurve(Filler::LineClipper &filler,
                const ContourCurve &curve,
                const Transformation &tr,
                const ContourCurve *prev);

    /* Light the subrects that the mapped curve intersects.
     * Returns the number of subrects that went from unlit
     * to lit.
     */
    unsigned int
    light_rects(Filler::LineClipper &filler, enum SubRect::lit_by_t tp) const;

    /* Adds the conic STC and anti-alias data to the
     * rects this curve lights
     */
    void
    add_data_to_subrects(Filler::LineClipper &filler, range_type<int> x_range) const;

    /* For those rects that have m_lit[lit_by_current_contour] false,
     * adds to their winding offset the effect of this curve on it.
     */
    void
    add_winding_offset(Filler::LineClipper &filler, range_type<int> x_range) const;

    /* Conveniance function that fetches the Intersection of the curve
     * against the side of a sub-rect
     * \param filler the backing of the data
     * \param ss which side of the sub-rect
     * \param xy which colum or row of teh sub-rect; if ss is
     *           minx_side or maxx_side specifies which rub-rect columm,
     *           otherwise specifies which sub-rect row.
     */
    const Intersection&
    get_intersection(const Filler::LineClipper &filler, enum side_t ss, int xy) const;

    /* The curve in pixel coordinates */
    ContourCurve m_mapped_curve;

    /* .x() holds the x-range (i.e. columns) of SubRects the curve's bounding box intersects
     * .y() holds the y-range (i.e. rows) of SubRects the curve's bounding box intersects
     */
    vecN<range_type<int>, 2> m_subrect_range;

    /* The tight bounding box of the curve */
    BoundingBox<float> m_bb;

    /* for each x with m_subrect_range.x().m_begin <= x < m_subrect_range.x().m_end,
     * we need the the intersections along the vertical line at the left
     * and right side of the block column against the curve. Similarily, for
     * each y with m_subrect_range.y().m_begin <= y < m_subrect_range.y().m_end,
     * we also need that list of intersections.
     *
     * Let m_subrect_range.x().m_begin <= i < m_subrect_range.x().m_end, then
     * the intersection of this curve against the vertical line
     * X = minx_side_value(i) is stored at
     * Filler::m_intersection_backing[m_intersections[minx_side] + I]
     * where I = i - m_subrect_range.x().m_begin.
     */
    vecN<int, 4> m_intersections;
  };

  class MappedContour
  {
  public:
    /* Ctor
     * \param filler the object that backs everything
     * \param padding the padding on each side of each subrect
     * \param contour list of curves
     * \param tr transformation to apply to the curves
     */
    MappedContour(Filler::LineClipper &filler,
                  c_array<const ContourCurve> contour,
                  bool is_closed, const Transformation &tr);

    /* Light the subrects that the curves of the mapped contour
     * intersects. Returns the number of subrects that went from
     * unlit to lit.
     */
    unsigned int
    light_rects(Filler::LineClipper &filler) const;

    /* Adds the conic STC and anti-alias data to the
     * rects this contour lights
     */
    void
    add_data_to_subrects(Filler::LineClipper &filler) const;

    /* Gives the curves backed by filler */
    c_array<const MappedCurve>
    curves(Filler::LineClipper &filler) const
    {
      c_array<const MappedCurve> return_value(make_c_array(filler.m_mapped_curve_backing));
      return return_value.sub_array(m_curves);
    }

    /* Range into Filler::m_mapped_curve_backing of
     * the curves of the contour.
     */
    range_type<int> m_curves;

    /* .x() holds the x-range (i.e. columns) of SubRects the contour's bounding box intersects
     * .y() holds the y-range (i.e. rows) of SubRects the contour's bounding box intersects
     */
    vecN<range_type<int>, 2> m_subrect_range;
  };

  /* ClippedCurve only cares about the line-segment contour;
   * the STC only needs a clipped contour on the underlying
   * segment contour.
   */
  class ClippedCurve
  {
  public:
    enum type_t:uint32_t
      {
        /* Indicates that the line segment of the ClippedCurve
         * source is the line segment connecting the start and
         * end point of a MappedCurve.
         */
        unclipped_type,

        /* Indicates that the line segment of the ClippedCurve
         * is a line segment connecting is from clipping a line
         * segment connecting the start and end point of a
         * MappedCurve.
         */
        clipped_type,

        /* Indicates that the line segment of the ClippedCurve
         * is an edge hugger.
         */
        edge_hugger_type,
      };

    /* Construct a ClippedCurve whose type() is unclipped_type */
    ClippedCurve(int curve, Filler::LineClipper &filler);

    /* Construct a ClippedCurve value from clipped another ClippedCurve;
     * the type will be edge_hugger_type if the curve is edge_hugger_type,
     * otherwise the type will be clipped_type
     */
    ClippedCurve(const ClippedCurve &curve, const vec2 &start_pt, const vec2 &end_pt):
      m_parent_curve(curve.m_parent_curve),
      m_type(curve.type() == edge_hugger_type ? edge_hugger_type : clipped_type),
      m_start_pt(start_pt),
      m_end_pt(end_pt)
    {
    }

    /* Construct a ClippedCurve whose type() is edge_hugger_type */
    ClippedCurve(const vec2 &start_pt, const vec2 &end_pt):
      m_parent_curve(-1),
      m_type(edge_hugger_type),
      m_start_pt(start_pt),
      m_end_pt(end_pt)
    {}

    const vec2&
    start_pt(void) const
    {
      return m_start_pt;
    }

    const vec2&
    end_pt(void) const
    {
      return m_end_pt;
    }

    enum type_t
    type(void) const
    {
      return m_type;
    }

    bool
    is_cancelling_edge(const ClippedCurve &rhs) const
    {
      return type() == edge_hugger_type
        && rhs.type() == edge_hugger_type
        && end_pt() == rhs.start_pt()
        && start_pt() == rhs.end_pt();
    }

    const MappedCurve&
    contour_curve(Filler::LineClipper &filler) const
    {
      ASTRALassert(m_parent_curve >= 0);
      ASTRALassert(m_parent_curve < static_cast<int>(filler.m_mapped_curve_backing.size()));
      ASTRALassert(m_type != edge_hugger_type);

      return filler.m_mapped_curve_backing[m_parent_curve];
    }

  private:
    /* if contructed from a MappedCurve, the index into
     * Filler::m_mapped_curve_backing of the source curve.
     * Only makes sense if m_type is not edge_hugger_type
     */
    int m_parent_curve;

    /* The curve type */
    enum type_t m_type;

    /* The start end end points of the curve */
    vec2 m_start_pt, m_end_pt;
  };

  /* Class that does performs clipping of a contour */
  class ContourClipper
  {
  public:
    /* Ctor
     * \param filler the backing of it all
     * \param clip_log clip logger, for debugging
     * \param src the contoour to be clipped
     * \param side what side to clip against
     * \param R what subrect col (if side is minx_side or maxx_side)
     *          or row (if side is miny_side or maxy_side)
     * \param dst location to which to place clipped curves
     */
    ContourClipper(Filler::LineClipper &filler,
                   c_array<const ClippedCurve> src,
                   enum side_t side, int R,
                   std::vector<ClippedCurve> *dst);

  private:
    float
    compute_clip_distance(const vec2 &p)
    {
      return is_max_side(m_clip_side) ?
        m_R_value - p[m_fc]:
        p[m_fc] - m_R_value;
    }

    vec2
    compute_induced_point(const vec2 &p0, float d0,
                          const vec2 &p1, float d1)
    {
      float t;
      vec2 p;

      t = d0 / (d0 - d1);
      p[1 - m_fc] = mix(p0[1 - m_fc], p1[1 - m_fc], t);
      p[m_fc] = m_R_value;

      return p;
    }

    void
    add_curve(std::vector<ClippedCurve> *dst, const ClippedCurve &curve);

    /* the box-side to clip against */
    enum side_t m_clip_side;

    /* the line_t derived from m_clip_side */
    enum line_t m_clip_line;

    /* the fixed coordinate of the clipping line */
    int m_fc;

    /* which box row or column */
    int m_R;

    /* the value of the clipping line */
    float m_R_value;
  };

  /* called Filler::CommonClipper::set_subrects() and fills
   * the array m_elementary_rects
   */
  void
  create_subrects(ivec2 mask_size, const ClipElement *clip_element,
                  c_array<const BoundingBox<float>> restrict_bbs);

  /* Walks the CombinedPath, mapping each contour and lighting
   * the rects that intersect the curves. If at any time the
   * number of lit rects is too large, then early aborts and
   * returns false. If returns true, then sparse fill shall
   * be executed and all fields of m_mapped_curve_backing and
   * m_mapped_contours will be computed along with the value
   * of SubRect::m_winding_offset
   */
  bool
  map_contours_and_light_rects(const CombinedPath &path);

  /* Generate a clipped contour from a MappedContour */
  void
  create_clipped_contour(const MappedContour &contour,
                         std::vector<ClippedCurve> *out_contour);

  /* process a MappedContour:
   *   - for each rect within MappedContour::m_subrect_range, either
   *     add to SubRect::m_winding_offset or add STCData to
   *     SubRect::m_encoder
   *
   * This means, we need to clip, in an efficient manner
   * the MappedContour against each of those sub-rects
   */
  void
  process_mapped_contour(const MappedContour &contour);

  /* Clips the passed contour to each sub-rect in the named
   * column in the row range passed and calls process_subrect().
   */
  void
  process_mapped_contour_column(c_array<const ClippedCurve> contour,
                                int box_col, range_type<int> box_row_range);

  /* Clips the passed contour to each sub-rect in the named
   * column in the row range passed and calls process_subrect().
   */
  void
  process_mapped_contour_row(c_array<const ClippedCurve> current,
                             int box_row, range_type<int> box_col_range);

  void
  process_subrect(c_array<const ClippedCurve> contour, int box_col, int box_row);

  void
  process_subrects_contour_is_huggers_only(c_array<const ClippedCurve> contour,
                                           const vecN<range_type<int>, 2> &boxes);

  /* Clips a clipped contour against a box side.
   * \param in_contour the contour to get clipped
   * \param side which box side
   * \param box_row_col which box row or column
   * \param clip_log clip logger, for debugging
   * \param workroom work room for the comptuation,
   *                  the returned value is an array
   *                  into workroom.
   */
  c_array<const ClippedCurve>
  clip_contour(c_array<const ClippedCurve> in_contour,
               enum side_t side, int box_row_col,
               std::vector<ClippedCurve> *workroom);

  static
  bool
  contour_is_edge_huggers_only(c_array<const ClippedCurve> contour);

  /* Walk each of the sub-rects:
   *  - those with no contour intersections land as ImaglessElement with
   *    color value indicating fully covered or fully uncovered.
   *  - those with contour intersections, get a RenderEncoderImage
   *    which will have the STCData of clipping the contours added to
   *    it, these elements land as Element values. If the fill-rule is
   *    a non-zero fill rule, then they also get extra convering rects
   *    to advance the winding number correctly.
   */
  reference_counted_ptr<const Image>
  build_sparse_image(const ClipElement *clip_element,
                     enum clip_combine_mode_t clip_combine_mode,
                     TileTypeTable *out_clip_combine_tile_data);

  SubRect&
  subrect(int X, int Y)
  {
    ASTRALassert(m_elementary_rects[subrect_id(X, Y)].ID().x() == X);
    ASTRALassert(m_elementary_rects[subrect_id(X, Y)].ID().y() == Y);
    return m_elementary_rects[subrect_id(X, Y)];
  }

  SubRect&
  subrect(ivec2 p)
  {
    return subrect(p.x(), p.y());
  }

  virtual
  RenderEncoderImage
  rect_encoder(int X, int Y) override
  {
    return subrect(X, Y).encoder();
  }

  virtual
  int
  rect_winding_offset(int X, int Y) override
  {
    return subrect(X, Y).m_winding_offset;
  }

  void
  cleanup(void);

  /* ItemData reused across all clipped contour */
  ItemData m_item_data;

  /* Backing of the contours mapped to render-coordinates  */
  std::vector<MappedCurve> m_mapped_curve_backing;

  /* A contour is a range into m_mapped_curve_backing */
  std::vector<MappedContour> m_mapped_contours;

  /* backing of the intersections, the 0'th element
   * is an empty intersection.
   */
  std::vector<Intersection> m_intersection_backing;

  /* backing for sub-rects B */
  std::vector<SubRect> m_elementary_rects;

  /* the number of SubRect's that are lit */
  unsigned int m_number_lit;

  /* the threshhold for number lit befoe
   * skipping parse filling
   */
  unsigned int m_thresh_lit;

  /* stats */
  unsigned int m_num_culled_paths, m_num_culled_contours, m_num_late_culled_contours;
  unsigned int m_total_num_paths, m_total_num_contours;

  /* workroom for computing animated contour values */
  std::vector<ContourCurve> m_workroom_curves;

  /* workroom for clipping mapped contours
   *  - m_clipped_contourA is used to prepare the clipping to columns
   *  - m_clipped_contourB is used to take clipped against a column
   *                       and make cliped against each rect of the
   *                       column
   */
  vecN<std::vector<ClippedCurve>, 2> m_clipped_contourA, m_clipped_contourB;

  /* use by MappedCurve::light_rects() to add conic STC and
   * anti-alias data to SubRect::m_encoder
   */
  std::vector<unsigned int> m_lit_by_curves_backing;
  detail::CustomSet m_lit_by_curves;

  /* used by SubRect to add a curve or line contour to m_builder */
  std::vector<vec2> m_workroom_line_contour;

  /* used by create_subrects() to realize the passed sub_rects value as tile ranges */
  std::vector<vecN<range_type<int>, 2>> m_range_values;
};

#endif

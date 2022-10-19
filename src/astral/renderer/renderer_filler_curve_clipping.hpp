/*!
 * \file renderer_filler_curve_clipping.hpp
 * \brief file renderer_filler_curve_clipping.hpp
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

#ifndef ASTRAL_RENDERER_FILLER_CURVE_CLIPPING_HPP
#define ASTRAL_RENDERER_FILLER_CURVE_CLIPPING_HPP

#include <astral/util/memory_pool.hpp>
#include <astral/renderer/renderer.hpp>
#include <astral/renderer/shader/fill_stc_shader.hpp>
#include "renderer_shared_util.hpp"
#include "renderer_cached_combined_path.hpp"
#include "renderer_filler.hpp"
#include "renderer_filler_common_clipper.hpp"

/* Self contained (monster) that breaks a fill of a CombinedPath
 * into sub-rectangles for the purpose of finding those sub-rectangles
 * that do not intersect the edges of the CombinedPath so that
 * those sub-rectangles are realized as just solid blits (or nothing).
 * The purpose is to save fill rate and bandwidth which stencil-then-cover
 * eats like popcorn.
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
 *  4. For each contour C, we need compute those subrect B that intersect the
 *     curves C and also for all other rects the effect of C on the winding
 *     number. Let Clipped(C, B) denote C clipped against a subrect B. Let R
 *     denote the box-range of columns and rows of the bounding box of C, i.e.
 *     R is essentially a vecN<range_type<int>, 2>. We compute the necessary
 *     data from C as follows.
 *      a. Initialize current = C. Clip from current the left column of boxes
 *         and run that column of boxes to process_mapped_contour_column().
 *         Update current to be missing the left column and update R to the
 *         reduced range of losing the left column. Repeat the process on the
 *         right column side of current. Then clip from current the top row
 *         of boxes and run that row of boxes to process_mapped_contour_row().
 *         Repeat this process on the bottom row. Continue repeating (a) until
 *         the number of box rows or columns is less than 3. The KEY performance
 *         helper is that if at any time after stripping a row or column, all
 *         of current consists of edge huggers, then the effect of the winding
 *         number of all rects of R is a constant and we can jump to computing
 *         that winding offset and doing an early out.
 *      b. continue running only clipping columns from (a) until the number of
 *         columns is less than 3
 *      c. continue running only clipping rows from (a) until the number of
 *         rows is less than 3
 *  5. The methods process_mapped_contour_column() and process_mapped_contour_row()
 *     essentially walk the column (resp row) of sub-rects and by calling
 *     process_subrect(), compute if the C has any curves hitting a rect B and
 *     if so adding the STCData of C clipped against B to C or if no curve hits,
 *     i.e. cll of Clipped(C, B) are edge huggers, then to increment the value
 *     of B.m_winding_offset appropiately.
 *  6. Once Step 4 & 5 are done on all contours C, each subrect B will have
 *     the STC data added to it for those contours that hit and the effect
 *     of the winding number of all those contours that do not have curves
 *     hitting it.
 *  7. If the winding rule is nonzero_fill_rule or complement_nonzero_fill_rule,
 *     for those subrects with STC data add abs(winding offset) winding rects
 *     around the sub-rect. If winding offset is negative orient one way,
 *     if positive the other way.
 *  9. If the winding rule is odd_even_fill_rule or complement_odd_even_fill_rule,
 *     for those SubRect with STC data invert the fill rule in the VirtualBuffer
 *     when then winding offset is odd.
 * 10. Using Filler::CommonClipper::create_sparse_image_from_rects() creates
 *     a Image where the tiles that are completely filled become
 *     ImageMipElement::white_element and those thare are completetly
 *     unfilled become ImageMipElement::empty_element tiles. Of key
 *     importance is that each rect is exactly one tile in size when the
 *     padding is included.
 *
 * The main diffuculty is computing the clipped contours. Numerical round-off
 * make the detection on what side a curve lies or its splits lie numerically
 * unstable (especially when the split is near the start or end of a curve or
 * if the start or end of the curve itself is near the clipping plane). To that
 * end the detection of "on what side" is done by computing the center of the
 * tight bounding box for all the splits and whichever center is furthest from
 * the clip-plane is used to decide. Even with this, there are risks of coordinate
 * unmatching and the code will (semi-silently) put in a connecting curve.
 * Debug builds or tweaked release builds can be made to assert instead.
 *
 */
class astral::Renderer::Implement::Filler::CurveClipper:public Filler::CommonClipper
{
public:
  explicit
  CurveClipper(Renderer::Implement &renderer):
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

  /* An Intersection stores the intersection of a
   * MappedCurve against a horizontal or vertical
   * line. There can be up to two intersections
   * because a fill can be made from line segments,
   * quadratic bezier curves or conics.
   */
  class Intersection
  {
  public:
    /* Ctor; computes the intersection of the given curve
     * against the line at v oriented as according to tp.
     * The intersection list does NOT include when the end
     * points of the curve go exactly through the clipping
     * line.
     */
    Intersection(enum line_t tp, float v, const ContourCurve &curve);

    /* Ctor to make an Intersection object with no intersections  */
    Intersection(void):
      m_count(0u)
    {}

    unsigned int
    light_rects(Filler::CurveClipper &filler, const ContourCurve &curve, enum line_t l, int v) const;

    c_array<const float>
    data(void) const
    {
      return c_array<const float>(m_data).sub_array(0, m_count);
    }

    /* Returns true precisely when the entire curve, including
     * the start and end point are on one side of a clipping
     * line; i.e. returns false if the curves crosses the clipping
     * line or when the start or end point are on the clipping
     * line. This is NOT equivalent to
     * Intersection(coord, curve, v).data().empty().
     */
    static
    bool
    on_one_open_side(int coord, const ContourCurve &curve, float v);

  private:
    /* number of intersections, can be 0, 1 or 2; a value of zero
     * represents that there is no intersection.
     */
    unsigned int m_count;

    /* the parameteric time of intersection, if there
     * are two intersections, then m_data[0] m_data[1]
     */
    vecN<float, 2> m_data;
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
    MappedCurve(Filler::CurveClipper &filler,
                const ContourCurve &curve,
                const Transformation &tr,
                const ContourCurve *prev);

    /* Light the subrects that the mapped curve intersects.
     * Returns the number of subrects that went from unlit
     * to lit.
     */
    unsigned int
    light_rects(Filler::CurveClipper &filler);

    /* Conveniance function that fetches the Intersection of the curve
     * against the side of a sub-rect
     * \param filler the backing of the data
     * \param ss which side of the sub-rect
     * \param xy which colum or row of teh sub-rect; if ss is
     *           minx_side or maxx_side specifies which rub-rect columm,
     *           otherwise specifies which sub-rect row.
     */
    const Intersection&
    get_intersection(const Filler::CurveClipper &filler, enum side_t ss, int xy) const;

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
    MappedContour(Filler::CurveClipper &filler,
                  c_array<const ContourCurve> contour,
                  bool is_closed, const Transformation &tr);

    /* Light the subrects that the curves of the mapped contour
     * intersects. Returns the number of subrects that went from
     * unlit to lit.
     */
    unsigned int
    light_rects(Filler::CurveClipper &filler);

    /* Gives the curves backed by filler */
    c_array<const MappedCurve>
    curves(Filler::CurveClipper &filler) const
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

  class ClippedCurve
  {
  public:
    /* Construct a ClippedCurve value from a MappedCurve
     * stored at Filler::m_mapped_curve_backing[curve]
     */
    ClippedCurve(int curve, Filler::CurveClipper &filler);

    /* Construct a ClippedCurve from a ContourCurve;
     * this ctor is issued when a MappedCurve is clipped.
     */
    explicit
    ClippedCurve(const ContourCurve &clipped_curve, bool hugs_boundary);

    /* Returns
     * .first is the geometry of the clipped curve
     * .second is true if the curve should get anti-aliased
     */
    const std::pair<ContourCurve, bool>&
    as_contour(void) const
    {
      return m_curve;
    }

    const vec2&
    start_pt(void) const
    {
      return m_curve.first.start_pt();
    }

    const vec2&
    end_pt(void) const
    {
      return m_curve.first.end_pt();
    }

    const ContourCurve&
    curve(void) const
    {
      return m_curve.first;
    }

    bool
    hugs_boundary(void) const
    {
      return !m_curve.second;
    }

    Intersection
    intersection(Filler::CurveClipper &filler, enum side_t clip_side, int R) const;

    bool
    is_cancelling_edge(const ClippedCurve &curve) const;

  private:
    /* if contructed from a MappedCurve, the index into
     * Filler::m_mapped_curve_backing of the source curve.
     * A value of -1 indicates that this ClippedCurve
     * does not come from a MappedCurve.
     */
    int m_parent_curve;

    /* .first = the curve
     * .second = does NOT hug the boundary
     */
    std::pair<ContourCurve, bool> m_curve;
  };

  class ClipLog;

  /* Class that does performs clipping of a contour */
  class ClippedContourBuilder
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
    ClippedContourBuilder(Filler::CurveClipper &filler,
                          ClipLog &clip_log,
                          c_array<const ClippedCurve> src,
                          enum side_t side, int R,
                          std::vector<ClippedCurve> *dst):
      m_dst(dst),
      m_filler(filler),
      m_clip_side(side),
      m_clip_line(line_t_from_side_t(side)),
      m_fc(fixed_coordinate(m_clip_line)),
      m_R(R),
      m_R_value(m_filler.side_value(m_R, m_clip_side)),
      m_prev_clipped(false),
      m_first_element_clipped(false),
      m_num_curves_processed(0),
      m_input(src),
      m_clip_log(clip_log)
    {
      ASTRALassert(m_dst);
      m_dst->clear();
    }

    /* Add to an array of ClippedCurve values this clipped
     * curve clipped against a side of a box row or column
     * \param curve curve to clip
     */
    void
    clip_curve(const ClippedCurve &curve);

    /* Complete the clipping, this will add, if necessary
     * a closing edge that goes along the boundary
     */
    void
    close_clipping_contour(void);

  private:
    void
    clip_curve_implement(const ClippedCurve &curve);

    /* Add's an edge hugger that connects the last curve added
     * to dst to this's curve's start.
     */
    void
    add_edge_hugger(const ClippedCurve &curve);

    /* checks if m_dst->back() and the passed curve should
     * be joined by an edge hugger
     */
    bool
    edge_hugger_detected(const ClippedCurve &c)
    {
      return !m_dst->empty()
        && points_different(m_dst->back().end_pt(), c.start_pt())
        && m_dst->back().end_pt()[m_fc] == m_R_value
        && c.start_pt()[m_fc] == m_R_value;
    }

    /* Add's this curve the dst and if necessary, adds an edge hugger */
    void
    add_curve(bool new_curve_clipped, const ClippedCurve &curve);

    void
    add_curve_impl(const ClippedCurve &c);

    bool
    inside_region(const vec2 p)
    {
      return is_max_side(m_clip_side) ?
        p[m_fc] < m_R_value :
        p[m_fc] > m_R_value;
    }

    /* location to which to write the contour clipped */
    std::vector<ClippedCurve> *m_dst;

    /* the backing of it all */
    Filler::CurveClipper &m_filler;

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

    bool m_prev_clipped, m_first_element_clipped;
    unsigned int m_num_curves_processed;
    c_array<const ClippedCurve> m_input;

    ClipLog &m_clip_log;
  };

  class SubRect
  {
  public:
    explicit
    SubRect(bool skip_rect_value):
      m_lit_by_path(false),
      m_winding_offset(0),
      m_curves_added(false),
      m_skip_rect(skip_rect_value),
      m_stc_builder(nullptr)
    {}

    /* Set m_lit_by_path to true. Returns 1 if the value
     * of m_lit_by_path went from false to true and returns
     * 0 otherwise.
     */
    unsigned int
    light_rect(void)
    {
      unsigned int return_value(m_lit_by_path ? 0 : 1);

      m_lit_by_path = true;
      return return_value;
    }

    /* is true exactly when atleast one curve of one contour
     * intersects this SubRect
     */
    bool m_lit_by_path;

    /* The winding contribution of all curves that go around the
     * SubRect but do not intersect it
     */
    int m_winding_offset;

    /* In exact arithematic, this would be the exact same value
     * as m_lit_by_path. However when a curve is clipped, it
     * might push an intersection that was on or close to the
     * rect boundary onto the other side. To avoid that, we
     * use this value to indicate a curve has been added to
     * the STCData of m_encoder.
     */
    bool m_curves_added;

    /* If true, skip this rect because it corrseponds to an empty
     * tile of the intersecting ClipElement. Default value set by ctor.
     */
    bool m_skip_rect;

    /* Those that are lit, get a RenderEncoderImage */
    RenderEncoderImage m_encoder;

    /* Those that are lit, also get an STCData::BuilderSet */
    STCData::BuilderSet *m_stc_builder;

    /* the transformation that maps to m_encoder coordinates */
    RenderValue<Transformation> m_tr;
  };

  /* Create the sub-rects
   * \param mask_size size of the area covered by the fill
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
                                ClipLog &clip_log, int box_col,
                                range_type<int> box_row_range);

  /* Clips the passed contour to each sub-rect in the named
   * column in the row range passed and calls process_subrect().
   */
  void
  process_mapped_contour_row(c_array<const ClippedCurve> current,
                             ClipLog &clip_log, int box_row,
                             range_type<int> box_col_range);

  /* Either adds the STCData if there are curves hit the box
   * or changes the, value of SubRect::m_winding_offset
   */
  void
  process_subrect(c_array<const ClippedCurve> contour, int box_col, int box_row);

  /* Called by process_mapped_contour() to indicate that a contour
   * is comprised solely of edges that hug the boundary and thus can
   * skip further clipping and effect the winding number across a
   * block of sub-rects.
   */
  void
  process_subrects_all_edge_huggers(c_array<const ClippedCurve> contour,
                                    const vecN<range_type<int>, 2> &boxes);

  /* Used by process_subrect() to add STC data
   * \param stc_builder to which to add STCData
   * \param tr the transformation from coordinates of
   *           contour to coordinates of buffer
   * \param contour the contour to add.
   */
  void
  add_stc_data(STCData::BuilderSet *stc_builder,
               RenderValue<Transformation> tr,
               c_array<const ClippedCurve> contour);

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
               ClipLog &clip_log,
               std::vector<ClippedCurve> *workroom);

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
    ASTRALassert(valid_subrect(X, Y));
    return m_elementary_rects[X + Y * m_number_elementary_rects.x()];
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
    const SubRect &R(subrect(X, Y));

    ASTRALassert(R.m_curves_added == R.m_encoder.valid());
    return R.m_encoder;
  }

  virtual
  int
  rect_winding_offset(int X, int Y) override
  {
    return subrect(X, Y).m_winding_offset;
  }

  static
  bool
  all_are_edge_huggers(c_array<const ClippedCurve> contour);

  static
  void
  print_clipped_contour(c_array<const ClippedCurve> contour,
                        int tab, std::ostream&);

  static
  bool
  points_different(const astral::vec2 &a, const astral::vec2 &b)
  {
    return a != b;
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
};

#endif

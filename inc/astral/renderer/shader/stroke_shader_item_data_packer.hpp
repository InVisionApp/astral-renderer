/*!
 * \file stroke_shader_item_data_packer.hpp
 * \brief file stroke_shader_item_data_packer.hpp
 *
 * Copyright 2021 by InvisionApp.
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

#ifndef ASTRAL_STROKE_SHADER_ITEM_DATA_PACKER_HPP
#define ASTRAL_STROKE_SHADER_ITEM_DATA_PACKER_HPP

#include <astral/util/vecN.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/transformation.hpp>
#include <astral/renderer/stroke_parameters.hpp>
#include <astral/renderer/shader/item_data.hpp>
#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/render_value.hpp>

namespace astral
{
  /*!
   * \brief
   * Class to scope encapsulate item data packers for stroking shaders
   */
  class StrokeShaderItemDataPacker
  {
  public:
    /*!
     * \brief
     * A \ref ItemDataPackerBase represents how item data
     * is packed for the shaders of a astral::StokeShader
     * to consume.
     */
    class ItemDataPackerBase
    {
    public:
      virtual
      ~ItemDataPackerBase()
      {}

      /*!
       * To be optionally implemented by a derived class
       * to return the FACTOR by which to inflate the
       * stroking radius for joins. The caller will handle
       * miter-joins itself. This is essentially to allow
       * for an implementation of ItemDataPackerBase to
       * handle the cases when joins are distorted.
       * \param join_style value of StrokeParameters::m_join
       * \param cap_style value of StrokeParameters::m_cap
       */
      virtual
      float
      join_stroke_inflate_factor(enum join_t join_style,
                                 enum cap_t cap_style) const
      {
        ASTRALunused(cap_style);
        ASTRALunused(join_style);
        return 1.0f;
      }

      /*!
       * Returns the FACTOR by which to inflate the
       * stroking radius for joins takng into account
       * the miter-limit.
       */
      float
      join_stroke_inflate_factor_with_miter(float miter_limit,
                                            enum join_t join_style,
                                            enum cap_t cap_style) const
      {
        float f;

        f = join_stroke_inflate_factor(join_style, cap_style);
        f = (join_style == join_miter) ? t_max(f, miter_limit) : f;

        return f;
      }

      /*!
       * To be optionally implemented by a derived class
       * to return the FACTOR by which to inflate the
       * stroking radius for edges.
       * miter-limit.
       * \param join_style value of StrokeParameters::m_join
       * \param cap_style value of StrokeParameters::m_cap
       */
      virtual
      float
      edge_stroke_inflate_factor(enum join_t join_style,
                                 enum cap_t cap_style) const
      {
        ASTRALunused(join_style);
        ASTRALunused(cap_style);
        return 1.0f;
      }

      /*!
       * To be implemented by a derived clas to return
       * the required size to pack the shader
       * data that this packer packs.
       */
      virtual
      unsigned int
      item_data_size(const StrokeParameters &stroke_params) const = 0;

      /*!
       * To be implemnted by a derived class to pack
       * the item data for stroking
       * \param logical_transformation_path transformation from
       *                                    path coordinate to
       *                                    logical coordinates
       * \param stroke_params stroke parameters specifying how to stroke
       * \param t the time interpolate
       * \param dst location to which to pack the item data
       */
      virtual
      void
      pack_item_data(RenderValue<Transformation> logical_transformation_path,
                     const StrokeParameters &stroke_params,
                     float t, c_array<gvec4> dst) const = 0;

      /*!
       * To be implemented by a derived class to return the
       * astral::ItemDataValueMapping associated to data packed
       * via pack_item_data().
       */
      virtual
      const ItemDataValueMapping&
      intrepreted_value_map(void) const = 0;

      /*!
       * To be implemented by a derived class to return true if joins,
       * caps, and glue joins collapse. For example, in hairline stroking.
       * \param pixel_transformation_logical the matrix portion of the
       *                                     transformation from logical to
       *                                     pixel coordinates
       * \param render_scale_factor the scaling factor applied from pixel
       *                            coordinates to surface coordinates, see
       *                            RenderEncoderBase::render_scale_factor()
       * \param stroke_params the stroking parameters
       */
      virtual
      bool
      caps_joins_collapse(const float2x2 &pixel_transformation_logical,
                          float render_scale_factor,
                          const StrokeParameters &stroke_params) const = 0;

      /*!
       * Convenience overload that set the time interpolate to 0,
       * i.e. is equivalent to
       * \code
       * pack_item_data(logical_transformation_path,
       *                stroke_params, 0.0f, dst);
       * \endcode
       * \param logical_transformation_path transformation from
       *                                    path coordinate to
       *                                    logical coordinates
       * \param stroke_params stroke parameters specifying how to stroke
       * \param dst location to which to pack the item data
       */
      void
      pack_item_data(RenderValue<Transformation> logical_transformation_path,
                     const StrokeParameters &stroke_params,
                     c_array<gvec4> dst) const
      {
        pack_item_data(logical_transformation_path,
                       stroke_params, 0.0f, dst);
      }
    };

    /*!
     * \brief
     * \ref ItemDataPackerBase derived class to pack shader
     * data that embodies only time interpolate, stroking width
     * and miter limit.
     */
    class ItemDataPacker:public ItemDataPackerBase
    {
    public:
      /*!
       * \brief
       * Enumeration that specifies how item data is packed
       */
      enum item_data_packing:uint32_t
        {
          /*!
           * Offset at whcih to pack the cookie for the transformation
           * that maps from path coordinates to logical coordinates
           *  - .x().f --> absolute value is 0.5 * StrokeParameters::m_width,
           *               i.e. the stroking radius. A negative value indicates
           *               that StrokeParameters::m_graceful_thin_stroking is
           *               true and a positive value indicates it is false
           *  - .y().f --> time interpolate
           *  - .z().u --> RenderValue<Transformation>::cookie()
           *  - .w().f --> sign(M) * sqrt(M * M - 1) where M is the signed
           *               miter-limit where a positive value indicates
           *               to clip miter-joins to the miter-limit and a
           *               negative value indicates to draw miter-joins
           *               that exceed the miter-limit as bevel joins.
           *               The value of sqrt(M * M - 1) is the maximum
           *               length to go along a tangent at a join before
           *               exceeding the miter-limit.
           */
          base_data_offset = 0,

          /*!
           * Number of item datas consumed.
           */
          item_data_count = 1
        };

      virtual
      bool
      caps_joins_collapse(const float2x2 &pixel_transformation_logical,
                          float render_scale_factor,
                          const StrokeParameters &stroke_params) const override;

      virtual
      void
      pack_item_data(RenderValue<Transformation> logical_transformation_path,
                     const StrokeParameters &stroke_params,
                     float t, c_array<gvec4> dst) const override;

      virtual
      unsigned int
      item_data_size(const StrokeParameters&) const override;

      virtual
      const ItemDataValueMapping&
      intrepreted_value_map(void) const override;
    };

    /*!
     * \brief
     * Specifies an element of a dash pattern; which consists of
     * the length to draw along the path and then the length to
     * skip along the path.
     */
    class DashPatternElement
    {
    public:
      /*!
       * Ctor
       * \param D value with which to initialize m_draw_length
       * \param S value with which to initialize m_skip_length
       */
      explicit
      DashPatternElement(float D = 0.0f, float S = 0.0f):
        m_draw_length(D),
        m_skip_length(S)
      {}

      /*!
       * How long along the path to draw, this value can be zero
       * to start a dash pattern with a skip length.
       */
      float m_draw_length;

      /*!
       * How long along the path after the length specified by
       * \ref m_draw_length to skip. A value of zero indicates
       * that it will be merged with the next \ref
       * DashPatternElement in a dash pattern.
       */
      float m_skip_length;
    };

    /*!
     * \brief
     * A StrokeShader::DashPattern is composed of
     * StrokeShader::DashPatternElement
     * values.
     *
     * The object is implemented as an std::vector of
     * StrokeShader::DashPatternElement values,
     * so to prevent memory allocation noise, resuse
     * StrokeShader::DashPattern values across
     * frames. Data is packed as follows:
     *   - [ItemDataPacker::base_data_offset] as in \ref ItemDataPacker
     *   - [ItemDataPacker::base_data_offset + 1].x().f --> sum over length of xz interval channels
     *   - [ItemDataPacker::base_data_offset + 1].y().f --> sum over length of yw interval channels
     *   - [ItemDataPacker::base_data_offset + 1].z().f --> dash_corner_radius()
     *   - [ItemDataPacker::base_data_offset + 1].w().f --> bits decribing the properties of the dash pattern, see DashPattern::flags_t for the meaning
     *   - [ItemDataPacker::base_data_offset + 2].x()   --> free
     *   - [ItemDataPacker::base_data_offset + 2].y().f --> length of last interval, positive value means draw, negative values means skip
     *   - [ItemDataPacker::base_data_offset + 2].z().f --> length of first interval, positive value means draw, negative values means skip
     *   - [ItemDataPacker::base_data_offset + 2].w().u --> number of intervals
     *   - [ItemDataPacker::base_data_offset + 3].x().f --> length of interval 0, positive value means draw, negative values means skip
     *   - [ItemDataPacker::base_data_offset + 3].y().f --> length of interval 1, positive value means draw, negative values means skip
     *   - [ItemDataPacker::base_data_offset + 3].z().f --> length of interval 2, positive value means draw, negative values means skip
     *   - [ItemDataPacker::base_data_offset + 3].w().f --> length of interval 3, positive value means draw, negative values means skip
     *   - [ItemDataPacker::base_data_offset + 4].x().f --> length of interval 4, positive value means draw, negative values means skip
     *   - [ItemDataPacker::base_data_offset + 4].y().f --> length of interval 5, positive value means draw, negative values means skip
     *   - [ItemDataPacker::base_data_offset + 4].z().f --> length of interval 6, positive value means draw, negative values means skip
     *   - [ItemDataPacker::base_data_offset + 4].w().f --> length of interval 7, positive value means draw, negative values means skip
     *   - ... and so on
     *   .
     *
     * NOTE: the intervals packed are not necessarily the same intervals given;
     * the intervals packed will be tweaked from the value of the dash_start_offset()
     * so that the shader can assume that the dash offset is zero. Additionally, the
     * length of the last interval is incremented by the length of the first
     * interval if both are draws or both are skips; this simplifies the logic of
     * a shader when computing the interval within the dash pattern.
     */
    class DashPattern:public ItemDataPacker
    {
    public:
      /*!
       * Enumeration to describe if and how a dash pattern
       * is adjusted per contour or per edge.
       */
      enum adjust_t:uint32_t
        {
          /*!
           * No adjustment is made to the elements
           */
          adjust_none = 0,

          /*!
           * The elements are compressed.
           */
          adjust_compress,

          /*!
           * The elements are stretched
           */
          adjust_stretch,

          number_adjust
        };

      /*!
       * Enumeration of bit masks used in the packed item data
       * of a DashPattern describing how the dash pattern is
       * modified by the length of the edges or contours.
       */
      enum flags_t:uint32_t
        {
          /*!
           * indicates that the dash pattern is taken as is and its
           * length is not adjusted
           */
          no_length_adjust = 0,

          /*!
           * Indicates that the length is adjusted by compressing
           * the draw and/or skip elements.
           */
          length_adjust_compress = 1,

          /*!
           * Indicates that the length is adjusted by stretching
           * the draw and/or skip elements.
           */
          length_adjust_stretch = 2,

          /*!
           * If this bit is up, then stroking is applied to each
           * edge seperately; otherwise it is applied to each
           * contour seperately.
           */
          stroke_starts_at_edge = 4,

          /*!
           * If this bit is up, indicates that .x and .z channels
           * of each dash pattern vec4 are to be compressed or
           * stretched.
           */
          adjust_xz_lengths = 8,

          /*!
           * If this bit is up, indicates that .y and .w channels
           * of each dash pattern vec4 are to be compressed or
           */
          adjust_yw_lengths = 16,

          /*!
           * Adjust both the draw and skip lengths
           */
          adjust_xz_and_yw_lengths = adjust_xz_lengths | adjust_yw_lengths
        };

      DashPattern(void);

      /*!
       * Clear the StrokeShader::DashPattern of all
       * StrokeShader::DashPatternElement values.
       */
      DashPattern&
      clear(void);

      /*!
       * Returns true if the dash pattern is empty
       * of StrokeShader::DashPatternElement values.
       */
      bool
      empty(void) const
      {
        return m_elements.empty();
      }

      /*!
       * Set the dash offset.
       */
      DashPattern&
      dash_start_offset(float f);

      /*!
       * Return the dash offset.
       */
      float
      dash_start_offset(void) const
      {
        return m_dash_offset;
      }

      /*!
       * Append a StrokeShader::DashPatternElement to the
       * StrokeShader::DashPattern
       */
      DashPattern&
      add(DashPatternElement e)
      {
        add_draw(e.m_draw_length);
        add_skip(e.m_skip_length);
        return *this;
      }

      /*!
       * Append a draw interval to the StrokeShader::DashPattern;
       * negative values are ignored (silently). A zero-value
       * indicates to add a point which induces caps.
       */
      DashPattern&
      add_draw(float e);

      /*!
       * Append a skip interval to the StrokeShader::DashPattern;
       * non-positive values are ignored (silently).
       */
      DashPattern&
      add_skip(float e);

      /*!
       * Set if the draw lengths of the DashPattern should
       * be adjusted
       */
      DashPattern&
      draw_lengths_adjusted(bool b)
      {
        m_draw_lengths_adjusted = b;
        return *this;
      }

      /*!
       * Returns the value of draw_lengths_adjusted(bool)
       */
      bool
      draw_lengths_adjusted(void) const
      {
        return m_draw_lengths_adjusted;
      }

      /*!
       * Set if the skip lengths of the DashPattern should
       * be adjusted
       */
      DashPattern&
      skip_lengths_adjusted(bool b)
      {
        m_skip_lengths_adjusted = b;
        return *this;
      }

      /*!
       * Returns the value of skip_lengths_adjusted(bool)
       */
      bool
      skip_lengths_adjusted(void) const
      {
        return m_skip_lengths_adjusted;
      }

      /*!
       * Sets the adjust mode; if the value is not \ref adjust_none
       * then the dash pattern is stretched or compressed so that
       * a multiple of it is the fitting lenght. The fitting length
       * is determined by dash_corner_radius() as follows:
       * - the fitting length is the length of the contour if dash_corner_radius() < 0
       * - the fitting length is the length of the edge minue 2 * dash_corner_radius()
       *   if dash_corner_radius() >= 0.0
       */
      DashPattern&
      adjust_mode(enum adjust_t adjust)
      {
        m_adjust = adjust;
        return *this;
      }

      /*!
       * Returne the value set in adjust_mode(enum adjust_t)
       */
      enum adjust_t
      adjust_mode(void) const
      {
        return m_adjust;
      }

      /*!
       * Set the length of the dash corner. If the value is non-negative,
       * then the dash pattern starts on each edge. A strictly positive
       * values means that that length around each join is stroked and the
       * dash pattern starts AFTER that length. If the value is negative,
       * then dash pattern starts on each contour.
       */
      DashPattern&
      dash_corner_radius(float f)
      {
        m_dash_corner = f;
        return *this;
      }

      /*!
       * Returns the value set by dash_corner(float)
       */
      float
      dash_corner_radius(void) const
      {
        return m_dash_corner;
      }

      /*!
       * Sets if the dash pattern starts per edge.
       */
      DashPattern&
      dash_pattern_per_edge(bool b)
      {
        m_dash_pattern_per_edge = b;
        return *this;
      }

      /*!
       * Returns the value set by dash_pattern_per_edge(bool)
       */
      bool
      dash_pattern_per_edge(void) const
      {
        return m_dash_pattern_per_edge;
      }

      /*!
       * Set the scale factor to apply to the input
       * of DashPatternElement values of this DashPattern.
       */
      DashPattern&
      scale_factor(float v)
      {
        m_scale_factor = v;
        return *this;
      }

      /*!
       * Returns the value set by scale_factor(float)
       */
      float
      scale_factor(void) const
      {
        return m_scale_factor;
      }

      /*!
       * Returns the dash pattern as specified by calls to
       * clear(), add(), add_draw() and add_skip().
       */
      c_array<const float>
      source_intervals(void) const
      {
        return make_c_array(m_elements);
      }

      virtual
      float
      join_stroke_inflate_factor(enum join_t join_style,
                                 enum cap_t cap_style) const override
      {
        ASTRALunused(join_style);

        /* when doing dashed stroking, if a dash ends just before
         * a join it can add a cap at that location, thus we inflate
         * by sqrt(2) when there are square caps to make sure we
         * capture the inflation from those added square caps.
         */
        return (cap_style == cap_square) ? ASTRAL_SQRT2 : 1.0f;
      }

      virtual
      float
      edge_stroke_inflate_factor(enum join_t join_style,
                                 enum cap_t cap_style) const override
      {
        ASTRALunused(join_style);
        return (cap_style == cap_square) ? ASTRAL_SQRT2 : 1.0f;
      }

      virtual
      unsigned int
      item_data_size(const StrokeParameters &stroke_params) const override;

      virtual
      void
      pack_item_data(RenderValue<Transformation> logical_transformation_path,
                     const StrokeParameters &stroke_params,
                     float t, c_array<gvec4> dst) const override;

    private:
      void
      prepare_packed_data(void) const;

      /* negative value indicates a skip, positive value
       * indicates a draw.
       */
      void
      add_implement(float length);

      uint32_t
      flags(void) const;

      void
      mark_dirty(void)
      {
        m_computed_intervals.clear();
        m_computed_intervals_filter_zero.clear();
        }

      void
      ready_computed_intervals(void) const;

      void
      ready_computed_intervals_filter_zero(void) const;

      /* input data */
      float m_total_length;
      std::vector<float> m_elements;
      float m_dash_offset;
      float m_dash_corner;
      enum adjust_t m_adjust;
      bool m_draw_lengths_adjusted;
      bool m_skip_lengths_adjusted;
      bool m_dash_pattern_per_edge;
      float m_scale_factor;

      /* computed data, with m_computed_intervals
       * being empty indicating that it needs to
       * be recomputed.
       */
      mutable std::vector<float> m_computed_intervals;
      mutable std::vector<float> m_computed_intervals_filter_zero;
      mutable float m_first_interval, m_last_interval;
      mutable vecN<float, 2> m_totals;
    };
  };

  /*!
   * Returns a string corresponing to the enum-value.
   */
  c_string
  label(enum StrokeShaderItemDataPacker::DashPattern::adjust_t);
};

#endif

/*!
 * \file item_path.hpp
 * \brief file item_path.hpp
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

#ifndef ASTRAL_SHADER_PATH_HPP
#define ASTRAL_SHADER_PATH_HPP

#include <vector>
#include <astral/util/util.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/relative_threshhold.hpp>
#include <astral/util/scale_translate.hpp>
#include <astral/contour_curve.hpp>

namespace astral
{
  ///@cond
  class RenderEngine;
  class StaticData;
  class Contour;
  class ContourCurve;
  class Path;
  class CombinedPath;
  ///@endcond

/*!\addtogroup Renderer
 * @{
 */

  /*!
   * \brief
   * A ItemPath represents a sequence of layers, each layer
   * representing a path filled with a color that are all rendered
   * together with a single rect. The typical use case is for
   * rendering glyphs or rendering paths where the path occupies
   * a small region on the screen. For that latter case, it is
   * expected that the ItemPath will rendering the path filled
   * better (in terms of performance) than using the STC algorithm.
   *
   * The geometry of the path data is scaled and translated
   * to the rect [-1, 1]x[-1, 1]. Each layer stores the
   * transformation from the logical coordinate system of
   * the input astral::CombinedPath to the unit rects.
   * The shader is expected to do an early out if the point
   * is outside of the path's bounding box.
   */
  class ItemPath:public reference_counted<ItemPath>::non_concurrent
  {
  public:
    /*!
     * \brief
     * A Geometry specifies the unchanging geometry
     * of an astral::ItemPath
     */
    class Geometry
    {
    public:
      /*!
       * Ctor. Initializes the Geometry as empty
       */
      Geometry(void);

      ~Geometry();

      /*!
       * Add the curves of a single contour.
       * \param tr transformation to apply to the contour
       * \param curves curves to add; it is required that for
       *               each astral::ContourCurve in the array
       *               that astral::ContourCurve::type() is NOT
       *               astral::ContourCurve::cubic_bezier, but
       *               all other curve types are supported.
       * \param is_closed if false, a closing line segment will
       *                  be added to close the curve
       * \param bb if non-null, gives the bounding box of curves
       */
      Geometry&
      add(const Transformation &tr, c_array<const ContourCurve> curves,
          bool is_closed, const BoundingBox<float> *bb = nullptr);

      /*!
       * Add a single astral::Contour
       * \param tr transformation to apply to the contour
       * \param contour contour holding curves
       * \param tol tolerance in coordinate system of
       *            coordiantes fed to the transformation tr
       *            approximate cubic bezier curves
       */
      Geometry&
      add(const Transformation &tr, const Contour &contour, float tol);

      /*!
       * Add a single astral::Contour
       * \param tr transformation to apply to the contour
       * \param contour contour holding curves
       * \param tol relative toleranace at which to
       *            approximate cubic bezier curves
       */
      Geometry&
      add(const Transformation &tr, const Contour &contour, RelativeThreshhold tol);

      /*!
       * Overload for ease of use, equivalent to
       * \code
       * add(Transformation(), contour, tol);
       * \endcode
       * \param contour contour holding curves
       * \param tol toleranace in coordinate system of
       *            the astral::Contour at which to
       *            approximate cubic bezier curves
       */
      Geometry&
      add(const Contour &contour, float tol)
      {
        return add(Transformation(), contour, tol);
      }

      /*!
       * Overload for ease of use, equivalent to
       * \code
       * add(Transformation(), contour, tol);
       * \endcode
       * \param contour contour holding curves
       * \param tol relative toleranace at which to
       *            approximate cubic bezier curves
       */
      Geometry&
      add(const Contour &contour, RelativeThreshhold tol)
      {
        return add(Transformation(), contour, tol);
      }

      /*!
       * Add an astral::CombinedPath, ignoring animated path
       * values.
       * \param path path(s) to add to the layer; animated
       *             path values are ignored.
       * \param tol toleranace in coordinate system of
       *            the astral::CombinedPath at which to
       *            approximate cubic bezier curves
       */
      Geometry&
      add(const CombinedPath &path, float tol);

      /*!
       * Add an astral::CombinedPath, ignoring animated path
       * values.
       * \param path path(s) to add to the layer; animated
       *             path values are ignored.
       * \param tol relative toleranace at which to
       *            approximate cubic bezier curves
       */
      Geometry&
      add(const CombinedPath &path, RelativeThreshhold tol);

    private:
      friend class ItemPath;

      class InputCurve;
      std::vector<InputCurve> m_curves;
      BoundingBox<float> m_bb;
      float m_error;
    };

    /*!
     * \brief
     * Class to specify how to generate the astral::ItemPath data
     */
    class GenerationParams
    {
    public:
      /*!
       * Ctor.
       */
      GenerationParams(void):
        m_cost(4.0f),
        m_max_recursion(4u)
      {}

      /*!
       * Set \ref m_cost.
       */
      GenerationParams&
      cost(float v)
      {
        m_cost = v;
        return *this;
      }

      /*!
       * Set \ref m_max_recursion.
       */
      GenerationParams&
      max_recursion(unsigned int v)
      {
        m_max_recursion = v;
        return *this;
      }

      /*!
       * Specifies the cost threshhold. If the average number
       * of curves that need to be tested within a band exceeds
       * this value, then partiion the band. The default value
       * is 4.0.
       */
      float m_cost;

      /*!
       * Specifies the maximum level of recursion used to split
       * a band to get within the \ref m_cost. Default value is 4.
       */
      unsigned int m_max_recursion;
    };

    /*!
     * Class to describe the properties of the realization
     * of an astral::ItemPath.
     */
    class Properties
    {
    public:
      /*!
       * The error between the geometry of a layer
       * and its realization; the error comes from
       * approximating cubic bezier curves by
       * sequences of quadratic bezier curves.
       */
      float m_error;

      /*!
       * Gives the bounding box of the layer
       */
      BoundingBox<float> m_bb;

      /*!
       * Gives the number of horizontal
       * and vertical bands of the layer
       */
      uvec2 m_number_bands;

      /*!
       * Gives the number of vec4-fp16 data the layer
       * requires.
       */
      unsigned int m_fp16_data_size;

      /*!
       * Gives the number of gvec4 data the layer
       * requires.
       */
      unsigned int m_generic_data_size;

      /*!
       * Gives the average number of curves over the
       * area of the layer that are used to compute
       * a winding number. The .x() holds the value
       * for horizontal bands and the .y() holds the
       * value for vertical bands.
       */
      vec2 m_average_render_cost;
    };

    /*!
     * \brief
     * An astral::ItemPath::Layer is a selection of a
     * single astral::ItemPath and how to render it.
     */
    class Layer
    {
    public:
      /*!
       * Ctor
       */
      Layer(const ItemPath &p):
        m_color(1.0f, 1.0f, 1.0f, 1.0f),
        m_item_path(p),
        m_fill_rule(nonzero_fill_rule)
      {}

      /*!
       * Set the value of \ref m_color.
       */
      Layer&
      color(const vec4 &v)
      {
        m_color = v;
        return *this;
      }

      /*!
       * Set the value of \ref m_transformation.
       */
      Layer&
      transformation(const ScaleTranslate &v)
      {
        m_transformation = v;
        return *this;
      }

      /*!
       * Set the value of \ref m_fill_rule.
       */
      Layer&
      fill_rule(enum fill_rule_t v)
      {
        m_fill_rule = v;
        return *this;
      }

      /*!
       * The color with which to render the layer with
       * alpha not pre-multiplied.
       */
      vec4 m_color;

      /*!
       * The astral::ItemPath object to use
       */
      const ItemPath &m_item_path;

      /*!
       * The fill rule to apply to the layer
       */
      enum fill_rule_t m_fill_rule;

      /*!
       * The transformation to apply to the layer
       */
      ScaleTranslate m_transformation;
    };

    /*!
     * Ctor.
     * \param geometry the geometry of the path
     * \param params specifies how the item data is realized
     */
    static
    reference_counted_ptr<ItemPath>
    create(const Geometry &geometry,
           GenerationParams params = GenerationParams())
    {
      return ASTRALnew ItemPath(geometry, params);
    }

    ~ItemPath();

    /*!
     * Returns the the properties of the astral::ItemPath.
     */
    const Properties&
    properties(void) const
    {
      return m_properties;
    }

    /*!
     * Pack a sequence of astral::ItemPath::Layer values.
     * This data can be used to either realize an
     * astral::ItemData or an astral::StaticData.
     *
     * The data is packed as follows.
     * - [0].xyzw.f Layer0 color
     * - [1].x.f ScaleTranslate::m_scale.x to transformation to apply to go from
     *           logical to coordinate system of curves of the layer
     * - [1].y.f ScaleTranslate::m_scale.y to transformation to apply to go from
     *           logical to coordinate system of curves of the layer
     * - [1].z.f ScaleTranslate::m_translate.x to transformation to apply to go from
     *           logical to coordinate system of curves of the layer
     * - [1].w.f ScaleTranslate::m_translate.y to transformation to apply to go from
     *           logical to coordinate system of curves of the layer
     * - [2].x.u Layer0 offset to header (headers live in generic data store)
     * - [2].z.u Layer0 fill rule
     * - [2].w.f Layer0 is last layer marker (negative value indicates is last layer)
     * - [3].xyzw.f Layer1 color
     * - [3].x.u Layer1 offset to header (headers live in generic data store)
     * - [3].z.u Layer1 fill rule
     * - [3].w.f Layer1 is last layer marker (negative value indicates is last layer)
     * - .
     * - .
     * .
     *
     * Where the header lives in the generic data store is packed
     * as
     * - [0].x.u number of horizontal bands (NH)
     * - [0].y.u number of vertical bands (NV)
     * - [1].x.u offset to horizontal band #0, min-side, band is in fp16 data
     * - [1].y.u number of curve in horizontal band #0, min-side
     * - [1].z.u offset to horizontal band #0, max-side, band is in fp16 data
     * - [1].w.u number of curve in horizontal band #0, max-side
     * - [2].x.u offset to horizontal band #1, min-side, band is in fp16 data
     * - [2].y.u number of curve in horizontal band #1, min-side
     * - [2].z.u offset to horizontal band #1, max-side, band is in fp16 data
     * - [2].w.u number of curve in horizontal band #1, max-side
     * - .
     * - .
     * - [1 + NH].x.u offset to vertical band #0, min-side, band is in fp16 data
     * - [1 + NH].y.u number of curve in vertical band #0, min-side
     * - [1 + NH].z.u offset to vertical band #0, max-side, band is in fp16 data
     * - [1 + NH].w.u number of curve in vertical band #0, max-side
     * - [2 + NH].x.u offset to vertical band #1, min-side, band is in fp16 data
     * - [2 + NH].y.u number of curve in vertical band #1, min-side
     * - [2 + NH].z.u offset to vertical band #1, max-side, band is in fp16 data
     * - [2 + NH].w.u number of curve in vertical band #1, max-side
     * - .
     * - .
     * .
     *
     * The bands are packed as follows in the fp16 data
     * - [0].xy curve0 ContourCurve::start_point()
     * - [0].zw curve0 ContourCurve::control_point()
     * - [1].xy curve0 ContourCurve::end_point()
     * - [1].z  curve0 ContourCurve::conic_weight()
     * - [1].w  early out value (value from correct side and
     *   coordinate of the bounding box to give the shader
     *   an early out to which to compare against.
     * .
     * with that the curves for vertical bands have their x and y
     * coordinates exchanged so that the shader can use same code
     * on horizontal and vertical bands. In addition, to reduce
     * shader code differences for the min-side and max-side
     * of the band, the written x-coordinate is always positive
     * as well and the shader should feed in abs(path_coord.P)
     * where P is x for horizontal bands and y for vertical
     * bands.
     *
     * The packed data is only valid as long as the astral::ItemPath
     * objects used in the ItemPath::Layer objects are alive.
     *
     * \param engine the astral::RenderEngine to allocate astral::StaticData
     * \param layers what layer to use
     * \param dst lcoation to which to pack the data
     * \returns the bounding box to contain all of the layers
     */
    static
    BoundingBox<float>
    pack_data(RenderEngine &engine,
              c_array<const Layer> layers, c_array<gvec4> dst);

    /*!
     * Returns the size of the array needed for pack_data().
     * \param number_layers the number of layers
     */
    static
    unsigned int
    data_size(unsigned int number_layers)
    {
      return 3u * number_layers;
    }

  private:
    class BandRegion;
    class BandedCurve;
    class Band;

    ItemPath(const Geometry &geometry, GenerationParams params);

    /* Upload to the GPU is delayed until the first
     * time it is needed
     */
    uint32_t
    header_location(RenderEngine &engine) const;

    GenerationParams m_params;
    Properties m_properties;

    /* transformation from coordinates of Geometry
     * to [-1, 1]x[-1, 1] how the geometty is packed
     * into the item data
     */
    ScaleTranslate m_tr;

    mutable std::vector<gvec4> m_header_data;
    mutable std::vector<vec4> m_band_data;
    mutable reference_counted_ptr<const StaticData> m_header;
    mutable reference_counted_ptr<const StaticData> m_bands;
  };
/*! @} */
}

#endif

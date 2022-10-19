/*!
 * \file layered_rect_atlas.hpp
 * \brief file layered_rect_atlas.hpp
 *
 * Adapted from: WRATHAtlasBase.hpp and WRATHAtlas.hpp of WRATH:
 *
 * Copyright 2013 by Nomovok Ltd.
 * Contact: info@nomovok.com
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#ifndef ASTRAL_LAYERED_RECT_ATLAS_HPP
#define ASTRAL_LAYERED_RECT_ATLAS_HPP

#include <astral/util/util.hpp>
#include <astral/util/reference_counted.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/c_array.hpp>


namespace astral {

/*!\addtogroup Utility
 * @{
 */

  /*!
   * \brief
   * A LayeredRectAtlas allocates rectangular regions from an
   * arrayof same sized rectanglur regions. Its main purpose
   * is to implement classic texture atlasing.
   *
   * TODO: make this private but reusable
   */
  class LayeredRectAtlas:public reference_counted<LayeredRectAtlas>::non_concurrent
  {
  public:
    /*!
     * \brief
     * A handle to allocating a region from the LayeredRectAtlas.
     */
    class Entry
    {
    public:
      /*!
       * Ctor initializes \ref Entry as invalid.
       */
      Entry(void):
        m_rectangle(nullptr)
      {}

      /*!
       * Returns true if and only if this \ref Entry
       * is valid.
       */
      bool
      valid(void) const
      {
        return m_rectangle != nullptr;
      }

      /*!
       * Returns the location of this \ref Entry
       * within the layer where it is located
       */
      ivec2
      location(void) const;

      /*!
       * Returns the size of this \ref Entry
       */
      ivec2
      dimensions(void) const;

      /*!
       * Returns what layer this \ref Entry resides
       */
      unsigned int
      layer(void) const;

    private:
      friend class LayeredRectAtlas;
      class Rectangle;

      Rectangle *m_rectangle;
    };

    /*!
     * Create and return a new astral::LayeredRectAtlas object.
     */
    static
    reference_counted_ptr<LayeredRectAtlas>
    create(void);

    virtual
    ~LayeredRectAtlas();

    /*!
     * Clears the LayeredRectAtlas, in doing so all Entry values
     * returned by \ref allocate_rectangle() must be discarded;
     * Provided as a conveniance, equivalent to
     * \code
     * clear(dimensions, number_layers());
     * \endcode
     * \param dimensions new dimensions size for each layer
     */
    void
    clear(ivec2 dimensions)
    {
      clear(dimensions, number_layers());
    }

    /*!
     * Clears the LayeredRectAtlas, in doing so all Entry values
     * returned by \ref allocate_rectangle() must be discarded.
     * Provided as a conveniance, equivalent to
     * \code
     * clear(dimensions(), number_layers());
     * \endcode
     */
    void
    clear(void)
    {
      clear(dimensions(), number_layers());
    }

    /*!
     * Clears the LayeredRectAtlas, in doing so all Entry values
     * returned by \ref allocate_rectangle() must be discarded.
     * \param dimensions new dimensions size for each layer
     * \param num_layers number of layers
     */
    void
    clear(ivec2 dimensions, unsigned int num_layers);

    /*!
     * Change the number of layers that the
     * LayeredRectAtlas has. This value cannot
     * shring the number of layers. To shrink
     * the number of layers, that can only
     * be done from \ref clear(ivec2, unsigned int).
     */
    void
    number_layers(unsigned int);

    /*!
     * Returns the number of layers that the
     * LayeredRectAtlas has
     */
    unsigned int
    number_layers(void) const;

    /*!
     * Returns the dimensions of each layer.
     */
    ivec2
    dimensions(void) const;

    /*!
     * Returns the a handle giving the location where
     * in the RectAtlas. Failure is indicated by if the
     * return \ref Entry has Entry::valid() return false.
     * \param dimension width and height of the rectangle
     */
    Entry
    allocate_rectangle(const ivec2 &dimension);

    /*!
     * Mark a rectangle as free in the LayeredRectAtlas.
     * On return, the passed Entry value must be discarded.
     */
    void
    free_rectangle(Entry);

    /*!
     * Mark multiple rectangles as free in the
     * LayeredRectAtlas; this will be more efficient
     * than freeing each rectangle one at a time.
     */
    void
    free_rectangles(c_array<const Entry>);

    /*!
     * Add a region to the atlas that can be used to allocate
     * space via allocate_rectangle(). The region can be part of
     * a region in a rectange returned by allocate_rectangle()
     * as well. Calling any of the clear() removes the added
     * user regions from the available list of freespace.
     *
     * NOTE: it is an error to pass a region that intersects
     *       the region specified by an \ref Entry if that
     *       Entry has been or will be passed to free_rectangle()
     *       or free_rectangles().
     *
     * \param location of min-min corner of the region
     * \param dimensions the size of the region
     * \param layer the layer of the region
     */
    void
    add_user_region(ivec2 location, ivec2 dimensions, unsigned int layer);

  private:
    class Implement;

    LayeredRectAtlas(void);
  };
/*! @} */

} //namespace astral

#endif

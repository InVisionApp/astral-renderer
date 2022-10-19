/*!
 * \file colorstop_sequence_atlas.hpp
 * \brief file colorstop_sequence_atlas.hpp
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

#ifndef ASTRAL_COLOR_STOP_SEQUENCE_ATLAS_HPP
#define ASTRAL_COLOR_STOP_SEQUENCE_ATLAS_HPP

#include <vector>
#include <map>
#include <algorithm>

#include <astral/util/reference_counted.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/color.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/interval_allocator.hpp>
#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/colorstop.hpp>

namespace astral
{
/*!\addtogroup RendererBackend
 * @{
 */

  ///@cond
  class ColorStopSequence;
  ///@endcond

  /*!
   * \brief
   * A ColorStopSequenceAtlasBacking represents the 3D API implementation
   * of the backing of an astral::ColorStopSequenceAtlas. The main reason for
   * its seperation from astral::ColorStopSequenceAtlas is to allow for \ref
   * astral::ColorStopSequenceAtlas to resize and potentially repack where all
   * \ref astral::ColorStopSequence values are realized in the backing store.
   */
  class ColorStopSequenceAtlasBacking:public reference_counted<ColorStopSequenceAtlasBacking>::non_concurrent
  {
  public:
    virtual
    ~ColorStopSequenceAtlasBacking()
    {}

    /*!
     * To be implemented by a derived class to load pixels
     * to the 3D API backing resource
     * \param layer which layer
     * \param start start of regions to load pixels
     * \param pixels actual pixel values.
     */
    virtual
    void
    load_pixels(int layer, int start, c_array<const u8vec4> pixels) = 0;

    /*!
     * Returns the size of each layer.
     */
    unsigned int
    layer_dimensions(void) const
    {
      return m_layer_dimensions;
    }

    /*!
     * Returns the number of layers that this
     * ColorStopSequenceAtlasBacking has.
     */
    unsigned int
    number_layers(void) const
    {
      return m_number_layers;
    }

    /*!
     * Increase the number of layer to atleast the passed
     * value; returns the number of layers after the resize.
     */
    unsigned int
    resize(unsigned int L)
    {
      ASTRALassert(L >= m_number_layers);
      m_number_layers = on_resize(L);
      return m_number_layers;
    }

  protected:
    /*!
     * To be implemened by a derived class to increase
     * the number of layer to atleast the passed value;
     * returns the number of layers after the resize.
     * The value of number_layers() is the number of layers
     * before the resize.
     */
    virtual
    unsigned int
    on_resize(unsigned int L) = 0;

    /*!
     * Protected ctor for derived classes.
     * \param num_layers the number of layers
     * \param layer_dims the dimensions of each of the layers
     */
    ColorStopSequenceAtlasBacking(unsigned int num_layers, unsigned int layer_dims):
      m_number_layers(num_layers),
      m_layer_dimensions(layer_dims)
    {}

  private:
    unsigned int m_number_layers;
    unsigned int m_layer_dimensions;
  };

  /*!
   * \brief
   * An astral::ColorStopSequenceAtlas represents the object that provides the backing
   * store for pixels of \ref astral::ColorStopSequence objects.
   */
  class ColorStopSequenceAtlas:public reference_counted<ColorStopSequenceAtlas>::non_concurrent
  {
  public:
    /*!
     * Ctor.
     * \param backing astral::ColorStopSequenceAltasBacking that stores the image data.
     */
    static
    reference_counted_ptr<ColorStopSequenceAtlas>
    create(const reference_counted_ptr<ColorStopSequenceAtlasBacking> &backing)
    {
      return ASTRALnew ColorStopSequenceAtlas(backing);
    }

    ~ColorStopSequenceAtlas();

    /*!
     * Create a ColorStopSequence from an array of color-stops where the
     * colors are interpolated in linear space along the gradient.
     *
     * NOTE: it is unsafe to call this outside of a Renderer::begin()/Renderer::end()
     *       pair in environments where the 3D API state is affected by anything
     *       else besides Astral.
     *
     * \param colorstops array of colorstops, cannot be empty
     * \param num_texels if non-zero, specifies how many texels the
     *                   1D texture representing the sequence will occupy.
     *                   A value of zero indicates that the atlas will
     *                   decide this value itself.
     */
    reference_counted_ptr<ColorStopSequence>
    create(c_array<const ColorStop<FixedPointColorLinear>> colorstops,
           unsigned int num_texels = 0u);

    /*!
     * Create a ColorStopSequence from an array of color-stops where the
     * colors are interpolated in sRGB space along the gradient.
     *
     * NOTE: it is unsafe to call this outside of a Renderer::begin()/Renderer::end()
     *       pair in environments where the 3D API state is affected by anything
     *       else besides Astral.
     *
     * \param colorstops array of colorstops, cannot be empty
     * \param num_texels if non-zero, specifies how many texels the
     *                   1D texture representing the sequence will occupy.
     *                   A value of zero indicates that the atlas will
     *                   decide this value itself.
     */
    reference_counted_ptr<ColorStopSequence>
    create(c_array<const ColorStop<FixedPointColor_sRGB>> colorstops,
           unsigned int num_texels = 0u);

    /*!
     * Create a ColorStopSequence from an array of color-stops.
     *
     * NOTE: it is unsafe to call this outside of a Renderer::begin()/Renderer::end()
     *       pair in environments where the 3D API state is affected by anything
     *       else besides Astral.
     *
     * \param colorstops array of colorstops, cannot be empty
     * \param colorspace color space in which to interpolate the colors along the gradient
     * \param num_texels if non-zero, specifies how many texels the
     *                   1D texture representing the sequence will occupy.
     *                   A value of zero indicates that the atlas will
     *                   decide this value itself.
     */
    reference_counted_ptr<ColorStopSequence>
    create(c_array<const ColorStop<vec4>> colorstops, enum colorspace_t colorspace,
           unsigned int num_texels = 0u);

    /*!
     * During a lock_resources()/unlock_resources() pair,
     * rather than freeing the regions of deleted \ref ColorStopSequence
     * objects directly, the regions are only marked to be
     * free and will be released on unlock_resources().
     * The use case is that during a Renderer::begin()/end()
     * pair, a \ref ColorStopSequence is used and its last reference
     * goes out of scope triggering its deletion, but because
     * the GPU still has not been sent the GPU commands to
     * draw, the pixels are needed until Renderer::end().
     * Thus, Renderer uses lock_resources() to keep the pixels
     * "alive" until it sends them to the GPU.
     */
    void
    lock_resources(void);

    /*!
     * Release the regions marked for deletion since
     * lock_rerources() was called.
     */
    void
    unlock_resources(void);

    /*!
     * Returns the backing of this ColorStopSequenceAtlas
     */
    const ColorStopSequenceAtlasBacking&
    backing(void) const
    {
      return *m_backing;
    }

  private:
    friend class ColorStopSequence;
    class MemoryPool;

    explicit
    ColorStopSequenceAtlas(const reference_counted_ptr<ColorStopSequenceAtlasBacking> &backing);

    void
    deallocate_region(const IntervalAllocator::Interval *interval);

    const IntervalAllocator::Interval*
    allocate_region(c_array<const u8vec4> colors);

    reference_counted_ptr<ColorStopSequenceAtlasBacking> m_backing;
    IntervalAllocator m_interval_allocator;
    std::vector<const IntervalAllocator::Interval*> m_delayed_frees;
    int m_lock_resources;
    MemoryPool *m_pool;
  };

/*! @} */

}

#endif

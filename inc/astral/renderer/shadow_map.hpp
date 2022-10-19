/*!
 * \file shadow_map.hpp
 * \brief file shadow_map.hpp
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

#ifndef ASTRAL_SHADOW_MAP_HPP
#define ASTRAL_SHADOW_MAP_HPP

#include <astral/util/reference_counted.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/interval_allocator.hpp>
#include <astral/renderer/render_target.hpp>
#include <astral/renderer/shadow_map_id.hpp>

namespace astral
{
  class Renderer;
  class ShadowMap;
  class ShadowMapID;
  class ShadowMapAtlas;
  class ShadowMapAtlasBacking;

  namespace detail
  {
    ///@cond

    /* The only purpose of this class is to allow
     * for Renderer to set m_offscreen_render_index
     * of ShadowMap without being a friend of it.
     *
     * Behavior:
     *   - passing value != InvalidRenderValue also
     *     sets render_generated  as true and marks
     *     the ShadowMap as in use. May only do so
     *     if m_offscreen_render_index is equal to
     *     InvalidRenderValue
     *   - passing value == InvalidRenderValue also
     *     marks the the ShadowMap as NOT in used.
     *     May only do so if m_offscreen_render_index
     *     is NOT InvalidRenderValue
     */
    class MarkShadowMapAsRenderTarget
    {
    private:
      friend class astral::ShadowMap;
      friend class astral::Renderer;

      explicit
      MarkShadowMapAsRenderTarget(uint32_t idx):
        m_offscreen_render_index(idx)
      {}

      uint32_t m_offscreen_render_index;
    };

    ///@endcond
  }

  /*!
   * An astral::ShadowMapAtlasBacking represents the
   * GPU backing for astral::ShadowMap objects.
   */
  class ShadowMapAtlasBacking:public reference_counted<ShadowMapAtlasBacking>::non_concurrent
  {
  public:
    /*!
     * Ctor.
     * \param width the width of the backing store, this value is immutable
     * \param height the height of the backing store, this value is mutable,
     *               this value must be a multiple of four
     */
    ShadowMapAtlasBacking(unsigned int width, unsigned int height):
      m_width(width),
      m_height(height)
    {
    }

    virtual
    ~ShadowMapAtlasBacking()
    {}

    /*!
     * Resize the atlas to be atleast the
     * specified size. Returns the actual
     * height after the resize.
     */
    unsigned int
    height(unsigned int L)
    {
      ASTRALassert(L >= height());
      m_height = on_resize(L);
      return m_height;
    }

    /*!
     * Returns the number of layer of the backing store
     */
    unsigned int
    height(void) const
    {
      return m_height;
    }

    /*!
     * Returns the width of the backing store
     */
    unsigned int
    width(void) const
    {
      return m_width;
    }

    /*!
     * To be implemented by a derived class to flush copies
     * accumulated by copy_pixels() to the backing.
     */
    virtual
    void
    flush_gpu(void) = 0;

    /*!
     * To be implemented by a derived class to copy pixels
     * from an astral::DepthBuffer to this backing.
     */
    virtual
    void
    copy_pixels(uvec2 dst_location, uvec2 size,
                DepthStencilBuffer &src, uvec2 src_location) = 0;

    /*!
     * To be implemented by a derived class to return the
     * astral::RenderTarget to which to render depth content.
     * This value may only change as a response to on_resize()
     * getting called.
     *
     * TODO: support the ability for a backing to have layers,
     *       this will mean that we need a value for the number
     *       of layers per render target (perferablly a power of
     *       two) and from that we can get the render target
     *       quickly from the y-coordinate.
     */
    virtual
    reference_counted_ptr<RenderTarget>
    render_target(void) const = 0;

  protected:
    /*!
     * To be implemented by a derived class to resize the atlas.
     * The value of height() is the value -before- the
     * resize is to occur. To return the actual height after
     * teh resize. The return value must be even.
     */
    virtual
    unsigned int
    on_resize(unsigned int new_height) = 0;

  private:
    unsigned int m_width, m_height;
  };

  /*!
   * An astral::ShadowMapAtlas represents the atlas that manages
   * astral::ShadowMap objects. An astral::ShadowMap consists
   * of four virtual 1D textures. To reduce the geometry load to
   * generate the four virtual 1D textures, the four are put into
   * pairs
   *  - min-y side <-----> backing @ [A.x(), A.x() + D] x {A.y() + 0}
   *  - max-y side <-----> backing @ [A.x(), A.x() + D] x {A.y() + 1}
   *  - min-x side <-----> backing @ [A.x(), A.x() + D] x {A.y() + 2}
   *  - max-x side <-----> backing @ [A.x(), A.x() + D] x {A.y() + 3}
   *  .
   * where
   *   - A = astral::ShadowMap::atlas_location()
   *   - D is astral::ShadowMap::dimensions()
   *
   * In contrast to astral::Image, rendering of an astral::ShadowMap
   * is done directly to the GPU resource. As such, when an astral::ShadowMap
   * is rendered, astral::Renderer must first clear the texels directly.
   */
  class ShadowMapAtlas:public reference_counted<ShadowMapAtlas>::non_concurrent
  {
  public:
    /*!
     * Ctor.
     * \param backing backing for the texels
     */
    static
    reference_counted_ptr<ShadowMapAtlas>
    create(ShadowMapAtlasBacking &backing)
    {
      return ASTRALnew ShadowMapAtlas(backing);
    }

    ~ShadowMapAtlas();

    /*!
     * Allocate an astral::ShadowMap
     * \param D width and height of area in pixels to compute occlusion
     * \param light_position the light position in shadowmap coordinates
     *                       that generates the shadowmap.
     */
    reference_counted_ptr<ShadowMap>
    create(int D, const vec2 &light_position);

    /*!
     * During a lock_resources()/unlock_resources() pair,
     * rather than freeing the regions of release tile
     * objects directly, the regions are only marked to be
     * free and will be released on unlock_resources(). The
     * use case is that during a Renderer::begin()/end()
     * pair, an astral::ShadowMap is used and its last
     * reference goes out of scope triggering its deletion,
     * but because the GPU still has not been sent the GPU
     * commands to draw, the pixels are needed until Renderer::end().
     * Thus, Renderer uses lock_resources() to keep the
     * pixels "alive" until it sends them to the GPU.
     *
     * Nesting is supported (though ill-advised) and resources
     * are cleared on the call to the outer-most unlock_resources().
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
     * Return the astral::ShadowMapAtlasBacking
     */
    ShadowMapAtlasBacking&
    backing(void)
    {
      return *m_backing;
    }

    /*!
     * Returns the astral::RenderTarget to which to render
     * depth content; the render target should NOT have
     * a color buffer attached to it.
     */
    const reference_counted_ptr<RenderTarget>&
    render_target(void) const
    {
      return m_render_target;
    }

    /*!
     * Return an astral::ShadowMap from a unique Image ID; if the
     * astral::ShadowMap is not alive with the passed ID, or if the
     * ID is not valid, will return nullptr.
     */
    ShadowMap*
    fetch_shadow_map(ShadowMapID ID);

  private:
    friend class ShadowMap;
    class MemoryPool;

    explicit
    ShadowMapAtlas(ShadowMapAtlasBacking &backing);

    const IntervalAllocator::Interval*
    allocate_interval(unsigned int sz);

    void
    release(const IntervalAllocator::Interval*);

    void
    free_shadow_map_id(ShadowMap *p);

    ShadowMapID
    allocate_shadow_map_id(ShadowMap *p);

    /* the buggers that actually hold the texels */
    reference_counted_ptr<ShadowMapAtlasBacking> m_backing;

    /* tracker for what texels are in use and are free;
     * IntervalAllocator::layer_length() is constant at
     * ShadowMapAtlasBacking::width_height().x(). One
     * wrinkle is that a light shadow is Wx2 pixels. In
     * addition, m_interval_allocator only does layers,
     * thus there is additional futzing to change the
     * IntervalAllocator::Interval::layer() into .y()
     */
    IntervalAllocator m_interval_allocator;

    /* incremented on lock_resources() and decremented on unlock_resources() */
    int m_resources_locked;

    /* incremented each time m_resources_locked is decremented to zero */
    uint64_t m_resources_unlock_count;

    /* requested free's during resource lock */
    std::vector<const IntervalAllocator::Interval*> m_delayed_frees;

    /* cache the RenderTarget of the backing */
    reference_counted_ptr<RenderTarget> m_render_target;

    /* list of free ShadowMapID available */
    std::vector<ShadowMapID> m_free_ids;

    /* get an image from ShadowMapID by grabbing
     * m_image_fetcher[ShadowMapID::m_slot]
     */
    std::vector<ShadowMap*> m_fetcher;

    /* pool to allocate ShadowMap objects */
    MemoryPool *m_pool;
  };

  /*!
   * An astral::ShadowMap represents four virtual 1D-texture
   * to be used for shadow casting of a light in a 2D scene.
   * The shadow map is so that the center of the shadow map is
   * the light. Let L be the location of the light in the same
   * coordinate system as p, Then the shader code to determine
   * if a point p is occluded is as follows
   * \code
   * T = atlas_location();
   * D = dimensions();
   *
   * //get the vector from the light to the point p.
   * v = p - L;
   *
   * // compute which side from which to sample
   * if (abs(v.x) > abs(v.y))
   *   {
   *      // min-x and max-x side are at +2 and +3
   *      T.y += 2u;
   *
   *      raw_signed_distance = v.x;
   *      virtual_texel_coordinate = v.y;
   *   }
   * else
   *   {
   *      raw_signed_distance = v.y;
   *      virtual_texel_coordinate = v.x;
   *   }
   *
   * // compute the depth value of p relative to the light
   * // the depth value is just the distance from the point
   * // p to the correct side of the shadow map normalized
   * // so that the distance from the light to that side is
   * // unity.
   * //
   * // the value of comptue_depth(r) is determined by the
   * // "depth" projection done in computing the shadow map.
   * // Examples:
   * //    compute_depth(t) = t / (1 + t)
   * //    compute_depth(t) = min(1, t * inverse_max)
   * p_depth = compute_depth(abs(raw_signed_distance));
   *
   * // adjust T for if on the max-side
   * if (raw_signed_distance > 0.0)
   *   {
   *      T.y += 1;
   *   }
   *
   * // perform "perspective divide" on the virtual_texel_coordinate
   * virtual_texel_coordinate /= abs(raw_signed_distance);
   *
   * //normalize virtual_texel_coordinate to [0, D]
   * d = (virtual_texel_coordinate + 1.0) * 0.5 * D;
   *
   * // perform the depth fetch; if the rect shadow map is a
   * // lower resolution, use a filtered fetch to give very
   * // fake soft shadows
   * depth = texelFetch(lightShadowTexture, ivec2(T.x + d, T.y), 0);
   *
   * // the light is occluded at p if depth is smaller than p_depth
   * occluded = (depth < p_depth);
   *
   * \endcode
   */
  class ShadowMap:public reference_counted<ShadowMap>::non_concurrent_custom_delete
  {
  public:
    /*!
     * Returns the location in the backing atlas
     */
    uvec2
    atlas_location(void) const
    {
      return m_atlas_location;
    }

    /*!
     * Returns the size of each virtual texture of the
     * ShadowMap.
     */
    unsigned int
    dimensions(void) const
    {
      return m_dimensions;
    }

    /*!
     * Returns the position of the light that generated the shadowmap
     * in shadow map coordinates
     */
    const vec2&
    light_position(void) const
    {
      return m_light_position;
    }

    /*!
     * Mark this astral::ShadowMap as in use until resources are
     * unlocked by ShadowMapAtlas::unlock_resources(). When a
     * shadow map is marked as in use, it is illegal to change its content.
     */
    void
    mark_in_use(void)
    {
      m_in_use_marker = m_atlas->m_resources_unlock_count + 1u;
    }

    /*!
     * Returns true if the astral::ShadowMap is marked as in use,
     * see also mark_in_use(). An astral::ShadowMap is marked as
     * in use by astral::RenderEncoderBase (and its related classes)
     * if the astral::ShadowMap is used in a draw. The object
     * will cease to be marked as in use as soon as the Renderer::end()
     * is issued to send the draw commands to the GPU backend.
     */
    bool
    in_use(void) const
    {
      return m_in_use_marker > m_atlas->m_resources_unlock_count;
    }

    /*!
     * Returns the unique ID for this astral::ShadowMap. ID values
     * are unique for the lifetime of the astral::ShadowMap, i.e.
     * no two alive different astral::ShadowMap objects will have
     * the same ID, but a created ID can have the ID of a previously
     * deleted astral::ShadowMap.
     */
    ShadowMapID
    ID(void) const
    {
      return m_shadow_map_id;
    }

    ///@cond
    uint32_t
    offscreen_render_index(void) const
    {
      return m_offscreen_render_index;
    }

    void
    mark_as_virtual_render_target(detail::MarkShadowMapAsRenderTarget v);

    static
    void
    delete_object(ShadowMap *p);
    ///@endcond

  private:
    friend class ShadowMapAtlas;

    ShadowMap(void)
    {}

    /* parent atlas */
    reference_counted_ptr<ShadowMapAtlas> m_atlas;

    /* position of light in ShadowMap coordinates */
    vec2 m_light_position;

    /* location holder for within the atlas */
    const IntervalAllocator::Interval *m_interval;

    /* derived from m_interval */
    uvec2 m_atlas_location;
    unsigned int m_dimensions;

    /* way to see if ShadowMap is in use */
    uint64_t m_in_use_marker;

    /* This value is an index into an array of render-order bookkeeping
     * data maintained by Renderer within a begin()/end() pair.
     */
    uint32_t m_offscreen_render_index;

    ShadowMapID m_shadow_map_id;
  };

}

#endif

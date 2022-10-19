/*!
 * \file renderer_workroom.cpp
 * \brief file renderer_workroom.cpp
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

#include <astral/renderer/renderer.hpp>

#include "renderer_virtual_buffer.hpp"
#include "renderer_storage.hpp"
#include "renderer_clip_geometry.hpp"
#include "renderer_workroom.hpp"

/* A Choose embodies a region allocator on a single rectangle
 * of size VirtualBuffer::render_scratch_buffer_size.
 */
class astral::Renderer::Implement::WorkRoom::ImageBufferLocation::Chooser:
  public reference_counted<Chooser>::non_concurrent
{
public:
  /* classes that implement ImageBufferLocationChooser */
  class UseLayeredRectAtlas;

  virtual
  ~Chooser()
  {}

  /* To be implemented by a derived class to declare that nothing
   * has been allocated from the region (whose width and height
   * are VirtualBuffer::render_scratch_buffer_size).
   */
  virtual
  void
  clear(void) = 0;

  /* Attempt to allocate a rectangle from the area. If
   * the allocation fails, return an ImageBufferLocation
   * indicating so.
   */
  virtual
  ImageBufferLocation
  allocate_rectangle(unsigned int width, unsigned int height) = 0;

  /* To be implemented by a derived class to sort
   * the VirtualBuffer's to make its allocation
   * better
   * \param renderer the Renderer object whose m_storage->virtual_buffers()
   *                 is all of the VirtualBuffers (do NOT sort that list)
   * \param virtual_buffer_ids array of indices into renderer.m_storage->virtual_buffers();
   *                           this is the array to sort and this array is a list
   *                           of only those buffers that are ready to be rendered.
   */
  virtual
  void
  sort_buffers(Renderer::Implement &renderer, c_array<unsigned int> virtual_buffer_ids) = 0;
};

class astral::Renderer::Implement::WorkRoom::ImageBufferLocation::Chooser::UseLayeredRectAtlas:
  public Chooser
{
public:
  UseLayeredRectAtlas(void)
  {
    m_allocator = LayeredRectAtlas::create();
  }

  virtual
  void
  clear(void) override final
  {
    m_allocator->clear(ivec2(VirtualBuffer::render_scratch_buffer_size), 1u);
  }

  virtual
  ImageBufferLocation
  allocate_rectangle(unsigned int width, unsigned int height) override final
  {
    LayeredRectAtlas::Entry E;

    /* prefer longer over taller */
    bool swap_dimensions(width <= height);

    if (swap_dimensions)
      {
        std::swap(width, height);
      }

    E = m_allocator->allocate_rectangle(ivec2(width, height));
    if (E.valid())
      {
        return ImageBufferLocation(swap_dimensions, E.location());
      }
    else
      {
        return ImageBufferLocation();
      }
  }

  virtual
  void
  sort_buffers(Renderer::Implement &renderer, c_array<unsigned int> virtual_buffer_ids) override final
  {
    std::sort(virtual_buffer_ids.begin(), virtual_buffer_ids.end(), VirtualBuffer::AreaSorter(renderer));
  }

private:
  reference_counted_ptr<LayeredRectAtlas> m_allocator;
};

class astral::Renderer::Implement::WorkRoom::BufferList::ReadyBufferHelperBase
{
public:
  virtual
  void
  prepare(Renderer::Implement &renderer, c_array<unsigned int> buffer_list) = 0;

  virtual
  bool
  allocate_offscreen_space(VirtualBuffer &buffer) = 0;
};

class astral::Renderer::Implement::WorkRoom::BufferList::ReadyImageBufferHelper:public ReadyBufferHelperBase
{
public:
  ReadyImageBufferHelper(ImageBufferLocation::Chooser *chooser):
    m_chooser(chooser)
  {
  }

  virtual
  void
  prepare(Renderer::Implement &renderer, c_array<unsigned int> buffer_list) override
  {
    m_chooser->sort_buffers(renderer, buffer_list);
    m_chooser->clear();
  }

  virtual
  bool
  allocate_offscreen_space(VirtualBuffer &buffer) override
  {
    ImageBufferLocation L;
    ivec2 sz;

    ASTRALassert(buffer.type() == VirtualBuffer::image_buffer || buffer.type() == VirtualBuffer::sub_image_buffer);
    sz = buffer.offscreen_render_size();

    ASTRALhard_assert(sz.x() <= VirtualBuffer::max_renderable_buffer_size && sz.y() <= VirtualBuffer::max_renderable_buffer_size);
    L = m_chooser->allocate_rectangle(sz.x(), sz.y());

    if (L.valid())
      {
        buffer.location_in_color_buffer(L);
        return true;
      }
    else
      {
        return false;
      }
  }

private:
  ImageBufferLocation::Chooser *m_chooser;
};

class astral::Renderer::Implement::WorkRoom::BufferList::ReadyShadowMapBufferHelper:public ReadyBufferHelperBase
{
public:
  ReadyShadowMapBufferHelper(IntervalAllocator &interval_allocator):
    m_interval_allocator(interval_allocator)
  {}

  virtual
  void
  prepare(Renderer::Implement &renderer, c_array<unsigned int> buffer_list) override
  {
    uvec2 offscreen_buffer_size(VirtualBuffer::render_scratch_buffer_size);

    /* sort the buffers in decreasing length order to help the region_allocator fit more */
    std::sort(buffer_list.begin(), buffer_list.end(), VirtualBuffer::ShadowSizeSorter(renderer));

    /* clear the allocator to start anew; note that the number of layers
     * is one forth the height of the offscreen buffer; this is because
     * each shadow map is four pixels high, one for each side.
     */
    m_interval_allocator.clear(offscreen_buffer_size.x(), offscreen_buffer_size.y() >> 2u);
  }

  virtual
  bool
  allocate_offscreen_space(VirtualBuffer &buffer) override
  {
    const IntervalAllocator::Interval *p;

    ASTRALassert(buffer.type() == VirtualBuffer::shadowmap_buffer);
    p = m_interval_allocator.allocate(buffer.shadow_map()->dimensions());
    if (p)
      {
        buffer.location_in_depth_buffer(uvec2(p->range().m_begin, 4u * p->layer()));
        return true;
      }
    else
      {
        return false;
      }
  }

private:
  IntervalAllocator &m_interval_allocator;
};

/////////////////////////////////////////////////////
// astral::Renderer::Implement::WorkRoom::ImageBufferList methods
astral::Renderer::Implement::WorkRoom::ImageBufferList::
ImageBufferList(void)
{
  m_region_allocator = ASTRALnew ImageBufferLocation::Chooser::UseLayeredRectAtlas();
}

astral::Renderer::Implement::WorkRoom::ImageBufferList::
~ImageBufferList(void)
{
}

astral::c_array<unsigned int>
astral::Renderer::Implement::WorkRoom::ImageBufferList::
choose_ready_buffers(Renderer::Implement &renderer)
{
  ImageBufferLocation::Chooser *chooser;

  chooser = m_region_allocator.get();

  ReadyImageBufferHelper helper(chooser);
  return choose_ready_buffers_common(renderer, &helper);
}

astral::c_array<unsigned int>
astral::Renderer::Implement::WorkRoom::ShadowMapBufferList::
choose_ready_buffers(Renderer::Implement &renderer)
{
  ReadyShadowMapBufferHelper helper(m_interval_allocator);
  return choose_ready_buffers_common(renderer, &helper);
}

astral::c_array<unsigned int>
astral::Renderer::Implement::WorkRoom::BufferList::
choose_ready_buffers_common(Renderer::Implement &renderer, ReadyBufferHelperBase *helper)
{
  /* initialize m_buffers_ready_to_render with those buffers
   * that could have been rendered on the last iteration but
   * did not fit in the offscreen color buffer
   */
  m_ready_to_render.swap(m_ready_to_render_later);

  /* sort the buffers remaining into two categories:
   *  - Buffers with dependencies unresolved: m_not_ready_to_render
   *  - Buffers with dependencies met : m_ready_to_render
   */
  m_not_ready_to_render.clear();
  for (auto i : m_remaining)
    {
      VirtualBuffer *b;

      b = &renderer.m_storage->virtual_buffer(i);
      if (b->remaining_dependencies() == 0)
        {
          m_ready_to_render.push_back(i);
        }
      else
        {
          m_not_ready_to_render.push_back(i);
        }
    }

  /* all buffers that are not yet ready are for the next loop iteration */
  m_remaining.swap(m_not_ready_to_render);

  /* now fit as many buffers in m_buffers_ready_to_render into the
   * offscreen buffer for rendering. Those that fit land in
   * m_workroom->m_buffers_ready_to_render_now, those that don't
   * land in m_workroom->m_buffers_ready_to_render_later
   */
  m_ready_to_render_now.clear();
  m_ready_to_render_later.clear();

  helper->prepare(renderer, make_c_array(m_ready_to_render));
  for (auto i : m_ready_to_render)
    {
      VirtualBuffer &buffer(renderer.m_storage->virtual_buffer(i));

      if (helper->allocate_offscreen_space(buffer))
        {
          m_ready_to_render_now.push_back(i);
        }
      else
        {
          m_ready_to_render_later.push_back(i);
        }
    }

  /* return the buffers listed in m_ready_to_render_now */
  return make_c_array(m_ready_to_render_now);
}

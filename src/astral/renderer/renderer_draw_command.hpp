/*!
 * \file renderer_draw_command.hpp
 * \brief file renderer_draw_command.hpp
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

#ifndef ASTRAL_RENDERER_DRAW_COMMAND_HPP
#define ASTRAL_RENDERER_DRAW_COMMAND_HPP

#include <astral/renderer/renderer.hpp>
#include <astral/util/transformed_bounding_box.hpp>
#include "renderer_implement.hpp"

/* Class the encapsulates ONLY the vertices and
 * shaders of a single draw command.
 */
class astral::Renderer::Implement::DrawCommandVerticesShaders
{
public:
  DrawCommandVerticesShaders(Storage &storage, const RenderSupportTypes::Item<ColorItemShader> &item);
  DrawCommandVerticesShaders(Storage &storage, const RenderSupportTypes::Item<MaskItemShader> &item);
  DrawCommandVerticesShaders(Storage &storage, const RenderSupportTypes::Item<ShadowMapItemShader> &item);
  DrawCommandVerticesShaders(Storage &storage, const RenderSupportTypes::ColorItem &item);

  /* Value to pass to Storage::fetch_shader_ptrs() to
   * get the array of shader pointers.
   */
  range_type<unsigned int> m_shaders;

  /* Value to pass to Storage::fetch_vertex_ranges(), i.e.
   * this is NOT a raw set of vertices
   */
  range_type<unsigned int> m_vertex_range;

  /* first shader listed in m_shaders, should only be non-null
   * when m_shaders is a range of length exactly one
   */
  const ItemShader *m_only_shader;

  /* true if any of the shaders listed in m_shaders emits
   * partially covered fragments; only makes sense if
   * this DrawCommand is for color rendering.
   */
  bool m_an_item_shader_emits_partially_covered_fragments;

  /* true if any of the shaders listed in m_shaders emits
   * partially transparent fragments; only makes sense if
   * this DrawCommand is for color rendering.
   */
  bool m_an_item_shader_emits_transparent_fragments;

  /* shader type of all shaders of this */
  enum ItemShader::type_t m_shader_type;

private:
  class Helper;
};

/* Class that encapsulates a single draw command emitted
 * by astral::Renderer; its main addition to RenderValues
 * is a z-value by how much to increment z and an optional
 * index into storage in Renderer for a range of vertices
 * to use.
 */
class astral::Renderer::Implement::DrawCommand
{
public:
  explicit
  DrawCommand(const DrawCommandVerticesShaders &item):
    m_vertices_and_shaders(item),
    m_z(0u),
    m_order(0u),
    m_draw_deleted(false)
  {}

  void
  send_to_backend(Renderer::Implement &renderer,
                  RenderBackend::UberShadingKey::Cookie uber_shader_key,
                  RenderValue<ScaleTranslate> tr,
                  RenderBackend::ClipWindowValue cl,
                  unsigned int add_z,
                  bool permute_xy) const;

  /* Returns true if one of the shaders of this DrawCommand emits
   * partially covered fragments.
   */
  bool
  an_item_shader_emits_partially_covered_fragments(void) const
  {
    return m_vertices_and_shaders.m_an_item_shader_emits_partially_covered_fragments;
  }

  /* Returns true if one of the shaders of this DrawCommand emits
   * transparent fragments.
   */
  bool
  an_item_shader_emits_transparent_fragments(void) const
  {
    return m_vertices_and_shaders.m_an_item_shader_emits_transparent_fragments;
  }

  /* The RenderValues passed to RenderBackend::draw_render_data(),
   * made public for VirtualBuffer to set.
   */
  RenderValues m_render_values;

  /* if valid, use this uber-shader if no uber-shader was provided
   * in send_to_backend(). Keyed, by the value of shader_clipping
   */
  vecN<RenderBackend::UberShadingKey::Cookie, clip_window_value_type_count> m_sub_uber_shader_key;

private:
  friend class DrawCommandList;
  friend class DrawCommandDetailed;

  /* Specifies the vertices and shaders of the draw command */
  DrawCommandVerticesShaders m_vertices_and_shaders;

  /* set by DrawCommandList only */
  unsigned int m_z, m_order;

  /* if true, the draw was "deleted" and should not be emitted;
   * this happens when a draw is copied for one of the snapshot()
   * methods of RenderEncoderBase where the logic of snapshot()
   * issues a blit for copied draws to prevent double pixel
   * computation and the area of the draw was completley containted
   * witin the blit.
   */
  bool m_draw_deleted;
};

/*
 * User internall by DrawCommandList to specify an
 * interval of commands in a DrawCommandList to draw.
 * This is used to sort draw commands by GPU shader
 * to reduce state thrashing.
 */
class astral::Renderer::Implement::DrawCommandDetailed
{
public:
  /* Sorts by ItemShaderBackend::uniqueID() */
  bool
  operator<(const DrawCommandDetailed &rhs) const
  {
    ASTRALassert(m_cmd);
    ASTRALassert(m_cmd->m_vertices_and_shaders.m_only_shader);
    ASTRALassert(rhs.m_cmd);
    ASTRALassert(rhs.m_cmd->m_vertices_and_shaders.m_only_shader);
    return m_cmd->m_vertices_and_shaders.m_only_shader->backend().uniqueID() < rhs.m_cmd->m_vertices_and_shaders.m_only_shader->backend().uniqueID();
  }

  // pointer to DrawCommand
  const DrawCommand *m_cmd;

  // what ScaleTranslate to apply to items in the draws
  RenderValue<ScaleTranslate> m_scale_translate;

  // what ClipWindow, if any, to pass to the backend
  RenderBackend::ClipWindowValue m_clip_window;

  // VirtaulBuffer::start_z() value
  unsigned int m_start_z;

  // DrawCommandList::m_permute_xy value
  bool m_permute_xy;
};

/* A DrawCommandList represents a list of
 * DrawCommand values; there is one such list
 * per VirtualBuffer
 */
class astral::Renderer::Implement::DrawCommandList
{
public:
  /* Enumeration to specify is commands are for rendering
   * to a color, mask or shadowmap
   */
  enum render_type_t
    {
      render_color_image,
      render_mask_image,
      render_shadow_map,
    };

  /* There are three command lists:
   *  - occluders are to be drawn to not affect the color
   *              render target, these are for drawing occluders
   *              that block ALL content; drawn first
   *  - opaque are draws that write to color buffer that do
   *           not depend on the pixels below. These are drawn
   *           front-to-back to increase early-z
   *  - typical are the typical kind of draws that blend with
   *            current pixels in the color render target
   */
  enum command_list_t:uint32_t
    {
      occluder_command_list = 0,
      opaque_command_list,
      typical_command_list,

      number_command_list
    };

  /* only applies for color buffer rendering, gives the value
   * of ItemShaderBackend::uniqueID() of the first shader used.
   * When uber-shading is off, sorting by the first shader used
   * can help performance when uber-shading is off by reducing
   * shader changed; the main use case is for offscreen renders
   * made for an Effect which have a single draw to implement
   * the effect.
   */
  class FirstShaderUsed
  {
  public:
    FirstShaderUsed(void):
      m_first_item_shader_unique_id(-1),
      m_first_material_shader_unique_id(-1)
    {}

    bool
    operator<(FirstShaderUsed rhs) const
    {
      return (m_first_item_shader_unique_id != rhs.m_first_item_shader_unique_id) ?
        m_first_item_shader_unique_id < rhs.m_first_item_shader_unique_id :
        m_first_material_shader_unique_id < rhs.m_first_material_shader_unique_id;
    }

    /* a value of -1 means not set or nullptr */
    int m_first_item_shader_unique_id, m_first_material_shader_unique_id;
  };

  /* Provides a range into a buffer list to specify
   * the dependencies of a draw command.
   */
  class DependencyList
  {
  public:
    DependencyList(const std::vector<VirtualBuffer*> *p,
                   unsigned int begin, unsigned int end):
      m_buffer_list(p),
      m_range(begin, end)
    {}

    DependencyList(void):
      m_buffer_list(nullptr),
      m_range(0, 0)
    {}

    /* list of buffers, if non-null, pointer will stay valid until Renderer::Implement::end() */
    const std::vector<VirtualBuffer*> *m_buffer_list;

    /* range into *m_buffer_list */
    range_type<unsigned int> m_range;
  };

  /* Specifies a marker for a range of commands of a DrawCommandList */
  class SubListMarker
  {
  public:
    /* Default ctor that marks since the very first command */
    SubListMarker(void):
      m_location(0)
    {}

    /* Default ctor that starts at the next command added */
    explicit
    SubListMarker(DrawCommandList &cmd_list)
    {
      for (unsigned int i = 0; i < m_location.size(); ++i)
        {
          m_location[i] = cmd_list.m_commands[i].size();
        }
    }

  private:
    friend class DrawCommandList;

    vecN<unsigned int, number_command_list> m_location;
  };

  /* Functor base class used to get the rects of the
   * draws added to a DrawCommandList
   */
  class RectWalker
  {
  public:
    virtual
    ~RectWalker()
    {}

    /* Function called for TransformedBoundingBox value
     * of the draws of this DrawCommandList
     * \param C draw type
     * \param B TransformedBoundingBox in pixel coordinates
     */
    virtual
    void
    operator()(enum command_list_t C, const TransformedBoundingBox &B) = 0;

    /* If the RectWalker deems it no longer needs to continue
     * walking the rects, it should return true for this function
     * to stop adding. This function should be cheap.
     */
    virtual
    bool
    early_out(void) = 0;
  };

  /* Class to allow for a caller to know what dependencies
   * are present for a draws added in copy_commands().
   */
  class OnAddDependency
  {
  public:
    virtual
    ~OnAddDependency()
    {}

    /* Called by copy_commands() for each dependency
     * of each draw copied.
     */
    virtual
    void
    operator()(VirtualBuffer*) const
    {}
  };

  DrawCommandList(void):
    m_current_z(0u),
    m_current_draw(0u),
    m_pause_snapshot(false),
    m_hit_detection_root(nullptr),
    m_blit_processing(image_processing_count),
    m_storage(nullptr)
  {}

  /* Reset DrawCommandList to be used again */
  void
  clear(void)
  {
    m_current_z = 0u;
    m_current_draw = 0u;
    for (auto &p : m_commands)
      {
        p.clear();
      }
    m_unprocessed_rect_draws.clear();
    m_processed_rect_draws.clear();
    m_element_backing.clear();
    m_pause_snapshot_rect_draws.clear();
    m_pause_snapshot = false;
    m_hit_detection_root = nullptr;
    m_storage = nullptr;
  }

  void
  init(enum render_type_t tp,
       enum image_blit_processing_t blit_processing,
       const BoundingBox<float> &bb,
       Storage *storage)
  {
    ASTRALassert(m_storage == nullptr);
    ASTRALassert(m_hit_detection_root == nullptr);
    ASTRALassert(m_unprocessed_rect_draws.empty());
    ASTRALassert(m_processed_rect_draws.empty());
    ASTRALassert(m_commands[occluder_command_list].empty());
    ASTRALassert(m_commands[opaque_command_list].empty());
    ASTRALassert(m_commands[typical_command_list].empty());

    /* only mask buffer rendering should do mask processing */
    ASTRALassert(tp == render_mask_image || blit_processing != image_blit_stc_mask_processing);

    m_blit_processing = blit_processing;
    m_render_type = tp;
    if (tp == render_color_image)
      {
        m_storage = storage;
        m_hit_detection_root = m_element_backing.create(storage, bb);
      }
  }

  void
  init_as_render_shadow_map(void)
  {
    ASTRALassert(m_storage == nullptr);
    ASTRALassert(m_unprocessed_rect_draws.empty());
    ASTRALassert(m_processed_rect_draws.empty());
    ASTRALassert(m_commands[occluder_command_list].empty());
    ASTRALassert(m_commands[opaque_command_list].empty());
    ASTRALassert(m_commands[typical_command_list].empty());

    m_blit_processing = image_processing_none;
    m_render_type = render_shadow_map;
    m_hit_detection_root = nullptr;
  }

  enum image_blit_processing_t
  blit_processing(void) const
  {
    return m_blit_processing;
  }

  enum render_type_t
  render_type(void) const
  {
    return m_render_type;
  }

  bool
  renders_to_shadow_map(void) const
  {
    return m_render_type == render_shadow_map;
  }

  bool
  renders_to_color_buffer(void) const
  {
    return m_render_type == render_color_image;
  }

  bool
  renders_to_mask_buffer(void) const
  {
    return m_render_type == render_mask_image;
  }

  void
  add_command(bool draw_is_opaque, const DrawCommand &cmd,
              const RenderSupportTypes::RectRegion *region,
              RenderValue<Transformation> pixel_transformation_region,
              const DependencyList &dependency_list);

  bool
  has_commands(void) const
  {
    return !m_commands[opaque_command_list].empty()
      || !m_commands[typical_command_list].empty();
  }

  /* send only those draw commands that are opaque;
   * this call only makes sense when rendering to
   * a color buffer.
   */
  void
  send_opaque_commands_to_backend(Renderer::Implement &renderer,
                                  RenderBackend::UberShadingKey::Cookie uber_shader_key,
                                  RenderValue<ScaleTranslate> tr,
                                  RenderBackend::ClipWindowValue cl,
                                  unsigned int start_z, bool permute_xy);

  /* send all non-opaque commands to the backend */
  void
  send_commands_to_backend(Renderer::Implement &renderer,
                           RenderBackend::UberShadingKey::Cookie uber_shader_key,
                           RenderValue<ScaleTranslate> tr,
                           RenderBackend::ClipWindowValue cl,
                           unsigned int start_z, bool permute_xy);

  void
  send_occluders_to_backend(Renderer::Implement &renderer,
                            RenderBackend::UberShadingKey::Cookie uber_shader_key,
                            RenderValue<ScaleTranslate> tr,
                            RenderBackend::ClipWindowValue cl,
                            unsigned int start_z, bool permute_xy);

  unsigned int
  number_z(void) const
  {
    /* the +1 for the occluders */
    return m_current_z + 1;
  }

  void
  add_occluder(DrawCommand &cmd,
               const RenderSupportTypes::RectRegion *region,
               RenderValue<Transformation> pixel_transformation_region)
  {
    unsigned int z(0u);

    /* It is OK to copy occluders because the only place that copy_commands()
     * is used is in snapshot_logical() and snapshot_effect() and these
     * are for copying commands to a freshly made encoder which is immediately
     * ended after the commands are copied; thus it is ok to copy the commands.
     */
    cmd.m_render_values.m_blend_mode = BackendBlendMode(cmd.m_render_values.m_blend_mode.blend_mode(), false);
    add_command(occluder_command_list, cmd, z, region,
                pixel_transformation_region, DependencyList());
  }

  /* Copy the draw commands of another DrawCommandList
   * that intersect the given bounding box into this
   * DrawCommandList
   * \param src DrawCommandList from which to take commands
   * \param pixel_transformation_logical transformation from rect
   *                                     coordinates to pixel coordinates.
   *                                     If invalid, indictates that the
   *                                     transformation is the identity.
   * \param logical_bb the bounding box selector
   * \param logical_padding padding to add to logical_bb
   * \param delete_contained_cmds if true delete from src any
   *                              commands that are fully contained
   *                              by logical_bb (without logical_padding
   *                              applied to logical_bb).
   * \param add_dependency functor to call to inform caller of dependencies
   *                       of the draws added
   *
   * \returns number of commands copied
   */
  unsigned int
  copy_commands(DrawCommandList &src,
                RenderValue<Transformation> pixel_transformation_logical,
                const BoundingBox<float> &logical_bb,
                float logical_padding,
                bool delete_contained_cmds,
                const OnAddDependency &on_add_dependency);

  /* Commands added while snapshot is paused, do not impact
   * the commands reflected in copy_commands() when this is
   * a source until snapshot is unpaused.
   */
  void
  pause_snapshot(void)
  {
    m_pause_snapshot = true;
  }

  /* End all nesting of no overdraw sessions. */
  void
  unpause_snapshot(void)
  {
    m_pause_snapshot = false;
    flush_pause_snapshot_rect_draws();
  }

  /* call RenderBackend::UberShaderKey::add_shader() for each opaque draw present */
  void
  accumulate_opaques_shaders(Storage &storage, RenderBackend::UberShadingKey &backend) const
  {
    accumulate_shaders(storage, opaque_command_list, backend);
  }

  /* call RenderBackend::UberShaderKey::add_shader() for each typical draw present */
  void
  accumulate_typical_shaders(Storage &storage, RenderBackend::UberShadingKey &backend) const
  {
    accumulate_shaders(storage, typical_command_list, backend);
  }

  /* Returns the first shader value used for each command type */
  const vecN<FirstShaderUsed, number_command_list>&
  first_shader_used(void) const
  {
    return m_first_shader_used;
  }

  /* Given two markers, return an array of c_array<DrawCommand>
   * indexed by command_list_t for the commands specified
   * in the marker range as [begin, end), i.e. all commands
   * at or after begin that are also strictly before end.
   * The c_array<> values can become invalidated if a command
   * is added to this DrawCommandList.
   */
  vecN<c_array<DrawCommand>, number_command_list>
  sublist(SubListMarker begin, SubListMarker end)
  {
    vecN<c_array<DrawCommand>, number_command_list> return_value;

    for (unsigned int i = 0; i < number_command_list; ++i)
      {
        ASTRALassert(begin.m_location[i] <= end.m_location[i]);
        return_value[i] = make_c_array(m_commands[i]).sub_array(begin.m_location[i], end.m_location[i] - begin.m_location[i]);
      }

    return return_value;
  }

  /* Functionally equivalent to sublist(begin, end)[C], i.e. give
   * the commands of the named command type for the specified range.
   */
  c_array<DrawCommand>
  sublist(enum command_list_t C, SubListMarker begin, SubListMarker end)
  {
    ASTRALassert(begin.m_location[C] <= end.m_location[C]);
    return make_c_array(m_commands[C]).sub_array(begin.m_location[C], end.m_location[C] - begin.m_location[C]);
  }

  /* Calls RectWalker::operator() on each rect of each draw
   * of this DrawCommandList; only makes sense if rener_type()
   * is render_color_image.
   */
  void
  walk_rects_of_draws(RectWalker &rect_walker) const;

  /* Send the commands of multiple DrawCommandList objects to the
   * backend where the commands are sorted by shader. This is
   * for -only- rendering masks and shadowmaps.
   * \param renderer Renderer holding work-room and objects
   * \param begin iterator to first VirtualBuffer ID's; buffers are
   *              fetched via Renderer->m_storage->virtual_buffer().
   * \param end iterator to one past the last VirtualBuffer ID
   */
  static
  void
  send_commands_sorted_by_shader_to_backend(Renderer::Implement &renderer,
                                            c_array<const unsigned int>::iterator begin,
                                            c_array<const unsigned int>::iterator end);

private:
  /*!
   * Describes state of a ProcessedGeometry
   * during a query
   */
  enum command_status_t
    {
      /* command has not been checked */
      status_unchecked,

      /* command has been checked */
      status_checked,

      /* command has been checked and should be deleted */
      status_to_be_deleted,
    };

  class RectDraw
  {
  public:
    RectDraw(void):
      m_status(status_unchecked)
    {}

    /* which command list */
    enum command_list_t m_list;

    /* index into m_commands of the draw */
    unsigned int m_command;

    /* list of dependencies */
    DependencyList m_dependencies;

    /* region of the draw */
    TransformedBoundingBox m_rect;

    /* query status, this value is modified
     * during a query to mark it as examined
     * and/or to be deleted.
     */
    enum command_status_t m_status;
  };

  class QueryResult
  {
  public:
    void
    clear(void)
    {
      m_partially_hit.clear();
      m_fully_contained.clear();
      m_not_hit_but_tested.clear();
      m_workroom.clear();
    }

    /* Each is an array of indices into m_processed_rect_draws */
    std::vector<unsigned int> m_partially_hit;
    std::vector<unsigned int> m_fully_contained;
    std::vector<unsigned int> m_not_hit_but_tested;

    /* work room for deleting elements from HitDetectionElement objects */
    std::vector<unsigned int> m_workroom;
  };

  class SortRectDrawsByDrawOrder
  {
  public:
    SortRectDrawsByDrawOrder(const DrawCommandList &p):
      m_p(p)
    {}

    bool
    operator()(unsigned int lhs, unsigned int rhs) const
    {
      ASTRALassert(lhs < m_p.m_processed_rect_draws.size());
      ASTRALassert(rhs < m_p.m_processed_rect_draws.size());
      const RectDraw &lhs_draw(m_p.m_processed_rect_draws[lhs]);
      const RectDraw &rhs_draw(m_p.m_processed_rect_draws[rhs]);

      ASTRALassert(lhs_draw.m_command < m_p.m_commands[lhs_draw.m_list].size());
      ASTRALassert(rhs_draw.m_command < m_p.m_commands[rhs_draw.m_list].size());
      const DrawCommand &lhs_cmd(m_p.m_commands[lhs_draw.m_list][lhs_draw.m_command]);
      const DrawCommand &rhs_cmd(m_p.m_commands[rhs_draw.m_list][rhs_draw.m_command]);

      return lhs_cmd.m_order < rhs_cmd.m_order;
    }

  private:
    const DrawCommandList &m_p;
  };

  /* When rendering to a color buffer, a DrawCommandList needs to be able
   * to compute what draws intersect a given oriented rectangle quickly.
   * The main use-case is to support blend modes that cannot be performed
   * by a GPU's fixed function blender. When such blend modes are encountered,
   * Renderer creates a VirtualBuffer over the area of the draw having that
   * blend mode whose contents are the draws that intersect the area. The
   * surface of the child VirtualBuffer is then used by that draw with that
   * blend mode so it can perform blending via fragment shading.
   */
  class HitDetectionElement
  {
  public:
    HitDetectionElement(Storage *store, const BoundingBox<float> &bb, unsigned int generation = 0u);

    /* Add an HitDetectionElement to the hierarchy
     * \param list DrawCommandList that is the caller and provides the backings
     * \param rect_draw_idx index into m_processed_rect_draws
     */
    void
    add(DrawCommandList &list, unsigned int rect_draw_id);

    /* Query what RectDraw intersect a bounding box
     * in pixel coordinates.
     * \param list the holder of the draw commands
     * \param pixel_transformation_logical transformation from logial to pixel coordinates
     * \param logical_bb box to hit detect against
     * \param out_query place to write query result
     * \param delete_completely_contained if true remove from the
     *                                    hierarchy commands that
     *                                    land in m_fully_contained
     */
    void
    query(DrawCommandList &list,
          RenderValue<Transformation> pixel_transformation_logical,
          const BoundingBox<float> &logical_bb,
          float logical_padding,
          bool delete_completely_contained,
          QueryResult *out_query);

    const BoundingBox<float>&
    bb(void) const
    {
      return m_bb;
    }

  private:
    void
    query_take_all(DrawCommandList &list,
                   const TransformedBoundingBox &pixel_bb,
                   bool delete_completely_contained,
                   QueryResult *out_query);

    void
    query_implement(DrawCommandList &list,
                    const TransformedBoundingBox &pixel_bb,
                    const TransformedBoundingBox &pixel_bb_with_padding,
                    bool delete_completely_contained,
                    QueryResult *out_query);

    bool
    should_spawn(void) const;

    void
    spawn_children(DrawCommandList &list);

    void
    remove_deleted_draws(DrawCommandList &list, std::vector<unsigned int> &workroom);

    /* children created */
    bool m_children_spawned;

    /* generation, used to prevent very deep trees */
    unsigned int m_generation;

    /* actual children, realized as indices
     * into m_element_backing
     */
    vecN<HitDetectionElement*, 4> m_children;

    /* bounding box of HitDetectionElement's region */
    BoundingBox<float> m_bb;

    /* indices into m_processed_rect_draws */
    std::vector<unsigned int> *m_draws;
  };

  void
  process_unprocessed_regions(void);

  void
  accumulate_shaders(Storage &storage, enum command_list_t tp, RenderBackend::UberShadingKey &backend) const;

  void
  add_command(enum command_list_t tp,
              const DrawCommand &cmd, unsigned int z,
              const TransformedBoundingBox *region,
              const DependencyList &dependency_list);

  void
  add_command(enum command_list_t tp,
              const DrawCommand &cmd, unsigned int z,
              const RenderSupportTypes::RectRegion *region,
              RenderValue<Transformation> pixel_transformation_region,
              const DependencyList &dependency_list);

  void
  add_commands_detailed_to_list(std::vector<DrawCommandDetailed> *dst,
                                RenderValue<ScaleTranslate> tr,
                                RenderBackend::ClipWindowValue cl,
                                unsigned int start_z, bool permute_xy) const;

  void
  untag_elements(c_array<const unsigned int> processed_rect_draw_idxs);

  void
  copy_commands_helper(DrawCommandList &src,
                       c_array<const unsigned int> src_processed_rect_draw_idxs,
                       const OnAddDependency &on_add_dependency);

  HitDetectionElement&
  hit_detection_root(void)
  {
    ASTRALassert(m_hit_detection_root);
    return *m_hit_detection_root;
  }

  const HitDetectionElement&
  hit_detection_root(void) const
  {
    ASTRALassert(m_hit_detection_root);
    return *m_hit_detection_root;
  }

  void
  flush_pause_snapshot_rect_draws(void);

  /* When adding commands it is -critical- that the DrawCommand::m_z
   * value is set correctly. The value of m_current_z represents
   * the z-value to use to guarantee that the object drawn next
   * is not occluded by any of the previous elements.
   */
  unsigned int m_current_z;

  /* counter of draws added so far; the next draw's DrawCommand::m_order
   * is set to this value and then m_current_draw is incremented. The
   * order in which draws needs to be stored because when a query of
   * what draws git a rectangle is made, first a list of what draws
   * intersect is built with the hit-detection hierarchy and then that
   * list is sorted by DrawCommand::m_order and then the draws are added.
   */
  unsigned int m_current_draw;

  /* commands added */
  vecN<std::vector<DrawCommand>, number_command_list> m_commands;

  vecN<FirstShaderUsed, number_command_list> m_first_shader_used;

  /* commands added insid begin_pause_snapshot()/end_pause_snapshot()
   * session; these will be flushed to m_unprocessed_rect_draws when
   * snapshotting is not paused
   */
  std::vector<RectDraw> m_pause_snapshot_rect_draws;
  bool m_pause_snapshot;

  /* regions that have not yet been added to m_hit_detection_root */
  std::vector<RectDraw> m_unprocessed_rect_draws;

  /* regions that have been added to m_hit_detection_root */
  std::vector<RectDraw> m_processed_rect_draws;

  /* root element of hit detection */
  HitDetectionElement *m_hit_detection_root;

  /* backing for HitDetectionElement objects */
  MemoryPool<HitDetectionElement, 64> m_element_backing;

  /* The blit processing performed on blit to atlas */
  enum image_blit_processing_t m_blit_processing;

  /* rendering type */
  enum render_type_t m_render_type;

  /* used to allocate backings and to get to data */
  Storage *m_storage;

  /* temporary backng arrays for queries */
  QueryResult m_query_tmp;
};

#endif

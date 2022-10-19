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

#ifndef ASTRAL_RENDERER_FILLER_NON_SPARSE_HPP
#define ASTRAL_RENDERER_FILLER_NON_SPARSE_HPP

#include <astral/util/memory_pool.hpp>
#include <astral/renderer/renderer.hpp>
#include <astral/renderer/shader/fill_stc_shader.hpp>
#include "renderer_shared_util.hpp"
#include "renderer_cached_combined_path.hpp"
#include "renderer_filler.hpp"

class astral::Renderer::Implement::Filler::NonSparse:public Filler
{
public:
  explicit
  NonSparse(Renderer::Implement &renderer):
    Filler(renderer)
  {
  }

protected:
  virtual
  reference_counted_ptr<const Image>
  create_sparse_mask(ivec2 rect_size,
                     c_array<const BoundingBox<float>> subrects,
                     const CombinedPath &path,
                     const ClipElement *clip_element,
                     enum clip_combine_mode_t clip_combine_mode,
                     TileTypeTable *out_clip_combine_tile_data) override
  {
    ASTRALunused(rect_size);
    ASTRALunused(subrects);
    ASTRALunused(path);
    ASTRALunused(clip_element);
    ASTRALunused(clip_combine_mode);
    ASTRALunused(out_clip_combine_tile_data);
    return nullptr;
  }
};

#endif

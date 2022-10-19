/*!
 * \file compound_stroke_item_data_packer.hpp
 * \brief compound_stroke_item_data_packer.hpp
 *
 * Copyright 2020 by InvisionApp.
 *
 * Contact kevinrogovin@invisionapp.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef COMPOUND_STROKE_ITEM_DATA_PACKER_HPP
#define COMPOUND_STROKE_ITEM_DATA_PACKER_HPP

#include <astral/renderer/renderer.hpp>
#include <astral/renderer/shader/stroke_shader.hpp>

/* Packs the data behind T first, then the data behind the
 * other ItemDataPackerBase second. This is because a dashed
 * stroke has a variable size.
 */
template<typename T, typename Base = astral::StrokeShader::ItemDataPackerBase>
class CompoundStrokeItemDataPacker:public astral::StrokeShader::ItemDataPackerBase
{
public:
  CompoundStrokeItemDataPacker(const T &v, const Base &b):
    m_v(v),
    m_base(b)
  {
    const astral::ItemDataValueMapping &in_mapping(m_base.intrepreted_value_map());
    unsigned int v_size(m_v.item_data_size());

    for (const auto &entry : in_mapping.data())
      {
        m_mapping.add(entry.m_type, entry.m_channel, entry.m_component + v_size);
      }
  }

  virtual
  unsigned int
  item_data_size(const astral::StrokeParameters &stroke_params) const override
  {
    return m_v.item_data_size() + m_base.item_data_size(stroke_params);
  }

  virtual
  void
  pack_item_data(astral::RenderValue<astral::Transformation> logical_transformation_path,
                 const astral::StrokeParameters &stroke_params,
                 float t, astral::c_array<astral::gvec4> dst) const override
  {
    unsigned int v_size(m_v.item_data_size());

    m_v.pack_item_data(stroke_params, dst.sub_array(0, v_size));
    m_base.pack_item_data(logical_transformation_path,
                          stroke_params, t, dst.sub_array(v_size));
  }

  virtual
  bool
  caps_joins_collapse(const astral::float2x2 &pixel_transformation_logical,
                      const astral::vec2 &render_scale_factor,
                      const astral::StrokeParameters &stroke_params) const override
  {
    return m_base.caps_joins_collapse(pixel_transformation_logical,
                                      render_scale_factor,
                                      stroke_params);
  }

  virtual
  const astral::ItemDataValueMapping&
  intrepreted_value_map(void) const override
  {
    return m_mapping;
  }

private:
  const T &m_v;
  const Base &m_base;
  astral::ItemDataValueMapping m_mapping;
};

#endif

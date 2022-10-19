/*!
 * \file renderer_stc_data.cpp
 * \brief file renderer_stc_data.cpp
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

#include "renderer_stc_data.hpp"
#include "renderer_stc_data_builder_helper.hpp"

void
astral::Renderer::Implement::STCData::
copy_stc(std::vector<STCData> *stc_data_backing, SubSTCDataBacking *subelement_backing,
         VirtualArray &src_stcs, const BoundingBox<float> &bb, bool delete_contained)
{
  for (unsigned int i = 0, endi = src_stcs.size(); i < endi; ++i)
    {
      STCData &src_stc(src_stcs.element(i, *stc_data_backing));
      copy_stc(stc_data_backing, subelement_backing, src_stc, bb, delete_contained);
    }

  /* Remove any STCData's from src_stc's that are empty */
  for (unsigned int i = 0, endi = src_stcs.size(); i < endi;)
    {
      if (src_stcs.element(i, *stc_data_backing).empty())
        {
          ASTRALassert(endi > 0u);

          src_stcs.element(i, *stc_data_backing) = src_stcs.back(*stc_data_backing);
          src_stcs.pop_back();
          --endi;
        }
      else
        {
          ++i;
        }
    }
}

void
astral::Renderer::Implement::STCData::
copy_stc(std::vector<STCData> *dst, SubSTCDataBacking *subelement_backing,
         STCData &src_stc, const BoundingBox<float> &bb, bool delete_contained)
{
  STCData tmp(src_stc.m_transformation, src_stc.m_item_data, subelement_backing);

  for (unsigned int i = src_stc.m_sub.m_begin; i < src_stc.m_sub.m_end; ++i)
    {
      if (!src_stc.m_subelement_backing->optional_bb(i).m_provided)
        {
          tmp.add_range(src_stc.m_subelement_backing->range(i), &bb);
        }
      else if (src_stc.m_subelement_backing->optional_bb(i).m_bb.intersects(bb))
        {
          if (delete_contained && bb.contains(src_stc.m_subelement_backing->optional_bb(i).m_bb))
            {
              tmp.add_range(src_stc.m_subelement_backing->range(i), &src_stc.m_subelement_backing->optional_bb(i).m_bb);

              /* remove this entry by having the i'th entry hold the entry
               * at src_stc.m_sub.m_begin and then incrementing src_stc.m_sub.m_begin
               */
              if (i != src_stc.m_sub.m_begin)
                {
                  src_stc.m_subelement_backing->range(i) = src_stc.m_subelement_backing->range(src_stc.m_sub.m_begin);
                  src_stc.m_subelement_backing->optional_bb(i) = src_stc.m_subelement_backing->optional_bb(src_stc.m_sub.m_begin);
                }
              ++src_stc.m_sub.m_begin;
            }
          else
            {
              BoundingBox<float> Q(src_stc.m_subelement_backing->optional_bb(i).m_bb);

              Q.intersect_against(bb);
              tmp.add_range(src_stc.m_subelement_backing->range(i), &Q);
            }
        }
    }

  if (!tmp.empty())
    {
      dst->push_back(tmp);
    }
}

/*!
 * \file generic_hierarchy.hpp
 * \brief generic_hierarchy.hpp
 *
 * Adapted from: generic_hierarchy.hpp of FastUIDraw (https://github.com/intel/fastuidraw):
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#include <astral/util/util.hpp>
#include <astral/util/bounding_box.hpp>
#include <vector>


#ifndef ASTRAL_DEMO_GENERIC_HIERARCHY_HPP
#define ASTRAL_DEMO_GENERIC_HIERARCHY_HPP

class GenericHierarchy:astral::noncopyable
{
public:
  enum
    {
      not_found = ~0u
    };

  explicit
  GenericHierarchy(const astral::BoundingBox<float> &bbox);

  ~GenericHierarchy();

  void
  add(const astral::BoundingBox<float> &bbox, unsigned int reference);

  void
  query(const astral::BoundingBox<float> &bbox, std::vector<unsigned int> *output);

  void
  query(const astral::vec2 &p, std::vector<unsigned int> *output);

  unsigned int
  query(const astral::vec2 &p, astral::BoundingBox<float> *out_bb);

private:
  void *m_root;
};

#endif

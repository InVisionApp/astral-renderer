/*!
 * \file path.cpp
 * \brief file path.cpp
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

#include <astral/path.hpp>
#include <astral/renderer/combined_path.hpp>
#include <astral/renderer/render_data.hpp>
#include <astral/renderer/render_engine.hpp>
#include <astral/renderer/item_path.hpp>
#include <astral/util/ostream_utility.hpp>

#include "contour_approximator.hpp"
#include "generic_lod.hpp"

class astral::Path::DataGenerator:public reference_counted<DataGenerator>::non_concurrent
{
public:
  explicit
  DataGenerator(void)
  {}

  const ItemPath&
  fetch_item_path(float tol, const Path &path, float *actual_error)
  {
    const auto &e(m_item_path_entries.fetch(tol, path));
    if (actual_error)
      {
        *actual_error = e.error();
      }
    return e.data();
  }

private:
  class ItemPathEntry
  {
  public:
    ItemPathEntry(float tol, const Path &path):
      m_finalized(false)
    {
      ItemPath::GenerationParams P;

      /* The current default values of P are just guesses;
       * adding more levels of recursion can have a huge
       * memory storage cost and yet give only very modest
       * reductions of the average render cost
       */
      m_data = ItemPath::create(ItemPath::Geometry().add(path, tol), P);
    }

    explicit
    ItemPathEntry(const Path &path):
      ItemPathEntry(-1.0f, path)
    {}

    bool
    finalized(void) const { return m_finalized; }

    void
    finalize(void) { m_finalized = true; }

    float
    error(void) const { return m_data->properties().m_error; }

    ItemPathEntry
    create_refinement(const Path &path)
    {
      ASTRALassert(!m_finalized);
      finalize();
      return ItemPathEntry(ASTRAL_GENERIC_LOD_SUCCESSIVE_LOD_RATIO * error(), path);
    }

    const ItemPath&
    data(void) const
    {
      return *m_data;
    }

    /* returning 0 for size means that there is no
     * size upper bound for refinement.
     */
    unsigned int
    size(void) const
    {
      return 0u;
    }

  private:
    bool m_finalized;
    reference_counted_ptr<ItemPath> m_data;
  };

  detail::GenericLOD<ItemPathEntry> m_item_path_entries;
};

///////////////////////////////////////
// astral::Path methods
astral::Path::
Path(void):
  m_santize_curves_on_adding(true),
  m_bb_ready(true)
{}

astral::Path::
Path(Path &&obj) noexcept:
  m_santize_curves_on_adding(obj.m_santize_curves_on_adding),
  m_contours(std::move(obj.m_contours)),
  m_data_generator(obj.m_data_generator),
  m_bb_ready(obj.m_bb_ready),
  m_bb(obj.m_bb),
  m_cap_bb(obj.m_cap_bb)
{}

astral::Path::
~Path()
{}

astral::Path::
Path(const Path&) = default;

astral::Path&
astral::Path::
operator=(const Path&) = default;

astral::Path&
astral::Path::
operator=(Path &&obj) noexcept
{
  m_santize_curves_on_adding = obj.m_santize_curves_on_adding;
  m_contours = std::move(obj.m_contours);
  m_data_generator = obj.m_data_generator;
  m_bb = obj.m_bb;
  m_cap_bb = obj.m_cap_bb;
  m_bb_ready = obj.m_bb_ready;

  return *this;
}

void
astral::Path::
swap(Path &obj)
{
  std::swap(m_santize_curves_on_adding, obj.m_santize_curves_on_adding);
  m_contours.swap(obj.m_contours);
  m_data_generator.swap(obj.m_data_generator);
  std::swap(m_bb_ready, obj.m_bb_ready);
  std::swap(m_bb, obj.m_bb);
  std::swap(m_join_bb, obj.m_join_bb);
  std::swap(m_cap_bb, obj.m_cap_bb);
  std::swap(m_control_point_bb, obj.m_control_point_bb);
}

void
astral::Path::
santize_curves_on_adding(bool v)
{
  m_santize_curves_on_adding = v;
  if (!m_contours.empty())
    {
      m_contours.back()->santize_curves_on_adding(v);
    }
}

void
astral::Path::
prepare_to_add_curve(void)
{
  mark_dirty();
  if (m_contours.empty())
    {
      move(vec2(0.0f, 0.0f));
    }
  else if (m_contours.back()->closed())
    {
      c_array<const ContourCurve> curves(m_contours.back()->curves());
      if (curves.empty())
        {
          move(vec2(0.0f, 0.0f));
        }
      else
        {
          move(curves.back().end_pt());
        }
    }
}

astral::Path&
astral::Path::
move(vec2 pt)
{
  mark_dirty();
  m_contours.push_back(Contour::create());
  m_contours.back()->santize_curves_on_adding(m_santize_curves_on_adding);
  m_contours.back()->start(pt);

  return *this;
}

astral::Path&
astral::Path::
start_contour(const ContourCurve &curve)
{
  mark_dirty();
  m_contours.push_back(Contour::create());
  m_contours.back()->santize_curves_on_adding(m_santize_curves_on_adding);
  m_contours.back()->start(curve);

  return *this;
}

void
astral::Path::
mark_dirty(void)
{
  m_data_generator = nullptr;
  m_bb_ready = false;
}

astral::Path&
astral::Path::
close(void)
{
  if (!m_contours.empty() && !m_contours.back()->closed())
    {
      mark_dirty();
      m_contours.back()->close();
    }
  return *this;
}

astral::Path&
astral::Path::
add_contour(const ContourData &inC, unsigned int *q)
{
  unsigned int v;
  const ContourData *pC(&inC);
  ContourData sC;

  if (m_santize_curves_on_adding && !inC.is_sanitized())
    {
      sC = ContourData(inC);
      sC.sanitize();
      pC = &sC;
    }

  const ContourData &C(*pC);

  mark_dirty();
  if (!m_contours.empty() && !m_contours.back()->closed())
    {
      reference_counted_ptr<Contour> p;

      p = m_contours.back();
      m_contours.back() = Contour::create(C);
      m_contours.push_back(p);
      v = m_contours.size() - 2u;
    }
  else
    {
      m_contours.push_back(Contour::create(C));
      v = m_contours.size() - 1u;
    }

  if (q)
    {
      *q = v;
    }

  return *this;
}

void
astral::Path::
ready_bb(void) const
{
  if (!m_bb_ready)
    {
      m_bb_ready = true;
      m_bb.clear();
      m_join_bb.clear();
      m_cap_bb.clear();
      m_control_point_bb.clear();
      for (const auto &c : m_contours)
        {
          if (!c->curves().empty())
            {
              m_bb.union_box(c->bounding_box());
              m_join_bb.union_box(c->join_bounding_box());
              m_control_point_bb.union_box(c->control_point_bounding_box());
              if (!c->closed())
                {
                  m_cap_bb.union_point(c->curves().front().start_pt());
                  m_cap_bb.union_point(c->curves().back().end_pt());
                }
            }
          else
            {
              m_bb.union_point(c->start());
              m_cap_bb.union_point(c->start());
            }
        }
    }
}

const astral::BoundingBox<float>&
astral::Path::
bounding_box(void) const
{
  ready_bb();
  return m_bb;
}

const astral::BoundingBox<float>&
astral::Path::
control_point_bounding_box(void) const
{
  ready_bb();
  return m_control_point_bb;
}

const astral::BoundingBox<float>&
astral::Path::
open_contour_endpoint_bounding_box(void) const
{
  ready_bb();
  return m_cap_bb;
}

const astral::BoundingBox<float>&
astral::Path::
join_bounding_box(void) const
{
  ready_bb();
  return m_join_bb;
}

void
astral::Path::
clear(void)
{
  m_bb_ready = true;
  m_bb.clear();
  m_cap_bb.clear();
  m_join_bb.clear();
  m_contours.clear();
  m_data_generator = nullptr;
}

astral::Path::DataGenerator&
astral::Path::
data_generator(void) const
{
  if (!m_data_generator)
    {
      m_data_generator = ASTRALnew DataGenerator();
    }
  return *m_data_generator;
}

const astral::ItemPath&
astral::Path::
item_path(float tol, float *actual_error) const
{
  return data_generator().fetch_item_path(tol, *this, actual_error);
}

astral::Path::PointQueryResult
astral::Path::
distance_to_path(float tol, const vec2 &pt, float distance_cull) const
{
  PointQueryResult w;

  if (m_contours.empty())
    {
      return w;
    }

  if (distance_cull >= 0.0f)
    {
      const BoundingBox<float> &bb(bounding_box());

      if (!bb.contains(pt))
        {
          float D;

          D = bb.distance_to_boundary(pt);
          if (D >= distance_cull)
            {
              return w;
            }
        }
    }

  for (unsigned int i = 0, endi = m_contours.size(); i < endi; ++i)
    {
      w.absorb(i, m_contours[i]->distance_to_contour(tol, pt, distance_cull));
      if (w.m_closest_contour > 0)
        {
          distance_cull = (distance_cull < 0.0f) ?
            w.m_distance :
            t_min(distance_cull, w.m_distance);
        }
    }

  return w;
}

bool
astral::Path::
is_sanitized(void) const
{
  for (const reference_counted_ptr<Contour> &C : m_contours)
    {
      if (!C->is_sanitized())
        {
          return false;
        }
    }
  return true;
}

bool
astral::Path::
sanitize(void)
{
  bool return_value(false);

  for (const reference_counted_ptr<Contour> &C : m_contours)
    {
      bool r;

      r = C->sanitize();
      return_value = return_value || r;
    }

  if (return_value)
    {
      mark_dirty();
    }

  return return_value;
}

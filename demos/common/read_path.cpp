/*!
 * \file read_path.cpp
 * \brief read_path.cpp
 *
 * Adapted from: read_path.cpp of FastUIDraw (https://github.com/intel/fastuidraw):
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
#include <sstream>
#include <algorithm>
#include <list>

#include <astral/util/ostream_utility.hpp>
#include "read_path.hpp"

namespace
{
  enum pt_mode_t
    {
      reading_start_pt,
      reading_end_pt,
      reading_control_pt,
      reading_weight,
      reading_arc_angle,
    };

  void
  process_stats(std::vector<PerContourCommand> *dst,
                const astral::Path::arc_curve_stats &stats,
                astral::c_array<const astral::ContourCurve> curves)
  {
    if (!dst)
      {
        return;
      }

    ASTRALassert(curves.size() >= stats.m_number_curves);

    dst->back().m_curve_commands.push_back(PerCurveCommand());
    PerCurveCommand &cmd(dst->back().m_curve_commands.back());

    for (unsigned int c = 0, src = curves.size() - stats.m_number_curves; c < stats.m_number_curves; ++c, ++src)
      {
        cmd.m_parameter_space_lengths.push_back(stats.m_parameter_lengths[c]);
        cmd.m_curves.push_back(curves[src]);
      }
  }

  void
  process_stats(std::vector<PerContourCommand> *dst,
                astral::c_array<const astral::ContourCurve> curves)
  {
    if (!dst)
      {
        return;
      }
    ASTRALassert(!curves.empty());
    dst->back().m_curve_commands.push_back(PerCurveCommand());
    dst->back().m_curve_commands.back().m_parameter_space_lengths.push_back(1.0f);
    dst->back().m_curve_commands.back().m_curves.push_back(curves.back());
  }
}

void
PerContourCommand::
generate_compound_curve_contour(std::vector<astral::AnimatedPath::CompoundCurve> *dst) const
{
  ASTRALassert(dst);
  dst->clear();
  for (const auto &cmd : m_curve_commands)
    {
      astral::AnimatedPath::CompoundCurve v;

      v.m_curves = astral::make_c_array(cmd.m_curves);
      v.m_parameter_space_lengths = astral::make_c_array(cmd.m_parameter_space_lengths);
      dst->push_back(v);
    }
}

void
PerContourCommand::
generate_curve_contour(std::vector<astral::ContourCurve> *dst) const
{
  dst->clear();
  for (const auto &cmd : m_curve_commands)
    {
      for (const auto &curve : cmd.m_curves)
        {
          dst->push_back(curve);
        }
    }
}

void
PerContourCommand::
reverse(void)
{
  for (auto &v : m_curve_commands)
    {
      std::reverse(v.m_curves.begin(), v.m_curves.end());
      std::reverse(v.m_parameter_space_lengths.begin(), v.m_parameter_space_lengths.end());
    }
  std::reverse(m_curve_commands.begin(), m_curve_commands.end());
}

void
read_path(astral::Path *path, std::istream &source,
          PathCommand *dst)
{
  std::stringstream buffer;

  buffer << source.rdbuf();
  read_path(path, buffer.str(), dst);
}

void
read_path(astral::Path *path, const std::string &source,
          PathCommand *path_dst)
{
  if (!path && !path_dst)
    {
      return;
    }

  std::string filtered(source);

  std::replace(filtered.begin(), filtered.end(), '(', ' ');
  std::replace(filtered.begin(), filtered.end(), ')', ' ');
  std::replace(filtered.begin(), filtered.end(), ',', ' ');
  std::istringstream istr(filtered);

  astral::ContourData contour;
  bool close_contour(false), has_weight(false);
  bool reverse_contour(false), has_arc_angle(false);
  enum pt_mode_t pt_mode(reading_start_pt);
  astral::vec2 pt;
  int current_coordinate(0);
  astral::vecN<astral::vec2, 2> control_pts;
  unsigned int num_control_pts(0);
  float weight, arc_angle;
  enum astral::ContourCurve::continuation_t ctp(astral::ContourCurve::not_continuation_curve);
  astral::vecN<float, 8> parameter_lengths_backing;
  astral::Path::arc_curve_stats arc_add_stats;
  std::vector<PerContourCommand> *dst(nullptr);

  arc_add_stats.m_parameter_lengths = parameter_lengths_backing;
  while(istr)
    {
      std::string token;
      istr >> token;
      if (!istr.fail())
        {
          if (token == "]" || token == "}")
            {
              if (close_contour)
                {
                  if (has_weight && num_control_pts >= 1u)
                    {
                      contour.conic_close(weight, control_pts[0], ctp);
                    }
                  else if (has_arc_angle)
                    {
                      contour.arc_close(arc_angle, ctp, &arc_add_stats);
                      process_stats(dst, arc_add_stats, contour.curves());
                    }
                  else
                    {
                      astral::c_array<const astral::vec2> ctl(control_pts);

                      ctl = ctl.sub_array(0, num_control_pts);
                      contour.curve_close(ctl, ctp);
                      process_stats(dst, contour.curves());
                    }
                }

              if (reverse_contour)
                {
                  contour = contour.reverse();
                  if (dst)
                    {
                      dst->back().reverse();
                    }
                }

              unsigned int added_contour;
              if (path)
                {
                  path->add_contour(contour, &added_contour);
                }

              if (dst)
                {
                  if (path)
                    {
                      dst->back().m_src = &path->contour(added_contour);
                      dst->back().m_ID = added_contour;
                    }
                  else
                    {
                      dst->back().m_src = astral::Contour::create(contour);
                      dst->back().m_ID = 0u;
                    }

                  dst = nullptr;
                }

              pt_mode = reading_start_pt;
              num_control_pts = 0;
              has_weight = false;
              has_arc_angle = false;
              contour.clear();
            }
          else if (token == "[" || token == "{")
            {
              reverse_contour = false;
              close_contour = (token == "[");
              if (path_dst)
                {
                  dst = path_dst->fetch(close_contour);
                  dst->push_back(PerContourCommand());
                }
            }
          else if (token == "R[" || token == "R{")
            {
              reverse_contour = true;
              close_contour = (token == "R[");
              if (path_dst)
                {
                  dst = path_dst->fetch(close_contour);
                  dst->push_back(PerContourCommand());
                }
            }
          else if (token == "[[")
            {
              ASTRALassert(pt_mode == reading_end_pt);
              pt_mode = reading_control_pt;
            }
          else if (token == "]]")
            {
              ASTRALassert(pt_mode == reading_control_pt || pt_mode == reading_weight);
              pt_mode = reading_end_pt;
            }
          else if (token == "W" || token == "w")
            {
              ASTRALassert(pt_mode == reading_control_pt);
              pt_mode = reading_weight;
            }
          else if (token == "arc")
            {
              ASTRALassert(pt_mode == reading_end_pt);
              pt_mode = reading_arc_angle;
            }
          else
            {
              /* not a funky token so should be a number.  */
              float number;
              std::istringstream token_istr(token);
              token_istr >> number;

              if (!token_istr.fail())
                {
                  switch (pt_mode)
                    {
                    case reading_weight:
                      has_weight = true;
                      weight = number;
                      pt_mode = reading_control_pt;
                      break;

                    case reading_arc_angle:
                      has_arc_angle = true;
                      arc_angle = ASTRAL_PI * number / 180.0f;
                      pt_mode = reading_end_pt;
                      break;

                    case reading_control_pt:
                      pt[current_coordinate] = number;
                      if (current_coordinate == 1)
                        {
                          control_pts[num_control_pts] = pt;
                          ++num_control_pts;
                        }
                      ++current_coordinate;
                      break;

                    case reading_start_pt:
                      pt[current_coordinate] = number;
                      if (current_coordinate == 1)
                        {
                          contour.start(pt);
                          pt_mode = reading_end_pt;
                        }
                      ++current_coordinate;
                      break;

                    case reading_end_pt:
                      pt[current_coordinate] = number;
                      if (current_coordinate == 1)
                        {
                          if (has_weight && num_control_pts >= 1u)
                            {
                              contour.conic_to(weight, control_pts[0], pt, ctp);
                              process_stats(dst, contour.curves());
                              has_weight = false;
                            }
                          else if (has_arc_angle)
                            {
                              contour.arc_to(arc_angle, pt, ctp, &arc_add_stats);
                              process_stats(dst, arc_add_stats, contour.curves());
                              has_arc_angle = false;
                            }
                          else
                            {
                              astral::c_array<const astral::vec2> ctl(control_pts);

                              ctl = ctl.sub_array(0, num_control_pts);
                              contour.curve_to(ctl, pt, ctp);
                              process_stats(dst, contour.curves());
                              num_control_pts = 0;
                            }
                        }
                      ++current_coordinate;
                      break;
                    }

                  if (current_coordinate == 2)
                    {
                      current_coordinate = 0;
                    }
                }
            }
        }
    }
}

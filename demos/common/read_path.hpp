/*!
 * \file read_path.hpp
 * \brief read_path.hpp
 *
 * Adapted from: read_path.hpp of FastUIDraw (https://github.com/intel/fastuidraw):
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
#ifndef ASTRAL_DEMO_READ_PATH_HPP
#define ASTRAL_DEMO_READ_PATH_HPP

#include <string>
#include <vector>
#include <astral/path.hpp>
#include <astral/animated_path.hpp>

class PerCurveCommand
{
public:
  std::vector<float> m_parameter_space_lengths;
  std::vector<astral::ContourCurve> m_curves;
};

class PerContourCommand
{
public:
  std::vector<PerCurveCommand> m_curve_commands;
  astral::reference_counted_ptr<const astral::Contour> m_src;
  unsigned int m_ID;

  /* returned value can become invalided if m_curve_commands
   * is modified because of the moving backing store.
   */
  void
  generate_compound_curve_contour(std::vector<astral::AnimatedPath::CompoundCurve> *dst) const;

  void
  generate_curve_contour(std::vector<astral::ContourCurve> *dst) const;

  void
  reverse(void);
};

class PathCommand
{
public:
  std::vector<PerContourCommand>*
  fetch(bool is_closed)
  {
    return (is_closed) ?
      &m_closed_contours :
      &m_open_contours;
  }

  std::vector<PerContourCommand> m_open_contours;
  std::vector<PerContourCommand> m_closed_contours;
};


/* Read path data from an std::string and append that
 *  data to path. The format of the input is:
 *
 * [ marks the start of a closed outline
 * ] marks the end of a closed outline
 * { marks the start of an open outline
 * } marks the end of an open outline
 * [[ marks the start of a sequence of control points
 * ]] marks the end of a sequence of control points
 * arc marks an arc edge, the next value is the angle in degres
 * value0 value1 marks a coordinate (control point of edge point)
 *
 * \param path place to which to write the path, can be nullptr
 *             to indicate to not place the command into a path.
 * \param source string from which to read the path
 * \param dst if non-null, place to write how each curve
 *            command in the input is realized by the
 *            path
 */
void
read_path(astral::Path *path, const std::string &source,
          PathCommand *dst = nullptr);
void
read_path(astral::Path *path, std::istream &source,
          PathCommand *dst = nullptr);

#endif

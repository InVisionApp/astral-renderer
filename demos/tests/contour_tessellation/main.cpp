/*!
 * \file main.cpp
 * \brief main.cpp
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
#include <sstream>
#include <fstream>
#include <iostream>
#include <SDL.h>

#include <astral/path.hpp>
#include <astral/util/ostream_utility.hpp>

#include "generic_command_line.hpp"
#include "read_path.hpp"

class ContourTessellationTest:public command_line_register
{
public:
  ContourTessellationTest(void):
    m_path_file("", "path", "File from which to read the path", *this),
    m_target_tol(1e-3, "target_tol", "Target tolerance to aim for", *this),
    m_relative_tol(true, "relative_tol",
                   "If true tolerance is relative to the bounding box of the path",
                   *this)
  {}

  int
  main(int argc, char **argv);

private:
  void
  ready_path(void);

  astral::c_array<const astral::ContourCurve>
  get_stroke_curves(unsigned int contour, float tol, float *actual_tol)
  {
    return m_path.contour(contour).stroke_approximated_geometry(tol, actual_tol);
  }

  astral::c_array<const astral::ContourCurve>
  get_fill_curves(unsigned int contour, float tol, float *actual_tol)
  {
    return m_path.contour(contour).fill_approximated_geometry(tol, astral::contour_fill_approximation_allow_long_curves, actual_tol);
  }

  void
  print_approximation(const std::string &label,
                      astral::c_array<const astral::ContourCurve> (ContourTessellationTest::*function)(unsigned int, float, float*));

  command_line_argument_value<std::string> m_path_file;
  command_line_argument_value<float> m_target_tol;
  command_line_argument_value<bool> m_relative_tol;

  astral::Path m_path;
};

void
ContourTessellationTest::
ready_path(void)
{
  std::ifstream path_file(m_path_file.value().c_str());
  if (path_file)
    {
      read_path(&m_path, path_file);
    }

  if (m_path.number_contours() == 0)
    {
      m_path
        .move(astral::vec2(50.0f, 35.0f))
        .quadratic_to(astral::vec2(60.0f, 50.0f), astral::vec2(70.0f, 35.0f))
        .arc_to(ASTRAL_PI, astral::vec2(70.0f, -100.0f))
        .cubic_to(astral::vec2(60.0f, -150.0f), astral::vec2(30.0f, -50.0f), astral::vec2(0.0f, -100.0f))
        .arc_close(0.5f * ASTRAL_PI)

        .move(astral::vec2(200.0f, 200.0f))
        .line_to(astral::vec2(400.0f, 200.0f))
        .line_to(astral::vec2(400.0f, 400.0f))
        .line_to(astral::vec2(200.0f, 400.0f))
        .close()

        .move(astral::vec2(-50.0f, 100.0f))
        .line_to(astral::vec2(0.0f, 200.0f))
        .line_to(astral::vec2(100.0f, 300.0f))
        .line_to(astral::vec2(150.0f, 325.0f))
        .line_to(astral::vec2(150.0f, 100.0f))
        .close();
    }
}

void
ContourTessellationTest::
print_approximation(const std::string &label,
                    astral::c_array<const astral::ContourCurve> (ContourTessellationTest::*function)(unsigned int, float, float*))
{
  float error(0.0f);
  unsigned int total_size(0u);
  astral::vec2 path_size(m_path.bounding_box().as_rect().size());
  float relative_scale(1.0f / astral::t_max(path_size.x(), path_size.y()));

  for (unsigned int c = 0, numC = m_path.number_contours(); c < numC; ++c)
    {
      astral::c_array<const astral::ContourCurve> curves;
      float actual_error;

      curves = (this->*function)(c, m_target_tol.value(), &actual_error);
      error = astral::t_max(error, actual_error);
      total_size += curves.size();
    }

  std::cout << "\n\n ===== " << label << ", total_size = "
            << total_size << ", absolute_error = " << error
            << ", relative_error = " << error * relative_scale
            << " ====\n";

  for (unsigned int c = 0, numC = m_path.number_contours(); c < numC; ++c)
    {
      astral::c_array<const astral::ContourCurve> curves;
      astral::vec2 path_size(m_path.contour(c).bounding_box().as_rect().size());
      float relative_scale(1.0f / astral::t_max(path_size.x(), path_size.y()));
      float actual_error;

      curves = (this->*function)(c, m_target_tol.value(), &actual_error);
      std::cout << "\tContour #" << c << ", actual_error = "
                << actual_error << " (rel = " << actual_error * relative_scale
                << "), size = " << curves.size() << "\n";
      for (const auto &s : curves)
        {
          std::cout << "\t\t" << s << "\n";
        }
    }
}

int
ContourTessellationTest::
main(int argc, char **argv)
{
  if (argc == 2 && is_help_request(argv[1]))
    {
      std::cout << "\n\nUsage: " << argv[0];
      print_help(std::cout);
      print_detailed_help(std::cout);
      return 0;
    }

  std::cout << "\n\nRunning: \"";
  for(int i = 0; i < argc; ++i)
    {
      std::cout << argv[i] << " ";
    }

  parse_command_line(argc, argv);
  std::cout << "\n\n" << std::flush;

  ready_path();

  if (m_relative_tol.value() && !m_path.bounding_box().empty())
    {
      astral::vec2 v;

      v = m_path.bounding_box().as_rect().size();
      m_target_tol.value() *= astral::t_max(v.x(), v.y());
    }

  print_approximation("FillTessellation", &ContourTessellationTest::get_stroke_curves);
  print_approximation("StrokeTessellation", &ContourTessellationTest::get_fill_curves);

  return 0;
}

int
main(int argc, char **argv)
{
  ContourTessellationTest A;

  return A.main(argc, argv);
}

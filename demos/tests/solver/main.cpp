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
#include <iostream>
#include <random>
#include <algorithm>
#include <SDL.h>
#include <astral/util/polynomial.hpp>
#include <astral/util/ostream_utility.hpp>

#include "generic_command_line.hpp"

#if 0
template<typename T, size_t A>
std::ostream&
operator<<(std::ostream &ostr, const astral::Polynomial<T, A> &obj)
{
  for (size_t i = A; i > 0; --i)
    {
      if (obj.coeff(i) != T(0))
        {
          ostr << obj.coeff(i) << "*t^" << i << " + ";
        }
    }
  ostr << obj.coeff(0);
  return ostr;
}
#endif

template<typename T>
bool
sort_by_real(const std::complex<T> &lhs,
             const std::complex<T> &rhs)
{
  return (lhs.real() != rhs.real()) ?
    lhs.real() < rhs.real() :
    lhs.imag() < rhs.imag();
}

template<typename T, size_t N, size_t Max>
class PolyPowerPrinter
{
public:

  static
  void
  print_powers(const astral::Polynomial<T, N> &p)
  {
    astral::Polynomial<T, 0> q(T(1));
    PolyPowerPrinter::print_product<0>(q, p);
  }

private:
  enum { MaxN = N * Max };

  static
  void
  print_product(const astral::Polynomial<T, MaxN>&, const astral::Polynomial<T, N>&)
  {
  }

  template<size_t A>
  static
  void
  print_product(const astral::Polynomial<T, A> &pA, const astral::Polynomial<T, N> &p)
  {
    astral::Polynomial<T, A + N> q;

    q = pA * p;
    std::cout << "(" << p << ")^" << (A + N) / N << " = " << q << ", "
              << "d/dt = " << q.derivative() << "\n";
    print_product(q, p);
  }
};

enum accuracy_t
  {
    float_accuracy,
    double_accuracy,
    long_double_accuracy,
  };

class TestOptions:public command_line_register
{
public:
  TestOptions(void):
    m_num_test2(10, "num_test2",
                "Number of tests to run on quadratic solver where there are 2 unique roots",
                *this),
    m_num_test3(10, "num_test3",
                "Number of tests to run on cubic solver where there are 3 unique roots",
                *this),
    m_num_test4(10, "num_test4",
                "Number of tests to run on quartic solver where there are 4 unique roots",
                *this),
    m_num_test2_no_roots(10, "num_test2_no_roots",
                         "Number of tests to run on quadratic solver where there are no roots",
                         *this),
    m_num_test3_one_root(10, "num_test3_one_root",
                         "Number of tests to run on cubic solver where there is one root",
                         *this),
    m_num_test3_double_root(10, "num_test3_double_root",
                            "Number of tests to run on cubic solver where there is one double root and one normal root",
                            *this),
    m_num_test3_triple_root(10, "num_test3_triple_root",
                            "Number of tests to run on cubic solver where there is triple root",
                            *this),
    m_num_test4_no_roots(10, "num_test4_no_roots",
                         "Number of tests to run on quartic solver where there are no roots",
                         *this),
    m_num_test4_two_roots(10, "num_test4_two_roots",
                         "Number of tests to run on quartic solver where there two roots, neigher of which are double",
                          *this),
    m_num_test4_two_double_roots(10, "num_test4_two_double_roots",
                                 "Number of tests to run on quartic solver where there two double roots",
                                 *this),
    m_dist_min(-10.0f, "dist_min", "Minimum randome value to feed to solvers", *this),
    m_dist_max(10.0f, "dist_max", "Maximum randome value to feed to solvers", *this),
    m_accuracy(float_accuracy,
               enumerated_string_type<enum accuracy_t>()
               .add_entry("float", float_accuracy, "")
               .add_entry("double", double_accuracy, "")
               .add_entry("long_double", long_double_accuracy, ""),
               "accuracy", "specify accuary at which to run tests", *this)
  {}

  command_line_argument_value<int> m_num_test2;
  command_line_argument_value<int> m_num_test3;
  command_line_argument_value<int> m_num_test4;
  command_line_argument_value<int> m_num_test2_no_roots;
  command_line_argument_value<int> m_num_test3_one_root;
  command_line_argument_value<int> m_num_test3_double_root;
  command_line_argument_value<int> m_num_test3_triple_root;
  command_line_argument_value<int> m_num_test4_no_roots;
  command_line_argument_value<int> m_num_test4_two_roots;
  command_line_argument_value<int> m_num_test4_two_double_roots;
  command_line_argument_value<float> m_dist_min;
  command_line_argument_value<float> m_dist_max;
  enumerated_command_line_argument_value<enum accuracy_t> m_accuracy;
};

template<typename T>
class SolverTester
{
public:
  template<typename Distribution, typename Engine>
  static
  void
  run_tests(TestOptions &options, Distribution &dist, Engine &e)
  {
    SolverTester::test2(options.m_num_test2.value(), dist, e);
    SolverTester::test3(options.m_num_test3.value(), dist, e);
    SolverTester::test4(options.m_num_test4.value(), dist, e);
    SolverTester::test3_triple_root(options.m_num_test3_triple_root.value(), dist, e);
    SolverTester::test3_double_root(options.m_num_test3_double_root.value(), dist, e);
    SolverTester::test2_no_roots(options.m_num_test2_no_roots.value(), dist, e);
    SolverTester::test3_one_root(options.m_num_test3_one_root.value(), dist, e);
    SolverTester::test4_no_roots(options.m_num_test4_no_roots.value(), dist, e);
    SolverTester::test4_two_roots(options.m_num_test4_two_roots.value(), dist, e);
    SolverTester::test4_two_double_roots(options.m_num_test4_two_double_roots.value(), dist, e);
  }

private:
  template<typename Distribution, typename Engine, size_t N>
  static
  void
  prepare_linear_factors(astral::vecN<astral::Polynomial<T, 1>, N> &factored,
                         Distribution &dist, Engine &e)
  {
    for (unsigned int r = 0; r < N; ++r)
      {
        factored[r].coeff(1) = T(dist(e));
        factored[r].coeff(0) = T(dist(e));
      }
  }

  template<size_t N, size_t M>
  static
  void
  show_linear_roots(const astral::vecN<astral::Polynomial<T, 1>, M> &factored,
                    const astral::Polynomial<T, N> &P)
  {
    unsigned int num_roots;
    astral::vecN<T, N> roots_storage;
    astral::c_array<T> roots(roots_storage);

    if (N == M)
      {
        std::cout << "\tP = ";
        for (unsigned int i = 0; i < N; ++i)
          {
            std::cout << "(" << factored[i] << ")";
          }
        std::cout << "\n\t  = " << P << "\n";
      }

    num_roots = 0;
    for (unsigned int r = 0; r < M; ++r)
      {
        num_roots += astral::solve_polynomial<T>(factored[r], roots.sub_array(num_roots));
      }
    std::sort(roots.begin(), roots.begin() + num_roots);
    std::cout << "\t\tExpect " << num_roots << " roots: ";
    for (unsigned int r = 0; r < num_roots; ++r)
      {
        std::cout << roots[r] << " ";
      }
    std::cout << "\n";
  }

  template<size_t N>
  static
  void
  show_roots(const astral::Polynomial<T, N> &P)
  {
    unsigned int num_roots;
    astral::vecN<T, N> roots_storage;
    astral::vecN<std::complex<T>, N> complex_roots_storage;
    astral::c_array<T> roots(roots_storage);
    astral::c_array<std::complex<T>> complex_roots(complex_roots_storage);

    num_roots = astral::solve_polynomial<T>(P, roots, complex_roots_storage);
    std::sort(roots.begin(), roots.begin() + num_roots);
    std::sort(complex_roots_storage.begin(),
              complex_roots_storage.begin() + N - num_roots,
              sort_by_real<T>);

    std::cout << "\t\tFound  " << num_roots << " roots: ";
    for (unsigned int r = 0; r < num_roots; ++r)
      {
        std::cout << roots[r] << "(" << P.eval(roots[r]) << "), ";
      }
    std::cout << "\n\t\tComplex Roots:";
    for (unsigned int r = 0; r < N - num_roots; ++r)
      {
        std::complex<T> v(P.eval(complex_roots[r]));
        std::cout << complex_roots[r].real() << " + "
                  << complex_roots[r].imag() << "i "
                  << "(" << v.real() << " + " << v.imag() << "i), ";
      }
    std::cout << "\n\n";
  }

  template<typename Distribution, typename Engine>
  static
  astral::Polynomial<T, 2>
  generate_unsolvable_quadratic(Distribution &dist, Engine &e)
  {
    astral::Polynomial<T, 2> return_value;
    astral::Polynomial<T, 1> linear;

    linear.coeff(0) = T(dist(e));
    linear.coeff(1) = T(dist(e));

    return_value = astral::t_abs(T(dist(e))) * linear * linear + astral::t_abs(T(dist(e)));
    return return_value;
  }

  template<typename Distribution, typename Engine>
  static
  void
  test2(unsigned int num, Distribution &dist, Engine &e)
  {
    if (num == 0)
      {
        return;
      }

    astral::vecN<astral::Polynomial<T, 1>, 2> factored;
    astral::Polynomial<T, 2> P;

    std::cout << "Test Quadrtatic Solver:\n";
    for (unsigned int i = 0; i < num; ++i)
      {
        SolverTester::prepare_linear_factors(factored, dist, e);
        P = factored[0] * factored[1];
        SolverTester::show_linear_roots(factored, P);
        SolverTester::show_roots(P);
      }
  }

  template<typename Distribution, typename Engine>
  static
  void
  test2_no_roots(unsigned int num, Distribution &dist, Engine &e)
  {
    if (num == 0)
      {
        return;
      }
    std::cout << "Test quadratic solver on no-real roots:\n";
    for (unsigned int i = 0; i < num; ++i)
      {
        auto P = generate_unsolvable_quadratic(dist, e);

        std::cout << "\tP = " << P << "\n"
                  << "\t\tExpect 0 roots\n";
        SolverTester::show_roots(P);
      }
  }

  template<typename Distribution, typename Engine>
  static
  void
  test3(unsigned int num, Distribution &dist, Engine &e)
  {
    if (num == 0)
      {
        return;
      }
    astral::vecN<astral::Polynomial<T, 1>, 3> factored;
    astral::Polynomial<T, 3> P;

    std::cout << "Test Cubic Solver:\n";
    for (unsigned int i = 0; i < num; ++i)
      {
        SolverTester::prepare_linear_factors(factored, dist, e);
        P = factored[0] * factored[1] * factored[2];
        SolverTester::show_linear_roots(factored, P);
        SolverTester::show_roots(P);
      }
  }

  template<typename Distribution, typename Engine>
  static
  void
  test3_one_root(unsigned int num, Distribution &dist, Engine &e)
  {
    if (num == 0)
      {
        return;
      }
    std::cout << "Test cubic solver for just one real root:\n";
    for (unsigned int i = 0; i < num; ++i)
      {
        astral::vecN<astral::Polynomial<T, 1>, 1> linear;
        astral::Polynomial<T, 2> quad;
        astral::Polynomial<T, 3> P;

        SolverTester::prepare_linear_factors(linear, dist, e);
        quad = generate_unsolvable_quadratic(dist, e);
        P = quad * linear[0];
        std::cout << "\tP = (" << quad << ")(" << linear[0] << ")\n"
                  << "\t  = " << P << "\n";
        SolverTester::show_linear_roots(linear, P);
        SolverTester::show_roots(P);
      }
  }

  template<typename Distribution, typename Engine>
  static
  void
  test3_triple_root(unsigned int num, Distribution &dist, Engine &e)
  {
    if (num == 0)
      {
        return;
      }
    std::cout << "Test cubic solver for triple real root:\n";
    for (unsigned int i = 0; i < num; ++i)
      {
        astral::vecN<astral::Polynomial<T, 1>, 3> linear;
        astral::Polynomial<T, 3> P;

        linear[0].coeff(0) = T(dist(e));
        linear[0].coeff(1) = T(dist(e));
        linear[2] = linear[1] = linear[0];

        P = linear[0] * linear[1] * linear[2];
        SolverTester::show_linear_roots(linear, P);
        SolverTester::show_roots(P);
      }
  }

  template<typename Distribution, typename Engine>
  static
  void
  test3_double_root(unsigned int num, Distribution &dist, Engine &e)
  {
    if (num == 0)
      {
        return;
      }
    std::cout << "Test cubic solver for double real root:\n";
    for (unsigned int i = 0; i < num; ++i)
      {
        astral::vecN<astral::Polynomial<T, 1>, 3> linear;
        astral::Polynomial<T, 3> P;

        linear[0].coeff(0) = T(dist(e));
        linear[0].coeff(1) = T(dist(e));
        linear[1].coeff(0) = T(dist(e));
        linear[1].coeff(1) = T(dist(e));
        linear[2] = linear[1];

        P = linear[0] * linear[1] * linear[2];
        SolverTester::show_linear_roots(linear, P);
        SolverTester::show_roots(P);
      }
  }

  template<typename Distribution, typename Engine>
  static
  void
  test4(unsigned int num, Distribution &dist, Engine &e)
  {
    if (num == 0)
      {
        return;
      }
    astral::vecN<astral::Polynomial<T, 1>, 4> factored;
    astral::Polynomial<T, 4> P;

    std::cout << "Test Solver:\n";
    for (unsigned int i = 0; i < num; ++i)
      {
        SolverTester::prepare_linear_factors(factored, dist, e);
        P = factored[0] * factored[1] * factored[2] * factored[3];
        SolverTester::show_linear_roots(factored, P);
        SolverTester::show_roots(P);
      }
  }

  template<typename Distribution, typename Engine>
  static
  void
  test4_no_roots(unsigned int num, Distribution &dist, Engine &e)
  {
    if (num == 0)
      {
        return;
      }
    std::cout << "Test quartic solver on no-real roots:\n";
    for (unsigned int i = 0; i < num; ++i)
      {
        auto A = generate_unsolvable_quadratic(dist, e);
        auto B = generate_unsolvable_quadratic(dist, e);
        auto P = A * B;

        std::cout << "\tP = (" << A << ")(" << B << ")\n"
                  << "\t  = " << P << "\n";

        SolverTester::show_roots(P);
      }
  }

  template<typename Distribution, typename Engine>
  static
  void
  test4_two_roots(unsigned int num, Distribution &dist, Engine &e)
  {
    if (num == 0)
      {
        return;
      }
    std::cout << "Test quartic solver on two-real roots:\n";
    for (unsigned int i = 0; i < num; ++i)
      {
        astral::vecN<astral::Polynomial<T, 1>, 2> linear;
        astral::Polynomial<T, 2> quad;
        astral::Polynomial<T, 4> P;

        SolverTester::prepare_linear_factors(linear, dist, e);
        quad = generate_unsolvable_quadratic(dist, e);
        P = quad * linear[0] * linear[1];

        std::cout << "\tP = (" << quad << ")(" << linear[0] << ")(" << linear[1] << ")\n"
                  << "\t  = " << P << "\n";
        SolverTester::show_linear_roots(linear, P);
        SolverTester::show_roots(P);
      }
  }

  template<typename Distribution, typename Engine>
  static
  void
  test4_two_double_roots(unsigned int num, Distribution &dist, Engine &e)
  {
    if (num == 0)
      {
        return;
      }
    std::cout << "Test quartic solver on two double roots:\n";
    for (unsigned int i = 0; i < num; ++i)
      {
        astral::vecN<astral::Polynomial<T, 1>, 4> linear;
        astral::Polynomial<T, 4> P;

        linear[0].coeff(0) = linear[1].coeff(0) = T(dist(e));
        linear[0].coeff(1) = linear[1].coeff(1) = T(dist(e));
        linear[2].coeff(0) = linear[3].coeff(0) = T(dist(e));
        linear[2].coeff(1) = linear[3].coeff(1) = T(dist(e));

        P = linear[0] * linear[1] * linear[2] * linear[3];
        std::cout << "\tP = (" << linear[0] << ")^2 (" << linear[2] << ")^2\n"
                  << "\t  = " << P << "\n";
        SolverTester::show_linear_roots(linear, P);
        SolverTester::show_roots(P);
      }
  }
};

int
main(int argc, char **argv)
{
  TestOptions options;

  if (argc == 2 && options.is_help_request(argv[1]))
    {
      std::cout << "\n\nUsage: " << argv[0];
      options.print_help(std::cout);
      options.print_detailed_help(std::cout);
      return 0;
    }

  std::cout << "\n\nRunning: \"";
  for(int i = 0; i < argc; ++i)
    {
      std::cout << argv[i] << " ";
    }

  options.parse_command_line(argc, argv);
  std::cout << "\n\n" << std::flush;

  //std::random_device rd;
  std::mt19937 rd;
  float v0 = static_cast<float>(options.m_dist_min.value());
  float v1 = static_cast<float>(options.m_dist_max.value());
  std::uniform_real_distribution<float> dist(v0, v1);

  switch (options.m_accuracy.value())
    {
    case float_accuracy:
      {
        SolverTester<float>::run_tests(options, dist, rd);
      }
      break;

    case double_accuracy:
      {
        SolverTester<double>::run_tests(options, dist, rd);
      }
      break;

    case long_double_accuracy:
      {
        SolverTester<long double>::run_tests(options, dist, rd);
      }
      break;
    }

  return 0;
}

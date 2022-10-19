/*!
 * \file polynomial.hpp
 * \brief file polynomial.hpp
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

#ifndef ASTRAL_POLYNOMIAL_HPP
#define ASTRAL_POLYNOMIAL_HPP

#include <algorithm>
#include <complex>
#include <astral/util/vecN.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/math.hpp>

namespace astral
{
/*!\addtogroup Utility
 * @{
 */

  /*!
   * \brief
   * Template to give the max of two size_t compile
   * time constant values.
   */
  template<size_t A, size_t B>
  struct template_max
  {
    enum { value = A > B ? A : B };
  };

  /*!
   * \brief
   * Template class to hold a polynomial with overrides to perform
   * ring operations correctly.
   * \tparam T the type for the coefficients of the polynomial
   * \tparam D the degree of the polynomial
   */
  template<typename T, size_t D>
  class Polynomial
  {
  public:
    enum
      {
        /*!
         * enumeration to specify the degree
         */
        Degree = D,

        /*!
         * enumeration to give the degree one less, but for degree 0,
         * gives 0.
         */
        DepressedDegree = template_max<D, 1>::value - 1,
      };

    /*!
     * \brief
     * STL compliant typedef
     */
    typedef typename vecN<T, Degree + 1>::pointer pointer;

    /*!
     * \brief
     * STL compliant typedef
     */
    typedef typename vecN<T, Degree + 1>::const_pointer const_pointer;

    /*!
     * \brief
     * STL compliant typedef
     */
    typedef typename vecN<T, Degree + 1>::reference reference;

    /*!
     * \brief
     * STL compliant typedef
     */
    typedef typename vecN<T, Degree + 1>::const_reference const_reference;

    /*!
     * \brief
     * STL compliant typedef
     */
    typedef typename vecN<T, Degree + 1>::value_type value_type;

    /*!
     * \brief
     * STL compliant typedef
     */
    typedef typename vecN<T, Degree + 1>::size_type size_type;

    /*!
     * \brief
     * STL compliant typedef
     */
    typedef typename vecN<T, Degree + 1>::difference_type difference_type;

    /*!
     * \brief
     * STL compliant typedef
     */
    typedef typename vecN<T, Degree + 1>::iterator iterator;

    /*!
     * \brief
     * STL compliant typedef
     */
    typedef typename vecN<T, Degree + 1>::const_iterator const_iterator;

    /*!
     * Ctor, POD type T means coefficients are unitialized.
     */
    Polynomial(void)
    {
    }

    /*!
     * Ctor that intializes the constant coefficient with the
     * passed value and all others as 0
     * \param v value to use for the constant coefficient
     */
    explicit
    Polynomial(const T &v)
    {
      std::fill(m_data.begin() + 1, m_data.end(), T(0));
      m_data[0] = v;
    }

    /*!
     * Copy ctor
     * \param obj value from which to copy values
     */
    Polynomial(const Polynomial &obj) = default;

    /*!
     * assignment operator
     * \param rhs value from which to assign values
     */
    Polynomial&
    operator=(const Polynomial &rhs) = default;

    /*!
     * Swap operation
     * \param obj object with which to swap
     */
    void
    swap(Polynomial &obj)
    {
      m_data.swap(obj.m_data);
    }

    /*!
     * Returns the value of \ref Degree
     */
    static
    constexpr size_t
    degree(void)
    {
      return Degree;
    }

    /*!
     * Returns a C-style pointer to the coefficient array.
     */
    T*
    c_ptr(void) { return m_data.c_ptr(); }

    /*!
     * Returns a constant C-style pointer to the coefficient array.
     */
    const T*
    c_ptr(void) const { return m_data.c_ptr(); }

    /*!
     * Return a constant refernce to the t^j coefficient
     * \param j index of element to return.
     */
    const_reference
    coeff(size_type j) const
    {
      return m_data[j];
    }

    /*!
     * Return a refernce to the t^j coefficient
     * \param j index of element to return.
     */
    reference
    coeff(size_type j)
    {
      return m_data[j];
    }

    /*!
     * similair to coeff(), but returns a value
     * instead of a reference and if the index is
     * greater than the degree, return T(0)
     */
    T
    operator()(size_type j) const
    {
      return (j <= Degree) ? m_data[j] : T(0);
    }

    /*!
     * Negation operator
     */
    Polynomial
    operator-(void) const
    {
      Polynomial retval;
      for(size_type i = 0; i <= Degree; ++i)
        {
          retval.coeff(i) = -coeff(i);
        }
      return retval;
    }

    /*!
     * inplace multiply operator by scalar
     */
    void
    operator*=(const T &rhs)
    {
      m_data *= rhs;
    }

    /*!
     * inplace addition operator by scalar
     */
    void
    operator+=(const T &rhs)
    {
      m_data[0] += rhs;
    }

    /*!
     * inplace addition operator by polynomial
     */
    void
    operator+=(const Polynomial &rhs)
    {
      m_data += rhs.m_data;
    }

    /*!
     * inplace subtraction operator by scalar
     */
    void
    operator-=(const T &rhs)
    {
      m_data[0] -= rhs;
    }

    /*!
     * inplace addition operator by polynomial
     */
    void
    operator-=(const Polynomial &rhs)
    {
      m_data -= rhs.m_data;
    }

    /*!
     * multiply operator by scalar
     */
    Polynomial
    operator*(const T &rhs)
    {
      Polynomial return_value(*this);
      return_value *=rhs;
      return return_value;
    }

    /*!
     * addition operator by scalar
     */
    Polynomial
    operator+(const T &rhs)
    {
      Polynomial return_value(*this);
      return_value +=rhs;
      return return_value;
    }

    /*!
     * Returns the derivative of this Polynomial.
     */
    Polynomial<T, DepressedDegree>
    derivative(void) const
    {
      Polynomial<T, DepressedDegree> return_value(T(0));

      for (size_t i = 0; i < Degree; ++i)
        {
          return_value.coeff(i) = T(i + 1) * coeff(i + 1);
        }
      return return_value;
    }

    /*!
     * Returns the polynomial with the leading
     * term removed.
     */
    Polynomial<T, DepressedDegree>
    without_leading_term(void) const
    {
      Polynomial<T, DepressedDegree> return_value;

      std::copy(begin(), begin() + DepressedDegree + 1, return_value.begin());
      return return_value;
    }

    /*!
     * Returns the polynomial with the constant term removed
     * and then divided by the indeterminant.
     */
    Polynomial<T, DepressedDegree>
    shifted(void) const
    {
      Polynomial<T, DepressedDegree> return_value;
      if (Degree == 0)
        {
          return_value.coeff(0) = T(0);
        }
      else
        {
          std::copy(begin() + 1u, end(), return_value.begin());
        }
      return return_value;
    }

    /*!
     * Evaluate the polynomial at a value t.
     */
    T
    eval(T t) const
    {
      T return_value(coeff(Degree));

      for (size_t i = Degree; i >= 1u; --i)
        {
          return_value = t * return_value + coeff(i - 1);
        }
      return return_value;
    }

    /*!
     * Evaluate the polynomial at a complex value t.
     */
    std::complex<T>
    eval(std::complex<T> t) const
    {
      std::complex<T> return_value(coeff(Degree));

      for (size_t i = Degree; i >= 1u; --i)
        {
          return_value = t * return_value + coeff(i - 1);
        }
      return return_value;
    }

    /*!
     * Returns the coefficients of the polynomial
     * as a \ref c_array
     */
    c_array<T>
    as_c_array(void)
    {
      return c_array<T>(m_data);
    }

    /*!
     * Returns the coefficients of the polynomial
     * as a \ref c_array
     */
    c_array<const T>
    as_c_array(void) const
    {
      return c_array<const T>(m_data);
    }

    /*!
     * Returns the maximum of the absolute values
     * of all coefficients.
     */
    T
    max_abs_coeff(void) const
    {
      T return_value(t_abs(coeff(0)));

      for (size_t i = 1; i <= Degree; ++i)
        {
          return_value = t_max(return_value, t_abs(coeff(i)));
        }
      return return_value;
    }

    /*!
     * STL compliand size function, note that it is static
     */
    static
    size_t
    size(void) { return vecN<T, Degree + 1>::size(); }

    /*!
     * STL compliant iterator function.
     */
    iterator
    begin(void) { return m_data.begin(); }

    /*!
     * STL compliant iterator function.
     */
    const_iterator
    begin(void) const { return m_data.begin(); }

    /*!
     * STL compliant iterator function.
     */
    iterator
    end(void) { return m_data.end(); }

    /*!
     * STL compliant iterator function.
     */
    const_iterator
    end(void) const { return m_data.end(); }

    /*!
     * STL compliant back() function.
     */
    reference
    back(void) { return m_data.back(); }

    /*!
     * STL compliant back() function.
     */
    const_reference
    back(void) const { return m_data.back(); }

    /*!
     * STL compliant front() function.
     */
    reference
    front(void) {return  m_data.front(); }

    /*!
     * STL compliant front() function.
     */
    const_reference
    front(void) const { return m_data.front(); }

  private:
    vecN<T, Degree + 1> m_data;
  };

  /*!
   * Sums two Polynomial with possible different degrees
   * \param lhs left hand of operator
   * \param rhs right hand of operator
   */
  template<typename T, size_t A, size_t B>
  Polynomial<T, template_max<A, B>::value>
  operator+(const Polynomial<T, A> &lhs, const Polynomial<T, B> &rhs)
  {
    Polynomial<T, template_max<A, B>::value> return_value;
    for (size_t i = 0; i <= template_max<A, B>::value; ++i)
      {
        return_value.coeff(i) = lhs(i) + rhs(i);
      }
    return return_value;
  }

  /*!
   * Subtract two Polynomial with possible different degrees
   * \param lhs left hand of operator
   * \param rhs right hand of operator
   */
  template<typename T, size_t A, size_t B>
  Polynomial<T, template_max<A, B>::value>
  operator-(const Polynomial<T, A> &lhs, const Polynomial<T, B> &rhs)
  {
    Polynomial<T, template_max<A, B>::value> return_value(T(0));
    for (size_t i = 0; i <= template_max<A, B>::value; ++i)
      {
        return_value.coeff(i) = lhs(i) - rhs(i);
      }
    return return_value;
  }

  /*!
   * Multiply two Polynomial with possible different degrees
   * \param lhs left hand of operator
   * \param rhs right hand of operator
   */
  template<typename T, size_t A, size_t B>
  Polynomial<T, A + B>
  operator*(const Polynomial<T, A> &lhs, const Polynomial<T, B> &rhs)
  {
    Polynomial<T, A + B> return_value(T(0));
    for (size_t i = 0; i <= A; ++i)
      {
        for (size_t j = 0; j <= B; ++j)
          {
            return_value.coeff(i + j) += lhs.coeff(i) * rhs.coeff(j);
          }
      }
    return return_value;
  }

  /*!
   * Add a polynomial to a constant
   * \param lhs left hand of operator
   * \param rhs right hand of operator
   */
  template<typename T, size_t A>
  Polynomial<T, A>
  operator+(const T &lhs, const Polynomial<T, A> &rhs)
  {
    Polynomial<T, A> return_value(rhs);
    return_value.coeff(0) = lhs + return_value.coeff(0);
    return return_value;
  }

  /*!
   * Subtract a polynomial from a constant
   * \param lhs left hand of operator
   * \param rhs right hand of operator
   */
  template<typename T, size_t A>
  Polynomial<T, A>
  operator-(const T &lhs, const Polynomial<T, A> &rhs)
  {
    Polynomial<T, A> return_value(-rhs);
    return_value.coeff(0) = lhs - rhs.coeff(0);
    return return_value;
  }

  /*!
   * Multiply a polynomial by a constant
   * \param lhs left hand of operator
   * \param rhs right hand of operator
   */
  template<typename T, size_t A>
  Polynomial<T, A>
  operator*(const T &lhs, const Polynomial<T, A> &rhs)
  {
    Polynomial<T, A> return_value;
    for (size_t i = 0; i <= A; ++i)
      {
        return_value.coeff(i) = lhs * rhs.coeff(i);
      }
    return return_value;
  }

  /*!
   * Convert a polynomial of vecN values into a vecN of polynomials
   * \param P polynomial of vectors to covert to a vector of polynomial
   */
  template<typename T, size_t N, size_t A>
  vecN<Polynomial<T, A>, N>
  convert(const Polynomial<vecN<T, N>, A> &P)
  {
    vecN<Polynomial<T, A>, N> return_value;

    for (unsigned int d = 0; d <= A; ++d)
      {
        for (unsigned int i = 0; i < N; ++i)
          {
            return_value[i].coeff(d) = P.coeff(d)[i];
          }
      }
    return return_value;
  }

  /*!
   * Convert a vecN of polynomials into a polynomial of vecN values
   * \param P vector of polynomialsto covert to a polynomial of vectors
   */
  template<typename T, size_t N, size_t A>
  Polynomial<vecN<T, N>, A>
  convert(const vecN<Polynomial<T, A>, N> &P)
  {
    Polynomial<vecN<T, N>, A> return_value;

    for (unsigned int d = 0; d <= A; ++d)
      {
        for (unsigned int i = 0; i < N; ++i)
          {
            return_value.coeff(d)[i] = P[i].coeff(d);
          }
      }
    return return_value;
  }

  /*!
   * Compute the dot product of two polynomials vectors
   * \param lhs left hand of operator
   * \param rhs right hand of operator
   */
  template<typename T, size_t N, size_t A, size_t B>
  Polynomial<T, A + B>
  poly_dot(const vecN<Polynomial<T, A>, N> &lhs, const vecN<Polynomial<T, B>, N> &rhs)
  {
    Polynomial<T, A + B> return_value(T(0));

    for (size_t i = 0; i < N; ++i)
      {
        return_value += rhs[i] * lhs[i];
      }
    return return_value;
  }

  /*!
   * Compute the dot product of two vector polynomials
   * \param lhs left hand of operator
   * \param rhs right hand of operator
   */
  template<typename T, size_t N, size_t A, size_t B>
  Polynomial<T, A + B>
  poly_dot(const Polynomial<vecN<T, N>, A> &lhs, const Polynomial<vecN<T, N>, B> &rhs)
  {
    return poly_dot(convert(lhs), convert(rhs));
  }

  /*!
   * Solve a linear equation
   * \param L coefficient of polynomial, size must be two
   * \param solutions location to which to place the solutions
   * \returns the number of real solutions
   */
  template<typename T>
  unsigned int
  solve_linear(c_array<const T> L, c_array<T> solutions)
  {
    ASTRALassert(L.size() == 2);
    if (L[1] == T(0))
      {
        return 0;
      }

    solutions[0] = -L[0] / L[1];
    return 1;
  }

  /*!
   * Solve a quadratic equation; multi-roots are counted and listed in their multiplicity
   * \param Q the coefficients of the polynomial to solve for
   * \param solutions the array in which to place the found solutions
   * \param complex_solutions if non-empty, the array in which to place
   *                          the complex solutions
   * \returns the number of real solutions
   */
  template<typename T>
  unsigned int
  solve_quadratic(c_array<const T> Q, c_array<T> solutions,
                  c_array<std::complex<T>> complex_solutions = c_array<std::complex<T>>())
  {
    ASTRALassert(Q.size() == 3);
    ASTRALassert(solutions.size() >= 2);
    ASTRALassert(complex_solutions.empty() || complex_solutions.size() >= 2);

    if (Q[2] == T(0))
      {
        return solve_linear(Q.sub_array(0, 2), solutions);
      }

    T descr, root_descr;
    T inverse_quad(T(1) / Q[2]);

    descr = Q[1] * Q[1] - T(4) * Q[0] * Q[2];
    if (descr < T(0))
      {
        if (!complex_solutions.empty())
          {
            T imag, real;

            inverse_quad /= T(2);
            real = -Q[1] * inverse_quad;
            imag = inverse_quad * t_sqrt(t_abs(descr));

            complex_solutions[0].real(real);
            complex_solutions[0].imag(imag);

            complex_solutions[1].real(real);
            complex_solutions[1].imag(-imag);
          }
        return 0;
      }

    root_descr = t_sqrt(t_max(T(0), descr));
    if (Q[1] < T(0))
      {
        solutions[0] = T(0.5) * inverse_quad * (-Q[1] + root_descr);
        solutions[1] = T(2) * Q[0] / (-Q[1] + root_descr);
      }
    else
      {
        solutions[0] = T(2) * Q[0] / (-Q[1] - root_descr);
        solutions[1] = T(0.5) * inverse_quad * (-Q[1] - root_descr);
      }

    return 2;
  }

  /*!
   * Solve a cubic equation; multi-roots are counted and listed in their multiplicity
   * \param P the coefficients of the polynomial to solve for
   * \param solutions the array in which to place the found solutions
   * \param complex_solutions if non-empty, the array in which to place
   *                          the complex solutions
   * \returns the number of real solutions
   */
  template<typename T>
  unsigned int
  solve_cubic(c_array<const T> P, c_array<T> solutions,
              c_array<std::complex<T>> complex_solutions = c_array<std::complex<T>>())
  {
    ASTRALassert(P.size() == 4);
    ASTRALassert(solutions.size() >= 3);
    ASTRALassert(complex_solutions.empty() || complex_solutions.size() >= 2);

    if (P[3] == T(0))
      {
        return solve_quadratic(P.sub_array(0, 3), solutions, complex_solutions);
      }

    T inverse_a = T(1) / P[3];
    T b = P[2] * inverse_a;
    T c = P[1] * inverse_a;
    T d = P[0] * inverse_a;

    T b3 = b * b * b;
    T q = (T(2) * b3 - T(9) * b * c + T(27) * d) / T(54);
    T p = (b * b - T(3) * c) / T(9);
    T q2 = q * q;
    T p3 = p * p * p;
    T D = q2 - p3;
    T offset = -b / T(3);

    if (D >= T(0))
      {
        unsigned int quad_return_value;
        T rootD, G, H, bottom, r;

        rootD = t_sqrt(t_abs(D));

        /*
         * The value of G and H we want are
         *
         *  G = -q + sqrt(D)
         *  H = -q - sqrt(D)
         *
         * where
         *
         *  D = q * q - p3;
         *
         * we make it more numerically stable by noting that
         *
         *   G = -q + sqrt(D) = (q * q - D) / (-q - sqrt(D))
         *                     = p3 / (-q - sqrt(D))
         *
         * and
         *
         *   H = -q - sqrt(D) = (q * q - D) / (-q + sqrt(D))
         *                     = p3 / (-q + sqrt(D))
         *
         * So, if q > 0, we use the alternative form of G and
         * if q < 0, we use the alternative form of H.
         */
        bottom = (-q - t_sign(q) * rootD);
        G = p3 / bottom;
        H = bottom;

        r = t_cbrt(G) + t_cbrt(H);
        solutions[0] = r + offset;

        /* Recall that
         *
         *  x^3 + bx^2 + cx + d = 0 (1)
         *
         * is the same as
         *
         *  t^3 - 3pt + 2q (2)
         *
         * where x = t - offset. We have that
         * r = t_cbrt(G) + t_cbrt(H) is a solution
         * to solution to (2) which means that
         *
         * t^3 - 3pt + 2q = (t^2 + At + B)(t - r)
         *
         * for some real A and B. This gives us the
         * equations:
         *
         *     -Br = 2q
         *  B - Ar = -3p
         *   A - r = 0
         *
         * which yields
         *
         *   A = r
         *   B = -3p + r^2
         *
         * The roots to the quadratic -should- be complex; however round-off
         * error might actually make them real.
         */
        vecN<T, 3> quad;

        quad[2] = T(1);
        quad[1] = r;
        quad[0] = -T(3) * p + r * r;
        quad_return_value = solve_quadratic(c_array<const T>(quad), solutions.sub_array(1), complex_solutions);

        if (quad_return_value != 0)
          {
            solutions[1] += offset;
            solutions[2] += offset;
          }
        else if (!complex_solutions.empty())
          {
            complex_solutions[0] += offset;
            complex_solutions[1] += offset;
          }

        return 1 + quad_return_value;
      }
    else
      {
        const T TWO_PI_OVER_3 = T(2.09439510239);
        T r, rc, theta, coeff;

        r = q / t_sqrt(p3);
        rc = t_max(T(-1), t_min(T(1), r));
        theta = t_acos(rc) / T(3);
        coeff = T(-2) * t_sqrt(p);

        solutions[0] = offset + coeff * t_cos(theta);
        solutions[1] = offset + coeff * t_cos(theta + TWO_PI_OVER_3);
        solutions[2] = offset + coeff * t_cos(theta - TWO_PI_OVER_3);

        return 3;
      }
  }

  /*!
   * Let r = sqrt(a * a - b), computes in a numerically
   * more stable way the values r1 = a + r and r2 = a - r
   * and returns the value of r.
   * \param a value
   * \param b value
   * \param r1 location to place value r1
   * \param r2 location to place value r2
   */
  template<typename T>
  T
  stable_radical(T a, T b, T *r1, T *r2)
  {
    T r;

    r = t_sqrt(t_max(a * a - b, T(0)));
    if (a > T(0))
      {
        *r1 = a + r;
        *r2 = b / *r1;
      }
    else
      {
        *r2 = a - r;
        *r1 = b / *r2;
      }

    return r;
  }

  /*!
   * Solve a quartic equation, triple and double roots are counted
   * multiple times.
   * \param P the coefficients of the polynomial to solve for
   * \param solutions the array in which to place the found solutions
   * \param complex_solutions if non-empty, the array in which to place
   *                          the complex solutions
   * \returns the number of real solutions
   */
  template<typename T>
  unsigned int
  solve_quartic(c_array<const T> P, c_array<T> solutions,
                c_array<std::complex<T>> complex_solutions = c_array<std::complex<T>>())
  {
    ASTRALassert(P.size() == 5);
    ASTRALassert(solutions.size() >= 4);
    ASTRALassert(complex_solutions.empty() || complex_solutions.size() >= 4);

    if (P[4] == T(0))
      {
        return solve_cubic(P.sub_array(0, 4), solutions, complex_solutions);
      }

    /* Algorithm comes from
     *
     *  https://github.com/sasamil/Quartic/blob/master/theorymath_eng.docx
     *
     * It is as follows. Any quartic polynomial with real coefficients
     * admits a factorization into two quadratic polynomials with real
     * coefficients (this is the Fundemental Theorem of Algebra restricted
     * to the real line):
     *
     *   t^4 + at^3 + bt^2+ ct + d = (t^2 + p1 t + q1)(t^2 + p2 t + q2) (1)
     *
     * Which generates the system of equations
     *
     *   p1 + p2        = a  (2)
     *   p1p2 + q1 + q2 = b  (3)
     *   p1q2 + p2q1    = c  (4)
     *   q1q2           = d  (5)
     *
     * Introducing a new variable y = q1 + q2, let q = q1 and p = p1,
     * then (1) gives that p2 = a - p1. Then
     *
     * (5) ---> q(y - q) = d     --->  q^2 - yq + d = 0      (6)
     * (3) ---> p(a - p) + y = b --->  p^2 - ap + b - y = 0  (7)
     *
     * Using (6) and (7), we get two roots for p and q:
     *
     *         y +- sqrt(y^2 - 4d)
     *    q = --------------------        (8)
     *
     *         a +- sqrt(a^2 - 4b + 4y)
     *    p = -------------------------   (9)
     *                   2
     *
     * We can then plug in these roots above into (4). Remarkably, a critical
     * cancellation happens and one gets
     *
     *   2c = ay +/- sqrt((y^2 - 4d)(a^2 - 4b + 4y))
     *
     * where the pairing of +- of the p and q affects the sign on the sqrt().
     * Squaring both sides (which washes out the sign) and hammering with
     * algebra yields:
     *
     *   4y^3 + (-4b)y^2 + (-16d + 4ac)y + (16bd - 4da^2 - 4c^2) = 0
     *
     * Killing off the common factor of 4 gives
     *
     *  y^3 + (-b)y^2 + (ac - 4d)y + (4db - a^2 - c^2) = 0
     *
     * which is a cubic we solve with solve_cubic(). That cubic
     * has (essentially) 1 or 3 roots. If it has one root, that
     * is *THE* y we are to use. If there are three real y's,
     * (y1, y2, y3), then we need to dig deeper to decide which of
     * the roots are acceptable or not. From construction, we know that
     * for one of these y's that q1 and q2 are real. From the solution
     * of q1 and q2, that happens when y^2 - 4d >= 0. So since we know
     * that there is a real solution, then atleast one of these roots
     * satsifies that, thus just take the y with largest absolute value.
     *
     * Once we have the y, computing p or q is straigtfoward. If we
     * get q first, we use 8 to get q1 and q2. To get p1 and p2, we
     * do not use (9), and instead use (2) and (4) which give the
     * linear system
     *
     *   |  1   1 |   | p1 |   | a |
     *   |        | * |    | = |   |
     *   | q2  q1 |   | p2 |   | c |
     *
     * which by linear algebra gives
     *
     *        a * q1 - c      a * q1 - c
     *  p1 = ------------ = ---------------
     *          q1 - q2      sqrt(y^2 - 4d)
     *
     *        c - a * q2      c - a * q2
     *  p2 = ------------ = ---------------
     *          q1 - q2      sqrt(y^2 - 4d)
     *
     * We can also choose to solve for p1/p2 first by (9)
     * and generate q1 and q2 afterwards using(3) and (4)
     * which give the following linear system:
     *
     *   |  1   1 |   | q1 |   | b - p1p2 |   | y |
     *   |        | * |    | = |          | = |   |
     *   | p2  p1 |   | q2 |   |    c     |   | c |
     *
     * where the last equality follow from the solution of
     * p1 and p2 in terms of y.
     *
     * Thus,
     *
     *        y * p1 - c        y * p1 - c
     *  q1 = ------------ = --------------------
     *          p1 - p2      sqrt(a^2 - 4b + 4y)
     *
     *        c - y * p2        c - y * p2
     *  q2 = ------------ = --------------------
     *          p1 - p2      sqrt(a^2 - 4b + 4y)
     *
     * Which we choose: to find p1/p2 or q1/q2 from radicals is based off
     * of which of the radicals is most positive: Mp = a^2 - 4b + 4y
     * or Mq = y^2 - 4d. In addition, when choosing the y root to use
     * for Mp we do not use the y with largest absolute value, instead
     * we use the y-root which is the most positive (i.e. a simple max).
     * Doing so makes the quartic solver more numerically stable.
     *
     * The quadratic formula working on the quadratics from (1) reduces to
     * the half the value of the roots returned by stable_radical(-p1, 4q1)
     * and stable_radical(-p2, 4q2)
     *
     * This code is NOT derived from the GPL'd code of the repo. Only the
     * algebraic derivation from the docx file is used. Indeed, even there
     * we have added additional features to make it more robust. Additionally,
     * this code strongly differs from the GPL'd code as well:
     *  - template function
     *  - no memory allocation
     *  - ignores double root of y-cubic (this might be an issue actually)
     *  - better numerical stablity via stable_radical(p, 4q)
     *  - better numerical stablity from the Mp and Mq choosing
     *  - additional algebraic simplifications
     */

    T inv = T(1) / P[4];
    T a = P[3] * inv;
    T b = P[2] * inv;
    T c = P[1] * inv;
    T d = P[0] * inv;
    T q1, q2, p1, p2, M, Mp, Mq, y, max_y, b_max_y;
    unsigned int return_value(0);

    vecN<T, 4> cubic_storage;
    vecN<T, 3> cubic_root_storage;
    c_array<const T> cubic(cubic_storage);
    c_array<T> cubic_roots(cubic_root_storage);
    unsigned int num_cubic_roots;

    cubic_storage[3] = T(1);
    cubic_storage[2] = -b;
    cubic_storage[1] = a * c - T(4) * d;
    cubic_storage[0] = -a * a * d - c * c + T(4) * b * d;

    /* Ick. We require to have a -real- solution to the
     * cubic, not a complex one. TODO: add some code that
     * takes the complex roots and performs some numerical
     * guesses to decide if the complex roots are actually
     * real roots where the imaginary parts come from
     * round-off error.
     */
    num_cubic_roots = solve_cubic(cubic, cubic_roots);
    y = max_y = cubic_roots[0];
    if (num_cubic_roots > 1u)
      {
        if (t_abs(cubic_roots[1]) > t_abs(y))
          {
            y = cubic_roots[1];
          }
        max_y = t_max(max_y, cubic_roots[1]);
      }
    if (num_cubic_roots > 2u)
      {
        if (t_abs(cubic_roots[2]) > t_abs(y))
          {
            y = cubic_roots[2];
          }
        max_y = t_max(max_y, cubic_roots[2]);
      }

    b_max_y = T(4) * (b - max_y);
    Mq = y * y - T(4) * d;
    Mp = a * a - b_max_y;
    if (Mp > Mq)
      {
        T rootM, r1, r2;

        rootM = stable_radical(a, b_max_y, &r1, &r2);

        p1 = r1 / T(2);
        p2 = r2 / T(2);

        q1 = (max_y * p1 - c) / rootM;
        q2 = (c - max_y * p2) / rootM;
      }
    else
      {
        T rootM, r1, r2;

        rootM = stable_radical(y, T(4) * d, &r1, &r2);
        q1 = r1 / T(2);
        q2 = r2 / T(2);

        p1 = (a * q1 - c) / rootM;
        p2 = (c - a * q2) / rootM;
      }

    M = p1 * p1 - T(4) * q1;
    if (M >= T(0))
      {
        T r1, r2;

        stable_radical(-p1, T(4) * q1, &r1, &r2);
        solutions[0] = r1 / T(2);
        solutions[1] = r2 / T(2);

        solutions = solutions.sub_array(2);
        return_value += 2;
      }
    else if (!complex_solutions.empty())
      {
        T real, imag;

        real = -p1 / T(2);
        imag = t_sqrt(t_abs(M)) / T(2);

        complex_solutions[0].real(real);
        complex_solutions[0].imag(imag);

        complex_solutions[1].real(real);
        complex_solutions[1].imag(-imag);

        complex_solutions = complex_solutions.sub_array(2);
      }

    M = p2 * p2 - T(4) * q2;
    if (M >= T(0))
      {
        T r1, r2;

        stable_radical(-p2, T(4) * q2, &r1, &r2);
        solutions[0] = r1 / T(2);
        solutions[1] = r2 / T(2);

        return_value += 2;
      }
    else if (!complex_solutions.empty())
      {
        T real, imag;

        real = -p2 / T(2);
        imag = t_sqrt(t_abs(M)) / T(2);

        complex_solutions[0].real(real);
        complex_solutions[0].imag(imag);

        complex_solutions[1].real(real);
        complex_solutions[1].imag(-imag);
      }

    return return_value;
  }

  /*!
   * Solve a linear equation
   * \param polynomial the polynomial to solve for
   * \param solutions the array in which to place the found solutions
   * \param complex_solutions if non-empty, the array in which to place
   *                          the complex solutions
   * \returns the number of real solutions
   */
  template<typename T>
  unsigned int
  solve_polynomial(const Polynomial<T, 1> &polynomial, c_array<T> solutions,
                   c_array<std::complex<T>> complex_solutions = c_array<std::complex<T>>())
  {
    ASTRALunused(complex_solutions);
    return solve_linear(polynomial.as_c_array(), solutions);
  }

  /*!
   * Solve a quadratic equation, a double root is counted twice
   * \param polynomial the polynomial to solve for
   * \param solutions the array in which to place the found solutions
   * \param complex_solutions if non-empty, the array in which to place
   *                          the complex solutions
   * \returns the number of real solutions
   */
  template<typename T>
  unsigned int
  solve_polynomial(const Polynomial<T, 2> &polynomial, c_array<T> solutions,
                   c_array<std::complex<T>> complex_solutions = c_array<std::complex<T>>())
  {
    return solve_quadratic(polynomial.as_c_array(), solutions, complex_solutions);
  }

  /*!
   * Solve a cubic equation; multi-roots are counted and listed in their multiplicity
   * \param polynomial the polynomial to solve for
   * \param solutions the array in which to place the found solutions
   * \param complex_solutions if non-empty, the array in which to place
   *                          the complex solutions
   * \returns the number of real solutions
   */
  template<typename T>
  unsigned int
  solve_polynomial(const Polynomial<T, 3> &polynomial, c_array<T> solutions,
                   c_array<std::complex<T>> complex_solutions = c_array<std::complex<T>>())
  {
    return solve_cubic(polynomial.as_c_array(), solutions, complex_solutions);
  }

  /*!
   * Solve a quartic equation; multi-roots are counted and listed in their multiplicity
   * \param polynomial the polynomial to solve for
   * \param solutions the array in which to place the found solutions
   * \param complex_solutions if non-empty, the array in which to place
   *                          the complex solutions
   * \returns the number of real solutions
   */
  template<typename T>
  unsigned int
  solve_polynomial(const Polynomial<T, 4> &polynomial, c_array<T> solutions,
                   c_array<std::complex<T>> complex_solutions = c_array<std::complex<T>>())
  {
    return solve_quartic(polynomial.as_c_array(), solutions, complex_solutions);
  }

  /*!
   * Solve a polynomial equation of degree no more than four; multi-roots are counted
   * and listed in their multiplicity
   * \param polynomial the coefficients for the polynomial to solve for
   * \param solutions the array in which to place the found solutions
   * \param complex_solutions if non-empty, the array in which to place
   *                          the complex solutions
   * \returns the number of real solutions
   */
  template<typename T>
  unsigned int
  solve_polynomial(c_array<const T> polynomial, c_array<T> solutions,
                   c_array<std::complex<T>> complex_solutions = c_array<std::complex<T>>())
  {
    switch (polynomial.size())
      {
      default:
        ASTRALfailure("Too large degree polynomial passed");
        // fall through
      case 0:
      case 1:
        return 0;

      case 2:
        return solve_linear(polynomial, solutions);

      case 3:
        return solve_quadratic(polynomial, solutions, complex_solutions);

      case 4:
        return solve_cubic(polynomial, solutions, complex_solutions);

      case 5:
        return solve_quartic(polynomial, solutions, complex_solutions);
      }
  }
/*! @} */

}

#endif

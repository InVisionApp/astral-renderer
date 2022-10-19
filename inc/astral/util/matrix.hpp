/*!
 * \file matrix.hpp
 * \brief file matrix.hpp
 *
 * Adapted from: matrixGL.hpp of WRATH:
 *
 * Copyright 2013 by Nomovok Ltd.
 * Contact: info@nomovok.com
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */


#ifndef ASTRAL_MATRIX_HPP
#define ASTRAL_MATRIX_HPP

#include <astral/util/math.hpp>
#include <astral/util/vecN.hpp>

namespace astral {

/*!\addtogroup Utility
 * @{
 */

/*!
 * \brief
 * A generic matrix class with packing as follows:
 * \code
 * matrix::row_col(row, col) <--> matrix::col_row(col, row) <--> raw_data()[row + N * col]
 * \endcode
 *
 *\tparam N Number of rows of the matrix
 *\tparam M Number of columns of the matrix
 *\tparam T matrix entry type
 */
template<size_t N, size_t M, typename T = float>
class matrix
{
private:
  vecN<T, N * M> m_data;

public:
  /*!
   * \brief
   * Typedef to underlying vecN that holds
   * the matrix data.
   */
  typedef vecN<T, N * M> raw_data_type;

  enum
    {
      /*!
       * Enumeration value for size of the matrix.
       */
      number_rows = N,

      /*!
       * Enumeration value for size of the matrix.
       */
      number_cols = M,
    };

  /*!
   * Copy-constructor for a NxN matrix.
   * \param obj matrix to copy
   */
  matrix(const matrix &obj):
    m_data(obj.m_data)
  {}

  /*!
   * Ctor.
   * Initializes an NxM matrix as diagnols are 1
   * and other values are 0, for square matrices,
   * that is the identity matrix.
   */
  matrix(void)
  {
    reset();
  }

  /*!
   * Assignment operator
   */
  matrix&
  operator=(const matrix&) = default;

  /*!
   * Equality operator by checking each element individually.
   * \param obj vecN to which to compare.
   */
  bool
  operator==(const matrix &obj) const
  {
    return m_data == obj.m_data;
  }

  /*!
   * Provided as a conveniance, equivalent to
   * \code
   * !operator==(obj)
   * \endcode
   * \param obj vecN to which to compare.
   */
  bool
  operator!=(const matrix &obj) const
  {
    return !operator==(obj);
  }

  /*!
   * Set matrix as identity.
   */
  void
  reset(void)
  {
    for(unsigned int i = 0; i < M; ++i)
      {
        for(unsigned int j = 0; j < N; ++j)
          {
            m_data[N * i + j] = (i == j) ? T(1): T(0);
          }
      }
  }

  /*!
   * Swaps the values between this and the parameter matrix.
   * \param obj matrix to swap values with
   */
  void
  swap(matrix &obj)
  {
    m_data.swap(obj.m_data);
  }

  /*!
   * Returns a c-style pointer to the data.
   */
  T*
  c_ptr(void) { return m_data.c_ptr(); }

  /*!
   * Returns a constant c-style pointer to the data.
   */
  const T*
  c_ptr(void) const { return m_data.c_ptr(); }

  /*!
   * Returns a reference to raw data vector in the matrix.
   */
  vecN<T, N * M>&
  raw_data(void) { return m_data; }

  /*!
   * Returns a const reference to the raw data vectors in the matrix.
   */
  const vecN<T, N * M>&
  raw_data(void) const { return m_data; }

  /*!
   * Returns the named entry of the matrix
   * \param row row(vertical coordinate) in the matrix
   * \param col column(horizontal coordinate) in the matrix
   */
  T&
  row_col(unsigned int row, unsigned int col)
  {
    ASTRALassert(row < N);
    ASTRALassert(col < M);
    return m_data[N * col + row];
  }

  /*!
   * Returns the named entry of the matrix
   * \param row row(vertical coordinate) in the matrix
   * \param col column(horizontal coordinate) in the matrix
   */
  const T&
  row_col(unsigned int row, unsigned int col) const
  {
    ASTRALassert(row < N);
    ASTRALassert(col < M);
    return m_data[N * col + row];
  }

  /*!
   * Returns the named entry of the matrix; provided
   * as a conveniance to interface with systems where
   * access of elements is column then row or to more
   * easily access the transpose of the matrix.
   * \param col column(horizontal coordinate) in the matrix
   * \param row row(vertical coordinate) in the matrix
   */
  T&
  col_row(unsigned int col, unsigned row)
  {
    ASTRALassert(row < N);
    ASTRALassert(col < M);
    return m_data[N * col + row];
  }

  /*!
   * Returns the named entry of the matrix; provided
   * as a conveniance to interface with systems where
   * access of elements is column then row or to more
   * easily access the transpose of the matrix.
   * \param col column(horizontal coordinate) in the matrix
   * \param row row(vertical coordinate) in the matrix
   */
  const T&
  col_row(unsigned int col, unsigned row) const
  {
    ASTRALassert(row < N);
    ASTRALassert(col < M);
    return m_data[N * col + row];
  }

  /*!
   * Compute the transpose of the matrix
   * \param retval location to which to write the transpose
   */
  void
  transpose(matrix<M, N, T> &retval) const
  {
    for(unsigned int i = 0; i < N; ++i)
      {
        for(unsigned int j = 0; j < M; ++j)
          {
            retval.row_col(i, j) = row_col(j, i);
          }
      }
  }

  /*!
   * Returns a transpose of the matrix.
   */
  matrix<M, N, T>
  transpose(void) const
  {
    matrix<M, N, T>  retval;
    transpose(retval);
    return retval;
  }

  /*!
   * Operator for adding matrices together.
   * \param rhs target matrix
   */
  matrix
  operator+(const matrix &rhs) const
  {
    matrix out;
    out.m_data = m_data + rhs.m_data;
    return out;
  }

  /*!
   * Operator for substracting matrices from each other.
   * \param rhs target matrix
   */
  matrix
  operator-(const matrix &rhs) const
  {
    matrix out;
    out.m_data = m_data - rhs.m_data;
    return out;
  }

  /*!
   * Multiplies the matrix with a given scalar.
   * \param value scalar to multiply with
   */
  matrix
  operator*(T value) const
  {
    matrix out;
    out.m_data = m_data * value;
    return out;
  }

  /*!
   * Multiplies the given matrix with the given scalar
   * and returns the resulting matrix.
   * \param value scalar to multiply with
   * \param rhs right hand side of multiply
   */
  friend
  matrix
  operator*(T value, const matrix &rhs)
  {
    matrix out;
    out.m_data = rhs.m_data * value;
    return out;
  }

  /*!
   * Multiplies this matrix with the given matrix.
   * \param rhs right hand side of multiply
   */
  template<size_t K>
  matrix<N, K, T>
  operator*(const matrix<M, K, T> &rhs) const
  {
    unsigned int i,j,k;
    matrix<N, K, T> out;

    for(i = 0; i < N; ++i)
      {
        for(j = 0; j < K; ++j)
          {
            out.row_col(i, j) = T(0);
            for(k = 0; k < M; ++k)
              {
                out.row_col(i, j) += row_col(i, k) * rhs.row_col(k, j);
              }
          }
      }
    return out;
  }

  /*!
   * Computes the value of \ref matrix * \ref vecN
   * \param in right operand of multiply
   */
  vecN<T, N>
  operator*(const vecN<T, M> &in) const
  {
    vecN<T, N> retval;

    for(unsigned int i = 0; i < N;++i)
      {
        retval[i] = T(0);
        for(unsigned int j = 0; j < M; ++j)
          {
            retval[i] += row_col(i, j) * in[j];
          }
      }

    return retval;
  }

  /*!
   * Computes the value of \ref vecN * \ref matrix
   * \param in left operand of multiply
   * \param rhs right operand of multiply
   */
  friend
  vecN<T, M>
  operator*(const vecN<T, N> &in, const matrix &rhs)
  {
    vecN<T, M> retval;

    for(unsigned int i = 0; i < M; ++i)
      {
        retval[i] = T(0);
        for(unsigned int j = 0; j < N; ++j)
          {
            retval[i] += in[j] * rhs.row_col(j, i);
          }
      }
    return retval;
  }
};

/*!
 * \brief
 * Convenience typedef to matrix\<2, 2, float\>
 */
typedef matrix<2, 2, float> float2x2;

/*!
 * \brief
 * Convenience typedef to matrix\<3, 3, float\>
 */
typedef matrix<3, 3, float> float3x3;

/*!
 * \brief
 * Convenience typedef to matrix\<4, 4, float\>
 */
typedef matrix<4, 4, float> float4x4;

/*!
 * Enumeration to describe mapping properties
 * of a 2x2 matrix
 */
enum matrix_type_t:uint32_t
  {
    /*!
     * Indicates that the matrix is a diagonal
     * matrix.
     */
    matrix_diagonal,

    /*!
     * Indicates that the matrix has 0 in its
     * diagonal entries; these matrices still
     * map coordinate aligned rectangles to
     * coordinate aligned rectangles.
     */
    matrix_anti_diagonal,

    /*!
     * Indicates that the matrix does not preserve
     * coordiante aligned rectangles, i.e. every
     * singular value decomposition of the matrix
     * has a rotation.
     */
    matrix_generic,
  };

/*!
 * Computes the astral::matrix_type_t of a 2x2 matrix
 */
template<typename T>
enum matrix_type_t
compute_matrix_type(const matrix<2, 2, T> &M)
{
  if (M.row_col(1, 0) == T(0)
      && M.row_col(0, 1) == T(0))
    {
      return matrix_diagonal;
    }

  if (M.row_col(0, 0) == T(0)
      && M.row_col(1, 1) == T(0))
    {
      return matrix_anti_diagonal;
    }

  return matrix_generic;
}

/*!
 * Computes the dterminant of a 2x2 matrix
 */
template<typename T>
T
compute_determinant(const matrix<2, 2, T> &M)
{
  return M.row_col(0, 0) * M.row_col(1, 1) - M.row_col(0, 1) * M.row_col(1, 0);
}

/*!
 * Returns a 2x2 matrix that corresponds to applying
 * a scaleing.
 */
template<typename T>
matrix<2, 2, T>
scale_matix(const vecN<T, 2> &scale)
{
  matrix<2, 2, T> M;

  M.row_col(0, 0) = scale.x();
  M.row_col(1, 1) = scale.y();

  return M;
}

/*!
 * Returns a 2x2 matrix that corresponds to applying
 * a scaling.
 */
template<typename T>
matrix<2, 2, T>
scale_matrix(const T &scale)
{
  matrix<2, 2, T> M;

  M.row_col(0, 0) = scale;
  M.row_col(1, 1) = scale;

  return M;
}

/*!
 * Compute the singular values of a 2x2 matrix.
 */
vecN<float, 2>
compute_singular_values(const float2x2 &M, enum matrix_type_t tp);

/*!
 * Compute the singular values of a 2x2 matrix.
 */
inline
vecN<float, 2>
compute_singular_values(const float2x2 &M)
{
  return compute_singular_values(M, compute_matrix_type(M));
}

/*! @} */

} //namespace

#endif

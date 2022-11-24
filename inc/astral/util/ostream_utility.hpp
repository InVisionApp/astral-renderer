/*!
 * \file ostream_utility.hpp
 * \brief file ostream_utility.hpp
 *
 * Adapted from: ostream_utility.hpp of WRATH:
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



#ifndef ASTRAL_DEMO_OSTREAM_UTILITY_HPP
#define ASTRAL_DEMO_OSTREAM_UTILITY_HPP


#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <utility>

#include <astral/util/util.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/matrix.hpp>
#include <astral/util/rect.hpp>
#include <astral/util/bounding_box.hpp>
#include <astral/util/polynomial.hpp>
#include <astral/util/scale_translate.hpp>
#include <astral/util/transformation.hpp>
#include <astral/renderer/colorstop.hpp>
#include <astral/renderer/render_scale_factor.hpp>

namespace astral {

class ContourCurve;
class ContourData;

/*!\addtogroup Utility
 * @{
 */


/*!
 * \brief
 * Simple class with overloaded operator<<
 * to print a number of indenting characters
 * to an std::ostream.
 */
class format_tabbing
{
public:
  /*!
   * construct a format_tabbing
   * \param ct number of times to print the indent character
   * \param c indent character, default is a tab (i.e. \\t)
  */
  explicit
  format_tabbing(unsigned int ct, char c='\t'):m_count(ct), m_char(c) {}

  /*!
   * Number of times to print \ref m_char.
   */
  unsigned int m_count;

  /*!
   * Indent character to print.
   */
  char m_char;
};

/*!
 * \brief
 * Simple type to print an STL range of elements
 * via overloading operator<<.
 */
template<typename iterator>
class print_range_type
{
public:
  /*!
   * Ctor.
   * \param begin iterator to 1st element to print
   * \param end iterator to one past the last element to print
   * \param spacingCharacter string to print between consecutive elements
  */
  print_range_type(iterator begin, iterator end,
                   const std::string &spacingCharacter=", "):
    m_begin(begin), m_end(end), m_spacingCharacter(spacingCharacter) {}

  /*!
   * Iterator to first element to print
   */
  iterator m_begin;

  /*!
   * Iterator to one element past the last element to print
   */
  iterator m_end;

  /*!
   * string to print between consecutive elements
   */
  std::string m_spacingCharacter;
};

/*!
 * Returns a print_range_type to print a
 * range of elements to an std::ostream.
 * \tparam iterator type
 * \param begin iterator to 1st element to print
 * \param end iterator to one past the last element to print
 * \param str string to print between consecutive elements
*/
template<typename iterator>
print_range_type<iterator>
print_range(iterator begin, iterator end, const std::string &str=", ")
{
  return print_range_type<iterator>(begin, end, str);
}

/*!
 * \brief
 * Some description
 */
template<typename iterator>
class print_range_as_matrix_type
{
public:
  /*!
   * Ctor.
   * \param begin iterator to 1st element to print
   * \param end iterator to one past the last element to print
   * \param leadingDimension how many elements to print between rows
   * \param beginOfLine string to print at the start of each line
   * \param endOfLine string to print at the end of each line
   * \param spacingCharacter string to print between consecutive elements
   */
  print_range_as_matrix_type(iterator begin, iterator end,
                             unsigned int leadingDimension,
                             const std::string &beginOfLine="",
                             const std::string &endOfLine="\n",
                             const std::string &spacingCharacter=", "):
    m_begin(begin),
    m_end(end),
    m_spacingCharacter(spacingCharacter),
    m_leadingDimension(leadingDimension),
    m_endOfLine(endOfLine),
    m_beginOfLine(beginOfLine)
  {}

  /*!
   * Iterator to first element to print
   */
  iterator m_begin;

  /*!
   * Iterator to one element past the last element to print
   */
  iterator m_end;

  /*!
   * string to print between consecutive elements
   */
  std::string m_spacingCharacter;

  /*!
   * Leading dimension of the matrix
   */
  unsigned int m_leadingDimension;

  /*!
   * String to print at the end of each line
   */
  std::string m_endOfLine;

  /*!
   * String to print at the start of each line
   */
  std::string m_beginOfLine;
};

/*!
 * Create an astral::print_range_as_matrix_type value
 * \param begin iterator to 1st element to print
 * \param end iterator to one past the last element to print
 * \param leadingDimension how many elements to print between rows
 * \param beginOfLine string to print at the start of each line
 * \param endOfLine string to print at the end of each line
 * \param str string to print between consecutive elements
 */
template<typename iterator>
print_range_as_matrix_type<iterator>
print_range_as_matrix(iterator begin, iterator end,
                      unsigned int leadingDimension,
                      const std::string &beginOfLine="",
                      const std::string &endOfLine="\n",
                      const std::string &str=", ")
{
  return print_range_as_matrix_type<iterator>(begin, end, leadingDimension,
                                              beginOfLine, endOfLine, str);
}
/*! @} */

}

/*!\addtogroup Utility
 * @{
 */

namespace std
{

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param obj value to stream
 */
ostream&
operator<<(ostream &str, const astral::ColorStop<astral::vec4> &obj);

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param obj value to stream
 */
ostream&
operator<<(ostream &str, const astral::ColorStop<astral::FixedPointColorLinear> &obj);

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param obj value to stream
 */
ostream&
operator<<(ostream &str, const astral::ColorStop<astral::FixedPointColor_sRGB> &obj);

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param obj value to stream
 */
ostream&
operator<<(ostream &str, const astral::ContourCurve &obj);

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param obj value to stream
 */
ostream&
operator<<(ostream &str, const astral::ContourData &obj);

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param obj values to stream
 */
template<typename T>
ostream&
operator<<(ostream &str, astral::c_array<T> obj);

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param obj values to stream
 */
template<typename T>
ostream&
operator<<(ostream &str, astral::c_array<const T> obj);

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param obj values to stream
 */
template<typename T, size_t N>
ostream&
operator<<(ostream &str, const astral::vecN<T, N> &obj);

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param obj values to stream
 */
template<typename T>
ostream&
operator<<(ostream &str, const astral::range_type<T> &obj);

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param obj values to stream
 */
template<typename iterator>
ostream&
operator<<(ostream &str,
           const astral::print_range_type<iterator> &obj);

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param obj values to stream
 */
template<typename iterator>
ostream&
operator<<(ostream &str,
           const astral::print_range_as_matrix_type<iterator> &obj);

/*!
 * operator<< overload for streaming tabbing
 * \param str std::ostream to stream to
 * \param tabber values to stream
 */
ostream&
operator<<(ostream &str, const astral::format_tabbing &tabber);

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param obj values to stream
 */
template<typename T, typename S>
ostream&
operator<<(ostream &str, const pair<T, S> &obj);

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param obj values to stream
 */
template<typename T, typename _Compare, typename _Alloc>
ostream&
operator<<(ostream &str, const set<T,_Compare,_Alloc> &obj);

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param obj values to stream
 */
template<typename T, typename _Alloc>
ostream&
operator<<(ostream &str, const list<T,_Alloc> &obj);

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param obj values to stream
 */
template<typename T, typename _Alloc>
ostream&
operator<<(ostream &str, const vector<T,_Alloc> &obj);

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param matrix values to stream
 */
template<size_t N, size_t M, typename T>
ostream&
operator<<(ostream &str, const astral::matrix<N, M, T> &matrix);

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param obj values to stream
 */
template<typename T>
ostream&
operator<<(ostream &str, const astral::ScaleTranslateT<T> &obj);

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param obj values to stream
 */
template<typename T>
ostream&
operator<<(ostream &str, const astral::RectT<T> &obj);

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param obj values to stream
 */
template<typename T>
ostream&
operator<<(ostream &str, const astral::BoundingBox<T> &obj);

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param obj value to stream
 */
template<typename T, size_t D>
ostream&
operator<<(ostream &str, const astral::Polynomial<T, D> &obj);

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param obj value to stream
 */
ostream&
operator<<(ostream &str, const astral::RenderScaleFactor &obj);

/*!
 * operator<< overload for streaming
 * \param str std::ostream to stream to
 * \param obj value to stream
 */
ostream&
operator<<(ostream &str, const astral::Transformation &obj);

/////////////////////////////////////////
// Implementation

template<typename T>
ostream&
operator<<(ostream &str, const astral::range_type<T> &obj)
{
  str << "[" << obj.m_begin << "," << obj.m_end << ")";
  return str;
}

template<typename iterator>
ostream&
operator<<(ostream &str,
           const astral::print_range_type<iterator> &obj)
{
  iterator iter;

  for(iter = obj.m_begin; iter != obj.m_end; ++iter)
    {
      if (iter != obj.m_begin)
        {
          str << obj.m_spacingCharacter;
        }
      str << (*iter);
    }
  return str;
}

template<typename iterator>
ostream&
operator<<(ostream &str,
           const astral::print_range_as_matrix_type<iterator> &obj)
{
  iterator iter;
  int idx;

  for(iter=obj.m_begin, idx=0; iter!=obj.m_end; ++iter)
    {
      if (idx == 0)
        {
          str << obj.m_beginOfLine;
        }
      else
        {
          str << obj.m_spacingCharacter;
        }

      str << (*iter);

      ++idx;
      if (idx == obj.m_leadingDimension)
        {
          str << obj.m_endOfLine;
          idx = 0;
        }
    }
  return str;
}

inline
ostream&
operator<<(ostream &str, const astral::format_tabbing &tabber)
{
  for(unsigned int i = 0; i < tabber.m_count; ++i)
    {
      str << tabber.m_char;
    }
  return str;
}

template<typename T, size_t N>
ostream&
operator<<(ostream &str, const astral::vecN<T, N> &obj)
{
  str <<  "( " << astral::print_range(obj.begin(), obj.end(), ", ") << " )";
  return str;
}

template<typename T>
ostream&
operator<<(ostream &str, astral::c_array<const T> obj)
{
  str <<  "( " << astral::print_range(obj.begin(), obj.end(), ", ") << " )";
  return str;
}

template<typename T>
ostream&
operator<<(ostream &str, astral::c_array<T> obj)
{
  str <<  "( " << astral::print_range(obj.begin(), obj.end(), ", ") << " )";
  return str;
}

template<typename T, typename S>
ostream&
operator<<(ostream &str, const pair<T, S> &obj)
{
  str << "(" << obj.first << "," << obj.second << ")";
  return str;
}

template<typename T, typename _Compare, typename _Alloc>
ostream&
operator<<(ostream &str, const set<T,_Compare,_Alloc> &obj)
{
  str <<  "{ " << astral::print_range(obj.begin(), obj.end(), ", ") << " }";
  return str;
}

template<typename T, typename _Alloc>
ostream&
operator<<(ostream &str, const list<T,_Alloc> &obj)
{
  str << "( " << astral::print_range(obj.begin(), obj.end(), ", ") << " )";
  return str;
}

template<typename T, typename _Alloc>
ostream&
operator<<(ostream &str, const vector<T,_Alloc> &obj)
{
  str << "( " << astral::print_range(obj.begin(), obj.end(), ", ") << " )";
  return str;
}

template<size_t N, size_t M, typename T>
ostream&
operator<<(ostream &str, const astral::matrix<N, M, T> &matrix)
{
  for(unsigned int row = 0; row < N; ++row)
    {
      str << "|";
      for(unsigned int col = 0; col < M; ++col)
        {
          str << setw(10) << matrix.row_col(row, col) << " ";
        }
      str << "|\n";
    }
  return str;
}

template<typename T>
ostream&
operator<<(ostream &str, const astral::ScaleTranslateT<T> &obj)
{
  str << "(scale = " << obj.m_scale << ", "
      << "translate = " << obj.m_translate << ")";
  return str;
}

template<typename T>
ostream&
operator<<(ostream &str, const astral::RectT<T> &obj)
{
  str << "[" << obj.m_min_point.x() << ", "
      << obj.m_max_point.x() << "]x["
      << obj.m_min_point.y() << ", "
      << obj.m_max_point.y() << "]";
  return str;
}

template<typename T>
ostream&
operator<<(ostream &str, const astral::BoundingBox<T> &obj)
{
  if (obj.empty())
    {
      str << "{Empty}";
    }
  else
    {
      str << "[" << obj.min_point().x() << ", "
          << obj.max_point().x() << "]x["
          << obj.min_point().y() << ", "
          << obj.max_point().y() << "]";
    }
  return str;
}

template<typename T, size_t D>
ostream&
operator<<(ostream &str, const astral::Polynomial<T, D> &obj)
{
  for (size_t i = D; i > 0; --i)
    {
      str << obj.coeff(i) << "*t^" << i << " + ";
    }
  str << obj.coeff(0);
  return str;
}

inline
ostream&
operator<<(ostream &str, const astral::RenderScaleFactor &obj)
{
  /* NOTE: the implementation must come AFTER the definition
   *       for the overload with vecN because m_scale_factor
   *       is a vec2 AND RenderScaleFactor also has an implicit
   *       ctor from a vec2.
   */
  str << "{factor = " << obj.m_scale_factor << ", relative = "
      << boolalpha << obj.m_relative << "}";
  return str;
}

inline
ostream&
operator<<(ostream &str, const astral::Transformation &obj)
{
  str << "(matrix = " << obj.m_matrix.raw_data() << ", "
      << "translate = " << obj.m_translate << ")";
  return str;
}

}

/*! @} */


#endif

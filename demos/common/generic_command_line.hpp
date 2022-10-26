/*
  Copyright (c) 2009, Kevin Rogovin All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
    * notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
    * copyright notice, this list of conditions and the following
    * disclaimer in the documentation and/or other materials provided
    * with the distribution.  Neither the name of the Kevin Rogovin or
    * kRogue Technologies  nor the names of its contributors may
    * be used to endorse or promote products derived from this
    * software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


/** \file generic_command_line.hpp */
#ifndef ASTRAL_DEMO_GENERIC_COMMAND_LINE_HPP
#define ASTRAL_DEMO_GENERIC_COMMAND_LINE_HPP

#include <iostream>
#include <vector>
#include <algorithm>
#include <map>
#include <list>
#include <set>
#include <string>
#include <sstream>
#include <ciso646>
#include <type_traits>
#include <astral/util/util.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/ostream_utility.hpp>

#include "simple_time.hpp"

class command_line_register;
class command_line_argument;


/*!
 * A command_line_register walks the a command
 * line argument list, calling its command_line_argument
 * children's check_arg() method to get the values
 * from a command line argument list.
 */
class command_line_register:astral::noncopyable
{
public:
  command_line_register(void)
  {}

  ~command_line_register();

  void
  parse_command_line(int argc, char **argv);

  void
  parse_command_line(astral::c_array<const std::string> strings);

  void
  print_help(std::ostream&) const;

  void
  print_detailed_help(std::ostream&) const;

  static
  bool
  is_help_request(const std::string &v)
  {
    return v == std::string("-help")
      || v == std::string("--help")
      || v == std::string("-h");
  }

private:
  friend class command_line_argument;
  std::vector<command_line_argument*> m_children;
};

/*!
 * A command_line_argument reads from an argument
 * list to set a value.
 */
class command_line_argument:astral::noncopyable
{
public:
  explicit
  command_line_argument(command_line_register &parent):
    m_parent(&parent)
  {
    m_location = parent.m_children.size();
    parent.m_children.push_back(this);
  }

  command_line_argument(void):
    m_location(-1),
    m_parent(nullptr)
  {}

  virtual
  ~command_line_argument();

  void
  attach(command_line_register &p)
  {
    ASTRALassert(!m_parent);

    m_parent = &p;
    m_location = m_parent->m_children.size();
    m_parent->m_children.push_back(this);
  }

  command_line_register*
  parent(void)
  {
    return m_parent;
  }

  /*!
   * To be implemented by a dervied class.
   * \param args list of arguments remaining to process
   * returns the number of arguments consumed, 0 is allowed.
   */
  virtual
  int
  check_arg(astral::c_array<const std::string> args)=0;

  virtual
  void
  print_command_line_description(std::ostream&) const=0;

  virtual
  void
  print_detailed_description(std::ostream&) const=0;

  static
  std::string
  format_description_string(const std::string &name, const std::string &desc);

  static
  std::string
  produce_formatted_detailed_description(const std::string &cmd,
                                         const std::string &desc);

  static
  std::string
  tabs_to_spaces(const std::string &pin);

private:
  friend class command_line_register;
  int m_location;
  command_line_register *m_parent;
};

/*!
 * not a command line option, but prints a separator for detailed help
 */
class command_separator:public command_line_argument
{
public:
  command_separator(const std::string &label,
                    command_line_register &parent):
    command_line_argument(parent),
    m_label(label)
  {}

  virtual
  int
  check_arg(astral::c_array<const std::string>) override final
  {
    return 0;
  }

  virtual
  void
  print_command_line_description(std::ostream&) const override final
  {}

  virtual
  void
  print_detailed_description(std::ostream &ostr) const override final
  {
    ostr << "\n\n---------- " << m_label << " ------------------\n";
  }

private:
  std::string m_label;
};

/*
  not a command line option, but prints an about
 */
class command_about:public command_line_argument
{
public:
  command_about(const std::string &label,
                command_line_register &parent):
    command_line_argument(parent),
    m_label(tabs_to_spaces(format_description_string("", label)))
  {}

  virtual
  int
  check_arg(astral::c_array<const std::string>) override final
  {
    return 0;
  }

  virtual
  void
  print_command_line_description(std::ostream&) const override final
  {}

  virtual
  void
  print_detailed_description(std::ostream &ostr) const override final
  {
    ostr << "\n\n " << m_label << "\n";
  }

private:
  std::string m_label;
};

template<typename T>
void
readvalue_from_string(T &value,
                      const std::string &value_string)
{
  std::istringstream istr(value_string);
  istr >> value;
}

template<>
inline
void
readvalue_from_string(std::string &value,
                      const std::string &value_string)
{
  value = value_string;
}

template<typename T, size_t N>
void
readvalue_from_string(astral::vecN<T, N> &value,
                      const std::string &value_string)
{
  std::string R(value_string);

  std::replace(R.begin(), R.end(), ':', ' ');
  std::istringstream istr(R);

  for (size_t i = 0; i < N; ++i)
    {
      istr >> value[i];
    }
}

template<>
inline
void
readvalue_from_string(bool &value, const std::string &value_string)
{
  if (value_string == std::string("on")
      || value_string == std::string("true"))
    {
      value = true;
    }
  else if (value_string==std::string("off")
           || value_string==std::string("false"))
    {
      value = false;
    }
}

template<typename T>
void
writevalue_to_stream(const T&value, std::ostream &ostr)
{
  ostr << value;
}

template<>
inline
void
writevalue_to_stream(const bool &value, std::ostream &ostr)
{
  if (value)
    {
      ostr << "on/true";
    }
  else
    {
      ostr << "off/false";
    }
}

/* Crazy overloads to make simple_time work with command_line_argument_value;
 * if the value is set by command line then it is also paused.
 */
template<>
inline
void
readvalue_from_string(simple_time &value,
                      const std::string &value_string)
{
  std::istringstream istr(value_string);
  float ms(0.0f), us;

  istr >> ms;
  us = ms * 1000.0f;
  value.set_time(static_cast<int64_t>(us));
}

template<>
inline
void
writevalue_to_stream(const simple_time &value, std::ostream &ostr)
{
  if (value.paused())
    {
      ostr << static_cast<float>(value.elapsed_us()) / 1000.0f
           << " ms (paused)";
    }
  else
    {
      ostr << "animated";
    }
}

class LabelDescription
{
public:
  void
  set_description(const std::string &desc)
  {
    m_description = desc;
  }

  void
  add_value(const std::string &v)
  {
    m_values_as_set.insert(v);
    m_values_as_list.push_back(v);
  }

  const std::string&
  description(void) const
  {
    return m_description;
  }

  const std::list<std::string>&
  values(void) const
  {
    return m_values_as_list;
  }

  bool
  has_value(const std::string &v)
  {
    return m_values_as_set.find(v) != m_values_as_set.end();
  }

private:
  std::string m_description;
  std::set<std::string> m_values_as_set;
  std::list<std::string> m_values_as_list;
};

template<typename T>
class enumerated_string_type
{
public:
  std::map<std::string, typename std::remove_reference<T>::type> m_value_strings;
  std::map<typename std::remove_reference<T>::type, LabelDescription> m_value_Ts;

  enumerated_string_type(void)
  {}

  template<typename F>
  enumerated_string_type(F f, unsigned int max_value)
  {
    for (unsigned int i = 0; i < max_value; ++i)
      {
        T v;

        v = static_cast<T>(i);
        add_entry(f(v), v, "");
      }
  }

  enumerated_string_type(astral::c_string (*f)(typename std::remove_reference<T>::type), unsigned int max_value)
  {
    for (unsigned int i = 0; i < max_value; ++i)
      {
        T v;

        v = static_cast<T>(i);
        add_entry(f(v), v, "");
      }
  }

  enumerated_string_type&
  add_entry(const std::string &label, typename std::remove_reference<T>::type v, const std::string &description)
  {
    m_value_strings[label] = v;
    m_value_Ts[v].set_description(description);
    m_value_Ts[v].add_value(label);
    return *this;
  }

  enumerated_string_type&
  add_entry_alias(const std::string &label, typename std::remove_reference<T>::type v)
  {
    m_value_strings[label] = v;
    m_value_Ts[v].add_value(label);
    return *this;
  }
};

template<typename T>
class enumerated_type
{
public:
  T m_value;
  enumerated_string_type<typename std::remove_reference<T>::type> m_label_set;

  enumerated_type(T v, const enumerated_string_type<typename std::remove_reference<T>::type> &L):
    m_value(v),
    m_label_set(L)
  {}
};

template<typename T>
std::string
read_format_description(astral::type_tag<T>)
{
  return std::string();
}

template<typename T>
class command_line_argument_value:public command_line_argument
{
public:
  command_line_argument_value(T v, const std::string &nm,
                              const std::string &desc,
                              command_line_register &p,
                              bool print_at_set=true):
    command_line_argument(p),
    m_name(nm),
    m_set_by_command_line(false),
    m_print_at_set(print_at_set),
    m_value(v)
  {
    std::ostringstream ostr;
    ostr << "\n\t"
         << m_name << " (default value=";

    writevalue_to_stream(m_value, ostr);
    ostr << ") " << format_description_string(m_name, desc + read_format_description(astral::type_tag<T>()));
    m_description = tabs_to_spaces(ostr.str());
  }

  const std::string&
  label(void) const
  {
    return m_name;
  }

  bool
  set_by_command_line(void) const
  {
    return m_set_by_command_line;
  }

  virtual
  int
  check_arg(astral::c_array<const std::string> args) override final
  {
    const std::string &str(args.front());
    std::string::const_iterator iter;

    iter = std::find(str.begin(), str.end(), '=');
    if (iter == str.end())
      {
        iter = std::find(str.begin(), str.end(), ':');
      }

    if (iter != str.end() && m_name == std::string(str.begin(), iter))
      {
        std::string val(iter + 1, str.end());

        readvalue_from_string(m_value, val);
        if (m_print_at_set)
          {
            std::cout << "\n\t" << m_name
                      << " set to ";
            writevalue_to_stream(m_value, std::cout);
          }

        m_set_by_command_line = true;
        return 1;
      }
    else if (args.size() >= 2u && str == m_name)
      {
        const std::string &val(args[1]);

        readvalue_from_string(m_value, val);
        if (m_print_at_set)
          {
            std::cout << "\n\t" << m_name
                      << " set to ";
            writevalue_to_stream(m_value, std::cout);
          }
        m_set_by_command_line = true;
        return 2;
      }
    return 0;
  }

  virtual
  void
  print_command_line_description(std::ostream &ostr) const override final
  {
    ostr << "[" << m_name << "=value] "
         << "[" << m_name << ":value] "
         << "[" << m_name << " value]";
  }

  virtual
  void
  print_detailed_description(std::ostream &ostr) const override final
  {
    ostr << m_description;
  }

  const typename std::remove_reference<T>::type&
  value(void) const
  {
    return m_value;
  }

  typename std::remove_reference<T>::type&
  value(void)
  {
    return m_value;
  }

  const std::string&
  name(void) const
  {
    return m_name;
  }

private:
  std::string m_name;
  std::string m_description;
  bool m_set_by_command_line, m_print_at_set;
  T m_value;
};


template<typename T>
class enumerated_command_line_argument_value:public command_line_argument
{
public:
  enumerated_command_line_argument_value(T v, const enumerated_string_type<typename std::remove_reference<T>::type> &L,
                                         const std::string &nm, const std::string &desc,
                                         command_line_register &p,
                                         bool print_at_set=true):
    command_line_argument(p),
    m_name(nm),
    m_set_by_command_line(false),
    m_print_at_set(print_at_set),
    m_value(v, L)
  {
    std::ostringstream ostr;
    typename std::map<typename std::remove_reference<T>::type, LabelDescription>::const_iterator iter, end;

    ostr << "\n\t"
         << m_name << " (default value=";

    iter = m_value.m_label_set.m_value_Ts.find(v);
    if (iter != m_value.m_label_set.m_value_Ts.end())
      {
        ostr << iter->second.values().front();
      }
    else
      {
        ostr << v;
      }

    ostr << ")";
    std::ostringstream ostr_desc;
    ostr_desc << desc << " Possible values:\n\n";
    for(iter = m_value.m_label_set.m_value_Ts.begin(),
          end = m_value.m_label_set.m_value_Ts.end();
        iter != end; ++iter)
      {
        const std::list<std::string> &contents(iter->second.values());
        for (std::list<std::string>::const_iterator siter = contents.begin();
             siter != contents.end(); ++siter)
          {
            if (siter != contents.begin())
              {
                ostr_desc << "/";
              }
            ostr_desc << *siter;
          }
        if (!iter->second.description().empty())
          {
            ostr_desc << " : " << iter->second.description() << "\n";
          }
        ostr_desc << "\n";
      }
    ostr << format_description_string(m_name, ostr_desc.str());
    m_description = tabs_to_spaces(ostr.str());
  }

  bool
  set_by_command_line(void)
  {
    return m_set_by_command_line;
  }

  virtual
  int
  check_arg(astral::c_array<const std::string> args) override final
  {
    const std::string &str(args.front());
    std::string::const_iterator iter;

    iter = std::find(str.begin(), str.end(), '=');
    if (iter == str.end())
      {
        iter = std::find(str.begin(), str.end(), ':');
      }

    if (iter != str.end() && m_name == std::string(str.begin(), iter))
      {
        std::string val(iter + 1, str.end());
        typename std::map<std::string, typename std::remove_reference<T>::type>::const_iterator iter;

        iter = m_value.m_label_set.m_value_strings.find(val);
        if (iter != m_value.m_label_set.m_value_strings.end())
          {
            m_value.m_value = iter->second;
            if (m_print_at_set)
              {
                std::cout << "\n\t" << m_name
                          << " set to " << val;
                m_set_by_command_line = true;
              }
          }
        return 1;
      }
    else if (args.size() >= 2u && str == m_name)
      {
        const std::string &val(args[1]);
        typename std::map<std::string, typename std::remove_reference<T>::type>::const_iterator iter;

        iter = m_value.m_label_set.m_value_strings.find(val);
        if (iter != m_value.m_label_set.m_value_strings.end())
          {
            m_value.m_value = iter->second;
            if (m_print_at_set)
              {
                std::cout << "\n\t" << m_name
                          << " set to " << val;
                m_set_by_command_line = true;
              }
          }
        return 2;
      }
    return 0;
  }

  virtual
  void
  print_command_line_description(std::ostream &ostr) const override final
  {
    ostr << "[" << m_name << "=value] "
         << "[" << m_name << ":value] "
         << "[" << m_name << " value]";
  }

  virtual
  void
  print_detailed_description(std::ostream &ostr) const override final
  {
    ostr << m_description;
  }

  typename std::remove_reference<T>::type
  value(void) const
  {
    return m_value.m_value;
  }

  typename std::remove_reference<T>::type&
  value(void)
  {
    return m_value.m_value;
  }

  const std::string&
  name(void) const
  {
    return m_name;
  }

private:
  std::string m_name;
  std::string m_description;
  bool m_set_by_command_line, m_print_at_set;
  enumerated_type<T> m_value;
};

#endif

/*!
 * \file command_line_list.hpp
 * \brief command_line_list.hpp
 *
 * Adapted from: command_line_list.hpp of FastUIDraw (https://github.com/intel/fastuidraw):
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
#ifndef ASTRAL_DEMO_COMMAND_LINE_LIST_HPP
#define ASTRAL_DEMO_COMMAND_LINE_LIST_HPP

#include <list>
#include <dirent.h>
#include <string>
#include <fstream>
#include <astral/util/util.hpp>
#include <astral/util/c_array.hpp>
#include <astral/util/ostream_utility.hpp>
#include <astral/renderer/image.hpp>

#include "generic_command_line.hpp"
#include "ImageLoader.hpp"
#include "read_colorstops.hpp"
#include "read_dash_pattern.hpp"

template<typename T>
class command_line_list:
  public command_line_argument,
  public std::list<T>
{
public:
  command_line_list(const std::string &nm,
                    const std::string &desc,
                    command_line_register &p):
    command_line_argument(p),
    m_name(nm)
  {
    std::ostringstream ostr;
    ostr << "\n\t" << m_name << " value"
         << format_description_string(m_name, desc);
    m_description = tabs_to_spaces(ostr.str());
  }

  virtual
  int
  check_arg(astral::c_array<const std::string> args) override final
  {
    if (args.size() >= 2u && args.front() == m_name)
      {
        T v;
        readvalue_from_string(v, args[1]);
        this->push_back(v);
        std::cout << "\n\t" << m_name << " added: ";
        writevalue_to_stream(v, std::cout);
        return 2;
      }
    return 0;
  }

  virtual
  void
  print_command_line_description(std::ostream &ostr) const override final
  {
    ostr << "[" << m_name << " value] ";
  }

  virtual
  void
  print_detailed_description(std::ostream &ostr) const override final
  {
    ostr << m_description;
  }

  void
  clear(void)
  {
    std::list<T>::clear();
  }

private:
  std::string m_name, m_description;
};

template<typename F>
class command_line_list_loader:command_line_list<std::string>
{
public:
  class element
  {
  public:
    std::string m_filename;
    F m_loaded_value;
  };

  command_line_list_loader(const std::string &nm,
                           const std::string &desc,
                           command_line_register &p):
    command_line_list<std::string>(nm, desc, p),
    m_loaded(false)
  {}

  astral::c_array<const element>
  elements(void)
  {
    if (!m_loaded)
      {
        m_loaded = true;
        process_list();
      }

    return astral::make_c_array(m_elements);
  }

  void
  clear(void)
  {
    command_line_list<std::string>::clear();
    m_elements.clear();
  }

private:
  virtual
  enum astral::return_code
  load_element(const std::string &filename, F *out_value) = 0;

  void
  add_entry(const std::string &filename)
  {
    DIR *dir;

    dir = opendir(filename.c_str());
    if (!dir)
      {
        enum astral::return_code R;
        element E;

        E.m_filename = filename;
        R = load_element(E.m_filename, &E.m_loaded_value);
        if (R == astral::routine_success)
          {
            m_elements.push_back(E);
          }
        return;
      }

    for (struct dirent *entry = readdir(dir); entry; entry = readdir(dir))
      {
        std::string file;
        file = entry->d_name;
        if (file != ".." && file != ".")
          {
            add_entry(filename + "/" + file);
          }
      }
    closedir(dir);
  }

  void
  process_list(void)
  {
    const std::list<std::string> &files(*this);
    for (const std::string &nm: files)
      {
        add_entry(nm);
      }
  }

  bool m_loaded;
  std::vector<element> m_elements;
};

class command_line_list_images:
  public command_line_list_loader<astral::reference_counted_ptr<ImageLoader>>
{
public:
  command_line_list_images(std::ostream *emit_txt_on_load,
                           const std::string &nm,
                           const std::string &desc,
                           command_line_register &p):
    command_line_list_loader<astral::reference_counted_ptr<ImageLoader>>(nm, desc, p),
    m_emit_txt_on_load(emit_txt_on_load)
  {}

  command_line_list_images(const std::string &nm,
                           const std::string &desc,
                           command_line_register &p):
    command_line_list_loader<astral::reference_counted_ptr<ImageLoader>>(nm, desc, p),
    m_emit_txt_on_load(nullptr)
  {}

private:
  virtual
  enum astral::return_code
  load_element(const std::string &filename,
               astral::reference_counted_ptr<ImageLoader> *out_value) override final
  {
    astral::reference_counted_ptr<ImageLoader> &p(*out_value);

    if (m_emit_txt_on_load)
      {
        (*m_emit_txt_on_load) << "Loading image \"" << filename << "\"..." << std::flush;
      }

    p = ImageLoader::create(filename);

    if (m_emit_txt_on_load)
      {
        if (p->non_empty())
          {
            (*m_emit_txt_on_load) << " completed, image size = " << p->dimensions() << "\n";
          }
        else
          {
            (*m_emit_txt_on_load) << " failed\n";
          }
      }

    return (p->non_empty()) ?
      astral::routine_success :
      astral::routine_fail;
  }

  std::ostream *m_emit_txt_on_load;
};

template<enum astral::colorspace_t colorspace>
class command_line_list_colorstops:
  public command_line_list_loader<std::vector<astral::ColorStop<astral::FixedPointColor<colorspace>>>>
{
public:
  command_line_list_colorstops(const std::string &nm,
                               const std::string &desc,
                               command_line_register &p):
    command_line_list_loader<std::vector<astral::ColorStop<astral::FixedPointColor<colorspace>>>>(nm, desc, p)
  {}

private:
  virtual
  enum astral::return_code
  load_element(const std::string &filename,
               std::vector<astral::ColorStop<astral::FixedPointColor_sRGB>> *out_value) override final
  {
    std::ifstream file(filename.c_str());
    read_colorstops(*out_value, file);

    return (out_value->empty()) ?
      astral::routine_fail:
      astral::routine_success;
  }
};

class command_line_list_dash_pattern:
  public command_line_list_loader<std::vector<astral::StrokeShader::DashPatternElement>>
{
public:
  command_line_list_dash_pattern(const std::string &nm,
                               const std::string &desc,
                               command_line_register &p):
    command_line_list_loader<std::vector<astral::StrokeShader::DashPatternElement>>(nm, desc, p)
  {}

private:
  virtual
  enum astral::return_code
  load_element(const std::string &filename,
               std::vector<astral::StrokeShader::DashPatternElement> *out_value) override final
  {
    std::ifstream file(filename.c_str());
    read_dash_pattern(*out_value, file);

    return (out_value->empty()) ?
      astral::routine_fail:
      astral::routine_success;
  }
};

#endif

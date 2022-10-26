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


#include <ctype.h>
#include "generic_command_line.hpp"

/////////////////////////////////////
// command_line_register methods
command_line_register::
~command_line_register()
{
  for(command_line_argument *p : m_children)
    {
      if (p)
        {
          p->m_parent = nullptr;
          p->m_location = -1;
        }
    }
}


void
command_line_register::
parse_command_line(int argc, char **argv)
{
  if (argc == 0)
    {
      return;
    }

  /* assume first argument is the name of the executable */
  --argc;
  ++argv;

  std::vector<std::string> arg_strings(argc);
  astral::c_array<const std::string> args(astral::make_c_array(arg_strings));

  for(int i = 0; i < argc; ++i)
    {
      arg_strings[i] = argv[i];
    }

  parse_command_line(args);
}

void
command_line_register::
parse_command_line(astral::c_array<const std::string> args)
{
  std::vector<std::vector<std::string>> unknown_options(1);

  while (!args.empty())
    {
      bool arg_taken(false);

      /*
       * we do not use the iterator interface because an argument
       * value may add new ones triggering a resize of m_children.
       */
      for(unsigned int i = 0; !arg_taken && i < m_children.size(); ++i)
        {
           command_line_argument *p(m_children[i]);

           if (p)
             {
               int incr;

               incr = p->check_arg(args);
               if (incr > 0)
                 {
                   args = args.sub_array(incr);
                   arg_taken = true;
                 }
             }
        }

      if (!arg_taken)
        {
          unknown_options.back().push_back(args.front());
          args = args.sub_array(1);
        }
      else if (!unknown_options.back().empty())
        {
          unknown_options.push_back(std::vector<std::string>());
        }
    }

  for (const std::vector<std::string> &seq : unknown_options)
    {
      if (!seq.empty())
        {
          std::cout << "\n\tUnknown command sequence:";
          for (const std::string &v : seq)
            {
              std::cout << " " << v;
            }
        }
    }
}

void
command_line_register::
print_help(std::ostream &ostr) const
{
  for(command_line_argument *p : m_children)
    {
      if (p)
         {
           ostr << " ";
           p->print_command_line_description(ostr);
         }
    }
  ostr << "\n";
}


void
command_line_register::
print_detailed_help(std::ostream &ostr) const
{
  for(command_line_argument *p : m_children)
    {
      if (p)
         {
           p->print_detailed_description(ostr);
         }
    }
  ostr << "\n";
}

/////////////////////////////
//command_line_argument methods
command_line_argument::
~command_line_argument()
{
  if (m_parent && m_location >= 0)
    {
      m_parent->m_children[m_location] = nullptr;
    }
}

#define TAB_LENGTH 4

std::string
command_line_argument::
tabs_to_spaces(const std::string &v)
{
  std::vector<char> vc;
  for(char ch : v)
    {
      if (ch != '\t')
        {
          vc.push_back(ch);
        }
      else
        {
          for(int i = 0; i < TAB_LENGTH; ++i)
            {
              vc.push_back(' ');
            }
        }
    }
  return std::string(vc.begin(), vc.end());
}

std::string
command_line_argument::
produce_formatted_detailed_description(const std::string &cmd,
                                       const std::string &desc)
{
  std::ostringstream ostr;
  ostr << "\n\t" << cmd << " "
       << format_description_string(cmd, desc);
  return tabs_to_spaces(ostr.str());
}

std::string
command_line_argument::
format_description_string(const std::string &name, const std::string &desc)
{
  std::string empty(name);
  std::ostringstream ostr;
  int l, nl;
  std::string::const_iterator iter, end;
  const int line_length(70);

  nl=name.length();
  std::fill(empty.begin(), empty.end(), ' ');

  for(iter = desc.begin(), end = desc.end(); iter!=end;)
    {
      if (*iter != '\n')
        {
          ostr << "\n\t" << empty;
        }

      for(; iter != end && *iter == '\n'; ++iter)
        {
          ostr << "\n\t" << empty;
        }

      for(;iter != end && *iter == ' '; ++iter)
        {}

      for(l = nl + TAB_LENGTH; l < line_length && iter != end && *iter != '\n'; ++iter)
        {
          ostr << *iter;
          if (*iter == '\t')
            {
              l += TAB_LENGTH;
            }
          else
            {
              ++l;
            }
        }

      for(;iter != end && !isspace(*iter); ++iter)
        {
          ostr << *iter;
        }
    }
  ostr << "\n\t" << empty;
  return tabs_to_spaces(ostr.str());
}

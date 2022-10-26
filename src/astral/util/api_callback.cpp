/*!
 * \file api_callback.cpp
 * \brief file api_callback.cpp
 *
 * Copyright 2018 by Intel.
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

#include <list>
#include <mutex>
#include <functional>
#include <iostream>
#include <astral/util/api_callback.hpp>

namespace
{
  class APICallbackSetPrivate;
  class CallBackPrivate;
  typedef std::list<astral::APICallbackSet::CallBack*> CallBackList;

  class APICallbackSetPrivate:public astral::noncopyable
  {
  public:
    explicit
    APICallbackSetPrivate(astral::c_string label);

    CallBackList::iterator
    insert(astral::APICallbackSet::CallBack *q);

    void
    erase(CallBackList::iterator iter);

    template<typename F>
    void
    call_callbacks(const F &fptr);

    void
    get_proc_function(void* (*get_proc)(astral::c_string))
    {
      m_mutex.lock();
      m_get_proc = get_proc;
      m_get_proc_data = nullptr;
      m_mutex.unlock();
    }

    void
    get_proc_function(void *datum,
                      void* (*get_proc)(void*, astral::c_string))
    {
      m_mutex.lock();
      m_get_proc = nullptr;
      m_get_proc_data = get_proc;
      m_data = datum;
      m_mutex.unlock();
    }

    void*
    get_proc(astral::c_string function_name)
    {
      void *return_value(nullptr);
      m_mutex.lock();
      if (m_get_proc)
        {
          return_value = m_get_proc(function_name);
        }
      else if (m_get_proc_data)
        {
          return_value = m_get_proc_data(m_data, function_name);
        }
      else
        {
          std::cerr << m_label << ": get_proc function pointer not set when "
                    << "fetching function \"" << function_name << "\"\n";
        }
      m_mutex.unlock();
      return return_value;
    }

    astral::c_string
    label(void) const
    {
      return m_label.c_str();
    }

  private:
    std::string m_label;
    std::recursive_mutex m_mutex;
    bool m_in_callback_sequence;

    CallBackList m_list;
    void* (*m_get_proc)(astral::c_string);
    void* (*m_get_proc_data)(void *data, astral::c_string);
    void *m_data;
  };

  class CallBackPrivate:public astral::noncopyable
  {
  public:
    explicit
    CallBackPrivate(APICallbackSetPrivate *s):
      m_parent(s),
      m_active(true)
    {
      ASTRALassert(m_parent);
    }

    CallBackList::iterator m_location;
    APICallbackSetPrivate *m_parent;
    bool m_active;
  };
}

///////////////////////////////////////
// APICallbackSetPrivate methods
APICallbackSetPrivate::
APICallbackSetPrivate(astral::c_string label):
  m_label(label ? label : ""),
  m_in_callback_sequence(false),
  m_get_proc(nullptr)
{}

CallBackList::iterator
APICallbackSetPrivate::
insert(astral::APICallbackSet::CallBack *q)
{
  CallBackList::iterator return_value;
  m_mutex.lock();
  return_value = m_list.insert(m_list.end(), q);
  m_mutex.unlock();
  return return_value;
}

void
APICallbackSetPrivate::
erase(CallBackList::iterator iter)
{
  m_mutex.lock();
  m_list.erase(iter);
  m_mutex.unlock();
}

template<typename F>
void
APICallbackSetPrivate::
call_callbacks(const F &fptr)
{
  m_mutex.lock();
  if (!m_in_callback_sequence)
    {
      m_in_callback_sequence = true;
      for(auto iter = m_list.begin(); iter != m_list.end(); ++iter)
        {
          fptr(*iter);
        }
      m_in_callback_sequence = false;
    }
  m_mutex.unlock();
}

/////////////////////////////////////////////////
// astral::APICallbackSet::CallBack methods
astral::APICallbackSet::CallBack::
CallBack(APICallbackSet *parent)
{
  CallBackPrivate *d;
  APICallbackSetPrivate *dp;
  dp = static_cast<APICallbackSetPrivate*>(parent->m_d);

  ASTRALassert(dp);
  d = ASTRALnew CallBackPrivate(dp);
  d->m_location = dp->insert(this);

  m_d = d;
}

astral::APICallbackSet::CallBack::
~CallBack()
{
  CallBackPrivate *d;

  d = static_cast<CallBackPrivate*>(m_d);
  if (d->m_active)
    {
      d->m_parent->erase(d->m_location);
    }
  ASTRALdelete(d);
}

bool
astral::APICallbackSet::CallBack::
active(void) const
{
  CallBackPrivate *d;
  d = static_cast<CallBackPrivate*>(m_d);
  return d->m_active;
}

void
astral::APICallbackSet::CallBack::
active(bool b)
{
  CallBackPrivate *d;
  d = static_cast<CallBackPrivate*>(m_d);

  if (d->m_active == b)
    {
      return;
    }

  d->m_active = b;
  if (b)
    {
      d->m_location = d->m_parent->insert(this);
    }
  else
    {
      d->m_parent->erase(d->m_location);
    }
}

//////////////////////////////////////////////
// astral::APICallbackSet methods
astral::APICallbackSet::
APICallbackSet(c_string label)
{
  m_d = ASTRALnew APICallbackSetPrivate(label);
}

astral::APICallbackSet::
~APICallbackSet()
{
  APICallbackSetPrivate *d;
  d = static_cast<APICallbackSetPrivate*>(m_d);
  ASTRALdelete(d);
}

astral::c_string
astral::APICallbackSet::
label(void) const
{
  APICallbackSetPrivate *d;
  d = static_cast<APICallbackSetPrivate*>(m_d);
  return d->label();
}

void
astral::APICallbackSet::
get_proc_function(void* (*get_proc)(c_string))
{
  APICallbackSetPrivate *d;
  d = static_cast<APICallbackSetPrivate*>(m_d);
  d->get_proc_function(get_proc);
}

void
astral::APICallbackSet::
get_proc_function(void *data, void* (*get_proc)(void*, c_string))
{
  APICallbackSetPrivate *d;
  d = static_cast<APICallbackSetPrivate*>(m_d);
  d->get_proc_function(data, get_proc);
}

void*
astral::APICallbackSet::
get_proc(c_string function_name)
{
  APICallbackSetPrivate *d;
  d = static_cast<APICallbackSetPrivate*>(m_d);
  return d->get_proc(function_name);
}

void
astral::APICallbackSet::
call_unloadable_function(c_string function_name)
{
  APICallbackSetPrivate *d;
  d = static_cast<APICallbackSetPrivate*>(m_d);
  d->call_callbacks(std::bind(&CallBack::on_call_unloadable_function,
                              std::placeholders::_1, function_name));
}

void
astral::APICallbackSet::
pre_call(c_string call_string_values,
         c_string call_string_src,
         c_string function_name,
         void *function_ptr,
         c_string src_file, int src_line)
{
  APICallbackSetPrivate *d;
  d = static_cast<APICallbackSetPrivate*>(m_d);
  d->call_callbacks(std::bind(&CallBack::pre_call,
                              std::placeholders::_1,
                              call_string_values,
                              call_string_src,
                              function_name,
                              function_ptr,
                              src_file, src_line));
}

void
astral::APICallbackSet::
post_call(c_string call_string_values,
          c_string call_string_src,
          c_string function_name,
          c_string error_string,
          void *function_ptr,
          c_string src_file, int src_line)
{
  APICallbackSetPrivate *d;
  d = static_cast<APICallbackSetPrivate*>(m_d);
  d->call_callbacks(std::bind(&CallBack::post_call,
                              std::placeholders::_1,
                              call_string_values,
                              call_string_src,
                              function_name,
                              error_string,
                              function_ptr,
                              src_file, src_line));
}

void
astral::APICallbackSet::
message(c_string message, c_string src_file, int src_line)
{
  APICallbackSetPrivate *d;
  d = static_cast<APICallbackSetPrivate*>(m_d);
  d->call_callbacks(std::bind(&CallBack::message,
                              std::placeholders::_1,
                              message, src_file, src_line));
}

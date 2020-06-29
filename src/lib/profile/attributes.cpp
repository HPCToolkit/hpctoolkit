// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2019-2020, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

#include "attributes.hpp"

#include "util/log.hpp"

#include <limits>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <sstream>

using namespace hpctoolkit;

static constexpr auto max_ulong = std::numeric_limits<unsigned long>::max();
static constexpr auto max_uint32 = std::numeric_limits<uint32_t>::max();

ProfileAttributes::ProfileAttributes()
  : m_name(), m_job(max_ulong), m_path(), m_env() {};

ThreadAttributes::ThreadAttributes()
  : m_hostid(max_uint32), m_mpirank(max_ulong), m_threadid(max_ulong), m_procid(max_ulong) {};

void ProfileAttributes::name(std::string&& s) {
  if(s.empty())
    util::log::fatal() << "Attempt to give a Profile an empty name!";
  if(!m_name.empty())
    util::log::fatal() << "Attempt to directly give a Profile a name twice!";
  m_name = std::move(s);
}

void ProfileAttributes::path(stdshim::filesystem::path&& p) {
  if(p.empty())
    util::log::fatal() << "Attempt to give a Profile an empty path!";
  if(!m_path.empty())
    util::log::fatal() << "Attempt to directly give a Profile a path twice!";
  m_path = std::move(p);
}

bool ProfileAttributes::has_job() const noexcept { return m_job != max_ulong; }
unsigned long ProfileAttributes::job() const {
  if(!has_job())
    util::log::fatal() << "Attempt to get an unset job id value!";
  return m_job;
}
void ProfileAttributes::job(unsigned long j) {
  if(has_job())
    util::log::fatal() << "Attempt to directly give a Profile a job id twice!";
  m_job = j;
}

const std::string* ProfileAttributes::environment(const std::string& v) const noexcept {
  auto it = m_env.find(v);
  return it != m_env.end() ? &it->second : nullptr;
}
void ProfileAttributes::environment(const std::string& var,
                                    const std::string& val) {
  if(m_env.count(var) != 0)
    util::log::fatal() << "Attempt to directly give a Profile values for an environment variable twice!";
  m_env.emplace(var, val);
}

bool ThreadAttributes::has_hostid() const noexcept { return m_hostid != max_uint32; }
uint32_t ThreadAttributes::hostid() const {
  if(!has_hostid())
    util::log::fatal() << "Attempt to get an unset host id value!";
  return m_hostid;
}
void ThreadAttributes::hostid(uint32_t hid) {
  if(has_hostid())
    util::log::fatal() << "Attempt to directly give a Thread a host id twice!";
  m_hostid = hid;
}

bool ThreadAttributes::has_mpirank() const noexcept { return m_mpirank != max_ulong; }
unsigned long ThreadAttributes::mpirank() const {
  if(!has_mpirank())
    util::log::fatal() << "Attempt to get an unset MPI rank value!";
  return m_mpirank;
}
void ThreadAttributes::mpirank(unsigned long rank) {
  if(has_mpirank())
    util::log::fatal() << "Attempt to directly give a Thread an MPI rank twice!";
  m_mpirank = rank;
}

bool ThreadAttributes::has_threadid() const noexcept { return m_threadid != max_ulong; }
unsigned long ThreadAttributes::threadid() const {
  if(!has_threadid())
    util::log::fatal() << "Attempt to get an unset thread id value!";
  return m_threadid;
}
void ThreadAttributes::threadid(unsigned long id) {
  if(has_threadid())
    util::log::fatal() << "Attempt to directly give a Thread a thread id twice!";
  m_threadid = id;
}

bool ThreadAttributes::has_procid() const noexcept { return m_procid != max_ulong; }
unsigned long ThreadAttributes::procid() const {
  if(!has_procid())
    util::log::fatal() << "Attempt to get an unset process id value!";
  return m_procid;
}
void ThreadAttributes::procid(unsigned long pid) {
  if(has_procid())
    util::log::fatal() << "Attempt to directly give a Thread a process id twice!";
  m_procid = pid;
}

bool ProfileAttributes::merge(const ProfileAttributes& o) {
  bool ok = true;
  if(m_name.empty()) m_name = o.m_name;
  else if(!o.m_name.empty() && m_name != o.m_name) {
    util::log::warning() << "Merging profiles with different names: `"
      << m_name << "' and `" << o.m_name << "'!";
    ok = false;
  }

  if(m_job == max_ulong) m_job = o.m_job;
  else if(o.m_job != max_ulong && m_job != o.m_job) {
    util::log::warning() << "Merging profiles with different job ids: "
      << m_job << " and " << o.m_job << "!";
    ok = false;
  }

  if(m_path.empty()) m_path = o.m_path;
  else if(!o.m_path.empty() && m_path != o.m_path) {
    util::log::warning() << "Merging profiles with different paths: `"
      << m_path.string() << "' and `" << o.m_path.string() << "'!";
    ok = false;
  }

  for(const auto& e: o.m_env) {
    auto it = m_env.find(e.first);
    if(it == m_env.end()) m_env.insert(e);
    else if(it->second != e.second) {
      util::log::warning() << "Merging profiles with different values for environment `"
        << e.first << "': `" << it->second << "' and `" << e.second << "'!";
      ok = false;
    }
  }

  return ok;
}

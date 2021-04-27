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

ProfileAttributes::ProfileAttributes() = default;
ThreadAttributes::ThreadAttributes() = default;

void ProfileAttributes::name(std::string&& s) {
  assert(!s.empty() && "Attempt to give a profile an empty name!");
  assert(!m_name && "Attempt to overwrite a previously set profile name!");
  m_name = std::move(s);
}

void ProfileAttributes::path(stdshim::filesystem::path&& p) {
  assert(!p.empty() && "Attempt to give a profile an empty path!");
  assert(!m_path && "Attempt to overwrite a previously set profile path!");
  m_path = std::move(p);
}

void ProfileAttributes::job(unsigned long j) {
  assert(!m_job && "Attempt to overwrite a previously set profile job id!");
  m_job = j;
}

const std::string* ProfileAttributes::environment(const std::string& v) const noexcept {
  auto it = m_env.find(v);
  if(it == m_env.end()) return nullptr;
  return &it->second;
}
void ProfileAttributes::environment(const std::string& var,
                                    const std::string& val) {
  assert(m_env.count(var) == 0 && "Attempt to overwrite a previously set profile environment variable!");
  m_env.emplace(var, val);
}

void ThreadAttributes::procid(unsigned long pid) {
  assert(!m_procid && "Attempt to overwrite a previously set profile process id!");
  m_procid = pid;
}

void ThreadAttributes::timepointCnt(unsigned long long cnt) {
  assert(!m_timepointCnt && "Attempt to overwrite a previously set timepoint count!");
  m_timepointCnt = cnt;
}

const std::vector<pms_id_t>& ThreadAttributes::idTuple() const noexcept {
  assert(!m_idTuple.empty() && "Thread has an empty hierarchical id tuple!");
  return m_idTuple;
}

void ThreadAttributes::idTuple(const std::vector<pms_id_t>& tuple) {
  assert(m_idTuple.empty() && "Attempt to overwrite a previously set thread hierarchical id tuple!");
  assert(!tuple.empty() && "No tuple given to ThreadAttributes::idTuple");
  for(const auto& t: tuple) {
    switch(t.kind) {
    case IDTUPLE_NODE: m_hostid = t.index; break;
    case IDTUPLE_RANK: m_mpirank = t.index; break;
    case IDTUPLE_THREAD: m_threadid = t.index; break;
    case IDTUPLE_GPUSTREAM: m_threadid = t.index + 500; break;
    default: break;
    }
  }
  m_idTuple = tuple;
}

// Stringification
std::ostream& std::operator<<(std::ostream& os, const ThreadAttributes& ta) noexcept {
  bool first = true;
  for(const auto& kv: ta.idTuple()) {
    if(!first) os << ' ';
    else first = false;
    switch(kv.kind) {
    case IDTUPLE_SUMMARY:
      os << "SUMMARY";
      if(kv.index != 0) os << "(" << kv.index << ")";
      break;
    case IDTUPLE_NODE:
      os << "NODE{" << std::hex << std::setw(8) << kv.index << std::dec << '}';
      break;
    case IDTUPLE_RANK: os << "RANK{" << kv.index << "}"; break;
    case IDTUPLE_THREAD: os << "THREAD{" << kv.index << "}"; break;
    case IDTUPLE_GPUDEVICE: os << "GPUDEVICE{" << kv.index << "}"; break;
    case IDTUPLE_GPUCONTEXT: os << "GPUCONTEXT{" << kv.index << "}"; break;
    case IDTUPLE_GPUSTREAM: os << "GPUSTREAM{" << kv.index << "}"; break;
    case IDTUPLE_CORE: os << "CORE{" << kv.index << "}"; break;
    default: os << "[" << kv.kind << "]{" << kv.index << "}"; break;
    }
  }
  return os;
}

bool ProfileAttributes::merge(const ProfileAttributes& o) {
  bool ok = true;
  if(!m_name) m_name = o.m_name;
  else if(o.m_name && m_name != o.m_name) {
    util::log::warning() << "Merging profiles with different names: `"
      << *m_name << "' and `" << *o.m_name << "'!";
    ok = false;
  }

  if(!m_job) m_job = o.m_job;
  else if(o.m_job && m_job != o.m_job) {
    util::log::warning() << "Merging profiles with different job ids: "
      << *m_job << " and " << *o.m_job << "!";
    ok = false;
  }

  if(!m_path) m_path = o.m_path;
  else if(o.m_path && m_path != o.m_path) {
    util::log::warning() << "Merging profiles with different paths: `"
      << m_path->string() << "' and `" << o.m_path->string() << "'!";
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

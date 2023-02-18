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
// Copyright ((c)) 2002-2023, Rice University
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

#include "mpi/one2one.hpp"

#include "util/log.hpp"

#include <limits>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <sstream>

using namespace hpctoolkit;

Thread::Thread(ud_t::struct_t& rs, ThreadAttributes attr)
  : userdata(rs, std::cref(*this)), attributes(std::move(attr)) {
  assert(attributes.ok());
}

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
void ProfileAttributes::environment(std::string var, std::string val) {
  assert(m_env.count(var) == 0 && "Attempt to overwrite a previously set profile environment variable!");
  m_env.emplace(std::move(var), std::move(val));
}

void ProfileAttributes::idtupleName(uint16_t kind, std::string name) {
  assert(m_idtupleNames.count(kind) == 0 && "Attempt to overwrite a previously set profile idtuple name!");
  m_idtupleNames.emplace(kind, std::move(name));
}

bool ThreadAttributes::ok() const noexcept {
  return !m_idTuple.empty();
}

unsigned long long ThreadAttributes::ctxTimepointMaxCount() const noexcept {
  return m_ctxTimepointStats ? m_ctxTimepointStats->first : 0;
}
unsigned int ThreadAttributes::ctxTimepointDisorder() const noexcept {
  return m_ctxTimepointStats ? m_ctxTimepointStats->second : 0;
}
void ThreadAttributes::ctxTimepointStats(unsigned long long cnt, unsigned int disorder) noexcept {
  assert(!m_ctxTimepointStats && "Attempt to overwrite previously set timepoint stats!");
  m_ctxTimepointStats = {cnt, disorder};
}

unsigned long long ThreadAttributes::metricTimepointMaxCount(const Metric& m) const noexcept {
  auto it = m_metricTimepointStats.find(m);
  if(it == m_metricTimepointStats.end()) return 0;
  return it->second.first;
}
unsigned int ThreadAttributes::metricTimepointDisorder(const Metric& m) const noexcept {
  auto it = m_metricTimepointStats.find(m);
  if(it == m_metricTimepointStats.end()) return 0;
  return it->second.second;
}
void ThreadAttributes::metricTimepointStats(const Metric& m, unsigned long long cnt, unsigned int disorder) {
  bool newElem = m_metricTimepointStats.insert({m, {cnt, disorder}}).second;
  assert(newElem && "Attempt to overwrite previously set timepoint stats!");
}


const std::vector<pms_id_t>& ThreadAttributes::idTuple() const noexcept {
  assert(!m_idTuple.empty() && "Thread has an empty hierarchical id tuple!");
  return m_idTuple;
}
void ThreadAttributes::idTuple(std::vector<pms_id_t> tuple) {
  assert(m_idTuple.empty() && "Attempt to overwrite a previously set thread hierarchical id tuple!");
  assert(!tuple.empty() && "No tuple given to ThreadAttributes::idTuple");
  m_idTuple = std::move(tuple);
}

void ThreadAttributes::finalize(ThreadAttributes::FinalizeState& state) {
  for(size_t i = 0; i < m_idTuple.size(); i++) {
    auto& id = m_idTuple[i];
    switch(IDTUPLE_GET_INTERPRET(id.kind)) {
    case IDTUPLE_IDS_BOTH_VALID:
    case IDTUPLE_IDS_LOGIC_ONLY:
      break;
    case IDTUPLE_IDS_LOGIC_GLOBAL: {
      uint16_t kind = IDTUPLE_GET_KIND(id.kind);
      if(mpi::World::rank() == 0) {
        id.logical_index = state.globalIdxMap[kind].get(id.physical_index);
      } else {
        std::unique_lock<std::mutex> l(state.mpilock);
        mpi::send<std::uint16_t>(kind, 0, mpi::Tag::ThreadAttributes_1);
        mpi::send<std::uint64_t>(id.physical_index, 0, mpi::Tag::ThreadAttributes_1);
        id.logical_index = mpi::receive<std::uint64_t>(0, mpi::Tag::ThreadAttributes_1);
      }
      id.kind = IDTUPLE_COMPOSE(kind, IDTUPLE_IDS_BOTH_VALID);
      break;
    }
    case IDTUPLE_IDS_LOGIC_LOCAL: {
      uint16_t kind = IDTUPLE_GET_KIND(id.kind);
      std::vector<uint16_t> key;
      key.reserve(i * 5 + 1);
      for(size_t j = 0; j < i; j++) {
        key.push_back(IDTUPLE_GET_KIND(m_idTuple[j].kind));
        key.push_back((uint16_t)((m_idTuple[j].physical_index >> 48) & 0xffff));
        key.push_back((uint16_t)((m_idTuple[j].physical_index >> 32) & 0xffff));
        key.push_back((uint16_t)((m_idTuple[j].physical_index >> 16) & 0xffff));
        key.push_back((uint16_t)(m_idTuple[j].physical_index & 0xffff));
      }
      key.push_back(kind);
      if(mpi::World::rank() == 0) {
        id.logical_index = state.localIdxMap[std::move(key)].get(id.physical_index);
      } else {
        std::unique_lock<std::mutex> l(state.mpilock);
        mpi::send<std::uint16_t>(IDTUPLE_INVALID, 0, mpi::Tag::ThreadAttributes_1);
        mpi::send(std::move(key), 0, mpi::Tag::ThreadAttributes_1);
        mpi::send<std::uint64_t>(id.physical_index, 0, mpi::Tag::ThreadAttributes_1);
        id.logical_index = mpi::receive<std::uint64_t>(0, mpi::Tag::ThreadAttributes_1);
      }
      id.kind = IDTUPLE_COMPOSE(kind, IDTUPLE_IDS_BOTH_VALID);
      break;
    }
    }
  }
}

ThreadAttributes::FinalizeState::FinalizeState() {
  if(mpi::World::size() > 1 && mpi::World::rank() == 0) {
    server = std::thread([this]{
      while(auto query = mpi::receive_server<std::uint16_t>(mpi::Tag::ThreadAttributes_1)) {
        auto [kind, src] = *query;
        auto& cmap = kind != IDTUPLE_INVALID ? globalIdxMap[kind]
            : localIdxMap[mpi::receive_vector<std::uint16_t>(src, mpi::Tag::ThreadAttributes_1)];
        mpi::send<std::uint64_t>(cmap.get(mpi::receive<std::uint64_t>(src, mpi::Tag::ThreadAttributes_1)),
                                 src, mpi::Tag::ThreadAttributes_1);
      }
    });
  }
}
ThreadAttributes::FinalizeState::~FinalizeState() {
  if(server.joinable()) {
    mpi::cancel_server<std::uint16_t>(mpi::Tag::ThreadAttributes_1);
    server.join();
  }
}

uint64_t ThreadAttributes::FinalizeState::CountingLookupMap::get(uint64_t k) {
  std::unique_lock<std::mutex> l(lock);
  auto it = map.find(k);
  if(it != map.end()) return it->second;
  uint64_t v = map.size();
  bool ok = map.insert({k, v}).second;
  assert(ok);
  return v;
}

size_t ThreadAttributes::FinalizeState::LocalHash::operator()(const std::vector<uint16_t>& v) const noexcept {
  size_t sponge = 0x15;
  for(auto x: v) sponge = (sponge << 5) ^ (sponge >> 33) ^ x;
  return sponge;
}

// Stringification
std::ostream& std::operator<<(std::ostream& os, const ThreadAttributes& ta) noexcept {
  bool first = true;
  for(const auto& kv: ta.idTuple()) {
    if(!first) os << ' ';
    else first = false;
    const char* intr = "ERR";
    switch(IDTUPLE_GET_INTERPRET(kv.kind)) {
    case IDTUPLE_IDS_BOTH_VALID: intr = "BOTH"; break;
    case IDTUPLE_IDS_LOGIC_LOCAL: intr = "GEN LOCAL"; break;
    case IDTUPLE_IDS_LOGIC_GLOBAL: intr = "GEN GLOBAL"; break;
    case IDTUPLE_IDS_LOGIC_ONLY: intr = "SINGLE"; break;
    }
    switch(IDTUPLE_GET_KIND(kv.kind)) {
    case IDTUPLE_SUMMARY:
      os << "SUMMARY";
      if(kv.kind != IDTUPLE_SUMMARY) os << '[' << intr << ']';
      return os;
    case IDTUPLE_NODE: os << "NODE(" << intr << "){"; break;
    case IDTUPLE_RANK: os << "RANK(" << intr << "){"; break;
    case IDTUPLE_THREAD: os << "THREAD(" << intr << "){"; break;
    case IDTUPLE_GPUDEVICE: os << "GPUDEVICE(" << intr << "){"; break;
    case IDTUPLE_GPUCONTEXT: os << "GPUCONTEXT(" << intr << "){"; break;
    case IDTUPLE_GPUSTREAM: os << "GPUSTREAM(" << intr << "){"; break;
    case IDTUPLE_CORE: os << "CORE(" << intr << "){"; break;
    default: os << "[" << IDTUPLE_GET_KIND(kv.kind) << "](" << intr << "){"; break;
    }
    if(IDTUPLE_GET_INTERPRET(kv.kind) != IDTUPLE_IDS_LOGIC_ONLY)
      os << kv.physical_index << ", ";
    os << kv.logical_index << '}';
  }
  return os;
}

bool ProfileAttributes::merge(const ProfileAttributes& o) {
  bool ok = true;
  if(!m_name) m_name = o.m_name;
  else if(o.m_name && m_name != o.m_name) {
    util::log::vwarning() << "Merging profiles with different names: `"
      << *m_name << "' and `" << *o.m_name << "'!";
    ok = false;
  }

  if(!m_job) m_job = o.m_job;
  else if(o.m_job && m_job != o.m_job) {
    util::log::vwarning() << "Merging profiles with different job ids: "
      << *m_job << " and " << *o.m_job << "!";
    ok = false;
  }

  if(!m_path) m_path = o.m_path;
  else if(o.m_path && m_path != o.m_path) {
    util::log::vwarning() << "Merging profiles with different paths: `"
      << m_path->string() << "' and `" << o.m_path->string() << "'!";
    ok = false;
  }

  for(const auto& e: o.m_env) {
    auto it = m_env.find(e.first);
    if(it == m_env.end()) m_env.insert(e);
    else if(it->second != e.second) {
      util::log::vwarning() << "Merging profiles with different values for environment `"
        << e.first << "': `" << it->second << "' and `" << e.second << "'!";
      ok = false;
    }
  }

  for(const auto& e: o.m_idtupleNames) {
    auto it = m_idtupleNames.find(e.first);
    if(it == m_idtupleNames.end()) m_idtupleNames.insert(e);
    else if(it->second != e.second) {
      util::log::vwarning() << "Merging profiles with different values for tuple kind "
        << e.first << ": '" << it->second << "' and '" << e.second << "'!";
      ok = false;
    }
  }

  return ok;
}

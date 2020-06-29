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
// Copyright ((c)) 2020, Rice University
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

#include "packed.hpp"

#include "../util/log.hpp"

#include <stack>

using namespace hpctoolkit;
using namespace sources;

template<class T> static T unpack(std::vector<uint8_t>::const_iterator&) noexcept;

template<>
std::string unpack<std::string>(std::vector<uint8_t>::const_iterator& it) noexcept {
  std::string out;
  for(; *it != '\0'; ++it) out += *it;
  ++it;  // First location after the string
  return out;
}
template<>
std::uint64_t unpack<std::uint64_t>(std::vector<uint8_t>::const_iterator& it) noexcept {
  // Little-endian order. Same as in sinks/packed.cpp.
  std::uint64_t out = 0;
  for(int shift = 0x00; shift < 0x40; shift += 0x08) {
    out |= ((std::uint64_t)*it) << shift;
    ++it;
  }
  return out;
}
template<>
double unpack<double>(std::vector<uint8_t>::const_iterator& it) noexcept {
  // Assumes doubles are compatible across systems
  union { double d; std::uint64_t u; } x;
  x.u = unpack<std::uint64_t>(it);
  return x.d;
}

Packed::Packed() {};

std::vector<uint8_t>::const_iterator Packed::unpackAttributes(iter_t it) noexcept {
  // Format: [job or magic] [name] [path] [env cnt] ([env key] [env val]...)
  ProfileAttributes attr;
  auto job = unpack<std::uint64_t>(it);
  if(job != (0xFEF1F0F3ULL << 32)) attr.job(job);
  auto name = unpack<std::string>(it);
  if(!name.empty()) attr.name(std::move(name));
  auto path = unpack<std::string>(it);
  if(!path.empty()) attr.path(std::move(path));
  auto cnt = unpack<std::uint64_t>(it);
  for(std::size_t i = 0; i < cnt; i++) {
    auto k = unpack<std::string>(it);
    auto v = unpack<std::string>(it);
    attr.environment(k, v);
  }
  sink.attributes(std::move(attr));

  // Format: [cnt] ([metric name] [metric description]...)
  metrics.clear();
  cnt = unpack<std::uint64_t>(it);
  for(std::size_t i = 0; i < cnt; i++) {
    auto name = unpack<std::string>(it);
    auto description = unpack<std::string>(it);
    metrics.emplace_back(sink.metric(name, description, Metric::Type::linear));
  }
  return it;
}

std::vector<uint8_t>::const_iterator Packed::unpackReferences(iter_t it) noexcept {
  // Format: [cnt] ([module path]...)
  modules.clear();
  auto cnt = unpack<std::uint64_t>(it);
  for(std::size_t i = 0; i < cnt; i++) {
    auto name = unpack<std::string>(it);
    modules.emplace_back(sink.module(name));
  }
  return it;
}

std::vector<uint8_t>::const_iterator Packed::unpackContexts(iter_t it) noexcept {
  std::stack<std::reference_wrapper<Context>,
    std::vector<std::reference_wrapper<Context>>> tip;
  // Format: children... [sentinal]
  while(1) {
    auto next = unpack<std::uint64_t>(it);
    if(next == (0xFEF1F0F3ULL << 32)) {
      if(tip.empty()) break;
      tip.pop();
      continue;
    }

    Scope s;
    if(next == (0xF0F1F2F3ULL << 32)) {
      // Format: [magic] children... [sentinal]
      s = Scope{};  // Unknown scope
    } else {
      // Format: [module id] [offset] children... [sentinal]
      auto off = unpack<std::uint64_t>(it);
      s = Scope{modules.at(next), off};  // Module scope
    }
    auto& c = tip.empty() ? sink.context(s) : sink.context(tip.top(), s);
    tip.push(c);
  }
  return it;
}

void Packed::ContextTracker::context(const Context& c, unsigned int& id) {
  target.emplace(id, std::ref(const_cast<Context&>(c)));
}

std::vector<uint8_t>::const_iterator Packed::unpackMetrics(iter_t it, const ctx_map_t& cs) noexcept {
  // Format: [cnt] ([context ID] ([metrics]...)...)
  auto cnt = unpack<std::uint64_t>(it);
  for(std::size_t i = 0; i < cnt; i++) {
    Context& c = cs.at(unpack<std::uint64_t>(it));
    for(Metric& m: metrics) {
      auto a = unpack<double>(it);
      auto b = unpack<double>(it);
      sink.add(c, m, {a, b});
    }
  }
  return it;
}

std::vector<uint8_t>::const_iterator Packed::unpackTimepoints(iter_t it) noexcept {
  auto min = unpack<std::uint64_t>(it);
  auto max = unpack<std::uint64_t>(it);
  if(min != 0) sink.timepoint(std::chrono::nanoseconds(min));
  if(max != 0) sink.timepoint(std::chrono::nanoseconds(max));
  return it;
}

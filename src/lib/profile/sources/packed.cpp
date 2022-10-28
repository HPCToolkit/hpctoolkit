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
// Copyright ((c)) 2019-2022, Rice University
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

#include "../util/vgannotations.hpp"

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
std::uint8_t unpack<std::uint8_t>(std::vector<uint8_t>::const_iterator& it) noexcept {
  return *(it++);
}
template<>
std::uint16_t unpack<std::uint16_t>(std::vector<uint8_t>::const_iterator& it) noexcept {
  // Little-endian order. Same as in sinks/packed.cpp.
  std::uint16_t out = 0;
  for(int shift = 0; shift < 16; shift += 8) {
    out |= ((std::uint16_t)*it) << shift;
    ++it;
  }
  return out;
}
template<>
std::uint64_t unpack<std::uint64_t>(std::vector<uint8_t>::const_iterator& it) noexcept {
  // Little-endian order. Same as in sinks/packed.cpp.
  std::uint64_t out = 0;
  for(int shift = 0; shift < 64; shift += 8) {
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
    attr.environment(std::move(k), std::move(v));
  }
  cnt = unpack<std::uint64_t>(it);
  for(std::size_t i = 0; i < cnt; i++) {
    auto k = unpack<std::uint16_t>(it);
    auto v = unpack<std::string>(it);
    attr.idtupleName(k, std::move(v));
  }
  sink.attributes(std::move(attr));

  // TODO: Add [scopes] to the below
  // Format: [cnt] ([metric name] [metric description]...)
  metrics.clear();
  cnt = unpack<std::uint64_t>(it);
  for(std::size_t i = 0; i < cnt; i++) {
    Metric::Settings s;
    s.name = unpack<std::string>(it);
    s.description = unpack<std::string>(it);
    metrics.emplace_back(sink.metric(s));
  }

  // TODO: Add [scopes] to the below
  // Format: [cnt] ([estat name] [estat description] [cnt] ([isString ? 1 : 0] ([string] | [metric id])...)...)
  cnt = unpack<std::uint64_t>(it);
  assert(cnt == 0 && "ExtraStatistics in MPI mode are currently disabled");
  for(std::size_t i = 0; i < cnt; i++) {
    ExtraStatistic::Settings s;
    s.name = unpack<std::string>(it);
    s.description = unpack<std::string>(it);

#if 0
    auto ecnt = unpack<std::uint64_t>(it);
    s.formula.reserve(ecnt);
    for(std::size_t ei = 0; ei < ecnt; ei++) {
      switch(unpack<std::uint8_t>(it)) {
      case 1:  // string
        s.formula.emplace_back(unpack<std::string>(it));
        break;
      case 0: {  // metric id
        Metric& m = metrics.at(unpack<std::uint64_t>(it));
        s.formula.emplace_back(ExtraStatistic::MetricPartialRef{
            m, m.statsAccess().requestSumPartial()});
        break;
      }
      default:
        assert(false && "Invalid case in Packed attributes!");
        std::abort();
      }
    }

    sink.extraStatistic(std::move(s));
#endif
  }

  auto min = unpack<std::uint64_t>(it);
  auto max = unpack<std::uint64_t>(it);
  if(min != 0 && max != 0) {
    sink.timepointBounds(std::chrono::nanoseconds(min),
                         std::chrono::nanoseconds(max));
  }

  for(auto& m: metrics) sink.metricFreeze(m);
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
  std::stack<std::reference_wrapper<Context>, std::vector<std::reference_wrapper<Context>>> tip;
  // Format: <global> children... [sentinel]
  auto globalTy = unpack<std::uint64_t>(it);
  assert(globalTy == (std::uint64_t)Scope::Type::global && "Packed Contexts claim root is non-global?");
  while(1) {
    auto next = unpack<std::uint64_t>(it);
    if(next == (0xFEF1F0F3ULL << 32)) {
      if(tip.empty()) break;
      tip.pop();
      continue;
    }

    Scope s;
    switch(next) {
    case (std::uint64_t)Scope::Type::point: {
      // Format: [module id] [offset] children... [sentinel]
      auto midx = unpack<std::uint64_t>(it);
      auto off = unpack<std::uint64_t>(it);
      s = Scope{modules.at(midx), off};
      break;
    }
    case (std::uint64_t)Scope::Type::placeholder: {
      // Format: [placeholder] children... [sentinel]
      auto ph = unpack<std::uint64_t>(it);
      s = Scope{Scope::placeholder, ph};
      break;
    }
    case (std::uint64_t)Scope::Type::unknown:
      s = Scope{};
      break;
    case (std::uint64_t)Scope::Type::global:
      assert(false && "Packed unpacked global Scope that wasn't the root!");
      std::abort();
    default:
      assert(false && "Unrecognized Scope type while unpacking Contexts!");
    }
    auto& c = sink.context(tip.empty() ? sink.global() : tip.top().get(),
        {Relation::call, s}).second;
    tip.push(c);
  }
  return it;
}

void Packed::ContextTracker::write() {};

void Packed::ContextTracker::notifyContext(const Context& c) noexcept {
  target.emplace(c.userdata[src.identifier()], std::ref(const_cast<Context&>(c)));
}

std::vector<uint8_t>::const_iterator Packed::unpackMetrics(iter_t it, const ctx_map_t& cs) noexcept {
  // Format: [cnt] ([context ID] ([metrics]...)...)
  auto cnt = unpack<std::uint64_t>(it);
  for(std::size_t i = 0; i < cnt; i++) {
    Context& c = cs.at(unpack<std::uint64_t>(it));
    for(Metric& m: metrics) {
      c.data().markUsed(m, MetricScopeSet(unpack<MetricScopeSet::int_type>(it)));

      util::optional_ref<StatisticAccumulator> accums;
      for(const auto& p: m.partials()) {
        double point = unpack<double>(it);
        double function = unpack<double>(it);
        double execution = unpack<double>(it);
        if(point != 0 || function != 0 || execution != 0) {
          if(!accums) accums = c.data().statisticsFor(m);
          auto accum = accums->get(p);
          if(point != 0) accum.add(MetricScope::point, point);
          if(function != 0) accum.add(MetricScope::function, function);
          if(execution != 0) accum.add(MetricScope::execution, execution);
        }
      }
    }
  }
  return it;
}

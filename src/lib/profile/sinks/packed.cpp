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

#include "lib/profile/util/vgannotations.hpp"

#include "packed.hpp"

#include <stack>

using namespace hpctoolkit;
using namespace sinks;

// Helpers for packing various things.
static void pack(std::vector<std::uint8_t>& out, const std::string& s) noexcept {
  out.reserve(out.size() + s.size() + 1);  // Allocate the space early
  for(auto c: s) {
    if(c == '\0') c = '?';
    out.push_back(c);
  }
  out.push_back('\0');
}
static void pack(std::vector<std::uint8_t>& out, const std::uint8_t v) noexcept {
  out.push_back(v);
}
static void pack(std::vector<std::uint8_t>& out, const std::uint64_t v) noexcept {
  // Little-endian order. Just in case the compiler can optimize it away.
  for(int shift = 0x00; shift < 0x40; shift += 0x08)
    out.push_back((v >> shift) & 0xff);
}
static void pack(std::vector<std::uint8_t>& out, const double v) noexcept {
  // Assumes doubles are compatible across systems
  union { double d; std::uint64_t u; } x;
  x.d = v;
  pack(out, x.u);
}

Packed::Packed()
  : metrics(), moduleIDs(), minTime(std::chrono::nanoseconds::max()),
    maxTime(std::chrono::nanoseconds::min()) {};

void Packed::packAttributes(std::vector<std::uint8_t>& out) noexcept {
  // Format: [job or magic] [name] [path] [env cnt] ([env key] [env val]...)
  const auto& attr = src.attributes();
  pack(out, (std::uint64_t)attr.job().value_or(0xFEF1F0F3ULL << 32));
  pack(out, attr.name().value_or(""));
  pack(out, attr.path() ? attr.path()->string() : "");
  pack(out, attr.environment().size());
  for(const auto& kv: attr.environment()) {
    pack(out, kv.first);
    pack(out, kv.second);
  }

  // TODO: Add [scopes] to the below
  // Format: [cnt] ([metric name] [metric description]...)
  metrics.clear();
  pack(out, src.metrics().size());
  std::unordered_map<const Metric*, size_t> metids;
  for(const Metric& m: src.metrics().citerate()) {
    metids.emplace(&m, metrics.size());
    metrics.emplace_back(m);
    pack(out, m.name());
    pack(out, m.description());
  }

  // TODO: Add [scopes] to the below
  // Format: [cnt] ([estat name] [estat description] [cnt] ([isString ? 1 : 0] ([string] | [metric id])...)...)
  pack(out, src.extraStatistics().size());
  for(const ExtraStatistic& es: src.extraStatistics().citerate()) {
    pack(out, es.name());
    pack(out, es.description());
    const auto& form = es.formula();
    pack(out, form.size());
    for(const auto& e: form) {
      if(std::holds_alternative<std::string>(e)) {
        pack(out, (std::uint8_t)1);
        pack(out, std::get<std::string>(e));
      } else {
        pack(out, (std::uint8_t)0);
        const auto& mp = std::get<ExtraStatistic::MetricPartialRef>(e);
        pack(out, metids.find(&mp.metric)->second);
      }
    }
  }
}

void Packed::packReferences(std::vector<std::uint8_t>& out) noexcept {
  // Format: [cnt] ([module path]...)
  moduleIDs.clear();
  pack(out, src.modules().size());
  for(const Module& m: src.modules().citerate()) {
    moduleIDs.emplace(&m, moduleIDs.size());
    pack(out, m.path().string());
  }
}

void Packed::packContexts(std::vector<std::uint8_t>& out) noexcept {
  src.contexts().citerate([&](const Context& c){
    pack(out, (std::uint64_t)c.scope().type());
    switch(c.scope().type()) {
    case Scope::Type::point:
    case Scope::Type::call: {
      // Format: <type> [module id] [offset] children... [sentinal]
      auto mo = c.scope().point_data();
      pack(out, moduleIDs.at(&mo.first));
      pack(out, mo.second);
      break;
    }
    case Scope::Type::unknown:
    case Scope::Type::global:
      // Format: <type> children... [sentinal]
      break;
    case Scope::Type::classified_point:
    case Scope::Type::classified_call:
    case Scope::Type::function:
    case Scope::Type::inlined_function:
    case Scope::Type::loop:
    case Scope::Type::line:
    case Scope::Type::concrete_line:
      util::log::fatal() << "Unhandled Scope type encountered in Packed!";
    }
  }, [&](const Context& c){
    pack(out, (std::uint64_t)0xFEF1F0F3ULL << 32);
  });
}

void Packed::packMetrics(std::vector<std::uint8_t>& out) noexcept {
  // Format: [cnt] ([context ID] ([metrics]...)...)
  auto start = out.size();
  std::uint64_t cnt = 0;
  pack(out, cnt);
  src.contexts().citerate([&](const Context& c){
    pack(out, (std::uint64_t)c.userdata[src.identifier()]);
    for(const Metric& m: metrics) {
      for(const auto& p: m.partials()) {
        if(auto v = m.getFor(c)) {
          pack(out, (double)v->get(p).get(MetricScope::point).value_or(0));
          pack(out, (double)v->get(p).get(MetricScope::function).value_or(0));
          pack(out, (double)v->get(p).get(MetricScope::execution).value_or(0));
        } else {
          pack(out, (double)0);
          pack(out, (double)0);
          pack(out, (double)0);
        }
      }
    }
    cnt++;
  }, nullptr);
  // Skip back and overwrite the beginning.
  std::vector<std::uint8_t> tmp;
  pack(tmp, cnt);
  for(const auto b: tmp) out[start++] = b;
}

void Packed::notifyTimepoint(std::chrono::nanoseconds tp) {
  std::chrono::nanoseconds old = minTime.load(std::memory_order_relaxed);
  while(tp < old
        && !minTime.compare_exchange_weak(old, tp, std::memory_order_relaxed));
  old = maxTime.load(std::memory_order_relaxed);
  while(old < tp
        && !maxTime.compare_exchange_weak(old, tp, std::memory_order_relaxed));
}

void Packed::packTimepoints(std::vector<std::uint8_t>& out) noexcept {
  auto min = minTime.load(std::memory_order_relaxed);
  auto max = maxTime.load(std::memory_order_relaxed);
  bool minValid = min < std::chrono::nanoseconds::max();
  bool maxValid = max > std::chrono::nanoseconds::min();
  if(minValid != maxValid) util::log::fatal() << "Timepoint half invalid???";
  if(minValid) {
    pack(out, (std::uint64_t)min.count());
    pack(out, (std::uint64_t)max.count());
  } else {
    pack(out, (std::uint64_t)0);
    pack(out, (std::uint64_t)0);
  }
}

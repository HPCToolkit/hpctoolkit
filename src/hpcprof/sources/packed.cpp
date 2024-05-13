// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#include "../util/vgannotations.hpp"

#include "packed.hpp"

#include "../util/log.hpp"

#include <limits>
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

static_assert(std::is_same_v<StatisticAccumulator::raw_t, std::array<double, 5>>, "unpack needs to be updated!");
template<>
StatisticAccumulator::raw_t unpack<StatisticAccumulator::raw_t>(std::vector<uint8_t>::const_iterator& it) noexcept {
  return StatisticAccumulator::raw_t{
    unpack<double>(it), unpack<double>(it), unpack<double>(it),
    unpack<double>(it), unpack<double>(it),
  };
}

Packed::Packed() {};
Packed::Packed(const IdTracker& tracker) : tracker(tracker) {};

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

  // Format: [cnt] ([metric name] [metric description] [scopes] [visibility] [orderId])...
  metrics.clear();
  cnt = unpack<std::uint64_t>(it);
  for(std::size_t i = 0; i < cnt; i++) {
    Metric::Settings s;
    s.name = unpack<std::string>(it);
    s.description = unpack<std::string>(it);
    s.scopes = MetricScopeSet(unpack<MetricScopeSet::int_type>(it));
    s.visibility = (Metric::Settings::visibility_t)unpack<std::uint8_t>(it);
    auto o = unpack<std::uint64_t>(it);
    if(o != std::numeric_limits<std::uint64_t>::max())
      s.orderId = o;
    auto nparts = unpack<std::uint8_t>(it);
    metrics.emplace_back(std::ref(sink.metric(s)), nparts);
  }

  // Format: [cnt] ([estat name] [estat description] [scopes] [formula])...
  // Where [formula] = [kind] [formula args]...
  cnt = unpack<std::uint64_t>(it);
  for(std::size_t i = 0; i < cnt; i++) {
    ExtraStatistic::Settings s;
    s.name = unpack<std::string>(it);
    s.description = unpack<std::string>(it);
    s.scopes = MetricScopeSet(unpack<MetricScopeSet::int_type>(it));

    const std::function<Expression()> unpackExpression = [&]() -> Expression {
      Expression::Kind kind = (Expression::Kind)unpack<std::uint8_t>(it);
      switch(kind) {
      case Expression::Kind::constant:
        return Expression(unpack<double>(it));
      case Expression::Kind::variable: {
        Metric& m = metrics.at(unpack<std::uint64_t>(it)).first;
        m.statsAccess().requestSumPartial();
        return Expression((Expression::uservalue_t)&m);
      }
      case Expression::Kind::subexpression:
        assert(false && "Attempt to unpack invalid Expression, should not have sub-Expressions");
        abort();
      default: {
        assert(kind >= Expression::Kind::first_op);
        const int argcnt = unpack<std::uint8_t>(it);
        std::vector<Expression> args;
        args.reserve(argcnt);
        for(int i = 0; i < argcnt; i++)
          args.emplace_back(unpackExpression());
        return Expression(kind, std::move(args));
      }
      }
    };
    s.formula = unpackExpression();
    sink.extraStatistic(std::move(s));
  }

  for(auto& [m, nparts]: metrics) {
    sink.metricFreeze(m);
    assert(m.get().partials().size() == nparts && "Inconsistent number of partials across ranks!");
  }
  return it;
}

std::vector<uint8_t>::const_iterator Packed::unpackReferences(iter_t it) noexcept {
  // Format: [cnt] (([module path] [module relative path])...)
  modules.clear();
  auto cnt = unpack<std::uint64_t>(it);
  for(std::size_t i = 0; i < cnt; i++) {
    auto name = unpack<std::string>(it);
    auto relative_name = unpack<std::string>(it);
    if(!relative_name.empty()){
      modules.emplace_back(sink.module(name, relative_name));
    }else{
      modules.emplace_back(sink.module(name));
    }
  }
  return it;
}

std::vector<uint8_t>::const_iterator Packed::unpackContexts(iter_t it) noexcept {
  std::stack<std::reference_wrapper<Context>, std::vector<std::reference_wrapper<Context>>> tip;
  // Format: <global> children... [sentinel]
  [[maybe_unused]] auto globalTy = unpack<std::uint64_t>(it);
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
    case (std::uint64_t)Scope::Type::function:
    case (std::uint64_t)Scope::Type::lexical_loop:
    case (std::uint64_t)Scope::Type::binary_loop:
    case (std::uint64_t)Scope::Type::line:
      // Unrepresented Scopes, ignore and press on
      tip.push(tip.top());
      continue;
    default:
      assert(false && "Unrecognized Scope type while unpacking Contexts!");
      std::abort();
    }
    auto& c = sink.context(tip.empty() ? sink.global() : tip.top().get(),
        {Relation::call, s}).second;
    tip.push(c);
  }

  // Format: [cnt] ([Scope.type] Scope...)...
  auto cnt = unpack<std::uint64_t>(it);
  for(std::uint64_t i = 0; i < cnt; ++i) {
    auto next = unpack<std::uint64_t>(it);
    Scope s;
    switch(next) {
    case (std::uint64_t)Scope::Type::point: {
      // Format: [Scope.type] [module id] [offset]
      auto midx = unpack<std::uint64_t>(it);
      auto off = unpack<std::uint64_t>(it);
      s = Scope{modules.at(midx), off};
      break;
    }
    default:
      assert(false && "Unrecognized Scope type while unpacking Contexts!");
      std::abort();
    }

    sink.contextFlowGraph(s);
  }

  return it;
}

void Packed::IdTracker::write() {};

void Packed::IdTracker::notifyContext(const Context& c) noexcept {
  contexts.emplace(c.userdata[src.identifier()], std::ref(const_cast<Context&>(c)));
}

void Packed::IdTracker::notifyMetric(const Metric& m) noexcept {
  metrics.emplace(m.userdata[src.identifier()], std::cref(m));
}

std::vector<uint8_t>::const_iterator Packed::unpackMetrics(iter_t it) noexcept {
  // Format: [cnt] ([context ID] [cnt] ([metric ID] [use] [values]...)...)...
  assert(tracker && "unpackMetrics can only be used if an IdTracker is provided!");
  auto cnt = unpack<std::uint64_t>(it);
  for(std::size_t i = 0; i < cnt; i++) {
    Context& c = tracker->contexts.at(unpack<std::uint64_t>(it));
    auto mcnt = unpack<std::uint64_t>(it);
    for(std::size_t j = 0; j < mcnt; j++) {
      const Metric& m = tracker->metrics.at(unpack<std::uint64_t>(it));
      c.data().markUsed(m, MetricScopeSet(unpack<MetricScopeSet::int_type>(it)));

      util::optional_ref<StatisticAccumulator> accums;
      for(const auto& p: m.partials()) {
        auto value = unpack<StatisticAccumulator::raw_t>(it);
        if(value != StatisticAccumulator::rawZero()) {
          if(!accums) accums = c.data().statisticsFor(m);
          accums->get(p).addRaw(value);
        }
      }
    }
  }
  return it;
}

std::vector<uint8_t>::const_iterator Packed::unpackTimepoints(iter_t it) noexcept {
  auto min = unpack<std::uint64_t>(it);
  auto max = unpack<std::uint64_t>(it);
  if(min != 0 && max != 0) {
    sink.timepointBounds(std::chrono::nanoseconds(min),
                         std::chrono::nanoseconds(max));
  }
  return it;
}

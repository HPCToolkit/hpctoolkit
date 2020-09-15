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

#include "metric.hpp"

#include "context.hpp"
#include "attributes.hpp"

#include <thread>
#include <ostream>

using namespace hpctoolkit;

static double atomic_add(std::atomic<double>& a, const double v) noexcept {
  double old = a.load(std::memory_order_relaxed);
  while(!a.compare_exchange_weak(old, old+v, std::memory_order_relaxed));
  return old;
}

static bool pullsExclusive(const Context& parent, const Context& child) {
  switch(child.scope().type()) {
  case Scope::Type::function:
  case Scope::Type::inlined_function:
  case Scope::Type::loop:
    return false;
  case Scope::Type::unknown:
  case Scope::Type::point:
    switch(parent.scope().type()) {
    case Scope::Type::function:
    case Scope::Type::inlined_function:
    case Scope::Type::loop:
      return true;
    case Scope::Type::unknown:
    case Scope::Type::point:
    case Scope::Type::global:
      return false;
    }
  case Scope::Type::global:
    util::log::fatal{} << "Operation invalid for the global Context!";
  }
  std::abort();  // unreachable
}

unsigned int Metric::ScopedIdentifiers::get(MetricScope s) const noexcept {
  switch(s) {
  case MetricScope::point: return point;
  case MetricScope::exclusive: return exclusive;
  case MetricScope::inclusive: return inclusive;
  default: util::log::fatal{} << "Invalid Metric::scope value!";
  }
  std::abort();  // unreachable
}

MetricScopeSet Metric::scopes() const noexcept {
  // For now, its always exclusive/inclusive
  return MetricScopeSet(MetricScope::exclusive) + MetricScopeSet(MetricScope::inclusive);
}

void AccumulatorRef::add(MetricScope s, double v) noexcept {
  if(v == 0) return;
  switch(s) {
  case MetricScope::point: util::log::fatal{} << "TODO: Support point Metric::Scope!";
  case MetricScope::exclusive:
    atomic_add(accum->exclusive, v);
    break;
  case MetricScope::inclusive:
    atomic_add(accum->inclusive, v);
    break;
  }
}

AccumulatorRef Metric::addTo(Context& c) noexcept {
  return c.data[this];
}

void ThreadAccumulatorRef::add(double v) noexcept {
  if(v != 0) atomic_add(accum->exclusive, v);
}

ThreadAccumulatorRef Metric::addTo(Thread::Temporary& t, Context& c) noexcept {
  return t.data[&c][this];
}

static stdshim::optional<double> opt0(double d) {
  return d == 0 ? stdshim::optional<double>{} : d;
}

stdshim::optional<double> AccumulatorCRef::get(MetricScope s) const noexcept {
  if(accum == nullptr) return {};
  switch(s) {
  case MetricScope::point: util::log::fatal{} << "TODO: Support point Metric::Scope!";
  case MetricScope::exclusive: return opt0(accum->exclusive.load(std::memory_order_relaxed));
  case MetricScope::inclusive: return opt0(accum->inclusive.load(std::memory_order_relaxed));
  default: util::log::fatal{} << "Invalid Scope value!";
  };
  std::abort();  // unreachable
}

AccumulatorCRef Metric::getFor(const Context& c) const noexcept {
  auto* a = c.data.find(this);
  if(a == nullptr) return {};
  return *a;
}

stdshim::optional<double> ThreadAccumulatorCRef::get(MetricScope s) const noexcept {
  if(accum == nullptr) return {};
  switch(s) {
  case MetricScope::point: util::log::fatal{} << "TODO: Support point Metric::Scope!";
  case MetricScope::exclusive:
    return opt0(accum->exclusive.load(std::memory_order_relaxed));
  case MetricScope::inclusive:
    return opt0(accum->inclusive);
  default: util::log::fatal{} << "Invalid Scope value!";
  }
  std::abort();  // unreachable
}

ThreadAccumulatorCRef Metric::getFor(const Thread::Temporary& t, const Context& c) const noexcept {
  auto* cd = t.data.find(&c);
  if(cd == nullptr) return {};
  auto* md = cd->find(this);
  if(md == nullptr) return {};
  return *md;
}

void Metric::finalize(Thread::Temporary& t) noexcept {
  // For each Context we need to know what its children are. But we only care
  // about ones that have decendants with actual data. So we construct a
  // temporary subtree with all the bits.
  const Context* global = nullptr;
  std::unordered_map<const Context*, std::vector<const Context*>> children;
  {
    std::vector<const Context*> newContexts;
    newContexts.reserve(t.data.size());
    for(const auto& cx: t.data.citerate()) newContexts.emplace_back(cx.first);
    while(!newContexts.empty()) {
      decltype(newContexts) next;
      next.reserve(newContexts.size());
      for(const Context* cp: newContexts) {
        if(!cp->direct_parent()) {
          if(global != nullptr) util::log::fatal{} << "Multiple root contexts???";
          global = cp;
          continue;
        }
        auto x = children.emplace(cp->direct_parent(), std::vector<const Context*>{cp});
        if(x.second) next.push_back(cp->direct_parent());
        else x.first->second.push_back(cp);
      }
      next.shrink_to_fit();
      newContexts = std::move(next);
    }
  }

  // Now that the critical subtree is built, recursively propagate up.
  using md_t = util::locked_unordered_map<const Metric*, MetricAccumulator>;
  std::function<const md_t&(const Context*)> propagate = [&](const Context* c) -> const md_t&{
    md_t& data = t.data[c];
    // Handle the internal propagation first, so we don't get mixed up.
    for(auto& mx: data.iterate()) {
      mx.second.inclusive = mx.second.exclusive.load(std::memory_order_relaxed);
    }

    auto ccit = children.find(c);
    if(ccit != children.end()) {
      // First recurse and ensure the values for our children are prepped.
      // Remember the pointer for each child's metric data.
      std::vector<std::reference_wrapper<const md_t>> submds;
      submds.reserve(ccit->second.size());
      for(const Context* cc: ccit->second) submds.push_back(propagate(cc));

      // Then go through each and sum into our bits
      for(std::size_t i = 0; i < submds.size(); i++) {
        const Context* cc = ccit->second[i];
        const md_t& ccmd = submds[i];
        const bool pullex = pullsExclusive(*c, *cc);
        for(const auto& mx: ccmd.citerate()) {
          auto& accum = data[mx.first];
          if(pullex) atomic_add(accum.exclusive, mx.second.exclusive.load(std::memory_order_relaxed));
          accum.inclusive += mx.second.inclusive;
        }
      }
    }

    // Now that our bits are stable, accumulate back into the Statistics
    auto& cdata = const_cast<Context*>(c)->data;
    for(const auto& mx: data.citerate()) {
      auto& accum = cdata[mx.first];
      atomic_add(accum.exclusive, mx.second.exclusive.load(std::memory_order_relaxed));
      atomic_add(accum.inclusive, mx.second.inclusive);
    }
    return data;
  };
  propagate(global);
}

std::size_t std::hash<Metric::Settings>::operator()(const Metric::Settings &s) const noexcept {
  const auto h1 = std::hash<std::string>{}(s.name);
  const auto h2 = std::hash<std::string>{}(s.description);
  return h1 ^ ((h2 << 1) | (h2 >> (-1 + 8 * sizeof h2)));
}

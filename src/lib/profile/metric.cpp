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

#include <thread>
#include <ostream>

using namespace hpctoolkit;

static double atomic_add(std::atomic<double>& a, const double& v) noexcept {
  double old = a.load(std::memory_order_relaxed);
  while(!a.compare_exchange_weak(old, old+v, std::memory_order_relaxed));
  return old;
}

static bool pullsExclusive(Context& c) {
  switch(c.scope().type()) {
  case Scope::Type::function:
  case Scope::Type::inlined_function:
  case Scope::Type::loop:
    return true;
  case Scope::Type::unknown:
  case Scope::Type::point:
  case Scope::Type::global:
    return false;
  }
  return false;
}

void Metric::AccumulatorRef::add(Metric::Scope s, double v) noexcept {
  if(v == 0) return;
  switch(s) {
  case Scope::point: util::log::fatal{} << "TODO: Support point Metric::Scope!";
  case Scope::exclusive:
    atomic_add(accum->exclusive, v);
    break;
  case Scope::inclusive:
    atomic_add(accum->inclusive, v);
    break;
  }
}

Metric::AccumulatorRef Metric::getFor(Context& c) noexcept {
  return c.data[member];
}

void Metric::ThreadAccumulatorRef::add(double v) noexcept {
  if(v != 0) atomic_add(*exclusive, v);
}

Metric::ThreadAccumulatorRef Metric::getFor(Thread::Temporary& t, Context& c) noexcept {
  auto& td = t.data[tmember];
  return {td.exclusive[&c], td.inclusive[&c]};
}

static stdshim::optional<double> opt0(double d) {
  return d == 0 ? stdshim::optional<double>{} : d;
}

stdshim::optional<double> Metric::CAccumulatorRef::get(Metric::Scope s) const noexcept {
  if(accum == nullptr) return {};
  switch(s) {
  case Scope::point: util::log::fatal{} << "TODO: Support point Metric::Scope!";
  case Scope::exclusive: return opt0(accum->exclusive.load(std::memory_order_relaxed));
  case Scope::inclusive: return opt0(accum->inclusive.load(std::memory_order_relaxed));
  default: util::log::fatal{} << "Invalid Scope value!";
  };
  std::abort();  // unreachable
}

Metric::CAccumulatorRef Metric::cgetFor(const Context& c) const noexcept {
  auto* a = c.data.find(member);
  if(a == nullptr) return {};
  return *a;
}

stdshim::optional<double> Metric::CThreadAccumulatorRef::get(Metric::Scope s) const noexcept {
  switch(s) {
  case Scope::point: util::log::fatal{} << "TODO: Support point Metric::Scope!";
  case Scope::exclusive:
    return exclusive != nullptr ? opt0(exclusive->load(std::memory_order_relaxed))
                                : stdshim::optional<double>{};
  case Scope::inclusive:
    return inclusive != nullptr ? opt0(*inclusive) : stdshim::optional<double>{};
  default: util::log::fatal{} << "Invalid Scope value!";
  }
  std::abort();  // unreachable
}

Metric::CThreadAccumulatorRef Metric::cgetFor(const Thread::Temporary& t, const Context& c) const noexcept {
  auto* td = t.data.find(tmember);
  if(td == nullptr) return {};
  auto iit = td->inclusive.find(const_cast<Context*>(&c));
  if(iit == td->inclusive.end()) return {};
  auto* e = td->exclusive.find(const_cast<Context*>(&c));
  if(e) return {*e, iit->second};
  return {nullptr, iit->second};
}

void Metric::finalize(Thread::Temporary& t) noexcept {
  auto* local_p = t.data.find(tmember);
  if(!local_p) return;  // We have no data for this.
  auto& local = *local_p;

  // For each Context, we need to know what its children are. But we only care
  // about ones that have decendants with exclusive data.
  // So we take a pre-processing step to build a subtree map.
  Context* global = nullptr;
  std::unordered_map<Context*, std::vector<Context*>> childMap;
  std::vector<Context*> newContexts;
  for(const auto& cei: local.exclusive.citerate())
    newContexts.emplace_back(cei.first);
  while(!newContexts.empty()) {
    std::vector<Context*> nextNew;
    for(Context* cp: newContexts) {
      if(!cp->direct_parent()) {
        if(global) util::log::fatal() << "Multiple root Contexts???";
        global = cp;
        continue;
      }
      auto x = childMap.emplace(cp->direct_parent(), 0);
      if(x.second) nextNew.emplace_back(cp->direct_parent());
      x.first->second.emplace_back(cp);
    }
    newContexts = std::move(nextNew);
  }

  // Now that we have the map, recursively propagate up, and accumulate.
  std::function<double(Context*)> propagate = [&](Context* c) -> double{
    auto* exc = local.exclusive.find(c);
    auto& inc = local.inclusive[c];
    inc = exc ? exc->load(std::memory_order_relaxed) : 0;
    auto it = childMap.find(c);
    if(it != childMap.end()) {
      for(Context* cc: it->second) {
        if(pullsExclusive(*c)) {
          if(!exc) exc = &local.exclusive[c];
          auto* excc = local.exclusive.find(cc);
          if(excc) atomic_add(*exc, excc->load(std::memory_order_relaxed));
        }
        inc += propagate(cc);
      }
    }
    auto& accum = c->data[member];
    if(exc) atomic_add(accum.exclusive, exc->load(std::memory_order_relaxed));
    atomic_add(accum.inclusive, inc);
    return inc;
  };
  propagate(global);
}

std::size_t std::hash<Metric::Settings>::operator()(const Metric::Settings &s) const noexcept {
  const auto h1 = std::hash<std::string>{}(s.name);
  const auto h2 = std::hash<std::string>{}(s.description);
  return h1 ^ ((h2 << 1) | (h2 >> (-1 + 8 * sizeof h2)));
}

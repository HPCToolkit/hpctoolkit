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

void Metric::add(Context& c, std::pair<double, double> ei) const noexcept {
  if(ei.first == 0 && ei.second == 0) return;
  auto& accum = c.data[member];
  if(ei.first != 0) atomic_add(accum.exclusive, ei.first);
  if(ei.second != 0) atomic_add(accum.inclusive, ei.second);
}

void Metric::add(Thread::Temporary& t, Context& c, double v) const noexcept {
  if(type == Type::artifical)
    util::log::fatal{} << "Attempt to emit data for an artifical Metric!";
  if(v != 0) atomic_add(t.data[tmember].exclusive[&c], v);
}

std::pair<double, double> Metric::getFor(const Context& c) const noexcept {
  std::pair<double, double> x{0, 0};
  auto* a = c.data.find(member);
  if(a) {
    x.first = a->exclusive.load(std::memory_order_relaxed);
    x.second = a->inclusive.load(std::memory_order_relaxed);
  }
  return x;
}

std::pair<double, double> Metric::getFor(const Thread::Temporary& t, const Context& c) const noexcept {
  std::pair<double, double> ei{0, 0};
  auto* local = t.data.find(tmember);
  if(local) {
    auto* exc = local->exclusive.find(const_cast<Context*>(&c));
    if(exc) ei.first = exc->load(std::memory_order_relaxed);

    auto inc = local->inclusive.find(const_cast<Context*>(&c));
    if(inc != local->inclusive.end()) ei.second = inc->second;
  }
  return ei;
}

void Metric::finalize(Thread::Temporary& t) noexcept {
  // Artifical metrics go directly into the tree, so we don't need anything.
  if(type == Type::artifical) return;

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

std::size_t std::hash<Metric::ident>::operator()(const Metric::ident &m) const noexcept {
  const auto h1 = std::hash<std::string>{}(m.name);
  const auto h2 = std::hash<std::string>{}(m.description);
  return h1 ^ ((h2 << 1) | (h2 >> (-1 + 8 * sizeof h2)));
}

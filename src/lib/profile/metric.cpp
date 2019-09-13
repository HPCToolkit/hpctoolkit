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

MetricDatum& MetricDatum::operator+=(const double& v) noexcept {
  atomic_add(v_int_ex, v);
  operator<<=(v);
  return *this;
}

MetricDatum& MetricDatum::operator<<=(const double& v) noexcept {
  atomic_add(v_int_in, v);
  return *this;
}

MetricDatum& MetricDatum::operator+=(const MetricDatum& o) noexcept {
  atomic_add(v_int_ex, o.v_int_ex.load(std::memory_order_relaxed));
  atomic_add(v_int_in, o.v_int_in.load(std::memory_order_relaxed));
  return *this;
}

void Metric::add(Context& c, double v) const noexcept {
  if(v == 0) return;
  // This node and maybe its parent get exclusive.
  atomic_add(c.data[member].exclusive, v);
  if(c.direct_parent()) {
    auto& p = *c.direct_parent();
    switch(p.scope().type()) {
    case Scope::Type::function:
    case Scope::Type::inlined_function:
    case Scope::Type::loop:
      atomic_add(p.data[member].exclusive, v);
      break;
    case Scope::Type::unknown:
    case Scope::Type::point:
    case Scope::Type::global:
      break;
    }
  }
  // The inclusive runs up the tree to all the ancestors.
  double vsum = v;
  Context* cp = &c;
  auto* d = &c.data[member];
  while(cp != nullptr) {
    if(atomic_add(d->shared, vsum) != 0) break;  // No need for us anymore
    // Wait for any others to add their bits, and prep the next iteration.
    cp = cp->direct_parent();
    if(cp) d = &cp->data[member];
    std::this_thread::yield();
    // Exchange and propegate the shared sum
    vsum = d->shared.exchange(0, std::memory_order_relaxed);
    atomic_add(d->inclusive, vsum);
  }
}

std::pair<double, double> Metric::getFor(const Context& c) const noexcept {
  auto* d = c.data.find(member);
  if(!d) return {0, 0};
  return {d->exclusive.load(std::memory_order_relaxed),
          d->inclusive.load(std::memory_order_relaxed)};
}

std::size_t std::hash<Metric::ident>::operator()(const Metric::ident &m) const noexcept {
  const auto h1 = std::hash<std::string>{}(m.name);
  const auto h2 = std::hash<std::string>{}(m.description);
  return h1 ^ ((h2 << 1) | (h2 >> (-1 + 8 * sizeof h2)));
}

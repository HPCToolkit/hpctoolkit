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

#include "accumulators.hpp"

#include "context.hpp"
#include "metric.hpp"

#include <yaml-cpp/yaml.h>

#include <ostream>

using namespace hpctoolkit;

std::ostream& hpctoolkit::operator<<(std::ostream& os, MetricScope ms) {
  switch(ms) {
  case MetricScope::point: return os << "point";
  case MetricScope::function: return os << "function";
  case MetricScope::execution: return os << "execution";
  }
  std::abort();
}
YAML::Emitter& hpctoolkit::operator<<(YAML::Emitter& e, MetricScope ms) {
  switch(ms) {
  case MetricScope::point: return e << "point";
  case MetricScope::function: return e << "function";
  case MetricScope::execution: return e << "execution";
  }
  std::abort();
}

static double atomic_add(std::atomic<double>& a, const double v) noexcept {
  double old = a.load(std::memory_order_relaxed);
  while(!a.compare_exchange_weak(old, old+v, std::memory_order_relaxed));
  return old;
}

static double atomic_op(std::atomic<double>& a, const double v, Statistic::combination_t op) noexcept {
  double old = a.load(std::memory_order_relaxed);
  switch(op) {
  case Statistic::combination_t::sum:
    while(!a.compare_exchange_weak(old, old+v, std::memory_order_relaxed));
    break;
  case Statistic::combination_t::min:
    while((v < old || old == 0) && !a.compare_exchange_weak(old, v, std::memory_order_relaxed));
    break;
  case Statistic::combination_t::max:
    while((v > old || old == 0) && !a.compare_exchange_weak(old, v, std::memory_order_relaxed));
    break;
  }
  return old;
}

StatisticAccumulator::StatisticAccumulator(const Metric& m)
  : partials(m.partials().size()) {};

void StatisticAccumulator::PartialRef::add(MetricScope s, double v) noexcept {
  assert(v > 0 && "Attempt to add 0 value to a Partial!");
  switch(s) {
  case MetricScope::point: atomic_op(partial.point, v, statpart.combinator()); return;
  case MetricScope::function: atomic_op(partial.function, v, statpart.combinator()); return;
  case MetricScope::execution: atomic_op(partial.execution, v, statpart.combinator()); return;
  }
  assert(false && "Invalid MetricScope!");
  std::abort();
}

StatisticAccumulator::PartialCRef StatisticAccumulator::get(const StatisticPartial& p) const noexcept {
  return {partials[p.m_idx], p};
}
StatisticAccumulator::PartialRef StatisticAccumulator::get(const StatisticPartial& p) noexcept {
  return {partials[p.m_idx], p};
}

void MetricAccumulator::add(double v) noexcept {
  if(v == 0) util::log::warning{} << "Adding a 0-metric value!";
  atomic_add(point, v);
}

static std::optional<double> opt0(double d) {
  return d == 0 ? std::optional<double>{} : d;
}

std::optional<double> StatisticAccumulator::PartialRef::get(MetricScope s) const noexcept {
  partial.validate();
  switch(s) {
  case MetricScope::point: return opt0(partial.point.load(std::memory_order_relaxed));
  case MetricScope::function: return opt0(partial.function.load(std::memory_order_relaxed));
  case MetricScope::execution: return opt0(partial.execution.load(std::memory_order_relaxed));
  };
  assert(false && "Invalid MetricScope!");
  std::abort();
}
std::optional<double> StatisticAccumulator::PartialCRef::get(MetricScope s) const noexcept {
  partial.validate();
  switch(s) {
  case MetricScope::point: return opt0(partial.point.load(std::memory_order_relaxed));
  case MetricScope::function: return opt0(partial.function.load(std::memory_order_relaxed));
  case MetricScope::execution: return opt0(partial.execution.load(std::memory_order_relaxed));
  };
  assert(false && "Invalid MetricScope!");
  std::abort();
}

void StatisticAccumulator::Partial::validate() const noexcept {
  if(point.load(std::memory_order_relaxed) != 0) return;
  if(function.load(std::memory_order_relaxed) != 0) return;
  if(execution.load(std::memory_order_relaxed) != 0) return;
  util::log::warning{} << "Returning a Statistic accumulator with no value!";
}

const StatisticAccumulator* Metric::getFor(const Context& c) const noexcept {
  return c.data.find(*this);
}

std::optional<double> MetricAccumulator::get(MetricScope s) const noexcept {
  validate();
  switch(s) {
  case MetricScope::point: return opt0(point.load(std::memory_order_relaxed));
  case MetricScope::function: return opt0(function);
  case MetricScope::execution: return opt0(execution);
  }
  assert(false && "Invalid MetricScope!");
  std::abort();
}

void MetricAccumulator::validate() const noexcept {
  if(point.load(std::memory_order_relaxed) != 0) return;
  if(function != 0) return;
  if(execution != 0) return;
  util::log::warning{} << "Returning a Metric accumulator with no value!";
}

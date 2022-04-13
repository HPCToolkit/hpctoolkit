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

#include "accumulators.hpp"

#include "context.hpp"
#include "metric.hpp"

#include <ostream>
#include <stack>

using namespace hpctoolkit;

static const std::string ms_point = "point";
static const std::string ms_function = "function";
static const std::string ms_execution = "execution";

const std::string& hpctoolkit::stringify(MetricScope ms) {
  switch(ms) {
  case MetricScope::point: return ms_point;
  case MetricScope::function: return ms_function;
  case MetricScope::execution: return ms_execution;
  }
  std::abort();
}
std::ostream& hpctoolkit::operator<<(std::ostream& os, MetricScope ms) {
  return os << stringify(ms);
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
  assert((point.load(std::memory_order_relaxed) != 0
          || function.load(std::memory_order_relaxed) != 0
          || execution.load(std::memory_order_relaxed) != 0)
    && "Attempt to access a StatisticAccumulator with 0 value!");
}

util::optional_ref<const StatisticAccumulator> Metric::getFor(const Context& c) const noexcept {
  return c.data().m_statistics.find(*this);
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
  assert((point.load(std::memory_order_relaxed) != 0
          || function != 0 || execution != 0)
    && "Attempt to access a MetricAccumulator with 0 value!");
}

MetricScopeSet MetricAccumulator::getNonZero() const noexcept {
  MetricScopeSet out;
  if(point.load(std::memory_order_relaxed) != 0) out |= MetricScope::point;
  if(function != 0) out |= MetricScope::function;
  if(execution != 0) out |= MetricScope::execution;
  return out;
}

StatisticAccumulator& PerContextAccumulators::statisticsFor(const Metric& m) noexcept {
  return m_statistics.emplace(std::piecewise_construct,
                              std::forward_as_tuple(m),
                              std::forward_as_tuple(m)).first;
}

void PerContextAccumulators::markUsed(const Metric& m, MetricScopeSet ms) noexcept {
  m_metricUsage[m] |= ms & m.scopes();
}

void PerThreadTemporary::finalize() noexcept {
  // Before doing anything else, we need to redistribute the metric values
  // attributed to Reconstructions and FlowGraphs within this Thread.
  {
    std::unordered_map<util::reference_index<const Context>,
      std::unordered_map<util::reference_index<const Metric>, double>> outputs;
    const auto add = [&](const Context& c, const Metric& m, double v) {
      if(v == 0) return;
      auto [it, first] = outputs[c].try_emplace(m, v);
      if(!first) it->second += v;
    };

    // First redistrubute the Reconstructions, since those are a bit easier.
    for(const auto& [r, input]: r_data.citerate()) {
      auto [factors, hasEC] = r->rescalingFactors(c_data);
      {
        auto inFs = r->interiorFactors(r_data, hasEC);
        assert(factors.size() == inFs.size());
        std::transform(factors.begin(), factors.end(), inFs.cbegin(),
                       factors.begin(), std::multiplies<double>{});
      }

      // Re-attribute the values back to the "final" Contexts.
      const auto& finals = r->m_finals;
      assert(factors.size() == finals.size());
      for(const auto& [m, va]: input.citerate()) {
        if(auto v = va.get(MetricScope::point)) {
          auto handling = r->graph().handler()(m);
          for(size_t i = 0; i < finals.size(); i++)
            add(finals[i], m, factors[i] * *v);
        }
      }
    }
    r_data.clear();  // Free some memory early

    // For the rescaling factors we need the summed call counts from all the
    // reconstruction groups. Sum them up here.
    // While we're here, also fold them into the proper Context metric values.
    std::unordered_map<util::reference_index<const Context>,
      std::unordered_map<util::reference_index<const Metric>, double>> r_sums;
    for(auto& [idx, group]: r_groups.iterate()) {
      for(const auto& [c, input]: group.c_data.citerate()) {
        auto& data = c_data[c];
        for(const auto& [m, va]: input.citerate()) {
          if(auto v = va.get(MetricScope::point)) {
            auto [it, first] = r_sums[c].try_emplace(m, *v);
            if(!first) it->second += *v;

            data[m].add(*v);
          }
        }
      }
    }

    // Cache the rescaling factors here, to keep from recalculating them.
    std::unordered_map<util::reference_index<const ContextReconstruction>,
      std::vector<double>> r_rescalingFactors;
    const auto rescalingFactors = [&](const ContextReconstruction& r) -> const std::vector<double>& {
      auto [it, first] = r_rescalingFactors.try_emplace(r);
      if(first) it->second = r.rescalingFactors(r_sums);
      return it->second;
    };

    // Now process the FlowGraphs, per-reconstruction group.
    for(auto& [idx, group]: r_groups.iterate()) {
      for(const auto& [fg_c, input]: group.fg_data.citerate()) {
        assert(!input.empty());
        auto& fg = const_cast<ContextFlowGraph&>(fg_c.get());
        const auto& reconsts = group.fg_reconsts.at(fg);
        // If there are no Reconstructions in this group, there must be a bug in
        // hpcrun with range-association. We can't do anything with this data so
        // we just drop it.
        //
        // FIXME: We should throw an ERROR when this happens, but it's a known
        // bug with NVIDIA's code and we don't want to cause undue noise. So for
        // now we skip silently.
        if(reconsts.empty()) continue;

        auto [exFactors, hasEC] = fg.exteriorFactors(reconsts, group.c_data);

        auto inFs = fg.interiorFactors(group.fg_data, std::move(hasEC));
        for(auto& [r, factors]: std::move(exFactors)) {
          assert(factors.size() == inFs.size());
          std::transform(factors.begin(), factors.end(), inFs.cbegin(),
                         factors.begin(), std::multiplies<double>{});

          const auto& rsFs = rescalingFactors(r);
          assert(factors.size() == rsFs.size());
          std::transform(factors.begin(), factors.end(), rsFs.cbegin(),
                         factors.begin(), std::multiplies<double>{});

          // Re-attribute the values back to the "final" Contexts.
          const auto& finals = r->m_finals;
          assert(factors.size() == finals.size());
          for(const auto& [m, va]: input.citerate()) {
            if(auto v = va.get(MetricScope::point)) {
              auto handling = fg.handler()(m);
              for(size_t i = 0; i < finals.size(); i++)
                add(finals[i], m, factors[i] * *v);
            }
          }
          factors.clear();  // Free some memory early
        }
      }

      // Free up the memory within the reconstruction group
      group.c_data.clear();
      group.fg_data.clear();
      group.c_entries.clear();
      group.fg_reconsts.clear();
    }
    r_groups.clear();

    // Fold the redistributed values back into the larger Context tree data
    for(const auto& cvs: outputs) {
      auto& data = c_data[cvs.first];
      for(const auto& mv: cvs.second) {
        data[mv.first].add(mv.second);
      }
    }
  }

  // For each Context we need to know what its children are. But we only care
  // about ones that have descendants with actual data. So we construct a
  // temporary subtree with all the bits.
  util::optional_ref<const Context> global;
  std::unordered_map<util::reference_index<const Context>,
    std::unordered_set<util::reference_index<const Context>>> children;
  for(const auto& cx: c_data.citerate()) {
    std::reference_wrapper<const Context> c = cx.first;
    while(auto p = c.get().direct_parent()) {
      auto x = children.insert({*p, {}});
      x.first->second.emplace(c.get());
      if(!x.second) break;
      c = *p;
    }
    if(!c.get().direct_parent()) {
      assert((!global || global == &c.get()) && "Multiple root contexts???");
      assert(c.get().scope().flat().type() == Scope::Type::global && "Root context without (global) Scope!");
      global = c.get();
    }
  }
  if(!global) return;  // Apparently there's nothing to propagate

  // Now that the critical subtree is built, recursively propagate up.
  using md_t = decltype(c_data)::mapped_type;
  struct frame_t {
    frame_t(const Context& c) : ctx(c) {};
    frame_t(const Context& c, const decltype(children)::mapped_type& v)
      : ctx(c), here(v.cbegin()), end(v.cend()) {};
    const Context& ctx;
    decltype(children)::mapped_type::const_iterator here;
    decltype(children)::mapped_type::const_iterator end;
    std::vector<std::pair<std::reference_wrapper<const Context>,
                          std::reference_wrapper<const md_t>>> submds;
  };
  std::stack<frame_t, std::vector<frame_t>> stack;

  // Post-order in-memory tree traversal
  {
    auto git = children.find(*global);
    if(git == children.end()) stack.emplace(*global);
    else stack.emplace(*global, git->second);
  }
  while(!stack.empty()) {
    if(stack.top().here != stack.top().end) {
      // This frame still has children to handle
      const Context& c = *stack.top().here;
      auto ccit = children.find(c);
      ++stack.top().here;
      if(ccit == children.end()) stack.emplace(c);
      else stack.emplace(c, ccit->second);
      continue;  // We'll come back eventually
    }

    const Context& c = stack.top().ctx;
    md_t& data = c_data[c];
    // Handle the internal propagation first, so we don't get mixed up.
    for(auto& mx: data.iterate()) {
      mx.second.execution = mx.second.function
                          = mx.second.point.load(std::memory_order_relaxed);
    }

    // Go through our children and sum into our bits
    for(std::size_t i = 0; i < stack.top().submds.size(); i++) {
      const auto& sub = stack.top().submds[i];
      const bool pullfunc = !isCall(sub.first.get().scope().relation());
      for(const auto& mx: sub.second.get().citerate()) {
        auto& accum = data[mx.first];
        if(pullfunc) accum.function += mx.second.function;
        accum.execution += mx.second.execution;
      }
    }

    // Now that our bits are stable, accumulate back into the per-Context data
    auto& cdata = const_cast<Context&>(c).m_data.m_statistics;
    auto& musage = const_cast<Context&>(c).m_data.m_metricUsage;
    for(const auto& mx: data.citerate()) {
      musage[mx.first] |= mx.second.getNonZero() & mx.first->scopes();
      auto& accum = cdata.emplace(std::piecewise_construct,
        std::forward_as_tuple(mx.first), std::forward_as_tuple(mx.first)).first;
      for(size_t i = 0; i < mx.first->partials().size(); i++) {
        auto& partial = mx.first->partials()[i];
        auto& atomics = accum.partials[i];
        atomic_op(atomics.point, partial.m_accum.evaluate(mx.second.point.load(std::memory_order_relaxed)), partial.combinator());
        atomic_op(atomics.function, partial.m_accum.evaluate(mx.second.function), partial.combinator());
        atomic_op(atomics.execution, partial.m_accum.evaluate(mx.second.execution), partial.combinator());
      }
    }

    stack.pop();
    if(!stack.empty()) stack.top().submds.emplace_back(c, data);
  }
}

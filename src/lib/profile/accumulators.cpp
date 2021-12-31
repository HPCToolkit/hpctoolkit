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

#include <stack>

using namespace hpctoolkit;

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

static bool pullsFunction(Scope parent, Scope child) {
  switch(child.type()) {
  // Function-type Scopes, placeholder and unknown (which could be a call)
  case Scope::Type::function:
  case Scope::Type::inlined_function:
  case Scope::Type::unknown:
  case Scope::Type::placeholder:
    return false;
  case Scope::Type::point:
  case Scope::Type::loop:
  case Scope::Type::line:
    switch(parent.type()) {
    // Function-type scopes, and unknown (which could be a call)
    case Scope::Type::unknown:
    case Scope::Type::function:
    case Scope::Type::inlined_function:
    case Scope::Type::loop:
    case Scope::Type::line:
      return true;
    case Scope::Type::global:
    case Scope::Type::point:
    case Scope::Type::placeholder:
      return false;
    }
    break;
  case Scope::Type::global:
    assert(false && "global Context should have no parent!");
    std::abort();
  }
  assert(false && "Unhandled Scope type!");
  std::abort();
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
    for(const auto& [r, input]: r_data.citerate()) {
      auto factors = r->interiorFactors(r_data);
      const auto& finals = r->m_finals;
      assert(factors.size() == finals.size());
      {
        auto rsFs = r->rescalingFactors(c_data);
        assert(factors.size() == rsFs.size());
        std::transform(factors.begin(), factors.end(), rsFs.begin(),
                       factors.begin(), std::multiplies<double>{});
      }
      for(const auto& [m, va]: input.citerate()) {
        if(auto v = va.get(MetricScope::point)) {
          auto handling = r->graph().handler()(m);
          for(size_t i = 0; i < finals.size(); i++)
            add(finals[i], m, factors[i] * *v);
        }
      }
    }
    for(auto& [idx, group]: r_groups.iterate()) {
      for(const auto& [c, input]: group.c_data.citerate()) {
        for(const auto& [m, va]: input.citerate()) {
          if(auto v = va.get(MetricScope::point))
            add(c, m, *v);
        }
      }
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

        auto inFactors = fg.interiorFactors(group.fg_data);
        for(auto& [r, factors]: fg.exteriorFactors(reconsts, group.c_data)) {
          const auto& finals = r->m_finals;
          assert(finals.size() == factors.size() && finals.size() == inFactors.size());
          std::transform(factors.begin(), factors.end(), inFactors.begin(),
                         factors.begin(), std::multiplies<double>{});
          {
            auto rsFs = r->rescalingFactors(group.c_data);
            assert(factors.size() == rsFs.size());
            std::transform(factors.begin(), factors.end(), rsFs.begin(),
                           factors.begin(), std::multiplies<double>{});
          }
          for(const auto& [m, va]: input.citerate()) {
            if(auto v = va.get(MetricScope::point)) {
              auto handling = fg.handler()(m);
              for(size_t i = 0; i < finals.size(); i++)
                add(finals[i], m, factors[i] * *v);
            }
          }
          factors.clear();
        }
      }
      group.c_data.clear();
      group.fg_data.clear();
      group.c_entries.clear();
      group.fg_reconsts.clear();
    }
    r_data.clear();
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
      assert(c.get().scope().type() == Scope::Type::global && "Root context without (global) Scope!");
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
      const Context& cc = stack.top().submds[i].first;
      const md_t& ccmd = stack.top().submds[i].second;
      const bool pullfunc = pullsFunction(c.scope(), cc.scope());
      for(const auto& mx: ccmd.citerate()) {
        auto& accum = data[mx.first];
        if(pullfunc) accum.function += mx.second.function;
        accum.execution += mx.second.execution;
      }
    }

    // Now that our bits are stable, accumulate back into the Statistics
    auto& cdata = const_cast<Context&>(c).m_data.stats;
    for(const auto& mx: data.citerate()) {
      auto& accum = cdata.emplace(std::piecewise_construct,
        std::forward_as_tuple(mx.first), std::forward_as_tuple(mx.first)).first;
      for(size_t i = 0; i < mx.first->partials().size(); i++) {
        auto& partial = mx.first->partials()[i];
        auto& atomics = accum.partials[i];
        atomic_op(atomics.point, partial.m_accum(mx.second.point.load(std::memory_order_relaxed)), partial.combinator());
        atomic_op(atomics.function, partial.m_accum(mx.second.function), partial.combinator());
        atomic_op(atomics.execution, partial.m_accum(mx.second.execution), partial.combinator());
      }
    }

    stack.pop();
    if(!stack.empty()) stack.top().submds.emplace_back(c, data);
  }
}

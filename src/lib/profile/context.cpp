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

#include "context.hpp"

#include "module.hpp"
#include "metric.hpp"
#include "lexical.hpp"

#include <cassert>
#include <stack>
#include <stdexcept>
#include <vector>

using namespace hpctoolkit;

//
// Context
//

Context::Context(ud_t::struct_t& rs, util::optional_ref<Context> p, Scope s)
  : userdata(rs, std::ref(*this)), children_p(new children_t()),
    reconsts_p(new reconsts_t()), m_parent(p), u_scope(s) {};
Context::Context(Context&& c)
  : userdata(std::move(c.userdata), std::ref(*this)),
    children_p(new children_t()), reconsts_p(new reconsts_t()),
    m_parent(c.m_parent), u_scope(c.u_scope) {};

Context::~Context() noexcept {
  // C++ generates a recursive algorithm for this by default
  // So we replace it with a post-order tree traversal
  try {
    iterate(nullptr, [](Context& c) { c.children_p.reset(nullptr); });
  } catch(...) {};  // If we run into errors, just let the recursion handle it.
}

void Context::iterate(const std::function<void(Context&)>& pre,
                      const std::function<void(Context&)>& post) {
  struct frame_t {
    frame_t(Context& c)
      : ctx(c), iter(c.children_p->iterate()),
        here(iter.begin()), end(iter.end()) {};
    Context& ctx;
    using iter_t = decltype(children_p->iterate());
    iter_t iter;
    decltype(iter.begin()) here;
    decltype(iter.end()) end;
  };
  std::stack<frame_t, std::vector<frame_t>> stack;
  if(pre) pre(*this);
  if(children_p) stack.emplace(*this);
  while(!stack.empty()) {
    if(stack.top().here != stack.top().end) {
      // We have more children to handle
      Context& c = *stack.top().here++;
      if(pre) pre(c);
      if(c.children_p) stack.emplace(c);
      continue;
    }

    Context& c = stack.top().ctx;
    stack.pop();
    if(post) post(c);
  }
}

void Context::citerate(const std::function<void(const Context&)>& pre,
                       const std::function<void(const Context&)>& post) const {
  struct frame_t {
    frame_t(const Context& c)
      : ctx(c), iter(c.children_p->citerate()),
        here(iter.begin()), end(iter.end()) {};
    const Context& ctx;
    using iter_t = decltype(children_p->citerate());
    iter_t iter;
    decltype(iter.begin()) here;
    decltype(iter.end()) end;
  };
  std::stack<frame_t, std::vector<frame_t>> stack;
  if(pre) pre(*this);
  if(children_p) stack.emplace(*this);
  while(!stack.empty()) {
    if(stack.top().here != stack.top().end) {
      // We have more children to handle
      const Context& c = *stack.top().here++;
      if(pre) pre(c);
      if(c.children_p) stack.emplace(c);
      continue;
    }

    const Context& c = stack.top().ctx;
    stack.pop();
    if(post) post(c);
  }
}

std::pair<Context&,bool> Context::ensure(Scope s) {
  auto x = children_p->emplace(userdata.base(), *this, std::move(s));
  return {x.first(), x.second};
}

//
// Shared private factor calculation pieces
//

using mvals_t = util::locked_unordered_map<util::reference_index<const Metric>,
                                           MetricAccumulator>;
template<class Ctx>
using perctx_mvals_t = util::locked_unordered_map<
    util::reference_index<const Ctx>, mvals_t>;

namespace {

class Shared {
public:
  void insert(const Metric& m, double v, const ContextFlowGraph& graph) {
    if(queried.insert(m).second) {
      auto handling = graph.handler()(m);
      if(handling.exteriorLogical) {
        if(metric_exteriorLogical) {
          // The handler claims multiple Metrics are usable for exterior
          // factor calculations. We don't know which to pick, so abort.
          util::log::fatal{} << "Multiple suitable Metrics for exterior (logical) factor calculations: "
            << metric_exteriorLogical->name() << " vs. " << m.name();
        }
        metric_exteriorLogical = m;
        exteriorLogicalIsAlsoExterior = handling.exterior;
      } else if(handling.exterior) {
        if(metric_exterior) {
          // The handler claims multiple Metrics are usable for exterior
          // factor calculations. We don't know which to pick, so abort.
          util::log::fatal{} << "Multiple suitable Metrics for exterior factor calculations: "
            << metric_exterior->name() << " vs. " << m.name();
        }
        metric_exterior = m;
      }
      if(handling.interior) {
        if(metric_interior) {
          // The handler claims multiple Metrics are usable for interior
          // factor calculations. We don't know which to pick, so abort.
          util::log::fatal{} << "Multiple suitable Metrics for interior factor calculations: "
            << metric_interior->name() << " vs. " << m.name();
        }
        metric_interior = m;
      }
    }
    if(metric_exterior == &m)
      total_val_exterior += v;
    else if(metric_exteriorLogical == &m)
      total_val_exteriorLogical += v;
  }

  void insert(const std::pair<const util::reference_index<const Metric>,
                              MetricAccumulator>& mv,
              const ContextFlowGraph& graph) {
    return insert(mv.first, mv.second.get(MetricScope::point).value_or(0), graph);
  }

  util::optional_ref<const Metric> exteriorLogical() const {
    util::optional_ref<const Metric> res = metric_exteriorLogical
        ? metric_exteriorLogical : metric_exterior;
#ifndef NDEBUG
    // Check that the answer is consistent
    if(!exteriorLogicalObserved) exteriorLogicalObserved = res;
    else assert(exteriorLogicalObserved == res);
#endif
    return res;
  }
  util::optional_ref<const Metric> exterior() const {
    util::optional_ref<const Metric> res = metric_exterior ? metric_exterior
        : exteriorLogicalIsAlsoExterior ? metric_exteriorLogical : std::nullopt;
#ifndef NDEBUG
    // Check that the answer is consistent
    if(!exteriorObserved) exteriorObserved = res;
    else assert(exteriorObserved == res);
#endif
    return res;
  }
  double total_exterior() const {
    return metric_exterior ? total_val_exterior
           : exteriorLogicalIsAlsoExterior ? total_val_exteriorLogical
           : 0;
  }

  util::optional_ref<const Metric> interior() const { return metric_interior; }

private:
  std::unordered_set<util::reference_index<const Metric>> queried;
  util::optional_ref<const Metric> metric_exterior;
  util::optional_ref<const Metric> metric_exteriorLogical;
  bool exteriorLogicalIsAlsoExterior = false;
  double total_val_exterior = 0;
  double total_val_exteriorLogical = 0;
  util::optional_ref<const Metric> metric_interior;

#ifndef NDEBUG
  mutable util::optional_ref<const Metric> exteriorLogicalObserved;
  mutable util::optional_ref<const Metric> exteriorObserved;
#endif
};

class ExteriorFactors {
public:
  bool skip(const ContextFlowGraph::Template& t, const Shared&) {
    return entry_vals.find(t.entry()) != entry_vals.end();
  }
  void insert(const Scope& entry,
              const std::pair<const util::reference_index<const Metric>,
                              MetricAccumulator>& mv,
              const Shared& shared) {
    if(shared.exterior() == &mv.first.get()) {
      if(auto vv = mv.second.get(MetricScope::point)) {
        entry_vals.emplace(entry, *vv);
      }
    }
  }
  bool empty() { return entry_vals.empty(); }
  std::vector<double> extract(const ContextFlowGraph& graph,
                              const Shared& shared) const {
    if(!shared.exterior()) {
      // We never saw the right Metric for this formulation. Unclear how to
      // recover in this case, so for now just abort.
      util::log::fatal{} << "No suitable Metrics for exterior factor calculations!";
    }
    assert(shared.total_exterior() != 0 && !entry_vals.empty());
    const auto& templates = graph.templates();
    // The exterior factor for an entry e is calculated as
    //   factor = weight(e) / sum(weight(e') for e' in {t.entry for i in templates})
    // In short, the numerator is entry_vals[e] and the denominator shared.total_entry().
    //
    // The return from this function is based on the templates, so we need to
    // inflate the entry_vals map. To save a loop do the division while inflating.
    std::vector<double> t_factors;
    t_factors.reserve(templates.size());
    for(const auto& t: templates) {
      auto it = entry_vals.find(t.entry());
      t_factors.push_back(it == entry_vals.end() ? (double)0
                          : it->second / shared.total_exterior());
    }
    return t_factors;
  }

private:
  std::unordered_map<Scope, double> entry_vals;
};

class InteriorFactors {
public:
  bool skip(const Scope& p, const Shared&) {
    return top_vals.find(p) != top_vals.end();
  }
  void insert(const Scope& p, const Metric& m, double v,
              const Shared& shared) {
    if(shared.interior() == &m && v != 0) {
      top_vals.emplace(p, v);
    }
  }
  std::vector<double> extract(const ContextFlowGraph& graph,
                              const Shared& shared) const {
    const auto& templates = graph.templates();

    // The interior factor for a Scope-path p = [p[0],...,p[n]] and entry e is calculated as
    //   factor = 1
    //   group = {t.path for t in templates if t.entry == e}
    //   for i in range(n):
    //     factor *= weight(p[i]) / sum(weight(s') for s' in {p'[i] for p' in group})
    //     group = {p' in group if p'[i] == p[i]}
    // The idea is that the "full" factor (1) is divided between diverging paths
    // based on the input metric values, working from the "entry" to the "exit"
    // of the possible path(s).
    //
    // To make things simple, the Templates are already sorted by entry and path,
    // so the groups will always be contiguous. Initial groups are 1 for each
    // entry, with a factor of 1 since we don't have exterior factors here.
    std::vector<double> factors(templates.size(), 0);
    struct group_t {
      std::deque<ContextFlowGraph::Template>::const_iterator begin;
      std::deque<ContextFlowGraph::Template>::const_iterator end;
      double factor;
    };
    std::deque<group_t> groups;
    {
      auto beg = templates.begin();
      for(auto it = beg; it != templates.end(); ++it) {
        if(it->entry() != beg->entry()) {
          groups.push_back({beg, it, 1});
          beg = it;
        }
      }
      groups.push_back({beg, templates.end(), 1});
    }
    for(size_t i = 0; !groups.empty(); i++) {
      std::deque<group_t> next_groups;
      // First pass: split groups into subgroups where the paths diverge
      for(auto& g: groups) {
        size_t next_start = next_groups.size();
        // Split the group into subgroups based on the diverging paths
        double total_v = 0;
        auto beg = g.begin;
        for(auto it = beg; it != g.end; ++it) {
          assert(it->path().size() > i);
          if(beg->path()[i] != it->path()[i]) {
            auto vit = top_vals.find(beg->path()[i]);
            double v = vit == top_vals.end() ? (double)0 : vit->second;
            next_groups.push_back({beg, it, v});
            total_v += v;
            beg = it;
          }
        }
        auto vit = top_vals.find(beg->path()[i]);
        double v = vit == top_vals.end() ? (double)0 : vit->second;
        next_groups.push_back({beg, g.end, v});
        total_v += v;
        // Then go back and fixup the factors based on the division.
        if(total_v > 0) {
          for(size_t i = next_start, e = next_groups.size(); i < e; ++i)
            next_groups[i].factor = g.factor * next_groups[i].factor / total_v;
        } else {
          // Special case: if there is no metric value at this split, divide evenly
          const double cnt = next_groups.size() - next_start;
          for(size_t i = next_start, e = next_groups.size(); i < e; ++i)
            next_groups[i].factor = g.factor / cnt;
        }
      }
      groups = std::move(next_groups);
      // Second pass: resolve any groups that can't or have no need to continue
      for(auto& g: groups) {
        if(g.factor == 0 || g.begin->path().size() == i+1
           || std::distance(g.begin, g.end) == 1) {
          // This group is done, save the factor back in the output
          size_t idx = std::distance(templates.begin(), g.begin);
          for(auto it = g.begin; it != g.end; ++it, ++idx)
            factors[idx] = g.factor;
        }
      }
      groups = std::move(next_groups);
    }
    return factors;
  }
private:
  std::unordered_map<Scope, double> top_vals;
};

}  // namespace

//
// ContextFlowGraph
//

ContextFlowGraph::ContextFlowGraph(Scope s)
  : u_scope(s), m_fallback(s) {}

ContextFlowGraph::ContextFlowGraph(ContextFlowGraph&& o)
  : u_scope(std::move(o.u_scope)), m_templates(std::move(o.m_templates)),
    m_handler(std::move(o.m_handler)) {}

ContextFlowGraph::~ContextFlowGraph() = default;

void ContextFlowGraph::add(Template t) {
  assert(std::all_of(t.path().cbegin(), t.path().cend(),
                     [&](const Scope& s){ return s != scope(); })
         && "FlowGraph::Templates cannot be recursive!");
  m_templates.emplace_back(std::move(t));
}

void ContextFlowGraph::handler(std::function<MetricHandling(const Metric&)> h) {
  m_handler = std::move(h);
}

void ContextFlowGraph::freeze(const std::function<util::optional_ref<ContextFlowGraph>(const Scope&)>& create) {
  assert(m_templates.empty() == !m_handler && "FlowGraph::Templates only make sense with a handler!");

  // Sort the templates lexographically by their entries + paths, to make it
  // easier for us to calculate the factors later.
  std::sort(m_templates.begin(), m_templates.end(),
    [](const auto& a, const auto& b) {
      return a.entry() != b.entry() ? a.entry() < b.entry() : a.path() < b.path();
    });
  // Verify that no path is a prefix of another. It's unclear what to do if this
  // is the case, so we just abort.
  for(size_t i = 1; i < m_templates.size(); i++) {
    const auto& a = m_templates[i-1];
    const auto& b = m_templates[i];
    // The sort from before will make sure a prefix directly precedes the longer
    // path, so we can just compare subsequent pairs.
    if(a.entry() == b.entry() && a.path().size() <= b.path().size()) {
      if(std::mismatch(a.path().begin(), a.path().end(), b.path().begin()).first == a.path().end()) {
        util::log::fatal{} << "FlowGraph::Template paths cannot be prefixes of each other!";
      }
    }
  }

  // Scan through all the Templates and fill m_entries and m_siblings.
  for(const auto& t: m_templates) {
    m_entries.insert(t.entry());
    for(const Scope& s: t.path()) {
      if(m_siblings.count(s) == 0) {
        auto sib = create(s);
        if(!sib) {
          // TODO: This should really fail semi-gracefully and result in an
          // empty FlowGraph, but for now just die.
          util::log::fatal{} << "Missing sibling FlowGraph data for: " << s;
        }
        m_siblings.emplace(s, *sib);
      }
    }
  }
  // Always check that we didn't accidentally recurse. There are other
  // assertions to help debugging, but this is the cheapest so it's always on.
  if(m_siblings.count(scope()) > 0)
    util::log::fatal{} << "Recursive ContextFlowGraph was generated, unable to continue!";

  m_frozen_once.signal();
}

const std::deque<ContextFlowGraph::Template>& ContextFlowGraph::templates() const noexcept {
  m_frozen_once.wait();
  return m_templates;
}

const std::function<ContextFlowGraph::MetricHandling(const Metric&)>& ContextFlowGraph::handler() const noexcept {
  m_frozen_once.wait();
  return m_handler;
}

bool ContextFlowGraph::empty() const {
  m_frozen_once.wait();
  return !m_handler;
}

const std::unordered_set<Scope>& ContextFlowGraph::entries() const {
  m_frozen_once.wait();
  return m_entries;
}

std::unordered_map<util::reference_index<const ContextReconstruction>,
                   std::vector<double>>
ContextFlowGraph::exteriorFactors(
    const std::unordered_set<util::reference_index<const ContextReconstruction>>& reconsts,
    const perctx_mvals_t<Context>& c_data) const {
  m_frozen_once.wait();

  std::unordered_map<util::reference_index<const ContextReconstruction>,
                     ExteriorFactors> factors_acc;
  Shared shared;
  for(const ContextReconstruction& r: reconsts) {
    assert(&r.graph() == this);
    util::optional_ref<ExteriorFactors> factors;
    for(const auto& [entry_s, entry_c]: r.m_entries) {
      if(auto mvs = c_data.find(entry_c)) {
        if(!factors) factors = factors_acc[r];
        for(const auto& mv: mvs->citerate()) shared.insert(mv, *this);
        // Now the Shared should be more-or-less stable, so we can use Factors
        for(const auto& mv: mvs->citerate())
          factors->insert(entry_s, mv, shared);
      }
    }
  }
  std::unordered_map<util::reference_index<const ContextReconstruction>,
                     std::vector<double>> factors;
  factors.reserve(factors_acc.size());
  for(auto& [r, acc]: factors_acc)
    factors.emplace(r, std::move(acc).extract(*this, shared));
  return factors;
}

std::vector<double> ContextFlowGraph::interiorFactors(
    const perctx_mvals_t<ContextFlowGraph>& fg_data) const {
  return interiorFactors_impl<mvals_t>(
    [&](const Scope& p) -> util::optional_ref<const mvals_t> {
      return fg_data.find(m_siblings.at(p));
    }, [&](const mvals_t& mvs, const Metric& m) -> double {
      return mvs.at(m).get(MetricScope::point).value_or(0);
    }, [&](const mvals_t& mvs, const auto& f){
      for(const auto& [m, v]: mvs.citerate())
        f(m, v.get(MetricScope::point).value_or(0));
    });
}

template<class T, class Find, class At, class ForAll>
std::vector<double> ContextFlowGraph::interiorFactors_impl(
    const Find& find, const At&, const ForAll& forall) const {
  m_frozen_once.wait();

  // Shortcut: if there's only one Template, the factor is 1. Period. Also
  // handles the edge-case where there is no interior (path) in the FlowGraph.
  if(m_templates.size() == 1) return {1.};

  Shared shared;
  InteriorFactors factors;
  for(const auto& t: m_templates) {
    for(const auto& p: t.path()) {
      if(factors.skip(p, shared)) continue;
      util::optional_ref<const T> mvs = find(p);
      if(mvs) {
        forall(*mvs, [&](const Metric& m, double v){
          shared.insert(m, v, *this);
        });
        forall(*mvs, [&](const Metric& m, double v){
          factors.insert(p, m, v, shared);
        });
      }
    }
  }
  return factors.extract(*this, shared);
}

//
// ContextReconstruction
//

ContextReconstruction::ContextReconstruction(Context& r, ContextFlowGraph& g)
  : u_graph(g), m_root(r) {}

ContextReconstruction::ContextReconstruction(ContextReconstruction&& o)
  : u_graph(o.graph()), m_root(o.m_root), m_siblings(std::move(o.m_siblings)),
    m_finals(std::move(o.m_finals)) {}

ContextReconstruction::~ContextReconstruction() = default;

void ContextReconstruction::instantiate(
    const std::function<Context&(Context&, const Scope&)>& create_base,
    const std::function<ContextReconstruction&(const Scope&)>& create_sib) {
  const auto& templates = graph().templates();  // Also waits for freeze
  assert(!templates.empty());

  // Fill in m_entries and m_siblings. They're roughly the same as the FlowGraph.
  m_entries.reserve(graph().m_entries.size());
  for(const auto& s: graph().m_entries)
    m_entries.insert({s, create_base(root(), s)});
  m_siblings.reserve(graph().m_siblings.size());
  for(const auto& [s, s_fg]: graph().m_siblings)
    m_siblings.insert({s, create_sib(s)});

  // Fill in m_exits, based on the paths and exit Scopes.
  m_finals.reserve(templates.size());
  for(const auto& t: templates) {
    std::reference_wrapper<Context> c = root();
    for(const auto& s: t.path()) c = create_base(c, s);
    m_finals.emplace_back(create_base(c, graph().scope()));
  }

  m_instantiated.signal();
}

std::vector<double> ContextReconstruction::rescalingFactors(
    const perctx_mvals_t<Context>& c_data) const {
  return rescalingFactors_impl<mvals_t>(
    [&](const Context& entry_c) -> util::optional_ref<const mvals_t> {
      return c_data.find(entry_c);
    }, [&](const mvals_t& mvs, const Metric& m) -> double {
      return mvs.at(m).get(MetricScope::point).value_or(0);
    }, [&](const mvals_t& mvs, const auto& f){
      for(const auto& [m, v]: mvs.citerate())
        f(m, v.get(MetricScope::point).value_or(0));
    });
}

std::vector<double> ContextReconstruction::rescalingFactors(
    const std::unordered_map<util::reference_index<const Context>,
      std::unordered_map<util::reference_index<const Metric>,
        double>>& c_data) const {
  using mvs_t = std::unordered_map<util::reference_index<const Metric>, double>;
  return rescalingFactors_impl<mvs_t>(
    [&](const Context& entry_c) -> util::optional_ref<const mvs_t> {
      auto mvs_it = c_data.find(entry_c);
      if(mvs_it == c_data.end()) return std::nullopt;
      return mvs_it->second;
    }, [&](const mvs_t& mvs, const Metric& m) -> double {
      return mvs.at(m);
    }, [&](const mvs_t& mvs, const auto& f){
      for(const auto& [m, v]: mvs) f(m, v);
    });
}

template<class T, class Find, class At, class ForAll>
std::vector<double> ContextReconstruction::rescalingFactors_impl(
    const Find& find, const At& at, const ForAll& forall) const {
  m_instantiated.wait();
  auto& templates = graph().templates();

  Shared shared;
  std::unordered_map<Scope, double> factors;
  factors.reserve(m_entries.size());
  for(const auto& [entry_s, entry_c]: m_entries) {
    util::optional_ref<const T> mvs = find(entry_c);
    if(mvs) {
      forall(*mvs, [&](const Metric& m, double v){
        shared.insert(m, v, graph());
      });
      if(!shared.exterior()) {
        // We never saw the right Metric for this formulation. Unclear how to
        // recover in this case, so for now just abort.
        util::log::fatal{} << "No suitable Metrics for rescaling factor calculations!";
      }
      if(shared.exterior() == shared.exteriorLogical()) {
        // Shortcut, the factor is just 1
        factors.try_emplace(entry_s, 1);
      } else {
        factors.try_emplace(entry_s, at(*mvs, *shared.exteriorLogical())
            / at(*mvs, *shared.exterior()));
      }
    }
  }

  // Expand for the final output
  std::vector<double> out;
  out.reserve(templates.size());
  for(const auto& t: templates) {
    auto fit = factors.find(t.entry());
    out.push_back(fit != factors.end() ? fit->second : 0.);
  }
  return out;
}

std::vector<double> ContextReconstruction::exteriorFactors(
    const perctx_mvals_t<Context>& c_data) const {
  m_instantiated.wait();
  auto& templates = graph().templates();

  // Shortcut: if there's only one Template, the factor is 1. Period.
  if(templates.size() == 1) return {1.};

  Shared shared;
  ExteriorFactors factors;
  for(const auto& t: templates) {
    if(factors.skip(t, shared)) continue;
    if(auto mvs = c_data.find(m_entries.at(t.entry()))) {
      for(const auto& mv: mvs->citerate()) shared.insert(mv, graph());
      // Now the Shared should be more-or-less stable, so we can use Factors
      for(const auto& mv: mvs->citerate())
        factors.insert(t.entry(), mv, shared);
    }
  }
  return factors.extract(graph(), shared);
}

std::vector<double> ContextReconstruction::interiorFactors(
    const perctx_mvals_t<ContextReconstruction>& r_data) const {
  return graph().interiorFactors_impl<mvals_t>(
    [&](const Scope& p) -> util::optional_ref<const mvals_t> {
      return r_data.find(m_siblings.at(p));
    }, [&](const mvals_t& mvs, const Metric& m) -> double {
      return mvs.at(m).get(MetricScope::point).value_or(0);
    }, [&](const mvals_t& mvs, const auto& f){
      for(const auto& [m, v]: mvs.citerate())
        f(m, v.get(MetricScope::point).value_or(0));
    });
}

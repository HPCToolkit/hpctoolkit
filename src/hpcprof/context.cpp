// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

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

Context::Context(ud_t::struct_t& rs, util::optional_ref<Context> p, NestedScope s)
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

std::pair<Context&,bool> Context::ensure(NestedScope s) {
  auto x = children_p->emplace(userdata.base(), *this, std::move(s));
  return {x.first(), x.second};
}

using mvals_t = util::locked_unordered_map<util::reference_index<const Metric>,
                                           MetricAccumulator>;
template<class Ctx>
using perctx_mvals_t = util::locked_unordered_map<
    util::reference_index<const Ctx>, mvals_t>;

//
// ContextFlowGraph
//

ContextFlowGraph::ContextFlowGraph(Scope s)
  : u_scope(s), m_fallback(s) {}

ContextFlowGraph::ContextFlowGraph(ContextFlowGraph&& o)
  : u_scope(std::move(o.u_scope)), m_templates(std::move(o.m_templates)),
    m_handler(std::move(o.m_handler)) {}

ContextFlowGraph::~ContextFlowGraph() noexcept = default;

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
  assert((m_templates.empty() || m_handler) && "FlowGraph::Templates only make sense with a handler!");

  // Sort the templates reverse-lexographically by their paths, to make it
  // easier for us to calculate the interior factors later.
  std::sort(m_templates.begin(), m_templates.end(),
    [](const auto& a, const auto& b) {
      return std::lexicographical_compare(a.path().rbegin(), a.path().rend(),
                                          b.path().rbegin(), b.path().rend());
    });
  // Verify that no path is a suffix of another. It's unclear what to do if this
  // is the case, so we just abort.
  for(size_t i = 1; i < m_templates.size(); i++) {
    const auto& a = m_templates[i-1];
    const auto& b = m_templates[i];
    // The sort from before will make sure a suffix directly precedes the longer
    // path, so we can just compare subsequent pairs.
    if(a.path().size() <= b.path().size()) {
      if(std::mismatch(a.path().rbegin(), a.path().rend(), b.path().rbegin()).first == a.path().rend()) {
        util::log::fatal{} << "FlowGraph::Template paths cannot be suffixes of each other!";
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
  return m_templates.empty();
}

const std::unordered_set<Scope>& ContextFlowGraph::entries() const {
  m_frozen_once.wait();
  return m_entries;
}

std::pair<
  std::unordered_map<util::reference_index<const ContextReconstruction>,
                     std::vector<double>>,
  std::vector<bool>
> ContextFlowGraph::exteriorFactors(
    const std::unordered_set<util::reference_index<const ContextReconstruction>>& reconsts,
    const perctx_mvals_t<Context>& c_data) const {
  m_frozen_once.wait();

  // First sum up the denominators
  std::unordered_map<Scope, double> entry_totals;
  util::optional_ref<const Metric> metricExterior;
  bool exteriorIsAlsoLogical = false;
  {
    std::unordered_set<util::reference_index<const Metric>> queried;
    for(const ContextReconstruction& r: reconsts) {
      assert(&r.graph() == this);
      for(const auto& [entry_s, entry_c]: r.m_entries) {
        if(auto mvs = c_data.find(entry_c)) {
          for(const auto& [m, v]: mvs->citerate()) {
            if(!queried.emplace(m).second) continue;
            auto handling = m_handler(m);
            if(handling.exterior) {
              if(metricExterior
                 && exteriorIsAlsoLogical == handling.exteriorLogical) {
                // The handler claims multiple Metrics are usable for exterior
                // factor calculations. We don't know which to pick, so abort.
                util::log::fatal{} << "Multiple suitable Metrics for exterior factor calculations: "
                  << metricExterior->name() << " vs. " << m->name();
              } else if(!metricExterior || exteriorIsAlsoLogical) {
                metricExterior = m;
                exteriorIsAlsoLogical = handling.exteriorLogical;
              }
            }
          }
          if(metricExterior) {
            if(auto vv = mvs->find(*metricExterior)) {
              double v = vv->get(MetricScope::point).value_or(0);
              auto [it, first] = entry_totals.try_emplace(entry_s, v);
              if(!first) it->second += v;
            }
          }
        }
      }
    }
  }
  if(!metricExterior) {
    // We never saw the right Metric for this formulation. Unclear how to
    // recover in this case, so for now just abort.
    util::log::fatal{} << "No suitable Metrics for exterior factor calculations!";
  }

  // Then divide to get the final result. Do it during expansion to save a loop.
  std::unordered_map<util::reference_index<const ContextReconstruction>,
                     std::vector<double>> factors;
  factors.reserve(reconsts.size());
  std::vector<bool> hasEC(m_templates.size(), false);
  for(const ContextReconstruction& r: reconsts) {
    util::optional_ref<std::vector<double>> r_factors;
    std::size_t idx = 0;
    for(const auto& t: m_templates) {
      if(auto mvs = c_data.find(r.m_entries.at(t.entry()))) {
        if(auto vv = mvs->find(*metricExterior)) {
          if(!r_factors) {
            r_factors = factors[r];
            r_factors->resize(m_templates.size(), 0);
          }
          double v = vv->get(MetricScope::point).value_or(0);
          (*r_factors)[idx] = v / entry_totals.at(t.entry());
          hasEC[idx] = hasEC[idx] || v != 0;
        }
      }
      ++idx;
    }
  }
  return {std::move(factors), std::move(hasEC)};
}

std::vector<double> ContextFlowGraph::interiorFactors(
    const perctx_mvals_t<ContextFlowGraph>& fg_data,
    const std::vector<bool>& hasEC) const {
  return interiorFactors_impl<mvals_t>(
    [&](const Scope& p) -> util::optional_ref<const mvals_t> {
      return fg_data.find(m_siblings.at(p));
    }, [&](const mvals_t& mvs, const Metric& m) -> double {
      return mvs.at(m).get(MetricScope::point).value_or(0);
    }, [&](const mvals_t& mvs, const auto& f){
      for(const auto& [m, v]: mvs.citerate())
        f(m, v.get(MetricScope::point).value_or(0));
    }, hasEC);
}

template<class T, class Find, class At, class ForAll>
std::vector<double> ContextFlowGraph::interiorFactors_impl(
    const Find& find, const At& at, const ForAll& forall,
    const std::vector<bool>& hasEC) const {
  assert(hasEC.size() == m_templates.size());
  m_frozen_once.wait();

  // Shortcut: if there's only one Template, the factor is 1 (or 0). Period.
  // Also handles the edgecase where there is no interior path in the FlowGraph.
  if(m_templates.size() == 1) return {hasEC[0] ? 1. : 0.};

  // Shortcut 2: if there's <= 1 true in hasEC, the factor is 1 (or 0).
  {
    std::optional<size_t> single;
    size_t idx = 0;
    for(const auto& x: hasEC) {
      if(x) {
        if(single) { single = -1; break; }
        else single = idx;
      }
      ++idx;
    }
    if(single != -1) {
      std::vector<double> factors(m_templates.size(), 0.);
      if(single) factors[*single] = 1.;
      return factors;
    }
  }

  // Find the Metric we will need for interior factor calculations, if we can.
  // If we can't, we'll still do our best from just the static paths.
  util::optional_ref<const Metric> metricInterior;
  {
    std::unordered_set<util::reference_index<const Metric>> queried;
    for(const auto& [s, fg]: m_siblings) {
      if(auto mvs = find(s)) {
        forall(*mvs, [&](const Metric& m, double){
          if(!queried.emplace(m).second) return;
          if(m_handler(m).interior) {
            if(metricInterior) {
              // The handler claims multiple Metrics are usable for interior
              // factor calculations. We don't know which to pick, so abort.
              util::log::fatal{} << "Multiple suitable Metrics for interior factor calculations: "
                << metricInterior->name() << " vs. " << m.name();
            }
            metricInterior = m;
          }
        });
      }
    }
  }

  // To save on calculations, we group the Templates by their shared suffix.
  // Thanks to the sort in freeze, these are always contiguous groupings.
  struct Group {
    decltype(m_templates)::const_iterator first;
    decltype(m_templates)::const_iterator last;
    double factor;
  };
  std::deque<Group> groups;
  groups.push_back({m_templates.begin(), m_templates.end(), 1.});

  // The interior factors are calculated by looking at shared (or not) suffixes
  // between the different possible paths. These are multiplied over the course
  // of multiple "rounds" until there are no more groups to process.
  std::vector<double> factors(m_templates.size(), 0);
  for(size_t ridx = 0; !groups.empty(); ++ridx) {
    const auto rAtIdx = [ridx](const auto& path){
      return path[path.size() - ridx - 1];
    };
    decltype(groups) next_groups;

    // First pass: try to split groups into sub-groups when paths diverge, in
    // the caller's direction.
    for(auto& g: groups) {
      size_t next_first = next_groups.size();
      double total_v = 0;

      // We want to break g up into sub-Groups based on divergence in the
      // ridx'th caller. But we also want to ignore Templates that are simply
      // impossible because they were never "entered" (!hasEC[...]).
      //
      // So we keep two iterator-cursors, marking the first and (inclusive) last
      // possible Template within the current sub-Group. When a new possible
      // Template appears, we either extend the current sub-Group to include it,
      // or start a new sub-Group to represent the divergence.
      auto first = m_templates.end();
      auto ilast = first;
      for(auto it = g.first; it != g.last; ++it) {
        // Skip over any Templates that are impossible
        if(!hasEC[std::distance(m_templates.begin(), it)]) continue;
        assert(it->path().size() > ridx);

        if(first != m_templates.end()) {
          if(rAtIdx(first->path()) == rAtIdx(it->path())) {
            // This Template is the same as the current sub-Group. Just expand
            // the sub-Group to include this one and continue along.
            ilast = it;
            continue;
          }

          // This Template diverges from the current sub-Group. Save the current
          // sub-Group before we reset.
          double v = 0;
          if(metricInterior)
            if(auto mvs = find(rAtIdx(first->path())))
              v = at(*mvs, *metricInterior);
          next_groups.push_back({first, ++ilast, v});
          total_v += v;
        }

        // This Template is the first in a new sub-Group. Reset the cursors
        // to represent this.
        first = ilast = it;
      }

      // At the end of the previous loop, we should still have one sub-Group
      // left. Save it now.
      assert(first != m_templates.end() && ilast != m_templates.end()
             && "Entire group was impossible according to hasEC?");
      double v = 0;
      if(metricInterior)
        if(auto mvs = find(rAtIdx(first->path())))
          v = at(*mvs, *metricInterior);
      next_groups.push_back({first, ++ilast, v});
      total_v += v;

      // ...Then go back through the new groups and do the division.
      if(total_v > 0) {
        for(size_t i = next_first, e = next_groups.size(); i < e; ++i)
          next_groups[i].factor = g.factor * next_groups[i].factor / total_v;
      } else {
        // Special case: if we don't have any clue where these values came from,
        // divide evenly among the statically available options.
        for(size_t i = next_first, e = next_groups.size(); i < e; ++i)
          next_groups[i].factor = g.factor / (double)(e - next_first);
      }
    }
    groups = std::move(next_groups);
    next_groups.clear();

    // Second pass: resolve any groups that can be and assign their factors.
    for(auto& g: groups) {
      if(g.factor == 0  // No need to continue processing
         || g.first->path().size() == ridx+1  // Path is terminating here
         || std::distance(g.first, g.last) == 1) {  // Group contains 1 Template
        // If any of the above is true, this group does not need to continue
        // processing, we can assign the factors now.
        size_t idx = std::distance(m_templates.begin(), g.first);
        for(auto it = g.first; it != g.last; ++it, ++idx)
          if(hasEC[idx]) factors[idx] = g.factor;
      } else next_groups.emplace_back(std::move(g));
    }
    groups = std::move(next_groups);
  }
  return factors;
}

//
// ContextReconstruction
//

ContextReconstruction::ContextReconstruction(Context& r, ContextFlowGraph& g)
  : u_graph(g), m_root(r) {}

ContextReconstruction::ContextReconstruction(ContextReconstruction&& o)
  : u_graph(o.graph()), m_root(o.m_root), m_siblings(std::move(o.m_siblings)),
    m_finals(std::move(o.m_finals)) {}

ContextReconstruction::~ContextReconstruction() noexcept = default;

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

std::pair<std::vector<double>, std::vector<bool>>
ContextReconstruction::rescalingFactors(
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
    }).first;
}

template<class T, class Find, class At, class ForAll>
std::pair<std::vector<double>, std::vector<bool>>
ContextReconstruction::rescalingFactors_impl(
    const Find& find, const At& at, const ForAll& forall) const {
  m_instantiated.wait();
  auto& templates = graph().templates();

  std::unordered_map<Scope, double> factors;
  factors.reserve(m_entries.size());

  {
    util::optional_ref<const Metric> metricExterior;
    util::optional_ref<const Metric> metricExteriorLogical;
    bool exteriorLogicalIsAlsoExterior = false;
    std::unordered_set<util::reference_index<const Metric>> queried;
    for(const auto& [entry_s, entry_c]: m_entries) {
      util::optional_ref<const T> mvs = find(entry_c);
      if(mvs) {
        // Find the metrics we need for the rescaling factor calculations.
        forall(*mvs, [&](const Metric& m, double){
          if(!queried.emplace(m).second) return;
          auto handling = graph().handler()(m);
          if(handling.exteriorLogical) {
            if(metricExteriorLogical) {
              // The handler claims multiple Metrics are usable for rescaling
              // factor calculations. We don't know which to pick, so abort.
              util::log::fatal{} << "Multiple suitable Metrics for rescaling factor calculations (exteriorLogical): "
                << metricExteriorLogical->name() << " vs. " << m.name();
            }
            metricExteriorLogical = m;
            exteriorLogicalIsAlsoExterior = handling.exterior;
          } else if(handling.exterior) {
            if(metricExterior) {
              // The handler claims multiple Metrics are usable for rescaling
              // factor calculations. We don't know which to pick, so abort.
              util::log::fatal{} << "Multiple suitable Metrics for rescaling factor calculations (exterior): "
                << metricExterior->name() << " vs. " << m.name();
            }
            metricExterior = m;
          }
        });
        assert(metricExteriorLogical || !exteriorLogicalIsAlsoExterior);
        if(!metricExterior && !exteriorLogicalIsAlsoExterior) {
          // We never saw the right Metric for this formulation. Unclear how to
          // recover in this case, so for now just abort.
          util::log::fatal{} << "No suitable Metrics for rescaling factor calculations!";
        }

        // Based on the results above, generate the final factor
        if(!metricExterior || !metricExteriorLogical) {
          // Shortcut: only one call count metric, so the factor is just 1.
          factors.try_emplace(entry_s, 1);
        } else {
          factors.try_emplace(entry_s, at(*mvs, *metricExteriorLogical)
                                       / at(*mvs, *metricExterior));
        }
      }
    }
  }

  // Expand for the final output
  std::vector<double> out;
  out.reserve(templates.size());
  std::vector<bool> hasEC;
  hasEC.reserve(templates.size());
  for(const auto& t: templates) {
    auto fit = factors.find(t.entry());
    out.push_back(fit != factors.end() ? fit->second : 0.);
    hasEC.push_back(fit != factors.end() && fit->second != 0);
  }
  return {std::move(out), std::move(hasEC)};
}

std::vector<double> ContextReconstruction::interiorFactors(
    const perctx_mvals_t<ContextReconstruction>& r_data,
    const std::vector<bool>& hasEC) const {
  return graph().interiorFactors_impl<mvals_t>(
    [&](const Scope& p) -> util::optional_ref<const mvals_t> {
      return r_data.find(m_siblings.at(p));
    }, [&](const mvals_t& mvs, const Metric& m) -> double {
      return mvs.at(m).get(MetricScope::point).value_or(0);
    }, [&](const mvals_t& mvs, const auto& f){
      for(const auto& [m, v]: mvs.citerate())
        f(m, v.get(MetricScope::point).value_or(0));
    }, hasEC);
}

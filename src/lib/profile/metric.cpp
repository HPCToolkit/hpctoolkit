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

#include <yaml-cpp/yaml.h>

#include <forward_list>
#include <stack>
#include <thread>
#include <ostream>

using namespace hpctoolkit;

std::ostream& hpctoolkit::operator<<(std::ostream& os, Statistic::combination_t c) {
  switch(c) {
  case Statistic::combination_t::sum: return os << "sum";
  case Statistic::combination_t::min: return os << "min";
  case Statistic::combination_t::max: return os << "max";
  }
  std::abort();
}
YAML::Emitter& hpctoolkit::operator<<(YAML::Emitter& e, Statistic::combination_t c) {
  switch(c) {
  case Statistic::combination_t::sum: return e << "sum";
  case Statistic::combination_t::min: return e << "min";
  case Statistic::combination_t::max: return e << "max";
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

ExtraStatistic::ExtraStatistic(Settings s)
  : u_settings(std::move(s)) {
  assert(!u_settings().formula.empty() && "ExtraStatistics must have a non-empty formula!");

  const auto& fmt = u_settings().format;
  size_t pos = 0;
  bool found = false;
  while((pos = fmt.find('%', pos)) != std::string::npos) {
    if(fmt[pos+1] == '%') pos += 2;
    else {
      if(found)
        util::log::fatal{} << "Multiple arguments in format string for ExtraStatistic: " << fmt;
      found = true;
      pos += 1;
    }
  }
}

bool ExtraStatistic::Settings::operator==(const Settings& o) const noexcept {
  bool res = Metric::Settings::operator==(o);
  if(res && formula != o.formula) {
      util::log::fatal f{};
      f << "EStats with the same name but different formulas: ";
      if(formula.size() != o.formula.size()) {
        f << "#" << formula.size() << " != " << o.formula.size();
      } else {
        f << "\n";
        for(size_t i = 0; i < formula.size(); i++) {
          f << "  " << i << ": ";
          if(std::holds_alternative<std::string>(formula[i])) {
            f << std::get<std::string>(formula[i]);
          } else {
            const auto& mp = std::get<ExtraStatistic::MetricPartialRef>(formula[i]);
            f << "[" <<  mp.metric.name() << "]." << mp.partialIdx;
          }
          f << " " << (formula[i] == o.formula[i] ? "=" : "!") << "= ";
          if(std::holds_alternative<std::string>(o.formula[i])) {
            f << std::get<std::string>(o.formula[i]);
          } else {
            const auto& mp = std::get<ExtraStatistic::MetricPartialRef>(o.formula[i]);
            f << "[" <<  mp.metric.name() << "]." << mp.partialIdx;
          }
          f << "\n";
        }
      }
  }
  return res;
}

Statistic::Statistic(std::string suff, bool showp, formula_t form, bool showBD)
  : m_suffix(std::move(suff)), m_showPerc(showp), m_formula(std::move(form)),
    m_visibleByDefault(showBD) {};

Metric::Metric(Metric&& o)
  : userdata(std::move(o.userdata), std::cref(*this)),
    u_settings(std::move(o.u_settings)),
    m_thawed_stats(std::move(o.m_thawed_stats)),
    m_thawed_sumPartial(o.m_thawed_sumPartial),
    m_frozen(o.m_frozen.load(std::memory_order_relaxed)),
    m_partials(std::move(o.m_partials)),
    m_stats(std::move(o.m_stats)) {};

Metric::Metric(ud_t::struct_t& rs, Settings s)
  : userdata(rs, std::cref(*this)), u_settings(std::move(s)),
    m_thawed_sumPartial(std::numeric_limits<std::size_t>::max()),
    m_frozen(false) {};

std::unique_lock<std::mutex> Metric::StatsAccess::synchronize() {
  if(m.m_frozen.load(std::memory_order_acquire)) return {};
  std::unique_lock<std::mutex> l{m.m_frozen_lock};
  if(m.m_frozen.load(std::memory_order_relaxed)) return {};
  return l;
}

void Metric::StatsAccess::requestStatistics(Statistics ss) {
  // If the Metric is invisible, we don't actually need to do anything
  if(m.visibility() == Settings::visibility_t::invisible) {
    assert(!ss.sum && !ss.mean && !ss.min && !ss.max && !ss.stddev && !ss.cfvar
           && "Attempt to request Statistics from an invisible Metric!");
    return;
  }

  auto l = synchronize();
  if(!l) {
    // If we're already frozen, this operation must be idempotent.
    assert((!ss.sum    || m.m_thawed_stats.sum)    && "Attempt to request :Sum from a frozen Metric!");
    assert((!ss.mean   || m.m_thawed_stats.mean)   && "Attempt to request :Mean from a frozen Metric!");
    assert((!ss.min    || m.m_thawed_stats.min)    && "Attempt to request :Min from a frozen Metric!");
    assert((!ss.max    || m.m_thawed_stats.max)    && "Attempt to request :Max from a frozen Metric!");
    assert((!ss.stddev || m.m_thawed_stats.stddev) && "Attempt to request :StdDev from a frozen Metric!");
    assert((!ss.cfvar  || m.m_thawed_stats.cfvar)  && "Attempt to request :CfVar from a frozen Metric!");
    return;
  }
  m.m_thawed_stats.sum = m.m_thawed_stats.sum || ss.sum;
  m.m_thawed_stats.mean = m.m_thawed_stats.mean || ss.mean;
  m.m_thawed_stats.min = m.m_thawed_stats.min || ss.min;
  m.m_thawed_stats.max = m.m_thawed_stats.max || ss.max;
  m.m_thawed_stats.stddev = m.m_thawed_stats.stddev || ss.stddev;
  m.m_thawed_stats.cfvar = m.m_thawed_stats.cfvar || ss.cfvar;
}

std::size_t Metric::StatsAccess::requestSumPartial() {
  auto l = synchronize();
  if(m.m_thawed_sumPartial != std::numeric_limits<std::size_t>::max())
    return m.m_thawed_sumPartial;
  assert(l && "Unable to satisfy :Sum Partial request on a frozen Metric!");

  m.m_thawed_sumPartial = m.m_partials.size();
  m.m_partials.push_back({"x", [](double x) -> double { return x; },
                          Statistic::combination_t::sum, m.m_thawed_sumPartial});
  return m.m_thawed_sumPartial;
}

bool Metric::freeze() {
  if(m_frozen.load(std::memory_order_acquire)) return false;
  std::unique_lock<std::mutex> l{m_frozen_lock};
  if(m_frozen.load(std::memory_order_relaxed)) return false;

  const Statistics& ss = m_thawed_stats;

  size_t cntIdx = -1;
  if(ss.mean || ss.stddev || ss.cfvar) {
    cntIdx = m_partials.size();
    m_partials.push_back({"1", [](double x) -> double { return x == 0 ? 0 : 1; },
                          Statistic::combination_t::sum, cntIdx});
  }
  if(m_thawed_sumPartial == std::numeric_limits<std::size_t>::max()
     && (ss.sum || ss.mean || ss.stddev || ss.cfvar)) {
    m_thawed_sumPartial = m_partials.size();
    m_partials.push_back({"x", [](double x) -> double { return x; },
                          Statistic::combination_t::sum, m_thawed_sumPartial});
  }
  size_t x2Idx = -1;
  if(ss.stddev || ss.cfvar) {
    x2Idx = m_partials.size();
    m_partials.push_back({"x^2", [](double x) -> double { return x * x; },
                          Statistic::combination_t::sum, x2Idx});
  }
  size_t minIdx = -1;
  if(ss.min) {
    minIdx = m_partials.size();
    m_partials.push_back({"x", [](double x) -> double { return x; },
                          Statistic::combination_t::min, minIdx});
  }
  size_t maxIdx = -1;
  if(ss.max) {
    maxIdx = m_partials.size();
    m_partials.push_back({"x", [](double x) -> double { return x; },
                          Statistic::combination_t::max, maxIdx});
  }

  if(ss.sum)
    m_stats.push_back({"Sum", true, {(Statistic::formula_t::value_type)m_thawed_sumPartial},
                       u_settings().visibility == Settings::visibility_t::shownByDefault});
  if(ss.mean)
    m_stats.push_back({"Mean", false, {m_thawed_sumPartial, "/", cntIdx},
                       u_settings().visibility == Settings::visibility_t::shownByDefault});
  if(ss.stddev)
    m_stats.push_back({"StdDev", false,
      {"sqrt((", x2Idx, "/", cntIdx, ") - pow(", m_thawed_sumPartial, "/", cntIdx, ", 2))"},
      u_settings().visibility == Settings::visibility_t::shownByDefault});
  if(ss.cfvar)
    m_stats.push_back({"CfVar", false,
      {"sqrt((", x2Idx, "/", cntIdx, ") - pow(", m_thawed_sumPartial, "/", cntIdx, ", 2)) / (", m_thawed_sumPartial, "/", cntIdx, ")"},
      u_settings().visibility == Settings::visibility_t::shownByDefault});
  if(ss.min)
    m_stats.push_back({"Min", false, {(Statistic::formula_t::value_type)minIdx},
                       u_settings().visibility == Settings::visibility_t::shownByDefault});
  if(ss.max)
    m_stats.push_back({"Max", false, {(Statistic::formula_t::value_type)maxIdx},
                       u_settings().visibility == Settings::visibility_t::shownByDefault});

  m_frozen.store(true, std::memory_order_release);
  return true;
}

const std::vector<StatisticPartial>& Metric::partials() const noexcept {
  assert(m_frozen.load(std::memory_order_relaxed) && "Attempt to access a Metric's Partials before a freeze!");
  return m_partials;
}
const std::vector<Statistic>& Metric::statistics() const noexcept {
  assert(m_frozen.load(std::memory_order_relaxed) && "Attempt to access a Metric's Statistics before a freeze!");
  return m_stats;
}

const MetricAccumulator* Metric::getFor(const Thread::Temporary& t, const Context& c) const noexcept {
  auto* cd = t.data.find(c);
  if(cd == nullptr) return nullptr;
  return cd->find(*this);
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

void Metric::prefinalize(Thread::Temporary& t) noexcept {
  // Before anything else happens, we need to handle the Superpositions present
  // in the Context tree and distribute their data.
  for(const auto& cd: t.sp_data.citerate()) {
    const SuperpositionedContext& c = cd.first;

    // Helper function to determine the factoring Metric used for the given Context.
    auto findDistributor = [&](ContextRef c) -> util::optional_ref<const Metric> {
      const decltype(t.data)::mapped_type* d = nullptr;
      if(auto tc = std::get_if<Context>(c)) d = t.data.find(*tc);
      else if(auto tc = std::get_if<SuperpositionedContext>(c)) d = t.sp_data.find(*tc);
      else abort();  // unreachable
      if(d == nullptr) return {};
      util::optional_ref<const Metric> m;
      for(const auto& ma: d->citerate()) {
        const Metric& cm = ma.first;
        if(cm.name() == "GINS") {
          if(!m) m = cm;
          else if(&*m != &cm)
            util::log::fatal{} << "Multiple distributing Metrics in the same Context: "
              << "\"" << m->name() << "\" != \"" << cm.name() << "\"";
        }
      }
      return m;
    };
    util::optional_ref<const Metric> distributor;

    // Before we begin we sort the routes lexigraphically, along with their
    // associated targets. This saves us some time later.
    std::vector<std::reference_wrapper<const SuperpositionedContext::Target>>
      targets(c.m_targets.begin(), c.m_targets.end());
    std::sort(targets.begin(), targets.end(), [](const auto& a, const auto& b)-> bool{
      return a.get().route < b.get().route;
    });

    // To ensure proper numeric stability, we group routes together based
    // on their prefixes, assigning a fraction of the whole to each group.
    struct group_t {
      decltype(targets)::iterator begin;
      decltype(targets)::iterator end;
      double value;
    };
    std::forward_list<group_t> groups;
    groups.push_front({targets.begin(), targets.end(), 1});
    for(std::size_t idx = 0; !groups.empty(); idx++) {
      std::forward_list<group_t> new_groups;
      for(auto& g: groups) {
        // Termination case: once a group only has one element, we can distribute!
        if(std::distance(g.begin, g.end) == 1) {
          for(const auto& ma: cd.second.citerate()) {
            auto rv = ma.second.point.load(std::memory_order_relaxed);
            auto v = rv * g.value;
            auto& tc = std::get<Context>(g.begin->get().target);
            atomic_add(t.data[tc][ma.first].point, v);
          }
          continue;
        }

        // Other termination case: groups with 0 value don't need to be processed.
        if(g.value == 0) continue;

        // Construct new groups based on elements sharing a Context prefix
        std::forward_list<group_t> next;
        std::size_t nextCnt = 0;
        double totalValue = 0;
        {
          group_t cur = {g.begin, g.begin, 0};
          cur.end++;
          for(; cur.end != g.end; cur.end++) {
            const auto& vb = cur.begin->get().route;
            const auto& ve = cur.end->get().route;
            if(vb.size() <= idx && ve.size() <= idx) continue;
            if(vb.size() <= idx) {
              next.push_front(cur);
              nextCnt++;
              cur.begin = cur.end;
            } else if(vb[idx] != ve[idx]) {
              auto dm = findDistributor(vb[idx]);
              if(dm) {
                if(!distributor) distributor = dm;
                else if(&*distributor != &*dm)
                  util::log::fatal{} << "Multiple distributing Metrics under the same Superposition:"
                    << distributor->name() << " != " << dm->name();
                if(auto tc = std::get_if<Context>(vb[idx]))
                  cur.value = t.data[*tc][*dm].point.load(std::memory_order_relaxed);
                else if(auto tc = std::get_if<SuperpositionedContext>(vb[idx]))
                  cur.value = t.sp_data[*tc][*dm].point.load(std::memory_order_relaxed);
                else abort();  // unreachable
                totalValue += cur.value;
              }
              next.push_front(cur);
              nextCnt++;
              cur.begin = cur.end;
            }
          }
        }

        // If we have any value to play with, try to distribute it.
        // Otherwise we distribute evenly across all the possiblities.
        for(auto& ng: next) {
          if(totalValue == 0) ng.value = g.value / nextCnt;
          else ng.value = g.value * ng.value / totalValue;
        }

        // Add all the new groups into the list for the next round
        new_groups.splice_after(new_groups.before_begin(), std::move(next));
      }
      groups = std::move(new_groups);
    }
  }
}

void Metric::crossfinalize(const CollaborativeContext& cc) noexcept {
  if(cc.data.empty())
    util::log::fatal{} << "CollaborativeContext with no Threads to distribute to?";

  // Collect together a set of all the possible targeted Threads we can
  // distribute to, in the event that we have to fall back to flat distribution
  std::vector<std::pair<decltype(cc.data)::mapped_type::key_type, double>> default_tfactors;
  {
    std::unordered_set<decltype(cc.data)::mapped_type::key_type> targets;
    double total = 0;
    for(const auto& sdata: cc.data.citerate()) {
      for(const auto& tcdata: sdata.second.citerate()) {
        if(!targets.emplace(tcdata.first).second) continue;
        double factor = 0;
        for(const auto& macc: tcdata.second.citerate()) {
          if(macc.first->name() == "GKER:COUNT") {
            factor = macc.second.point.load(std::memory_order_relaxed);
            break;
          }
        }
        total += factor;
        if(factor > 0)
          default_tfactors.emplace_back(tcdata.first, factor);
      }
    }
    for(auto& tcf: default_tfactors) tcf.second /= total;
  }

  // Process everything one top-level Scope at a time
  for(const auto& sc: cc.m_shadow) {
    // Pregenerate the factors to use when distributing across the targets
    // Also sum the metrics for the roots back into the Threads
    std::vector<std::pair<decltype(cc.data)::mapped_type::key_type, double>> local_tfactors;
    const decltype(cc.data)::mapped_type* sdata = cc.data.find(sc.first);
    if(sdata != nullptr) {
      local_tfactors.reserve(sdata->size());
      double total = 0;
      for(const auto& tcdata: sdata->citerate()) {
        Thread::Temporary& tt = tcdata.first.first;
        const Context& c = tcdata.first.second;
        auto& cdata = tt.data[c];
        double factor = 0;
        for(const auto& macc: tcdata.second.citerate()) {
          auto val = macc.second.point.load(std::memory_order_relaxed);
          atomic_add(cdata[macc.first].point, val);
          if(macc.first->name() == "GKER:COUNT")
            factor = val;
        }
        total += factor;
        if(factor > 0)
          local_tfactors.emplace_back(tcdata.first, factor);
      }
      for(auto& tf: local_tfactors) tf.second /= total;
    }
    const auto& tfactors = local_tfactors.empty() ? default_tfactors : local_tfactors;

    // Walk the SharedContext tree and sum the results back into the Threads
    using frame_t = std::reference_wrapper<CollaborativeSharedContext>;
    std::stack<frame_t, std::vector<frame_t>> queue;
    queue.emplace(*sc.second);
    while(!queue.empty()) {
      auto& shad = queue.top().get();
      queue.pop();

      if(!shad.data.empty()) {
        for(const auto& tf: tfactors) {
          const Context& c = shad.m_shadowing.at(tf.first.second.get());
          auto& cdata = tf.first.first.get().data[c];
          for(const auto& macc: shad.data.citerate()) {
            const Metric& m = macc.first;
            auto val = macc.second.point.load(std::memory_order_relaxed);
            atomic_add(cdata[m].point, val * tf.second);
          }
        }
      }
      for(const auto& sc: shad.m_children)
        queue.emplace(*sc.second);
    }
  }
}

void Metric::finalize(Thread::Temporary& t) noexcept {
  // For each Context we need to know what its children are. But we only care
  // about ones that have decendants with actual data. So we construct a
  // temporary subtree with all the bits.
  util::optional_ref<const Context> global;
  std::unordered_map<util::reference_index<const Context>,
    std::unordered_set<util::reference_index<const Context>>> children;
  for(const auto& cx: t.data.citerate()) {
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
  using md_t = decltype(t.data)::mapped_type;
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
    md_t& data = t.data[c];
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
    auto& cdata = const_cast<Context&>(c).data;
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

std::size_t std::hash<Metric::Settings>::operator()(const Metric::Settings &s) const noexcept {
  const auto h1 = std::hash<std::string>{}(s.name);
  const auto h2 = std::hash<std::string>{}(s.description);
  return h1 ^ ((h2 << 1) | (h2 >> (-1 + 8 * sizeof h2)));
}

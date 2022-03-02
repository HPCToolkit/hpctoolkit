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

#ifndef HPCTOOLKIT_PROFILE_ACCUMULATORS_H
#define HPCTOOLKIT_PROFILE_ACCUMULATORS_H

#include "scope.hpp"

#include "util/locked_unordered.hpp"
#include "util/streaming_sort.hpp"

#include <atomic>
#include <bitset>
#include <chrono>
#include <iosfwd>
#include <optional>
#include <vector>

namespace hpctoolkit {

class Metric;
class StatisticPartial;
class Thread;
class Context;
class ContextReconstruction;
class ContextFlowGraph;

/// Every Metric can have values at multiple Scopes pertaining to the subtree
/// rooted at a particular Context with Metric data.
enum class MetricScope : size_t {
  /// Encapsulates the current Context, and no other nodes. This references
  /// exactly where the data arose, and is the smallest MetricScope.
  point,

  /// Encapsulates the current Context and any descendants not connected by a
  /// function-type Scope. This represents the cost of a function outside of
  /// any child function calls.
  /// Called "exclusive" in the Viewer.
  function,

  /// Encapsulates the current Context and all descendants. This represents
  /// the entire execution spawned by a single source code construct, and is
  /// the largest MetricScope.
  /// Called "inclusive" in the Viewer.
  execution,
};

/// Standardized stringification for MetricScope constants.
std::ostream& operator<<(std::ostream&, MetricScope);
const std::string& stringify(MetricScope);

/// Bitset-like object used as a set of Scope values.
class MetricScopeSet final : private std::bitset<3> {
private:
  using base = std::bitset<3>;
  MetricScopeSet(const base& b) : base(b) {};
public:
  using int_type = uint8_t;
  constexpr MetricScopeSet() = default;
  constexpr MetricScopeSet(MetricScope s) : base(1<<static_cast<size_t>(s)) {};
  MetricScopeSet(std::initializer_list<MetricScope> l) : base(0) {
    for(const auto s: l) base::set(static_cast<size_t>(s));
  }
  explicit constexpr MetricScopeSet(int_type val) : base(val) {};

  static inline constexpr struct all_t {} all = {};
  constexpr MetricScopeSet(all_t)
    : base(std::numeric_limits<unsigned long long>::max()) {}

  bool has(MetricScope s) const noexcept { return base::operator[](static_cast<size_t>(s)); }
  auto operator[](MetricScope s) noexcept { return base::operator[](static_cast<size_t>(s)); }

  MetricScopeSet operator|(const MetricScopeSet& o) { return (base)*this | (base)o; }
  MetricScopeSet operator+(const MetricScopeSet& o) { return (base)*this | (base)o; }
  MetricScopeSet operator&(const MetricScopeSet& o) { return (base)*this & (base)o; }
  MetricScopeSet& operator|=(const MetricScopeSet& o) { base::operator|=(o); return *this; }
  MetricScopeSet& operator+=(const MetricScopeSet& o) { base::operator|=(o); return *this; }
  MetricScopeSet& operator&=(const MetricScopeSet& o) { base::operator&=(o); return *this; }
  bool operator==(const MetricScopeSet& o) { return base::operator==(o); }
  bool operator!=(const MetricScopeSet& o) { return base::operator!=(o); }

  int_type toInt() const noexcept { return base::to_ullong(); }

  using base::count;

  class const_iterator final {
  public:
    const_iterator() {};

    const_iterator(const const_iterator&) = default;
    const_iterator& operator=(const const_iterator&) = default;

    bool operator==(const const_iterator& o) const {
      return set == o.set && (set == nullptr || scope == o.scope);
    }
    bool operator!=(const const_iterator& o) const { return !operator==(o); }

    MetricScope operator*() const { return scope; }
    const MetricScope* operator->() const { return &scope; }

    const_iterator& operator++() {
      assert(set != nullptr);
      size_t ms = static_cast<size_t>(scope);
      for(++ms; ms < set->size() && !set->has(static_cast<MetricScope>(ms));
          ++ms);
      if(ms >= set->size()) set = nullptr;
      else scope = static_cast<MetricScope>(ms);
      return *this;
    }
    const_iterator operator++(int) {
      const_iterator old = *this;
      operator++();
      return old;
    }

  private:
    friend class MetricScopeSet;
    const MetricScopeSet* set = nullptr;
    MetricScope scope;
  };

  const_iterator begin() const {
    const_iterator it;
    it.set = this;
    it.scope = static_cast<MetricScope>(0);
    if(!base::operator[](0)) ++it;
    return it;
  }
  const_iterator end() const { return {}; }
};

/// Accumulator structure for the data implicitly bound to a Thread and Context.
class MetricAccumulator final {
public:
  MetricAccumulator() : point(0), function(0), execution(0) {};

  MetricAccumulator(const MetricAccumulator&) = delete;
  MetricAccumulator& operator=(const MetricAccumulator&) = delete;
  MetricAccumulator(MetricAccumulator&&) = default;
  MetricAccumulator& operator=(MetricAccumulator&&) = delete;

  /// Add some value to this Accumulator. Only point-Scope is allowed.
  // MT: Internally Synchronized
  void add(double) noexcept;

  /// Get the Thread-local sum of Metric value, for a particular Metric Scope.
  // MT: Safe (const), Unstable (before ThreadFinal wavefront)
  std::optional<double> get(MetricScope) const noexcept;

private:
  void validate() const noexcept;
  MetricScopeSet getNonZero() const noexcept;

  friend class PerThreadTemporary;
  std::atomic<double> point;
  double function;
  double execution;
};

/// Accumulators and other related fields local to a Thread.
class PerThreadTemporary final {
public:
  // Access to the backing thread.
  operator Thread&() noexcept { return m_thread; }
  operator const Thread&() const noexcept { return m_thread; }
  Thread& thread() noexcept { return m_thread; }
  const Thread& thread() const noexcept { return m_thread; }

  // Movable, not copiable
  PerThreadTemporary(const PerThreadTemporary&) = delete;
  PerThreadTemporary(PerThreadTemporary&&) = default;
  PerThreadTemporary& operator=(const PerThreadTemporary&) = delete;
  PerThreadTemporary& operator=(PerThreadTemporary&&) = delete;

  /// Reference to the Metric data for a particular Context in this Thread.
  /// Returns `std::nullopt` if none is present.
  // MT: Safe (const), Unstable (before notifyThreadFinal)
  auto accumulatorsFor(const Context& c) const noexcept {
    return c_data.find(c);
  }

  /// Reference to all of the Metric data on Thread.
  // MT: Safe (const), Unstable (before notifyThreadFinal)
  const auto& accumulators() const noexcept { return c_data; }

private:
  Thread& m_thread;

  friend class ProfilePipeline;
  PerThreadTemporary(Thread& t) : m_thread(t) {};

  // Finalize the MetricAccumulators for a Thread.
  // MT: Internally Synchronized
  void finalize() noexcept;

  // Bits needed for handling timepoints
  std::chrono::nanoseconds minTime = std::chrono::nanoseconds::max();
  std::chrono::nanoseconds maxTime = std::chrono::nanoseconds::min();
  template<class Tp>
  struct TimepointsData {
    bool unboundedDisorder = false;
    util::bounded_streaming_sort_buffer<Tp, util::compare_only_first<Tp>> sortBuf;
    std::vector<Tp> staging;
  };
  TimepointsData<std::pair<std::chrono::nanoseconds,
    std::reference_wrapper<const Context>>> ctxTpData;
  util::locked_unordered_map<util::reference_index<const Metric>,
    TimepointsData<std::pair<std::chrono::nanoseconds, double>>> metricTpData;

  friend class Metric;
  util::locked_unordered_map<util::reference_index<const Context>,
    util::locked_unordered_map<util::reference_index<const Metric>,
      MetricAccumulator>> c_data;
  util::locked_unordered_map<util::reference_index<const ContextReconstruction>,
    util::locked_unordered_map<util::reference_index<const Metric>,
      MetricAccumulator>> r_data;

  struct RGroup {
    util::locked_unordered_map<util::reference_index<const Context>,
      util::locked_unordered_map<util::reference_index<const Metric>,
        MetricAccumulator>> c_data;
    util::locked_unordered_map<util::reference_index<const ContextFlowGraph>,
      util::locked_unordered_map<util::reference_index<const Metric>,
        MetricAccumulator>> fg_data;

    std::mutex lock;
    std::unordered_map<Scope,
      std::unordered_set<util::reference_index<Context>>> c_entries;
    std::unordered_map<util::reference_index<ContextFlowGraph>,
      std::unordered_set<util::reference_index<const ContextReconstruction>>>
        fg_reconsts;
  };
  util::locked_unordered_map<uint64_t, RGroup> r_groups;
};

/// Accumulator structure for the Statistics implicitly bound to a Context.
class StatisticAccumulator final {
private:
  /// Each Accumulator is a vector of Partial accumulators, each of which is
  /// bound implicitly to a particular Statistic::Partial.
  class Partial final {
  public:
    Partial() : point(0), function(0), execution(0) {};

    Partial(const Partial&) = delete;
    Partial& operator=(const Partial&) = delete;
    Partial(Partial&&) = default;
    Partial& operator=(Partial&&) = delete;
  private:
    void validate() const noexcept;

    friend class StatisticAccumulator;
    friend class PerThreadTemporary;
    std::atomic<double> point;
    std::atomic<double> function;
    std::atomic<double> execution;
  };

public:
  class PartialRef final {
  public:
    PartialRef() = delete;
    PartialRef(const PartialRef&) = delete;
    PartialRef& operator=(const PartialRef&) = delete;
    PartialRef(PartialRef&&) = default;
    PartialRef& operator=(PartialRef&&) = default;

    /// Add some value to this particular Partial, for a particular MetricScope.
    // MT: Internally Synchronized
    void add(MetricScope, double) noexcept;

    /// Get this Partial's accumulation, for a particular MetricScope.
    // MT: Safe (const), Unstable (before `metrics` wavefront)
    std::optional<double> get(MetricScope) const noexcept;

  private:
    friend class StatisticAccumulator;
    PartialRef(Partial& p, const StatisticPartial& sp)
      : partial(p), statpart(sp) {};

    Partial& partial;
    const StatisticPartial& statpart;
  };

  class PartialCRef final {
  public:
    PartialCRef() = delete;
    PartialCRef(const PartialCRef&) = delete;
    PartialCRef& operator=(const PartialCRef&) = delete;
    PartialCRef(PartialCRef&&) = default;
    PartialCRef& operator=(PartialCRef&&) = default;

    /// Get this Partial's accumulation, for a particular MetricScope.
    // MT: Safe (const), Unstable (before `metrics` wavefront)
    std::optional<double> get(MetricScope) const noexcept;

  private:
    friend class StatisticAccumulator;
    PartialCRef(const Partial& p, const StatisticPartial& sp)
      : partial(p), statpart(sp) {};

    const Partial& partial;
    const StatisticPartial& statpart;
  };

  StatisticAccumulator(const Metric&);

  StatisticAccumulator(const StatisticAccumulator&) = delete;
  StatisticAccumulator& operator=(const StatisticAccumulator&) = delete;
  StatisticAccumulator(StatisticAccumulator&&) = default;
  StatisticAccumulator& operator=(StatisticAccumulator&&) = delete;

  /// Get the Partial accumulator for a particular Partial Statistic.
  // MT: Safe (const)
  PartialCRef get(const StatisticPartial&) const noexcept;
  PartialRef get(const StatisticPartial&) noexcept;

private:
  friend class PerThreadTemporary;
  std::vector<Partial> partials;
};

/// Accumulators and related fields local to a Context. In particular, holds
/// the statistics.
class PerContextAccumulators final {
public:
  PerContextAccumulators() = default;
  ~PerContextAccumulators() = default;

  PerContextAccumulators(const PerContextAccumulators&) = delete;
  PerContextAccumulators(PerContextAccumulators&&) = default;
  PerContextAccumulators& operator=(const PerContextAccumulators&) = delete;
  PerContextAccumulators& operator=(PerContextAccumulators&&) = delete;

  /// Access the Statistics data attributed to this Context
  // MT: Safe (const), Unstable (before `metrics` wavefront)
  const auto& statistics() const noexcept { return m_statistics; }

  /// Access the Statistics data for a particular Metric, attributed to this Context
  // MT: Internally Synchronized
  StatisticAccumulator& statisticsFor(const Metric& m) noexcept;

  /// Helper wrapper to allow a MetricScopeSet to be atomically modified.
  /// Used for the metricUsage() data.
  class AtomicMetricScopeSet final {
  public:
    AtomicMetricScopeSet() : val(MetricScopeSet().toInt()) {}
    ~AtomicMetricScopeSet() = default;

    AtomicMetricScopeSet(const AtomicMetricScopeSet&) = delete;
    AtomicMetricScopeSet(AtomicMetricScopeSet&&) = delete;
    AtomicMetricScopeSet& operator=(const AtomicMetricScopeSet&) = delete;
    AtomicMetricScopeSet& operator=(AtomicMetricScopeSet&&) = delete;

    /// Union the given MetricScopeSet into this atomic set
    // MT: Internally Synchronized
    AtomicMetricScopeSet& operator|=(MetricScopeSet ms) noexcept {
      val.fetch_or(ms.toInt(), std::memory_order_relaxed);
      return *this;
    }

    /// Load the current value of the MetricScopeSet
    // MT: Safe (const), Unstable (before `metrics` wavefront)
    MetricScopeSet get() const noexcept {
      return MetricScopeSet(val.load(std::memory_order_relaxed));
    }
    operator MetricScopeSet() const noexcept { return get(); }

  private:
    std::atomic<MetricScopeSet::int_type> val;
  };

  /// Access the use of this Context in Metric values
  // MT: Safe (const), Unstable (before `metrics` wavefront)
  const auto& metricUsage() const noexcept { return m_metricUsage; }

  /// Get the use of this Context for a particular Metric.
  // MT: Safe (const), Unstable (before `metrics` wavefront)
  MetricScopeSet metricUsageFor(const Metric& m) const noexcept {
    if(auto use = m_metricUsage.find(m)) return use->get();
    return {};
  }

  /// Mark this Context as having been used for the given Metric values
  // MT: Internally Synchronized
  void markUsed(const Metric& m, MetricScopeSet ms) noexcept;

private:
  friend class PerThreadTemporary;
  friend class Metric;
  friend class ProfilePipeline;
  util::locked_unordered_map<util::reference_index<const Metric>,
    StatisticAccumulator> m_statistics;
  util::locked_unordered_map<util::reference_index<const Metric>,
    AtomicMetricScopeSet> m_metricUsage;
};

}  // namespace hpctoolkit

#endif  // HPCTOOLKIT_PROFILE_ACCUMULATORS_H

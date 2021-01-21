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

#ifndef HPCTOOLKIT_PROFILE_ACCUMULATORS_H
#define HPCTOOLKIT_PROFILE_ACCUMULATORS_H

#include <atomic>
#include <bitset>
#include "stdshim/optional.hpp"
#include <vector>

namespace hpctoolkit {

class Metric;
class StatisticPartial;

/// Every Metric can have values at multiple Scopes pertaining to the subtree
/// rooted at a particular Context with Metric data.
enum class MetricScope : size_t {
  /// Encapsulates the current Context, and no other nodes. This references
  /// exactly where the data arose, and is the smallest MetricScope.
  point,

  /// Encapsulates the current Context and any decendants not connected by a
  /// function-type Scope. This represents the cost of a function outside of
  /// any child function calls.
  /// Called "exclusive" in the Viewer.
  function,

  /// Encapsulates the current Context and all decendants. This represents
  /// the entire execution spawned by a single source code construct, and is
  /// the largest MetricScope.
  /// Called "inclusive" in the Viewer.
  execution,
};

/// Bitset-like object used as a set of Scope values.
class MetricScopeSet final : private std::bitset<3> {
private:
  using base = std::bitset<3>;
  MetricScopeSet(const base& b) : base(b) {};
public:
  MetricScopeSet() = default;
  MetricScopeSet(MetricScope s) : base(1<<static_cast<size_t>(s)) {};
  MetricScopeSet(std::initializer_list<MetricScope> l) : base(0) {
    for(const auto s: l) base::set(static_cast<size_t>(s));
  }

  static inline constexpr struct all_t {} all = {};
  MetricScopeSet(all_t) : base(0) { base::set(); }

  bool has(MetricScope s) const noexcept { return base::operator[](static_cast<size_t>(s)); }

  MetricScopeSet operator|(const MetricScopeSet& o) { return (base)*this | (base)o; }
  MetricScopeSet operator+(const MetricScopeSet& o) { return (base)*this | (base)o; }
  MetricScopeSet operator&(const MetricScopeSet& o) { return (base)*this & (base)o; }
  MetricScopeSet& operator|=(const MetricScopeSet& o) { base::operator|=(o); return *this; }
  MetricScopeSet& operator+=(const MetricScopeSet& o) { base::operator|=(o); return *this; }
  MetricScopeSet& operator&=(const MetricScopeSet& o) { base::operator&=(o); return *this; }

  using base::count;
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

  /// Get the Thread-local (:Sum) Statistic accumulation, for a particular Metric Scope.
  // MT: Safe (const), Unstable (before ThreadFinal wavefront)
  stdshim::optional<double> get(MetricScope) const noexcept;

private:
  void validate() const noexcept;

  friend class Metric;
  std::atomic<double> point;
  double function;
  double execution;
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
    friend class Metric;
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
    stdshim::optional<double> get(MetricScope) const noexcept;

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
    stdshim::optional<double> get(MetricScope) const noexcept;

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
  friend class Metric;
  std::vector<Partial> partials;
};

}

#endif  // HPCTOOLKIT_PROFILE_ACCUMULATORS_H

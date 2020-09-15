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

#ifndef HPCTOOLKIT_PROFILE_METRIC_H
#define HPCTOOLKIT_PROFILE_METRIC_H

#include "attributes.hpp"

#include "util/atomic_unordered.hpp"
#include "util/locked_unordered.hpp"
#include "util/uniqable.hpp"
#include "util/ragged_vector.hpp"

#include <atomic>
#include <bitset>
#include "stdshim/optional.hpp"
#include <vector>

namespace hpctoolkit {

class Context;

/// Every Metric can have values at multiple Scopes pertaining to the subtree
/// rooted at a particular Context with Metric data.
enum MetricScope {
  /// Encapsulates the current Context, and no other nodes. This references
  /// exactly where the data arose, and is the smallest Scope.
  point,

  /// Encapsulates the current Context and any direct children generated from
  /// the same source code location. This represents the rough "cause" of the
  /// data, and provides useful information on non-`point` Contexts.
  exclusive,

  /// Encapsulates the current Context and its entire subtree. This represents
  /// the entire execution contained within a single function call (or other
  /// source code construct).
  inclusive,
};

/// Bitset-like object used as a set of Scope values.
class MetricScopeSet final : private std::bitset<3> {
private:
  using base = std::bitset<3>;
  MetricScopeSet(const base& b) : base(b) {};
public:
  MetricScopeSet() = default;
  MetricScopeSet(MetricScope s) : base(1<<s) {};

  bool has(MetricScope s) { return base::operator[](s); }

  MetricScopeSet operator|(const MetricScopeSet& o) { return (base)*this | (base)o; }
  MetricScopeSet operator+(const MetricScopeSet& o) { return (base)*this | (base)o; }
  MetricScopeSet operator&(const MetricScopeSet& o) { return (base)*this & (base)o; }
  MetricScopeSet& operator|=(const MetricScopeSet& o) { base::operator|=(o); return *this; }
  MetricScopeSet& operator+=(const MetricScopeSet& o) { base::operator|=(o); return *this; }
  MetricScopeSet& operator&=(const MetricScopeSet& o) { base::operator&=(o); return *this; }

  using base::count;
};

/// Accumulator structure for the Statistics implicitly bound to a Context.
class StatisticAccumulator final {
public:
  StatisticAccumulator() : exclusive(0), inclusive(0) {};

private:
  friend class Metric;
  friend class AccumulatorCRef;
  friend class AccumulatorRef;
  // Currently only for :Sum Statistics
  std::atomic<double> exclusive;
  std::atomic<double> inclusive;
};

class AccumulatorCRef;
class AccumulatorRef;
class ThreadAccumulatorCRef;
class ThreadAccumulatorRef;

// Just a simple metric class, nothing to see here
class Metric final {
private:
  friend class AccumulatorCRef;
  friend class AccumulatorRef;

public:
  using ud_t = util::ragged_vector<const Metric&>;

  /// Set of identifiers unique to each Metric Scope that a Metric may have.
  struct ScopedIdentifiers final {
    unsigned int point;
    unsigned int exclusive;
    unsigned int inclusive;
    unsigned int get(MetricScope s) const noexcept;
  };

  /// Structure to be used for creating new Metrics. Encapsulates a number of
  /// smaller settings into a convienent structure.
  struct Settings final {
    std::string name;
    std::string description;

    bool operator==(const Settings& o) const noexcept {
      return name == o.name && description == o.description;
    }
  };

  Metric(ud_t::struct_t& rs, const Settings& s)
    : Metric(rs, Settings(s)) {};
  Metric(ud_t::struct_t& rs, Settings&& s)
    : userdata(rs, std::cref(*this)), u_settings(std::move(s)) {};
  Metric(Metric&& m)
    : userdata(std::move(m.userdata), std::cref(*this)),
      u_settings(std::move(m.u_settings)) {};

  const std::string& name() const { return u_settings().name; }
  const std::string& description() const { return u_settings().description; }

  mutable ud_t userdata;

  /// Get the set of Scopes that this Metric supports.
  MetricScopeSet scopes() const noexcept;

private:
  /// Obtain the AccumulatorRef for a particular Context.
  // MT: Internally Synchronized
  AccumulatorRef addTo(Context&) noexcept;

public:
  /// Obtain a read-only AccumulatorRef for a particular Context.
  // MT: Safe (const), Unstable (before `metrics` wavefront)
  AccumulatorCRef getFor(const Context& c) const noexcept;

private:
  /// Obtain the ThreadAccumulatorRef for a particular Context.
  // MT: Internally Synchronized
  ThreadAccumulatorRef addTo(Thread::Temporary&, Context&) noexcept;

public:
  /// Obtain a read-only ThreadAccumulatorRef for a particular Context.
  // MT: Safe (const), Unstable (before `metrics` wavefront)
  ThreadAccumulatorCRef getFor(const Thread::Temporary&, const Context& c) const noexcept;

private:
  util::uniqable_key<Settings> u_settings;

  friend class ProfilePipeline;
  // Finalize the MetricAccumulators for a Thread.
  // MT: Internally Synchronized
  static void finalize(Thread::Temporary& t) noexcept;

  friend class util::uniqued<Metric>;
  util::uniqable_key<Settings>& uniqable_key() { return u_settings; }
};

/// Constant reference to the Metric accumulators on a particular Context.
class AccumulatorCRef final {
public:
  AccumulatorCRef(AccumulatorCRef&&) = default;
  AccumulatorCRef& operator=(AccumulatorCRef&&) = default;

  /// Get the (:Sum) Statistic accumulation, for a particular Scope.
  // MT: Safe (const), Unstable (before `metrics` wavefront)
  stdshim::optional<double> get(MetricScope) const noexcept;

private:
  const StatisticAccumulator* accum;

  friend class Metric;
  AccumulatorCRef() : accum(nullptr) {};
  AccumulatorCRef(const StatisticAccumulator& a) : accum(&a) {};
};

/// Reference to the Metric accumulators on a particular Context.
class AccumulatorRef final {
public:
  AccumulatorRef(AccumulatorRef&&) = default;
  AccumulatorRef& operator=(AccumulatorRef&&) = default;

  /// Accumulate some extra (:Sum) Statistic value, for a particular Metric Scope.
  /// May invalidate other (C)AccumulatorRefs for the same Metric and Context.
  // MT: Internally Synchronized.
  void add(MetricScope, double) noexcept;

private:
  StatisticAccumulator* accum;

  friend class Metric;
  AccumulatorRef(StatisticAccumulator& a) : accum(&a) {};
};

/// Constant reference to the thread-local Metric accumulators on a particular Context.
class ThreadAccumulatorCRef final {
public:
  ThreadAccumulatorCRef(ThreadAccumulatorCRef&&) = default;
  ThreadAccumulatorCRef& operator=(ThreadAccumulatorCRef&&) = default;

  /// Get the Thread-local (:Sum) Statistic accumulation, for a particular Metric Scope.
  // MT: Safe (const), Unstable (before ThreadFinal wavefront)
  stdshim::optional<double> get(MetricScope) const noexcept;

private:
  const MetricAccumulator* accum;

  friend class Metric;
  ThreadAccumulatorCRef() : accum(nullptr) {};
  ThreadAccumulatorCRef(const MetricAccumulator& a) : accum(&a) {};
};

/// Reference to the thread-local Metric accumulators on a particular Context.
class ThreadAccumulatorRef final {
public:
  ThreadAccumulatorRef(ThreadAccumulatorRef&&) = default;
  ThreadAccumulatorRef& operator=(ThreadAccumulatorRef&&) = default;

  /// Accumulate some thread-local Metric data on the referenced Context.
  /// This is always point-Scope accumulation, there is no alternative.
  /// May invalidate other (C)ThreadAccumulatorRefs for the same Thread, Metric and Context.
  // MT: Internally Synchronized
  void add(double) noexcept;

private:
  MetricAccumulator* accum;

  friend class Metric;
  ThreadAccumulatorRef(MetricAccumulator& a) : accum(&a) {};
};

}

namespace std {
  using namespace hpctoolkit;
  template<> struct hash<Metric::Settings> {
    std::size_t operator()(const Metric::Settings&) const noexcept;
  };
}

#endif  // HPCTOOLKIT_PROFILE_METRIC_H

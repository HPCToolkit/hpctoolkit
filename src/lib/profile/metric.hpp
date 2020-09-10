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

#include "context.hpp"
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

// Just a simple metric class, nothing to see here
class Metric {
private:
  struct Accumulator {
    Accumulator() : exclusive(0), inclusive(0) {};
    ~Accumulator() = default;
    std::atomic<double> exclusive;
    std::atomic<double> inclusive;
  };
  struct ThreadLocal {
    ThreadLocal() = default;
    ~ThreadLocal() = default;
    util::locked_unordered_map<Context*, std::atomic<double>> exclusive;
    std::unordered_map<Context*, double> inclusive;
  };

public:
  using ud_t = util::ragged_vector<const Metric&>;

  /// Every Metric can have values at multiple Scopes pertaining to the subtree
  /// rooted at a particular Context with Metric data.
  enum Scope {
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
  class ScopeSet final : private std::bitset<3> {
  private:
    using base = std::bitset<3>;
    ScopeSet(const base& b) : base(b) {};
  public:
    ScopeSet() = default;
    ScopeSet(Scope s) : base(s) {};

    bool has(Scope s) { return base::operator[](s); }

    ScopeSet operator|(const ScopeSet& o) { return *this | o; }
    ScopeSet operator&(const ScopeSet& o) { return *this & o; }
    ScopeSet& operator|=(const ScopeSet& o) { base::operator|=(o); return *this; }
    ScopeSet& operator&=(const ScopeSet& o) { base::operator&=(o); return *this; }
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

  Metric(ud_t::struct_t& rs, Context::met_t::struct_t& ms,
         Thread::met_t::struct_t& ts, const Settings& s)
    : Metric(rs, ms, ts, Settings(s)) {};
  Metric(ud_t::struct_t& rs, Context::met_t::struct_t& ms,
         Thread::met_t::struct_t& ts, Settings&& s)
    : userdata(rs, std::cref(*this)), u_settings(std::move(s)),
      member(ms.add<Accumulator>()), tmember(ts.add<ThreadLocal>()) {};
  Metric(Metric&& m)
    : userdata(std::move(m.userdata), std::cref(*this)),
      u_settings(std::move(m.u_settings)), member(std::move(m.member)),
      tmember(std::move(m.tmember)) {};

  const std::string& name() const { return u_settings().name; }
  const std::string& description() const { return u_settings().description; }

  mutable ud_t userdata;

  /// Get the set of Scopes that this Metric supports.
  ScopeSet scopes() const noexcept;

  /// Constant reference to the Metric accumulators on a particular Context.
  class CAccumulatorRef final {
  public:
    CAccumulatorRef(CAccumulatorRef&&) = default;
    CAccumulatorRef& operator=(CAccumulatorRef&&) = default;

    /// Get the (:Sum) Statistic accumulation, for a particular Scope.
    // MT: Safe (const), Unstable (before `metrics` wavefront)
    stdshim::optional<double> get(Scope) const noexcept;

  private:
    const Accumulator* accum;

    friend class Metric;
    CAccumulatorRef() : accum(nullptr) {};
    CAccumulatorRef(const Accumulator& a) : accum(&a) {};
  };

  /// Reference to the Metric accumulators on a particular Context.
  class AccumulatorRef final {
  public:
    AccumulatorRef(AccumulatorRef&&) = default;
    AccumulatorRef& operator=(AccumulatorRef&&) = default;

    /// Accumulate some extra (:Sum) Statistic value, for a particular Scope.
    /// May invalidate other (C)AccumulatorRefs for the same Metric and Context.
    // MT: Internally Synchronized.
    void add(Scope, double) noexcept;

  private:
    Accumulator* accum;

    friend class Metric;
    AccumulatorRef(Accumulator& a) : accum(&a) {};
  };

  /// Obtain the AccumulatorRef for a particular Context.
  // MT: Internally Synchronized
  AccumulatorRef getFor(Context&) noexcept;

  /// Obtain a read-only AccumulatorRef for a particular Context.
  // MT: Safe (const), Unstable (before `metrics` wavefront)
  CAccumulatorRef getFor(const Context& c) const noexcept { return cgetFor(c); }
  CAccumulatorRef cgetFor(const Context& c) const noexcept;

  /// Constant reference to the thread-local Metric accumulators on a particular Context.
  class CThreadAccumulatorRef final {
  public:
    CThreadAccumulatorRef(CThreadAccumulatorRef&&) = default;
    CThreadAccumulatorRef& operator=(CThreadAccumulatorRef&&) = default;

    /// Get the Thread-local (:Sum) Statistic accumulation, for a particular Scope.
    // MT: Safe (const), Unstable (before ThreadFinal wavefront)
    stdshim::optional<double> get(Scope) const noexcept;

  private:
    const std::atomic<double>* exclusive;
    const double* inclusive;

    friend class Metric;
    CThreadAccumulatorRef()
      : exclusive(nullptr), inclusive(nullptr) {};
    CThreadAccumulatorRef(std::nullptr_t, const double& i)
      : exclusive(nullptr), inclusive(&i) {};
    CThreadAccumulatorRef(const std::atomic<double>& e, const double& i)
      : exclusive(&e), inclusive(&i) {};
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
    std::atomic<double>* exclusive;
    double* inclusive;

    friend class Metric;
    ThreadAccumulatorRef(std::atomic<double>& e, double& i)
      : exclusive(&e), inclusive(&i) {};
  };

  /// Obtain the ThreadAccumulatorRef for a particular Context.
  // MT: Internally Synchronized
  ThreadAccumulatorRef getFor(Thread::Temporary&, Context&) noexcept;

  /// Obtain a read-only ThreadAccumulatorRef for a particular Context.
  // MT: Safe (const), Unstable (before `metrics` wavefront)
  CThreadAccumulatorRef getFor(const Thread::Temporary& t, const Context& c) const noexcept { return cgetFor(t, c); }
  CThreadAccumulatorRef cgetFor(const Thread::Temporary&, const Context& c) const noexcept;

private:
  util::uniqable_key<Settings> u_settings;

  Context::met_t::typed_member_t<Accumulator> member;
  Thread::met_t::typed_member_t<ThreadLocal> tmember;

  friend class ProfilePipeline;
  // Finalize this Metric's Thread-local data. Non-destructive.
  // MT: Internally Synchronized
  void finalize(Thread::Temporary& t) noexcept;

  friend class util::uniqued<Metric>;
  util::uniqable_key<Settings>& uniqable_key() { return u_settings; }
};

}

namespace std {
  using namespace hpctoolkit;
  template<> struct hash<Metric::Settings> {
    std::size_t operator()(const Metric::Settings&) const noexcept;
  };
}

#endif  // HPCTOOLKIT_PROFILE_METRIC_H

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

#include "accumulators.hpp"
#include "attributes.hpp"

#include "util/atomic_unordered.hpp"
#include "util/locked_unordered.hpp"
#include "util/uniqable.hpp"
#include "util/ragged_vector.hpp"

#include <atomic>
#include <bitset>
#include <functional>
#include "stdshim/optional.hpp"
#include <variant>
#include <vector>

namespace hpctoolkit {

class Context;

class Metric;
class StatisticPartial;

/// A Statistic represents a combination of Metric data across all Threads,
/// on a per-Context basis. These are generated via three formulas:
///  - "accumulate": converts the thread-local value into an accumulation,
///  - "combine": combines two accumulations into a valid accumulation,
///  - "finalize": converts an accumulation into a presentable final value.
/// In total, this allows for condensed information regarding the entire
/// profiled execution, while still permitting inspection of individual Threads.
class Statistic final {
public:
  Statistic(const Statistic&) = delete;
  Statistic(Statistic&&) = default;
  Statistic& operator=(const Statistic&) = delete;
  Statistic& operator=(Statistic&&) = default;

  // Only a few combination formulas are permitted. This is the set.
  enum class combination_t { sum, min, max };

  // The other two formulas are best represented by C++ functions.
  using accumulate_t = std::function<double(double)>;
  using finalize_t = std::function<double(const std::vector<double>&)>;

  /// Statistics are created by the associated Metric.
  Statistic() = delete;

  /// Get the additional suffix associated with this Statistic.
  /// E.g. "Sum" or "Avg".
  // MT: Safe (const)
  const std::string& suffix() const noexcept { return m_suffix; }

  /// Get whether the percentage should be shown for this Statistic.
  /// TODO: Figure out what property this indicates mathematically
  // MT: Safe (const)
  bool showPercent() const noexcept { return m_showPerc; }

  /// Get whether this Statistic should be presented by default.
  // MT: Safe (const)
  bool visibleByDefault() const noexcept { return m_visibleByDefault; }

  /// Type for formulas. Each element is either a string or the index of a Partial.
  /// If all such indices are replaced by variable names and the entire vector
  /// concatinated, the result is a C-like math formula.
  using formula_t = std::vector<std::variant<size_t, std::string>>;

  /// Get the formula used generate the final value for this Statistic.
  // MT: Safe (const)
  const formula_t& finalizeFormula() const noexcept { return m_formula; }

private:
  const std::string m_suffix;
  const bool m_showPerc;
  const formula_t m_formula;
  const bool m_visibleByDefault;

  friend class Metric;
  Statistic(std::string, bool, formula_t, bool);
};

/// A StatisticPartial is the "accumulate" and "combine" parts of a Statistic.
/// There may be multiple Partials used for a Statistic, and multiple Statistics
/// can share the same Partial.
class StatisticPartial final {
public:
  StatisticPartial(const StatisticPartial&) = delete;
  StatisticPartial(StatisticPartial&&) = default;
  StatisticPartial& operator=(const StatisticPartial&) = delete;
  StatisticPartial& operator=(StatisticPartial&&) = default;

  /// Get the combination function used for this Partial
  // MT: Safe (const)
  Statistic::combination_t combinator() const noexcept { return m_combin; }

private:
  const Statistic::accumulate_t m_accum;
  const Statistic::combination_t m_combin;
  const std::size_t m_idx;

  friend class Metric;
  friend class StatisticAccumulator;
  StatisticPartial() = default;
  StatisticPartial(Statistic::accumulate_t a, Statistic::combination_t c, std::size_t idx)
    : m_accum(std::move(a)), m_combin(std::move(c)), m_idx(idx) {};
};

/// Metrics represent something that is measured at execution.
class Metric final {
public:
  using ud_t = util::ragged_vector<const Metric&>;

  /// Set of identifiers unique to each Metric Scope that a Metric may have.
  struct ScopedIdentifiers final {
    unsigned int point;
    unsigned int function;
    unsigned int execution;
    unsigned int get(MetricScope s) const noexcept;
  };

  /// Structure to be used for creating new Metrics. Encapsulates a number of
  /// smaller settings into a convienent structure.
  struct Settings {
    Settings()
      : scopes(MetricScopeSet::all), visibility(visibility_t::shownByDefault)
      {};
    Settings(std::string n, std::string d) : Settings() {
      name = std::move(n);
      description = std::move(d);
    }

    Settings(Settings&&) = default;
    Settings(const Settings&) = default;
    Settings& operator=(Settings&&) = default;
    Settings& operator=(const Settings&) = default;

    std::string name;
    std::string description;

    // The usual cost-based Metrics have all possible Scopes, but not all
    // Metrics do. This specifies which ones are available.
    MetricScopeSet scopes;

    // Metrics can have multiple visibilities depending on their needs.
    enum class visibility_t {
      shownByDefault, hiddenByDefault, invisible
    } visibility;

    bool operator==(const Settings& o) const noexcept {
      return name == o.name && description == o.description;
    }
  };

  /// Eventually the set of requested Statistics will be more general, but for
  /// now we just use an explicit bitfield.
  struct Statistics final {
    Statistics()
      : sum(false), mean(false), min(false), max(false), stddev(false),
        cfvar(false) {};

    bool sum : 1;
    bool mean : 1;
    bool min : 1;
    bool max : 1;
    bool stddev : 1;
    bool cfvar : 1;
  };

  const std::string& name() const noexcept { return u_settings().name; }
  const std::string& description() const noexcept { return u_settings().description; }
  MetricScopeSet scopes() const noexcept { return u_settings().scopes; }
  Settings::visibility_t visibility() const noexcept { return u_settings().visibility; }

  mutable ud_t userdata;

  /// List the StatisticPartials that are included in this Metric.
  // MT: Safe (const)
  const std::vector<StatisticPartial>& partials() const noexcept;

  /// List the Statistics that are included in this Metric.
  // MT: Safe (const)
  const std::vector<Statistic>& statistics() const noexcept;

  /// Obtain a pointer to the Statistic Accumulators for a particular Context.
  /// Returns `nullptr` if no Statistic data exists for the given Context.
  // MT: Safe (const), Unstable (before `metrics` wavefront)
  const StatisticAccumulator* getFor(const Context& c) const noexcept;

  /// Obtain a pointer to the Thread-local Accumulator for a particular Context.
  /// Returns `nullptr` if no metric data exists for the given Context.
  // MT: Safe (const), Unstable (before notifyThreadFinal)
  const MetricAccumulator* getFor(const Thread::Temporary&, const Context& c) const noexcept;

  Metric(Metric&& m);

  /// Special reference to a Metric for adding Statistics and whatnot.
  /// Useful for limiting access to a Metric for various purposes.
  class StatsAccess final {
  public:
    StatsAccess(Metric& m) : m(m) {};

    /// Request the given standard Statistics to be added to the referenced Metric.
    // MT: Internally Synchronized
    void requestStatistics(Statistics);

    /// Request a Partial representing the sum of relavent values.
    // MT: Internally Synchronized
    std::size_t requestSumPartial();

  private:
    Metric& m;

    std::unique_lock<std::mutex> synchronize();
  };

  /// Convience function to generate a StatsAccess handle.
  StatsAccess statsAccess() { return StatsAccess{*this}; }

private:
  util::uniqable_key<Settings> u_settings;
  Statistics m_thawed_stats;
  std::size_t m_thawed_sumPartial;
  std::mutex m_frozen_lock;
  std::atomic<bool> m_frozen;
  std::vector<StatisticPartial> m_partials;
  std::vector<Statistic> m_stats;

  friend class ProfilePipeline;
  Metric(ud_t::struct_t&, Settings);
  // "Freeze" the current Statistics and Partials. All requests through
  // StatsAccess must be processed before this point.
  // Returns true if this call was the one to perform the freeze.
  // MT: Internally Synchronized
  bool freeze();

  // Partially finalize the MetricAccumulators for a Thread, everything that can
  // be done before crossfinalize.
  // MT: Internally Synchronized
  static void prefinalize(Thread::Temporary& t) noexcept;

  // Test whether crossfinalization is needed for this Thread.
  // MT: Interally Synchronized
  static bool needsCrossfinalize(const Thread::Temporary& t) noexcept;

  // Partially finalize the MetricAccumulators for a set of Threads,
  // distributing across Threads if needed.
  // MT: Internally Synchronized
  static void crossfinalize(std::vector<Thread::Temporary>& ts) noexcept;

  // Finalize the MetricAccumulators for a Thread.
  // MT: Internally Synchronized
  static void finalize(Thread::Temporary& t) noexcept;

  friend class util::uniqued<Metric>;
  util::uniqable_key<Settings>& uniqable_key() { return u_settings; }
};

/// On occasion there is a need to add new Statistics that are based on values
/// from multiple Metrics instead of just one, usually to present abstract
/// overview values that can't be processed any other way. These are written
/// as "Extra" Statistics, which are much like Statistics but are not bound
/// to any particular Metric.
class ExtraStatistic final {
public:
  ExtraStatistic(ExtraStatistic&& o) = default;
  ExtraStatistic(const ExtraStatistic&) = delete;

  struct MetricPartialRef final {
    MetricPartialRef(const Metric& m, std::size_t i)
      : metric(m), partialIdx(i) {};
    const Metric& metric;
    std::size_t partialIdx;

    const StatisticPartial& partial() const noexcept {
      return metric.partials()[partialIdx];
    }

    bool operator==(const MetricPartialRef& o) const noexcept {
      return &metric == &o.metric && partialIdx == o.partialIdx;
    }
  };

  /// Every ExtraStatistic is defined by a formula like this. If every
  /// Metric/Partial pair is replaced by a variable name and the entire vector
  /// concatinated, the result is a C-like math formula.
  using formula_t = std::vector<std::variant<std::string, MetricPartialRef>>;

  /// The Settings for an ExtraStatistic are much like those for standard
  /// Metrics, with a few additions.
  struct Settings : public Metric::Settings {
    Settings() : Metric::Settings(), showPercent(true) {};
    Settings(Metric::Settings&& s)
      : Metric::Settings(std::move(s)), showPercent(true) {};

    Settings(Settings&& o) = default;
    Settings(const Settings&) = default;
    Settings& operator=(Settings&&) = default;
    Settings& operator=(const Settings&) = default;

    /// Whether the percentage should be shown for this ExtraStatistic.
    /// TODO: Figure out what property this indicates mathematically
    bool showPercent : 1;

    /// Formula used to calculate the values for the ExtraStatistic.
    formula_t formula;

    /// Printf-style format string to use when rendering resulting values.
    /// This should only contain a single conversion specification (argument)
    std::string format;

    bool operator==(const Settings& o) const noexcept;
  };

  /// ExtraStatistics are only ever created by the Pipeline.
  ExtraStatistic() = delete;

  const std::string& name() const noexcept { return u_settings().name; }
  const std::string& description() const noexcept { return u_settings().description; }
  MetricScopeSet scopes() const noexcept { return u_settings().scopes; }
  Settings::visibility_t visibility() const noexcept { return u_settings().visibility; }
  bool showPercent() const noexcept { return u_settings().showPercent; }
  const formula_t& formula() const noexcept { return u_settings().formula; }
  const std::string& format() const noexcept { return u_settings().format; }

  /// Get whether this Statistic should be presented by default.
  // MT: Safe (const)
  bool visibleByDefault() const noexcept { return u_settings().visibility == Settings::visibility_t::shownByDefault; }

private:
  util::uniqable_key<Settings> u_settings;

  friend class ProfilePipeline;
  ExtraStatistic(Settings);

  friend class util::uniqued<ExtraStatistic>;
  util::uniqable_key<Settings>& uniqable_key() { return u_settings; }
};

}

namespace std {
  using namespace hpctoolkit;
  template<> struct hash<Metric::Settings> {
    std::size_t operator()(const Metric::Settings&) const noexcept;
  };
  template<> struct hash<ExtraStatistic::Settings> {
    std::hash<Metric::Settings> h;
    std::size_t operator()(const ExtraStatistic::Settings& s) const noexcept {
      return h(s);
    }
  };
}

#endif  // HPCTOOLKIT_PROFILE_METRIC_H

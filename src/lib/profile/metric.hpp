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
#include "stdshim/optional.hpp"
#include <vector>

namespace hpctoolkit {

class Context;

// Just a simple metric class, nothing to see here
class Metric final {
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

  /// Obtain a reference to the Statistic Accumulators for a particular Context.
  /// Returns an empty optional if no Statistic data exists for the given Context.
  // MT: Safe (const), Unstable (before `metrics` wavefront)
  stdshim::optional<const StatisticAccumulator&> getFor(const Context& c) const noexcept;

  /// Obtain a reference to the Thread-local Accumulator for a particular Context.
  /// Returns an empty optional if no metric data exists for the given Context.
  // MT: Safe (const), Unstable (before notifyThreadFinal)
  stdshim::optional<const MetricAccumulator&> getFor(const Thread::Temporary&, const Context& c) const noexcept;

private:
  util::uniqable_key<Settings> u_settings;

  friend class ProfilePipeline;
  // Finalize the MetricAccumulators for a Thread.
  // MT: Internally Synchronized
  static void finalize(Thread::Temporary& t) noexcept;

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

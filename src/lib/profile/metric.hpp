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

  /// Enumeration for the Metric's type. Classifies the various Metrics that are
  /// available and could be emitted.
  enum Type {
    /// Artifical Metrics are never measured directly, instead their data is
    /// generated from a more complex process, usually by the Source.
    /// These have neither exclusive or inclusive Metric accumulations.
    /// Until further notice, no thread-local data can be emitted for artifical Metrics.
    artifical,

    /// Linear Metrics are ones where the measurement space is a group, that is,
    /// sums make rational sense.
    /// These have both exclusive and inclusive Metric accumulations.
    linear,
  };

  /// Structure to be used for creating new Metrics. Encapsulates a number of
  /// smaller settings into a convienent structure.
  struct Settings final {
    std::string name;
    std::string description;
    Type type;

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

  Type type() const noexcept { return u_settings().type; }
  const std::string& name() const { return u_settings().name; }
  const std::string& description() const { return u_settings().description; }

  mutable ud_t userdata;

  // Accumulate this Metric on a Context for a particular Thread.
  // MT: Internally Synchronized
  void add(Thread::Temporary& t, Context&, double) const noexcept;

  // Finalize this Metric's Thread-local data. Non-destructive.
  // MT: Internally Synchronized
  void finalize(Thread::Temporary& t) noexcept;

  // Accumulate this Metric's Statistics on a Context.
  // Temporary interface.
  // MT: Internally Synchronized
  void add(Context&, std::pair<double, double>) const noexcept;

  // Get the Statistic accumulations on a Context.
  // Temporary interface.
  // MT: Safe (const), Unstable (before `metrics` wavefront)
  std::pair<double, double> getFor(const Context&) const noexcept;

  // Get the Statistic accumulation from a particular Thread, on a Context.
  // Temporary interface.
  // MT: Safe (const), Unstable (before ThreadFinal wavefront)
  std::pair<double, double> getFor(const Thread::Temporary& t, const Context&) const noexcept;

private:
  util::uniqable_key<Settings> u_settings;

  Context::met_t::typed_member_t<Accumulator> member;
  Thread::met_t::typed_member_t<ThreadLocal> tmember;

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

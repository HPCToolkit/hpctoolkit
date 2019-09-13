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

#ifndef HPCTOOLKIT_PROFILE_PROFILESINKS_HPCMETRICDB_H
#define HPCTOOLKIT_PROFILE_PROFILESINKS_HPCMETRICDB_H

#include "../profilesink.hpp"

#include "../util/once.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include "../stdshim/filesystem.hpp"

namespace hpctoolkit::profilesinks {

class ExperimentXML;

/// ProfileSink for the metricdb format.
class HPCMetricDB final : public ProfileSink {
public:
  ~HPCMetricDB() = default;

  /// Constructor, with a reference to the output database directory.
  HPCMetricDB(const stdshim::filesystem::path&, bool separated = false);
  HPCMetricDB(stdshim::filesystem::path&&, bool separated = false);

  /// Write out as much data as possible. See ProfileSink::write.
  bool write(std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max());
  bool help(std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max());

  DataClass accepts() const noexcept {
    using namespace hpctoolkit::literals;
    return "mc"_dat;
  }

  ExtensionClass requires() const noexcept {
    return ExtensionClass::identifier;
  }

  void notifyPipeline();

  void notifyContextData(const Thread&, const Context&, const Metric&, double);

  /// Return the tag for the experiment.xml, or an empty tag if empty.
  std::string exmlTag();

private:
  bool separated;
  stdshim::filesystem::path dir;

  internal::Once ready;

  struct state_t {
    stdshim::filesystem::path path;
    std::size_t base;
    std::size_t perthread;
    std::size_t perline;
    std::vector<const Thread*> threads;
    std::vector<const Metric*> metrics;
    std::vector<const Context*> contexts;
  };
  const state_t* state;
  std::atomic<std::size_t> threadHelp;
  std::atomic<std::size_t> threadDone;

  struct udThread {
    udThread() = default;
    udThread(const Thread&) : udThread() {};
    ~udThread() = default;

    std::unordered_map<const Metric*,
      internal::atomic_unordered_map<const Context*, MetricDatum>> data;
  };
  Thread::ud_t::typed_member_t<udThread> ud_thread;

  struct udMetric {
    udMetric(const Metric&, HPCMetricDB&);
    ~udMetric() = default;

    std::string inc_open;
    std::string ex_open;
  };
  Metric::ud_t::typed_member_t<udMetric> ud_metric;
};

}

#endif  // HPCTOOLKIT_PROFILE_PROFILESINKS_HPCMETRICDB_H

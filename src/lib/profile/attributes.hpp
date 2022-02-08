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

#ifndef HPCTOOLKIT_PROFILE_ATTRIBUTES_H
#define HPCTOOLKIT_PROFILE_ATTRIBUTES_H

#include "accumulators.hpp"
#include "scope.hpp"

#include "util/ragged_vector.hpp"
#include "util/ref_wrappers.hpp"
#include "util/streaming_sort.hpp"
#include "lib/prof-lean/id-tuple.h"

#include "stdshim/filesystem.hpp"
#include <functional>
#include <optional>
#include <thread>
#include <unordered_map>

namespace hpctoolkit {

class Context;
class ContextReconstruction;
class ContextFlowGraph;
class Metric;

/// Attributes unique to a particular thread within a profile. Whether this is
/// a thread (as in thd_create) or a process (as in an MPI process), this
/// roughly represents a single serial execution within the profile.
class ThreadAttributes {
public:
  // Default (empty) constructor.
  ThreadAttributes();
  ~ThreadAttributes() = default;

  ThreadAttributes(const ThreadAttributes& o) = default;
  ThreadAttributes& operator=(const ThreadAttributes& o) = default;
  ThreadAttributes(ThreadAttributes&& o) = default;
  ThreadAttributes& operator=(ThreadAttributes&& o) = default;

  /// Check whether this ThreadAttributes is suitable for creating a Thread.
  // MT: Safe (const)
  bool ok() const noexcept;

  /// Get or set the hierarchical tuple assigned to this Thread.
  /// Cannot be set to an empty vector.
  // MT: Externally Synchronized
  const std::vector<pms_id_t>& idTuple() const noexcept;
  void idTuple(std::vector<pms_id_t>);

  /// Get or set the vital statistics (maximum count and expected disorder) for
  /// the Context-type timepoints in this Thread.
  // MT: Externally Synchronized
  unsigned long long ctxTimepointMaxCount() const noexcept;
  void ctxTimepointStats(unsigned long long cnt, unsigned int disorder) noexcept;

  /// Get or set the vital statistics (maximum count and expected disorder) for
  /// the Metric-type timepoints in this Thread for the given Metric.
  // MT: Externally Synchronized
  unsigned long long metricTimepointMaxCount(const Metric&) const noexcept;
  void metricTimepointStats(const Metric&, unsigned long long cnt, unsigned int disorder);

private:
  std::vector<pms_id_t> m_idTuple;
  std::optional<std::pair<unsigned long long, unsigned int>> m_ctxTimepointStats;
  std::unordered_map<util::reference_index<const Metric>,
      std::pair<unsigned long long, unsigned int>> m_metricTimepointStats;

  friend class ProfilePipeline;
  unsigned int ctxTimepointDisorder() const noexcept;
  unsigned int metricTimepointDisorder(const Metric&) const noexcept;
  const auto& metricTimepointDisorders() const noexcept {
    return m_metricTimepointStats;
  }

  class FinalizeState {
  public:
    FinalizeState();
    ~FinalizeState();

    FinalizeState(FinalizeState&&) = delete;
    FinalizeState& operator=(FinalizeState&&) = delete;
    FinalizeState(const FinalizeState&) = delete;
    FinalizeState& operator=(const FinalizeState&) = delete;

  private:
    friend class ThreadAttributes;

    class CountingLookupMap {
      std::mutex lock;
      std::unordered_map<uint64_t, uint64_t> map;
    public:
      CountingLookupMap() = default;
      ~CountingLookupMap() = default;

      uint64_t get(uint64_t);
    };
    struct LocalHash {
      std::hash<uint16_t> h_u16;
      std::size_t operator()(const std::vector<uint16_t>& v) const noexcept;
    };
    util::locked_unordered_map<uint16_t, CountingLookupMap> globalIdxMap;
    util::locked_unordered_map<std::vector<uint16_t>, CountingLookupMap,
                               std::shared_mutex, LocalHash> localIdxMap;

    std::thread server;
    std::mutex mpilock;
  };

  // Finalize this ThreadAttributes using the given shared state.
  void finalize(FinalizeState&);
};

/// A single Thread within a Profile. Or something like that.
class Thread {
public:
  using ud_t = util::ragged_vector<const Thread&>;

  Thread(ud_t::struct_t& rs, ThreadAttributes attr);
  Thread(Thread&& o)
    : userdata(std::move(o.userdata), std::cref(*this)),
      attributes(std::move(o.attributes)) {};

  mutable ud_t userdata;

  /// Attributes accociated with this Thread.
  ThreadAttributes attributes;
};

/// Attributes unique to a particular profile. Since a profile represents an
/// entire single execution (potentially on multiple processors), all of these
/// should be constant within a profile. If it isn't, we just issue warnings.
class ProfileAttributes {
public:
  /// Default (empty) constructor. No news is no news.
  ProfileAttributes();
  ~ProfileAttributes() = default;

  /// Get or set the name of the program being executed. Usually this is the
  /// basename of the path, but just in case it isn't always...
  // MT: Externally Synchronized
  const std::optional<std::string>& name() const noexcept { return m_name; }
  void name(const std::string& s) { name(std::string(s)); };
  void name(std::string&&);

  /// Get or set the path to the program that was profiled.
  // MT: Externally Synchronized
  const std::optional<stdshim::filesystem::path>& path() const noexcept { return m_path; }
  void path(const stdshim::filesystem::path& p) {
    path(stdshim::filesystem::path(p));
  }
  void path(stdshim::filesystem::path&&);

  /// Get or set the job number corrosponding to the profiled program.
  /// If the job number is not known, has_job() will return false and job()
  /// will throw an error.
  // MT: Externally Synchronized
  const std::optional<unsigned long>& job() const noexcept { return m_job; }
  void job(unsigned long);

  /// Get or set individual environment variables. Note that the getter may
  /// return `nullptr` if the variable is not known to be set.
  // MT: Externally Synchronized
  const std::string* environment(const std::string& var) const noexcept;
  void environment(std::string var, std::string val);

  /// Access the entire environment for this profile.
  // MT: Unstable (const)
  const std::unordered_map<std::string, std::string>& environment() const noexcept {
    return m_env;
  }

  /// Append to the hierarchical tuple name dictionary
  // MT: Externally Synchronized
  void idtupleName(uint16_t kind, std::string name);

  /// Access the entire hierarchical tuple name dictionary
  // MT: Unstable (const)
  const std::unordered_map<uint16_t, std::string>& idtupleNames() const noexcept {
    return m_idtupleNames;
  }

  /// Merge another PAttr into this one. Uses the given callback to issue
  /// human-readable warnings (at least for now). Returns true if the process
  /// proceeded without errors.
  // MT: Externally Synchronized
  bool merge(const ProfileAttributes&);

private:
  std::optional<std::string> m_name;
  std::optional<unsigned long> m_job;
  std::optional<stdshim::filesystem::path> m_path;
  std::unordered_map<std::string, std::string> m_env;
  std::unordered_map<uint16_t, std::string> m_idtupleNames;
};

}

namespace std {
  std::ostream& operator<<(std::ostream&, const hpctoolkit::ThreadAttributes&) noexcept;
}

#endif  // HPCTOOLKIT_PROFILE_ATTRIBUTES_H

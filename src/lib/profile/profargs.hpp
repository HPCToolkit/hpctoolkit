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
// Copyright ((c)) 2020, Rice University
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

#ifndef HPCTOOLKIT_PROFILE_PROFARGS_H
#define HPCTOOLKIT_PROFILE_PROFARGS_H

#include "metricsource.hpp"
#include "finalizer.hpp"

#include "stdshim/filesystem.hpp"
#include <functional>

namespace hpctoolkit {

/// Argument parser front-end for hpcprof{,-mpi}.
class ProfArgs {
public:
  /// Parse the command line and fill *this
  ProfArgs(int, const char* const*);
  ~ProfArgs() = default;

  /// Sources and corrosponding paths specified as arguments.
  std::vector<std::pair<std::unique_ptr<MetricSource>, stdshim::filesystem::path>> sources;

  /// (Structfile) Finalizers and corrosponding paths specified as arguments.
  std::vector<std::pair<std::unique_ptr<Finalizer>, stdshim::filesystem::path>> structs;

  /// Title for the resulting database.
  std::string title;

  /// Path prefix transformation pairs
  std::unordered_map<stdshim::filesystem::path, stdshim::filesystem::path> prefixes;

  /// Path prefix expansion function and bound function
  std::vector<stdshim::filesystem::path> prefix(const stdshim::filesystem::path&) const noexcept;

  /// Number of threads to use for processing
  unsigned int threads;

  /// Whether to emit line-level or instruction-level data
  bool instructionGrain;

  /// Summary metrics to include in the output
  // NOTE: For now only :Sum is supported and always on. In the near future
  //       this will change, and it will be done via Finalizers.
  std::vector<std::string> stats;

  /// Path for the root database directory, or output file
  stdshim::filesystem::path output;

  /// Whether to copy sources into the output database
  bool include_sources;

  /// Whether to include trace data in the output database
  bool include_traces;

  /// Whether to include thread-local data in the output database
  bool include_thread_local;

  /// Enum for possible output formats for profile data
  enum class Format {
    // experiment.xml + metric-db + hpctrace, the current database format.
    exmldb,
  };
  /// Requested output format
  Format format;
};

}

#endif  // HPCTOOLKIT_PROFILE_PROFARGS_H

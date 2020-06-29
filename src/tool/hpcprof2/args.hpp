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

#ifndef HPCTOOLKIT_PROF2_ARGS_H
#define HPCTOOLKIT_PROF2_ARGS_H

#include "lib/profile/source.hpp"
#include "lib/profile/finalizer.hpp"

#include "lib/profile/stdshim/filesystem.hpp"
#include <functional>

namespace hpctoolkit {

/// Argument parser front-end for hpcprof{,-mpi}.
class ProfArgs {
public:
  /// Parse the command line and fill *this
  ProfArgs(int, char* const*);
  ~ProfArgs() = default;

  /// Sources and corrosponding paths specified as arguments.
  std::vector<std::pair<std::unique_ptr<ProfileSource>, stdshim::filesystem::path>> sources;

  /// (Structfile) Finalizers and corrosponding paths specified as arguments.
  std::vector<std::pair<std::unique_ptr<ProfileFinalizer>, stdshim::filesystem::path>> structs;

  /// Struct file warning Finalizer
  class StructWarner final : public ProfileFinalizer {
  public:
    StructWarner(ProfArgs& a) : args(a) {};
    ~StructWarner() = default;

    ExtensionClass provides() const noexcept override { return ExtensionClass::classification; }
    ExtensionClass requires() const noexcept override { return {}; }
    void module(const Module&, Classification&) override;

  private:
    ProfArgs& args;
  };

  /// Title for the resulting database.
  std::string title;

  /// Path prefix transformation pairs
  std::unordered_map<stdshim::filesystem::path, stdshim::filesystem::path> prefixes;

  /// Path prefix expansion Finalizer
  class Prefixer final : public ProfileFinalizer {
  public:
    Prefixer(ProfArgs& a) : args(a) {};
    ~Prefixer() = default;

    ExtensionClass provides() const noexcept override { return ExtensionClass::resolvedPath; }
    ExtensionClass requires() const noexcept override { return {}; }
    void file(const File&, stdshim::filesystem::path&) override;
    void module(const Module&, stdshim::filesystem::path&) override;

  private:
    ProfArgs& args;
  };

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
    // experiment.xml + sparse thing, the indev database format.
    sparse,
  };
  /// Requested output format
  Format format;

private:
  std::unordered_map<stdshim::filesystem::path, std::vector<stdshim::filesystem::path>> structheads;
};

}

#endif  // HPCTOOLKIT_PROF2_ARGS_H

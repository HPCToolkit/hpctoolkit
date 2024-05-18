// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROF2_ARGS_H
#define HPCTOOLKIT_PROF2_ARGS_H

#include "source.hpp"
#include "finalizer.hpp"

#include "stdshim/filesystem.hpp"
#include <functional>

namespace hpctoolkit {

/// Argument parser front-end for hpcprof{,-mpi}.
class ProfArgs {
public:
  /// Parse the command line and fill *this
  ProfArgs(int, char* const*);
  ~ProfArgs() = default;

  /// Sources and corresponding paths specified as arguments.
  std::vector<std::pair<std::unique_ptr<ProfileSource>, stdshim::filesystem::path>> sources;

  /// Index of the argument group the source belongs to.
  std::vector<std::size_t> source_args;

  /// KernelSymbols Finalizers from properly named measurements directories
  std::vector<std::pair<std::unique_ptr<ProfileFinalizer>, stdshim::filesystem::path>> ksyms;

  /// (Structfile) Finalizers and corresponding paths specified as arguments.
  std::vector<std::pair<std::unique_ptr<ProfileFinalizer>, stdshim::filesystem::path>> structs;

  /// Finalizer that warns when a Structfile is present but missed due to path differences
  class StructPartialMatch final : public ProfileFinalizer {
  public:
    StructPartialMatch(ProfArgs& a) : args(a) {};
    ~StructPartialMatch() = default;

    ExtensionClass provides() const noexcept override { return ExtensionClass::classification; }
    ExtensionClass requirements() const noexcept override { return {}; }
    std::optional<std::pair<util::optional_ref<Context>, Context&>>
    classify(Context&, NestedScope&) noexcept override;
    bool resolve(ContextFlowGraph&) noexcept override;

  private:
    ProfArgs& args;
  };

  /// Title for the resulting database.
  std::string title;

  /// Path prefix transformation pairs
  std::unordered_map<stdshim::filesystem::path, stdshim::filesystem::path,
                     stdshim::hash_path> prefixes;

  /// Statistics adding Transformer
  class StatisticsExtender final : public ProfileFinalizer {
  public:
    StatisticsExtender(ProfArgs& a) : args(a) {};
    ~StatisticsExtender() = default;

    ExtensionClass provides() const noexcept override {
      return ExtensionClass::statistics;
    }
    ExtensionClass requirements() const noexcept override { return {}; }

    void appendStatistics(const Metric&, Metric::StatsAccess) noexcept override;

  private:
    ProfArgs& args;
  };

  /// Path prefix expansion Finalizer
  class Prefixer final : public ProfileFinalizer {
  public:
    Prefixer(ProfArgs& a) : args(a) {};
    ~Prefixer() = default;

    ExtensionClass provides() const noexcept override { return ExtensionClass::resolvedPath; }
    ExtensionClass requirements() const noexcept override { return {}; }
    std::optional<stdshim::filesystem::path> resolvePath(const File&) noexcept override;
    std::optional<stdshim::filesystem::path> resolvePath(const Module&) noexcept override;

  private:
    ProfArgs& args;
  };

  /// Number of threads to use for processing
  unsigned int threads;

  /// Summary Statistics to include in the output
  struct Stats final {
    bool sum : 1;
    bool mean : 1;
    bool min : 1;
    bool max : 1;
    bool stddev : 1;
    bool cfvar : 1;

    Stats() : sum(true), mean(false), min(false), max(false), stddev(false),
              cfvar(false) {};
  } stats;

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
    /// *.db + metrics.yaml, the current database format
    metadb,
  };
  /// Requested output format
  Format format;

  /// Maximum size (in bytes) to use DWARF parsing for.
  uintmax_t dwarfMaxSize;

  /// Whether to enable "Valgrind-unclean" mode, which disables some deallocations.
  bool valgrindUnclean;

private:
  bool foreign;
  std::once_flag onceMissingGPUCFGs;
  std::unordered_set<stdshim::filesystem::path, stdshim::hash_path> structpaths;
  std::unordered_map<stdshim::filesystem::path, std::vector<stdshim::filesystem::path>,
                     stdshim::hash_path> structheads;
  std::unordered_set<stdshim::filesystem::path, stdshim::hash_path> allowedForeignDirs;
};

}

#endif  // HPCTOOLKIT_PROF2_ARGS_H

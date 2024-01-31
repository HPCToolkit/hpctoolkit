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
// Copyright ((c)) 2002-2024, Rice University
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

#ifndef HPCTOOLKIT_PROFILE_SOURCES_HPCRUN4_H
#define HPCTOOLKIT_PROFILE_SOURCES_HPCRUN4_H

#include "../expression.hpp"
#include "../source.hpp"

#include "../util/locked_unordered.hpp"
#include "../util/ref_wrappers.hpp"

#include <memory>
#include "../stdshim/filesystem.hpp"

// Forward declaration of a structure.
extern "C" typedef struct hpcrun_sparse_file hpcrun_sparse_file_t;

namespace hpctoolkit::sources {

/// ProfileSource for version 4.0 of the hpcrun file format. Uses the new
/// sparse format for bits.
class Hpcrun4 final : public ProfileSource {
public:
  ~Hpcrun4();

  // You shouldn't create these directly, use ProfileSource::create_for
  Hpcrun4() = delete;

  bool valid() const noexcept override;

  /// Get the basename of the measured executable.
  std::string exe_basename() const;

  /// Read in enough data to satisfy a request or until a timeout is reached.
  /// See `ProfileSource::read(...)`.
  void read(const DataClass&) override;

  DataClass provides() const noexcept override;
  DataClass finalizeRequest(const DataClass&) const noexcept override;

private:
  bool realread(const DataClass&);

  // Transfer of attributes from header-open time to read-time.
  bool fileValid;
  bool attrsValid;
  ProfileAttributes attrs;
  bool tattrsValid;
  ThreadAttributes tattrs;

  // Tracefile setup and arrangements.
  bool setupTrace(unsigned int) noexcept;
  PerThreadTemporary* thread;
  bool callTrace;

  // The actual file. Details for reading handled in prof-lean.
  hpcrun_sparse_file_t* file;
  stdshim::filesystem::path path;

  struct metric_t {
    metric_t(Metric& metric) : metric(metric) {};
    Metric& metric;
    // Multiplicitive factor applied to all metric values before injection
    uint64_t factor = 1;
    // If true, the Metric applies to the relation instead of the full Context
    bool isRelation : 1;
    // If true, the values should be interpreted as ints instead of floats
    bool isInt : 1;
  };

  // ID to Metric mapping.
  std::unordered_map<unsigned int, metric_t> metrics;

  // ID to Module mapping.
  std::unordered_map<unsigned int, Module&> modules;

  // Recursive functions for parsing formulas into Expressions
  std::optional<std::tuple<Expression::Kind, int, bool, unsigned int>>
    peekFormulaOperator(std::istream&) const;
  Expression parseFormulaPrimary(std::istream&) const;
  Expression parseFormula1(std::istream&, Expression, int) const;
  Expression parseFormula(std::istream&) const;

  // Simple single Context.
  struct singleCtx_t {
    singleCtx_t(util::optional_ref<Context> par, std::pair<Context&, Context&> ctxs)
      : par(par), rel(ctxs.first), full(ctxs.second) {};
    singleCtx_t(util::optional_ref<Context> par, Context& rel, Context& full)
      : par(par), rel(rel), full(full) {};
    util::optional_ref<Context> par;  ///< Parent Context
    Context& rel;  ///< Context referring to the Relation
    Context& full;  ///< Full nested Context
  };
  // Inlined Reconstruction (eg. GPU PC sampling in serialized mode).
  struct reconstructedCtx_t { ContextReconstruction& ctx; };
  // Reference to an outlined range tree, GPU context node. Has no metrics.
  // first.par is the root, first.rel is the entry-Context.
  using refRangeContext_t = std::pair<const singleCtx_t&, PerThreadTemporary&>;
  // Reference to an outlined range tree, range node. Has kernel metrics.
  using refRange_t = std::pair<const refRangeContext_t&, uint64_t /* group id */>;
  // Outlined range tree root. Has no metrics, never actually represented.
  using outlinedRangeRoot_t = int;
  // Outlined range tree, GPU context node. Has no metrics.
  struct outlinedRangeContext_t { PerThreadTemporary& thread; };
  // Outlined range tree, range node. Has no metrics.
  using outlinedRange_t = std::pair<PerThreadTemporary&, uint64_t>;
  // Outlined range tree, sample node. Has instruction-level metrics.
  using outlinedRangeSample_t = std::pair<const outlinedRange_t&, ContextFlowGraph&>;

  // ID to Context-like mapping.
  std::unordered_map<unsigned int, std::variant<
      singleCtx_t,
      reconstructedCtx_t,
      refRangeContext_t,
      refRange_t,
      outlinedRangeRoot_t,
      outlinedRangeContext_t,
      outlinedRange_t,
      outlinedRangeSample_t
    >> nodes;

  // Flag for whether we've warned about top-level context demotion
  bool warned_top_demotion = false;

  // Path to the tracefile, and offset of the actual data blob.
  stdshim::filesystem::path tracepath;
  long trace_off;
  bool trace_sort;

  // We're all friends here.
  friend std::unique_ptr<ProfileSource> ProfileSource::create_for(const stdshim::filesystem::path&);
  Hpcrun4(const stdshim::filesystem::path&);
};

}

#endif  // HPCTOOLKIT_PROFILE_SOURCES_HPCRUN4_H

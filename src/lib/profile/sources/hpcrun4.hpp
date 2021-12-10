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

#ifndef HPCTOOLKIT_PROFILE_SOURCES_HPCRUN4_H
#define HPCTOOLKIT_PROFILE_SOURCES_HPCRUN4_H

#include "../source.hpp"

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

  // The actual file. Details for reading handled in prof-lean.
  hpcrun_sparse_file_t* file;
  stdshim::filesystem::path path;

  // ID to Metric mapping. Also marks whether the values are in int format.
  std::unordered_map<unsigned int, std::pair<Metric&, bool>> metrics;

  // ID to Module mapping.
  std::unordered_map<unsigned int, Module&> modules;

  // ID to Context-like mapping.
  std::unordered_map<unsigned int, std::variant<
      // Simple single Context. First term is the parent, second is the Context.
      std::pair<Context*, Context*>,
      // Inlined Reconstruction (eg. GPU PC sampling in serialized mode).
      ContextReconstruction*,
      // Reference to an outlined range tree, GPU context node. Has no metrics.
      // First Context is the root, second is the entry-Context.
      std::pair<const std::pair<Context*, Context*>*, PerThreadTemporary*>,
      // Reference to an outlined range tree, range node. Has kernel metrics.
      std::pair<const std::pair<const std::pair<Context*, Context*>*, PerThreadTemporary*>*, uint64_t /* group id */>,
      // Outlined range tree root. Has no metrics, never actually represented.
      int,
      // Outlined range tree, GPU context node. Has no metrics.
      PerThreadTemporary*,
      // Outlined range tree, range node. Has no metrics.
      std::pair<PerThreadTemporary*, uint64_t>,
      // Outlined range tree, sample node. Has instruction-level metrics.
      std::pair<const std::pair<PerThreadTemporary*, uint64_t>*, ContextFlowGraph*>
    >> nodes;

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

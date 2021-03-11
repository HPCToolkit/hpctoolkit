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

  /// Read in enough data to satify a request or until a timeout is reached.
  /// See `ProfileSource::read(...)`.
  void read(const DataClass&) override;

  DataClass provides() const noexcept override;
  DataClass finalizeRequest(const DataClass&) const noexcept override;

private:
  // Transfer of attributes from header-open time to read-time.
  bool fileValid;
  bool attrsValid;
  ProfileAttributes attrs;
  bool tattrsValid;
  ThreadAttributes tattrs;

  // Tracefile setup and arrangements.
  bool setupTrace() noexcept;
  Thread::Temporary* thread;

  // The actual file. Details for reading handled in prof-lean.
  hpcrun_sparse_file_t* file;
  stdshim::filesystem::path path;

  // The various ID to Object mappings.
  std::unordered_map<unsigned int, Metric&> metrics;
  std::unordered_map<unsigned int, bool> metricInt;
  std::unordered_map<unsigned int, Module&> modules;
  std::unordered_map<unsigned int, ContextRef> nodes;
  std::unordered_map<unsigned int, std::pair<ContextRef, Scope>> templates;
  std::unordered_map<unsigned int, uint64_t> contextroots;
  unsigned int partial_node_id;  // ID for the partial unwind fake root node
  unsigned int unknown_node_id;  // ID for unwinds that start from "nowhere," but somehow aren't partial.

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

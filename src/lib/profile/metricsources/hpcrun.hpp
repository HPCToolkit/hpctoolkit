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

#ifndef HPCTOOLKIT_PROFILE_METRICSOURCES_HPCRUN_H
#define HPCTOOLKIT_PROFILE_METRICSOURCES_HPCRUN_H

#include "../metricsource.hpp"

#include <memory>
#include "../stdshim/filesystem.hpp"

namespace hpctoolkit::metricsources {

/// MetricSource for files in the current .hpcrun format. Requires it to be a
/// real file (so seeking is possible).
class HpcrunFSv2 final : public MetricSource {
public:
  ~HpcrunFSv2() = default;

  // You shouldn't create these directly, use MetricSource::create_for
  HpcrunFSv2() = delete;

  /// Read in enough data to satify a request or until a timeout is reached.
  /// See `MetricSource::read(...)`.
  bool read(const DataClass&, const DataClass&, ProfilePipeline::timeout_t);

  void bindPipeline(ProfilePipeline::SourceEnd&&) noexcept;
  DataClass provides() const noexcept { return can_provide; }

private:
  bool setupTrace() noexcept;
  DataClass can_provide;

  ProfileAttributes attrs;
  ThreadAttributes tattrs;
  Thread* thread;

  struct seekfile {
    seekfile(const stdshim::filesystem::path&, long = 0);
    ~seekfile();

    long mark();  // Update curpos with the current position.
    void seek(long);  // Jump-seek to a new location, eventually.
    void advance(long);  // Jump-seek forward to a new location, eventually.
    void prep();  // Actually seek to the "current" location.

    std::FILE* file;
    long curpos;
    bool atpos;
  };

  stdshim::filesystem::path path;
  bool read_headers;
  bool read_cct;
  bool warned;
  struct epoch_off {
    long header;
    long cct;
    bool read_cct;
    bool passed;
    std::unordered_map<uint16_t, Module&> module_ids;
    uint32_t partial_node_id;
    uint32_t unknown_node_id;
    std::unordered_map<uint32_t, Context*> node_ids;
    epoch_off() : epoch_off(0) {};
    epoch_off(long h) : header(h), cct(0), read_cct(false), passed(false),
                        partial_node_id(0), unknown_node_id(0) {};
  };
  std::vector<epoch_off> epoch_offsets;
  std::vector<const Metric*> metric_order;
  std::vector<bool> metric_int;

  stdshim::filesystem::path tracepath;
  long trace_off;
  bool read_trace;

  friend std::unique_ptr<MetricSource> MetricSource::create_for(const stdshim::filesystem::path&);
  HpcrunFSv2(const stdshim::filesystem::path&);
};

}

#endif  // HPCTOOLKIT_PROFILE_METRICSOURCES_HPCRUN_H

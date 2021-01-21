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

#ifndef HPCTOOLKIT_PROFILE_SINKS_HPCTRACEDB2_H
#define HPCTOOLKIT_PROFILE_SINKS_HPCTRACEDB2_H

#include "../sink.hpp"

#include <chrono>

namespace hpctoolkit::sinks {

class HPCTraceDB2 final : public ProfileSink {
public:
  ~HPCTraceDB2() = default;

  /// Constructor, with a reference to the output database directory.
  HPCTraceDB2(const stdshim::filesystem::path&);

  /// Write out as much data as possible. See ProfileSink::write.
  void write() override;

  DataClass wavefronts() const noexcept override { return DataClass::threads; }

  DataClass accepts() const noexcept override {
    using ds = DataClass;
    return ds::threads | ds::contexts | ds::timepoints;
  }

  ExtensionClass requires() const noexcept override {
    using es = ExtensionClass;
    return es::identifier;
  }

  void notifyPipeline() noexcept override;

  void notifyWavefront(DataClass) override;
  void notifyThread(const Thread&) override;
  void notifyTimepoint(std::chrono::nanoseconds);
  void notifyTimepoint(const Thread&, ContextRef::const_t, std::chrono::nanoseconds) override;
  void notifyThreadFinal(const Thread::Temporary&) override;

  /// Check whether a Context ever appears in the traces.
  bool seen(const Context&);

  /// Return the tag for the experiment.xml, or an empty string if empty.
  std::string exmlTag();

private:
  stdshim::filesystem::path dir;
  stdshim::filesystem::path trace_p;
  std::atomic<bool> has_traces;
  std::atomic<std::chrono::nanoseconds> min;
  std::atomic<std::chrono::nanoseconds> max;

  void mmupdate(std::chrono::nanoseconds min, std::chrono::nanoseconds max);

  util::Once threadsReady;

  struct uds;

  class traceHdr {
  public:
    traceHdr(const Thread&, HPCTraceDB2& tdb);
    ~traceHdr() = default;

    uint32_t prof_info_idx;
    uint16_t trace_idx;
    uint64_t start;
    uint64_t end;
  }; 

  class udContext {
  public:
    udContext(const Context&, HPCTraceDB2& tdb) : uds(tdb.uds), used(false) {};
    ~udContext() = default;

    struct uds& uds;
    std::atomic<bool> used;
  };

  class udThread {
  public:
    udThread(const Thread&, HPCTraceDB2&);
    ~udThread() = default;

    struct uds& uds;
    bool has_trace;
    std::chrono::nanoseconds minTime;
    std::chrono::nanoseconds maxTime;

    std::FILE* trace_file;
    traceHdr trace_hdr;
    uint64_t tmcntr;

  };

  struct uds {
    Context::ud_t::typed_member_t<udContext> context;
    Thread::ud_t::typed_member_t<udThread> thread;
  } uds;


  //***************************************************************************
  // trace_hdr
  //***************************************************************************
  #define INVALID_HDR    -1
  #define MULTIPLE_8(v) (v + 7) & ~7

  uint64_t trace_hdrs_size;

  uint64_t getTotalNumTraces(); 
  std::vector<uint64_t> calcStartEnd();
  void assignHdrs(const std::vector<uint64_t>& trace_offs);

  template <typename T>
  void exscan(std::vector<T>& data);


};

}

#endif  // HPCTOOLKIT_PROFILE_SINKS_HPCTRACEDB2_H

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

#ifndef HPCTOOLKIT_PROFILE_PROFILESINKS_HPCTRACEDB_H
#define HPCTOOLKIT_PROFILE_PROFILESINKS_HPCTRACEDB_H

#include "../profilesink.hpp"

#include <chrono>

namespace hpctoolkit::profilesinks {

class HPCTraceDB final : public ProfileSink {
public:
  ~HPCTraceDB() = default;

  /// Constructor, with a reference to the output database directory.
  /// If all is true, it can be assumed that all tracefiles will be written
  /// by this HPCTraceDB (aka, not prof-mpi).
  HPCTraceDB(const stdshim::filesystem::path&, bool all);
  HPCTraceDB(stdshim::filesystem::path&&, bool all);

  /// Write out as much data as possible. See ProfileSink::write.
  bool write(
    std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max());

  DataClass accepts() const noexcept {
    using ds = DataClass;
    return ds::contexts | ds::trace;
  }

  ExtensionClass requires() const noexcept {
    using es = ExtensionClass;
    return es::identifier;
  }

  void notifyPipeline();

  void traceStart(const Thread&, unsigned long, std::chrono::nanoseconds);
  void trace(const Thread&, unsigned long, std::chrono::nanoseconds, const Context&);
  void traceEnd(const Thread&, unsigned long);

  /// Check whether a Context ever appears in the traces.
  bool seen(const Context&);

  /// Notify of the presence of trace data, without the actual data.
  void extra(std::chrono::nanoseconds beg, std::chrono::nanoseconds end);

  /// Return the tag for the experiment.xml, or an empty string if empty.
  std::string exmlTag();

private:
  bool is_omniocular;
  stdshim::filesystem::path dir;
  std::atomic<bool> has_traces;
  std::chrono::nanoseconds min;
  std::chrono::nanoseconds max;

  struct uds;

  class udContext {
  public:
    udContext(const Context&, HPCTraceDB& tdb) : uds(tdb.uds), used(false) {};
    ~udContext() = default;

    struct uds& uds;
    std::atomic<bool> used;
  };

  class udThread {
  public:
    udThread(const Thread&, HPCTraceDB&);
    ~udThread() = default;

    struct uds& uds;
    stdshim::filesystem::path path;
    std::FILE* file;
    bool has_trace;
    std::chrono::nanoseconds minTime;
    std::chrono::nanoseconds maxTime;
  };

  struct uds {
    Context::ud_t::typed_member_t<udContext> context;
    Thread::ud_t::typed_member_t<udThread> thread;
  } uds;
};

}

#endif  // HPCTOOLKIT_PROFILE_PROFILESINKS_HPCTRACEDB_H

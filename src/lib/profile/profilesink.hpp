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

#ifndef HPCTOOLKIT_PROFILE_PROFILESINK_H
#define HPCTOOLKIT_PROFILE_PROFILESINK_H

#include "profile.hpp"
#include "util/locked_unordered.hpp"

#include <chrono>

namespace hpctoolkit {

/// Base class for all sinks of profile data.
class ProfileSink {
public:
  virtual ~ProfileSink() = default;

  /// Write as much data from the Profile as possible into the Sink. If any
  /// operation would have to block for longer than the timeout, return false
  /// and have the user come back later.
  // MT: Externally Synchronized
  virtual bool write(
    std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max()) = 0;

  /// Try to help another thread that is currently in a write(). Returns whether
  /// the help is complete (and so the master thread has to finish up).
  // MT: Internally Synchronized
  virtual bool help(
    std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max()) {
    return true;  // i.e. Be default, its a single-threaded operation.
  }

  /// Bind a new Pipeline to this Sink.
  void bindPipeline(ProfilePipeline::SinkEnd&& se) {
    src = std::move(se);
    notifyPipeline();
  }

  /// Notify the Sink that a Pipeline has been bound, and register any userdata.
  virtual void notifyPipeline() {};

  /// Query what Classes of data this Sink is able to accept.
  // MT: Safe (const)
  virtual DataClass accepts() const noexcept = 0;

  /// Query what Classes of extended data this Sink needs to function.
  // MT: Safe (const)
  virtual ExtensionClass requires() const noexcept = 0;

  /// Notify the Sink that a new Module has been created, returning a tag for it.
  // MT: Internally Synchronized
  virtual void notifyModule(const Module&) {};

  /// Notify the Sink that a new File has been created, returning a tag for it.
  // MT: Internally Synchronized
  virtual void notifyFile(const File&) {};

  /// Notify the Sink that a new Function has been created, returning a tag for it.
  // MT: Internally Synchronized
  virtual void notifyFunction(const Module&, const Function&) {};

  /// Notify the Sink that a new Metric has been created, returning a tag.
  // MT: Internally Synchronized
  virtual void notifyMetric(const Metric&) {};

  /// Notify the sink that a new Context has been created, returning a tag.
  // MT: Internally Synchronized
  virtual void notifyContext(const Context&) {};

  /// Notify the sink that a new Thread has been created, returning a tag.
  // MT: Internally Synchronized
  virtual void notifyThread(const Thread&) {};

  /// Notify the Sink that thread-local Metric data is about to be added to a Context.
  // MT: Internally Synchronized
  virtual void notifyContextData(const Thread&, const Context&, const Metric&, double) {};

  /// Called when a new trace segment enters the Pipeline.
  // MT: Internally Synchronized
  virtual void traceStart(const Thread&, unsigned long, std::chrono::nanoseconds) {};

  /// Called for every timepoint within a trace segment.
  // MT: Internally Synchronized
  virtual void trace(const Thread&, unsigned long, std::chrono::nanoseconds, const Context&) {};

  /// Called one a trace segment has ended.
  // MT: Internally Synchronized
  virtual void traceEnd(const Thread&, unsigned long) {};

protected:
  /// You should never create a base ProfileSink. Use a subclass.
  ProfileSink() = default;

  ProfilePipeline::SinkEnd src;
};

}

#endif  // HPCTOOLKIT_PROFILE_PROFILESINK_H

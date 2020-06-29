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

#ifndef HPCTOOLKIT_PROFILE_SINK_H
#define HPCTOOLKIT_PROFILE_SINK_H

#include "pipeline.hpp"
#include "util/locked_unordered.hpp"

#include <chrono>

namespace hpctoolkit {

/// Base class for all sinks of profile data.
class ProfileSink {
public:
  virtual ~ProfileSink() = default;

  /// Write as much data from the Pipeline as possible into the Sink. If any
  /// operation would have to block for longer than the timeout, return false
  /// and have the user come back later.
  /// NOTE: Semantic under review.
  // MT: Externally Synchronized
  virtual bool write(ProfilePipeline::timeout_t);

  /// Write as much data from the Pipeline as possible.
  /// NOTE: Semantic under review.
  // MT: Externally Synchronized
  virtual void write();

  /// Try to help another thread that is currently in a write(). Returns whether
  /// the help is complete (and so the master thread has to finish up).
  // MT: Internally Synchronized
  virtual bool help(std::chrono::nanoseconds timeout);

  /// Bind a new Pipeline to this Sink.
  void bindPipeline(ProfilePipeline::Sink&& se) noexcept;

  /// Notify the Sink that a Pipeline has been bound, and register any userdata.
  virtual void notifyPipeline() noexcept;

  /// Query what Classes of data this Sink is able to accept.
  // MT: Safe (const)
  virtual DataClass accepts() const noexcept = 0;

  /// Query what Classes of data this Sink wants early wavefronts for.
  // MT: Safe (const)
  virtual DataClass wavefronts() const noexcept;

  /// Query what Classes of extended data this Sink needs to function.
  // MT: Safe (const)
  virtual ExtensionClass requires() const noexcept = 0;

  /// Notify the Sink that a requested wavefront has passed.
  // MT: Internally Synchronized
  virtual void notifyWavefront(DataClass::singleton_t);

  /// Notify the Sink that a new Module has been created.
  // MT: Internally Synchronized
  virtual void notifyModule(const Module&);

  /// Notify the Sink that a new File has been created.
  // MT: Internally Synchronized
  virtual void notifyFile(const File&);

  /// Notify the Sink that a new Metric has been created.
  // MT: Internally Synchronized
  virtual void notifyMetric(const Metric&);

  /// Notify the Sink that a new Context has been created.
  // MT: Internally Synchronized
  virtual void notifyContext(const Context&);

  /// Notify the Sink that a new Thread has been created.
  // MT: Internally Synchronized
  virtual void notifyThread(const Thread&);

  /// Notify the Sink that a timepoint has been registered. The overload called
  /// is the one with the most arguments as allowed by the result of accepts().
  // MT: Internally Synchronized
  virtual void notifyTimepoint(std::chrono::nanoseconds);
  virtual void notifyTimepoint(const Thread&, std::chrono::nanoseconds);
  virtual void notifyTimepoint(const Context&, std::chrono::nanoseconds);
  virtual void notifyTimepoint(const Thread&, const Context&, std::chrono::nanoseconds);

  /// Notify the Sink that a Thread has finished.
  // MT: Internally Synchronized
  virtual void notifyThreadFinal(const Thread::Temporary&);

protected:
  /// You should never create a base ProfileSink. Use a subclass.
  ProfileSink() = default;

  ProfilePipeline::Sink src;
};

}

#endif  // HPCTOOLKIT_PROFILE_SINK_H

// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_SINK_H
#define HPCTOOLKIT_PROFILE_SINK_H

#include "pipeline.hpp"
#include "util/locked_unordered.hpp"
#include "util/parallel_work.hpp"

#include <chrono>
#include <memory>

namespace hpctoolkit {

/// Base class for all sinks of profile data.
class ProfileSink {
public:
  virtual ~ProfileSink() = default;

  /// Write as much data from the Pipeline as possible.
  // MT: Externally Synchronized
  virtual void write() = 0;

  /// Try to assist another thread that is currently in a write(). Returns the
  /// amount this call contributed to the overall workshare.
  /// Unless this is overridden, Sinks are assumed to be single-threaded.
  // MT: Internally Synchronized
  virtual util::WorkshareResult help();

  /// Bind a new Pipeline to this Sink.
  // MT: Externally Synchronized
  void bindPipeline(ProfilePipeline::Sink&& se) noexcept;

  /// Notify the Sink that a Pipeline has been bound, and register any userdata.
  // MT: Externally Synchronized
  virtual void notifyPipeline() noexcept;

  /// Query what Classes of data this Sink is able to accept.
  // MT: Safe (const)
  virtual DataClass accepts() const noexcept = 0;

  /// Query what Classes of data this Sink wants early wavefronts for.
  // MT: Safe (const)
  virtual DataClass wavefronts() const noexcept;

  /// Query what Classes of extended data this Sink needs to function.
  // MT: Safe (const)
  virtual ExtensionClass requirements() const noexcept = 0;

  /// Notify the Sink that a requested wavefront has passed. The argument
  /// specifies the set of currently passed wavefronts.
  // MT: Internally Synchronized
  virtual void notifyWavefront(DataClass);

  /// Notify the Sink that a new Module has been created.
  // MT: Internally Synchronized
  virtual void notifyModule(const Module&);

  /// Notify the Sink that a new File has been created.
  // MT: Internally Synchronized
  virtual void notifyFile(const File&);

  /// Notify the Sink that a new Metric has been created.
  // MT: Internally Synchronized
  virtual void notifyMetric(const Metric&);

  /// Notify the Sink that a new ExtraStatistic has been created.
  // MT: Internally Synchronized
  virtual void notifyExtraStatistic(const ExtraStatistic&);

  /// Notify the Sink that a new Context has been created.
  // MT: Internally Synchronized
  virtual void notifyContext(const Context&);

  /// Notify the Sink that a new Thread has been created.
  // MT: Internally Synchronized
  virtual void notifyThread(const Thread&);

  /// Notify the Sink that some number of Context-type timepoints have been emitted.
  // MT: Internally Synchronized
  virtual void notifyTimepoints(const Thread&, const std::vector<
    std::pair<std::chrono::nanoseconds, std::reference_wrapper<const Context>>>&);

  /// Notify the Sink that the next Context-type timepoint will not be the sequentially next,
  /// but instead be rewound back to the first.
  virtual void notifyCtxTimepointRewindStart(const Thread&);

  /// Notify the Sink that some number of Metric-type timepoints have been emitted.
  // MT: Internally Synchronized
  virtual void notifyTimepoints(const Thread&, const Metric&, const std::vector<
    std::pair<std::chrono::nanoseconds, double>>&);

  /// Notify the Sink that the next Metric-type timepoint will not be the sequentially next,
  /// but instead be rewound back to the first.
  virtual void notifyMetricTimepointRewindStart(const Thread&, const Metric&);

  /// Notify the Sink that a Thread has finished.
  // MT: Internally Synchronized
  virtual void notifyThreadFinal(std::shared_ptr<const PerThreadTemporary>);

protected:
  /// You should never create a base ProfileSink. Use a subclass.
  ProfileSink() = default;

  ProfilePipeline::Sink src;
};

}

#endif  // HPCTOOLKIT_PROFILE_SINK_H

// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#include "util/vgannotations.hpp"

#include "sink.hpp"

#include "util/log.hpp"

using namespace hpctoolkit;

util::WorkshareResult ProfileSink::help() {
  // Unless specified otherwise, Sinks are single-threaded
  return {false, true};
}

void ProfileSink::bindPipeline(ProfilePipeline::Sink&& se) noexcept {
  src = std::move(se);
  notifyPipeline();
}

DataClass ProfileSink::wavefronts() const noexcept { return {}; }

void ProfileSink::notifyPipeline() noexcept {};

void ProfileSink::notifyWavefront(DataClass) {};
void ProfileSink::notifyModule(const Module&) {};
void ProfileSink::notifyFile(const File&) {};
void ProfileSink::notifyMetric(const Metric&) {};
void ProfileSink::notifyExtraStatistic(const ExtraStatistic&) {};
void ProfileSink::notifyContext(const Context&) {};
void ProfileSink::notifyThread(const Thread&) {};
void ProfileSink::notifyTimepoints(const Thread& t, const std::vector<
  std::pair<std::chrono::nanoseconds, std::reference_wrapper<const Context>>>&) {};
void ProfileSink::notifyCtxTimepointRewindStart(const Thread&) {};
void ProfileSink::notifyTimepoints(const Thread& t, const Metric&, const std::vector<
  std::pair<std::chrono::nanoseconds, double>>&) {};
void ProfileSink::notifyMetricTimepointRewindStart(const Thread&, const Metric&) {};
void ProfileSink::notifyThreadFinal(std::shared_ptr<const PerThreadTemporary>) {};

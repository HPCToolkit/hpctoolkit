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
// Copyright ((c)) 2002-2023, Rice University
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

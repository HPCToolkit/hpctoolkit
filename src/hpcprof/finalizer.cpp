// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#include "finalizer.hpp"

using namespace hpctoolkit;

void ProfileFinalizer::bindPipeline(ProfilePipeline::Source&& se) noexcept {
  sink = std::move(se);
  notifyPipeline();
}

void ProfileFinalizer::notifyPipeline() noexcept {};

std::optional<unsigned int> ProfileFinalizer::identify(const Module&) noexcept {
  return std::nullopt;
}
std::optional<unsigned int> ProfileFinalizer::identify(const File&) noexcept {
  return std::nullopt;
}
std::optional<Metric::Identifier> ProfileFinalizer::identify(const Metric&) noexcept {
  return std::nullopt;
}
std::optional<unsigned int> ProfileFinalizer::identify(const Context&) noexcept {
  return std::nullopt;
}
std::optional<unsigned int> ProfileFinalizer::identify(const Thread&) noexcept {
  return std::nullopt;
}

std::optional<stdshim::filesystem::path> ProfileFinalizer::resolvePath(const File&) noexcept {
  return std::nullopt;
}
std::optional<stdshim::filesystem::path> ProfileFinalizer::resolvePath(const Module&) noexcept {
  return std::nullopt;
}

std::optional<std::pair<util::optional_ref<Context>, Context&>>
ProfileFinalizer::classify(Context&, NestedScope&) noexcept {
  return std::nullopt;
}

bool ProfileFinalizer::resolve(ContextFlowGraph&) noexcept {
  return false;
}

void ProfileFinalizer::appendStatistics(const Metric&, Metric::StatsAccess) noexcept {};

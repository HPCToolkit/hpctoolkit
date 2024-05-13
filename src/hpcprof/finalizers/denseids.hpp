// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_FINALIZERS_DENSEIDS_H
#define HPCTOOLKIT_PROFILE_FINALIZERS_DENSEIDS_H

#include "../finalizer.hpp"

namespace hpctoolkit::finalizers {

// Simple dense-id generating Finalizer.
class DenseIds final : public ProfileFinalizer {
public:
  DenseIds();
  ~DenseIds() = default;

  ExtensionClass provides() const noexcept override {
    return ExtensionClass::identifier;
  }
  ExtensionClass requirements() const noexcept override { return {}; }

  std::optional<unsigned int> identify(const Module&) noexcept override;
  std::optional<unsigned int> identify(const File&) noexcept override;
  std::optional<Metric::Identifier> identify(const Metric&) noexcept override;
  std::optional<unsigned int> identify(const Context&) noexcept override;
  std::optional<unsigned int> identify(const Thread&) noexcept override;

private:
  std::atomic<unsigned int> mod_id;
  std::atomic<unsigned int> file_id;
  std::atomic<unsigned int> met_id;
  std::atomic<unsigned int> ctx_id;
  std::atomic<unsigned int> t_id;
};

}

#endif  // HPCTOOLKIT_PROFILE_FINALIZERS_DENSEIDS_H

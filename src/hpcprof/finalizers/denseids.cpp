// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#include "denseids.hpp"

using namespace hpctoolkit;
using namespace finalizers;

DenseIds::DenseIds()
  : mod_id(0), file_id(0), met_id(0), ctx_id(1), t_id(0) {};

std::optional<unsigned int> DenseIds::identify(const Module&) noexcept {
  return mod_id.fetch_add(1, std::memory_order_relaxed);
}
std::optional<unsigned int> DenseIds::identify(const File&) noexcept {
  return file_id.fetch_add(1, std::memory_order_relaxed);
}
std::optional<Metric::Identifier> DenseIds::identify(const Metric& m) noexcept {
  auto inc = std::max<size_t>(m.partials().size(), 1) * m.scopes().size();
  return Metric::Identifier(m, met_id.fetch_add(inc, std::memory_order_relaxed));
}
std::optional<unsigned int> DenseIds::identify(const Context& c) noexcept {
  if(!c.direct_parent())
    return 0;  // Reserved for the root Context
  return ctx_id.fetch_add(1, std::memory_order_relaxed);
}
std::optional<unsigned int> DenseIds::identify(const Thread&) noexcept {
  return t_id.fetch_add(1, std::memory_order_relaxed);
}

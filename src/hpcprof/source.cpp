// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#include "source.hpp"

#include "util/log.hpp"
#include "sources/hpcrun4.hpp"

#include <stdexcept>

using namespace hpctoolkit;

std::unique_ptr<ProfileSource> ProfileSource::create_for(const stdshim::filesystem::path& p, const stdshim::filesystem::path& meas) {
  // All we do is go down the list and try every file-based source.
  std::unique_ptr<ProfileSource> r;
  r.reset(new sources::Hpcrun4(p, meas));
  if(r->valid()) return r;

  // Unrecognized or unsupported format
  return nullptr;
}

bool ProfileSource::valid() const noexcept { return true; }

void ProfileSource::bindPipeline(ProfilePipeline::Source&& se) noexcept {
  sink = std::move(se);
}

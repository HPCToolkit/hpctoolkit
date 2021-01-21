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

#include "source.hpp"

#include "util/log.hpp"
#include "sources/hpcrun4.hpp"

#include <stdexcept>

using namespace hpctoolkit;

std::unique_ptr<ProfileSource> ProfileSource::create_for(const stdshim::filesystem::path& p) {
  // All we do is go down the list and try every file-based source.
  std::unique_ptr<ProfileSource> r;
  r.reset(new sources::Hpcrun4(p));
  if(r->valid()) return r;

  // Unrecognized or unsupported format
  return nullptr;
}

bool ProfileSource::valid() const noexcept { return true; }
std::pair<bool, bool> ProfileSource::requiresOrderedRegions() const noexcept {
  return {false, false};
}

void ProfileSource::bindPipeline(ProfilePipeline::Source&& se) noexcept {
  sink = std::move(se);
}

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

#include "logical.hpp"

#include "lib/prof-lean/hpcfmt.h"

#include <cstring>

using namespace hpctoolkit;
using namespace finalizers;

LogicalFile::LogicalFile() = default;

void LogicalFile::notifyPipeline() noexcept {
  ud = sink.structs().module.add_default<udModule>(
    [this](udModule& data, const Module& m){
      load(m, data);
    });
}

std::optional<std::pair<util::optional_ref<Context>, Context&>>
LogicalFile::classify(Context& parent, NestedScope& ns) noexcept {
  if(ns.flat().type() == Scope::Type::point) {
    auto mo = ns.flat().point_data();
    const auto& udm = mo.first.userdata[ud];
    if(udm.isLogical) {
      auto lineno = mo.second & 0xFFFFFFFFULL;
      auto it = udm.map.find(mo.second >> 32);
      if(it != udm.map.end()) {
        if(const auto* prf = std::get_if<std::reference_wrapper<const File>>(&it->second)) {
          ns.flat() = {prf->get(), lineno};
        } else if(const auto* pf = std::get_if<const Function>(&it->second)) {
          ns.flat() = Scope(*pf);
        } else {
          // unreachable
          std::terminate();
        }
      }
      return {std::make_pair(std::nullopt, std::ref(parent))};
    }
  }
  return std::nullopt;
}

void LogicalFile::load(const Module& m, udModule& data) noexcept {
  const auto& rpath = m.userdata[sink.resolvedPath()];
  const auto& mpath = rpath.empty() ? m.path() : rpath;
  FILE* f = fopen(mpath.c_str(), "rb");
  if(f == nullptr) return;  // Can't do anything if we can't open it

  char buf[11] = {0};
  std::fread(buf, 10, 1, f);
  if(std::strcmp(buf, "HPCLOGICAL") != 0) {
    std::fclose(f);
    return;  // Not a logical file
  }

  // Read in stanzas until anything bad happens
  int ret;
  do {
    uint32_t fid = 0;
    char* funcname = NULL;
    char* filename = NULL;
    uint32_t lineno = 0;
    if((ret = hpcfmt_int4_fread(&fid, f)) != HPCFMT_OK) break;
    if((ret = hpcfmt_str_fread(&funcname, f, std::malloc)) != HPCFMT_OK) { ret = HPCFMT_ERR; break; }
    if((ret = hpcfmt_str_fread(&filename, f, std::malloc)) != HPCFMT_OK) { ret = HPCFMT_ERR; break; }
    if((ret = hpcfmt_int4_fread(&lineno, f)) != HPCFMT_OK) { ret = HPCFMT_ERR; break; }

    bool ok = true;
    if(funcname[0] != '\0') {
      Function f(m, std::nullopt, funcname);
      if(filename[0] != '\0')
        f.sourceLocation(sink.file(filename), lineno);
      ok = data.map.try_emplace(fid, std::move(f)).second;
    } else if(filename[0] != '\0') {
      ok = data.map.try_emplace(fid, sink.file(filename)).second;
    } else
      util::log::fatal{} << "Invalid logical stanza, both names are empty!";

    if(!ok)
      util::log::fatal{} << "Invalid logical stanza, same id used twice!";

    std::free(funcname);
    std::free(filename);
  } while(1);

  // If it wasn't an EOF that stopped us, warn that there may be missing data
  if(ret != HPCFMT_EOF)
    util::log::warning{} << "Logical metadata file " << mpath << " appears to be truncated, some logical frames may be missing!";

  std::fclose(f);
  data.isLogical = true;
}

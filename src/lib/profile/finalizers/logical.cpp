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

#include "logical.hpp"

#include "lib/support-lean/hpctoolkit_demangle.h"
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
      auto fid = mo.second >> 32;
      if(fid == 0) {
        // Indicates an unknown logical region. Map to (unknown).
        ns.flat() = Scope();
        return {std::make_pair(std::nullopt, std::ref(parent))};
      }
      auto it = udm.map.find(fid);
      if(it == udm.map.end()) {
        // Definitely logical, but not in the list. This is an error, map to (unknown).
        // TODO: Make an ERROR here
        ns.flat() = Scope();
        return {std::make_pair(std::nullopt, std::ref(parent))};
      }
      auto lineno = mo.second & 0xFFFFFFFFULL;
      if(const auto* prf = std::get_if<std::reference_wrapper<const File>>(&it->second)) {
        // Indicates a line within a File
        ns.flat() = {prf->get(), lineno};
        return {std::make_pair(std::nullopt, std::ref(parent))};
      }
      if(const auto* prf = std::get_if<std::reference_wrapper<const Function>>(&it->second)) {
        // Indicates a line within a Function
        const Function& f = prf->get();
        auto srcloc = f.sourceLocation();
        if(lineno == 0 || !srcloc) {
          // No line or missing file information, skip the line Context.
          ns.flat() = Scope(f);
          return {std::make_pair(std::nullopt, std::ref(parent))};
        }
        Context& func = sink.context(parent, {ns.relation(), Scope(f)}).second;
        ns = {Relation::enclosure, Scope(srcloc->first, lineno)};
        return {std::make_pair(std::ref(func), std::ref(func))};
      }

      // unreachable
      std::terminate();
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
  if(std::fread(buf, 10, 1, f) < 1 || std::strcmp(buf, "HPCLOGICAL") != 0) {
    std::fclose(f);
    return;  // Not a logical file
  }

  // Identify the logical "category" and merge the Functions + Modules.
  // TODO: Should we record an identifier in the file itself instead of using the filename?
  const std::string id = mpath.stem();
  auto& funcs = funcTableMap[id];
  const auto& cm = sink.module(std::string("<logical ") + id + ">");

  // Read in stanzas until anything bad happens
  int ret;
  do {
    uint32_t fid = 0;
    char* funcname = NULL;
    char funcmang = 0;
    char* filename = NULL;
    uint32_t lineno = 0;
    if((ret = hpcfmt_int4_fread(&fid, f)) != HPCFMT_OK) break;
    if((ret = hpcfmt_str_fread(&funcname, f, std::malloc)) != HPCFMT_OK) { ret = HPCFMT_ERR; break; }
    if(fread(&funcmang, sizeof(char), 1, f) < 1) { ret = HPCFMT_ERR; break; }
    if((ret = hpcfmt_str_fread(&filename, f, std::malloc)) != HPCFMT_OK) { ret = HPCFMT_ERR; break; }
    if((ret = hpcfmt_int4_fread(&lineno, f)) != HPCFMT_OK) { ret = HPCFMT_ERR; break; }

    switch(funcmang) {
    case 0:  // No mangling
      break;
    case 1: {  // C++ style mangling
      if(funcname != nullptr && funcname[0] != '\0') {
        char* dn = hpctoolkit_demangle(funcname);
        if(dn != nullptr) {
          std::free(funcname);
          funcname = dn;
        }
      }
      break;
    }
    default:
      util::log::fatal{} << "Invalid logical stanza, unrecognized mangling " << funcmang;
    }

    bool ok = true;
    if(funcname[0] != '\0') {
      Function f(cm, std::nullopt, funcname);
      if(filename[0] != '\0')
        f.sourceLocation(sink.file(filename), lineno);
      const Function& cf = funcs.emplace(std::move(f)).first;
      ok = data.map.try_emplace(fid, std::cref(cf)).second;
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

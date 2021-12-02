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
// Copyright ((c)) 2019-2021, Rice University
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

#include "kernelsyms.hpp"

#include "lib/support-lean/demangle.h"
#include "pipeline.hpp"
#include "../util/lzmastream.hpp"

#include "include/linux_info.h"

#include <limits>
#include <fstream>
#include <sstream>
#include <string>

using namespace hpctoolkit;
using namespace finalizers;

KernelSymbols::KernelSymbols(stdshim::filesystem::path root)
  : root(std::move(root)) {}

void KernelSymbols::notifyPipeline() noexcept {
  ud = sink.structs().module.add_initializer<Classification>(
    [this](Classification& c, const Module& m){
      load(m, c);
    });
}

util::optional_ref<Context> KernelSymbols::classify(Context& c, Scope& s) noexcept {
  if(s.type() == Scope::Type::point) {
    auto mo = s.point_data();
    auto scopes = mo.first.userdata[ud].getScopes(mo.second);
    if(scopes.empty()) return std::nullopt;
    std::reference_wrapper<Context> cc = c;
    for(const auto& s: scopes) cc = sink.context(cc, s);
    return cc.get();
  }
  return std::nullopt;
}

void KernelSymbols::load(const Module& m, Classification& c) noexcept {
  // We only take action if the Module's path is relatively awkward
  if(m.path().has_root_path() || m.path().has_parent_path()) return;
  // Check for the <...> markers that this is a "special case"
  std::string name = m.path().filename();
  if(name.front() != '<' || name.back() != '>') return;
  name = name.substr(1, name.size()-2);

  // Check if we have a symbols file for this one
  stdshim::filesystem::path syms = root / name;
  if(!stdshim::filesystem::is_regular_file(syms)) return;

  // Give it a shot, catch any errors if things go south
  try {
    std::ifstream symsfile_base(syms);
    util::ilzmastream symsfile(symsfile_base.rdbuf());

    bool sawBadLine = false;

    // We'll need to sort everything out at the end
    std::vector<std::pair<uint64_t, Classification::Block*>> entries;

    // Keep reading lines until we run into really bad problems
    std::string linestr;
    while(std::getline(symsfile, linestr)) {
      if(linestr.empty()) continue;  // Skip over blank lines
      std::istringstream line(std::move(linestr));

      // Parse the required fields on this line first, if we fail we skip
      uint64_t addr;
      std::string typestr;
      std::string name;
      line >> std::hex >> addr >> typestr >> name;
      if(!line || typestr.size() != 1) {
        if(!sawBadLine) {
          sawBadLine = true;
          util::log::error{} << "Failed to parse entry from symbols file " << syms.string()
            << ", some functions within " << name << " will be extended across the corrupted entries";
        }
        continue;
      }
      if(typestr.front() != 't' && typestr.front() != 'T') continue;

      // The module name is optional
      std::string modulename;
      line >> modulename;

      // Stitch together the full name of the function
      std::string fname;
      {
        std::ostringstream ss;
        ss << name;
        if(!modulename.empty()) ss << " " << modulename;
        ss << " " << LINUX_KERNEL_NAME;
        fname = ss.str();
      }

      // Log this entry
      entries.push_back({addr, c.addScope(c.addFunction(m, addr, std::move(fname)))});

      // Nom the end-of-line and any other whitespace until the next (real) line
      symsfile >> std::ws;
    }
    if(symsfile.bad()) {
      util::log::error{} << "I/O failure while reading from symbols file " << syms.string();
    }

    // Sort the entries and register them as ranges
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b){ return a.first < b.first; });
    std::remove_reference_t<decltype(entries.front())> prev = {0, nullptr};
    for(const auto& e: entries) {
      if(prev.second != nullptr)
        c.setScope({prev.first, e.first-1}, prev.second);
      prev = e;
    }
    if(prev.second != nullptr)
      c.setScope({prev.first, std::numeric_limits<uint64_t>::max()}, prev.second);

  } catch(std::exception& e) {
    util::log::info{} << "Exception caught while parsing symbols data from "
      << syms << " for " << name << "\n"
         "  what(): " << e.what();
  }
}

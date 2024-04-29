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
// Copyright ((c)) 2002-2024, Rice University
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

#include "../../support-lean/hpctoolkit_demangle.h"
#include "../pipeline.hpp"
#include "../util/lzmastream.hpp"

#include "../../../include/linux_info.h"

#include <limits>
#include <fstream>
#include <sstream>
#include <string>

using namespace hpctoolkit;
using namespace finalizers;

KernelSymbols::KernelSymbols() {}

void KernelSymbols::notifyPipeline() noexcept {
  ud = sink.structs().module.add_default<udModule>(
    [this](udModule& data, const Module& m){
      load(m, data);
    });
}

std::optional<std::pair<util::optional_ref<Context>, Context&>>
KernelSymbols::classify(Context& c, NestedScope& ns) noexcept {
  if(ns.flat().type() == Scope::Type::point) {
    auto mo = ns.flat().point_data();
    const auto& udm = mo.first.userdata[ud];
    auto symit = udm.symbols.find(mo.second);
    if(symit != udm.symbols.end()) {
      auto& cc = sink.context(c, {ns.relation(), Scope(symit->second)}).second;
      ns.relation() = Relation::enclosure;
      return std::make_pair(std::ref(cc), std::ref(cc));
    }
  }
  return std::nullopt;
}

void KernelSymbols::load(const Module& m, udModule& ud) noexcept {
  // We only take action if the Module's relative path starts with kernel_symbols/
  if(m.relative_path().empty() || m.relative_path().parent_path().filename() != "kernel_symbols") return;

  // Check if we have a symbols file for this one
  stdshim::filesystem::path syms = m.path();
  if(!stdshim::filesystem::is_regular_file(syms)) return;

  // Give it a shot, catch any errors if things go south
  try {
    std::ifstream symsfile_base(syms);
    util::ilzmastream symsfile(symsfile_base.rdbuf());

    bool sawBadLine = false;

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
      ud.symbols.emplace(addr, Function(m, addr, std::move(fname)));

      // Nom the end-of-line and any other whitespace until the next (real) line
      symsfile >> std::ws;
    }
    if(symsfile.bad()) {
      util::log::error{} << "I/O failure while reading from symbols file " << syms.string();
    }

    // Make sure this is consistent before continuing along
    ud.symbols.make_consistent();
  } catch(std::exception& e) {
    util::log::vwarning{} << "Exception caught while parsing symbols data from "
      << syms << " for " << m.path() << "\n"
         "  what(): " << e.what();
    ud.symbols.clear();
  }
}

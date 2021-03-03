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

#include "module.hpp"

#include "lib/support-lean/demangle.h"
#include "pipeline.hpp"

#include <elfutils/libdw.h>
#include <dwarf.h>

#include <stdexcept>
#include <unordered_map>
#include <limits>
#include <algorithm>
extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
}

using namespace hpctoolkit;

std::vector<Scope> Classification::getScopes(uint64_t pos) const noexcept {
  std::vector<Scope> res;
  auto it = ll_scopeblocks.find({pos, pos});
  if(it != ll_scopeblocks.end()) {
    for(Block* sc = it->second; sc != nullptr; sc = sc->parent)
      res.push_back(sc->scope);
  }
  std::reverse(res.begin(), res.end());
  return res;
}

std::tuple<const File*, uint64_t, bool> Classification::getLine(uint64_t pos) const noexcept {
  auto lsp = getLineScope(pos);
  if(lsp != nullptr) return {lsp->file, lsp->line, lsp->isCall};
  return {nullptr, 0, false};
}

Scope Classification::classifyLine(Scope s) const noexcept {
  if(s.type() != Scope::Type::point && s.type() != Scope::Type::call)
    util::log::fatal{} << "Attempt to line-classify a non-point type Scope: " << s;
  auto mo = s.point_data();
  auto lsp = getLineScope(mo.second);
  if(lsp == nullptr || lsp->file == nullptr) return s;
  return lsp->isCall || s.type() == Scope::Type::call
    ? Scope{Scope::call, mo.first, mo.second, *lsp->file, lsp->line}
    : Scope{mo.first, mo.second, *lsp->file, lsp->line};
}

const Classification::LineScope* Classification::getLineScope(uint64_t pos) const noexcept {
  auto it = std::lower_bound(lll_scopes.begin(), lll_scopes.end(),
                             LineScope(pos, nullptr, 0));
  if(it != lll_scopes.end() && it->addr == pos) return &*it;
  if(it == lll_scopes.begin()) return nullptr;
  --it;
  return &*it;
}

void Classification::Block::addRoute(std::vector<route_t> from) noexcept {
  if(parent != nullptr)
    util::log::fatal{} << "Attempt to add route to a non-root Block!";
  if(from.empty()) return;
  routes.emplace_front(std::move(from));
  routeCnt += 1;
}

std::vector<std::vector<Scope>> Classification::getRoutes(uint64_t addr) const noexcept {
  Block& b = *({
    auto it = ll_scopeblocks.find({addr, addr});
    if(it == ll_scopeblocks.end() || it->second == nullptr) {
      return {};
    }
    Block* p = it->second;
    while(p->parent != nullptr) p = p->parent;
    p;
  });
  std::vector<std::vector<Scope>> routes;
  routes.reserve(b.routeCnt);
  for(const auto& r: b.routes) {
    routes.emplace_back();
    for(const auto vhop: r) {
      if(std::holds_alternative<uint64_t>(vhop)) {
        routes.back().push_back({Scope::call, mod, std::get<uint64_t>(vhop)});
      } else if(std::holds_alternative<const Block*>(vhop)) {
        for(const Block* hop = std::get<const Block*>(vhop); hop != nullptr;
            hop = hop->parent)
          routes.back().push_back(hop->scope);
      } else std::abort();  // unreachable
    }
    routes.back().shrink_to_fit();
  }
  return routes;
}

void Classification::setScope(const Interval& i, Block* sc) noexcept {
  // First just try inserting. It might not overlap with anything.
  auto x = ll_scopeblocks.emplace(i, sc);
  if(x.second) return;
  // For each interval it overlaps with, try to find the gap before it.
  Interval gap(i.lo, 0);
  auto eqrange = ll_scopeblocks.equal_range(i);
  for(auto& it = eqrange.first; it != eqrange.second; it++) {
    gap.hi = it->first.lo - 1;
    if(gap.lo < gap.hi) ll_scopeblocks.emplace_hint(it, gap, sc);
    gap.lo = it->first.hi + 1;
  }
  // Final gap to the end of i.
  gap.hi = i.hi;
  if(gap.lo < gap.hi) ll_scopeblocks.emplace(gap, sc);
}

bool Classification::filled(const Interval& i) const noexcept {
  auto eqrange = ll_scopeblocks.equal_range(i);
  if(eqrange.first == eqrange.second) return false;
  Interval gap(i.lo, 0);
  for(auto& it = eqrange.first; it != eqrange.second; it++) {
    gap.hi = it->first.lo - 1;
    if(gap.lo < gap.hi) return false;
    gap.lo = it->first.hi + 1;
  }
  return gap.lo >= i.hi;
}

bool Classification::empty() const noexcept {
  return ll_scopeblocks.empty() && lll_scopes.empty();
}

void Classification::setLines(std::vector<LineScope>&& lscopes) {
  if(lscopes.size() == 0) return;
  lscopes.reserve(lscopes.size()+lll_scopes.size());
  lscopes.insert(lscopes.end(), lll_scopes.begin(), lll_scopes.end());
  std::sort(lscopes.begin(), lscopes.end(),
            [](const LineScope& a, const LineScope& b) -> bool {
    return a.addr < b.addr;
  });

  lll_scopes.clear();
  lll_scopes.reserve(lscopes.size());
  for(auto& ls: lscopes) {
    auto& llb = lll_scopes.back();
    if(lll_scopes.size() == 0 || lll_scopes.back().addr != ls.addr)
      lll_scopes.emplace_back(std::move(ls));
    else if(ls.file && ls.line > 0 && (ls.file != llb.file || ls.line < llb.line))
      llb = std::move(ls);
  }
  lll_scopes.shrink_to_fit();
}

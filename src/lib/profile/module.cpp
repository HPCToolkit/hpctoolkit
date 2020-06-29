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

constexpr std::size_t Classification::scopenone;

std::vector<Scope> Classification::getScopes(uint64_t pos) const noexcept {
  std::vector<Scope> res;
  auto it = ll_scopeidxs.find({pos, pos});
  if(it != ll_scopeidxs.end()) {
    for(std::size_t scidx = it->second; scidx != scopenone;
        scidx = scopechains[scidx].parentidx) {
      res.push_back(scopechains[scidx].scope);
    }
  }
  return res;
}

std::pair<const File*, uint64_t> Classification::getLine(uint64_t pos) const noexcept {
  auto lsp = getLineScope(pos);
  if(lsp) return {lsp->file, lsp->line};
  return {nullptr, 0};
}

uint64_t Classification::getCanonicalAddr(uint64_t pos) const noexcept {
  auto lsp = getLineScope(pos);
  return lsp ? lsp->canonicalAddr : pos;
}

const Classification::LineScope* Classification::getLineScope(uint64_t pos) const noexcept {
  auto it = std::lower_bound(lll_scopes.begin(), lll_scopes.end(),
                             LineScope(pos, nullptr, 0));
  if(it == lll_scopes.begin()) return nullptr;
  if(it != lll_scopes.end() && it->addr == pos) return &*it;
  --it;
  return &*it;
}

void Classification::setScope(const Interval& i, std::size_t scidx) noexcept {
  // First just try inserting. It might not overlap with anything.
  auto x = ll_scopeidxs.emplace(i, scidx);
  if(x.second) return;
  // For each interval it overlaps with, try to find the gap before it.
  Interval gap(i.lo, 0);
  auto eqrange = ll_scopeidxs.equal_range(i);
  for(auto& it = eqrange.first; it != eqrange.second; it++) {
    gap.hi = it->first.lo - 1;
    if(gap.lo < gap.hi) ll_scopeidxs.emplace_hint(it, gap, scidx);
    gap.lo = it->first.hi + 1;
  }
  // Final gap to the end of i.
  gap.hi = i.hi;
  if(gap.lo < gap.hi) ll_scopeidxs.emplace(gap, scidx);
}

bool Classification::filled(const Interval& i) const noexcept {
  auto eqrange = ll_scopeidxs.equal_range(i);
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
  return ll_scopeidxs.empty() && lll_scopes.empty();
}

void Classification::setLines(std::vector<LineScope>&& lscopes) {
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

  struct flhash {
    std::hash<const File*> h_f;
    std::hash<uint64_t> h_l;
    std::size_t operator()(const std::pair<const File*,uint64_t>& v) const noexcept {
      auto y = h_l(v.second);
      return h_f(v.first) | (y >> 9) | (y << 55);
    }
  };
  std::unordered_map<std::pair<const File*,uint64_t>, uint64_t, flhash> canon;

  for(auto& ls : lll_scopes) {
    auto x = canon.insert({{ls.file, ls.line}, ls.addr});
    ls.canonicalAddr = x.first->second;
  }
}

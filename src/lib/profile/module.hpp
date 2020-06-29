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

#ifndef HPCTOOLKIT_PROFILE_MODULE_H
#define HPCTOOLKIT_PROFILE_MODULE_H

#include "scope.hpp"
#include "util/ragged_vector.hpp"
#include "util/locked_unordered.hpp"
#include "util/once.hpp"

#include <map>
#include <unordered_set>
#include <vector>
#include <future>
#include "stdshim/filesystem.hpp"

namespace hpctoolkit {

// Classifications represent the binding from offsets within a Module to the
// structural Contexts that we often want to insert.
class Classification {
public:
  Classification() = default;
  ~Classification() = default;

  struct Interval {
    uint64_t lo;
    uint64_t hi;

    Interval(uint64_t l, uint64_t h) : lo(l), hi(h) {};
    ~Interval() = default;

    bool operator<(const Interval& o) const { return hi < o.lo; }

    bool contains(const Interval& o) const { return lo <= o.lo && o.hi <= hi; }
    bool contains(uint64_t a) const { return lo <= a && a <= hi; }
  };

  /// Look up the stack of Scopes for the given address. If the address
  /// references a currently unknown region, the result will be empty.
  /// If only the lowest-level Scope is needed, see getScope.
  // MT: Safe (const)
  std::vector<Scope> getScopes(uint64_t) const noexcept;

  /// Look up the file and line info for the given address. If unknown, responds
  /// with a `nullptr` file at line 0.
  // MT: Safe (const)
  std::pair<const File*, uint64_t> getLine(uint64_t) const noexcept;

  /// Look up the canonical address for the given address. If unknown, responds
  /// with the original address.
  // MT: Safe (const)
  uint64_t getCanonicalAddr(uint64_t) const noexcept;

  /// The master table for Functions. These can be used to generate Scopes for
  /// various addScope and setScope.
  // MT: Externally Synchronized
  std::unordered_set<std::unique_ptr<Function>> functions;

  /// Add a new defaulted Function to the master table. Mildly easier override.
  // MT: Externally Synchronized
  template<class... Args>
  Function& addFunction(Args&&... args) {
    return *functions.emplace(new Function(std::forward<Args>(args)...)).first->get();
  }

  /// Special identifier for the NULL Scope trie node.
  static constexpr std::size_t scopenone = std::numeric_limits<std::size_t>::max();

  /// Add a new entry to the Scope trie. Returns an identifier for the resulting
  /// node.
  // MT: Externally Synchronized
  template<class... Args>
  std::size_t addScope(Args&&... args) noexcept {
    std::size_t r = scopechains.size();
    scopechains.emplace_back(std::forward<Args>(args)...);
    return r;
  }

  /// Overwrite the lowest-level Scope for the given range.
  // MT: Externally Synchronized
  void setScope(const Interval&, std::size_t) noexcept;

  /// Check whether an Interval has a remaining gap within it somewhere.
  // MT: Safe (const)
  bool filled(const Interval&) const noexcept;

  /// Check whether any Intervals have been classified yet.
  // MT: Safe (const)
  bool empty() const noexcept;

  struct LineScope {
    uint64_t addr;
    const File* file;
    uint64_t line;
    uint64_t canonicalAddr;
    LineScope(uint64_t a, const File* f, uint64_t l)
      : addr(a), file(f), line(l), canonicalAddr(0) {};
    bool operator<(const LineScope& o) const noexcept {
      if(addr != o.addr) return addr < o.addr;
      if(file != o.file) {
        if(file == nullptr) return true;
        if(o.file == nullptr) return false;
        return file->path() < o.file->path();
      }
      return line < o.line;
    }
  };

  /// Look up the LineScope for the given address. If unknown, returns nullptr.
  // MT: Safe (const)
  const LineScope* getLineScope(uint64_t) const noexcept;

  /// Pull in line data from the given array.
  // MT: Externally Synchronized
  void setLines(std::vector<LineScope>&& lscopes);

private:
  struct ScopeChain {
    std::size_t parentidx;
    Scope scope;
    ScopeChain(Function& f, std::size_t p)
      : parentidx(p), scope(f) {};
    ScopeChain(Function& f, const File& fn, uint64_t l, std::size_t p)
      : parentidx(p), scope(f, fn, l) {};
    ScopeChain(const Scope::loop_t&, const File& fn, uint64_t l, std::size_t p)
      : parentidx(p), scope(Scope::loop, fn, l) {};
  };
  std::map<Interval, std::size_t> ll_scopeidxs;
  std::vector<ScopeChain> scopechains;
  std::vector<LineScope> lll_scopes;
};

// Just a simple load module class, nothing to see here
class Module {
public:
  using ud_t = util::ragged_vector<const Module&>;

  Module(ud_t::struct_t& rs)
    : userdata(rs, std::cref(*this)), u_path() {};
  Module(Module&& m)
    : userdata(std::move(m.userdata), std::cref(*this)),
      u_path(std::move(m.u_path)) {};
  Module(ud_t::struct_t& rs, const stdshim::filesystem::path& p)
    : Module(rs, stdshim::filesystem::path(p)) {};
  Module(ud_t::struct_t& rs, stdshim::filesystem::path&& p)
    : userdata(rs, std::cref(*this)), u_path(std::move(p)) {};
  ~Module() = default;

  mutable ud_t userdata;

  // Absolute path to the module in question
  const stdshim::filesystem::path& path() const { return u_path; }

private:
  util::uniqable_key<stdshim::filesystem::path> u_path;

  friend class util::uniqued<Module>;
  util::uniqable_key<stdshim::filesystem::path>& uniqable_key() {
    return u_path;
  }
};

}

#endif  // HPCTOOLKIT_PROFILE_MODULE_H

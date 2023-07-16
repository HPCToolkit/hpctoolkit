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

#include "util/vgannotations.hpp"

#include "packedids.hpp"

#include "util/stable_hash.hpp"
#include "util/log.hpp"
#include "mpi/core.hpp"

#include <algorithm>

using namespace hpctoolkit;

// Helpers for packing various things.
static void pack(std::vector<std::uint8_t>& out, const std::string& s) noexcept {
  out.reserve(out.size() + s.size() + 1);  // Allocate the space early
  for(auto c: s) {
    if(c == '\0') c = '?';
    out.push_back(c);
  }
  out.push_back('\0');
}
static void pack(std::vector<std::uint8_t>& out, const std::uint64_t v) noexcept {
  // Little-endian order. Just in case the compiler can optimize it away.
  for(int shift = 0x00; shift < 0x40; shift += 0x08)
    out.push_back((v >> shift) & 0xff);
}

IdPacker::IdPacker() = default;

void IdPacker::notifyWavefront(DataClass ds) {
  if(ds.hasContexts() && ds.hasAttributes()) {  // This is it!
    std::vector<uint8_t> ct;

    // Format: [global id] [cnt] ([parent id] [nonce] [cnt] ([child hash] [child id])...)...
    pack(ct, (std::uint64_t)src.contexts().userdata[src.identifier()]);
    auto cntPtr = ct.size();
    std::size_t cnt = 0;
    pack(ct, (std::uint64_t)cnt);
    src.contexts().citerate([&](const Context& c){
      ++cnt;

      // Save hash states for each of the children
      std::vector<std::tuple<util::stable_hash_state, std::uint64_t, unsigned int, std::reference_wrapper<const Context>>> children;
      for(const Context& cc: c.children().citerate()) {
        util::stable_hash_state hashstate;
        hashstate << cc.scope();
        children.emplace_back(std::move(hashstate), 0, cc.userdata[src.identifier()], cc);
      }

      // Scan for a suitable nonce that gives the children unique hashes
      std::uint8_t nonce = 255;
      for(std::uint8_t trial_nonce = 0; trial_nonce < 20; ++trial_nonce) {
        for(auto& [hashstate, hash, id, cc]: children) {
          auto copy_hashstate = hashstate;
          copy_hashstate << trial_nonce;
          hash = copy_hashstate.squeeze();
        }
        std::sort(children.begin(), children.end(), [](auto& a, auto& b) -> bool {
          return std::get<1>(a) < std::get<1>(b);
        });

        bool is_unique = true;
        for(std::size_t i = 1; i < children.size(); ++i) {
          if(std::get<1>(children[i-1]) == std::get<1>(children[i])) {
            is_unique = false;
            break;
          }
        }
        if(is_unique) {
          nonce = trial_nonce;
          break;
        }
      }
      if(nonce == 255)
        util::log::fatal{} << "Unable to find a suitable unique hash function!";

      // Save the results
      pack(ct, (std::uint64_t)c.userdata[src.identifier()]);
      ct.push_back(nonce);
      pack(ct, (std::uint64_t)children.size());
      for(const auto& [hashstate, hash, id, cc]: children) {
        pack(ct, (std::uint64_t)hash);
        pack(ct, (std::uint64_t)id);
      }
    }, nullptr);
    {
      std::vector<std::uint8_t> cntv;
      pack(cntv, (std::uint64_t)cnt);
      for(const auto& b: cntv)
        ct[cntPtr++] = b;
    }

    // Format: [metric cnt] ([id] [name])...
    pack(ct, (std::uint64_t)src.metrics().size());
    for(auto& m: src.metrics().citerate()) {
      pack(ct, (std::uint64_t)m().userdata[src.identifier()].base());
      pack(ct, m().name());
    }

    notifyPacked(std::move(ct));
  }
}

// Helpers for unpacking various things
template<class T> static T unpack(std::vector<uint8_t>::const_iterator&) noexcept;
template<>
std::string unpack<std::string>(std::vector<uint8_t>::const_iterator& it) noexcept {
  std::string out;
  for(; *it != '\0'; ++it) out += *it;
  ++it;  // First location after the string
  return out;
}
template<>
std::uint64_t unpack<std::uint64_t>(std::vector<uint8_t>::const_iterator& it) noexcept {
  // Little-endian order. Same as in sinks/packed.cpp.
  std::uint64_t out = 0;
  for(int shift = 0x00; shift < 0x40; shift += 0x08) {
    out |= ((std::uint64_t)*it) << shift;
    ++it;
  }
  return out;
}

IdUnpacker::IdUnpacker(std::vector<uint8_t>&& c) : ctxtree(std::move(c)) {
  auto it = ctxtree.cbegin();
  globalid = ::unpack<std::uint64_t>(it);
}

void IdUnpacker::unpack() noexcept {
  auto it = ctxtree.cbegin();

  // Format: [global id] [cnt] ([parent id] [nonce] [cnt] ([child hash] [child id])...)...
  ::unpack<std::uint64_t>(it);  // Skip over the global id
  auto cnt = ::unpack<std::uint64_t>(it);
  for(std::size_t i = 0; i < cnt; ++i) {
    auto pid = ::unpack<std::uint64_t>(it);
    auto& out = idmap[pid]; // parent id
    out.first = *it++;  // nonce
    auto ccnt = ::unpack<std::uint64_t>(it);
    for(std::size_t j = 0; j < ccnt; ++j) {
      auto hash = ::unpack<std::uint64_t>(it);
      unsigned int id = ::unpack<std::uint64_t>(it);
      out.second[hash] = id;
    }
  }

  // Format: [metric cnt] ([id] [name])...
  cnt = ::unpack<std::uint64_t>(it);
  for(std::size_t i = 0; i < cnt; i++) {
    auto id = ::unpack<std::uint64_t>(it);
    auto name = ::unpack<std::string>(it);
    metmap.insert({std::move(name), id});
  }

  ctxtree.clear();
}

std::optional<unsigned int> IdUnpacker::identify(const Context& c) noexcept {
  util::call_once(once, [this]{ unpack(); });
  if(!c.direct_parent())
    return globalid;
  const auto& ids = idmap.at(c.direct_parent()->userdata[sink.identifier()]);
  util::stable_hash_state hashstate;
  hashstate << c.scope() << ids.first;
  auto hash = hashstate.squeeze();
  return ids.second.at(hash);
}

std::optional<Metric::Identifier> IdUnpacker::identify(const Metric& m) noexcept {
  util::call_once(once, [this]{ unpack(); });
  auto it = metmap.find(m.name());
  assert(it != metmap.end() && "No data for Metric `m`!");
  return Metric::Identifier(m, it->second);
}

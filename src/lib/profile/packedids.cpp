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
// Copyright ((c)) 2019-2022, Rice University
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

#include "util/log.hpp"
#include "mpi/core.hpp"

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

IdPacker::IdPacker() : stripcnt(0), buffersize(0) {};

void IdPacker::notifyPipeline() noexcept {
  udOnce = src.structs().context.add<ctxonce>(std::ref(*this));
}

void IdPacker::notifyContextExpansion(const Context& from, Scope s, const Context& to) {
  // Check that we haven't handled this particular expansion already
  if(!from.userdata[udOnce].seen.emplace(s).second) return;
  stripcnt.fetch_add(1, std::memory_order_relaxed);

  // Nab a pseudo-random buffer to fill with our data
  auto hash = std::hash<const Context*>{}(&from) ^ std::hash<Scope>{}(s);
  static_assert(std::numeric_limits<decltype(hash)>::radix == 2, "Non-binary architecture?");
  unsigned char idx = hash & 0xff;
  for(int i = 8; i < std::numeric_limits<decltype(hash)>::digits; i += 8)
    idx ^= (hash >> i) & 0xff;

  auto& buffer = stripbuffers[idx].second;
  std::unique_lock<std::mutex> lock(stripbuffers[idx].first);
  auto oldsz = buffer.size();

  // Helper function to trace an expansion and record it
  auto trace = [&](const Context& leaf) {
    std::vector<std::reference_wrapper<const Context>> stack;
    util::optional_ref<const Context> c;
    for(c = leaf; c && c != &from; c = c->direct_parent())
      stack.emplace_back(*c);
    assert(c && "Found NULL while mapping expansion, is someone trying to be clever?");
    assert(!stack.empty() && "Context expansion did not actually expand?");

    pack(buffer, (std::uint64_t)stack.size());
    for(auto it = stack.crbegin(), ite = stack.crend(); it != ite; ++it) {
      const Context& c = it->get();
      buffer.push_back((std::uint8_t)c.scope().relation());
      pack(buffer, (std::uint64_t)c.userdata[src.identifier()]);
    }
  };

  // Now we can write the entry for out friends to work with.
  // Format: [parent id] (Scope)
  auto cid = from.userdata[src.identifier()];
  pack(buffer, (std::uint64_t)cid);
  switch(s.type()) {
  case Scope::Type::point: {
    // Format: [module id] [offset]
    auto mo = s.point_data();
    pack(buffer, (std::uint64_t)mo.first.userdata[src.identifier()]);
    pack(buffer, (std::uint64_t)mo.second);
    break;
  }
  case Scope::Type::placeholder:
    // Format: [magic] [placeholder]
    pack(buffer, (std::uint64_t)0xF3F2F1F0ULL << 32);
    pack(buffer, (std::uint64_t)s.enumerated_data());
    break;
  case Scope::Type::unknown:
    // Format: [magic]
    pack(buffer, (std::uint64_t)0xF0F1F2F3ULL << 32);
    break;
  case Scope::Type::global:
  case Scope::Type::function:
  case Scope::Type::loop:
  case Scope::Type::line:
    assert(false && "PackedIds can't handle non-point Contexts!");
    std::abort();
  }

  // Format: [cnt] ((Relation) [context id])...
  trace(to);

  buffersize.fetch_add(buffer.size() - oldsz, std::memory_order_relaxed);
}

void IdPacker::notifyWavefront(DataClass ds) {
  if(ds.hasReferences() && ds.hasContexts()) {  // This is it!
    std::vector<uint8_t> ct;
    // Format: [global id] [mod cnt] (modules) [map cnt] (map entries...)
    pack(ct, (std::uint64_t)src.contexts().userdata[src.identifier()]);

    std::vector<std::string> mods;
    for(const Module& m: src.modules().iterate()) {
      const auto& id = m.userdata[src.identifier()];
      if(mods.size() <= id) mods.resize(id+1, "");
      mods.at(id) = m.path().string();
    }
    pack(ct, (std::uint64_t)mods.size());
    for(auto& s: mods) pack(ct, std::move(s));

    pack(ct, (std::uint64_t)stripcnt.load(std::memory_order_relaxed));
    ct.reserve(ct.size() + buffersize.load(std::memory_order_relaxed));
    for(const auto& ls: stripbuffers)
      ct.insert(ct.end(), ls.second.begin(), ls.second.end());

    // Format: ... [met cnt] ([id] [name])...
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
  ::unpack<std::uint64_t>(it);  // Skip over the global id

  exmod = &sink.module("/nonexistent/exmod");

  auto cnt = ::unpack<std::uint64_t>(it);
  for(std::size_t i = 0; i < cnt; i++)
    modmap.emplace_back(sink.module(::unpack<std::string>(it)));

  cnt = ::unpack<std::uint64_t>(it);
  for(std::size_t i = 0; i < cnt; i++) {
    Scope s;
    // Format: [parent id] (Scope) [cnt] [children ids]...
    unsigned int parent = ::unpack<std::uint64_t>(it);
    auto next = ::unpack<std::uint64_t>(it);
    if(next == (0xF0F1F2F3ULL << 32)) {
      // Format: [magic]
      s = {};  // Unknown Scope
    } else if(next == (0xF3F2F1F0ULL << 32)) {
      // Format: [magic] [placeholder]
      s = {Scope::placeholder, ::unpack<std::uint64_t>(it)};
    } else {
      // Format: [module id] [offset]
      auto off = ::unpack<std::uint64_t>(it);
      s = {modmap.at(next), off};
    }
    std::size_t cnt = ::unpack<std::uint64_t>(it);
    if(cnt == 0) {
      // TODO: Figure out how to handle Superpositions in IdPacker and IdUnpacker
      assert(false && "IdUnpacker currently doesn't handle Superpositions!");
      std::abort();
    }
    auto& scopes = exmap[parent][{(Relation)*it, s}];
    for(std::size_t x = 0; x < cnt; x++) {
      Relation rel = (Relation)*it;
      it++;
      auto id = ::unpack<std::uint64_t>(it);
      scopes.emplace_back(rel, Scope(*exmod, id));
    }
  }

  cnt = ::unpack<std::uint64_t>(it);
  for(std::size_t i = 0; i < cnt; i++) {
    auto id = ::unpack<std::uint64_t>(it);
    auto name = ::unpack<std::string>(it);
    metmap.insert({std::move(name), id});
  }

  ctxtree.clear();
}

std::optional<std::pair<util::optional_ref<Context>, Context&>>
IdUnpacker::classify(Context& c, NestedScope& ns) noexcept {
  util::call_once(once, [this]{ unpack(); });
  bool first = true;
  auto x = exmap.find(c.userdata[sink.identifier()]);
  assert(x != exmap.end() && "Missing data for Context `co`!");
  auto y = x->second.find(ns);
  assert(y != x->second.end() && "Missing data for Scope `s` from Context `co`!");
  util::optional_ref<Context> cr;
  std::reference_wrapper<Context> cc = c;
  for(const auto& next: y->second) {
    if(!first) {
      cc = sink.context(cc, ns).second;
      if(!cr) cr = cc;
    }
    ns = next;
    first = false;
  }
  return std::make_pair(cr, cc);
}

std::optional<unsigned int> IdUnpacker::identify(const Context& c) noexcept {
  switch(c.scope().flat().type()) {
  case Scope::Type::global:
    return globalid;
  case Scope::Type::point: {
    auto mo = c.scope().flat().point_data();
    assert(&mo.first == exmod && "point Scopes must reference the marker Module!");
    return mo.second;
  }
  default:
    assert(false && "Unhandled Scope in IdUnpacker");
    std::abort();
  }
}

std::optional<Metric::Identifier> IdUnpacker::identify(const Metric& m) noexcept {
  util::call_once(once, [this]{ unpack(); });
  auto it = metmap.find(m.name());
  assert(it != metmap.end() && "No data for Metric `m`!");
  return Metric::Identifier(m, it->second);
}

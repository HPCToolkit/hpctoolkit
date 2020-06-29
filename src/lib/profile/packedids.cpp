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

#include "packedids.hpp"

#include "util/log.hpp"

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

IdPacker::IdPacker() : ctxcnt(0) {}

Context& IdPacker::Classifier::context(Context& c, Scope& s) {
  std::vector<std::reference_wrapper<Context>> v;
  auto& cc = ClassificationTransformer::context(c, s, v);

  // We also need the ID of the final child Context. So we just emit it
  // and let the anti-recursion keep us from spinning out of control.
  v.emplace_back(sink.context(cc, s));

  // Now we can write the entry for out friends to work with.
  // Format: [parent id] (Scope) [cnt] ([type] [child id])...
  std::unique_lock<std::mutex> l(shared.ctxtree_m);
  if(shared.ctxseen[&c].emplace(s).second) {
    auto cid = c.userdata[sink.identifier()];
    pack(shared.ctxtree, (std::uint64_t)cid);
    if(s.type() == Scope::Type::point) {
      // Format: [module id] [offset]
      auto mo = s.point_data();
      pack(shared.ctxtree, (std::uint64_t)mo.first.userdata[sink.identifier()]);
      pack(shared.ctxtree, (std::uint64_t)mo.second);
    } else if(s.type() == Scope::Type::unknown) {
      // Format: [magic]
      pack(shared.ctxtree, (std::uint64_t)0xF0F1F2F3ULL << 32);
    } else
      util::log::fatal{} << "PackedIds can't handle non-point Contexts!";
    pack(shared.ctxtree, (std::uint64_t)v.size());
    for(Context& ct: v) {
      switch(ct.scope().type()) {
      case Scope::Type::global:
        util::log::fatal{} << "Global Contexts shouldn't come out of expansion!";
        break;
      case Scope::Type::unknown:
      case Scope::Type::point:
        shared.ctxtree.emplace_back(0);
        break;
      case Scope::Type::function:
      case Scope::Type::inlined_function:
        shared.ctxtree.emplace_back(1);
        break;
      case Scope::Type::loop:
        shared.ctxtree.emplace_back(2);
        break;
      }
      pack(shared.ctxtree, (std::uint64_t)ct.userdata[sink.identifier()]);
    }
    shared.ctxcnt += 1;
  }

  return cc;
}

IdPacker::Sink::Sink(IdPacker& s) : shared(s), wave(2) {};

void IdPacker::Sink::notifyWavefront(DataClass::singleton_t ds) {
  DataClass d = ds;
  int val = 2;
  if(d.hasReferences()) val = wave.fetch_sub(1, std::memory_order_acquire) - 1;
  if(d.hasContexts()) val = wave.fetch_sub(1, std::memory_order_acquire) - 1;
  if(val == 0) {  // This is it!
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

    pack(ct, (std::uint64_t)shared.ctxcnt);
    ct.insert(ct.end(), shared.ctxtree.begin(), shared.ctxtree.end());

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

void IdUnpacker::Expander::unpack() {
  auto it = shared.ctxtree.cbegin();
  ::unpack<std::uint64_t>(it);  // Skip over the global id

  shared.exmod = &sink.module("/nonexistent/exmod");
  shared.exfile = &sink.file("/nonexistent/exfile");
  shared.exfunc.reset(new Function(*shared.exmod));

  auto cnt = ::unpack<std::uint64_t>(it);
  for(std::size_t i = 0; i < cnt; i++)
    shared.modmap.emplace_back(sink.module(::unpack<std::string>(it)));

  cnt = ::unpack<std::uint64_t>(it);
  for(std::size_t i = 0; i < cnt; i++) {
    Scope s;
    // Format: [parent id] (Scope) [cnt] [children ids]...
    unsigned int parent = ::unpack<std::uint64_t>(it);
    auto next = ::unpack<std::uint64_t>(it);
    if(next == (0xF0F1F2F3ULL << 32)) {
      // Format: [magic]
      s = {};  // Unknown Scope
    } else {
      // Format: [module id] [offset]
      auto off = ::unpack<std::uint64_t>(it);
      s = {shared.modmap.at(next), off};
    }
    std::size_t cnt = ::unpack<std::uint64_t>(it);
    auto& scopes = shared.exmap[parent][s];
    for(std::size_t x = 0; x < cnt; x++) {
      auto ty = *it;
      it++;
      auto id = ::unpack<std::uint64_t>(it);
      switch(ty) {
      case 0:  // unknown or point -> point
        scopes.emplace_back(*shared.exmod, id);
        break;
      case 1:  // function or inlined_function -> inlined_function
        scopes.emplace_back(*shared.exfunc, *shared.exfile, id);
        break;
      case 2:  // loop -> loop
        scopes.emplace_back(Scope::loop, *shared.exfile, id);
        break;
      default:
        util::log::fatal{} << "Unrecognized packed Scope type " << ty;
      }
    }
  }
  shared.ctxtree.clear();
}

Context& IdUnpacker::Expander::context(Context& c, Scope& s) {
  util::call_once(shared.once, [this]{ unpack(); });
  Context* cp = &c;
  bool first = true;
  for(const auto& next: shared.exmap.at(c.userdata[sink.identifier()]).at(s)) {
    if(!first) cp = &sink.context(*cp, s);
    s = next;
    first = false;
  }
  return *cp;
}

void IdUnpacker::Finalizer::context(const Context& c, unsigned int& id) {
  switch(c.scope().type()) {
  case Scope::Type::global:
    id = shared.globalid;
    return;
  case Scope::Type::point: {
    auto mo = c.scope().point_data();
    if(&mo.first != shared.exmod)
      util::log::fatal{} << "Point scope with real Module in IdUnpacker!";
    id = mo.second;
    return;
  }
  case Scope::Type::inlined_function:
    if(&c.scope().function_data() != shared.exfunc.get())
      util::log::fatal{} << "inlined_function scope with real Function in IdUnpacker!";
    // fallthrough
  case Scope::Type::loop: {
    auto fl = c.scope().line_data();
    if(&fl.first != shared.exfile)
      util::log::fatal{} << "inlined_function scope with real File in IdUnpacker!";
    id = fl.second;
    return;
  }
  default:
    util::log::fatal{} << "Unrecognized Scope in IdUnpacker!";
  }
}

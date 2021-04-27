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

#include "scope.hpp"

#include "util/log.hpp"
#include "module.hpp"

#include <cassert>
#include <stdexcept>

using namespace hpctoolkit;

Scope::Scope() : ty(Type::unknown), data() {};
Scope::Scope(const Module& m, uint64_t o)
  : ty(Type::point), data(m, o) {};
Scope::Scope(const Module& m, uint64_t o, const File& s, uint64_t l)
  : ty(Type::classified_point), data(m, o, s, l) {};
Scope::Scope(call_t, const Module& m, uint64_t o)
  : ty(Type::call), data(m, o) {};
Scope::Scope(call_t, const Module& m, uint64_t o, const File& s, uint64_t l)
  : ty(Type::classified_call), data(m, o, s, l) {};
Scope::Scope(const Function& f) : ty(Type::function), data(f) {};
Scope::Scope(const Function& f, const File& s, uint64_t l)
  : ty(Type::inlined_function), data(f,s,l) {};
Scope::Scope(loop_t, const File& s, uint64_t l)
  : ty(Type::loop), data(s,l) {};
Scope::Scope(const File& s, uint64_t l)
  : ty(Type::line), data(s,l) {};
Scope::Scope(const File& s, uint64_t l, const Module& m, uint64_t o)
  : ty(Type::concrete_line), data(m,o,s,l) {};
Scope::Scope(ProfilePipeline&) : ty(Type::global), data() {};

std::pair<const Module&, uint64_t> Scope::point_data() const {
  switch(ty) {
  case Type::point:
  case Type::call:
    return data.point;
  case Type::classified_point:
  case Type::classified_call:
  case Type::concrete_line:
    return data.point_line.point;
  default:
    assert(false && "point_data is only valid on point Scopes!");
    std::abort();
  }
}

const Function& Scope::function_data() const {
  switch(ty) {
  case Type::function: return data.function;
  case Type::inlined_function: return data.function_line.function;
  default:
    assert(false && "function_data is only valid on function Scopes!");
    std::abort();
  }
}

std::pair<const File&, uint64_t> Scope::line_data() const {
  switch(ty) {
  case Type::classified_point:
  case Type::classified_call:
  case Type::concrete_line:
    return data.point_line.line;
  case Type::inlined_function: return data.function_line.line;
  case Type::loop:
  case Type::line:
    return data.line;
  default:
    assert(false && "line_data is only valid on line Scopes!");
    std::abort();
  }
}

bool Scope::operator==(const Scope& o) const noexcept {
  if(ty != o.ty) return false;
  switch(ty) {
  case Type::unknown: return true;
  case Type::global: return true;
  case Type::point:
  case Type::call:
    return data.point == o.data.point;
  case Type::classified_point:
  case Type::classified_call:
    return data.point_line == o.data.point_line;
  case Type::concrete_line: return data.point_line.line == o.data.point_line.line;
  case Type::function: return data.function == o.data.function;
  case Type::inlined_function: return data.function_line == o.data.function_line;
  case Type::loop:
  case Type::line:
    return data.line == o.data.line;
  }
  assert(false && "Invalid ty while comparing Scopes!");
  std::abort();
}

// Hashes
static constexpr unsigned int bits = std::numeric_limits<std::size_t>::digits;
static constexpr unsigned int mask = bits - 1;
static_assert(0 == (bits & (bits - 1)), "value to rotate must be a power of 2");
static constexpr std::size_t rotl(std::size_t n, unsigned int c) noexcept {
  return (n << (mask & c)) | (n >> (-(mask & c)) & mask);
}
std::size_t std::hash<Scope>::
operator()(const Scope &l) const noexcept {
  switch(l.ty) {
  case Scope::Type::unknown: return 0x5;
  case Scope::Type::global: return 0x3;
  case Scope::Type::point:
  case Scope::Type::call: {
    std::size_t sponge = l.ty == Scope::Type::point ? 0x9 : 0x19;
    sponge = rotl(sponge ^ h_mod(l.data.point.m), 1);
    sponge = rotl(sponge ^ h_u64(l.data.point.offset), 3);
    return sponge;
  }
  case Scope::Type::classified_point:
  case Scope::Type::classified_call: {
    std::size_t sponge = l.ty == Scope::Type::classified_point ? 0xC : 0x1C;
    sponge = rotl(sponge ^ h_mod(l.data.point_line.point.m), 1);
    sponge = rotl(sponge ^ h_u64(l.data.point_line.point.offset), 3);
    sponge = rotl(sponge ^ h_file(l.data.point_line.line.s), 5);
    sponge = rotl(sponge ^ h_u64(l.data.point_line.line.l), 7);
    return sponge;
  }
  case Scope::Type::function: return h_func(l.data.function.f);
  case Scope::Type::inlined_function: {
    std::size_t sponge = 0xE;
    sponge = rotl(sponge ^ h_func(l.data.function_line.function.f), 1);
    sponge = rotl(sponge ^ h_file(l.data.function_line.line.s), 3);
    sponge = rotl(sponge ^ h_u64(l.data.function_line.line.l), 5);
    return sponge;
  }
  case Scope::Type::loop:
  case Scope::Type::line: {
    std::size_t sponge = l.ty == Scope::Type::loop ? 0x11 : 0x13;
    sponge = rotl(sponge ^ h_file(l.data.line.s), 1);
    sponge = rotl(sponge ^ h_u64(l.data.line.l), 3);
    return sponge;
  }
  case Scope::Type::concrete_line: {
    std::size_t sponge = 0x17;
    sponge = rotl(sponge ^ h_file(l.data.point_line.line.s), 1);
    sponge = rotl(sponge ^ h_u64(l.data.point_line.line.l), 3);
    return sponge;
  }
  }
  return 0;  // unreachable
};

// Stringification
std::ostream& std::operator<<(std::ostream& os, const Scope& s) noexcept {
  auto point_str = [&]() -> std::string {
    std::ostringstream ss;
    auto mo = s.point_data();
    ss << "/" << mo.first.path().filename().string() << "+" << std::hex << mo.second;
    return ss.str();
  };
  auto func_str = [&]() -> std::string {
    std::ostringstream ss;
    const auto& f = s.function_data();
    ss << f.name;
    if(f.file)
      ss << "@/" << f.file->path().filename().string() << ":" << f.line;
    return ss.str();
  };
  auto line_str = [&]() -> std::string {
    std::ostringstream ss;
    auto fl = s.line_data();
    ss << fl.first.path().filename().string() << ":" << fl.second;
    return ss.str();
  };

  switch(s.type()) {
  case Scope::Type::unknown: return os << "(unknown)";
  case Scope::Type::global: return os << "(global)";
  case Scope::Type::point: return os << "(point){" << point_str() << "}";
  case Scope::Type::call: return os << "(call){" << point_str() << "}";
  case Scope::Type::classified_point:
    return os << "(c_point){" << point_str() << " @ " << line_str() << "}";
  case Scope::Type::classified_call:
    return os << "(c_call){" << point_str() << " @ " << line_str() << "}";
  case Scope::Type::function: return os << "(func){" << func_str() << "}";
  case Scope::Type::inlined_function:
    return os << "(inlined_func){" << func_str() << " called at " << line_str() << "}";
  case Scope::Type::loop: return os << "(loop){" << line_str() << "}";
  case Scope::Type::line: return os << "(line){" << line_str() << "}";
  case Scope::Type::concrete_line:
    return os << "(line){" << line_str() << " from " << point_str() << "}";
  }
  return os;
}

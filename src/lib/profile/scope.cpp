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

#include <stdexcept>

using namespace hpctoolkit;

constexpr Scope::loop_t Scope::loop;

Scope::Scope() : ty(Type::unknown), data() {};
Scope::Scope(const Module& m, uint64_t o) : ty(Type::point), data(m, o) {};
Scope::Scope(const Function& f) : ty(Type::function), data(f) {};
Scope::Scope(const Function& f, const File& s, uint64_t l)
  : ty(Type::inlined_function), data(f,s,l) {};
Scope::Scope(const Scope::loop_t&, const File& s, uint64_t l)
  : ty(Type::loop), data(s,l) {};
Scope::Scope(ProfilePipeline&) : ty(Type::global), data() {};

std::pair<const Module&, uint64_t> Scope::point_data() const {
  if(ty != Type::point)
    util::log::fatal() << "point_data() called on non-point Scope!";
  return {*data.module.module, data.module.offset};
}

const Function& Scope::function_data() const {
  switch(ty) {
  case Type::function: return *data.function.f;
  case Type::inlined_function: return *data.inlined_function.f;
  default: break;
  }
  util::log::fatal() << "function_data() called on non-function Scope!";
  std::exit(-1);
}

std::pair<const File&, uint64_t> Scope::line_data() const {
  switch(ty) {
  case Type::inlined_function:
    return {*data.inlined_function.s, data.inlined_function.l};
  case Type::loop: return {*data.line.s, data.line.l};
  default: break;
  }
  util::log::fatal() << "line_data() called on non-line-based Scope!";
  std::exit(-1);
}

bool Scope::operator==(const Scope& o) const noexcept {
  if(ty != o.ty) return false;
  switch(ty) {
  case Type::unknown: return true;
  case Type::global: return true;
  case Type::point:
    return data.module.module == o.data.module.module
        && data.module.offset == o.data.module.offset;
  case Type::function: return data.function.f == o.data.function.f;
  case Type::inlined_function:
    return data.inlined_function.f == o.data.inlined_function.f
        && data.inlined_function.s == o.data.inlined_function.s
        && data.inlined_function.l == o.data.inlined_function.l;
  case Type::loop:
    return data.line.s == o.data.line.s && data.line.l == o.data.line.l;
  }
  return false;  // unreachable
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
  case Scope::Type::point: {
    std::size_t sponge = 0x9;
    sponge = rotl(sponge ^ h_mod(l.data.module.module), 1);
    sponge = rotl(sponge ^ h_u64(l.data.module.offset), 3);
    return sponge;
  }
  case Scope::Type::function: return h_func(l.data.function.f);
  case Scope::Type::inlined_function: {
    std::size_t sponge = 0xC;
    sponge = rotl(sponge ^ h_func(l.data.inlined_function.f), 1);
    sponge = rotl(sponge ^ h_file(l.data.inlined_function.s), 3);
    sponge = rotl(sponge ^ h_u64(l.data.inlined_function.l), 5);
    return sponge;
  }
  case Scope::Type::loop: {
    std::size_t sponge = 0x11;
    sponge = rotl(sponge ^ h_file(l.data.line.s), 1);
    sponge = rotl(sponge ^ h_u64(l.data.line.l), 3);
    return sponge;
  }
  }
  return 0;  // unreachable
};

// Stringification
std::ostream& std::operator<<(std::ostream& os, const Scope& s) noexcept {
  switch(s.type()) {
  case Scope::Type::unknown: return os << "(unknown)";
  case Scope::Type::global: return os << "(global)";
  case Scope::Type::point: return os << "(point)";
  case Scope::Type::function: return os << "(func)";
  case Scope::Type::inlined_function: return os << "(inlined_func)";
  case Scope::Type::loop: return os << "(loop)";
  }
  return os;
}

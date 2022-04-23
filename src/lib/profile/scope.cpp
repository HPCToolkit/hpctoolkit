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

#include "scope.hpp"

#include "lexical.hpp"
#include "module.hpp"
#include "util/log.hpp"

#include "lib/prof-lean/placeholders.h"

#include <cassert>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

using namespace hpctoolkit;

Scope::Scope() : ty(Type::unknown), data(){};
Scope::Scope(const Module& m, uint64_t o) : ty(Type::point), data(m, o){};
Scope::Scope(const Function& f) : ty(Type::function), data(f){};
Scope::Scope(loop_t, const File& s, uint64_t l) : ty(Type::loop), data(s, l){};
Scope::Scope(const File& s, uint64_t l) : ty(Type::line), data(s, l){};
Scope::Scope(placeholder_t, uint64_t v) : ty(Type::placeholder), data(v){};
Scope::Scope(ProfilePipeline&) : ty(Type::global), data(){};

NestedScope::NestedScope(Relation r, Scope s) : m_relation(r), m_flat(std::move(s)) {
  assert(
      (m_relation != Relation::global || m_flat.type() == Scope::Type::global)
      && "Relation::global should only every be used with the Scope::Type::global!");
}

std::pair<const Module&, uint64_t> Scope::point_data() const {
  switch (ty) {
  case Type::point: return data.point;
  default: assert(false && "point_data is only valid on point Scopes!"); std::abort();
  }
}

const Function& Scope::function_data() const {
  switch (ty) {
  case Type::function: return data.function;
  default: assert(false && "function_data is only valid on function Scopes!"); std::abort();
  }
}

std::pair<const File&, uint64_t> Scope::line_data() const {
  switch (ty) {
  case Type::loop:
  case Type::line: return data.line;
  default: assert(false && "line_data is only valid on line Scopes!"); std::abort();
  }
}

uint64_t Scope::enumerated_data() const {
  switch (ty) {
  case Type::placeholder: return data.enumerated;
  default: assert(false && "enumerated_data is only valid on placeholder Scopes!"); std::abort();
  }
}

std::string_view Scope::enumerated_pretty_name() const {
  switch (ty) {
  case Type::placeholder: {
    const char* stdname = get_placeholder_name(data.enumerated);
    if (stdname == NULL)
      return std::string_view();
    return stdname;
  }
  default:
    assert(false && "enumerated_pretty_name is only valid on placeholder Scopes!");
    std::abort();
  }
}

std::string Scope::enumerated_fallback_name() const {
  switch (ty) {
  case Type::placeholder: {
    std::ostringstream ss;
    ss << std::hex;
    for (int shift = 56; shift >= 0; shift -= 8) {
      unsigned char c = (unsigned char)((data.enumerated >> shift) & 0xff);
      if (std::isprint(c))
        ss << c;
      else
        ss << '\\' << std::setw(2) << c;
    }
    return ss.str();
  }
  default:
    assert(false && "enumerated_fallback_name is only valid on placeholder Scopes!");
    std::abort();
  }
}

bool hpctoolkit::isCall(Relation r) noexcept {
  switch (r) {
  case Relation::global:
  case Relation::enclosure: return false;
  case Relation::call:
  case Relation::inlined_call: return true;
  }
  std::abort();
}

bool Scope::operator==(const Scope& o) const noexcept {
  if (ty != o.ty)
    return false;
  switch (ty) {
  case Type::unknown: return true;
  case Type::global: return true;
  case Type::point: return data.point == o.data.point;
  case Type::function: return data.function == o.data.function;
  case Type::loop:
  case Type::line: return data.line == o.data.line;
  case Type::placeholder: return data.enumerated == o.data.enumerated;
  }
  assert(false && "Invalid ty while comparing Scopes!");
  std::abort();
}
bool NestedScope::operator==(const NestedScope& o) const noexcept {
  return m_relation == o.m_relation && m_flat == o.m_flat;
}

bool Scope::operator<(const Scope& o) const noexcept {
  if (ty != o.ty)
    return ty < o.ty;
  switch (ty) {
  case Type::unknown: return false;  // Always equal
  case Type::global: return false;   // Always equal
  case Type::point: return data.point < o.data.point;
  case Type::function: return data.function < o.data.function;
  case Type::loop:
  case Type::line: return data.line < o.data.line;
  case Type::placeholder: return data.enumerated < o.data.enumerated;
  }
  assert(false && "Invalid ty while comparing Scopes!");
  std::abort();
}
bool NestedScope::operator<(const NestedScope& o) const noexcept {
  return m_relation != o.m_relation ? m_relation < o.m_relation : m_flat < o.m_flat;
}

// Hashes
static constexpr unsigned int bits = std::numeric_limits<std::size_t>::digits;
static constexpr unsigned int mask = bits - 1;
static_assert(0 == (bits & (bits - 1)), "value to rotate must be a power of 2");
static constexpr std::size_t rotl(std::size_t n, unsigned int c) noexcept {
  return (n << (mask & c)) | (n >> (-(mask & c)) & mask);
}
std::size_t std::hash<Scope>::operator()(const Scope& l) const noexcept {
  switch (l.ty) {
  case Scope::Type::unknown: return 0x5;
  case Scope::Type::global: return 0x3;
  case Scope::Type::point: {
    std::size_t sponge = 0x9;
    sponge = rotl(sponge ^ h_mod(l.data.point.m), 1);
    sponge = rotl(sponge ^ h_u64(l.data.point.offset), 3);
    return sponge;
  }
  case Scope::Type::function: return h_func(l.data.function.f);
  case Scope::Type::loop:
  case Scope::Type::line: {
    std::size_t sponge = l.ty == Scope::Type::loop ? 0x11 : 0x13;
    sponge = rotl(sponge ^ h_file(l.data.line.s), 1);
    sponge = rotl(sponge ^ h_u64(l.data.line.l), 3);
    return sponge;
  }
  case Scope::Type::placeholder: return rotl(0x15 ^ h_u64(l.data.enumerated), 1);
  }
  return 0;  // unreachable
};
std::size_t std::hash<NestedScope>::operator()(const NestedScope& ns) const noexcept {
  return rotl(h_rel(ns.relation()), 5) ^ h_scope(ns.flat());
}

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
    ss << f.name();
    if (auto src = f.sourceLocation())
      ss << "@/" << src->first.path().filename().string() << ":" << src->second;
    return ss.str();
  };
  auto line_str = [&]() -> std::string {
    std::ostringstream ss;
    auto fl = s.line_data();
    ss << fl.first.path().filename().string() << ":" << fl.second;
    return ss.str();
  };
  auto enumerated_str = [&]() -> std::string {
    std::ostringstream ss;
    ss << "0x" << std::hex << s.enumerated_data() << std::dec << " '"
       << s.enumerated_fallback_name() << "'";
    auto pretty = s.enumerated_pretty_name();
    if (!pretty.empty())
      ss << " \"" << pretty << "\"";
    return ss.str();
  };

  switch (s.type()) {
  case Scope::Type::unknown: return os << "(unknown)";
  case Scope::Type::global: return os << "(global)";
  case Scope::Type::point: return os << "(point){" << point_str() << "}";
  case Scope::Type::function: return os << "(func){" << func_str() << "}";
  case Scope::Type::loop: return os << "(loop){" << line_str() << "}";
  case Scope::Type::line: return os << "(line){" << line_str() << "}";
  case Scope::Type::placeholder: return os << "(placeholder){" << enumerated_str() << "}";
  }
  return os;
}

static const std::string rel_global = "global";
static const std::string rel_enclosure = "enclosure";
static const std::string rel_pcall = "nominal call";
static const std::string rel_icall = "inlined call";
std::string_view hpctoolkit::stringify(Relation r) noexcept {
  switch (r) {
  case Relation::global: return rel_global;
  case Relation::enclosure: return rel_enclosure;
  case Relation::call: return rel_pcall;
  case Relation::inlined_call: return rel_icall;
  }
  std::abort();
}
std::ostream& std::operator<<(std::ostream& os, Relation r) noexcept {
  os << stringify(r);
  return os;
}

std::ostream& std::operator<<(std::ostream& os, const NestedScope& ns) noexcept {
  switch (ns.relation()) {
  case Relation::global: break;
  case Relation::enclosure: os << "(enclosed sub-Scope):"; break;
  case Relation::call: os << "(nominal call to):"; break;
  case Relation::inlined_call: os << "(inlined call to):"; break;
  }
  os << ns.flat();
  return os;
}

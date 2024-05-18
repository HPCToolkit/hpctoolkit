// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#include "lexical.hpp"

#include "module.hpp"

#include "util/log.hpp"
#include "util/stable_hash.hpp"

#include <cassert>

using namespace hpctoolkit;

File::File(File&& f)
  : userdata(std::move(f.userdata), std::ref(*this)),
    u_path(std::move(f.path())) {};

File::File(ud_t::struct_t& rs, stdshim::filesystem::path p)
  : userdata(rs, std::ref(*this)), u_path(std::move(p)) {
  assert(!u_path().empty() && "Attempt to create a File with an empty path!");
}

util::stable_hash_state& hpctoolkit::operator<<(util::stable_hash_state& h, const File& f) noexcept {
  return h << f.path();
}

Function::Function(const Module& m, std::optional<uint64_t> o, std::string n)
  : m_module(&m), m_offset(o), m_name(std::move(n)) {};
Function::Function(const Module& m, std::optional<uint64_t> o, std::string n,
                   const File& f, uint64_t l)
  : m_module(&m), m_offset(o), m_name(std::move(n)), m_file(f), m_line(l) {};

void Function::offset(uint64_t offset) noexcept {
  assert(!m_offset && "Attempt to overwrite a Function's offset, use += instead!");
  m_offset = offset;
}

void Function::name(std::string name) noexcept {
  assert(!name.empty() && "Attempt to set a Function with an empty name!");
  assert(m_name.empty() && "Attempt to overwrite a Function's name, use += instead!");
  m_name = std::move(name);
}

void Function::sourceLocation(const File& file, uint64_t line) noexcept {
  assert(!m_file && "Attempt to overwrite a Function's source location, use += instead!");
  m_file = file;
  m_line = line;
}

void Function::mergeSmall(const Function& o) noexcept {
  if(!m_offset || (o.m_offset && *m_offset > *o.m_offset))
    m_offset = o.m_offset;
  if(!m_file || (o.m_file && (m_file->path() > o.m_file->path() ||
     (m_file == o.m_file && (m_line == 0 || (o.m_line > 0 && m_line > o.m_line)))))) {
    m_file = o.m_file;
    m_line = o.m_line;
  }
}
Function& Function::operator+=(const Function& o) noexcept {
  assert(m_module == o.m_module && "Attempt to merge incompatible Functions!");
  mergeSmall(o);
  if(m_name.empty() || (!o.m_name.empty() && m_name > o.m_name))
    m_name = o.m_name;
  return *this;
}
Function& Function::operator+=(Function&& o) noexcept {
  assert(m_module == o.m_module && "Attempt to merge incompatible Functions!");
  mergeSmall(o);
  if(m_name.empty() || (!o.m_name.empty() && m_name > o.m_name))
    m_name = std::move(o.m_name);
  return *this;
}

util::stable_hash_state& hpctoolkit::operator<<(util::stable_hash_state& h, const Function& f) noexcept {
  h << f.module() << f.offset().value_or(0) << f.name();
  if(auto sl = f.sourceLocation())
    h << sl->first << sl->second;
  return h;
}

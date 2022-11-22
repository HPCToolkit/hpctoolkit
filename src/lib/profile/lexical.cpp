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

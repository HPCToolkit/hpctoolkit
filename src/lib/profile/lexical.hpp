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
// Copyright ((c)) 2002-2024, Rice University
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

#ifndef HPCTOOLKIT_PROFILE_LEXICAL_H
#define HPCTOOLKIT_PROFILE_LEXICAL_H

#include "util/uniqable.hpp"
#include "util/ragged_vector.hpp"
#include "util/ref_wrappers.hpp"
#include "util/stable_hash.hpp"

#include "stdshim/filesystem.hpp"
#include <string>
#include <string_view>

namespace hpctoolkit {

namespace util {
class stable_hash_state;
}

class Module;

/// Application source File. Main data is just the full path to the source file,
/// but also includes userdata for more complex handling.
class File {
public:
  using ud_t = util::ragged_vector<const File&>;

  // Files must be created by the Pipeline
  File() = delete;
  ~File() = default;

  // Minimal motion for STL containers
  File(File&&);
  File(const File&) = delete;
  File& operator=(File&&) = delete;
  File& operator=(const File&) = delete;

  /// Get the full path to the source file
  // MT: Safe (const)
  const stdshim::filesystem::path& path() const { return u_path; }

  mutable ud_t userdata;

private:
  util::uniqable_key<stdshim::filesystem::path> u_path;

  File(ud_t::struct_t& rs, stdshim::filesystem::path p);

  friend class util::uniqued<File>;
  util::uniqable_key<stdshim::filesystem::path>& uniqable_key() {
    return u_path;
  }
};

util::stable_hash_state& operator<<(util::stable_hash_state&, const File&) noexcept;

/// High-level application Function(-like construct), within a particular
/// Module. These do not have userdata and are not allocated by the Pipeline.
///
/// It is expected that every Function object is a different logical/lexical
/// function in the source code.
class Function {
public:
  /// Functions can be constructed with some or all of their pieces.
  /// The arguments follow the available getter methods.
  explicit Function(const Module& mod)
    : Function(mod, std::nullopt) {};
  Function(const Module& mod, std::optional<uint64_t> offset)
    : Function(mod, offset, std::string()) {};
  Function(const Module&, std::optional<uint64_t>, std::string);
  Function(const Module& mod, std::string name, const File& file, uint64_t line)
    : Function(mod, std::nullopt, std::move(name), file, line) {};
  Function(const Module&, std::optional<uint64_t>, std::string, const File&, uint64_t);

  ~Function() = default;

  // Movable and copiable
  Function(Function&&) = default;
  Function(const Function&) = default;
  Function& operator=(Function&&) = default;
  Function& operator=(const Function&) = default;

  // Comparable
  bool operator==(const Function& o) const noexcept {
    return m_module == o.m_module && m_offset == o.m_offset && m_name == o.m_name
           && ((!m_file && !o.m_file)
               || (&*m_file == &*o.m_file && m_line == o.m_line));
  }
  bool operator!=(const Function& o) const noexcept { return !operator==(o); }

  /// Get the Module this Function is fully contained within.
  // MT: Safe (const)
  const Module& module() const noexcept { return *m_module; }

  /// Get the offset of this Function within the Module. For binaries, this is
  /// usually the offset of the entry instruction.
  /// If the offset is not known, returns std::nullopt.
  // MT: Safe (const)
  std::optional<uint64_t> offset() const noexcept { return m_offset; }

  /// Set the offset of this Function within its Module. The offset must not
  /// have been set in this Function prior to this call.
  // MT: Externally Synchronized
  void offset(uint64_t) noexcept;

  /// Get the name for this Function. If multiple names are available, this is
  /// the most human-readable one.
  /// If empty, this is an anonymous Function.
  // MT: Safe (const)
  const std::string& name() const noexcept { return m_name; }

  /// Set the name for this Function. The name must have been empty prior to
  /// this call, and `name` cannot be empty.
  // MT: Externally Synchronized
  void name(std::string) noexcept;

  /// Get the source file and line where this Function was defined.
  /// If unknown, returns std::nullopt.
  // MT: Safe (const)
  std::optional<std::pair<const File&, uint64_t>> sourceLocation() const noexcept {
    if(!m_file) return std::nullopt;
    return std::pair<const File&, uint64_t>(*m_file, m_line);
  }

  /// Set the source File and line where this Function was defined. The location
  /// must not have been set in this Function prior to this call.
  // MT: Externally Synchronized
  void sourceLocation(const File&, uint64_t) noexcept;

  /// Merge this Function with another, filling in data that is unknown in this
  /// Function and known in `other`. If the data between the two differ, a
  /// consistent one is chosen for the final result (i.e. += is commutative).
  // MT: Externally Synchronized
  Function& operator+=(const Function& other) noexcept;
  Function& operator+=(Function&& other) noexcept;

private:
  void mergeSmall(const Function&) noexcept;

  const Module* m_module;
  std::optional<uint64_t> m_offset;
  std::string m_name;
  util::optional_ref<const File> m_file;
  uint64_t m_line;
};

util::stable_hash_state& operator<<(util::stable_hash_state&, const Function&) noexcept;

}  // namespace hpctoolkit

template<>
struct std::hash<hpctoolkit::Function> {
  hpctoolkit::util::stable_hash<hpctoolkit::Function> stable;
  std::size_t operator()(const hpctoolkit::Function& f) const noexcept {
    return stable(f);
  }
};

#endif  // HPCTOOLKIT_PROFILE_LEXICAL_H

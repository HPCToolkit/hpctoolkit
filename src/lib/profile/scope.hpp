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

#ifndef HPCTOOLKIT_PROFILE_SCOPE_H
#define HPCTOOLKIT_PROFILE_SCOPE_H

#include "util/uniqable.hpp"
#include "util/ragged_vector.hpp"

#include "stdshim/filesystem.hpp"

namespace hpctoolkit {

class ProfilePipeline;

class Module;
class Function;
class File;

/// The Location of some Context, roughly. Basically its an indentifier of a
/// scope in the source (or binary) code.
class Scope {
public:
  /// Constructor for the default, from an "unknown" scope.
  Scope();

  /// Constructor for single-instruction "point" Scopes.
  Scope(const Module&, uint64_t offset);

  /// Constructor for Function-wide Scopes.
  explicit Scope(const Function&);

  /// Constructor for inlined Function call Scopes.
  Scope(const Function&, const File&, uint64_t line);

  struct loop_t {};
  static inline constexpr loop_t loop = {};

  /// Constructor for loop-construct Scopes.
  Scope(loop_t, const File&, uint64_t line);

  /// Constructor for single-line Scopes.
  Scope(const File&, uint64_t line);

  /// Copy constructors
  Scope(const Scope& s) = default;
  Scope& operator=(const Scope&) = default;

  /// Types of possible Scopes, roughly corrosponding to their sources.
  enum class Type {
    unknown,  ///< Some amount of missing Context data, of unknown depth.
    global,  ///< Scope of the global Context, root of the entire execution.
    point,  ///< A single instruction within the application, thus a "point".
    function,  ///< A normal ordinary function within the application.
    inlined_function,  ///< A function that has been inlined into an inclosing function Scope.
    loop,  ///< A loop-like construct, potentially source-level.
    line,  ///< A single line within the original source.
  };

  /// Get the Type of this Location. In case that happens to be interesting.
  Type type() const noexcept { return ty; }

  /// For 'point' scopes, get the Module and offset of this Scope.
  /// Throws if the Scope is not a '*_point', '*_call' or 'concrete_line' Scope.
  std::pair<const Module&, uint64_t> point_data() const;

  /// For '*function' scopes, get the Function that created this Scope.
  /// Throws if the Scope is not a 'function' or 'inlined_function' Scope.
  const Function& function_data() const;

  /// For Scopes with line info, get the line that created this Scope.
  /// Throws if the Scope is not a '*_line', 'loop' or 'classified_*' Scope.
  std::pair<const File&, uint64_t> line_data() const;

  // Comparison, as usual
  bool operator==(const Scope& o) const noexcept;
  bool operator!=(const Scope& o) const noexcept { return !operator==(o); }

private:
  Type ty;
  union Data {
    struct {} empty;
    struct point_u {
      const Module* m;
      uint64_t offset;
      bool operator==(const point_u& o) const {
        return m == o.m && offset == o.offset;
      }
      operator std::pair<const Module&, uint64_t>() const {
        return {*m, offset};
      }
    } point;
    struct function_u {
      const Function* f;
      bool operator==(const function_u& o) const {
        return f == o.f;
      }
      operator const Function&() const {
        return *f;
      }
    } function;
    struct line_u {
      const File* s;
      uint64_t l;
      bool operator==(const line_u& o) const {
        return s == o.s && l == o.l;
      }
      operator std::pair<const File&, uint64_t>() const {
        return {*s, l};
      }
    } line;
    struct function_line_u {
      function_u function;
      line_u line;
      bool operator==(const function_line_u& o) const {
        return function == o.function && line == o.line;
      }
    } function_line;
    Data() : empty{} {};
    Data(const Module& m, uint64_t o)
      : point{&m, o} {};
    Data(const Function& f)
      : function{&f} {};
    Data(const Function& f, const File& s, uint64_t l)
      : function_line{{&f}, {&s, l}} {};
    Data(const File& s, uint64_t l)
      : line{&s, l} {};
  } data;

  friend class std::hash<Scope>;

  // A special constructor only available to Profiles for the `global` Scope.
  friend class ProfilePipeline;
  explicit Scope(ProfilePipeline&);
};

// A full representation of a Function written somewhere in the code.
// Implicitly bound to a Module.
class Function {
public:
  Function(const Module& m)
    : name(), offset(0), file(nullptr), line(0), m_mod(m) {};
  Function(Function&& f) = default;
  Function(const Module& m, const std::string& n, uint64_t o = 0,
           const File* f = nullptr, uint64_t l = 0)
    : Function(m, std::string(n), o, f, l) {};
  Function(const Module& m, std::string&& n, uint64_t o = 0,
           const File* f = nullptr, uint64_t l = 0)
    : name(std::move(n)), offset(o), file(f), line(l), m_mod(m) {};
  ~Function() = default;

  // Human-friendly name for the function.
  std::string name;

  // Offset into the Module for this Function
  uint64_t offset;

  // Source information for this Function.
  const File* file;
  uint64_t line;

  // Technically this isn't right... but the current alternative is worse.
  const Module& module() const noexcept { return m_mod; }

private:
  const Module& m_mod;
};

// A full representation for a source File the code was built with.
class File {
public:
  using ud_t = util::ragged_vector<const File&>;

  File(ud_t::struct_t& rs) : userdata(rs, std::ref(*this)), u_path() {};
  File(File&& f)
    : userdata(std::move(f.userdata), std::ref(*this)), u_path(std::move(f.path())) {};
  File(ud_t::struct_t& rs, const stdshim::filesystem::path& p)
    : userdata(rs, std::ref(*this)), u_path(p) {};
  File(ud_t::struct_t& rs, stdshim::filesystem::path&& p)
    : userdata(rs, std::ref(*this)), u_path(p) {};
  ~File() = default;

  // Full-ish path to the file in question
  const stdshim::filesystem::path& path() const { return u_path; }

  mutable ud_t userdata;

private:
  util::uniqable_key<stdshim::filesystem::path> u_path;

  friend class util::uniqued<File>;
  util::uniqable_key<stdshim::filesystem::path>& uniqable_key() {
    return u_path;
  }
};

}

namespace std {
  using namespace hpctoolkit;
  template<> struct hash<Scope> {
    std::hash<const Function*> h_func;
    std::hash<const Module*> h_mod;
    std::hash<const File*> h_file;
    std::hash<uint64_t> h_u64;
    std::size_t operator()(const Scope&) const noexcept;
  };
  std::ostream& operator<<(std::ostream&, const Scope&) noexcept;
}

#endif  // HPCTOOLKIT_PROFILE_SCOPE_H

// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_SCOPE_H
#define HPCTOOLKIT_PROFILE_SCOPE_H

#include "util/uniqable.hpp"
#include "util/ragged_vector.hpp"

#include "stdshim/filesystem.hpp"
#include <string>
#include <string_view>

namespace hpctoolkit {

namespace util {
class stable_hash_state;
}

class ProfilePipeline;

class Module;
class Function;
class File;

/// The (flat) Scope in which operations occur, everything from functions or
/// or lines to individual instructions.
///
/// (Flat) Scopes represent a single, static (vauge) area of the application,
/// they do not consider surrounding Scopes or what they might mean in context.
/// See NestedScope for that.
class Scope {
public:
  /// Constructor for the default, from an "unknown" scope.
  Scope();

  /// Constructor for single-instruction "point" Scopes.
  Scope(const Module&, uint64_t offset);

  /// Constructor for Function-wide Scopes.
  explicit Scope(const Function&);

  struct loop_t {};
  static inline constexpr loop_t loop = {};

  /// Constructor for loop-construct Scopes.
  Scope(loop_t, const File&, uint64_t line);
  Scope(loop_t, const Module&, uint64_t offset, const File&, uint64_t line);

  /// Constructor for single-line Scopes.
  Scope(const File&, uint64_t line);

  struct placeholder_t {};
  static inline constexpr placeholder_t placeholder = {};

  /// Constructor for placeholder Scopes.
  Scope(placeholder_t, uint64_t value);

  /// Copy constructors
  Scope(const Scope& s) = default;
  Scope& operator=(const Scope&) = default;

  /// Full list of possible Scopes that can be represented.
  enum class Type {
    unknown,  ///< Some amount of missing Context data, of unknown depth.
    global,  ///< Scope of the global Context, root of the entire execution.
    point,  ///< A single instruction within the application, thus a "point".
    function,  ///< A normal ordinary function within the application.
    lexical_loop,  ///< A loop-like construct, potentially source-level.
    binary_loop,  ///< A loop-like construct, potentially source-level.
    line,  ///< A single line within the original source.
    placeholder,  ///< A marker context with special meaning (and nothing else).
  };

  /// Get the Type of this Location. In case that happens to be interesting.
  Type type() const noexcept { return ty; }

  /// For 'point' scopes, get the Module and offset of this Scope.
  /// Throws if the Scope is not a '*_point', '*_call' or 'concrete_line' Scope.
  std::pair<const Module&, uint64_t> point_data() const;

  /// For '*function' scopes, get the Function that created this Scope.
  /// Throws if the Scope is not a 'function' Scope.
  const Function& function_data() const;

  /// For Scopes with line info, get the line that created this Scope.
  /// Throws if the Scope is not a '*_line', 'loop' or 'classified_*' Scope.
  std::pair<const File&, uint64_t> line_data() const;

  /// For Scopes with enumerated info, get the raw enumeration value.
  /// Throws if the Scope is not a 'placeholder' Scope.
  uint64_t enumerated_data() const;

  /// For Scopes with enumerated info, get the constant "pretty" name for the
  /// value if one exists, otherwise returns an empty string.
  ///
  /// Exact meaning is slightly different depending on the Scope Type:
  ///  - 'placeholder': Standardized name (`<...>`) for the placeholder.
  /// Throws if the Scope is not one of the above types.
  std::string_view enumerated_pretty_name() const;

  /// For Scopes with enumerated info, get the fallback name for the value.
  ///
  /// Exact meaning is slightly different depending on the Scope Type:
  ///  - 'placeholder': "Shortcode" generated from the placeholder value.
  /// Throws if the Scope is not one of the above types.
  std::string enumerated_fallback_name() const;

  // Comparison, as usual
  bool operator==(const Scope& o) const noexcept;
  bool operator!=(const Scope& o) const noexcept { return !operator==(o); }
  // Total ordering
  bool operator<(const Scope& o) const noexcept;
  bool operator<=(const Scope& o) const noexcept { return !operator>(o); }
  bool operator>(const Scope& o) const noexcept { return o.operator<(*this); }
  bool operator>=(const Scope& o) const noexcept { return !operator<(o); }

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
      bool operator<(const point_u& o) const {
        return m != o.m ? m < o.m : offset < o.offset;
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
      bool operator<(const function_u& o) const {
        return f < o.f;
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
      bool operator<(const line_u& o) const {
        return s != o.s ? s < o.s : l < o.l;
      }
      operator std::pair<const File&, uint64_t>() const {
        return {*s, l};
      }
    } line;
    struct point_line_u {
      struct point_u point;
      struct line_u line;
      bool operator==(const point_line_u& o) const {
        return point == o.point && line == o.line;
      }
      bool operator<(const point_line_u& o) const {
        return !(point == o.point) ? point < o.point : line < o.line;
      }
    } point_line;
    uint64_t enumerated;
    Data() : empty{} {};
    Data(const Module& m, uint64_t o)
      : point{&m, o} {};
    Data(const Function& f)
      : function{&f} {};
    Data(const File& s, uint64_t l)
      : line{&s, l} {};
    Data(const Module& m, uint64_t o, const File& s, uint64_t l)
      : point_line{{&m, o}, {&s, l}} {};
    Data(uint64_t l)
      : enumerated{l} {};
  } data;

  friend class std::hash<Scope>;
  friend util::stable_hash_state& operator<<(util::stable_hash_state&, const Scope&) noexcept;

  // A special constructor only available to Profiles for the `global` Scope.
  friend class ProfilePipeline;
  explicit Scope(ProfilePipeline&);
};

util::stable_hash_state& operator<<(util::stable_hash_state&, const Scope&) noexcept;

/// The Relation a NestedScope has with its parent Scope. We nest Scopes to
/// indicate a number of different relationships while retaining a consistent
/// tree structure, this lists all the different possible Relations.
///
/// For all of these (except `global`), there is a parent Scope and a child
/// Scope, related by the given Relation.
enum class Relation {
  /// Special value specific to the root Context. There is no parent Scope.
  global,

  /// The parent Scope fully encloses the child Scope, the child Scope is a
  /// (usually strict) sub-Scope of the parent. For example, a source line
  /// within a function.
  enclosure,

  /// The parent Scope called the child Scope, using a typical nominal call.
  /// For example, a function called from a point instruction.
  call,

  /// The parent Scope called the child Scope, but the call was inlined and so
  /// did not "actually happen". For example, a function called from a source
  /// line (not a point since there is no associated call instruction).
  inlined_call,
};

/// Check if `r` indicates that the parent Scope called the child Scope.
bool isCall(Relation r) noexcept;

/// Stringification support for Relation enumeration constants
std::string_view stringify(Relation) noexcept;

util::stable_hash_state& operator<<(util::stable_hash_state&, Relation) noexcept;

/// Flat Scope that has a Relation with its parent. These are the core scopes
/// listed in the Context tree.
class NestedScope final {
public:
  NestedScope(Relation, Scope);
  ~NestedScope() = default;

  NestedScope(NestedScope&&) = default;
  NestedScope(const NestedScope&) = default;
  NestedScope& operator=(NestedScope&&) = default;
  NestedScope& operator=(const NestedScope&) = default;

  /// Access the Relation this NestedScope has with its parent.
  // MT: Externally Synchronized
  Relation& relation() noexcept { return m_relation; }

  /// Get the Relation this NestedScope has with its parent.
  // MT: Safe (const)
  const Relation& relation() const noexcept { return m_relation; }

  /// Access the flat Scope this NestedScope represents.
  // MT: Externally Synchronized
  Scope& flat() noexcept { return m_flat; }

  /// Get the flat Scope this NestedScope represents.
  // MT: Safe (const)
  const Scope& flat() const noexcept { return m_flat; }

  // Comparison, as usual
  bool operator==(const NestedScope& o) const noexcept;
  bool operator!=(const NestedScope& o) const noexcept { return !operator==(o); }
  // Total ordering
  bool operator<(const NestedScope& o) const noexcept;
  bool operator<=(const NestedScope& o) const noexcept { return !operator>(o); }
  bool operator>(const NestedScope& o) const noexcept { return o.operator<(*this); }
  bool operator>=(const NestedScope& o) const noexcept { return !operator<(o); }

private:
  Relation m_relation;
  Scope m_flat;
};

util::stable_hash_state& operator<<(util::stable_hash_state&, const NestedScope&) noexcept;

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
  template<> struct hash<NestedScope> {
    std::hash<Relation> h_rel;
    std::hash<Scope> h_scope;
    std::size_t operator()(const NestedScope&) const noexcept;
  };
  std::ostream& operator<<(std::ostream&, const Scope&) noexcept;
  std::ostream& operator<<(std::ostream&, Relation) noexcept;
  std::ostream& operator<<(std::ostream&, const NestedScope&) noexcept;
}

#endif  // HPCTOOLKIT_PROFILE_SCOPE_H

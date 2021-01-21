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

#ifndef HPCTOOLKIT_PROFILE_DATACLASS_H
#define HPCTOOLKIT_PROFILE_DATACLASS_H

#include <bitset>

namespace hpctoolkit {

/// Classification of the various kinds of data Pipelines handle. This is
/// written as a class to allow for bitmask-like operations.
class DataClass {
public:
  ~DataClass() = default;

  // Default constructor corrosponds to the empty set
  constexpr DataClass() noexcept : mask() {};

  // Enum-like class hierarchy for singleton sets.
private:
  class singleton_c { protected: singleton_c() = default; };
#define CONTENTS : public singleton_c {                  \
  DataClass operator|(const DataClass& o) const noexcept \
    { return DataClass(*this) | o; }                     \
  DataClass operator+(const DataClass& o) const noexcept \
    { return DataClass(*this) + o; }                     \
  DataClass operator-(const DataClass& o) const noexcept \
    { return DataClass(*this) - o; }                     \
  DataClass operator&(const DataClass& o) const noexcept \
    { return DataClass(*this) & o; }                     \
}
  struct attributes_c CONTENTS;
  struct threads_c    CONTENTS;
  struct references_c CONTENTS;
  struct metrics_c    CONTENTS;
  struct contexts_c   CONTENTS;
  struct timepoints_c CONTENTS;
#undef CONTENTS

public:
  using singleton_t  = const singleton_c&;
  using attributes_t = const attributes_c&;
  using threads_t    = const threads_c&;
  using references_t = const references_c&;
  using metrics_t    = const metrics_c&;
  using contexts_t   = const contexts_c&;
  using timepoints_t = const timepoints_c&;

  /// Emits the execution context for the Profile itself.
  static constexpr attributes_c attributes = {};
  /// Emits execution contexts for each Thread within the Profile.
  static constexpr threads_c    threads = {};
  /// Emits the Profile's references to the outside filesystem.
  static constexpr references_c references = {};
  /// Emits the individual measurements
  static constexpr metrics_c    metrics    = {};
  /// Emits the locations in which data was gathered.
  static constexpr contexts_c   contexts   = {};
  /// Emits the moments in time in which data was gathered.
  static constexpr timepoints_c timepoints = {};

  constexpr DataClass(const singleton_t& o) : mask(
    &o == &attributes ? 1<<0 :
    &o == &threads    ? 1<<1 :
    &o == &references ? 1<<2 :
    &o == &metrics    ? 1<<3 :
    &o == &contexts   ? 1<<4 :
    &o == &timepoints ? 1<<5 : 0) {};

  // Universal set
  static DataClass constexpr all() { return DataClass((1<<6) - 1); }

  // Named queries for particular elements
  bool hasAny()        const noexcept { return mask.any(); }
  bool hasAttributes() const noexcept { return mask[0]; }
  bool hasThreads()    const noexcept { return mask[1]; }
  bool hasReferences() const noexcept { return mask[2]; }
  bool hasMetrics()    const noexcept { return mask[3]; }
  bool hasContexts()   const noexcept { return mask[4]; }
  bool hasTimepoints() const noexcept { return mask[5]; }

  // Query for whether there are any of such and so
  bool has(singleton_t o) const noexcept { return allOf(o); }
  bool anyOf(const DataClass& o) const noexcept { return operator&(o).hasAny(); }
  bool allOf(const DataClass& o) const noexcept { return operator&(o) == o; }

  // DataClasses support | and + as union, - as set subtraction, and & as intersection.
  DataClass operator|(const DataClass& o) const noexcept { return mask | o.mask; }
  DataClass operator+(const DataClass& o) const noexcept { return operator|(o); }
  DataClass operator-(const DataClass& o) const noexcept { return mask & ~o.mask; }
  DataClass operator&(const DataClass& o) const noexcept { return mask & o.mask; }

  // Assignment variants
  DataClass& operator|=(const DataClass& o) noexcept { mask |= o.mask; return *this; }
  DataClass& operator+=(const DataClass& o) noexcept { return operator|=(o); }
  DataClass& operator-=(const DataClass& o) noexcept { mask &= ~o.mask; return *this; }
  DataClass& operator&=(const DataClass& o) noexcept { mask &= o.mask; return *this; }

  // Comparison operations
  bool operator==(const DataClass& o) const noexcept { return mask == o.mask; }
  bool operator!=(const DataClass& o) const noexcept { return !(operator==(o)); }

  // Debug printing
  template<class C, class T>
  friend std::basic_ostream<C,T>& operator<<(std::basic_ostream<C,T>& os,
                                             const DataClass& d) {
    os << '[';
    if(d.hasAttributes()) os << 'A';
    if(d.hasThreads()) os << 'T';
    if(d.hasReferences()) os << 'R';
    if(d.hasContexts()) os << 'C';
    if(d.anyOf(attributes | threads | references | contexts)
       && d.anyOf(metrics | timepoints)) os << ' ';
    if(d.hasMetrics()) os << 'm';
    if(d.hasTimepoints()) os << 't';
    os << ']';
    return os;
  }

private:
  constexpr DataClass(unsigned long long v) : mask(v) {};
  std::bitset<6> mask;
  constexpr DataClass(const decltype(mask)& m) : mask(m) {};
};

namespace literals::data {
using Class = DataClass;
static constexpr Class::attributes_t attributes = Class::attributes;
static constexpr Class::threads_t    threads    = Class::threads;
static constexpr Class::references_t references = Class::references;
static constexpr Class::metrics_t    metrics    = Class::metrics;
static constexpr Class::contexts_t   contexts   = Class::contexts;
static constexpr Class::timepoints_t timepoints = Class::timepoints;
}

/// Classification of the various kinds of data Pipelines can extend the
/// original data with. Written as a class for bitmask-like operations.
class ExtensionClass {
public:
  ~ExtensionClass() = default;

  // Default constructor corrosponds to the empty set
  ExtensionClass() noexcept : mask() {};

  // Enum-like class hierarchy for singleton sets.
private:
  class singleton_c { protected: singleton_c() = default; };
#define CONTENTS {                                       \
  ExtensionClass operator|(const ExtensionClass& o) const noexcept \
    { return ExtensionClass(*this) | o; }                     \
  ExtensionClass operator+(const ExtensionClass& o) const noexcept \
    { return ExtensionClass(*this) + o; }                     \
  ExtensionClass operator-(const ExtensionClass& o) const noexcept \
    { return ExtensionClass(*this) - o; }                     \
  ExtensionClass operator&(const ExtensionClass& o) const noexcept \
    { return ExtensionClass(*this) & o; }                     \
}
  struct classification_c    : public singleton_c CONTENTS;
  struct identifier_c        : public singleton_c CONTENTS;
  struct mscopeIdentifiers_c : public singleton_c CONTENTS;
  struct resolvedPath_c      : public singleton_c CONTENTS;
#undef CONTENTS

public:
  using singleton_t         = const singleton_c&;
  using classification_t    = const classification_c&;
  using identifier_t        = const identifier_c&;
  using mscopeIdentifiers_t = const mscopeIdentifiers_c&;
  using resolvedPath_t      = const resolvedPath_c&;

  /// Extends Modules with information on source lines and Functions.
  static constexpr classification_c    classification    = {};
  /// Extends most data with unique numerical identifiers.
  static constexpr identifier_c        identifier        = {};
  /// Extends Metrics with extra Scope-specific identifiers.
  static constexpr mscopeIdentifiers_c mscopeIdentifiers = {};
  /// Extends Files and Modules with the real path in the current filesystem.
  static constexpr resolvedPath_c      resolvedPath      = {};

  constexpr ExtensionClass(const singleton_t& o) : mask(
    &o == &classification    ? 1<<0 :
    &o == &identifier        ? 1<<1 :
    &o == &resolvedPath      ? 1<<2 :
    &o == &mscopeIdentifiers ? 1<<3 : 0) {};

  // Universal set
  static ExtensionClass constexpr all() { return ExtensionClass((1<<4) - 1); }

  // Named queries for particular elements
  bool hasAny()               const noexcept { return mask.any(); }
  bool hasClassification()    const noexcept { return mask[0]; }
  bool hasIdentifier()        const noexcept { return mask[1]; }
  bool hasResolvedPath()      const noexcept { return mask[2]; }
  bool hasMScopeIdentifiers() const noexcept { return mask[3]; }

  // Query for whether there are any of such and so
  bool anyOf(const ExtensionClass& o) const noexcept { return operator&(o).hasAny(); }
  bool allOf(const ExtensionClass& o) const noexcept { return operator&(o) == o; }

  // ExtensionClasses support | and + as union, - as set subtraction, and & as intersection.
  ExtensionClass operator|(const ExtensionClass& o) const noexcept { return mask | o.mask; }
  ExtensionClass operator+(const ExtensionClass& o) const noexcept { return operator|(o); }
  ExtensionClass operator-(const ExtensionClass& o) const noexcept { return mask & ~o.mask; }
  ExtensionClass operator&(const ExtensionClass& o) const noexcept { return mask & o.mask; }

  // Assignment variants
  ExtensionClass& operator|=(const ExtensionClass& o) noexcept { mask |= o.mask; return *this; }
  ExtensionClass& operator+=(const ExtensionClass& o) noexcept { return operator|=(o); }
  ExtensionClass& operator-=(const ExtensionClass& o) noexcept { mask &= ~o.mask; return *this; }
  ExtensionClass& operator&=(const ExtensionClass& o) noexcept { mask &= o.mask; return *this; }

  // Comparison operations
  bool operator==(const ExtensionClass& o) const noexcept { return mask == o.mask; }
  bool operator!=(const ExtensionClass& o) const noexcept { return !(operator==(o)); }

  // Debug printing
  template<class C, class T>
  friend std::basic_ostream<C,T>& operator<<(std::basic_ostream<C,T>& os,
                                             const ExtensionClass& e) {
    os << '[';
    if(e.hasIdentifier()) os << 'i';
    if(e.hasMScopeIdentifiers()) os << 'm';
    if(e.anyOf(identifier | mscopeIdentifiers)
       && e.anyOf(classification | resolvedPath)) os << ' ';
    if(e.hasClassification()) os << 'c';
    if(e.hasResolvedPath()) os << 'r';
    os << ']';
    return os;
  }

private:
  constexpr ExtensionClass(unsigned long long v) : mask(v) {};
  std::bitset<4> mask;
  constexpr ExtensionClass(const decltype(mask)& m) : mask(m) {};
};

namespace literals::extensions {
using Class = ExtensionClass;
static constexpr Class::classification_t    classification    = Class::classification;
static constexpr Class::identifier_t        identifier        = Class::identifier;
static constexpr Class::mscopeIdentifiers_t mscopeIdentifiers = Class::mscopeIdentifiers;
static constexpr Class::resolvedPath_t      resolvedPath      = Class::resolvedPath;
}
}  // namespace hpctoolkit

#endif  // HPCTOOLKIT_PROFILE_DATACLASS_H

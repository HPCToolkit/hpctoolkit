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
// Copyright ((c)) 2002-2023, Rice University
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

  // Default constructor corresponds to the empty set
  constexpr DataClass() noexcept : mask() {};

private:
  // Enum-like class hierarchy for singleton sets.
  class singleton_c { protected: singleton_c() = default; };
#define CONTENTS(BIT) : public singleton_c {             \
  DataClass operator|(const DataClass& o) const noexcept \
    { return DataClass(*this) | o; }                     \
  DataClass operator+(const DataClass& o) const noexcept \
    { return DataClass(*this) + o; }                     \
  DataClass operator-(const DataClass& o) const noexcept \
    { return DataClass(*this) - o; }                     \
  DataClass operator&(const DataClass& o) const noexcept \
    { return DataClass(*this) & o; }                     \
  constexpr operator DataClass() const noexcept          \
    { return DataClass(1ULL << BIT); }                   \
private:                                                 \
  friend class DataClass;                                \
  static constexpr int bit = BIT;                        \
}
  struct attributes_c       CONTENTS(0);
  struct threads_c          CONTENTS(1);
  struct references_c       CONTENTS(2);
  struct metrics_c          CONTENTS(3);
  struct contexts_c         CONTENTS(4);
  struct ctxTimepoints_c    CONTENTS(5);
  struct metricTimepoints_c CONTENTS(6);
  std::bitset<7> mask;
#undef CONTENTS

public:
  /// Emits the execution context for the Profile itself.
  static constexpr attributes_c       attributes = {};
  /// Emits execution contexts for each Thread within the Profile.
  static constexpr threads_c          threads = {};
  /// Emits the Profile's references to the outside filesystem.
  static constexpr references_c       references = {};
  /// Emits the individual measurements
  static constexpr metrics_c          metrics    = {};
  /// Emits the locations in which data was gathered.
  static constexpr contexts_c         contexts   = {};
  /// Emits the locations in which data was gathered, over time.
  static constexpr ctxTimepoints_c    ctxTimepoints = {};
  /// Emits values for gathered measurements, over time.
  static constexpr metricTimepoints_c metricTimepoints = {};

  // Universal set
  static DataClass constexpr all() {
    return (1ULL<<decltype(mask)(0).size()) - 1;
  }

  // Named queries for particular elements
  bool hasAny()              const noexcept { return mask.any(); }
  bool hasAttributes()       const noexcept { return has(attributes); }
  bool hasThreads()          const noexcept { return has(threads); }
  bool hasReferences()       const noexcept { return has(references); }
  bool hasMetrics()          const noexcept { return has(metrics); }
  bool hasContexts()         const noexcept { return has(contexts); }
  bool hasCtxTimepoints()    const noexcept { return has(ctxTimepoints); }
  bool hasMetricTimepoints() const noexcept { return has(metricTimepoints); }

  // Query for whether there are any of such and so
  template<class Singleton>
  std::enable_if_t<std::is_base_of_v<singleton_c, Singleton>, bool>
  has(Singleton o) const noexcept { return mask[Singleton::bit]; }
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
    if(d.has(attributes)) os << 'A';
    if(d.has(threads)) os << 'T';
    if(d.has(references)) os << 'R';
    if(d.has(contexts)) os << 'C';
    if(d.anyOf(attributes | threads | references | contexts)
       && d.anyOf(metrics | ctxTimepoints)) os << ' ';
    if(d.has(metrics)) os << 'm';
    if(d.has(ctxTimepoints)) os << 't';
    if(d.has(metricTimepoints)) os << 'v';
    os << ']';
    return os;
  }

private:
  constexpr DataClass(unsigned long long v) : mask(v) {};
  constexpr DataClass(decltype(mask) m) : mask(m) {};
};

namespace literals::data {
using Class = DataClass;
static constexpr auto attributes       = Class::attributes;
static constexpr auto threads          = Class::threads;
static constexpr auto references       = Class::references;
static constexpr auto metrics          = Class::metrics;
static constexpr auto contexts         = Class::contexts;
static constexpr auto ctxTimepoints    = Class::ctxTimepoints;
static constexpr auto metricTimepoints = Class::metricTimepoints;
}

/// Classification of the various kinds of data Pipelines can extend the
/// original data with. Written as a class for bitmask-like operations.
class ExtensionClass {
public:
  ~ExtensionClass() = default;

  // Default constructor corresponds to the empty set
  ExtensionClass() noexcept : mask() {};

private:
  // Enum-like class hierarchy for singleton sets.
  class singleton_c { protected: singleton_c() = default; };
#define CONTENTS(BIT) : public singleton_c {                       \
  ExtensionClass operator|(const ExtensionClass& o) const noexcept \
    { return ExtensionClass(*this) | o; }                          \
  ExtensionClass operator+(const ExtensionClass& o) const noexcept \
    { return ExtensionClass(*this) + o; }                          \
  ExtensionClass operator-(const ExtensionClass& o) const noexcept \
    { return ExtensionClass(*this) - o; }                          \
  ExtensionClass operator&(const ExtensionClass& o) const noexcept \
    { return ExtensionClass(*this) & o; }                          \
  constexpr operator ExtensionClass() const noexcept               \
    { return ExtensionClass(1<<BIT); }                             \
private:                                                           \
  friend class ExtensionClass;                                     \
  static constexpr int bit = BIT;                                  \
}
  struct classification_c    CONTENTS(0);
  struct identifier_c        CONTENTS(1);
  struct resolvedPath_c      CONTENTS(2);
  struct statistics_c        CONTENTS(3);
  std::bitset<4> mask;
#undef CONTENTS

public:
  /// Extends Modules with information on source lines and Functions.
  static constexpr classification_c    classification    = {};
  /// Extends most data with unique numerical identifiers.
  static constexpr identifier_c        identifier        = {};
  /// Extends Files and Modules with the real path in the current filesystem.
  static constexpr resolvedPath_c      resolvedPath      = {};
  /// Extends Metrics with additional Statistics for better summary analysis.
  static constexpr statistics_c        statistics        = {};

  // Universal set
  static ExtensionClass constexpr all() {
    return (1ULL<<decltype(mask)(0).size()) - 1;
  }

  // Named queries for particular elements
  bool hasAny()               const noexcept { return mask.any(); }
  bool hasClassification()    const noexcept { return has(classification); }
  bool hasIdentifier()        const noexcept { return has(identifier); }
  bool hasResolvedPath()      const noexcept { return has(resolvedPath); }
  bool hasStatistics()        const noexcept { return has(statistics); }

  // Query for whether there are any of such and so
  template<class Singleton>
  std::enable_if_t<std::is_base_of_v<singleton_c, Singleton>, bool>
  has(Singleton s) const noexcept { return mask[Singleton::bit]; }
  bool anyOf(const ExtensionClass& o) const noexcept { return operator&(o).hasAny(); }
  bool allOf(const ExtensionClass& o) const noexcept { return operator&(o) == o; }

  // Copy-constructable and copy-assignable
  ExtensionClass(const ExtensionClass&) noexcept = default;
  ExtensionClass& operator=(const ExtensionClass&) noexcept = default;
  ExtensionClass(ExtensionClass&&) noexcept = default;
  ExtensionClass& operator=(ExtensionClass&&) noexcept = default;

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
    if(e.has(identifier)) os << 'i';
    if(e.has(statistics)) os << 's';
    if(e.anyOf(identifier | statistics)
       && e.anyOf(classification | resolvedPath)) os << ' ';
    if(e.has(classification)) os << 'c';
    if(e.has(resolvedPath)) os << 'r';
    os << ']';
    return os;
  }

private:
  constexpr ExtensionClass(unsigned long long v) : mask(v) {};
  constexpr ExtensionClass(decltype(mask) m) : mask(m) {};
};

namespace literals::extensions {
using Class = ExtensionClass;
static constexpr auto classification    = Class::classification;
static constexpr auto identifier        = Class::identifier;
static constexpr auto resolvedPath      = Class::resolvedPath;
static constexpr auto statistics        = Class::statistics;
}
}  // namespace hpctoolkit

#endif  // HPCTOOLKIT_PROFILE_DATACLASS_H

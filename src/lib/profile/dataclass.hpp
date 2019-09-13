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
  DataClass() noexcept : mask() {};

  // Enum-like class hierarchy for singleton sets.
private:
  class singleton_c { protected: singleton_c() = default; };
#define CONTENTS {                                       \
  DataClass operator|(const DataClass& o) const noexcept \
    { return DataClass(*this) | o; }                     \
  DataClass operator+(const DataClass& o) const noexcept \
    { return DataClass(*this) + o; }                     \
  DataClass operator-(const DataClass& o) const noexcept \
    { return DataClass(*this) - o; }                     \
  DataClass operator&(const DataClass& o) const noexcept \
    { return DataClass(*this) & o; }                     \
}
  struct attributes_c : public singleton_c CONTENTS;
  struct references_c : public singleton_c CONTENTS;
  struct metrics_c    : public singleton_c CONTENTS;
  struct contexts_c   : public singleton_c CONTENTS;
  struct trace_c      : public singleton_c CONTENTS;
#undef CONTENTS

public:
  using singleton_t  = const singleton_c&;
  using attributes_t = const attributes_c&;
  using references_t = const references_c&;
  using metrics_t    = const metrics_c&;
  using contexts_t   = const contexts_c&;
  using trace_t      = const trace_c&;

  static constexpr attributes_c attributes = {};
  static constexpr references_c references = {};
  static constexpr metrics_c    metrics    = {};
  static constexpr contexts_c   contexts   = {};
  static constexpr trace_c      trace      = {};

  constexpr DataClass(const singleton_t& o) : mask(
    &o == &attributes ? 1<<0 :
    &o == &references ? 1<<1 :
    &o == &metrics    ? 1<<2 :
    &o == &contexts   ? 1<<3 :
    &o == &trace      ? 1<<4 : 0) {};

  // Universal set
  static DataClass constexpr all() { return DataClass((1<<5) - 1); }

  // Named queries for particular elements
  bool has_any()        const noexcept { return mask.any(); }
  bool has_attributes() const noexcept { return mask[0]; }
  bool has_references() const noexcept { return mask[1]; }
  bool has_metrics()    const noexcept { return mask[2]; }
  bool has_contexts()   const noexcept { return mask[3]; }
  bool has_trace()      const noexcept { return mask[4]; }

  // Query for whether there are any of such and so
  bool has_any(const DataClass& o) const noexcept { return operator&(o).has_any(); }
  bool has_all(const DataClass& o) const noexcept { return operator&(o) == o; }

  // DataClasses are copiable and movable
  DataClass(const DataClass& o) : mask(o.mask) {};
  DataClass(DataClass&& o) : mask(std::move(o.mask)) {};

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

private:
  constexpr DataClass(unsigned long long v) : mask(v) {};
  std::bitset<5> mask;
  constexpr DataClass(const decltype(mask)& m) : mask(m) {};
};

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
  struct classification_c : public singleton_c CONTENTS;
  struct identifier_c     : public singleton_c CONTENTS;
#undef CONTENTS

public:
  using singleton_t      = const singleton_c&;
  using classification_t = const classification_c&;
  using identifier_t     = const identifier_c&;

  static constexpr classification_c classification = {};
  static constexpr identifier_c     identifier     = {};

  constexpr ExtensionClass(const singleton_t& o) : mask(
    &o == &classification ? 1<<0 :
    &o == &identifier     ? 1<<1 : 0) {};

  // Universal set
  static ExtensionClass constexpr all() { return ExtensionClass((1<<2) - 1); }

  // Named queries for particular elements
  bool has_any()            const noexcept { return mask.any(); }
  bool has_classification() const noexcept { return mask[0]; }
  bool has_identifier()     const noexcept { return mask[1]; }

  // Query for whether there are any of such and so
  bool has_any(const ExtensionClass& o) const noexcept { return operator&(o).has_any(); }
  bool has_all(const ExtensionClass& o) const noexcept { return operator&(o) == o; }

  // ExtensionClasses are copiable and movable
  ExtensionClass(const ExtensionClass& o) : mask(o.mask) {};
  ExtensionClass(ExtensionClass&& o) : mask(std::move(o.mask)) {};

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
private:
  constexpr ExtensionClass(unsigned long long v) : mask(v) {};
  std::bitset<2> mask;
  constexpr ExtensionClass(const decltype(mask)& m) : mask(m) {};
};

namespace literals {

DataClass operator""_dat(const char* s, std::size_t sz);

ExtensionClass operator""_ext(const char* s, std::size_t sz);

}  // namespace literals
}  // namespace hpctoolkit

#endif  // HPCTOOLKIT_PROFILE_DATACLASS_H

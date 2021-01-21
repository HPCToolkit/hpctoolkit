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

#ifndef HPCTOOLKIT_PROFILE_UTIL_UNIQABLE_H
#define HPCTOOLKIT_PROFILE_UTIL_UNIQABLE_H

#include <functional>
#include <memory>
#include <iterator>
#include <utility>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>

/// \file
/// Helper header for writing managable "uniqable" objects. A commong trait
/// among managed objects is the concept that they need to be unique'd among a
/// proper subset of their members, while others need to stay mutable for
/// linking into a larger substrate. The STL structures impose an extra `const`
/// to ensure the contents are (logically) accurate. Some approaches are:
///  1. Use `const_cast`. Downside is that `operator=` becomes unusuable.
///  2. Use `mutable`. Downside is that `const` is significantly weakened.
///  3. Use a `std::pair` underneath. Downside is a C++ type management mess.
/// We take an alternative of approach 2, using templates and a runtime check
/// to make everything work out in the end.
///
/// Any class `X` that wishes to be "uniqable" requires the following:
///  - Any members of Obj that are to be unique'd (e.g. `X::uniq`) on must be
///    wrapped in `util::uniqable_key`. The internal value is available via
///    calling (`X::uniq`) or implicit conversion.
///  - Rather than the usual data structures, special versions designed to
///    handle the difference are needed (`uniqued_set`, `unordered_uniqued_set`,
///    and `locked_unordered_uniqued_set`).

namespace hpctoolkit::util {

namespace {
  template<template<typename...> class, typename...>
  struct is_instantiation : public std::false_type {};

  template<template<typename...> class U, typename... T>
  struct is_instantiation<U, U<T...>> : public std::true_type {};
}

template<class, class> class locked_unordered_set;

// Wrapper that inserts a `const` into the stack.
template<class T>
class uniqable_key {
public:
  template<class... Args>
  uniqable_key(Args&&... args) : real(std::forward<Args>(args)...) {};
  ~uniqable_key() = default;

  using type = T;

  const T& operator()() const noexcept { return real; }
  operator const T&() const noexcept { return real; }

private:
  const T real;
};

namespace {
  template<class> struct is_ukey_ref : public std::false_type {};
  template<class T>
  struct is_ukey_ref<uniqable_key<T>&> : public std::true_type {};
}

// SFINAE checker for uniqable.
template<class T>
using is_uniqable = typename is_ukey_ref<
    decltype(std::declval<T>().uniqable_key())
  >::value;

// Wrapper to let the STL handle the internals by uniqable_key rather than by
// direct value, as well as to insert a `mutable` into the stack.
template<class T> //, typename std::enable_if<is_uniqable<T>::value, int>::type = 0>
class uniqued {
public:
  uniqued(const uniqued& o) : real(o.real) {};
  uniqued(uniqued&& o) : real(std::move(o.real)) {};
  template<class... Args>
  uniqued(Args&&... args) : real(std::forward<Args>(args)...) {};
  ~uniqued() = default;

  using type = T;

  // Various conversion operators. Allows automatic or x() syntax.
  T& operator()() const { return real; }
  operator T&() const { return real; }

private:
  friend class std::hash<uniqued>;
  mutable T real;
  using key_type = typename std::decay<decltype(std::declval<T>().uniqable_key())>::type::type;

  const key_type& _u_key() const { return real.uniqable_key(); }

public:
  bool operator==(const uniqued& o) const { return _u_key() == o._u_key(); }
  bool operator<(const uniqued& o) const { return _u_key() < o._u_key(); }
};

// A simple class to add ::at and ::operator[] support to sets, for unique'd
// objects (to make the interface more... conveient).
template<class Set>
class maplike : public Set {
private:
  using wrapped_type = typename Set::value_type::type;

public:
  template<class... Args>
  maplike(Args&&... args) : Set(std::forward<Args>(args)...) {};
  ~maplike() = default;

  wrapped_type& at(const wrapped_type& k) {
    auto it = Set::find(k);
    if(it == Set::end()) throw std::out_of_range("Invalid key in maplike set!");
    return *it;
  }
  const wrapped_type& at(const wrapped_type& k) const {
    auto it = Set::find(k);
    if(it == Set::end()) throw std::out_of_range("Invalid key in maplike set!");
    return *it;
  }

  wrapped_type& operator[](const wrapped_type& k) {
    return *Set::emplace(k).first;
  }
  wrapped_type& operator[](wrapped_type&& k) {
    return *Set::emplace(k).first;
  }
};

// Convenience templates for when you want to use maplike and uniqued.
template<template<class...> class T, class U, class... A>
using uniqued_maplike = maplike<T<uniqued<U>, A...>>;

template<class U, class... A>
using uniqued_set = uniqued_maplike<std::set, U, A...>;

template<class U, class... A>
using unordered_uniqued_set = uniqued_maplike<std::unordered_set, U, A...>;

template<class U, class... A>
using locked_unordered_uniqued_set = uniqued_maplike<util::locked_unordered_set, U, A...>;

}

template<class T>
class std::hash<hpctoolkit::util::uniqued<T>> {
private:
  using u_type = typename hpctoolkit::util::uniqued<T>;
  using k_type = typename u_type::key_type;
  std::hash<k_type> real;

public:
  std::size_t operator()(const u_type& v) const { return real(v._u_key()); }
};

#endif  // HPCTOOLKIT_PROFILE_UTIL_UNIQUABLE_H

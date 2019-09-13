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

#ifndef HPCTOOLKIT_PROFILE_UTIL_LOCKED_UNORDERED_H
#define HPCTOOLKIT_PROFILE_UTIL_LOCKED_UNORDERED_H

#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <type_traits>
#include <stdexcept>
#include "../stdshim/shared_mutex.hpp"

namespace hpctoolkit::internal {

namespace {
  struct general_ {};
  struct special_ : general_ {};

  template<class Mtx>
  using eif_shared = typename std::enable_if<
      std::is_void<decltype(std::declval<Mtx>().lock_shared())>::value,
    int>::type;
}

/// A simple parallel wrapper around a std::unordered_map.
template<class K, class V, class M = stdshim::shared_mutex,
         class H = std::hash<K>, class E = std::equal_to<K>>
class locked_unordered_map {
protected:
  using real_t = std::unordered_map<K, V, H, E>;

public:
  locked_unordered_map() = default;
  locked_unordered_map(std::initializer_list<typename real_t::value_type> it)
    : real(it) {};
  locked_unordered_map(locked_unordered_map&& o) : real(std::move(o.real)) {};
  ~locked_unordered_map() = default;

  locked_unordered_map& operator=(locked_unordered_map&& o) {
    real = std::move(o.real);
    return *this;
  }

  using size_type = typename real_t::size_type;
  using difference_type = typename real_t::difference_type;
  using allocator_type = typename real_t::allocator_type;
  using pointer = typename std::allocator_traits<allocator_type>::pointer;
  using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;

  using iterator = typename real_t::iterator;
  using const_iterator = typename real_t::const_iterator;

  /// Get the value for a key, creating an entry if nessesary.
  // MT: Internally Synchronized
  V& operator[](const K& k) { return opget(special_(), k).first; }
  V& operator[](K&& k) { return opget(special_(), k).first; }

  /// Get the value for a key, throwing if it doesn't exist.
  // MT: Internally Synchronized
  const V& at(const K& k) const {
    auto r = find(k);
    if(r == nullptr) throw std::out_of_range("Attempt to at() a nonexistent key!");
    return *r;
  }

  /// Insert an entry into the map, if it didn't already exist.
  // MT: Internally Synchronized
  std::pair<V&,bool> insert(const typename real_t::value_type& v) {
    return opget(special_(), v.first, v.second);
  }

  /// Add a new pair to the map, if it didn't already exist.
  // MT: Internally Synchronized
  template<class... Args>
  std::pair<V&,bool> emplace(Args&&... args) {
    typename real_t::value_type v(std::forward<Args>(args)...);
    return opget(special_(), std::move(v.first), std::move(v.second));
  }

  /// Look up an entry in the map, returning a pointer or nullptr.
  // MT: Internally Synchronized, Unstable
  V* find(const K& k) { return opget_r(special_(), k); }
  const V* find(const K& k) const { return opget_r(special_(), k); }

  /// Check whether the map is empty.
  // MT: Externally Synchronized
  bool empty() const noexcept { return real.empty(); }

  /// Rudimentary iteration support. Just for now.
  iterator begin() noexcept { return real.begin(); }
  const_iterator begin() const noexcept { return real.begin(); }
  const_iterator cbegin() const noexcept { return real.cbegin(); }
  iterator end() noexcept { return real.end(); }
  const_iterator end() const noexcept { return real.end(); }
  const_iterator cend() const noexcept { return real.cend(); }

  /// Get the size of the map.
  // MT: Externally Synchronized
  size_type size() const noexcept {
    return real.size();
  }

protected:
  mutable M lock;
  real_t real;

private:
  template<class T, class... Args, class Mtx, eif_shared<Mtx> = 0>
  std::pair<V&, bool> opget(special_, T k, Args&&... args) {
    {
      stdshim::shared_lock<M> l(lock);
      auto x = real.find(k);
      if(x != real.end()) return {x->second, false};
    }
    return opget(general_(), k, std::forward<Args>(args)...);
  }
  template<class T, class... Args>
  std::pair<V&, bool> opget(general_, T k, Args&&... args) {
    std::unique_lock<M> l(lock);
    auto x = real.emplace(std::piecewise_construct,
                          std::tuple<T>(std::forward<T>(k)),
                          std::tuple<Args...>(std::forward<Args>(args)...));
    return {x.first->second, x.second};
  }
  template<class T, class Mtx, eif_shared<Mtx> = 0>
  V* opget_r(special_, const T& k) {
    stdshim::shared_lock<M> l(lock);
    auto x = real.find(k);
    return x == real.end() ? nullptr : &x->second;
  }
  template<class T, class Mtx, eif_shared<Mtx> = 0>
  const V* opget_r(special_, const T& k) const {
    stdshim::shared_lock<M> l(lock);
    auto x = real.find(k);
    return x == real.end() ? nullptr : &x->second;
  }
  template<class T>
  V* opget_r(general_, const T& k) {
    std::unique_lock<M> l(lock);
    auto x = real.find(k);
    return x == real.end() ? nullptr : &x->second;
  }
  template<class T>
  const V* opget_r(general_, const T& k) const {
    std::unique_lock<M> l(lock);
    auto x = real.find(k);
    return x == real.end() ? nullptr : &x->second;
  }
};

/// A simple parallel wrapper around a std::unordered_set.
template<class K, class M = stdshim::shared_mutex>
class locked_unordered_set {
protected:
  using real_t = std::unordered_set<K>;

public:
  locked_unordered_set() = default;
  ~locked_unordered_set() = default;

  using value_type = typename real_t::value_type;
  using iterator = typename real_t::iterator;
  using const_iterator = typename real_t::const_iterator;

  /// Insert a new element into the set, returning a reference.
  // MT: Internally Synchronized
  template<class... Args>
  std::pair<const K&, bool> emplace(Args&&... args) {
    return opget(K(std::forward<Args>(args)...), special_());
  }

  /// Variant of emplace() that strips the second argument.
  // MT: Internally Synchronized
  template<class... Args>
  const K& ensure(Args&&... args) {
    return *emplace(std::forward<Args>(args)...).first;
  }

  /// Look for whether an element (or its equivalent) is in the set.
  // MT: Weird But Not Really Synchronized
  iterator find(const K& k) { return opget_r(k, special_()); }
  const_iterator find(const K& k) const { return opget_r(k, special_()); }

  /// Check whether the map is empty.
  // MT: Externally Synchronized
  bool empty() const noexcept { return real.empty(); }

  /// Get the current size of the set.
  // MT: Externally Synchronized
  std::size_t size() const noexcept { return real.size(); }

  /// Erase an element.
  // MT: Internally Synchronized
  std::size_t erase(const K& k) {
    std::unique_lock<M> lk(lock);
    return real.erase(k);
  }

  /// Rudimentary iteration support. Just for now.
  iterator begin() noexcept { return real.begin(); }
  const_iterator begin() const noexcept { return real.begin(); }
  const_iterator cbegin() const noexcept { return real.cbegin(); }
  iterator end() noexcept { return real.end(); }
  const_iterator end() const noexcept { return real.end(); }
  const_iterator cend() const noexcept { return real.cend(); }

protected:
  M lock;
  real_t real;

private:
  std::pair<const K&, bool> opget(const K& k, general_) {
    std::unique_lock<M> l(lock);
    auto x = real.emplace(k);
    return {*x.first, x.second};
  }
  std::pair<const K&, bool> opget(K&& k, general_) {
    std::unique_lock<M> l(lock);
    auto x = real.emplace(std::move(k));
    return {*x.first, x.second};
  }

  iterator opget_r(const K& k, general_) {
    std::unique_lock<M> l(lock);
    return real.find(k);
  }
  const_iterator opget_r(const K& k, general_) const {
    std::unique_lock<M> l(lock);
    return real.find(k);
  }

  template<class Mtx, eif_shared<Mtx> = 0>
  std::pair<const K&, bool> opget(const K& k, special_) {
    {
      stdshim::shared_lock<Mtx> l(lock);
      auto x = real.find(k);
      if(x != real.end()) return {*x, false};
    }
    {
      std::unique_lock<Mtx> l(lock);
      auto x = real.emplace(k);
      return {*x.first, x.second};
    }
  }
  template<class Mtx, eif_shared<Mtx> = 0>
  std::pair<const K&, bool> opget(K&& k, special_) {
    {
      stdshim::shared_lock<Mtx> l(lock);
      auto x = real.find(k);
      if(x != real.end()) return {*x, false};
    }
    {
      std::unique_lock<Mtx> l(lock);
      auto x = real.emplace(std::move(k));
      return {*x.first, x.second};
    }
  }

  template<class Mtx, eif_shared<Mtx> = 0>
  iterator opget_r(const K& k, special_) {
    stdshim::shared_lock<M> l(lock);
    return real.find(k);
  }
  template<class Mtx, eif_shared<Mtx> = 0>
  const_iterator opget_r(const K& k, special_) const {
    stdshim::shared_lock<M> l(lock);
    return real.find(k);
  }
};

}

#endif  // HPCTOOLKIT_PROFILE_UTIL_LOCKED_UNORDERED_H

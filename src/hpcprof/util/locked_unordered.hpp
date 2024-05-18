// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_UTIL_LOCKED_UNORDERED_H
#define HPCTOOLKIT_PROFILE_UTIL_LOCKED_UNORDERED_H

#include "ref_wrappers.hpp"

#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <type_traits>
#include <stdexcept>
#include "../stdshim/shared_mutex.hpp"

namespace hpctoolkit::util {

namespace {
  template<class...> struct sfinae_true : std::true_type {};

  // C++20 Concepts for dummies (C++11)
  template<class M> static auto SharedMutex_helper(int)
    -> sfinae_true<decltype(std::declval<M>().lock_shared()),
                   decltype(std::declval<M>().try_lock_shared()),
                   decltype(std::declval<M>().unlock_shared())>;
  template<class M> static auto SharedMutex_helper(long)
    -> std::false_type;
  template<class M> struct SharedMutex : decltype(SharedMutex_helper<M>((int)0)) {};

  // Sanity check that everything is working as designed
  static_assert(SharedMutex<stdshim::shared_mutex>::value);
  static_assert(!SharedMutex<std::mutex>::value);

  // Helper: expands to the most read-friendly lock supported by a mutex.
  template<class M> using su_lock = typename std::conditional<
    SharedMutex<M>::value, std::shared_lock<M>, std::unique_lock<M>>::type;
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

  using key_type = typename real_t::key_type;
  using mapped_type = typename real_t::mapped_type;
  using value_type = typename real_t::value_type;
  using size_type = typename real_t::size_type;
  using difference_type = typename real_t::difference_type;
  using hasher = typename real_t::hasher;
  using key_equal = typename real_t::key_equal;
  using reference = typename real_t::reference;
  using const_reference = typename real_t::const_reference;
  using allocator_type = typename real_t::allocator_type;
  using pointer = typename std::allocator_traits<allocator_type>::pointer;
  using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;

  using iterator = typename real_t::iterator;
  using const_iterator = typename real_t::const_iterator;

  /// Get the value for a key, creating an entry if necessary.
  // MT: Internally Synchronized
  V& operator[](const K& k) { return opget(su_lock<M>(lock), k).first; }
  V& operator[](K&& k) { return opget(su_lock<M>(lock), std::move(k)).first; }

  /// Get the value for a key, throwing if it doesn't exist.
  // MT: Internally Synchronized
  V& at(const K& k) {
    auto r = find(k);
    if(!r) throw std::out_of_range("Attempt to at() a nonexistent key!");
    return *r;
  }
  const V& at(const K& k) const {
    auto r = find(k);
    if(!r) throw std::out_of_range("Attempt to at() a nonexistent key!");
    return *r;
  }

  /// Insert an entry into the map, if it didn't already exist.
  // MT: Internally Synchronized
  std::pair<V&,bool> insert(const typename real_t::value_type& v) {
    return opget(su_lock<M>(lock), v.first, v.second);
  }

  /// Add a new pair to the map, if it didn't already exist.
  // MT: Internally Synchronized
  template<class... Args>
  std::pair<V&,bool> emplace(Args&&... args) {
    typename real_t::value_type v(std::forward<Args>(args)...);
    return opget(su_lock<M>(lock), std::move(v.first), std::move(v.second));
  }

  // Add a new element to the map, if the key was not found before.
  template<class... Args>
  std::pair<V&,bool> try_emplace(const K& k, Args&&... args) {
    return opget(su_lock<M>(lock), k, std::forward<Args>(args)...);
  }
  template<class... Args>
  std::pair<V&,bool> try_emplace(K&& k, Args&&... args) {
    return opget(su_lock<M>(lock), std::move(k), std::forward<Args>(args)...);
  }

  /// Look up an entry in the map. May return std::nullopt.
  // MT: Internally Synchronized, Unstable
  optional_ref<V> find(const K& k) { return opget_r(su_lock<M>(lock), k); }
  optional_ref<const V> find(const K& k) const { return opget_r(su_lock<M>(lock), k); }

  /// Clear the map.
  // MT: Externally Synchronized
  void clear() noexcept { real.clear(); }

  /// Check whether the map is empty.
  // MT: Externally Synchronized
  bool empty() const noexcept { return real.empty(); }

  // Erase an element from the map. Returns the number of elements removed (0 or 1).
  // MT: Internally Synchronized
  size_type erase(const K& key) noexcept {
    std::unique_lock<M> l(lock);
    return real.erase(key);
  }

private:
  /// Iteration support structure, to ensure the internal lock is held.
  class iteration {
  public:
    iterator begin() const noexcept { return from.real.begin(); }
    iterator end() const noexcept { return from.real.end(); }
  private:
    friend class locked_unordered_map;
    locked_unordered_map& from;
    std::unique_lock<M> lk;
    iteration(locked_unordered_map& m) : from(m), lk(m.lock) {};
  };
  class const_iteration {
  public:
    const_iterator begin() const noexcept { return from.real.begin(); }
    const_iterator end() const noexcept { return from.real.end(); }
  private:
    friend class locked_unordered_map;
    const locked_unordered_map& from;
    su_lock<M> lk;
    const_iteration(const locked_unordered_map& m) : from(m), lk(m.lock) {};
  };

public:
  /// Iteration support.
  iteration iterate() noexcept { return *this; }
  const_iteration iterate() const noexcept { return *this; }
  const_iteration citerate() const noexcept { return *this; }

  /// Get the size of the map.
  // MT: Externally Synchronized
  size_type size() const noexcept {
    return real.size();
  }

protected:
  mutable M lock;
  real_t real;

private:
  template<class KA, class... Args>
  std::pair<V&, bool> opget(std::unique_lock<M>&&, KA&& k, Args&&... args) {
    auto x = real.emplace(std::piecewise_construct,
                          std::forward_as_tuple(std::forward<KA>(k)),
                          std::forward_as_tuple(std::forward<Args>(args)...));
    return {x.first->second, x.second};
  }
  template<class Mtx, class KA, class... Args>
  std::pair<V&, bool> opget(std::shared_lock<Mtx>&& l, KA&& k, Args&&... args) {
    {
      std::shared_lock<Mtx> l2 = std::move(l);
      auto x = real.find(k);
      if(x != real.end()) return {x->second, false};
    }
    return opget(std::unique_lock<Mtx>(lock), std::forward<KA>(k), std::forward<Args>(args)...);
  }

  optional_ref<V> opget_r(su_lock<M>&&, K k) {
    auto x = real.find(std::move(k));
    if(x == real.end()) return std::nullopt;
    return x->second;
  }
  optional_ref<const V> opget_r(su_lock<M>&&, K k) const {
    auto x = real.find(std::move(k));
    if(x == real.end()) return std::nullopt;
    return x->second;
  }
};

/// A simple parallel wrapper around a std::unordered_set.
template<class K, class M = stdshim::shared_mutex, class H = std::hash<K>,
         class E = std::equal_to<K>>
class locked_unordered_set {
protected:
  using real_t = std::unordered_set<K, H, E>;

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
    return opget(K(std::forward<Args>(args)...), su_lock<M>(lock));
  }

  /// Variant of emplace() that strips the second argument.
  // MT: Internally Synchronized
  template<class... Args>
  const K& ensure(Args&&... args) {
    return *emplace(std::forward<Args>(args)...).first;
  }

  /// Look for whether an element (or its equivalent) is in the set.
  // MT: Weird But Not Really Synchronized
  iterator find(const K& k) { return opget_r(k, su_lock<M>(lock)); }
  const_iterator find(const K& k) const { return opget_r(k, su_lock<M>(lock)); }

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

private:
  /// Iteration support structure, to ensure the internal lock is held.
  class iteration {
  public:
    iterator begin() const noexcept { return from.real.begin(); }
    iterator end() const noexcept { return from.real.end(); }
  private:
    friend class locked_unordered_set;
    locked_unordered_set& from;
    std::unique_lock<M> lk;
    iteration(locked_unordered_set& m) : from(m), lk(m.lock) {};
  };
  class const_iteration {
  public:
    const_iterator begin() const noexcept { return from.real.begin(); }
    const_iterator end() const noexcept { return from.real.end(); }
  private:
    friend class locked_unordered_set;
    const locked_unordered_set& from;
    su_lock<M> lk;
    const_iteration(const locked_unordered_set& m) : from(m), lk(m.lock) {};
  };

public:
  /// Iteration support.
  iteration iterate() noexcept { return *this; }
  const_iteration iterate() const noexcept { return *this; }
  const_iteration citerate() const noexcept { return *this; }

protected:
  mutable M lock;
  real_t real;

private:
  std::pair<const K&, bool> opget(K k, std::unique_lock<M>&&) {
    auto x = real.emplace(std::move(k));
    return {*x.first, x.second};
  }
  template<class Mtx>
  std::pair<const K&, bool> opget(const K& k, std::shared_lock<Mtx>&& l) {
    {
      std::shared_lock<Mtx> l2 = std::move(l);
      auto x = real.find(k);
      if(x != real.end()) return {*x, false};
    }
    return opget(k, std::unique_lock<Mtx>(lock));
  }
  template<class Mtx>
  std::pair<const K&, bool> opget(K&& k, std::shared_lock<Mtx>&& l) {
    {
      std::shared_lock<Mtx> l2 = std::move(l);
      auto x = real.find(k);
      if(x != real.end()) return {*x, false};
    }
    return opget(std::move(k), std::unique_lock<Mtx>(lock));
  }

  iterator opget_r(const K& k, su_lock<M>&&) {
    return real.find(k);
  }
  const_iterator opget_r(const K& k, su_lock<M>&&) const {
    return real.find(k);
  }
};

}

#endif  // HPCTOOLKIT_PROFILE_UTIL_LOCKED_UNORDERED_H

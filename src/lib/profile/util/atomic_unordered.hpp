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

#ifndef HPCTOOLKIT_PROFILE_UTIL_ATOMIC_UNORDERED_H
#define HPCTOOLKIT_PROFILE_UTIL_ATOMIC_UNORDERED_H

#include <atomic>
#include <cmath>
#include <memory>
#include "../stdshim/shared_mutex.hpp"

namespace hpctoolkit::util {

template<class K, class H = std::hash<K>, class Eq = std::equal_to<K>,
         class A = std::allocator<K>>
class atomic_unordered_set {
public:
  using key_type = K;
  using value_type = K;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using hasher = H;
  using key_equal = Eq;
  using allocator_type = A;
  using reference = K&;
  using const_reference = const K&;
  using pointer = typename std::allocator_traits<A>::pointer;
  using const_pointer = typename std::allocator_traits<A>::const_pointer;

  /// Constructors for empty sets, with the given initial size.
  atomic_unordered_set() : atomic_unordered_set(0) {};
  atomic_unordered_set(size_type sz, const A& a)
    : atomic_unordered_set(sz, H(), Eq(), a) {};
  atomic_unordered_set(size_type sz, const H& h, const A& a)
    : atomic_unordered_set(sz, h, Eq(), a) {};
  explicit atomic_unordered_set(const A& a)
    : atomic_unordered_set(0, H(), Eq(), a) {};
  explicit atomic_unordered_set(size_type sz, const H& h = H(),
                                const Eq& eq = Eq(), const A& a = A());

  /// Constructors for sets copied from a given iterator.
  template<class It>
  atomic_unordered_set(It beg, It end, size_type sz, const A& a)
    : atomic_unordered_set(beg, end, sz, H(), Eq(), a) {};
  template<class It>
  atomic_unordered_set(It beg, It end, size_type sz, const H& h, const A& a)
    : atomic_unordered_set(beg, end, sz, h, Eq(), a) {};
  template<class It>
  atomic_unordered_set(It beg, It end, size_type sz = 0, const H& h = H(),
                       const Eq& eq = Eq(), const A& a = A());

  /// Constructors for sets copied from another set.
  // MT: Externally Synchronized (o)
  atomic_unordered_set(const atomic_unordered_set& o)
    : atomic_unordered_set(o, std::allocator_traits<allocator_type>
      ::select_on_container_copy_construction(o.get_allocator())) {};
  atomic_unordered_set(const atomic_unordered_set& o, const A& a);

  /// Constructors for sets moved from another set.
  // MT: Externally Synchronized (o)
  atomic_unordered_set(atomic_unordered_set&& o)
    : atomic_unordered_set(std::move(o), o.m_alloc) {};
  atomic_unordered_set(atomic_unordered_set&& o, const A& a);

  /// Constructors for sets initialized with an initializer list.
  atomic_unordered_set(std::initializer_list<value_type> it, size_type sz,
                       const A& a)
    : atomic_unordered_set(it, sz, H(), Eq(), a) {};
  atomic_unordered_set(std::initializer_list<value_type> it, size_type sz,
                       const H& h, const A& a)
    : atomic_unordered_set(it, sz, h, Eq(), a) {};
  atomic_unordered_set(std::initializer_list<value_type> it, size_type sz = 0,
                       const H& h = H(), const Eq& eq = Eq(), const A& a = A())
    : atomic_unordered_set(it.begin(), it.end(), sz, h, eq, a) {};

  /// Destructor.
  ~atomic_unordered_set();

  /// Copy assignment
  // MT: Externally Synchronized (this, o)
  atomic_unordered_set& operator=(const atomic_unordered_set& o);

  /// Move assignment
  // MT: Externally Synchronized (this, o)
  atomic_unordered_set& operator=(atomic_unordered_set&& o);

  /// Replacement assignment
  // MT: Externally Synchronized
  atomic_unordered_set& operator=(std::initializer_list<value_type> it);

  /// Get the allocator in use by this container.
  // MT: Externally Synchronized
  A get_allocator() const { return m_alloc; }

  // TODO: iterator, const_iterator, begin(), cbegin(), end(), cend()
  using iterator = value_type*;
  using const_iterator = const value_type*;
  iterator begin();
  const_iterator begin() const;
  const_iterator cbegin() const;
  iterator end();
  const_iterator end() const;
  const_iterator cend() const;

  /// Check if the container is empty.
  // MT: Internally Synchronized, Unstable
  bool empty() const noexcept { return size() == 0; }

  /// Get the number of elements currently in the container.
  // MT: Internally Synchronized, Unstable
  size_type size() const noexcept;

  /// Get the (theoretical) upper bound on the size of the container.
  // MT: Safe (static/const)
  size_type max_size() const noexcept {
    return std::numeric_limits<difference_type>::max();
  }

  /// Clear the container, resulting in an empty set.
  // MT: Externally Synchronized
  void clear() noexcept;

  /// Insert a new element into the container, by copy or by move.
  // MT: Internally Synchronized
  std::pair<iterator, bool> insert(const value_type& k);
  std::pair<iterator, bool> insert(value_type&& k);

  /// Insert a new element into the container, with an (unused) hint.
  // MT: Internally Synchronized
  iterator insert(const_iterator, const value_type& k) { return insert(k).first; }
  iterator insert(const_iterator, value_type&& k) { return insert(std::move(k)).first; }

  /// Insert a sequence of elements into the container.
  // MT: Internally Synchronized
  template<class It>
  void insert(It beg, It end) { for(It it = beg; it != end; ++it) insert(*it); }
  void insert(std::initializer_list<value_type> il) { insert(il.begin(), il.end()); }

  /// Insert a new element into the container, by construction.
  // MT: Internally Synchronized
  template<class... Args>
  std::pair<iterator, bool> emplace(Args&&... args);

  /// Insert a new element into the container, by construction with a hint.
  // MT: Internally Synchronized
  template<class... Args>
  std::pair<iterator, bool> emplace_hint(const_iterator, Args&&... args) {
    return emplace(std::forward<Args>(args)...);
  }

  /// Remove an element from the container, given its position.
  // MT: Externally Synchronized
  iterator erase(const_iterator pos);

  /// Remove a number of elements from the container.
  // MT: Externally Synchronized
  iterator erase(const_iterator beg, const_iterator end);

  /// Remove the element matching the given one, if it exists.
  // MT: Externally Synchronized
  size_type erase(const K& k);

  /// Swap the contents of two containers.
  // MT: Externally Synchronized (this, o)
  void swap(atomic_unordered_set& o);

  /// Get the number of elements that are equivalent to k.
  // MT: Internally Synchronized, Unstable
  size_type count(const K& k) const;

  /// Find the element if any exist equivalent to k.
  // MT: Internally Synchronized, Unstable
  iterator find(const K& k);
  const_iterator find(const K& k) const;

  /// Get the range of elements equivalent to k.
  // MT: Internally Synchronized, Unstable
  std::pair<iterator,iterator> equal_range(const K& k) {
    auto x = find(k);
    return {x, x};
  }
  std::pair<const_iterator,const_iterator> equal_range(const K& k) const {
    auto x = find(k);
    return {x, x};
  }

  /// Get the current maximum size of the container.
  // MT: Internally Synchronized, Unstable
  size_type slot_count() const;

  /// Get the current usage factor.
  // MT: Internally Synchronized, Unstable
  float usage_factor() const;

  /// Get the current maximum usage factor.
  // MT: Externally Synchronized
  float max_usage_factor() const { return max_usage; }

  /// Set the current maximum usage factor. May issue a rehash.
  // MT: Externally Synchronized
  void max_usage_factor(float m);

  /// Sets the total number of slots to the larger of sz and size(),
  /// and rehashes the contents.
  // MT: Externally Synchronized
  void rehash(size_type sz);

  /// Reserve enough space for the given number of elements.
  // MT: Externally Synchronized
  void reserve(size_type cnt) {
    if(slot_count()*max_usage_factor() > cnt) return;
    rehash(std::ceil(cnt / max_usage_factor()));
  }

  /// Get the function used to hash the elements.
  // MT: Externally Synchronized
  H hash_function() const { return m_hash; }

  /// Get the function used to compare elements.
  // MT: Externally Synchronized
  Eq key_eq() const { return m_equal; }

private:
  // Tuning parameters.
  float max_usage;

  // Important storage bits, for proper standards compat
  H m_hash;
  Eq m_equal;
  A m_alloc;
};

template<class K, class V, class H = std::hash<K>, class Eq = std::equal_to<K>,
         class A = std::allocator<std::pair<const K, V>>>
class atomic_unordered_map {
public:
  using key_type = K;
  using mapped_type = V;
  using value_type = std::pair<const K, V>;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using hasher = H;
  using key_equal = Eq;
  using allocator_type = A;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = typename std::allocator_traits<A>::pointer;
  using const_pointer = typename std::allocator_traits<A>::const_pointer;

  /// Constructors for empty sets, with the given initial size.
  atomic_unordered_map() : atomic_unordered_map(0) {};
  atomic_unordered_map(size_type sz, const A& a)
    : atomic_unordered_map(sz, H(), Eq(), a) {};
  atomic_unordered_map(size_type sz, const H& h, const A& a)
    : atomic_unordered_map(sz, h, Eq(), a) {};
  explicit atomic_unordered_map(const A& a)
    : atomic_unordered_map(0, H(), Eq(), a) {};
  explicit atomic_unordered_map(size_type sz, const H& h = H(),
                                const Eq& eq = Eq(), const A& a = A())
    : lock(), data_cnt(sz), data(nullptr), used(0),
      max_usage(0.8), m_hash(h), m_equal(eq), m_alloc(a) {
    if(sz != 0) {
      data = new std::atomic<value_type*>[sz];
      for(size_type i = 0; i < sz; i++)
        data[i].store(nullptr, std::memory_order_relaxed);
    }
  }

  /// Constructors for sets copied from a given iterator.
  template<class It>
  atomic_unordered_map(It beg, It end, size_type sz, const A& a)
    : atomic_unordered_map(beg, end, sz, H(), Eq(), a) {};
  template<class It>
  atomic_unordered_map(It beg, It end, size_type sz, const H& h, const A& a)
    : atomic_unordered_map(beg, end, sz, h, Eq(), a) {};
  template<class It>
  atomic_unordered_map(It beg, It end, size_type sz = 0, const H& h = H(),
                       const Eq& eq = Eq(), const A& a = A());

  /// Constructors for sets copied from another set.
  // MT: Externally Synchronized (o)
  atomic_unordered_map(const atomic_unordered_map& o)
    : atomic_unordered_map(o, std::allocator_traits<allocator_type>
      ::select_on_container_copy_construction(o.get_allocator())) {};
  atomic_unordered_map(const atomic_unordered_map& o, const A& a);

  /// Constructors for sets moved from another set.
  // MT: Externally Synchronized (o)
  atomic_unordered_map(atomic_unordered_map&& o)
    : atomic_unordered_map(std::move(o), o.m_alloc) {};
  atomic_unordered_map(atomic_unordered_map&& o, const A& a);

  /// Constructors for sets initialized with an initializer list.
  atomic_unordered_map(std::initializer_list<value_type> it, size_type sz,
                       const A& a)
    : atomic_unordered_map(it, sz, H(), Eq(), a) {};
  atomic_unordered_map(std::initializer_list<value_type> it, size_type sz,
                       const H& h, const A& a)
    : atomic_unordered_map(it, sz, h, Eq(), a) {};
  atomic_unordered_map(std::initializer_list<value_type> it, size_type sz = 0,
                       const H& h = H(), const Eq& eq = Eq(), const A& a = A())
    : atomic_unordered_map(it.begin(), it.end(), sz, h, eq, a) {};

  /// Destructor.
  // MT: Externally Synchronized
  ~atomic_unordered_map() {
    for(size_type i = 0; i < data_cnt; i++)
      if(data[i]) delete data[i];
    if(data != nullptr) delete[] data;
  }

  /// Copy assignment
  // MT: Externally Synchronized (this, o)
  atomic_unordered_map& operator=(const atomic_unordered_map& o);

  /// Move assignment
  // MT: Externally Synchronized (this, o)
  atomic_unordered_map& operator=(atomic_unordered_map&& o);

  /// Replacement assignment
  // MT: Externally Synchronized
  atomic_unordered_map& operator=(std::initializer_list<value_type> it);

  /// Get the allocator in use by this container.
  // MT: Externally Synchronized
  A get_allocator() const { return m_alloc; }

  class const_iterator {
  public:
    const_iterator() : max(0), idx(0), data(nullptr) {};
    ~const_iterator() = default;
    const_iterator& operator++(int) { const_iterator tmp=*this; operator++(); return tmp; }
    bool operator!=(const const_iterator& o) { return !operator==(o); }
    bool operator==(const const_iterator& o) {
      if(data == nullptr && o.data == nullptr) return true;
      return data == o.data && idx == o.idx;
    }
    value_type& operator*() { return *operator->(); }
    value_type* operator->() { return data[idx].load(std::memory_order_relaxed); }
    const_iterator& operator++() { idx++; return _update(); }
  private:
    friend class atomic_unordered_map;
    const_iterator(std::atomic<value_type*>* d, size_type c, size_type i)
      : max(c), idx(i), data(d) {};
    const_iterator& _update() {
      while(idx < max && data[idx].load(std::memory_order_relaxed) == nullptr)
        idx++;
      return *this;
    }
    size_type max;
    size_type idx;
    std::atomic<value_type*>* data;
  };

  // TODO: iterator, const_iterator, begin(), cbegin(), end(), cend()
  using iterator = value_type*;
  iterator begin();
  const_iterator begin() const { return const_iterator(data, data_cnt, 0)._update(); }
  const_iterator cbegin() const { return begin(); }
  iterator end();
  const_iterator end() const { return const_iterator(data, data_cnt, data_cnt); }
  const_iterator cend() const { return end(); }

  /// Check if the container is empty.
  // MT: Internally Synchronized, Unstable
  bool empty() const noexcept { return size() == 0; }

  /// Get the number of elements currently in the container.
  // MT: Internally Synchronized, Unstable
  size_type size() const noexcept {
    return used.load(std::memory_order_relaxed);
  }

  /// Get the (theoretical) upper bound on the size of the container.
  // MT: Safe (static/const)
  size_type max_size() const noexcept {
    return std::numeric_limits<difference_type>::max();
  }

  /// Clear the container, resulting in an empty set.
  // MT: Externally Synchronized
  void clear() noexcept;

  /// Insert a new element into the container, by copy or by move.
  // MT: Internally Synchronized
  std::pair<iterator, bool> insert(const value_type& k);
  std::pair<iterator, bool> insert(value_type&& k);

  /// Insert a new element into the container, with an (unused) hint.
  // MT: Internally Synchronized
  iterator insert(const_iterator, const value_type& k) { return insert(k).first; }
  iterator insert(const_iterator, value_type&& k) { return insert(std::move(k)).first; }

  /// Insert a sequence of elements into the container.
  // MT: Internally Synchronized
  template<class It>
  void insert(It beg, It end) { for(It it = beg; it != end; ++it) insert(*it); }
  void insert(std::initializer_list<value_type> il) { insert(il.begin(), il.end()); }

  /// Insert a new element into the container, by construction.
  // MT: Internally Synchronized
  template<class... Args>
  std::pair<iterator, bool> emplace(Args&&... args) {
    std::shared_lock<stdshim::shared_mutex> lk(lock);
    return _emplace(lk, std::forward<Args>(args)...);
    // Speculate that we will insert. Prep things to do so.
    value_type* newvp = new value_type(std::forward<Args>(args)...);
    auto mused = used.fetch_add(1, std::memory_order_relaxed);
    reserve(mused+1, lk);
    // Now do the insertion.
    std::size_t hi = m_hash(newvp->first);
    for(unsigned int i = 0; true; i++) {
      value_type* vp = _probe(hi, newvp, lk);
      if(vp == nullptr) return {newvp, true};
      if(m_equal(vp->first, newvp->first)) {
        delete newvp;
        return {vp, false};
      }
      hi = _nexthash(hi, i+1);
    }
  }

  /// Insert a new element into the container, by construction with a hint.
  // MT: Internally Synchronized
  template<class... Args>
  std::pair<iterator, bool> emplace_hint(const_iterator, Args&&... args) {
    return emplace(std::forward<Args>(args)...);
  }

  /// Insert a new element into the container, by construction, unless there is
  /// already an element with the specified key.
  // MT: Internally Synchronized
  template<class... Args>
  std::pair<iterator,bool> try_emplace(const K& k, Args&&... args) {
    std::shared_lock<stdshim::shared_mutex> lk(lock);
    auto x = _find(k, lk);
    if(x != nullptr) return {x, false};
    return _emplace(lk, std::piecewise_construct,
                    std::forward_as_tuple(k),
                    std::forward_as_tuple(std::forward<Args>(args)...));
  }
  template<class... Args>
  std::pair<iterator,bool> try_emplace(K&& k, Args&&... args) {
    std::shared_lock<stdshim::shared_mutex> lk(lock);
    auto x = _find(std::move(k), lk);
    if(x != nullptr) return {x, false};
    return _emplace(lk, std::piecewise_construct,
                    std::forward_as_tuple(std::move(k)),
                    std::forward_as_tuple(std::forward<Args>(args)...));
  }
  template<class... Args>
  iterator try_emplace(const_iterator h, const K& k, Args&&... args);
  template<class... Args>
  iterator try_emplace(const_iterator h, K&& k, Args&&... args);

  /// Remove an element from the container, given its position.
  // MT: Externally Synchronized
  iterator erase(const_iterator pos);

  /// Remove a number of elements from the container.
  // MT: Externally Synchronized
  iterator erase(const_iterator beg, const_iterator end);

  /// Remove the element matching the given one, if it exists.
  // MT: Externally Synchronized
  size_type erase(const K& k);

  /// Swap the contents of two containers.
  // MT: Externally Synchronized (this, o)
  void swap(atomic_unordered_map& o);

  /// Looks up a value in the container, throwing std::out_of_range if not found.
  // MT: Internally Synchronized, Unstable
  V& at(const K& k) {
    auto it = find(k);
    if(it == end()) throw std::out_of_range();
    return it->second;
  }
  const V& at(const K& k) const {
    auto it = find(k);
    if(it == end()) throw std::out_of_range();
    return it->second;
  }

  /// Access the element mapped from the given key, inserting it if not present.
  // MT: Internally Synchronized
  V& operator[](const K& k) {
    return try_emplace(k).first->second;
  }
  V& operator[](K&& k) {
    return try_emplace(std::move(k)).first->second;
  }

  /// Get the number of elements that are equivalent to k.
  // MT: Internally Synchronized, Unstable
  size_type count(const K& k) const;

  /// Find the element if any exist equivalent to k.
  // MT: Internally Synchronized, Unstable
  iterator find(const K& k) {
    std::shared_lock<stdshim::shared_mutex> lk(lock);
    return _find(std::move(k), lk);
  }
  iterator find_pointer(const K& k) const {
    std::shared_lock<stdshim::shared_mutex> lk(lock);
    return _find(std::move(k), lk);
  }
  const_iterator find(const K& k) const;

  /// Get the range of elements equivalent to k.
  // MT: Internally Synchronized, Unstable
  std::pair<iterator,iterator> equal_range(const K& k) {
    auto x = find(k);
    return {x, x};
  }
  std::pair<const_iterator,const_iterator> equal_range(const K& k) const {
    auto x = find(k);
    return {x, x};
  }

  /// Get the current maximum size of the container.
  // MT: Internally Synchronized, Unstable
  size_type slot_count() const;

  /// Get the current usage factor.
  // MT: Internally Synchronized, Unstable
  float usage_factor() const;

  /// Get the current maximum usage factor.
  // MT: Externally Synchronized
  float max_usage_factor() const { return max_usage; }

  /// Set the current maximum usage factor. May issue a rehash.
  // MT: Externally Synchronized
  void max_usage_factor(float m);

  /// Sets the total number of slots to the larger of sz and size(),
  /// and rehashes the contents.
  // MT: Externally Synchronized
  void rehash(size_type sz) {
    sz = std::max(sz, size());
    auto olddata = data;
    auto olddata_cnt = data_cnt;
    data = new std::atomic<value_type*>[sz];
    data_cnt = sz;
    for(size_type idx = 0; idx < data_cnt; idx++)
      data[idx].store(nullptr, std::memory_order_relaxed);
    for(size_type idx = 0; idx < olddata_cnt; idx++) {
      auto vp = olddata[idx].load(std::memory_order_relaxed);
      if(vp != nullptr) {
        auto hi = m_hash(vp->first);
        for(unsigned int i = 0; true; i++) {
          if(_probe_unsafe(hi, vp) == nullptr) break;
          hi = _nexthash(hi, i+1);
        }
      }
    }
    if(olddata != nullptr) delete[] olddata;
  }

  /// Reserve enough space for the given number of elements.
  // MT: Externally Synchronized
  void reserve(size_type cnt) {
    if(slot_count()*max_usage_factor() > cnt) return;
    rehash(std::ceil(cnt / max_usage_factor()));
  }

  /// Get the function used to hash the elements.
  // MT: Externally Synchronized
  H hash_function() const { return m_hash; }

  /// Get the function used to compare elements.
  // MT: Externally Synchronized
  Eq key_eq() const { return m_equal; }

private:
  // Probe the given hash in the data strip, returning its current value, and
  // replacing its value with the given one if not present.
  // Returns nullptr if the given node was inserted.
  value_type* _probe(std::size_t hash, value_type* v, std::shared_lock<stdshim::shared_mutex>&) {
    if(data_cnt == 0) return nullptr;
    auto& a = data[hash % data_cnt];
    if(v == nullptr) return a.load(std::memory_order_acquire);
    value_type* vp = nullptr;
    a.compare_exchange_strong(vp, v, std::memory_order_release,
                                     std::memory_order_acquire);
    return vp;
  }
  value_type* _probe_unsafe(std::size_t hash, value_type* v) {
    if(data_cnt == 0) return nullptr;
    auto& a = data[hash % data_cnt];
    auto oldvp = a.load(std::memory_order_relaxed);
    if(oldvp == nullptr) a.store(v, std::memory_order_relaxed);
    return oldvp;
  }

  // Probe the given hash in the data strip, returning its current value.
  value_type* _probe_const(std::size_t hash, std::shared_lock<stdshim::shared_mutex>&) const {
    if(data_cnt == 0) return nullptr;
    auto& a = data[hash % data_cnt];
    return a.load(std::memory_order_acquire);
  }

  // Sequence of hash values to try next.
  static std::size_t _nexthash(std::size_t last, unsigned int idx) {
    return last + 1;
  }

  // Reserve enough space for the given number of elements, but threadsafe.
  void reserve(size_type cnt, std::shared_lock<stdshim::shared_mutex>& lk) {
    if(cnt <= data_cnt*max_usage) return;
    // Time for a rehash.
    lk.unlock();
    {
      std::unique_lock<stdshim::shared_mutex> ulk(lock);
      rehash(std::ceil((cnt * 1.25) / max_usage));
    }
    lk.lock();
  }

  // Emplace, but with the lock outside.
  template<class... Args>
  std::pair<iterator, bool> _emplace(std::shared_lock<stdshim::shared_mutex>& lk, Args&&... args) {
    // Speculate that we will insert. Prep things to do so.
    value_type* newvp = new value_type(std::forward<Args>(args)...);
    auto mused = used.fetch_add(1, std::memory_order_relaxed);
    reserve(mused+1, lk);
    // Now do the insertion.
    std::size_t hi = m_hash(newvp->first);
    for(unsigned int i = 0; true; i++) {
      value_type* vp = _probe(hi, newvp, lk);
      if(vp == nullptr) return {newvp, true};
      if(m_equal(vp->first, newvp->first)) {
        delete newvp;
        return {vp, false};
      }
      hi = _nexthash(hi, i+1);
    }
  }

  // Find, but with the lock outside, and without an iterator.
  value_type* _find(const K& k, std::shared_lock<stdshim::shared_mutex>& lk) const {
    std::size_t hi = m_hash(k);
    for(unsigned int i = 0; true; i++) {
      value_type* vp = _probe_const(hi, lk);
      if(vp == nullptr) return nullptr;
      if(m_equal(k, vp->first)) return vp;
      hi = _nexthash(hi, i+1);
    }
  }

  // Data strip / slot array, and maintaining lock.
  mutable stdshim::shared_mutex lock;
  size_type data_cnt;
  std::atomic<value_type*>* data;

  // (Prospective) size of the container, used for usage factor calculations.
  std::atomic<size_type> used;

  // Tuning parameters.
  float max_usage;

  // Important storage bits, for proper standards compat
  H m_hash;
  Eq m_equal;
  A m_alloc;
};

}

#endif  // HPCTOOLKIT_PROFILE_UTIL_ATOMIC_UNORDERED_H

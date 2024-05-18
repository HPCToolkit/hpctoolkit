// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_UTIL_CACHE_H
#define HPCTOOLKIT_PROFILE_UTIL_CACHE_H

#include <array>
#include <cassert>
#include <limits>
#include <optional>

namespace hpctoolkit::util {

/// Linearly-probed statically-allocated LRU cache. For the smallest cases.
template<class Key, class Value, std::size_t N>
class linear_lru_cache {
public:
  /// Construct a default (empty) cache.
  linear_lru_cache() {
    for(auto& n: nodes) n = Node();
  }

  ~linear_lru_cache() {
    for(auto& n: nodes) n = Node();
  }

  // To make the template simple, not movable or copiable. For now.
  linear_lru_cache(linear_lru_cache&&) = delete;
  linear_lru_cache& operator=(linear_lru_cache&&) = delete;
  linear_lru_cache(const linear_lru_cache&) = delete;
  linear_lru_cache& operator=(const linear_lru_cache&) = delete;

  /// Lookup a Key in the cache. If it doesn't exist, call the given function to
  /// create the Value. Returns the associated Value.
  template<class F>
  Value lookup(Key k, const F& f) {
    std::size_t last2 = std::numeric_limits<std::size_t>::max();
    std::size_t last = std::numeric_limits<std::size_t>::max();
    for(std::size_t idx = head; idx != std::numeric_limits<std::size_t>::max();
        last2 = last, last = idx, idx = nodes[idx].next) {
      if(nodes[idx].key == k) {
        // Shift this node to the top of the stack before returning
        nodes[idx].next = head != idx ? head
            : std::numeric_limits<std::size_t>::max();  // No cycles round here
        if(last != std::numeric_limits<std::size_t>::max())
          nodes[last].next = std::numeric_limits<std::size_t>::max();
        return *nodes[idx].value;
      }
    }
    Value v = f(k);
    if(used < N) {
      // We have extra space, use some of that.
      nodes[used].key = k;
      nodes[used].value = v;
      nodes[used].next = head;
      head = used;
      used++;
    } else {
      // We're out of space. last is the LRU, it'll get evicted, its node reused
      // and moved to the top of the stack.
      nodes[last].key = k;
      nodes[last].value = v;
      nodes[last].next = head != last ? head
          : std::numeric_limits<std::size_t>::max();  // No cycles round here
      head = last;
      if(last2 != std::numeric_limits<std::size_t>::max())
        nodes[last2].next = std::numeric_limits<std::size_t>::max();
    }
    return v;
  }

private:
  // The actual data is stored as a singly-linked statically-allocated list
  struct Node {
    Node() = default;
    ~Node() = default;

    Node(Node&&) = default;
    Node& operator=(Node&&) = default;
    Node(const Node&) = default;
    Node& operator=(const Node&) = default;

    std::optional<Key> key;
    std::optional<Value> value;
    std::size_t next = std::numeric_limits<std::size_t>::max();
  };
  std::array<Node, N> nodes;
  std::size_t head = std::numeric_limits<std::size_t>::max();
  std::size_t used = 0;
};

/// Specialization for 2-sized caches
template<class Key, class Value>
class linear_lru_cache<Key, Value, 2> {
public:
  /// Construct a default (empty) cache.
  linear_lru_cache() = default;
  ~linear_lru_cache() = default;

  // To make the template simple, not movable or copiable. For now.
  linear_lru_cache(linear_lru_cache&&) = delete;
  linear_lru_cache& operator=(linear_lru_cache&&) = delete;
  linear_lru_cache(const linear_lru_cache&) = delete;
  linear_lru_cache& operator=(const linear_lru_cache&) = delete;

  /// Lookup a Key in the cache. If it doesn't exist, call the given function to
  /// create the Value. Returns the associated Value.
  template<class F>
  Value lookup(Key k, const F& f) {
    {
      auto& mru = getMRU();
      if(!mru) {
        // Empty cache, fill the first entry and return it
        mru = std::pair<Key, Value>(k, f(k));
        return mru->second;
      }
      if(mru->first == k) return mru->second;
    }
    // At this point the MRU doesn't have it, so no matter what happens it will
    // not be the MRU after this lookup. So we can swap up here.
    swapMRU();
    auto& ent = getMRU();
    if(ent && ent->first == k) return ent->second;
    // Miss. Replace the (now) MRU with the generated value and continue.
    ent = std::pair<Key, Value>(k, f(k));
    return ent->second;
  }

private:
  // The actual data is stored in these two members, which one is the MRU is
  // stored in a simple boolean.
  std::optional<std::pair<Key, Value>> entry0;
  std::optional<std::pair<Key, Value>> entry1;
  bool mru_state = false;

  // Helpers to read/manipulate the MRU state quickly
  void swapMRU() noexcept { mru_state = !mru_state; }
  auto& getMRU() noexcept { return mru_state ? entry1 : entry0; }
};

/// Specialization for 1-sized caches
template<class Key, class Value>
class linear_lru_cache<Key, Value, 1> {
public:
  /// Construct a default (empty) cache.
  linear_lru_cache() = default;
  ~linear_lru_cache() = default;

  // To make the template simple, not movable or copiable. For now.
  linear_lru_cache(linear_lru_cache&&) = delete;
  linear_lru_cache& operator=(linear_lru_cache&&) = delete;
  linear_lru_cache(const linear_lru_cache&) = delete;
  linear_lru_cache& operator=(const linear_lru_cache&) = delete;

  /// Lookup a Key in the cache. If it doesn't exist, call the given function to
  /// create the Value. Returns the associated Value.
  template<class F>
  Value lookup(Key k, const F& f) {
    if(entry && entry->first == k) return entry->second;
    entry = std::pair<Key, Value>(k, f(k));
    return entry->second;
  }

private:
  std::optional<std::pair<Key, Value>> entry;
};

}

#endif  // HPCTOOLKIT_PROFILE_UTIL_CACHE_H

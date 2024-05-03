// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_UTIL_RANGE_MAP_H
#define HPCTOOLKIT_PROFILE_UTIL_RANGE_MAP_H

#include "ref_wrappers.hpp"

#include <algorithm>
#include <deque>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include <utility>

namespace hpctoolkit::util {

/// Simple structure suitable for representing intervals. The usual ordering
/// operators for this type instead define a partial order where overlapping
/// types are incomparable. This makes it really useful for keys in a std::map.
///
/// An extra template argument allows overriding the comparison operation. This
/// comparison must implement a total order.
template<class T, class Compare = std::less<T>>
class interval {
private:
  Compare comp;

public:
  // Start and end of the interval. May be equal for a very small interval.
  // Presumes C++-style indexing, where end is one-after-end.
  T begin;
  T end;

  // Default constructor, if T is default-constructable
  interval() = default;
  ~interval() = default;

  // Can also be constructed from two values.
  constexpr interval(const T& a, const T& b)
    : begin(std::min<T>(a, b, comp)), end(std::max<T>(a, b, comp)) {}

  // Empty intervals can be constructed from one value.
  constexpr interval(const T& v) : begin(v), end(v) {}

  /// Check if this is an empty interval.
  bool empty() const {
    return !comp(begin, end) && !comp(end, begin);
  }
};

template<class T, class C>
constexpr bool operator<(const interval<T, C>& a, const interval<T, C>& b) {
  C comp;
  return !comp(b.begin, a.end) && comp(a.begin, b.begin);
}
template<class T, class C>
constexpr bool operator<=(const interval<T, C>& a, const interval<T, C>& b) {
  return a < b || a == b;
}
template<class T, class C>
constexpr bool operator>(const interval<T, C>& a, const interval<T, C>& b) {
  return b < a;
}
template<class T, class C>
constexpr bool operator>=(const interval<T, C>& a, const interval<T, C>& b) {
  return a > b || a == b;
}
template<class T, class C>
constexpr bool operator==(const interval<T, C>& a, const interval<T, C>& b) {
  C comp;
  return !comp(a.begin, b.begin) && !comp(b.begin, a.begin)
         && !comp(a.end, b.end) && !comp(b.end, a.end);
}
template<class T, class C>
constexpr bool operator!=(const interval<T, C>& a, const interval<T, C>& b) {
  return !(a == b);
}

/// Subtract an interval out of another. Two sub-intervals of `a` are returned,
/// the first is strictly before `b` and the second is strictly after `b`.
/// One or both of the intervals may be empty.
template<class T, class C>
constexpr std::pair<interval<T, C>, interval<T, C>>
operator-(const interval<T, C>& a, const interval<T, C>& b) {
  C comp;
  if(a < b) return {a, interval<T, C>(a.end, a.end)};
  if(b < a) return {interval<T, C>(a.begin, a.begin), a};
  return {
    interval<T,C>(std::min<T>(a.begin, b.begin, comp), b.begin),
    interval<T,C>(b.end, std::max<T>(a.end, b.end, comp))
  };
}

/// Debugging support for printing intervals
template<class T, class C>
std::ostream& operator<<(std::ostream& os, const interval<T, C>& v) {
  return os << '[' << v.begin << '-' << v.end << ')';
}

/// Wrapper for std::vector/deque to make it a std::map with range-based lookup.
/// Unlike a normal map, lookups that don't match exactly return a pair with the
/// next lower key. Multiple values for a key can be inserted, they are merged
/// into one based on Merge.
///
/// Internally this is represented as a std::vector or std::deque, using binary
/// search to find keys. This gives it a balanced O(log N) lookup time with
/// lower memory cost after a call to make_consistent().
///
/// MT: Safe when const, externally synchronized when non-const.
template<class K, class V, class Merge,
         class Compare = std::less<K>,
         class Container = std::deque<std::pair<K, V>>>
class range_map {
public:
  using key_type = K;
  using mapped_type = V;
  using value_type = std::pair<K, V>;

  static_assert(std::is_move_assignable_v<V>,
                "range_map only works on move-assignable types");

  // Default constructor constructs an empty container
  range_map() = default;
  ~range_map() = default;

  // Can also be constructed with a custom merge or comparison object
  explicit range_map(const Merge& merge, const Compare& comp = Compare())
    : m_merge(merge), m_compare(comp) {}

  // Movable and copiable
  range_map(range_map&&) = default;
  range_map(const range_map&) = default;
  range_map& operator=(range_map&&) = default;
  range_map& operator=(const range_map&) = default;

  /// The Container can change at will if so desired. When possible, the source
  /// will first be made consistent to reduce the overall memory cost.
  template<class OtherContainer>
  range_map(const range_map<K, V, Merge, OtherContainer>& other)
    : m_ranges(other.m_ranges.size()), m_consistent(other.m_consistent) {
    std::copy(other.m_ranges.cbegin(), other.m_ranges.cend(), m_ranges.begin());
  }
  template<class OtherContainer>
  range_map(range_map<K, V, Merge, OtherContainer>& other) {
    other.make_consistent();
    m_ranges.resize(other.m_ranges.size());
    std::copy(other.m_ranges.cbegin(), other.m_ranges.cend(), m_ranges.begin());
  }
  template<class OtherContainer>
  range_map(range_map<K, V, Merge, OtherContainer>&& other) {
    other.make_consistent();
    m_ranges.resize(other.m_ranges.size());
    std::move(other.m_ranges.begin(), other.m_ranges.end(), m_ranges.begin());
  }

  /// Lookup the given value in the map. Throws if no range contains the value.
  V& at(const K& key) {
    iterator it = find(key);
    if(it == end()) throw std::out_of_range("at() called with out-of-range key");
    return it->second;
  }
  const V& at(const K& key) const {
    const_iterator it = find(key);
    if(it == end()) throw std::out_of_range("at() called with out-of-range key");
    return it->second;
  }

  using iterator = typename Container::iterator;
  using const_iterator = typename Container::const_iterator;

  /// Iterate over the elements in this range_map. These are sorted by the keys.
  /// Note that const access requires that the range_map is already
  /// consistent, possibly from a previous call to consistent().
  iterator begin() {
    make_consistent();
    return m_ranges.begin();
  }
  const_iterator begin() const { return cbegin(); }
  const_iterator cbegin() const {
    // if(!m_consistent)
    //   throw std::logic_error("Attempt to read from a const inconsistent range_map!");
    return m_ranges.cbegin();
  }

  iterator end() {
    make_consistent();
    return m_ranges.end();
  }
  const_iterator end() const { return cend(); }
  const_iterator cend() const {
    // if(!m_consistent)
    //   throw std::logic_error("Attempt to read from a const inconsistent range_map!");
    return m_ranges.cend();
  }

  iterator rbegin() {
    make_consistent();
    return m_ranges.rbegin();
  }
  const_iterator rbegin() const { return crbegin(); }
  const_iterator crbegin() const {
    if(!m_consistent)
      throw std::logic_error("Attempt to read from a const inconsistent range_map!");
    return m_ranges.crbegin();
  }

  iterator rend() {
    make_consistent();
    return m_ranges.rend();
  }
  const_iterator rend() const { return crend(); }
  const_iterator crend() const {
    if(!m_consistent)
      throw std::logic_error("Attempt to read from a const inconsistent range_map!");
    return m_ranges.crend();
  }

  /// Check if this range_map is empty.
  bool empty() const noexcept { return m_ranges.empty(); }

  using size_type = typename Container::size_type;

  /// Check how large this range_map could theoretically grow.
  size_type max_size() const noexcept { return m_ranges.max_size(); }

  /// Clear this range_map, along with all its contained data.
  void clear() noexcept {
    m_ranges.clear();
    m_consistent = true;
  }

  /// Insert a new range into the range_map. The range will start at the key
  /// in the pair and end at the sequentially next range element.
  /// If the range matches one already in the map, the values will be merged
  /// using Merge::operator() (unlike std::map which ignores the new value).
  void insert(const value_type& value) {
    if(m_consistent && !m_ranges.empty()) {
      if(m_compare(value.first, m_ranges.back().first)) {
        m_consistent = false;  // New out-of-order element
      } else if(!m_compare(m_ranges.back().first, value.first)) {
        // Incomparable keys, so we call it equal and merge it instead.
        m_merge(m_ranges.back().second, value.second);
        return;
      }
    }
    m_ranges.push_back(value);
  }
  void insert(value_type&& value) {
    if(m_consistent && !m_ranges.empty()) {
      if(m_compare(value.first, m_ranges.back().first)) {
        m_consistent = false;  // New out-of-order element
      } else if(!m_compare(m_ranges.back().first, value.first)) {
        // Incomparable keys, so we call it equal and merge it instead.
        m_merge(m_ranges.back().second, std::move(value.second));
        return;
      }
    }
    m_ranges.emplace_back(std::move(value));
  }
  template<class P>
  std::enable_if_t<std::is_constructible_v<value_type, P&&>,
  void> insert(P&& value) { return emplace(std::forward<P>(value)); }

  /// Insert a new range into the range_map, constructing in-place.
  /// See insert for other details.
  template<class... Args>
  void emplace(Args&&... args) {
    if(m_ranges.empty() || !m_consistent) {
      m_ranges.emplace_back(std::forward<Args>(args)...);
    } else {
      value_type& last = m_ranges.back();
      m_ranges.emplace_back(std::forward<Args>(args)...);
      if(m_compare(m_ranges.back().first, last.first)) {
        // New out-of-order element
        m_consistent = false;
      } else if(!m_compare(last.first, m_ranges.back().first)) {
        // Incomparable keys, so we call it equal and merge it.
        m_merge(last.second, std::move(m_ranges.back().second));
        m_ranges.pop_back();
      }
    }
  }

  /// Insert a new range into the range_map, constructing in-place. Identical to
  /// emplace but is slightly more efficient.
  /// See insert for other details.
  template<class... Args>
  void try_emplace(const K& key, Args&&... args) {
    if(m_consistent && !m_ranges.empty()) {
      if(m_compare(key, m_ranges.back().first)) {
        m_consistent = false;  // New out-of-order element
      } else if(!m_compare(m_ranges.back().first, key)) {
        // Incomparable keys, so we call it equal and merge it instead.
        m_merge(m_ranges.back().second, V(std::forward<Args>(args)...));
        return;
      }
    }
    m_ranges.emplace_back(std::piecewise_construct, std::forward_as_tuple(key),
                          std::forward_as_tuple(std::forward<Args>(args)...));
  }
  template<class... Args>
  void try_emplace(K&& key, Args&&... args) {
    if(m_consistent && !m_ranges.empty()) {
      if(m_compare(key, m_ranges.back().first)) {
        m_consistent = false;  // New out-of-order element
      } else if(!m_compare(m_ranges.back().first, key)) {
        // Incomparable keys, so we call it equal and merge it instead.
        m_merge(m_ranges.back().second, V(std::forward<Args>(args)...));
        return;
      }
    }
    m_ranges.emplace_back(std::piecewise_construct, std::forward_as_tuple(std::move(key)),
                          std::forward_as_tuple(std::forward<Args>(args)...));
  }

  /// Swap the contents of this range_map with another. For funsies.
  void swap(range_map& other) {
    using namespace std;
    swap(m_ranges, other.m_ranges);
    std::swap(m_consistent, other.m_consistent);
  }

  /// Get the range containing a particular key. Returns end() if none contain it.
  /// Requires the range_map to be consistent, calls make_consistent if not const.
  iterator find(const K& key) {
    iterator it = lower_bound(key);
    if(it != end() && !m_compare(it->first, key) && !m_compare(key, it->first))
      return it;
    if(it == begin()) return end();
    return std::prev(it);
  }
  const_iterator find(const K& key) const {
    const_iterator it = lower_bound(key);
    if(it != end() && !m_compare(it->first, key) && !m_compare(key, it->first))
      return it;
    if(it == begin()) return end();
    return std::prev(it);
  }

  /// Check whether any range contains the given key.
  /// Requires the range_map to be consistent, calls make_consistent if not const.
  bool contains(const K& key) {
    make_consistent();
    return !m_compare(key, m_ranges.front().first);
  }
  bool contains(const K& key) const {
    if(!m_consistent)
      throw std::logic_error("Attempt to read from a const inconsistent range_map!");
    return !m_compare(key, m_ranges.front().first);
  }

  /// Get the range of elements surrounding the given key.
  /// Equivalent to lower_bound and upper_bound.
  /// Requires the range_map to be consistent, calls make_consistent if not const.
  std::pair<iterator, iterator> equal_range(const K& key) {
    return {lower_bound(key), upper_bound(key)};
  }
  std::pair<const_iterator, const_iterator> equal_range(const K& key) const {
    return {lower_bound(key), upper_bound(key)};
  }

  /// Get the first element not less than (>=) the given key.
  /// Requires the range_map to be consistent, calls make_consistent if not const.
  iterator lower_bound(const K& key) {
    return std::lower_bound(begin(), end(), key,
        [&](const value_type& a, const K& b){ return m_compare(a.first, b); });
  }
  const_iterator lower_bound(const K& key) const {
    return std::lower_bound(cbegin(), cend(), key,
        [&](const value_type& a, const K& b){ return m_compare(a.first, b); });
  }

  /// Get the first element greater than (>) the given key.
  /// Requires the range_map to be consistent, calls make_consistent if not const.
  iterator upper_bound(const K& key) {
    return std::upper_bound(begin(), end(), key,
        [&](const value_type& a, const K& b){ return m_compare(a.first, b); });
  }
  const_iterator upper_bound(const K& key) const {
    return std::upper_bound(cbegin(), cend(), key,
        [&](const value_type& a, const K& b){ return m_compare(a.first, b); });
  }

  /// Make this range_map consistent. This may merge ranges with the same key,
  /// or otherwise shrink the storage required.
  ///
  /// This involves sorts and other things, so call sparingly.
  void make_consistent() {
    if(m_consistent) return;
    std::sort(m_ranges.begin(), m_ranges.end(),
        [&](const value_type& a, const value_type& b){
          return m_compare(a.first, b.first);
        });
    // The rest of this is like std::unique, but merges instead of overwriting.
    typename Container::iterator cur = m_ranges.begin();
    typename Container::iterator last = m_ranges.end();
    typename Container::iterator result = cur;
    while((cur = std::next(cur)) != last) {
      if(!m_compare(result->first, cur->first)) {
        // The keys must be equal, since result < cur, result.key >= cur.key
        // and we're sorted so keys should be in order. Merge.
        m_merge(result->second, std::move(cur->second));
      } else if((result = std::next(result)) != cur) {
        // New range to output, and the new result < cur. Move.
        *result = std::move(*cur);
      }
    }
    // Erase everything after the last range.
    m_ranges.erase(std::next(result), m_ranges.end());
    m_consistent = true;
  }

private:
  Merge m_merge;
  Compare m_compare;
  // All the ranges are stashed in here until frozen_range_map construction.
  Container m_ranges;
  // If true, m_ranges is properly sorted and compressed.
  bool m_consistent = true;
};

namespace range_merge {

/// Simple merger for range_map, where the final value for a range is always
/// the minimum of the available values.
template<class T = void, class Compare = std::less<T>>
class min {
protected:
  Compare comp;
public:
  void operator()(T& out, const T& in) const {
    if(comp(in, const_cast<const T&>(out))) out = in;
  }
  void operator()(T& out, T&& in) const {
    if(comp(const_cast<const T&>(in), const_cast<const T&>(out)))
      out = std::move(in);
  }
};
template<class Compare>
class min<void, Compare> {
protected:
  Compare comp;
public:
  template<class T, class U>
  void operator()(T& out, U&& in) const {
    if(comp(const_cast<const T&>(in), const_cast<const U&>(out)))
      out = std::forward<U>(in);
  }
};

/// Simple merger for range_map, which just throws an error when attempting to
/// merge if the values are not already equal.
template<class T = void, class Compare = std::equal_to<T>>
class throw_unequal {
protected:
  Compare comp;
public:
  void operator()(const T& a, const T& b) const {
    if(!comp(a, b))
      throw std::logic_error("Conflicting values for entry in range_map!");
  }
};
template<class Compare>
class throw_unequal<void, Compare> {
protected:
  Compare comp;
public:
  template<class T>
  void operator()(const T& a, const T& b) const {
    if(!comp(a, b))
      throw std::logic_error("Conflicting values for entry in range_map!");
  }
};

/// Simple merger for range_map, that always throws. Useful for when there
/// should never be overlapping ranges.
template<class T = void>
struct always_throw {
  void operator()(const T&, const T&) const {
    throw std::logic_error("Conflicting ranges in range_map!");
  }
};
template<>
struct always_throw<void> {
  template<class T>
  void operator()(const T&, const T&) const {
    throw std::logic_error("Conflicting ranges in range_map!");
  }
};

/// Simple merger for range_map, where the final value for a range is the
/// "truthy" value of the available values. Useful for optionals.
template<class T = void, class FallbackMerge = throw_unequal<T>>
class truthy {
protected:
  FallbackMerge fallbackMerge;
public:
  void operator()(T& out, const T& in) const {
    if(in) {
      if(const_cast<const T&>(out)) {
        fallbackMerge(out, in);
      } else out = in;
    }
  }
  void operator()(T& out, T&& in) const {
    if(const_cast<const T&>(in)) {
      if(const_cast<const T&>(out)) {
        fallbackMerge(out, std::move(in));
      } else out = std::move(in);
    }
  }
};
template<class FallbackMerge>
class truthy<void, FallbackMerge> {
protected:
  FallbackMerge fallbackMerge;
public:
  template<class T, class U>
  void operator()(T& out, U&& in) const {
    if(const_cast<const U&>(in)) {
      if(const_cast<const T&>(out)) {
        fallbackMerge(out, std::forward<U>(in));
      } else out = std::forward<U>(in);
    }
  }
};

}  // namespace merge

}  // namespace hpctoolkit

#endif  // HPCTOOLKIT_PROFILE_UTIL_RANGE_MAP_H

// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_UTIL_STREAMING_SORT_H
#define HPCTOOLKIT_PROFILE_UTIL_STREAMING_SORT_H

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <vector>

namespace hpctoolkit::util {

namespace detail {
template<class X, class Y, class Cmp>
struct compare_only_first_pair_impl : public Cmp {
  bool operator()(const std::pair<X, Y>& a, const std::pair<X, Y>& b) const {
    return Cmp::operator()(a.first, b.first);
  }
};
template<class X, class Y, class Cmp>
struct compare_only_second_pair_impl : public Cmp {
  bool operator()(const std::pair<X, Y>& a, const std::pair<X, Y>& b) const {
    return Cmp::operator()(a.second, b.second);
  }
};
}

/// Helper comparison for pairs, compares by the first element.
template<class, class = void> struct compare_only_first {};
template<class X, class Y>
struct compare_only_first<std::pair<X, Y>, void>
  : public detail::compare_only_first_pair_impl<X, Y, std::less<X>> {};
template<class X, class Y, class Cmp>
struct compare_only_first<std::pair<X, Y>, Cmp>
  : public detail::compare_only_first_pair_impl<X, Y, Cmp> {};

/// Helper comparison for pairs, compares by the second element.
template<class, class = void> struct compare_only_second {};
template<class X, class Y>
struct compare_only_second<std::pair<X, Y>, void>
  : public detail::compare_only_second_pair_impl<X, Y, std::less<Y>> {};
template<class X, class Y, class Cmp>
struct compare_only_second<std::pair<X, Y>, Cmp>
  : public detail::compare_only_second_pair_impl<X, Y, Cmp> {};

/// Container specialized for streaming sorting algorithms.
///
/// A number of algorithms require memory-efficient K-disordered sorting, where
/// it is known that every element is at most K places from its proper location
/// in the sorted order. For memory-efficiency only K elements need to be
/// stored, the remainder can be "streamed" through the container. In practice,
/// this is all done with a consistently maintained min-heap.
template<class T, class Cmp = std::less<T>, class Alloc = std::allocator<T>>
class streaming_sort_buffer {
  // The STL heap algorithms make a max-heap. Since we want a min-heap, this
  // reverses the arguments to get what we actually want.
  struct RevCmp : public Cmp {
    bool operator()(const T& a, const T& b) {
      return Cmp::operator()(b, a);
    }
  } m_rcmp;
  // Storage for the maintained min-heap.
  std::vector<T, Alloc> m_heap;

public:
  /// Construct a default (empty) buffer.
  streaming_sort_buffer() = default;
  /// Construct from an initial vector of elements.
  template<class A>
  explicit streaming_sort_buffer(std::vector<T, A> data)
    : m_heap(std::move(data)) {
    std::make_heap(m_heap.begin(), m_heap.end(), m_rcmp);
  }

  ~streaming_sort_buffer() = default;

  streaming_sort_buffer(streaming_sort_buffer&&) = default;
  streaming_sort_buffer& operator=(streaming_sort_buffer&& o) {
    streaming_sort_buffer::swap(o);
    return *this;
  }
  streaming_sort_buffer(const streaming_sort_buffer&) = default;
  streaming_sort_buffer& operator=(const streaming_sort_buffer&) = default;

  // Most of our container implementation comes from the std::vector
  auto get_allocator() const noexcept { return m_heap.get_allocator(); }
  auto empty() const noexcept { return m_heap.empty(); }
  auto size() const noexcept { return m_heap.size(); }
  auto max_size() const noexcept { return m_heap.max_size(); }
  void reserve(size_t cap) { return m_heap.reserve(cap); }
  auto capacity() const noexcept { return m_heap.capacity(); }
  void shrink_to_fit() { return m_heap.shrink_to_fit(); }
  void clear() { return m_heap.clear(); }

  // Iteration is a bit special, we only support constant iteration.
  auto begin() const noexcept { return m_heap.cbegin(); }
  auto cbegin() const noexcept { return m_heap.cbegin(); }
  auto end() const noexcept { return m_heap.cend(); }
  auto cend() const noexcept { return m_heap.cend(); }

  /// Swap the contents of this buffer with another.
  void swap(streaming_sort_buffer& o) {
    m_heap.swap(o.m_heap);
    using std::swap;
    swap(m_rcmp, o.m_rcmp);
  }

  /// Add a new element to the buffer, increasing the size. Returns true if the
  /// new element is the smallest of all the elements currently in the buffer.
  bool push(const T& value) {
    return emplace(T(value));
  }
  bool push(T&& value) {
    return emplace(std::move(value));
  }

  /// Add a new element to the buffer, increasing the size. Returns true if the
  /// new element is the smallest of all the elements currently in the buffer.
  template<class... Args>
  bool emplace(Args&&... args) {
    m_heap.emplace_back(std::forward<Args>(args)...);
    return push_heap_last();
  }

  /// Remove the smallest element from the buffer, decreasing the size.
  /// The buffer must not be empty before calling this.
  T pop() {
    std::pop_heap(m_heap.begin(), m_heap.end(), m_rcmp);
    T res = std::move(m_heap.back());
    m_heap.pop_back();
    return res;
  }

  /// Pop the smallest element and push a new element, retaining current size.
  /// Equiv. to `std::make_pair(pop(), push(value))`.
  std::pair<T, bool> replace(const T& value) {
    return em_replace(T(value));
  }
  std::pair<T, bool> replace(T&& value) {
    return em_replace(std::move(value));
  }

  /// Pop the smallest element and push a new element, retaining current size.
  /// Equiv. to `std::make_pair(pop(), push(args...))`.
  template<class... Args>
  std::pair<T, bool> em_replace(Args&&... args) {
    std::pop_heap(m_heap.begin(), m_heap.end(), m_rcmp);
    T res = std::move(m_heap.back());
    m_heap.back() = T(std::forward<Args>(args)...);
    return {res, push_heap_last()};
  }

  /// Extract all remaining elements in the buffer in sorted order.
  /// Equiv. to repeated pop().
  std::vector<T, Alloc> sorted() && {
    std::sort_heap(m_heap.begin(), m_heap.end(), m_rcmp);
    // The previous std::sort_heap call leaves the heap in descending order.
    // Reverse to recover the correct order.
    std::reverse(m_heap.begin(), m_heap.end());
    return std::move(m_heap);
  }

private:
  // Equiv. to std::push_heap(m_heap.begin(), m_heap.end(), m_rcmp), but returns
  // true if the root element (0) was swapped in the process, which happens iff
  // the newly-added element is now the smallest in the heap.
  bool push_heap_last() {
    size_t idx = m_heap.size() - 1;
    while(idx > 0) {
      size_t par = (idx - 1) / 2;
      if(m_rcmp(m_heap[idx], m_heap[par])) {
        // idx is bigger than its parent, so we're done!
        // idx > 0 so we didn't have to swap the root.
        return false;
      }
      // idx is <= its parent, so swap and bubble upwards
      using std::swap;
      swap(m_heap[idx], m_heap[par]);
      idx = par;
    }
    // idx == 0, we must have had to swap the root.
    return true;
  }
};

/// Variant of streaming_sort_buffer that has an upper bound on its size.
/// This bound is set dynamically on construction.
template<class T, class Cmp = std::less<T>, class Alloc = std::allocator<T>>
class bounded_streaming_sort_buffer
  : public streaming_sort_buffer<T, Cmp, Alloc> {
  using Base = streaming_sort_buffer<T, Cmp, Alloc>;

  // Hard upper bound on the size of this buffer. Never changes.
  size_t m_bound;
public:
  /// Default construct an empty buffer with 0 bound.
  bounded_streaming_sort_buffer() : Base(), m_bound(0) {};
  /// Construct an empty buffer of the given bound
  explicit bounded_streaming_sort_buffer(size_t bound)
    : Base(), m_bound(bound) {
    Base::reserve(m_bound);
  }
  /// Construct from the given vector, the bound will be the initial size.
  template<class A>
  explicit bounded_streaming_sort_buffer(std::vector<T, A> data)
    : Base(std::move(data)), m_bound(Base::size()) {};
  /// Construct from the given vector and explicit bound.
  template<class A>
  bounded_streaming_sort_buffer(size_t bound, std::vector<T, A> data)
    : Base(std::move(data)), m_bound(bound) {
    if(Base::size() > m_bound) {
      throw std::logic_error("Attempt to over-fill a "
          "bounded_streaming_sort_buffer on construction!");
    }
    Base::reserve(m_bound);
  }

  ~bounded_streaming_sort_buffer() = default;

  bounded_streaming_sort_buffer(bounded_streaming_sort_buffer&&) = default;
  bounded_streaming_sort_buffer& operator=(bounded_streaming_sort_buffer&&) = default;
  bounded_streaming_sort_buffer(const bounded_streaming_sort_buffer&) = default;
  bounded_streaming_sort_buffer& operator=(const bounded_streaming_sort_buffer&) = default;

  /// Check if this buffer is full (has hit the hard bound).
  /// If true, push and emplace will throw.
  bool full() const noexcept { return Base::size() >= m_bound; }

  /// Get the upper bound for the size of this buffer.
  size_t bound() const noexcept { return m_bound; }

  /// Add a new element to the buffer. Throws if the buffer is already full.
  bool push(const T& value) {
    if(Base::size() >= m_bound)
      throw std::logic_error("Cannot push into a full bounded_streaming_sort_buffer!");
    return Base::push(value);
  }
  bool push(T&& value) {
    if(Base::size() >= m_bound)
      throw std::logic_error("Cannot push into a full bounded_streaming_sort_buffer!");
    return Base::push(std::move(value));
  }

  /// Add a new element to the buffer. Throws if the buffer is already full.
  template<class... Args>
  bool emplace(Args&&... args) {
    if(Base::size() >= m_bound)
      throw std::logic_error("Cannot emplace into a full bounded_streaming_sort_buffer!");
    return Base::emplace(std::forward<Args>(args)...);
  }
};

}

#endif  // HPCTOOLKIT_PROFILE_UTIL_STREAMING_SORT_H

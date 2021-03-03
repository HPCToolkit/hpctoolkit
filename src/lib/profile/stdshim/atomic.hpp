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
// Copyright ((c)) 2020, Rice University
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

#ifndef HPCTOOLKIT_STDSHIM_ATOMIC_H
#define HPCTOOLKIT_STDSHIM_ATOMIC_H

// This file is one of multiple "stdshim" headers, which act as a seamless
// transition library across versions of the STL. Mostly all this does is
// backport features from C++20 into C++11, sometimes by using class inheritence
// tricks, and sometimes by importing implementations from Boost or ourselves.
// Also see Google's Abseil project.

// This is the shim for <atomic>

#include "version.hpp"

#include <atomic>

#if !STD_HAS(atomic_wait)
#include "futex-detail.h"

#include <cstdint>
#include <limits>
#include <type_traits>
#endif

namespace hpctoolkit::stdshim {

#if STD_HAS(atomic_wait)
// If we have C++20, atomics are just atomics.
template<class T>
using atomic = std::atomic<T>;

#else  // !STD_HAS(atomic_wait)

namespace detail {
// In C++20 atomics will have futex-like operations, but its not clear exactly
// how they'll be implemented in the back. Since Linux futexes are limited to
// 32-bit objects, we munge everything through a uint32_t version.
class atomic_uint32 : public std::atomic<std::uint32_t> {
public:
  atomic_uint32() noexcept = default;
  constexpr atomic_uint32(std::uint32_t v) : std::atomic<std::uint32_t>(v) {};
  atomic_uint32(const atomic_uint32&) = delete;

  /// Compare the current value with that given and atomically block.
  /// If futexes are not available, acts as std::this_thread_yield
  /// (i.e. instantanious spurious wakeup).
  void wait(std::uint32_t, std::memory_order = std::memory_order_seq_cst) const noexcept;
  void wait(std::uint32_t, std::memory_order = std::memory_order_seq_cst) const volatile noexcept;

  /// Wake up at least one thread currently blocked on this Futex.
  /// If futexes are not available, no-ops.
  void notify_one() noexcept;
  void notify_one() volatile noexcept;

  /// Wake up all threads currently blocked on this Futex.
  /// If futexes are not available, no-ops.
  void notify_all() noexcept;
  void notify_all() volatile noexcept;

private:
  std::uint32_t* ptr() noexcept;
  const std::uint32_t* ptr() const noexcept;
  volatile std::uint32_t* ptr() volatile noexcept;
  const volatile std::uint32_t* ptr() const volatile noexcept;
};

// This detail provides all the common (non-Integral) bits for an atomic template.
template<class T>
class atomic_nonint : private atomic_uint32 {
public:
  atomic_nonint() noexcept = default;
  constexpr atomic_nonint(T v) noexcept : atomic_uint32(static_cast<std::uint32_t>(v)) {};
  atomic_nonint(const atomic_nonint&) = delete;

  T operator=(T v) noexcept { store(v); return v; }
  T operator=(T v) volatile noexcept { store(v); return v; }
  atomic_nonint& operator=(const atomic_nonint&) = delete;
  atomic_nonint& operator=(const atomic_nonint&) volatile = delete;

  using atomic_uint32::is_lock_free;

  void store(T v, std::memory_order o = std::memory_order_seq_cst) noexcept {
    atomic_uint32::store(static_cast<std::uint32_t>(v), o);
  }
  void store(T v, std::memory_order o = std::memory_order_seq_cst) volatile noexcept {
    atomic_uint32::store(static_cast<std::uint32_t>(v), o);
  }

  T load(std::memory_order o = std::memory_order_seq_cst) const noexcept {
    return static_cast<T>(atomic_uint32::load(o));
  }
  T load(std::memory_order o = std::memory_order_seq_cst) const volatile noexcept {
    return static_cast<T>(atomic_uint32::load(o));
  }

  operator T() const noexcept { return load(); }
  operator T() const volatile noexcept { return load(); }

  T exchange(T v, std::memory_order o = std::memory_order_seq_cst) noexcept {
    return static_cast<T>(atomic_uint32::exchange(static_cast<std::uint32_t>(v), o));
  }
  T exchange(T v, std::memory_order o = std::memory_order_seq_cst) volatile noexcept {
    return static_cast<T>(atomic_uint32::exchange(static_cast<std::uint32_t>(v), o));
  }

  bool compare_exchange_weak(T& e, T d, std::memory_order s, std::memory_order f) noexcept {
    std::uint32_t ex = static_cast<std::uint32_t>(e);
    bool res = atomic_uint32::compare_exchange_weak(ex, static_cast<std::uint32_t>(d), s, f);
    e = static_cast<T>(ex);
    return res;
  }
  bool compare_exchange_weak(T& e, T d, std::memory_order s, std::memory_order f) volatile noexcept {
    std::uint32_t ex = static_cast<std::uint32_t>(e);
    bool res = atomic_uint32::compare_exchange_weak(ex, static_cast<std::uint32_t>(d), s, f);
    e = static_cast<T>(ex);
    return res;
  }

  bool compare_exchange_weak(T& e, T d, std::memory_order o = std::memory_order_seq_cst) noexcept {
    std::uint32_t ex = static_cast<std::uint32_t>(e);
    bool res = atomic_uint32::compare_exchange_weak(ex, static_cast<std::uint32_t>(d), o);
    e = static_cast<T>(ex);
    return res;
  }
  bool compare_exchange_weak(T& e, T d, std::memory_order o = std::memory_order_seq_cst) volatile noexcept {
    std::uint32_t ex = static_cast<std::uint32_t>(e);
    bool res = atomic_uint32::compare_exchange_weak(ex, static_cast<std::uint32_t>(d), o);
    e = static_cast<T>(ex);
    return res;
  }

  bool compare_exchange_strong(T& e, T d, std::memory_order s, std::memory_order f) noexcept {
    std::uint32_t ex = static_cast<std::uint32_t>(e);
    bool res = atomic_uint32::compare_exchange_weak(ex, static_cast<std::uint32_t>(d), s, f);
    e = static_cast<T>(ex);
    return res;
  }
  bool compare_exchange_strong(T& e, T d, std::memory_order s, std::memory_order f) volatile noexcept {
    std::uint32_t ex = static_cast<std::uint32_t>(e);
    bool res = atomic_uint32::compare_exchange_weak(ex, static_cast<std::uint32_t>(d), s, f);
    e = static_cast<T>(ex);
    return res;
  }

  bool compare_exchange_strong(T& e, T d, std::memory_order o = std::memory_order_seq_cst) noexcept {
    std::uint32_t ex = static_cast<std::uint32_t>(e);
    bool res = atomic_uint32::compare_exchange_weak(ex, static_cast<std::uint32_t>(d), o);
    e = static_cast<T>(ex);
    return res;
  }
  bool compare_exchange_strong(T& e, T d, std::memory_order o = std::memory_order_seq_cst) volatile noexcept {
    std::uint32_t ex = static_cast<std::uint32_t>(e);
    bool res = atomic_uint32::compare_exchange_weak(ex, static_cast<std::uint32_t>(d), o);
    e = static_cast<T>(ex);
    return res;
  }

  void wait(T v, std::memory_order o = std::memory_order_seq_cst) const noexcept {
    atomic_uint32::wait(static_cast<std::uint32_t>(v), o);
  }
  void wait(T v, std::memory_order o = std::memory_order_seq_cst) const volatile noexcept {
    atomic_uint32::wait(static_cast<std::uint32_t>(v), o);
  }

  using atomic_uint32::notify_one;
  using atomic_uint32::notify_all;
};

// This detail appends the special Integral functions
template<class T>
class atomic_int : public atomic_nonint<T> {
public:
  atomic_int() noexcept = default;
  constexpr atomic_int(T v) noexcept : atomic_nonint<T>(v) {};
  atomic_int(const atomic_int&) = delete;

  T fetch_add(T v, std::memory_order o = std::memory_order_seq_cst) noexcept {
    return static_cast<T>(atomic_uint32::fetch_add(static_cast<std::uint32_t>(v), o));
  }
  T fetch_add(T v, std::memory_order o = std::memory_order_seq_cst) volatile noexcept {
    return static_cast<T>(atomic_uint32::fetch_add(static_cast<std::uint32_t>(v), o));
  }

  T fetch_sub(T v, std::memory_order o = std::memory_order_seq_cst) noexcept {
    return static_cast<T>(atomic_uint32::fetch_sub(static_cast<std::uint32_t>(v), o));
  }
  T fetch_sub(T v, std::memory_order o = std::memory_order_seq_cst) volatile noexcept {
    return static_cast<T>(atomic_uint32::fetch_sub(static_cast<std::uint32_t>(v), o));
  }

  T fetch_and(T v, std::memory_order o = std::memory_order_seq_cst) noexcept {
    return static_cast<T>(atomic_uint32::fetch_and(static_cast<std::uint32_t>(v), o));
  }
  T fetch_and(T v, std::memory_order o = std::memory_order_seq_cst) volatile noexcept {
    return static_cast<T>(atomic_uint32::fetch_and(static_cast<std::uint32_t>(v), o));
  }

  T fetch_or(T v, std::memory_order o = std::memory_order_seq_cst) noexcept {
    return static_cast<T>(atomic_uint32::fetch_or(static_cast<std::uint32_t>(v), o));
  }
  T fetch_or(T v, std::memory_order o = std::memory_order_seq_cst) volatile noexcept {
    return static_cast<T>(atomic_uint32::fetch_or(static_cast<std::uint32_t>(v), o));
  }

  T fetch_xor(T v, std::memory_order o = std::memory_order_seq_cst) noexcept {
    return static_cast<T>(atomic_uint32::fetch_xor(static_cast<std::uint32_t>(v), o));
  }
  T fetch_xor(T v, std::memory_order o = std::memory_order_seq_cst) volatile noexcept {
    return static_cast<T>(atomic_uint32::fetch_xor(static_cast<std::uint32_t>(v), o));
  }

  T operator++() noexcept { return fetch_add(1) + 1; }
  T operator++() volatile noexcept { return fetch_add(1) + 1; }
  T operator++(int) noexcept { return fetch_add(1); }
  T operator++(int) volatile noexcept { return fetch_add(1); }
  T operator--() noexcept { return fetch_sub(1) - 1; }
  T operator--() volatile noexcept { return fetch_sub(1) - 1; }
  T operator--(int) noexcept { return fetch_sub(1); }
  T operator--(int) volatile noexcept { return fetch_sub(1); }

  T operator+=(T v) noexcept { return fetch_add(v) + v; }
  T operator+=(T v) volatile noexcept { return fetch_add(v) + v; }
  T operator-=(T v) noexcept { return fetch_sub(v) - v; }
  T operator-=(T v) volatile noexcept { return fetch_sub(v) - v; }
  T operator&=(T v) noexcept { return fetch_and(v) & v; }
  T operator&=(T v) volatile noexcept { return fetch_and(v) & v; }
  T operator|=(T v) noexcept { return fetch_or(v) | v; }
  T operator|=(T v) volatile noexcept { return fetch_or(v) | v; }
  T operator^=(T v) noexcept { return fetch_xor(v) ^ v; }
  T operator^=(T v) volatile noexcept { return fetch_xor(v) ^ v; }
};

// For integral types of sufficiently small sizes we allow casting to an
// underlying uint32_t.
template<class T, bool> struct is_futex_int_helper : std::false_type {};
template<class T>
struct is_futex_int_helper<T, true> : std::integral_constant<bool,
  std::numeric_limits<T>::radix == 2
  && (std::numeric_limits<T>::digits + std::is_signed<T>::value ? 1 : 0) <= 32>
  {};
template<class T>
using is_futex_int = is_futex_int_helper<T, std::is_integral<T>::value>;

// Enumeration types are also allowed if their underlying type is small.
template<class T, bool> struct is_futex_enum_helper : std::false_type {};
template<class T>
struct is_futex_enum_helper<T, true>
  : is_futex_int<typename std::underlying_type<T>::type> {};
template<class T>
using is_futex_enum = is_futex_enum_helper<T, std::is_enum<T>::value>;

}  // namespace detail

// Stitch together all the details. If we don't have a wrapper, use a normal atomic.
template<class T>
using atomic = typename std::conditional<std::is_same<T, std::uint32_t>::value,
  detail::atomic_uint32, typename std::conditional<detail::is_futex_int<T>::value,
  detail::atomic_int<T>, typename std::conditional<detail::is_futex_enum<T>::value,
  detail::atomic_nonint<T>,
  std::atomic<T>
  >::type>::type>::type;

#endif  // STD_HAS(atomic_wait)

}  // namespace hpctoolkit::stdshim

#ifndef STDSHIM_DONT_UNDEF
#undef STD_HAS
#endif

#endif  // HPCTOOLKIT_STDSHIM_ATOMIC_H

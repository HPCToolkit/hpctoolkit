// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#include "atomic.hpp"

#include <thread>

using namespace hpctoolkit::stdshim;
using namespace detail;

// Check whether atomic<uint32_t> has a uint32_t as its first member. If it
// doesn't we can't really use futexes.
static struct AtomicCheck {
  AtomicCheck() {
    atomic<std::uint32_t> a(0x1B880836);
    auto& r = reinterpret_cast<uint32_t&>(a);
    uint32_t v = r;
    result = (v == 0x1B880836) && (alignof(atomic<std::uint32_t>) >= 4);
  };

  bool result;
  operator bool() { return result; }
} isConvertable;

#ifdef HAVE_FUTEX_H

void atomic_uint32::wait(std::uint32_t old, std::memory_order) const noexcept {
  if(isConvertable) hpctoolkit_futex_wait(ptr(), old);
  else std::this_thread::yield();
}
void atomic_uint32::wait(std::uint32_t old, std::memory_order) const volatile noexcept {
  if(isConvertable) hpctoolkit_futex_wait_v(ptr(), old);
  else std::this_thread::yield();
}

void atomic_uint32::notify_one() noexcept {
  if(isConvertable) hpctoolkit_futex_notify_one(ptr());
}
void atomic_uint32::notify_one() volatile noexcept {
  if(isConvertable) hpctoolkit_futex_notify_one_v(ptr());
}

void atomic_uint32::notify_all() noexcept {
  if(isConvertable) hpctoolkit_futex_notify_all(ptr());
}
void atomic_uint32::notify_all() volatile noexcept {
  if(isConvertable) hpctoolkit_futex_notify_all_v(ptr());
}

#else  // !HAVE_FUTEX_H

void atomic_uint32::wait(std::uint32_t old, std::memory_order) const noexcept {
  std::this_thread::yield();
}
void atomic_uint32::wait(std::uint32_t old, std::memory_order) const volatile noexcept {
  std::this_thread::yield();
}

void atomic_uint32::notify_one() noexcept {};
void atomic_uint32::notify_one() volatile noexcept {};

void atomic_uint32::notify_all() noexcept {};
void atomic_uint32::notify_all() volatile noexcept {};

#endif  // HAVE_FUTEX_H

std::uint32_t* atomic_uint32::ptr() noexcept {
  return reinterpret_cast<std::uint32_t*>(this);
}
const std::uint32_t* atomic_uint32::ptr() const noexcept {
  return reinterpret_cast<const std::uint32_t*>(this);
}
volatile std::uint32_t* atomic_uint32::ptr() volatile noexcept {
  return reinterpret_cast<volatile std::uint32_t*>(this);
}
const volatile std::uint32_t* atomic_uint32::ptr() const volatile noexcept {
  return reinterpret_cast<const volatile std::uint32_t*>(this);
}

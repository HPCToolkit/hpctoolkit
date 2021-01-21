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

#include "atomic.hpp"

#include "include/hpctoolkit-config.h"

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

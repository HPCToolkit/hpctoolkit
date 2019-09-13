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

#ifndef HPCTOOLKIT_PROFILE_UTIL_FUTEX_H
#define HPCTOOLKIT_PROFILE_UTIL_FUTEX_H

#ifdef __cplusplus
#include "../stdshim/version.hpp"

#include <atomic>
#include <cstdint>

namespace hpctoolkit::internal {
namespace detail {
extern "C" {
#else
#include <stdint.h>
#endif  // __cplusplus

// This part and this part alone is shared between C and C++
void hpctoolkit_futex_wait(const uint32_t*, uint32_t);
void hpctoolkit_futex_wait_v(const volatile uint32_t*, uint32_t);
void hpctoolkit_futex_notify_one(uint32_t*);
void hpctoolkit_futex_notify_one_v(volatile uint32_t*);
void hpctoolkit_futex_notify_all(uint32_t*);
void hpctoolkit_futex_notify_all_v(volatile uint32_t*);

void hpctoolkit_futex_yield();

#ifdef __cplusplus
}  // extern "C"
}  // namespace detail

/// Class-wrapper for a structure like a Linux futex, when possible. Exact
/// operation when not possible noted in comments.
/// Interface identical to C++20's std::atomic<uint32_t>.
class Futex : public std::atomic<std::uint32_t> {
public:
  using std::atomic<std::uint32_t>::atomic;
  ~Futex() = default;

#if !STD_HAS(atomic_wait)
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
#endif
};

}  // namespace hpctoolkit::internal
#endif  // __cplusplus

#ifndef STDSHIM_DONT_UNDEF
#undef STD_HAS
#endif

#endif  // HPCTOOLKIT_PROFILE_UTIL_FUTEX_H

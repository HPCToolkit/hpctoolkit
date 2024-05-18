// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_STDSHIM_FUTEX_DETAIL_H
#define HPCTOOLKIT_STDSHIM_FUTEX_DETAIL_H

#include <stdint.h>

#ifdef __cplusplus
namespace hpctoolkit::stdshim::detail {
extern "C" {
#endif

void hpctoolkit_futex_wait(const uint32_t*, uint32_t);
void hpctoolkit_futex_wait_v(const volatile uint32_t*, uint32_t);
void hpctoolkit_futex_notify_one(uint32_t*);
void hpctoolkit_futex_notify_one_v(volatile uint32_t*);
void hpctoolkit_futex_notify_all(uint32_t*);
void hpctoolkit_futex_notify_all_v(volatile uint32_t*);

#ifdef __cplusplus
}  // extern "C"
}  // namespace hpctoolkit::stdshim::detail
#endif

#endif  // HPCTOOLKIT_STDSHIM_FUTEX_DETAIL_H

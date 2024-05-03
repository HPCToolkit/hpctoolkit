// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#define _GNU_SOURCE
#include "futex-detail.h"

// Now I ask, do we have futexes or not?
#ifdef HAVE_FUTEX_H
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <sys/time.h>
#include <errno.h>
#include <limits.h>

void hpctoolkit_futex_wait(const uint32_t* word, uint32_t val) {
  syscall(SYS_futex, word, FUTEX_WAIT_PRIVATE, val, NULL, NULL, 0);
}
void hpctoolkit_futex_wait_v(volatile const uint32_t* word, uint32_t val) {
  syscall(SYS_futex, word, FUTEX_WAIT_PRIVATE, val, NULL, NULL, 0);
}

void hpctoolkit_futex_notify_one(uint32_t* word) {
  syscall(SYS_futex, word, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
}
void hpctoolkit_futex_notify_one_v(volatile uint32_t* word) {
  syscall(SYS_futex, word, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
}

void hpctoolkit_futex_notify_all(uint32_t* word) {
  syscall(SYS_futex, word, FUTEX_WAKE_PRIVATE, INT_MAX, NULL, NULL, 0);
}
void hpctoolkit_futex_notify_all_v(volatile uint32_t* word) {
  syscall(SYS_futex, word, FUTEX_WAKE_PRIVATE, INT_MAX, NULL, NULL, 0);
}

#endif

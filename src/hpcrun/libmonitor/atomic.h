// SPDX-FileCopyrightText: 2007-2023 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Libmonitor atomic ops.
 */

#ifndef  _MONITOR_ATOMIC_OPS_
#define  _MONITOR_ATOMIC_OPS_

/*
 *  The API that libmonitor supports.
 */
static inline long
compare_and_swap(volatile long *ptr, long old, long new)
{
    return __sync_val_compare_and_swap(ptr, old, new);
}

#endif  /* _MONITOR_ATOMIC_OPS_ */

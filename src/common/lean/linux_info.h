// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef linux_info_h

#define LINUX_KERNEL_NAME_REAL          "vmlinux"
#define LINUX_KERNEL_NAME               "["  LINUX_KERNEL_NAME_REAL  "]"

#define LINUX_KERNEL_SYMBOL_FILE_SHORT  "kallsyms"

#define LINUX_KERNEL_SYMBOL_FILE        "/proc/" LINUX_KERNEL_SYMBOL_FILE_SHORT
#define LINUX_PERF_EVENTS_FILE          "/proc/sys/kernel/perf_event_paranoid"
#define LINUX_PERF_EVENTS_MAX_RATE      "/proc/sys/kernel/perf_event_max_sample_rate"
#define LINUX_KERNEL_KPTR_RESTICT       "/proc/sys/kernel/kptr_restrict"

// measurement subdirectory where kallsyms files from compute nodes will be recorded
#define KERNEL_SYMBOLS_DIRECTORY        "kernel_symbols"

#endif /* linux_info_h */

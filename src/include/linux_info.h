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
// Copyright ((c)) 2002-2020, Rice University
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

// -*-Mode: C++;-*- // technically C99

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
// Copyright ((c)) 2002-2022, Rice University
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

#ifndef AUDIT_AUDITAPI_H
#define AUDIT_AUDITAPI_H

#include <link.h>
#include <stdbool.h>

#include "include/hpctoolkit-config.h"

// The entirety of this file only makes sense in the dynamic case, so
// error if someone tries to use it in the static case.
#ifdef HPCRUN_STATIC_LINK
#error The auditor interface only makes sense in the dynamic case!
#endif

// Structure used to represent an active link map entry. Most informational
// fields are filled by the auditor.
typedef struct auditor_map_entry_t {
  // Full path to the mapped library, after passing through realpath and
  // handling the main application name.
  char* path;

  // Byterange of the mapped region
  void* start;
  void* end;

  // Synthesized data as if from dl_iterate_phdr. Not all fields are filled,
  // check `dl_info_sz` for which are and aren't.
  struct dl_phdr_info dl_info;
  size_t dl_info_sz;

  // Corrosponding load_module_t for this here thing.
  struct load_module_t* load_module;

  // link_map entry for this library, if that makes sense for this entry.
  struct link_map* map;
} auditor_map_entry_t;

typedef struct auditor_hooks_t {
  // Called whenever a new entry is entering the link map.
  void (*open)(auditor_map_entry_t* entry);

  // Called whenever a previously `open`'d entry is removed from the link map.
  void (*close)(auditor_map_entry_t* entry);

  // Called whenever the link map becomes "stable" again, after a batch of
  // previous modifications. If `additive` is true, the link map was being
  // added to directly before this call.
  void (*stable)(bool additive);
} auditor_hooks_t;

typedef struct auditor_exports_t {
  // Called by the mainlib once it is prepared to receive notifications.
  void (*mainlib_connected)(const char* vdso_path);
  // Called by the mainlib when it no longer wishes to receive notifications.
  void (*mainlib_disconnect)();

  // Purified environment that has our effects (mostly) removed.
  char** pure_environ;

  // Exports from libc to aid in wrapper evasion
  int (*pipe)(int[2]);
  int (*close)(int);
  pid_t (*waitpid)(pid_t, int*, int);
  int (*clone)(int (*)(void*), void*, int, void*, ...);
  int (*execve)(const char*, char* const[], char* const[]);
} auditor_exports_t;

// Called as early as possible in the process startup, before any static
// initialization (including libc's). Most things don't work here, so
// use the `initialize` hook for things after that.
typedef void (*auditor_attach_pfn_t)(const auditor_exports_t*, auditor_hooks_t*);
extern void hpcrun_auditor_attach(const auditor_exports_t*, auditor_hooks_t*);

extern const auditor_exports_t* auditor_exports;

// Called by the mainlib to initialize the fake auditor in the event that
// one of the other interfaces has been called already.
extern void hpcrun_init_fake_auditor();

#endif  // AUDIT_AUDITAPI_H

// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef AUDIT_AUDITAPI_H
#define AUDIT_AUDITAPI_H

#include <link.h>
#include <stdbool.h>
#include <stdarg.h>
#include <signal.h>


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

  // Corresponding load_module_t for this here thing.
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

  // Called when iterating back over the list of loaded objects in the private namespace.
  int (*dl_iterate_phdr)(int (*cb)(struct dl_phdr_info * info, size_t size, void* data),
                         void *data);
} auditor_hooks_t;

typedef struct auditor_exports_t {
  // Called by the mainlib once it is prepared to receive notifications.
  void (*mainlib_connected)(const char* vdso_path, const auditor_hooks_t*);

  // Called by the mainlib when it no longer wishes to receive notifications.
  void (*mainlib_disconnect)();

  // Called to load and bind new libraries in the auditor's namespace.
  // Same semantics as #hpcrun_bind_v.
  void (*hpcrun_bind_v)(const char*, va_list);

  // Exports from libc to aid in wrapper evasion
  void (*exit)(int);
  int (*sigprocmask)(int, const sigset_t*, sigset_t*);
  int (*pthread_sigmask)(int, const sigset_t*, sigset_t*);
  int (*sigaction)(int, const struct sigaction* restrict, struct sigaction* restrict);
} auditor_exports_t;

// Get a pointer to the current set of auditor exports
extern const auditor_exports_t* auditor_exports();

typedef const auditor_exports_t* (*pfn_connect_to_auditor)();
__attribute__((visibility("default")))
const auditor_exports_t* hpcrun_connect_to_auditor();

#endif  // AUDIT_AUDITAPI_H

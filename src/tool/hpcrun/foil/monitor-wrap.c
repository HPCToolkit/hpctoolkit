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
// Copyright ((c)) 2002-2023, Rice University
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

#include "foil.h"
#include "main.h"

#include <monitor.h>

extern typeof(monitor_at_main) __wrap_monitor_at_main;
HPCRUN_EXPOSED void __wrap_monitor_at_main() {
  LOOKUP_FOIL_BASE(base, monitor_at_main);
  return base();
}

extern typeof(monitor_begin_process_exit) __wrap_monitor_begin_process_exit;
HPCRUN_EXPOSED void __wrap_monitor_begin_process_exit(int how) {
  LOOKUP_FOIL_BASE(base, monitor_begin_process_exit);
  return base(how);
}

extern typeof(monitor_fini_process) __wrap_monitor_fini_process;
HPCRUN_EXPOSED void __wrap_monitor_fini_process(int how, void* data) {
  LOOKUP_FOIL_BASE(base, monitor_fini_process);
  return base(how, data);
}

extern typeof(monitor_fini_thread) __wrap_monitor_fini_thread;
HPCRUN_EXPOSED void __wrap_monitor_fini_thread(void* init_thread_data) {
  LOOKUP_FOIL_BASE(base, monitor_fini_thread);
  return base(init_thread_data);
}

extern typeof(monitor_init_mpi) __wrap_monitor_init_mpi;
HPCRUN_EXPOSED void __wrap_monitor_init_mpi(int* argc, char*** argv) {
  LOOKUP_FOIL_BASE(base, monitor_init_mpi);
  return base(argc, argv);
}

extern typeof(monitor_init_process) __wrap_monitor_init_process;
HPCRUN_EXPOSED void* __wrap_monitor_init_process(int* argc, char** argv, void* data) {
  LOOKUP_FOIL_BASE(base, monitor_init_process);
  return base(argc, argv, data);
}

extern typeof(monitor_init_thread) __wrap_monitor_init_thread;
HPCRUN_EXPOSED void* __wrap_monitor_init_thread(int tid, void* data) {
  LOOKUP_FOIL_BASE(base, monitor_init_thread);
  return base(tid, data);
}

extern typeof(monitor_mpi_pre_init) __wrap_monitor_mpi_pre_init;
HPCRUN_EXPOSED void __wrap_monitor_mpi_pre_init() {
  LOOKUP_FOIL_BASE(base, monitor_mpi_pre_init);
  return base();
}

extern typeof(monitor_post_fork) __wrap_monitor_post_fork;
HPCRUN_EXPOSED void __wrap_monitor_post_fork(pid_t child, void* data) {
  LOOKUP_FOIL_BASE(base, monitor_post_fork);
  return base(child, data);
}

extern typeof(monitor_pre_fork) __wrap_monitor_pre_fork;
HPCRUN_EXPOSED void* __wrap_monitor_pre_fork() {
  LOOKUP_FOIL_BASE(base, monitor_pre_fork);
  return base();
}

extern typeof(monitor_reset_stacksize) __wrap_monitor_reset_stacksize;
HPCRUN_EXPOSED size_t __wrap_monitor_reset_stacksize(size_t old_size) {
  LOOKUP_FOIL_BASE(base, monitor_reset_stacksize);
  return base(old_size);
}

extern typeof(monitor_start_main_init) __wrap_monitor_start_main_init;
HPCRUN_EXPOSED void __wrap_monitor_start_main_init() {
  LOOKUP_FOIL_BASE(base, monitor_start_main_init);
  return base();
}

extern typeof(monitor_thread_post_create) __wrap_monitor_thread_post_create;
HPCRUN_EXPOSED void __wrap_monitor_thread_post_create(void* data) {
  LOOKUP_FOIL_BASE(base, monitor_thread_post_create);
  return base(data);
}

extern typeof(monitor_thread_pre_create) __wrap_monitor_thread_pre_create;
HPCRUN_EXPOSED void* __wrap_monitor_thread_pre_create() {
  LOOKUP_FOIL_BASE(base, monitor_thread_pre_create);
  return base();
}

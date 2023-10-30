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

HPCRUN_EXPOSED void monitor_at_main() {
  LOOKUP_FOIL_BASE(base, monitor_at_main);
  return base();
}

HPCRUN_EXPOSED void monitor_begin_process_exit(int how) {
  LOOKUP_FOIL_BASE(base, monitor_begin_process_exit);
  return base(how);
}

HPCRUN_EXPOSED void monitor_fini_process(int how, void* data) {
  LOOKUP_FOIL_BASE(base, monitor_fini_process);
  return base(how, data);
}

HPCRUN_EXPOSED void monitor_fini_thread(void* init_thread_data) {
  LOOKUP_FOIL_BASE(base, monitor_fini_thread);
  return base(init_thread_data);
}

HPCRUN_EXPOSED void monitor_init_mpi(int* argc, char*** argv) {
  LOOKUP_FOIL_BASE(base, monitor_init_mpi);
  return base(argc, argv);
}

HPCRUN_EXPOSED void* monitor_init_process(int* argc, char** argv, void* data) {
  LOOKUP_FOIL_BASE(base, monitor_init_process);
  return base(argc, argv, data);
}

HPCRUN_EXPOSED void* monitor_init_thread(int tid, void* data) {
  LOOKUP_FOIL_BASE(base, monitor_init_thread);
  return base(tid, data);
}

HPCRUN_EXPOSED void monitor_mpi_pre_init() {
  LOOKUP_FOIL_BASE(base, monitor_mpi_pre_init);
  return base();
}

HPCRUN_EXPOSED void monitor_post_fork(pid_t child, void* data) {
  LOOKUP_FOIL_BASE(base, monitor_post_fork);
  return base(child, data);
}

HPCRUN_EXPOSED void* monitor_pre_fork() {
  LOOKUP_FOIL_BASE(base, monitor_pre_fork);
  return base();
}

HPCRUN_EXPOSED size_t monitor_reset_stacksize(size_t old_size) {
  LOOKUP_FOIL_BASE(base, monitor_reset_stacksize);
  return base(old_size);
}

HPCRUN_EXPOSED void monitor_start_main_init() {
  LOOKUP_FOIL_BASE(base, monitor_start_main_init);
  return base();
}

HPCRUN_EXPOSED void monitor_thread_post_create(void* data) {
  LOOKUP_FOIL_BASE(base, monitor_thread_post_create);
  return base(data);
}

HPCRUN_EXPOSED void* monitor_thread_pre_create() {
  LOOKUP_FOIL_BASE(base, monitor_thread_pre_create);
  return base();
}

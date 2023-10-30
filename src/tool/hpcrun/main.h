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

#ifndef main_h
#define main_h

#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

// FIXME: not in master-gpu-trace. why?
// #include <tool/hpcrun/sample-sources/nvidia/stream-tracing.h>
//#include <tool/hpcrun/sample-sources/amd/amd.h>

extern bool hpcrun_is_initialized();

extern bool hpcrun_is_safe_to_sync(const char* fn);
extern void hpcrun_set_safe_to_sync(void);

extern void hpcrun_force_dlopen(bool forced);

//
// fetch the full path of the execname
//
extern char* hpcrun_get_execname(void);

typedef void siglongjmp_fcn(sigjmp_buf, int);

typedef struct hpcrun_aux_cleanup_t  hpcrun_aux_cleanup_t;

hpcrun_aux_cleanup_t * hpcrun_process_aux_cleanup_add( void (*func) (void *), void * arg);
void hpcrun_process_aux_cleanup_remove(hpcrun_aux_cleanup_t * node);

// ** HACK to accommodate PAPI-C w cuda component & gpu blame shifting

extern void special_cuda_ctxt_actions(bool enable);

extern bool hpcrun_suppress_sample();

extern void hpcrun_prepare_measurement_subsystem(bool is_child);

// Foil base functions
extern void foilbase_monitor_at_main();
extern void foilbase_monitor_begin_process_exit(int);
extern void foilbase_monitor_fini_process(int, void*);
extern void foilbase_monitor_fini_thread(void*);
extern void foilbase_monitor_init_mpi(int *argc, char ***argv);
extern void* foilbase_monitor_init_process(int *argc, char **argv, void* data);
extern void* foilbase_monitor_init_thread(int tid, void* data);
extern void foilbase_monitor_mpi_pre_init();
extern void foilbase_monitor_post_fork(pid_t child, void* data);
extern void* foilbase_monitor_pre_fork();
extern size_t foilbase_monitor_reset_stacksize(size_t old_size);
extern void foilbase_monitor_start_main_init();
extern void foilbase_monitor_thread_post_create(void* data);
extern void* foilbase_monitor_thread_pre_create();


#endif  // ! main_h

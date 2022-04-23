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

#ifndef PAPI_C_EXTENDED_INFO_H
#define PAPI_C_EXTENDED_INFO_H

#include <stdbool.h>

typedef void (*get_event_set_proc_t)(int* ev_s);
typedef int (*add_event_proc_t)(int ev_s, int evcode);
typedef void (*finalize_event_set_proc_t)(void);
typedef void (*setup_proc_t)(void);
typedef void (*teardown_proc_t)(void);
typedef void (*start_proc_t)(void);
typedef void (*stop_proc_t)(void);
typedef bool (*pred_proc_t)(const char* name);

typedef struct sync_info_list_t {
  const pred_proc_t pred;
  const get_event_set_proc_t get_event_set;
  const add_event_proc_t add_event;
  const finalize_event_set_proc_t finalize_event_set;
  const setup_proc_t sync_setup;
  const teardown_proc_t sync_teardown;
  const start_proc_t sync_start;
  const stop_proc_t sync_stop;
  const bool process_only;
  struct sync_info_list_t* next;
} sync_info_list_t;

extern bool component_uses_sync_samples(int cidx);
extern get_event_set_proc_t component_get_event_set(int cidx);
extern add_event_proc_t component_add_event_proc(int cidx);
extern finalize_event_set_proc_t component_finalize_event_set(int cidx);
extern setup_proc_t sync_setup_for_component(int cidx);
extern teardown_proc_t sync_teardown_for_component(int cidx);
extern start_proc_t sync_start_for_component(int cidx);
extern stop_proc_t sync_stop_for_component(int cidx);
extern void papi_c_sync_register(sync_info_list_t* info);

#endif  // PAPI_C_EXTENDED_INFO_H

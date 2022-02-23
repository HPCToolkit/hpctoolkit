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

#ifndef PAPI_C_H
#define PAPI_C_H

/******************************************************************************
 * local includes
 *****************************************************************************/

#include "papi-c-extended-info.h"

#include "sample_source_obj.h"
#include "cct.h"


/******************************************************************************
 * type declarations 
 *****************************************************************************/

typedef struct {
  const char *name;
  bool inUse;
  int eventSet;
  source_state_t state;
  int some_derived;
  bool scale_by_thread_count;
  long long prev_values[MAX_EVENTS];
  cct_node_t *cct_node;
  bool is_gpu_sync;
  bool setup_process_only;
  get_event_set_proc_t get_event_set;
  add_event_proc_t add_event;
  finalize_event_set_proc_t finalize_event_set;
  start_proc_t start;
  read_proc_t read;
  stop_proc_t stop;
  setup_proc_t setup;
  teardown_proc_t teardown;
} papi_component_info_t;


typedef struct {
  int num_components;
  papi_component_info_t component_info[0];
} papi_source_info_t;


/******************************************************************************
 * external declarations 
 *****************************************************************************/

extern int get_component_event_set(papi_component_info_t* ci);

#endif // PAPI_C_H

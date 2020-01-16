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

// support for deferred callstack resolution

#ifndef __OMPT_DEFER_H__
#define __OMPT_DEFER_H__


//******************************************************************************
// local includes  
//******************************************************************************

#include <hpcrun/thread_data.h>
#include <hpcrun/unwind/common/backtrace_info.h>

#include "ompt-types.h"



//******************************************************************************
// macros
//******************************************************************************

#define IS_UNRESOLVED_ROOT(addr) (addr->ip_norm.lm_id == (uint16_t)UNRESOLVED)



//******************************************************************************
// interface functions 
//******************************************************************************

// check whether the lazy resolution is needed in an unwind
int
need_defer_cntxt
(
 void
);


// resolve the contexts 
void 
resolve_cntxt
(
 void
);


void 
resolve_cntxt_fini
(
 thread_data_t *thread_data
);


void 
resolve_other_cntxt
(
 thread_data_t *thread_data
);


uint64_t 
is_partial_resolve
(
 cct_node_t *prefix
);


// deferred region ID assignment
void 
init_region_id
(
 void
);


cct_node_t *
hpcrun_region_lookup
(
 uint64_t id
);


int 
length_trl
(
 ompt_trl_el_t* regions_list_head, 
 int of_freelist
);


int 
register_thread
(
 int level
);


void 
register_thread_to_all_regions
(
 void
);


void 
register_to_all_regions
(
 void
);


void 
try_resolve_context
(
 void
);


int 
try_resolve_one_region_context
(
 void
);


void 
resolve_one_region_context
(
 ompt_region_data_t* region_data
);


void 
ompt_resolve_region_contexts_poll
(
 void
);


void 
ompt_resolve_region_contexts
(
 int is_process
);


// function which provides call path for regions where thread is the master
void 
provide_callpath_for_regions_if_needed
(
  backtrace_info_t* bt, 
  cct_node_t* cct
);


void 
provide_callpath_for_end_of_the_region
(
 backtrace_info_t *bt, 
 cct_node_t *cct
);


// stack reorganization
void 
add_region_and_ancestors_to_stack
(
 ompt_region_data_t *region_data, 
 bool team_master
);


void 
tmp_end_region_resolve
(
 ompt_notification_t *notification, 
 cct_node_t* prefix
);

#endif

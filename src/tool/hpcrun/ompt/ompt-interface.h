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

#ifndef __OMPT_INTERFACE_H__
#define __OMPT_INTERFACE_H__


//*****************************************************************************
// local includes
//*****************************************************************************

#include <hpcrun/cct/cct.h>

#include "omp-tools.h"
#include "ompt-types.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define OMPT_BASE_T_STAR(ptr) (ompt_base_t *)ptr
#define OMPT_BASE_T_STAR_STAR(ptr) (ompt_base_t **)&ptr
#define OMPT_BASE_T_GET_NEXT(ptr) ptr->next.next

#define OMPT_REGION_DATA_T_START(ptr) (ompt_region_data_t *)ptr
#define OMPT_NOTIFICATION_T_START(ptr) (ompt_notification_t *)ptr
#define OMPT_TRL_EL_T_START(ptr) (ompt_trl_el_t *)ptr

#define ompt_set_callback(e, cb) \
  ompt_set_callback_internal(e, (ompt_callback_t) cb)



//*****************************************************************************
// interface functions
//*****************************************************************************

//------------------------------------------------------------------------------
// hpcrun wrappers for ompt interfaces
//------------------------------------------------------------------------------

int 
hpcrun_ompt_get_parallel_info
(
 int ancestor_level,
 ompt_data_t **parallel_data,
 int *team_size
);


uint64_t 
hpcrun_ompt_get_unique_id
(
 void
);


ompt_data_t * 
hpcrun_ompt_get_task_data
(
 int level
);


uint64_t 
hpcrun_ompt_outermost_parallel_id
(
 void
);


uint64_t 
hpcrun_ompt_get_parallel_info_id
(
 int ancestor_level
);


void 
hpcrun_ompt_get_parallel_info_id_pointer
(
 int ancestor_level, 
 uint64_t *region_id
);


ompt_state_t 
hpcrun_ompt_get_state
(
 uint64_t *wait_id
);


ompt_frame_t *
hpcrun_ompt_get_task_frame
(
 int level
);


int 
hpcrun_ompt_get_thread_num
(
 int level
);


// returns 1 if the current state represents a form of overhead
int 
hpcrun_omp_state_is_overhead
(
 void
);


void *
hpcrun_ompt_get_idle_frame
(
 void
);


uint64_t 
hpcrun_ompt_get_blame_target
(
 void
);


int 
hpcrun_ompt_elide_frames
(
 void
);


void 
ompt_mutex_blame_shift_request
(
 void
);


void 
ompt_idle_blame_shift_request
(
 void
);


int
ompt_task_full_context_p
(
 void
);

//-----------------------------------------------------------------------------
// allocate and free notifications
//-----------------------------------------------------------------------------

ompt_notification_t * 
hpcrun_ompt_notification_alloc
(
 void
);


// free adds entity to freelist
void 
hpcrun_ompt_notification_free
(
 ompt_notification_t *notification
);


//-----------------------------------------------------------------------------
// allocate and free thread's regions
//-----------------------------------------------------------------------------

ompt_trl_el_t * 
hpcrun_ompt_trl_el_alloc
(
 void
);


void 
hpcrun_ompt_trl_el_free
(
 ompt_trl_el_t *thread_region
);


ompt_region_data_t * 
hpcrun_ompt_get_region_data
(
 int ancestor_level
);


//-----------------------------------------------------------------------------
// access to region data
//-----------------------------------------------------------------------------

ompt_region_data_t * 
hpcrun_ompt_get_current_region_data
(
 void
);


ompt_region_data_t * 
hpcrun_ompt_get_parent_region_data
(
 void
);


//-----------------------------------------------------------------------------
// freelist manipulation 
//-----------------------------------------------------------------------------

ompt_base_t * 
freelist_remove_first
(
 ompt_base_t **head
);


void 
freelist_add_first
(
 ompt_base_t *new, 
 ompt_base_t **head
);


ompt_set_result_t 
ompt_set_callback_internal
(
  ompt_callbacks_t event,
  ompt_callback_t callback
);


#endif // _OMPT_INTERFACE_H_

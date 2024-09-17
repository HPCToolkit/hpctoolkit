// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __OMPT_INTERFACE_H__
#define __OMPT_INTERFACE_H__


//*****************************************************************************
// local includes
//*****************************************************************************

#include "../cct/cct.h"

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

typedef struct {
  int task_type_flags;
  ompt_data_t *task_data;
  ompt_data_t *parallel_data;
  ompt_frame_t *task_frame;
  int thread_num;
} hpcrun_task_info_t;



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


bool
hpcrun_ompt_get_task_info
(
 int level,
 hpcrun_task_info_t *ti
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

extern ompt_start_tool_result_t *
hpcrun_ompt_start_tool
(
 unsigned int omp_version,
 const char *runtime_version
);

#endif // _OMPT_INTERFACE_H_

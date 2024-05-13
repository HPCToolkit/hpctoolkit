// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __OMPT_TYPES_H__
#define __OMPT_TYPES_H__

//******************************************************************************
// OMPT
//******************************************************************************

#include "omp-tools.h"



//******************************************************************************
// local includes
//******************************************************************************

#ifndef __cplusplus
#include <stdatomic.h>
#else
#include <atomic>
#endif

#include "../cct/cct.h"



//******************************************************************************
// macros
//******************************************************************************
#define task_data_invalid ((void*)~0)
#define ompt_base_nil ((ompt_base_t*)0)
#define ompt_base_invalid ((ompt_base_t*)~0)



//******************************************************************************
// forward declarations of types
//******************************************************************************

struct ompt_base_s;
struct ompt_queue_data_s;
struct ompt_notification_s;
struct ompt_threads_queue_s;
struct ompt_thread_region_freelist_s;



//******************************************************************************
// type declarations
//******************************************************************************

typedef union ompt_next_u ompt_next_t;
union ompt_next_u{
  struct ompt_base_s *next;
  _Atomic (struct ompt_base_s*)anext;
};


typedef struct ompt_base_s{
   ompt_next_t next; // FIXME: this should be Atomic too
} ompt_base_t;


// ompt_wfq_s
typedef struct ompt_wfq_s{
  _Atomic(ompt_base_t*)head;
} ompt_wfq_t;


typedef struct ompt_region_data_s {
  // region freelist, must be at the begin because od up/downcasting
  // like inherited class in C++, inheretes from base_t
  ompt_next_t next;

  // region's wait free queue
  ompt_wfq_t queue;

  // region's freelist which belongs to thread
  ompt_wfq_t *thread_freelist;

  // region id
  uint64_t region_id;

  // call_path to the region
  cct_node_t *call_path;

  // depth of the region, starts from zero
  int depth;

  struct ompt_region_data_s *next_region;
} ompt_region_data_t;


typedef struct ompt_notification_s{
  // it can also cover freelist to, we do not need another next_freelist
  ompt_next_t next;

  ompt_region_data_t *region_data;

  // region id
  uint64_t region_id;

  // struct ompt_threads_queue_s *threads_queue;
  ompt_wfq_t *threads_queue;

  // pointer to the cct pseudo node of the region that should be resolve
  cct_node_t* unresolved_cct;
} ompt_notification_t;


// trl = Thread's Regions List
// el  = element
typedef struct ompt_trl_el_s {
  // inherit from base_t
  ompt_next_t next;

  // previous in double-linked list
  struct ompt_trl_el_s* prev;

  // stores region's information
  ompt_region_data_t* region_data;
} ompt_trl_el_t;


// region stack element which points to the corresponding
// notification, and says if thread took sample and if the
// thread is the master in team
typedef struct region_stack_el_s {
  ompt_notification_t *notification;
  bool took_sample;
  bool team_master;
  ompt_frame_t *parent_frame;
} region_stack_el_t;


// FIXME vi3: ompt_data_t freelist manipulation

#endif

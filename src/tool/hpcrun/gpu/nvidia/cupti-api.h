// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
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

#ifndef cupti_api_h
#define cupti_api_h



//******************************************************************************
// nvidia includes
//******************************************************************************

#include <cupti.h>
#include <stdbool.h>

#include <hpcrun/cct/cct.h>

//******************************************************************************
// constants
//******************************************************************************

extern CUpti_ActivityKind
external_correlation_activities[];

extern CUpti_ActivityKind
data_motion_explicit_activities[];

extern CUpti_ActivityKind
data_motion_implicit_activities[];

extern CUpti_ActivityKind
kernel_invocation_activities[];

extern CUpti_ActivityKind
kernel_execution_activities[];

extern CUpti_ActivityKind
driver_activities[];

extern CUpti_ActivityKind
runtime_activities[];

extern CUpti_ActivityKind
overhead_activities[];

typedef enum {
  cupti_set_all = 1,
  cupti_set_some = 2,
  cupti_set_none = 3
} cupti_set_status_t;


#define CUPTI_PC_SAMPLING_PERIOD_NULL -1
#define CUPTI_CORRELATION_ID_NULL 0

//******************************************************************************
// interface functions
//******************************************************************************

int
cupti_bind
(
 void
);


void
cupti_activity_process
(
 CUpti_Activity *activity
);


void 
cupti_buffer_alloc 
(
 uint8_t **buffer, 
 size_t *buffer_size, 
 size_t *maxNumRecords
);


void
cupti_callback_enable
(
 CUpti_SubscriberHandle subscriber,
 CUpti_CallbackId cbid,
 CUpti_CallbackDomain domain
);


void
cupti_callback_disable
(
 CUpti_SubscriberHandle subscriber,
 CUpti_CallbackId cbid,
 CUpti_CallbackDomain domain
);


void
cupti_callbacks_subscribe
(
 void
);


void
cupti_callbacks_unsubscribe
(
 void
);


void
cupti_correlation_enable
(
 void
);


void
cupti_correlation_disable
(
 void
);


void
cupti_pc_sampling_enable
(
 CUcontext context,
 int frequency 
);


void
cupti_pc_sampling_disable
(
 CUcontext context
);


cupti_set_status_t 
cupti_monitoring_set
(
 const CUpti_ActivityKind activity_kinds[],
 bool enable
);


void
cupti_device_timestamp_get
(
 CUcontext context,
 uint64_t *time
);


void cupti_activity_timestamp_get
(
 uint64_t *time
);


void 
cupti_init
(
 void
);


void 
cupti_start
(
 void
);


void 
cupti_pause
(
 CUcontext context,
 bool begin_pause
);


void 
cupti_finalize
(
 void
);


void
cupti_device_buffer_config
(
 size_t buf_size,
 size_t sem_size
);


void
cupti_num_dropped_records_get
(
 CUcontext context,
 uint32_t streamId,
 size_t* dropped 
);


bool
cupti_buffer_cursor_advance
(
  uint8_t *buffer,
  size_t size,
  CUpti_Activity **current
);


void 
cupti_buffer_completion_callback
(
 CUcontext ctx,
 uint32_t streamId,
 uint8_t *buffer,
 size_t size,
 size_t validSize
);


void
cupti_load_callback_cuda
(
 CUcontext context,
 uint32_t module_id, 
 const void *cubin, 
 size_t cubin_size
);


void
cupti_unload_callback_cuda
(
 CUcontext context,
 uint32_t module_id, 
 const void *cubin, 
 size_t cubin_size
);


//******************************************************************************
// finalizer and initializer
//******************************************************************************

void
cupti_activity_flush
(
 void
);


void
cupti_device_flush
(
 void *args,
 int how
);


void
cupti_device_shutdown
(
 void *args,
 int how
);


void
cupti_device_init
(
);


//******************************************************************************
// cupti status
//******************************************************************************

bool
cupti_thread_activity_flag_get
(
 void
);


void
cupti_thread_activity_flag_set
(
 void
);


void
cupti_thread_activity_flag_unset
(
 void
);


bool
cupti_runtime_api_flag_get
(
 void
);


void
cupti_runtime_api_flag_unset
(
 void
);


void
cupti_runtime_api_flag_set
(
 void
);


cct_node_t *
cupti_kernel_ph_get
(
 void
);


void
cupti_kernel_ph_set
(
 cct_node_t *node
);


cct_node_t *
cupti_trace_ph_get
(
 void
);


void
cupti_trace_ph_set
(
 cct_node_t *node
);


void
cupti_correlation_id_push
(
 uint64_t id
);


uint64_t
cupti_correlation_id_pop
(
 void
);


uint64_t
cupti_runtime_correlation_id_get
(
 void
);


void
cupti_runtime_correlation_id_set
(
 uint64_t
);


uint64_t
cupti_driver_correlation_id_get
(
 void
);


void
cupti_driver_correlation_id_set
(
 uint64_t correlation_id
);


void
cupti_fast_unwind_set
(
 bool fast_unwind
);


bool
cupti_fast_unwind_get
(
);


void
cupti_correlation_threshold_set
(
 int32_t
);


int32_t
cupti_correlation_threshold_get
(
);


void
cupti_backoff_base_set
(
 int32_t backoff_base
);


int32_t
cupti_backoff_base_get
(
);


void
cupti_sync_yield_set
(
 bool sync_yield
);


bool
cupti_sync_yield_get
(
);


void
cupti_callbacks_subscribe2
(
);


void
cupti_callbacks_unsubscribe2
(
);


void
cupti_device_flush2
(
 void *args,
 int how
);


void
cupti_device_shutdown2
(
 void *args,
 int how
);

#endif

// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef cupti_api_h
#define cupti_api_h



//******************************************************************************
// nvidia includes
//******************************************************************************

#include <cupti.h>



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


//******************************************************************************
// interface functions
//******************************************************************************


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
 const  CUpti_ActivityKind activity_kinds[],
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
 uint32_t module_id,
 const void *cubin,
 size_t cubin_size
);


void
cupti_unload_callback_cuda
(
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

void
cupti_stop_flag_set
(
 void
);


void
cupti_stop_flag_unset
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



#endif

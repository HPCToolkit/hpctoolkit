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



/******************************************************************************
 * global include files
 *****************************************************************************/

#include <sys/param.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include <hpcrun/safe-sampling.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/device-finalizers.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/utilities/hpcrun-nanotime.h>

#include "ompt-interface.h"
#include "ompt-device-map.h"
#include "ompt-placeholders.h"

#include "gpu/gpu-op-placeholders.h"
#include "gpu/gpu-application-thread-api.h"
#include "gpu/gpu-correlation-channel.h"
#include "gpu/gpu-correlation-channel-set.h"
#include "gpu/gpu-correlation-id.h"
#include "gpu/gpu-metrics.h"
#include "gpu/gpu-monitoring.h"
#include "gpu/gpu-monitoring-thread-api.h"
#include "gpu/gpu-trace.h"

#include "gpu/ompt/ompt-gpu-api.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define FOREACH_OMPT_DATA_OP(macro)				     \
  macro(op, ompt_target_data_alloc, ompt_tgt_alloc)		     \
  macro(op, ompt_target_data_delete, ompt_tgt_delete)		     \
  macro(op, ompt_target_data_transfer_to_device, ompt_tgt_copyin)    \
  macro(op, ompt_target_data_transfer_from_device, ompt_tgt_copyout)

// with OMPT support turned on, callpath pruning should not be necessary
#define PRUNE_CALLPATH 0

#define OMPT_ACTIVITY_DEBUG 0

#if OMPT_ACTIVITY_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif

#define OMPT_API_FNTYPE(fn) fn##_t

#define OMPT_API_FUNCTION(return_type, fn, args)  \
  typedef return_type (*OMPT_API_FNTYPE(fn)) args

#define OMPT_TARGET_API_FUNCTION(return_type, fn, args)  \
  OMPT_API_FUNCTION(return_type, fn, args)

#define FOREACH_OMPT_TARGET_FN(macro) \
  macro(ompt_get_device_time) \
  macro(ompt_translate_time) \
  macro(ompt_set_trace_ompt) \
  macro(ompt_start_trace) \
  macro(ompt_pause_trace) \
  macro(ompt_stop_trace) \
  macro(ompt_flush_trace) \
  macro(ompt_get_record_type) \
  macro(ompt_get_record_ompt) \
  macro(ompt_get_record_abstract) \
  macro(ompt_advance_buffer_cursor)



//*****************************************************************************
// type declarations
//*****************************************************************************

typedef struct ompt_device_entry_t {
  int device_id;
  ompt_device_t *device;
  struct ompt_device_entry_t *next;
} ompt_device_entry_t;



//*****************************************************************************
// forward declarations
//*****************************************************************************

static void ompt_dump(ompt_record_ompt_t *r) __attribute__((unused));



//*****************************************************************************
// static variables
//*****************************************************************************

static device_finalizer_fn_entry_t device_finalizer_flush;
static device_finalizer_fn_entry_t device_finalizer_trace;
static device_finalizer_fn_entry_t device_finalizer_shutdown;

static int ompt_shutdown_complete = 0;

static ompt_device_entry_t *device_list = 0;

static __thread bool ompt_need_flush = false;



//*****************************************************************************
// private operations
//*****************************************************************************

static void
device_list_insert
(
 int device_id,
 ompt_device_t *device
)
{
  // FIXME: replace with splay-uint64
  ompt_device_entry_t *e = (ompt_device_entry_t *)
    malloc(sizeof(ompt_device_entry_t));
  e->device_id = device_id;
  e->device = device;
  e->next = device_list;
  device_list = e;
  PRINT("device_list_insert id=%d device=%p\n", device_id, device);
}

//------------------------------------------------
// declare function pointers for target functions
//------------------------------------------------

#define ompt_decl_name(fn) \
  fn ## _t  fn;

FOREACH_OMPT_TARGET_FN(ompt_decl_name)

#undef ompt_decl_name


//*****************************************************************************
// thread-local variables
//*****************************************************************************

static __thread cct_node_t *target_node = NULL;
static __thread cct_node_t *trace_node = NULL;

static __thread bool ompt_runtime_api_flag = false;

//*****************************************************************************
// device operations
//*****************************************************************************

static void
hpcrun_ompt_op_id_notify(ompt_scope_endpoint_t endpoint,
                         ompt_id_t host_op_id,
                         ip_normalized_t ip_norm)
{
  // A runtime API must be implemented by driver APIs.
  if (endpoint == ompt_scope_begin) {
    // Enter a ompt runtime api
    PRINT("enter ompt runtime op %lu\n", host_op_id);
    ompt_runtime_api_flag = true;

    gpu_application_thread_process_activities();

#if 0
    ompt_correlation_id_push(host_op_id);
#endif

    gpu_op_ccts_t gpu_op_ccts;
    memset(&gpu_op_ccts, 0, sizeof(gpu_op_ccts_t));

    hpcrun_safe_enter();

    cct_addr_t frm;
    memset(&frm, 0, sizeof(cct_addr_t));
    frm.ip_norm = ip_norm;
    cct_node_t *api_node = hpcrun_cct_insert_addr(target_node, &frm);

    gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags_all);

    hpcrun_safe_exit();

    trace_node = gpu_op_ccts.ccts[gpu_placeholder_type_trace];

    // Inform the worker about the placeholders
    uint64_t cpu_submit_time = hpcrun_nanotime();
    PRINT("producing correlation %lu\n", host_op_id);
    gpu_correlation_channel_produce(host_op_id, &gpu_op_ccts, cpu_submit_time);
  } else {
    PRINT("exit ompt runtime op %lu\n", host_op_id);
    // Enter a runtime api
    ompt_runtime_api_flag = false;
#if 0
    // Pop the id and make a notification
    ompt_correlation_id_pop();
#endif
    // Clear kernel status
    trace_node = NULL;
  }

  return;
}


void
ompt_bind_names(ompt_function_lookup_t lookup)
{
#define ompt_bind_name(fn) \
  fn = (fn ## _t ) lookup(#fn); \
  PRINT("look up function %s, got %p\n", #fn, fn);

  FOREACH_OMPT_TARGET_FN(ompt_bind_name)

#undef ompt_bind_name
}


#define BUFFER_SIZE (1024 * 1024 * 8)

static void
ompt_buffer_request
(
 int device_id,
 ompt_buffer_t **buffer,
 size_t *bytes
)
{
  *bytes = BUFFER_SIZE;
  *buffer = (ompt_buffer_t *)malloc(*bytes);
  assert(buffer);
}


static void
ompt_buffer_release
(
 ompt_buffer_t *buffer
)
{
  free(buffer);
}


static void
ompt_dump
(
 ompt_record_ompt_t *r
)
{
  if (r) {
    printf("r=%p type=%d time=%lu thread_id=%lu target_id=0x%lx\n",
	   r, r->type, r->time, r->thread_id, r->target_id);

    switch (r->type) {
    case ompt_callback_target:
      // case ompt_callback_target_emi:
      {
	ompt_record_target_t target_rec = r->record.target;
	printf("\tTarget task: kind=%d endpoint=%d device=%d task_id=%lu target_id=0x%lx codeptr=%p\n",
	       target_rec.kind, target_rec.endpoint, target_rec.device_num,
	       target_rec.task_id, target_rec.target_id, target_rec.codeptr_ra);
	break;
      }
    case ompt_callback_target_data_op:
      // case ompt_callback_target_data_op_emi:
      {
	ompt_record_target_data_op_t target_data_op_rec =
	  r->record.target_data_op;
	printf("\tTarget data op: host_op_id=%lu optype=%d src_addr=%p "
	       "src_device=%d dest_addr=%p dest_device=%d bytes=%lu "
	       "end_time=%lu duration=%luus codeptr=%p\n",
	       target_data_op_rec.host_op_id, target_data_op_rec.optype,
	       target_data_op_rec.src_addr, target_data_op_rec.src_device_num,
	       target_data_op_rec.dest_addr, target_data_op_rec.dest_device_num,
	       target_data_op_rec.bytes, target_data_op_rec.end_time,
	       target_data_op_rec.end_time - r->time,
	       target_data_op_rec.codeptr_ra);
	break;
      }
    case ompt_callback_target_submit:
      // case ompt_callback_target_submit_emi:
      {
	ompt_record_target_kernel_t target_kernel_rec = r->record.target_kernel;
	printf("\tTarget kernel: host_op_id=%lu requested_num_teams=%u "
	       "granted_num_teams=%u end_time=%lu duration=%luus\n",
	       target_kernel_rec.host_op_id,
	       target_kernel_rec.requested_num_teams,
	       target_kernel_rec.granted_num_teams, target_kernel_rec.end_time,
	       target_kernel_rec.end_time - r->time);
	break;
      }
    default:
      assert(0);
      break;
    }
  }
}


static ompt_device_t *
ompt_get_device
(
 int device_id
)
{
  ompt_device_entry_t *e = device_list;
  while (e) {
    if (e->device_id == device_id) return e->device;
    e = e->next;
  }
  return 0;
}


static void
ompt_finalize_flush
(
 void *arg,
 int how
)
{
  PRINT("ompt_finalize_flush enter\n");

  ompt_device_entry_t *e = device_list;
  while (e) {
    PRINT("ompt_finalize_flush flush id=%d device=%p\n",
	  e->device_id, e->device);
    if (ompt_need_flush) ompt_flush_trace(e->device);
    e = e->next;
  }

  gpu_application_thread_process_activities();

  PRINT("ompt_finalize_flush exit\n");
}


static void
ompt_finalize_shutdown
(
 void *arg,
 int how
)
{
  PRINT("ompt_finalize_shutdown enter\n");

  ompt_device_entry_t *e = device_list;
  while (e) {
    PRINT("ompt_finalize_flush flush id=%d device=%p\n",
	  e->device_id, e->device);
    ompt_stop_trace(e->device);
    e = e->next;
  }
  ompt_shutdown_complete = 1;
  gpu_application_thread_process_activities();
  PRINT("ompt_finalize_shutdown exit\n");
}


static void
ompt_finalize_trace
(
 void *arg,
 int how
)
{
  PRINT("ompt_finalize_trace enter\n");
  gpu_trace_fini(arg, how);
  PRINT("ompt_finalize_trace exit\n");
}



static void
ompt_buffer_complete
(
 int device_id,
 ompt_buffer_t *buffer,
 size_t bytes,
 ompt_buffer_cursor_t begin,
 int buffer_owned
)
{
  PRINT("ompt_callback_buffer_complete enter device=%d\n", device_id);
  if (ompt_shutdown_complete == 0) {

    gpu_monitoring_thread_activities_ready();

    ompt_device_t *device = ompt_get_device(device_id);

    // signal advance to return pointer to first record
    ompt_buffer_cursor_t current = begin;
    int status = 1;
    while (status) {
      // extract the next record from the buffer
      ompt_record_ompt_t *record = ompt_get_record_ompt(buffer, current);

      // a buffer may be empty, so the first record may be NULL
      if (record == NULL) break;

      // process the record
      ompt_activity_process(record);

      // advance the cursor to the next record
      // status will be 0 if there is no next record
      status = ompt_advance_buffer_cursor(device, buffer, bytes, current,
					  &current);
    }
  }

  if (buffer_owned) ompt_buffer_release(buffer);

  PRINT("ompt_callback_buffer_complete exit device=%d\n", device_id);
}


void
ompt_trace_configure(ompt_device_t *device)
{
  // indicate desired monitoring
  ompt_set_trace_ompt(device, 1, 0);

  // turn on monitoring previously indicated
  ompt_start_trace(device, ompt_buffer_request,
		   ompt_buffer_complete);
}


void
ompt_device_initialize(int device_num,
                       const char *type,
                       ompt_device_t *device,
                       ompt_function_lookup_t lookup,
                       const char *documentation)
{
  PRINT("ompt_device_initialize->%s, %d\n", type, device_num);

  ompt_bind_names(lookup);

  ompt_trace_configure(device);

  device_list_insert(device_num, device);
  ompt_device_map_insert(device_num, device, type);
}


void
ompt_device_finalize(int device_num)
{
  PRINT("ompt_device_finalize id=%d\n", device_num);
}


void
ompt_device_load(int device_num,
                 const char *filename,
                 int64_t file_offset,
                 const void *file_addr,
                 size_t bytes,
                 const void *host_addr,
                 const void *device_addr,
                 uint64_t module_id)
{
  PRINT("ompt_device_load->%s, %d\n", filename, device_num);

#if 0 // FIXME
  cupti_load_callback_cuda(module_id, host_addr, bytes);
#endif
}


void
ompt_device_unload(int device_num,
                   uint64_t module_id)
{
  //cubin_id_map_delete(module_id);
}


#if PRUNE_CALLPATH
static int
get_load_module
(
  cct_node_t *node
)
{
  cct_addr_t *addr = hpcrun_cct_addr(target_node);
  ip_normalized_t ip = addr->ip_norm;
  return ip.lm_id;
}
#endif


void
ompt_target_callback_emi
(
  ompt_target_t kind,
  ompt_scope_endpoint_t endpoint,
  int device_num,
  ompt_data_t *task_data,
  ompt_data_t *target_task_data,
  ompt_data_t *target_data,
  const void *codeptr_ra
)
{
  if (endpoint == ompt_scope_end) {
    target_node = NULL;
    return;
  }

  ompt_need_flush = true;

  target_data->value = gpu_correlation_id();
  PRINT("ompt_target_callback->target_id 0x%lx\n", target_data->value);

  // XXX(Keren): Do not use openmp callbacks to consume and produce records
  // HPCToolkit always subscribes its own cupti callback
  //
  //cupti_stop_flag_set();
  //cupti_correlation_channel_init();
  //cupti_activity_channel_init();
  //cupti_correlation_channel_consume();

  // sample a record
  hpcrun_metricVal_t zero_metric_incr = {.i = 0};
  int zero_metric_id = 0; // nothing to see here

  ucontext_t uc;
  getcontext(&uc);
  thread_data_t *td = hpcrun_get_thread_data();
  td->overhead++;
  // NOTE(keren): hpcrun_safe_enter prevent self interruption
  hpcrun_safe_enter();

  int skip_this_frame = 1; // omit this procedure frame on the call path
  target_node =
    hpcrun_sample_callpath(&uc, zero_metric_id, zero_metric_incr,
                           skip_this_frame, 1, NULL).sample_node;

#if PRUNE_CALLPATH
  // the load module for the runtime library that supports offloading
  int lm = get_load_module(target_node);

  // drop nodes on the call chain until we find one that is not in the load
  // module for runtime library that supports offloading
  for (;;) {
    target_node = hpcrun_cct_parent(target_node);
    if (get_load_module(target_node) != lm) break;
  }
#endif

  hpcrun_safe_exit();
  td->overhead--;
}

#define FOREACH_OMPT_DATA_OP(macro)				     \
  macro(op, ompt_target_data_alloc, ompt_tgt_alloc)		     \
  macro(op, ompt_target_data_delete, ompt_tgt_delete)		     \
  macro(op, ompt_target_data_transfer_to_device, ompt_tgt_copyin)    \
  macro(op, ompt_target_data_transfer_from_device, ompt_tgt_copyout) 

void
ompt_data_op_callback_emi
(
  ompt_scope_endpoint_t endpoint,
  ompt_data_t *target_task_data,
  ompt_data_t *target_data,
  ompt_id_t *host_op_id,
  ompt_target_data_op_t optype,
  void *src_addr,
  int src_device_num,
  void *dest_addr,
  int dest_device_num,
  size_t bytes,
  const void *codeptr_ra
)
{
  ompt_placeholder_t op = ompt_placeholders.ompt_tgt_none;
  switch (optype) {                       
#define ompt_op_macro(op, ompt_op_type, ompt_op_class) \
    case ompt_op_type:                                 \
      op = ompt_placeholders.ompt_op_class;                              \
      break;

    FOREACH_OMPT_DATA_OP(ompt_op_macro);

#undef ompt_op_macro
    default:
      break;
  }

  hpcrun_ompt_op_id_notify(endpoint, host_op_id, op.pc_norm);
}


void
ompt_submit_callback_emi
(
 ompt_scope_endpoint_t endpoint,
 ompt_data_t *target_data,
 ompt_id_t *host_op_id,
 unsigned int requested_num_teams
)
{
  PRINT("ompt_submit_callback enter->target_id %" PRIu64 "\n", target_id);
  hpcrun_ompt_op_id_notify(endpoint, host_op_id, ompt_placeholders.ompt_tgt_kernel.pc_norm);
  PRINT("ompt_submit_callback exit->target_id %" PRIu64 "\n", target_id);
}


void
ompt_map_callback(ompt_id_t target_id,
                  unsigned int nitems,
                  void **host_addr,
                  void **device_addr,
                  size_t *bytes,
                  unsigned int *mapping_flags)
{
  ompt_need_flush = true;
}


bool
ompt_runtime_status_get
(
 void
)
{
  return ompt_runtime_api_flag;
}


cct_node_t *
ompt_trace_node_get
(
 void
)
{
  return trace_node;
}

void
prepare_device
(
 void
)
{
  PRINT("ompt_initialize->prepare_device enter\n");

  device_finalizer_flush.fn = ompt_finalize_flush;
  device_finalizer_register(device_finalizer_type_flush,
			    &device_finalizer_flush);

  device_finalizer_shutdown.fn = ompt_finalize_shutdown;
  device_finalizer_register(device_finalizer_type_shutdown,
			    &device_finalizer_shutdown);

  device_finalizer_trace.fn = ompt_finalize_trace;
  device_finalizer_register(device_finalizer_type_shutdown,
			    &device_finalizer_trace);

  ompt_set_callback
    (ompt_callback_device_initialize, ompt_device_initialize);
  ompt_set_callback
    (ompt_callback_device_finalize, ompt_device_finalize);
  ompt_set_callback
    (ompt_callback_device_load, ompt_device_load);
  ompt_set_callback
    (ompt_callback_device_unload, ompt_device_unload);
  ompt_set_callback
    (ompt_callback_target_emi, ompt_target_callback_emi);
  ompt_set_callback
    (ompt_callback_target_data_op_emi, ompt_data_op_callback_emi);
  ompt_set_callback
    (ompt_callback_target_submit_emi, ompt_submit_callback_emi);
  ompt_set_callback
    (ompt_callback_target_map, ompt_map_callback);

  PRINT("ompt_initialize->prepare_device exit\n");
}

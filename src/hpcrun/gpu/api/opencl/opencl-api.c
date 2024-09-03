// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// system includes
//******************************************************************************

#define _GNU_SOURCE

#include <assert.h>
#include <inttypes.h>
#include <errno.h>     // errno
#include <fcntl.h>     // open
#include <pthread.h>   // pthread_once
#include <sys/stat.h>  // mkdir
#include <sys/types.h>
#include <unistd.h>
#include <math.h>      // pow



//******************************************************************************
// local includes
//******************************************************************************

#include "../../../safe-sampling.h"
#include "../../../sample-sources/libdl.h"
#include "../../activity/gpu-activity.h"
#include "../../activity/gpu-activity-channel.h"
#include "../../activity/gpu-activity-process.h"
#include "../common/gpu-binary.h"
#include "../../operation/gpu-operation-multiplexer.h"
#include "../../activity/correlation/gpu-correlation-channel.h"
#include "../../gpu-application-thread-api.h"
#include "../../gpu-monitoring-thread-api.h"
#include "../../activity/gpu-op-placeholders.h"
#ifdef ENABLE_GTPIN
#include "../intel/gtpin/gtpin-instrumentation.h"
#endif
#include "intel/papi/papi-metric-collector.h"
#include "../../../messages/messages.h"
#include "../../../sample-sources/libdl.h"
#include "../../blame-shifting/opencl/opencl-blame.h"
#include "../../../files.h"
#include "../../../utilities/hpcrun-nanotime.h"
#include "../../../../common/lean/crypto-hash.h"
#include "../../../../common/lean/elf-extract.h"
#include "../../../../common/lean/spinlock.h"
#include "../../../../common/lean/splay-uint64.h"
#include <stdatomic.h>
#include "../../../../common/lean/usec_time.h"

#include "opencl-api.h"
#include "opencl-api-wrappers.h"
#include "opencl-activity-translate.h"
#include "opencl-memory-manager.h"
#include "opencl-h2d-map.h"
#include "opencl-queue-map.h"
#include "opencl-context-map.h"
#include "opencl-kernel-loadmap-map.h"
#include "intel/optimization-check.h"

#include <CL/cl.h>


//******************************************************************************
// macros
//******************************************************************************

#define CPU_NANOTIME() (usec_time() * 1000)

#define opencl_path() "libOpenCL.so"

#define LINE_TABLE_FLAG " -gline-tables-only "

#define CORRELATION_ID_INVALID -1

#define BUFFER_ID_INVALID -1

#define DEFAULT_OPENCL_STREAM_ID 0

#define CL_PROGRAM_DEBUG_INFO_SIZES_INTEL 0x4101
#define CL_PROGRAM_DEBUG_INFO_INTEL       0x4100



//******************************************************************************
// local data
//******************************************************************************

static atomic_ullong opencl_h2d_pending_operations;
static atomic_uint correlation_id_counter = 0;
// Global pending operation count for all threads
static atomic_uint opencl_pending_operations = 0;

// The thread itself how many pending operations
static __thread atomic_int opencl_self_pending_operations = 0;
// Mark if a thread has invoked any opencl call
// If yes, we can flush all opencl activities when the thread terminates
static __thread bool opencl_api_flag = false;

static spinlock_t opencl_h2d_lock = SPINLOCK_UNLOCKED;

#ifdef ENABLE_GTPIN
static bool gtpin_instrumentation = false;
#endif

static bool optimization_check = false;
static atomic_uint total_num_devices = 0;
static bool ENABLE_BLAME_SHIFTING = false;

static bool gpu_utilization_enabled = false;


//******************************************************************************
// private operations
//******************************************************************************

static void
opencl_write_debug_binary
(
 unsigned char *binary,
 size_t size
)
{

  unsigned char *section;
  size_t section_size;

#define INTEL_CL_DEBUG_SECTION_NAME "Intel(R) OpenCL Device Debug"
  if (elf_section_info(binary, size, INTEL_CL_DEBUG_SECTION_NAME, &section, &section_size)) {
    const char *buf = (const char *) section; // avoid warning; all bits will be written
    uint32_t loadmap_module_id;
    gpu_binary_save(buf, section_size, true /* mark_used */, &loadmap_module_id);
  }
}


static cl_uint
opencl_device_count
(
  typeof(&clGetProgramInfo) pfn_clGetProgramInfo,
  cl_program program
)
{
  cl_uint num_devices;

  cl_int ret = pfn_clGetProgramInfo(program, CL_PROGRAM_NUM_DEVICES,
                                    sizeof(cl_uint), &num_devices, 0);

  if (ret != CL_SUCCESS) num_devices = 0;

  return num_devices;
}


static cl_uint
opencl_binary_sizes
(
  typeof(&clGetProgramInfo) pfn_clGetProgramInfo,
  cl_program program,
  cl_int device_count,
  size_t *binary_sizes
)
{
  cl_int ret = pfn_clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES,
                                    device_count * sizeof(size_t), binary_sizes, 0);
  return ret;
}


static bool
opencl_binaries
(
  typeof(&clGetProgramInfo) pfn_clGetProgramInfo,
  cl_program program,
  cl_uint *_num_devices,
  unsigned char ***_binaries,
  size_t **_binary_sizes
)
{
  // variable initializations needed for error handling
  cl_uint i = 0;
  unsigned char **binaries = 0;
  size_t *binary_sizes = 0;

  cl_uint num_devices = opencl_device_count(pfn_clGetProgramInfo, program);

  if (num_devices > 0) {
    binary_sizes = (size_t *) malloc(sizeof(size_t) * num_devices);

    if (binary_sizes == 0) goto no_binary_sizes;

    cl_uint ret_code = opencl_binary_sizes(pfn_clGetProgramInfo, program, num_devices, binary_sizes);
    if (ret_code == CL_SUCCESS) {
      binaries = (unsigned char **) malloc(sizeof(size_t) * num_devices);
      if (binaries == 0) goto no_binaries;

      for(i = 0; i < num_devices; i++){
        if (binary_sizes[i] > 0) {
          binaries[i] = (unsigned char *) malloc(binary_sizes[i]);
          if (binaries[i] == 0) goto not_all_binaries;
        } else {
          binaries[i] = 0;
        }
      }
    }
  }

  cl_int ret = pfn_clGetProgramInfo(program, CL_PROGRAM_BINARIES,
                                    num_devices * sizeof(unsigned char *), binaries, 0);

  if (ret != CL_SUCCESS) goto error;

  *_num_devices = num_devices;
  *_binaries = binaries;
  *_binary_sizes = binary_sizes;

  return true;

 error:
  i = num_devices - 1;

 not_all_binaries:
  for(; i >= 0; i--){
    if (binaries[i]) free(binaries[i]);
  }
  if (binaries) free(binaries);

 no_binaries:
  if (binary_sizes) free(binary_sizes);

 no_binary_sizes:

  return false;
}


static uint32_t
getCorrelationId
(
 void
)
{
  return atomic_fetch_add(&correlation_id_counter, 1);
}


static void
initializeKernelCallBackInfo
(
 opencl_object_t *ker_info,
 cl_command_queue command_queue,
 uint32_t module_id
)
{
  opencl_queue_map_entry_t *qe = opencl_cl_queue_map_lookup((uint64_t)command_queue);
  uint32_t context_id = opencl_cl_queue_map_entry_context_id_get(qe);
  uint32_t queue_id = opencl_cl_queue_map_entry_queue_id_get(qe);

  ETMSG(OPENCL, "submit kernel to context %" PRIu32 " queue %" PRIu32 "\n", context_id, queue_id);

  ker_info->details.context_id = context_id;
  ker_info->details.stream_id = queue_id;
  ker_info->details.module_id = module_id;
  ker_info->pending_operations = &opencl_self_pending_operations;
}


static void
initializeMemcpyCallBackInfo
(
 opencl_object_t *cpy_info,
 gpu_memcpy_type_t type,
 size_t size,
 cl_command_queue command_queue
)
{
  opencl_queue_map_entry_t *qe = opencl_cl_queue_map_lookup((uint64_t)command_queue);
  uint32_t context_id = opencl_cl_queue_map_entry_context_id_get(qe);
  uint32_t queue_id = opencl_cl_queue_map_entry_queue_id_get(qe);

  ETMSG(OPENCL, "submit memcpy to context %u queue %u\n", context_id, queue_id);

  cpy_info->kind = GPU_ACTIVITY_MEMCPY;
  cpy_info->details.cpy_cb.type = type;
  cpy_info->details.cpy_cb.fromHostToDevice = (type == GPU_MEMCPY_H2D);
  cpy_info->details.cpy_cb.fromDeviceToHost = (type == GPU_MEMCPY_D2H);
  cpy_info->details.cpy_cb.size = size;
  cpy_info->details.context_id = context_id;
  cpy_info->details.stream_id = queue_id;
  cpy_info->pending_operations = &opencl_self_pending_operations;
}


static void
initializeMemoryCallBackInfo
(
 opencl_object_t *mem_info,
 cl_mem_flags flags,
 size_t size
)
{
  mem_info->kind = GPU_ACTIVITY_MEMORY;
  if (flags & CL_MEM_USE_HOST_PTR) {
    // Managed by the host
    mem_info->details.mem_cb.type = GPU_MEM_MANAGED;
  } else if (flags & CL_MEM_ALLOC_HOST_PTR) {
    // Use host memory
    mem_info->details.mem_cb.type = GPU_MEM_PINNED;
  } else {
    // Normal
    mem_info->details.mem_cb.type = GPU_MEM_DEVICE;
  }

  mem_info->details.mem_cb.size = size;
}


#define INITIALIZE_CALLBACK_INFO(f, obj, args) \
  f args; \
  obj->pending_operations = &opencl_self_pending_operations;


#define SET_EVENT_POINTER(eventp, event, obj) \
  cl_event my_event; \
  if (!event) { \
    obj->internal_event = true; \
    eventp = &my_event; \
  } else { \
    eventp = event; \
    obj->internal_event = false; \
  }


// we are dumping the debuginfo since the binary does not have debugsection
static void
clBuildProgramCallback
(
 cl_program program,
 void* user_data
)
{
}


static void __attribute__((unused))
opencl_h2d_pending_operations_adjust
(
 int value
)
{
  atomic_fetch_add(&opencl_h2d_pending_operations, value);
}


static void
opencl_operation_multiplexer_push
(
 gpu_interval_t interval,
 opencl_object_t *obj,
 uint32_t correlation_id
)
{
  gpu_activity_t gpu_activity;
  memset(&gpu_activity, 0, sizeof(gpu_activity_t));

  // The actual entry
  opencl_activity_translate(&gpu_activity, obj, interval);
  gpu_operation_multiplexer_push(obj->details.initiator_channel,
    obj->pending_operations, &gpu_activity);
}


static void
opencl_activity_process
(
  typeof(&clGetEventProfilingInfo) pfn_clGetEventProfilingInfo,
  cl_event event,
  opencl_object_t *obj,
  uint32_t correlation_id
)
{
  gpu_interval_t interval;
  memset(&interval, 0, sizeof(gpu_interval_t));
  opencl_timing_info_get(pfn_clGetEventProfilingInfo, &interval, event);

  opencl_operation_multiplexer_push(interval, obj, correlation_id);
}


static void __attribute__((unused))
opencl_clSetKernelArg_activity_process
(
 opencl_h2d_map_entry_t *entry,
 opencl_object_t *cb_data
)
{
  gpu_activity_t gpu_activity;
  memset(&gpu_activity, 0, sizeof(gpu_activity_t));

  uint32_t correlation_id = opencl_h2d_map_entry_correlation_get(entry);
  opencl_h2d_map_entry_size_get(entry);
  cb_data->details.ker_cb.correlation_id = correlation_id;

  gpu_interval_t interval;
  memset(&interval, 0, sizeof(gpu_interval_t));

  opencl_activity_translate(&gpu_activity, cb_data, interval);

  ETMSG(OPENCL, "cb_data->details.initiator_channel: %p", cb_data->details.initiator_channel);
  gpu_operation_multiplexer_push(cb_data->details.initiator_channel,
    cb_data->pending_operations, &gpu_activity);
}


static uint64_t __attribute__((unused))
opencl_get_buffer_id
(
 const void *arg
)
{
  if (arg != NULL) {
    cl_mem buffer = *(cl_mem*)arg;
    return (uint64_t)buffer;
  } else {
    return BUFFER_ID_INVALID;
  }
}


static bool __attribute__((unused))
opencl_isClArgBuffer
(
 const void *arg
)
{
  /*
   * There are 2 scenarios in which opencl_isClArgBuffer will return false
   * 1. When clCreateBuffer was not called for arg before calling clSetKernelArg
   * 2. clEnqueueWriteBuffer is being called for arg. We shouldn't be recording duplicate H2D calls
   * */
  uint64_t buffer_id = opencl_get_buffer_id(arg);
  bool isBuffer;
  if (buffer_id == BUFFER_ID_INVALID) {
    isBuffer = false;
  } else {
    opencl_h2d_map_entry_t *entry = opencl_h2d_map_lookup(buffer_id);
    isBuffer = entry ? true : false;
  }
  //ETMSG(OPENCL, "opencl_isClArgBuffer. buffer_id: %"PRIu64". isBuffer: %d",  buffer_id, isBuffer);
  return isBuffer;
}


static void __attribute__((unused))
add_H2D_metrics_to_cct_node
(
 opencl_h2d_map_entry_t *entry,
 splay_visit_t visit_type,
 void *arg
)
{
  opencl_object_t *cb_data = opencl_h2d_map_entry_callback_info_get(entry);
  cl_basic_callback_t cb_basic = opencl_cb_basic_get(cb_data);
  opencl_cb_basic_print(cb_basic, "Completion_Callback");

  opencl_clSetKernelArg_activity_process(entry, cb_data);
  opencl_h2d_map_entry_buffer_id_get(entry);
  opencl_h2d_pending_operations_adjust(-1);
}


static void __attribute__((unused))
opencl_update_ccts_for_setClKernelArg
(
 void
)
{
  spinlock_lock(&opencl_h2d_lock);
  opencl_h2d_map_count();
  if (atomic_load(&opencl_h2d_pending_operations) > 0) {
    opencl_update_ccts_for_h2d_nodes(add_H2D_metrics_to_cct_node);
  }
  spinlock_unlock(&opencl_h2d_lock);
}


static void __attribute__((unused))
opencl_wait_for_non_clSetKernelArg_pending_operations
(
 void
)
{
  ETMSG(OPENCL, "pending h2D operations: %lu", atomic_load(&opencl_h2d_pending_operations));
  while (atomic_load(&opencl_pending_operations) != atomic_load(&opencl_h2d_pending_operations));
}


static void
opencl_wait_for_self_pending_operations
(
 void
)
{
  ETMSG(OPENCL, "pending self operations: %lu", atomic_load(&opencl_self_pending_operations));
  while (atomic_load(&opencl_self_pending_operations) != 0);
}


static void __attribute__((unused))
opencl_wait_for_all_pending_operations
(
 void
)
{
  ETMSG(OPENCL, "pending operations: %lu", atomic_load(&opencl_pending_operations));
  while (atomic_load(&opencl_pending_operations) != 0);
}


static bool
is_opencl_blame_shifting_enabled
(
 void
)
{
  return (ENABLE_BLAME_SHIFTING == true);
}



//******************************************************************************
// interface operations
//******************************************************************************

cl_basic_callback_t
opencl_cb_basic_get
(
 opencl_object_t *cb_data
)
{
  cl_basic_callback_t cb_basic;

  if (cb_data->kind == GPU_ACTIVITY_KERNEL) {
    cb_basic.correlation_id = cb_data->details.ker_cb.correlation_id;
    cb_basic.kind = cb_data->kind;
    cb_basic.type = 0; // not valid

  } else if (cb_data->kind == GPU_ACTIVITY_MEMCPY) {
    cb_basic.correlation_id = cb_data->details.mem_cb.correlation_id;
    cb_basic.kind = cb_data->kind;
    cb_basic.type = cb_data->details.mem_cb.type;
  }
  cb_basic.cct_node = cb_data->details.cct_node;

  return cb_basic;
}


void
opencl_cb_basic_print
(
 cl_basic_callback_t cb_basic,
 char *title
)
{
  ETMSG(OPENCL, "%s | Activity kind: %s | type: %s | correlation id: %"PRIu64 "| cct_node = %p",
        title,
        gpu_kind_to_string(cb_basic.kind),
        gpu_type_to_string(cb_basic.type),
        cb_basic.correlation_id,
        cb_basic.cct_node);
}


void
opencl_subscriber_callback
(
 opencl_object_t *obj
)
{
  // We invoked an opencl operation
  opencl_api_flag = true;

  uint32_t correlation_id = getCorrelationId();

  // Init operations
  atomic_fetch_add(obj->pending_operations, 1);

  gpu_placeholder_type_t placeholder_type = gpu_placeholder_type_count;
  gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;

  switch (obj->kind) {

    case GPU_ACTIVITY_MEMCPY:
      {
        obj->details.cpy_cb.correlation_id = correlation_id;
        if (obj->details.cpy_cb.type == GPU_MEMCPY_H2D){
          gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
            gpu_placeholder_type_copyin);

          placeholder_type = gpu_placeholder_type_copyin;

        } else if (obj->details.cpy_cb.type == GPU_MEMCPY_D2H){
          gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
            gpu_placeholder_type_copyout);

          placeholder_type = gpu_placeholder_type_copyout;
        }
        break;
      }

    case GPU_ACTIVITY_KERNEL:
      {
        obj->details.ker_cb.correlation_id = correlation_id;
        gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
          gpu_placeholder_type_kernel);

        gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
          gpu_placeholder_type_trace);

        placeholder_type = gpu_placeholder_type_kernel;

        break;
      }

    case GPU_ACTIVITY_MEMORY:
      {
        obj->details.mem_cb.correlation_id = correlation_id;
        gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
          gpu_placeholder_type_alloc);

        placeholder_type = gpu_placeholder_type_alloc;
        break;
      }

    default:
      assert(false && "Invalid activity kind!");
      hpcrun_terminate();
  }

  cct_node_t *api_node =
    gpu_application_thread_correlation_callback(correlation_id);
  gpu_op_ccts_t gpu_op_ccts;

  hpcrun_safe_enter();
  gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
  cct_node_t *cct_ph = gpu_op_ccts_get(&gpu_op_ccts, placeholder_type);
  hpcrun_safe_exit();

  if (obj->details.module_id != LOADMAP_INVALID_MODULE_ID &&
    obj->kind == GPU_ACTIVITY_KERNEL && (hpcrun_cct_children(cct_ph) == NULL)) {
    ip_normalized_t kernel_ip;
    kernel_ip.lm_id = (uint16_t) obj->details.module_id;
    kernel_ip.lm_ip = 0;  // offset=0
    cct_node_t *kernel =
      hpcrun_cct_insert_ip_norm(cct_ph, kernel_ip, true);
    hpcrun_cct_retain(kernel);
  }

  gpu_application_thread_process_activities();

  obj->details.cct_node = cct_ph;
  obj->details.initiator_channel = gpu_activity_channel_get_local();
  obj->details.submit_time = CPU_NANOTIME();

#ifdef ENABLE_GTPIN
  if (obj->kind == GPU_ACTIVITY_KERNEL && gtpin_instrumentation) {
    // Callback to produce gtpin correlation
    gtpin_produce_runtime_callstack(&gpu_op_ccts);
  }
#endif
}


void
opencl_activity_completion_callback
(
 cl_event event,
 cl_int event_command_exec_status,
 void *user_data
)
{
  hpcrun_thread_init_mem_pool_once(TOOL_THREAD_ID, NULL, HPCRUN_NO_TRACE, true);
  opencl_object_t *cb_data = (opencl_object_t*)user_data;
  cl_basic_callback_t cb_basic = opencl_cb_basic_get(cb_data);

  if (event_command_exec_status == CL_COMPLETE) {
    opencl_cb_basic_print(cb_basic, "Completion_Callback");
    opencl_activity_process(cb_data->pfn_clGetEventProfilingInfo, event, cb_data, cb_basic.correlation_id);
  }

  if (is_opencl_blame_shifting_enabled() && cb_data->kind == GPU_ACTIVITY_KERNEL && event_command_exec_status == CL_COMPLETE) {
                opencl_kernel_epilogue(cb_data->pfn_clGetEventProfilingInfo, event);
        }

        if (cb_data->internal_event) {
    ((typeof(&clReleaseEvent))cb_data->pfn_clReleaseEvent)(event);
  }

  // Finish operations
  opencl_free(cb_data);
}


void
opencl_timing_info_get
(
  typeof(&clGetEventProfilingInfo) pfn_clGetEventProfilingInfo,
  gpu_interval_t *interval,
  cl_event event
)
{
  cl_ulong commandStart = 0;
  cl_ulong commandEnd = 0;

  pfn_clGetEventProfilingInfo
         (event, CL_PROFILING_COMMAND_START,
          sizeof(commandStart), &commandStart, NULL);

  pfn_clGetEventProfilingInfo
         (event, CL_PROFILING_COMMAND_END,
          sizeof(commandEnd), &commandEnd, NULL);

  ETMSG(OPENCL, "duration [%lu, %lu]", commandStart, commandEnd);

  gpu_interval_set(interval, (uint64_t)commandStart, (uint64_t)commandEnd);
}


void
opencl_api_initialize
(
 gpu_instrumentation_t *inst_options
)
{
  ETMSG(OPENCL, "CL_TARGET_OPENCL_VERSION: %d", CL_TARGET_OPENCL_VERSION);
  // we need this even when instrumentation is off inorder to get kernel names in hpcviewer
  hpcrun_thread_init_mem_pool_once(TOOL_THREAD_ID, NULL, HPCRUN_NO_TRACE, true);

  // GPU_IDLE_CAUSE should be attributed only to application thread
  // application thread calls this fn
  thread_data_t* td   = hpcrun_get_thread_data();
  td->application_thread_0 = true;

  if (gpu_instrumentation_enabled(inst_options)) {
#ifdef ENABLE_GTPIN
    gtpin_instrumentation = true;
    gtpin_instrumentation_options(inst_options);
#endif
  }

  atomic_store(&correlation_id_counter, 0);
  atomic_store(&opencl_pending_operations, 0);
  atomic_store(&opencl_h2d_pending_operations, 0);
}


cl_int
foilbase_clBuildProgram
(
  typeof(&clBuildProgram) pfn_real,
  typeof(&clGetProgramInfo) pfn_clGetProgramInfo,
  cl_program program,
  cl_uint num_devices,
  const cl_device_id* device_list,
  const char* options,
  void (CL_CALLBACK* pfn_notify)(cl_program program, void* user_data),
  void* user_data
)
{
  ETMSG(OPENCL, "inside clBuildProgram_wrapper");
  // XXX(Aaron): Caution, what's the maximum length of options?
  size_t len_options = options == NULL ? 0 : strlen(options);
  size_t len_flag = strlen(LINE_TABLE_FLAG);
  char *options_with_debug_flags = malloc(len_options + len_flag + 1);
  memset(options_with_debug_flags, 0, len_options + len_flag + 1);
  if (options != NULL) {
    strcat(options_with_debug_flags, options);
  }
  cl_int ret = pfn_real(program, num_devices, device_list,
                        options_with_debug_flags, clBuildProgramCallback,
                        user_data);
  free(options_with_debug_flags);

  {
    cl_uint num_devices;
    unsigned char **binaries;
    size_t *binary_sizes;
    if (opencl_binaries(pfn_clGetProgramInfo, program, &num_devices, &binaries, &binary_sizes)) {
      for (cl_uint i = 0; i < num_devices; i++) {
        if (binary_sizes[i] > 0) {
          opencl_write_debug_binary(binaries[i], binary_sizes[i]);
          free(binaries[i]);
        }
      }
      free(binary_sizes);
      free(binaries);
    }
  }

  return ret;
}


cl_context
foilbase_clCreateContext
(
  typeof(&clCreateContext) pfn_real,
  typeof(&clGetPlatformIDs) pfn_clGetPlatformIDs,
  typeof(&clGetDeviceIDs) pfn_clGetDeviceIDs,
  const cl_context_properties *properties,
  cl_uint num_devices,
  const cl_device_id *devices,
  void (CL_CALLBACK* pfn_notify) (const char *errinfo, const void *private_info, size_t cb, void *user_data),
  void *user_data,
  cl_int *errcode_ret
)
{
  ETMSG(OPENCL, "inside clCreateContext wrapper");
  cl_context context = pfn_real(properties, num_devices, devices, pfn_notify, user_data, errcode_ret);
  if (optimization_check && *errcode_ret == CL_SUCCESS) {
    recordDeviceCount(num_devices, devices);

    cl_uint platformCount;
    cl_platform_id* platforms;
    cl_uint platformDeviceCount;
    unsigned int num_devices = 0;

    pfn_clGetPlatformIDs(0, NULL, &platformCount);
    platforms = (cl_platform_id*) malloc(sizeof(cl_platform_id) * platformCount);
    pfn_clGetPlatformIDs(platformCount, platforms, NULL);

    for (int i = 0; i < platformCount; i++) {
      pfn_clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, 0, NULL, &platformDeviceCount);
      num_devices += platformDeviceCount;
    }
    atomic_store_explicit(&total_num_devices, num_devices, memory_order_release);
  }
  return context;
}


cl_command_queue
foilbase_clCreateCommandQueue
(
  typeof(&clCreateCommandQueue) pfn_real,
  cl_context context,
  cl_device_id device,
  cl_command_queue_properties properties,
  cl_int *errcode_ret
)
{
  // enabling profiling
  properties |= (cl_command_queue_properties)CL_QUEUE_PROFILING_ENABLE;

  cl_command_queue queue = pfn_real(context, device, properties,errcode_ret);

  if (optimization_check && *errcode_ret == CL_SUCCESS) {
    isQueueInInOrderExecutionMode(&properties);
  }

  uint32_t context_id = opencl_cl_context_map_update((uint64_t)context);
  opencl_cl_queue_map_update((uint64_t)queue, context_id);
  if (optimization_check && *errcode_ret == CL_SUCCESS) {
    recordQueueContext(queue, context);
  }

        if(is_opencl_blame_shifting_enabled()) {
                opencl_queue_prologue(queue);
        }
  return queue;
}


cl_command_queue
foilbase_clCreateCommandQueueWithProperties
(
  typeof(&clCreateCommandQueueWithProperties) pfn_real,
  cl_context context,
  cl_device_id device,
  const cl_queue_properties* properties,
  cl_int* errcode_ret
)
{
  cl_queue_properties *queue_properties = (cl_queue_properties *)properties;
  if (properties == NULL) {
    queue_properties = (cl_queue_properties *)malloc(sizeof(cl_queue_properties) * 3);
    queue_properties[0] = CL_QUEUE_PROPERTIES;
    queue_properties[1] = CL_QUEUE_PROFILING_ENABLE;
    queue_properties[2] = 0;
  } else {
    int queue_props_id = -1;
    int props_count = 0;
    while (properties[props_count] != 0) {
      if (properties[props_count] == CL_QUEUE_PROPERTIES) {
        queue_props_id = props_count;
        ++props_count;
      } else if (properties[props_count] == CL_QUEUE_SIZE) {
        ++props_count;
      }
      ++props_count;
    }

    if (queue_props_id >= 0 && queue_props_id + 1 < props_count) {
      queue_properties = (cl_queue_properties *)malloc(sizeof(cl_queue_properties) * (props_count + 1));
      for (int i = 0; i < props_count; ++i) {
        queue_properties[i] = properties[i];
      }
      // We do have a queue property entry, just enable profiling
      queue_properties[queue_props_id + 1] |= CL_QUEUE_PROFILING_ENABLE;
      queue_properties[props_count] = 0;
    } else {
      // We do not have a queue property entry, need to allocate a queue property entry and set up
      queue_properties = (cl_queue_properties *)malloc(sizeof(cl_queue_properties) * (props_count + 3));
      for (int i = 0; i < props_count; ++i) {
        queue_properties[i] = properties[i];
      }
      queue_properties[props_count] = CL_QUEUE_PROPERTIES;
      queue_properties[props_count + 1] = CL_QUEUE_PROFILING_ENABLE;
      queue_properties[props_count + 2] = 0;
    }
  }
  cl_command_queue queue = pfn_real(context, device, queue_properties, errcode_ret);

  if (optimization_check && *errcode_ret == CL_SUCCESS) {
    isQueueInInOrderExecutionMode(properties);
  }

  if (queue_properties != NULL) {
    // The property is created by us
    free(queue_properties);
  }

  uint32_t context_id = opencl_cl_context_map_update((uint64_t)context);
  opencl_cl_queue_map_update((uint64_t)queue, context_id);
  if (optimization_check && *errcode_ret == CL_SUCCESS) {
    recordQueueContext(queue, context);
  }

        if(is_opencl_blame_shifting_enabled()) {
                opencl_queue_prologue(queue);
        }

  if (optimization_check && *errcode_ret == CL_SUCCESS) {
    recordQueueContext(queue, context);
  }
  return queue;
}


static uint32_t
getKernelModuleId
(
  typeof(&clGetKernelInfo) pfn_clGetKernelInfo,
  cl_kernel ocl_kernel
)
{
  size_t kernel_name_size;
  cl_int status = pfn_clGetKernelInfo (ocl_kernel, CL_KERNEL_FUNCTION_NAME, 0, NULL, &kernel_name_size);
  char kernel_name[kernel_name_size];
  status = pfn_clGetKernelInfo (ocl_kernel, CL_KERNEL_FUNCTION_NAME, kernel_name_size, kernel_name, NULL);
  if (status != CL_SUCCESS)
    hpcrun_terminate();
  uint64_t kernel_name_id = get_numeric_hash_id_for_string(kernel_name, kernel_name_size);
  opencl_kernel_loadmap_map_entry_t *e = opencl_kernel_loadmap_map_lookup(kernel_name_id);
  uint32_t module_id = LOADMAP_INVALID_MODULE_ID;
  if (e) {
    module_id = opencl_kernel_loadmap_map_entry_module_id_get(e);
  }
  return module_id;
}


cl_int
foilbase_clEnqueueNDRangeKernel
(
  typeof(&clEnqueueNDRangeKernel) pfn_real,
  typeof(&clGetKernelInfo) pfn_clGetKernelInfo,
  typeof(&clRetainEvent) pfn_clRetainEvent,
  typeof(&clGetEventProfilingInfo) pfn_clGetEventProfilingInfo,
  typeof(&clReleaseEvent) pfn_clReleaseEvent,
  typeof(&clSetEventCallback) pfn_clSetEventCallback,
  cl_command_queue command_queue,
  cl_kernel ocl_kernel,
  cl_uint work_dim,
  const size_t *global_work_offset,
  const size_t *global_work_size,
  const size_t *local_work_size,
  cl_uint num_events_in_wait_list,
  const cl_event *event_wait_list,
  cl_event *event
)
{
  uint32_t module_id = getKernelModuleId(pfn_clGetKernelInfo, ocl_kernel);

  opencl_object_t *kernel_info = opencl_malloc_kind(pfn_clGetEventProfilingInfo, pfn_clReleaseEvent, GPU_ACTIVITY_KERNEL);
  INITIALIZE_CALLBACK_INFO(initializeKernelCallBackInfo, kernel_info, (kernel_info, command_queue, module_id))


  cl_event *eventp = NULL;
  SET_EVENT_POINTER(eventp, event, kernel_info)

  cl_int return_status = pfn_real(command_queue, ocl_kernel, work_dim,
                                global_work_offset, global_work_size, local_work_size,
                                num_events_in_wait_list, event_wait_list, eventp);
  if (optimization_check && return_status == CL_SUCCESS) {
    isKernelSubmittedToMultipleQueues(ocl_kernel, command_queue);
    areKernelParamsAliased(ocl_kernel, module_id);
  }

  opencl_subscriber_callback(kernel_info);

        if(is_opencl_blame_shifting_enabled()) {
                opencl_kernel_prologue(pfn_clRetainEvent, *eventp, module_id);
        }

  ETMSG(OPENCL, "Registering callback for kind: Kernel. "
                "Correlation id: %"PRIu64 "", kernel_info->details.ker_cb.correlation_id);

  pfn_clSetEventCallback(*eventp, CL_COMPLETE, &opencl_activity_completion_callback, kernel_info);

  return return_status;
}


// this is a simplified version of clEnqueueNDRangeKernel, TODO: check if code duplication can be avoided
cl_int
foilbase_clEnqueueTask
(
  typeof(&clEnqueueTask) pfn_real,
  typeof(&clGetKernelInfo) pfn_clGetKernelInfo,
  typeof(&clGetEventProfilingInfo) pfn_clGetEventProfilingInfo,
  typeof(&clReleaseEvent) pfn_clReleaseEvent,
  typeof(&clSetEventCallback) pfn_clSetEventCallback,
  cl_command_queue command_queue,
  cl_kernel kernel,
  cl_uint num_events_in_wait_list,
  const cl_event* event_wait_list,
  cl_event* event
)
{
  uint32_t module_id = getKernelModuleId(pfn_clGetKernelInfo, kernel);

  opencl_object_t *kernel_info = opencl_malloc_kind(pfn_clGetEventProfilingInfo, pfn_clReleaseEvent, GPU_ACTIVITY_KERNEL);
  INITIALIZE_CALLBACK_INFO(initializeKernelCallBackInfo, kernel_info, (kernel_info, command_queue, module_id))

  opencl_subscriber_callback(kernel_info);

  cl_event *eventp = NULL;
  SET_EVENT_POINTER(eventp, event, kernel_info);

  cl_int return_status = pfn_real(command_queue, kernel,
                                num_events_in_wait_list, event_wait_list, eventp);
  if (optimization_check && return_status == CL_SUCCESS) {
    isKernelSubmittedToMultipleQueues(kernel, command_queue);
    areKernelParamsAliased(kernel, module_id);
  }

  ETMSG(OPENCL, "Registering callback for kind: Kernel. "
                "Correlation id: %"PRIu64 "", kernel_info->details.ker_cb.correlation_id);

  pfn_clSetEventCallback(*eventp, CL_COMPLETE, &opencl_activity_completion_callback, kernel_info);

  return return_status;
}


cl_int
foilbase_clEnqueueReadBuffer
(
  typeof(&clEnqueueReadBuffer) pfn_real,
  typeof(&clGetEventProfilingInfo) pfn_clGetEventProfilingInfo,
  typeof(&clReleaseEvent) pfn_clReleaseEvent,
  typeof(&clSetEventCallback) pfn_clSetEventCallback,
  cl_command_queue command_queue,
  cl_mem buffer,
  cl_bool blocking_read,
  size_t offset,
  size_t cb,
  void *ptr,
  cl_uint num_events_in_wait_list,
  const cl_event *event_wait_list,
  cl_event *event
)
{
  ETMSG(OPENCL, "inside clEnqueueReadBuffer wrapper");

  opencl_object_t *cpy_info = opencl_malloc_kind(pfn_clGetEventProfilingInfo, pfn_clReleaseEvent, GPU_ACTIVITY_MEMCPY);
  INITIALIZE_CALLBACK_INFO(initializeMemcpyCallBackInfo, cpy_info, (cpy_info, GPU_MEMCPY_D2H, cb, command_queue))

  opencl_subscriber_callback(cpy_info);

  cl_event *eventp = NULL;
  SET_EVENT_POINTER(eventp, event, cpy_info);

  cl_int return_status = pfn_real(command_queue, buffer, blocking_read, offset,
       cb, ptr, num_events_in_wait_list, event_wait_list, eventp);
  if (optimization_check && return_status == CL_SUCCESS) {
    recordD2HCall(buffer);
  }

  ETMSG(OPENCL, "Registering callback for kind MEMCPY, type: D2H. "
                "Correlation id: %"PRIu64 "", cpy_info->details.cpy_cb.correlation_id);
  ETMSG(OPENCL, "%d(bytes) of data being transferred from device to host",
        (long)cb);


  pfn_clSetEventCallback(*eventp, CL_COMPLETE, &opencl_activity_completion_callback, cpy_info);

  return return_status;
}


cl_int
foilbase_clEnqueueWriteBuffer
(
  typeof(&clEnqueueWriteBuffer) pfn_real,
  typeof(&clGetEventProfilingInfo) pfn_clGetEventProfilingInfo,
  typeof(&clReleaseEvent) pfn_clReleaseEvent,
  typeof(&clSetEventCallback) pfn_clSetEventCallback,
  cl_command_queue command_queue,
  cl_mem buffer,
  cl_bool blocking_write,
  size_t offset,
  size_t cb,
  const void *ptr,
  cl_uint num_events_in_wait_list,
  const cl_event *event_wait_list,
  cl_event *event
)
{
  ETMSG(OPENCL, "inside clEnqueueWriteBuffer wrapper. cl_mem buffer: %p", buffer);
  opencl_object_t *cpy_info = opencl_malloc_kind(pfn_clGetEventProfilingInfo, pfn_clReleaseEvent, GPU_ACTIVITY_MEMCPY);
  INITIALIZE_CALLBACK_INFO(initializeMemcpyCallBackInfo, cpy_info, (cpy_info, GPU_MEMCPY_H2D, cb, command_queue))

  opencl_subscriber_callback(cpy_info);

  cl_event *eventp = NULL;
  SET_EVENT_POINTER(eventp, event, cpy_info);

  cl_int return_status = pfn_real(command_queue, buffer, blocking_write, offset, cb, ptr,
                          num_events_in_wait_list, event_wait_list, eventp);
  if (optimization_check && return_status == CL_SUCCESS) {
    recordH2DCall(buffer);
  }

  ETMSG(OPENCL, "Registering callback for kind MEMCPY, type: H2D. "
                "Correlation id: %"PRIu64 "", cpy_info->details.cpy_cb.correlation_id);
  ETMSG(OPENCL, "%d(bytes) of data being transferred from host to device",
        (long)cb);

  pfn_clSetEventCallback(*eventp, CL_COMPLETE, &opencl_activity_completion_callback, cpy_info);

  return return_status;
}


void*
foilbase_clEnqueueMapBuffer
(
  typeof(&clEnqueueMapBuffer) pfn_real,
  typeof(&clGetEventProfilingInfo) pfn_clGetEventProfilingInfo,
  typeof(&clReleaseEvent) pfn_clReleaseEvent,
  typeof(&clSetEventCallback) pfn_clSetEventCallback,
  cl_command_queue command_queue,
  cl_mem buffer,
  cl_bool blocking_map,
  cl_map_flags map_flags,
  size_t offset,
  size_t size,
  cl_uint num_events_in_wait_list,
  const cl_event* event_wait_list,
  cl_event* event,
  cl_int* errcode_ret
)
{
  ETMSG(OPENCL, "inside clEnqueueMapBuffer wrapper");

  opencl_object_t *cpy_info = opencl_malloc_kind(pfn_clGetEventProfilingInfo, pfn_clReleaseEvent, GPU_ACTIVITY_MEMCPY);
  if (map_flags == CL_MAP_READ) {
    INITIALIZE_CALLBACK_INFO(initializeMemcpyCallBackInfo, cpy_info, (cpy_info, GPU_MEMCPY_D2H, size, command_queue));
  } else {
    //map_flags == CL_MAP_WRITE || map_flags == CL_MAP_WRITE_INVALIDATE_REGION
    INITIALIZE_CALLBACK_INFO(initializeMemcpyCallBackInfo, cpy_info, (cpy_info, GPU_MEMCPY_H2D, size, command_queue));
  }

  opencl_subscriber_callback(cpy_info);

  cl_event *eventp = NULL;
  SET_EVENT_POINTER(eventp, event, cpy_info);

  void *map_ptr = pfn_real(command_queue, buffer, blocking_map, map_flags, offset,
                     size, num_events_in_wait_list, event_wait_list, eventp, errcode_ret);

  if (map_flags == CL_MAP_READ) {
    ETMSG(OPENCL, "Registering callback for kind MEMCPY, type: D2H. "
                  "Correlation id: %"PRIu64 "", cpy_info->details.cpy_cb.correlation_id);
    ETMSG(OPENCL, "%d(bytes) of data being transferred from device to host",
          (long)size);
  } else {
    ETMSG(OPENCL, "Registering callback for kind MEMCPY, type: H2D. "
                  "Correlation id: %"PRIu64 "", cpy_info->details.cpy_cb.correlation_id);
    ETMSG(OPENCL, "%d(bytes) of data being transferred from host to device",
          (long)size);
  }

  pfn_clSetEventCallback(*eventp, CL_COMPLETE, &opencl_activity_completion_callback, cpy_info);

  return map_ptr;
}


cl_mem
foilbase_clCreateBuffer
(
  typeof(&clCreateBuffer) pfn_real,
  typeof(&clGetEventProfilingInfo) pfn_clGetEventProfilingInfo,
  typeof(&clReleaseEvent) pfn_clReleaseEvent,
  cl_context context,
  cl_mem_flags flags,
  size_t size,
  void* host_ptr,
  cl_int* errcode_ret
)
{
  ETMSG(OPENCL, "clCreateBuffer flags: %u, size: %"PRIu64 "", flags, size);

  opencl_object_t *mem_info = opencl_malloc_kind(pfn_clGetEventProfilingInfo, pfn_clReleaseEvent, GPU_ACTIVITY_MEMORY);
  INITIALIZE_CALLBACK_INFO(initializeMemoryCallBackInfo, mem_info, (mem_info, flags, size))

  opencl_subscriber_callback(mem_info);

  gpu_interval_t interval;
  interval.start = CPU_NANOTIME();
  cl_mem buffer = pfn_real(context, flags, size, host_ptr, errcode_ret);
  interval.end = CPU_NANOTIME();

  opencl_operation_multiplexer_push(interval, mem_info, mem_info->details.mem_cb.correlation_id);

  opencl_free(mem_info);

  return buffer;
}


cl_int
foilbase_clSetKernelArg
(
  typeof(&clSetKernelArg) pfn_real,
  cl_kernel kernel,
  cl_uint arg_index,
  size_t arg_size,
  const void* arg_value
)
{
  cl_int status = pfn_real(kernel, arg_index, arg_size, arg_value);
  if (optimization_check && status == CL_SUCCESS) {
    recordKernelParams(kernel, arg_value, arg_size);
  }
  return status;
}


cl_int
foilbase_clWaitForEvents
(
  typeof(&clWaitForEvents) pfn_real,
  typeof(&clReleaseEvent) pfn_clReleaseEvent,
  typeof(&clGetEventInfo) pfn_clGetEventInfo,
  cl_uint num_events,
  const cl_event* event_list
)
{
  ETMSG(OPENCL, "clWaitForEvents called");
  cl_command_queue *queues;
  queues = (cl_command_queue*)malloc(num_events*sizeof(cl_command_queue));

  if(is_opencl_blame_shifting_enabled()) {
    for (int i = 0; i < num_events; i++) {
      size_t queue_size;
      pfn_clGetEventInfo(event_list[i], CL_EVENT_COMMAND_QUEUE, 0, NULL, &queue_size);
      char queue_data[queue_size];
      pfn_clGetEventInfo(event_list[i], CL_EVENT_COMMAND_QUEUE, queue_size, queue_data, NULL);
      queues[i] = *(cl_command_queue*)queue_data;
      opencl_sync_prologue(queues[i]);
    }
  }

  cl_int status = pfn_real(num_events, event_list);

  if(is_opencl_blame_shifting_enabled()) {
    for (int i = 0; i < num_events; i++) {
      opencl_sync_epilogue(pfn_clReleaseEvent, queues[i], (uint16_t)num_events);
    }
  }
  return status;
}


cl_int
foilbase_clReleaseMemObject
(
  typeof(&clReleaseMemObject) pfn_real,
  cl_mem mem
)
{
  cl_int status = pfn_real(mem);
  if (optimization_check && status == CL_SUCCESS) {
    clearBufferEntry(mem);
  }
        return status;
}


cl_int
foilbase_clReleaseKernel
(
  typeof(&clReleaseKernel) pfn_real,
  cl_kernel kernel
)
{
  ETMSG(OPENCL, "clReleaseKernel called for kernel: %"PRIu64 "", (uint64_t)kernel);

  cl_int status = pfn_real(kernel);
  if (optimization_check && status == CL_SUCCESS) {
    clearKernelQueues(kernel);
    clearKernelParams(kernel);
  }
  return status;
}


cl_int
foilbase_clReleaseCommandQueue
(
  typeof(&clReleaseCommandQueue) pfn_real,
  cl_command_queue command_queue
)
{
  ETMSG(OPENCL, "clReleaseCommandQueue called");
  cl_int status = pfn_real(command_queue);

  if (optimization_check && status == CL_SUCCESS) {
    clearQueueContext(command_queue);
  }
  if (is_opencl_blame_shifting_enabled() && status == CL_SUCCESS) {
    opencl_queue_epilogue(command_queue);
  }
  return status;
}


uint64_t
get_numeric_hash_id_for_string
(
 const char *mem_ptr,
 size_t mem_size
)
{
  // Compute hash for mem_ptr with mem_size
  unsigned char hash[CRYPTO_HASH_LENGTH];
  crypto_compute_hash(mem_ptr, mem_size, hash, CRYPTO_HASH_LENGTH);

  size_t i;
  uint64_t num_hash = 0;
  for (i = 0; i < CRYPTO_HASH_LENGTH; ++i) {
    num_hash += ((uint64_t)pow(hash[i], i+1) % 0xFFFFFFFF);
  }
  return num_hash;
}


cl_int
foilbase_clFinish
(
  typeof(&clFinish) pfn_real,
  typeof(&clReleaseEvent) pfn_clReleaseEvent,
  cl_command_queue command_queue
)
{
  ETMSG(OPENCL, "clFinish called");
  // on the assumption that clFinish is synchronous, we have sandwiched it with calls to sync_prologue and sync_epilogue
  if(is_opencl_blame_shifting_enabled()) {
    opencl_sync_prologue(command_queue);
  }
  cl_int status = pfn_real(command_queue);
  if(is_opencl_blame_shifting_enabled()) {
    opencl_sync_epilogue(pfn_clReleaseEvent, command_queue, 1);
  }
  return status;
}



void
opencl_optimization_check_enable
(
 void
)
{
  optimization_check = true;
  ETMSG(OPENCL, "Intel optimization check enabled");
}


void
opencl_blame_shifting_enable
(
 void
)
{
  ENABLE_BLAME_SHIFTING = true;
        ETMSG(OPENCL, "Opencl Blame-Shifting enabled");
}


void
set_gpu_utilization_flag
(
 void
)
{
  gpu_utilization_enabled = true;
}


bool
get_gpu_utilization_flag
(
 void
)
{
  return gpu_utilization_enabled;
}


cct_node_t*
place_cct_under_opencl_kernel
(
  uint32_t kernel_module_id
)
{
  cct_node_t *api_node = gpu_application_thread_correlation_callback(0);
  gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;
  gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
          gpu_placeholder_type_kernel);
  gpu_placeholder_type_t placeholder_type = gpu_placeholder_type_kernel;
  gpu_op_ccts_t gpu_op_ccts;

  hpcrun_safe_enter();
  gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
  cct_node_t *cct_ph = gpu_op_ccts_get(&gpu_op_ccts, placeholder_type);
  hpcrun_safe_exit();

  if (hpcrun_cct_children(cct_ph) == NULL) {
    ip_normalized_t kernel_ip;
    kernel_ip.lm_id = (uint16_t) kernel_module_id;
    kernel_ip.lm_ip = 0;  // offset=0
    cct_node_t *kernel_cct =
      hpcrun_cct_insert_ip_norm(cct_ph, kernel_ip, true);
    hpcrun_cct_retain(kernel_cct);
  }
  return cct_ph;
}


void
opencl_api_thread_finalize
(
 void *args,
 int how
)
{
  if (opencl_api_flag) {
    // If I have invoked any opencl api, I have to attribute all my activities to my ccts
    opencl_api_flag = false;

    atomic_bool wait;
    atomic_store(&wait, true);
    gpu_activity_t gpu_activity;
    memset(&gpu_activity, 0, sizeof(gpu_activity_t));

    gpu_activity.kind = GPU_ACTIVITY_FLUSH;
    gpu_activity.details.flush.wait = &wait;
    gpu_operation_multiplexer_push(gpu_activity_channel_get_local(), NULL, &gpu_activity);

    // Wait until operations are drained
    // Operation channel is FIFO
    while (atomic_load(&wait)) {}

    // Wait until my activities are drained
    if (how == MONITOR_EXIT_NORMAL) opencl_wait_for_self_pending_operations();

    // Now I can attribute activities
    gpu_application_thread_process_activities();
  }
}


void
opencl_api_process_finalize
(
 void *args,
 int how
)
{
  if (gpu_utilization_enabled) {
    intel_papi_teardown();
  }
  if (optimization_check) { // is this the right to do final optimization checks
    // we cannot get cct nodes using gpu_application_thread_correlation_callback inside fini-thread callback
    // monitor_block_shootdown() inside libmonitor blocks this call
    isSingleDeviceUsed();
    areAllDevicesUsed(atomic_load_explicit(&total_num_devices, memory_order_acquire));
  }
  gpu_operation_multiplexer_fini();
}

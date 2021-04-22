//******************************************************************************
// system includes
//******************************************************************************

#include <ucontext.h>               // getcontext



//******************************************************************************
// local includes
//******************************************************************************

#include "optimization-check.h"
#include "maps/queue-context-map.h"
#include "maps/kernel-queue-map.h"
#include "maps/kernel-param-map.h"
#include "pointer-alias-check-helper.h"
#include "maps/buffer-map.h"
#include "maps/device-map.h"

#include <hpcrun/gpu/gpu-activity.h>                // intel_optimization_t
#include <hpcrun/gpu/gpu-metrics.h>                 // gpu_metrics_attribute
#include <hpcrun/gpu/gpu-application-thread-api.h>  // gpu_application_thread_correlation_callback



//******************************************************************************
// local data
//******************************************************************************

static uint usedDeviceCount = 0;
static cct_node_t *firstDeviceCCT = NULL;



//******************************************************************************
// private functions
//******************************************************************************

static void
create_activity_object
(
 gpu_activity_t *ga,
 cct_node_t *cct_node,
 intel_optimization_t *activity
)
{
  ga->kind = GPU_ACTIVITY_INTEL_OPTIMIZATION;
  ga->cct_node = cct_node;
  ga->details.intel_optimization.intelOptKind = activity->intelOptKind;
  ga->details.intel_optimization.val = 1; // metric values will be 1(bool)
}


static void
record_intel_optimization_metrics
(
 cct_node_t *cct_node,
 intel_optimization_t *i
)
{
  gpu_activity_t ga;
  create_activity_object(&ga, cct_node, i);
  gpu_metrics_attribute(&ga);
}



//******************************************************************************
// interface functions
//******************************************************************************

/* is queue set to in-order execution */
void
isQueueInInOrderExecutionMode
(
	cl_command_queue_properties *properties
)
{
  // if properties is not specified an in-order host command queue is created for the specified device
	bool inorder = !properties ? true : !(*properties && CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);

  if (inorder) {
    cct_node_t *cct_node = gpu_application_thread_correlation_callback(0);
    intel_optimization_t i;
    i.intelOptKind = INORDER_QUEUE;
    record_intel_optimization_metrics(cct_node, &i);
  }
}


/* is kernel executed in multiple queues/contexts */
void
recordQueueContext
(
 cl_command_queue queue,
 cl_context context
)
{
  queue_context_map_insert((uint64_t)queue, (uint64_t)context);   
}


void
clearQueueContext
(
 cl_command_queue queue
)
{
  queue_context_map_delete((uint64_t)queue);   
  
}


void
isKernelSubmittedToMultipleQueues
(
 cl_kernel kernel,
 cl_command_queue queue
)
{
  /* Queues provide a channel to submit kernels for execution on an accelerator. Queues also hold a context that describes the state of the device.
   * This state includes the contents of buffers and any memory needed to execute the kernels.
   * The runtime keeps track of the current device context and avoids unnecessary memory transfers between host and device.
   * Therefore, it is better to submit and launch kernels from one context together as opposed to interleaving the kernel submissions in different contexts.
   *
   * If we submit the kernels to the same queue, we get best performance because all the kernels are able to just transfer the needed inputs once at the
   * beginning and do all their computations.
   * 
   * If the kernels are submitted to different queues that share the same context, the performance is similar to submitting it to one queue.
   * 
   * When a kernel is submitted to a new queue with a different context, the JIT process compiles the kernel to the new
   * device associated with the context. If this JIT compilation time is discounted, the actual execution of the kernels is similar.
   *
   * If the kernels are submitted to three different queues that have have three different contexts, performance degrades because at kernel
   * invocation, the runtime needs to transfer all input buffers to the accelerator every time. In addition, the kernels will be JITed for each of the contexts.
   *
   * If for some reason there is a need to use different queues, the problem can be alleviated by creating the queues with shared context.
   * This will prevent the need to transfer the input buffers, but the memory footprint of the kernels will increase because all the output buffers have be
   * resident at the same time in the context whereas earlier the same memory on the device could be used for the output buffers.*/ 
  
  queue_context_map_entry_t *qc_entry = queue_context_map_lookup((uint64_t)queue);
  uint64_t queue_context = queue_context_map_entry_context_id_get(qc_entry);
  kernel_queue_map_entry_t *kq_entry = kernel_queue_map_insert((uint64_t)kernel, (uint64_t)queue, queue_context);
  qc_node_t *list_head = kernel_queue_map_entry_qc_list_get(kq_entry);

  // check if the list has multiple queues. If yes, are the contexts same or different?
  qc_node_t *curr = list_head;
  while (curr) {
    uint64_t queue_id = curr->queue_id;
    uint64_t context_id = curr->context_id;
    if (queue_id != (uint64_t)queue) {
      queue_context_map_entry_t *qce = queue_context_map_lookup(queue_id);
      if (!qce) {
        // this queue has been deleted. We need to delete this qc_node_t 
        continue;
      }
      // warning 1: kernel being passed to multiple queues
      cct_node_t *cct_node = gpu_application_thread_correlation_callback(0);
      intel_optimization_t i;
      i.intelOptKind = KERNEL_TO_MULTIPLE_QUEUES;
      record_intel_optimization_metrics(cct_node, &i);

      if (context_id != (uint64_t)queue_context) {
        // warning 2: kernel passed to multiple queues with different context  
        i.intelOptKind = KERNEL_TO_MULTIPLE_QUEUES_MULTIPLE_CONTEXTS;
        record_intel_optimization_metrics(cct_node, &i);
      }
    }
    curr = curr->next;
  }
}


void
clearKernelQueues
(
 cl_kernel kernel
)
{
  kernel_queue_map_delete((uint64_t)kernel);
}


/* are buffer pointers aliased */
void
recordKernelParams
(
 cl_kernel kernel,
 void *mem,
 size_t size
)
{
  kernel_param_map_insert((uint64_t)kernel, mem, size);
}


void
areKernelParamsAliased
(
 cl_kernel kernel
)
{
  /* Kernels typically operate on arrays of elements that are provided as pointer arguments. When the compiler cannot determine whether these pointers
   * alias each other, it will conservatively assume that they do, in which case it will not reorder operations on these pointers */
  kernel_param_map_entry_t *entry = kernel_param_map_lookup((uint64_t)kernel);
  if (!entry) {
    // entry will be null when the kernel has no parameters
    return;
  }
  kp_node_t *kp_list = kernel_param_map_entry_kp_list_get(entry);
  bool aliased = checkIfMemoryRegionsOverlap(kp_list);
  
  if (!aliased) {
    cct_node_t *cct_node = gpu_application_thread_correlation_callback(0);
    intel_optimization_t i;
    i.intelOptKind = KERNEL_PARAMS_NOT_ALIASED;
    record_intel_optimization_metrics(cct_node, &i);
  }
  // this check happens during kernel execution.
  // after a kernel is executed, new params could be added for the kernel
  // so we need to clear the previously set params
  clearKernelParams(kernel);
}


void
clearKernelParams
(
 cl_kernel kernel
)
{
  kernel_param_map_delete((uint64_t)kernel);
}


/* Single device AOT compilation */
void
recordDeviceCount
(
 uint num_devices,
 const cl_device_id *devices
)
{
  for (int i = 0; i < num_devices; i++) {
    bool new_device = device_map_insert((uint64_t)devices[i]);
    if (new_device) {
      usedDeviceCount++;
    }
    if (usedDeviceCount == 1) {
      // first device. If we need to record SINGLE_DEVICE_USE_AOT_COMPILATION or ALL_DEVICES_NOT_USED, this is the place to do it
      firstDeviceCCT = gpu_application_thread_correlation_callback(0);
    }
  }
}


void
isSingleDeviceUsed
(
 void
)
{
  /* The Intel(R) oneAPI DPC++ Compiler converts a DPC++ program into an intermediate language called SPIR-V and stores that in the binary
  produced by the compilation process. The advantage of producing this intermediate file instead of the binary is that this code can
  be run on any hardware platform by translating the SPIR-V code into the assembly code of the platform at runtime. This process of translating
  the intermediate code present in the binary is called JIT compilation (just-in-time compilation). JIT compilation can happen on demand at runtime.
  There are multiple ways in which this JIT compilation can be controlled. By default, all the SPIR-V code present in the binary is translated
  upfront at the beginning of the execution of the first offloaded kernel.

  The overhead of JIT compilation at runtime can be avoided by ahead-of-time (AOT) compilation (it is enabled by appropriate switches on the compile-line).
  With AOT compile, the binary will contain the actual assembly code of the platform that was selected during compilation instead of the SPIR-V
  intermediate code. The advantage is that we do not need to JIT compile the code from SPIR-V to assembly during execution, which makes the code run
  faster. The disadvantage is that now the code cannot run anywhere other than the platform for which it was compiled.

  We can use AOT in the case where a single device is used */

  // is this check simplistic? Maybe the user creates a context with many devices, but never uses the context.
  bool singleDevice = (usedDeviceCount == 1) ? true : false;
  if (singleDevice) {
    intel_optimization_t i;
    i.intelOptKind = SINGLE_DEVICE_USE_AOT_COMPILATION;
    record_intel_optimization_metrics(firstDeviceCCT, &i);
  }
}


/* Is output of a kernel passed as input to another kernel */
void
recordH2DCall
(
 cl_mem buffer
)
{
  /*The cost of moving data between host and device is quite high especially in the case of discrete accelerators. So it is very important
  to avoid data transfers between host and device as much as possible. In some situations it may be required to bring the data that was
  computed by a kernel(kernel1) on the accelerator to the host and do some operation on it and send it back to the device(kernel2) for further processing.
  In such situation we will end up paying for the cost of device to host transfer and then again host to device transfer.

  We can perform kernel fusion one level further and fuse both kernel1 and kernel2. This gives very good performance since it avoids the intermediate
  memory transfers completely in addition to avoiding launching an additional kernel. Most of the performance benefit in this case is due to improvement
  in locality of memory reference */

  buffer_map_entry_t *entry = buffer_map_update((uint64_t)buffer, 1, 0);
  int D2H_count = buffer_map_entry_D2H_count_get(entry);
  if (D2H_count > 0) {
    // this H2D call happens after a D2H call for the same memory object
    // are we missing any scenarios ?
    // what about calls from clEnqueueMapBuffer and clSetKernelArg
    cct_node_t *cct_node = gpu_application_thread_correlation_callback(0);
    intel_optimization_t i;
    i.intelOptKind = OUTPUT_OF_KERNEL_INPUT_TO_ANOTHER_KERNEL;
    record_intel_optimization_metrics(cct_node, &i);
  }
}


void
recordD2HCall
(
 cl_mem buffer
)
{
  buffer_map_update((uint64_t)buffer, 0, 1);
}


void
clearBufferEntry
(
 cl_mem buffer
)
{
  buffer_map_delete((uint64_t)buffer);
}


/* using host device as an accelerator */
static uint
getTotalDeviceCount
(
 void
)
{
  cl_uint platformCount;
  cl_platform_id* platforms;
  cl_uint platformDeviceCount;
  uint totalDeviceCount;

  clGetPlatformIDs(0, NULL, &platformCount);
  platforms = (cl_platform_id*) malloc(sizeof(cl_platform_id) * platformCount);
  clGetPlatformIDs(platformCount, platforms, NULL);

  for (int i = 0; i < platformCount; i++) {
    clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, 0, NULL, &platformDeviceCount);
    totalDeviceCount += platformDeviceCount;
  }
  return totalDeviceCount;
}


void
areAllDevicesUsed
(
 void
)
{
  /* DPC++ provides access to different kinds of devices through abstraction of device selectors. Queues can be created for each of the devices,
  and kernels can be submitted to them for execution. All kernel submits in DPC++ are non-blocking, which means that once the kernel is submitted
  to a queue for execution, the host does not wait for it to finish unless waiting on the queue is explicitly requested. This allows the host to
  do some work itself or initiate work on other devices while the kernel is executing on the accelerator.
  The host CPU can be treated as an accelerator and the DPCPP can submit kernels to it for execution. This is completely independent and
  orthogonal to the job done by the host to orchestrate the kernel submission and creation. The underlying operating system manages the kernels
  submitted to the CPU accelerator as another process and uses the same openCL/Level0 runtime mechanisms to exchange information with the host device.

  my addition: We can infact use all accelerators for distributing large computations
  In order to achieve good balance one will have to split the work proportional to the capability of the accelerator instead of distributing it evenly
  */

  uint totalDeviceCount = getTotalDeviceCount();
  if (totalDeviceCount != usedDeviceCount) {
    intel_optimization_t i;
    i.intelOptKind = ALL_DEVICES_NOT_USED;
    record_intel_optimization_metrics(firstDeviceCCT, &i);  // this may crash when firstDeviceCCT == NULL
  }
}


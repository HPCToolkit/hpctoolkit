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

#include <hpcrun/metrics.h>                 // hpcrun_metrics_new_kind, hpcrun_set_new_metric_info, hpcrun_close_kind
#include <hpcrun/sample_event.h>            // hpcrun_sample_callpath



//******************************************************************************
// local includes
//******************************************************************************

// metrics
int inorder_queue_metric_id;
int kernel_to_multiple_queues;
int kernel_to_multiple_queues_multiple_contexts;
int kernel_params_not_aliased;



//******************************************************************************
// interface functions
//******************************************************************************

void
optimization_metrics_enable
(
 void
)
{
  kind_info_t *optimization_kind = hpcrun_metrics_new_kind();
  // Create metrics for intel optimization
  inorder_queue_metric_id = hpcrun_set_new_metric_info(optimization_kind, "INORDER_QUEUE");
  kernel_to_multiple_queues = hpcrun_set_new_metric_info(optimization_kind, "KERNEL_TO_MULTIPLE_QUEUES");
  kernel_to_multiple_queues_multiple_contexts = hpcrun_set_new_metric_info(optimization_kind, "KERNEL_TO_MULTIPLE_QUEUES_MULTIPLE_CONTEXTS");
  kernel_params_not_aliased = hpcrun_set_new_metric_info(optimization_kind, "KERNEL_PARAMS_NOT_ALIASED");

  hpcrun_close_kind(optimization_kind);  
}


/* is queue set to in-order execution */
void
isQueueInInOrderExecutionMode
(
	cl_command_queue_properties properties
)
{
	bool inorder = !(properties && CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);
  ucontext_t context;
  getcontext(&context);
  cct_node_t *cct_node = hpcrun_sample_callpath(&context, inorder_queue_metric_id, (cct_metric_data_t){.i = (int)inorder}, 0 /*skipInner */ , 0 /*isSync */, NULL ).sample_node;
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
      /*ucontext_t context;
      getcontext(&context);
      cct_node_t *cct_node = hpcrun_sample_callpath(&context, kernel_to_multiple_queues, (cct_metric_data_t){.i = 1}, 0, 0, NULL ).sample_node;*/

      if (context_id != (uint64_t)queue_context) {
        // warning 2: kernel passed to multiple queues with different context  
        ucontext_t context;
        getcontext(&context);
        cct_node_t *cct_node = hpcrun_sample_callpath(&context, kernel_to_multiple_queues_multiple_contexts, (cct_metric_data_t){.i = 1}, 0, 0, NULL).sample_node;
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
  kp_node_t *kp_list = kernel_param_map_entry_kp_list_get(entry);
  bool aliased = checkIfMemoryRegionsOverlap(kp_list);
  
  if (!aliased) {
    ucontext_t context;
    getcontext(&context);
    cct_node_t *cct_node = hpcrun_sample_callpath(&context, kernel_params_not_aliased, (cct_metric_data_t){.i = 1}, 0, 0, NULL ).sample_node;
  }
}


void
clearKernelParams
(
 cl_kernel kernel
)
{
  kernel_param_map_delete((uint64_t)kernel);
}


/* Strategy:
 * at each CPU sample, the CPU/host thread will query the completion status of kernel at head each queue in StreamQs datastructure
 * But this approach was needed for Nvidia streams that had in-order kernel execution.
 * Opencl has 2 kernel-execution modes: in-order and out-of-order.
 * Thus we need to rethink our strategy
 * */

/* Scenarios/Action:
 * CPU: inactive, GPU: active			Action: GPU_IDLE_CAUSE for calling_context at GPU is incremented by sampling interval
 * CPU: inactive, GPU: inactive		Action: Need to figure out how to handle this scenario
 * CPU: active, 	GPU: inactive		Action:	CPU_IDLE_CAUSE for calling_context at CPU is incremented by sampling interval
 * CPU: active, 	GPU: active			Action: Nothing to do
 * */

// #ifdef ENABLE_OPENCL_BLAME_SHIFTING

//******************************************************************************
// system includes
//******************************************************************************

#include <monitor.h>													// monitor_real_abort
#include <stdbool.h>													// bool



//******************************************************************************
// local includes
//******************************************************************************

// #include "gpu-blame-opencl-datastructure.h"

#include <hpcrun/cct/cct.h>										// cct_node_t
#include <hpcrun/sample-sources/gpu_blame.h>	// g_active_threads
#include <hpcrun/thread_data.h>								// gpu_data
#include <hpcrun/trace.h>											// hpcrun_trace_isactive

#include <lib/prof-lean/hpcrun-opencl.h>			// CL_SUCCESS, cl_event, etc
#include <lib/prof-lean/spinlock.h>						// spinlock_t, SPINLOCK_UNLOCKED
#include <lib/prof-lean/stdatomic.h>					// atomic_fetch_add
#include <lib/support-lean/timer.h>						// time_getTimeReal



/******************************************************************************
 * forward declarations
 *****************************************************************************/

// Each event_list_node_t maintains information about an asynchronous opencl activity (kernel or memcpy)
// Note that there are some implicit memory transfers that dont have events bound to them
typedef struct event_list_node_t {
    // cudaEvent inserted immediately before and after the activity
    cl_event event;

    // start and end times of event_start and event_end
    unsigned long event_start_time;
    unsigned long event_end_time;

    // CCT node of the CPU thread that launched this activity
    cct_node_t *launcher_cct;
    // CCT node of the stream
    cct_node_t *stream_launcher_cct;

    // Outstanding threads that need to examine this activity
    uint32_t ref_count;

    // our internal splay tree id for the corresponding cudaStream for this activity
    uint32_t stream_id;
    union {
        struct event_list_node_t *next;
        struct event_list_node_t *next_free_node;
    };
} event_list_node_t;


// Per GPU stream information
typedef struct stream_node_t {
    // pointer to most recently issued activity
    struct event_list_node_t *latest_event_node;
    // pointer to the oldest unfinished activity of this stream
    struct event_list_node_t *unfinished_event_node;
    // pointer to the next stream which has activities pending
    struct stream_node_t *next_unfinished_stream;
    
		/*
		// hpcrun profiling and tracing infp
    struct core_profile_trace_data_t *st;
    // used to remove from hpcrun cleanup list if stream is explicitly destroyed
    hpcrun_aux_cleanup_t * aux_cleanup_info;
    // IDLE NODE persistent id for this stream
    int32_t idle_node_id;
		*/
} stream_node_t;


typedef struct IPC_data_t {
    uint32_t device_id;
    _Atomic(uint64_t) outstanding_kernels;
    uint64_t num_threads_at_sync_all_procs;
} IPC_data_t;



//******************************************************************************
// macros
//******************************************************************************

#define SHARED_BLAMING_INITIALISED (ipc_data != NULL)
#define DECR_SHARED_BLAMING_DS(field)  do{ if(SHARED_BLAMING_INITIALISED) atomic_fetch_add(&(ipc_data->field), -1L); }while(0) 



//******************************************************************************
// local data
//******************************************************************************

int cpu_idle_metric_id;
int gpu_time_metric_id;
int cpu_idle_cause_metric_id;
int gpu_idle_metric_id;

// num threads in the process
uint64_t g_active_threads;

// bool isCPUInactive;	// should this variable be atomic?

static IPC_data_t * ipc_data;

// lock for blame-shifting activities
static spinlock_t bs_lock = SPINLOCK_UNLOCKED;

// is inter-process blaming enabled?
static bool g_do_shared_blaming;

// First stream with pending activities
static stream_node_t *g_unfinished_stream_list_head;

static uint64_t g_num_threads_at_sync;



//******************************************************************************
// private operations
//******************************************************************************

/*bool
isCPUBusy() {
	return !isCPUInactive;
}


bool
isGPUBusy() {
	return false;
}*/


// inspect activities finished on each stream and record metrics accordingly
static uint32_t cleanup_finished_events() {
	uint32_t num_unfinished_streams = 0;
	stream_node_t *prev_stream = NULL;
	stream_node_t *next_stream = NULL;
	stream_node_t *cur_stream = g_unfinished_stream_list_head;

	while (cur_stream != NULL) {
		assert(cur_stream->unfinished_event_node && " Can't point unfinished stream to null");
		next_stream = cur_stream->next_unfinished_stream;

		event_list_node_t *current_event_node = cur_stream->unfinished_event_node;
		while (current_event_node) {

			// we need access to the node that contains the event
			cl_int err_cl = clGetEventProfilingInfo(current_event_node->event, CL_PROFILING_COMMAND_END,
																							sizeof(cl_ulong), &(current_event_node->event_end_time), NULL);

			if (err_cl == CL_SUCCESS) {

				// Decrement   ipc_data->outstanding_kernels
				DECR_SHARED_BLAMING_DS(outstanding_kernels);

				// record start time
				float elapsedTime;      // in millisec with 0.5 microsec resolution as per CUDA

				// in cuda, g_start_of_world_time was used. What was the purpose of that
				err_cl = clGetEventProfilingInfo(current_event_node->event, CL_PROFILING_COMMAND_START,
																					sizeof(cl_ulong), &(current_event_node->event_start_time), NULL);

				// soft failure
				if (err_cl != CL_SUCCESS) {
					EMSG("clGetEventProfilingInfo failed");
					break;
				}
				elapsedTime = current_event_node->event_end_time - current_event_node->event_start_time; 
				assert(elapsedTime > 0);

				/*
					 uint64_t micro_time_start = (uint64_t) (((double) elapsedTime) * 1000) + g_start_of_world_time;
					 CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[cudaEventElapsedTimeEnum].cudaEventElapsedTimeReal(&elapsedTime, g_start_of_world_event, current_event_node->event_end));
					 assert(elapsedTime > 0);
					 uint64_t micro_time_end = (uint64_t) (((double) elapsedTime) * 1000) + g_start_of_world_time;
					 assert(micro_time_start <= micro_time_end);
					 */

				if(hpcrun_trace_isactive()) {
					// the purpose of tracing with blame-shifting was to find the issues caused by blocking system called. I dont think we need this block
				}


				// Add the kernel execution time to the gpu_time_metric_id
				cct_metric_data_increment(gpu_time_metric_id, current_event_node->launcher_cct, (cct_metric_data_t) {
						.i = (current_event_node->event_end_time - current_event_node->event_start_time)});

				// if we are doing deferred BS, we need this line, else remove
				event_list_node_t *deferred_node = current_event_node;
				// delete the current_event_node variable from the linkedlist
				current_event_node = current_event_node->next;

				// if we are doing deferred BS, we need this line, else remove
				// Add to_free to fre list
				if (g_num_threads_at_sync) {
					// some threads are waiting, hence add this kernel for deferred blaming
					/*
					deferred_node->ref_count = g_num_threads_at_sync;
					deferred_node->event_start_time = micro_time_start;
					deferred_node->event_end_time = micro_time_end;
					deferred_node->next = g_finished_event_nodes_tail->next;
					g_finished_event_nodes_tail->next = deferred_node;
					g_finished_event_nodes_tail = deferred_node;
					*/

				} else {
					clReleaseEvent(current_event_node->event);
				}
			} else {
				break;
			}
		}

		cur_stream->unfinished_event_node = current_event_node;
		if (current_event_node == NULL) {
			// set oldest and newest pointers to null
			cur_stream->latest_event_node = NULL;
			if (prev_stream == NULL) {
				g_unfinished_stream_list_head = next_stream;
			} else {
				prev_stream->next_unfinished_stream = next_stream;
			}
		} else {

			num_unfinished_streams++;
			prev_stream = cur_stream;
		}
		cur_stream = next_stream;
	}
	return num_unfinished_streams;
}



//******************************************************************************
// interface operations
//******************************************************************************

/*void
setCPUBusy() {
	isCPUInactive = false;
}


void
setCPUIdle() {
	isCPUInactive = true;
}


void opencl_gpu_blame_shifter1() {

	bool cpuBusy = isCPUBusy();
	bool gpuBusy = isGPUBusy();

	// CPU: inactive, GPU: active			Action: GPU_IDLE_CAUSE for calling_context at GPU is incremented by sampling interval
	if (!cpuBusy && gpuBusy) {

	}
	
	// CPU: inactive, GPU: inactive		Action: Need to figure out how to handle this scenario
	if (!cpuBusy && !gpuBusy) {

	}

	// CPU: active, 	GPU: inactive		Action:	CPU_IDLE_CAUSE for calling_context at CPU is incremented by sampling interval
	if (cpuBusy && !gpuBusy) {

	}

	// CPU: active, 	GPU: active			Action: Nothing to do
}*/


void sync_prologue() {
	TD_GET(gpu_data.is_thread_at_opencl_sync) = true;
}


void sync_epilogue() {
	TD_GET(gpu_data.is_thread_at_opencl_sync) = true;
}


////////////////////////////////////////////////
// CPU-GPU blame shift interface
////////////////////////////////////////////////

void
opencl_gpu_blame_shifter(void* dc, int metric_id, cct_node_t* node,  int metric_dc)
{
  metric_desc_t* metric_desc = hpcrun_id2metric(metric_id);
    
  // Only blame shift idleness for time metric.
  if ( !metric_desc->properties.time )
    return;
    
  uint64_t cur_time_us = 0;
  int ret = time_getTimeReal(&cur_time_us);
  if (ret != 0) {
    EMSG("time_getTimeReal (clock_gettime) failed!");
    monitor_real_abort();
  }
  uint64_t metric_incr = cur_time_us - TD_GET(last_time_us);
    
  // If we are already in a cuda API, then we can't call cleanup_finished_events() since CUDA could have taken the same lock. Hence we just return.
    
  bool is_threads_at_sync = TD_GET(gpu_data.is_thread_at_opencl_sync);

	// CUDA needed to defer BS during sync. ops, if we have no such need for opencl, we can remove this block
  if (is_threads_at_sync) {
    if(SHARED_BLAMING_INITIALISED) {
			// uncomment this when you understand what these variables mean for opencl context
      // TD_GET(gpu_data.accum_num_sync_threads) += ipc_data->num_threads_at_sync_all_procs; 
      // TD_GET(gpu_data.accum_num_samples) += 1;
    } 
    return;
  }
  
	// is the purpose of this lock to provide atomicity to each blame-shifting call
  spinlock_lock(&bs_lock);
  uint32_t num_unfinished_streams = 0;
  stream_node_t *unfinished_event_list_head = 0;
    
  num_unfinished_streams = cleanup_finished_events();
  unfinished_event_list_head = g_unfinished_stream_list_head;
    
  if (num_unfinished_streams) {
        
    //SHARED BLAMING: kernels need to be blamed for idleness on other procs/threads.
    if(SHARED_BLAMING_INITIALISED && ipc_data->num_threads_at_sync_all_procs && !g_num_threads_at_sync) {
      for (stream_node_t * unfinished_stream = unfinished_event_list_head; unfinished_stream; unfinished_stream = unfinished_stream->next_unfinished_stream) {
	//TODO: FIXME: the local threads at sync need to be removed, /T has to be done while adding metric
	//increment (either one of them).
	// How is the CPU idle at this time? If loop at L:300 exits if the thread is at sync
	cct_metric_data_increment(cpu_idle_cause_metric_id, unfinished_stream->unfinished_event_node->launcher_cct, (cct_metric_data_t) {
	    .r = metric_incr / g_active_threads}
	  );
      }
    }
  }
  else {

    /*** Code to account for Overload factor has been removed(if we need if, add it back from gpu_blame-overrides) ***/
        
    // GPU is idle iff   ipc_data->outstanding_kernels == 0 
    // If ipc_data is NULL, then this process has not made GPU calls so, we are blind and declare GPU idle w/o checking status of other processes
    // There is no better solution yet since we dont know which GPU card we should be looking for idleness. 
    if(g_do_shared_blaming){
      if ( !ipc_data || atomic_load(&(ipc_data->outstanding_kernels)) == 0) { // GPU device is truely idle i.e. no other process is keeping it busy
	// Increment gpu_ilde by metric_incr
	cct_metric_data_increment(gpu_idle_metric_id, node, (cct_metric_data_t) {
	    .i = metric_incr});
      }
    } else {
      // Increment gpu_ilde by metric_incr
      cct_metric_data_increment(gpu_idle_metric_id, node, (cct_metric_data_t) {
	  .i = metric_incr});            
    }
        
  }
  spinlock_unlock(&bs_lock);
}

// #endif	// ENABLE_OPENCL_BLAME_SHIFTING

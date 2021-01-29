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

#include <fcntl.h>														// O_CREAT, O_RDWR
#include <monitor.h>													// monitor_real_abort
#include <stdbool.h>													// bool
#include <sys/mman.h>													// shm_open
#include <ucontext.h>           							// getcontext



//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-blame-opencl-datastructure.h"		// event_list_node_t, queue_node_t

#include <hpcrun/cct/cct.h>										// cct_node_t
#include <hpcrun/constructors.h>							// HPCRUN_CONSTRUCTOR
#include <hpcrun/memory/mmap.h>								// hpcrun_mmap_anon
#include <hpcrun/sample_event.h>							// hpcrun_sample_callpath
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

typedef struct IPC_data_t {
	uint32_t device_id;
	_Atomic(uint64_t) outstanding_kernels;
	_Atomic(uint64_t) num_threads_at_sync_all_procs;
} IPC_data_t;



//******************************************************************************
// macros
//******************************************************************************

#define MAX_SHARED_KEY_LENGTH (100)

#define SHARED_BLAMING_INITIALISED (ipc_data != NULL)

#define INCR_SHARED_BLAMING_DS(field)  do{ if(SHARED_BLAMING_INITIALISED) atomic_fetch_add(&(ipc_data->field), 1L); }while(0) 
#define DECR_SHARED_BLAMING_DS(field)  do{ if(SHARED_BLAMING_INITIALISED) atomic_fetch_add(&(ipc_data->field), -1L); }while(0) 

#define ADD_TO_FREE_EVENTS_LIST(node_ptr) do { (node_ptr)->next_free_node = g_free_event_nodes_head; \
g_free_event_nodes_head = (node_ptr); }while(0)



//******************************************************************************
// local data
//******************************************************************************

int cpu_idle_metric_id;
int gpu_time_metric_id;
int cpu_idle_cause_metric_id;
int gpu_idle_metric_id;

// num threads in the process
atomic_uint_fast64_t g_active_threads;

// lock for blame-shifting activities
static spinlock_t bs_lock = SPINLOCK_UNLOCKED;

static uint64_t g_num_threads_at_sync;

static event_list_node_t *g_free_event_nodes_head;

// First queue with pending activities
static queue_node_t *g_unfinished_queue_list_head;

// I think this can be deleted
// Last queue with pending activities
static event_list_node_t *g_finished_event_nodes_tail;

// I think this can be deleted
// dummy activity node
static event_list_node_t dummy_event_node = {
	.event_start_time = 0,
	.event_end_time = 0,
	.launcher_cct = 0,
	.queue_launcher_cct = 0 
};

// is inter-process blaming enabled?
static bool g_do_shared_blaming;

static IPC_data_t *ipc_data;



/******************** Utilities ********************/
/******************** CONSTRUCTORS ********************/

static char shared_key[MAX_SHARED_KEY_LENGTH];


static void destroy_shared_memory(void * p) {
    // we should munmap, but I will not do since we dont do it in so many other places in hpcrun
    // munmap(ipc_data);
    shm_unlink((char *)shared_key);
}


static inline void create_shared_memory() {
    int device_id = 1;	// static device_id
    int fd ;
    sprintf(shared_key, "/gpublame%d",device_id);
    if ( (fd = shm_open(shared_key, O_RDWR | O_CREAT, 0666)) < 0 ) {
        EEMSG("Failed to shm_open (%s) on device %d, retval = %d", shared_key, device_id, fd);
        monitor_real_abort();
    }
    if ( ftruncate(fd, sizeof(IPC_data_t)) < 0 ) {
        EEMSG("Failed to ftruncate() on device %d",device_id);
        monitor_real_abort();
    }
    
    if( (ipc_data = mmap(NULL, sizeof(IPC_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 )) == MAP_FAILED ) {
        EEMSG("Failed to mmap() on device %d",device_id);
        monitor_real_abort();
    }

    hpcrun_process_aux_cleanup_add(destroy_shared_memory, (void *) shared_key);    
}


static void
InitCpuGpuBlameShiftOpenclDataStructs
(
	void
)
{
	char * shared_blaming_env;
	g_unfinished_queue_list_head = NULL;
	g_finished_event_nodes_tail = &dummy_event_node;
	dummy_event_node.next = g_finished_event_nodes_tail;
	shared_blaming_env = getenv("HPCRUN_ENABLE_SHARED_GPU_BLAMING");
	if(shared_blaming_env) {
		g_do_shared_blaming = atoi(shared_blaming_env);
	}
	// uncomment later
	g_do_shared_blaming = true;
	create_shared_memory();
}


HPCRUN_CONSTRUCTOR(CpuGpuBlameShiftInit)(void)
{
	if (getenv("DEBUG_HPCRUN_GPU_CONS"))
		fprintf(stderr, "CPU-GPU blame shift constructor called\n");
	// no dlopen calls in static case
	// #ifndef HPCRUN_STATIC_LINK
	InitCpuGpuBlameShiftOpenclDataStructs();
	// #endif // ! HPCRUN_STATIC_LINK
}

/******************** END CONSTRUCTORS ****/



//******************************************************************************
// private operations
//******************************************************************************

// Initialize hpcrun core_profile_trace_data for a new queue
static inline core_profile_trace_data_t*
hpcrun_queue_data_alloc_init
(
	int id
)
{
	core_profile_trace_data_t *st = hpcrun_mmap_anon(sizeof(core_profile_trace_data_t));
	// FIXME: revisit to perform this memstore operation appropriately.
	//memstore = td->memstore;
	//memset(st, 0xfe, sizeof(core_profile_trace_data_t));
	memset(st, 0, sizeof(core_profile_trace_data_t));
	//td->memstore = memstore;
	//hpcrun_make_memstore(&td->memstore, is_child);
	st->id = id;
	st->epoch = hpcrun_malloc(sizeof(epoch_t));
	st->epoch->csdata_ctxt = copy_thr_ctxt(TD_GET(core_profile_trace_data.epoch)->csdata.ctxt); //copy_thr_ctxt(thr_ctxt);
	hpcrun_cct_bundle_init(&(st->epoch->csdata), (st->epoch->csdata).ctxt);
	st->epoch->loadmap = hpcrun_getLoadmap();
	st->epoch->next  = NULL;
	hpcrun_cct2metrics_init(&(st->cct2metrics_map)); //this just does st->map = NULL;


	st->trace_min_time_us = 0;
	st->trace_max_time_us = 0;
	st->hpcrun_file  = NULL;

	return st;
}


static void
add_queue_node_to_splay_tree
(
	cl_command_queue queue
)
{
	uint64_t queue_id = (uint64_t)queue;
	queue_node_t *node = (queue_node_t*)hpcrun_malloc(sizeof(queue_node_t));
	node->st = hpcrun_queue_data_alloc_init(queue_id);	
	node->event_list_head = NULL;
	node->event_list_tail = NULL;
	node->next_unfinished_queue = NULL;
	queue_map_insert(queue_id, node);
}


static cct_node_t*
queue_duplicate_cpu_node
(
	core_profile_trace_data_t *st,
	ucontext_t *context,
	cct_node_t *node
)
{
	cct_bundle_t* cct= &(st->epoch->csdata);
	cct_node_t * tmp_root = cct->tree_root;
	hpcrun_cct_insert_path(&tmp_root, node);
	return tmp_root;
}


// Insert a new event node corresponding to a kernel in a queue
// Caller is responsible for calling monitor_disable_new_threads() : why?
static void
create_and_insert_event
(
	uint64_t queue_id,
	queue_node_t *queue_node,
	cl_event event,
	cct_node_t *launcher_cct,
	cct_node_t *queue_launcher_cct
)
{
	event_list_node_t *event_node;
	if (g_free_event_nodes_head) {
		// get from free list
		event_node = g_free_event_nodes_head;
		g_free_event_nodes_head = g_free_event_nodes_head->next_free_node;

	} else {
		// allocate new node
		event_node = (event_list_node_t *) hpcrun_malloc(sizeof(event_list_node_t));
	}
	event_node->event = event;
	event_node->queue_launcher_cct = queue_launcher_cct;
	event_node->launcher_cct = launcher_cct;
	event_node->queue_id = queue_id;
	event_node->next = NULL;

	if (queue_node->event_list_head == NULL) {
		queue_node->event_list_head = event_node;
		queue_node->event_list_tail = event_node;
		queue_node->next_unfinished_queue = g_unfinished_queue_list_head;
		g_unfinished_queue_list_head = queue_node;
	} else {
		queue_node->event_list_tail->next = event_node;
		queue_node->event_list_tail = event_node;
	}
}


// inspect activities finished on each queue and record metrics accordingly
static uint32_t
cleanup_finished_events
(
	void
)
{
	uint32_t num_unfinished_queues = 0;
	queue_node_t *prev_queue = NULL;
	queue_node_t *next_queue = NULL;
	queue_node_t *cur_queue = g_unfinished_queue_list_head;

	while (cur_queue != NULL) {
		assert(cur_queue->event_list_head && " Can't point unfinished queue to null");
		next_queue = cur_queue->next_unfinished_queue;

		event_list_node_t *current_event_node = cur_queue->event_list_head;
		bool cur_queue_unfinished = false;

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
				clReleaseEvent(current_event_node->event);
				if (current_event_node == cur_queue->event_list_head) {
					if (cur_queue->event_list_head == cur_queue->event_list_tail) {
						cur_queue->event_list_head = NULL;
						cur_queue->event_list_tail = NULL;
					} else {
						cur_queue->event_list_head = current_event_node->next;
					}
				}
				// we need to update prev_node->next to completely eliminate reference to current node
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
					ADD_TO_FREE_EVENTS_LIST(deferred_node);
				}
			} else {
				cur_queue_unfinished = true;
				current_event_node = current_event_node->next;
			}
		}

		//cur_queue->event_list_head = current_event_node;
		if (cur_queue_unfinished == false) {
			// set oldest and newest pointers to null
			cur_queue->event_list_head = NULL;
			cur_queue->event_list_tail = NULL;
			if (prev_queue == NULL) {
				g_unfinished_queue_list_head = next_queue;
			} else {
				prev_queue->next_unfinished_queue = next_queue;
			}
		} else {
			num_unfinished_queues++;
			prev_queue = cur_queue;
		}
		cur_queue = next_queue;
	}
	return num_unfinished_queues;
}



//******************************************************************************
// interface operations
//******************************************************************************

void
queue_prologue
(
	cl_command_queue queue
)
{
	add_queue_node_to_splay_tree(queue);
}


void
kernel_prologue
(
	cl_event event,
	cl_command_queue queue
)
{
	// increment the reference count for the event
	clRetainEvent(event);
	//create_queue0_if_needed(queue);
	uint64_t queue_id = (uint64_t) queue;
	// HPCRUN_ASYNC_BLOCK_SPIN_LOCK;

	// why should thread be marked blocked on sync? 
	// TD_GET(gpu_data.is_thread_at_cuda_sync) = true;
	ucontext_t context;
	getcontext(&context);
	int skip_inner = 1; // what value should be passed here?
	// why is cpu_idle_metric_id being passed here?
	cct_node_t *cct_node = hpcrun_sample_callpath(&context, cpu_idle_metric_id, (cct_metric_data_t){.i = 0}, skip_inner /*skipInner */ , 1 /*isSync */, NULL ).sample_node;
	
	queue_map_entry_t *entry = queue_map_lookup(queue_id);
	queue_node_t *queue_node = queue_map_entry_queue_node_get(entry);
	// NULL checks need to be added for entry and queue_node
	
	cct_node_t *queue_cct = queue_duplicate_cpu_node(queue_node->st, &context, cct_node);
	monitor_disable_new_threads();	// why?
	create_and_insert_event(queue_id, queue_node, event, cct_node, queue_cct);
	INCR_SHARED_BLAMING_DS(outstanding_kernels);
}


void
sync_prologue
(
 cl_command_queue queue
)
{
	// get the queue node in splay-tree associated with the queue. We will need it for attributing CPU_IDLE_CAUSE
	uint64_t queue_id = (uint64_t) queue;
	queue_map_entry_t *entry = queue_map_lookup(queue_id);
	queue_node_t *queue_node = queue_map_entry_queue_node_get(entry);
	TD_GET(gpu_data.queue_responsible_for_cpu_sync) = queue_node;

	TD_GET(gpu_data.is_thread_at_opencl_sync) = true;
	INCR_SHARED_BLAMING_DS(num_threads_at_sync_all_procs);
}


void
sync_epilogue
(
 void
)
{
	// TODO: remove all completed kernel events from StreamQs
	TD_GET(gpu_data.queue_responsible_for_cpu_sync) = NULL;

	TD_GET(gpu_data.is_thread_at_opencl_sync) = false;
	DECR_SHARED_BLAMING_DS(num_threads_at_sync_all_procs);
}



////////////////////////////////////////////////
// CPU-GPU blame shift interface
////////////////////////////////////////////////

void
opencl_gpu_blame_shifter
(
 void* dc,
 int metric_id,
 cct_node_t* node,
 int metric_dc
)
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
				
		// during a sample, host thread is in sync block waiting for a GPU kernel to complete execution. Mark CPU as idle at this sample
		queue_node_t *queue_node = TD_GET(gpu_data.queue_responsible_for_cpu_sync);
		if (queue_node != NULL) {
			cct_node_t *gpu_cct = queue_node->event_list_head->launcher_cct;
			cct_metric_data_increment(cpu_idle_cause_metric_id, gpu_cct, (cct_metric_data_t) {
					.r = metric_incr / atomic_load(&g_active_threads)}
					);
		} else {
			// cant find queue responsoble for cpu sync	
		}
		if(SHARED_BLAMING_INITIALISED) {
			// uncomment this when you understand what these variables mean for opencl context
			// TD_GET(gpu_data.accum_num_sync_threads) += ipc_data->num_threads_at_sync_all_procs; 
			// TD_GET(gpu_data.accum_num_samples) += 1;
		} 
		return;
	}

	// is the purpose of this lock to provide atomicity to each blame-shifting call
	spinlock_lock(&bs_lock);
	uint32_t num_unfinished_queues = 0;
	queue_node_t *unfinished_event_list_head = 0;

	num_unfinished_queues = cleanup_finished_events();
	unfinished_event_list_head = g_unfinished_queue_list_head;

	if (num_unfinished_queues) {

		//SHARED BLAMING: kernels need to be blamed for idleness on other procs/threads.
		if(SHARED_BLAMING_INITIALISED && atomic_load(&ipc_data->num_threads_at_sync_all_procs)) { // && !g_num_threads_at_sync
			for (queue_node_t * unfinished_queue = unfinished_event_list_head; unfinished_queue; unfinished_queue = unfinished_queue->next_unfinished_queue) {
				//TODO: FIXME: the local threads at sync need to be removed, /T has to be done while adding metric
				//increment (either one of them).
				// How is the CPU idle at this time? If loop at L:300 exits if the thread is at sync
				// Careful, unfinished_event_node was replaced by event_list_head. Does that replacement make sense
				cct_metric_data_increment(cpu_idle_cause_metric_id, unfinished_queue->event_list_head->launcher_cct, (cct_metric_data_t) {
						.r = metric_incr / atomic_load(&g_active_threads)}
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

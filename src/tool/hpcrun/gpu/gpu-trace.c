//******************************************************************************
// local includes
//******************************************************************************

#include <pthread.h>



//******************************************************************************
// libmonitor 
//******************************************************************************

#include <monitor.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/stdatomic.h>

#include <hpcrun/cct/cct.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/threadmgr.h>
#include <hpcrun/trace.h>

// #include "gpu-context-stream-id-map.h"
#include "gpu-trace.h"
#include "gpu-trace-channel.h"
#include "gpu-trace-item.h"



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_trace_t {
  pthread_t thread;
  gpu_trace_channel_t *trace_channel;
} gpu_trace_t;

typedef void *(*pthread_start_routine_t)(void *);



//******************************************************************************
// local variables
//******************************************************************************

static _Atomic(bool) stop_trace_flag;

static atomic_ullong stream_counter;

static atomic_ullong stream_id;



//******************************************************************************
// private operations
//******************************************************************************

static gpu_trace_t *
gpu_trace_new
(
 void
)
{
  gpu_trace_t *trace = hpcrun_malloc_safe(sizeof(gpu_trace_t));
  trace->trace_channel = gpu_trace_channel_alloc();
  return trace;
}


static cct_node_t *
gpu_trace_cct_root
(
 thread_data_t* td
)
{
  return td->core_profile_trace_data.epoch->csdata.tree_root;
}


static cct_node_t *
gpu_trace_cct_no_thread
(
 thread_data_t* td
)
{
  cct_node_t *no_thread = 
    td->core_profile_trace_data.epoch->csdata.special_no_thread_node;

  return no_thread;
}


static cct_node_t *
gpu_trace_cct_insert_context
(
 thread_data_t* td,
 cct_node_t *path
)
{
  cct_node_t *leaf = 
    hpcrun_cct_insert_path_return_leaf(gpu_trace_cct_root(td), path);

  return leaf;
}


static uint64_t
gpu_trace_time
(
 uint64_t gpu_time
)
{
  // return time in ns
  uint64_t time = gpu_time; 

  return time;
}


static void
gpu_trace_stream_append
(
 thread_data_t* td,
 cct_node_t *leaf,
 uint64_t time
)
{
  hpcrun_trace_append_stream(&td->core_profile_trace_data, leaf, 0, 
			     td->prev_dLCA, time); 
}


static void
gpu_trace_first
(
 thread_data_t* td,
 cct_node_t *no_thread,
 uint64_t start
)
{
  static __thread bool first = true;

  if (first) {
    first = false;
    gpu_trace_stream_append(td, no_thread, start - 1);
  }
}


static void
consume_one_trace_item
(
 thread_data_t* td,
 cct_node_t *call_path,
 uint64_t start_time,
 uint64_t end_time
)
{
  cct_node_t *leaf = gpu_trace_cct_insert_context(td, call_path);

  cct_node_t *no_thread = gpu_trace_cct_no_thread(td);

  uint64_t start = gpu_trace_time(start_time);
  uint64_t end   = gpu_trace_time(end_time);

  gpu_trace_first(td, no_thread, start);

  gpu_trace_stream_append(td, leaf, start);

  gpu_trace_stream_append(td, no_thread, end + 1);
}


static void
gpu_trace_activities_process
(
 thread_data_t *td,
 gpu_trace_t *thread_args
)
{
  gpu_trace_channel_consume(thread_args->trace_channel, td, 
			    consume_one_trace_item);
}


static void
gpu_trace_activities_await
(
 gpu_trace_t* thread_args
)
{
  gpu_trace_channel_await(thread_args->trace_channel);
}


static int
gpu_trace_stream_id
(
 void
)
{
  // FIXME: this is a bad way to compute a stream id 
  int id = 500 + atomic_fetch_add(&stream_id, 1);

  return id;
}


static thread_data_t *
gpu_trace_stream_acquire
(
 void
)
{
  thread_data_t* td = NULL;

  int id = gpu_trace_stream_id();

  hpcrun_threadMgr_non_compact_data_get(id, NULL, &td);

  hpcrun_set_thread_data(td);

  return td;
}


static void
gpu_trace_stream_release
(
 thread_data_t *td
)
{
  epoch_t *epoch = TD_GET(core_profile_trace_data.epoch);

  hpcrun_threadMgr_data_put(epoch, td);

  atomic_fetch_add(&stream_counter, -1);
}



//******************************************************************************
// interface operations
//******************************************************************************

void 
gpu_trace_init
(
 void
)
{
  atomic_store(&stop_trace_flag, false);
  atomic_store(&stream_counter, 0);
  atomic_store(&stream_id, 0);
}


void *
gpu_trace_record
(
 gpu_trace_t *thread_args
)
{
  thread_data_t* td = gpu_trace_stream_acquire();

  while (!atomic_load(&stop_trace_flag)) {
    gpu_trace_activities_process(td, thread_args);
    gpu_trace_activities_await(thread_args);
  }

  gpu_trace_activities_process(td, thread_args);

  gpu_trace_stream_release(td);

  return NULL;
}


void
gpu_trace_fini
(
 void *arg
)
{
  atomic_store(&stop_trace_flag, true);

  gpu_context_stream_map_signal_all();

  while (atomic_load(&stream_counter));
}


gpu_trace_t *
gpu_trace_create
(
 void
)
{
  // Init variables
  gpu_trace_t *trace = gpu_trace_new();

  // Create a new thread for the stream without libmonitor watching
  monitor_disable_new_threads();

  atomic_fetch_add(&stream_counter, 1);

  pthread_create(&trace->thread, NULL, (pthread_start_routine_t) gpu_trace_record, 
		 trace);

  monitor_enable_new_threads();

  return trace;
}

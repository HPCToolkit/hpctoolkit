// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/branches/hpctoolkit-gpu-blame-shift-proto/src/tool/hpcrun/sample-sources/gpu_blame.h $
// $Id: itimer.c 3784 2012-05-10 22:35:51Z mc29 $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2011, Rice University
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
// **

#ifndef __GPU_BLAME_H__
#define __GPU_BLAME_H__
#include <sys/ipc.h>
#include <sys/shm.h>
#include <hpcrun/cct/cct.h>
#include <cuda.h>
#include <cuda_runtime.h>
#include <dlfcn.h>

#ifdef ENABLE_CUDA

#include "gpu_blame-cuda-runtime-header.h"
#include "gpu_blame-cuda-driver-header.h"
#include <hpcrun/core_profile_trace_data.h>

//
// Blame shiting interface
//
#define MAX_STREAMS (500)

///TODO: evaluate this option : FORCE CLEANUP
#define SYNCHRONOUS_CLEANUP do{  hpcrun_safe_enter_async(NULL); 		\
spinlock_lock(&g_gpu_lock); 			\
cleanup_finished_events(); 			\
spinlock_unlock(&g_gpu_lock);			\
hpcrun_safe_exit(); } while(0)


// Visible types

typedef struct event_list_node_t {
    
    cudaEvent_t event_end;
    cudaEvent_t event_start;
    
    uint64_t event_start_time;
    uint64_t event_end_time;
    cct_node_t *launcher_cct;
    cct_node_t *stream_launcher_cct;
    uint32_t ref_count;
    uint32_t stream_id;
    union {
        struct event_list_node_t *next;
        struct event_list_node_t *next_free_node;
    };
} event_list_node_t;


typedef struct stream_node_t {
    struct core_profile_trace_data_t *st;
    struct event_list_node_t *latest_event_node;
    struct event_list_node_t *unfinished_event_node;
    struct stream_node_t *next_unfinished_stream;
    // ----------------------------------------
    // IDLE NODE persistent id for this stream
    // ----------------------------------------
    int32_t idle_node_id;
    
} stream_node_t;


/* states for accounting overload potential */
enum overloadPotentialState{
	START_STATE=0,
	WORKING_STATE,
	SYNC_STATE,
	OVERLOADABLE_STATE
};


typedef struct stream_to_id_map_t {
    cudaStream_t stream;
    
    uint32_t id;
    struct stream_to_id_map_t *left;
    struct stream_to_id_map_t *right;
} stream_to_id_map_t;


typedef struct IPC_data_t {
    uint32_t device_id;
    //uint64_t leeches;
    uint64_t outstanding_kernels;
    spinlock_t ipc_lock;
} IPC_data_t;

extern IPC_data_t * ipc_data;
extern int g_shmid;

// Visible global variables
extern stream_node_t *g_unfinished_stream_list_head;
extern event_list_node_t *g_finished_event_nodes_tail;
extern event_list_node_t dummy_event_node;
extern cudaRuntimeFunctionPointer_t  cudaRuntimeFunctionPointer[];
extern cuDriverFunctionPointer_t  cuDriverFunctionPointer[];
extern stream_to_id_map_t stream_to_id[MAX_STREAMS];
extern struct stream_to_id_map_t *stream_to_id_tree_root;
extern stream_node_t g_stream_array[MAX_STREAMS];


extern int cpu_idle_metric_id;
extern int gpu_activity_time_metric_id;
extern int cpu_idle_cause_metric_id;
extern int gpu_idle_metric_id;
extern int gpu_overload_potential_metric_id;
extern int cpu_overlap_metric_id;
extern int gpu_overlap_metric_id;
extern int stream_special_metric_id;
extern int h_to_d_data_xfer_metric_id;
extern int d_to_h_data_xfer_metric_id;

extern bool g_cpu_gpu_enabled;
extern spinlock_t g_gpu_lock;


extern uint64_t g_active_threads;
extern bool g_do_shared_blaming;
extern uint32_t g_cuda_launch_skip_inner;

extern uint64_t g_active_threads;
extern stream_node_t *g_unfinished_stream_list_head;
extern event_list_node_t *g_finished_event_nodes_tail;
extern event_list_node_t dummy_event_node;
extern struct stream_to_id_map_t *stream_to_id_tree_root;


// Visible function declarations
extern void gpu_blame_shift_itimer_signal_handler(cct_node_t * node, uint64_t cur_time_us, uint64_t metric_incr);
extern void CloseAllStreams(struct stream_to_id_map_t *root); 
extern uint32_t cleanup_finished_events();
extern void hpcrun_stream_finalize(core_profile_trace_data_t *cptd);
extern void hpcrun_set_gpu_proxy_present();

#endif
#endif

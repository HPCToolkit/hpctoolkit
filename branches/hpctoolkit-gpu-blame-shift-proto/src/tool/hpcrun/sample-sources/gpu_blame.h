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
//
// Blame shiting interface
//
#define MAX_STREAMS (100)
#define DEVICE_ID (0)


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
    struct stream_data_t *st;
    struct event_list_node_t *latest_event_node;
    struct event_list_node_t *unfinished_event_node;
    struct stream_node_t *next_unfinished_stream;
} stream_node_t;




/*
enum cudaRuntimeAPIIndex {
    CUDA_THREAD_SYNCHRONIZE,
    CUDA_STREAM_SYNCHRONIZE,
    CUDA_EVENT_SYNCHRONIZE,
    CUDA_STREAM_WAIT_EVENT,
    CUDA_DEVICE_SYNCHRONIZE,
    CUDA_CONFIGURE_CALL,
    CUDA_LAUNCH,
    CUDA_STREAM_DESTROY,
    CUDA_STREAM_CREATE,
    CUDA_MALLOC,
    CUDA_FREE,
    CUDA_MEMCPY_ASYNC,
    CUDA_MEMCPY,
    CUDA_MEMCPY_2D,
    CUDA_EVENT_ELAPSED_TIME,
    CUDA_EVENT_CREATE,
    CUDA_EVENT_RECORD,
    CUDA_EVENT_DESTROY,
    
    CUDA_MAX_APIS
};

typedef struct cudaRuntimeFunctionPointer {
    union {
        cudaError_t(*generic) (void);
        cudaError_t(*cudaThreadSynchronizeReal) (void);
        cudaError_t(*cudaStreamSynchronizeReal) (cudaStream_t);
        cudaError_t(*cudaEventSynchronizeReal) (cudaEvent_t);
        cudaError_t(*cudaStreamWaitEventReal) (cudaStream_t, cudaEvent_t, unsigned int);
        cudaError_t(*cudaDeviceSynchronizeReal) (void);
        cudaError_t(*cudaConfigureCallReal) (dim3, dim3, size_t, cudaStream_t);
        cudaError_t(*cudaLaunchReal) (const char *);
        cudaError_t(*cudaStreamDestroyReal) (cudaStream_t);
        cudaError_t(*cudaStreamCreateReal) (cudaStream_t *);
        cudaError_t(*cudaMallocReal) (void **, size_t);
        cudaError_t(*cudaFreeReal) (void *);
        cudaError_t(*cudaMemcpyAsyncReal) (void *dst, const void *src, size_t count, enum cudaMemcpyKind kind, cudaStream_t);
        cudaError_t(*cudaMemcpyReal) (void *dst, const void *src, size_t count, enum cudaMemcpyKind kind);
        cudaError_t(*cudaMemcpy2DReal) (void *dst, size_t dpitch, const void *src, size_t spitch, size_t width, size_t height, enum cudaMemcpyKind kind);
        cudaError_t(*cudaEventElapsedTimeReal) (float *ms, cudaEvent_t start, cudaEvent_t end);
        cudaError_t(*cudaEventCreateReal) (cudaEvent_t * event);
        cudaError_t(*cudaEventRecordReal) (cudaEvent_t event, cudaStream_t stream);
        cudaError_t(*cudaEventDestroyReal) (cudaEvent_t event);
        
    };
    const char *functionName;
} cudaRuntimeFunctionPointer_t;


enum cuDriverAPIIndex {
    CU_STREAM_CREATE,
    CU_STREAM_DESTROY,
    CU_STREAM_SYNCHRONIZE,
    CU_EVENT_SYNCHRONIZE,
    CU_LAUNCH_GRID_ASYNC,
    CU_CTX_DESTROY,
    
    CU_EVENT_CREATE,
    CU_EVENT_RECORD,
    CU_EVENT_DESTROY,
    
    CU_MEMCPY_H_TO_D_ASYNC,
    CU_MEMCPY_H_TO_D,
    CU_MEMCPY_D_TO_H_ASYNC,
    CU_MEMCPY_D_TO_H,
    
    CU_EVENT_ELAPSED_TIME,
    
    CU_MAX_APIS
};

typedef struct cuDriverFunctionPointer {
    union {
        CUresult(*generic) (void);
        CUresult(*cuStreamCreateReal) (CUstream * phStream, unsigned int Flags);
        CUresult(*cuStreamDestroyReal) (CUstream hStream);
        CUresult(*cuStreamSynchronizeReal) (CUstream hStream);
        CUresult(*cuEventSynchronizeReal) (CUevent event);
        CUresult(*cuLaunchGridAsyncReal) (CUfunction f, int grid_width, int grid_height, CUstream hStream);
        CUresult(*cuCtxDestroyReal) (CUcontext ctx);
        
        CUresult(*cuEventCreateReal) (CUevent * phEvent, unsigned int Flags);
        CUresult(*cuEventRecordReal) (CUevent hEvent, CUstream hStream);
        CUresult(*cuEventDestroyReal)(CUevent  event);
        
        CUresult(*cuMemcpyHtoDAsyncReal) (CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount, CUstream hStream);
        CUresult(*cuMemcpyHtoDReal) (CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount);
        CUresult(*cuMemcpyDtoHAsyncReal) (void *dstHost, CUdeviceptr srcDevice, size_t ByteCount, CUstream hStream);
        CUresult(*cuMemcpyDtoHReal) (void *dstHost, CUdeviceptr srcDevice, size_t ByteCount);
        
        CUresult(*cuEventElapsedTimeReal) (float *pMilliseconds, CUevent hStart, CUevent hEnd);
        
    };
    const char *functionName;
} cuDriverFunctionPointer_t;
*/

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

extern int g_cpu_gpu_proxy_count;
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



#endif
#endif

// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2021, Rice University
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

#ifndef gpu_metric_h
#define gpu_metric_h



//*****************************************************************************
// local includes
//*****************************************************************************

#include <include/gpu-metric-names.h>

#include "gpu-activity.h"



//*****************************************************************************
// macros
//*****************************************************************************


typedef enum {
  GPU_INST_STALL_ANY                   = 0
} gpu_inst_stall_all_t;


typedef enum {
  GPU_GMEM_LD_CACHED_BYTES             = 0,
  GPU_GMEM_LD_UNCACHED_BYTES           = 1,
  GPU_GMEM_ST_BYTES                    = 2,
  GPU_GMEM_LD_CACHED_L2TRANS           = 3,
  GPU_GMEM_LD_UNCACHED_L2TRANS         = 4,
  GPU_GMEM_ST_L2TRANS                  = 5,
  GPU_GMEM_LD_CACHED_L2TRANS_THEOR     = 6,
  GPU_GMEM_LD_UNCACHED_L2TRANS_THEOR   = 7,
  GPU_GMEM_ST_L2TRANS_THEOR            = 8
} gpu_gmem_ops_t;


typedef enum {
  GPU_LMEM_LD_BYTES                    = 0,
  GPU_LMEM_ST_BYTES                    = 1,
  GPU_LMEM_LD_TRANS                    = 2,
  GPU_LMEM_ST_TRANS                    = 3,
  GPU_LMEM_LD_TRANS_THEOR              = 4,
  GPU_LMEM_ST_TRANS_THEOR              = 5
} gpu_lmem_ops_t;



//--------------------------------------------------------------------------
// indexed metrics
//--------------------------------------------------------------------------

// gpu memory allocation/deallocation
#define FORALL_GMEM(macro)					\
  macro("GMEM:UNK (B)",           GPU_MEM_UNKNOWN,		\
	"GPU memory alloc/free: unknown memory kind (bytes)")	\
  macro("GMEM:PAG (B)",           GPU_MEM_PAGEABLE,		\
	"GPU memory alloc/free: pageable memory (bytes)")	\
  macro("GMEM:PIN (B)",           GPU_MEM_PINNED,		\
	"GPU memory alloc/free: pinned memory (bytes)")		\
  macro("GMEM:DEV (B)",           GPU_MEM_DEVICE,		\
	"GPU memory alloc/free: device memory (bytes)")		\
  macro("GMEM:ARY (B)",           GPU_MEM_ARRAY,		\
	"GPU memory alloc/free: array memory (bytes)")		\
  macro("GMEM:MAN (B)",           GPU_MEM_MANAGED,		\
	"GPU memory alloc/free: managed memory (bytes)")	\
  macro("GMEM:DST (B)",           GPU_MEM_DEVICE_STATIC,	\
	"GPU memory alloc/free: device static memory (bytes)")	\
  macro("GMEM:MST (B)",           GPU_MEM_MANAGED_STATIC,	\
	"GPU memory alloc/free: managed static memory (bytes)") \
  macro("GMEM:COUNT",             GPU_MEM_COUNT, \
        "GPU memory alloc/free: count")


// gpu memory set
#define FORALL_GMSET(macro)					\
  macro("GMSET:UNK (B)",          GPU_MEM_UNKNOWN,		\
	"GPU memory set: unknown memory kind (bytes)")		\
  macro("GMSET:PAG (B)",          GPU_MEM_PAGEABLE,		\
	"GPU memory set: pageable memory (bytes)")		\
  macro("GMSET:PIN (B)",          GPU_MEM_PINNED,		\
	"GPU memory set: pinned memory (bytes)")		\
  macro("GMSET:DEV (B)",          GPU_MEM_DEVICE,		\
	"GPU memory set: device memory (bytes)")		\
  macro("GMSET:ARY (B)",          GPU_MEM_ARRAY,		\
	"GPU memory set: array memory (bytes)")			\
  macro("GMSET:MAN (B)",          GPU_MEM_MANAGED,		\
	"GPU memory set: managed memory (bytes)")		\
  macro("GMSET:DST (B)",          GPU_MEM_DEVICE_STATIC,	\
	"GPU memory set: device static memory (bytes)")		\
  macro("GMSET:MST (B)",          GPU_MEM_MANAGED_STATIC,	\
	"GPU memory set: managed static memory (bytes)") \
  macro("GMSET:COUNT",            GPU_MEM_COUNT, \
        "GPU memory set: count")


#define FORALL_GPU_INST_STALL(macro)					\
  macro(GPU_INST_METRIC_NAME ":STL_ANY",     GPU_INST_STALL_ANY,	\
	"GPU instruction stalls: any")					\
  macro(GPU_INST_METRIC_NAME ":STL_NONE",    GPU_INST_STALL_NONE,	\
	"GPU instruction stalls: no stall")				\
  macro(GPU_INST_METRIC_NAME ":STL_IFET",    GPU_INST_STALL_IFETCH,	\
	"GPU instruction stalls: await availability of next "		\
	"instruction (fetch or branch delay)")				\
  macro(GPU_INST_METRIC_NAME ":STL_IDEP",    GPU_INST_STALL_IDEPEND,	\
	"GPU instruction stalls: await satisfaction of instruction "	\
	"input dependence")						\
  macro(GPU_INST_METRIC_NAME ":STL_GMEM",    GPU_INST_STALL_GMEM,	\
	"GPU instruction stalls: await completion of global memory "	\
	"access")							\
  macro(GPU_INST_METRIC_NAME ":STL_TMEM",    GPU_INST_STALL_TMEM,	\
	"GPU instruction stalls: texture memory request queue full")	\
  macro(GPU_INST_METRIC_NAME ":STL_SYNC",    GPU_INST_STALL_SYNC,	\
	"GPU instruction stalls: await completion of thread or "	\
	"memory synchronization")					\
  macro(GPU_INST_METRIC_NAME ":STL_CMEM",    GPU_INST_STALL_CMEM,	\
	"GPU instruction stalls: await completion of constant or "	\
	"immediate memory access")					\
  macro(GPU_INST_METRIC_NAME ":STL_PIPE",    GPU_INST_STALL_PIPE_BUSY,	\
	"GPU instruction stalls: await completion of required "	\
	"compute resources")					\
  macro(GPU_INST_METRIC_NAME ":STL_MTHR",    GPU_INST_STALL_MEM_THROTTLE, \
	"GPU instruction stalls: global memory request queue full")	\
  macro(GPU_INST_METRIC_NAME ":STL_NSEL",    GPU_INST_STALL_NOT_SELECTED, \
	"GPU instruction stalls: not selected for issue but ready")	\
  macro(GPU_INST_METRIC_NAME ":STL_OTHR",    GPU_INST_STALL_OTHER,	\
	"GPU instruction stalls: other")				\
  macro(GPU_INST_METRIC_NAME ":STL_SLP",     GPU_INST_STALL_SLEEP,	\
	"GPU instruction stalls: sleep")


// gpu explicit copy
#define FORALL_GXCOPY(macro)					\
  macro("GXCOPY:UNK (B)",         GPU_MEMCPY_UNK,		\
	"GPU explicit memory copy: unknown kind (bytes)")	\
  macro("GXCOPY:H2D (B)",         GPU_MEMCPY_H2D,		\
	"GPU explicit memory copy: host to device (bytes)")	\
  macro("GXCOPY:D2H (B)",         GPU_MEMCPY_D2H,		\
	"GPU explicit memory copy: device to host (bytes)")	\
  macro("GXCOPY:H2A (B)",         GPU_MEMCPY_H2A,		\
	"GPU explicit memory copy: host to array (bytes)")	\
  macro("GXCOPY:A2H (B)",         GPU_MEMCPY_A2H,		\
	"GPU explicit memory copy: array to host (bytes)")	\
  macro("GXCOPY:A2A (B)",         GPU_MEMCPY_A2A,		\
	"GPU explicit memory copy: array to array (bytes)")	\
  macro("GXCOPY:A2D (B)",         GPU_MEMCPY_A2D,		\
	"GPU explicit memory copy: array to device (bytes)")	\
  macro("GXCOPY:D2A (B)",         GPU_MEMCPY_D2A,		\
	"GPU explicit memory copy: device to array (bytes)")	\
  macro("GXCOPY:D2D (B)",         GPU_MEMCPY_D2D,		\
	"GPU explicit memory copy: device to device (bytes)")	\
  macro("GXCOPY:H2H (B)",         GPU_MEMCPY_H2H,		\
	"GPU explicit memory copy: host to host (bytes)")	\
  macro("GXCOPY:P2P (B)",         GPU_MEMCPY_P2P,		\
	"GPU explicit memory copy: peer to peer (bytes)") \
  macro("GXCOPY:COUNT",           GPU_MEMCPY_COUNT, \
        "GPU explicit memory copy: count")


#define FORALL_GSYNC(macro)					\
  macro("GSYNC:UNK (sec)",         GPU_SYNC_UNKNOWN,		\
	"GPU synchronizations: unknown kind")			\
  macro("GSYNC:EVT (sec)",         GPU_SYNC_EVENT,		\
	"GPU synchronizations: event")				\
  macro("GSYNC:STRE (sec)",        GPU_SYNC_STREAM_EVENT_WAIT,	\
	"GPU synchronizations: stream event wait")		\
  macro("GSYNC:STR (sec)",         GPU_SYNC_STREAM,		\
	"GPU synchronizations: stream")				\
  macro("GSYNC:CTX (sec)",         GPU_SYNC_CONTEXT,		\
	"GPU synchronizations: context")     \
  macro("GSYNC:COUNT",           GPU_SYNC_COUNT, \
        "GPU synchronizations: count")

// gpu global memory access
#define FORALL_GGMEM(macro)						\
  macro("GGMEM:LDC (B)",          GPU_GMEM_LD_CACHED_BYTES,		\
	"GPU global memory: load cacheable memory (bytes)")		\
  macro("GGMEM:LDU (B)",          GPU_GMEM_LD_UNCACHED_BYTES,		\
	"GPU global memory: load uncacheable memory (bytes)")		\
  macro("GGMEM:ST (B)",           GPU_GMEM_ST_BYTES,			\
	"GPU global memory: store (bytes)")				\
  									\
  macro("GGMEM:LDC (L2T)",        GPU_GMEM_LD_CACHED_L2TRANS,		\
	"GPU global memory: load cacheable (L2 cache transactions)")	\
  macro("GGMEM:LDU (L2T)",        GPU_GMEM_LD_UNCACHED_L2TRANS,		\
	"GPU global memory: load uncacheable (L2 cache transactions)")	\
  macro("GGMEM:ST (L2T)",         GPU_GMEM_ST_L2TRANS,			\
	"GPU global memory: store (L2 cache transactions)")		\
  				  					\
  macro("GGMEM:LDCT (L2T)",       GPU_GMEM_LD_CACHED_L2TRANS_THEOR,	\
	"GPU global memory: load cacheable "				\
	"(L2 cache transactions, theoretical)")				\
  macro("GGMEM:LDUT (L2T)",       GPU_GMEM_LD_UNCACHED_L2TRANS_THEOR,	\
	"GPU global memory: load uncacheable "				\
	"(L2 cache transactions, theoretical)")				\
  macro("GGMEM:STT (L2T)",        GPU_GMEM_ST_L2TRANS_THEOR,		\
	"GPU global memory: store "					\
	"(L2 cache transactions, theoretical)")


// gpu local memory access
#define FORALL_GLMEM(macro)					\
  macro("GLMEM:LD (B)",           GPU_LMEM_LD_BYTES,		\
	"GPU local memory: load (bytes)")			\
  macro("GLMEM:ST (B)",           GPU_LMEM_ST_BYTES,		\
	"GPU local memory: store (bytes)")			\
								\
  macro("GLMEM:LD (T)",           GPU_LMEM_LD_TRANS,		\
	"GPU local memory: load (transactions)")		\
  macro("GLMEM:ST (T)",           GPU_LMEM_ST_TRANS,		\
	"GPU local memory: store (transactions)")		\
								\
  macro("GLMEM:LDT (T)",          GPU_LMEM_LD_TRANS_THEOR,	\
	"GPU local memory: load (transactions, theoretical)")	\
  macro("GLMEM:STT (T)",          GPU_LMEM_ST_TRANS_THEOR,	\
	"GPU local memory: store (transactions, theoretical)")

//--------------------------------------------------------------------------
// scalar metrics 
//--------------------------------------------------------------------------


// gpu activity times
#define FORALL_GTIMES(macro)					\
  macro("GPUOP (sec)",              GPU_TIME_OP,		\
	"GPU time: all operations (seconds)")  \
  macro("GKER (sec)",               GPU_TIME_KER,			\
	"GPU time: kernel execution (seconds)")			\
  macro("GMEM (sec)",               GPU_TIME_MEM,			\
	"GPU time: memory allocation/deallocation (seconds)")	\
  macro("GMSET (sec)",              GPU_TIME_MSET,		\
	"GPU time: memory set (seconds)")			\
  macro("GXCOPY (sec)",             GPU_TIME_XCOPY,		\
	"GPU time: explicit data copy (seconds)")		\
  macro("GICOPY (sec)",             GPU_TIME_ICOPY,		\
	"GPU time: implicit data copy (seconds)")		\
  macro("GSYNC (sec)",              GPU_TIME_SYNC,		\
	"GPU time: synchronization (seconds)")

// gpu instruction count
#define FORALL_GPU_INST(macro)			\
  macro(GPU_INST_METRIC_NAME ": frequency", GPU_INST_EXEC_COUNT,	\
	"GPU instruction/basic-block execution count")  \
  macro(GPU_INST_METRIC_NAME ": latency", GPU_INST_LATENCY,	\
	"GPU instruction latency")  \
  macro(GPU_INST_METRIC_NAME ": active simd", GPU_INST_ACT_SIMD_LANES,	\
	"GPU active simd lanes")  \
  macro(GPU_INST_METRIC_NAME ": wasted simd", GPU_INST_WASTE_SIMD_LANES,	\
	"GPU wasted simd lanes")  \
  macro(GPU_INST_METRIC_NAME ": total simd", GPU_INST_TOT_SIMD_LANES,	\
	"GPU total simd lanes")   \
  macro(GPU_INST_METRIC_NAME ": scalar simd loss", GPU_INST_SCALAR_SIMD_LOSS,	\
	"GPU simd lanes lost due to scalar instructions")   \
  macro(GPU_INST_METRIC_NAME ": C", GPU_INST_COVERED_LATENCY,	\
	"GPU covered latency")  \
  macro(GPU_INST_METRIC_NAME ": U", GPU_INST_UNCOVERED_LATENCY,	\
	"GPU uncovered latency")   \
  macro(GPU_INST_METRIC_NAME ": threads for covering latency", GPU_INST_THR_NEEDED_FOR_COVERING_LATENCY,	\
	"GPU threads for covering latency (1 + U/C)")


// gpu kernel characteristics
#define FORALL_KINFO(macro)						\
  macro("GKER:STMEM_ACUMU (B)",         GPU_KINFO_STMEM_ACUMU,		\
	"GPU kernel: static memory accumulator [internal use only]")	\
  macro("GKER:DYMEM_ACUMU (B)",         GPU_KINFO_DYMEM_ACUMU,	        \
	"GPU kernel: dynamic memory accumulator [internal use only]") 	\
  macro("GKER:LMEM_ACUMU (B)",          GPU_KINFO_LMEM_ACUMU,		\
	"GPU kernel: local memory accumulator [internal use only]") 	\
  macro("GKER:FGP_ACT_ACUMU",           GPU_KINFO_FGP_ACT_ACUMU,	\
	"GPU kernel: fine-grain parallelism accumulator [internal use only]") \
  macro("GKER:FGP_MAX_ACUMU",           GPU_KINFO_FGP_MAX_ACUMU,	\
	"GPU kernel: fine-grain parallelism accumulator [internal use only]") \
  macro("GKER:THR_REG_ACUMU",           GPU_KINFO_REGISTERS_ACUMU,	\
	"GPU kernel: thread register count accumulator [internal use only]") \
  macro("GKER:BLK_THR_ACUMU",           GPU_KINFO_BLK_THREADS_ACUMU,	\
	"GPU kernel: thread count accumulator [internal use only]")	\
  macro("GKER:BLK_SM_ACUMU",            GPU_KINFO_BLK_SMEM_ACUMU,	\
	"GPU kernel: block local memory accumulator [internal use only]") \
  macro("GKER:BLKS_ACUMU",           GPU_KINFO_BLKS_ACUMU,	\
	"GPU kernel: block count accumulator [internal use only]")	\
  macro("GKER:STMEM (B)",         GPU_KINFO_STMEM,			\
	"GPU kernel: static memory (bytes)")				\
  macro("GKER:DYMEM (B)",         GPU_KINFO_DYMEM,			\
	"GPU kernel: dynamic memory (bytes)")				\
  macro("GKER:LMEM (B)",          GPU_KINFO_LMEM,			\
	"GPU kernel: local memory (bytes)")				\
  macro("GKER:FGP_ACT",           GPU_KINFO_FGP_ACT,			\
	"GPU kernel: fine-grain parallelism, actual")			\
  macro("GKER:FGP_MAX",           GPU_KINFO_FGP_MAX,			\
	"GPU kernel: fine-grain parallelism, maximum")			\
  macro("GKER:THR_REG",           GPU_KINFO_REGISTERS,			\
	"GPU kernel: thread register count")				\
  macro("GKER:BLK_THR",           GPU_KINFO_BLK_THREADS,		\
	"GPU kernel: thread count")					\
  macro("GKER:BLK_SM (B)",            GPU_KINFO_BLK_SMEM,		\
	"GPU kernel: block local memory (bytes)")			\
  macro("GKER:BLKS",            GPU_KINFO_BLKS,		\
	"GPU kernel: block count")			\
  macro("GKER:COUNT",             GPU_KINFO_COUNT,  			\
	"GPU kernel: launch count")					\
  macro("GKER:OCC_THR",               GPU_KINFO_OCCUPANCY_THR,		\
	"GPU kernel: theoretical occupancy (FGP_ACT / FGP_MAX)")          \
  

// gpu implicit copy
#define FORALL_GICOPY(macro)					\
  macro("GICOPY:UNK (B)",         GPU_ICOPY_UNKNOWN,		\
	"GPU implicit copy: unknown kind (bytes)")		\
  macro("GICOPY:H2D (B)",         GPU_ICOPY_H2D_BYTES,		\
	"GPU implicit copy: host to device (bytes)")		\
  macro("GICOPY:D2H (B)",         GPU_ICOPY_D2H_BYTES,		\
	"GPU implicit copy: device to host (bytes)")		\
  macro("GICOPY:D2D (B)",         GPU_ICOPY_D2D_BYTES,		\
	"GPU implicit copy: device to device (bytes)")		\
  macro("GICOPY:CPU_PF",          GPU_ICOPY_CPU_PF,		\
	"GPU implicit copy: CPU page faults")			\
  macro("GICOPY:GPU_PF",          GPU_ICOPY_GPU_PF,		\
	"GPU implicit copy: GPU page faults")			\
  macro("GICOPY:THRASH",          GPU_ICOPY_THRASH,		\
	"GPU implicit copy: CPU thrashing page faults "		\
	"(data frequently migrating between processors)")	\
  macro("GICOPY:THROT",           GPU_ICOPY_THROT,		\
	"GPU implicit copy: throttling "			\
	"(prevent thrashing by delaying page fault service)")	\
  macro("GICOPY:RMAP",            GPU_ICOPY_RMAP,		\
	"GPU implicit copy: remote maps "			\
	"(prevent thrashing by pinning memory for a time with "	\
	"some processor mapping and accessing it remotely)")  \
  macro("GICOPY:COUNT",           GPU_ICOPY_COUNT,  \
  "GPU implicit copy: count")


// gpu branch
#define FORALL_GBR(macro)					\
    macro("GBR:DIV",                GPU_BR_DIVERGED,		\
	  "GPU branches: diverged")				\
  macro("GBR:EXE",                GPU_BR_EXECUTED,		\
	"GPU branches: executed")


// gpu sampling information
#define FORALL_GSAMP_INT(macro)					\
  macro("GSAMP:DRP",              GPU_SAMPLE_DROPPED,		\
	"GPU PC samples: dropped")				\
  macro("GSAMP:EXP",              GPU_SAMPLE_EXPECTED,		\
	"GPU PC samples: expected")				\
  macro("GSAMP:TOT",              GPU_SAMPLE_TOTAL,		\
	"GPU PC samples: measured")				\
  macro("GSAMP:PER (cyc)",        GPU_SAMPLE_PERIOD,		\
	"GPU PC samples: period (GPU cycles)")

#define FORALL_GSAMP_REAL(macro)				\
  macro("GSAMP:UTIL",             GPU_SAMPLE_UTILIZATION,	\
    "GPU utilization computed using PC sampling")

#define FORALL_GSAMP(macro)			\
  FORALL_GSAMP_INT(macro)			\
  FORALL_GSAMP_REAL(macro)				



//******************************************************************************
// interface operations
//******************************************************************************

//--------------------------------------------------
// enable default GPU metrics
//--------------------------------------------------

void
gpu_metrics_default_enable
(
 void
);


//--------------------------------------------------
// record NVIDIA kernel info
//--------------------------------------------------

void
gpu_metrics_KINFO_enable
(
 void
);


//--------------------------------------------------
// record implicit copy metrics for unified memory
//--------------------------------------------------

void
gpu_metrics_GICOPY_enable
(
 void
);


//----------------------------------------
// instruction sampling metrics
//----------------------------------------

// record gpu instruction sample counts
void
gpu_metrics_GPU_INST_enable
(
 void
);
 

// record NVIDIA GPU instruction stall reasons
void
gpu_metrics_GPU_INST_STALL_enable
(
 void
);


// record NVIDIA GPU instruction sampling statistics
void
gpu_metrics_GSAMP_enable
(
 void
);


//--------------------------------------------------
// record global memory access statistics
//--------------------------------------------------

void
gpu_metrics_GGMEM_enable
(
 void
);


//--------------------------------------------------
// record local memory access statistics
//--------------------------------------------------

void
gpu_metrics_GLMEM_enable
(
 void
);


//--------------------------------------------------
// record branch statistics
//--------------------------------------------------

void
gpu_metrics_GBR_enable
(
 void
);


//--------------------------------------------------
// attribute GPU measurements to an application 
// thread's calling context tree
//--------------------------------------------------

void
gpu_metrics_attribute
(
 gpu_activity_t *activity
);



#endif


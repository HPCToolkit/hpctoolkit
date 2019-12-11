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
// Copyright ((c)) 2002-2019, Rice University
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

#include "gpu-activity.h"



//*****************************************************************************
// macros
//*****************************************************************************

enum {
  GPU_INST_STALL_ALL = 0
} gpu_inst_stall_all_t;


typedef enum {
  GPU_GMEM_LD_CACHED_BYTES = 0,
  GPU_GMEM_LD_UNCACHED_BYTES = 1,
  GPU_GMEM_ST_BYTES = 2,
  GPU_GMEM_LD_CACHED_L2TRANS = 3,
  GPU_GMEM_LD_UNCACHED_L2TRANS = 4,
  GPU_GMEM_ST_L2TRANS = 5,
  GPU_GMEM_LD_CACHED_L2TRANS_THEOR = 6,
  GPU_GMEM_LD_UNCACHED_L2TRANS_THEOR = 7,
  GPU_GMEM_ST_L2TRANS_THEOR = 8
} gpu_gmem_ops_t;


typedef enum {
  GPU_LMEM_LD_BYTES = 0,
  GPU_LMEM_ST_BYTES = 1,
  GPU_LMEM_LD_TRANS = 2,
  GPU_LMEM_ST_TRANS = 3,
  GPU_LMEM_LD_TRANS_THEOR = 4,
  GPU_LMEM_ST_TRANS_THEOR = 5
} gpu_lmem_ops_t;



//--------------------------------------------------------------------------
// indexed metrics
//--------------------------------------------------------------------------

// gpu memory allocation/deallocation
#define FORALL_GMEM(macro)					\
  macro("GMEM:UNK (b)",           GPU_MEM_UNKNOWN)		\
  macro("GMEM:PAG (b)",           GPU_MEM_PAGEABLE)		\
  macro("GMEM:PIN (b)",           GPU_MEM_PINNED)		\
  macro("GMEM:DEV (b)",           GPU_MEM_DEVICE)		\
  macro("GMEM:ARY (b)",           GPU_MEM_ARRAY)		\
  macro("GMEM:MAN (b)",           GPU_MEM_MANAGED)		\
  macro("GMEM:DST (b)",           GPU_MEM_DEVICE_STATIC)	\
  macro("GMEM:MST (b)",           GPU_MEM_MANAGED_STATIC)


// gpu memory set
#define FORALL_GMSET(macro)					\
  macro("GMSET:UNK (b)",          GPU_MEM_UNKNOWN)		\
  macro("GMSET:PAG (b)",          GPU_MEM_PAGEABLE)		\
  macro("GMSET:PIN (b)",          GPU_MEM_PINNED)		\
  macro("GMSET:DEV (b)",          GPU_MEM_DEVICE)		\
  macro("GMSET:ARY (b)",          GPU_MEM_ARRAY)		\
  macro("GMSET:MAN (b)",          GPU_MEM_MANAGED)		\
  macro("GMSET:DST (b)",          GPU_MEM_DEVICE_STATIC)	\
  macro("GMSET:MST (b)",          GPU_MEM_MANAGED_STATIC)


#define FORALL_GPU_INST_STALL(macro)					\
  macro("GINS:STL_ALL",           GPU_INST_STALL_ALL)			\
  macro("GINS:STL_NONE",          GPU_INST_STALL_NONE)			\
  macro("GINS:STL_IFET",          GPU_INST_STALL_IFETCH)		\
  macro("GINS:STL_IDEP",          GPU_INST_STALL_IDEPEND)		\
  macro("GINS:STL_GMEM",          GPU_INST_STALL_GMEM)			\
  macro("GINS:STL_TMEM",          GPU_INST_STALL_TMEM)			\
  macro("GINS:STL_CMEM",          GPU_INST_STALL_CMEM)			\
  macro("GINS:STL_SYNC",          GPU_INST_STALL_SYNC)			\
  macro("GINS:STL_MTHR",          GPU_INST_STALL_MEM_THROTTLE)		\
  macro("GINS:STL_NSEL",          GPU_INST_STALL_NOT_SELECTED)		\
  macro("GINS:STL_OTHR",          GPU_INST_STALL_OTHER)			\
  macro("GINS:STL_SLP",           GPU_INST_STALL_SLEEP)			\
  macro("GINS:STL_INV",           GPU_INST_STALL_INVALID)		


// gpu explicit copy
#define FORALL_GXCOPY(macro)					\
  macro("GXCOPY:UNK (B)",         GPU_MEMCPY_UNK)		\
  macro("GXCOPY:H2D (B)",         GPU_MEMCPY_H2D)		\
  macro("GXCOPY:D2H (B)",         GPU_MEMCPY_D2H)		\
  macro("GXCOPY:H2A (B)",         GPU_MEMCPY_H2A)		\
  macro("GXCOPY:A2H (B)",         GPU_MEMCPY_A2H)		\
  macro("GXCOPY:A2A (B)",         GPU_MEMCPY_A2A)		\
  macro("GXCOPY:A2D (B)",         GPU_MEMCPY_A2D)		\
  macro("GXCOPY:D2A (B)",         GPU_MEMCPY_D2A)		\
  macro("GXCOPY:D2D (B)",         GPU_MEMCPY_D2D)		\
  macro("GXCOPY:H2H (B)",         GPU_MEMCPY_H2H)		\
  macro("GXCOPY:P2P (B)",         GPU_MEMCPY_P2P)


#define FORALL_GSYNC(macro)					\
  macro("GSYNC:UNK (us)",         GPU_SYNC_UNKNOWN)		\
  macro("GSYNC:EVT (us)",         GPU_SYNC_EVENT)		\
  macro("GSYNC:STRE (us)",        GPU_SYNC_STREAM_EVENT_WAIT)	\
  macro("GSYNC:STR (us)",         GPU_SYNC_STREAM)		\
  macro("GSYNC:CTX (us)",         GPU_SYNC_CONTEXT)

// gpu global memory access
#define FORALL_GGMEM(macro)						\
  macro("GGMEM:LDC (B)",          GPU_GMEM_LD_CACHED_BYTES)		\
  macro("GGMEM:LDU (B)",          GPU_GMEM_LD_UNCACHED_BYTES)		\
  macro("GGMEM:ST (B)",           GPU_GMEM_ST_BYTES)			\
  									\
  macro("GGMEM:LDC (L2T)",        GPU_GMEM_LD_CACHED_L2TRANS)		\
  macro("GGMEM:LDU (L2T)",        GPU_GMEM_LD_UNCACHED_L2TRANS)		\
  macro("GGMEM:ST (L2T)",         GPU_GMEM_ST_L2TRANS)			\
  				  					\
  macro("GGMEM:LDCT (L2T)",       GPU_GMEM_LD_CACHED_L2TRANS_THEOR)	\
  macro("GGMEM:LDUT (L2T)",       GPU_GMEM_LD_UNCACHED_L2TRANS_THEOR)	\
  macro("GGMEM:STT (L2T)",        GPU_GMEM_ST_L2TRANS_THEOR)


// gpu local memory access
#define FORALL_GLMEM(macro)					\
  macro("GLMEM:LD (B)",           GPU_LMEM_LD_BYTES)		\
  macro("GLMEM:ST (B)",           GPU_LMEM_ST_BYTES)		\
								\
  macro("GLMEM:LD (T)",           GPU_LMEM_LD_TRANS)		\
  macro("GLMEM:ST (T)",           GPU_LMEM_ST_TRANS)		\
								\
  macro("GLMEM:LDT (T)",          GPU_LMEM_LD_TRANS_THEOR)	\
  macro("GLMEM:STT (T)",          GPU_LMEM_ST_TRANS_THEOR)

//--------------------------------------------------------------------------
// scalar metrics 
//--------------------------------------------------------------------------


// gpu activity times
#define FORALL_GTIMES(macro)					\
  macro("GKER (us)",              GPU_TIME_KER)			\
  macro("GMEM (us)",              GPU_TIME_MEM)			\
  macro("GMSET (us)",             GPU_TIME_MSET)		\
  macro("GXCOPY (us)",            GPU_TIME_XCOPY)		\
  macro("GICOPY (us)",            GPU_TIME_ICOPY)		\
  macro("GSYNC (us)",             GPU_TIME_SYNC)		\


// gpu instruction count
#define FORALL_GPU_INST(macro)			\
  macro("GINS",                   GPU_INST_ALL)


// gpu kernel characteristics
#define FORALL_KINFO(macro)					\
  macro("GKER:STMEM (B)",         GPU_KINFO_STMEM)		\
  macro("GKER:DYMEM (B)",         GPU_KINFO_DYMEM)		\
  macro("GKER:LMEM (B)",          GPU_KINFO_LMEM)		\
  macro("GKER:FGP_ACT",           GPU_KINFO_FGP_ACT)		\
  macro("GKER:FGP_MAX",           GPU_KINFO_FGP_MAX)		\
  macro("GKER:THR_REG",           GPU_KINFO_REGISTERS)		\
  macro("GKER:BLK_THR",           GPU_KINFO_BLK_THREADS)	\
  macro("GKER:BLK_SM",            GPU_KINFO_BLK_SMEM)		\
  macro("GKER:COUNT ",            GPU_KINFO_COUNT)		\
  macro("GKER:OCC",               GPU_KINFO_OCCUPANCY)


// gpu implicit copy
#define FORALL_GICOPY(macro)					\
  macro("GICOPY:INV",             GPU_ICOPY_INVALID)		\
  macro("GICOPY:H2D (B)",         GPU_ICOPY_H2D_BYTES)		\
  macro("GICOPY:D2H (B)",         GPU_ICOPY_D2H_BYTES)		\
  macro("GICOPY:D2D (B)",         GPU_ICOPY_D2D_BYTES)		\
  macro("GICOPY:CPU_PF",          GPU_ICOPY_CPU_PF)		\
  macro("GICOPY:GPU_PF",          GPU_ICOPY_GPU_PF)		\
  macro("GICOPY:THRASH",          GPU_ICOPY_THRASH)		\
  macro("GICOPY:THROT",           GPU_ICOPY_THROT)		\
  macro("GICOPY:RMAP",            GPU_ICOPY_RMAP)		


// gpu branch
#define FORALL_GBR(macro)					\
  macro("GBR:DIV",                GPU_BR_DIVERGED)		\
  macro("GBR:EXE",                GPU_BR_EXECUTED)


// gpu sampling information
#define FORALL_GSAMP_INT(macro)					\
  macro("GSAMP:DRP",              GPU_SAMPLE_DROPPED)		\
  macro("GSAMP:EXP",              GPU_SAMPLE_EXPECTED)		\
  macro("GSAMP:TOT",              GPU_SAMPLE_TOTAL)		\
  macro("GSAMP:PER (cyc)",        GPU_SAMPLE_PERIOD)

#define FORALL_GSAMP_REAL(macro)				\
  macro("GSAMP:UTIL",             GPU_SAMPLE_UTILIZATION)

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


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
// **

#ifndef __GPU_BLAME_H__
#define __GPU_BLAME_H__
#include <cuda.h>
#include <cuda_runtime.h>

//#ifdef ENABLE_CUDA

//#include "gpu_blame-cuda-runtime-header.h"
//#include "gpu_blame-cuda-driver-header.h"
#include <lib/prof-lean/stdatomic.h>		// atomic_uint_fast64_t
#include <hpcrun/core_profile_trace_data.h>
#include <hpcrun/main.h>

//
// Blame shiting interface
//
#define MAX_STREAMS (500)

// Visible types

extern bool g_cpu_gpu_enabled;

// num threads in the process
extern atomic_uint_fast64_t g_active_threads;

// Visible function declarations
extern  void hpcrun_stream_finalize(void* st);
extern void hpcrun_set_gpu_proxy_present();

//#endif
#endif

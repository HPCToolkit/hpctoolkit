// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// File: ss-list.h
//
// Purpose:
//   This file contains a list of sample sources wrapped by a call to an
//   unspecified macro. The intended use of this file is to define the
//   macro, include the file elsewhere one or more times to register the
//   sample sources. This is not defined as a FORALL macro that applies
//   a macro to each of the sample source names so that this file can
//   contain ifdefs if a sample source is unused on a platform.
//
//******************************************************************************

SAMPLE_SOURCE_DECL_MACRO(ga)
SAMPLE_SOURCE_DECL_MACRO(io)
#ifdef ENABLE_CLOCK_REALTIME
SAMPLE_SOURCE_DECL_MACRO(itimer)
#endif

#ifdef HPCRUN_SS_LINUX_PERF
SAMPLE_SOURCE_DECL_MACRO(linux_perf)
#endif

SAMPLE_SOURCE_DECL_MACRO(memleak)

SAMPLE_SOURCE_DECL_MACRO(none)

#ifdef HPCRUN_SS_PAPI
SAMPLE_SOURCE_DECL_MACRO(papi)
#endif

SAMPLE_SOURCE_DECL_MACRO(directed_blame)

#ifdef HOST_CPU_x86_64
SAMPLE_SOURCE_DECL_MACRO(retcnt)
#endif

#ifdef HPCRUN_SS_PAPI_C_CUPTI
SAMPLE_SOURCE_DECL_MACRO(papi_c_cupti)
#endif

#ifdef HPCRUN_SS_PAPI_C_INTEL
SAMPLE_SOURCE_DECL_MACRO(papi_c_intel)
#endif

#ifdef HPCRUN_SS_NVIDIA
SAMPLE_SOURCE_DECL_MACRO(nvidia_gpu)
#endif

#ifdef HPCRUN_SS_AMD
SAMPLE_SOURCE_DECL_MACRO(amd_gpu)
#endif

#ifdef HPCRUN_SS_AMD
SAMPLE_SOURCE_DECL_MACRO(openmp_gpu)
#endif

#ifdef HPCRUN_SS_AMD
SAMPLE_SOURCE_DECL_MACRO(amd_rocprof)
#endif

#ifdef HPCRUN_SS_LEVEL0
SAMPLE_SOURCE_DECL_MACRO(level0)
#endif
#ifdef HPCRUN_SS_OPENCL
SAMPLE_SOURCE_DECL_MACRO(opencl)
#endif

#ifdef HPCRUN_CPU_GPU_IDLE
SAMPLE_SOURCE_DECL_MACRO(cpu_gpu_idle)
#endif

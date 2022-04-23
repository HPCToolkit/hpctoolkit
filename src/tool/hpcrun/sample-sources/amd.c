// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2022, Rice University
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

#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>

#ifndef HPCRUN_STATIC_LINK
#include <dlfcn.h>
#endif

#include "amd.h"
#include "common.h"
#include "libdl.h"
#include "sample_source_obj.h"
#include "simple_oo.h"

#include "hpcrun/control-knob.h"
#include "hpcrun/device-finalizers.h"
#include "hpcrun/gpu/amd/hip-api.h"
#include "hpcrun/gpu/amd/rocprofiler-api.h"
#include "hpcrun/gpu/amd/roctracer-api.h"
#include "hpcrun/gpu/gpu-activity.h"
#include "hpcrun/gpu/gpu-metrics.h"
#include "hpcrun/gpu/gpu-trace.h"
#include "hpcrun/hpcrun_options.h"
#include "hpcrun/hpcrun_stats.h"
#include "hpcrun/metrics.h"
#include "hpcrun/module-ignore-map.h"
#include "hpcrun/ompt/ompt-interface.h"
#include "hpcrun/safe-sampling.h"
#include "hpcrun/sample_event.h"
#include "hpcrun/sample_sources_registered.h"
#include "hpcrun/thread_data.h"
#include "hpcrun/trace.h"

#include "lib/prof-lean/hpcrun-fmt.h"

#include <lush/lush-backtrace.h>
#include <messages/messages.h>
#include <monitor.h>
#include <roctracer_hip.h>
#include <utilities/tokenize.h>

#define AMD_ROCM "gpu=amd"

static device_finalizer_fn_entry_t device_finalizer_flush;
static device_finalizer_fn_entry_t device_finalizer_shutdown;
static device_finalizer_fn_entry_t device_trace_finalizer_shutdown;

static void METHOD_FN(init) {
  self->state = INIT;
}

static void METHOD_FN(thread_init) {
  TMSG(CUDA, "thread_init");
}

static void METHOD_FN(thread_init_action) {
  TMSG(CUDA, "thread_init_action");
}

static void METHOD_FN(start) {
  TMSG(CUDA, "start");
  TD_GET(ss_state)[self->sel_idx] = START;
}

static void METHOD_FN(thread_fini_action) {
  TMSG(CUDA, "thread_fini_action");
}

static void METHOD_FN(stop) {
  hpcrun_get_thread_data();

  TD_GET(ss_state)[self->sel_idx] = STOP;
}

static void METHOD_FN(shutdown) {
  self->state = UNINIT;
}

static bool METHOD_FN(supports_event, const char* ev_str) {
#ifndef HPCRUN_STATIC_LINK
  return hpcrun_ev_is(ev_str, AMD_ROCM);
#else
  return false;
#endif
}

static void METHOD_FN(process_event_list, int lush_metrics) {
  int nevents = (self->evl).nevents;
  gpu_metrics_default_enable();
  hpcrun_set_trace_metric(HPCRUN_GPU_TRACE_FLAG);
  TMSG(CUDA, "nevents = %d", nevents);

#ifndef HPCRUN_STATIC_LINK
  if (hip_bind()) {
    EEMSG("hpcrun: unable to bind to HIP AMD library %s\n", dlerror());
    monitor_real_exit(-1);
  }
#endif
}

static void METHOD_FN(finalize_event_list) {
#ifndef HPCRUN_STATIC_LINK
  if (roctracer_bind() != DYNAMIC_BINDING_STATUS_OK) {
    EEMSG("hpcrun: unable to bind to AMD roctracer library %s\n", dlerror());
    monitor_real_exit(-1);
  }
#endif

#if 0
  // Fetch the event string for the sample source
  // only one event is allowed
  char* evlist = METHOD_CALL(self, get_event_str);
  char* event = start_tok(evlist);
#endif
  roctracer_init();

  device_finalizer_flush.fn = roctracer_flush;
  device_finalizer_register(device_finalizer_type_flush, &device_finalizer_flush);

  device_finalizer_shutdown.fn = roctracer_fini;
  device_finalizer_register(device_finalizer_type_shutdown, &device_finalizer_shutdown);

  // initialize gpu tracing
  gpu_trace_init();

  // Register shutdown function to finalize gpu tracing and write trace files
  device_trace_finalizer_shutdown.fn = gpu_trace_fini;
  device_finalizer_register(device_finalizer_type_shutdown, &device_trace_finalizer_shutdown);
}

static void METHOD_FN(gen_event_set, int lush_metrics) {}

static void METHOD_FN(display_events) {
  printf("===========================================================================\n");
  printf("Available AMD GPU events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf(
      "%s\t\tComprehensive operation-level monitoring on an AMD GPU.\n"
      "\t\tCollect timing information on GPU kernel invocations,\n"
      "\t\tmemory copies, etc.\n",
      AMD_ROCM);
  printf("\n");
}

#define ss_name amd_gpu
#define ss_cls  SS_HARDWARE

#include "ss_obj.h"

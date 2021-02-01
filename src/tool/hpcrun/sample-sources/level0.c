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
// Copyright ((c)) 2002-2020, Rice University
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

//******************************************************************************
// system includes
//******************************************************************************

#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <stdbool.h>

#include <pthread.h>

#ifndef HPCRUN_STATIC_LINK
#include <dlfcn.h>
#endif



//******************************************************************************
// libmonitor
//******************************************************************************

#include <monitor.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "libdl.h"
#include "level0.h"

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"

#include <hpcrun/control-knob.h>
#include <hpcrun/device-finalizers.h>
#include <hpcrun/gpu/amd/roctracer-api.h>
#include <hpcrun/gpu/gpu-activity.h>
#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/gpu/gpu-trace.h>
#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/metrics.h>
#include <hpcrun/module-ignore-map.h>
#include <hpcrun/ompt/ompt-interface.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>

#include <utilities/tokenize.h>
#include <messages/messages.h>
#include <lush/lush-backtrace.h>
#include <lib/prof-lean/hpcrun-fmt.h>




//******************************************************************************
// macros
//******************************************************************************

#define LEVEL0 "gpu=level0"

static device_finalizer_fn_entry_t device_finalizer_shutdown;

//******************************************************************************
// interface operations
//******************************************************************************

static void
METHOD_FN(init)
{
  self->state = INIT;
}


static void
METHOD_FN(thread_init)
{
  TMSG(CUDA, "thread_init");
}


static void
METHOD_FN(thread_init_action)
{
  TMSG(CUDA, "thread_init_action");
}


static void
METHOD_FN(start)
{
  TMSG(CUDA, "start");
}


static void
METHOD_FN(thread_fini_action)
{
  TMSG(CUDA, "thread_fini_action");
}


static void
METHOD_FN(stop)
{
  hpcrun_get_thread_data();
  TD_GET(ss_state)[self->sel_idx] = STOP;
}


static void
METHOD_FN(shutdown)
{
  self->state = UNINIT;
}


static bool
METHOD_FN(supports_event, const char *ev_str)
{
#ifndef HPCRUN_STATIC_LINK
  return hpcrun_ev_is(ev_str, LEVEL0);
#else
  return false;
#endif


}

static void
METHOD_FN(process_event_list, int lush_metrics)
{
  int nevents = (self->evl).nevents;
  gpu_metrics_default_enable();
  TMSG(CUDA,"nevents = %d", nevents);
}

static void
METHOD_FN(finalize_event_list)
{
#ifndef HPCRUN_STATIC_LINK
  if (level0_bind() != DYNAMIC_BINDING_STATUS_OK) {
    EEMSG("hpcrun: unable to bind to Level0 library %s\n", dlerror());
    monitor_real_exit(-1);
  }
#endif
  level0_init();

  // Init records
  gpu_trace_init();

  device_finalizer_shutdown.fn = level0_fini;
  device_finalizer_register(device_finalizer_type_shutdown, &device_finalizer_shutdown);
}


static void
METHOD_FN(gen_event_set,int lush_metrics)
{
}


static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available Level0 GPU events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("%s\t\tOperation-level monitoring on an Intel GPU.\n"
	 "\t\tCollect timing information on GPU kernel invocations,\n"
	 "\t\tmemory copies, etc.\n",
	 LEVEL0);
  printf("\n");
}



//**************************************************************************
// object
//**************************************************************************

#define ss_name level0
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

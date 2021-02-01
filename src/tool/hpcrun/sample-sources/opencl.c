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
// local includes
//******************************************************************************

#include "common.h"

#include <monitor.h> 

#include <hpcrun/device-finalizers.h>
#include <hpcrun/gpu/gpu-trace.h>
#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/gpu/gpu-trace.h>
#include <hpcrun/gpu/opencl/opencl-api.h>
#include <hpcrun/thread_data.h>

#include <messages/messages.h>

#include <utilities/tokenize.h>



//******************************************************************************
// type declarations
//******************************************************************************

#define GPU_STRING "gpu=opencl"
#define ENABLE_INSTRUMENTATION "gpu=opencl,inst"
#define NO_THRESHOLD  1L

static device_finalizer_fn_entry_t device_finalizer_flush;
static device_finalizer_fn_entry_t device_finalizer_shutdown;


//******************************************************************************
// type declarations
//******************************************************************************

static char opencl_name[128];



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
  TMSG(OPENCL, "thread_init");
}


static void
METHOD_FN(thread_init_action)
{
  TMSG(OPENCL, "thread_init_action");
}


static void
METHOD_FN(start)
{
  TMSG(OPENCL, "start");
}


static void
METHOD_FN(thread_fini_action)
{
  TMSG(OPENCL, "thread_fini_action");
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
  return (hpcrun_ev_is(ev_str, GPU_STRING) || hpcrun_ev_is(ev_str, ENABLE_INSTRUMENTATION));
  #else
  return false;
  #endif
}


static void
METHOD_FN(process_event_list, int lush_metrics)
{
  int nevents = (self->evl).nevents;
  TMSG(OPENCL,"nevents = %d", nevents);
  gpu_metrics_default_enable();
  gpu_metrics_KINFO_enable();

  char* evlist = METHOD_CALL(self, get_event_str);
  char* event = start_tok(evlist);
  long th;
  hpcrun_extract_ev_thresh(event, sizeof(opencl_name), opencl_name,
    &th, NO_THRESHOLD);

  if (hpcrun_ev_is(opencl_name, GPU_STRING)) {
  } else if (hpcrun_ev_is(opencl_name, ENABLE_INSTRUMENTATION)) {
    gpu_metrics_GPU_INST_enable();
    opencl_instrumentation_enable();
  }
}


static void
METHOD_FN(finalize_event_list)
{
  #ifndef HPCRUN_STATIC_LINK
  if (opencl_bind()) {
    EEMSG("hpcrun: unable to bind to opencl library %s\n", dlerror());
    monitor_real_exit(-1);
  }
  #endif
  opencl_api_initialize();

  device_finalizer_flush.fn = opencl_api_thread_finalize;
  device_finalizer_register(device_finalizer_type_flush, &device_finalizer_flush);

  device_finalizer_shutdown.fn = opencl_api_process_finalize;
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
  printf("Available OPENCL GPU events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("%s\t\tOperation-level monitoring for opencl on a GPU.\n"
    "\t\tCollect timing information on GPU kernel invocations,\n"
    "\t\tmemory copies, etc.\n",
    GPU_STRING);
  printf("\n");
}



//**************************************************************************
// object
//**************************************************************************

#define ss_name opencl
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

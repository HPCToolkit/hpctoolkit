// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/sample-sources/papi.c $
// $Id: papi.c 3328 2010-12-23 23:39:09Z tallent $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2017, Rice University
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

//
// CUPTI synchronous sampling via PAPI sample source simple oo interface
//

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <papi.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <stdbool.h>

#include <pthread.h>

#include <cuda.h>
#include <cupti.h>

/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/metrics.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>
#include <utilities/tokenize.h>
#include <messages/messages.h>
#include <lush/lush-backtrace.h>
#include <lib/prof-lean/hpcrun-fmt.h>


/******************************************************************************
 * macros
 *****************************************************************************/


#define OVERFLOW_MODE 0
#define NO_THRESHOLD  1L

#define PAPI_CUDA_COMPONENT_ID 1
#define CUPTI_LAUNCH_CALLBACK_DEPTH 7



/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static void hpcrun_cuda_kernel_callback(void *userdata, 
					CUpti_CallbackDomain domain,
					CUpti_CallbackId cbid, 
					const CUpti_CallbackData *cbInfo);

static void check_cupti_error(int err, char *cuptifunc);

static void event_fatal_error(int ev_code, int papi_ret);

/******************************************************************************
 * interface operations
 *****************************************************************************/

static void
METHOD_FN(init)
{
  self->state = INIT;
}

static void
METHOD_FN(thread_init)
{
  TMSG(CUDA, "thread init OK");
}

static void
METHOD_FN(thread_init_action)
{
  TMSG(CUDA, "thread_init_action ok");
}

static void
METHOD_FN(start)
{
}

static void
METHOD_FN(thread_fini_action)
{
}

static void
METHOD_FN(stop)
{
  thread_data_t *td = hpcrun_get_thread_data();

  TD_GET(ss_state)[self->evset_idx] = STOP;
}

static void
METHOD_FN(shutdown)
{
  self->state = UNINIT;
}


static bool
METHOD_FN(supports_event,const char *ev_str)
{
  if (self->state == UNINIT){
    METHOD_CALL(self, init);
  }
  
  char evtmp[1024];
  int ec;
  long th;

  hpcrun_extract_ev_thresh(ev_str, sizeof(evtmp), evtmp, &th, 
			   NO_THRESHOLD);

  return 0;
}
 
static void
METHOD_FN(process_event_list, int lush_metrics)
{
  int nevents = (self->evl).nevents;

  TMSG(CUDA,"nevents = %d", nevents);

  hpcrun_pre_allocate_metrics(nevents + num_lush_metrics);

  for (i = 0; i < nevents; i++) {
    char buffer[PAPI_MAX_STR_LEN];
    int metric_id = hpcrun_new_metric(); /* weight */
    METHOD_CALL(self, store_metric_id, i, metric_id);
    PAPI_event_code_to_name(self->evl.events[i].event, buffer);
    TMSG(CUDA, "metric for event %d = %s", i, buffer);
    hpcrun_set_metric_info_and_period(metric_id, strdup(buffer),
				      MetricFlags_ValFmt_Int,
				      self->evl.events[i].thresh);
  }
}

static void
METHOD_FN(gen_event_set,int lush_metrics)
{
}

static void
METHOD_FN(display_events)
{
  PAPI_event_info_t info;
  char name[200];
  int ev, ret, num_total;

  printf("===========================================================================\n");
  printf("Available CUDA events\n");
  printf("===========================================================================\n");
  printf("Name\t\t\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");

  printf("Total CUDA events: %d\n", num_total);
  printf("\n");
}

/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name cuda
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

/******************************************************************************
 * private operations 
 *****************************************************************************/








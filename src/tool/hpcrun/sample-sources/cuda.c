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
  PAPI_set_debug(0x3ff);

  // PAPI_library_init spawns threads yields DEADLOCK !!!
  // --- Ignore new threads for init call ---
  //
  monitor_disable_new_threads();
  int ret = PAPI_library_init(PAPI_VER_CURRENT);
  monitor_enable_new_threads();

  TMSG(CUDA,"PAPI_library_init = %d", ret);
  TMSG(CUDA,"PAPI_VER_CURRENT =  %d", PAPI_VER_CURRENT);
  if (ret != PAPI_VER_CURRENT){
    STDERR_MSG("Fatal error: PAPI_library_init() failed with version mismatch.\n"
        "HPCToolkit was compiled with version 0x%x but run on version 0x%x.\n"
        "Check the HPCToolkit installation and try again.",
	PAPI_VER_CURRENT, ret);
    exit(1);
  }
  self->state = INIT;
}

static void
METHOD_FN(thread_init)
{
  TMSG(CUDA, "thread init");
  int retval = PAPI_thread_init(pthread_self);
  if (retval != PAPI_OK) {
    EEMSG("PAPI_thread_init NOT ok, retval = %d", retval);
    monitor_real_abort();
  }
  TMSG(CUDA, "thread init OK");
}

static void
METHOD_FN(thread_init_action)
{
  TMSG(CUDA, "register thread");
  int retval = PAPI_register_thread();
  if (retval != PAPI_OK) {
    EEMSG("PAPI_register_thread NOT ok, retval = %d", retval);
    monitor_real_abort();
  }
  TMSG(CUDA, "register thread ok");
}

static void
METHOD_FN(start)
{
  int cuptiErr;
  CUpti_SubscriberHandle subscriber;

  TMSG(CUDA,"start called");

  cuptiErr = cuptiSubscribe(&subscriber, 
			    (CUpti_CallbackFunc)hpcrun_cuda_kernel_callback, 
			    (void *) NULL);
  check_cupti_error(cuptiErr, "cuptiSubscribe");

  cuptiErr = cuptiEnableCallback(1, subscriber, CUPTI_CB_DOMAIN_RUNTIME_API, 
                                 CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020);
  check_cupti_error(cuptiErr, "cuptiEnableCallback");

  TD_GET(ss_state)[self->evset_idx] = START;
}

static void
METHOD_FN(thread_fini_action)
{
  TMSG(CUDA, "unregister thread");
  int rval = PAPI_unregister_thread();
  if (rval != PAPI_OK) {
    TMSG(CUDA, "warning: CUDA PAPI_unregister_thread (%d): %s.", 
	rval, PAPI_strerror(rval));
  }
}

static void
METHOD_FN(stop)
{
  thread_data_t *td = hpcrun_get_thread_data();

  int eventSet = td->eventSet[self->evset_idx];

  source_state_t my_state = TD_GET(ss_state)[self->evset_idx];

  TMSG(CUDA,"stop called");

  if (my_state == STOP) {
    TMSG(CUDA,"PAPI CUDA stop called on an already stopped event set %d",eventSet);
    return;
  }

  if (my_state != START) {
    TMSG(CUDA,"*WARNING* PAPI CUDA stop called on event set that has not been started");
    return;
  }

  TD_GET(ss_state)[self->evset_idx] = STOP;
}

static void
METHOD_FN(shutdown)
{
  int thr_id = TD_GET(id);
  EEMSG("CUDA/PAPI shutdown from thread %d", thr_id);
  if (thr_id != 0) {
    EMSG("Shutdown op for cuda sample source called from thread %d", thr_id);
    return;
  }

  METHOD_CALL(self, stop); // make sure stop has been called

  thread_data_t *td = hpcrun_get_thread_data();
  int eventSet = td->eventSet[self->evset_idx];
  
  int rval; // for PAPI return codes

  /* Error need not be fatal -- we've already got our data! */
  rval = PAPI_cleanup_eventset(eventSet);
  if (rval != PAPI_OK) {
    TMSG(CUDA, "warning: CUDA PAPI_cleanup_eventset (%d): %s.", 
	rval, PAPI_strerror(rval));
  }

  rval = PAPI_destroy_eventset(&eventSet);
  if (rval != PAPI_OK) {
    TMSG(CUDA, "warning: CUDA PAPI_destroy_eventset (%d): %s.", 
	rval, PAPI_strerror(rval));
  }

  td->eventSet[self->evset_idx] = PAPI_NULL;

  PAPI_shutdown();

  self->state = UNINIT;
}


#define CUDA_PREFIX "CUDA."

// Return true if PAPI recognizes the name, whether supported or not.
// We'll handle unsupported events later.
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

  // handle only events for the CUDA component
  if (strncmp(evtmp, CUDA_PREFIX, strlen(CUDA_PREFIX)) == 0) {
    return PAPI_event_name_to_code(evtmp, &ec) == PAPI_OK;
  }
  return 0;
}
 
static void
METHOD_FN(process_event_list, int lush_metrics)
{
  char *event;
  int i, ret;
  int num_lush_metrics = 0;

  hpcrun_set_trace_metric(HPCRUN_GPU_TRACE_FLAG);
  char* evlist = METHOD_CALL(self, get_event_str);
  for (event = start_tok(evlist); more_tok(); event = next_tok()) {
    char name[1024];
    int evcode;
    long thresh;

    TMSG(CUDA,"checking event spec = %s",event);
    if (hpcrun_extract_ev_thresh(event, sizeof(name), name, &thresh, 
				 NO_THRESHOLD)) {
      AMSG("WARNING: %s is specified with a sampling threshold. "
	   "No thresholds supported for CUDA events", name);
    }
    ret = PAPI_event_name_to_code(name, &evcode);
    if (ret != PAPI_OK) {
      EMSG("unexpected failure in PAPI process_event_list(): "
	   "PAPI_event_name_to_code() returned %s (%d)",
	   PAPI_strerror(ret), ret);
      hpcrun_ssfail_unsupported("PAPI", name);
    }
    if (PAPI_query_event(evcode) != PAPI_OK) {
      hpcrun_ssfail_unsupported("PAPI", name);
    }

    TMSG(CUDA,"got event code = %x, thresh = %ld", evcode, thresh);
    METHOD_CALL(self, store_event, evcode, NO_THRESHOLD);
  }
  int nevents = (self->evl).nevents;
  TMSG(CUDA,"nevents = %d", nevents);

  hpcrun_pre_allocate_metrics(nevents + num_lush_metrics);

  kind_info_t *cuda_kind = hpcrun_metrics_new_kind();

  for (i = 0; i < nevents; i++) {
    char buffer[PAPI_MAX_STR_LEN];
    PAPI_event_code_to_name(self->evl.events[i].event, buffer);
    TMSG(CUDA, "metric for event %d = %s", i, buffer);
    int metric_id = /* weight */
      hpcrun_set_new_metric_info_and_period(cuda_kind, strdup(buffer),
        MetricFlags_ValFmt_Int, self->evl.events[i].thresh, metric_property_none);
    METHOD_CALL(self, store_metric_id, i, metric_id);
  }

  hpcrun_close_kind(cuda_kind);
}

static void
METHOD_FN(finalize_event_list)
{
}

static void
METHOD_FN(gen_event_set,int lush_metrics)
{
  int i;
  int ret;
  int eventSet;

  eventSet = PAPI_NULL;
  TMSG(CUDA,"create event set");
  ret = PAPI_create_eventset(&eventSet);
  TMSG(CUDA,"PAPI_create_eventset = %d, eventSet = %d", ret, eventSet);
  if (ret != PAPI_OK) {
    hpcrun_abort("Failure: PAPI_create_eventset.Return code = %d ==> %s", 
		 ret, PAPI_strerror(ret));
  }

  int nevents = (self->evl).nevents;
  for (i = 0; i < nevents; i++) {
    int evcode = self->evl.events[i].event;
    ret = PAPI_add_event(eventSet, evcode);
    TMSG(CUDA, "PAPI_add_event(eventSet=%d, event_code=%x)", eventSet, evcode);
    if (ret != PAPI_OK) {
      EMSG("failure in PAPI gen_event_set(): "
	   "PAPI_add_event() returned: %s (%d)",
	   PAPI_strerror(ret), ret);
      event_fatal_error(evcode, ret);
    }
  }

  thread_data_t *td = hpcrun_get_thread_data();
  td->eventSet[self->evset_idx] = eventSet;
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

#ifdef PAPI_COMPONENT_STUFF_FIGURED_OUT
  const PAPI_component_info_t *pci = PAPI_get_component_info(1);
  printf("PAPI component name '%s' '%s' '%s' '%s'\n", pci->name, pci->version, 
	 pci->support_version, pci->kernel_version);
#endif // PAPI_COMPONENT_STUFF_FIGURED_OUT

  num_total = 0;
  ev = PAPI_NATIVE_MASK |  PAPI_COMPONENT_MASK(PAPI_CUDA_COMPONENT_ID);
  ret = PAPI_OK;
#ifdef PAPI_ENUM_FIRST
  ret = PAPI_enum_event(&ev, PAPI_ENUM_FIRST);
#endif
  while (ret == PAPI_OK) {
    if (PAPI_query_event(ev) == PAPI_OK) {
      PAPI_event_code_to_name(ev, name);
      if (strncmp(name, CUDA_PREFIX, strlen(CUDA_PREFIX)) == 0 || 1) { 
      	PAPI_get_event_info(ev, &info);
      	num_total++;
      	printf("%-30s\t%s\n", name, info.long_descr);
      }
    }
    ret = PAPI_enum_event(&ev, PAPI_ENUM_EVENTS);
  }
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

static void
event_fatal_error(int ev_code, int papi_ret)
{
  char name[1024];

  PAPI_event_code_to_name(ev_code, name);
  if (PAPI_query_event(ev_code) != PAPI_OK) {
    hpcrun_ssfail_unsupported("CUDA", name);
  }
  if (papi_ret == PAPI_ECNFLCT) {
    hpcrun_ssfail_conflict("CUDA", name);
  }
  hpcrun_ssfail_unsupported("CUDA", name);
}


static void 
check_cupti_error(int err, char *cuptifunc)			
{
  if (err != CUPTI_SUCCESS) {
    const char *errstr;                                     
    cuptiGetResultString(err, &errstr);                    
#ifdef CUPTI_ERRORS_UNMYSTIFIED
    hpcrun_abort("error: CUDA CUPTI API function '%s' "
		 "failed with message '%s' \n", cuptifunc, errstr);
#endif // CUPTI_ERRORS_UNMYSTIFIED
  }
}

void CUPTIAPI
hpcrun_cuda_kernel_callback(void *userdata,
			    CUpti_CallbackDomain domain,
			    CUpti_CallbackId cbid, 
			    const CUpti_CallbackData *cbInfo)
{
  thread_data_t* td = hpcrun_get_thread_data();
  sample_source_t* self = &obj_name();

  int nevents  = self->evl.nevents;
  int cudaEventSet = td->eventSet[self->evset_idx];
  
  // This callback is enabled only for kernel launch; anything else is an error.
  if (cbid != CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020) {
    hpcrun_abort("CUDA CUPTI callback seen for unexpected "
		 "interface operation: callback id  %d\n", cbid); 
  }

  if (cbInfo->callbackSite == CUPTI_API_ENTER) {
    cudaThreadSynchronize();

    TMSG(CUDA,"starting CUDA monitoring w event set %d",cudaEventSet);
    int ret = PAPI_start(cudaEventSet);
    if (ret != PAPI_OK){
      EMSG("CUDA monitoring failed to start. PAPI_start failed with %s (%d)", 
	   PAPI_strerror(ret), ret);
    }  
  }
    
  if (cbInfo->callbackSite == CUPTI_API_EXIT) {
    cudaThreadSynchronize();
    long_long *eventValues = 
      (long_long *) alloca(sizeof(long_long) * (nevents+2));

    TMSG(CUDA,"stopping CUDA monitoring w event set %d",cudaEventSet);
    PAPI_stop(cudaEventSet, eventValues);
    TMSG(CUDA,"stopped CUDA monitoring w event set %d",cudaEventSet);

    ucontext_t uc;
    TMSG(CUDA,"getting context in CUDA event handler");
    getcontext(&uc);
    TMSG(CUDA,"got context in CUDA event handler");
    hpcrun_async_block();
    TMSG(CUDA,"blocked async event in CUDA event handler");
    {
      int i;
      for (i = 0; i < nevents; i++) 
	{
	  int metric_id = hpcrun_event2metric(&_cuda_obj, i);

	  TMSG(CUDA, "sampling call path for metric_id = %d", metric_id);
	  hpcrun_sample_callpath(&uc, metric_id, eventValues[i]/*metricIncr*/, 
				 CUPTI_LAUNCH_CALLBACK_DEPTH/*skipInner*/, 
				 0/*isSync*/, NULL);
	  TMSG(CUDA, "sampled call path for metric_id = %d", metric_id);
	}
    }
    TMSG(CUDA,"unblocking async event in CUDA event handler");
    hpcrun_async_unblock();
    TMSG(CUDA,"unblocked async event in CUDA event handler");
  }
}

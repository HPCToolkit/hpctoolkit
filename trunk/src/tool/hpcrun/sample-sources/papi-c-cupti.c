// ******************* System Includes ********************
#include <ucontext.h> 
#include <dlfcn.h>

#include <stdbool.h>
#include <string.h>
#include <stdint.h>
// *********************************************************


// ******************** PAPI *******************************
#include <papi.h>
// *********************************************************

// ******************** MONITOR *******************************
#include <monitor.h>
// *********************************************************

// ******************** GPU includes ***********************
#include <cuda_runtime_api.h>
#include <cupti.h>
// *********************************************************

// ******* HPCToolkit Includes *********************************
#include <lib/prof-lean/spinlock.h>

#include <hpcrun/thread_data.h>
#include <messages/messages.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_sources_all.h>
#include <sample-sources/common.h>
#include <sample-sources/ss-obj-name.h>
// *********************************************************

// ******** local includes ***********
#include "papi-c.h"
#include "papi-c-extended-info.h"
// ***********************************

// ****************** Convenience macros *******************

#define CUPTI_LAUNCH_CALLBACK_DEPTH 7

#define Cupti_call(fn, ...)                                    \
{                                                              \
  int ret = fn(__VA_ARGS__);                                   \
  if (ret != CUPTI_SUCCESS) {                                  \
    const char* errstr;                                        \
    dcuptiGetResultString(ret, &errstr);                        \
    hpcrun_abort("error: CUDA/CUPTI API "                      \
                 #fn " failed w error code %d ==> '%s'\n",     \
                 ret, errstr);                                 \
  }                                                            \
}

#define Cupti_call_silent(fn, ...)                             \
{                                                              \
  (void) fn(__VA_ARGS__);                                      \
}

#define Chk_dlopen(v, lib, flags)                     \
  void* v = monitor_real_dlopen(lib, flags);          \
  if (! v) {                                          \
    fprintf(stderr, "gpu dlopen %s failed\n", lib);   \
    return;                                           \
  }                                                   \

#define Chk_dlsym(h, fn) {                                \
  dlerror();                                              \
  d ## fn = dlsym(h, #fn);                                \
  char* e = dlerror();                                    \
  if (e) {                                                \
    fprintf(stderr, "dlsym(%s) fails w '%s'\n", #fn, e);  \
    return;                                               \
  }                                                       \
}
// ***********************************************************

typedef struct {
  int nevents;
  int event_set;
  sample_source_t* self;
} papi_cuda_data_t;

static bool event_set_created = false;
static bool event_set_finalized = false;

static papi_cuda_data_t local = {};

static spinlock_t cupti_lock = SPINLOCK_UNLOCKED;
static spinlock_t setup_lock = SPINLOCK_UNLOCKED;

// ******************** cuda/cupti functions ***********************
// Some cuda/cupti functions must not be wrapped! So, we fetch them via dlopen.
// NOTE: naming convention is to prepend the letter "d" to the actual function
// The indirect functions are below.
//
cudaError_t (*dcudaThreadSynchronize)(void);

CUptiResult (*dcuptiGetResultString)(CUptiResult result, const char** str); 

CUptiResult (*dcuptiSubscribe)(CUpti_SubscriberHandle* subscriber,
                               CUpti_CallbackFunc callback, 
                               void* userdata);

CUptiResult (*dcuptiEnableCallback)(uint32_t enable,
                                    CUpti_SubscriberHandle subscriber, 
                                    CUpti_CallbackDomain domain,
                                    CUpti_CallbackId cbid);

CUptiResult (*dcuptiUnsubscribe)(CUpti_SubscriberHandle subscriber); 


// *****************************************************************
typedef struct cuda_callback_t {
  sample_source_t* ss;
  int event_set;
} cuda_callback_t;

//
// populate the cuda/cupti functions via dlopen
//

static void
dlgpu(void)
{
  // only use dlfunctions in NON static case
#ifndef HPCRUN_STATIC_LINK
  Chk_dlopen(cudart, "libcudart.so", RTLD_NOW | RTLD_GLOBAL);
  Chk_dlsym(cudart, cudaThreadSynchronize);

  Chk_dlopen(cupti, "libcupti.so", RTLD_NOW | RTLD_GLOBAL);
  Chk_dlsym(cupti, cuptiGetResultString);
  Chk_dlsym(cupti, cuptiSubscribe);
  Chk_dlsym(cupti, cuptiEnableCallback);
  Chk_dlsym(cupti, cuptiUnsubscribe);
#endif // ! HPCRUN_STATIC_LINK
}

//
// noop routine
//
static void
papi_c_no_action(void)
{
  ;
}

//
// Predicate to determine if this component is being referenced
//
static bool
is_papi_c_cuda(const char* name)
{
  return strstr(name, "cuda") == name;
}

static void CUPTIAPI
hpcrun_cuda_kernel_callback(void* userdata,
			    CUpti_CallbackDomain domain,
			    CUpti_CallbackId cbid, 
			    const CUpti_CallbackData* cbInfo)
{
  TMSG(CUDA, "Got Kernel Callback");

  papi_cuda_data_t* cuda_data = userdata;
  int nevents = cuda_data->nevents;
  int cudaEventSet = cuda_data->event_set;
  sample_source_t* self = cuda_data->self;


  TMSG(CUDA, "nevents = %d, cuda event set = %x", nevents, cudaEventSet);

  // This callback is enabled only for kernel launch; anything else is an error.
  if (cbid != CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020) {
    hpcrun_abort("CUDA CUPTI callback seen for unexpected "
		 "interface operation: callback id  %d\n", cbid); 
  }

  if (cbInfo->callbackSite == CUPTI_API_ENTER) {
    TMSG(CUDA, "Cupti API -ENTER- portion");
    // MC recommends FIXME: Unnecessary, but use cudaDeviceSynchronize
      // exclusive access to launcher
    spinlock_lock(&cupti_lock);
    TMSG(CUPTI, "-ACQ-lock");
    dcudaThreadSynchronize();

    TMSG(CUPTI,"-- PRE launch callback");
    TMSG(CUDA, "Start monitoring with event set %d", cudaEventSet);
    int ret = PAPI_start(cudaEventSet);
    if (ret != PAPI_OK){
      EMSG("CUDA monitoring failed to start. PAPI_start failed with %s (%d)", 
	   PAPI_strerror(ret), ret);
    }
  }
  TMSG(CUDA, "Past (or done with) CUDA -ENTER- portion");


  if (cbInfo->callbackSite == CUPTI_API_EXIT) {
    TMSG(CUDA, "Cupti API -EXIT- portion");
    // MC recommends Use cudaDeviceSynchronize
    dcudaThreadSynchronize();
    TMSG(CUPTI, "-- POST launch callback");
    long_long eventValues[nevents+2];
    
    TMSG(CUDA,"stopping CUDA monitoring w event set %d",cudaEventSet);
    int ret = PAPI_stop(cudaEventSet, eventValues);
    if (ret != PAPI_OK){
      EMSG("CUDA monitoring failed to -stop-. PAPI_stop failed with %s (%d)", 
	   PAPI_strerror(ret), ret);
    }  
    TMSG(CUDA,"stopped CUDA monitoring w event set %d",cudaEventSet);

    ucontext_t uc;
    TMSG(CUDA,"getting context in CUDA event handler");
    getcontext(&uc);
    TMSG(CUDA,"got context in CUDA event handler");
    bool safe = hpcrun_safe_enter();
    TMSG(CUDA,"blocked async event in CUDA event handler");
    {
      int i;
      for (i = 0; i < nevents; i++) 
	{
	  int metric_id = hpcrun_event2metric(self, i);

	  TMSG(CUDA, "sampling call path for metric_id = %d", metric_id);
	  hpcrun_sample_callpath(&uc, metric_id, eventValues[i]/*metricIncr*/, 
				 CUPTI_LAUNCH_CALLBACK_DEPTH/*skipInner*/, 
				 0/*isSync*/);
	  TMSG(CUDA, "sampled call path for metric_id = %d", metric_id);
	}
    }
    TMSG(CUDA,"unblocking async event in CUDA event handler");
    if (safe) hpcrun_safe_exit();
    TMSG(CUDA,"unblocked async event in CUDA event handler");

    spinlock_unlock(&cupti_lock);
    TMSG(CUPTI,"-REL-lock\n");
  }
  TMSG(CUDA, "At end (past -EXIT-)");
}

static CUpti_SubscriberHandle subscriber;

//
// sync setup for cuda/cupti
//
static void
papi_c_cupti_setup(void)
{
  // FIXME: Remove local definition
  // CUpti_SubscriberHandle subscriber;

  static bool one_time = false;

  spinlock_lock(&setup_lock);
  TMSG(CUDA, "CUPTI setup acquire lock");
  if (one_time) {
    spinlock_unlock(&setup_lock);
    TMSG(CUDA, "CUPTI setup release lock (setup already called)");
    return;
  }

  TMSG(CUDA,"sync setup called");

  thread_data_t* td = hpcrun_get_thread_data();
  local.self = hpcrun_fetch_source_by_name("papi");

  local.nevents  = local.self->evl.nevents;

  // get cuda event set

  int cuda_component_idx;
  int n_components = PAPI_num_components();

  for (int i = 0; i < n_components; i++) {
    if (is_papi_c_cuda(PAPI_get_component_info(i)->name)) {
      cuda_component_idx = i;
      break;
    }
  }

  papi_source_info_t* psi = td->ss_info[local.self->sel_idx].ptr;
  local.event_set = get_component_event_set(psi, cuda_component_idx);

  Cupti_call(dcuptiSubscribe, &subscriber,
             (CUpti_CallbackFunc)hpcrun_cuda_kernel_callback, 
             &local);
             
  Cupti_call(dcuptiEnableCallback, 1, subscriber,
             CUPTI_CB_DOMAIN_RUNTIME_API,
             CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020);

  one_time = true;
  spinlock_unlock(&setup_lock);
  TMSG(CUDA, "CUPTI setup release lock");
}

//
// Get or create a cupti event set --- but only ONCE per process
//
void
papi_c_cupti_get_event_set(int* ev_s)
{
  TMSG(CUDA, "Get event set");
  spinlock_lock(&setup_lock);
  TMSG(CUDA, "Cupti lock acquired");
  if (! event_set_created) {
    TMSG(CUDA, "No event set created, so create one");
    int ret = PAPI_create_eventset(ev_s);
    if (ret != PAPI_OK) {
      hpcrun_abort("Failure: PAPI_create_eventset.Return code = %d ==> %s", 
                   ret, PAPI_strerror(ret));
    }
    local.event_set = *ev_s;
    event_set_created = true;
    TMSG(CUDA, "Event set %d created", local.event_set);
  }
  spinlock_unlock(&setup_lock);
  TMSG(CUDA, "Cupti lock released");
}

int
papi_c_cupti_add_event(int ev_s, int ev)
{
  int rv = PAPI_OK;
  TMSG(CUDA, "Adding event to cupti event set");
  spinlock_lock(&setup_lock);
  TMSG(CUDA, "Cupti lock acquired");
  if (! event_set_finalized) {
    TMSG(CUDA, "Really add event %x to cupti event set", ev);
    rv = PAPI_add_event(local.event_set, ev);
    TMSG(CUDA, "Check event set passed in = %d, cuda event set = %d", ev_s, local.event_set);
  }
  spinlock_unlock(&setup_lock);
  TMSG(CUDA, "Cupti lock released");
  return rv;
}

void
papi_c_cupti_finalize_event_set(void)
{
  spinlock_lock(&setup_lock);
  event_set_finalized = true;
  spinlock_unlock(&setup_lock);
}


//
// sync teardown for cuda/cupti
//
static void
papi_c_cupti_teardown(void)
{
  static bool one_time = false;
  spinlock_lock(&setup_lock);
  if (one_time) return;

  TMSG(CUDA,"sync teardown called (=unsubscribe)");
  
  Cupti_call(dcuptiUnsubscribe, subscriber);
  one_time = true;
  spinlock_unlock(&setup_lock);
}

static sync_info_list_t cuda_component = {
  .pred = is_papi_c_cuda,
  .get_event_set = papi_c_cupti_get_event_set,
  .add_event = papi_c_cupti_add_event,
  .finalize_event_set = papi_c_cupti_finalize_event_set,
  .sync_setup = papi_c_cupti_setup,
  .sync_teardown = papi_c_cupti_teardown,
  .sync_start = papi_c_no_action,
  .sync_stop = papi_c_no_action,
  .process_only = true,
  .next = NULL,
};

__attribute__((constructor))
void
papi_c_cupti_register(void)
{
  // fetch actual cuda/cupti functions
  dlgpu();
  papi_c_sync_register(&cuda_component);
}

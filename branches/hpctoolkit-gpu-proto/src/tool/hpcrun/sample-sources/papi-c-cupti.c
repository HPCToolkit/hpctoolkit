// ******************* System Includes ********************
#include <alloca.h>
#include <ucontext.h> 
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

// ******************** PAPI *******************************
#include <papi.h>
// *********************************************************

// ******************** GPU includes ***********************
#include <cuda.h>
#include <cupti.h>
// *********************************************************

// ******* HPCToolkit Includes *********************************
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

typedef struct cuda_callback_t {
  sample_source_t* ss;
  int event_set;
} cuda_callback_t;

#define CUPTI_LAUNCH_CALLBACK_DEPTH 7

#define CUPTI_ERRORS_UNMYSTIFIED
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

  thread_data_t* td = hpcrun_get_thread_data();
  sample_source_t* self = hpcrun_fetch_source_by_name("papi");

  int nevents  = self->evl.nevents;

  // get cuda event set

  int cuda_component_idx;
  int n_components = PAPI_num_components();

  for (int i = 0; i < n_components; i++) {
    if (is_papi_c_cuda(PAPI_get_component_info(i)->name)) {
      cuda_component_idx = i;
      break;
    }
  }

  papi_source_info_t* psi = td->ss_info[self->evset_idx].ptr;
  int cudaEventSet = get_component_event_set(psi, cuda_component_idx);

  TMSG(CUDA, "nevents = %d, cuda event set = %x", nevents, cudaEventSet);

  // This callback is enabled only for kernel launch; anything else is an error.
  if (cbid != CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020) {
    hpcrun_abort("CUDA CUPTI callback seen for unexpected "
		 "interface operation: callback id  %d\n", cbid); 
  }

  if (cbInfo->callbackSite == CUPTI_API_ENTER) {
    // MC recommends FIXME: cbInfo->????? (says what kind of callback) == KERNEL_LAUNCH
    // MC recommends FIXME: Unnecessary, but use cudaDeviveSynchronize
    cudaThreadSynchronize();

    TMSG(CUDA,"starting CUDA monitoring w event set %d",cudaEventSet);
    int ret = PAPI_start(cudaEventSet);
    if (ret != PAPI_OK){
      EMSG("CUDA monitoring failed to start. PAPI_start failed with %s (%d)", 
	   PAPI_strerror(ret), ret);
    }  
  }
    
  if (cbInfo->callbackSite == CUPTI_API_EXIT) {
    // MC recommends Use cudaDeviceSynchronize
    cudaThreadSynchronize();
    long_long eventValues[nevents+2];
    
#if 0 // FIXME before checkin: take away if 0
    long_long *eventValues = 
      (long_long *) alloca(sizeof(long_long) * (nevents+2));
#endif

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
    hpcrun_safe_enter();
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
    hpcrun_safe_exit();
    TMSG(CUDA,"unblocked async event in CUDA event handler");
  }
}

//
// sync start for cuda/cupti
//
static void
papi_c_sync_start(void)
{
  int cuptiErr;
  CUpti_SubscriberHandle subscriber;

  TMSG(CUDA,"sync start called");
  
  cuptiErr = cuptiSubscribe(&subscriber, 
			    (CUpti_CallbackFunc)hpcrun_cuda_kernel_callback, 
			    NULL);
  check_cupti_error(cuptiErr, "cuptiSubscribe");

  cuptiErr = cuptiEnableCallback(1, subscriber, CUPTI_CB_DOMAIN_RUNTIME_API, 
                                 CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020);
  check_cupti_error(cuptiErr, "cuptiEnableCallback");
}

static sync_info_list_t cuda_component = {
  .pred = is_papi_c_cuda,
  .sync_start = papi_c_sync_start,
  .next = NULL,
};

__attribute__((constructor))
void
papi_c_cupti_register(void)
{
  papi_c_sync_register(&cuda_component);
}

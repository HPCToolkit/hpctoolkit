// -*-Mode: C++;-*- // technically C99
// $Id$

//
// itimer sample source simple oo interface
//

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/time.h>           /* setitimer() */
#include <ucontext.h>           /* struct ucontext */
#include <assert.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

/*---------------------------
 * monitor
 *--------------------------*/
#include "monitor.h"

/*---------------------------
 * csprof
 *--------------------------*/
#include "common.sample_source.h"
#include "csprof_options.h"
#include "metrics.h"
#include "pmsg.h"
#include "registered_sample_sources.h"
#include "sample_event.h"
#include "sample_source.h"
#include "simple_oo.h"
#include "thread_data.h"


/******************************************************************************
 * macros
 *****************************************************************************/

#define DEBUG_FEW 0 

#define CSPROF_PROFILE_SIGNAL SIGPROF
#define CSPROF_PROFILE_TIMER ITIMER_PROF

#ifdef CATAMOUNT
#define CSPROF_PROFILE_SIGNAL SIGALRM
#define CSPROF_PROFILE_TIMER ITIMER_REAL
#endif

#ifdef __ppc64__ 
#define BLUE_GENE_P_COMPUTE 1
#endif

/******************************************************************************
 * forward declarations 
 *****************************************************************************/

#ifdef BLUE_GENE_P_COMPUTE
static int csprof_itimer_signal_handler_bgp(int sig, siginfo_t *siginfo, 
	void *context);
#endif

static int csprof_itimer_signal_handler(int sig, siginfo_t *siginfo, void *context);



/******************************************************************************
 * local variables
 *****************************************************************************/

static struct itimerval itimer;

// ******* METHOD DEFINITIONS ***********

static void
METHOD_FN(init)
{
  self->state = INIT; // no actual init actions necessary for itimer
}

static void
METHOD_FN(start)
{
  TMSG(ITIMER_CTL,"starting itimer");
  setitimer(CSPROF_PROFILE_TIMER, &itimer, NULL);

  TD_GET(ss_state)[self->evset_idx] = START;

  // int rv = setitimer(CSPROF_PROFILE_TIMER, &itimer, NULL);
  // return rv
}

static void
METHOD_FN(stop)
{
  struct itimerval itimer;
  int rc;

  timerclear(&itimer.it_value);
  timerclear(&itimer.it_interval);
  rc = setitimer(CSPROF_PROFILE_TIMER, &itimer, NULL);
  TMSG(ITIMER_CTL,"stopping itimer");
  TD_GET(ss_state)[self->evset_idx] = STOP;
  // return rc;
}

static void
METHOD_FN(shutdown)
{
  METHOD_CALL(self,stop); // make sure stop has been called
  self->state = UNINIT;
}

static int
METHOD_FN(supports_event,const char *ev_str)
{
  return (strstr(ev_str,"WALLCLOCK") != NULL);
}
 
// (arbitrary) code for WALLCLOCK event
#define WALLCLOCK 0

static void
METHOD_FN(process_event_list,int lush_metrics)
{
  long period = 5000L;

  char *_p = strchr(METHOD_CALL(self,get_event_str),':');
  if ( _p) {
    period = strtol(_p+1,NULL,10);
  }
  else {
    TMSG(OPTIONS,"WALLCLOCK event default period (5000) selected");
  }
  
  METHOD_CALL(self,store_event,WALLCLOCK,period);
  TMSG(OPTIONS,"wallclock period set to %ld",period);

  int seconds = period / 1000000;
  int microseconds = period % 1000000;

  TMSG(OPTIONS,"init timer w sample_period = %ld, seconds = %ld, usec = %ld",
       period, seconds, microseconds);

  /* signal once after the given delay */
  itimer.it_value.tv_sec = seconds;
  itimer.it_value.tv_usec = microseconds;

#if 0 // use interval for BG/P
  /* no automatic restart */
  itimer.it_interval.tv_sec = 0;
  itimer.it_interval.tv_usec = 0;
#else
  // automatic restart, f BG/P
  itimer.it_interval.tv_sec = seconds;
  itimer.it_interval.tv_usec = microseconds;
#endif
}

static void
METHOD_FN(gen_event_set,int lush_metrics)
{
  int ret = csprof_set_max_metrics(1 + lush_metrics);
  
  if (ret > 0) {
    long sample_period = self->evl.events[0].thresh;

    int metric_id = csprof_new_metric();
    TMSG(ITIMER_CTL,"setting metric id = %d,period = %ld",metric_id,sample_period);
    csprof_set_metric_info_and_period(metric_id, "WALLCLOCK (ms)",
				      HPCFILE_METRIC_FLAG_ASYNC,
				      sample_period);
    
    // FIXME:LUSH: inadequacy compounded by inadequacy of metric
    // interface.  Cf. papi version.
    if (lush_metrics == 1) {
      int lush_metric_id = csprof_new_metric();
      assert(lush_metric_id == 1);
      csprof_set_metric_info_and_period(lush_metric_id, "P non-work (ms)",
					HPCFILE_METRIC_FLAG_ASYNC | HPCFILE_METRIC_FLAG_REAL,
					sample_period);
    }
  }
  thread_data_t *td = csprof_get_thread_data();
  td->eventSet[self->evset_idx] = 0xDEAD; // Event sets not relevant for itimer

#ifdef BLUE_GENE_P_COMPUTE
  monitor_sigaction(CSPROF_PROFILE_SIGNAL, (monitor_sighandler_t*) 
	&csprof_itimer_signal_handler_bgp, 0, NULL);
#else
  monitor_sigaction(CSPROF_PROFILE_SIGNAL, &csprof_itimer_signal_handler, 0, NULL);
#endif
}

/***************************************************************************
 * object
 ***************************************************************************/

sample_source_t _itimer_obj = {
  // common methods

  .add_event     = csprof_ss_add_event,
  .store_event   = csprof_ss_store_event,
  .get_event_str = csprof_ss_get_event_str,
  .started       = csprof_ss_started,

  // specific methods

  .init = init,
  .start = start,
  .stop  = stop,
  .shutdown = shutdown,
  .supports_event = supports_event,
  .process_event_list = process_event_list,
  .gen_event_set = gen_event_set,

  // data
  .evl = {
    .evl_spec = {[0] = '\0'},
    .nevents = 0
  },
  .evset_idx = 0,
  .name = "itimer",
  .state = UNINIT
};

/******************************************************************************
 * constructor 
 *****************************************************************************/
static void itimer_obj_reg(void) __attribute__ ((constructor));

static void
itimer_obj_reg(void)
{
  csprof_ss_register(&_itimer_obj);
}

/******************************************************************************
 * private operations 
 *****************************************************************************/

#include <ucontext.h>

// BG/P specific at the moment

#define ADVANCE_BP(bp) *((void **)(bp))
#define NEXT_PC(bp)    *(((void **)(bp))+1)

#define ITIMER_METRIC_ID 0

static int
csprof_itimer_signal_handler_bgp(int sig, siginfo_t *siginfo, void *context) 
{
  ucontext_t ctx;

  if ( ! context ) {
    getcontext(&ctx);

    unsigned long tr1;
    unsigned long r1 = ctx.uc_mcontext.regs->gpr[1];

#if 0
    TMSG(GETCONTEXT,"fetch from getcontext: r1 = %p,pc = %p",(void *)r1,(void *)pc);
#endif

    for(int i=1; i <= 3;i++){
      r1 = (long)ADVANCE_BP(r1);
#if 0
      TMSG(GETCONTEXT,"i = %d: (bp = %p) next pc from frame = %p",i,r1,NEXT_PC(r1));
#endif
      if (i == 2) tr1 = r1;
    }
    void **tramp = (void **)NEXT_PC(tr1);

#if 0
    TMSG(GETCONTEXT,"tramp location = %p",tramp);
    TMSG(GETCONTEXT,"prior word before tramp on stack = %d",*((long *)tramp - 1));
    PMSG(GETCONTEXT,"========================================");
    void **sp = tramp - 1;
    for (int i = 0;i < 828;i++,sp--){
      TMSG(GETCONTEXT,"stack[%d][%p] = %p",i,sp,*sp);
    }
#endif

    // magic offsets to pluck the register values out of the kernel register state
    long *ip_ptr = (long *) tramp-39;
    long *lr_ptr = (long *) tramp-36;
    long *r0_ptr = (long *) tramp-201;
    long *r1_ptr = (long *) tramp-200;

    long ip = *ip_ptr;
    long lr = *lr_ptr;
    long r0 = *r0_ptr;
    long r1s = *r1_ptr;

#if 0
    TMSG(GETCONTEXT,"ip  = %x (at location %p)",ip, ip_ptr);
    TMSG(GETCONTEXT,"lr  = %x (at location %p)",lr, lr_ptr);
    TMSG(GETCONTEXT,"r0  = %x (at location %p)",r0, r0_ptr);
#endif

    ctx.uc_mcontext.regs->nip = ip;
    ctx.uc_mcontext.regs->link = lr;
    ctx.uc_mcontext.regs->gpr[0] = r0;
    ctx.uc_mcontext.regs->gpr[1] = r1;

    TMSG(GETCONTEXT,"synthesized ucontext: r1 = %p, r1s = %p, ip = %p lr = %p r0 = %p", r1 , r1s , ip, lr, r0);
    context = &ctx;
  }
  return csprof_itimer_signal_handler(sig, siginfo, context);
}

static int
csprof_itimer_signal_handler(int sig, siginfo_t *siginfo, void *context)
{
#if DEBUG_FEW
  static int ninterrupts = 0;
#endif

  if (!csprof_handling_synchronous_sample_p()) {
    TMSG(ITIMER_HANDLER,"Itimer sample event");
    csprof_sample_event(context, ITIMER_METRIC_ID, 1 /*sample_count*/);
  }

#if 0 // use interval, since BG/P doesn't work without it
  METHOD_CALL(&_itimer_obj,start);
#endif

#if DEBUG_FEW
 if (++ninterrupts > 4) exit(1);
#endif

  return 0; /* tell monitor that the signal has been handled */
}

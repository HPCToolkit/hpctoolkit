/* driver implementation using interval timers and SIGPROF */

/* system includes */
#include <sys/time.h>           /* setitimer() */
#include <signal.h>
#include <sys/signal.h>         /* sigaction() */
#include <ucontext.h>           /* struct ucontext */

#include <stdio.h>
#include <setjmp.h>

#include "bad_unwind.h"
#include "general.h"
#include "driver.h"
#include "structs.h"
#include "libc.h"
#include "util.h"
#include "sighand.h"
#include "metrics.h"
#include "interface.h"
#include "segv_handler.h"
#include "dump_backtraces.h"
#include "pmsg.h"
#include "name.h"

#ifndef STATIC_ONLY
#define CSPROF_PROFILE_SIGNAL SIGPROF
#define CSPROF_PROFILE_TIMER ITIMER_PROF
#else
#define CSPROF_PROFILE_SIGNAL SIGALRM
#define CSPROF_PROFILE_TIMER ITIMER_REAL
#endif

int s1 = 0;
int s2 = 0;
int s3 = 0;

#ifdef MPI_SPECIAL
#define STATIC_RANK 0
static int chosen_rank = STATIC_RANK;
#endif

extern csprof_state_t *
csprof_check_for_new_epoch(csprof_state_t *state);

/* indices into a treenode's `metrics' array */
#define WEIGHT_METRIC 0
#define CALLS_METRIC 1

static struct itimerval itimer;

static void csprof_take_profile_sample(csprof_state_t *, struct ucontext *);

sigset_t prof_sigset;

#ifdef NO
void _zflags(void)
{
  s1 = 0;
  s2 = 0;
  s3 = 0;
}
#endif

static void
csprof_init_sigset(sigset_t *ss)
{
    sigemptyset(ss);
    MSG(1,"sigaddset csprof signal");
    sigaddset(ss, CSPROF_PROFILE_SIGNAL);
}

unsigned long sample_period = 0L;
unsigned long seconds = 0L;
unsigned long microseconds = 0L;

static void
csprof_init_timer(csprof_options_t *options)
{
#ifdef NO
    unsigned long sample_period = options->sample_period;
    unsigned long seconds = sample_period / 1000000;
    unsigned long microseconds = sample_period % 1000000;
#endif
    sample_period = options->sample_period;
    seconds = sample_period / 1000000;
    microseconds = sample_period % 1000000;

    MSG(1,"init timer w sample_period = %ld, seconds = %ld, usec = %ld",
        sample_period, seconds, microseconds);
    /* begin timing after the given delay */
    itimer.it_value.tv_sec = seconds;
    itimer.it_value.tv_usec = microseconds;

#if defined(CSPROF_THREADS) && defined(CSPROF_ROUND_ROBIN_SIGNAL)
    /* continually signal after that */
    MSG(1,"Threads style signal\n");
    itimer.it_interval.tv_sec = seconds;
    itimer.it_interval.tv_usec = microseconds;
#else
    /* we do our own reset */
    MSG(1,"own rest style");
    itimer.it_interval.tv_sec = 0;
    itimer.it_interval.tv_usec = 0;
#endif
}

void csprof_set_timer(void) 
{

  setitimer(CSPROF_PROFILE_TIMER, &itimer, NULL);
  MSG(1,"called csprof_set_timer");

#if 0
  if(success != 0) {
    perror("setitimer");
  }
#endif
}

void csprof_disable_timer(void)
{
  struct itimerval itimer;
  timerclear(&itimer.it_value);
  setitimer(CSPROF_PROFILE_TIMER, &itimer, NULL);
}

#if defined(CSPROF_THREADS) && defined(CSPROF_ROUND_ROBIN_SIGNAL)
static volatile long outstanding_signal_count = 0;

/* following John May's approach with some slight modifications */
static void
csprof_signal_all_threads(pthread_t master)
{
    csprof_list_node_t *runner;
    int i;

    runner = all_threads.head;

    for(i=0; runner != NULL; runner = runner->next) {
        pthread_t t = (pthread_t)runner->ip;

        /* mostly dead? */
        if(runner->sp != CSPROF_PTHREAD_DEAD) {
            /* direct the thread to take a sample */
            if(!pthread_equal(master, t)) {
                csprof_atomic_increment(&outstanding_signal_count);
                /* this is not entirely safe; we might send the thread a
                   signal whilst it tries to handle another signal, which
                   is a surefire way to take processes down in flames. */
                pthread_kill(t, SIGPROF);
            }
        }
    }
}

static void
csprof_driver_distribute_samples(csprof_state_t *state,
				 struct ucontext *ctx)
{
  pthread_t me = pthread_self();

  MSG(,"in distribute samples\n");
  if(state == NULL) {
    DBGMSG_PUB(CSPROF_DBG_PTHREAD, "NULL state, signaling threads");
    csprof_signal_all_threads(me);
  }
  else if(outstanding_signal_count == 0) {
    DBGMSG_PUB(CSPROF_DBG_PTHREAD, "non-NULL state, signaling threads");
    /* I am the master thread for this round of signalling */
    csprof_signal_all_threads(me);

    csprof_take_profile_sample(state, ctx);
    csprof_state_flag_clear(state, CSPROF_THRU_TRAMP);
  }
  else {
    DBGMSG_PUB(CSPROF_DBG_PTHREAD, "outstanding signals, taking a sample");
    csprof_atomic_decrement(&outstanding_signal_count);

    csprof_take_profile_sample(state, ctx);
    csprof_state_flag_clear(state, CSPROF_THRU_TRAMP);
  }
}
#endif

#ifdef CSPROF_TRAMPOLINE_BACKEND
extern void csprof_trampoline_end();
extern void csprof_trampoline2();
extern void csprof_trampoline2_end();
#endif

static void
csprof_undo_swizzled_data(csprof_state_t *state, void *ctx)
{
#ifdef CSPROF_TRAMPOLINE_BACKEND
    if(csprof_swizzle_patch_is_valid(state)) {
        csprof_remove_trampoline(state, ctx);
    }
#endif
}

void
csprof_swizzle_with_context(csprof_state_t *state, void *ctx)
{
#ifdef CSPROF_TRAMPOLINE_BACKEND
    struct lox l;
    int proceed = csprof_find_return_address_for_context(state, &l, ctx);

    if(proceed) {
        /* record */
        if(l.stored.type == REGISTER) {
            state->swizzle_patch = (void **) l.stored.location.reg;
        }
        else {
            state->swizzle_patch = l.stored.location.address;
        }
        /* recording the return is machine-dependent */

        csprof_insert_trampoline(state, &l, ctx);
    }
#endif
}

#ifdef NO
static int
dc(void)
{
    return 0;
}
#endif

static void
csprof_take_profile_sample(csprof_state_t *state, struct ucontext *ctx)
{
    mcontext_t *context = &ctx->uc_mcontext;
    void *pc = csprof_get_pc(context);

    csprof_set_last_sample_addr((unsigned long) pc);
    csprof_increment_raw_sample_count();

    MSG(1,"csprof take profile sample");
    if(/* trampoline isn't exactly active during exception handling */
       csprof_state_flag_isset(state, CSPROF_EXC_HANDLING)
       /* dynamical libraries are in flux; don't bother */
       || csprof_epoch_is_locked()
       /* general checking for addresses in libraries */
       || csprof_context_is_unsafe(ctx)) {
      MSG(1,"No trampoline, s1 = %x, s2 = %x, s3 = %x",s1,s2,s3);
        /* ugh, don't even bother */
        state->trampoline_samples++;
        // _zflags();

        return;
    }

    state->context_pc = pc;
    DBGMSG_PUB(1, "Signalled at %#lx", pc);

    /* check to see if shared library state has changed out from under us */
    state = csprof_check_for_new_epoch(state);

    MSG(1,"undo swizzled data\n");
    csprof_undo_swizzled_data(state, context);

#if defined(__ia64__) && defined(__linux__) 
    /* force insertion from the root */
    state->treenode = NULL;
    state->bufstk = state->bufend;
#endif
    if(csprof_sample_callstack(state, WEIGHT_METRIC, 1, context) == CSPROF_OK) {
      MSG(1,"about to swizzle w context\n");
      csprof_swizzle_with_context(state, (void *)context);
    }

    csprof_state_flag_clear(state, CSPROF_TAIL_CALL | CSPROF_EPILOGUE_RA_RELOADED | CSPROF_EPILOGUE_SP_RESET);
}

int samples_taken    = 0;
int bad_unwind_count = 0;
int csprof_sample    = 0;

static void csprof_itimer_signal_handler(int sig, siginfo_t *siginfo, void *context){

  sigjmp_buf_t *it = get_bad_unwind();
  samples_taken++;
#ifdef MPI_SPECIAL
  if (csprof_mpirank() < 0){
    PMSG(ITIMER_HANDLER,"sample before mpi_init");
    csprof_set_timer();
    return;
  }
  if (csprof_mpirank() != chosen_rank){
    PMSG(ITIMER_HANDLER,"got signal, but csprof_rank = %d",csprof_mpirank());
    csprof_set_timer();
    return;
  }
#endif
  MSG(1,"got itimer signal");

  csprof_sample = 1;
  setup_segv();
  if (!sigsetjmp(it->jb,1)){
    CSPROF_SIGNAL_HANDLER_GUTS(context);
  }
  else {
    csprof_state_t *state = csprof_get_state();
    EMSG("got bad unwind: context_pc = %p, unwind_pc = %p\n\n",state->context_pc,
         state->unwind_pc);
    dump_backtraces(state, state->unwind);
    bad_unwind_count++;
  }
  csprof_sample = 0;
  csprof_set_timer();
}

static void csprof_init_signal_handler(void){
  struct sigaction sa;
  int ret;

  MSG(1,"Got to init signal handler\n");

  /* set up handler for profiling purposes */
  sa.sa_sigaction = csprof_itimer_signal_handler;
  /* do we care very much about other signals being delivered
     while we're recording profiling information? */
  sigemptyset(&sa.sa_mask);
  /* get new-style signals and automagically restart any system calls
     that we interrupt -- especially useful when using SIGPROF for
     CSPROF_PROFILE_SIGNAL. */
  sa.sa_flags = SA_SIGINFO | SA_RESTART;

  ret = sigaction(CSPROF_PROFILE_SIGNAL, &sa, 0);
}

extern void unw_init(void);

static void heartbeat_init(csprof_options_t *opts){
  csprof_init_signal_handler();
  csprof_init_timer(opts);
  csprof_init_sigset(&prof_sigset);
  csprof_set_timer();
}

void csprof_process_driver_init(csprof_options_t *opts){
  setup_segv();
  unw_init();
  {
    int metric_id;

    csprof_set_max_metrics(2);
    metric_id = csprof_new_metric(); /* weight */
    csprof_set_metric_info_and_period(metric_id, "# samples",
                                      CSPROF_METRIC_ASYNCHRONOUS,
                                      opts->sample_period);
    metric_id = csprof_new_metric(); /* calls */
    csprof_set_metric_info_and_period(metric_id, "# returns",
                                      CSPROF_METRIC_FLAGS_NIL, 1);
  }
  heartbeat_init(opts);
}

void csprof_thread_driver_init(csprof_options_t *opts){
  heartbeat_init(opts);
}

void csprof_driver_init(csprof_state_t *state,csprof_options_t *opts){

    csprof_init_signal_handler();
    setup_segv();
    unw_init();
    csprof_init_timer(opts);
    csprof_init_sigset(&prof_sigset);
    {
      int metric_id;

      csprof_set_max_metrics(2);
      metric_id = csprof_new_metric(); /* weight */
      csprof_set_metric_info_and_period(metric_id, "# samples",
					CSPROF_METRIC_ASYNCHRONOUS,
					opts->sample_period);
      metric_id = csprof_new_metric(); /* calls */
      csprof_set_metric_info_and_period(metric_id, "# returns",
					CSPROF_METRIC_FLAGS_NIL, 1);
    }
    csprof_set_timer();
}

void csprof_driver_fini(csprof_state_t *state, csprof_options_t *opts)
{
    csprof_disable_timer();
}

#ifdef CSPROF_THREADS


void
csprof_driver_thread_init(csprof_state_t *state)
{
    /* no support required */
}

void
csprof_driver_thread_fini(csprof_state_t *state)
{
  MSG(1,"itimer driver thread fini");
    /* no support required */
}

#endif

void csprof_driver_suspend(csprof_state_t *state){
    /* no support required */
}

void csprof_driver_resume(csprof_state_t *state) {
#if (defined(CSPROF_THREADS) && !defined(CSPROF_ROUND_ROBIN_SIGNAL)) || (!defined(CSPROF_THREADS))
    csprof_set_timer();
#endif
}

#if 1
void csprof_driver_forbid_samples(csprof_state_t *state,
                                  csprof_profile_token_t *data){
  csprof_sigmask(SIG_BLOCK, &prof_sigset, data);
}

void
csprof_driver_permit_samples(csprof_state_t *state,
                             csprof_profile_token_t *data)
{
    csprof_sigmask(SIG_SETMASK, data, NULL);
}
#endif

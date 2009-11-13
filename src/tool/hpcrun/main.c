// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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

//***************************************************************************
// system include files 
//***************************************************************************

#include <pthread.h>
#include <unistd.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#ifdef LINUX
#include <linux/unistd.h>
#endif


//***************************************************************************
// libmonitor include files
//***************************************************************************

#include <monitor.h>


//***************************************************************************
// user include files 
//***************************************************************************

#include <include/uint.h>

#include "main.h"

#include "disabled.h"
#include "env.h"
#include "loadmap.h"
#include "files.h"
#include "fnbounds_interface.h"
#include "hpcrun_dlfns.h"
#include "hpcrun_options.h"
#include "hpcrun_return_codes.h"
#include "hpcrun_stats.h"
#include "name.h"

#include "metrics.h"

#include "sample_event.h"
#include <sample-sources/none.h>
#include "sample_sources_registered.h"
#include "sample_sources_all.h"
#include "segv_handler.h"

#include "epoch.h"
#include "thread_data.h"
#include "thread_use.h"
#include "trace.h"
#include "write_data.h"

#include <memory/hpcrun-malloc.h>

#include <monitor-exts/monitor_ext.h>

#include <cct/cct.h>

#include <unwind/common/backtrace.h>
#include <unwind/common/unwind.h>
#include <unwind/common/splay-interval.h>

#include <lush/lush-backtrace.h>
#include <lush/lush-pthread.h>

#include <lib/prof-lean/atomic.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof-lean/hpcio.h>

#include <messages/messages.h>
#include <messages/debug-flag.h>


//***************************************************************************
// constants
//***************************************************************************

enum _local_const {
  PROC_NAME_LEN = 2048
};



//***************************************************************************
// global variables
//***************************************************************************

int lush_metrics = 0; // FIXME: global variable for now



//***************************************************************************
// local variables 
//***************************************************************************

static volatile int DEBUGGER_WAIT = 1;

static hpcrun_options_t opts;
static bool hpcrun_is_initialized_private = false;

static sigset_t prof_sigset;



//***************************************************************************
// inline functions
//***************************************************************************

static inline bool
hpcrun_is_initialized()
{
  return hpcrun_is_initialized_private;
}



//***************************************************************************
// *** Important note about libmonitor callbacks ***
//
//  In libmonitor callbacks, block two things:
//
//   1. Skip the callback if hpcrun is not yet initialized.
//   2. Block async reentry for the duration of the callback.
//
// Init-process and init-thread are where we do the initialization, so
// they're special.  Also, libmonitor promises that init and fini process
// and thread are run in sequence, but dlopen, fork, pthread-create
// can occur out of sequence (in init constructor).
//***************************************************************************

//***************************************************************************
// internal operations 
//***************************************************************************

//------------------------------------
// process level 
//------------------------------------

void
hpcrun_init_internal()
{
  hpcrun_loadmap_init(hpcrun_static_loadmap());

  hpcrun_thread_data_new();
  hpcrun_thread_memory_init();
  hpcrun_thread_data_init(0, NULL);

  // WARNING: a perfmon bug requires us to fork off the fnbounds
  // server before we call PAPI_init, which is done in argument
  // processing below. Also, fnbounds_init must be done after the
  // memory allocator is initialized.
  fnbounds_init();
  hpcrun_options__init(&opts);
  hpcrun_options__getopts(&opts);

  trace_init(); // this must go after thread initialization
  trace_open();

  // Initialize logical unwinding agents (LUSH)
  if (opts.lush_agent_paths[0] != '\0') {
    epoch_t* epoch = TD_GET(epoch);
    TMSG(MALLOC," -init_internal-: lush allocation");
    lush_agents = (lush_agent_pool_t*)hpcrun_malloc(sizeof(lush_agent_pool_t));
    hpcrun_logicalUnwind(true);
    lush_agent_pool__init(lush_agents, opts.lush_agent_paths);
    EMSG("Logical Unwinding Agent: %s (%p / %p)", opts.lush_agent_paths,
	 epoch, lush_agents);
  }

  lush_metrics = (lush_agents) ? 1 : 0;

  // tallent: this is harmless, but should really only occur for pthread agent
  lushPthr_processInit();


  sigemptyset(&prof_sigset);
  sigaddset(&prof_sigset,SIGPROF);

  setup_segv();
  unw_init();

  // sample source setup
  SAMPLE_SOURCES(init);
  SAMPLE_SOURCES(process_event_list, lush_metrics);
  SAMPLE_SOURCES(gen_event_set, lush_metrics);

  // set up initial 'epoch' 
  
  TMSG(EPOCH,"process init setting up initial epoch/loadmap");
  hpcrun_epoch_init();

  // start the sampling process

  SAMPLE_SOURCES(start);

  hpcrun_is_initialized_private = true;
}


void
hpcrun_fini_internal()
{
  NMSG(FINI,"process");
  int ret = monitor_real_sigprocmask(SIG_BLOCK,&prof_sigset,NULL);
  if (ret){
    EMSG("WARNING: process fini could not block SIGPROF, ret = %d",ret);
  }

  hpcrun_unthreaded_data();
  epoch_t *epoch = TD_GET(epoch);

  if (hpcrun_is_initialized()) {
    hpcrun_is_initialized_private = false;

    NMSG(FINI,"process attempting sample shutdown");

    SAMPLE_SOURCES(stop);
    SAMPLE_SOURCES(shutdown);

    // shutdown LUSH agents
    if (lush_agents) {
      lush_agent_pool__fini(lush_agents);
      lush_agents = NULL;
    }

    // N.B. short-circuit, if monitoring is disabled
    if (hpcrun_get_disabled()) {
      return;
    }

    fnbounds_fini();

    hpcrun_finalize_current_loadmap();

    hpcrun_write_profile_data(epoch);

    hpcrun_stats_print_summary();
    
    messages_fini();
  }
}


//------------------------------------
// thread level 
//------------------------------------

void
hpcrun_init_thread_support()
{
  hpcrun_init_pthread_key();
  hpcrun_set_thread0_data();
  hpcrun_threaded_data();
}


void*
hpcrun_thread_init(int id, cct_ctxt_t* thr_ctxt)
{
  thread_data_t *td = hpcrun_allocate_thread_data();
  td->suspend_sampling = 1; // begin: protect against spurious signals


  hpcrun_set_thread_data(td);

  hpcrun_thread_data_new();
  hpcrun_thread_memory_init();
  hpcrun_thread_data_init(id, thr_ctxt);

  epoch_t* epoch = TD_GET(epoch);

  // handle event sets for sample sources
  SAMPLE_SOURCES(gen_event_set,lush_metrics);

  // set up initial 'epoch'
  TMSG(EPOCH,"process init setting up initial epoch/loadmap");
  hpcrun_epoch_init();

  // start the sample sources
  SAMPLE_SOURCES(start);

  int ret = monitor_real_pthread_sigmask(SIG_UNBLOCK,&prof_sigset,NULL);
  if (ret){
    EMSG("WARNING: Thread init could not unblock SIGPROF, ret = %d",ret);
  }
  return (void *)epoch;
}


void
hpcrun_thread_fini(epoch_t *epoch)
{
  TMSG(FINI,"thread fini");
  if (hpcrun_is_initialized()) {
    TMSG(FINI,"thread finit stops sampling");
    SAMPLE_SOURCES(stop);
    lushPthr_thread_fini(&TD_GET(pthr_metrics));
    hpcrun_finalize_current_loadmap();

    // FIXME: currently breaks the build.
#if 0
    EMSG("Backtrace for last sample event:\n");
    dump_backtrace(epoch, epoch->unwind);
#endif // defined(HOST_SYSTEM_IBM_BLUEGENE)

    hpcrun_write_profile_data(epoch);
  }
}



//***************************************************************************
// process control (via libmonitor)
//***************************************************************************

void*
monitor_init_process(int *argc, char **argv, void* data)
{
  char* process_name;
  char  buf[PROC_NAME_LEN];

  if (getenv("CSPROF_WAIT")) {
    while(DEBUGGER_WAIT);
  }

  // FIXME: if the process fork()s before main, then argc and argv
  // will be NULL in the child here.  MPT on CNL does this.
  process_name = "unknown";
  if (argv != NULL && argv[0] != NULL) {
    process_name = argv[0];
  }
  else {
    int len = readlink("/proc/self/exe", buf, PROC_NAME_LEN - 1);
    if (len > 1) {
      buf[len] = 0;
      process_name = buf;
    }
  }

  hpcrun_set_using_threads(false);

  files_set_executable(process_name);

  hpcrun_registered_sources_init();

  messages_init();

  char *s = getenv(HPCRUN_EVENT_LIST);
  if (s == NULL){
    s = getenv("CSPROF_OPT_EVENT");
  }
  hpcrun_sample_sources_from_eventlist(s);

  hpcrun_process_sample_source_none();

  if (!hpcrun_get_disabled()) {
    files_set_directory();
  }

  TMSG(PROCESS,"init");

  messages_logfile_create();

  hpcrun_init_internal();
  if (ENABLED(TST)){
    EEMSG("TST debug ctl is active!");
    STDERR_MSG("Std Err message appears");
  }
  hpcrun_async_unblock();

  return data;
}


void
monitor_fini_process(int how, void* data)
{
  hpcrun_async_block();

  hpcrun_fini_internal();
  trace_close();
  fnbounds_fini();

  hpcrun_async_unblock();
}


static struct _ff {
  int flag;
} from_fork;


void*
monitor_pre_fork(void)
{
  if (! hpcrun_is_initialized()) {
    return NULL;
  }
  hpcrun_async_block();

  NMSG(PRE_FORK,"pre_fork call");

  if (SAMPLE_SOURCES(started)) {
    NMSG(PRE_FORK,"sources shutdown");
    SAMPLE_SOURCES(stop);
    SAMPLE_SOURCES(shutdown);
  }

  NMSG(PRE_FORK,"finished pre_fork call");
  hpcrun_async_unblock();

  return (void *)(&from_fork);
}


void
monitor_post_fork(pid_t child, void* data)
{
  if (! hpcrun_is_initialized()) {
    return;
  }
  hpcrun_async_block();

  NMSG(POST_FORK,"Post fork call");

  if (!SAMPLE_SOURCES(started)){
    NMSG(POST_FORK,"sample sources re-init+re-start");
    SAMPLE_SOURCES(init);
    SAMPLE_SOURCES(gen_event_set,0); // FIXME: pass lush_metrics here somehow
    SAMPLE_SOURCES(start);
  }

  NMSG(POST_FORK,"Finished post fork");
  hpcrun_async_unblock();
}


//***************************************************************************
// MPI control (via libmonitor)
//***************************************************************************

//
// On some systems, taking a signal inside MPI_Init breaks MPI_Init.
// So, turn off sampling (not just block) within MPI_Init, with the
// control variable MPI_RISKY to bypass this.
//
void
monitor_mpi_pre_init(void)
{
  if (! ENABLED(MPI_RISKY)) {
#if defined(HOST_SYSTEM_IBM_BLUEGENE)
    // Turn sampling off.
    SAMPLE_SOURCES(stop);
#endif
  }
}


void
monitor_init_mpi(int *argc, char ***argv)
{
  if (! ENABLED(MPI_RISKY)) {
#if defined(HOST_SYSTEM_IBM_BLUEGENE)
    // Turn sampling back on.
    SAMPLE_SOURCES(start);
#endif
  }
}


//***************************************************************************
// thread control (via libmonitor)
//***************************************************************************

void
monitor_init_thread_support(void)
{
  hpcrun_async_block();

  TMSG(THREAD,"REALLY init_thread_support ---");
  hpcrun_init_thread_support();
  hpcrun_set_using_threads(1);
  TMSG(THREAD,"Init thread support done");

  hpcrun_async_unblock();
}


void*
monitor_thread_pre_create(void)
{
  // N.B.: monitor_thread_pre_create() can be called before
  // monitor_init_thread_support() or even monitor_init_process().
  if (! hpcrun_is_initialized()) {
    return NULL;
  }

  // INVARIANTS at this point:
  //   1. init-process has occurred.
  //   2. current execution context is either the spawning process or thread.
  hpcrun_async_block();
  TMSG(THREAD,"pre create");

  // -------------------------------------------------------
  // Capture new thread's creation context, skipping 1 level of context
  //   WARNING: Do not move the call to getcontext()
  // -------------------------------------------------------
  cct_ctxt_t* thr_ctxt = NULL;

  ucontext_t context;
  int ret = getcontext(&context);
  if (ret != 0) {
    EMSG("error: monitor_thread_pre_create: getcontext = %d", ret);
    goto fini;
  }

  int metric_id = 0; // FIXME: obtain index of first metric
  //FIXME: skipInner below really needs to be 1. right now, it is
  //       being left as 0 because hpcprof (in some cases) yields
  //       the name monitor_adjust_stack rather than 
  //       pthread_create for the resulting innermost frame.
  cct_node_t* n =
    hpcrun_sample_callpath(&context, metric_id, 0/*metricIncr*/,
			   0/*skipInner*/, 1/*isSync*/);

  // MFAGAN: NEED TO COPY CONTEXT BTRACE TO NON-FREEABLE MEM
  n = hpcrun_copy_btrace(n);

  TMSG(THREAD,"before lush malloc");
  TMSG(MALLOC," -thread_precreate: lush malloc");
  epoch_t* epoch = hpcrun_get_epoch();
  thr_ctxt = hpcrun_malloc(sizeof(cct_ctxt_t));
  TMSG(THREAD,"after lush malloc, thr_ctxt = %p",thr_ctxt);
  thr_ctxt->context = n;
  thr_ctxt->parent = epoch->csdata_ctxt;

 fini:
  TMSG(THREAD,"->finish pre create");
  hpcrun_async_unblock();
  return thr_ctxt;
}


void
monitor_thread_post_create(void* data)
{
  if (! hpcrun_is_initialized()) {
    return;
  }
  hpcrun_async_block();

  TMSG(THREAD,"post create");
  TMSG(THREAD,"done post create");

  hpcrun_async_unblock();
}


void* 
monitor_init_thread(int tid, void* data)
{
  NMSG(THREAD,"init thread %d",tid);
  void* thread_data = hpcrun_thread_init(tid, (cct_ctxt_t*)data);
  TMSG(THREAD,"back from init thread %d",tid);

  trace_open();
  hpcrun_async_unblock();

  return thread_data;
}


void
monitor_fini_thread(void* init_thread_data)
{
  hpcrun_async_block();

  epoch_t *epoch = (epoch_t *)init_thread_data;

  hpcrun_thread_fini(epoch);
  trace_close();

  hpcrun_async_unblock();
}


size_t
monitor_reset_stacksize(size_t old_size)
{
  static const size_t MEG = (1024 * 1024);

  size_t new_size = old_size + MEG;

  if (new_size < 2 * MEG)
    new_size = 2 * MEG;

  return new_size;
}


//***************************************************************************
// (sig)longjmp for trampoline (via monitor extensions)
//***************************************************************************

typedef void longjmp_fcn(jmp_buf, int);

#ifdef HPCRUN_STATIC_LINK
extern longjmp_fcn    __real_longjmp;
extern siglongjmp_fcn __real_siglongjmp;
#endif

static longjmp_fcn    *real_longjmp = NULL;
static siglongjmp_fcn *real_siglongjmp = NULL;


siglongjmp_fcn *
hpcrun_get_real_siglongjmp(void)
{
  MONITOR_EXT_GET_NAME_WRAP(real_siglongjmp, siglongjmp);
  return real_siglongjmp;
}


void
MONITOR_EXT_WRAP_NAME(longjmp)(jmp_buf buf, int val)
{
  hpcrun_async_block();
  MONITOR_EXT_GET_NAME_WRAP(real_longjmp, longjmp);

  hpcrun_async_unblock();
  (*real_longjmp)(buf, val);

  // Never reached, but silence a compiler warning.
  EEMSG("return from real longjmp(), should never get here");
  _exit(1);
}


void
MONITOR_EXT_WRAP_NAME(siglongjmp)(sigjmp_buf buf, int val)
{
  hpcrun_async_block();
  hpcrun_get_real_siglongjmp();

  hpcrun_async_unblock();
  (*real_siglongjmp)(buf, val);

  // Never reached, but silence a compiler warning.
  EEMSG("return from real siglongjmp(), should never get here");
  _exit(1);
}


//***************************************************************************
// thread control (via our monitoring extensions)
//***************************************************************************

// ---------------------------------------------------------
// mutex_lock
// ---------------------------------------------------------

#ifdef LUSH_PTHREADS

typedef int mutex_lock_fcn(pthread_mutex_t *);

#ifdef HPCRUN_STATIC_LINK
//extern mutex_lock_fcn __real_pthread_mutex_lock;
extern mutex_lock_fcn __real_pthread_mutex_trylock;
extern mutex_lock_fcn __real_pthread_mutex_unlock;
#endif // HPCRUN_STATIC_LINK

//static mutex_lock_fcn *real_mutex_lock = NULL;
static mutex_lock_fcn *real_mutex_trylock = NULL;
static mutex_lock_fcn *real_mutex_unlock = NULL;


int
MONITOR_EXT_WRAP_NAME(pthread_mutex_lock)(pthread_mutex_t* lock)
{
  // N.B.: do not use dlsym() to obtain "real_pthread_mutex_lock"
  // because dlsym() indirectly calls calloc(), which can call
  // pthread_mutex_lock().
  extern int __pthread_mutex_lock(pthread_mutex_t* lock);
  //MONITOR_EXT_GET_NAME_WRAP(real_mutex_lock, pthread_mutex_lock);

  if (0) { TMSG(MONITOR_EXTS, "%s", __func__); }

  if (hpcrun_is_initialized()) {
    lushPthr_mutexLock_pre(&TD_GET(pthr_metrics), lock);
  }

  int ret = __pthread_mutex_lock(lock);

  if (hpcrun_is_initialized()) {
    lushPthr_mutexLock_post(&TD_GET(pthr_metrics), lock /*,ret*/);
  }

  return ret;
}


int
MONITOR_EXT_WRAP_NAME(pthread_mutex_trylock)(pthread_mutex_t* lock)
{
  MONITOR_EXT_GET_NAME_WRAP(real_mutex_trylock, pthread_mutex_trylock);
  if (0) { TMSG(MONITOR_EXTS, "%s", __func__); }

  int ret = (*real_mutex_trylock)(lock);

  if (hpcrun_is_initialized()) {
    lushPthr_mutexTrylock_post(&TD_GET(pthr_metrics), lock, ret);
  }

  return ret;
}


int
MONITOR_EXT_WRAP_NAME(pthread_mutex_unlock)(pthread_mutex_t* lock)
{
  MONITOR_EXT_GET_NAME_WRAP(real_mutex_unlock, pthread_mutex_unlock);
  if (0) { TMSG(MONITOR_EXTS, "%s", __func__); }

  int ret = (*real_mutex_unlock)(lock);

  if (hpcrun_is_initialized()) {
    lushPthr_mutexUnlock_post(&TD_GET(pthr_metrics), lock /*,ret*/);
  }

  return ret;
}

#endif // LUSH_PTHREADS


// ---------------------------------------------------------
// spin_lock
// ---------------------------------------------------------

#ifdef LUSH_PTHREADS

typedef int spinlock_fcn(pthread_spinlock_t *);

#ifdef HPCRUN_STATIC_LINK
extern spinlock_fcn __real_pthread_spin_lock;
extern spinlock_fcn __real_pthread_spin_trylock;
extern spinlock_fcn __real_pthread_spin_unlock;
extern spinlock_fcn __real_pthread_spin_destroy;
#endif // HPCRUN_STATIC_LINK

static spinlock_fcn *real_spin_lock = NULL;
static spinlock_fcn *real_spin_trylock = NULL;
static spinlock_fcn *real_spin_unlock = NULL;
static spinlock_fcn *real_spin_destroy = NULL;


int
MONITOR_EXT_WRAP_NAME(pthread_spin_lock)(pthread_spinlock_t* lock)
{
  MONITOR_EXT_GET_NAME_WRAP(real_spin_lock, pthread_spin_lock);
  if (0) { TMSG(MONITOR_EXTS, "%s", __func__); }

  pthread_spinlock_t* real_lock = lock;
  if (hpcrun_is_initialized()) {
    real_lock = lushPthr_spinLock_pre(&TD_GET(pthr_metrics), lock);
  }

#if (LUSH_PTHR_FN_TY == 3)
  int ret = lushPthr_spin_lock(lock);
#else
  int ret = (*real_spin_lock)(real_lock);
#endif

  if (hpcrun_is_initialized()) {
    lushPthr_spinLock_post(&TD_GET(pthr_metrics), lock /*,ret*/);
  }

  return ret;
}


int
MONITOR_EXT_WRAP_NAME(pthread_spin_trylock)(pthread_spinlock_t* lock)
{
  MONITOR_EXT_GET_NAME_WRAP(real_spin_trylock, pthread_spin_trylock);
  if (0) { TMSG(MONITOR_EXTS, "%s", __func__); }

  pthread_spinlock_t* real_lock = lock;
  if (hpcrun_is_initialized()) {
    real_lock = lushPthr_spinTrylock_pre(&TD_GET(pthr_metrics), lock);
  }

#if (LUSH_PTHR_FN_TY == 3)
  int ret = lushPthr_spin_trylock(lock);
#else
  int ret = (*real_spin_trylock)(real_lock);
#endif

  if (hpcrun_is_initialized()) {
    lushPthr_spinTrylock_post(&TD_GET(pthr_metrics), lock, ret);
  }

  return ret;
}


int
MONITOR_EXT_WRAP_NAME(pthread_spin_unlock)(pthread_spinlock_t* lock)
{
  MONITOR_EXT_GET_NAME_WRAP(real_spin_unlock, pthread_spin_unlock);
  if (0) { TMSG(MONITOR_EXTS, "%s", __func__); }

  pthread_spinlock_t* real_lock = lock;
  if (hpcrun_is_initialized()) {
    real_lock = lushPthr_spinUnlock_pre(&TD_GET(pthr_metrics), lock);
  }

#if (LUSH_PTHR_FN_TY == 3)
  int ret = lushPthr_spin_unlock(lock);
#else
  int ret = (*real_spin_unlock)(real_lock);
#endif

  if (hpcrun_is_initialized()) {
    lushPthr_spinUnlock_post(&TD_GET(pthr_metrics), lock /*,ret*/);
  }

  return ret;
}


int
MONITOR_EXT_WRAP_NAME(pthread_spin_destroy)(pthread_spinlock_t* lock)
{
  MONITOR_EXT_GET_NAME_WRAP(real_spin_destroy, pthread_spin_destroy);
  if (0) { TMSG(MONITOR_EXTS, "%s", __func__); }

  pthread_spinlock_t* real_lock = lock;
  if (hpcrun_is_initialized()) {
    real_lock = lushPthr_spinDestroy_pre(&TD_GET(pthr_metrics), lock);
  }

  int ret = (*real_spin_destroy)(real_lock);

  if (hpcrun_is_initialized()) {
    lushPthr_spinDestroy_post(&TD_GET(pthr_metrics), lock /*,ret*/);
  }

  return ret;
}

#endif // LUSH_PTHREADS


// ---------------------------------------------------------
// cond_wait
// ---------------------------------------------------------

#ifdef LUSH_PTHREADS

typedef int cond_init_fcn(pthread_cond_t *, const pthread_condattr_t *);
typedef int cond_destroy_fcn(pthread_cond_t *);
typedef int cond_wait_fcn(pthread_cond_t *, pthread_mutex_t *);
typedef int cond_timedwait_fcn(pthread_cond_t *, pthread_mutex_t *,
			       const struct timespec *);
typedef int cond_signal_fcn(pthread_cond_t *);

#ifdef HPCRUN_STATIC_LINK
extern cond_init_fcn    __real_pthread_cond_init;
extern cond_destroy_fcn __real_pthread_cond_destroy;
extern cond_wait_fcn      __real_pthread_cond_wait;
extern cond_timedwait_fcn __real_pthread_cond_timedwait;
extern cond_signal_fcn __real_pthread_cond_signal;
extern cond_signal_fcn __real_pthread_cond_broadcast;
#endif // HPCRUN_STATIC_LINK

static cond_init_fcn    *real_cond_init = NULL;
static cond_destroy_fcn *real_cond_destroy = NULL;
static cond_wait_fcn      *real_cond_wait = NULL;
static cond_timedwait_fcn *real_cond_timedwait = NULL;
static cond_signal_fcn *real_cond_signal = NULL;
static cond_signal_fcn *real_cond_broadcast = NULL;


// N.B.: glibc defines multiple versions of the cond-wait functions.
// For some reason, dlsym-ing any one routine does *not* necessarily
// obtain the correct version.  It turns out to be necessary to
// override a 'covering set' of the cond-wait functions to obtain a
// consistent set.

int
MONITOR_EXT_WRAP_NAME(pthread_cond_init)(pthread_cond_t* cond,
					 const pthread_condattr_t* attr)
{
  MONITOR_EXT_GET_NAME_WRAP(real_cond_init, pthread_cond_init);
  if (0) { TMSG(MONITOR_EXTS, "%s", __func__); }
  return (*real_cond_init)(cond, attr);
}


int
MONITOR_EXT_WRAP_NAME(pthread_cond_destroy)(pthread_cond_t* cond)
{
  MONITOR_EXT_GET_NAME_WRAP(real_cond_destroy, pthread_cond_destroy);
  if (0) { TMSG(MONITOR_EXTS, "%s", __func__); }
  return (*real_cond_destroy)(cond);
}


int
MONITOR_EXT_WRAP_NAME(pthread_cond_wait)(pthread_cond_t* cond,
					 pthread_mutex_t* mutex)
{
  MONITOR_EXT_GET_NAME_WRAP(real_cond_wait, pthread_cond_wait);
  if (0) { TMSG(MONITOR_EXTS, "%s", __func__); }

  if (hpcrun_is_initialized()) {
    lushPthr_condwait_pre(&TD_GET(pthr_metrics));
  }

  int ret = (*real_cond_wait)(cond, mutex);

  if (hpcrun_is_initialized()) {
    lushPthr_condwait_post(&TD_GET(pthr_metrics) /*,ret*/);
  }

  return ret;
}


int
MONITOR_EXT_WRAP_NAME(pthread_cond_timedwait)(pthread_cond_t* cond,
					      pthread_mutex_t* mutex,
					      const struct timespec* tspec)
{
  MONITOR_EXT_GET_NAME_WRAP(real_cond_timedwait, pthread_cond_timedwait);
  if (0) { TMSG(MONITOR_EXTS, "%s", __func__); }

  if (hpcrun_is_initialized()) {
    lushPthr_condwait_pre(&TD_GET(pthr_metrics));
  }

  int ret = (*real_cond_timedwait)(cond, mutex, tspec);

  if (hpcrun_is_initialized()) {
    lushPthr_condwait_post(&TD_GET(pthr_metrics) /*,ret*/);
  }

  return ret;
}


int
MONITOR_EXT_WRAP_NAME(pthread_cond_signal)(pthread_cond_t* cond)
{
  MONITOR_EXT_GET_NAME_WRAP(real_cond_signal, pthread_cond_signal);
  if (0) { TMSG(MONITOR_EXTS, "%s", __func__); }
  return (*real_cond_signal)(cond);
}


int
MONITOR_EXT_WRAP_NAME(pthread_cond_broadcast)(pthread_cond_t* cond)
{
  MONITOR_EXT_GET_NAME_WRAP(real_cond_broadcast, pthread_cond_broadcast);
  if (0) { TMSG(MONITOR_EXTS, "%s", __func__); }
  return (*real_cond_broadcast)(cond);
}

#endif // LUSH_PTHREADS


//***************************************************************************
// dynamic linking control (via libmonitor)
//***************************************************************************


#ifndef HPCRUN_STATIC_LINK

void
monitor_pre_dlopen(const char *path, int flags)
{
  if (! hpcrun_is_initialized()) {
    return;
  }
  hpcrun_async_block();

  hpcrun_pre_dlopen(path, flags);
  hpcrun_async_unblock();
}


void
monitor_dlopen(const char *path, int flags, void* handle)
{
  if (! hpcrun_is_initialized()) {
    return;
  }
  hpcrun_async_block();

  hpcrun_dlopen(path, flags, handle);
  hpcrun_async_unblock();
}


void
monitor_dlclose(void* handle)
{
  if (! hpcrun_is_initialized()) {
    return;
  }
  hpcrun_async_block();

  hpcrun_dlclose(handle);
  hpcrun_async_unblock();
}


void
monitor_post_dlclose(void* handle, int ret)
{
  if (! hpcrun_is_initialized()) {
    return;
  }
  hpcrun_async_block();

  hpcrun_post_dlclose(handle, ret);
  hpcrun_async_unblock();
}

#endif /* ! HPCRUN_STATIC_LINK */

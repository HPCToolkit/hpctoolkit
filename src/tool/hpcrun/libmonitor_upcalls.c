// -*-Mode: C++;-*- // technically C99
// $Id$


//***************************************************************************
// system include files 
//***************************************************************************
//
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef LINUX
#include <linux/unistd.h>
#endif



//***************************************************************************
// local include files 
//***************************************************************************

#include "monitor.h"
#include "csproflib.h"
#include "libmonitor_upcalls.h"

#include "env.h"
#include "files.h"
#include "name.h"
#include "epoch.h"
#include "structs.h"
#include "sample_event.h"
#include "csprof_monitor_callbacks.h"
#include "pmsg.h"
#include "sample_sources_registered.h"
#include "sample_sources_all.h"
#include "csprof_dlfns.h"
#include "fnbounds_interface.h"
#include "thread_data.h"
#include "thread_use.h"
#include "trace.h"



//***************************************************************************
// local variables 
//***************************************************************************

static volatile int DEBUGGER_WAIT = 1;

bool csprof_no_samples = true;

//***************************************************************************
// interface functions
//***************************************************************************

//
// Whenever a thread enters csprof synchronously via a monitor
// callback, don't allow it to reenter asynchronously via a signal
// handler.  Too much of csprof is not signal-handler safe to allow
// this.  For example, printing debug messages could deadlock if the
// signal hits while holding the MSG lock.
//
// This block is only needed per-thread, so the "suspend_sampling"
// thread data is a convenient place to store this.
//
static inline void
hpcrun_async_block(void)
{
  TD_GET(suspend_sampling) = 1;
}

static inline void
hpcrun_async_unblock(void)
{
  TD_GET(suspend_sampling) = 0;
}

int
hpcrun_async_is_blocked(void)
{
  return (! csprof_td_avail()) || (TD_GET(suspend_sampling) && !ENABLED(ASYNC_RISKY));
}


//
// In the monitor callbacks, block two things:
//
//   1. Skip the callback if csprof is not yet initialized.
//   2. Block async reentry for the duration of the callback.
//
// Init-process and init-thread are where we do the initialization, so
// they're special.  Also, monitor promises that init and fini process
// and thread are run in sequence, but dlopen, fork, pthread-create
// can occur out of sequence (in init constructor).
//
#define PROC_NAME_LEN  2048
void *
monitor_init_process(int *argc, char **argv, void *data)
{
  char *process_name;
  char buf[PROC_NAME_LEN];

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

  csprof_set_using_threads(0);

  files_set_executable(process_name);

  csprof_registered_sources_init();
  char *s = getenv(HPCRUN_EVENT_LIST);
  if (s == NULL){
    s = getenv("CSPROF_OPT_EVENT");
  }
  csprof_sample_sources_from_eventlist(s);
  csprof_no_samples = csprof_check_named_source("NONE");
  if (getenv("SHOW_NONE") && csprof_no_samples) {
    fprintf(stderr,"NOTE: sample source NONE is specified\n");
  }

  files_set_directory();

  pmsg_init();
  TMSG(PROCESS,"init");

  csprof_init_internal();
  if (ENABLED(TST)){
    EEMSG("TST debug ctl is active!");
    STDERR_MSG("Std Err message appears");
  }
  hpcrun_async_unblock();

  return data;
}


static struct _ff {
  int flag;
} from_fork;


void *
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
monitor_post_fork(pid_t child, void *data)
{
  if (! hpcrun_is_initialized()) {
    return;
  }
  hpcrun_async_block();

  NMSG(POST_FORK,"Post fork call");

  if (!SAMPLE_SOURCES(started)){
    NMSG(POST_FORK,"sample sources re-init+re-start");
    SAMPLE_SOURCES(init);
#if 0
	//--------------------------------------------------------
	// comment out event list processing in post fork because 
	// we inherit an initialized event list across the fork 
	// 2008 06 08 - John Mellor-Crummey
	//-----------------------------------------------------
    SAMPLE_SOURCES(process_event_list);
#endif
    SAMPLE_SOURCES(gen_event_set,0); // FIXME: pass lush_metrics here somehow
    SAMPLE_SOURCES(start);
  }

  NMSG(POST_FORK,"Finished post fork");
  hpcrun_async_unblock();
}


void
monitor_fini_process(int how, void *data)
{
  hpcrun_async_block();

  csprof_fini_internal();
  trace_close();
  fnbounds_fini();

  hpcrun_async_unblock();
}


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


#ifdef CSPROF_THREADS

void
monitor_init_thread_support(void)
{
  hpcrun_async_block();

  NMSG(THREAD,"REALLY init_thread_support ---");
  csprof_init_thread_support();
  csprof_set_using_threads(1);
  NMSG(THREAD,"Init thread support done");

  hpcrun_async_unblock();
}


void *
monitor_thread_pre_create(void)
{
  // N.B.: monitor_thread_pre_create() can be called before
  // monitor_init_thread_support() or even monitor_init_process().
  if (! hpcrun_is_initialized()) {
    return NULL;
  }
  hpcrun_async_block();

  NMSG(THREAD,"pre create");

  void *ret = csprof_thread_pre_create();

  NMSG(THREAD,"->finish pre create");
  hpcrun_async_unblock();

  return ret;
}


void
monitor_thread_post_create(void *dc)
{
  if (! hpcrun_is_initialized()) {
    return;
  }
  hpcrun_async_block();

  NMSG(THREAD,"post create");
  csprof_thread_post_create(dc);
  NMSG(THREAD,"done post create");

  hpcrun_async_unblock();
}


void *
monitor_init_thread(int tid, void *data)
{
  NMSG(THREAD,"init thread %d",tid);
  void *thread_data = csprof_thread_init(tid, (lush_cct_ctxt_t*)data);
  NMSG(THREAD,"back from init thread %d",tid);

  trace_open();
  hpcrun_async_unblock();

  return thread_data;
}


void
monitor_thread_pre_lock(void)
{
  if (! hpcrun_is_initialized()) {
    return;
  }
  //hpcrun_async_block();

  TMSG(MONITOR_EXTS, "%s", __func__);
#ifdef LUSH_PTHREADS
  lush_pthr__lock_pre(&TD_GET(pthr_metrics));
#endif

  //hpcrun_async_unblock();
}


void
monitor_thread_post_lock(int result)
{
  if (! hpcrun_is_initialized()) {
    return;
  }
  //hpcrun_async_block();

  TMSG(MONITOR_EXTS, "%s", __func__);
#ifdef LUSH_PTHREADS
  lush_pthr__lock_post(&TD_GET(pthr_metrics));
#endif

  //hpcrun_async_unblock();
}


void
monitor_thread_post_trylock(int result)
{
  if (! hpcrun_is_initialized()) {
    return;
  }
  // hpcrun_async_block();

  TMSG(MONITOR_EXTS, "%s", __func__);
#ifdef LUSH_PTHREADS
  lush_pthr__trylock(&TD_GET(pthr_metrics), result);
#endif

  // hpcrun_async_unblock();
}


void
monitor_thread_unlock(void)
{
  if (! hpcrun_is_initialized()) {
    return;
  }
  // hpcrun_async_block();

  TMSG(MONITOR_EXTS, "%s", __func__);
#ifdef LUSH_PTHREADS
  lush_pthr__unlock(&TD_GET(pthr_metrics));
#endif

  // hpcrun_async_unblock();
}


void
monitor_thread_pre_cond_wait(void)
{
  if (! hpcrun_is_initialized()) {
    return;
  }
  // hpcrun_async_block();

  TMSG(MONITOR_EXTS, "%s", __func__);
#ifdef LUSH_PTHREADS
  lush_pthr__condwait_pre(&TD_GET(pthr_metrics));
#endif

  // hpcrun_async_unblock();
}


void
monitor_thread_post_cond_wait(int result)
{
  if (! hpcrun_is_initialized()) {
    return;
  }
  //hpcrun_async_block();

  TMSG(MONITOR_EXTS, "%s", __func__);
#ifdef LUSH_PTHREADS
  lush_pthr__condwait_post(&TD_GET(pthr_metrics));
#endif

  //hpcrun_async_unblock();
}


void
monitor_fini_thread(void *init_thread_data)
{
  hpcrun_async_block();

  csprof_state_t *state = (csprof_state_t *)init_thread_data;

  csprof_thread_fini(state);
  trace_close();

  hpcrun_async_unblock();
}


#define MEG  (1024 * 1024)
size_t
monitor_reset_stacksize(size_t old_size)
{
  size_t new_size = old_size + MEG;

  if (new_size < 2 * MEG)
    new_size = 2 * MEG;

  return new_size;
}

#endif  /* CSPROF_THREADS */



#ifndef HPCRUN_STATIC_LINK

void
monitor_pre_dlopen(const char *path, int flags)
{
  if (! hpcrun_is_initialized()) {
    return;
  }
  hpcrun_async_block();

  csprof_pre_dlopen(path, flags);
  hpcrun_async_unblock();
}


void
monitor_dlopen(const char *path, int flags, void *handle)
{
  if (! hpcrun_is_initialized()) {
    return;
  }
  hpcrun_async_block();

  csprof_dlopen(path, flags, handle);
  hpcrun_async_unblock();
}


void
monitor_dlclose(void *handle)
{
  if (! hpcrun_is_initialized()) {
    return;
  }
  hpcrun_async_block();

  csprof_dlclose(handle);
  hpcrun_async_unblock();
}


void
monitor_post_dlclose(void *handle, int ret)
{
  if (! hpcrun_is_initialized()) {
    return;
  }
  hpcrun_async_block();

  csprof_post_dlclose(handle, ret);
  hpcrun_async_unblock();
}

#endif /* ! HPCRUN_STATIC_LINK */

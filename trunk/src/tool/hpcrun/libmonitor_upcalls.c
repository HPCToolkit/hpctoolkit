// -*-Mode: C++;-*- // technically C99
// $Id$


//***************************************************************************
// system include files 
//***************************************************************************
//
#include <pthread.h>

#ifdef LINUX
#include <linux/unistd.h>
#endif



//***************************************************************************
// local include files 
//***************************************************************************

#include "monitor.h"

#include "samples_sources_all.h"
#include "files.h"
#include "general.h"
#include "name.h"
#include "epoch.h"
#include "structs.h"
#include "sample_event.h"
#include "csprof_monitor_callbacks.h"
#include "pmsg.h"
#include "registered_sample_sources.h"
#include "csprof_dlfns.h"
#include "fnbounds_interface.h"
#include "trace.h"


//***************************************************************************
// global variables 
//***************************************************************************

// --------------------------------------------------------------------------
// default is single threaded. monitor_init_thread_support sets this variable
// to 1 if threads are used.
// --------------------------------------------------------------------------
int csprof_using_threads = 0;



//***************************************************************************
// local variables 
//***************************************************************************

volatile int DEBUGGER_WAIT = 1;

//***************************************************************************
// interface functions
//***************************************************************************

void *
monitor_init_process(int *argc, char **argv, void *data)
{
  char *process_name = argv[0];

  if (getenv("CSPROF_WAIT")){
    while(DEBUGGER_WAIT);
  }

  files_set_directory();
  files_set_executable(process_name);

  pmsg_init();
  NMSG(PROCESS,"init");

  csprof_init_internal();

  return data;
}

static struct _ff {
  int flag;
} from_fork;

void *
monitor_pre_fork(void)
{
  NMSG(PRE_FORK,"pre_fork call");

  if (SAMPLE_SOURCES(started)) {
    NMSG(PRE_FORK,"sources shutdown");
    SAMPLE_SOURCES(stop);
    SAMPLE_SOURCES(shutdown);
  }

  NMSG(PRE_FORK,"finished pre_fork call");

  return (void *)(&from_fork);
}

void
monitor_post_fork(pid_t child, void *data)
{
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
}


void
monitor_fini_process(int how, void *data)
{
  csprof_fini_internal();
  trace_close();
  fnbounds_fini();
}


#ifdef CSPROF_THREADS

void
monitor_init_thread_support(void)
{
  NMSG(THREAD,"REALLY init_thread_support ---");
  csprof_init_thread_support();
  csprof_using_threads = 1;
  NMSG(THREAD,"Init thread support done");
}


void *
monitor_thread_pre_create(void)
{
  // N.B.: monitor_thread_pre_create() can be called before
  // monitor_init_thread_support() or even monitor_init_process().
  NMSG(THREAD,"pre create");

  void *ret = csprof_thread_pre_create();

  NMSG(THREAD,"->finish pre create");
  return ret;
  // return csprof_thread_pre_create();
}


void
monitor_thread_post_create(void *dc)
{
  NMSG(THREAD,"post create");
  csprof_thread_post_create(dc);
  NMSG(THREAD,"done post create");
}


void *
monitor_init_thread(int tid, void *data)
{
  NMSG(THREAD,"init thread %d",tid);
  void *thread_data = csprof_thread_init(tid, (lush_cct_ctxt_t*)data);
  NMSG(THREAD,"back from init thread %d",tid);

  trace_open();

  return thread_data;
}


void 
monitor_fini_thread(void *init_thread_data)
{
  csprof_state_t *state = (csprof_state_t *)init_thread_data;

  csprof_thread_fini(state);

  trace_close();
}

#endif


#ifndef HPCRUN_STATIC_LINK

void
monitor_pre_dlopen(const char *path, int flags)
{
  csprof_pre_dlopen(path, flags);
}


void 
monitor_dlopen(const char *path, int flags, void *handle)
{
  csprof_dlopen(path, flags, handle);

#if 0
  csprof_epoch_t *epoch;
  csprof_epoch_lock();

  /* create a new epoch */
  epoch = csprof_epoch_new();

  dl_add_module(path);

  csprof_epoch_unlock();
#endif
}


void 
monitor_dlclose(void *handle)
{
  csprof_dlclose(handle);
/*
  assert(0);

  dl_remove_library(handle);
  FIXME: delete intervals from the splay tree too
*/
}

#endif /* HPCRUN_STATIC_LINK */

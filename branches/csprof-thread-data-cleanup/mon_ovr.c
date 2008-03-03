// -*-Mode: C++;-*- // technically C99
// $Id$

//************************* System Include Files ****************************

#include <pthread.h>

//*************************** User Include Files ****************************

#include "general.h"
#include "killsafe.h"
#include "monitor.h"
#include "name.h"
#include "epoch.h"
#include "dl_bound.h"
#include "structs.h"
#include "sample_event.h"
#include "csprof_monitor_callbacks.h"

#include "pmsg.h"

#ifdef LINUX
#include <linux/unistd.h>
#endif

//*************************** Forward Declarations **************************

#define M(s) write(2,s"\n",strlen(s)+1)



//***************************************************************************

extern int wait_for_gdb;

void
monitor_init_process(char *process,int *argc,char **argv,unsigned pid)
{
  if (getenv("CSPROF_WAIT")){
    while(wait_for_gdb);
  }
  csprof_set_executable_name(process);
  pmsg_init(process);
  NMSG(PROCESS,"init");
  csprof_init_internal();
}


void
monitor_fini_process(void)
{
  // M("monitor calling csprof_fini_internal");
  csprof_fini_internal();
}

#if 0
void
monitor_init_library(void)
{
  //  extern void csprof_init_internal(void);
  // M("monitor init lib (NOT) calling csprof_init_internal");
}

void monitor_fini_library(void)
{
  //  extern void csprof_fini_internal(void);
}
#endif

// always need this variable, but only init thread support will turn it on
int csprof_using_threads = 0;

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
  killsafe_t    *safe;

  NMSG(THREAD,"init thread %d",tid);
  safe = (killsafe_t *)malloc(sizeof(killsafe_t));
  csprof_thread_init(safe, tid, (lush_cct_ctxt_t*)data);
  NMSG(THREAD,"back from init thread %d",tid);

  return (void *) safe;
}


void monitor_fini_thread(void *init_thread_data )
{
  csprof_state_t *state = ((killsafe_t *)init_thread_data)->state;

  csprof_thread_fini(state);
}


void monitor_dlopen(const char *path, int flags, void *handle)
{
  csprof_epoch_t *epoch;
  csprof_epoch_lock();

  /* create a new epoch */
  epoch = csprof_epoch_new();

  dl_add_module(path);

  csprof_epoch_unlock();
}


void monitor_dlclose(void *handle) // (const char *library)
{
// we really want the library name
/*
 
  assert(0);

  dl_remove_library(library);
  FIXME: delete intervals from the splay tree too
*/
}


#endif

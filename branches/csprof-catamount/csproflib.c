// -*-Mode: C++;-*- // technically C99
// $Id$

// * BeginRiceCopyright *****************************************************
/*
  Copyright ((c)) 2002, Rice University 
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  * Neither the name of Rice University (RICE) nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  This software is provided by RICE and contributors "as is" and any
  express or implied warranties, including, but not limited to, the
  implied warranties of merchantability and fitness for a particular
  purpose are disclaimed. In no event shall RICE or contributors be
  liable for any direct, indirect, incidental, special, exemplary, or
  consequential damages (including, but not limited to, procurement of
  substitute goods or services; loss of use, data, or profits; or
  business interruption) however caused and on any theory of liability,
  whether in contract, strict liability, or tort (including negligence
  or otherwise) arising in any way out of the use of this software, even
  if advised of the possibility of such damage.
*/
// ******************************************************* EndRiceCopyright *

//***************************************************************************
//
// File: 
//    $Source$
//
// Purpose:
//    Initialize, finalize csprof
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

// call stack profiler: profiles one process running on one processor
//   platforms: alpha tru64 unix
//     -- will profile accross a fork() ??? [this includes system()]
//     -- add support for profiling DSOs (use stuff from papiprof)
//     -- add support for profiling threads
//
//   limitations to implicit interface: 
//     - cannot profile any programs that are restricted from using
//       LD_PRELOAD (for security reasons)

/* system include files */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

/* user include files */

#include "xpthread.h"
#include "atomic.h"
#include "libc.h"
#include "csproflib.h"
#include "csproflib_private.h"
#include "env.h"
#include "killsafe.h"
#include "mem.h"
#include "csprof_csdata.h"
#include "segv_handler.h"
#include "epoch.h"
#include "metrics.h"
#include "itimer.h"
#include "papi_sample.h"
#include "sample_event.h"

#include <lush/lush.h>

#include "name.h"

#include "hpcfile_csproflib.h"
#include "pmsg.h"
#include "prim_unw.h"
#include "dl_bound.h"
#include "dbg_extra.h"

#if 0
/* the library's basic state */
csprof_status_t status = CSPROF_STATUS_UNINIT;
#endif
static csprof_options_t opts;

#if !defined(CSPROF_SYNCHRONOUS_PROFILING) 
sigset_t prof_sigset;
#endif

int wait_for_gdb = 1;
int csprof_initialized = 0;

extern void _start(void);
extern int __stop___libc_freeres_ptrs;

void *static_epoch_offset;
void *static_epoch_end;
long static_epoch_size;

//
// process level setup (assumes pthreads not started yet)
//

static int evs;

void
csprof_init_internal(void)
{
  if (getenv("CSPROF_WAIT")){
    while(wait_for_gdb);
  }
  pmsg_init();

#ifdef DBG_EXTRA
  dbg_init();
#endif

  csprof_options__init(&opts);
  csprof_options__getopts(&opts);

#ifdef STATIC_ONLY
  static_epoch_offset = (void *)&_start;
  static_epoch_end    = (void *)&__stop___libc_freeres_ptrs;
  static_epoch_size   = (long) (static_epoch_end - static_epoch_offset);
#endif     

  /* private memory store for the initial thread is done below */

  csprof_malloc_init(opts.mem_sz, 0);

  /* epoch poking needs the memory manager init'd() (and
     the thread-specific memory manager if need be) */
  /* figure out what libraries we currently have loaded */
  csprof_epoch_lock();
  csprof_epoch_new();
  csprof_epoch_unlock();
  dl_init();
    
  /* profiling state needs the memory manager init'd */
  csprof_state_t *state = csprof_malloc(sizeof(csprof_state_t));

  csprof_set_state(state);

  csprof_state_init(state);
  csprof_state_alloc(state);

  // Initialize LUSH agents
  if (opts.lush_agent_paths[0] != '\0') {
    state->lush_agents = 
      (lush_agent_pool_t*)csprof_malloc(sizeof(lush_agent_pool_t));
    lush_agent_pool__init(state->lush_agents, opts.lush_agent_paths);
    MSG(0xfeed, "***> LUSH: %s (%p / %p) ***", opts.lush_agent_paths, 
	state, state->lush_agents);
  }

#if !defined(CSPROF_SYNCHRONOUS_PROFILING)
  MSG(1,"sigemptyset(prof_sigset)");
  sigemptyset(&prof_sigset);
  sigaddset(&prof_sigset,SIGPROF);
#endif

  setup_segv();
  unw_init();

#if 0
  csprof_set_max_metrics(2);
  int metric_id = csprof_new_metric(); /* weight */
  csprof_set_metric_info_and_period(metric_id, "# samples",
                                    CSPROF_METRIC_ASYNCHRONOUS,
                                    opts.sample_period);
  metric_id = csprof_new_metric(); /* calls */
  csprof_set_metric_info_and_period(metric_id, "# returns",
                                    CSPROF_METRIC_FLAGS_NIL, 1);
#endif
  if (opts.sample_source == ITIMER){
    csprof_itimer_init(&opts);
    if (csprof_itimer_start()){
      EMSG("WARNING: couldn't start itimer");
    }
  } else { // PAPI
    papi_setup();
    //    papi_event_info_from_opt(&opts,&code,&thresh);
    papi_parse_evlist(opts.papi_event_list);
    evs = papi_event_init();
    PMSG(PAPI,"PROCESS INIT papi event list = %d",evs);
    papi_pulse_init(evs);
  }
  csprof_initialized = 1;
}

#ifdef CSPROF_THREADS
extern pthread_key_t thread_node_key;
extern pthread_key_t prof_data_key;
extern pthread_key_t mem_store_key;
#include "thread_use.h"
#include "thread_data.h"

void
csprof_init_thread_support(int id)
{
  csprof_state_t *state = csprof_get_state();

  MSG(1,"csproflib init f initial thread");
  csprof_pthread_init_data();

  pthread_setspecific(mem_store_key,(void *)csprof_get_memstore());
  pthread_setspecific(prof_data_key,(void *)state);

  state->pstate.thrid = id;

  // Switch to threaded variants for various components of csprof
  mem_threaded();
  state_threaded();
  MSG(1,"switch to threaded versions complete");
}

void *
csprof_thread_pre_create(void)
{
  // N.B.: Can be called before init-thread-support or even init-process.
  // Therefore, we ignore any calls before process init time.

  int ret;

  if (!csprof_initialized) {
    return NULL;
  }

  // INVARIANTS at this point:
  //   1. init-process has occurred.
  //   2. current execution context is either the spawning process or thread.

  // Disable signals (could also disable timers)
  ret = pthread_sigmask(SIG_BLOCK,&prof_sigset,NULL);
  if (ret){
    EMSG("WARNING: Thread init could not block SIGPROF, ret = %d",ret);
  }

  // -------------------------------------------------------
  // Capture new thread's creation context.
  // -------------------------------------------------------
  csprof_state_t* state = csprof_get_state();

  ucontext_t context;
  ret = getcontext(&context);
  if (ret != 0) {
    EMSG("Error: getcontext = %d", ret); 
  }

  int metric_id = 0; // FIXME: should be able to obtain index of first metric
  if ( !(metric_id < csprof_num_recorded_metrics()) ) {
    DIE("Won't need this once the above is fixed", __FILE__, __LINE__);
  }

  // insert into CCT as a placeholder
  csprof_cct_node_t* n;
  n = csprof_sample_event(&context, metric_id, 0 /*sample_count*/);

  lush_cct_ctxt_t* thr_ctxt = csprof_malloc(sizeof(lush_cct_ctxt_t));
  thr_ctxt->context = n;
  thr_ctxt->parent = state->csdata_ctxt;

  return thr_ctxt;

  // Enable signals (done in post_create)
}

void
csprof_thread_post_create(void *dc)
{
  int ret = pthread_sigmask(SIG_UNBLOCK,&prof_sigset,NULL);
  if (ret){
    EMSG("WARNING: Thread init could not unblock SIGPROF, ret = %d",ret);
  }
}

void 
csprof_thread_init(killsafe_t *kk, int id, lush_cct_ctxt_t* thr_ctxt)
{
  csprof_state_t *state;
  csprof_mem_t *memstore;
  int ret;

  memstore = csprof_malloc_init(1, 0);

  if(memstore == NULL) {
    EMSG("Couldn't allocate mem for csprof_malloc in thread");
    abort();
  }
  DBGMSG_PUB(CSPROF_DBG_PTHREAD, "Setting mem_store_key f thread %d",id);
  pthread_setspecific(mem_store_key, memstore);

  state = csprof_malloc(sizeof(csprof_state_t));
  if(state == NULL) {
    DBGMSG_PUB(1, "Couldn't allocate memory for profiling state in thread");
  }

  DBGMSG_PUB(CSPROF_DBG_PTHREAD, "Allocated thread state, now init'ing and alloc'ing f thread %d",id);

  csprof_state_t* process_state = csprof_get_safe_state();

  MSG(0xfeed, "=> csprof_init_thread: 0 (%p)", process_state);
  csprof_state_init(state);
  csprof_state_alloc(state);
  state->lush_agents = process_state->lush_agents; // LUSH

  state->pstate.thrid = id; // local thread id in state
  state->csdata_ctxt = thr_ctxt;

  pthread_setspecific(prof_data_key,(void *)state);

  kk->state = state;  // save pointer to state data in killsafe

    /* FIXME: is this the right way to do things? */

  MSG(1,"driver init f thread");
  if (opts.sample_source == ITIMER){
    if (csprof_itimer_start()){
      EMSG("WARNING: couldn't start itimer");
    }
  }
  else { // PAPI
    PMSG(PAPI,"PAPI Thread init: id = %d",id);
    thread_data_t *td = (thread_data_t *) pthread_getspecific(my_thread_specific_key);
    td->eventSet = papi_event_init();
    papi_pulse_init(td->eventSet);
  }
  ret = pthread_sigmask(SIG_UNBLOCK,&prof_sigset,NULL);
  if (ret){
    EMSG("WARNING: Thread init could not unblock SIGPROF, ret = %d",ret);
  }
}


void
csprof_thread_fini(csprof_state_t *state)
{
  thread_data_t *td = (thread_data_t *) pthread_getspecific(my_thread_specific_key);

  if (csprof_initialized){
    MSG(1,"csprof thread fini");
    if (opts.sample_source == ITIMER){
      int fail = csprof_itimer_stop();
      if (fail){
        EMSG("WARNING: failed to stop itimer (in thread)");
      }
    } else { // PAPI
      PMSG(PAPI,"PAPI Thread fini: id = %d",td->id);
      papi_pulse_fini(td->eventSet);
    }
    csprof_write_profile_data(state);
  }
}
#endif

// csprof_fini_internal: 
// errors: handles all errors

void
csprof_fini_internal(void)
{
  extern int segv_count;
  extern int samples_taken;
  extern int bad_unwind_count;
  csprof_state_t *state;

  int ret = sigprocmask(SIG_BLOCK,&prof_sigset,NULL);
  if (ret){
    EMSG("WARNING: process fini could not block SIGPROF, ret = %d",ret);
  }

  if (csprof_initialized) {
    if (opts.sample_source == ITIMER){
      int fail = csprof_itimer_stop();
      if (fail){
        EMSG("WARNING: failed to stop itimer (in process)");
      }
    } else { // PAPI
      papi_pulse_fini(evs);
    }

    dl_fini();

    MSG(CSPROF_MSG_SHUTDOWN, "writing profile data");
    state = csprof_get_safe_state();
    csprof_write_profile_data(state);

    // shutdown LUSH agents
    if (state->lush_agents) {
      lush_agent_pool__fini(state->lush_agents);
    }

    EMSG("host %ld: %d samples total, %d samples dropped (%d segvs)\n",
	 gethostid(), samples_taken, bad_unwind_count, segv_count);

    pmsg_fini();
  }
}

csprof_state_t *
csprof_check_for_new_epoch(csprof_state_t *state)
{
  /* ugh, nasty race condition here:

  1. shared library state has changed since the last profile
  signal, so we enter the if;

  2. somebody else dlclose()'s a library which holds something
  located in our backtrace.  this is not in itself a problem,
  since we don't bother doing anything on dlclose()...;

  3. somebody else (thread in step 2 or a different thread)
  dlopen()'s a new shared object, which begins an entirely
  new epoch--one which does not include the shared object
  which resides in our backtrace;

  4. we create a new state which receives the epoch from step 3,
  not step 1, which is wrong.

  attempt to take baby steps to stop this.  more drastic action
  would involve grabbing the epoch lock, but I believe that would
  be unacceptably slow (both in the atomic instruction overhead
  and the simple fact that most programs are not frequent users
  of dl*). */

  csprof_epoch_t *current = csprof_get_epoch();

  if(state->epoch != current) {
    csprof_state_t *newstate = csprof_malloc(sizeof(csprof_state_t));

    MSG(CSPROF_MSG_EPOCH, "Creating new epoch...");

    /* we don't have to go through the usual csprof_state_{init,alloc}
       business here because most of the stuff we want is already
       in `state' */
    memcpy(newstate, state, sizeof(csprof_state_t));

    /* we do have to reinitialize the tree, though */
    csprof_csdata__init(&newstate->csdata);

    /* and reinsert backtraces */
    if(newstate->bufend - newstate->bufstk != 0) {
      newstate->treenode = NULL;
      csprof_state_insert_backtrace(newstate, 0, /* pick one */
                                    newstate->bufend - 1,
                                    newstate->bufstk,
                                    0);
    }

    /* and inform the state about its epoch */
    newstate->epoch = current;

    /* and finally, set the new state */
    csprof_set_state(newstate);

#ifdef CSPROF_THREADS
    ;
#endif

    return newstate;
  }
  else {
    return state;
  }
}

/* only meant for debugging errors, so it's not subject to the normal
   DBG variables and suchlike. */
void 
csprof_print_backtrace(csprof_state_t *state)
{
    csprof_frame_t *frame = state->bufstk;
    csprof_cct_node_t *tn = state->treenode;

    while(state->bufend != frame) {
        ERRMSG("Node: %#lx/%#lx, treenode: %#lx", __FILE__, __LINE__,
               frame->ip, frame->sp, tn);
        frame++;
        if(tn) tn = tn->parent;
    }
}

/* writing profile data */

int csprof_write_profile_data(csprof_state_t *state)
{

  extern int csprof_using_threads;

  int ret = CSPROF_OK, ret1, ret2;
  FILE* fs;

  /* Generate a filename */
  char fnm[CSPROF_FNM_SZ];


  if (csprof_using_threads){
    sprintf(fnm, "%s/%s.%lx-%x-%ld%s", opts.out_path,
            csprof_get_executable_name(), gethostid(), state->pstate.pid,
            state->pstate.thrid, CSPROF_OUT_FNM_SFX);
  }
  else {
    sprintf(fnm, "%s/%s.%lx-%x%s", opts.out_path, csprof_get_executable_name(),
            gethostid(), state->pstate.pid, CSPROF_OUT_FNM_SFX);
  }
  MSG(CSPROF_MSG_DATAFILE, "CSPROF write_profile_data: Writing %s", fnm);

  /* Open file for writing; fail if the file already exists. */
  fs = hpcfile_open_for_write(fnm, /* overwrite */ 0);
  ret1 = hpcfile_csprof_write(fs, csprof_get_metric_data());

  MSG(CSPROF_MSG_DATAFILE, "Done writing metric data");

  if(ret1 != HPCFILE_OK) {
    goto error;
  }

  MSG(CSPROF_MSG_DATAFILE, "Preparing to write epochs");
  csprof_write_all_epochs(fs);

  MSG(CSPROF_MSG_DATAFILE, "Done writing epochs");
  /* write profile states out to disk */
  {
    csprof_state_t *runner = state;
    unsigned int nstates = 0;
    unsigned long tsamps = 0;

    /* count states */
    while(runner != NULL) {
      if(runner->epoch != NULL) {
	nstates++;
	tsamps += runner->trampoline_samples;
      }
      runner = runner->next;
    }

    hpc_fwrite_le4(&nstates, fs);
    hpc_fwrite_le8(&tsamps, fs);

    /* write states */
    runner = state;

    while(runner != NULL) {
      if(runner->epoch != NULL) {
	MSG(CSPROF_MSG_DATAFILE, "Writing %ld nodes", runner->csdata.num_nodes);
	ret2 = csprof_csdata__write_bin(fs, runner->epoch->id, 
					&runner->csdata, runner->csdata_ctxt);
          
	if(ret2 != CSPROF_OK) {
	  MSG(CSPROF_MSG_DATAFILE, "Error writing tree %#lx", &runner->csdata);
	  MSG(CSPROF_MSG_DATAFILE, "Number of tree nodes lost: %ld", runner->csdata.num_nodes);
	  ERRMSG("could not save profile data to file '%s'", __FILE__, __LINE__, fnm);
	  perror("write_profile_data");
	  ret = CSPROF_ERR;
	}
      }
      else {
	MSG(CSPROF_MSG_DATAFILE, "Not writing tree %#lx; null epoch", &runner->csdata);
	MSG(CSPROF_MSG_DATAFILE, "Number of tree nodes lost: %ld", runner->csdata.num_nodes);
      }

      runner = runner->next;
    }
  }
          
  if(ret1 == HPCFILE_OK && ret2 == CSPROF_OK) {
    MSG(CSPROF_MSG_DATAFILE, "saved profile data to file '%s'", fnm);
  }
  /* if we've gotten this far, there haven't been any fatal errors */
  goto end;

 error:
 end:
  hpcfile_close(fs);

  return ret;
}

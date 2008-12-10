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

#include "sample_sources_all.h"
#include "atomic.h"
#include "files.h"
#include "libc.h"
#include "csproflib.h"
#include "csproflib_private.h"
#include "csprof_monitor_callbacks.h"
#include "env.h"
#include "mem.h"
#include "csprof_csdata.h"
#include "segv_handler.h"
#include "epoch.h"
#include "metrics.h"
#include "trace.h"

#include "csprof_options.h"

#include "sample_event.h"
#include "state.h"

#include "monitor.h"

#include "thread_use.h"
#include "thread_data.h"

#include "name.h"

#include "hpcfile_csproflib.h"
#include "pmsg.h"
#include "unwind.h"
#include "fnbounds_interface.h"
#include "intervals.h"

#include <lush/lush.h>
#include <lush/lush-backtrace.h>

static csprof_options_t opts;

#if !defined(CSPROF_SYNCHRONOUS_PROFILING) 
sigset_t prof_sigset;
#endif

int csprof_initialized = 0;

extern void _start(void);
extern int __stop___libc_freeres_ptrs;

void *static_epoch_offset;
void *static_epoch_end;
long static_epoch_size;

//
// process level setup (assumes pthreads not started yet)
//

int lush_metrics = 0; // FIXME: global variable for now

void
csprof_init_internal(void)
{
  /* private memory store for the initial thread is done below */

  csprof_thread_data_init(0,CSPROF_MEM_SZ_DEFAULT,0);

  /* epoch poking needs the memory manager init'd() (and
     the thread-specific memory manager if need be) */
  /* figure out what libraries we currently have loaded */

    csprof_epoch_lock();
    csprof_epoch_new();
    csprof_epoch_unlock();

    // WARNING: a perfmon bug requires us to fork off the fnbounds server before
    // we call PAPI_init, which is done in argument processing below. Also, fnbounds_init
    // must be done after the memory allocator is initialized.
  fnbounds_init();
  csprof_options__init(&opts);
  csprof_options__getopts(&opts);

  trace_init(); // this must go after thread initialization
  trace_open(); 

  // Initialize LUSH agents
  if (opts.lush_agent_paths[0] != '\0') {
    csprof_state_t* state = TD_GET(state);
    TMSG(MALLOC," -init_internal-: lush allocation");
    lush_agents = (lush_agent_pool_t*)csprof_malloc(sizeof(lush_agent_pool_t));
    lush_agent_pool__init(lush_agents, opts.lush_agent_paths);
    MSG(0xfeed, "***> LUSH: %s (%p / %p) ***", opts.lush_agent_paths, 
	state, lush_agents);
  }

  lush_metrics = (lush_agents) ? 1 : 0;

  sigemptyset(&prof_sigset);
  sigaddset(&prof_sigset,SIGPROF);

  setup_segv();
  unw_init();

  // sample source setup

  SAMPLE_SOURCES(init);
  SAMPLE_SOURCES(process_event_list,lush_metrics);
  SAMPLE_SOURCES(gen_event_set,lush_metrics);
  SAMPLE_SOURCES(start);

  csprof_initialized = 1;
}

#ifdef CSPROF_THREADS

void
csprof_init_thread_support(void)
{
  csprof_init_pthread_key();
  csprof_set_thread0_data();
  csprof_threaded_data();
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

  // -------------------------------------------------------
  // Capture new thread's creation context.
  // -------------------------------------------------------
  csprof_state_t* state = csprof_get_state();

  csprof_handling_synchronous_sample(1); 

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
  n = csprof_sample_event(&context, metric_id, 0 /* metric_units_consumed */);

  // tallent: only drop one to account for inlining.
  if (n) { n = n->parent; }
    // drop two innermost levels of context
    //    csprof_thread_pre_create -> monitor_thread_pre_create
    //for(int i = 0; i < 2 ; i++) { }

  TMSG(THREAD,"before lush malloc");
  TMSG(MALLOC," -thread_precreate: lush malloc");
  lush_cct_ctxt_t* thr_ctxt = csprof_malloc(sizeof(lush_cct_ctxt_t));
  TMSG(THREAD,"after lush malloc, thr_ctxt = %p",thr_ctxt);
  thr_ctxt->context = n;
  thr_ctxt->parent = state->csdata_ctxt;

  return thr_ctxt;

  // Enable signals (done in post_create)
}

void
csprof_thread_post_create(void *dc)
{
  csprof_handling_synchronous_sample(0); 
}

void *
csprof_thread_init(int id, lush_cct_ctxt_t* thr_ctxt)
{
  thread_data_t *td  = csprof_allocate_thread_data();
  csprof_set_thread_data(td);
  csprof_thread_data_init(id,1,0);

  csprof_state_t *state = TD_GET(state);

  state->pstate.thrid = id; // local thread id in state
  state->csdata_ctxt = thr_ctxt;

  // start sampling sources
  TMSG(INIT,"starting sampling sources");

  SAMPLE_SOURCES(gen_event_set,lush_metrics);
  SAMPLE_SOURCES(start);

  int ret = monitor_real_pthread_sigmask(SIG_UNBLOCK,&prof_sigset,NULL);
  if (ret){
    EMSG("WARNING: Thread init could not unblock SIGPROF, ret = %d",ret);
  }
  return (void *)state;
}

void
csprof_thread_fini(csprof_state_t *state)
{
  TMSG(FINI,"thread fini");
  if (csprof_initialized){
    TMSG(FINI,"thread finit stops sampling");
    SAMPLE_SOURCES(stop);
    csprof_write_profile_data(state);
  }
}
#endif

// csprof_fini_internal: 
// errors: handles all errors

void
csprof_fini_internal(void)
{
  NMSG(FINI,"process");
  int ret = monitor_real_sigprocmask(SIG_BLOCK,&prof_sigset,NULL);
  if (ret){
    EMSG("WARNING: process fini could not block SIGPROF, ret = %d",ret);
  }

  csprof_unthreaded_data();
  csprof_state_t *state = TD_GET(state);

  if (csprof_initialized) {
    NMSG(FINI,"process attempting sample shutdown");
    SAMPLE_SOURCES(stop);
    SAMPLE_SOURCES(shutdown);

    fnbounds_fini();

    csprof_write_profile_data(state);

    // shutdown LUSH agents
    if (lush_agents) {
      lush_agent_pool__fini(lush_agents);
    }

    AMSG(" %d samples, %d filtered, %d dropped (%d segvs), %ld intervals %ld suspicious",
	 samples_taken, filtered_samples, bad_unwind_count, segv_count, ui_count(), suspicious_count());

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
    TMSG(MALLOC," -new_epoch-");
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
				    (cct_metric_data_t){ .i = 0 });
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
void csprof_print_backtrace(csprof_state_t *state)
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
  int ret = CSPROF_OK, ret1, ret2;
  FILE* fs;

  /* Generate a filename */
  char fnm[CSPROF_FNM_SZ];


  int rank = monitor_mpi_comm_rank();
  if (rank < 0) rank = 0;
  files_profile_name(fnm, rank, CSPROF_FNM_SZ);

  MSG(CSPROF_MSG_DATAFILE, "CSPROF write_profile_data: Writing %s", fnm);

  /* Open file for writing; fail if the file already exists. */
  fs = hpcfile_open_for_write(fnm, /* overwrite */ 0);

  hpcfile_csprof_data_t *tmp = csprof_get_metric_data();
  NMSG(DATA_WRITE,"metric data target = %s",tmp->target);
  NMSG(DATA_WRITE,"metric data num metrics = %d",tmp->num_metrics);
  hpcfile_csprof_metric_t *tmp1 = tmp->metrics;
  for (int i=0;i<tmp->num_metrics;i++,tmp1++){
    NMSG(DATA_WRITE,"--metric %s period = %ld",tmp1->metric_name,tmp1->sample_period);
  }
  ret1 = hpcfile_csprof_write(fs, tmp);

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
    unsigned int num_ccts = 0;
    unsigned long num_tramp_samps = 0;

    /* count states */
    while(runner != NULL) {
      if(runner->epoch != NULL) {
	num_ccts++;
	num_tramp_samps += runner->trampoline_samples;
      }
      runner = runner->next;
    }

    hpc_fwrite_le4(&num_ccts, fs);
    hpc_fwrite_le8(&num_tramp_samps, fs);

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

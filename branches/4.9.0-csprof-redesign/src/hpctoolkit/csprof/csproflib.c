// -*-Mode: C;-*-
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
//    csproflib.c
//
// Purpose:
//    [The purpose of this file]
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

#include <sys/stat.h>

#include <sys/types.h>
#include <signal.h>
#include <sys/signal.h>         /* sigaction(), sigemptyset() */
#include <sys/resource.h>
#include <errno.h>
#include <ucontext.h>           /* struct ucontext */
#include <unistd.h>

/* user include files */

#include "xpthread.h"
#include "general.h"
#include "atomic.h"
#include "libc.h"
#include "csproflib.h"
#include "csproflib_private.h"
#include "env.h"
#include "killsafe.h"
#include "mem.h"
#include "csprof_csdata.h"
#include "interface.h"
#include "util.h"
#include "driver.h"
#include "epoch.h"
#include "metrics.h"
#include "dump_backtraces.h"

#include <lush/lush.h>

#include "hpcfile_csproflib.h"


/* forward declarations and definitions of all kinds */

/* the library's basic state */
csprof_status_t status = CSPROF_STATUS_UNINIT;
static csprof_options_t opts;

//static void csprof_print_state(csprof_state_t *, CONTEXT *);

#if !defined(CSPROF_SYNCHRONOUS_PROFILING) 
sigset_t prof_sigset;
#endif


/* initialization and finalization of the library */

#ifdef NOMON
#ifdef __GNUC__
void csprof_init_internal() __attribute__((constructor));
void csprof_fini_internal() __attribute__((destructor));
#endif

#ifndef __GNUC__
/* Implicit interface */
void
_init()
{
  csprof_init_internal();
}

void
_fini()
{
  csprof_fini_internal();
}
#endif
#endif /* NOMON */
/* Explicit interface */
void
csprof_init()
{
  csprof_init_internal();
}

void
csprof_fini()
{
  csprof_fini_internal(); 
}

//***************************************************************************
// Initialize and Finalize this library
//***************************************************************************

/* FIXME: this whole implicit/explicit interface thing has gotten hosed
   over the transition to alpha/multiple threads/trampoline.  go back
   and examine the issues involved */

// Difficulties related to initializing/finalizing the library:
//
//   Case 1: Implicit interface: It is important to note that when
//   this library is loaded with LD_PRELOAD, _init may be called
//   multiple times per process.  A common example of this is when an
//   application program is called through a shell's 'exec'.  The
//   library is initialized when the shell interpreter is loaded and
//   then again when the application is loaded.  Note that the library
//   loads with *completely fresh* state each time -- exec replaces
//   the current process *without calling library finalizers*.  We
//   need to support profiling accross such indirect invocation where
//   1) library state is lost if only saved in memory and 2) hw
//   performance counters continue to run.
//
//   Case 2: Explicit interface: If the explicit interface is used,
//   the implicit interface will *also* be called as this library
//   loads and unloads.  In this case, since the library is not loaded
//   multiple times, memory state is preserved.  We need to support
//   multiple calls to the initializer and finalizer where state is
//   preserved.

volatile int CSPROF_WAIT = 1;

extern void _start(void);
extern int __stop___libc_freeres_ptrs;

void *static_epoch_offset;
char *static_epoch_xname;
void *static_epoch_end;
long static_epoch_size;

//
// process level setup (assumes pthreads not started yet)
//
void csprof_init_internal(void)
{
  if (getenv("CSPROF_WAIT")) {
    DBGMSG_PUB(1, "CSPROF_WAIT... (pid=%d)", getpid());
    while(CSPROF_WAIT);
  }
  csprof_libc_init();
  MSG(CSPROF_MSG_SHUTDOWN, "***> csprof init internal ***");

  csprof_options__init(&opts);
  csprof_options__getopts(&opts);

#ifdef STATIC_ONLY
  static_epoch_offset = (void *)&_start;
  static_epoch_end    = (void *)&__stop___libc_freeres_ptrs;
  static_epoch_size   = (long) (static_epoch_end - static_epoch_offset);
#endif     

  if (status == CSPROF_STATUS_INIT) { 
    MSG(CSPROF_MSG_SHUTDOWN, "!!! csprof init called again !!!");
  }
  else {
    MSG(CSPROF_MSG_SHUTDOWN, "csprof init 1");

    /* private memory store for the initial thread is done below */

    MSG(1,"calling malloc init");
    csprof_malloc_init(opts.mem_sz, 0);

  /* epoch poking needs the memory manager init'd() (and
     the thread-specific memory manager if need be) */
  /* figure out what libraries we currently have loaded */
    csprof_epoch_lock();
    csprof_epoch_new();
    csprof_epoch_unlock();
    
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

    MSG(CSPROF_MSG_SHUTDOWN, "***> csprof init 2 ***");
  }

#if !defined(CSPROF_SYNCHRONOUS_PROFILING)
  MSG(1,"sigemptyset(prof_sigset)");
  sigemptyset(&prof_sigset);
#endif

  // MSG(CSPROF_MSG_SHUTDOWN, "***> csprof init 3 ***");

  csprof_process_driver_init(&opts);

  // MSG(CSPROF_MSG_SHUTDOWN, "***> csprof init 4 ***");

  status = CSPROF_STATUS_INIT;
}

#ifdef CSPROF_THREADS
extern pthread_key_t thread_node_key;
extern pthread_key_t prof_data_key;
extern pthread_key_t mem_store_key;

void csprof_init_thread_support(int id)
{

  csprof_state_t *state = csprof_get_state();

  MSG(1,"csproflib init f initial thread");
  csprof_pthread_init_data();

  pthread_setspecific(mem_store_key,(void *)csprof_get_memstore());
  pthread_setspecific(prof_data_key,(void *)state);
  // do NOT create a list node -- monitor should make that unnecessary

  // Switch to threaded variants for various components of csprof

  state->pstate.thrid = id;

  mem_threaded();
  state_threaded();
  MSG(1,"switch to threaded versions complete");
}

void csprof_thread_init(killsafe_t *kk,int id)
{

  csprof_state_t *state;
  csprof_mem_t *memstore;

  // csprof_pthread_init_funcptrs();  // SHOULDN'T NEED THIS
  // csprof_pthread_init_data();      // SHOULDN'T NEED THIS, called by init_support

  // csprof_pthread_state_init(kk);  // SHOULDN'T NEED THIS -- follow process_init scheme
  memstore = csprof_malloc_init(1, 0);

  if(memstore == NULL) {
    DIE("Couldn't allocate mem for csprof_malloc in thread",
        __FILE__, __LINE__);
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

  pthread_setspecific(prof_data_key,(void *)state);

  kk->state = state;  // save pointer to state data in killsafe

    /* FIXME: is this the right way to do things? */

  MSG(1,"driver init f thread");
  csprof_thread_driver_init(&opts);
}

void csprof_thread_fini(csprof_state_t *state)
{
  extern void csprof_disable_timer(void);
  // int csprof_write_profile_data(csprof_state_t *s);

  MSG(1,"csprof thread fini");
  csprof_disable_timer();
  csprof_write_profile_data(state);
}
#endif


// csprof_fini_internal: 
// errors: handles all errors
void csprof_fini_internal(void)
{
    extern int segv_count;
    extern int samples_taken;
    extern int bad_unwind_count;
    csprof_state_t *state;

    /* Prevent multiple finalizations [Case 2] */
    if (status == CSPROF_STATUS_FINI) {
        MSG(CSPROF_MSG_SHUTDOWN, "***< csprof fini ***");
        return;
    }

    /* do this *before* doing everything else.  it's "technically" incorrect,
       since we're not actually "done" until all the functions below are
       called.  but this enables the signal handler to not screw with our
       state.  FIXME: maybe provide another level--CSPROF_STATUS_SHUT_DOWN? */
    status = CSPROF_STATUS_FINI;

    // stop the profile driver
    csprof_driver_fini(state, &opts);

    // write profile data
    MSG(CSPROF_MSG_SHUTDOWN, "writing profile data");
    state = csprof_get_safe_state();
    csprof_write_profile_data(state);
    printf("%d samples total, %d samples dropped\n", samples_taken, segv_count+bad_unwind_count);

    // shutdown LUSH agents
    if (state->lush_agents) {
      lush_agent_pool__fini(state->lush_agents);
    }
}


/* timing utility */
static void
csprof_timers(double *cpu, double *et)
{
    struct rusage r;
    struct timeval t;
	
    getrusage(RUSAGE_SELF, &r);
    *cpu = r.ru_utime.tv_sec + r.ru_utime.tv_usec*1.0e-6;
	
    gettimeofday(&t, (struct timezone *)0);
    *et = t.tv_sec + t.tv_usec*1.0e-6;
}


/* the C trampoline */
#ifdef CSPROF_TRAMPOLINE_BACKEND

/* the !defined(CSPROF_PERF) are intended to help reduce the code footprint
   of the trampoline.  every little bit counts */
void *
csprof_trampoline2(void **stackptr)
{
    csprof_state_t *state;
    void *return_ip;
    struct lox l;
    int should_insert;

    state = csprof_get_state();
    csprof_state_flag_set(state, CSPROF_EXC_HANDLING);
    return_ip = state->swizzle_return;

    DBGMSG_PUB(1, "%%trampoline returning to %p", return_ip);
    DBGMSG_PUB(1, "%%stackptr = %p => %p", stackptr, *stackptr);
    {
        csprof_cct_node_t *treenode = (csprof_cct_node_t *)state->treenode;

        assert(treenode != NULL);

#if defined(CSPROF_LAST_EDGE_ONLY)
        if(!csprof_state_flag_isset(state, CSPROF_THRU_TRAMP)) {
#endif
            /* count this; FIXME HACK */
            treenode->metrics[1]++;
#if defined(CSPROF_LAST_EDGE_ONLY)
        }
#endif

        csprof_bt_pop(state);
        state->treenode = treenode->parent;

        csprof_state_verify_backtrace_invariants(state);
    }

    if(return_ip != csprof_bt_top_ip(state)) {
#if !defined(CSPROF_PERF)
      csprof_frame_t *x = state->bufstk - 1;

#if 0
      for( ; x != state->bufend; ++x) {
	printf("ip %#lx | sp %#lx\n", x->ip, x->sp);
      }
#else
      dump_backtraces(state, 0);
#endif


        ERRMSG("Thought we were returning to %#lx, but %#lx",
               __FILE__, __LINE__, return_ip, csprof_bt_top_ip(state));
        ERRMSG("csprof_trampoline2 %#lx",
               __FILE__, __LINE__, csprof_trampoline2);
	csprof_dump_loaded_modules();
#endif
        DIE("Returning to an unexpected location.",
            __FILE__, __LINE__);
    }

    should_insert = csprof_find_return_address_for_function(state, &l, stackptr);

    if(should_insert) {
        if(l.stored.type == REGISTER) {
            int reg = l.stored.location.reg;

            DBGMSG_PUB(1, "%%trampoline patching register %d", reg);
            state->swizzle_patch = reg;
            state->swizzle_return =
                csprof_replace_return_address_onstack(stackptr, reg);
        }
        else {
            void **address = l.stored.location.address;

            DBGMSG_PUB(1, "%%trampoline patching address %p = %p",
		       address, *address);

            state->swizzle_patch = address;
            if(*address != CSPROF_TRAMPOLINE_ADDRESS) {
                state->swizzle_return = *address;
                *address = CSPROF_TRAMPOLINE_ADDRESS;
            }
            else {
                DIE("Trampoline not patched out at %#lx", __FILE__, __LINE__,
                    address);
            }
        }
    }
    else {
        /* clear out swizzle info */
        state->swizzle_patch = 0;
        state->swizzle_return = 0;
    }

    csprof_state_flag_set(state, CSPROF_THRU_TRAMP);
    csprof_state_flag_clear(state, CSPROF_EXC_HANDLING | CSPROF_SIGNALED_DURING_TRAMPOLINE);

    
    return return_ip;
}

void
csprof_trampoline2_end()
{
}
#endif

csprof_state_t *csprof_check_for_new_epoch(csprof_state_t *state)
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

    MSG(0xfeed/*CSPROF_MSG_EPOCH*/, "Creating new epoch...");

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
    // csprof_duplicate_thread_state_reference(newstate);
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
static void
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
    sprintf(fnm, "%s/%s%lx-%x-%ld%s", opts.out_path,
            CSPROF_FNM_PFX, state->pstate.hostid, state->pstate.pid,
            state->pstate.thrid, CSPROF_OUT_FNM_SFX);
  }
  else {
    sprintf(fnm, "%s/%s%lx-%x%s", opts.out_path, CSPROF_FNM_PFX,
            state->pstate.hostid, state->pstate.pid, CSPROF_OUT_FNM_SFX);
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
                ret2 = csprof_csdata__write_bin(&runner->csdata,
                                                runner->epoch->id, fs);
          
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


/* option handling */
/* FIXME: this needs to be split up a little bit for different backends */

int csprof_options__init(csprof_options_t* x)
{
  memset(x, 0, sizeof(*x));

  x->mem_sz = CSPROF_MEM_SZ_INIT;
  x->event = CSPROF_EVENT;
  x->sample_period = CSPROF_SMPL_PERIOD;
  
  return CSPROF_OK;
}

int csprof_options__fini(csprof_options_t* x)
{
  return CSPROF_OK;
}

/* NOTE: assumes no private 'heap' memory is available yet */
int csprof_options__getopts(csprof_options_t* x)
{

  char tmp[CSPROF_PATH_SZ];
  char* s;
  int i = 0;

#ifndef CSPROF_PERF
  /* Global option: CSPROF_OPT_VERBOSITY */
  s = getenv(CSPROF_OPT_VERBOSITY);
  if (s) {
    i = atoi(s);
    if ((0 <= i) && (i <= 65536)) {
      CSPROF_MSG_LVL = i;
      fprintf(stderr, "setting message level to %d\n",i);
    }
    else {
      DIE("value of option `%s' [%s] not integer between 0-9", __FILE__, __LINE__,
          CSPROF_OPT_VERBOSITY, s);
    }
  } 

  /* Global option: CSPROF_OPT_DEBUG */
  s = getenv(CSPROF_OPT_DEBUG);
  if (s) {
    i = atoi(s);
    /* FIXME: would like to provide letters as mnemonics, much like Perl */
    CSPROF_DBG_LVL_PUB = i;
  }
#endif

  /* Option: CSPROF_OPT_LUSH_AGENTS */
  s = getenv(CSPROF_OPT_LUSH_AGENTS);
  if (s) {
    strcpy(x->lush_agent_paths, s);
  }
  else {
    x->lush_agent_paths[0] = '\0';
  }

  /* Option: CSPROF_OPT_MAX_METRICS */
  s = getenv(CSPROF_OPT_MAX_METRICS);
  if (s) {
    i = atoi(s);
    if ((0 <= i) && (i <= 10)) {
      x->max_metrics = i;
    }
    else {
      DIE("value of option `%s' [%s] not integer between 0-10", __FILE__,
          __LINE__, CSPROF_OPT_MAX_METRICS, s);
    }
  }
  else {
    x->max_metrics = 5;
  }

  /* Option: CSPROF_OPT_SAMPLE_PERIOD */
  s = getenv(CSPROF_OPT_SAMPLE_PERIOD);
  if (s) {
    long l;
    char* s1;
    errno = 0; /* set b/c return values on error are all valid numbers! */
    l = strtol(s, &s1, 10);
    if (errno != 0 || l < 1 || *s1 != '\0') {
      DIE("value of option `%s' [%s] is an invalid decimal integer", __FILE__, __LINE__,
          CSPROF_OPT_SAMPLE_PERIOD, s);
    }
    else {
      x->sample_period = l;
    } 
  }
  else {
    x->sample_period = 5000; /* microseconds */
  }

  /* Option: CSPROF_OPT_MEM_SZ */
  s = getenv(CSPROF_OPT_MEM_SZ);
  if(s) {
    unsigned long l;
    char *s1;
    errno = 0;
    l = strtoul(s, &s1, 10);
    if(errno != 0) {
      DIE("value of option `%s' [%s] is an invalid decimal integer",
          __FILE__, __LINE__, CSPROF_OPT_MEM_SZ, s);
    }
    /* FIXME: may want to consider adding sanity checks (initial memory
       sizes that are too high or too low) */
    if(*s1 == '\0') {
      x->mem_sz = l;
    }
    /* convinience */
    else if(*s1 == 'M' || *s1 == 'm') {
      x->mem_sz = l * 1024 * 1024;
    }
    else if(*s1 == 'K' || *s1 == 'k') {
      x->mem_sz = l * 1024;
    }
    else {
      DIE("unrecognized memory size unit `%c'",
          __FILE__, __LINE__, *s1);
    }
  }
  else {
    /* provide a reasonable default */
    x->mem_sz = 2 * 1024 * 1024;
  }

  /* Option: CSPROF_OPT_OUT_PATH */
  s = getenv(CSPROF_OPT_OUT_PATH);
  if (s) {
    i = strlen(s);
    if(i==0) {
      strcpy(tmp, ".");
    }
    if((i + 1) > CSPROF_PATH_SZ) {
      DIE("value of option `%s' [%s] has a length greater than %d", __FILE__, __LINE__,
          CSPROF_OPT_OUT_PATH, s, CSPROF_PATH_SZ);
    }
    strcpy(tmp, s);
  }
  else {
    strcpy(tmp, ".");
  }
  
  if (realpath(tmp, x->out_path) == NULL) {
    DIE("could not access path `%s': %s", __FILE__, __LINE__, tmp, strerror(errno));
  }

  return CSPROF_OK;
}

// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2021, Rice University
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

#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

#ifdef LINUX
#include <linux/unistd.h>
#include <linux/limits.h>
#endif

//***************************************************************************
// libmonitor include files
//***************************************************************************

#include <monitor.h>


//***************************************************************************
// user include files 
//***************************************************************************

#include <include/uint.h>

#include <include/hpctoolkit-config.h>

#include "main.h"

#include "disabled.h"
#include "env.h"
#include "loadmap.h"
#include "files.h"
#include "fnbounds_interface.h"
#include "fnbounds_table_interface.h"
#include "hpcrun-initializers.h"
#include "hpcrun_options.h"
#include "hpcrun_return_codes.h"
#include "hpcrun_stats.h"
#include "name.h"
#include "start-stop.h"
#include "custom-init.h"
#include "cct_insert_backtrace.h"
#include "safe-sampling.h"

#include "metrics.h"

#include "sample_event.h"
#include <sample-sources/none.h>
#include <sample-sources/itimer.h>

#include "sample_sources_registered.h"
#include "sample_sources_all.h"
#include "segv_handler.h"
#include "sample_prob.h"
#include "term_handler.h"

#include "device-initializers.h"
#include "device-finalizers.h"
#include "module-ignore-map.h"
#include "epoch.h"
#include "thread_data.h"
#include "threadmgr.h"
#include "thread_finalize.h"
#include "thread_use.h"
#include "trace.h"
#include "write_data.h"
#include "sample-sources/itimer.h"
#include <utilities/token-iter.h>

#include <memory/hpcrun-malloc.h>
#include <memory/mmap.h>
#include <tool/hpcrun/sample-sources/gpu/stream-tracing.h>
#include <monitor-exts/monitor_ext.h>

#include <cct/cct.h>

#include <unwind/common/backtrace.h>
#include <unwind/common/unwind.h>

#include <utilities/arch/context-pc.h>

#include <lush/lush-backtrace.h>
#include <lush/lush-pthread.h>

#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/vdso.h>

#include <messages/messages.h>
#include <messages/debug-flag.h>

#include <loadmap.h>

// The auditor only applies to the dynamic case.
#ifndef HPCRUN_STATIC_LINK
#include "audit/audit-api.h"
#endif
extern void hpcrun_set_retain_recursion_mode(bool mode);

#ifdef HPCRUN_HAVE_CUSTOM_UNWINDER
extern void hpcrun_dump_intervals(void* addr);
#endif


//***************************************************************************
// macros
//***************************************************************************

#define MONITOR_INITIALIZE 1

#if  MONITOR_INITIALIZE == 0
#define monitor_initialize() \
    assert(0 && "entry into hpctoolkit prior to initialization");
#endif


//***************************************************************************
// local data types. Primarily for passing data between pre_PHASE, PHASE, and post_PHASE
//***************************************************************************

typedef struct local_thread_data_t {
  cct_ctxt_t* thr_ctxt;
} local_thread_data_t;

typedef struct fork_data_t {
  int flag;
  bool is_child;
} fork_data_t;


//***************************************************************************
// constants
//***************************************************************************

enum _local_const {
  PROC_NAME_LEN = 2048
};

//***************************** concrete data structure definition **********
struct hpcrun_aux_cleanup_t {
  void  (* func) (void *); // function to invoke on cleanup
  void * arg; // argument to pass to func
  struct hpcrun_aux_cleanup_t * next;
  struct hpcrun_aux_cleanup_t * prev;
};

//***************************************************************************
// forward declarations
//***************************************************************************

static int
dump_interval_handler(int sig, siginfo_t* info, void* ctxt)
__attribute__ ((unused));

//***************************************************************************
// global variables
//***************************************************************************

int lush_metrics = 0; // FIXME: global variable for now

bool hpcrun_no_unwind = false;

/******************************************************************************
 * (public declaration) thread-local variables
 *****************************************************************************/
static __thread bool hpcrun_thread_suppress_sample = true;


//***************************************************************************
// local variables 
//***************************************************************************

static hpcrun_options_t opts;
static bool hpcrun_is_initialized_private = false;
static bool hpcrun_dlopen_forced = false;
static bool safe_to_sync_sample = false;
static void* main_addr = NULL;
static void* main_lower = NULL;
static void* main_upper = (void*) (intptr_t) -1;
#ifndef HPCRUN_STATIC_LINK
static void* main_addr_dl = NULL;
static void* main_lower_dl = NULL;
static void* main_upper_dl = (void*) (intptr_t) -1;
#endif
static spinlock_t hpcrun_aux_cleanup_lock = SPINLOCK_UNLOCKED;
static hpcrun_aux_cleanup_t * hpcrun_aux_cleanup_list_head = NULL;
static hpcrun_aux_cleanup_t * hpcrun_aux_cleanup_free_list_head = NULL;
static char execname[PATH_MAX] = {'\0'};

static int monitor_fini_process_how = 0;

//***************************************************************************
// Interface functions for suppressing samples
//***************************************************************************

bool hpcrun_suppress_sample()
{
  return hpcrun_thread_suppress_sample;
}


//
// Local functions
//
static void
setup_main_bounds_check(void* main_addr)
{
  // record bound information for the symbol main statically linked 
  // into an executable, or a PLT stub named main and the function
  // to which it will be dynamically bound. these function bounds will
  // later be used to validate unwinds in the main thread by the function 
  // hpcrun_inbounds_main.

  load_module_t* lm = NULL;

  // record bound information about the function bounds of the 'main'
  // function passed into libc_start_main as real_main. this might be
  // a trampoline in the PLT.
  if (main_addr) {
#if defined(__PPC64__) || defined(HOST_CPU_IA64)
    main_addr = *((void**) main_addr);
#endif
    fnbounds_enclosing_addr(main_addr, &main_lower, &main_upper, &lm);
  }

#ifndef HPCRUN_STATIC_LINK
  // record bound information about the function bounds of a global
  // dynamic symbol named 'main' (if any).
  // passed into libc_start_main as real_main. this might be a
  // trampoline in the PLT.
  dlerror();
  main_addr_dl = dlsym(RTLD_NEXT,"main");
  if (main_addr_dl) {
    fnbounds_enclosing_addr(main_addr_dl, &main_lower_dl, &main_upper_dl, &lm);
  }
#endif
}

//
// Derive the full executable name from the
// process name. Store in a local variable.
//
static void
copy_execname(char* process_name)
{
  char tmp[PATH_MAX] = {'\0'};
  char* rpath = realpath(process_name, tmp);
  char* src = (rpath != NULL) ? rpath : process_name;

  strncpy(execname, src, sizeof(execname));
}

//
// *** Accessor functions ****
//

void
hpcrun_force_dlopen(bool forced)
{
  hpcrun_dlopen_forced = forced;
}

bool
hpcrun_is_initialized()
{
  return hpcrun_is_initialized_private;
}

void*
hpcrun_get_addr_main(void)
{
  return main_addr;
}

bool
hpcrun_inbounds_main(void* addr)
{
  // address is in a main routine statically linked into the executable
  int in_static_main = (main_lower <= addr) & (addr <= main_upper);
  int in_main = in_static_main;

#ifndef HPCRUN_STATIC_LINK
  // address is in a main routine dynamically linked into the executable
  int in_dynamic_main = (main_lower_dl <= addr) & (addr <= main_upper_dl);
  in_main |= in_dynamic_main;
#endif

  return in_main;
}

//
// fetch the execname
// note: execname has no value before main().
//
char*
hpcrun_get_execname(void)
{
  return execname;
}

//
// the char* fn argument is for debugging:
//  It has no effect in this incarnation
//
bool
hpcrun_is_safe_to_sync(const char* fn)
{
  return safe_to_sync_sample;
}

void
hpcrun_set_safe_to_sync(void)
{
  safe_to_sync_sample = true;
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

static int
abort_timeout_handler(int sig, siginfo_t* siginfo, void* context)
{
  EEMSG("hpcrun: abort timeout activated - context pc %p", 
    hpcrun_context_pc(context)); 
  monitor_real_abort();
  
  return 0; /* keep compiler happy, but can't get here */
}


static void 
hpcrun_set_abort_timeout()
{
  char *error_timeout = getenv("HPCRUN_ABORT_TIMEOUT");
  if (error_timeout) {
     int seconds = atoi(error_timeout);
     if (seconds != 0) {
       EEMSG("hpcrun: abort timeout armed");
       monitor_sigaction(SIGALRM, &abort_timeout_handler, 0, NULL);
       alarm(seconds);
     }
  }
}

//------------------------------------
// ** local routines & data to support interval dumping **
//------------------------------------

siglongjmp_fcn* hpcrun_get_real_siglongjmp(void);

#ifdef HPCRUN_HAVE_CUSTOM_UNWINDER
static sigjmp_buf ivd_jb;

static int
dump_interval_handler(int sig, siginfo_t* info, void* ctxt)
{
  (*hpcrun_get_real_siglongjmp())(ivd_jb, 9);
  return 0;
}
#endif

//------------------------------------
// process level 
//------------------------------------

void
hpcrun_init_internal(bool is_child)
{
  hpcrun_memory_reinit();
  hpcrun_mmap_init();
  hpcrun_thread_data_init(0, NULL, is_child, hpcrun_get_num_sample_sources());

  main_addr = monitor_get_addr_main();
  setup_main_bounds_check(main_addr);
  TMSG(MAIN_BOUNDS, "main addr %p ==> lower %p, upper %p", main_addr, main_lower, main_upper);

  hpcrun_options__init(&opts);
  hpcrun_options__getopts(&opts);

  hpcrun_trace_init(); // this must go after thread initialization
  hpcrun_trace_open(&(TD_GET(core_profile_trace_data)));

  // Decide whether to retain full single recursion, or collapse recursive calls to
  // first instance of recursive call
  hpcrun_set_retain_recursion_mode(hpcrun_get_env_bool("HPCRUN_RETAIN_RECURSION"));

  // Initialize logical unwinding agents (LUSH)
  if (opts.lush_agent_paths[0] != '\0') {
    epoch_t* epoch = TD_GET(core_profile_trace_data.epoch);
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

  hpcrun_setup_segv();


#ifdef HPCRUN_HAVE_CUSTOM_UNWINDER
  if (getenv("HPCRUN_ONLY_DUMP_INTERVALS")) {
    fnbounds_table_t table = fnbounds_fetch_executable_table();
    TMSG(INTERVALS_PRINT, "table data = %p", table.table);
    TMSG(INTERVALS_PRINT, "table length = %d", table.len);

    if (monitor_sigaction(SIGSEGV, &dump_interval_handler, 0, NULL)) {
      fprintf(stderr, "Could not install dump interval segv handler\n");
      monitor_real_exit(1);
    }

    for (void** e = table.table; e < table.table + table.len - 1; e++) {
      fprintf(stderr, "======== %p Intervals ========\n", *e);
      if (e > table.table || ! sigsetjmp(ivd_jb, 1))
        hpcrun_dump_intervals(*e);
      else
        fprintf(stderr, "--Error: skipped--\n");
      fprintf(stderr, "\n");
      fflush(stderr);
    }
    exit(0);
  }
#endif  // HPCRUN_HAVE_CUSTOM_UNWINDER

  hpcrun_stats_reinit();
  hpcrun_start_stop_internal_init();

  // sample source setup

  TMSG(PROCESS, "Sample source setup");
  //
  // NOTE: init step no longer necessary.
  //       -all- possible (e.g. registered) sample sources call their own init method
  //       no need to do it twice.
  //

  if (! is_child) {
    SAMPLE_SOURCES(process_event_list, lush_metrics);
    SAMPLE_SOURCES(finalize_event_list);
    hpcrun_metrics_data_finalize();
  }
  SAMPLE_SOURCES(gen_event_set, lush_metrics);

  // set up initial 'epoch'

  TMSG(EPOCH,"process init setting up initial epoch/loadmap");
  hpcrun_epoch_init(NULL);

#ifdef SPECIAL_DUMP_INTERVALS
  {
      // temporary debugging code for x86 / ppc64

      extern void hpcrun_dump_intervals(void* addr2);
      char* addr1 = getenv("ADDR1");
      char* addr2 = getenv("ADDR2");

      if (addr1 != NULL) {
        addr1 = (void*) (uintptr_t) strtol(addr1, NULL, 0);
        fprintf(stderr,"address 1 = %p\n", addr1);
        hpcrun_dump_intervals(addr1);
        fflush(NULL);
      }

      if (addr2 != NULL) {
        addr2 = (void*) (uintptr_t) strtol(addr2, NULL, 0);
        fprintf(stderr,"address 2 = %p\n", addr2);
        hpcrun_dump_intervals(addr2);
        fflush(NULL);
      }
      if (addr1 || addr2) monitor_real_exit(0);
    }
#endif

  hpcrun_initializers_apply();

  // start the sampling process

  hpcrun_enable_sampling();
  hpcrun_set_safe_to_sync();

  // release the wallclock handler -for this thread-
  hpcrun_itimer_wallclock_ok(true);

  // NOTE: hack to ensure that sample source start can be delayed until mpi_init
  if (hpctoolkit_sampling_is_active() && ! getenv("HPCRUN_MPI_ONLY")) {
    SAMPLE_SOURCES(start);
  }

  hpcrun_is_initialized_private = true;

  // FIXME: this isn't in master-gpu-trace. how is it managed?
  // stream_tracing_init();
}

#define GET_NEW_AUX_CLEANUP_NODE(node_ptr) do {                               \
if (hpcrun_aux_cleanup_free_list_head) {                                      \
node_ptr = hpcrun_aux_cleanup_free_list_head;                                 \
hpcrun_aux_cleanup_free_list_head = hpcrun_aux_cleanup_free_list_head->next;  \
} else {                                                                      \
node_ptr = (hpcrun_aux_cleanup_t *) hpcrun_malloc(sizeof(hpcrun_aux_cleanup_t));         \
}                                                                             \
} while(0)

#define ADD_TO_FREE_AUX_CLEANUP_LIST(node_ptr) do { (node_ptr)->next = hpcrun_aux_cleanup_free_list_head; \
hpcrun_aux_cleanup_free_list_head = (node_ptr); }while(0)

// Add a callback function and its argument to a doubly-linked list of things to cleanup at process termination. 
// Don't rely on sample source data in the implementation of the callback.
// Caller needs to ensure that the entry is safe.

hpcrun_aux_cleanup_t * hpcrun_process_aux_cleanup_add( void (*func) (void *), void * arg) 
{
  spinlock_lock(&hpcrun_aux_cleanup_lock); 
  hpcrun_aux_cleanup_t * node;
  GET_NEW_AUX_CLEANUP_NODE(node);
  node->func = func;
  node->arg = arg;
 
  node->prev = NULL;
  node->next = hpcrun_aux_cleanup_list_head;
  if (hpcrun_aux_cleanup_list_head) {
    hpcrun_aux_cleanup_list_head->prev = node;
  }
  hpcrun_aux_cleanup_list_head = node;
  spinlock_unlock(&hpcrun_aux_cleanup_lock); 
  return node;
}

// Delete a node from cleanup list.
// Caller needs to ensure that the entry is safe.
void hpcrun_process_aux_cleanup_remove(hpcrun_aux_cleanup_t * node)
{
  assert (node != NULL);
  spinlock_lock(&hpcrun_aux_cleanup_lock); 
  if (node->prev) {
    if (node->next) {
      node->next->prev = node->prev;
    }
    node->prev->next = node->next;
  } else {
    if (node->next) {
      node->next->prev = NULL;
    }
    hpcrun_aux_cleanup_list_head = node->next;
  }
  ADD_TO_FREE_AUX_CLEANUP_LIST(node);   
  spinlock_unlock(&hpcrun_aux_cleanup_lock); 
}

// This will be called after sample sources have been shutdown.
// Don't rely on sample source data in the implementation of the callback.
static void hpcrun_process_aux_cleanup_action()
{
  // Assumed to be single threaded and hence not taking any locks here
  hpcrun_aux_cleanup_t * p = hpcrun_aux_cleanup_list_head;
  hpcrun_aux_cleanup_t * q;
  while (p) {
    p->func(p->arg);
    q = p;
    p = p->next;
    ADD_TO_FREE_AUX_CLEANUP_LIST(q);
  }
  hpcrun_aux_cleanup_list_head = NULL;
}

/***
 * This routine is called at the end of the program to:
 *   call sample-sources to stop and shutdown 
 *   clean hpcrun action
 *   clean thread manager (write profile data and closing resources)
 *   terminate hpcfnbounds
 ***/ 
void
hpcrun_fini_internal()
{
  hpcrun_disable_sampling();

  TMSG(FINI, "process");

//  hpcrun_unthreaded_data();

  if (hpcrun_is_initialized()) {
    hpcrun_is_initialized_private = false;

    TMSG(FINI, "process attempting sample shutdown");

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

    // Call all registered auxiliary functions before termination.
    // This typically means flushing files that were not done by their creators.
    device_finalizer_apply(device_finalizer_type_flush, monitor_fini_process_how);
    device_finalizer_apply(device_finalizer_type_shutdown, monitor_fini_process_how);

    hpcrun_process_aux_cleanup_action();

    int is_process = 1;
    thread_finalize(is_process);

// FIXME: this isn't in master-gpu-trace. how is it managed?
    // stream_tracing_fini();

    // assign id tuple for main thread
    thread_data_t* td = hpcrun_get_thread_data();
    hpcrun_id_tuple_cputhread(td);

    // write all threads' profile data and close trace file
    hpcrun_threadMgr_data_fini(hpcrun_get_thread_data());

    fnbounds_fini();
    hpcrun_stats_print_summary();
    messages_fini();
  }
}


//------------------------------------
// thread level 
//------------------------------------

#ifdef USE_GCC_THREAD
extern __thread monitor_tid;
#endif // USE_GCC_THREAD

void
hpcrun_init_thread_support()
{
  hpcrun_init_pthread_key();
  hpcrun_set_thread0_data();
  hpcrun_threaded_data();
  SAMPLE_SOURCES(thread_init);
}

// DEBUG support
static void
logit(cct_node_t* n, cct_op_arg_t arg, size_t l)
{
  int iarg = (int) (intptr_t) arg;
  TMSG(THREAD_CTXT, "thr %d -- %d: lm-id: %d  lm-ip: %p",
       iarg,
       hpcrun_cct_persistent_id(n),
       hpcrun_cct_addr(n)->ip_norm.lm_id,
       hpcrun_cct_addr(n)->ip_norm.lm_ip);
}

void*
hpcrun_thread_init(int id, local_thread_data_t* local_thread_data, bool has_trace)
{
  bool demand_new_thread = false;
  cct_ctxt_t* thr_ctxt = local_thread_data ? local_thread_data->thr_ctxt : NULL;

  hpcrun_mmap_init();

  // ----------------------------------------
  // call thread manager to get a thread data. If there is unused thread data,
  //  we can recycle it, otherwise we need to allocate a new one.
  // If we allocate a new one, we need to initialize the data and trace file.
  // ----------------------------------------

  thread_data_t* td = NULL;
  hpcrun_threadMgr_data_get_safe(id, thr_ctxt, &td, has_trace, demand_new_thread);
  hpcrun_set_thread_data(td);

  td->inside_hpcrun = 1;  // safe enter, disable signals

  
  if (ENABLED(THREAD_CTXT)) {
    if (thr_ctxt) {
      hpcrun_walk_path(thr_ctxt->context, logit, (cct_op_arg_t) (intptr_t) id);
    } else {
      EMSG("Thread id %d passes null context", id);
    }
  }

  epoch_t* epoch = TD_GET(core_profile_trace_data.epoch);

  if (! hpcrun_thread_suppress_sample) {
    // handle event sets for sample sources
    SAMPLE_SOURCES(gen_event_set,lush_metrics);
    // sample sources take thread specific action prior to start (often is a 'registration' action);
    SAMPLE_SOURCES(thread_init_action);

    // start the sample sources
    SAMPLE_SOURCES(start);

    // release the wallclock handler -for this thread-
    hpcrun_itimer_wallclock_ok(true);
  }

  return (void*) epoch;
}

/**
 * Routine to handle the end of the thread:
 *   call sample sources to stop and finish the thread action
 *   notify thread manager of the end of the thread (so that it can
 *      either clean-up the data, or reuse the data for another thread)
 **/ 
void
hpcrun_thread_fini(epoch_t *epoch)
{
  TMSG(FINI,"thread fini");

  // take no action if this thread is suppressed
  if (!hpcrun_thread_suppress_sample) {
    TMSG(FINI,"thread finit stops sampling");
    SAMPLE_SOURCES(stop);
    SAMPLE_SOURCES(thread_fini_action);
    lushPthr_thread_fini(&TD_GET(pthr_metrics));

    if (hpcrun_get_disabled()) {
      return;
    }

    device_finalizer_apply(device_finalizer_type_flush, monitor_fini_process_how);

    int is_process = 0;
    thread_finalize(is_process);

    // inform thread manager that we are terminating the thread
    // thread manager may enqueue the thread_data (in compact mode)
    // or flush the data into hpcrun file
    int add_separator = 0;
    thread_data_t* td = hpcrun_get_thread_data();

    // assign id tuple for pthreads
    hpcrun_id_tuple_cputhread(td);

    hpcrun_threadMgr_data_put(epoch, td, add_separator);

    TMSG(PROCESS, "End of thread");
  }
}

//***************************************************************************
// hpcrun debugging support 
//***************************************************************************

volatile int HPCRUN_DEBUGGER_WAIT = 1;

void 
hpcrun_continue()
{
  HPCRUN_DEBUGGER_WAIT = 0;
}


void 
hpcrun_wait()
{
  bool HPCRUN_WAIT = hpcrun_get_env_bool("HPCRUN_WAIT");

  if (HPCRUN_WAIT) {
    while (HPCRUN_DEBUGGER_WAIT);

    // when the user program forks, we don't want to wait to have a debugger 
    // attached for each exec along a chain of fork/exec. if that is what
    // you want when debugging, make your own arrangements. 
    unsetenv("HPCRUN_WAIT");
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

  hpcrun_thread_suppress_sample = false;

  fork_data_t* fork_data = (fork_data_t*) data;
  bool is_child = data && fork_data->is_child;

  hpcrun_wait();

#ifndef HPCRUN_STATIC_LINK
  if(hpcrun_get_env_bool("HPCRUN_AUDIT_FAKE_AUDITOR"))
    hpcrun_init_fake_auditor();
#endif

#if 0
  // temporary patch to avoid deadlock within PAMI's optimized implementation
    // of all-to-all. a problem was observed when PAMI's optimized all-to-all
    // implementation was invoked on behalf of darshan_shutdown
    putenv("PAMID_COLLECTIVES=0");
#endif // defined(HOST_SYSTEM_IBM_BLUEGENE)

  hpcrun_sample_prob_init();

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

  copy_execname(process_name);
  hpcrun_files_set_executable(process_name);

  // We initialize the load map and fnbounds before registering sample source.
  // This is because sample source init (such as PAPI)  may dlopen other libraries,
  // which will trigger our library monitoring code and fnbound queries
  hpcrun_initLoadmap();

  // We do not want creating the measurement directory when
  // the user only wants to see the complete event list
  if (getenv("HPCRUN_LIST_EVENT")) {
    hpcrun_set_disabled();
  }
  // We need to initialize messages related functions and set up measurement directory,
  // so that we can write vdso and prevent fnbounds print messages to the terminal.
  messages_init();
  if (!hpcrun_get_disabled()) {
    hpcrun_files_set_directory();
    messages_logfile_create();

    // must initialize unwind recipe map before initializing fnbounds
    // because mapping of load modules affects the recipe map.
    hpcrun_unw_init();

    // We need to save vdso before initializing fnbounds this
    // is because fnbounds_init will iterate over the load map
    // and will invoke analysis on vdso
    hpcrun_save_vdso();

    // init callbacks for each device //Module_ignore_map is here
    hpcrun_initializer_init();

    // fnbounds must be after module_ignore_map
    fnbounds_init();
#ifndef HPCRUN_STATIC_LINK
    auditor_exports->mainlib_connected(get_saved_vdso_path());
#endif
  }

  hpcrun_registered_sources_init();

  hpcrun_do_custom_init();


  // for debugging, limit the life of the execution with an alarm.
  char* life  = getenv("HPCRUN_LIFETIME");
  if (life != NULL){
    int seconds = atoi(life);
    if (seconds > 0) alarm((unsigned int) seconds);
  }

  // see if unwinding has been turned off
  // the same setting governs whether or not fnbounds is needed or used.
  hpcrun_no_unwind = hpcrun_get_env_bool("HPCRUN_NO_UNWIND");

  char* s = getenv(HPCRUN_EVENT_LIST);

  if (! is_child) {
    hpcrun_sample_sources_from_eventlist(s);
  }

  hpcrun_set_abort_timeout();

  hpcrun_process_sample_source_none();

  TMSG(PROCESS,"hpcrun_files_set_executable called w process name = %s", process_name);

  TMSG(PROCESS,"init");


  hpcrun_sample_prob_mesg();

  TMSG(PROCESS, "I am a %s process", is_child ? "child" : "parent");

  hpcrun_init_internal(is_child);

  if (ENABLED(TST)){
    EEMSG("TST debug ctl is active!");
    STDERR_MSG("Std Err message appears");
  }


  hpcrun_safe_exit();

  return data;
}


void
monitor_fini_process(int how, void* data)
{
  if (hpcrun_get_disabled()) {
    return;
  }

  hpcrun_safe_enter();

  monitor_fini_process_how = how;

  hpcrun_fini_internal();

  hpcrun_safe_exit();
}

void
monitor_begin_process_exit(int how)
{
//TODO:Check with John if we should delete this or adjust hpcrun_fini_internal
#if 0
  if (hpcrun_get_disabled()) {
    return;
  }


  hpcrun_safe_enter();

  if (hpcrun_is_initialized()) {
    TMSG(FINI, "process attempting sample shutdown");

    SAMPLE_SOURCES(stop);
    SAMPLE_SOURCES(shutdown);

    // Call all registered auxiliary functions before termination.
    // This typically means flushing files that were not done by their creators.
    device_finalizer_apply(device_finalizer_type_flush);
    device_finalizer_apply(device_finalizer_type_shutdown);
  }


  hpcrun_safe_exit();
#endif
}

static fork_data_t from_fork;

void*
monitor_pre_fork(void)
{
  if (!hpcrun_is_initialized()) {
    monitor_initialize();
  }

  hpcrun_safe_enter();

  TMSG(PRE_FORK,"pre_fork call");

  if (SAMPLE_SOURCES(started)) {
    TMSG(PRE_FORK,"sources shutdown");
    SAMPLE_SOURCES(stop);
    SAMPLE_SOURCES(shutdown);
  }

  TMSG(PRE_FORK,"finished pre_fork call");
  from_fork.is_child = true;

  hpcrun_safe_exit();

  return (void *)(&from_fork);
}


void
monitor_post_fork(pid_t child, void* data)
{
  if (! hpcrun_is_initialized()) {
    return;
  }
  hpcrun_safe_enter();

  TMSG(POST_FORK,"Post fork call");

  if (!SAMPLE_SOURCES(started)){
    TMSG(POST_FORK,"sample sources re-init+re-start");
    SAMPLE_SOURCES(init);
    SAMPLE_SOURCES(gen_event_set,0); // FIXME: pass lush_metrics here somehow
    SAMPLE_SOURCES(start);
  }

  TMSG(POST_FORK,"Finished post fork");
  hpcrun_safe_exit();
}


//***************************************************************************
// MPI control (via libmonitor)
//***************************************************************************

//
// On some systems, taking a signal inside MPI_Init breaks MPI_Init.
// So, turn off sampling (not just block) within MPI_Init, with the
// control variable MPI_RISKY to bypass this.  This is a problem on
// IBM BlueGene and Cray XK6 (interlagos).
//
void
monitor_mpi_pre_init(void)
{
  if (!hpcrun_is_initialized()) {
    monitor_initialize();
  }

  hpcrun_safe_enter();

  TMSG(MPI, "Pre MPI_Init");
  if (! ENABLED(MPI_RISKY)) {
    // Turn sampling off.
    TMSG(MPI, "Stopping Sample Sources");
    SAMPLE_SOURCES(stop);
  }
  hpcrun_safe_exit();
}


void
monitor_init_mpi(int *argc, char ***argv)
{
  hpcrun_safe_enter();

  TMSG(MPI, "Post MPI_Init");
  if (! ENABLED(MPI_RISKY)) {
    // Turn sampling back on.
    TMSG(MPI, "Restart Sample Sources");
    SAMPLE_SOURCES(start);
  }
  hpcrun_safe_exit();
}


//***************************************************************************
// thread control (via libmonitor)
//***************************************************************************

void
monitor_init_thread_support(void)
{
  hpcrun_safe_enter();

  TMSG(THREAD,"REALLY init_thread_support ---");
  hpcrun_init_thread_support();
  hpcrun_set_using_threads(1);
  TMSG(THREAD,"Init thread support done");

  hpcrun_safe_exit();
}

void*
monitor_thread_pre_create(void)
{
  // N.B.: monitor_thread_pre_create() can be called before
  // monitor_init_thread_support() or even monitor_init_process().
  if (! hpcrun_is_initialized() || hpcrun_get_disabled()) {
    return NULL;
  }

  struct monitor_thread_info mti;
  monitor_get_new_thread_info(&mti);
  void *thread_pre_create_address = mti.mti_create_return_addr;

  if (module_ignore_map_inrange_lookup(thread_pre_create_address)) {
    return NULL;
  }
  
  hpcrun_safe_enter();
  local_thread_data_t* rv = hpcrun_malloc(sizeof(local_thread_data_t));

  // INVARIANTS at this point:
  //   1. init-process has occurred.
  //   2. current execution context is either the spawning process or thread.
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
  
  cct_node_t* n = hpcrun_gen_thread_ctxt(&context);

  TMSG(THREAD,"before lush malloc");
  TMSG(MALLOC," -thread_precreate: lush malloc");
  epoch_t* epoch = hpcrun_get_thread_epoch();
  thr_ctxt = hpcrun_malloc(sizeof(cct_ctxt_t));
  TMSG(THREAD,"after lush malloc, thr_ctxt = %p",thr_ctxt);
  thr_ctxt->context = n;
  thr_ctxt->parent = epoch->csdata_ctxt;
  TMSG(THREAD_CTXT, "context = %d, parent = %d", hpcrun_cct_persistent_id(thr_ctxt->context),
       thr_ctxt->parent ? hpcrun_cct_persistent_id(thr_ctxt->parent->context) : -1);
  rv->thr_ctxt = thr_ctxt;

 fini:

  TMSG(THREAD,"->finish pre create");
  hpcrun_safe_exit();

  return rv;
}


void
monitor_thread_post_create(void* data)
{
  if (! hpcrun_is_initialized()) {
    return;
  }
  hpcrun_safe_enter();

  TMSG(THREAD,"post create");
  TMSG(THREAD,"done post create");

  hpcrun_safe_exit();
}

void* 
monitor_init_thread(int tid, void* data)
{
#ifdef USE_GCC_THREAD
  monitor_tid = tid;
#endif

  hpcrun_thread_suppress_sample = false;
  //
  // Do nothing if ignoring thread
  //
  Token_iterate(tok, getenv("HPCRUN_IGNORE_THREAD"), " ,",
    {
      if (atoi(tok) == tid) {
        hpcrun_thread_suppress_sample = true;
      }
    });

  void *thread_begin_address = monitor_get_addr_thread_start();

  if (module_ignore_map_inrange_lookup(thread_begin_address)) {
    hpcrun_thread_suppress_sample = true;
  }

  hpcrun_safe_enter();

  TMSG(THREAD,"init thread %d",tid);
  void* thread_data = hpcrun_thread_init(tid, (local_thread_data_t*) data, ! hpcrun_thread_suppress_sample);
  TMSG(THREAD,"back from init thread %d",tid);

  hpcrun_threadmgr_thread_new();

  hpcrun_safe_exit();

  return thread_data;
}


void
monitor_fini_thread(void* init_thread_data)
{
  hpcrun_threadmgr_thread_delete();

  if (hpcrun_get_disabled()) {
    return;
  }

  hpcrun_safe_enter();

  epoch_t *epoch = (epoch_t *)init_thread_data;
  hpcrun_thread_fini(epoch);
  hpcrun_safe_exit();
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

// FIXME: Comment-out the overrides of longjmp() and siglongjmp() for
// now.  We currently don't use them and _FORTIFY_SOURCE in newer gnu
// libc breaks this code.
//
// Before re-enabling, we need to better understand how gnu libc and
// <bits/setjmp2.h> map longjmp() and siglongjmp() to __longjmp_chk()
// and what is the right way to intercept them.  Also, find a way
// around the 3-1 name mapping.
//
// Note: be sure to reset 'monitor_wrap_names' in hpclink.

#if 1

static siglongjmp_fcn *real_siglongjmp = NULL;

siglongjmp_fcn*
hpcrun_get_real_siglongjmp(void)
{
  return real_siglongjmp;
}

void
hpcrun_set_real_siglongjmp(void)
{
  MONITOR_EXT_GET_NAME(real_siglongjmp, siglongjmp);
}

#else

typedef void longjmp_fcn(jmp_buf, int);

#ifdef HPCRUN_STATIC_LINK
extern longjmp_fcn    __real_longjmp;
extern siglongjmp_fcn __real_siglongjmp;
#endif

static longjmp_fcn    *real_longjmp = NULL;
static siglongjmp_fcn *real_siglongjmp = NULL;


siglongjmp_fcn*
hpcrun_get_real_siglongjmp(void)
{
  return real_siglongjmp;
}

void
hpcrun_set_real_siglongjmp(void)
{
  MONITOR_EXT_GET_NAME_WRAP(real_siglongjmp, siglongjmp);
}

void
MONITOR_EXT_WRAP_NAME(longjmp)(jmp_buf buf, int val)
{
  hpcrun_safe_enter();
  MONITOR_EXT_GET_NAME_WRAP(real_longjmp, longjmp);

  hpcrun_safe_exit();
  (*real_longjmp)(buf, val);

  // Never reached, but silence a compiler warning.
  EEMSG("return from real longjmp(), should never get here");
  _exit(1);
}


void
MONITOR_EXT_WRAP_NAME(siglongjmp)(sigjmp_buf buf, int val)
{
  hpcrun_safe_enter();
  hpcrun_get_real_siglongjmp();

  hpcrun_safe_exit();
  (*real_siglongjmp)(buf, val);

  // Never reached, but silence a compiler warning.
  EEMSG("return from real siglongjmp(), should never get here");
  _exit(1);
}
#endif

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

static void auditor_open(auditor_map_entry_t* entry) {
  hpcrun_safe_enter();
  entry->load_module = fnbounds_map_dso(entry->path,
    entry->start, entry->end, &entry->dl_info);
  hpcrun_safe_exit();
}

static void auditor_close(auditor_map_entry_t* entry) {
  hpcrun_safe_enter();
  hpcrun_loadmap_unmap(entry->load_module);
  hpcrun_safe_exit();
}

static void auditor_stable(bool additive) {
  if(!hpcrun_td_avail()) return;
  hpcrun_safe_enter();
  if(additive) fnbounds_fini();
  hpcrun_safe_exit();
}

static void auditor_init() {
  if(!hpcrun_is_initialized())
    monitor_initialize();
}

const auditor_exports_t* auditor_exports;
void hpcrun_auditor_attach(const auditor_exports_t* exports, auditor_hooks_t* hooks) {
  auditor_exports = exports;
  hooks->initialize = auditor_init;
  hooks->open = auditor_open;
  hooks->close = auditor_close;
  hooks->stable = auditor_stable;
}

#endif /* ! HPCRUN_STATIC_LINK */


//----------------------------------------------------------------------

// FIXME: Add a weak symbol for cplus_demangle() for hpclink in the
// static case.  Something is pulling in hpctoolkit_demangle() and
// thus cplus_demangle() into libhpcrun.o and this breaks hpclink,
// even though nothing actually uses them.  But the real fix should be
// in the lib Makefiles.

#ifdef HPCRUN_STATIC_LINK

char * __attribute__ ((weak))
cplus_demangle(char *str, int opts)
{
  return strdup(str);
}

#endif


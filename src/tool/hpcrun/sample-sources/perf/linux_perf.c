// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
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

//
// Linux perf sample source interface
//


/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/syscall.h> 
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

/******************************************************************************
 * linux specific headers
 *****************************************************************************/
#include <linux/perf_event.h>
#include <linux/version.h>


/******************************************************************************
 * libmonitor
 *****************************************************************************/
#include <monitor.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include "sample-sources/simple_oo.h"
#include "sample-sources/sample_source_obj.h"
#include "sample-sources/common.h"

#include <hpcrun/cct_insert_backtrace.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/loadmap.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/metrics.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/sample-sources/blame-shift/blame-shift.h>
#include <hpcrun/utilities/tokenize.h>
#include <hpcrun/utilities/arch/context-pc.h>

#include <evlist.h>

#include <lib/prof-lean/hpcrun-metric.h> // prefix for metric helper

#include <include/linux_info.h> 

#ifdef ENABLE_PERFMON
#include "perfmon-util.h"
#endif

#include "perf-util.h"    // u64, u32 and perf_mmap_data_t
#include "perf_mmap.h"

//******************************************************************************
// macros
//******************************************************************************

#define LINUX_PERF_DEBUG 0

// default the number of samples per second
// linux perf tool has default of 4000. It looks very high but
// visually the overhead is still small
#define DEFAULT_THRESHOLD  4000

#ifndef sigev_notify_thread_id
#define sigev_notify_thread_id  _sigev_un._tid
#endif

#define THREAD_SELF     0
#define CPU_ANY        -1
#define GROUP_FD       -1
#define PERF_FLAGS      0
#define PERF_REQUEST_0_SKID      2
#define PERF_WAKEUP_EACH_SAMPLE  1

#define EXCLUDE_CALLCHAIN_USER   1


#define PERF_SIGNAL SIGIO

#define PERF_EVENT_AVAILABLE_UNKNOWN 0
#define PERF_EVENT_AVAILABLE_NO      1
#define PERF_EVENT_AVAILABLE_YES     2

#define RAW_NONE        0
#define RAW_IBS_FETCH   1
#define RAW_IBS_OP      2

// -----------------------------------------------------
// Predefined events
// -----------------------------------------------------

#define EVNAME_KERNEL_BLOCK     "KERNEL_BLOCK"
#define EVNAME_CONTEXT_SWITCHES "CS"


// -----------------------------------------------------
// precise ip / skid options
// -----------------------------------------------------

#define HPCRUN_OPTION_PRECISE_IP "HPCRUN_PRECISE_IP"

// default option for precise_ip: autodetect skid
#define PERF_EVENT_AUTODETECT_SKID    4

// constants of precise_ip (see the man page)
#define PERF_EVENT_SKID_ZERO_REQUIRED    3
#define PERF_EVENT_SKID_ZERO_REQUESTED   2
#define PERF_EVENT_SKID_CONSTANT         1
#define PERF_EVENT_SKID_ARBITRARY        0

#define PATH_KERNEL_KPTR_RESTICT    "/proc/sys/kernel/kptr_restrict"
#define PATH_KERNEL_PERF_PARANOID   "/proc/sys/kernel/perf_event_paranoid"

//******************************************************************************
// type declarations
//******************************************************************************


typedef struct perf_event_callchain_s {
  u64 nr;        /* number of IPs */ 
  u64 ips[];     /* vector of IPs */
} pe_callchain_t;


/******************************************************************************
 * external thread-local variables
 *****************************************************************************/




//******************************************************************************
// forward declarations 
//******************************************************************************

static void 
restart_perf_event(int fd);

static bool 
perf_thread_init(event_info_t *event, event_thread_t *et);

static void 
perf_thread_fini(int nevents);

static cct_node_t *
perf_add_kernel_callchain(
  cct_node_t *leaf, void *data
);

static int perf_event_handler(
  int sig, 
  siginfo_t* siginfo, 
  void* context
);

static void
kernel_block_handler( event_thread_t *current_event, sample_val_t sv,
    perf_mmap_data_t mmap_data);


//******************************************************************************
// constants
//******************************************************************************

// ordered in increasing precision
const int perf_skid_precision[] = {
  PERF_EVENT_SKID_ARBITRARY,
  PERF_EVENT_SKID_CONSTANT,
  PERF_EVENT_SKID_ZERO_REQUESTED,
  PERF_EVENT_SKID_ZERO_REQUIRED
};

const int perf_skid_flavors = sizeof(perf_skid_precision)/sizeof(int);


#ifndef ENABLE_PERFMON
static const char *event_name = "CPU_CYCLES";
#endif

static const char * dashes_separator = 
  "---------------------------------------------------------------------------\n";
static const char * equals_separator =
  "===========================================================================\n";


//******************************************************************************
// forward declaration
//******************************************************************************

static void register_blocking(event_info_t *event_desc);

//******************************************************************************
// local variables
//******************************************************************************

static event_custom_t events_predefined[] = {
		{.name         = EVNAME_KERNEL_BLOCK,
		 .register_fn  = register_blocking,   // call backs
		 .handler_fn   = kernel_block_handler, // call backs
		 .metric_index = 0,   // these fields to be defined later
		 .metric_desc  = NULL // these fields to be defined later
		}};

static uint16_t perf_kernel_lm_id;

static sigset_t sig_mask;

// Special case to make perf init a soft failure.
// Make sure that we don't use perf if it won't work.
static int perf_unavail = 0;

// Possible value of precise ip:
//  0  SAMPLE_IP can have arbitrary skid.
//  1  SAMPLE_IP must have constant skid.
//  2  SAMPLE_IP requested to have 0 skid.
//  3  SAMPLE_IP must have 0 skid.
//  4  Detect automatically to have the most precise possible (default)
static int perf_precise_ip = PERF_EVENT_AUTODETECT_SKID;

// a list of main description of events, shared between threads
// once initialize, this list doesn't change (but event description can change)
static event_info_t  *event_desc; 


//******************************************************************************
// thread local variables
//******************************************************************************

// a list of event information, private for each thread
static event_thread_t   __thread   *event_thread; 


//******************************************************************************
// private operations 
//******************************************************************************


// calling perf event open system call
static long
perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
         int cpu, int group_fd, unsigned long flags)
{
   int ret;

   ret = syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
   return ret;
}


// return the index of the predefined event if the name matches
// return -1 otherwise
static event_custom_t*
getPredefinedEventID(const char *name)
{
  int elems = sizeof(events_predefined) / sizeof(event_custom_t);
  
  for (int i=0; i<elems; i++) {
    const char  *event = events_predefined[i].name;
    if (strncmp(event, name, strlen(event)) == 0) {
      return &events_predefined[i];
    }
  }
  return NULL;
}

//----------------------------------------------------------
// stop all events
//----------------------------------------------------------
static void
perf_stop(event_thread_t event)
{
  if ( event.fd >= 0 ) {
    monitor_real_pthread_sigmask(SIG_BLOCK, &sig_mask, NULL);
    // disable the counter
    int ret = ioctl(event.fd, PERF_EVENT_IOC_DISABLE, 0);
    if (ret == -1) {
      EMSG("Warning: cannot disable counter fd %d: %s,", 
            event.fd, strerror(errno));
    }
    TMSG(LINUX_PERF, "disabled: %d", event.fd);
    ret = close(event.fd);
    if (ret == -1) {
      EMSG("Error while closing fd %d: %s", 
            event.fd, strerror(errno));
    }
    TMSG(LINUX_PERF, "closed: %d", event.fd);
  }
}


//----------------------------------------------------------
// predicates that test perf availability
//----------------------------------------------------------

static FILE*
perf_kernel_syms_avail()
{
  FILE *ksyms = fopen(LINUX_KERNEL_SYMBOL_FILE, "r");
  bool success = (ksyms != NULL);
  if (success) fclose(ksyms);
  return ksyms;
}

//----------------------------------------------------------
// Interface to see if the kernel symbol is available
// this function caches the value so that we don't need
//   enquiry the same question all the time.
//----------------------------------------------------------
static bool
is_perf_ksym_available()
{
	enum perf_ksym_e {PERF_UNDEFINED, PERF_AVAILABLE, PERF_UNAVAILABLE} ;

  // if kernel symbols are available, we will attempt to collect kernel
  // callchains and add them to our call paths
  static enum perf_ksym_e ksym_status = PERF_UNDEFINED;

  if (ksym_status == PERF_UNDEFINED) {
  	FILE *file_ksyms = perf_kernel_syms_avail();

    if (file_ksyms != NULL) {
      hpcrun_kernel_callpath_register(perf_add_kernel_callchain);
      perf_kernel_lm_id = hpcrun_loadModule_add(LINUX_KERNEL_NAME);
      ksym_status = PERF_AVAILABLE;
    } else {
    	ksym_status = PERF_UNAVAILABLE;
    }
  }
  return (ksym_status == PERF_AVAILABLE);
}

//----------------------------------------------------------
// check if perf event is available
//----------------------------------------------------------

static bool
perf_event_avail()
{
  static int checked = PERF_EVENT_AVAILABLE_UNKNOWN;
  
  switch (checked) {
  case PERF_EVENT_AVAILABLE_UNKNOWN:
    {
      struct stat buf;
      int rc = stat(PATH_KERNEL_PERF_PARANOID, &buf);
      checked = (rc == 0) ? PERF_EVENT_AVAILABLE_YES : PERF_EVENT_AVAILABLE_NO; 
    }
  case PERF_EVENT_AVAILABLE_NO:      
  case PERF_EVENT_AVAILABLE_YES:     
    break;
  }

  return (checked == PERF_EVENT_AVAILABLE_YES);     
}


//----------------------------------------------------------
// initialization
//----------------------------------------------------------

static void 
perf_init()
{
  perf_mmap_init();

  // initialize mask to block PERF_SIGNAL 
  sigemptyset(&sig_mask);
  sigaddset(&sig_mask, PERF_SIGNAL);

  monitor_sigaction(PERF_SIGNAL, &perf_event_handler, 0, NULL);
  monitor_real_pthread_sigmask(SIG_UNBLOCK, &sig_mask, NULL);
}

/*************************************
 * get the perf code and type of the given event name
 * if perfmon is enabled, we query the info from perfmon
 * otherwise we depend on the predefined events
 *************************************/
static int
perf_get_pmu_code_type(const char *name, u64 *event_code, u64* event_type)
{
#ifdef ENABLE_PERFMON
  if (!pfmu_getEventType(name, event_code, event_type)) {
     EMSG("Linux perf event not recognized: %s", name);
     return -1;
  }
  if (event_type<0) {
     EMSG("Linux perf event type not recognized: %s", name);
     return -1;
  }
  return 1;
#else
	// default pmu if we don't have perfmon
	*event_code = PERF_COUNT_HW_BUS_CYCLES;
	*event_type = PERF_TYPE_HARDWARE;

  return 0;
#endif
}



//----------------------------------------------------------
// find the best precise ip value in this platform
//----------------------------------------------------------
static u64
get_precise_ip()
{
  static int precise_ip = -1;

  if (precise_ip >= 0)
    return precise_ip;

  struct perf_event_attr attr;

  memset(&attr, 0, sizeof(attr));
  attr.config = PERF_COUNT_HW_CPU_CYCLES; // Perf's cycle event
  attr.type   = PERF_TYPE_HARDWARE;     // it's a hardware event

  // start with the most restrict skid (3) then 2, 1 and 0
  // this is specified in perf_event_open man page
  // if there's a change in the specification, we need to change 
  // this one too (unfortunately)
  for(int i=perf_skid_flavors-1; i>=0; i--) {
    attr.precise_ip = perf_skid_precision[i];

    // ask sys to "create" the event
    // it returns -1 if it fails.
    int ret = perf_event_open(&attr,
            THREAD_SELF, CPU_ANY, 
            GROUP_FD, PERF_FLAGS);
    if (ret >= 0) {
      close(ret);
      precise_ip = i;
      // just quit when the returned value is correct
      return i;
    }
  }
  precise_ip = 0;
  return precise_ip;
}

//----------------------------------------------------------
// generic initialization for event attributes
// return true if the initialization is successful,
//   false otherwise.
//----------------------------------------------------------
static int
perf_attr_init(
  u64 event_code, u64 event_type,
  struct perf_event_attr *attr,
  bool usePeriod, u64 threshold,
  u64  sampletype
)
{
  // by default, we always ask for sampling period information
  unsigned int sample_type = sampletype | PERF_SAMPLE_PERIOD;

  // for datacentric:
  // sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_ADDR | PERF_SAMPLE_CPU | PERF_SAMPLE_TID ;
  memset(attr, 0, sizeof(struct perf_event_attr));

  attr->size   = sizeof(struct perf_event_attr); /* Size of attribute structure */
  attr->type   = event_type;       /* Type of event     */
  attr->config = event_code;       /* Type-specific configuration */

  if (!usePeriod) {
    attr->freq          = 1;
    attr->sample_freq   = threshold;       /* Frequency of sampling  */
  } else {
    attr->freq          = 0;
    attr->sample_period = threshold;       /* Period of sampling     */
  }

  attr->precise_ip    = perf_precise_ip;   /* the precision is either detected automatically
                                              as precise as possible or  on the user's variable.  */
  attr->wakeup_events = PERF_WAKEUP_EACH_SAMPLE;
  attr->disabled      = 1;       /* the counter will be enabled later  */
  attr->sample_stack_user = 4096;

  if (is_perf_ksym_available()) {
    /* Records the callchain */
    attr->sample_type            = sample_type | PERF_SAMPLE_CALLCHAIN;
    attr->exclude_callchain_user = EXCLUDE_CALLCHAIN_USER;
  } else {
    attr->sample_type            = sample_type | PERF_SAMPLE_IP;
  }
  // DEBUG: this attributes are specified in perf tool
#if 0
  attr->sample_type = 39;
  attr->mmap  = 1;
  attr->mmap2 = 1;
  attr->inherit = 1;
  attr->task = 1;
  attr->comm = 1;
#endif
  return true;
}
//----------------------------------------------------------
// initialize an event
//  event_num: event number
//  name: name of event (has to be recognized by perf event)
//  threshold: sampling threshold 
//----------------------------------------------------------
static bool
perf_thread_init(event_info_t *event, event_thread_t *et)
{
  et->event = event;
  // ask sys to "create" the event
  // it returns -1 if it fails.
  et->fd = perf_event_open(&event->attr,
            THREAD_SELF, CPU_ANY, 
            GROUP_FD, PERF_FLAGS);
  TMSG(LINUX_PERF, "dbg register event %d, fd: %d, skid: %d", 
    event->id, et->fd, event->attr.precise_ip);

  // check if perf_event_open is successful
  if ( et->fd <0 ) {
    EMSG("Linux perf event open %d failed: %s", 
  event->id, strerror(errno));
    return false;
  }

  // create mmap buffer for this file 
  et->mmap = set_mmap(et->fd);

  // make sure the file I/O is asynchronous
  int flag = fcntl(et->fd, F_GETFL, 0);
  int ret  = fcntl(et->fd, F_SETFL, flag | O_ASYNC );
  if (ret == -1) {
    EMSG("Can't set notification for event %d, fd: %d: %s", 
      event->id, et->fd, strerror(errno));
  }

  // need to set PERF_SIGNAL to this file descriptor
  // to avoid POLL_HUP in the signal handler
  ret = fcntl(et->fd, F_SETSIG, PERF_SIGNAL);
  if (ret == -1) {
    EMSG("Can't set SIGIO for event %d, fd: %d: %s", 
      event->id, et->fd, strerror(errno));
  }

  // set file descriptor owner to this specific thread
  struct f_owner_ex owner;
  owner.type = F_OWNER_TID;
  owner.pid  = syscall(SYS_gettid);
  ret = fcntl(et->fd, F_SETOWN_EX, &owner);
  if (ret == -1) {
    EMSG("Can't set thread owner for event %d, fd: %d: %s", 
      event->id, et->fd, strerror(errno));
  }

  ioctl(et->fd, PERF_EVENT_IOC_RESET, 0);
  return (ret >= 0);
}


//----------------------------------------------------------
// actions when the program terminates: 
//  - unmap the memory
//  - close file descriptors used by each event
//----------------------------------------------------------
void
perf_thread_fini(int nevents)
{
  for(int i=0; i<nevents; i++) {
    if (event_thread[i].fd) close(event_thread[i].fd);
    if (event_thread[i].mmap) 
      perf_unmmap(event_thread[i].mmap);
  }
}



//----------------------------------------------------------
// extend a user-mode callchain with kernel frames (if any)
//----------------------------------------------------------
static cct_node_t *
perf_add_kernel_callchain(
  cct_node_t *leaf, void *data_aux
)
{
  cct_node_t *parent = leaf;
  
  if (data_aux == NULL)  {
    return parent;
  }
  perf_mmap_data_t *data = (perf_mmap_data_t*) data_aux;
  if (data->nr > 0) {
    // add kernel IPs to the call chain top down, which is the 
    // reverse of the order in which they appear in ips
    for (int i = data->nr - 1; i >= 0; i--) {

      uint16_t lm_id = perf_kernel_lm_id;
#if 0
      // ------------------------------------------------------------------
      // case if we want to include user call chain as well:
      // Wild assumption : if hpcrun cannot find the load module of a certain
      //   address, it is possible the address is from the kernel
      // ------------------------------------------------------------------
      void *addr = (void *) data->ips[i];
      load_module_t *lm = hpcrun_loadmap_findByAddr(addr, addr+1);
      if (lm != NULL) lm_id = lm->id;
      // ------------------------------------------------------------------
#endif
      ip_normalized_t npc = { .lm_id = lm_id, .lm_ip = data->ips[i] };
      cct_addr_t frm = { .ip_norm = npc };
      cct_node_t *child = hpcrun_cct_insert_addr(parent, &frm);
      parent = child;
    }
  }
  return parent;
}


// ---------------------------------------------
// get the index of the file descriptor
// ---------------------------------------------

static event_thread_t*
get_fd_index(int nevents, int fd)
{
  for(int i=0; i<nevents; i++) {
    if (event_thread[i].fd == fd)
      return &(event_thread[i]);
  }
  return NULL; 
}


static long
getEnvLong(const char *env_var, long default_value)
{
  const char *str_val= getenv(env_var);

  if (str_val) {
    char *end_ptr;
    long val = strtol( str_val, &end_ptr, 10 );
    if ( end_ptr != env_var && (val < LONG_MAX && val > LONG_MIN) ) {
      return val;
    }
  }
  // invalid value
  return default_value;
}

/***********************************************************************
 * Method to handle kernel blocking. Called for every context switch event
 ***********************************************************************/ 
static void
kernel_block_handler( event_thread_t *current_event, sample_val_t sv,
    perf_mmap_data_t mmap_data)
{
  if (current_event != NULL && sv.sample_node != NULL) {
    uint64_t blocking_time = hpcrun_get_blocking_time(sv.sample_node);

    if (blocking_time == 0) {
    	// we are entering the kernel: store the time
      hpcrun_set_blocking_time(sv.sample_node, mmap_data.time);

    } else if (blocking_time <= mmap_data.time) {
      // we are leaving the kernel: add the time spent in the kernel into the metric
      u64 delta = mmap_data.time - blocking_time;

      int metric_index =  current_event->event->metric_custom->metric_index;
      cct_metric_data_increment(metric_index,
                                sv.sample_node,
                               (cct_metric_data_t){.i = delta});

      // reset the time
      hpcrun_set_blocking_time(sv.sample_node, 0);
    } else {
      EMSG("Invalid blocking time on node %p: %d", sv.sample_node, blocking_time);
    }
  }
}

/***************************************************************
 * Register events to compute blocking time in the kernel
 * We use perf's sofrware context switch event to compute the 
 * time spent inside the kernel. For this, we need to sample everytime
 * a context switch occurs, and compute the time when entering the
 * kernel vs leaving the kernel. See perf_event_handler.
 * We need two metrics for this:
 * - blocking time metric to store the time spent in the kernel
 * - context switch metric to store the number of context switches
 ****************************************************************/ 
static void
register_blocking(event_info_t *event_desc)
{
  // ------------------------------------------
  // create metric to compute blocking time
  // ------------------------------------------
  event_desc->metric_custom->metric_index = hpcrun_new_metric();
  event_desc->metric_custom->metric_desc  = hpcrun_set_metric_info_and_period(
      	event_desc->metric_custom->metric_index, EVNAME_KERNEL_BLOCK,
        MetricFlags_ValFmt_Int, 1 /* period */, metric_property_none);

  // ------------------------------------------
  // create metric to store context switches
  // ------------------------------------------
  event_desc->metric      = hpcrun_new_metric();
  event_desc->metric_desc = hpcrun_set_metric_info_and_period(
      event_desc->metric, EVNAME_CONTEXT_SWITCHES,
      MetricFlags_ValFmt_Real, 1 /* period*/, metric_property_none);

  // ------------------------------------------
  // set context switch event description to be used when creating
  //  perf event of this type on each thread
  // ------------------------------------------
  /* PERF_SAMPLE_STACK_USER may also be good to use */
  u64 sample_type = PERF_SAMPLE_IP   | PERF_SAMPLE_TID       |
		    PERF_SAMPLE_TIME | PERF_SAMPLE_CALLCHAIN | 
		    PERF_SAMPLE_CPU  | PERF_SAMPLE_PERIOD;
  
  perf_attr_init(PERF_COUNT_SW_CONTEXT_SWITCHES, PERF_TYPE_SOFTWARE,
                 &(event_desc->attr),
                 true        /* use_period*/, 
		         1           /* sample every context switch*/,
                 sample_type /* need additional info for sample type */);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,3,0)
  event_desc->attr.context_switch = 1;
#endif
  event_desc->attr.sample_id_all = 1;
  // ------------------------------------------
  // additional info for perf event metric
  // ------------------------------------------
  event_desc->metric_desc->info_data.is_frequency = (event_desc->attr.freq == 1);
}

/******************************************************************************
 * method functions
 *****************************************************************************/

// --------------------------------------------------------------------------
// event occurs when the sample source is initialized
// this method is called first before others
// --------------------------------------------------------------------------
static void
METHOD_FN(init)
{
  TMSG(LINUX_PERF, "%d: init", self->sel_idx);

  perf_unavail = ( !perf_event_avail() );

  // checking the option of multiplexing:
  // the env variable is set by hpcrun or by user (case for static exec)

  perf_precise_ip = getEnvLong( HPCRUN_OPTION_PRECISE_IP, PERF_EVENT_AUTODETECT_SKID );
  if (perf_precise_ip<0 || perf_precise_ip>=PERF_EVENT_AUTODETECT_SKID ) {
    // make sure the user has the correct value of precise ip
    perf_precise_ip = get_precise_ip();   /*  requested to have 0 skid.  */
  }
  self->state = INIT;
  TMSG(LINUX_PERF, "%d: init OK", self->sel_idx);
}


// --------------------------------------------------------------------------
// when a new thread is created and has been started
// this method is called after "start"
// --------------------------------------------------------------------------
static void
METHOD_FN(thread_init)
{
  TMSG(LINUX_PERF, "%d: thread init", self->sel_idx);

  TMSG(LINUX_PERF, "%d: thread init OK", self->sel_idx);
}


// --------------------------------------------------------------------------
// start of the thread
// --------------------------------------------------------------------------
static void
METHOD_FN(thread_init_action)
{
  TMSG(LINUX_PERF, "%d: thread init action", self->sel_idx);

  TMSG(LINUX_PERF, "%d: thread init action OK", self->sel_idx);
}


// --------------------------------------------------------------------------
// start of application thread
// --------------------------------------------------------------------------
static void
METHOD_FN(start)
{
  TMSG(LINUX_PERF, "%d: start", self->sel_idx);

  if (perf_unavail) { 
    return; 
  }

  source_state_t my_state = TD_GET(ss_state)[self->sel_idx];

  // make LINUX_PERF start idempotent.  the application can turn on sampling
  // anywhere via the start-stop interface, so we can't control what
  // state LINUX_PERF is in.

  if (my_state == START) {
    TMSG(LINUX_PERF,"%d: *NOTE* LINUX_PERF start called when already in state START",
         self->sel_idx);
    return;
  }

  int nevents  = (self->evl).nevents; 
  event_thread = (event_thread_t*) hpcrun_malloc(sizeof(event_thread_t) * nevents);
  
  // setup all requested events
  // if an event cannot be initialized, we still keep it in our list
  //  but there will be no samples
  for (int i=0; i<nevents; i++) {
    // initialize this event. If it's valid, we set the metric for the event
    if (perf_thread_init( &(event_desc[i]), &(event_thread[i])) ) { 
      restart_perf_event( event_thread[i].fd );
    }
  }

  thread_data_t* td = hpcrun_get_thread_data();
  td->ss_state[self->sel_idx] = START;

  TMSG(LINUX_PERF, "%d: start OK", self->sel_idx);
}

// --------------------------------------------------------------------------
// end of thread
// --------------------------------------------------------------------------
static void
METHOD_FN(thread_fini_action)
{
  TMSG(LINUX_PERF, "%d: unregister thread", self->sel_idx);
  if (perf_unavail) { return; }

  TMSG(LINUX_PERF, "%d: unregister thread OK", self->sel_idx);
}


// --------------------------------------------------------------------------
// end of the application
// --------------------------------------------------------------------------
static void
METHOD_FN(stop)
{
  TMSG(LINUX_PERF, "%d: stop", self->sel_idx);

  if (perf_unavail) return; 

  source_state_t my_state = TD_GET(ss_state)[self->sel_idx];
  if (my_state == STOP) {
    TMSG(LINUX_PERF,"%d: *NOTE* PERF stop called when already in state STOP",
         self->sel_idx);
    return;
  }

  if (my_state != START) {
    TMSG(LINUX_PERF,"%d: *WARNING* PERF stop called when not in state START",
         self->sel_idx);
    return;
  }

  int nevents  = (self->evl).nevents; 
  for (int i=0; i<nevents; i++)
  {
    perf_stop( event_thread[i] );
  }
  thread_data_t* td = hpcrun_get_thread_data();
  td->ss_state[self->sel_idx] = STOP;

  TMSG(LINUX_PERF, "%d: stop OK", self->sel_idx);
}

// --------------------------------------------------------------------------
// really end
// --------------------------------------------------------------------------
static void
METHOD_FN(shutdown)
{
  TMSG(LINUX_PERF, "shutdown");

  if (perf_unavail) { return; }

  METHOD_CALL(self, stop); // make sure stop has been called
  // FIXME: add component shutdown code here

  int nevents = (self->evl).nevents; 
  perf_thread_fini(nevents);

#ifdef ENABLE_PERFMON
  // terminate perfmon
  pfmu_fini();
#endif

  self->state = UNINIT;
  TMSG(LINUX_PERF, "shutdown OK");
}


// --------------------------------------------------------------------------
// Return true if Linux perf recognizes the name, whether supported or not.
// We'll handle unsupported events later.
// --------------------------------------------------------------------------
static bool
METHOD_FN(supports_event, const char *ev_str)
{
  TMSG(LINUX_PERF, "supports event %s", ev_str);

#ifdef ENABLE_PERFMON
  // perfmon is smart enough to detect if pfmu has been initialized or not
  pfmu_init();
#endif

  if (perf_unavail) { return false; }

  if (self->state == UNINIT){
    METHOD_CALL(self, init);
  }

  // first, check if the event is a predefined event
  if (getPredefinedEventID(ev_str) != NULL)
    return true;

  bool result = false;
  // this is not a predefined event, we need to consult to perfmon (if enabled)
#ifdef ENABLE_PERFMON
  long thresh;
  char ev_tmp[1024];
  hpcrun_extract_ev_thresh(ev_str, sizeof(ev_tmp), ev_tmp, &thresh, DEFAULT_THRESHOLD) ;
  result = pfmu_isSupported(ev_tmp) >= 0;
#else
  result = (strncmp(event_name, ev_str, strlen(event_name)) == 0);
#endif

  return result;
}

 
// --------------------------------------------------------------------------
// handle a list of events
// --------------------------------------------------------------------------
static void
METHOD_FN(process_event_list, int lush_metrics)
{
  TMSG(LINUX_PERF, "process event list");
  if (perf_unavail) { return; }

  metric_desc_properties_t prop = metric_property_none;
  char *event;

  char *evlist = METHOD_CALL(self, get_event_str);
  int num_events = 0;

  // TODO: stupid way to count the number of events

  for (event = start_tok(evlist); more_tok(); event = next_tok()) {
    num_events++;
  }
  // manually, setup the number of events. In theory, this is to be done
  //  automatically. But in practice, it didn't. Not sure why.
  self->evl.nevents = num_events;
  
  // setup all requested events
  // if an event cannot be initialized, we still keep it in our list
  //  but there will be no samples
  size_t size = sizeof(event_info_t) * num_events;
  event_desc = (event_info_t*) hpcrun_malloc(size);
  if (event_desc == NULL) {
	  EMSG("Unable to allocate %d bytes", size);
	  return;
  }
  memset(event_desc, 0, size);

  int i=0;

  // ----------------------------------------------------------------------
  // for each perf's event, create the metric descriptor which will be used later
  // during thread initialization for perf event creation
  // ----------------------------------------------------------------------
  for (event = start_tok(evlist); more_tok(); event = next_tok(), i++) {
    char name[1024];
    long threshold = 1;

    TMSG(LINUX_PERF,"checking event spec = %s",event);

    int period_type = hpcrun_extract_ev_thresh(event, sizeof(name), name, &threshold,
       DEFAULT_THRESHOLD);

    // ------------------------------------------------------------
    // need a special case if we have our own customized  predefined  event
    // This "customized" event will use one or more perf events
    // ------------------------------------------------------------
    event_desc[i].metric_custom = getPredefinedEventID(name);

    if (event_desc[i].metric_custom != NULL) {
    	if (event_desc[i].metric_custom->register_fn != NULL) {
    	  // special registration for customized event
        event_desc[i].metric_custom->register_fn( &event_desc[i] );
        continue;
    	}
    }
    u64 event_code, event_type;
    int isPMU = perf_get_pmu_code_type(name, &event_code, &event_type);
    if (isPMU < 0)
    	// case for unknown event
    	// it is impossible to be here, unless the code is buggy
    	continue;

    bool is_period = (period_type == 1);

    // Normal perf event: initialize the event attributes
    perf_attr_init(event_code, event_type, &(event_desc[i].attr), is_period, threshold, 0);

    // ------------------------------------------------------------
    // initialize the property of the metric
    // if the metric's name has "CYCLES" it mostly a cycle metric 
    //  this assumption is not true, but it's quite closed
    // ------------------------------------------------------------

    prop = (strstr(name, "CYCLES") != NULL) ? metric_property_cycles : metric_property_none;

    char *name_dup = strdup(name); // we need to duplicate the name of the metric until the end
                                   // since the OS will free it, we don't have to do it in hpcrun
    // set the metric for this perf event
    event_desc[i].metric = hpcrun_new_metric();
   
    // ------------------------------------------------------------
    // if we use frequency (event_type=1) then the periodd is not deterministic, 
    // it can change dynamically. In this case, the period is 1
    // ------------------------------------------------------------
    if (!is_period) {
      // using frequency : the threshold is always 1, 
      //                   since the period is determine dynamically
      threshold = 1;
    }
    metric_desc_t *m = hpcrun_set_metric_info_and_period(event_desc[i].metric, name_dup,
            MetricFlags_ValFmt_Real, threshold, prop);

    if (m == NULL) {
      EMSG("Error: unable to create metric #%d: %s", index, name);
    } else {
    		m->info_data.is_frequency = (event_desc[i].attr.freq == 1);
    }
    event_desc[i].metric_desc = m;
  }

  if (num_events > 0)
    perf_init();
}


// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
static void
METHOD_FN(gen_event_set, int lush_metrics)
{
}


// --------------------------------------------------------------------------
// list events
// --------------------------------------------------------------------------
static void
METHOD_FN(display_events)
{
  if (perf_event_avail()) {
    printf(equals_separator);
    printf("Available Linux perf events\n");
    printf(equals_separator);

#ifdef ENABLE_PERFMON
    // perfmon is smart enough to detect if pfmu has been initialized or not
    pfmu_init();
    printf(dashes_separator);
    pfmu_showEventList();
    pfmu_fini();
#else
    printf("Name\t\tDescription\n");
    printf(dashes_separator);

    printf("%s\tTotal cycles.\n", 
     "PERF_COUNT_HW_CPU_CYCLES");
    printf("\n");
#endif
    printf("\n");
    if (events_predefined != NULL) {
      printf("\n%s",equals_separator);
      printf("Available customized events\n");
      printf(equals_separator);
      printf("Name\t\tDescription\n");

      int elems = sizeof(events_predefined) / sizeof(event_custom_t);

      for (int i=0; i<elems; i++) {
        const char  *event = events_predefined[i].name;
        printf("%s\n", event);
      }
      printf("\n");
    }
  }
}



// ------------------------------------------------------------
// Disable perf event
// ------------------------------------------------------------
static void
disable_perf_event(int fd)
{
#if 0
  if ( fd >= 0 ) { 
    int ret = ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    if (ret < 0) {
      TMSG(LINUX_PERF, "error fd %d in IOC_DISABLE", fd);
    }
  }
#endif
}

// ------------------------------------------------------------
// Refresh a disabled perf event
// ------------------------------------------------------------

static void 
restart_perf_event(int fd)
{
  if ( fd >= 0 ) { 
    int ret = ioctl(fd, PERF_EVENT_IOC_REFRESH, 1);
    if (ret == -1) {
      TMSG(LINUX_PERF, "error fd %d in IOC_REFRESH: %s", fd, strerror(errno));
    }
  }
}
/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name linux_perf
#define ss_cls SS_HARDWARE

#include "sample-sources/ss_obj.h"

// ---------------------------------------------
// signal handler
// ---------------------------------------------

static int
perf_event_handler(
  int sig, 
  siginfo_t* siginfo, 
  void* context
)
{
  if (siginfo->si_code < 0) {
    // signal not generated by kernel for profiling
    TMSG(LINUX_PERF, "signal si_code %d < 0 indicates not from kernel", 
         siginfo->si_code);
    return 1; // tell monitor the signal has not been handled.
  }
#if 0
  // FIXME: (laks) sometimes we have signal code other than POll_HUP
  // and still has a valid information (x86 on les). Temporarily, we 
  // don't have to guard on si_code, checking si_fd should be fine.
  if (siginfo->si_code != POLL_HUP) {
    // SIGPOLL = SIGIO, expect POLL_HUP instead of POLL_IN
    TMSG(LINUX_PERF, "signal si_code %d (fd: %d) not generated by SIGIO", 
   siginfo->si_code, siginfo->si_fd);
    return 1; // tell monitor the signal has not been handled.
  }
#endif
  // first, we need to disable the event
  int fd = siginfo->si_fd;
  disable_perf_event(fd);

  // if the interrupt came while inside our code, then drop the sample
  // and return and avoid the potential for deadlock.

  void *pc = hpcrun_context_pc(context); 

  if (! hpcrun_safe_enter_async(pc)) {
    hpcrun_stats_num_samples_blocked_async_inc();
    restart_perf_event(fd);
    return 0; // tell monitor the signal has been handled.
  }

  // ----------------------------------------------------------------------------
  // get the index of the file descriptor (if we have multiple events)
  // if the file descriptor is not on the list, we shouldn't store the 
  // metrics. Perhaps we should throw away?
  // ----------------------------------------------------------------------------

  sample_source_t *self = &obj_name();

  int nevents = self->evl.nevents;
  event_thread_t *current = get_fd_index(nevents, fd);

  // store the metric if the metric index is correct
  if ( current != NULL ) {
    // reading info from mmapped buffer
    perf_mmap_data_t mmap_data;
    memset(&mmap_data, 0, sizeof(perf_mmap_data_t));

    read_perf_buffer(current, &mmap_data);

    // ----------------------------------------------------------------------------
    // for event with frequency, we need to increase the counter by its period
    // sampling taken by perf event kernel
    // ----------------------------------------------------------------------------
    uint64_t metric_inc = 1;
    if (current->event->attr.freq==1 && mmap_data.period>0)
      metric_inc = mmap_data.period;

    // ----------------------------------------------------------------------------  
    // record time enabled and time running
    // if the time enabled is not the same as running time, then it's multiplexed
    // ----------------------------------------------------------------------------  
    u64 time_enabled = current->mmap->time_enabled;
    u64 time_running = current->mmap->time_running;

    // ----------------------------------------------------------------------------
    // the estimate count = raw_count * scale_factor
    //	            = metric_inc * time_enabled / time running
    // ----------------------------------------------------------------------------
    double scale_f = (double) time_enabled / time_running;
    double counter = scale_f * metric_inc;

    // ----------------------------------------------------------------------------
    // set additional information for the metric description
    // ----------------------------------------------------------------------------
    metric_aux_info_t *info_aux = &(current->event->metric_desc->info_data);
    // check if this event is multiplexed. we need to notify the user that a multiplexed
    //  event is not accurate at all.
    // Note: perf event can report the scale to be close to 1 (like 1.02 or 0.99).
    //       we need to use a range of value to see if it's multiplexed or not
    info_aux->is_multiplexed    |= (scale_f>1.5);

    // case of multiplexed or frequency-based sampling, we need to store the mean and
    // the standard deviation of the sampling period
    double prev_mean             = info_aux->threshold_mean;
    info_aux->num_samples++;
    info_aux->threshold_mean    += (counter-info_aux->threshold_mean) / info_aux->num_samples;
    info_aux->threshold_stdev   += (counter-info_aux->threshold_mean) * (counter-prev_mean);

    // ----------------------------------------------------------------------------
    // update the cct and add callchain if necessary
    // ----------------------------------------------------------------------------
    sample_val_t sv = hpcrun_sample_callpath(context, current->event->metric, 
		      (hpcrun_metricVal_t) {.r=counter},
          0/*skipInner*/, 0/*isSync*/, (void*) &mmap_data);

    // ----------------------------------------------------------------------------
    // if the current event is a customized one, it requires a special handling
    // ----------------------------------------------------------------------------
    if (current->event->metric_custom != NULL) {
      if (current->event->metric_custom->handler_fn != NULL) {
      		current->event->metric_custom->handler_fn(current, sv, mmap_data);
      }
    }
    //blame_shift_apply( current->event->metric, sv.sample_node, 1 /*metricIncr*/);
#if LINUX_PERF_DEBUG
    // this part is for debugging purpose
    if (current->event->attr.freq == 0) {
      if (current->event->attr.sample_period != mmap_data.period) {
        TMSG(LINUX_PERF, "period kernel is not the same: %d vs. %d", 
             current->event->attr.sample_period, mmap_data.period);
      }
    }
#endif
  } else {
    // signal not from perf event
    TMSG(LINUX_PERF, "signal si_code %d with fd %d: unknown perf event", 
       siginfo->si_code, fd);
    hpcrun_safe_exit();
    restart_perf_event(fd);
    return 1; // tell monitor the signal has not been handled.
  }

  hpcrun_safe_exit();

  // restart the event 
  restart_perf_event(fd);

  return 0; // tell monitor the signal has been handled.
}


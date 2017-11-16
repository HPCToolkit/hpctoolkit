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
// Copyright ((c)) 2002-2017, Rice University
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
#include <math.h>

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

#include "perf-util.h"        // u64, u32 and perf_mmap_data_t
#include "perf_mmap.h"        // api for parsing mmapped buffer
#include "event_custom.h"     // api for pre-defined events

#include "sample-sources/display.h" // api to display available events

#include "kernel_blocking.h"  // api for predefined kernel blocking event

//******************************************************************************
// macros
//******************************************************************************

#define LINUX_PERF_DEBUG 0

// default the number of samples per second
// linux perf tool has default of 4000. It looks very high but
// visually the overhead is still small for them.
// however, for some machines, the overhead is significant, and
//  somehow it causes the kernel to adjust the period threshold to
//  less than 100.
// 300 samples per sec looks has relatively similar percentage
// with perf tool
#define DEFAULT_THRESHOLD  300

#ifndef sigev_notify_thread_id
#define sigev_notify_thread_id  _sigev_un._tid
#endif

// replace SIGIO with SIGRTMIN to support multiple events
// We know that:
// - realtime uses SIGRTMIN+3
// - PAPI uses SIGRTMIN+2
// so SIGRTMIN+4 is a safe bet (temporarily)
#define PERF_SIGNAL (SIGRTMIN+4)

#define PERF_EVENT_AVAILABLE_UNKNOWN 0
#define PERF_EVENT_AVAILABLE_NO      1
#define PERF_EVENT_AVAILABLE_YES     2

#define RAW_NONE        0
#define RAW_IBS_FETCH   1
#define RAW_IBS_OP      2

#define PERF_MULTIPLEX_RANGE 1.2

#define PATH_KERNEL_KPTR_RESTICT    "/proc/sys/kernel/kptr_restrict"
#define PATH_KERNEL_PERF_PARANOID   "/proc/sys/kernel/perf_event_paranoid"


//******************************************************************************
// type declarations
//******************************************************************************


enum threshold_e { PERIOD, FREQUENCY };
struct event_threshold_s {
  long             threshold_num;
  enum threshold_e threshold_type;
};

//******************************************************************************
// forward declarations 
//******************************************************************************

static int
restart_perf_event(int fd);

static bool 
perf_thread_init(event_info_t *event, event_thread_t *et);

static void 
perf_thread_fini(int nevents, event_thread_t *event_thread);

static int perf_event_handler(
  int sig, 
  siginfo_t* siginfo, 
  void* context
);


//******************************************************************************
// constants
//******************************************************************************


#ifndef ENABLE_PERFMON
static const char *event_name = "CPU_CYCLES";
#endif



//******************************************************************************
// local variables
//******************************************************************************


static sigset_t sig_mask;

// a list of main description of events, shared between threads
// once initialize, this list doesn't change (but event description can change)
static event_info_t  *event_desc = NULL;

static struct event_threshold_s default_threshold = {DEFAULT_THRESHOLD, FREQUENCY};


/******************************************************************************
 * external thread-local variables
 *****************************************************************************/
extern __thread bool hpcrun_thread_suppress_sample;


//******************************************************************************
// private operations 
//******************************************************************************

//----------------------------------------------------------
// stop an event
//----------------------------------------------------------
static void
perf_stop(event_thread_t event)
{
  if ( event.fd >= 0 ) {

    // ------------------------------
    // disable the counter
    // ------------------------------
    int ret = ioctl(event.fd, PERF_EVENT_IOC_DISABLE, 0);
    if (ret == -1) {
      EMSG("Warning: cannot disable counter fd %d: %s,", 
            event.fd, strerror(errno));
    }
    TMSG(LINUX_PERF, "disabled: %d", event.fd);
  }
}

/*
 * Enable all the counters
 */ 
static void
perf_start_all(int nevents, event_thread_t *event_thread)
{
  int i;
  for(i=0; i<nevents; i++) {
    ioctl(event_thread[i].fd, PERF_EVENT_IOC_ENABLE, 0);
  }
}

/*
 * Disable all the counters
 */ 
static void
perf_stop_all(int nevents, event_thread_t *event_thread)
{
  int i;
  for(i=0; i<nevents; i++) {
    ioctl(event_thread[i].fd, PERF_EVENT_IOC_DISABLE, 0);
  }
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
            THREAD_SELF, CPU_ANY, GROUP_FD, PERF_FLAGS);
  TMSG(LINUX_PERF, "dbg register event %d, fd: %d, skid: %d, c: %d, t: %d, period: %d, freq: %d",
    event->id, et->fd, event->attr.precise_ip, event->attr.config,
	event->attr.type, event->attr.sample_freq, event->attr.freq);

  // check if perf_event_open is successful
  if (et->fd < 0) {
    EMSG("Linux perf event open %d (%d) failed: %s",
          event->id, event->attr.config, strerror(errno));
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
    EMSG("Can't set signal for event %d, fd: %d: %s",
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
static void
perf_thread_fini(int nevents, event_thread_t *event_thread)
{
  for(int i=0; i<nevents; i++) {
    if (event_thread[i].fd) 
      close(event_thread[i].fd);

    if (event_thread[i].mmap) 
      perf_unmmap(event_thread[i].mmap);
  }
}


// ---------------------------------------------
// get the index of the file descriptor
// ---------------------------------------------

static event_thread_t*
get_fd_index(int nevents, int fd, event_thread_t *event_thread)
{
  for(int i=0; i<nevents; i++) {
    if (event_thread[i].fd == fd)
      return &(event_thread[i]);
  }
  return NULL; 
}


static sample_val_t*
record_sample(event_thread_t *current, perf_mmap_data_t *mmap_data,
    void* context, sample_val_t* sv)
{
  // ----------------------------------------------------------------------------
  // for event with frequency, we need to increase the counter by its period
  // sampling taken by perf event kernel
  // ----------------------------------------------------------------------------
  uint64_t metric_inc = 1;
  if (current->event->attr.freq==1 && mmap_data->period > 0)
    metric_inc = mmap_data->period;

  // ----------------------------------------------------------------------------
  // record time enabled and time running
  // if the time enabled is not the same as running time, then it's multiplexed
  // ----------------------------------------------------------------------------
  u64 time_enabled = current->mmap->time_enabled;
  u64 time_running = current->mmap->time_running;

  // ----------------------------------------------------------------------------
  // the estimate count = raw_count * scale_factor
  //              = metric_inc * time_enabled / time running
  // ----------------------------------------------------------------------------
  double scale_f = (double) time_enabled / time_running;

  // for period-based sampling with no multiplexing, there is no need to adjust
  // the scale. Also for software event. For them, the value of time_enabled
  //  and time_running are incorrect (the ratio is less than 1 which doesn't make sense)

  if (scale_f < 1.0)
    scale_f = 1.0;

  double counter = scale_f * metric_inc;

  // ----------------------------------------------------------------------------
  // set additional information for the metric description
  // ----------------------------------------------------------------------------
  thread_data_t *td = hpcrun_get_thread_data();
  metric_aux_info_t *info_aux = &(td->core_profile_trace_data.perf_event_info[current->event->metric]);

  // check if this event is multiplexed. we need to notify the user that a multiplexed
  //  event is not accurate at all.
  // Note: perf event can report the scale to be close to 1 (like 1.02 or 0.99).
  //       we need to use a range of value to see if it's multiplexed or not
  info_aux->is_multiplexed    |= (scale_f>PERF_MULTIPLEX_RANGE);

  // case of multiplexed or frequency-based sampling, we need to store the mean and
  // the standard deviation of the sampling period
  info_aux->num_samples++;
  const double delta    = counter - info_aux->threshold_mean;
  info_aux->threshold_mean += delta / info_aux->num_samples;

  // ----------------------------------------------------------------------------
  // update the cct and add callchain if necessary
  // ----------------------------------------------------------------------------
  sampling_info_t info = {.sample_clock = 0, .sample_data = mmap_data};

  *sv = hpcrun_sample_callpath(context, current->event->metric,
        (hpcrun_metricVal_t) {.r=counter},
        0/*skipInner*/, 0/*isSync*/, &info);

  return sv;
}


/***
 * get the default event count threshold by looking from the environment variable
 * (HPCRUN_PERF_COUNT) set by hpcrun when user specifies -c option
 */
static struct event_threshold_s
init_default_count()
{
  const char *str_val= getenv("HPCRUN_PERF_COUNT");
  if (str_val == NULL) {
    return default_threshold;
  }
  int res = hpcrun_extract_threshold(str_val, &default_threshold.threshold_num, DEFAULT_THRESHOLD);
  if (res == 1)
    default_threshold.threshold_type = PERIOD;

  return default_threshold;
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

  // checking the option of multiplexing:
  // the env variable is set by hpcrun or by user (case for static exec)

  self->state = INIT;

  // init events
  kernel_blocking_init();

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

  source_state_t my_state = TD_GET(ss_state)[self->sel_idx];

  // make LINUX_PERF start idempotent.  the application can turn on sampling
  // anywhere via the start-stop interface, so we can't control what
  // state LINUX_PERF is in.

  if (my_state == START) {
    TMSG(LINUX_PERF,"%d: *NOTE* LINUX_PERF start called when already in state START",
         self->sel_idx);
    return;
  }

  int nevents = (self->evl).nevents;

  event_thread_t *event_thread = (event_thread_t *)TD_GET(ss_info)[self->sel_idx].ptr;

  for (int i=0; i<nevents; i++)
  {
    int ret = ioctl(event_thread[i].fd, PERF_EVENT_IOC_RESET, 0);
    if (ret == -1) {
      TMSG(LINUX_PERF, "error fd %d in IOC_RESET: %s", event_thread[i].fd, strerror(errno));
    }

    restart_perf_event( event_thread[i].fd );
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

  TMSG(LINUX_PERF, "%d: unregister thread OK", self->sel_idx);
}


// --------------------------------------------------------------------------
// end of the application
// --------------------------------------------------------------------------
static void
METHOD_FN(stop)
{
  TMSG(LINUX_PERF, "%d: stop", self->sel_idx);

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

  event_thread_t *event_thread = TD_GET(ss_info)[self->sel_idx].ptr;
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

  METHOD_CALL(self, stop); // make sure stop has been called
  // FIXME: add component shutdown code here

  event_thread_t *event_thread = TD_GET(ss_info)[self->sel_idx].ptr;
  int nevents = (self->evl).nevents; 

  perf_thread_fini(nevents, event_thread);

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

  if (self->state == UNINIT){
    METHOD_CALL(self, init);
  }

  // extract the event name and the threshold (unneeded in this phase)
  long thresh;
  char ev_tmp[1024];
  hpcrun_extract_ev_thresh(ev_str, sizeof(ev_tmp), ev_tmp, &thresh, DEFAULT_THRESHOLD) ;

  // check if the event is a predefined event
  if (event_custom_find(ev_tmp) != NULL)
    return true;

  // this is not a predefined event, we need to consult to perfmon (if enabled)
#ifdef ENABLE_PERFMON
  return pfmu_isSupported(ev_tmp) >= 0;
#else
  return (strncmp(event_name, ev_str, strlen(event_name)) == 0);
#endif
}


 
// --------------------------------------------------------------------------
// handle a list of events
// --------------------------------------------------------------------------
static void
METHOD_FN(process_event_list, int lush_metrics)
{
  TMSG(LINUX_PERF, "process event list");

  metric_desc_properties_t prop = metric_property_none;
  char *event;

  char *evlist = METHOD_CALL(self, get_event_str);
  int num_events = 0;

  // TODO: stupid way to count the number of events
  // manually, setup the number of events. In theory, this is to be done
  //  automatically. But in practice, it didn't. Not sure why.

  for (event = start_tok(evlist); more_tok(); event = next_tok(), num_events++);

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

  default_threshold = init_default_count();

  // ----------------------------------------------------------------------
  // for each perf's event, create the metric descriptor which will be used later
  // during thread initialization for perf event creation
  // ----------------------------------------------------------------------
  for (event = start_tok(evlist); more_tok(); event = next_tok(), i++) {
    char name[1024];
    long threshold = 1;

    TMSG(LINUX_PERF,"checking event spec = %s",event);

    int period_type = hpcrun_extract_ev_thresh(event, sizeof(name), name, &threshold,
        default_threshold.threshold_num);

    // ------------------------------------------------------------
    // need a special case if we have our own customized  predefined  event
    // This "customized" event will use one or more perf events
    // ------------------------------------------------------------
    event_desc[i].metric_custom = event_custom_find(name);

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

    // ------------------------------------------------------------
    // initialize the perf event attributes
    // all threads and file descriptor will reuse the same attributes.
    // ------------------------------------------------------------
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
    // if we use frequency (event_type=1) then the period is not deterministic,
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
      m->is_frequency_metric = (event_desc[i].attr.freq == 1);
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
  TMSG(LINUX_PERF, "gen_event_set");

  int nevents 	  = (self->evl).nevents;
  int num_metrics = hpcrun_get_num_metrics();

  // a list of event information, private for each thread
  event_thread_t  *event_thread = (event_thread_t*) hpcrun_malloc(sizeof(event_thread_t) * nevents);

  // allocate and initialize perf_event additional metric info

  size_t mem_metrics_size = num_metrics * sizeof(metric_aux_info_t);
  metric_aux_info_t* aux_info = (metric_aux_info_t*) hpcrun_malloc(mem_metrics_size);
  memset(aux_info, 0, mem_metrics_size);

  thread_data_t* td = hpcrun_get_thread_data();

  td->core_profile_trace_data.perf_event_info = aux_info;
  td->ss_info[self->sel_idx].ptr = event_thread;


  // setup all requested events
  // if an event cannot be initialized, we still keep it in our list
  //  but there will be no samples

  for (int i=0; i<nevents; i++)
  {
    // initialize this event. If it's valid, we set the metric for the event
    if (!perf_thread_init( &(event_desc[i]), &(event_thread[i])) ) {
      TMSG(LINUX_PERF, "FAIL to initialize %s", event_desc[i].metric_desc->name);
    }
  }

  TMSG(LINUX_PERF, "gen_event_set OK");
}


// --------------------------------------------------------------------------
// list events
// --------------------------------------------------------------------------
static void
METHOD_FN(display_events)
{
  event_custom_display(stdout);

  display_header(stdout, "Available Linux perf events");

#ifdef ENABLE_PERFMON
  // perfmon is smart enough to detect if pfmu has been initialized or not
  pfmu_init();
  pfmu_showEventList();
  pfmu_fini();
#else
  printf("Name\t\tDescription\n");
  display_line_single(stdout);

  printf("%s\tTotal cycles.\n",
   "PERF_COUNT_HW_CPU_CYCLES");
  printf("\n");
#endif
  printf("\n");
}


// --------------------------------------------------------------------------
// read a counter from the file descriptor,
//  and returns the value of the counter
// Note: this function is used for debugging purpose in gdb
// --------------------------------------------------------------------------
long
read_fd(int fd)
{
  char buffer[1024];
  if (fd <= 0)
    return 0;

  size_t t = read(fd, buffer, 1024);
  if (t>0) {
    return atoi(buffer);
  }
  return -1;
}


// ------------------------------------------------------------
// Refresh a disabled perf event
// returns -1 if error, non-negative is success (any returns from ioctl)
// ------------------------------------------------------------

static int
restart_perf_event(int fd)
{
  if (fd < 0) {
    TMSG(LINUX_PERF, "Unable to start event: fd is not valid");
    return -1;
  }

  int ret = ioctl(fd, PERF_EVENT_IOC_RESET, 0);
  if (ret == -1) {
    TMSG(LINUX_PERF, "error fd %d in PERF_EVENT_IOC_RESET: %s", fd, strerror(errno));
  }

  ret = ioctl(fd, PERF_EVENT_IOC_REFRESH, 1);
  if (ret == -1) {
    TMSG(LINUX_PERF, "error fd %d in IOC_REFRESH: %s", fd, strerror(errno));
  }
  return ret;
}
/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name linux_perf
#define ss_cls SS_HARDWARE
#define ss_sort_order  70

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
  // ----------------------------------------------------------------------------
  // disable all counters
  // ----------------------------------------------------------------------------
  sample_source_t *self = &obj_name();
  event_thread_t *event_thread = TD_GET(ss_info)[self->sel_idx].ptr;

  int nevents = self->evl.nevents;

  perf_stop_all(nevents, event_thread);

  // ----------------------------------------------------------------------------
  // check #0:
  // if the interrupt came while inside our code, then drop the sample
  // and return and avoid the potential for deadlock.
  // ----------------------------------------------------------------------------

  void *pc = hpcrun_context_pc(context);

  if (! hpcrun_safe_enter_async(pc)) {
    hpcrun_stats_num_samples_blocked_async_inc();
    restart_perf_event(siginfo->si_fd);
    perf_start_all(nevents, event_thread);

    return 0; // tell monitor the signal has been handled.
  }

  // ----------------------------------------------------------------------------
  // check #1: check if signal generated by kernel for profiling
  // ----------------------------------------------------------------------------

  if (siginfo->si_code < 0) {
    TMSG(LINUX_PERF, "signal si_code %d < 0 indicates not from kernel", 
         siginfo->si_code);
    perf_start_all(nevents, event_thread);

    return 1; // tell monitor the signal has not been handled.
  }

  // ----------------------------------------------------------------------------
  // check #2:
  // if sampling disabled explicitly for this thread, skip all processing
  // ----------------------------------------------------------------------------
  if (hpcrun_thread_suppress_sample) {
    return 0;
  }

  int fd = siginfo->si_fd;

  // ----------------------------------------------------------------------------
  // check #3: we expect only POLL_HUP, not POLL_IN
  // Sometimes we have signal code other than POll_HUP
  // and still has a valid information (x86 on les).
  // ----------------------------------------------------------------------------
#if 0
  if (siginfo->si_code != POLL_HUP) {
    TMSG(LINUX_PERF, "signal si_code %d (fd: %d) not generated by signal %d",
        siginfo->si_code, siginfo->si_fd, PERF_SIGNAL);

    restart_perf_event(fd);
    perf_start_all(nevents, event_thread);
    return 0; // tell monitor the signal has not been handled.
  }
#endif

  // ----------------------------------------------------------------------------
  // check #4:
  // check the index of the file descriptor (if we have multiple events)
  // if the file descriptor is not on the list, we shouldn't store the 
  // metrics. Perhaps we should throw away?
  // ----------------------------------------------------------------------------

  event_thread_t *current = get_fd_index(nevents, fd, event_thread);

  if (current == NULL) {
    // signal not from perf event
    TMSG(LINUX_PERF, "signal si_code %d with fd %d: unknown perf event",
       siginfo->si_code, fd);
    hpcrun_safe_exit();

    restart_perf_event(fd);
    perf_start_all(nevents, event_thread);

    return 1; // tell monitor the signal has not been handled.
  }

  TMSG(LINUX_PERF, "signal sampling.");
  // ----------------------------------------------------------------------------
  // parse the buffer until it finishes reading all buffers
  // ----------------------------------------------------------------------------

  int more_data = 0;
  do {
    perf_mmap_data_t mmap_data;
    memset(&mmap_data, 0, sizeof(perf_mmap_data_t));

    // reading info from mmapped buffer
    more_data = read_perf_buffer(current, &mmap_data);
    TMSG(LINUX_PERF, "record buffer: sid: %d, ip: %p, addr: %p, id: %d", mmap_data.sample_id, mmap_data.ip, mmap_data.addr, mmap_data.id);

    sample_val_t sv;
    memset(&sv, 0, sizeof(sample_val_t));

    if (mmap_data.header_type == PERF_RECORD_SAMPLE)
      record_sample(current, &mmap_data, context, &sv);

    kernel_block_handler(current, sv, &mmap_data);

  } while (more_data);

  hpcrun_safe_exit();

  restart_perf_event(fd);

  perf_start_all(nevents, event_thread);

  return 0; // tell monitor the signal has been handled.
}


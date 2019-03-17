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
// Copyright ((c)) 2002-2019, Rice University
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
#include "sample-sources/ss-errno.h"
 
#include <hpcrun/cct_insert_backtrace.h>
#include <hpcrun/files.h>
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
#include <limits.h>   // PATH_MAX
#include <lib/prof-lean/hpcrun-metric.h> // prefix for metric helper
#include <lib/support-lean/OSUtil.h>     // hostid

#include <include/linux_info.h> 

#include "perfmon-util.h"

#include "perf-util.h"        // u64, u32 and perf_mmap_data_t
#include "perf_mmap.h"        // api for parsing mmapped buffer
#include "perf_skid.h"
#include "perf_event_open.h"

#include "event_custom.h"     // api for pre-defined events

#include "sample-sources/display.h" // api to display available events

#include "kernel_blocking.h"  // api for predefined kernel blocking event

#include "lib/support-lean/compress.h"

//******************************************************************************
// macros
//******************************************************************************

#define LINUX_PERF_DEBUG 0

// default number of samples per second per thread
//
// linux perf has a default of 4000. this seems high, but the overhead for perf
// is still small.  however, for some processors (e.g., KNL), overhead 
// at such a high sampling rate is significant and as a result, the kernel
// will adjust the threshold to less than 100.
//
// 300 samples per sec with hpctoolkit has a similar overhead as perf
#define DEFAULT_THRESHOLD  HPCRUN_DEFAULT_SAMPLE_RATE

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

#define PERF_MULTIPLEX_RANGE 1.2

#define FILE_BUFFER_SIZE (1024*1024)

#define DEFAULT_COMPRESSION 5

#define PERF_FD_FINALIZED (-2)


//******************************************************************************
// type declarations
//******************************************************************************


enum threshold_e { PERIOD, FREQUENCY };
struct event_threshold_s {
  long             threshold_val;
  enum threshold_e threshold_type;
};

//******************************************************************************
// forward declarations 
//******************************************************************************

static bool 
perf_thread_init(event_info_t *event, event_thread_t *et);

static void 
perf_thread_fini(int nevents, event_thread_t *event_thread);

static int 
perf_event_handler( int sig, siginfo_t* siginfo, void* context);


//******************************************************************************
// constants
//******************************************************************************

static const struct timespec nowait = {0, 0};



//******************************************************************************
// local variables
//******************************************************************************


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


/* 
 * determine whether the perf sample source has been finalized for this thread
 */ 
static int 
perf_was_finalized
(
 int nevents, 
 event_thread_t *event_thread
)
{
  return nevents >= 1 && event_thread[0].fd == PERF_FD_FINALIZED;
}


/*
 * Enable all the counters
 */ 
static void
perf_start_all(int nevents, event_thread_t *event_thread)
{
  int i, ret;

  for(i=0; i<nevents; i++) {
    int fd = event_thread[i].fd;
    if (fd<0) 
      continue; 
 
    ret = ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);

    if (ret == -1) {
      EMSG("Can't enable event with fd: %d: %s", fd, strerror(errno));
    }
  }
}

/*
 * Disable all the counters
 */ 
static void
perf_stop_all(int nevents, event_thread_t *event_thread)
{
  int i, ret;

  for(i=0; i<nevents; i++) {
    int fd = event_thread[i].fd;
    if (fd<0) 
      continue; 
 
    ret = ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    if (ret == -1) {
      EMSG("Can't disable event with fd: %d: %s", fd, strerror(errno));
    }
  }
}

static int
perf_get_pmu_support(const char *name, struct perf_event_attr *event_attr)
{
  return pfmu_getEventAttribute(name, event_attr);
}

/****
 * copy /proc/kallsyms file into hpctoolkit output directory
 * return
 *  1 if the copy is successful
 *  0 if the target file already exists
 *  -1 if something wrong happens
 */
static int
copy_kallsyms()
{
  char *source = LINUX_KERNEL_SYMBOL_FILE;

  FILE *infile = fopen(source, "r");
  if (infile == NULL)
    return -1;

  char  dest[PATH_MAX], kernel_name[PATH_MAX];
  char  dest_directory[PATH_MAX];
  const char *output_directory = hpcrun_files_output_directory();

  snprintf(dest_directory, PATH_MAX, "%s/%s", output_directory,
           KERNEL_SYMBOLS_DIRECTORY);

  OSUtil_setCustomKernelName(kernel_name, PATH_MAX);

  // we need to keep the host-id to be exactly the same template
  // as the hpcrun file. If the filename format changes in hpcun
  //  we need to adapt again here.

  snprintf(dest, PATH_MAX, "%s/%s", dest_directory, kernel_name);

  // test if the file already exist
  struct stat st = {0};
  if (stat(dest, &st) >= 0) {
    return 0; // file already exists
  }

  mkdir(dest_directory, S_IRWXU | S_IRGRP | S_IXGRP);

  FILE *outfile = fopen(dest, "wx");

  if (outfile == NULL)
    return -1;

  compress_deflate(infile, outfile, DEFAULT_COMPRESSION);

  fclose(infile);
  fclose(outfile);

  TMSG(LINUX_PERF, "copy %s into %s", source, dest);

  return 1;
}

//----------------------------------------------------------
// initialization
//----------------------------------------------------------

static void 
perf_init()
{
  // copy /proc/kallsyms file into hpctoolkit output directory
  // only if the value of kptr_restric is zero

  if (perf_util_is_ksym_available()) {
    //copy the kernel symbol table
    int ret = copy_kallsyms();
    TMSG(LINUX_PERF, "copy_kallsyms result: %d", ret);
  }

  perf_mmap_init();

  // initialize sigset to contain PERF_SIGNAL 
  sigset_t sig_mask;
  sigemptyset(&sig_mask);
  sigaddset(&sig_mask, PERF_SIGNAL);

  // arrange to block monitor shootdown signal while in perf_event_handler
  // FIXME: this assumes that monitor's shootdown signal is SIGRTMIN+8
  struct sigaction perf_sigaction;
  sigemptyset(&perf_sigaction.sa_mask);
  sigaddset(&perf_sigaction.sa_mask, SIGRTMIN+8);
  perf_sigaction.sa_flags = 0;

  monitor_sigaction(PERF_SIGNAL, &perf_event_handler, 0, &perf_sigaction);
  monitor_real_pthread_sigmask(SIG_UNBLOCK, &sig_mask, NULL);
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
  TMSG(LINUX_PERF, "event fd: %d, skid: %d, code: %d, type: %d, period: %d, freq: %d",
        et->fd, event->attr.precise_ip, event->attr.config,
        event->attr.type, event->attr.sample_freq, event->attr.freq);

  // check if perf_event_open is successful
  if (et->fd < 0) {
    EMSG("Linux perf event open failed"
         " id: %d, fd: %d, skid: %d,"
         " config: %d, type: %d, sample_freq: %d,"
         " freq: %d, error: %s",
         event->id, et->fd, event->attr.precise_ip, 
         event->attr.config, event->attr.type, event->attr.sample_freq, 
         event->attr.freq, strerror(errno));
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

  ret = ioctl(et->fd, PERF_EVENT_IOC_RESET, 0);
  if (ret == -1) {
    EMSG("Can't reset event %d, fd: %d: %s", 
      event->id, et->fd, strerror(errno));
  }
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
  // suppress perf signal while we shut down perf monitoring
  sigset_t perf_sigset;
  sigemptyset(&perf_sigset);
  sigaddset(&perf_sigset, PERF_SIGNAL);
  monitor_real_pthread_sigmask(SIG_BLOCK, &perf_sigset, NULL);

  for(int i=0; i<nevents; i++) {
    if (!event_thread) {
       continue; // in some situations, it is possible a shutdown signal is delivered
       	         // while hpcrun is in the middle of abort.
		 // in this case, all information is null and we shouldn't
		 // start profiling.
    }
    if (event_thread[i].fd >= 0) {
      close(event_thread[i].fd);
      event_thread[i].fd = PERF_FD_FINALIZED;
    }

    if (event_thread[i].mmap) { 
      perf_unmmap(event_thread[i].mmap);
      event_thread[i].mmap = 0;
    }
  }

  // consume any pending PERF signals for this thread
  for (;;) {
    siginfo_t siginfo;
    // negative return value means no signals left pending
    if (sigtimedwait(&perf_sigset,  &siginfo, &nowait) < 0) break;
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
  if (current == NULL || current->event == NULL || current->event->metric < 0)
    return NULL;

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

  blame_shift_apply(current->event->metric, sv->sample_node, 
                    counter /*metricIncr*/);

  return sv;
}

/***
 * (1) ensure that the default rate for frequency-based sampling is below the maximum.
 * (2) if the environment variable HPCRUN_PERF_COUNT is set, use it to set the threshold
 */
static void
set_default_threshold()
{
  static int initialized = 0;

  if (!initialized) {
    int max_rate_m1 = perf_util_get_max_sample_rate() - 1;
    if (default_threshold.threshold_val > max_rate_m1) {
      default_threshold.threshold_val = max_rate_m1;
    }
    const char *val_str = getenv("HPCRUN_PERF_COUNT");
    if (val_str != NULL) {
      TMSG(LINUX_PERF, "HPCRUN_PERF_COUNT = %s", val_str);
      int res = hpcrun_extract_threshold(val_str, &default_threshold.threshold_val, max_rate_m1);
      if (res == 1) {
        default_threshold.threshold_type = PERIOD;
      }
    }
    initialized = 1;
  }
  TMSG(LINUX_PERF, "default threshold = %d", default_threshold.threshold_val);
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

  pfmu_init();

  perf_util_init();

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

  int nevents        = (self->evl).nevents;
  event_thread_t *et = (event_thread_t *)TD_GET(ss_info)[self->sel_idx].ptr;

  //  enable all perf_events
  perf_start_all(nevents, et);

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

  METHOD_CALL(self, stop); // stop the sample source 

  event_thread_t *event_thread = TD_GET(ss_info)[self->sel_idx].ptr;
  int nevents = (self->evl).nevents; 

  perf_thread_fini(nevents, event_thread);

  self->state = UNINIT;

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

  perf_stop_all(nevents, event_thread);

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

  METHOD_CALL(self, stop); // stop the sample source 

  event_thread_t *event_thread = TD_GET(ss_info)[self->sel_idx].ptr;
  int nevents = (self->evl).nevents; 

  perf_thread_fini(nevents, event_thread);

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

  if (self->state == UNINIT){
    METHOD_CALL(self, init);
  }

  // extract the event name and the threshold (unneeded in this phase)
  long thresh;
  char *ev_tmp;

  // check if the user specifies explicitly precise event
  perf_skid_parse_event(ev_str, &ev_tmp);

  hpcrun_extract_ev_thresh(ev_tmp, strlen(ev_tmp), ev_tmp, &thresh, 0) ;

  // check if the event is a predefined event
  if (event_custom_find(ev_tmp) != NULL) {
    free(ev_tmp);
    return true;
  }

  // this is not a predefined event, we need to consult to perfmon (if enabled)
  bool retval = pfmu_isSupported(ev_tmp) >= 0;
  free(ev_tmp);
  return retval;
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

  for (event = start_tok(evlist); more_tok(); event = next_tok(), num_events++);
  
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

  set_default_threshold();

  // ----------------------------------------------------------------------
  // for each perf's event, create the metric descriptor which will be used later
  // during thread initialization for perf event creation
  // ----------------------------------------------------------------------
  for (event = start_tok(evlist); more_tok(); event = next_tok(), i++) {
    char *name;
    long threshold = 1;

    TMSG(LINUX_PERF,"checking event spec = %s",event);

    perf_skid_parse_event(event, &name);
    int period_type = hpcrun_extract_ev_thresh(name, strlen(name), name, &threshold,
        default_threshold.threshold_val);

    // ------------------------------------------------------------
    // need a special case if we have our own customized  predefined  event
    // This "customized" event will use one or more perf events
    // ------------------------------------------------------------
    event_desc[i].metric_custom = event_custom_find(name);

    if (event_desc[i].metric_custom != NULL) {
      if (event_desc[i].metric_custom->register_fn != NULL) {
    	// special registration for customized event
        event_desc[i].metric_custom->register_fn( &event_desc[i] );
        METHOD_CALL(self, store_event, event_desc[i].attr.config, threshold);
        continue;
      }
    }

    struct perf_event_attr *event_attr = &(event_desc[i].attr);

    int isPMU = perf_get_pmu_support(name, event_attr);
    if (isPMU < 0)
      // case for unknown event
      // it is impossible to be here, unless the code is buggy
      continue;

    bool is_period = (period_type == 1);

    // ------------------------------------------------------------
    // initialize the generic perf event attributes for this event
    // all threads and file descriptor will reuse the same attributes.
    // ------------------------------------------------------------
    perf_util_attr_init(event, event_attr, is_period, threshold, 0);

    // ------------------------------------------------------------
    // initialize the property of the metric
    // if the metric's name has "CYCLES" it mostly a cycle metric 
    //  this assumption is not true, but it's quite closed
    // ------------------------------------------------------------

    if (strcasestr(name, "CYCLES") != NULL) {
      prop = metric_property_cycles;
      blame_shift_source_register(bs_type_cycles);
    } else {
      prop = metric_property_none;
    }

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
    METHOD_CALL(self, store_event, event_attr->config, threshold);
    free(name);
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

  // -------------------------------------------------------------------------
  // TODO: we need to fix this allocation.
  //       there is no need to allocate a memory if we are reusing thread data
  // -------------------------------------------------------------------------

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
      EEMSG("Failed to initialize the %s event.: %s", event_desc[i].metric_desc->name,
             strerror(errno));
      exit(1);
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

  pfmu_showEventList();
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



/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name linux_perf
#define ss_cls SS_HARDWARE
#define ss_sort_order  60

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
  HPCTOOLKIT_APPLICATION_ERRNO_SAVE();

  // ----------------------------------------------------------------------------
  // check #0:
  // if the interrupt came while inside our code, then drop the sample
  // and return and avoid the potential for deadlock.
  // ----------------------------------------------------------------------------

  void *pc = hpcrun_context_pc(context);

  if (! hpcrun_safe_enter_async(pc)) {
    hpcrun_stats_num_samples_blocked_async_inc();

    HPCTOOLKIT_APPLICATION_ERRNO_RESTORE();

    return 0; // tell monitor that the signal has been handled
  }

  // ----------------------------------------------------------------------------
  // disable all counters
  // ----------------------------------------------------------------------------

  sample_source_t *self = &obj_name();
  event_thread_t *event_thread = TD_GET(ss_info)[self->sel_idx].ptr;

  int nevents = self->evl.nevents;

  // if finalized already, refuse to handle any more samples
  if (perf_was_finalized(nevents, event_thread)) {
    HPCTOOLKIT_APPLICATION_ERRNO_RESTORE();

    return 0; // tell monitor that the signal has been handled
  }

  perf_stop_all(nevents, event_thread);

  // ----------------------------------------------------------------------------
  // check #1: check if signal generated by kernel for profiling
  // ----------------------------------------------------------------------------

  if (siginfo->si_code < 0) {
    TMSG(LINUX_PERF, "signal si_code %d < 0 indicates not from kernel", 
         siginfo->si_code);
    perf_start_all(nevents, event_thread);

    HPCTOOLKIT_APPLICATION_ERRNO_RESTORE();

    return 1; // tell monitor the signal has not been handled
  }

  // ----------------------------------------------------------------------------
  // check #2:
  // if sampling disabled explicitly for this thread, skip all processing
  // ----------------------------------------------------------------------------
  if (hpcrun_thread_suppress_sample) {
    HPCTOOLKIT_APPLICATION_ERRNO_RESTORE();

    return 0; // tell monitor that the signal has been handled
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

    HPCTOOLKIT_APPLICATION_ERRNO_RESTORE();

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

    perf_start_all(nevents, event_thread);

    HPCTOOLKIT_APPLICATION_ERRNO_RESTORE();

    return 1; // tell monitor the signal has not been handled
  }

  // ----------------------------------------------------------------------------
  // parse the buffer until it finishes reading all buffers
  // ----------------------------------------------------------------------------

  int more_data = 0;
  do {
    perf_mmap_data_t mmap_data;
    memset(&mmap_data, 0, sizeof(perf_mmap_data_t));

    // reading info from mmapped buffer
    more_data = read_perf_buffer(current, &mmap_data);

    sample_val_t sv;
    memset(&sv, 0, sizeof(sample_val_t));

    if (mmap_data.header_type == PERF_RECORD_SAMPLE)
      record_sample(current, &mmap_data, context, &sv);

    kernel_block_handler(current, sv, &mmap_data);

  } while (more_data);

  perf_start_all(nevents, event_thread);

  hpcrun_safe_exit();

  HPCTOOLKIT_APPLICATION_ERRNO_RESTORE();

  return 0; // tell monitor that the signal has been handled
}


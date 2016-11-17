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

#include <linux/perf_event.h>

#include <sys/syscall.h> 
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>



/******************************************************************************
 * libmonitor
 *****************************************************************************/
#include <monitor.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"

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

#include <include/linux_info.h> 

#ifdef ENABLE_PERFMON
#include "perfmon-util.h"
#endif


//******************************************************************************
// macros
//******************************************************************************

#define DEFAULT_THRESHOLD  2000000L

#ifndef sigev_notify_thread_id
#define sigev_notify_thread_id  _sigev_un._tid
#endif

#define THREAD_SELF 	 	 0
#define CPU_ANY 	        -1
#define GROUP_FD 	        -1
#define PERF_FLAGS 		 0
#define PERF_REQUEST_0_SKID 	 2
#define PERF_WAKEUP_EACH_SAMPLE  1

#define MMAP_OFFSET_0 0

#define EXCLUDE_CALLCHAIN_USER   1


#define PERF_SIGNAL SIGIO

#define PERF_DATA_PAGE_EXP        0      // use 2^PERF_DATA_PAGE_EXP pages
#define PERF_DATA_PAGES           (1 << PERF_DATA_PAGE_EXP)  

#define BUFFER_FRONT              ((char *) current_perf_mmap + pagesize)
#define BUFFER_SIZE               (tail_mask + 1)
#define BUFFER_OFFSET(tail)       ((tail) & tail_mask)

#define PERF_MMAP_SIZE(pagesz)    ((pagesz) * (PERF_DATA_PAGES + 1)) 
#define PERF_TAIL_MASK(pagesz)    (((pagesz) * PERF_DATA_PAGES) - 1) 

#define PERF_EVENT_AVAILABLE_UNKNOWN 0
#define PERF_EVENT_AVAILABLE_NO      1
#define PERF_EVENT_AVAILABLE_YES     2



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct perf_event_header pe_header_t;

typedef struct perf_event_callchain_s {
  uint64_t    nr;        /* number of IPs */ 
  uint64_t    ips[];     /* vector of IPs */
} pe_callchain_t;

typedef struct perf_event_mmap_page pe_mmap_t;



/******************************************************************************
 * external thread-local variables
 *****************************************************************************/

extern __thread bool hpcrun_thread_suppress_sample;



//******************************************************************************
// forward declarations 
//******************************************************************************

static bool 
perf_thread_init(int event_num);

static void 
perf_thread_fini(int nevents);

static cct_node_t *
perf_add_kernel_callchain(
  cct_node_t *leaf
);

static int perf_event_handler(
  int sig, 
  siginfo_t* siginfo, 
  void* context
);


//******************************************************************************
// local variables
//******************************************************************************


static uint16_t perf_kernel_lm_id;

static bool perf_ksyms_avail;

static int metric_id[MAX_EVENTS];

static struct perf_event_attr events[MAX_EVENTS];

static sigset_t sig_mask;

static int pagesize;
static size_t tail_mask;

#ifndef ENABLE_PERFMON
static const char *event_name = "PERF_COUNT_HW_CPU_CYCLES";
#endif

static const char * dashes_separator = 
  "---------------------------------------------------------------------------\n";
static const char * equals_separator =
  "===========================================================================\n";

// Special case to make perf init a soft failure.
// Make sure that we don't use perf if it won't work.
static int perf_unavail = 0;



//******************************************************************************
// thread local variables
//******************************************************************************


long                        __thread my_kernel_samples;
long                        __thread my_user_samples;

int                         __thread myid;

int                         __thread perf_threadinit = 0;

struct sigevent             __thread sigev;
struct timespec             __thread real_start; 
struct timespec             __thread cpu_start; 

int                         __thread perf_thread_fd[MAX_EVENTS];
pe_mmap_t                   __thread *perf_mmap[MAX_EVENTS];

pe_mmap_t                   __thread *current_perf_mmap;


//******************************************************************************
// private operations 
//******************************************************************************

static long
perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
	       int cpu, int group_fd, unsigned long flags)
{
   int ret;

   ret = syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
   return ret;
}


//----------------------------------------------------------
// starting a perf event by setting the signal and enabling the event
//----------------------------------------------------------
static int
perf_start(int event_num)
{
  int ret;

  monitor_real_pthread_sigmask(SIG_UNBLOCK, &sig_mask, NULL);

  fcntl(perf_thread_fd[event_num], F_SETSIG, SIGIO);
  fcntl(perf_thread_fd[event_num], F_SETOWN,getpid());
  fcntl(perf_thread_fd[event_num], F_SETFL, O_RDWR |  O_ASYNC | O_NONBLOCK);

  struct f_owner_ex owner;
  owner.type = F_OWNER_TID;
  owner.pid  = syscall(SYS_gettid);
  ret = fcntl(perf_thread_fd[event_num], F_SETOWN_EX, &owner);

  if (ret == -1) {
    TMSG(LINUX_PERF, "can't set fcntl(F_SETOWN_EX) on %d: %s\n", 
	    perf_thread_fd[event_num], strerror(errno));
  } else {
    ioctl(perf_thread_fd[event_num], PERF_EVENT_IOC_RESET, 0);
  }
  return (ret >= 0);
}


//----------------------------------------------------------
// stop all events
//----------------------------------------------------------
static void
perf_stop()
{
//  if (!perf_initialized) return;

  if (perf_thread_fd[0]) {
    monitor_real_pthread_sigmask(SIG_BLOCK, &sig_mask, NULL);
    ioctl(perf_thread_fd[0], PERF_EVENT_IOC_DISABLE, 0);
  }
}


//----------------------------------------------------------
// read from perf_events mmap'ed buffer
//----------------------------------------------------------

static int 
perf_read(
  void *buf, 
  size_t bytes_wanted
)
{
  // front of the circular data buffer
  char *data = BUFFER_FRONT; 

  // compute bytes available in the circular buffer 
  size_t bytes_available = current_perf_mmap->data_head - current_perf_mmap->data_tail;

  if (bytes_wanted > bytes_available) return -1;

  // compute offset of tail in the circular buffer
  unsigned long tail = BUFFER_OFFSET(current_perf_mmap->data_tail);

  long bytes_at_right = BUFFER_SIZE - tail;

  // bytes to copy to the right of tail
  size_t right = bytes_at_right < bytes_wanted ? bytes_at_right : bytes_wanted;

  // copy bytes from tail position
  memcpy(buf, data + tail, right);

  // if necessary, wrap and continue copy from left edge of buffer
  if (bytes_wanted > right) {
    size_t left = bytes_wanted - right;
    memcpy(buf + right, data, left);
  }

  // update tail after consuming bytes_wanted
  current_perf_mmap->data_tail += bytes_wanted;

  return 0;
}


static inline int
perf_read_header(
  pe_header_t *hdr
)
{
  return perf_read(hdr, sizeof(pe_header_t));
}


static inline int
perf_read_uint64_t(
  uint64_t *val
)
{
  return perf_read(val, sizeof(uint64_t));
}


//----------------------------------------------------------
// predicates that test perf availability
//----------------------------------------------------------

static bool
perf_kernel_syms_avail()
{
  FILE *ksyms = fopen(LINUX_KERNEL_SYMBOL_FILE, "r");
  bool success = (ksyms != NULL);
  if (success) fclose(ksyms);
  return success;
}


static bool
perf_event_avail()
{
  static int checked = PERF_EVENT_AVAILABLE_UNKNOWN;
  
  switch (checked) {
  case PERF_EVENT_AVAILABLE_UNKNOWN:
    {
      struct stat buf;
      int rc = stat("/proc/sys/kernel/perf_event_paranoid", &buf);
      checked = (rc == 0) ? PERF_EVENT_AVAILABLE_YES : PERF_EVENT_AVAILABLE_NO; 
    }
  case PERF_EVENT_AVAILABLE_NO:      
  case PERF_EVENT_AVAILABLE_YES:     
    break;
  }

  return (checked == PERF_EVENT_AVAILABLE_YES);     
}


//----------------------------------------------------------
// initialization and finalization
//----------------------------------------------------------

static void 
perf_init()
{
  pagesize = sysconf(_SC_PAGESIZE);
  tail_mask = PERF_TAIL_MASK(pagesize);

  // initialize mask to block PERF_SIGNAL 
  sigemptyset(&sig_mask);
  sigaddset(&sig_mask, PERF_SIGNAL);

  // if kernel symbols are available, we will attempt to collect kernel 
  // callchains and add them to our call paths 
  perf_ksyms_avail = perf_kernel_syms_avail();

  if (perf_ksyms_avail) {
    hpcrun_kernel_callpath_register(perf_add_kernel_callchain);
    perf_kernel_lm_id = hpcrun_loadModule_add(LINUX_KERNEL_NAME);
  }
 
  monitor_sigaction(PERF_SIGNAL, &perf_event_handler, 0, NULL);
}


//----------------------------------------------------------
// generic initialization for event attributes
// return true if the initialization is successful,
//   false otherwise.
//----------------------------------------------------------
static int
perf_attr_init(
  const char *name,
  struct perf_event_attr *attr,
  long threshold
)
{
  // if perfmon is disabled, by default the event is cycle
  unsigned int event_code = PERF_COUNT_HW_CPU_CYCLES;
  unsigned int event_type = PERF_TYPE_HARDWARE;

#ifdef ENABLE_PERFMON
  if (!pfmu_getEventCode(name, &event_code)) {
      EMSG("Linux perf event not recognized: %s", name);
      return false;
  }
  event_type = pfmu_getEventType(name);
  if (event_type<0) {
      EMSG("Linux perf event type not recognized: %s", name);
      return false;
  }
#endif

  memset(attr, 0, sizeof(struct perf_event_attr));

  attr->size   = sizeof(struct perf_event_attr); /* Size of attribute structure */
  attr->type   = event_type;			 /* Type of event 		*/
  attr->config = event_code;			 /* Type-specific configuration */

  attr->sample_period = threshold;		 /* Period of sampling 		*/
  /* PERF_SAMPLE_READ | PERF_SAMPLE_CALLCHAIN | PERF_SAMPLE_STACK_USER; */
  attr->precise_ip    = PERF_REQUEST_0_SKID;	 /*  requested to have 0 skid.  */
  attr->wakeup_events = PERF_WAKEUP_EACH_SAMPLE;
  attr->disabled      = 1; 			/* the counter will be enabled later  */
  attr->sample_stack_user = 4096;

  //attr->read_format = PERF_FORMAT_GROUP; 
  // attr->pinned      = 0; 

  if (perf_ksyms_avail) {
    attr->sample_type   = PERF_SAMPLE_CALLCHAIN;  /* Records the callchain */
    attr->exclude_callchain_user = EXCLUDE_CALLCHAIN_USER;
  } else {
    attr->sample_type = PERF_SAMPLE_IP;
  }
  TMSG(LINUX_PERF, "init event %s: type: %d, code: %x", name, event_type, event_code);
  return true;
}


//----------------------------------------------------------
// allocate mmap for a given file descriptor
//----------------------------------------------------------
static pe_mmap_t*
set_mmap(int perf_fd)
{
  void *map_result = 
    mmap(NULL, PERF_MMAP_SIZE(pagesize), PROT_WRITE | PROT_READ, 
	   MAP_SHARED, perf_fd, MMAP_OFFSET_0);

  if (map_result == MAP_FAILED) {
      EMSG("Linux perf mmap failed: %s", strerror(errno));
      return NULL;
  }

  pe_mmap_t *mmap  = (pe_mmap_t *) map_result;

  if (mmap) {
    memset(mmap, 0, sizeof(pe_mmap_t));
    mmap->version = 0; 
    mmap->compat_version = 0; 
    mmap->data_head = 0; 
    mmap->data_tail = 0; 
  }
  return mmap;
}

//----------------------------------------------------------
// initialize an event
//  event_num: event number
//  name: name of event (has to be recognized by perf event)
//  threshold: sampling threshold 
//----------------------------------------------------------
static bool
perf_thread_init(int event_num)
{
  // "create"the event
  perf_thread_fd[event_num] = perf_event_open(&events[event_num],
			      THREAD_SELF, CPU_ANY, 
			      GROUP_FD, PERF_FLAGS);

  TMSG(LINUX_PERF, "dbg register event %d, fd: %d", event_num, perf_thread_fd[event_num]);

  // check if perf_event_open is successful
  if (perf_thread_fd[event_num] <0 ) {
      EMSG("Linux perf event open failed: %s", strerror(errno));
      return false;
  }
  perf_mmap[event_num] = set_mmap(perf_thread_fd[event_num]);

  return (perf_start(event_num));
}


//----------------------------------------------------------
// actions when the program terminates: 
//  - unmap the memory
//  - close file descriptors used by each event
//----------------------------------------------------------
void
perf_thread_fini(int nevents)
{
  if (perf_thread_fd[0]) {
    int i;
    
    for (i=0; i< nevents; i++) {
      close(perf_thread_fd[i]);
      munmap(perf_mmap[i], PERF_MMAP_SIZE(pagesize));
    }
  }
}


//----------------------------------------------------------
// signal handling and processing of kernel callchains
//----------------------------------------------------------

// extend a user-mode callchain with kernel frames (if any)
static cct_node_t *
perf_add_kernel_callchain(
  cct_node_t *leaf
)
{
  cct_node_t *parent = leaf;
  pe_header_t hdr; 

  if (perf_read_header(&hdr) == 0) {
    if (hdr.type == PERF_RECORD_SAMPLE) {
      if (hdr.size <= 0) {
	return parent;
      }
      uint64_t n_frames;
      // determine how many frames in the call chain 
      if (perf_read_uint64_t(&n_frames) == 0) {

	if (n_frames > 0) {
	  // allocate space to receive IPs for kernel callchain 
	  uint64_t *ips = alloca(n_frames * sizeof(uint64_t));

	  // read the IPs for the frames 
	  if (perf_read(ips, n_frames * sizeof(uint64_t)) == 0) {

	    // add kernel IPs to the call chain top down, which is the 
	    // reverse of the order in which they appear in ips
	    for (int i = n_frames - 1; i > 0; i--) {
	      ip_normalized_t npc = 
		{ .lm_id = perf_kernel_lm_id, .lm_ip = ips[i] };
	      cct_addr_t frm = { .ip_norm = npc };
	      cct_node_t *child = hpcrun_cct_insert_addr(parent, &frm);
	      parent = child;
	    }
	  } else {
	    TMSG(LINUX_PERF, "unable to read all frames on fd %d", 
		 perf_thread_fd[0]);
	  }
	}
      } else {
	TMSG(LINUX_PERF, "unable to read number of frames on fd %d", 
	     perf_thread_fd[0]);
      }
    }
  }

  return parent;
}


// ---------------------------------------------
// get the index of the file descriptor
// ---------------------------------------------

static int
get_fd_index(int num_events, int fd)
{
  int i;
  
  for (i=0; i<num_events; i++) {
    if (perf_thread_fd[i] == fd)
      return i;
  } 
  // file descriptor not recognized
  return -1; 
}




/******************************************************************************
 * method functions
 *****************************************************************************/

static void
METHOD_FN(init)
{
  TMSG(LINUX_PERF, "%d: init", self->sel_idx);

#ifdef ENABLE_PERFMON
  // perfmon is smart enough to detect if pfmu has been initialized or not
  pfmu_init();
#endif
  perf_unavail = ( !perf_event_avail() );

  self->state = INIT;
  TMSG(LINUX_PERF, "%d: init OK", self->sel_idx);
}


static void
METHOD_FN(thread_init)
{
  TMSG(LINUX_PERF, "%d: thread init", self->sel_idx);
  if (perf_unavail) { return; }

#ifdef ENABLE_PERFMON
  // perfmon is smart enough to detect if pfmu has been initialized or not
  pfmu_init();
#endif

  TMSG(LINUX_PERF, "%d: thread init OK", self->sel_idx);
}


static void
METHOD_FN(thread_init_action)
{
  TMSG(LINUX_PERF, "%d: thread init action", self->sel_idx);

  if (perf_unavail) { return; }

  TMSG(LINUX_PERF, "%d: thread init action OK", self->sel_idx);
}


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

  // setup all requested events
  // if an event cannot be initialized, we still keep it in our list
  //  but there will be no samples

  int nevents = (self->evl).nevents; 

  for (int i=0; i<nevents; i++) {

    // initialize this event. If it's valid, we set the metric for the event
    if (perf_thread_init(i)) { 
      ioctl(perf_thread_fd[i], PERF_EVENT_IOC_ENABLE, 0);
    }
  }

  thread_data_t* td = hpcrun_get_thread_data();
  td->ss_state[self->sel_idx] = START;

  TMSG(LINUX_PERF, "%d: start OK", self->sel_idx);
}

static void
METHOD_FN(thread_fini_action)
{
  TMSG(LINUX_PERF, "%d: unregister thread", self->sel_idx);
  if (perf_unavail) { return; }

  //int nevents = (self->evl).nevents; 
  //perf_thread_fini(nevents);
  TMSG(LINUX_PERF, "%d: unregister thread OK", self->sel_idx);
}


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

  perf_stop();

  thread_data_t* td = hpcrun_get_thread_data();
  td->ss_state[self->sel_idx] = STOP;

  TMSG(LINUX_PERF, "%d: stop OK", self->sel_idx);
}

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


// Return true if Linux perf recognizes the name, whether supported or not.
// We'll handle unsupported events later.
static bool
METHOD_FN(supports_event, const char *ev_str)
{
  TMSG(LINUX_PERF, "supports event %s", ev_str);

  if (perf_unavail) { return false; }

  if (self->state == UNINIT){
    METHOD_CALL(self, init);
  }

  bool ret;

#ifdef ENABLE_PERFMON
  long thresh;
  char ev_tmp[1024];
  if (! hpcrun_extract_ev_thresh(ev_str, sizeof(ev_tmp), ev_tmp, &thresh, DEFAULT_THRESHOLD)) {
    AMSG("WARNING: %s using default threshold %ld, "
   	"better to use an explicit threshold.", ev_str, DEFAULT_THRESHOLD);
  }
  ret = pfmu_isSupported(ev_tmp);
#else
  ret = (strncmp(event_name, ev_str, strlen(event_name)) == 0); 
#endif

  TMSG(LINUX_PERF, "supports event %s", (ret? "OK" : "FAIL"));
  return ret;
}

 
static void
METHOD_FN(process_event_list, int lush_metrics)
{
  TMSG(LINUX_PERF, "process event list");
  if (perf_unavail) { return; }

  metric_desc_properties_t prop = metric_property_none;
  char *event;

  perf_init();

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
  int i=0;
  for (event = start_tok(evlist); more_tok(); event = next_tok(), i++) {
    char name[1024];
    long threshold;

    TMSG(LINUX_PERF,"checking event spec = %s",event);

    hpcrun_extract_ev_thresh(event, sizeof(name), name, &threshold, 
			     DEFAULT_THRESHOLD);

    // initialize the event attributes
    struct perf_event_attr *attr = &events[i];
    perf_attr_init(name, attr, threshold);

    // set the metric
    metric_id[i] = hpcrun_new_metric();

    // store the metric
    METHOD_CALL(self, store_metric_id, i, metric_id[i]);
      
    // initialize the property of the metric
    // if the metric's name has "CYCLES" it mostly a cycle metric 
    //  this assumption is not true, but it's quite closed

    prop = (strstr(name, "CYCLES") != NULL) ? metric_property_cycles : metric_property_none;

    hpcrun_set_metric_info_and_period(metric_id[i], strdup(name),
				    MetricFlags_ValFmt_Int,
				    threshold, prop);
  }
}


static void
METHOD_FN(gen_event_set, int lush_metrics)
{
}


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
  }
}



// ------------------------------------------------------------
// Disable perf event
// ------------------------------------------------------------
static void
disable_perf_event(int fd)
{
#if 0
  int rc = (0>ioctl(fd, PERF_EVENT_IOC_DISABLE, 0));
  if (rc) {
    TMSG(LINUX_PERF, "error fd %d in IOC_DISABLE", fd);
  }
#endif
}

// ------------------------------------------------------------
// Refresh a disabled perf event
// ------------------------------------------------------------

static void 
restart_perf_event(int fd)
{
#if 0
  int rc = ioctl(fd, PERF_EVENT_IOC_REFRESH, 1);
  if (rc == -1) {
    TMSG(LINUX_PERF, "error fd %d in IOC_REFRESH", fd);
  }
#endif
}
/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name linux_perf
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

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

  // get the index of the file descriptor (if we have multiple events)
  // if the file descriptor is not on the list, we shouldn't store the 
  // metrics. Perhaps we should throw away?

  sample_source_t *self = &obj_name();

  int nevents = self->evl.nevents;
  int index   = get_fd_index(nevents, fd);

  // store the metric if the metric index is correct
  if ( index >= 0 ) {
    // TODO: hack solution to point to the mmap of the current event
    // a more reliable way to do this is to pass by argument to hpcrun_sample
    // however, this solution requires too much changes
    current_perf_mmap = perf_mmap[index];

    sample_val_t sv = hpcrun_sample_callpath(context, metric_id[index], 1,
					   0/*skipInner*/, 0/*isSync*/);

    blame_shift_apply(metric_id[index], sv.sample_node, 1 /*metricIncr*/);
  } else {
    // signal not from perf event
    TMSG(LINUX_PERF, "signal si_code %d with fd %d: unknown perf event", 
	 siginfo->si_code, fd);
    restart_perf_event(fd);
    return 1; // tell monitor the signal has not been handled.
  }

  hpcrun_safe_exit();

  // restart the event 
  restart_perf_event(fd);

  return 0; // tell monitor the signal has been handled.
}


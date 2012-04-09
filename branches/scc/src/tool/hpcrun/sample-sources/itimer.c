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
// Copyright ((c)) 2002-2011, Rice University
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
// itimer sample source simple oo interface
//

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <signal.h>
#include <sys/time.h>           /* setitimer() */
#include <ucontext.h>           /* struct ucontext */


/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>


/******************************************************************************
 * local includes
 *****************************************************************************/
#include "sample_source_obj.h"
#include "common.h"

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>

#include <hpcrun/metrics.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/thread_data.h>

#include <lush/lush-backtrace.h>
#include <messages/messages.h>

#include <utilities/tokenize.h>
#include <utilities/arch/context-pc.h>

#include <unwind/common/unwind.h>

#include <lib/support-lean/timer.h>

/******************************************************************************
 * macros
 *****************************************************************************/

#if defined(CATAMOUNT)
#   define HPCRUN_PROFILE_SIGNAL           SIGALRM
#   define HPCRUN_PROFILE_TIMER            ITIMER_REAL
#else
#  define HPCRUN_PROFILE_SIGNAL            SIGPROF
#  define HPCRUN_PROFILE_TIMER             ITIMER_PROF
#endif

#define SECONDS_PER_HOUR                   3600

#if !defined(HOST_SYSTEM_IBM_BLUEGENE)
#  define USE_ELAPSED_TIME_FOR_WALLCLOCK
#endif

#define RESET_ITIMER_EACH_SAMPLE

#if defined(RESET_ITIMER_EACH_SAMPLE)

#  if defined(HOST_SYSTEM_IBM_BLUEGENE)
  //--------------------------------------------------------------------------
  // Blue Gene/P compute node support for itimer incorrectly delivers SIGALRM
  // in one-shot mode. To sidestep this problem, we use itimer in 
  // interval mode, but with an interval so long that we never expect to get 
  // a repeat interrupt before resetting it. 
  //--------------------------------------------------------------------------
#    define AUTOMATIC_ITIMER_RESET_SECONDS(x)            (SECONDS_PER_HOUR) 
#    define AUTOMATIC_ITIMER_RESET_MICROSECONDS(x)       (0)
#  else  // !defined(HOST_SYSTEM_IBM_BLUEGENE)
#    define AUTOMATIC_ITIMER_RESET_SECONDS(x)            (0) 
#    define AUTOMATIC_ITIMER_RESET_MICROSECONDS(x)       (0)
#  endif // !defined(HOST_SYSTEM_IBM_BLUEGENE)

#else  // !defined(RESET_ITIMER_EACH_SAMPLE)

#  define AUTOMATIC_ITIMER_RESET_SECONDS(x)              (x)
#  define AUTOMATIC_ITIMER_RESET_MICROSECONDS(x)         (x)

#endif // !defined(RESET_ITIMER_EACH_SAMPLE)

#define DEFAULT_PERIOD  5000L

/******************************************************************************
 * local constants
 *****************************************************************************/

enum _local_const {
  ITIMER_EVENT = 0,    // itimer has only 1 event
  P5_EVENT = 1
};

/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static int
itimer_signal_handler(int sig, siginfo_t *siginfo, void *context);

int P5name2code (char* name);
/******************************************************************************
 * local variables
 *****************************************************************************/

static struct itimerval itimer;

static const struct itimerval zerotimer = {
  .it_interval = {
    .tv_sec  = 0L,
    .tv_usec = 0L
  },

  .it_value    = {
    .tv_sec  = 0L,
    .tv_usec = 0L
  }

};

static int sample_num_id = -1;

// ******* Thermal sensors support ****
// There are two thermal sensors. One is near the mesh and the other
// is near core0 of the tile
#include<unistd.h>
#include<sys/mman.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>

#define CRB_OWN    0xf8000000
#define CONTL      0x40
#define VAL        0x48

static int mesh_sensor_id = -1;
static int core_sensor_id = -1;

void write_thermal_counter(int val)
{
  typedef volatile unsigned char* t_vcharp;

  int PAGE_SIZE, NCMDeviceFD;
  t_vcharp MappedAddr;
  unsigned int alignedAddr, pageOffset, ConfigAddr;

  ConfigAddr=CRB_OWN+CONTL;
  PAGE_SIZE=getpagesize();

  if((NCMDeviceFD=open("/dev/rckncm", O_RDWR|O_SYNC))<0)
  {
    hpcrun_ssfail_start("write thermal counter: open /dev/rckncm for SCC");
  }

  alignedAddr = ConfigAddr & (~(PAGE_SIZE-1));
  pageOffset = ConfigAddr - alignedAddr;

  MappedAddr=(t_vcharp)mmap(NULL, PAGE_SIZE, PROT_WRITE|PROT_READ, MAP_SHARED, NCMDeviceFD, alignedAddr);

  if(MappedAddr == MAP_FAILED)
  {
    hpcrun_ssfail_start("write thermal counter: mmap for SCC");
  }

  *(unsigned int*)(MappedAddr+pageOffset) = val;
  munmap((void*)MappedAddr, PAGE_SIZE);
  close(NCMDeviceFD);
}

void read_thermal_counter(unsigned int *mesh_counter, unsigned int *core_counter)
{
  typedef volatile unsigned char* t_vcharp;

  int PAGE_SIZE, NCMDeviceFD;
  unsigned int result;
  t_vcharp MappedAddr;
  unsigned int alignedAddr, pageOffset, ConfigAddr;

  ConfigAddr=CRB_OWN+VAL;
  PAGE_SIZE=getpagesize();

  if((NCMDeviceFD=open("/dev/rckncm", O_RDWR|O_SYNC))<0)
  {
    hpcrun_ssfail_start("read thermal counter: open /dev/rckncm for SCC");
    exit(-1);
  }

  alignedAddr = ConfigAddr & (~(PAGE_SIZE-1));
  pageOffset = ConfigAddr - alignedAddr;

  MappedAddr=(t_vcharp)mmap(NULL, PAGE_SIZE, PROT_WRITE|PROT_READ, MAP_SHARED, NCMDeviceFD, alignedAddr);

  if(MappedAddr == MAP_FAILED)
  {
    hpcrun_ssfail_start("read thermal counter: mmap for SCC");
    exit(-1);
  }

  result = *(unsigned int*)(MappedAddr+pageOffset);
  munmap((void*)MappedAddr, PAGE_SIZE);

  *mesh_counter = result >> 13;
  *core_counter = result & 0x1fff;
  close(NCMDeviceFD);
}

unsigned int coreID()
{
  typedef volatile unsigned char* t_vcharp;

  int PAGE_SIZE, NCMDeviceFD;

  unsigned int result, tileID, coreID, x_val, y_val,
        coreID_mask=0x00000007, x_mask=0x00000078, y_mask=0x00000780;

  t_vcharp MappedAddr;
  unsigned int alignedAddr, pageOffset, ConfigAddr;

  ConfigAddr=CRB_OWN+0x100;
  PAGE_SIZE=getpagesize();

  if((NCMDeviceFD=open("/dev/rckncm", O_RDWR|O_SYNC))<0)
  {
    perror("open");
    exit(-1);
  }

  alignedAddr = ConfigAddr & (~(PAGE_SIZE-1));
  pageOffset = ConfigAddr - alignedAddr;

  MappedAddr=(t_vcharp)mmap(NULL, PAGE_SIZE, PROT_WRITE|PROT_READ, MAP_SHARED, NCMDeviceFD, alignedAddr);

  if(MappedAddr == MAP_FAILED)
  {
    perror("mmap");
    exit(-1);
  }

  result = *(unsigned int*)(MappedAddr+pageOffset);
  munmap((void*)MappedAddr, PAGE_SIZE);
  close(NCMDeviceFD);

  coreID = result & coreID_mask;  
  return coreID;
}

// ******* CALIPER SUPPORT ************
#include "libperfctr.h"

static struct vperfctr *self;
static struct vperfctr_control control;

static struct perfctr_sum_ctrs thiscounter;
static uint64_t precounter0, precounter1;
static char *P5name0, *P5name1;
static char name0[100], name1[100];
void do_init(void)
{
  struct perfctr_cpus_info *cpus_info;

  self = vperfctr_open();
  if(!self)
  {
    perror("vperfctr_open");
    exit(1);
  }
}

void do_setup (struct perfctr_cpu_control *cpu_control, char* name)
{
  int eventcode0, eventcode1;

  P5name0 = strtok(name, ".");
  P5name1 = strtok(NULL, ".");
  strcpy(name0, P5name0);
  strcpy(name1, P5name1);
  TMSG(ITIMER_CTL, "P5 event name is %s and %s", name0, name1);  

  unsigned int tsc_on = 0;
  unsigned int nractrs = 2;
  unsigned int pmc_map0 = 0;
  unsigned int pmc_map1 = 1;
  unsigned int evntsel0 = 0;
  unsigned int evntsel1 = 0;

  memset(cpu_control, 0, sizeof *cpu_control);
  eventcode0 = P5name2code(P5name0);
  eventcode1 = P5name2code(P5name1);
  if(eventcode0 == -1)
  {
    eventcode0 = 0x00;
    strcpy(name0, "P5_DATA_READ");
  }
  if(eventcode1 == -1)
  {
    eventcode1 = 0x03;
    strcpy(name1, "P5_DATA_READ_MISS");
  }
//  evntsel0 = 0x00 | (3 << 6);
  evntsel0 = eventcode0 | (3 << 6);
  evntsel1 = eventcode1 | (3 << 6);
  cpu_control->tsc_on = tsc_on;
  cpu_control->nractrs = nractrs;
  cpu_control->pmc_map[0]=pmc_map0;
  cpu_control->pmc_map[1]=pmc_map1;
  cpu_control->evntsel[0] = evntsel0;
  cpu_control->evntsel[1] = evntsel1;

  if(vperfctr_control(self, &control) < 0)
  {
    perror("vperfctr_control");
    exit(1);
  }
}

void do_read(struct perfctr_sum_ctrs *sum)
{
  vperfctr_read_ctrs(self, sum);
}

static long period = DEFAULT_PERIOD;

static sigset_t sigset_itimer;

// ******* METHOD DEFINITIONS ***********
static void
METHOD_FN(init)
{
  TMSG(ITIMER_CTL, "setting up itimer interrupt");
  sigemptyset(&sigset_itimer);
  sigaddset(&sigset_itimer, HPCRUN_PROFILE_SIGNAL);

  int ret = monitor_real_sigprocmask(SIG_UNBLOCK, &sigset_itimer, NULL);

  if (ret){
    EMSG("WARNING: Thread init could not unblock SIGPROF, ret = %d",ret);
  }
  self->state = INIT;
}

static void
METHOD_FN(thread_init)
{
  TMSG(ITIMER_CTL, "thread init (no action needed)");
}

static void
METHOD_FN(thread_init_action)
{
  TMSG(ITIMER_CTL, "thread action (noop)");
}

static void
METHOD_FN(start)
{
  if (! hpcrun_td_avail()){
    TMSG(ITIMER_CTL, "Thread data unavailable ==> sampling suspended");
    return; // in the unlikely event that we are trying to start, but thread data is unavailable,
            // assume that all sample source ops are suspended.
  }

  TMSG(ITIMER_CTL,"starting itimer w value = (%d,%d), interval = (%d,%d)",
       itimer.it_value.tv_sec,
       itimer.it_value.tv_usec,
       itimer.it_interval.tv_sec,
       itimer.it_interval.tv_usec);

  if (setitimer(HPCRUN_PROFILE_TIMER, &itimer, NULL) != 0) {
    TMSG(ITIMER_CTL, "setitimer failed to start!!");
    EMSG("setitimer failed (%d): %s", errno, strerror(errno));
    hpcrun_ssfail_start("itimer");
  }

#ifdef USE_ELAPSED_TIME_FOR_WALLCLOCK
  int ret = time_getTimeReal(&TD_GET(last_time_us));
  if (ret != 0) {
    EMSG("time_getTimeReal (clock_gettime) failed!");
    abort();
  }
#endif

  TD_GET(ss_state)[self->evset_idx] = START;
 //************caliper: read the init value of counters **************
  do_read(&thiscounter);
  precounter0 = thiscounter.pmc[0];
  precounter1 = thiscounter.pmc[1];
  TMSG(ITIMER_CTL, "precounter1 is %d", precounter1);
  //******************end***************************

}

static void
METHOD_FN(thread_fini_action)
{
  TMSG(ITIMER_CTL, "thread action (noop)");
}

static void
METHOD_FN(stop)
{
  int rc;

  rc = setitimer(HPCRUN_PROFILE_TIMER, &zerotimer, NULL);

  write_thermal_counter(0);

  TMSG(ITIMER_CTL,"stopping itimer");
  TD_GET(ss_state)[self->evset_idx] = STOP;
}

static void
METHOD_FN(shutdown)
{
  TMSG(ITIMER_CTL, "shutodown itimer");
  METHOD_CALL(self, stop); // make sure stop has been called

  int ret = monitor_real_sigprocmask(SIG_BLOCK, &sigset_itimer, NULL);
  if (ret){
    EMSG("WARNING: process fini could not block SIGPROF, ret = %d",ret);
  }

  self->state = UNINIT;
}

static bool
METHOD_FN(supports_event, const char *ev_str)
{
  return (strstr(ev_str,"WALLCLOCK") != NULL);
}
 
int P5_metric_id0, P5_metric_id1;

static void
METHOD_FN(process_event_list, int lush_metrics)
{

  TMSG(ITIMER_CTL, "process event list, lush_metrics = %d", lush_metrics);
  // fetch the event string for the sample source
  char* _p = METHOD_CALL(self, get_event_str);
  
  //
  // EVENT: Only 1 wallclock event
  //
  char* event = start_tok(_p);

  char name[1024]; // local buffer needed for extract_ev_threshold

  TMSG(ITIMER_CTL,"checking event spec = %s",event);

  // extract event threshold
  hpcrun_extract_ev_thresh(event, sizeof(name), name, &period, DEFAULT_PERIOD);

  // store event threshold
  METHOD_CALL(self, store_event, ITIMER_EVENT, period);
  METHOD_CALL(self, store_event, P5_EVENT, period);
  TMSG(OPTIONS,"wallclock period set to %ld",period);

  // set up file local variables for sample source control
  int seconds = period / 1000000;
  int microseconds = period % 1000000;

  TMSG(OPTIONS,"init timer w sample_period = %ld, seconds = %ld, usec = %ld",
       period, seconds, microseconds);

  // signal once after the given delay
  itimer.it_value.tv_sec = seconds;
  itimer.it_value.tv_usec = microseconds;

  // macros define whether automatic restart or not
  itimer.it_interval.tv_sec  =  AUTOMATIC_ITIMER_RESET_SECONDS(seconds);
  itimer.it_interval.tv_usec =  AUTOMATIC_ITIMER_RESET_MICROSECONDS(microseconds);

  // handle metric allocation
  hpcrun_pre_allocate_metrics(1 + lush_metrics);
  
  int metric_id = hpcrun_new_metric();
  METHOD_CALL(self, store_metric_id, ITIMER_EVENT, metric_id);
  
  //************* Setup calipers ***********************
  do_init();
  do_setup(&control.cpu_control,name);
  P5_metric_id0 = hpcrun_new_metric();
  TMSG(ITIMER_CTL, "name0, 1 are %s, %s", name0, name1);
  hpcrun_set_metric_info_and_period(P5_metric_id0, name0,
                                    MetricFlags_ValFmt_Int,
                                    1);
  P5_metric_id1 = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(P5_metric_id1, name1,
                                    MetricFlags_ValFmt_Int,
                                    1);

  //************* Setup Thermal sensors ****************
  // init the thermal control counter
  write_thermal_counter(9556);
  mesh_sensor_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(mesh_sensor_id, "MESH_THERMAL",
                                    MetricFlags_ValFmt_Int,
                                    1);
  core_sensor_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(core_sensor_id, "CORE_THERMAL",
                                    MetricFlags_ValFmt_Int,
                                    1);

  sample_num_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(sample_num_id, "SAMPLE_NUM",
                                    MetricFlags_ValFmt_Int,
                                    1);
 //************caliper: read the init value of counters **************
//  do_read(&thiscounter);
//  precounter0 = thiscounter.pmc[0];
//  precounter1 = thiscounter.pmc[1];
//  TMSG(ITIMER_CTL, "precounter1 is %d", precounter1);
  //******************end***************************

  // set metric information in metric table

#ifdef USE_ELAPSED_TIME_FOR_WALLCLOCK
# define sample_period 1
#else
# define sample_period period
#endif

  TMSG(ITIMER_CTL, "setting metric itimer period = %ld", sample_period);
  hpcrun_set_metric_info_and_period(metric_id, "WALLCLOCK (us)",
				    MetricFlags_ValFmt_Int,
				    sample_period);
  if (lush_metrics == 1) {
    int mid_idleness = hpcrun_new_metric();
    lush_agents->metric_time = metric_id;
    lush_agents->metric_idleness = mid_idleness;

    hpcrun_set_metric_info_and_period(mid_idleness, "idleness (us)",
				      MetricFlags_ValFmt_Real,
				      sample_period);
  }

  event = next_tok();
  if (more_tok()) {
    EMSG("MULTIPLE WALLCLOCK events detected! Using first event spec: %s");
  }
  // 
  thread_data_t *td = hpcrun_get_thread_data();
  td->eventSet[self->evset_idx] = 0xDEAD;
}

//
// There is only 1 event for itimer, hence the event "set" is always the same.
// The signal setup, however, is done here.
//
static void
METHOD_FN(gen_event_set, int lush_metrics)
{
  monitor_sigaction(HPCRUN_PROFILE_SIGNAL, &itimer_signal_handler, 0, NULL);
}

static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available itimer events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("WALLCLOCK\tWall clock time used by the process in microseconds\n");
  printf("\n");
}

/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name itimer
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

/******************************************************************************
 * private operations 
 *****************************************************************************/

static int
itimer_signal_handler(int sig, siginfo_t* siginfo, void* context)
{
  // Must check for async block first and avoid any MSG if true.
  void* pc = hpcrun_context_pc(context);
  if (hpcrun_async_is_blocked(pc)) {
    if (ENABLED(ITIMER_CTL)) {
      ; // message to stderr here for debug
    }
    hpcrun_stats_num_samples_blocked_async_inc();
  }
  else {
    TMSG(ITIMER_HANDLER,"Itimer sample event");

    uint64_t metric_incr = 1; // default: one time unit
#ifdef USE_ELAPSED_TIME_FOR_WALLCLOCK
    uint64_t cur_time_us;
    int ret = time_getTimeReal(&cur_time_us);
    if (ret != 0) {
      EMSG("time_getTimeReal (clock_gettime) failed!");
      abort();
    }
    metric_incr = cur_time_us - TD_GET(last_time_us);
#endif

    int metric_id = hpcrun_event2metric(&_itimer_obj, ITIMER_EVENT);
    cct_node_t *node = hpcrun_sample_callpath(context, metric_id, metric_incr,
			   0/*skipInner*/, 0/*isSync*/);
    //*********** Add caliper event metrics to CCT *********
    do_read(&thiscounter);
    uint64_t value0 = thiscounter.pmc[0]-precounter0;
    uint64_t value1 = thiscounter.pmc[1]-precounter1;
   
    TMSG(ITIMER_CTL, "count result is %ld, %ld", value0, value1);
    cct_metric_data_increment(P5_metric_id0,
		    hpcrun_cct_metrics(node) + P5_metric_id0,
		    (cct_metric_data_t){.i = value0});
    cct_metric_data_increment(P5_metric_id1,
		    hpcrun_cct_metrics(node) + P5_metric_id1,
		    (cct_metric_data_t){.i = value1});
//    hpcrun_sample_callpath(context, P5_metric_id0, value0,
//                           0/*skipInner*/, 0/*isSync*/);
//    hpcrun_sample_callpath(context, P5_metric_id1, value1,
//                           0/*skipInner*/, 0/*isSync*/);
    //********************** end *************************

    //***************** Add thermal counter support ************
    unsigned int mesh_counter[10], core_counter[10];
    unsigned int mesh_counter_all = 0;
    unsigned int core_counter_all = 0;
    int i;
    // only let core0 read the sensor counters
    if(coreID() == 0) {
    // read the counter 10 times to minimize the noise
	    for(i=0; i<10; i++)
	    {
		    read_thermal_counter(&mesh_counter[i], &core_counter[i]);
		    mesh_counter_all += mesh_counter[i];
		    core_counter_all += core_counter[i];
	    }
	    cct_metric_data_increment(mesh_sensor_id,
			    hpcrun_cct_metrics(node) + mesh_sensor_id,
			    (cct_metric_data_t){.i = mesh_counter_all/10});
	    cct_metric_data_increment(core_sensor_id,
			    hpcrun_cct_metrics(node) + core_sensor_id,
			    (cct_metric_data_t){.i = core_counter_all/10});
	    cct_metric_data_increment(sample_num_id,
			    hpcrun_cct_metrics(node) + sample_num_id,
			    (cct_metric_data_t){.i = 1});
    }
  }
  if (hpcrun_is_sampling_disabled()) {
    TMSG(SPECIAL, "No itimer restart, due to disabled sampling");
    return 0;
  }

#ifdef RESET_ITIMER_EACH_SAMPLE
  METHOD_CALL(&_itimer_obj, start);
#endif

  return 0; /* tell monitor that the signal has been handled */
}


//*************This is used to comvert event name to event code for CALIPER ******************
struct P5_event_set
{
  char *name;
  int  code;
};

static const struct P5_event_set P5_events[]=
{
  {"P5_DATA_READ", 0x00},
  {"P5_DATA_WRITE", 0x01},
  {"P5_DATA_TLB_MISS", 0x02},
  {"P5_DATA_READ_MISS", 0x03},
  {"P5_DATA_WRITE_MISS", 0x04},
  {"P5_WRITE_HIT_TO_M_OR_E_STATE_LINES", 0x05},
  {"P5_DATA_CACHE_LINES_WRITTEN_BACK", 0x06},
  {"P5_EXTERNAL_SNOOPS", 0x07},
  {"P5_EXTERNAL_DATA_CACHE_SNOOP_HITS", 0x08},
  {"P5_MEMORY_ACCESSES_IN_BOTH_PIPES", 0x09},
  {"P5_BANK_CONFLICTS", 0x0A},
  {"P5_MISALIGNED_DATA_MEMORY_OR_IO_REFERENCES", 0x0B},
  {"P5_CODE_READ", 0x0C},
  {"P5_CODE_TLB_MISS", 0x0D},
  {"P5_CODE_CACHE_MISS", 0x0E},
  {"P5_ANY_SEGMENT_REGISTER_LOADED", 0x0F},
  {"P5_BRANCHES", 0x12},
  {"P5_BTB_HITS", 0x13},
  {"P5_TAKEN_BRANCH_OR_BTB_HIT", 0x14},
  {"P5_PIPELINE_FLUSHES", 0x15},
  {"P5_INSTRUCTIONS_EXECUTED", 0x16},
  {"P5_INSTRUCTIONS_EXECUTED_V_PIPE", 0x17},
  {"P5_BUS_CYCLE_DURATION", 0x18},
  {"P5_WRITE_BUFFER_FULL_STALL_DURATION", 0x19},
  {"P5_WAITING_FOR_DATA_MEMORY_READ_STALL_DURATION", 0x1A},
  {"P5_STALL_ON_WRITE_TO_AN_E_OR_M_STATE_LINE", 0x1B},
  {"P5_LOCKED_BUS_CYCLE", 0x1C},
  {"P5_IO_READ_OR_WRITE_CYCLE", 0x1D},
  {"P5_NONCACHEABLE_MEMORY_READS", 0x1E},
  {"P5_PIPELINE_AGI_STALLS", 0x1F},
  {"P5_FLOPS", 0x22},
  {"P5_BREAKPOINT_MATCH_ON_DR0_REGISTER", 0x23},
  {"P5_BREAKPOINT_MATCH_ON_DR1_REGISTER", 0x24},
  {"P5_BREAKPOINT_MATCH_ON_DR2_REGISTER", 0x25},
  {"P5_BREAKPOINT_MATCH_ON_DR3_REGISTER", 0x26},
  {"P5_HARDWARE_INTERRUPTS", 0x27},
  {"P5_DATA_READ_OR_WRITE", 0x28},
  {"P5_DATA_READ_MISS_OR_WRITE_MISS", 0x29},
};

int P5name2code(char *name)
{
  int code;
  int i;

  int num_event = sizeof(P5_events)/sizeof(struct P5_event_set);
  TMSG(ITIMER_CTL, "P5 event number is %d, %s", num_event, name);
  for (i=0; i<num_event; i++)
  {
    if(strcmp(P5_events[i].name, name) == 0)
    {
      code = P5_events[i].code;
      break;
    }
  }
  if(i == num_event)
    code = -1;
  TMSG(ITIMER_CTL, "P5 event code is %d", code);
  return code;
}

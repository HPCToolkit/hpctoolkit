// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/sample-sources/ibsop.c $
// $Id: itimer.c 3368 2011-01-20 21:19:28Z xl10 $
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
#include <sys/syscall.h>
#include <unistd.h>


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
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/thread_data.h>

#include <lush/lush-backtrace.h>
#include <messages/messages.h>

#include <utilities/tokenize.h>
#include <utilities/arch/context-pc.h>
#include <utilities/ip-normalized.h>

#include <unwind/common/unwind.h>

#include <lib/support-lean/timer.h>

#include <hpcrun/datacentric.h>
#include <numa.h>
#include <numaif.h>
#include <hpcrun/loadmap.h>

/******************************************************************************
 * special includes from libpfm for IBS
 *****************************************************************************/
#include <perfmon/pfmlib.h>
#include <perfmon/perfmon_dfl_smpl.h>
#include <perfmon/perfmon.h>
#include <perfmon/pfmlib_amd64.h>

/* control the file descriptors */
#include <fcntl.h>

/******************************************************************************
 * macros
 *****************************************************************************/

#define HPCRUN_PROFILE_SIGNAL           SIGIO
#define MAX_FD 				100
#define METRIC_NUM			12
#define DEFAULT_PERIOD			65535L

/******************************************************************************
 * local constants
 *****************************************************************************/

enum _local_const {
  IBS_OP_EVENT = 0    // IBS OP has only 1 event
};

/******************************************************************************
 * local inline functions
 *****************************************************************************/
static inline pid_t
gettid()
{
        return (pid_t)syscall(__NR_gettid);
}

/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static void
hpcrun_metric_min(int metric_id, metric_set_t* set,
                      hpcrun_metricVal_t incr);
static void
hpcrun_metric_max(int metric_id, metric_set_t* set,
                      hpcrun_metricVal_t incr);

static int
ibsop_signal_handler(int sig, siginfo_t *siginfo, void *context);

/******************************************************************************
 * local variables
 *****************************************************************************/
static sigset_t sigset_ibs;

struct over_args {
        pfmlib_amd64_input_param_t inp_mod;
        pfmlib_output_param_t outp;
        pfmlib_amd64_output_param_t outp_mod;
        pfarg_pmc_t pc[PFMLIB_MAX_PMCS];
        pfarg_pmd_t pd[PFMLIB_MAX_PMDS];
        pfarg_ctx_t ctx;
        pfarg_load_t load_args;
        int fd;
        pid_t tid;
        pthread_t self;
        int count;
        // workaround
        pfmlib_reg_t tmppc;
};

struct over_args fd2ov[MAX_FD];

//metrics for one sample: 0: min address, 1: max address, 2: #load+#store, 
//3: #cache misses, 4: #latency, 5: L1 tlb miss, 6: L2 tlb miss, 7: local DRAM access
//8: remote DRAM access, 9: # of access to caches of local cores, 10: # of access to caches 
//of remote cores, 11: # of total ibs samples.
static int metrics [METRIC_NUM];

static long period = DEFAULT_PERIOD;

// Metrics for numa related metrics
static int numa_lat_metric_id = -1;
static int numa_match_metric_id = -1;
static int numa_mismatch_metric_id = -1;

static int low_offset_metric_id = -1;
static int high_offset_metric_id = -1;

static int *location_metric_id;

// ******* METHOD DEFINITIONS ***********
static void
METHOD_FN(init)
{
  return;
  if(pfm_initialize() != PFMLIB_SUCCESS){
    EEMSG("libpfm init NOT ok");
    monitor_real_abort();
  }

  int type;
  pfm_get_pmu_type(&type);
  if(type != PFMLIB_AMD64_PMU){
    return;
    EEMSG("CPU is not AMD64");
    monitor_real_abort();
  }

  TMSG(IBS_SAMPLE, "setting up ibs interrupt");
  sigemptyset(&sigset_ibs);
  sigaddset(&sigset_ibs, HPCRUN_PROFILE_SIGNAL);

  int ret = monitor_real_sigprocmask(SIG_UNBLOCK, &sigset_ibs, NULL);

  if (ret){
    EMSG("WARNING: Thread init could not unblock SIGIO, ret = %d",ret);
  }
  self->state = INIT;
}

static void
METHOD_FN(thread_init)
{
  TMSG(IBS_SAMPLE, "thread init (no action needed)");
}

static void
METHOD_FN(thread_init_action)
{
  TMSG(IBS_SAMPLE, "thread action (noop)");
}

static void
METHOD_FN(start)
{
  int ret;
  struct over_args *ov;
  thread_data_t *td;

  if (! hpcrun_td_avail()){
    TMSG(IBS_SAMPLE, "Thread data unavailable ==> sampling suspended");
    return; // in the unlikely event that we are trying to start, but thread data is unavailable,
            // assume that all sample source ops are suspended.
  }
  td = hpcrun_get_thread_data();
  
  TMSG(IBS_SAMPLE, "starting IBS %d %d", td->core_profile_trace_data.id, period);

  ov = &fd2ov[td->core_profile_trace_data.id];
  // not reinit for the IBS setting
  if(ov->fd) pfm_unload_context(ov->fd);
  memset(ov, 0, sizeof(struct over_args));

  ov->inp_mod.ibsop.maxcnt = period;
  ov->inp_mod.ibsop.options = IBS_OPTIONS_UOPS;
  ov->inp_mod.flags |= PFMLIB_AMD64_USE_IBSOP;

  ret = pfm_dispatch_events(NULL, &ov->inp_mod, &ov->outp, &ov->outp_mod);
  if(ret != PFMLIB_SUCCESS){
    EMSG("Cannot dispatch events: %s", pfm_strerror(ret));
    monitor_real_abort();
  }
  if(ov->outp.pfp_pmc_count != 1){
    EMSG("Unexpected PMC register count: %d", ov->outp.pfp_pmc_count);
    monitor_real_abort();
  }
  if (ov->outp.pfp_pmd_count != 1) {
    EMSG("Unexpected PMD register count: %d", ov->outp.pfp_pmd_count);
    monitor_real_abort();
  }
  if (ov->outp_mod.ibsop_base != 0) {
    EMSG("Unexpected IBSOP base register: %d", ov->outp_mod.ibsop_base);
    monitor_real_abort();
  }

  /* program the PMU */  
  ov->pc[0].reg_num=ov->outp.pfp_pmcs[0].reg_num;
  ov->pc[0].reg_value=ov->outp.pfp_pmcs[0].reg_value;

  ov->pd[0].reg_num=ov->outp.pfp_pmds[0].reg_num;//pd[0].reg_num=7
  ov->pd[0].reg_value=0;

  ov->pd[0].reg_flags |= PFM_REGFL_OVFL_NOTIFY;

  ov->pd[0].reg_smpl_pmds[0]=((1UL<<7)-1)<<ov->outp.pfp_pmds[0].reg_num;

  /* setup the file descriptor */
  int fd = pfm_create_context(&ov->ctx, NULL, 0, 0);
  if(fd==-1){
    EMSG("error in create context");
    hpcrun_ssfail_start("IBS");
  }
  ov->fd = fd;

  /* write performance counters */
  if(pfm_write_pmcs(ov->fd,ov->pc,ov->outp.pfp_pmc_count)){
    EMSG("error in write pmc");
    hpcrun_ssfail_start("IBS");
  }

  if(pfm_write_pmds(ov->fd,ov->pd,ov->outp.pfp_pmd_count)){
    EMSG("error in write pmd");
    hpcrun_ssfail_start("IBS");
  }
  
  /* record thrad tid */
  ov->load_args.load_pid = gettid();

  /* load context */
  if(pfm_load_context(ov->fd, &ov->load_args) != 0){
    EMSG("error in load");
    // if context is reloaded, just return
    // errno EEXIST means it has already load the context for the fd
    if (errno == EEXIST) return;
    hpcrun_ssfail_start("IBS");
  }
  
  /* configure file descriptor to make sure the samples
     go to the correct thread */
  ret = fcntl(ov->fd, F_SETFL, fcntl(ov->fd, F_GETFL, 0) | O_ASYNC);
  if (ret == -1){
    EMSG("cannot set ASYNC");
    hpcrun_ssfail_start("IBS");
  }

  #ifndef F_SETOWN_EX
  #define F_SETOWN_EX     15
  #define F_GETOWN_EX     16

  #define F_OWNER_TID     0
  #define F_OWNER_PID     1
  #define F_OWNER_GID     2

  struct f_owner_ex {
       int     type;
       pid_t   pid;
  };
  #endif
  {
    struct f_owner_ex fown_ex;
    fown_ex.type = F_OWNER_TID;
    fown_ex.pid  = gettid();
    ret = fcntl(ov->fd, F_SETOWN_EX, &fown_ex);
    if (ret == -1){
      EMSG("cannot set ownership");
      hpcrun_ssfail_start("IBS");
    }
  }

  ret = fcntl(ov->fd, F_SETSIG, SIGIO);
  if (ret < 0){
    EMSG("cannot set SIGIO");
    hpcrun_ssfail_start("IBS");
  }

  if(pfm_self_start(ov->fd) == -1){
    EMSG("start error");
    hpcrun_ssfail_start("IBS");
  }

  TD_GET(ss_state)[self->evset_idx] = START;
  
  /* update thread private data */
  td->ibs_ptr = (void *)ov; 
}

static void
METHOD_FN(thread_fini_action)
{
  TMSG(ITIMER_CTL, "thread action (noop)");
}

static void
METHOD_FN(stop)
{
  struct over_args *ov;
  thread_data_t *td = hpcrun_get_thread_data();

  source_state_t my_state = TD_GET(ss_state)[self->evset_idx];

  if (my_state == STOP) {
    TMSG(IBS_SAMPLE, "--stop called on an already stopped measurement");
    return;
  }

  if (my_state != START) {
    TMSG(IBS_SAMPLE, "*WARNING* Stop called on IBS that has not been started");
    return;
  }

  ov = (struct over_args *)td->ibs_ptr;
  if(pfm_self_stop(ov->fd) == -1){
    EMSG("fail to stop IBS");
  }

  int i;
  for(i = 0; i < MAX_FD; i++){
    if(fd2ov[i].fd == ov->fd)
      break;
  }
  TMSG(IBS_SAMPLE, "fd=%d, i=%d, count=%d", ov->fd, i, fd2ov[i].count);

  TMSG(IBS_SAMPLE, "stopping IBS op");

  TD_GET(ss_state)[self->evset_idx] = STOP;
}

static void
METHOD_FN(shutdown)
{
  TMSG(IBS_SAMPLE, "shutodown IBS op");
  METHOD_CALL(self, stop); // make sure stop has been called

  int ret = monitor_real_sigprocmask(SIG_BLOCK, &sigset_ibs, NULL);
  if (ret){
    EMSG("WARNING: process fini could not block SIGIO, ret = %d",ret);
  }

  self->state = UNINIT;
}

static bool
METHOD_FN(supports_event, const char *ev_str)
{
  return (strstr(ev_str,"IBS_OP") != NULL);
}
 
static void
METHOD_FN(process_event_list, int lush_metrics)
{

  TMSG(IBS_SAMPLE, "process event list, lush_metrics = %d", lush_metrics);
  // fetch the event string for the sample source
  char* evlist = METHOD_CALL(self, get_event_str);
  
  //
  // EVENT: Only 1 IBS_OP event
  //
  char* event = start_tok(evlist);

  char name[1024]; // local buffer needed for extract_ev_threshold

  TMSG(IBS_SAMPLE,"checking event spec = %s",event);

  // extract event threshold
  hpcrun_extract_ev_thresh(event, sizeof(name), name, &period, DEFAULT_PERIOD);
  period = period << 4;
  TMSG(IBS_SAMPLE,"IBS_SAMPLE period set to %ld",period);

  // store event threshold
  METHOD_CALL(self, store_event, IBS_OP_EVENT, period);

  /* setup all metrics */
  int i;
  for(i = 2; i < METRIC_NUM; i++) {
    metrics[i] = hpcrun_new_metric();
    METHOD_CALL(self, store_metric_id, IBS_OP_EVENT, metrics[i]);
    TMSG(IBS_SAMPLE,"setting metric%d IBS,period = %ld",i,period);

#if 0
    if(i == 0)
      hpcrun_set_metric_info_w_fn(metrics[i], "ADDR(min)",
				  MetricFlags_ValFmt_Int,
				  1, addr_min);
    if(i == 1)
      hpcrun_set_metric_info_w_fn(metrics[i], "ADDR(max)",              
                                  MetricFlags_ValFmt_Int,               
                                  1, addr_max);
#endif
    if(i == 2)
      hpcrun_set_metric_info_and_period(metrics[i], "#(LD+ST)", MetricFlags_ValFmt_Int, 1, metric_property_none);
    if(i == 3)
      hpcrun_set_metric_info_and_period(metrics[i], "CACHE_MISS", MetricFlags_ValFmt_Int, 1, metric_property_none);
    if(i == 4)
      hpcrun_set_metric_info_and_period(metrics[i], "LATENCY", MetricFlags_ValFmt_Int, 1, metric_property_none);
    if(i == 5)
      hpcrun_set_metric_info_and_period(metrics[i], "L1_TLB_MISS", MetricFlags_ValFmt_Int, 1, metric_property_none);
    if(i == 6)
      hpcrun_set_metric_info_and_period(metrics[i], "L2_TLB_MISS", MetricFlags_ValFmt_Int, 1, metric_property_none);
    if(i == 7)
      hpcrun_set_metric_info_and_period(metrics[i], "L_DRAM_ACCESS", MetricFlags_ValFmt_Int, 1, metric_property_none);
    if(i == 8)
      hpcrun_set_metric_info_and_period(metrics[i], "R_DRAM_ACCESS", MetricFlags_ValFmt_Int, 1, metric_property_none);
    if(i == 9)
      hpcrun_set_metric_info_and_period(metrics[i], "L_CACHE_ACCESS", MetricFlags_ValFmt_Int, 1, metric_property_none);
    if(i == 10)
      hpcrun_set_metric_info_and_period(metrics[i], "R_CACHE_ACCESS", MetricFlags_ValFmt_Int, 1, metric_property_none);
    if(i == 11)
      hpcrun_set_metric_info_and_period(metrics[i], "SAMPLE_NUM", MetricFlags_ValFmt_Int, 1, metric_property_none);
    if (lush_metrics == 1) {
      int mid_idleness = hpcrun_new_metric();
      lush_agents->metric_time = metrics[i];
      lush_agents->metric_idleness = mid_idleness;

      hpcrun_set_metric_info_and_period(mid_idleness, "idleness (us)",
				      MetricFlags_ValFmt_Real,
				      period, metric_property_none);
    }
  }

  numa_lat_metric_id = hpcrun_new_metric(); /* metric for latency caused by remote accesses */
  hpcrun_set_metric_info_and_period(numa_lat_metric_id, "NUMA_LATENCY",
                                    MetricFlags_ValFmt_Int,
                                    1, metric_property_none);
  numa_match_metric_id = hpcrun_new_metric(); /* create numa metric id (access matches data location in NUMA node) */
  hpcrun_set_metric_info_and_period(numa_match_metric_id, "NUMA_MATCH",
                                    MetricFlags_ValFmt_Int,
                                    1, metric_property_none);
  numa_mismatch_metric_id = hpcrun_new_metric(); /* create numa metric id (access does not match data location in NUMA node) */
  hpcrun_set_metric_info_and_period(numa_mismatch_metric_id, "NUMA_MISMATCH",
                                    MetricFlags_ValFmt_Int,
                                    1, metric_property_none);
  low_offset_metric_id = hpcrun_new_metric(); /* create data range metric id (low offset) */
  hpcrun_set_metric_info_w_fn(low_offset_metric_id, "LOW_OFFSET",
                                    MetricFlags_ValFmt_Real,
                                    1, hpcrun_metric_min, metric_property_none);
  high_offset_metric_id = hpcrun_new_metric(); /* create data range metric id (high offset) */
  hpcrun_set_metric_info_w_fn(high_offset_metric_id, "HIGH_OFFSET",
                                    MetricFlags_ValFmt_Real,
                                    1, hpcrun_metric_max, metric_property_none);

  // create data location (numa node) metrics
  int numa_node_num = numa_num_configured_nodes();
  location_metric_id = (int *) hpcrun_malloc(numa_node_num * sizeof(int));
  for (i = 0; i < numa_node_num; i++) {
    location_metric_id[i] = hpcrun_new_metric();
    char metric_name[128];
    sprintf(metric_name, "NUMA_NODE%d", i);
    hpcrun_set_metric_info_and_period(location_metric_id[i], strdup(metric_name),
                                       MetricFlags_ValFmt_Int,
                                       1, metric_property_none);
  }

  event = next_tok();
  if (more_tok()) {
    EMSG("MULTIPLE IBS OP events detected! Using first event spec: %s");
  }
}

//
// There is only 1 event for ibs op, hence the event "set" is always the same.
// The signal setup, however, is done here.
//
static void
METHOD_FN(gen_event_set, int lush_metrics)
{
  monitor_sigaction(HPCRUN_PROFILE_SIGNAL, &ibsop_signal_handler, 0, NULL);
}

static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available IBS OP events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("IBS OP\tInstruction based sampling for uops\n");
  printf("\n");
}

/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name ibsop
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

/******************************************************************************
 * private operations 
 *****************************************************************************/

static void
hpcrun_metric_min(int metric_id, metric_set_t* set,
                      hpcrun_metricVal_t incr)
{
  metric_desc_t* minfo = hpcrun_id2metric(metric_id);
  if (!minfo) {
    return;
  }

  hpcrun_metricVal_t* loc = hpcrun_metric_set_loc(set, metric_id);
  switch (minfo->flags.fields.valFmt) {
    case MetricFlags_ValFmt_Int:
      if(loc->i > incr.i || loc->i == 0) loc->i = incr.i; break;
    case MetricFlags_ValFmt_Real:
      if(loc->r > incr.r || loc->r == 0.0) loc->r = incr.r; break;
    default:
      assert(false);
  }
}

static void
hpcrun_metric_max(int metric_id, metric_set_t* set,
                      hpcrun_metricVal_t incr)
{
  metric_desc_t* minfo = hpcrun_id2metric(metric_id);
  if (!minfo) {
    return;
  }

  hpcrun_metricVal_t* loc = hpcrun_metric_set_loc(set, metric_id);
  switch (minfo->flags.fields.valFmt) {
    case MetricFlags_ValFmt_Int:
      if(loc->i < incr.i || loc->i == 0) loc->i = incr.i; break;
    case MetricFlags_ValFmt_Real:
      if(loc->r < incr.r || loc->r == 0.0) loc->r = incr.r; break;
    default:
      assert(false);
  }
}

static int
ibsop_signal_handler(int sig, siginfo_t* siginfo, void* context)
{
  void *ip; // this is the precise IP from IBS
  uint64_t data2, data3;
  uint64_t linear_addr = 0;
  // physical address will be used later for cache conflict analysis
  // uint64_t physical_addr = 0;
  pfarg_msg_t msg;
  ibsopdata2_t *opdata2;
  ibsopdata3_t *opdata3;
  int ret;
  int fd;
  int lat = 0;
  struct over_args *ov;
  cct_node_t *node = NULL;
  void *start = NULL;
  void *end = NULL;
  unsigned long cpu = (unsigned long) TD_GET(core_profile_trace_data).id;

  /* extract thread private data */
  thread_data_t *td = hpcrun_get_thread_data();
  ov = (struct over_args *)td->ibs_ptr;
  fd = siginfo->si_fd;
  if(ov->fd != fd){
    TMSG(IBS_SAMPLE, "fd is NOT the same fd=%d, si_fd=%d", ov->fd, fd);
  }
  int i;
  for(i = 0; i < MAX_FD; i++){
    if(fd2ov[i].fd == fd)
      break;
  }
  ov = &fd2ov[i];
  ov->count++;
  ip = NULL; //init ip

  // Must check for async block first and avoid any MSG if true.
  void* pc = hpcrun_context_pc(context);
  if (!hpcrun_safe_enter_async(pc)) {
    if (ENABLED(IBS_SAMPLE)) {
      ; // message to stderr here for debug
    }
    hpcrun_stats_num_samples_blocked_async_inc();
    if (!hpcrun_is_sampling_disabled()) {
      if(pfm_restart(fd)){
        EMSG("Fail to restart IBS");
        monitor_real_abort();
      }
    }
    /* must call read which is used to restart IBS */
    ret = read(fd, &msg, sizeof(msg));
    return 0;
  }
  else {
    TMSG(IBS_SAMPLE,"op sample event");

    /* read the msr directly to get the performance data */
    data2 = data3 = 0;
    
    ret = read(fd, &msg, sizeof(msg));
    if(msg.type == PFM_MSG_OVFL)
    {
      ov->pd[0].reg_num = 8;
      pfm_read_pmds(fd, ov->pd, 1);
      ip = (void *)ov->pd[0].reg_value;
      TMSG(IBS_SAMPLE, "pc is %p, ip is %p", pc, ip);
      
      ov->pd[0].reg_num = 11;
      pfm_read_pmds(fd, ov->pd, 1);
      data3 = ov->pd[0].reg_value;
      opdata3 = (ibsopdata3_t *)(&data3);
      cct_node_t *data_node = NULL;
      if((opdata3->reg.ibsldop == 1) || (opdata3->reg.ibsstop == 1)) {
	if(ENABLED(DATACENTRIC)) TD_GET(ldst) = 1;
      }
      if(((opdata3->reg.ibsldop == 1) || (opdata3->reg.ibsstop == 1)) && (opdata3->reg.ibsdclinaddrvalid == 1))
      {
	ov->pd[0].reg_num = 12;
  	pfm_read_pmds(fd, ov->pd, 1);
	linear_addr = ov->pd[0].reg_value;
	/* lookup the address in the splay tree, which is used to asscociate IBS samples
	   to allocation samples */
        if(ENABLED(DATACENTRIC)) {
          TD_GET(ea) = (void *)linear_addr;
	  data_node = splay_lookup((void *)linear_addr, &start, &end);
          if (!data_node) {
	    // check the static data
	    load_module_t *lm = hpcrun_loadmap_findByAddr(ip, ip);
	    if(lm && lm->dso_info) {
	      void *static_data_addr = static_data_interval_splay_lookup(&(lm->dso_info->data_root), (void *)linear_addr, &start, &end);
	      if(static_data_addr) {
		TD_GET(lm_id) = lm->id;
		TD_GET(lm_ip) = (uintptr_t)static_data_addr;
   	      }
	      TMSG(IBS_SAMPLE, "loadmap is %s, root begins at %p, ea is %p, static data is %p\n", lm->dso_info->name, lm->dso_info->data_root->start, (void *)linear_addr, static_data_addr);
	    }
          }
        }
      }
      TD_GET(data_node) = data_node;
      TD_GET(pc) = ip;
      TD_GET(start) = start;
      TD_GET(end) = end;
      /* this is the first metric logging routine, must keep it to unwind the call stack */
      node = hpcrun_sample_callpath(context, metrics[11], 1, 0, 0).sample_node;
      TD_GET(start) = NULL;
      TD_GET(end) = NULL;
      TD_GET(data_node) = NULL;
      TD_GET(pc) = NULL;
      TD_GET(ldst) = 0;
      TD_GET(lm_id) = 0;
      TD_GET(lm_ip) = NULL;
      TD_GET(ea) = NULL;

      // compute data range info (offset %)
      if(start && end) {
        float offset = (float)((void *)linear_addr - start)/(end - start);
        if (! hpcrun_has_metric_set(node)) {
          cct2metrics_assoc(node, hpcrun_metric_set_new());
        }
        metric_set_t* set = hpcrun_get_metric_set(node);
        hpcrun_metric_min(low_offset_metric_id, set, (cct_metric_data_t){.r = offset});
        if (end - start <= 8) // one access covers the whole range
          hpcrun_metric_max(high_offset_metric_id, set, (cct_metric_data_t){.r = 1.0});
        else
          hpcrun_metric_max(high_offset_metric_id, set, (cct_metric_data_t){.r = offset});
      }

      // compute numa-related metrics
      int numa_access_node = numa_node_of_cpu(cpu);
      int numa_location_node;
      void *addr = (void *)linear_addr;
      int ret_code = move_pages(0, 1, &addr, NULL, &numa_location_node, 0);
      if(ret_code == 0 && numa_location_node >= 0) {
//printf("data_addr is %p, cpu is %u, numa_access_node is %d, numa_loaction_node is %d\n",data_addr, cpu, numa_access_node, numa_location_node);
        if(numa_access_node == numa_location_node)
          cct_metric_data_increment(numa_match_metric_id, node, (cct_metric_data_t){.i = 1});
        else
          cct_metric_data_increment(numa_mismatch_metric_id, node, (cct_metric_data_t){.i = 1});
        // location metric
        cct_metric_data_increment(location_metric_id[numa_location_node], node, (cct_metric_data_t){.i = 1});
      }

      /* only collect memory access samples */
      if((opdata3->reg.ibsldop == 1) ||
	 (opdata3->reg.ibsstop == 1))
      {
	cct_metric_data_increment(metrics[2], node, (cct_metric_data_t){.i = 1});
	
	/* NB data are avail only when load and dc miss */
	if((opdata3->reg.ibsldop == 1) && (opdata3->reg.ibsdcmiss == 1))
	{
	  ov->pd[0].reg_num = 10;
 	  pfm_read_pmds(fd, ov->pd, 1);
	  data2 = ov->pd[0].reg_value;
	  opdata2 = (ibsopdata2_t *)(&data2);

	  /* read from local DRAM */
	  if((opdata2->reg.nbibsreqsrc == 0x3) && (opdata2->reg.nbibsreqdstproc == 0x0))
	    cct_metric_data_increment(metrics[7], node, (cct_metric_data_t){.i = 1});
	  /* read from remote DRAM */
          if((opdata2->reg.nbibsreqsrc == 0x3) && (opdata2->reg.nbibsreqdstproc == 0x1))
	    cct_metric_data_increment(metrics[8], node, (cct_metric_data_t){.i = 1});
	  /* read from local L3 cache */
          if(((opdata2->reg.nbibsreqsrc == 0x1) && (opdata2->reg.nbibsreqdstproc == 0x0)) ||
	  /* read from L1 or L2 of a local core */
	     ((opdata2->reg.nbibsreqsrc == 0x2) && (opdata2->reg.nbibsreqdstproc == 0x0)))
	    cct_metric_data_increment(metrics[9], node, (cct_metric_data_t){.i = 1});
	  /* read from L1, L2, L3 of a remote core */
          if((opdata2->reg.nbibsreqsrc == 0X2) && (opdata2->reg.nbibsreqdstproc == 0x1))
	    cct_metric_data_increment(metrics[10], node, (cct_metric_data_t){.i = 1});
	}
	if(opdata3->reg.ibsdcmiss == 1)
	  cct_metric_data_increment(metrics[3], node, (cct_metric_data_t){.i = 1});

	if(opdata3->reg.ibsdcl1tlbmiss == 1)
	  cct_metric_data_increment(metrics[5], node, (cct_metric_data_t){.i = 1});

	if(opdata3->reg.ibsdcl2tlbmiss == 1)
	  cct_metric_data_increment(metrics[6], node, (cct_metric_data_t){.i = 1});

	/* log latency */
	if((opdata3->reg.ibsldop == 1) && (opdata3->reg.ibsdcmiss == 1)){
	  lat = opdata3->reg.ibsdcmisslat;
	  if(lat > 0)
	    cct_metric_data_increment(metrics[4], node, (cct_metric_data_t){.i = lat});
            // log numa latency
	    if(((opdata2->reg.nbibsreqsrc == 0x3) && (opdata2->reg.nbibsreqdstproc == 0x1)) || 
	      ((opdata2->reg.nbibsreqsrc == 0X2) && (opdata2->reg.nbibsreqdstproc == 0x1)))
	      cct_metric_data_increment(numa_lat_metric_id, node, (cct_metric_data_t){.i = lat});
	}
      }
      else // this is NOT a memory access, just restart IBS
      {
	TMSG(IBS_SAMPLE, "NOT a memory access");
      }
    }
  }
  if (hpcrun_is_sampling_disabled()) {
    TMSG(IBS_SAMPLE, "No IBS restart, due to disabled sampling");
    hpcrun_safe_exit();
    return 0;
  }

  if(pfm_restart(fd)){
    EMSG("Fail to restart IBS");
    monitor_real_abort();
  }

  TMSG(IBS_SAMPLE, "Done with sample");

  hpcrun_safe_exit();
  return 0; /* tell monitor that the signal has been handled */
}

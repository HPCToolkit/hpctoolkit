// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/sample-sources/itimer.c $
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
#define METRIC_NUM			6
#define DEFAULT_PERIOD			65535L

/******************************************************************************
 * local constants
 *****************************************************************************/

enum _local_const {
  IBS_FETCH_EVENT = 0    // IBS FETCH has only 1 event
};

/******************************************************************************
 * local inline functions
 *****************************************************************************/
inline pid_t
gettid()
{
        return (pid_t)syscall(__NR_gettid);
}

/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static int
ibsfetch_signal_handler(int sig, siginfo_t *siginfo, void *context);
extern void 
metric_add(int metric_id, metric_set_t* set, cct_metric_data_t datum);
//extern void 
//update_bt(backtrace_t*, void*);

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

/* metrics for one sample: 0: Total samples, 1: instruction complete, 
   2: instruction cache miss, 3: L1 TLB miss, 4: L2 TLB miss, 5: Fetch latency */
static int metrics [METRIC_NUM];

static long period = DEFAULT_PERIOD;

// ******* METHOD DEFINITIONS ***********
static void
METHOD_FN(init)
{
  if(pfm_initialize() != PFMLIB_SUCCESS){
    EEMSG("libpfm init NOT ok");
    monitor_real_abort();
  }

  int type;
  pfm_get_pmu_type(&type);
  if(type != PFMLIB_AMD64_PMU){
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
  
  TMSG(IBS_SAMPLE, "starting IBS %d %d", td->id, period);

  ov = &fd2ov[td->id];
  memset(ov, 0, sizeof(struct over_args));

  ov->inp_mod.ibsfetch.maxcnt = period;
  ov->inp_mod.flags |= PFMLIB_AMD64_USE_IBSFETCH;

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
  if (ov->outp_mod.ibsfetch_base != 0) {
    EMSG("Unexpected IBS fetch base register: %d", ov->outp_mod.ibsfetch_base);
    monitor_real_abort();
  }

  /* program the PMU */  
  ov->pc[0].reg_num=ov->outp.pfp_pmcs[0].reg_num;
  ov->pc[0].reg_value=ov->outp.pfp_pmcs[0].reg_value;

  ov->pd[0].reg_num=ov->outp.pfp_pmds[0].reg_num;//pd[0].reg_num=3
  ov->pd[0].reg_value=0;

  ov->pd[0].reg_flags |= PFM_REGFL_OVFL_NOTIFY;

  ov->pd[0].reg_smpl_pmds[0]=((1UL<<3)-1)<<ov->outp.pfp_pmds[0].reg_num;

  /* setup the file descriptor */
  int fd = pfm_create_context(&ov->ctx, NULL, 0, 0);
  if(fd == -1){
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
  TMSG(IBS_SAMPLE, "thread action (noop)");
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
  TMSG(IBS_SAMPLE, "i=%d, count=%d", i, fd2ov[i].count);

  TMSG(IBS_SAMPLE, "stopping IBS fetch");

  TD_GET(ss_state)[self->evset_idx] = STOP;
}

static void
METHOD_FN(shutdown)
{
  TMSG(IBS_SAMPLE, "shutodown IBS fetch");
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
  return (strstr(ev_str,"IBS_FETCH") != NULL);
}
 
static void
METHOD_FN(process_event_list, int lush_metrics)
{

  TMSG(IBS_SAMPLE, "process event list, lush_metrics = %d", lush_metrics);
  // fetch the event string for the sample source
  char* _p = METHOD_CALL(self, get_event_str);
  
  //
  // EVENT: Only 1 IBS_OP event
  //
  char* event = start_tok(_p);

  char name[1024]; // local buffer needed for extract_ev_threshold

  TMSG(IBS_SAMPLE,"checking event spec = %s",event);

  // extract event threshold
  hpcrun_extract_ev_thresh(event, sizeof(name), name, &period, DEFAULT_PERIOD);
  period = period << 4;
  TMSG(IBS_SAMPLE,"IBS_SAMPLE period set to %ld",period);

  // store event threshold
  METHOD_CALL(self, store_event, IBS_FETCH_EVENT, period);

  /* setup all metrics */
  int i;
  for(i = 0; i < METRIC_NUM; i++){
    metrics[i] = hpcrun_new_metric();
    METHOD_CALL(self, store_metric_id, IBS_FETCH_EVENT, metrics[i]);
    TMSG(IBS_SAMPLE,"setting metric%d IBS,period = %ld",i,period);

    if(i == 0)
      hpcrun_set_metric_info_w_fn(metrics[i], "TOTAL_INS",
				  MetricFlags_ValFmt_Int,
				  1, metric_add);
    if(i == 1)
      hpcrun_set_metric_info_w_fn(metrics[i], "COMPLETED_INS",              
                                  MetricFlags_ValFmt_Int,               
                                  1, metric_add);
    if(i == 2)
      hpcrun_set_metric_info_w_fn(metrics[i], "IC_MISS",              
                                  MetricFlags_ValFmt_Int,               
                                  1, metric_add);
    if(i == 3)
      hpcrun_set_metric_info_w_fn(metrics[i], "L1_ITLB_MISS", 
                                  MetricFlags_ValFmt_Int,
                                  1, metric_add);
    if(i == 4)
      hpcrun_set_metric_info_w_fn(metrics[i], "L2_ITLB_MISS", 
                                  MetricFlags_ValFmt_Int,
                                  1, metric_add);
    if(i == 5)
      hpcrun_set_metric_info_w_fn(metrics[i], "INS_FETCH_LAT", 
                                  MetricFlags_ValFmt_Int,
                                  1, metric_add);

    if (lush_metrics == 1) {
      int mid_idleness = hpcrun_new_metric();
      lush_agents->metric_time = metrics[i];
      lush_agents->metric_idleness = mid_idleness;

      hpcrun_set_metric_info_and_period(mid_idleness, "idleness (us)",
				      MetricFlags_ValFmt_Real,
				      period);
    }
  }

  event = next_tok();
  if (more_tok()) {
    EMSG("MULTIPLE IBS_FETCH events detected! Using first event spec: %s");
  }
  // 
  thread_data_t *td = hpcrun_get_thread_data();
  td->eventSet[self->evset_idx] = 0xDEAD;
}

//
// There is only 1 event for ibs fetch, hence the event "set" is always the same.
// The signal setup, however, is done here.
//
static void
METHOD_FN(gen_event_set, int lush_metrics)
{
  monitor_sigaction(HPCRUN_PROFILE_SIGNAL, &ibsfetch_signal_handler, 0, NULL);
}

static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available IBS FETCH events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("IBS FETCH\tInstruction based sampling\n");
  printf("\n");
}

/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name ibsfetch
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

/******************************************************************************
 * private operations 
 *****************************************************************************/

static int
ibsfetch_signal_handler(int sig, siginfo_t* siginfo, void* context)
{
  void *ip; // this is the precise IP from IBS
//  uint64_t linear_addr = 0;
  uint64_t fetchdata;
  pfarg_msg_t msg;
  ibsfetchctl_t *ibsfetchdata;
  int ret;
  int fd;
  struct over_args *ov;
  cct_node_t *node = NULL;

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
  if (hpcrun_async_is_blocked(pc)) {
    hpcrun_stats_num_samples_blocked_async_inc();
    ret = read(fd, &msg, sizeof(msg)); //this is needed to restart IBS
  }
  else {
    fetchdata = 0;
    ret = read(fd, &msg, sizeof(msg));
    if(msg.type == PFM_MSG_OVFL)
    {
      ov->pd[0].reg_num = 5;
      pfm_read_pmds(fd,ov->pd,1);
      ip = (void *)ov->pd[0].reg_value;
      TMSG(IBS_SAMPLE, "real ip is %p", ip);
      ov->pd[0].reg_num = 4;
      pfm_read_pmds(fd, ov->pd, 1);
      fetchdata = ov->pd[0].reg_value;
      ibsfetchdata = (ibsfetchctl_t *)(&fetchdata);
      node = hpcrun_sample_callpath(context, metrics[0], 1, 0, 0);
      assert(node != NULL);

      if(ibsfetchdata->reg.ibsfetchcomp == 1)
      {
//         hpcrun_sample_callpath(context, metrics[1], 1, 0, 0);
	 cct_metric_data_increment(metrics[1], node, (cct_metric_data_t){.i = 1});
      }
      if(ibsfetchdata->reg.ibsicmiss == 1)
      {
//         hpcrun_sample_callpath(context, metrics[2], 1, 0, 0);
	 cct_metric_data_increment(metrics[2], node, (cct_metric_data_t){.i = 1});
      }
      if(ibsfetchdata->reg.ibsl1tlbmiss == 1)
      {
//         hpcrun_sample_callpath(context, metrics[3], 1, 0, 0);
	 cct_metric_data_increment(metrics[3], node, (cct_metric_data_t){.i = 1});
      }
      if(ibsfetchdata->reg.ibsl2tlbmiss == 1)
      {
//         hpcrun_sample_callpath(context, metrics[4], 1, 0, 0);
	 cct_metric_data_increment(metrics[4], node, (cct_metric_data_t){.i = 1});
      }
//      hpcrun_sample_callpath(context, metrics[5], ibsfetchdata->reg.ibsfetchlat, 0, 0);
      cct_metric_data_increment(metrics[5], node, (cct_metric_data_t){.i = ibsfetchdata->reg.ibsfetchlat});
    }
  }

  if (hpcrun_is_sampling_disabled()) {
    TMSG(IBS_SAMPLE, "No IBS restart, due to disabled sampling");
    return 0;
  }

  if(pfm_restart(fd)){
    EMSG("Fail to restart IBS");
    monitor_real_abort();
  }

  return 0; /* tell monitor that the signal has been handled */
}

#if 0
void update_bt(backtrace_t* bt, void* ibs_addr)
{
  if(is_kernel(ibs_addr)){
    hpcrun_bt_add_leaf_child(bt, ibs_addr);
  }
  else{
    hpcrun_bt_modify_leaf_addr(bt, ibs_addr);
  }
}
#endif

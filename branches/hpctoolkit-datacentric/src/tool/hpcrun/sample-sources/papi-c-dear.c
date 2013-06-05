// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/sample-sources/dear.c $
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

#include <hpcrun/datacentric.h>
#include <hpcrun/loadmap.h>

/******************************************************************************
 * special includes from libpfm for DEAR
 *****************************************************************************/
#include <perfmon/perfmon.h>
#include <perfmon/perfmon_default_smpl.h>
#include <perfmon/pfmlib_montecito.h>

/* control the file descriptors */
#include <fcntl.h>

/******************************************************************************
 * macros
 *****************************************************************************/

#define HPCRUN_PROFILE_SIGNAL           SIGIO
#define MAX_FD 				100
#define DEFAULT_PERIOD			(40)

#define M_PMD(x)                (1UL<<(x))
#define DEAR_REGS_MASK          (M_PMD(32)|M_PMD(33)|M_PMD(36))

typedef pfm_default_smpl_hdr_t          dear_hdr_t;
typedef pfm_default_smpl_entry_t        dear_entry_t;
typedef pfm_default_smpl_ctx_arg_t      dear_ctx_t;
#define DEAR_FMT_UUID                   PFM_DEFAULT_SMPL_UUID

#define METRIC_NUM                      3

#if defined(__ECC) && defined(__INTEL_COMPILER)
/* if you do not have this file, your compiler is too old */
#include <ia64intrin.h>

#define hweight64(x)    _m64_popcnt(x)

#elif defined(__GNUC__)

#define NUM_PMCS PFMLIB_MAX_PMCS
#define NUM_PMDS PFMLIB_MAX_PMDS
static __inline__ int
hweight64 (unsigned long x)
{
        unsigned long result;
        __asm__ ("popcnt %0=%1" : "=r" (result) : "r" (x));
        return (int)result;
}

#else
#error "you need to provide inline assembly from your compiler"
#endif

/******************************************************************************
 * local constants
 *****************************************************************************/

static pfm_uuid_t buf_fmt_id = DEAR_FMT_UUID;

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

static int
dear_signal_handler(int sig, siginfo_t *siginfo, void *context);

/******************************************************************************
 * local variables
 *****************************************************************************/
static sigset_t sigset_dear;

struct over_args {
        pfmlib_input_param_t inp;
        pfmlib_output_param_t outp;
        pfarg_reg_t pc[NUM_PMCS];
        pfarg_reg_t pd[NUM_PMDS];
        dear_ctx_t ctx;
        pfarg_load_t load_args;
        int fd;
        pid_t tid;
        pthread_t self;
        unsigned long entry_size;
        int count;
};

struct over_args fd2ov[MAX_FD];

static int metrics [METRIC_NUM];

static int low_offset_metric_id = -1;
static int high_offset_metric_id = -1;

static long period = DEFAULT_PERIOD;

pfmlib_event_t ev;

// ******* METHOD DEFINITIONS ***********

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

static void
METHOD_FN(init)
{
  if(pfm_initialize() != PFMLIB_SUCCESS){
    EEMSG("libpfm init NOT ok");
    monitor_real_abort();
  }

  int type;
  pfm_get_pmu_type(&type);
  if(type != PFMLIB_MONTECITO_PMU){
    EEMSG("CPU is not MONTECITO");
    monitor_real_abort();
  }

  TMSG(DEAR_SAMPLE, "setting up dear interrupt");
  sigemptyset(&sigset_dear);
  sigaddset(&sigset_dear, HPCRUN_PROFILE_SIGNAL);

  int ret = monitor_real_sigprocmask(SIG_UNBLOCK, &sigset_dear, NULL);

  if (ret){
    EMSG("WARNING: Thread init could not unblock SIGIO, ret = %d",ret);
  }
  self->state = INIT;
}

static void
METHOD_FN(thread_init)
{
  TMSG(DEAR_SAMPLE, "thread init (no action needed)");
}

static void
METHOD_FN(thread_init_action)
{
  TMSG(DEAR_SAMPLE, "thread action (noop)");
}

static void
METHOD_FN(start)
{
  int ret;
  struct over_args *ov;
  thread_data_t *td;

  if (! hpcrun_td_avail()){
    TMSG(DEAR_SAMPLE, "Thread data unavailable ==> sampling suspended");
    return; // in the unlikely event that we are trying to start, but thread data is unavailable,
            // assume that all sample source ops are suspended.
  }
  td = hpcrun_get_thread_data();
  
  TMSG(DEAR_SAMPLE, "starting IBS %d %d", td->core_profile_trace_data.id, period);

  ov = &fd2ov[td->core_profile_trace_data.id];
  memset(ov, 0, sizeof(struct over_args));

  ov->inp.pfp_dfl_plm = PFM_PLM3;
  ov->inp.pfp_event_count = 1;
  ov->inp.pfp_events[0] = ev;

  ret = pfm_dispatch_events(&ov->inp, NULL, &ov->outp, NULL);
  if(ret != PFMLIB_SUCCESS){
    EMSG("Cannot dispatch events: %s", pfm_strerror(ret));
    monitor_real_abort();
  }

  memcpy(ov->ctx.ctx_arg.ctx_smpl_buf_id, buf_fmt_id, sizeof(pfm_uuid_t));

  ov->ctx.buf_arg.buf_size = getpagesize()/24;

  /* setup the file descriptor */
  if(perfmonctl(0, PFM_CREATE_CONTEXT, &ov->ctx, 1) == -1) {
    if(errno == ENOSYS) {
      EMSG("the kernel does not have performance monitoring support");
      hpcrun_ssfail_start("DEAR");
    }
    EMSG("cannot create PFM context");
    hpcrun_ssfail_start("DEAR");
  }
  ov->fd = ov->ctx.ctx_arg.ctx_fd;

  int i;
  for(i = 0; i < ov->outp.pfp_pmc_count; i++) {
    ov->pc[i].reg_num = ov->outp.pfp_pmcs[i].reg_num;
    ov->pc[i].reg_value = ov->outp.pfp_pmcs[i].reg_value;
  }

  for(i = 0; i < ov->outp.pfp_pmd_count; i++) {
    ov->pd[i].reg_num = ov->outp.pfp_pmds[i].reg_num;
  }

  ov->pc[0].reg_flags |= PFM_REGFL_OVFL_NOTIFY;

  ov->pc[0].reg_smpl_pmds[0] = DEAR_REGS_MASK;

  ov->entry_size = sizeof(dear_entry_t) + (hweight64(DEAR_REGS_MASK)<<3);

  ov->pd[0].reg_value       = - period;
  ov->pd[0].reg_long_reset  = - period;
  ov->pd[0].reg_short_reset = - period;
  /* write performance counters */
  if (perfmonctl(ov->fd, PFM_WRITE_PMCS, ov->pc, ov->outp.pfp_pmc_count)) {
    EMSG("error in writing pmc");
    hpcrun_ssfail_start("DEAR");
  }
  
  if (perfmonctl(ov->fd, PFM_WRITE_PMDS, ov->pd, ov->outp.pfp_pmd_count)) {
    EMSG("error in writing pmd");
    hpcrun_ssfail_start("DEAR");
  }
  /* record thrad tid */
  ov->load_args.load_pid = gettid();

  /* load context */
  if(perfmonctl(ov->fd, PFM_LOAD_CONTEXT, &ov->load_args, 1) != 0){
    EMSG("error in load");
    hpcrun_ssfail_start("IBS");
  }
  
  /* configure file descriptor to make sure the samples
     go to the correct thread */
  ret = fcntl(ov->fd, F_SETFL, fcntl(ov->fd, F_GETFL, 0) | O_ASYNC);
  if (ret == -1){
    EMSG("cannot set ASYNC");
    hpcrun_ssfail_start("DEAR");
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
      hpcrun_ssfail_start("DEAR");
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
    TMSG(DEAR_SAMPLE, "--stop called on an already stopped measurement");
    return;
  }

  if (my_state != START) {
    TMSG(DEAR_SAMPLE, "*WARNING* Stop called on IBS that has not been started");
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
  TMSG(DEAR_SAMPLE, "fd=%d, i=%d, count=%d", ov->fd, i, fd2ov[i].count);

  TMSG(DEAR_SAMPLE, "stopping IBS op");

  TD_GET(ss_state)[self->evset_idx] = STOP;
}

static void
METHOD_FN(shutdown)
{
  TMSG(DEAR_SAMPLE, "shutodown IBS op");
  METHOD_CALL(self, stop); // make sure stop has been called

  int ret = monitor_real_sigprocmask(SIG_BLOCK, &sigset_dear, NULL);
  if (ret){
    EMSG("WARNING: process fini could not block SIGIO, ret = %d",ret);
  }

  self->state = UNINIT;
}

static bool
METHOD_FN(supports_event, const char *ev_str)
{
  return (strstr(ev_str,"data_ear") != NULL);
}
 
static void
METHOD_FN(process_event_list, int lush_metrics)
{

  TMSG(DEAR_SAMPLE, "process event list, lush_metrics = %d", lush_metrics);
  // fetch the event string for the sample source
  char* evlist = METHOD_CALL(self, get_event_str);
  
  //
  // EVENT: Only 1 DEAR event
  //
  char* event = start_tok(evlist);

  char name[1024]; // local buffer needed for extract_ev_threshold

  TMSG(DEAR_SAMPLE,"checking event spec = %s",event);

  // extract event threshold
  hpcrun_extract_ev_thresh(event, sizeof(name), name, &period, DEFAULT_PERIOD);
  TMSG(DEAR_SAMPLE,"DEAR_SAMPLE period set to %ld",period);
  
  if(pfm_find_full_event(name, &ev) != PFMLIB_SUCCESS) {
    EMSG("the event is not supported");
    hpcrun_ssfail_start("DEAR");
  }

  // store event threshold
//  METHOD_CALL(self, store_event, IBS_OP_EVENT, period);

  /* setup all metrics */
  metrics[0] = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(metrics[0], strdup(name), MetricFlags_ValFmt_Int, period, metric_property_none);
  metrics[1] = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(metrics[1], "LATENCY", MetricFlags_ValFmt_Int, 1, metric_property_none);
  metrics[2] = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(metrics[2], "SAMPLE_NUM", MetricFlags_ValFmt_Int, 1, metric_property_none);

  low_offset_metric_id = hpcrun_new_metric(); /* create data range metric id (low offset) */
  hpcrun_set_metric_info_w_fn(low_offset_metric_id, "LOW_OFFSET",
                                    MetricFlags_ValFmt_Real,
                                    1, hpcrun_metric_min, metric_property_none);
  high_offset_metric_id = hpcrun_new_metric(); /* create data range metric id (high offset) */
  hpcrun_set_metric_info_w_fn(high_offset_metric_id, "HIGH_OFFSET",
                                    MetricFlags_ValFmt_Real,
                                    1, hpcrun_metric_max, metric_property_none);
  event = next_tok();
  if (more_tok()) {
    EMSG("MULTIPLE IBS OP events detected! Using first event spec: %s");
  }
}

static void
METHOD_FN(gen_event_set, int lush_metrics)
{
  monitor_sigaction(HPCRUN_PROFILE_SIGNAL, &dear_signal_handler, 0, NULL);
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

#define ss_name dear
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

/******************************************************************************
 * private operations 
 *****************************************************************************/

static int
dear_signal_handler(int sig, siginfo_t* siginfo, void* context)
{
  void *ip; // this is the precise IP from DEAR
  uint64_t linear_addr = 0; // effetive address from DEAR
  int ret;
  int fd;
  int lat = 0; // latency from DEAR
  struct over_args *ov;
  cct_node_t *node = NULL;
  cct_node_t *data_node = NULL;
  dear_hdr_t *hdr;
  dear_entry_t *ent;
  pfm_mont_pmd_reg_t *reg;
  size_t count;
  unsigned long last_ovfl = ~0UL;
  unsigned long pos;
  pfarg_msg_t msg;
  void *start = NULL;
  void *end = NULL;

  /* extract thread private data */
  thread_data_t *td = hpcrun_get_thread_data();
  ov = (struct over_args *)td->ibs_ptr;
  fd = siginfo->si_fd;
  if(ov->fd != fd){
    TMSG(DEAR_SAMPLE, "fd is NOT the same fd=%d, si_fd=%d", ov->fd, fd);
  }
  // it's necessary to add this read to avoid overflow wedge
  read(fd, &msg, sizeof(msg));
  if(msg.type != PFM_MSG_OVFL) {
    if(perfmonctl(fd, PFM_RESTART, NULL, 0)) {
      EMSG("Fail to restart DEAR");
      monitor_real_abort();
    }
  }

  int i;
  for(i = 0; i < MAX_FD; i++){
    if(fd2ov[i].fd == fd)
      break;
  }
  ov = &fd2ov[i];
  ov->count++;
  ip = NULL; //init ip

  hdr = (dear_hdr_t *)ov->ctx.ctx_arg.ctx_smpl_vaddr;
  if (hdr->hdr_overflows <= last_ovfl && last_ovfl != ~0UL) {
    if(perfmonctl(fd, PFM_RESTART, NULL, 0)) {
      EMSG("Fail to restart DEAR");
      monitor_real_abort();
    }
    return 0;
  }
  // Must check for async block first and avoid any MSG if true.
  void* pc = hpcrun_context_pc(context);
  if (!hpcrun_safe_enter_async(pc)) {
    if (ENABLED(DEAR_SAMPLE)) {
      ; // message to stderr here for debug
    }
    hpcrun_stats_num_samples_blocked_async_inc();
    if (!hpcrun_is_sampling_disabled()) {
      if(perfmonctl(fd, PFM_RESTART, NULL, 0)){
        EMSG("Fail to restart DEAR");
        monitor_real_abort();
      }
    }
    return 0;
  }
  else {
    TMSG(DEAR_SAMPLE,"dear sample event");
    pos = (unsigned long)(hdr+1);
    count = hdr->hdr_count;
    if(count != 1) EMSG("check that count is not 1");
    TMSG(DEAR_SAMPLE, "count is %d", (int)count);

    // the count should always be 1
    while(count--) {
      ret = 0;
      ent = (dear_entry_t *)pos;
      reg = (pfm_mont_pmd_reg_t *)(ent+1);
      TMSG(DEAR_SAMPLE, "PMD32: 0x%016lx", reg->pmd32_mont_reg.dear_daddr);
      linear_addr = reg->pmd32_mont_reg.dear_daddr;
      reg++;
      TMSG(DEAR_SAMPLE, "PMD33: 0x%016lx, latency %u",
                            reg->pmd_val,
                            reg->pmd33_mont_reg.dear_latency);
      lat = reg->pmd33_mont_reg.dear_latency;
      reg++;
      TMSG(DEAR_SAMPLE, "PMD36: 0x%016lx, valid %c, address 0x%016lx",
                                reg->pmd_val,
                                reg->pmd36_mont_reg.dear_vl ? 'Y': 'N',
                                (reg->pmd36_mont_reg.dear_iaddr << 4) |
                                (unsigned long)reg->pmd36_mont_reg.dear_slot);
      if(reg->pmd36_mont_reg.dear_vl)
        ip = (void *)((reg->pmd36_mont_reg.dear_iaddr << 4) | (unsigned long)reg->pmd36_mont_reg.dear_slot);

      pos += ov->entry_size;

    }
    if (ENABLED(DATACENTRIC)) {
      TD_GET(ldst) = 1;
      TD_GET(ea) = (void *)linear_addr;
      data_node = splay_lookup((void *)linear_addr, &start, &end);
      if(!data_node) {
        // check the static data
	load_module_t *lm = hpcrun_loadmap_findByAddr(ip, ip);
        if(lm && lm->dso_info) {
          void *static_data_addr = static_data_interval_splay_lookup(&(lm->dso_info->data_root), (void *)linear_addr, &start, &end);
	  if(static_data_addr) {
            TD_GET(lm_id) = lm->id;
	    TD_GET(lm_ip) = (uintptr_t)static_data_addr;
          }
          else {
	    // cannot map back to variables
	  }
        }
      }
    }
    TD_GET(data_node) = data_node;
    TD_GET(pc) = ip;
    node = hpcrun_sample_callpath(context, metrics[0], 1, 0, 0).sample_node;
    TD_GET(data_node) = NULL;
    TD_GET(pc) = NULL;
    TD_GET(ldst) = 0;
    TD_GET(lm_id) = 0;
    TD_GET(lm_ip) = NULL;
    TD_GET(ea) = NULL;

    // compute data range info (offset %)
    void *data_addr = (void *)linear_addr;
    if(start && end) {
      float offset = (float)(data_addr - start)/(end - start);
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


    cct_metric_data_increment(metrics[1], node, (cct_metric_data_t){.i = lat});
    cct_metric_data_increment(metrics[2], node, (cct_metric_data_t){.i = 1});
  }
  if (hpcrun_is_sampling_disabled()) {
    TMSG(DEAR_SAMPLE, "No IBS restart, due to disabled sampling");
    hpcrun_safe_exit();
    return 0;
  }

  if(perfmonctl(fd, PFM_RESTART, NULL, 0)) {
    EMSG("Fail to restart IBS");
    monitor_real_abort();
  }

  TMSG(DEAR_SAMPLE, "Done with sample");

  hpcrun_safe_exit();
  return 0; /* tell monitor that the signal has been handled */
}

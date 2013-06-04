// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/sample-sources/pebs.c $
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
// data-centric sample source for PEBS (intel core2). Copy this file to papi-c.c
// when use data-centric analysis running on Intel core2 with PEBS 
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
#include <numa.h>
#include <numaif.h>
#include <hpcrun/loadmap.h>

/******************************************************************************
 * special includes from libpfm for PEBS (Intel Core/Atom processors)
 *****************************************************************************/
#include <perfmon/perfmon.h>
#include <perfmon/perfmon_pebs_core_smpl.h>

#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_core.h>
#include <perfmon/pfmlib_intel_atom.h>

#include <hpcrun/unwind/x86-family/x86-comp-ea.h>
/* control the file descriptors */
#include <fcntl.h>

#include <sys/mman.h>
/******************************************************************************
 * macros
 *****************************************************************************/

#define HPCRUN_PROFILE_SIGNAL           SIGIO
#define MAX_FD 				100
#define DEFAULT_PERIOD			(100000ULL)
#define NUM_PMCS        16
#define NUM_PMDS        16

typedef pfm_pebs_core_smpl_hdr_t        smpl_hdr_t;
typedef pfm_pebs_core_smpl_entry_t      smpl_entry_t;
typedef pfm_pebs_core_smpl_arg_t        smpl_arg_t;
#define FMT_NAME                        PFM_PEBS_CORE_SMPL_NAME

#define METRIC_NUM                      3


/******************************************************************************
 * local constants
 *****************************************************************************/


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
pebs_signal_handler(int sig, siginfo_t *siginfo, void *context);

/******************************************************************************
 * local variables
 *****************************************************************************/
static sigset_t sigset_pebs;

struct over_args {
        pfmlib_input_param_t inp;
        pfmlib_output_param_t outp;
 	pfmlib_core_input_param_t mod_inp;
        pfarg_pmc_t pc[NUM_PMCS];
        pfarg_pmd_t pd[NUM_PMDS];
        pfarg_ctx_t ctx;
	smpl_arg_t buf_arg;
        pfarg_load_t load_args;
        int fd;
        pid_t tid;
        pthread_t self;
        int count;
};

struct over_args fd2ov[MAX_FD];

static int metrics [METRIC_NUM];

static long period = DEFAULT_PERIOD;

pfmlib_event_t ev;

static int numa_match_metric_id = -1;
static int numa_mismatch_metric_id = -1;

static int low_offset_metric_id = -1;
static int high_offset_metric_id = -1;

static int *location_metric_id;

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
  if(type != PFMLIB_INTEL_CORE_PMU && type != PFMLIB_INTEL_ATOM_PMU){
    EEMSG("CPU is not Intel Core or Atom processor");
    monitor_real_abort();
  }

  TMSG(DEAR_SAMPLE, "setting up pebs interrupt");
  sigemptyset(&sigset_pebs);
  sigaddset(&sigset_pebs, HPCRUN_PROFILE_SIGNAL);

  int ret = monitor_real_sigprocmask(SIG_UNBLOCK, &sigset_pebs, NULL);

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
  ov->mod_inp.pfp_core_pebs.pebs_used = 1;

  ret = pfm_dispatch_events(&ov->inp, &ov->mod_inp, &ov->outp, NULL);
  if(ret != PFMLIB_SUCCESS){
    EMSG("Cannot dispatch events: %s", pfm_strerror(ret));
    monitor_real_abort();
  }

  ov->buf_arg.buf_size = getpagesize()/10;

//  ov->buf_arg.cnt_reset = -period;
//  ov->buf_arg.intr_thres = (ov->buf_arg.buf_size/sizeof(smpl_entry_t))*90/100;

  ov->fd = pfm_create_context(&ov->ctx, FMT_NAME, &ov->buf_arg, sizeof(ov->buf_arg));
  /* setup the file descriptor */
  if(ov->fd == -1) {
    if(errno == ENOSYS) {
      EMSG("the kernel does not have performance monitoring support");
      hpcrun_ssfail_start("PEBS");
    }
    EMSG("cannot create PFM context");
    hpcrun_ssfail_start("PEBS");
  }

  int i;
  for(i = 0; i < ov->outp.pfp_pmc_count; i++) {
    ov->pc[i].reg_num = ov->outp.pfp_pmcs[i].reg_num;
    ov->pc[i].reg_value = ov->outp.pfp_pmcs[i].reg_value;
    
    if(ov->pc[i].reg_num == 0)
      ov->pc[i].reg_flags = PFM_REGFL_NO_EMUL64;
  }

  for(i = 0; i < ov->outp.pfp_pmd_count; i++) {
    ov->pd[i].reg_num = ov->outp.pfp_pmds[i].reg_num;
  }

  ov->pd[0].reg_flags |= PFM_REGFL_OVFL_NOTIFY;
  ov->pd[0].reg_value       = - period;
  ov->pd[0].reg_long_reset  = - period;
  ov->pd[0].reg_short_reset = - period;
  /* write performance counters */
  if (pfm_write_pmcs(ov->fd, ov->pc, ov->outp.pfp_pmc_count)) {
    EMSG("error in writing pmc");
    hpcrun_ssfail_start("PEBS");
  }
  
  if (pfm_write_pmds(ov->fd, ov->pd, ov->outp.pfp_pmd_count)) {
    EMSG("error in writing pmd");
    hpcrun_ssfail_start("PEBS");
  }
  /* record thrad tid */
  ov->load_args.load_pid = gettid();

  /* load context */
  if(pfm_load_context(ov->fd, &ov->load_args) == -1){
    EMSG("error in load");
    hpcrun_ssfail_start("IBS");
  }
  
  /* configure file descriptor to make sure the samples
     go to the correct thread */
  ret = fcntl(ov->fd, F_SETFL, fcntl(ov->fd, F_GETFL, 0) | O_ASYNC);
  if (ret == -1){
    EMSG("cannot set ASYNC");
    hpcrun_ssfail_start("PEBS");
  }

#if 0
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
      hpcrun_ssfail_start("PEBS");
    }
  }
#endif

  ret = fcntl(ov->fd, F_SETOWN, getpid());
  if (ret == -1) {
    EMSG("cannot set own");
    hpcrun_ssfail_start("PEBS");
  }
  ret = fcntl(ov->fd, F_SETSIG, SIGIO);
  if (ret < 0){
    EMSG("cannot set SIGIO");
    hpcrun_ssfail_start("PEBS");
  }

  if(pfm_start(ov->fd, NULL) == -1){
    EMSG("start error");
    hpcrun_ssfail_start("PEBS");
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
  if(pfm_stop(ov->fd) == -1){
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

  int ret = monitor_real_sigprocmask(SIG_BLOCK, &sigset_pebs, NULL);
  if (ret){
    EMSG("WARNING: process fini could not block SIGIO, ret = %d",ret);
  }

  self->state = UNINIT;
}

static bool
METHOD_FN(supports_event, const char *ev_str)
{
  return (strstr(ev_str,":") != NULL);
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
    hpcrun_ssfail_start("PEBS");
  }

  // store event threshold
//  METHOD_CALL(self, store_event, IBS_OP_EVENT, period);

  /* setup all metrics */
  metrics[0] = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(metrics[0], strdup(name), MetricFlags_ValFmt_Int, period, metric_property_none);
  metrics[1] = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(metrics[1], "LOADS", MetricFlags_ValFmt_Int, period, metric_property_none);
  metrics[2] = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(metrics[2], "STORES", MetricFlags_ValFmt_Int, period, metric_property_none);

  numa_match_metric_id = hpcrun_new_metric(); /* create numa metric id (access matches data location in NUMA node) */
  hpcrun_set_metric_info_and_period(numa_match_metric_id, "NUMA_MATCH",
                                    MetricFlags_ValFmt_Int,
                                    period, metric_property_none);
  numa_mismatch_metric_id = hpcrun_new_metric(); /* create numa metric id (access does not match data location in NUMA node) */
  hpcrun_set_metric_info_and_period(numa_mismatch_metric_id, "NUMA_MISMATCH",
                                    MetricFlags_ValFmt_Int,
                                    period, metric_property_none);
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
  int i;
  for (i = 0; i < numa_node_num; i++) {
    location_metric_id[i] = hpcrun_new_metric();
    char metric_name[128];
    sprintf(metric_name, "NUMA_NODE%d", i);
    hpcrun_set_metric_info_and_period(location_metric_id[i], strdup(metric_name),
                                       MetricFlags_ValFmt_Int,
                                       period, metric_property_none);
  }

  event = next_tok();
  if (more_tok()) {
    EMSG("MULTIPLE IBS OP events detected! Using first event spec: %s");
  }
}

static void
METHOD_FN(gen_event_set, int lush_metrics)
{
  monitor_sigaction(HPCRUN_PROFILE_SIGNAL, &pebs_signal_handler, 0, NULL);
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

#define ss_name pebs 
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

/******************************************************************************
 * private operations 
 *****************************************************************************/

static int
pebs_signal_handler(int sig, siginfo_t* siginfo, void* context)
{
  void *ip; // this is the precise IP from PEBS
  uint64_t linear_addr = 0; // effetive address from PEBS
  int fd;
  struct over_args *ov;
  cct_node_t *node = NULL;
  cct_node_t *data_node = NULL;
  smpl_hdr_t *hdr;
  smpl_entry_t *ent;
  size_t count;
  unsigned long last_ovfl = ~0UL;
  pfarg_msg_t msg;
  int ls = 0; // 1 means load, 2 means store
  void *start = NULL;
  void *end = NULL;
  unsigned long cpu = (unsigned long) TD_GET(core_profile_trace_data).id;

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
    if(pfm_restart(fd)) {
      EMSG("Fail to restart DEAR");
      monitor_real_abort();
    }
    return 0;
  }

  int i;
  for(i = 0; i < MAX_FD; i++){
    if(fd2ov[i].fd == fd)
      break;
  }
  ov = &fd2ov[i];
  ov->count++;
  ip = NULL; //init ip

  void *buf_addr = mmap(NULL, (size_t)ov->buf_arg.buf_size, PROT_READ, MAP_PRIVATE, ov->fd, 0);
  if (buf_addr == MAP_FAILED) EMSG("Fail to mmap PEBS");
  hdr = (smpl_hdr_t *)buf_addr;
  if (hdr->overflows == last_ovfl) {
    if(pfm_restart(fd)) {
      EMSG("Fail to restart PEBS");
      monitor_real_abort();
    }
    munmap(buf_addr, (size_t)ov->buf_arg.buf_size);
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
      if(pfm_restart(fd)){
        EMSG("Fail to restart PEBS");
        monitor_real_abort();
      }
    }
    munmap(buf_addr, (size_t)ov->buf_arg.buf_size);
    return 0;
  }
  else {
    TMSG(DEAR_SAMPLE,"pebs sample event");
    count = (hdr->ds.pebs_index - hdr->ds.pebs_buf_base)/sizeof(*ent);
    if(count != 1) EMSG("check that count is not 1");
    TMSG(DEAR_SAMPLE, "count is %d", (int)count);
    ent   = (smpl_entry_t *)((unsigned long)(hdr+1)+ hdr->start_offs);

    // the count should always be 1
    while(count--) {
      ip = pebs_off_one_fix((void *)ent->ip);
      if(ip) linear_addr = compute_effective_addr(ip, (void *)ent, &ls);

      ent++;
    }
    if (ENABLED(DATACENTRIC) && (ls > 0)) {
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
    TD_GET(pc) = ip; // fix pebs off by 1 issue
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

    // compute numa-related metrics
    int numa_access_node = numa_node_of_cpu(cpu);
    int numa_location_node;
    void *addr = data_addr;
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
  }
  if(ls == 1)
    cct_metric_data_increment(metrics[1], node, (cct_metric_data_t){.i = 1});
  if(ls == 2)
    cct_metric_data_increment(metrics[2], node, (cct_metric_data_t){.i = 1});
  if (hpcrun_is_sampling_disabled()) {
    TMSG(DEAR_SAMPLE, "No IBS restart, due to disabled sampling");
    hpcrun_safe_exit();
    munmap(buf_addr, (size_t)ov->buf_arg.buf_size);
    return 0;
  }

  if(pfm_restart(fd)) {
    EMSG("Fail to restart IBS");
    monitor_real_abort();
  }

  TMSG(DEAR_SAMPLE, "Done with sample");

  munmap(buf_addr, (size_t)ov->buf_arg.buf_size);
  hpcrun_safe_exit();
  return 0; /* tell monitor that the signal has been handled */
}

// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/ibs.c $
// $Id: ibs.c 2600 2009-11-9 22:22:41Z Xu Liu $
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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
// ibs sample source simple oo interface
//

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>

#include <signal.h>
#include <sys/time.h>          
#include <ucontext.h>           /* struct ucontext */

#include <ctype.h>


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
#include "sample_source_obj.h"
#include <hpcrun/thread_data.h>

#include <lush/lush-backtrace.h>
#include <messages/messages.h>

#include <utilities/tokenize.h>

#include <lib/prof-lean/timer.h>
#include <unwind/common/unwind.h>

#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_amd64.h>
 
#include <perfmon/perfmon.h>
#include <perfmon/perfmon_dfl_smpl.h>
#include <fcntl.h>

/******************************************************************************
 * macros
 *****************************************************************************/

#   define HPCRUN_PROFILE_SIGNAL           SIGIO
#   define MAX_FD                          20
#   define METRIC_NUM                      4 //at most 4 counters
#   define COUNTER_NUM                      4 //at most 4 counters
#   define MIN(a,b)                        (((a)<=(b))?(a):(b))
#   define DEFAULT_TH                      1000000L

/******************************************************************************
 * local constants
 *****************************************************************************/
struct {
  char full_name[1024];
  int c_mask;
  int inv;
}cycle_events[COUNTER_NUM];
/******************************************************************************
 * local inline functions 
 *****************************************************************************/

/*
inline pid_t
gettid(void)
{
        return (pid_t)syscall(__NR_gettid);
}

*/
/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static int
cycle_signal_handler(int sig, siginfo_t *siginfo, void *context);

void
extract_ev_thresh_cycle(const char *in, int evlen, char *ev, long *th, int *c_mask, int *inv);

/******************************************************************************
 * local variables
 *****************************************************************************/

static sigset_t sigset_cycle;//sigset_ibs;

struct over_args {
        pfmlib_amd64_input_param_t inp_mod;                                              
        pfmlib_input_param_t inp; 
        pfmlib_output_param_t outp;                                            
        pfarg_pmc_t pc[PFMLIB_MAX_PMCS];                                                 
        pfarg_pmd_t pd[PFMLIB_MAX_PMDS];                                                 
        pfarg_ctx_t ctx;
        pfarg_load_t load_args;                                                          
        int fd;
        pid_t tid;
        pthread_t self;
	int count;                                                                
};                                                                                  
 
struct over_args fd2ov[MAX_FD];    

//static int metrics [METRIC_NUM];


// ******* METHOD DEFINITIONS ***********

// init function for cycle accounting: check CPU type
static void
METHOD_FN(init)
{
  int ret, type;
  ret = pfm_initialize();
  if (ret != PFMLIB_SUCCESS){
    STDERR_MSG("Fatal error: initialization with perfmon2 error.");
    exit(1);
}
  pfm_get_pmu_type(&type);
  if(type != PFMLIB_AMD64_PMU){ //for now, just support cycle accounting on AMD64
    STDERR_MSG("Fatal error: CPU type is not AMD64.");
    exit(1);
  }
  self->state = INIT;
}

static void
METHOD_FN(start)
{
  int ret;
  char event_name[1024];
  struct over_args *ov;
  thread_data_t *td1 = hpcrun_get_thread_data();

  if (! hpcrun_td_avail()){
    return; // in the unlikely event that we are trying to start, but thread data is unavailable,
            // assume that all sample source ops are suspended.
  }
  
  TMSG(IBS_SAMPLE,"starting cycle accounting sampling %d",td1->id);
  int nevents  = self->evl.nevents; //number of all events
  ov=&fd2ov[td1->id];
  memset(ov,0,sizeof(struct over_args));

  int i;
  size_t max_len;
  unsigned int desc;
  pfm_get_max_event_name_len(&max_len);
  for (i = 0; i<nevents; i++)
  {
    pfm_find_event_bycode (self->evl.events[i].event, &desc);
    pfm_get_event_name(desc, event_name, 1024);
    TMSG(IBS_SAMPLE,"start: event name is %s", cycle_events[i].full_name);//event_name);
    if(pfm_find_full_event(cycle_events[i].full_name, &ov->inp.pfp_events[i])!= PFMLIB_SUCCESS)
    {
      EMSG("find full event is wrong");
      exit(1);
    }
    /* these two steps set cycle accounting */
    if (cycle_events[i].inv == 1)
      ov->inp_mod.pfp_amd64_counters[i].flags |= PFM_AMD64_SEL_INV;
    if (cycle_events[i].c_mask > 0)
      ov->inp_mod.pfp_amd64_counters[i].cnt_mask = cycle_events[i].c_mask;
  }
  ov->inp.pfp_dfl_plm   = PFM_PLM3 | PFM_PLM0;
  ov->inp.pfp_event_count = nevents;

  ret=pfm_dispatch_events(&ov->inp, &ov->inp_mod, &ov->outp, NULL);
  if (ret != PFMLIB_SUCCESS) {
    EMSG("cannot dispatch events: %s", pfm_strerror(ret));
    exit(1);
  }

  for (i=0; i < ov->outp.pfp_pmc_count; i++) {
    ov->pc[i].reg_num   = ov->outp.pfp_pmcs[i].reg_num;
    ov->pc[i].reg_value = ov->outp.pfp_pmcs[i].reg_value;
  }
 
  for (i=0; i < ov->outp.pfp_pmd_count; i++) {
    TMSG(IBS_SAMPLE, "set pd, perio= %ld", self->evl.events[i].thresh);
    ov->pd[i].reg_num   = ov->outp.pfp_pmds[i].reg_num;
    ov->pd[i].reg_flags |= PFM_REGFL_OVFL_NOTIFY;
    ov->pd[i].reg_value = - self->evl.events[i].thresh;
    ov->pd[i].reg_long_reset = - self->evl.events[i].thresh;
    ov->pd[i].reg_short_reset = - self->evl.events[i].thresh;
  }

  int fd = pfm_create_context(&ov->ctx, NULL, 0, 0);
  if(fd==-1){
    EMSG("error in create context");
    hpcrun_ssfail_start("CYCLE");
  }
  ov->fd = fd;
  if(pfm_write_pmcs(ov->fd,ov->pc,ov->outp.pfp_pmc_count)){
    EMSG("error in write pmc");
    hpcrun_ssfail_start("CYCLE");
  }
 
  if(pfm_write_pmds(ov->fd,ov->pd,ov->outp.pfp_pmd_count)){
    EMSG("error in write pmd");
    hpcrun_ssfail_start("CYCLE");
  }
 
  ov->load_args.load_pid=gettid();
 
  if(pfm_load_context(ov->fd, &ov->load_args)){
    EMSG("error in load");
    hpcrun_ssfail_start("CYCLE");
  }
 
  ret = fcntl(ov->fd, F_SETFL, fcntl(ov->fd, F_GETFL, 0) | O_ASYNC);
  if (ret == -1){
    EMSG("cannot set ASYNC");
    hpcrun_ssfail_start("CYCLE");
  }
  ret = fcntl(ov->fd, F_SETOWN, gettid());
  if (ret == -1){
    EMSG("cannot set ownership");
    hpcrun_ssfail_start("CYCLE");
  }
  ret = fcntl(ov->fd, F_SETSIG, SIGIO);
  if (ret < 0){
    EMSG("cannot set SIGIO");
    hpcrun_ssfail_start("CYCLE");
  }
 
  if(pfm_self_start(ov->fd)==-1){
    EMSG("start error");
    hpcrun_ssfail_start("CYCLE");
  }

  TD_GET(ss_state)[self->evset_idx] = START;
  thread_data_t *td = hpcrun_get_thread_data();
  td->ibs_ptr = (void *)ov;
}

static void
METHOD_FN(stop)
{
  struct over_args *ov;
  thread_data_t *td = hpcrun_get_thread_data();
  ov=(struct over_args *)td->ibs_ptr;
  if(pfm_self_stop(ov->fd)==-1){
    EMSG("Fail to stop CYCLE");
  }
  int i;
  for(i=0;i<MAX_FD;i++)
  {
    if(fd2ov[i].fd==ov->fd)
      break;
  }
  TMSG(IBS_SAMPLE,"i=%d, count=%d",i,fd2ov[i].count);
  
  TMSG(IBS_SAMPLE,"stopping CYCLE");
  TD_GET(ss_state)[self->evset_idx] = STOP;
  // return rc;
}

static void
METHOD_FN(shutdown)
{
  METHOD_CALL(self, stop); // make sure stop has been called
  self->state = UNINIT;
}

static int
METHOD_FN(supports_event, const char *ev_str)
{
  return (strstr(ev_str,"CYCLE") != NULL);//in the command line CYCLE_XXXX@XXXX is needed
}
 

static void
METHOD_FN(process_event_list, int lush_metrics)
{
  char *event;
  int i, ret;
  int num_lush_metrics = 0;
 
#ifdef OLD_SS
  char *evlist = self->evl.evl_spec;
#endif
  char* evlist = METHOD_CALL(self, get_event_str);
  i = 0;
  for (event = start_tok(evlist); more_tok(); event = next_tok()) {
    char tmp_name[1024]; //CYCLE_XXXX
//    char name[1024]; //XXXX
    int evcode;
    unsigned int desc;
    long thresh;
 
    TMSG(IBS_SAMPLE,"checking event spec = %s",event);
    extract_ev_thresh_cycle(event, sizeof(tmp_name), tmp_name, &thresh, &cycle_events[i].c_mask, &cycle_events[i].inv);
    strcpy(cycle_events[i].full_name, tmp_name+6);//eliminate CYCLE_ prefix
    TMSG(IBS_SAMPLE, "full_name = %s", cycle_events[i].full_name);
    ret = pfm_find_event( cycle_events[i].full_name, &desc);
    if (ret != PFMLIB_SUCCESS ) {
      EMSG("unexpected failure in Perfmon process_event_list(): "
           "pfm_find_event() returned (%d)", ret);
      hpcrun_ssfail_unsupported("CYCLE",  cycle_events[i].full_name);
    }
    ret = pfm_get_event_code(desc, &evcode);
    if (ret != PFMLIB_SUCCESS ) {
      hpcrun_ssfail_unsupported("CYCLE",  cycle_events[i].full_name);
    }
 
    TMSG(PAPI,"got event code = %x, thresh = %ld", evcode, thresh);
    METHOD_CALL(self, store_event, evcode, thresh);
    i++;
  }
  int nevents = (self->evl).nevents;
  TMSG(IBS_SAMPLE,"nevents = %d", nevents);
 
  hpcrun_pre_allocate_metrics(nevents + num_lush_metrics);

  size_t max_len;//max length of name
  pfm_get_max_event_name_len(&max_len);

  for (i = 0; i < nevents; i++) {
    char buffer[max_len];
    int metric_id = hpcrun_new_metric(); /* weight */
    TMSG(IBS_SAMPLE, "allocate metric id = %d", metric_id);
    METHOD_CALL(self, store_metric_id, i, metric_id);
    unsigned int desc1;
    pfm_find_event_bycode (self->evl.events[i].event, &desc1);
    pfm_get_event_name (desc1, buffer, max_len);
    TMSG(IBS_SAMPLE, "metric for event %d = %s", i, cycle_events[i].full_name);
    hpcrun_set_metric_info_and_period(metric_id, strdup(cycle_events[i].full_name),
                                      HPCRUN_MetricFlag_Async,
                                      self->evl.events[i].thresh);
    // FIXME:LUSH: need a more flexible metric interface
    if (num_lush_metrics > 0 && strcmp(buffer, "PAPI_TOT_CYC") == 0) {
      // there should be one lush metric; its source is the last event
      assert(num_lush_metrics == 1 && (i == (nevents - 1)));
      int mid_idleness = hpcrun_new_metric();
      lush_agents->metric_time = metric_id;
      lush_agents->metric_idleness = mid_idleness;
 
      hpcrun_set_metric_info_and_period(mid_idleness, "idleness",
                                        HPCRUN_MetricFlag_Async | HPCRUN_MetricFlag_Real,
                                        self->evl.events[i].thresh);
    }
  }
}

//
// Event "sets" not possible for this sample source.
// It has only 1 event.
//
static void
METHOD_FN(gen_event_set, int lush_metrics)
{
  TMSG(IBS_SAMPLE, "In gen_event_set");
  sigemptyset(&sigset_cycle);
  sigaddset(&sigset_cycle, HPCRUN_PROFILE_SIGNAL);
  monitor_sigaction(HPCRUN_PROFILE_SIGNAL, &cycle_signal_handler, 0, NULL);
}

static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available CYCLE ACCOUNTING events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("CYCLE ACCOUNTING\tCYCLE ACCOUNTING uses different events to sample\n");
  printf("\n");
}

/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name cycle
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

/******************************************************************************
 * private operations 
 *****************************************************************************/

static int
cycle_signal_handler(int sig, siginfo_t* siginfo, void* context)
{
  int i;

  pfarg_msg_t msg;
  int ret, fd;
  struct over_args* ov;
  void* pc = context_pc(context);

  TMSG(IBS_SAMPLE, "in the handler function");
 
  // Must check for async block first and avoid any MSG if true.
  if (hpcrun_async_is_blocked(pc)) {
    hpcrun_stats_num_samples_blocked_async_inc();
    return 0;
  }

  thread_data_t *td = hpcrun_get_thread_data();
  ov = (struct over_args*)td->ibs_ptr;
  fd = siginfo->si_fd;
  if(ov->fd != fd)
  {
    TMSG(IBS_SAMPLE,"fd is not the same fd=%d si_fd=%d",ov->fd, fd);
  }
  for (i=0;i<MAX_FD;i++)
  {
    if(fd2ov[i].fd==fd)
      break;
  }
  ov=&fd2ov[i];
  ov->count++;

  ret=read(fd, &msg, sizeof(msg));
  if(msg.type==PFM_MSG_OVFL)
  {
    uint64_t bit = 1;
    TMSG(IBS_SAMPLE, "msg_ovfl_pmds[0] = %d", msg.pfm_ovfl_msg.msg_ovfl_pmds[0]);
    for (i = 0; i<64; i++)
    {
      
      if(((msg.pfm_ovfl_msg.msg_ovfl_pmds[0]) & (bit << (i))) > 0) //pmd[i] overflow
      {
        pfm_read_pmds(fd, &ov->pd[i], 1); //read pmd
        int metric_id = hpcrun_event2metric(&_cycle_obj, i);
        TMSG(IBS_SAMPLE,"sampling call path for metric_id = %d", metric_id);
        hpcrun_sample_callpath(context, metric_id, 1/*metricIncr*/,
                               0/*skipInner*/, 0/*isSync*/);
      }
    }
  }
 
  if (hpcrun_is_sampling_disabled()) {
    TMSG(IBS_SAMPLE, "No CYCLE restart, due to disabled sampling");
    return 0;
  }

  if(pfm_restart(fd)){
    EMSG("Fail to restart CYCLE");
    exit(1);
  }
  return 0;
}

void
extract_ev_thresh_cycle(const char *in, int evlen, char *ev, long *th, int *c_mask, int *inv)
{
  unsigned int len;

  char *dlm = strrchr(in, '@');
  char *dlm1 = strchr(in, '.');
  char *tmp;

  /*default value*/
  *c_mask = 0;
  *inv = 0;

  if (!dlm) {
    dlm = strrchr(in, ':');
  }
  if (dlm) {
    if (isdigit(dlm[1])) { // assume this is the threshold
     if(!dlm1){
      len = MIN(dlm - in, evlen);
      strncpy(ev, in, len);
      ev[len] = '\0';
      TMSG(IBS_SAMPLE, "extract event = %s", ev);
     }
     else{
      len = MIN(dlm1 - in, evlen);
      strncpy(ev, in, len);
      ev[len] = '\0';
      TMSG(IBS_SAMPLE, "extract event = %s", ev);
      if((tmp=strstr(dlm1, ".c_mask"))!=NULL) //has c_mask
      {
        char *dlm2 = strchr(tmp, '=');
        if(isdigit(dlm2[1]))
          *c_mask = strtol(dlm2+1, (char **)NULL, 10);
      }     
      if((tmp=strstr(dlm1, ".inv"))!=NULL) //has c_mask
      {
        char *dlm2 = strchr(tmp, '=');
        if(isdigit(dlm2[1]))
          *inv = strtol(dlm2+1, (char **)NULL, 10);
      } 
     }
    }
    else {
      dlm = NULL;
    }
  }

  if (!dlm) {
/*    len = strlen(in);
    strncpy(ev, in, len);
    ev[len] = '\0';
*/
    if(!dlm1){
      len = MIN(dlm - in, evlen);
      strncpy(ev, in, len);
      ev[len] = '\0';
      TMSG(IBS_SAMPLE, "extract event = %s", ev);
     }
    else{
      len = MIN(dlm1 - in, evlen);
      strncpy(ev, in, len);
      ev[len] = '\0';
      TMSG(IBS_SAMPLE, "extract event = %s", ev);
      if((tmp=strstr(dlm1, ".c_mask"))!=NULL) //has c_mask
      {
        char *dlm2 = strchr(tmp, '=');
        if(isdigit(dlm2[1]))
          *c_mask = strtol(dlm2+1, (char **)NULL, 10);
      }     
      if((tmp=strstr(dlm1, ".inv"))!=NULL) //has c_mask
      {
        char *dlm3 = strchr(tmp, '=');
        if(isdigit(dlm3[1]))
          *inv = strtol(dlm3+1, (char **)NULL, 10);
      } 
    }
  }

  *th = dlm ? strtol(dlm+1,(char **)NULL,10) : DEFAULT_TH;
  TMSG(IBS_SAMPLE, "c_mask = %d, inv = %d", *c_mask, *inv);
}


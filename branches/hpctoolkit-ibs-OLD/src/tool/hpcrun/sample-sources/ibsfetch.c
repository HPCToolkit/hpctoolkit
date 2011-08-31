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

#include "x86-decoder.h" //use xed to find prefetching op

#include "ibs_init.h" //find the splay tree
#include "splay.h"
#include "splay-interval.h"
/******************************************************************************
 * macros
 *****************************************************************************/

#   define HPCRUN_PROFILE_SIGNAL           SIGIO
#   define MAX_FD                          20
#   define METRIC_NUM                       6

#   define DEFAULT_FMT_FLAG                 0
#   define HEX_FMT_FLAG                     1

/******************************************************************************
 * local constants
 *****************************************************************************/

enum _local_const {
    IBS_EVENT = 0// IBS has only 1 event
};

/******************************************************************************
 * local inline functions 
 *****************************************************************************/


inline pid_t
gettid(void)
{
        return (pid_t)syscall(__NR_gettid);
}


/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static int
ibsfetch_signal_handler(int sig, siginfo_t *siginfo, void *context);

extern void metric_add(int metric_id, cct_metric_data_t* loc, cct_metric_data_t datum);
extern void update_bt(backtrace_t*, void*);
int is_kernel(void*);
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
};                                                                                  
 
struct over_args fd2ov[MAX_FD];    

//metrics for one sample: 0: Total samples, 1: instruction complete, 2: instruction cache miss, 3: L1 TLB miss, 4: L2 TLB miss, 5: Fetch latency
static int metrics [METRIC_NUM];

static long period=(65535);

// ******* METHOD DEFINITIONS ***********

// init function for IBS: check CPU type
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
  if(type != PFMLIB_AMD64_PMU){
    STDERR_MSG("Fatal error: CPU type is not AMD64.");
    exit(1);
  }
  self->state = INIT;
}

static void
METHOD_FN(start)
{
  int ret;
  struct over_args *ov;
  thread_data_t *td1 = hpcrun_get_thread_data();

  if (! hpcrun_td_avail()){
    return; // in the unlikely event that we are trying to start, but thread data is unavailable,
            // assume that all sample source ops are suspended.
  }
  
  TMSG(IBS_SAMPLE,"starting IBS %d %d",td1->id, period);
  
  ov=&fd2ov[td1->id];
  memset(ov,0,sizeof(struct over_args));

  ov->inp_mod.ibsfetch.maxcnt=period;
  ov->inp_mod.flags |= PFMLIB_AMD64_USE_IBSFETCH;

  ret=pfm_dispatch_events(NULL, &ov->inp_mod, &ov->outp, &ov->outp_mod);
  if (ret != PFMLIB_SUCCESS) {
    EMSG("cannot dispatch events: %s", pfm_strerror(ret));
    exit(1);
  }
  if (ov->outp.pfp_pmc_count != 1) {
    EMSG("Unexpected PMC register count: %d", ov->outp.pfp_pmc_count);
    exit(1);
  }
  if (ov->outp.pfp_pmd_count != 1) {
    EMSG("Unexpected PMD register count: %d", ov->outp.pfp_pmd_count);
    exit(1);
  }
  if (ov->outp_mod.ibsop_base != 0) {
    EMSG("Unexpected IBSOP base register: %d", ov->outp_mod.ibsop_base);
    exit(1);
  }

  ov->pc[0].reg_num=ov->outp.pfp_pmcs[0].reg_num;
  ov->pc[0].reg_value=ov->outp.pfp_pmcs[0].reg_value;

  ov->pd[0].reg_num=ov->outp.pfp_pmds[0].reg_num;//pd[0].reg_num=3
  ov->pd[0].reg_value=0;

  ov->pd[0].reg_flags |= PFM_REGFL_OVFL_NOTIFY;

  ov->pd[0].reg_smpl_pmds[0]=((1UL<<3)-1)<<ov->outp.pfp_pmds[0].reg_num;


  int fd = pfm_create_context(&ov->ctx, NULL, 0, 0);
  if(fd==-1){
    EMSG("error in create context");
    hpcrun_ssfail_start("IBS");
  }
  ov->fd = fd;
  if(pfm_write_pmcs(ov->fd,ov->pc,ov->outp.pfp_pmc_count)){
    EMSG("error in write pmc");
    hpcrun_ssfail_start("IBS");
  }
 
  if(pfm_write_pmds(ov->fd,ov->pd,ov->outp.pfp_pmd_count)){
    EMSG("error in write pmd");
    hpcrun_ssfail_start("IBS");
  }
 
  ov->load_args.load_pid=gettid();
 
  if(pfm_load_context(ov->fd, &ov->load_args)){
    EMSG("error in load");
    hpcrun_ssfail_start("IBS");
  }
 
  ret = fcntl(ov->fd, F_SETFL, fcntl(ov->fd, F_GETFL, 0) | O_ASYNC);
  if (ret == -1){
    EMSG("cannot set ASYNC");
    hpcrun_ssfail_start("IBS");
  }
//  ret = fcntl(ov->fd, F_SETOWN, gettid());
//  if (ret == -1){
//    EMSG("cannot set ownership");
//    hpcrun_ssfail_start("IBS");
//  }

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
 
  if(pfm_self_start(ov->fd)==-1){
    EMSG("start error");
    hpcrun_ssfail_start("IBS");
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
    EMSG("Fail to stop IBS");
  }
  int i;
  for(i=0;i<MAX_FD;i++)
  {
    if(fd2ov[i].fd==ov->fd)
      break;
  }
  TMSG(IBS_SAMPLE,"i=%d, count=%d",i,fd2ov[i].count);
  
  TMSG(IBS_SAMPLE,"stopping IBS");

  is_use_reuse = true; //set epoch bit for use and reuse analysis

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
  return (strstr(ev_str,"IBS_FETCH") != NULL);//in the command line IBS_SAMPLE@XXXX is needed
}
 

static void
METHOD_FN(process_event_list, int lush_metrics)
{
#ifdef OLD_SS
  char *_p = strchr(METHOD_CALL(self,get_event_str),'@');
  if ( _p) {
    period = strtol(_p+1,NULL,10);
  }
  else {
    TMSG(IBS_SAMPLE,"IBS event default period (0xFFFF) selected");
  }
  
  METHOD_CALL(self, store_event, IBS_EVENT, period);
  TMSG(IBS_SAMPLE,"IBS period set to %ld",period);
#endif
  // fetch the event string for the sample source
  char* _p = METHOD_CALL(self, get_event_str);

  //
  // EVENT: Only 1 IBS_SAMPLE event
  //
  char* event = start_tok(_p);

  char name[1024]; // local buffer needed for extract_ev_threshold
  
  TMSG(IBS_SAMPLE,"checking event spec = %s",event);

  //extract event threshold
  extract_ev_thresh_w_default(event, sizeof(name), name, &period,period);
  TMSG(IBS_SAMPLE,"IBS_SAMPLE period set to %ld",period);
  period=period<<4;

  //store event threshold
  METHOD_CALL(self, store_event, IBS_EVENT, period);
  TMSG(IBS_SAMPLE,"IBS_SAMPLE period set to %ld",period);

#define sample_period period

  int i;
  for(i=0;i<METRIC_NUM;i++){
    metrics[i] = hpcrun_new_metric();
    METHOD_CALL(self, store_metric_id, IBS_EVENT, metrics[i]);
    TMSG(IBS_SAMPLE,"setting metric%d IBS,period = %ld",i,sample_period);
//    hpcrun_set_metric_info_and_period(metric_id, "IBS_SAMPLE",
//				    HPCRUN_MetricFlag_Async,
//				    sample_period);
    if(i==0)
      hpcrun_set_metric_info_w_fn(metrics[i], "Total",
                                    HPCRUN_MetricFlag_Async, 1,//sample_period,
                                    metric_add, DEFAULT_FMT_FLAG);
    if(i==1)
      hpcrun_set_metric_info_w_fn(metrics[i], "Completed",
                                    HPCRUN_MetricFlag_Async, 1,//sample_period,
                                    metric_add, DEFAULT_FMT_FLAG);
    if(i==2)
      hpcrun_set_metric_info_w_fn(metrics[i], "ICache miss",
                                    HPCRUN_MetricFlag_Async, 1,//sample_period,
                                    metric_add, DEFAULT_FMT_FLAG);
    if(i==3)
      hpcrun_set_metric_info_w_fn(metrics[i], "L1 ITLB miss",
                                    HPCRUN_MetricFlag_Async, 1,//sample_period,
                                    metric_add, DEFAULT_FMT_FLAG);
    if(i==4)
      hpcrun_set_metric_info_w_fn(metrics[i], "L2 ITLB miss",
                                    HPCRUN_MetricFlag_Async, 1,//sample_period,
                                    metric_add, DEFAULT_FMT_FLAG);
    if(i==5)
      hpcrun_set_metric_info_w_fn(metrics[i], "Instruction fetch latency",
                                    HPCRUN_MetricFlag_Async, 1,//sample_period,
                                    metric_add, DEFAULT_FMT_FLAG);

    if (lush_metrics == 1) {
      int mid_idleness = hpcrun_new_metric();
      lush_agents->metric_time = metrics[i];
      lush_agents->metric_idleness = mid_idleness;

      hpcrun_set_metric_info_and_period(mid_idleness, "idleness",
				      HPCRUN_MetricFlag_Async | HPCRUN_MetricFlag_Real,
				      sample_period);
    }  
  }

  event = next_tok();
  if (more_tok()) {
    EMSG("MULTIPLE WALLCLOCK events detected! Using first event spec: %s");
  }

//  memset(fd2ov,0,sizeof(fd2ov));

  thread_data_t *td = hpcrun_get_thread_data();
  td->eventSet[self->evset_idx] = 0xDEAD;
}

//
// Event "sets" not possible for this sample source.
// It has only 1 event.
//
static void
METHOD_FN(gen_event_set, int lush_metrics)
{
  TMSG(IBS_SAMPLE, "In gen_event_set");
  sigemptyset(&sigset_ibs);
  sigaddset(&sigset_ibs, HPCRUN_PROFILE_SIGNAL);
  monitor_sigaction(HPCRUN_PROFILE_SIGNAL, &ibsfetch_signal_handler, 0, NULL);
}

static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available IBS events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("IBS_SAMPLE\tIBS uses cycles to sample\n");
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
  void *ip;
  uint64_t linear_addr = 0;
  uint64_t fetchdata;
  pfarg_msg_t msg;
  ibsfetchctl_t *ibsfetchdata;
  int ret;
  int fd;
  int lat = 0;//-1 means latency is invalid
  struct over_args* ov;
//  xed_decoded_inst_t xedd;
//  xed_decoded_inst_t *xptr = &xedd;
//  xed_error_enum_t xed_error;
  char inst_buf[1024];

  thread_data_t *td = hpcrun_get_thread_data();
  ov = (struct over_args*)td->ibs_ptr;
  fd = siginfo->si_fd;
  if(ov->fd != fd)
  {
    TMSG(IBS_SAMPLE,"fd is not the same fd=%d si_fd=%d",ov->fd, fd);
  }
  int i;
  for (i=0;i<MAX_FD;i++)
  {
    if(fd2ov[i].fd==fd)
      break;
  }
  ov=&fd2ov[i];
  ov->count++;

  ip=NULL;
  // Must check for async block first and avoid any MSG if true.
  void* pc = hpcrun_context_pc(context);
  if (hpcrun_async_is_blocked(pc)) {
    hpcrun_stats_num_samples_blocked_async_inc();
    ret=read(fd, &msg, sizeof(msg)); //this is needed to restart IBS
  }
  else {
    fetchdata=0;
    ret=read(fd, &msg, sizeof(msg));
    if(msg.type==PFM_MSG_OVFL)
    {
      ov->pd[0].reg_num=5;
      pfm_read_pmds(fd,ov->pd,1);
      ip=(void *)ov->pd[0].reg_value;
      TMSG(IBS_SAMPLE, "real ip is %p\n", ip);
      ov->pd[0].reg_num=4;
      pfm_read_pmds(fd,ov->pd,1);
      fetchdata=ov->pd[0].reg_value;
      ibsfetchdata=(ibsfetchctl_t *)(&fetchdata);
      hpcrun_sample_callpath_w_bt(context, metrics[0], 1,
                           update_bt, (void*)ip, 0/*isSync*/);
      if(ibsfetchdata->reg.ibsfetchcomp == 1)
      {
         hpcrun_sample_callpath_w_bt(context, metrics[1], 1,
                           update_bt, (void*)ip, 0/*isSync*/);
      }
      if(ibsfetchdata->reg.ibsicmiss == 1)
      {
         hpcrun_sample_callpath_w_bt(context, metrics[2], 1,
                           update_bt, (void*)ip, 0/*isSync*/);
      }
      if(ibsfetchdata->reg.ibsl1tlbmiss == 1)
      {
         hpcrun_sample_callpath_w_bt(context, metrics[3], 1,
                           update_bt, (void*)ip, 0/*isSync*/);
      }
      if(ibsfetchdata->reg.ibsl2tlbmiss == 1)
      {
         hpcrun_sample_callpath_w_bt(context, metrics[4], 1,
                           update_bt, (void*)ip, 0/*isSync*/);
      }
      hpcrun_sample_callpath_w_bt(context, metrics[5], ibsfetchdata->reg.ibsfetchlat,
                           update_bt, (void*)ip, 0/*isSync*/);
    }
  }

  if(pfm_restart(fd)){
    EMSG("Fail to restart IBSFETCH");
    exit(1);
  }

  return 0; /* tell monitor that the signal has been handled */
}

#if 0
malloc_list_s* new_malloc_node(malloc_list_s** list, uint32_t id)
{
  int i=0;
  malloc_list_s* tmp=NULL;
  malloc_list_s* current = *list;
  while(current != NULL)
  {
    i++;
    TMSG(IBS_SAMPLE, "i = %d", i);
    if(current->malloc_id == id)
      return NULL;
    tmp = current;
    current = current->next;
  }
  /*need to create a new node*/
  current = hpcrun_malloc(sizeof(malloc_list_s));
  current->malloc_id = id;
  current->next = NULL;
  if(*list!=NULL)//if list is null, no need to insert node
    tmp->next = current; // make the link in the list
  else if(*list==NULL)
    *list = current;//First init malloc list
  return current;
}

void addr_min(int metric_id, cct_metric_data_t* loc, cct_metric_data_t datum)
{
        if(loc->p<=0)
          loc->p=datum.p;
	else if(datum.p < loc->p)
		loc->p=datum.p;
        TMSG(IBS_SAMPLE,"min=%p",loc->p);
}

void addr_max(int metric_id, cct_metric_data_t* loc, cct_metric_data_t datum)
{
	if(datum.p > loc->p)
		loc->p=datum.p;
        TMSG(IBS_SAMPLE,"max=%p",loc->p);
}

void metric_add(int metric_id, cct_metric_data_t* loc, cct_metric_data_t datum)
{
        loc->i+=datum.i;
}

int is_kernel(void* addr)
{
  int bit=63;
  return ((uint64_t)addr >> bit);
}

void update_bt(backtrace_t* bt, void* ibs_addr)
{
  if(is_kernel(ibs_addr)){
    hpcrun_bt_add_leaf_child(bt, ibs_addr);
//    TMSG(IBS_SAMPLE, "this is a kernel address %p", ibs_addr);
  }
  else{
    hpcrun_bt_modify_leaf_addr(bt, ibs_addr);
//    TMSG(IBS_SAMPLE, "this is not a kernel address %p", ibs_addr);
  }
}

#endif

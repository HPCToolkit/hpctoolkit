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
#   define METRIC_NUM                      11

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
ibs_signal_handler(int sig, siginfo_t *siginfo, void *context);

void addr_min(int metric_id, cct_metric_data_t* loc, cct_metric_data_t datum);
void addr_max(int metric_id, cct_metric_data_t* loc, cct_metric_data_t datum);
void metric_add(int metric_id, cct_metric_data_t* loc, cct_metric_data_t datum);
void update_bt(backtrace_t*, void*);
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

//metrics for one sample: 0: min address, 1: max address, 2: #load+#store, 
//3: #cache misses, 4: #latency, 5: L1 tlb miss, 6: L2 tlb miss, 7: local DRAM access
//8: remote DRAM access, 9: # of access to caches of local cores, 10: # of access to caches 
//of remote cores
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

  ov->inp_mod.ibsop.maxcnt=period;
  ov->inp_mod.flags |= PFMLIB_AMD64_USE_IBSOP;

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

  ov->pd[0].reg_num=ov->outp.pfp_pmds[0].reg_num;//pd[0].reg_num=7
  ov->pd[0].reg_value=0;

  ov->pd[0].reg_flags |= PFM_REGFL_OVFL_NOTIFY;

  ov->pd[0].reg_smpl_pmds[0]=((1UL<<7)-1)<<ov->outp.pfp_pmds[0].reg_num;


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
  ret = fcntl(ov->fd, F_SETOWN, gettid());
  if (ret == -1){
    EMSG("cannot set ownership");
    hpcrun_ssfail_start("IBS");
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
  return (strstr(ev_str,"IBS_SAMPLE") != NULL);//in the command line IBS_SAMPLE@XXXX is needed
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
      hpcrun_set_metric_info_w_fn(metrics[i], "ADDR(min)",
                                    HPCRUN_MetricFlag_Async, 1,//sample_period,
                                    addr_min, HEX_FMT_FLAG);
    if(i==1)
      hpcrun_set_metric_info_w_fn(metrics[i], "ADDR(max)",
                                    HPCRUN_MetricFlag_Async, 1,//sample_period,
                                    addr_max, HEX_FMT_FLAG);
    if(i==2)
      hpcrun_set_metric_info_w_fn(metrics[i], "#(LD+ST)",
                                    HPCRUN_MetricFlag_Async, 1,//sample_period,
                                    metric_add, DEFAULT_FMT_FLAG);
    if(i==3)
      hpcrun_set_metric_info_w_fn(metrics[i], "CACHE_MISS",
                                    HPCRUN_MetricFlag_Async, 1,//sample_period,
                                    metric_add, DEFAULT_FMT_FLAG);
    if(i==4)
      hpcrun_set_metric_info_w_fn(metrics[i], "LATENCY",
                                    HPCRUN_MetricFlag_Async, 1,//sample_period,
                                    metric_add, DEFAULT_FMT_FLAG);
    if(i==5)
      hpcrun_set_metric_info_w_fn(metrics[i], "L1_TLB_MISS",
                                    HPCRUN_MetricFlag_Async, 1,//sample_period,
                                    metric_add, DEFAULT_FMT_FLAG);
    if(i==6)
      hpcrun_set_metric_info_w_fn(metrics[i], "L2_TLB_MISS",
                                    HPCRUN_MetricFlag_Async, 1,//sample_period,
                                    metric_add, DEFAULT_FMT_FLAG);
    if(i==7)
      hpcrun_set_metric_info_w_fn(metrics[i], "L_DRAM_ACCESS",
                                    HPCRUN_MetricFlag_Async, 1,//sample_period,
                                    metric_add, DEFAULT_FMT_FLAG);
    if(i==8)
      hpcrun_set_metric_info_w_fn(metrics[i], "R_DRAM_ACCESS",
                                    HPCRUN_MetricFlag_Async, 1,//sample_period,
                                    metric_add, DEFAULT_FMT_FLAG);
    if(i==9)
      hpcrun_set_metric_info_w_fn(metrics[i], "L_CACHE_ACCESS",
                                    HPCRUN_MetricFlag_Async, 1,//sample_period,
                                    metric_add, DEFAULT_FMT_FLAG);
    if(i==10)
      hpcrun_set_metric_info_w_fn(metrics[i], "R_CACHE_ACCESS",
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
  monitor_sigaction(HPCRUN_PROFILE_SIGNAL, &ibs_signal_handler, 0, NULL);
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

#define ss_name ibs
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

/******************************************************************************
 * private operations 
 *****************************************************************************/

static int
ibs_signal_handler(int sig, siginfo_t* siginfo, void* context)
{
  void *ip;
  uint64_t data3;
  uint64_t linear_addr = 0;
  pfarg_msg_t msg;
  ibsopdata3_t *opdata3;
  ibsopdata2_t *opdata2;
  int ret;
  int fd;
  int lat = 0;//-1 means latency is invalid
  struct over_args* ov;
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  xed_error_enum_t xed_error;
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
    data3=0;
//    memset(&opdata3,0,sizeof(ibsopdata3_t));
//    memset(&opdata2,0,sizeof(ibsopdata2_t));
    ret=read(fd, &msg, sizeof(msg));
    if(msg.type==PFM_MSG_OVFL)
    {
      ov->pd[0].reg_num=8;
      pfm_read_pmds(fd,ov->pd,1);
      ip=(void *)ov->pd[0].reg_value;
      ov->pd[0].reg_num=11;
      pfm_read_pmds(fd,ov->pd,1);
      data3=ov->pd[0].reg_value;
      opdata3=(ibsopdata3_t *)(&data3);
    
      if((opdata3->reg.ibsldop == 1)||(opdata3->reg.ibsstop == 1)){ //collect data only load & store instructions
        if(opdata3->reg.ibsstop == 1)
          TMSG(IBS_SAMPLE, "this is store op");
        if(!is_kernel(ip))//xed cannot process the kernel instruction
        {
          xed_decoded_inst_zero_set_mode(xptr, &x86_decoder_settings.xed_settings);
          xed_error = xed_decode(xptr, (uint8_t*) ip, 15);
          xed_format_xed(xptr, inst_buf, sizeof(inst_buf), (uint64_t) ip);
          TMSG(IBS_SAMPLE,"%p: %s", ip, xed_iclass_enum_t2str(iclass(xptr)));
        }

        hpcrun_sample_callpath_w_bt(context, metrics[2], 1,
                           update_bt, (void*)ip, 0/*isSync*/);
        if((opdata3->reg.ibsldop == 1)&&(opdata3->reg.ibsdcmiss == 1))//NB data only available when load and dc miss
        {
          ov->pd[0].reg_num=10;
          pfm_read_pmds(fd,ov->pd,1);
          uint64_t data2 = 0;
          data2=ov->pd[0].reg_value;
          opdata2=(ibsopdata2_t *)(&data2);

          if((opdata2->reg.nbibsreqsrc == 0x3)&&(opdata2->reg.nbibsreqdstproc == 0x0))//read from local DRAM
            hpcrun_sample_callpath_w_bt(context, metrics[7], 1,
                           update_bt, (void*)ip, 0/*isSync*/);
          if((opdata2->reg.nbibsreqsrc == 0x3)&&(opdata2->reg.nbibsreqdstproc == 0x1))//read from remote DRAM
            hpcrun_sample_callpath_w_bt(context, metrics[8], 1,
                           update_bt, (void*)ip, 0/*isSync*/);

          if(((opdata2->reg.nbibsreqsrc == 0x1)&&(opdata2->reg.nbibsreqdstproc == 0x0))||//read from local L3 cache
             ((opdata2->reg.nbibsreqsrc == 0x2)&&(opdata2->reg.nbibsreqdstproc == 0x0)))//read from L1 or L2 of a local core
            hpcrun_sample_callpath_w_bt(context, metrics[9], 1,
                           update_bt, (void*)ip, 0/*isSync*/);
          if((opdata2->reg.nbibsreqsrc == 0x2)&&(opdata2->reg.nbibsreqdstproc == 0x1))//read from L1, L2, L3 of a remote core
            hpcrun_sample_callpath_w_bt(context, metrics[10], 1,
                           update_bt, (void*)ip, 0/*isSync*/);
        }
        if(opdata3->reg.ibsdcmiss == 1)
          hpcrun_sample_callpath_w_bt(context, metrics[3], 1,
                           update_bt, (void*)ip, 0/*isSync*/);
        
        if(opdata3->reg.ibsdcl1tlbmiss == 1)
          hpcrun_sample_callpath_w_bt(context, metrics[5], 1,
                           update_bt, (void*)ip, 0/*isSync*/); 
        if(opdata3->reg.ibsdcl2tlbmiss == 1)
          hpcrun_sample_callpath_w_bt(context, metrics[6], 1,
                           update_bt, (void*)ip, 0/*isSync*/);

        if(opdata3->reg.ibsdclinaddrvalid)
        {
          ov->pd[0].reg_num=12;
          pfm_read_pmds(fd,ov->pd,1);
          linear_addr=ov->pd[0].reg_value;
          //latency is invalid for prefetch
          if((opdata3->reg.ibsldop == 1)&&(opdata3->reg.ibsdcmiss == 1)&&(!is_kernel(ip))&&(strstr(xed_iclass_enum_t2str(iclass(xptr)),"PREFETCH")==NULL))
            lat=opdata3->reg.ibsdcmisslat;
          else
            lat=-1;//latency is not valid for store
          if(!is_kernel(linear_addr)){ //the address touched should not be kernel address
            interval_tree_node* result_node = splaytree_lookup((void*)linear_addr);//find the address in splay tree
            if(result_node != NULL)
            {
              TMSG(IBS_SAMPLE,"find %p in splay tree with interval %d [%p, %p)", (void*)linear_addr, result_node->cct_id, result_node->start, result_node->end);
            }
            hpcrun_sample_callpath_w_bt(context, metrics[0], linear_addr,
                           update_bt, (void*)ip, 0/*isSync*/);
            hpcrun_sample_callpath_w_bt(context, metrics[1], linear_addr,
                           update_bt, (void*)ip, 0/*isSync*/);
          }
          else
            TMSG(IBS_SAMPLE,"this is kernel address");
        }
        else//no read memory then find out the latency
        {
          if((opdata3->reg.ibsldop == 1)&&(opdata3->reg.ibsdcmiss == 1)&&(!is_kernel(ip))&&(strstr(xed_iclass_enum_t2str(iclass(xptr)),"PREFETCH")==NULL))
            lat=opdata3->reg.ibsdcmisslat;
          else
            lat=-1;//latency is not valid for store
        }
      }
      else//no load and store op then restart
      {
        if(pfm_restart(fd)){
          EMSG("Fail to restart IBS");
          exit(1);
        }
        return 0;
      }
    }
    if(lat>0)
      hpcrun_sample_callpath_w_bt(context, metrics[4], lat,
                           update_bt, (void*)ip, 0/*isSync*/);

    TMSG(IBS_SAMPLE, "ip=%p, pc=%p, linear_addr=%p latency=%d",ip, hpcrun_context_pc(context),linear_addr,lat);
  }
  if (hpcrun_is_sampling_disabled()) {
    TMSG(IBS_SAMPLE, "No IBS restart, due to disabled sampling");
    return 0;
  }

  if(pfm_restart(fd)){
    EMSG("Fail to restart IBS");
    exit(1);
  }

  return 0; /* tell monitor that the signal has been handled */
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

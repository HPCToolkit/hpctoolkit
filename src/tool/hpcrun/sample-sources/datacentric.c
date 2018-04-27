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
// Copyright ((c)) 2002-2018, Rice University
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


/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>



/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/disabled.h>
#include <hpcrun/metrics.h>
#include <sample_event.h>
#include "sample_source_obj.h"
#include "common.h"
#include <main.h>
#include <hpcrun/sample_sources_registered.h>
#include "simple_oo.h"
#include <hpcrun/thread_data.h>

#include <messages/messages.h>
#include <utilities/tokenize.h>

#include "datacentric.h"
#include <safe-sampling.h>

#define DEFAULT_THRESHOLD (8 * 1024)

static int alloc_metric_id = -1;
static int free_metric_id = -1;


/******************************************************************************
 * segv signal handler
 *****************************************************************************/
// this segv handler is used to monitor first touches
void
segv_handler (int signal_number, siginfo_t *si, void *context)
{
  int pagesize = getpagesize();
  if (TD_GET(inside_hpcrun) && si && si->si_addr) {
    void *p = (void *)(((uint64_t)(uintptr_t) si->si_addr) & ~(pagesize-1));
    mprotect (p, pagesize, PROT_READ | PROT_WRITE);
    return;
  }
  if ( TD_GET(mem_data) == NULL)
    return;

  hpcrun_safe_enter();
  if (!si || !si->si_addr) {
    hpcrun_safe_exit();
    return;
  }
  void *start, *end;
  cct_node_t *data_node = splay_lookup((void *)si->si_addr, &start, &end);
  if (data_node) {
    void *p = (void *)(((uint64_t)(uintptr_t) start + pagesize-1) & ~(pagesize-1));
    mprotect (p, (uint64_t)(uintptr_t) end - (uint64_t)(uintptr_t) p, PROT_READ|PROT_WRITE);
    TD_GET(mem_data->data_node) = data_node;
    TD_GET(mem_data->first_touch) = 1;
    hpcrun_sample_callpath(context, alloc_metric_id, 
	(hpcrun_metricVal_t) {.i=0}, 
	0/*skipInner*/, 0/*isSync*/, NULL);
    TD_GET(mem_data->first_touch) = 0;
    TD_GET(mem_data->data_node) = NULL;
  }
  else {
    void *p = (void *)(((uint64_t)(uintptr_t) si->si_addr) & ~(pagesize-1));
    mprotect (p, pagesize, PROT_READ | PROT_WRITE);
  }
  hpcrun_safe_exit();
  return;
}

static inline void
set_segv_handler()
{
  struct sigaction sa;

  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = segv_handler;
  sigaction(SIGSEGV, &sa, NULL);
}

/******************************************************************************
 * method definitions
 *****************************************************************************/

static void
METHOD_FN(init)
{
  self->state = INIT; 

  // reset static variables to their virgin state
  alloc_metric_id = -1;
  free_metric_id = -1;
}


static void
METHOD_FN(thread_init)
{
  TMSG(DATACENTRIC, "thread init (no action needed)");
}

static void
METHOD_FN(thread_init_action)
{
  TMSG(DATACENTRIC, "thread action (noop)");
}

static void
METHOD_FN(start)
{
  TMSG(DATACENTRIC,"starting DATACENTRIC");

  TD_GET(mem_data) = (memory_data_t *)malloc(sizeof(memory_data_t));

  if (TD_GET(mem_data) == NULL) {
    EMSG("Cannot allocate memory %d bytes. Datacentric is not started",
        sizeof(memory_data_t));

    return; // do not start it
  }

  memset(TD_GET(mem_data), 0, sizeof(memory_data_t));
  TD_GET(ss_state)[self->sel_idx] = START;
}

static void
METHOD_FN(thread_fini_action)
{
  TMSG(DATACENTRIC, "thread fini (noop)");
}

static void
METHOD_FN(stop)
{
  TMSG(DATACENTRIC,"stopping DATACENTRIC");
  TD_GET(ss_state)[self->sel_idx] = STOP;
}


static void
METHOD_FN(shutdown)
{
  METHOD_CALL(self,stop); // make sure stop has been called
  self->state = UNINIT;
}


static bool
METHOD_FN(supports_event,const char *ev_str)
{
  return hpcrun_ev_is(ev_str,"DATACENTRIC");
}
 

// DATACENTRIC creates two metrics: bytes allocated and Bytes Freed.

static void
METHOD_FN(process_event_list,int lush_metrics)
{
  alloc_metric_id = hpcrun_new_metric();
  free_metric_id = hpcrun_new_metric();
  char *event;

  char* evlist = METHOD_CALL(self, get_event_str);
  for (event = start_tok(evlist); more_tok(); event = next_tok()) {
    char name[1024];
    long threshold;
    hpcrun_extract_ev_thresh(event, sizeof(name), name, &threshold, 
			   DEFAULT_THRESHOLD);

    TMSG(DATACENTRIC, "Setting up metrics for memory leak detection");

    hpcrun_set_metric_info(alloc_metric_id, "Bytes Allocated");
    hpcrun_set_metric_info(free_metric_id, "Bytes Freed");
  }

  // set and initialize segv signal
  set_segv_handler();
}



//
// Event sets not relevant for this sample source
// Events are generated by user code
//
static void
METHOD_FN(gen_event_set,int lush_metrics)
{
}


//
//
//
static void
METHOD_FN(display_events)
{
#if DATACENTRIC_DEBUG
  printf("===========================================================================\n");
  printf("Available memory leak detection events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("DATACENTRIC\tThe number of bytes allocated and freed per dynamic context\n");
  printf("\n");
#endif
}

/***************************************************************************
 * object
 ***************************************************************************/

//
// sync class is "SS_SOFTWARE" so that both synchronous and asynchronous sampling is possible
// 

#define ss_name datacentric
#define ss_cls SS_SOFTWARE

#include "ss_obj.h"


// ***************************************************************************
//  Interface functions
// ***************************************************************************

// increment the return count
//
// N.B. : This function is necessary to avoid exposing the retcnt_obj.
//        For the case of the retcnt sample source, the handler (the trampoline)
//        is separate from the sample source code.
//        Consequently, the interaction with metrics must be done procedurally

int
hpcrun_datacentric_alloc_id() 
{
  return alloc_metric_id;
}


int
hpcrun_datacentric_active() 
{
  if (hpcrun_is_initialized()) {
    return (TD_GET(ss_state)[obj_name().sel_idx] == START);
  } else {
    return 0;
  }
}



void
hpcrun_datacentric_free_inc(cct_node_t* node, int incr)
{
  if (node != NULL) {
    TMSG(DATACENTRIC, "\tfree (cct node %p): metric[%d] += %d", 
	 node, free_metric_id, incr);
    
    cct_metric_data_increment(free_metric_id,
			      node,
			      (cct_metric_data_t){.i = incr});
  }
}

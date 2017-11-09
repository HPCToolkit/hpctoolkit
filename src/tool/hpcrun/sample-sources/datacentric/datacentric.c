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
// Copyright ((c)) 2002-2017, Rice University
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
#include <stdbool.h>


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
#include <main.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/thread_data.h>

#include <messages/messages.h>
#include <utilities/tokenize.h>
#include <safe-sampling.h>

#include "datacentric.h"
#include "data_tree.h"

#include "sample-sources/display.h" // show the list of events
#include "sample-sources/perf/event_custom.h"

#include "place_folder.h"

/******************************************************************************
 * constants
 *****************************************************************************/
#define DEFAULT_THRESHOLD (8 * 1024)
#define EVNAME_DATACENTRIC "DATACENTRIC"


/******************************************************************************
 * data structure
 *****************************************************************************/
struct datacentric_info_s {
  cct_node_t *data_node;
  bool first_touch;
};

/******************************************************************************
 * local variables
 *****************************************************************************/
static int alloc_metric_id = -1;
static int free_metric_id = -1;

#define POINTER_TO_FUNCTION

#if defined(__PPC64__) || defined(HOST_CPU_IA64)
#define POINTER_TO_FUNCTION *(void**)
#endif

static cct_node_t*
hpcrun_cct_record_datacentric(cct_bundle_t* cct, cct_node_t* cct_cursor, void *data_aux)
{
  if (data_aux == NULL)
    return cct_cursor;

  // datacentric support: attach samples to data allocation cct
  struct datacentric_info_s *info = (struct datacentric_info_s *) data_aux;
  cct_node_t *data_node           = info->data_node;

  if (data_node) {
    cct_cursor = hpcrun_insert_special_node(cct->tree_root,
                                            POINTER_TO_FUNCTION FUNCTION_FOLDER_NAME(heap));

    // copy the call path of the malloc
    cct_cursor = hpcrun_cct_insert_path_return_leaf(data_node, cct_cursor);

    if (info->first_touch) {
      cct_cursor = hpcrun_insert_special_node(cct_cursor,
                                              POINTER_TO_FUNCTION FUNCTION_FOLDER_NAME(first_touch));
      cct_cursor = hpcrun_insert_special_node(cct_cursor,
                                              POINTER_TO_FUNCTION FUNCTION_FOLDER_NAME(access_heap));
    }
  }
  return cct_cursor;
}

/******************************************************************************
 * segv signal handler
 *****************************************************************************/
// this segv handler is used to monitor first touches
static void
segv_handler (int signal_number, siginfo_t *si, void *context)
{
  int pagesize = getpagesize();
  if (TD_GET(inside_hpcrun) && si && si->si_addr) {
    void *p = (void *)(((uint64_t)(uintptr_t) si->si_addr) & ~(pagesize-1));
    mprotect (p, pagesize, PROT_READ | PROT_WRITE);
    return;
  }
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

    sampling_info_t info;
    struct datacentric_info_s data_aux = {.data_node = data_node, .first_touch = true};

    info.sample_clock = 0;
    info.sample_custom_cct.update_before_fn = hpcrun_cct_record_datacentric;
    info.sample_custom_cct.update_after_fn  = 0;
    info.sample_custom_cct.data_aux         = &data_aux;

    TMSG(DATACENTRIC, "found node %p, p: %p, context: %p", data_node, p, context);

    hpcrun_sample_callpath(context, alloc_metric_id, 	(hpcrun_metricVal_t) {.i=1},
                            0/*skipInner*/, 0/*isSync*/, &info);
  }
  else {
    TMSG(DATACENTRIC, "NOT found node %p", context);
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

static void
datacentric_register(event_info_t *event_desc)
{
  if (alloc_metric_id >= 0)
    return; // been registered

  alloc_metric_id = hpcrun_new_metric();
  free_metric_id = hpcrun_new_metric();

  hpcrun_set_metric_info(alloc_metric_id, "Bytes Allocated");
  hpcrun_set_metric_info(free_metric_id, "Bytes Freed");

  // set and initialize segv signal
  set_segv_handler();

  struct event_threshold_s threshold = init_default_count();

  // hardware-specific data centric setup
  datacentric_hw_register(event_desc, &threshold);
}

static void
datacentric_handler(event_thread_t *current_event, sample_val_t sv,
    perf_mmap_data_t *mmap_data)
{
  TMSG(DATACENTRIC, "datacentric handler starts");
  TMSG(DATACENTRIC, "datacentric handler ends");
}

/******************************************************************************
 * method definitions
 *****************************************************************************/


void
datacentric_init()
{
  // reset static variables to their virgin state
  alloc_metric_id = -1;
  free_metric_id = -1;

  event_custom_t *event_datacentric = hpcrun_malloc(sizeof(event_custom_t));
  event_datacentric->name         = EVNAME_DATACENTRIC;
  event_datacentric->desc         = "Experimental counter: counting the memory latency.";
  event_datacentric->register_fn  = datacentric_register;   // call backs
  event_datacentric->handler_fn   = datacentric_handler;
  event_datacentric->metric_index = 0;        // these fields to be defined later
  event_datacentric->metric_desc  = NULL;     // these fields to be defined later
  event_datacentric->handle_type  = EXCLUSIVE;// call me only for my events

  event_custom_register(event_datacentric);
}




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
  return alloc_metric_id >= 0;
}



void
hpcrun_datacentric_free_inc(cct_node_t* node, int incr)
{
  if (node != NULL) {
    cct_metric_data_increment(free_metric_id,
			      node,
			      (cct_metric_data_t){.i = incr});
  }
}

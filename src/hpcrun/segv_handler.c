// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
// system include files
//***************************************************************************

#define _GNU_SOURCE

#include <sys/types.h>
#include <stddef.h>
#include <ucontext.h>
#include <signal.h>
#include <dlfcn.h>
#include <setjmp.h>
#include <signal.h>


//***************************************************************************
// libmonitor include files
//***************************************************************************

#include "libmonitor/monitor.h"


//***************************************************************************
// user include files
//***************************************************************************

#include "../common/lean/queue.h" // Singly-linkled list macros

#include "main.h"
#include "thread_data.h"
#include "handling_sample.h"
#include "hpcrun_stats.h"

#include "memchk.h"

#include "messages/messages.h"
#include "utilities/arch/context-pc.h"

#include "segv_handler.h"

//*************************** MACROS **************************



//*************************** type data structure **************************

// list of callbacks
typedef struct segv_list_s {
  hpcrun_sig_callback_t callback;
  SLIST_ENTRY(segv_list_s) entries;
} segv_list_t;


//*************************** Local variables **************************

static SLIST_HEAD(segv_list_head, segv_list_s) list_cb_head =
        SLIST_HEAD_INITIALIZER(segv_list_head);

//***************************************************************************
// catch SIGSEGVs
//***************************************************************************

// FIXME: tallent: should this be together with hpcrun_drop_sample?

int
hpcrun_sigsegv_handler(int sig, siginfo_t* siginfo, void* context)
{
  if (hpcrun_is_handling_sample()) {
    hpcrun_stats_num_samples_segv_inc();

    thread_data_t *td = hpcrun_get_thread_data();

    // -----------------------------------------------------
    // print context
    // -----------------------------------------------------
    if (ENABLED(UNW_SEGV_STOP)){
      EMSG("Unwind segv abort enabled ... Aborting!!");
      hpcrun_terminate();
    }

    EMSG("error: segv encountered");
    // TODO: print more context details

    // -----------------------------------------------------
    // longjump, if possible
    // -----------------------------------------------------
    sigjmp_buf_t *it = NULL;

    if (td->current_jmp_buf == &(td->bad_interval)) {
      it = &(td->bad_interval);

    } else if (td->current_jmp_buf == &(td->bad_unwind)) {
      it = &(td->bad_unwind);
      if (memchk((void *)it, '\0', sizeof(*it))) {
        EMSG("error: segv handler: invalid jmpbuf");
        // N.B. to handle this we need an 'outer' jump buffer that captures
        // the context right as we enter the sampling-trigger signal handler.
        hpcrun_terminate();
      }
    }

    hpcrun_bt_dump(td->btbuf_cur, "SEGV");

    // call clean-up callback functions
    segv_list_t *item;
    SLIST_FOREACH(item, &list_cb_head, entries) {
      if (item->callback != NULL)
        (*item->callback)();
    }

    siglongjmp(it->jb, 9);
    return 0;
  }
  else {
    // pass segv to another handler
    TMSG(SEGV, "NON unwind segv encountered");
    return 1;
  }
}


int
hpcrun_setup_segv()
{
  int ret = monitor_sigaction(SIGBUS, &hpcrun_sigsegv_handler, 0, NULL);
  if (ret != 0) {
    EMSG("Unable to install SIGBUS handler", __FILE__, __LINE__);
  }

  ret = monitor_sigaction(SIGSEGV, &hpcrun_sigsegv_handler, 0, NULL);
  if (ret != 0) {
    EMSG("Unable to install SIGSEGV handler", __FILE__, __LINE__);
  }

  return ret;
}

// ------------------------------------------------------------
// ------------------------------------------------------------

// Interface for callback registrations when a segv occurs.
// The callback function will be called when a segv happens.
// Returns: 0 if the function is already registered,
//      1 if the function is now added to the list
//      -1 if there's something wrong
// Warnning: this function is not thread safe.

int
hpcrun_segv_register_cb( hpcrun_sig_callback_t cb )
{
  segv_list_t *list_item = NULL;

  // searching if the callback is already registered
  SLIST_FOREACH(list_item, &list_cb_head, entries) {
    if (list_item->callback == cb)
      return 0;
  }

  list_item = (segv_list_t*) hpcrun_malloc(sizeof(segv_list_t));
  if (list_item == NULL)
        return -1;
  list_item->callback = cb;

  // add the callback into the list
  SLIST_INSERT_HEAD(&list_cb_head, list_item, entries);
  return 1;
}

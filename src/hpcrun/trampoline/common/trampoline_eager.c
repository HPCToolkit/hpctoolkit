// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// File: trampoline_eager.c
//
// Purpose: unwind optimization & iteration-based analysis on PPC.
//
// Implemented by marking the last bit of RA on the stack / in the register.
// Currently only supports PPC. May be extended to architectures that ignore
// the last bit of RA.
//
// Modification History:
//   2015/11/07 - created - Lai Wei and John Mellor-Crummey
//******************************************************************************

//******************************************************************************
// system includes
//******************************************************************************

#define _GNU_SOURCE

#include <stdbool.h>

//******************************************************************************
// local includes
//******************************************************************************

#include "trampoline.h"
#include "../../thread_data.h"
#include "../../cct/cct.h"
#include "../../messages/messages.h"
#include "../../safe-sampling.h"
#include "../../sample_event.h"
#include "../../sample-sources/retcnt.h"
#include "../../libmonitor/monitor.h"

extern bool hpcrun_get_retain_recursion_mode();

//******************************************************************************
// macros
//******************************************************************************

#define RETURN_ADDRESS_IS_MARKED(addr) (((unsigned long) addr) & 1)



//******************************************************************************
// external declarations
//******************************************************************************

extern void hpcrun_trampoline();
extern void hpcrun_trampoline_end();


//******************************************************************************
// interface operations
//******************************************************************************

void
hpcrun_trampoline_bt_dump(void)
{
  thread_data_t* td = hpcrun_get_thread_data();

  TMSG(TRAMP, "Num frames cached = %d ?= %d (cached_counter)",
       td->cached_bt_buf_frame_end - td->cached_bt_frame_beg, td->cached_frame_count);
  for (frame_t* f = td->cached_bt_frame_beg; f < td->cached_bt_buf_frame_end; f++) {
      TMSG(TRAMP, "frame ra_loc = %p, ra@loc = %p", f->ra_loc,
              f->ra_loc == NULL ? NULL : *((void**) f->ra_loc));
  }
}


void
hpcrun_init_trampoline_info(void)
{
  thread_data_t* td   = hpcrun_get_thread_data();

  TMSG(TRAMP, "INIT called, tramp state zeroed");
  td->tramp_present   = false;
  td->tramp_retn_addr = NULL;
  td->tramp_loc       = NULL;
  td->tramp_cct_node  = NULL;
}

bool hpcrun_trampoline_update(frame_t* stop_frame)
{
  thread_data_t* td = hpcrun_get_thread_data();

  if (!td->tramp_present) return false;

  frame_t* frame = td->tramp_frame;
  cct_node_t* node = td->tramp_cct_node;
  size_t count = td->cached_frame_count;
  uint32_t dLCA = 0;

  // Locate which frame is marked
  while (count > 0) {
    if (frame->ra_loc == stop_frame->ra_loc)
      break;

    cct_node_t* parent = (node) ? hpcrun_cct_parent(node) : NULL;
    if (  !hpcrun_get_retain_recursion_mode()
       && frame != td->cached_bt_frame_beg
       && frame != td->cached_bt_buf_frame_end-1
       && ip_normalized_eq(&(frame->the_function), &((frame-1)->the_function))
       && ip_normalized_eq(&(frame->the_function), &((frame+1)->the_function))
       )
      parent = node;
    else
      dLCA++;
    node = parent;

    frame++;
    count--;
  }

  // Check if valid
  if (count == 0) return false;
  if (node == NULL) return false;

  // Update corresponding values
  TMSG(TRAMP, "Marked addr located at node %p, cached frame count reduced from %d to %d",
          node, td->cached_frame_count, count);
  td->tramp_frame = frame;
  td->tramp_cct_node = node;
  td->cached_frame_count = count;
  td->dLCA += dLCA;
  return true;
}


bool
hpcrun_trampoline_interior(void* addr)
{
  return false;
}


// returns true iff at first address of trampoline code.
bool
hpcrun_trampoline_at_entry(void* addr)
{
  return RETURN_ADDRESS_IS_MARKED(addr);
}


void
hpcrun_trampoline_insert(cct_node_t* node)
{
  TMSG(TRAMP, "Mark start at node %p", node);
  thread_data_t* td = hpcrun_get_thread_data();
  if (! td->tramp_frame) {
    TMSG(TRAMP, " **No tramp frame: init tramp info");
    hpcrun_init_trampoline_info();
    return;
  }

  td->tramp_cct_node = node;
  td->tramp_present = true;

  frame_t* frame = td->tramp_frame;

  while (frame < td->cached_bt_buf_frame_end) {
    void* ra_loc = frame->ra_loc;
    if (!node) {
      TMSG(TRAMP, "**Tramp frame CCT node = NULL: init tramp info.");
      hpcrun_init_trampoline_info();
      return;
    }
    if (ra_loc) {
      void* ra = *((void**) ra_loc);
      if (RETURN_ADDRESS_IS_MARKED(ra)) {
          frame->ra_val = ra; // ra_val of the first RA marked frame may have been reset. Take a copy of the value of marked RA
                  break; // RA has already been marked
          }

      *((void**) ra_loc) = (void*)((unsigned long)ra | 1);
      frame->ra_val = *((void**) ra_loc); // Take a copy of the value of marked RA
      TMSG(TRAMP, "Marked return addr @ %p = %p", ra_loc, frame->ra_val);

      hpcrun_retcnt_inc(node, 1); // Increasing return count eagerly.
    } else {
      TMSG(TRAMP, "Tramp frame ra loc = NULL, skip marking this frame");
    }

    cct_node_t* parent = (node) ? hpcrun_cct_parent(node) : NULL;
    if (  !hpcrun_get_retain_recursion_mode()
       && frame != td->cached_bt_frame_beg
       && frame != td->cached_bt_buf_frame_end-1
       && ip_normalized_eq(&(frame->the_function), &((frame-1)->the_function))
       && ip_normalized_eq(&(frame->the_function), &((frame+1)->the_function))
       )
      parent = node;
    node = parent;
    frame++;
  }

  TMSG(TRAMP, "#%d frames are marked", frame - td->tramp_frame);
}


void
hpcrun_trampoline_remove(void)
{
  hpcrun_init_trampoline_info();
}

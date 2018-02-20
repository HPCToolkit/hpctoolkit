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
// Copyright ((c)) 2002-2015, Rice University
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
//   2005/11/07 - created - Lai Wei and John Mellor-Crummey
//******************************************************************************

//******************************************************************************
// system includes
//******************************************************************************

#include <stdbool.h>

//******************************************************************************
// local includes
//******************************************************************************

#include "trampoline.h"
#include <hpcrun/thread_data.h>
#include <cct/cct.h>
#include <messages/messages.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <sample-sources/retcnt.h>
#include <monitor.h>


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
       td->cached_bt_end - td->cached_bt, td->cached_frame_count);
  for (frame_t* f = td->cached_bt; f < td->cached_bt_end; f++) {
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

static bool hpcrun_trampoline_update(void* addr) 
{
  thread_data_t* td = hpcrun_get_thread_data();

  frame_t* frame = td->tramp_frame;
  cct_node_t* node = td->tramp_cct_node;
  size_t count = td->cached_frame_count;

  // Locate which frame is marked
  while (count > 0) {
    if (frame->ra_loc != NULL)
      if (frame->ra_val == addr) break;
    
    frame++;
    count--;
    node = (node) ? hpcrun_cct_parent(node) : NULL;
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
  bool ret = ((unsigned long)addr & 1 == 1);
  if (ret) {
    ret = hpcrun_trampoline_update(addr);
    if (!ret) TMSG(TRAMP, "Marked addr found but failed to locate corresponding frame");
  }
  return ret;
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

  while (frame < td->cached_bt_end) {
    void* ra_loc = frame->ra_loc;
    if (!node) {
      TMSG(TRAMP, "**Tramp frame CCT node = NULL: init tramp info.");
      hpcrun_init_trampoline_info();
      return;
    }
    if (!ra_loc) {
      TMSG(TRAMP, "Tramp frame ra loc = NULL, skip marking this frame");
      frame++;
      node = hpcrun_cct_parent(node);
      continue;
    }
    void* ra = *((void**) ra_loc);
    if ((unsigned long)ra & 1 == 1) break; // RA has already been marked
   
    *((void**) ra_loc) = (void*)((unsigned long)ra | 1);
    frame->ra_val = *((void**) ra_loc); // Take a copy of the value of marked RA
    TMSG(TRAMP, "Marked return addr @ %p = %p", ra_loc, frame->ra_val);

    hpcrun_retcnt_inc(node, 1); // Increasing return count eagerly.
    
    frame++;
    node = hpcrun_cct_parent(node);
  }

  TMSG(TRAMP, "#%d frames are marked", frame - td->tramp_frame);
}


void
hpcrun_trampoline_remove(void)
{
  hpcrun_init_trampoline_info();
}


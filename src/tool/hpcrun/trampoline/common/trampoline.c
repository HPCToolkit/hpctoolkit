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
// Copyright ((c)) 2002-2016, Rice University
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
// File: trampoline.c
//
// Purpose: architecture independent support for counting returns of sampled
//          frames using trampolines
//
// Modification History:
//   2009/09/15 - created - Mike Fagan and John Mellor-Crummey
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
    TMSG(TRAMP, "ra loc = %p, ra@loc = %p", f->ra_loc, *((void**) f->ra_loc));
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


// returns true if address is inside the assembly language trampoline code;
// returns false if at first address of trampoline code or outside.
bool
hpcrun_trampoline_interior(void* addr)
{
  return ((void*)hpcrun_trampoline < addr 
	  && addr <= (void*)hpcrun_trampoline_end);
}


// returns true iff at first address of trampoline code. 
bool
hpcrun_trampoline_at_entry(void* addr)
{
  return (addr == hpcrun_trampoline);
}


static bool
ok_to_advance(void* target, void* current)
{
  return (target > current) && (target < monitor_stack_bottom());
}

static cct_node_t*
hpcrun_trampoline_advance(void)
{
  thread_data_t* td = hpcrun_get_thread_data();
  cct_node_t* node = td->tramp_cct_node;
  void* current_frame_sp = td->tramp_frame->ra_loc;
  if (! td->cached_frame_count ) return NULL;

  TMSG(TRAMP, "Advance from node %p...", node);
  cct_node_t* parent = (node) ? hpcrun_cct_parent(node) : NULL;
  TMSG(TRAMP, " ... to node %p", parent);
  td->tramp_frame++;
  TMSG(TRAMP, "cached frame count reduced from %d to %d", td->cached_frame_count,
       td->cached_frame_count - 1);
  (td->cached_frame_count)--;
  if ( ! td->cached_frame_count ) {
    TMSG(TRAMP, "**cached frame count = 0");
  }
  else if (! ok_to_advance (td->tramp_frame->ra_loc, current_frame_sp) ) {
    EMSG("Encountered bad advance of trampoline ( target > stack_bottom or target < current\n"
	 "%p(target) %p(current) %p(bottom)",
	 td->tramp_frame->ra_loc, current_frame_sp, monitor_stack_bottom());
  }
  else if (! parent) {
    TMSG(TRAMP, "No parent node, trampoline self-removes");
  }
  else {
    TMSG(TRAMP, "... Trampoline advanced to %p", parent);
    hpcrun_trampoline_insert(parent);
    return parent;
  }
  TMSG(TRAMP,"*** trampoline self-removes ***");
  hpcrun_init_trampoline_info();
  return NULL;
}

void 
hpcrun_trampoline_insert(cct_node_t* node)
{
  TMSG(TRAMP, "insert into node %p", node);
  thread_data_t* td = hpcrun_get_thread_data();
  if (! td->tramp_frame) {
    TMSG(TRAMP, " **No tramp frame: init tramp info");
    hpcrun_init_trampoline_info();
    return;
  }
  void* addr        = td->tramp_frame->ra_loc;
  if (! addr) {
    TMSG(TRAMP, " **Tramp frame ra loc = NULL");
    hpcrun_init_trampoline_info();
    return;
  }

  TMSG(TRAMP, "Stack addr for retn addr = %p", addr);
  // save location where trampoline was placed
  td->tramp_loc       = addr;

  TMSG(TRAMP, "Actual return addr @ %p = %p", addr, *((void**) addr));
  // save the return address overwritten with trampoline address 
  td->tramp_retn_addr = *((void**) addr);

  *((void**)addr) = hpcrun_trampoline;
  td->tramp_cct_node = node;
  td->tramp_present = true;
}

void
hpcrun_trampoline_remove(void)
{
  thread_data_t* td = hpcrun_get_thread_data();
  if (td->tramp_present){
    TMSG(TRAMP, "removing live trampoline from %p", td->tramp_loc);
    TMSG(TRAMP, "confirm trampoline @ location: ra@tramp loc = %p == %p (tramp)",
	 *((void**)td->tramp_loc), hpcrun_trampoline);
    if (*((void**)td->tramp_loc) != hpcrun_trampoline) {
      EMSG("INTERNAL ERROR: purported trampoline location does NOT have a trampoline:"
	   " loc %p: %p != %p", td->tramp_loc, *((void**)td->tramp_loc), hpcrun_trampoline);
    }
    else {
      *((void**)td->tramp_loc) = td->tramp_retn_addr;
    }
  }
  hpcrun_init_trampoline_info();
}

void*
hpcrun_trampoline_handler(void)
{
  // probably not possible to get here from inside our code.
  hpcrun_safe_enter();

  TMSG(TRAMP, "Trampoline fired!");
  thread_data_t* td = hpcrun_get_thread_data();

  // get the address where we need to return
  void* ra = td->tramp_retn_addr;

  TMSG(TRAMP, " --real return addr returned to hpcrun_trampoline = %p", ra);
  hpcrun_retcnt_inc(td->tramp_cct_node, 1);

  TMSG(TRAMP, "About to advance trampoline ...");
#if OLD_TRAMP_ADV
  cct_node_t* n = hpcrun_trampoline_advance();
  TMSG(TRAMP, "... Trampoline advanced to %p", n);
  if (n)
    hpcrun_trampoline_insert(n);
  else {
    EMSG("NULL trampoline advance !!, trampoline removed");
    hpcrun_trampoline_remove();
  }
#endif
  hpcrun_trampoline_advance();
  hpcrun_safe_exit();

  return ra; // our assembly code caller will return to ra
}

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
#include <hpcrun/sample_event.h>
#include <sample-sources/retcnt.h>


//******************************************************************************
// external declarations
//******************************************************************************

extern void hpcrun_trampoline;
extern void hpcrun_trampoline_end;


//******************************************************************************
// interface operations
//******************************************************************************

void
hpcrun_init_trampoline_info(void)
{
  thread_data_t* td   = hpcrun_get_thread_data();

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
    return (&hpcrun_trampoline < addr && addr <= &hpcrun_trampoline_end);
}


// returns true iff at first address of trampoline code. 
bool
hpcrun_trampoline_at_entry(void* addr)
{
  return (addr == &hpcrun_trampoline);
}


cct_node_t*
hpcrun_trampoline_advance(void)
{
  thread_data_t* td = hpcrun_get_thread_data();
  cct_node_t* node = td->tramp_cct_node;
  TMSG(TRAMP, "Advance from node %p...", node);
  node = node->parent;
  TMSG(TRAMP, " ... to node %p", node);
  td->tramp_frame++;
  return node;
}


void 
hpcrun_trampoline_insert(cct_node_t* node)
{
  TMSG(TRAMP, "insert into node %p", node);
  thread_data_t* td = hpcrun_get_thread_data();
  void* addr        = td->tramp_frame->ra_loc;
  if (! addr) {
    hpcrun_init_trampoline_info();
    return;
  }

  // save location where trampoline was placed
  td->tramp_loc       = addr;

  // save the return address overwritten with trampoline address 
  td->tramp_retn_addr = *((void**) addr);

  *((void**)addr) = &hpcrun_trampoline;
  td->tramp_cct_node = node;
  td->tramp_present = true;
}


void
hpcrun_trampoline_remove(void)
{
  TMSG(TRAMP,"removed");
  thread_data_t* td = hpcrun_get_thread_data();
  if (td->tramp_present){
    *((void**)td->tramp_loc) = td->tramp_retn_addr;
  }
  hpcrun_init_trampoline_info();
}


void*
hpcrun_trampoline_handler(void)
{
  hpcrun_async_block();
  TMSG(TRAMP, "Trampoline fired!");
  thread_data_t* td = hpcrun_get_thread_data();

  // get the address where we need to return
  void* ra = td->tramp_retn_addr;

  hpcrun_retcnt_inc(td->tramp_cct_node, 1);

  cct_node_t* n = hpcrun_trampoline_advance();
  hpcrun_trampoline_insert(n);

  hpcrun_async_unblock();
  return ra; // our assembly code caller will return to ra
}

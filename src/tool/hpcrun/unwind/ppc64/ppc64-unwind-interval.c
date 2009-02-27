// -*-Mode: C++;-*- // technically C99
// $Id$

//***************************************************************************
//
// HPCToolkit's PPC64 Unwinder
//
//***************************************************************************

//************************* System Include Files ****************************

#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>


//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/uint.h>

#include "ppc64-unwind-interval.h"

#include "csprof-malloc.h"
#include "pmsg.h"
#include "atomic-ops.h"
#include "ui_tree.h"

#include "fnbounds_interface.h"

// FIXME: see note in mips-unwind.c
#include <lib/isa/instructionSets/ppc.h>


//*************************** Forward Declarations **************************

static interval_status 
ppc64_build_intervals(char *ins, unsigned int len);

static void 
ppc64_print_interval_set(unw_interval_t *first);

static const char *
ra_status_string(ra_loc l); 

static const char *
bp_status_string(bp_loc l);



//***************************************************************************
// global variables
//***************************************************************************

long ui_cnt = 0;
long suspicious_cnt = 0;



//***************************************************************************
// interface operations
//***************************************************************************

interval_status 
build_intervals(char *ins, unsigned int len)
{
  interval_status stat = ppc64_build_intervals(ins, len);

  ppc64_print_interval_set((unw_interval_t *) stat.first);

  return stat;
}


//***************************************************************************
// unw_interval_t interface
//***************************************************************************

unw_interval_t *
new_ui(char *start_addr,
       ra_loc ra_status, int ra_arg, bp_loc bp_status, 
       unw_interval_t *prev)
{
  unw_interval_t* u = (unw_interval_t*)csprof_ui_malloc(sizeof(unw_interval_t));

  u->common.start = start_addr;
  u->common.end = 0;
  u->common.prev = (splay_interval_t*)prev;
  u->common.next = NULL;

  if (prev) {
    ui_link(prev, u);
  }

  u->ra_status = ra_status;
  u->bp_status = bp_status;
  u->ra_arg    = ra_arg;

  fetch_and_add(&ui_cnt, 1);

  return u;
}


void 
ui_dump(unw_interval_t* u, int dump_to_stdout)
{
  TMSG(INTV, "start=%p end=%p ra_ty=%s bp_ty=%s ra_arg=%d next=%p prev=%p\n",
       (void *) u->common.start, (void *) u->common.end, 
       ra_status_string(u->ra_status), bp_status_string(u->bp_status), 
       u->ra_arg, u->common.next, u->common.prev);
}


//***************************************************************************

long 
ui_count(void)
{
  return ui_cnt;
}


long 
suspicious_count(void)
{
  return suspicious_cnt;
}


void
suspicious_interval(void *pc) 
{
  EMSG("suspicous interval for pc = %p", pc);
  fetch_and_add(&suspicious_cnt,1);
}


void 
ui_link(unw_interval_t* current, unw_interval_t* next)
{
  current->common.end = next->common.start;
  current->common.next= (splay_interval_t*)next;
}



//***************************************************************************
// private operations 
//***************************************************************************

#define STR(s) case s: return #s

static const char *
ra_status_string(ra_loc l) 
{
  switch(l) {
   STR(RA_SP_RELATIVE);
   STR(RA_STD_FRAME);
   STR(RA_BP_FRAME);
   STR(RA_REGISTER);
   STR(RA_POISON);
  default:
    assert(0);
  }
  return NULL;
}


static const char *
bp_status_string(bp_loc l)
{
  switch(l){
    STR(BP_UNCHANGED);
    STR(BP_SAVED);
    STR(BP_HOSED);
  default:
    assert(0);
  }
}


#if 0
static const char *
register_name(int reg)
{
   switch(reg) {
   case PPC_REG_LR: return "lr";
   case PPC_REG_R0: return "r0";
   default:          assert(0);
   }
   return NULL;
}
#endif


//***************************************************************************
// build_intervals: helpers
//***************************************************************************


//***************************************************************************
// build_intervals:
//***************************************************************************

// Typical PPC frames:
//   stwu r1, -32(r1)   ! store with update: store stack pointer (R1) at
//                      !   -32(r1) and *then* set R1 to -32(r1)
//   mflr r0            ! move from LR: (and store into R0)
//   stw  r0, 36(r1)    ! store word: store LR (return addr) in *parent* frame
//
//   // ... do some stuff ...
//
//   lwz r0, 36(r1)     ! load LR
//   mtlr r0            ! mtlr: move to LR (from r0)
//   blr                ! blr: return!
// 
// Observe:
//   1. Before mflr, RA is in LR and R1
//   1. After stw,   RA is in parent's frame; parent 

// states:
//   ra in LR, BP_UNCHANGED, RA_REGISTER
//   parent's BP in R1 - BP_UNCHANGED, RA_REGISTER
//   my BP in R1 - BP_SAVED, RA_REGISTER
//   ra relative to bp, BP_SAVED, BP_FRAME

static interval_status 
ppc64_build_intervals(char *beg_insn, unsigned int len)
{
#define NEXT_INS        ((char *) (cur_insn + 1))

  interval_status stat;

  unw_interval_t* beg_ui = new_ui(beg_insn, RA_REGISTER, PPC_REG_LR, 
				  BP_UNCHANGED, NULL);
  unw_interval_t* ui = beg_ui;
  unw_interval_t* nxt_ui = NULL;
  unw_interval_t* canon_ui = beg_ui;

  int frame_size = 0;
  int ra_disp = 0;

  uint32_t* cur_insn = (uint32_t*) beg_insn;
  uint32_t* end_insn = (uint32_t*) (beg_insn + len);

  while (cur_insn < end_insn) {
    TMSG(INTV,"trying 0x%x [%p,%p)\n", *cur_insn, cur_insn, end_insn);

    //--------------------------------------------------
    // move return address from LR to R0
    //--------------------------------------------------
    if (PPC_OP_MFLR_R0(*cur_insn)) {
      nxt_ui = new_ui(NEXT_INS, RA_REGISTER, PPC_REG_R0, ui->bp_status, ui);
      ui = nxt_ui;
    }
    
    //--------------------------------------------------
    // store return address into parent's frame
    // we don't need to remember the offset for the return address
    // because we can just get it out of the parent's frame
    //--------------------------------------------------
    else if (PPC_OP_STW_R0_R1(*cur_insn)) {
      short ra_disp = PPC_OPND_DISP(*cur_insn);
      if ((frame_size + 4 == (int) ra_disp) && (ra_disp > 4)) {
        nxt_ui = new_ui(NEXT_INS, RA_BP_FRAME, 0, BP_SAVED, ui);
        if (canon_ui == beg_ui) canon_ui = nxt_ui;
        ui = nxt_ui;
      }
    }

    //--------------------------------------------------
    // set up my frame
    // NOTE: return address remains in the register that 
    //       it was in before 
    //--------------------------------------------------
    else if (PPC_OP_STWU_R1_R1(*cur_insn)) {
      nxt_ui = new_ui(NEXT_INS, RA_REGISTER, ui->ra_arg, BP_SAVED, ui);
      frame_size = - PPC_OPND_DISP(*cur_insn);
      ui = nxt_ui;
    }

    //--------------------------------------------------
    // move return address from R0 to LR
    //--------------------------------------------------
    else if (PPC_OP_MTLR_R0(*cur_insn)) {
      nxt_ui = new_ui(NEXT_INS, RA_REGISTER, PPC_REG_LR, ui->bp_status, ui);
      ui = nxt_ui;
    }

    //--------------------------------------------------
    // reset frame pointer to caller's frame
    //--------------------------------------------------
    else if (PPC_OP_MR_R1(*cur_insn) || 
	     (PPC_OP_ADDI_R1(*cur_insn) 
	      && (PPC_OPND_DISP(*cur_insn) == frame_size))) {
      // assume the mr restores the proper value; we would have to track 
      // register values to be sure.
      nxt_ui = new_ui(NEXT_INS, ui->ra_status, ui->ra_arg, BP_UNCHANGED, ui);
      ui = nxt_ui;
    }

    //--------------------------------------------------
    // load return address into R0 from caller's frame
    //--------------------------------------------------
    else if (PPC_OP_LWZ_R0_R1(*cur_insn) 
	     && PPC_OPND_DISP(*cur_insn) == ra_disp) {
      nxt_ui = new_ui(NEXT_INS, RA_REGISTER, PPC_REG_R0, ui->bp_status, ui);
      ui = nxt_ui;
    }
    
    else if (PPC_OP_BLR(*cur_insn) && (cur_insn + 1 < end_insn)) {
      // a return, but not the last instruction in the routine
      if ((ui->ra_status != canon_ui->ra_status) &&
          (ui->ra_arg    != canon_ui->ra_arg) &&
          (ui->bp_status != canon_ui->bp_status)) {
         // don't make a new interval if it is the same as the present one.
         nxt_ui = new_ui(NEXT_INS, canon_ui->ra_status, canon_ui->ra_arg, 
                       canon_ui->bp_status, ui);
         ui = nxt_ui;
      }
    }

    cur_insn++;
  }

  ui->common.end = end_insn;

  stat.first_undecoded_ins = NULL;
  stat.errcode = 0;
  stat.first = (splay_interval_t *) beg_ui;

  return stat; 
}


static void 
ppc64_print_interval_set(unw_interval_t *beg_ui) 
{
  for (unw_interval_t* u = beg_ui; u; u = (unw_interval_t*)u->common.next) {
    ui_dump(u, 1);
  }
}


static void GCC_ATTR_UNUSED
ppc64_dump_intervals(char  *addr)
{
  void *s, *e;
  interval_status intervals;

  fnbounds_enclosing_addr(addr, &s, &e);

  uintptr_t llen = ((uintptr_t)e) - (uintptr_t)s;

  printf("build intervals from %p to %p (%"PRIuPTR")\n", s, e, llen);
  intervals = ppc64_build_intervals(s, (unsigned int) llen);

  ppc64_print_interval_set((unw_interval_t *) intervals.first);
}

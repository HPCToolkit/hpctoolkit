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
ra_ty_string(ra_ty_t l); 

static const char *
sp_ty_string(sp_ty_t l);



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
       ra_ty_t ra_ty, int ra_arg, sp_ty_t sp_ty, 
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

  u->ra_ty  = ra_ty;
  u->sp_ty  = sp_ty;
  u->ra_arg = ra_arg;

  fetch_and_add(&ui_cnt, 1);

  return u;
}


void 
ui_dump(unw_interval_t* u)
{
  if (!u) {
    return;
  }

  TMSG(INTV, "intv: start=%p end=%p ra_ty=%s sp_ty=%s ra_arg=%d next=%p prev=%p",
       (void *) u->common.start, (void *) u->common.end, 
       ra_ty_string(u->ra_ty), sp_ty_string(u->sp_ty), 
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
ra_ty_string(ra_ty_t l) 
{
  switch (l) {
   STR(RATy_NULL);
   STR(RATy_SPRel);
   STR(RATy_Reg);
  default:
    assert(0);
  }
  return NULL;
}


static const char *
sp_ty_string(sp_ty_t l)
{
  switch (l) {
    STR(SPTy_NULL);
    STR(SPTy_SPRel);
    STR(SPTy_Reg);
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

#define PPC_FRAME_LINKAGE_RA_OFST (sizeof(void*))

#define INSN(insn) ((char*)(insn))

static inline char*
nextInsn(uint32_t* insn) 
{ return INSN(insn + 1); }


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
// Notes:
// - When an alloca occurs, r0 remains the stack pointer for the
//   constant-sized portion of the frame and another register is used as the
//   stack pointer for the dynamically sized portion.
//
// States:
//   1. RA is in register (LR/R0); SP (r1) points to parent's frame
//   2. RA is in register (LR/R0); SP (r1) has been stored & updated
//   3. RA is SP-relative        ; SP (r1) has been stored & updated


static interval_status 
ppc64_build_intervals(char *beg_insn, unsigned int len)
{
  unw_interval_t* beg_ui = new_ui(beg_insn, RATy_Reg, PPC_REG_LR, SPTy_Reg, 
				  NULL);
  unw_interval_t* ui = beg_ui;
  unw_interval_t* nxt_ui = NULL;
  unw_interval_t* canon_ui = beg_ui;

  int frame_size = 0;
  int ra_disp = 0;

  uint32_t* cur_insn = (uint32_t*) beg_insn;
  uint32_t* end_insn = (uint32_t*) (beg_insn + len);

  while (cur_insn < end_insn) {
    TMSG(INTV, "insn: 0x%x [%p,%p)", *cur_insn, cur_insn, end_insn);

    //--------------------------------------------------
    // move return address from LR to R0
    //--------------------------------------------------
    if (PPC_OP_MFLR_R0(*cur_insn)) {
      nxt_ui = new_ui(nextInsn(cur_insn), RATy_Reg, PPC_REG_R0, ui->sp_ty, ui);
      ui = nxt_ui;
    }
    
    //--------------------------------------------------
    // store return address into parent's frame
    //--------------------------------------------------
    else if (PPC_OP_STW_R0_R1(*cur_insn)) {
      ra_disp = PPC_OPND_DISP(*cur_insn);
      // we do not need to remember the offset for the return address
      // because we can just pluck it out of the parent's frame
      if ((frame_size + PPC_FRAME_LINKAGE_RA_OFST == ra_disp) 
	  && (ra_disp > PPC_FRAME_LINKAGE_RA_OFST)) {
        nxt_ui = new_ui(nextInsn(cur_insn), RATy_SPRel, 0, SPTy_SPRel, ui);
        if (canon_ui == beg_ui) {
	  canon_ui = nxt_ui;
	}
        ui = nxt_ui;
      }
    }

    //--------------------------------------------------
    // create frame (adjust SP) and store parent's SP
    //--------------------------------------------------
    else if (PPC_OP_STWU_R1_R1(*cur_insn)) {
      // NOTE: return address remains same register
      frame_size = - PPC_OPND_DISP(*cur_insn);
      nxt_ui = new_ui(nextInsn(cur_insn), 
		      ui->ra_ty, ui->ra_arg, SPTy_SPRel, ui);
      ui = nxt_ui;
    }

    //--------------------------------------------------
    // move return address from R0 to LR
    //--------------------------------------------------
    else if (PPC_OP_MTLR_R0(*cur_insn)) {
      nxt_ui = new_ui(nextInsn(cur_insn), RATy_Reg, PPC_REG_LR, ui->sp_ty, ui);
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
      nxt_ui = new_ui(nextInsn(cur_insn), ui->ra_ty, ui->ra_arg, SPTy_Reg, ui);
      ui = nxt_ui;
    }

    //--------------------------------------------------
    // load return address into R0 from caller's frame
    //--------------------------------------------------
    else if (PPC_OP_LWZ_R0_R1(*cur_insn) 
	     && PPC_OPND_DISP(*cur_insn) == ra_disp) {
      nxt_ui = new_ui(nextInsn(cur_insn), RATy_Reg, PPC_REG_R0, ui->sp_ty, ui);
      ui = nxt_ui;
    }
    
    //--------------------------------------------------
    // interior returns/epilogues
    //--------------------------------------------------
    else if (PPC_OP_BLR(*cur_insn) && (cur_insn + 1 < end_insn)) {
      // An interior return.  Restore the canonical interval if necessary.
      if (!ui_cmp(ui, canon_ui)) {
	nxt_ui = new_ui(nextInsn(cur_insn), canon_ui->ra_ty, canon_ui->ra_arg, 
			canon_ui->sp_ty, ui);
	ui = nxt_ui;
      }
    }
    
    cur_insn++;
  }

  ui->common.end = end_insn;

  interval_status stat;
  stat.first_undecoded_ins = NULL;
  stat.errcode = 0;
  stat.first = (splay_interval_t *) beg_ui;

  return stat; 
}


static void 
ppc64_print_interval_set(unw_interval_t *beg_ui) 
{
  TMSG(INTV, "");
  for (unw_interval_t* u = beg_ui; u; u = (unw_interval_t*)u->common.next) {
    ui_dump(u);
  }
  TMSG(INTV, "");
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

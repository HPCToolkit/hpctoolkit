// -*-Mode: C++;-*- // technically C99
// $Id$

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdbool.h>

//*************************** User Include Files ****************************

#include <include/general.h>

// FIXME: Abuse the isa library by cherry-picking this special header.
// One possibility is to simply use the ISA lib -- doesn't xed
// internally use C++ stuff?
#include <lib/isa/instructionSets/mips.h>

#include "pmsg.h"

#include "splay-interval.h"

#include "mips-unwind-interval.h"

//*************************** Forward Declarations **************************

static interval_status 
mips64_build_intervals(uint32_t* beg_insn, uint32_t* end_insn, int verbose);


//***************************************************************************
// interface operations
//***************************************************************************

interval_status 
build_intervals(char* insn_beg, unsigned int len)
{
  // [my_insn_beg, my_insn_end)
  uint32_t* my_insn_beg = (uint32_t*)(insn_beg);
  uint32_t* my_insn_end = (uint32_t*)(insn_beg + len);

  interval_status x = mips64_build_intervals(my_insn_beg, my_insn_end, 0);
  return x;
}


//***************************************************************************
//
//***************************************************************************

#define INSN(insn) ((char*)insn)

static inline char*
nextInsn(uint32_t* insn) { return INSN(insn + 1); }


static inline bool 
isAdjustSPByConst(uint32_t insn)
{
  // example: daddiu sp,sp,-96
  uint32_t op = (insn & OP_MASK);
  if (op == DADDIU || op == ADDIU || op == ADDI || op == DADDI) {
    // The non-'U' probably never occur since they trap on overflow.
    return (REG_S(insn) == REG_SP) && (REG_T(insn) == REG_SP);
  }
  return false;
}

// positive value means frame allocation
static inline int
getAdjustSPByConstAmnt(uint32_t insn) { return - (int16_t)IMM(insn); }


static inline bool 
isStoreRegInFrame(uint32_t insn, int reg_base, int reg_src)
{
  // example: sd ra, 64(sp)
  // to be frame-relevant, reg_base should be REG_SP or REG_FP
  uint32_t op = (insn & OP_MASK);
  if (op == SD || op == SW) {
    return (REG_S(insn) == reg_base) && (REG_T(insn) == reg_src);
  }
  return false;
}

static inline int
getStoreRegInFrameOffset(uint32_t insn) { return (int16_t)IMM(insn); }


static inline bool 
isLoadRegFromFrame(uint32_t insn, int reg_base, int reg_dest)
{
  // example: ld ra, 64(sp)
  // to be frame-relevant, reg_base should be REG_SP or REG_FP
  uint32_t op = (insn & OP_MASK);
  if (op == LD || op == LW) {
    return (REG_S(insn) == reg_base) && (REG_T(insn) == reg_dest);
  }
  return false;
}


static inline bool 
isJumpToReg(uint32_t insn, int reg_to)
{
  if ( (insn & OP_MASK) == OPSpecial && (insn & OPSpecial_MASK) == JR ) {
    return (REG_S(insn) == reg_to);
  }
  return false;
}


//***************************************************************************

// MIPS is very regular. 
// - Calls (JAL/JALR/B*AL) store the return address in REG_RA.  (JALR
//   may be used to store the return address in a different register.)
// - Thus, on procedure entry, the return address is usually in REG_RA.
// - Frames are typically SP-relative (except for alloca's)
// - If a call is made, REG_RA is stored in the frame; after the call
//   and before a return is reloaded.

static interval_status 
mips64_build_intervals(uint32_t* beg_insn, uint32_t* end_insn, int verbose)
{
#if 0
  unwind_interval* beg_ui = new_ui(INSN(beg_insn),
				   RA_SP_RELATIVE, 0, 0,
				   BP_HOSED, 0, 0, NULL);
  unwind_interval* ui = beg_ui;
  
#else 

  unwind_interval* beg_ui = new_ui(INSN(beg_insn),
				   RA_REGISTER, 0, REG_RA/*FIXME:abuse*/,
				   BP_UNCHANGED, 0, 0, NULL);
  unwind_interval* ui = beg_ui;
  unwind_interval* nxt_ui = NULL;
  unwind_interval* canon_ui = beg_ui;

  int frame_sz = 0;

  uint32_t* cur_insn = beg_insn;
  while (cur_insn < end_insn) {
    //printf("trying 0x%x [%p,%p\n",*cur_insn, cur_insn, end_insn);

    //--------------------------------------------------
    // allocate/deallocate frame by adjusting stack pointer a fixed amount
    //--------------------------------------------------
    if (isAdjustSPByConst(*cur_insn)) {
      int amnt = getAdjustSPByConstAmnt(*cur_insn);
      frame_sz += amnt;

      int sp_ra_pos = ui->sp_ra_pos;
      if (ui->ra_status == RA_SP_RELATIVE) {
	sp_ra_pos += amnt;
      }

      nxt_ui = new_ui(nextInsn(cur_insn), 
		      ui->ra_status, sp_ra_pos, ui->bp_ra_pos,
		      BP_UNCHANGED, ui->sp_bp_pos, ui->bp_bp_pos, ui);
      link_ui(ui, nxt_ui); ui = nxt_ui;
    }
    //--------------------------------------------------
    // store return address into SP frame
    //--------------------------------------------------
    else if (isStoreRegInFrame(*cur_insn, REG_SP, REG_RA)) {
      int sp_ra_pos = getStoreRegInFrameOffset(*cur_insn);
      nxt_ui = new_ui(nextInsn(cur_insn),
		      RA_SP_RELATIVE, sp_ra_pos, 0,
		      BP_UNCHANGED, ui->sp_bp_pos, ui->bp_bp_pos, ui);
      link_ui(ui, nxt_ui); ui = nxt_ui;

      if (canon_ui == beg_ui) {
	canon_ui = nxt_ui;
      }
    }
    //--------------------------------------------------
    // load return address from SP frame
    //--------------------------------------------------
    else if (isLoadRegFromFrame(*cur_insn, REG_SP, REG_RA)) {
      nxt_ui = new_ui(nextInsn(cur_insn),
		      RA_REGISTER, 0, REG_RA/*FIXME:abuse*/,
		      BP_UNCHANGED, ui->sp_bp_pos, ui->bp_bp_pos, ui);
      link_ui(ui, nxt_ui); ui = nxt_ui;
    }
    //--------------------------------------------------
    // interior epilogues/returns
    //--------------------------------------------------
    else if (isJumpToReg(*cur_insn, REG_RA)
	&& ((cur_insn + 1/*delay slot*/ + 1) < end_insn)) {
      // An interior return.  Restore the canonical interval if necessary.
      if ((ui->ra_status != canon_ui->ra_status) &&
	  (ui->bp_ra_pos != canon_ui->bp_ra_pos) &&
          (ui->bp_status != canon_ui->bp_status)) {
	nxt_ui = new_ui(nextInsn(cur_insn),
			canon_ui->ra_status, canon_ui->sp_ra_pos, canon_ui->bp_ra_pos,
			canon_ui->bp_status, canon_ui->sp_bp_pos, canon_ui->bp_bp_pos, ui);
	link_ui(ui, nxt_ui); ui = nxt_ui;
      }
    }
    //--------------------------------------------------
    // allocate/deallocate frame by adjusting stack pointer a variable amount
    //--------------------------------------------------

    // FIXME/alloca: BP-relative
    //   move    s8,sp      [save SP]

    //   dsubu   sp,sp,v0   [alloca space]  --> the alloca flag
    //   sd      sp,24(s8)  [store SP]

    //   move    sp,s8      [dealloca space and restore stack pointer]

    // FIXME/alloca: store/restore the SP; store/retore into BP frame



    // FIXME:
    // void set_ui_canonical(unwind_interval *u, unwind_interval *value);
    // void set_ui_restored_canonical(unwind_interval *u, unwind_interval *value);


    cur_insn++;
  }
#endif

  ui->common.end = INSN(end_insn);

  interval_status stat;

  stat.first_undecoded_ins = NULL;
  stat.errcode = 0;
  stat.first = (splay_interval_t*)beg_ui;


  if (verbose) {
    for (unwind_interval* x = (unwind_interval*)stat.first; 
	 x; x = (unwind_interval*)x->common.next) {
      dump_ui(x, 0);
    }
  }

  return stat;
}

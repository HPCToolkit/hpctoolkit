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

#define INSN(insn) ((char*)(insn))

static inline char*
nextInsn(uint32_t* insn) 
{ return INSN(insn + 1); }


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
  unw_interval_t* beg_ui = new_ui(INSN(beg_insn), FrmTy_SP, FrmFlg_RAReg,
				  0, 0, REG_RA, NULL);
  unw_interval_t* ui = beg_ui;
  unw_interval_t* nxt_ui = NULL;
  unw_interval_t* canon_ui = beg_ui;

  uint32_t* cur_insn = beg_insn;
  while (cur_insn < end_insn) {
    //printf("trying 0x%x [%p,%p\n",*cur_insn, cur_insn, end_insn);

    //--------------------------------------------------
    // allocate/deallocate frame by adjusting stack pointer a fixed amount
    //--------------------------------------------------
    if (isAdjustSPByConst(*cur_insn)) {
      int amnt = getAdjustSPByConstAmnt(*cur_insn);
      
      int sp_pos = ui->sp_pos + amnt;
      
      int ra_arg = ui->ra_arg;
      if (!frameflg_isset(ui->flgs, FrmFlg_RAReg)) {
	ra_arg += amnt;
      }

      nxt_ui = new_ui(nextInsn(cur_insn), FrmTy_SP, ui->flgs, 
		      sp_pos, ui->bp_pos, ra_arg, ui);
      link_ui(ui, nxt_ui); ui = nxt_ui;
    }
    //--------------------------------------------------
    // store return address into SP frame
    //--------------------------------------------------
    else if (isStoreRegInFrame(*cur_insn, REG_SP, REG_RA)) {
      int ra_pos = getStoreRegInFrameOffset(*cur_insn);
      nxt_ui = new_ui(nextInsn(cur_insn), FrmTy_SP, FrmFlg_NULL,
		      ui->sp_pos, ui->bp_pos, ra_pos, ui);
      link_ui(ui, nxt_ui); ui = nxt_ui;

      if (canon_ui == beg_ui) {
	canon_ui = nxt_ui;
      }
    }
    //--------------------------------------------------
    // load return address from SP frame
    //--------------------------------------------------
    else if (isLoadRegFromFrame(*cur_insn, REG_SP, REG_RA)) {
      nxt_ui = new_ui(nextInsn(cur_insn), FrmTy_SP, FrmFlg_RAReg,
		      ui->sp_pos, ui->bp_pos, REG_RA, ui);
      link_ui(ui, nxt_ui); ui = nxt_ui;
    }
    //--------------------------------------------------
    // interior epilogues/returns
    //--------------------------------------------------
    else if (isJumpToReg(*cur_insn, REG_RA)
	&& ((cur_insn + 1/*delay slot*/ + 1) < end_insn)) {
      // An interior return.  Restore the canonical interval if necessary.
      if (!ui_cmp(ui, canon_ui)) {
	nxt_ui = new_ui(nextInsn(cur_insn + 1/*delay slot*/), 
			canon_ui->ty, canon_ui->flgs, canon_ui->sp_pos, 
			canon_ui->bp_pos, canon_ui->ra_arg, ui);
	link_ui(ui, nxt_ui); ui = nxt_ui;
      }
    }
    //--------------------------------------------------
    // allocate/deallocate frame by adjusting stack pointer a variable amount
    //--------------------------------------------------

    // FIXME/alloca: BP-relative
    //   sd      s8=fp,56(sp)
    //   move    s8=fp,sp   [save SP --> BP based]

    //   dsubu   sp,sp,v0   [alloca space]  --> the alloca flag
    //   sd      sp,24(s8)  [store SP]

    //   move    sp,s8=fp   [dealloca space and restore stack pointer]

    // FIXME/alloca: store/restore the SP; store/retore into BP frame



    // FIXME:
    // void set_ui_canonical(unw_interval_t *u, unw_interval_t *value);
    // void set_ui_restored_canonical(unw_interval_t *u, unw_interval_t *value);


    cur_insn++;
  }

  ui->common.end = INSN(end_insn);

  interval_status stat;

  stat.first_undecoded_ins = NULL;
  stat.errcode = 0;
  stat.first = (splay_interval_t*)beg_ui;


  if (verbose) {
    for (unw_interval_t* x = (unw_interval_t*)stat.first; 
	 x; x = (unw_interval_t*)x->common.next) {
      dump_ui(x, 0);
    }
  }

  return stat;
}

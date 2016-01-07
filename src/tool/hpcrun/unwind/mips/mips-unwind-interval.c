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

//***************************************************************************
//
// HPCToolkit's MIPS Unwinder
// 
// Nathan Tallent
// Rice University
//
// When part of HPCToolkit, this code assumes HPCToolkit's license;
// see www.hpctoolkit.org.
//
//***************************************************************************

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>


//*************************** User Include Files ****************************

#include <include/uint.h>
#include <include/gcc-attr.h>

#include "hpcrun_stats.h"
#include "mips-unwind-cfg.h"
#include "mips-unwind-interval.h"

#include <lib/isa-lean/mips/instruction-set.h>


//*************************** Forward Declarations **************************

static UNW_INTERVAL_t
mips_build_intervals(uint32_t* beg_insn, uint32_t* end_insn, 
		     bool retFirst, int verbose);

static void*
mips_find_proc(void* pc, void* module_beg);



//***************************************************************************
// interface operations
//***************************************************************************

// demand_interval: Given a PC 'pc', return an interval.  Note that
// 'pc' may be bogus (i.e, not even point to a text segment) and in this
// case UNW_INTERVAL_NULL should be returned.
UNW_INTERVAL_t
demand_interval(void* pc, bool isTopFrame)
{
#if (HPC_UNW_LITE)
  void* proc_beg = NULL, *mod_beg = NULL;
  dylib_find_proc(pc, &proc_beg, &mod_beg);

  if (!proc_beg && mod_beg && !isTopFrame) {
    // The procedure begin could not be found but pc is valid (since
    // the module begin was found).
    proc_beg = mips_find_proc(pc, mod_beg);
  }
  
  if (proc_beg) {
    uint32_t* beg = (uint32_t*)proc_beg; // [insn_beg, insn_end)
    uint32_t* end = (uint32_t*)pc; 
    return mips_build_intervals(beg, end, false/*retFirst*/, 0);
  }
  else {
    return UNW_INTERVAL_NULL;
  }
#else
  // N.B.: calls build_intervals() if necessary
  return (UNW_INTERVAL_t)hpcrun_addr_to_interval(pc, NULL, NULL);
#endif
}


#if (!HPC_UNW_LITE)
interval_status 
build_intervals(char* insn_beg, unsigned int len)
{
  // [my_insn_beg, my_insn_end)
  uint32_t* beg = (uint32_t*)(insn_beg);
  uint32_t* end = (uint32_t*)(insn_beg + len);

  UNW_INTERVAL_t beg_ui = mips_build_intervals(beg, end, true/*retFirst*/, 0);

  interval_status x = (interval_status){.first_undecoded_ins = NULL,
					.errcode = 0,
					.first = (splay_interval_t*)beg_ui};
  return x;
}
#endif

//***************************************************************************
// unw_interval_t interface
//***************************************************************************

#if (!HPC_UNW_LITE)
unw_interval_t* 
new_ui(char* start_addr, framety_t ty, int flgs,
       int sp_arg, int fp_arg, int ra_arg, unw_interval_t* prev)
{
  unw_interval_t* u = (unw_interval_t*)hpcrun_ui_malloc(sizeof(unw_interval_t));

  u->common.start = start_addr;
  u->common.end = 0;
  u->common.prev = (splay_interval_t*)prev;
  u->common.next = NULL;

  if (prev) {
    ui_link(prev, u);
  }

  u->ty   = ty;
  u->flgs = (int16_t)flgs;

  u->sp_arg = sp_arg;
  u->fp_arg = fp_arg;
  u->ra_arg = ra_arg;

  hpcrun_stats_num_unwind_intervals_total_inc();

  return u;
}
#endif


void 
ui_dump(unw_interval_t* u)
{
  if (!u) {
    return;
  }

  TMSG(INTV, "  [%p, %p) ty=%s flgs=%d sp_arg=%d fp_arg=%d ra_arg=%d",
       (void*)u->common.start, (void*)u->common.end,
       framety_string(u->ty), u->flgs, u->sp_arg, u->fp_arg, u->ra_arg);
  //TMSG(INTV, "  next=%p prev=%p", u->common.next, u->common.prev);
}


//***************************************************************************



void
suspicious_interval(void *pc) 
{
  EMSG("suspicous interval for pc = %p", pc);
  hpcrun_stats_num_unwind_intervals_suspicious_inc();
}


void 
ui_link(unw_interval_t* current, unw_interval_t* next)
{
  HPC_IFNO_UNW_LITE(current->common.end  = next->common.start;)
  HPC_IFNO_UNW_LITE(current->common.next = (splay_interval_t*)next;)
}


//***************************************************************************
// framety_t interface
//***************************************************************************

const char*
framety_string(framety_t ty)
{
#define framety_string_STR(s) case s: return #s

  switch (ty) {
    framety_string_STR(FrmTy_NULL);
    framety_string_STR(FrmTy_SP);
    framety_string_STR(FrmTy_FP);
    default:
      assert(0);
  }
  return NULL;

#undef framety_string_STR
}


//***************************************************************************
// build_intervals: inspect MIPS instructions
//***************************************************************************

#define INSN(insn) ((char*)(insn))

static inline char*
nextInsn(uint32_t* insn) 
{ return INSN(insn + 1); }


static inline bool 
isAdjustSPByConst(uint32_t insn)
{
  // example: daddiu sp,sp,-96
  uint32_t op = (insn & MIPS_OPCODE_MASK);
  if (op == MIPS_OP_DADDIU || op == MIPS_OP_ADDIU || 
      op == MIPS_OP_DADDI || op == MIPS_OP_ADDI) {
    // The non-'U' probably never occur since they trap on overflow.
    return ((MIPS_OPND_REG_S(insn) == MIPS_REG_SP) && 
	    (MIPS_OPND_REG_T(insn) == MIPS_REG_SP));
  }
  return false;
}


// positive value means frame allocation
static inline int
getAdjustSPByConstAmnt(uint32_t insn)
{ return - (int16_t)MIPS_OPND_IMM(insn); }


static inline bool 
isAdjustSPByVar(uint32_t insn)
{
  // example: dsubu sp,sp,v0
  // example: daddu sp,sp,v0 (where v0 is negative)
  uint32_t op = (insn & MIPS_OPCODE_MASK);
  uint32_t op_spc = (insn & MIPS_OPClass_Special_MASK);
  if (op == MIPS_OPClass_Special &&
      ( op_spc == MIPS_OP_DSUBU || op_spc == MIPS_OP_SUBU || 
	op_spc == MIPS_OP_DSUB  || op_spc == MIPS_OP_SUB ||
	op_spc == MIPS_OP_DADDU || op_spc == MIPS_OP_ADDU || 
	op_spc == MIPS_OP_DADD  || op_spc == MIPS_OP_ADD ) ) {
    // The non-'U' probably never occur since they trap on overflow.
    return ((MIPS_OPND_REG_D(insn) == MIPS_REG_SP) && 
	    (MIPS_OPND_REG_S(insn) == MIPS_REG_SP));
  }
  return false;
}


static inline bool 
isStoreRegInFrame(uint32_t insn, int reg_base, int reg_src)
{
  // example: sd ra, 64(sp)
  // to be frame-relevant, reg_base should be MIPS_REG_SP or MIPS_REG_FP
  uint32_t op = (insn & MIPS_OPCODE_MASK);
  if (op == MIPS_OP_SD || op == MIPS_OP_SW) {
    return ((MIPS_OPND_REG_S(insn) == reg_base) &&
	    (MIPS_OPND_REG_T(insn) == reg_src));
  }
  return false;
}


static inline int
getStoreRegInFrameOffset(uint32_t insn)
{ return (int16_t)MIPS_OPND_IMM(insn); }


static inline bool 
isLoadRegFromFrame(uint32_t insn, int reg_base, int reg_dest)
{
  // example: ld ra, 64(sp)
  // to be frame-relevant, reg_base should be MIPS_REG_SP or MIPS_REG_FP
  uint32_t op = (insn & MIPS_OPCODE_MASK);
  if (op == MIPS_OP_LD || op == MIPS_OP_LW) {
    return ((MIPS_OPND_REG_S(insn) == reg_base) &&
	    (MIPS_OPND_REG_T(insn) == reg_dest));
  }
  return false;
}


static inline bool 
isJumpToReg(uint32_t insn, int reg_to)
{
  if ( (insn & MIPS_OPCODE_MASK) == MIPS_OPClass_Special &&
       (insn & MIPS_OPClass_Special_MASK) == MIPS_OP_JR ) {
    return (MIPS_OPND_REG_S(insn) == reg_to);
  }
  return false;
}


static inline bool 
isCall(uint32_t insn)
{
  return ( (insn & MIPS_OPCODE_MASK) == MIPS_OPClass_Special &&
	   (insn & MIPS_OPClass_Special_MASK) == MIPS_OP_JALR &&
	   MIPS_OPND_REG_D(insn) == MIPS_REG_RA );
}


static inline bool 
isMoveReg(uint32_t insn, int reg_dst, int reg_src)
{
  // move rd, rs ==> addu rd, rs, rt=R0
  uint32_t op = (insn & MIPS_OPCODE_MASK);
  uint32_t op_spc = (insn & MIPS_OPClass_Special_MASK);
  if (op == MIPS_OPClass_Special && 
      (op_spc == MIPS_OP_DADDU || op_spc == MIPS_OP_ADDU || 
       op_spc == MIPS_OP_OR ||
       op_spc == MIPS_OP_DADD || op_spc == MIPS_OP_ADD)) {
    // The non-'U' probably never occur since they trap on overflow.
    return ((MIPS_OPND_REG_D(insn) == reg_dst) &&
	    (MIPS_OPND_REG_S(insn) == reg_src) &&
	    (MIPS_OPND_REG_T(insn) == MIPS_REG_0));
  }
  return false;
}


//***************************************************************************
// build_intervals: helpers
//***************************************************************************

static inline void 
checkSPOfst(int* ofst, int alt_ofst)
{
  // SP-relative offset: must be positive
  if (*ofst < 0) {
    EMSG("build_intervals: negative SP-relative offset (%d -> %d)", 
	 *ofst, alt_ofst);
    *ofst = alt_ofst;
  }
}


static inline void
checkUI(unw_interval_t* ui, framety_t ty, uint32_t* insn)
{
  if (ui->ty != ty) {
    EMSG("build_intervals: At pc=%p [0x%x], unexpected interval type %s", 
	 insn, *insn, framety_string(ty));
    ui_dump(ui);
  }
}


static inline int
convertSPToFPOfst(int frame_sz, int sp_rel)
{
  int fp_rel = (sp_rel == unwarg_NULL) ? unwarg_NULL : -(frame_sz - sp_rel);
  return fp_rel;
}


//***************************************************************************
// build_intervals:
//***************************************************************************

// MIPS is very regular. 
// - Calls (JAL/JALR/B*AL) store the return address in register RA.  (JALR
//   may be used to store the return address in a different register.)
// - Thus, on procedure entry, the return address is usually in register RA.
// - Frames are typically SP-relative (except for alloca's)
// - If a call is made, register RA is stored in the frame; after the call
//   and before a return, it is reloaded.

// Typical SP frame: GCC and Pathscale
//   daddiu  sp,sp,-96   ! allocate frame
//                       ! possibly allocate add'tl space
//   sd      ra,40(sp)   ! store RA before a call
//   jalr    t9          ! call
//   ld      ra,40(sp)   ! restore RA
//   jr      ra          ! return
//   daddiu  sp,sp,96    ! deallocate frame (in delay slot)

// Typical FP frame: GCC [note fp = s8]
//   daddiu  sp,sp,-96   ! allocate basic frame
//   sd      fp,56(sp)   ! preserve parent's FP
//   move    fp,sp       ! setup (BOGUS) FP
//   sd      ra,64(sp)   ! store RA
//   dsubu   sp,sp,v0    ! allocate more frame             [COMMON]
//                       ! store SP?
//   move    sp,fp       ! restore SP and deallocate frame [COMMON]
//   ld      ra,64(sp)   ! restore RA
//   ld      fp,56(sp)   ! restore FP
// 
// Typical FP frame: Pathscale [note fp = s8]              
//   move    v0,fp       ! [a] store parent's FP           [???]
//   daddiu  sp,sp,-96   ! allocate SP-frame               [COMMON]
//   daddiu  fp,sp,96    ! setup FP
//   sd      v0,-32(fp)  ! [a] store parent's FP
//   sd      ra,-64(fp)  ! store RA
//   sd      sp,-48(fp)  ! store SP
//   dsubu   sp,sp,v0    ! allocate more frame             [COMMON]
//   ld      ra,-64(fp)  ! restore RA
//   ld      at,-32(fp)  | [b] restore parent's FP         [???]
//   move    sp,fp       ! restore SP and deallocate frame [COMMON]
//   jr      ra
//   move    fp,at       ! [b] restore parent's FP


// mips_build_intervals: build intervals for the range [beg_insn,
// end_insn).  Returns the first interval if retFirst is true,
// otherwise returns the last.
static UNW_INTERVAL_t
mips_build_intervals(uint32_t* beg_insn, uint32_t* end_insn, 
		     bool retFirst, int verbose)
{
  UNW_INTERVAL_t beg_ui = NEW_UI(INSN(beg_insn), FrmTy_SP, FrmFlg_RAReg,
				 0, unwarg_NULL, MIPS_REG_RA, NULL);
  UNW_INTERVAL_t ui = beg_ui;
  UNW_INTERVAL_t nxt_ui = UNW_INTERVAL_NULL;

  // canonical intervals
  UNW_INTERVAL_t canon_ui   = beg_ui;
  UNW_INTERVAL_t canonSP_ui = beg_ui;
  UNW_INTERVAL_t canonFP_ui = UNW_INTERVAL_NULL; // currently not needed

  int fp_saved_reg = 0;

  uint32_t* cur_insn = beg_insn;
  while (cur_insn < end_insn) {
    //TMSG(INTV, "insn: 0x%x [%p,%p)", *cur_insn, cur_insn, end_insn);

    //--------------------------------------------------
    // 1. SP-frames
    // 
    // SP-frame --> SP-frame: alloc/dealloc constant-sized frame
    // FP-frame --> FP-frame: additional storage [UNCOMMON]
    //--------------------------------------------------
    if (isAdjustSPByConst(*cur_insn)) {
      int sp_arg, ra_arg;
      
      if (UI_FLD(ui,ty) == FrmTy_SP) {
	//checkUI(UI_ARG(ui), FrmTy_SP, cur_insn);
	int amnt = getAdjustSPByConstAmnt(*cur_insn);
	
	sp_arg = UI_FLD(ui,sp_arg) + amnt;
	checkSPOfst(&sp_arg, UI_FLD(ui,sp_arg));
	
	ra_arg = UI_FLD(ui,ra_arg);
	if (!frameflg_isset(UI_FLD(ui,flgs), FrmFlg_RAReg)) {
	  ra_arg += amnt;
	  checkSPOfst(&ra_arg, UI_FLD(ui,ra_arg));
	}
      }
      else {
	sp_arg = UI_FLD(ui,sp_arg);
	ra_arg = UI_FLD(ui,ra_arg);
      }

      nxt_ui = NEW_UI(nextInsn(cur_insn), UI_FLD(ui,ty), UI_FLD(ui,flgs), 
		      sp_arg, UI_FLD(ui,fp_arg), ra_arg, ui);
      ui = nxt_ui;
    }

    //--------------------------------------------------
    // SP-frame --> SP-frame: store RA/FP
    // *** canonical frame ***
    //--------------------------------------------------
    else if (isStoreRegInFrame(*cur_insn, MIPS_REG_SP, MIPS_REG_RA)) {
      checkUI(UI_ARG(ui), FrmTy_SP, cur_insn);

      // store return address
      int ra_arg = getStoreRegInFrameOffset(*cur_insn);
      int16_t flgs = UI_FLD(ui,flgs);
      frameflg_unset(&flgs, FrmFlg_RAReg);

      nxt_ui = NEW_UI(nextInsn(cur_insn), FrmTy_SP, flgs,
		      UI_FLD(ui,sp_arg), UI_FLD(ui,fp_arg), ra_arg, ui);
      ui = nxt_ui;

      canon_ui = canonSP_ui = nxt_ui;
    }
    else if (isStoreRegInFrame(*cur_insn, MIPS_REG_SP, MIPS_REG_FP)) {
      checkUI(UI_ARG(ui), FrmTy_SP, cur_insn);

      // store frame pointer (N.B. fp may be used as saved reg s8)
      int fp_arg = getStoreRegInFrameOffset(*cur_insn);
      nxt_ui = NEW_UI(nextInsn(cur_insn), FrmTy_SP, UI_FLD(ui,flgs),
		      UI_FLD(ui,sp_arg), fp_arg, UI_FLD(ui,ra_arg), ui);
      ui = nxt_ui;

      canon_ui = canonSP_ui = nxt_ui;
    }

    //--------------------------------------------------
    // SP-frame --> SP-frame: load RA by SP
    //--------------------------------------------------
    else if (isLoadRegFromFrame(*cur_insn, MIPS_REG_SP, MIPS_REG_RA)) {
      checkUI(UI_ARG(ui), FrmTy_SP, cur_insn);

      // sanity check: sp_arg must be positive!
      // TODO: apply this check to other SP-relative ops.  Fix prior intervals
      int sp_arg = UI_FLD(ui,sp_arg);
      if ( !(sp_arg > 0) ) {
	sp_arg = UI_FLD(canonSP_ui,sp_arg);
      }

      nxt_ui = NEW_UI(nextInsn(cur_insn), FrmTy_SP, FrmFlg_RAReg,
		      sp_arg, UI_FLD(ui,fp_arg), MIPS_REG_RA, ui);
      ui = nxt_ui;
    }


    //--------------------------------------------------
    // 2. General: interior epilogues before returns
    //--------------------------------------------------
    else if (isJumpToReg(*cur_insn, MIPS_REG_RA)
	     && ((cur_insn + 1/*delay slot*/ + 1) < end_insn)) {
      // An interior return.  Restore the canonical interval if necessary.
      // 
      // N.B.: Although the delay slot instruction may affect the
      // frame, because of the return, we will never see its effect
      // within this procedure.  Therefore, it is harmless to skip
      // processing of this delay slot.
      if (!ui_cmp(UI_ARG(ui), UI_ARG(canon_ui))) {
	nxt_ui = NEW_UI(nextInsn(cur_insn + 1/*delay slot*/),
			UI_FLD(canon_ui,ty), UI_FLD(canon_ui,flgs), 
			UI_FLD(canon_ui,sp_arg), UI_FLD(canon_ui,fp_arg),
			UI_FLD(canon_ui,ra_arg), ui);
	ui = nxt_ui;
	cur_insn++; // skip delay slot and align new interval
      }
    }

    //--------------------------------------------------
    // General: interior epilogues before callsites
    //--------------------------------------------------
    else if (isCall(*cur_insn)
	     && frameflg_isset(UI_FLD(ui,flgs), FrmFlg_RAReg)) {
      // The callsite (jalr) is clobbering r31, but RA is in r31.  We
      // assume this is an inconsistency arising from control flow
      // jumping around an interior epilogue before the callsite.
      // Restore the canonical interval.
      if (!ui_cmp(UI_ARG(ui), UI_ARG(canon_ui))) {
	nxt_ui = NEW_UI(nextInsn(cur_insn),
			UI_FLD(canon_ui,ty), UI_FLD(canon_ui,flgs), 
			UI_FLD(canon_ui,sp_arg), UI_FLD(canon_ui,fp_arg),
			UI_FLD(canon_ui,ra_arg), ui);
	ui = nxt_ui;
      }
    }


    //--------------------------------------------------
    // 3. Basic support for FP frames.
    // 
    // *-frame --> FP-frame: store RA by FP
    // *** canonical frame ***
    //--------------------------------------------------
    else if (isStoreRegInFrame(*cur_insn, MIPS_REG_FP, MIPS_REG_RA)) {
      int sp_arg, fp_arg;
      if (UI_FLD(ui,ty) == FrmTy_SP) {
	sp_arg = unwarg_NULL;
	fp_arg = convertSPToFPOfst(UI_FLD(ui,sp_arg), UI_FLD(ui,fp_arg));
      }
      else {
	sp_arg = UI_FLD(ui,sp_arg);
	fp_arg = UI_FLD(ui,fp_arg);
      }
      int ra_arg = getStoreRegInFrameOffset(*cur_insn);

      int16_t flgs = UI_FLD(ui,flgs);
      frameflg_unset(&flgs, FrmFlg_RAReg);

      nxt_ui = NEW_UI(nextInsn(cur_insn), FrmTy_FP, flgs,
		      sp_arg, fp_arg, ra_arg, ui);
      ui = nxt_ui;
      
      canon_ui = canonFP_ui = nxt_ui;
    }

    //--------------------------------------------------
    // *-frame --> FP-frame: store parent FP (saved in reg) by FP
    // *** canonical frame ***
    //--------------------------------------------------
    else if (isMoveReg(*cur_insn, MIPS_REG_V0, MIPS_REG_FP)) {
      frameflg_set(&UI_FLD(ui,flgs), FrmFlg_FPInV);
      fp_saved_reg = MIPS_REG_V0;
    }
    else if (isMoveReg(*cur_insn, MIPS_REG_V1, MIPS_REG_FP)) {
      frameflg_set(&UI_FLD(ui,flgs), FrmFlg_FPInV);
      fp_saved_reg = MIPS_REG_V1;
    }
    else if (frameflg_isset(UI_FLD(ui,flgs), FrmFlg_FPInV) &&
	     isStoreRegInFrame(*cur_insn, MIPS_REG_FP, fp_saved_reg)) {
      int sp_arg, ra_arg;
      if (UI_FLD(ui,ty) == FrmTy_SP) {
	sp_arg = unwarg_NULL;
	ra_arg = convertSPToFPOfst(UI_FLD(ui,sp_arg), UI_FLD(ui,ra_arg));
      }
      else {
	sp_arg = UI_FLD(ui,sp_arg);
	ra_arg = UI_FLD(ui,ra_arg);
      }
      int fp_arg = getStoreRegInFrameOffset(*cur_insn);

      nxt_ui = NEW_UI(nextInsn(cur_insn), FrmTy_FP, UI_FLD(ui,flgs),
		      sp_arg, fp_arg, ra_arg, ui);
      ui = nxt_ui;
      
      canon_ui = canonFP_ui = nxt_ui;
    }
    /* else if (store-parent's-SP): useless */

    //--------------------------------------------------
    // *-frame --> FP-frame: allocate variable-sized frame
    // *** canonical frame ***
    //--------------------------------------------------
    else if (isMoveReg(*cur_insn, MIPS_REG_FP, MIPS_REG_SP)) {
      // FP is set to SP, likely meaning it will point to the middle
      // of the frame (pos. offsets) rather than the top (neg. offsets)
      // ??? test that the canonical FP frame has not been set ???
      frameflg_set(&UI_FLD(ui,flgs), FrmFlg_FPOfstPos);
    }
    else if (isAdjustSPByVar(*cur_insn)) {
      // TODO: ensure "daddu sp,sp,v0" is allocation rather than deallocation

      // if canonical interval has been set and it is an SP-frame...
      if (!UI_CMP_OPT(canon_ui, beg_ui) && UI_FLD(canon_ui,ty) == FrmTy_SP) {
	int16_t flgs = UI_FLD(ui,flgs);
	int sp_arg, fp_arg, ra_arg;

	if (frameflg_isset(flgs, FrmFlg_FPOfstPos)) {
	  sp_arg = UI_FLD(canon_ui,sp_arg);
	  fp_arg = UI_FLD(canon_ui,fp_arg);
	  ra_arg = UI_FLD(canon_ui,ra_arg);
	}
	else {
	  sp_arg = unwarg_NULL;
	  fp_arg = convertSPToFPOfst(UI_FLD(canon_ui,sp_arg), 
				     UI_FLD(canon_ui,fp_arg));
	  ra_arg = convertSPToFPOfst(UI_FLD(canon_ui,sp_arg), 
				     UI_FLD(canon_ui,ra_arg));
	}
	  
	frameflg_set(&flgs, FrmFlg_FrmSzUnk);
	
	nxt_ui = NEW_UI(nextInsn(cur_insn), FrmTy_FP, flgs,
			sp_arg, fp_arg, ra_arg, ui);
	ui = nxt_ui;

	canon_ui = canonFP_ui = nxt_ui;
      }
    }

    //--------------------------------------------------
    // FP-frame --> FP-frame: load RA by FP
    //--------------------------------------------------
    else if (isLoadRegFromFrame(*cur_insn, MIPS_REG_FP, MIPS_REG_RA)) {
      checkUI(UI_ARG(ui), FrmTy_FP, cur_insn);
      nxt_ui = NEW_UI(nextInsn(cur_insn), FrmTy_FP, FrmFlg_RAReg,
		      UI_FLD(ui,sp_arg), UI_FLD(ui,fp_arg), MIPS_REG_RA, ui);
      ui = nxt_ui;
    }
    /* else if (load-parent's-FP): not needed */
    /* else if (load-parent's-SP): useless */

    //--------------------------------------------------
    // FP-frame --> SP-frame: deallocate (all/part of) frame by restoring SP
    //--------------------------------------------------
    else if (isMoveReg(*cur_insn, MIPS_REG_SP, MIPS_REG_FP)) {
      if (UI_FLD(ui,ty) == FrmTy_FP) {
	bool isFullDealloc = 
	  (!frameflg_isset(UI_FLD(canon_ui,flgs), FrmFlg_RAReg)
	   && frameflg_isset(UI_FLD(ui,flgs), FrmFlg_RAReg));
	
	int flgs = (isFullDealloc) ? FrmFlg_RAReg : UI_FLD(canonSP_ui,flgs);
	int sp_arg, fp_arg, ra_arg;
	sp_arg = (isFullDealloc) ? 0           : UI_FLD(canonSP_ui,sp_arg);
	fp_arg = (isFullDealloc) ? unwarg_NULL : UI_FLD(canonSP_ui,fp_arg);
	ra_arg = (isFullDealloc) ? MIPS_REG_RA : UI_FLD(canonSP_ui,ra_arg);
	nxt_ui = NEW_UI(nextInsn(cur_insn), FrmTy_SP, flgs,
			sp_arg, fp_arg, ra_arg, ui);
	ui = nxt_ui;
      }
      else {
	// wierd code, e.g., monitor_main! Assume rest of frame will
	// be deallocated normally.  Could restore a canonical frame
	// (FIXME).
      }
    }
    
    cur_insn++;
  }

  HPC_IFNO_UNW_LITE( UI_FLD(ui,common).end = INSN(end_insn); )

#if (!HPC_UNW_LITE)
  if (verbose) {
    for (UNW_INTERVAL_t x = beg_ui; x; x = (UNW_INTERVAL_t)x->common.next) {
      ui_dump(x);
    }
  }
#endif

  return (retFirst) ? beg_ui : ui;
}


static void* GCC_ATTR_UNUSED
mips_find_proc(void* pc, void* module_beg)
{
  // If we are within a valid text segment and if this is not the
  // first stack frame, we assume the following about the procedure
  // that pc is contained within:
  // - it is extremely likely to have a frame (i.e., not frameless)
  // - all prologue code has been executed (since pc should be a
  //   return address)
  // 
  // Therefore we perform a reverse search for the following pattern:
  //   daddiu sp, sp, -xxx   <- functional begin of routine
  //   sd     ra, xx(sp/fp)

  bool fnd_ra = false, fnd_alloc = false;
  
  uint32_t* cur_insn = (uint32_t*)pc;

  // 1. find the store of the return address
  while (cur_insn > (uint32_t*)module_beg && !fnd_ra) {
    if (isStoreRegInFrame(*cur_insn, MIPS_REG_SP, MIPS_REG_RA) ||
	isStoreRegInFrame(*cur_insn, MIPS_REG_FP, MIPS_REG_RA)) {
      fnd_ra = true; // advance to next insn
    }
    cur_insn--;
  }
  
  if (!fnd_ra) {
    return NULL;
  }
  
  // 2. find the stack allocation -- we hope there is only one!
  while (cur_insn > (uint32_t*)module_beg) {
    if (isAdjustSPByConst(*cur_insn) 
	&& getAdjustSPByConstAmnt(*cur_insn) > 0) {
      fnd_alloc = true;
      break;
    }
    cur_insn--;
  }

  return (fnd_alloc) ? cur_insn : NULL;
}

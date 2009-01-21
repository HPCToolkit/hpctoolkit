// -*-Mode: C++;-*- // technically C99
// $Id$

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>


//*************************** User Include Files ****************************

#include <include/general.h>

#include "mips-unwind-cfg.h"
#include "mips-unwind-interval.h"

// FIXME: Abuse the isa library by cherry-picking this special header.
// One possibility is to simply use the ISA lib -- doesn't xed
// internally use C++ stuff?
#include <lib/isa/instructionSets/mips.h>


//*************************** Forward Declarations **************************

static UNW_INTERVAL_t
mips_build_intervals(uint32_t* beg_insn, uint32_t* end_insn, 
		     bool retFirst, int verbose);


//***************************************************************************
// global variables
//***************************************************************************

long ui_cnt = 0;
long suspicious_cnt = 0;


//***************************************************************************
// interface operations
//***************************************************************************

UNW_INTERVAL_t
demand_interval(void* pc)
{
#if (HPC_UNW_LITE)
  uint32_t* insn_beg = (uint32_t*)dylib_find_lower_bound(pc);
  if (insn_beg) {
    uint32_t* insn_end = (uint32_t*)pc; // [insn_beg, insn_end)
    return mips_build_intervals(insn_beg, insn_end, false/*retFirst*/, 0);
  }
  else {
    return UNW_INTERVAL_NULL;
  }
#else
  // N.B.: calls build_intervals() if necessary
  return (UNW_INTERVAL_t)csprof_addr_to_interval(pc);
#endif
}


#if (!HPC_UNW_LITE)
interval_status 
build_intervals(char* insn_beg, unsigned int len)
{
  // [my_insn_beg, my_insn_end)
  uint32_t* my_insn_beg = (uint32_t*)(insn_beg);
  uint32_t* my_insn_end = (uint32_t*)(insn_beg + len);

  UNW_INTERVAL_t beg_ui = 
    mips_build_intervals(my_insn_beg, my_insn_end, true/*retFirst*/, 0);

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
  unw_interval_t* u = (unw_interval_t*)csprof_malloc(sizeof(unw_interval_t));

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

  fetch_and_add(&ui_cnt, 1);

  return u;
}
#endif


void 
ui_dump(unw_interval_t* u, int dump_to_stdout)
{
#if (!HPC_UNW_LITE)
  if (!u) {
    return;
  }

  char buf[256];
  
  sprintf(buf, "start=%p end=%p ty=%s flgs=%d sp_arg=%d fp_arg=%d ra_arg=%d next=%p prev=%p\n",
	  (void*)u->common.start, (void*)u->common.end,
	  framety_string(u->ty), u->flgs, u->sp_arg, u->fp_arg, u->ra_arg,
	  u->common.next, u->common.prev);

  EMSG(buf);
  if (dump_to_stdout) { 
    fprintf(stderr, "%s", buf);
    fflush(stderr);
  }
#endif
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
  HPC_IFNO_UNW_LITE(fetch_and_add(&suspicious_cnt,1);)
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
  uint32_t op = (insn & OP_MASK);
  if (op == DADDIU || op == ADDIU || op == DADDI || op == ADDI) {
    // The non-'U' probably never occur since they trap on overflow.
    return (REG_S(insn) == REG_SP) && (REG_T(insn) == REG_SP);
  }
  return false;
}


// positive value means frame allocation
static inline int
getAdjustSPByConstAmnt(uint32_t insn)
{ return - (int16_t)IMM(insn); }


static inline bool 
isAdjustSPByVar(uint32_t insn)
{
  // example: dsubu sp,sp,v0
  uint32_t op = (insn & OP_MASK);
  uint32_t op_spc = (insn & OPSpecial_MASK);
  if (op == OPSpecial &&
      (op_spc == DSUBU || op_spc == SUBU || op_spc == DSUB || op_spc == SUB)) {
    // The non-'U' probably never occur since they trap on overflow.
    return (REG_D(insn) == REG_SP) && (REG_S(insn) == REG_SP);
  }
  return false;
}


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
getStoreRegInFrameOffset(uint32_t insn)
{ return (int16_t)IMM(insn); }


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


static inline bool 
isMoveReg(uint32_t insn, int reg_dst, int reg_src)
{
  // move rd, rs ==> addu rd, rs, rt=R0
  uint32_t op = (insn & OP_MASK);
  uint32_t op_spc = (insn & OPSpecial_MASK);
  if (op == OPSpecial && 
      (op_spc == DADDU || op_spc == ADDU || 
       op_spc == OR ||
       op_spc == DADD || op_spc == ADD)) {
    // The non-'U' probably never occur since they trap on overflow.
    return ((REG_D(insn) == reg_dst) && (REG_S(insn) == reg_src) &&
	    (REG_T(insn) == REG_0));
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
    ui_dump(ui, 0);
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
// - Calls (JAL/JALR/B*AL) store the return address in REG_RA.  (JALR
//   may be used to store the return address in a different register.)
// - Thus, on procedure entry, the return address is usually in REG_RA.
// - Frames are typically SP-relative (except for alloca's)
// - If a call is made, REG_RA is stored in the frame; after the call
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
//   move    fp,sp       ! setup FP
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
				 0, unwarg_NULL, REG_RA, NULL);
  UNW_INTERVAL_t ui = beg_ui;
  UNW_INTERVAL_t nxt_ui = UNW_INTERVAL_NULL;

  // canonical intervals
  UNW_INTERVAL_t canon_ui   = beg_ui;
  UNW_INTERVAL_t canonSP_ui = beg_ui;
  UNW_INTERVAL_t canonFP_ui = UNW_INTERVAL_NULL; // currently not needed


  uint32_t* cur_insn = beg_insn;
  while (cur_insn < end_insn) {
    //printf("insn: 0x%x [%p,%p)\n", *cur_insn, cur_insn, end_insn);

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
    else if (isStoreRegInFrame(*cur_insn, REG_SP, REG_RA)) {
      checkUI(UI_ARG(ui), FrmTy_SP, cur_insn);

      // store return address
      int ra_arg = getStoreRegInFrameOffset(*cur_insn);
      nxt_ui = NEW_UI(nextInsn(cur_insn), FrmTy_SP, FrmFlg_NULL,
		      UI_FLD(ui,sp_arg), UI_FLD(ui,fp_arg), ra_arg, ui);
      ui = nxt_ui;

      canon_ui = canonSP_ui = nxt_ui;
    }
    else if (isStoreRegInFrame(*cur_insn, REG_SP, REG_FP)) {
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
    else if (isLoadRegFromFrame(*cur_insn, REG_SP, REG_RA)) {
      checkUI(UI_ARG(ui), FrmTy_SP, cur_insn);

      nxt_ui = NEW_UI(nextInsn(cur_insn), FrmTy_SP, FrmFlg_RAReg,
		      UI_FLD(ui,sp_arg), UI_FLD(ui,fp_arg), REG_RA, ui);
      ui = nxt_ui;
    }


    //--------------------------------------------------
    // 2. General: interior returns/epilogues
    //--------------------------------------------------
    else if (isJumpToReg(*cur_insn, REG_RA)
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
    // 3. Basic support for FP frames.
    // 
    // *-frame --> FP-frame: store RA by FP
    // *** canonical frame ***
    //--------------------------------------------------
    else if (isStoreRegInFrame(*cur_insn, REG_FP, REG_RA)) {
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
    else if (isMoveReg(*cur_insn, REG_V0, REG_FP)) {
      frameflg_set(&UI_FLD(ui,flgs), FrmFlg_FPInV0);
    }
    else if (frameflg_isset(UI_FLD(ui,flgs), FrmFlg_FPInV0) &&
	     isStoreRegInFrame(*cur_insn, REG_FP, REG_V0)) {
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
    else if (isAdjustSPByVar(*cur_insn)) {
      // if canonical interval has been set and is not an SP-frame...
      if (!UI_CMP_OPT(canon_ui, beg_ui) && UI_FLD(canon_ui,ty) == FrmTy_SP) {
	int fp_arg = convertSPToFPOfst(UI_FLD(canon_ui,sp_arg), 
				       UI_FLD(canon_ui,fp_arg));
	int ra_arg = convertSPToFPOfst(UI_FLD(canon_ui,sp_arg), 
				       UI_FLD(canon_ui,ra_arg));

	int16_t flgs = UI_FLD(ui,flgs);
	frameflg_set(&flgs, FrmFlg_FrmSzUnk);
	
	nxt_ui = NEW_UI(nextInsn(cur_insn), FrmTy_FP, flgs,
			unwarg_NULL, fp_arg, ra_arg, ui);
	ui = nxt_ui;

	canon_ui = canonFP_ui = nxt_ui;
      }
    }

    //--------------------------------------------------
    // FP-frame --> FP-frame: load RA by FP
    //--------------------------------------------------
    else if (isLoadRegFromFrame(*cur_insn, REG_FP, REG_RA)) {
      checkUI(UI_ARG(ui), FrmTy_FP, cur_insn);
      nxt_ui = NEW_UI(nextInsn(cur_insn), FrmTy_FP, FrmFlg_RAReg,
		      UI_FLD(ui,sp_arg), UI_FLD(ui,fp_arg), REG_RA, ui);
      ui = nxt_ui;
    }
    /* else if (load-parent's-FP): not needed */
    /* else if (load-parent's-SP): useless */

    //--------------------------------------------------
    // FP-frame --> SP-frame: deallocate (all/part of) frame by restoring SP
    //--------------------------------------------------
    else if (isMoveReg(*cur_insn, REG_SP, REG_FP)) {
      if (UI_FLD(ui,ty) == FrmTy_FP) {
	bool isFullDealloc = 
	  (!frameflg_isset(UI_FLD(canon_ui,flgs), FrmFlg_RAReg)
	   && frameflg_isset(UI_FLD(ui,flgs), FrmFlg_RAReg));
	
	int flgs = (isFullDealloc) ? FrmFlg_RAReg : UI_FLD(canonSP_ui,flgs);
	int sp_arg, fp_arg, ra_arg;
	sp_arg = (isFullDealloc) ? 0           : UI_FLD(canonSP_ui,sp_arg);
	fp_arg = (isFullDealloc) ? unwarg_NULL : UI_FLD(canonSP_ui,fp_arg);
	ra_arg = (isFullDealloc) ? REG_RA      : UI_FLD(canonSP_ui,ra_arg);
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
      ui_dump(x, 0);
    }
  }
#endif

  return (retFirst) ? beg_ui : ui;
}


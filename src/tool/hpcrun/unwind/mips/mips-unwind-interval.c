// -*-Mode: C++;-*- // technically C99
// $Id$

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

//*************************** User Include Files ****************************

#include <include/general.h>

#include "mips-unwind-interval.h"

#include "pmsg.h"
#include "atomic-ops.h"
#include <memory/mem.h>

// FIXME: Abuse the isa library by cherry-picking this special header.
// One possibility is to simply use the ISA lib -- doesn't xed
// internally use C++ stuff?
#include <lib/isa/instructionSets/mips.h>


//*************************** Forward Declarations **************************

static interval_status 
mips_build_intervals(uint32_t* beg_insn, uint32_t* end_insn, int verbose);


//***************************************************************************
// global variables
//***************************************************************************

long ui_cnt = 0;
long suspicious_cnt = 0;


//***************************************************************************
// interface operations
//***************************************************************************

interval_status 
build_intervals(char* insn_beg, unsigned int len)
{
  // [my_insn_beg, my_insn_end)
  uint32_t* my_insn_beg = (uint32_t*)(insn_beg);
  uint32_t* my_insn_end = (uint32_t*)(insn_beg + len);

  interval_status x = mips_build_intervals(my_insn_beg, my_insn_end, 0);
  return x;
}


//***************************************************************************
// unw_interval_t interface
//***************************************************************************

unw_interval_t* 
new_ui(char* start_addr, framety_t ty, frameflg_t flgs,
       int sp_pos, int fp_pos, int ra_arg, unw_interval_t* prev)
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
  u->flgs = flgs;

  u->sp_pos = sp_pos;
  u->fp_pos = fp_pos;
  u->ra_arg = ra_arg;

  fetch_and_add(&ui_cnt, 1);

  return u;
}


void 
ui_dump(unw_interval_t* u, int dump_to_stdout)
{
  char buf[256];
  
  sprintf(buf, "start=%p end=%p ty=%s flgs=%d sp_pos=%d fp_pos=%d ra_arg=%d next=%p prev=%p\n",
	  (void*)u->common.start, (void*)u->common.end,
	  framety_string(u->ty), u->flgs, u->sp_pos, u->fp_pos, u->ra_arg,
	  u->common.next, u->common.prev);

  EMSG(buf);
  if (dump_to_stdout) { 
    fprintf(stderr, "%s", buf);
    fflush(stderr);
  }
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
  current->common.end  = next->common.start;
  current->common.next = (splay_interval_t*)next;
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
      (op_spc == DADDU || op_spc == ADDU || op_spc == DADD || op_spc == ADD)) {
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
checkSPPos(int* pos, int alt_pos)
{
  // SP-relative position: must be positive
  if (*pos < 0) {
    EMSG("build_intervals: SP-relative pos'n not positive (%d)", *pos);
    *pos = alt_pos;
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
convertSPToFPPos(int frame_sz, int sp_rel)
{
  int fp_rel = (sp_rel == unwpos_NULL) ? 0 : -(frame_sz - sp_rel);
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
//   and before a return is reloaded.

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

static interval_status 
mips_build_intervals(uint32_t* beg_insn, uint32_t* end_insn, int verbose)
{
  unw_interval_t* beg_ui = new_ui(INSN(beg_insn), FrmTy_SP, FrmFlg_RAReg,
				  0, unwpos_NULL, REG_RA, NULL);
  unw_interval_t* ui = beg_ui;
  unw_interval_t* nxt_ui = NULL;

  // canonical intervals
  unw_interval_t* canon_ui   = beg_ui;
  unw_interval_t* canonSP_ui = beg_ui;
  unw_interval_t* canonFP_ui = NULL;


  uint32_t* cur_insn = beg_insn;
  while (cur_insn < end_insn) {
    //printf("trying 0x%x [%p,%p)\n",*cur_insn, cur_insn, end_insn);

    //--------------------------------------------------
    // SP-frame --> SP-frame: alloc/dealloc constant-sized frame
    //--------------------------------------------------
    if (isAdjustSPByConst(*cur_insn)) {
      checkUI(ui, FrmTy_SP, cur_insn);

      int amnt = getAdjustSPByConstAmnt(*cur_insn);
      
      int sp_pos = ui->sp_pos + amnt;
      checkSPPos(&sp_pos, ui->sp_pos);

      int ra_arg = ui->ra_arg;
      if (!frameflg_isset(ui->flgs, FrmFlg_RAReg)) {
	ra_arg += amnt;
	checkSPPos(&ra_arg, ui->ra_arg);
      }

      nxt_ui = new_ui(nextInsn(cur_insn), FrmTy_SP, ui->flgs, 
		      sp_pos, ui->fp_pos, ra_arg, ui);
      ui = nxt_ui;
    }
    //--------------------------------------------------
    // SP-frame --> SP-frame: store RA/FP
    // *** canonical frame ***
    //--------------------------------------------------
    else if (isStoreRegInFrame(*cur_insn, REG_SP, REG_RA)) {
      checkUI(ui, FrmTy_SP, cur_insn);

      // store return address
      int ra_pos = getStoreRegInFrameOffset(*cur_insn);
      nxt_ui = new_ui(nextInsn(cur_insn), FrmTy_SP, FrmFlg_NULL,
		      ui->sp_pos, ui->fp_pos, ra_pos, ui);
      ui = nxt_ui;

      canon_ui = canonSP_ui = nxt_ui;
    }
    else if (isStoreRegInFrame(*cur_insn, REG_SP, REG_FP)) {
      checkUI(ui, FrmTy_SP, cur_insn);

      // store frame pointer (N.B. fp may be used as saved reg s8)
      int fp_pos = getStoreRegInFrameOffset(*cur_insn);
      nxt_ui = new_ui(nextInsn(cur_insn), FrmTy_SP, ui->flgs,
		      ui->sp_pos, fp_pos, ui->ra_arg, ui);
      ui = nxt_ui;

      canon_ui = canonSP_ui = nxt_ui;
    }
    //--------------------------------------------------
    // SP-frame --> SP-frame: load RA by SP
    //--------------------------------------------------
    else if (isLoadRegFromFrame(*cur_insn, REG_SP, REG_RA)) {
      checkUI(ui, FrmTy_SP, cur_insn);

      nxt_ui = new_ui(nextInsn(cur_insn), FrmTy_SP, FrmFlg_RAReg,
		      ui->sp_pos, ui->fp_pos, REG_RA, ui);
      ui = nxt_ui;
    }

    //--------------------------------------------------
    // General: interior returns (epilogues)
    //--------------------------------------------------
    else if (isJumpToReg(*cur_insn, REG_RA)
	     && ((cur_insn + 1/*delay slot*/ + 1) < end_insn)) {
      // An interior return.  Restore the canonical interval if necessary.
      if (!ui_cmp(ui, canon_ui)) {
	nxt_ui = new_ui(nextInsn(cur_insn + 1/*delay slot*/), 
			canon_ui->ty, canon_ui->flgs, canon_ui->sp_pos, 
			canon_ui->fp_pos, canon_ui->ra_arg, ui);
	ui = nxt_ui;
	cur_insn++; // skip delay slot to align new interval
      }
    }


    // N.B.: It is difficult to detect beginning of a FP frame.  For
    // now we use the following.

    //--------------------------------------------------
    // SP-frame --> FP-frame: store RA by FP
    // *** canonical frame ***
    //--------------------------------------------------
    else if (isStoreRegInFrame(*cur_insn, REG_FP, REG_RA)) {
      checkUI(ui, FrmTy_SP, cur_insn);

      int fp_pos = convertSPToFPPos(ui->sp_pos, ui->fp_pos);
      int ra_pos = getStoreRegInFrameOffset(*cur_insn);
      nxt_ui = new_ui(nextInsn(cur_insn), FrmTy_FP, FrmFlg_NULL,
		      0, fp_pos, ra_pos, ui);
      ui = nxt_ui;

      canon_ui = canonFP_ui = nxt_ui;
    }
    /* else if (store-parent's-FP): FIXME */

    //--------------------------------------------------
    // *-frame --> FP-frame: allocate variable-sized frame
    // *** canonical frame ***
    //--------------------------------------------------
    else if (isAdjustSPByVar(*cur_insn)) {
      if (canon_ui != beg_ui && canon_ui->ty == FrmTy_SP) {
	int fp_pos = convertSPToFPPos(canon_ui->sp_pos, canon_ui->fp_pos);
	int ra_pos = convertSPToFPPos(canon_ui->sp_pos, canon_ui->ra_arg);
	nxt_ui = new_ui(nextInsn(cur_insn), FrmTy_FP, ui->flgs, 
			0, fp_pos, ra_pos, ui);
	ui = nxt_ui;

	canon_ui = canonFP_ui = nxt_ui;
      }
    }
    //--------------------------------------------------
    // FP-frame --> FP-frame: load RA by FP
    //--------------------------------------------------
    else if (isLoadRegFromFrame(*cur_insn, REG_FP, REG_RA)) {
      checkUI(ui, FrmTy_FP, cur_insn);
      nxt_ui = new_ui(nextInsn(cur_insn), FrmTy_FP, FrmFlg_RAReg,
		      ui->sp_pos, ui->fp_pos, REG_RA, ui);
      ui = nxt_ui;
    }
    /* else if (load-parent's-FP): FIXME */

    //--------------------------------------------------
    // FP-frame --> SP-frame: deallocate (all/part of) frame by restoring SP
    //--------------------------------------------------
    else if (isMoveReg(*cur_insn, REG_SP, REG_FP)) {
      if (ui->ty == FrmTy_FP) {
	bool isFullDealloc = (!frameflg_isset(canon_ui->flgs, FrmFlg_RAReg)
			      && frameflg_isset(ui->flgs, FrmFlg_RAReg));
	frameflg_t flgs = (isFullDealloc) ? FrmFlg_RAReg : canonSP_ui->flgs;
	int sp_pos      = (isFullDealloc) ? 0            : canonSP_ui->sp_pos;
	int fp_pos      = (isFullDealloc) ? unwpos_NULL  : canonSP_ui->fp_pos;
	int ra_arg      = (isFullDealloc) ? REG_RA       : canonSP_ui->ra_arg;
	nxt_ui = new_ui(nextInsn(cur_insn), FrmTy_SP, flgs, 
			sp_pos, fp_pos, ra_arg, ui);
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

  ui->common.end = INSN(end_insn);

  interval_status stat;

  stat.first_undecoded_ins = NULL;
  stat.errcode = 0;
  stat.first = (splay_interval_t*)beg_ui;


  if (verbose) {
    for (unw_interval_t* x = (unw_interval_t*)stat.first; 
	 x; x = (unw_interval_t*)x->common.next) {
      ui_dump(x, 0);
    }
  }

  return stat;
}


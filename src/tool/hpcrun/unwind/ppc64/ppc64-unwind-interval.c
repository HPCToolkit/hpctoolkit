
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
#include <include/min-max.h>

#include "ppc64-unwind-interval.h"

#include "hpcrun-malloc.h"
#include "ui_tree.h"

#include "fnbounds_interface.h"

#include <hpcrun/hpcrun_stats.h>
#include <messages/messages.h>

#include <lib/isa-lean/power/instruction-set.h>

#include <lib/prof-lean/atomic-op.h>


//*************************** Forward Declarations **************************

#define MYDBG 0

static interval_status 
ppc64_build_intervals(char *ins, unsigned int len);

static void
ppc64_print_interval_set(unw_interval_t *first);

static const char *
ra_ty_string(ra_ty_t ty); 

static const char *
sp_ty_string(sp_ty_t ty);



//***************************************************************************
// interface operations
//***************************************************************************

interval_status 
build_intervals(char *ins, unsigned int len)
{
  interval_status stat = ppc64_build_intervals(ins, len);
  
  if (MYDBG) {
    ppc64_print_interval_set((unw_interval_t *) stat.first);
  }

  return stat;
}


//***************************************************************************
// unw_interval_t interface
//***************************************************************************

unw_interval_t *
new_ui(char *start_addr, sp_ty_t sp_ty, ra_ty_t ra_ty, int sp_arg, int ra_arg,
       unw_interval_t *prev)
{
  unw_interval_t* u = (unw_interval_t*)hpcrun_ui_malloc(sizeof(unw_interval_t));

  u->common.start = start_addr;
  u->common.end = 0;
  u->common.prev = (splay_interval_t*)prev;
  u->common.next = NULL;

  if (prev) {
    ui_link(prev, u);
  }

  u->sp_ty  = sp_ty;
  u->ra_ty  = ra_ty;
  u->sp_arg = sp_arg;
  u->ra_arg = ra_arg;

  hpcrun_stats_num_unwind_intervals_total_inc();

  return u;
}


void 
ui_dump(unw_interval_t* u)
{
  if (!u) {
    return;
  }

  printf("  [%p, %p) ty=%-10s,%-10s sp_arg=%5d ra_arg=%5d\n",
       (void *) u->common.start, (void *) u->common.end, 
       sp_ty_string(u->sp_ty), ra_ty_string(u->ra_ty), u->sp_arg, u->ra_arg);
  // printf("     next=%p prev=%p", u->common.next, u->common.prev);
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
  current->common.end = next->common.start;
  current->common.next= (splay_interval_t*)next;
}



//***************************************************************************
// private operations 
//***************************************************************************

#define STR(s) case s: return #s

static const char *
ra_ty_string(ra_ty_t ty) 
{
  switch (ty) {
    STR(RATy_NULL);
    STR(RATy_Reg);
    STR(RATy_SPRel);
  default:
    assert(0);
  }
  return NULL;
}


static const char *
sp_ty_string(sp_ty_t ty)
{
  switch (ty) {
    STR(SPTy_NULL);
    STR(SPTy_Reg);
    STR(SPTy_SPRel);
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

static inline bool 
isInsn_MFLR(uint32_t insn, int* Rt)
{
  if ((insn & PPC_OP_XFX_SPR_MASK) == PPC_OP_MFLR) {
    *Rt = PPC_OPND_REG_T(insn);
    return true;
  }
  return false;
}


static inline bool 
isInsn_MTLR(uint32_t insn, int* Rt)
{
  if ((insn & PPC_OP_XFX_SPR_MASK) == PPC_OP_MTLR) {
    *Rt = PPC_OPND_REG_T(insn);
    return true;
  }
  return false;
}


static inline bool 
isInsn_STW(uint32_t insn, int Rs, int Ra)
{
  const int D = 0x0;  
  return (insn & PPC_INSN_D_MASK) == PPC_INSN_D(PPC_OP_STW, Rs, Ra, D);
}


static inline bool 
isInsn_STD(uint32_t insn, int Rs, int Ra)
{
  // std Rs Ra: store Rs at (Ra + D); set Ra to (Ra + D)
  const int D = 0x0;
  return (insn & (PPC_INSN_DS_MASK)) == PPC_INSN_DS(PPC_OP_STD, Rs, Ra, D);
}


static inline bool 
isInsn_LWZ(uint32_t insn, int Rt, int Ra)
{
  const int D = 0x0;
  return (insn & PPC_INSN_D_MASK) == PPC_INSN_D(PPC_OP_LWZ, Rt, Ra, D);
}


static inline bool 
isInsn_STWU(uint32_t insn, int Rs, int Ra)
{
  // stwu Rs Ra: store Rs at (Ra + D); set Ra to (Ra + D)
  const int D = 0x0;
  return (insn & PPC_INSN_D_MASK) == PPC_INSN_D(PPC_OP_STWU, Rs, Ra, D);
}


static inline bool 
isInsn_STDU(uint32_t insn, int Rs, int Ra)
{
  // stdu Rs Ra: store Rs at (Ra + D); set Ra to (Ra + D)
  const int D = 0x0;
  return (insn & (PPC_INSN_DS_MASK)) == PPC_INSN_DS(PPC_OP_STDU, Rs, Ra, D);
}


static inline bool 
isInsn_STWUX(uint32_t insn, int Ra)
{   
  // stwux Rs Ra Rb: store Rs at (Ra + Rb); set Ra to (Ra + Rb)
  const int Rs = 0, Rb = 0, Rc = 0x0;
  return ((insn & (PPC_OP_X_MASK | PPC_OPND_REG_A_MASK))
	  == PPC_INSN_X(PPC_OP_STWUX, Rs, Ra, Rb, Rc));
}

static inline bool 
isInsn_STDUX(uint32_t insn, int Ra)
{   
  // stdux Rs Ra Rb: store Rs at (Ra + Rb); set Ra to (Ra + Rb)
  const int Rs = 0, Rb = 0, Rc = 0x0;
  return ((insn & (PPC_OP_X_MASK | PPC_OPND_REG_A_MASK))
	  == PPC_INSN_X(PPC_OP_STDUX, Rs, Ra, Rb, Rc));
}


static inline bool 
isInsn_ADDI(uint32_t insn, int Rt, int Ra)
{
  const int SI = 0x0;
  return ((insn & PPC_INSN_D_MASK) == PPC_INSN_D(PPC_OP_ADDI, Rt, Ra, SI));
}


static inline bool 
isInsn_MR(uint32_t insn, int Ra)
{
  // mr Ra Rs = or Ra Rs Rb where Rs = Rb
  const int Rs = 0, Rc = 0x0;
  bool isMoveToRa = ((insn & (PPC_OP_X_MASK | PPC_OPND_REG_A_MASK))
		     == PPC_INSN_X(PPC_OP_MR, Rs, Ra, Rs, Rc));
  bool isRsEqRb = (PPC_OPND_REG_S(insn) == PPC_OPND_REG_B(insn));
  return (isMoveToRa && isRsEqRb);
}


static inline bool 
isInsn_BLR(uint32_t insn)
{ 
  return (insn == PPC_OP_BLR); 
}


//***************************************************************************

static inline int 
getRADispFromSPDisp(int sp_disp) 
{ 
#ifdef __PPC64__
  int disp = sp_disp + 2 * (sizeof(void*)); 
#else
  int disp = sp_disp + (sizeof(void*)); 
#endif
  return disp;
}


static inline int
getSPDispFromUI(unw_interval_t* ui)
{ 
  // if sp_ty != SPTy_SPRel, then frame size is 0
  return (ui->sp_ty == SPTy_SPRel) ? ui->sp_arg : 0;
}


#define INSN(insn) ((char*)(insn))

static inline char*
nextInsn(uint32_t* insn) 
{ return INSN(insn + 1); }


//***************************************************************************
// build_intervals:
//***************************************************************************

// R1 (almost) invariably remains the stack pointer, even for dynamically
// sized frames or very large frames.  Moreover, every non-leaf procedure
// stores the SP using stwu/stwux, creating a linked list of SPs.
//
// Exception: sometimes R1 appears to be updated non-atomically (i.e., without
// a variant of stwu):
//   lwz  r0,0(r1)  [load parent-SP, located at SP/r1]
//   mr   r1,r3     [clobber SP]
//   stw  r0,0(r3)  [save parent-SP at r3, making SP valid!] <-- sample!
//
// PPC frame layout:
//
//  bottom (outermost frame)
//  |-------------|
// A|             |
//  | RA          | <- stored by caller
//  | CR          |    (64-bit ABI only) 
//  | SP <---     | <- stored w/ stwu
//  |--------/----|
// B|       /     |                          Typical frame
//  | RA   /      | <- (by C)
//  | CR  /       |    (64-bit ABI only) 
//  | SP / <- <-  | <- (stwu)
//  |-------/--|--|
// C|      /   |  |                          Possible nasty frame
//  | []  /    |  | <- 
//  | SP /    /   | <- (stwu)
//  |- - - - / - -|  
//  |       /     | (xtra frame)
//  |      /      |
//  |     /       |
//  | SP /        | <- stwux
//  |-------------|


// Typical PPC frames (constant frame size <= 16 bits)
//   stwu r1, -32(r1)   ! store with update: store SP (r1) at -32(r1)
//                        and *then* set SP (r1) to -32(r1)
//   mflr r0            ! move from LR: (and store into RA/r0)
//   stw  r0, 36(r1)    ! store word: store RA (r0) in *parent* frame
//
//   ... compute ...
//
//   lwz r0, 36(r1)     ! load LR
//   mtlr r0            ! mtlr: move to LR (from r0)
//   blr                ! blr: return!
//
// States:
//   1. RA is in register (LR/r0); SP (r1) points to parent's frame
//   2. RA is in register (LR/r0); SP (r1) has been stored & updated
//   3. RA is SP-relative        ; SP (r1) has been stored & updated


// Nasty PPC frames have few common characteristics between compilers
// (frame size > 16 bit displacement field)
//   mflr    r0
//   mr      r12,r1       ! 
//   stw     r0,4(r1)     ! save RA in parent frame (note frame size is 0)
//   lis     r0,-1        ! r0 <- 0xffff0000 (-65536)
//   addic   r0,r0,-1664  ! r0 <- 67200 = -65536 + -1664
//   stwux   r1,r1,r0     ! store r1 at (r1 + r0); set r1 to (r1 + r0)
//
//   ... compute: cobber r12, of course! ...
// 
//   addis   r11,r1,1     ! r11 <- r1 + [0x10000 (65536)]
//   addi    r11,r11,1664 ! r11 <- r11 + 1664
//   lwz     r0,4(r11)    ! restore RA (r0)
//   mr      r1,r11       ! restore SP (r1)
//   mtlr    r0
//   blr

// flash: runtimeparameters_read (above)
// flash: <_ZN4DCMF7MappingC2ERNS_11PersonalityERNS_13MemoryManagerERNS_3Log3LogE>:
//   lis     r0,-3
//   mr      r12,r1
//   ori     r0,r0,49040
//   stwux   r1,r1,r0
//   mflr    r0
//   stw     r0,4(r12)
//   ...
//   lwz     r11,0(r1)
//   lwz     r0,4(r11)
//   mtlr    r0
//   mr      r1,r11

// Another nasty frame now in t1:



static interval_status 
ppc64_build_intervals(char *beg_insn, unsigned int len)
{
  unw_interval_t* beg_ui = 
    new_ui(beg_insn, SPTy_Reg, RATy_Reg, PPC_REG_SP, PPC_REG_LR, NULL);
  unw_interval_t* ui = beg_ui;
  unw_interval_t* nxt_ui = NULL;
  unw_interval_t* canon_ui = beg_ui;

  uint32_t* cur_insn = (uint32_t*) beg_insn;
  uint32_t* end_insn = (uint32_t*) (beg_insn + len);

  int reg;

  while (cur_insn < end_insn) {
    //TMSG(INTV, "insn: 0x%x [%p,%p)", *cur_insn, cur_insn, end_insn);

    //--------------------------------------------------
    // move return address from LR (to 'reg')
    //--------------------------------------------------
    if (ui->ra_ty == RATy_Reg &&
	ui->ra_arg == PPC_REG_LR &&
	isInsn_MFLR(*cur_insn, &reg)) {
      nxt_ui = new_ui(nextInsn(cur_insn), 
		      ui->sp_ty, RATy_Reg, ui->sp_arg, reg, ui);
      ui = nxt_ui;
    }
    //--------------------------------------------------
    // move return address to LR (from 'reg')
    //--------------------------------------------------
    else if (isInsn_MTLR(*cur_insn, &reg)) {
      // TODO: could scan backwards based on 'reg' (e.g., isInsn_LWZ)
      nxt_ui = new_ui(nextInsn(cur_insn), 
		      ui->sp_ty, RATy_Reg, ui->sp_arg, PPC_REG_LR, ui);
      ui = nxt_ui;
    }
    //--------------------------------------------------
    // store return address into parent's frame
    //   (may come before or after frame allocation)
    //--------------------------------------------------
    else if (ui->ra_ty == RATy_Reg && 
	     ui->ra_arg >= PPC_REG_R0 &&
	     isInsn_STW(*cur_insn, ui->ra_arg, PPC_REG_SP)) {
      int sp_disp = getSPDispFromUI(ui);
      int ra_disp = PPC_OPND_DISP(*cur_insn);
      if (getRADispFromSPDisp(sp_disp) == ra_disp) {
        nxt_ui = new_ui(nextInsn(cur_insn), 
			ui->sp_ty, RATy_SPRel, ui->sp_arg, ra_disp, ui);
        ui = nxt_ui;

	canon_ui = nxt_ui;
      }
    }
    //--------------------------------------------------
    // store return address into parent's frame
    //   (may come before or after frame allocation)
    //--------------------------------------------------
    else if (ui->ra_ty == RATy_Reg && 
	     ui->ra_arg >= PPC_REG_R0 &&
	     isInsn_STD(*cur_insn, ui->ra_arg, PPC_REG_SP)) {
      int sp_disp = getSPDispFromUI(ui);
      int ra_disp = PPC_OPND_DISP_DS(*cur_insn);
      if (getRADispFromSPDisp(sp_disp) == ra_disp) {
        nxt_ui = new_ui(nextInsn(cur_insn), 
			ui->sp_ty, RATy_SPRel, ui->sp_arg, ra_disp, ui);
        ui = nxt_ui;

	canon_ui = nxt_ui;
      }
    }
    //--------------------------------------------------
    // load return address from caller's frame (into R0)
    //--------------------------------------------------
    else if (isInsn_LWZ(*cur_insn, PPC_REG_RA, PPC_REG_SP)) {
      int sp_disp = getSPDispFromUI(ui);
      int ra_disp = PPC_OPND_DISP(*cur_insn);
      if (getRADispFromSPDisp(sp_disp) == ra_disp) {
	nxt_ui = new_ui(nextInsn(cur_insn), 
			ui->sp_ty, RATy_Reg, ui->sp_arg, PPC_REG_R0, ui);
	ui = nxt_ui;
      }
    }
    //--------------------------------------------------
    // allocate frame: adjust SP and store parent's SP
    //   (may come before or after storing of RA)
    //--------------------------------------------------
    else if (isInsn_STWU(*cur_insn, PPC_REG_SP, PPC_REG_SP)) {
      int sp_disp = - PPC_OPND_DISP(*cur_insn);
      int ra_arg = ((ui->ra_ty == RATy_SPRel) ? 
		    ui->ra_arg + sp_disp : ui->ra_arg);
      nxt_ui = new_ui(nextInsn(cur_insn), 
		      SPTy_SPRel, ui->ra_ty, sp_disp, ra_arg, ui);
      ui = nxt_ui;

      canon_ui = nxt_ui;
    }
    //--------------------------------------------------
    // allocate frame: adjust SP and store parent's SP
    //   (may come before or after storing of RA)
    //--------------------------------------------------
    else if (isInsn_STDU(*cur_insn, PPC_REG_SP, PPC_REG_SP)) {
      int sp_disp = - PPC_OPND_DISP_DS(*cur_insn);  
      int ra_arg = ((ui->ra_ty == RATy_SPRel) ? 
		    ui->ra_arg + sp_disp : ui->ra_arg);
      nxt_ui = new_ui(nextInsn(cur_insn), 
		      SPTy_SPRel, ui->ra_ty, sp_disp, ra_arg, ui);
      ui = nxt_ui;

      canon_ui = nxt_ui;
    }
    else if (isInsn_STWUX(*cur_insn, PPC_REG_SP)) {
      int sp_disp = -1; // N.B. currently we do not track this
      nxt_ui = new_ui(nextInsn(cur_insn),
		      SPTy_SPRel, ui->ra_ty, sp_disp, ui->ra_arg, ui);
      ui = nxt_ui;

      canon_ui = nxt_ui;
    }
    else if (isInsn_STDUX(*cur_insn, PPC_REG_SP)) {
      int sp_disp = -1; // N.B. currently we do not track this
      nxt_ui = new_ui(nextInsn(cur_insn),
		      SPTy_SPRel, ui->ra_ty, sp_disp, ui->ra_arg, ui);
      ui = nxt_ui;

      canon_ui = nxt_ui;
    }
    //--------------------------------------------------
    // deallocate frame: reset SP to parents's SP
    //--------------------------------------------------
    else if (isInsn_ADDI(*cur_insn, PPC_REG_SP, PPC_REG_SP)
	     && (PPC_OPND_DISP(*cur_insn) == getSPDispFromUI(ui))) {
      int sp_disp = - PPC_OPND_DISP(*cur_insn);  
      int ra_arg = ((ui->ra_ty == RATy_SPRel) ? 
		    ui->ra_arg + sp_disp : ui->ra_arg);
#if 0
      // debug return address offset not being adjusted properly.
      // now fixed.
      printf("addi: ra_ty=%d, sp_disp = %d, ui->ra_arg = %d, ra_arg = %d\n", ui->ra_ty, sp_disp, ui->ra_arg, ra_arg);
#endif
      nxt_ui = new_ui(nextInsn(cur_insn), 
		      SPTy_Reg, ui->ra_ty, PPC_REG_SP, ra_arg, ui);
      ui = nxt_ui;
    }
    else if (isInsn_MR(*cur_insn, PPC_REG_SP)) {
      // N.B. To be sure the MR restores SP, we would have to track
      // registers.  As a sanity check, test for a non-zero frame size
      if (getSPDispFromUI(ui) != 0) {
	nxt_ui = new_ui(nextInsn(cur_insn), 
			SPTy_Reg, ui->ra_ty, PPC_REG_SP, ui->ra_arg, ui);
	ui = nxt_ui;
      }
    }
    //--------------------------------------------------
    // interior returns/epilogues
    //--------------------------------------------------
    else if (isInsn_BLR(*cur_insn) && (cur_insn + 1 < end_insn)) {
      // TODO: ensure that frame has been deallocated and mtlr issued
      // and adjust intervals if necessary.

      // An interior return.  Restore the canonical interval if necessary.
      if (!ui_cmp(ui, canon_ui)) {
	nxt_ui = new_ui(nextInsn(cur_insn), canon_ui->sp_ty, canon_ui->ra_ty,
			canon_ui->sp_arg, canon_ui->ra_arg, ui);
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


void 
ppc64_dump_intervals(void* addr)
{
  void *s, *e;
  interval_status intervals;

  fnbounds_enclosing_addr(addr, &s, &e, NULL);

  uintptr_t llen = ((uintptr_t)e) - (uintptr_t)s;

  printf("build intervals from %p to %p (%"PRIuPTR")\n", s, e, llen);
  intervals = ppc64_build_intervals(s, (unsigned int) llen);

  ppc64_print_interval_set((unw_interval_t *) intervals.first);
}

void
hpcrun_dump_intervals(void* addr)
{
  ppc64_dump_intervals(addr);
}

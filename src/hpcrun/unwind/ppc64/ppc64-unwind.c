// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
//
// HPCToolkit's PPC64 Unwinder
//
//***************************************************************************


//************************* System Include Files ****************************

#define _GNU_SOURCE

#include <stddef.h>
#include <stdbool.h>
#include <ucontext.h>
#include <assert.h>


//************************ External Include Files *************************

#include "../../libmonitor/monitor.h"


//*************************** User Include Files ****************************

#include "../common/unwind.h"
#include "../common/unw-datatypes.h"
#include "../common/libunw_intervals.h"

#include "ppc64-unwind-interval.h"

#include "../../sample_event.h"

#include "../common/uw_recipe_map.h"

#include "../../messages/messages.h"

#include "../../utilities/ip-normalized.h"
#include "../../utilities/arch/mcontext.h"

#include "../../utilities/arch/ppc64/instruction-set.h"
#include "../common/fence_enum.h"



//***************************************************************************
// macros
//***************************************************************************

#define MYDBG 0



//***************************************************************************
// type declarations
//***************************************************************************

//
// register codes (only 1 at the moment)
//
typedef unw_frame_regnum_t unw_reg_code_t;

typedef enum {
  UnwFlg_NULL = 0,
  UnwFlg_StackTop,
} unw_flag_t;



//***************************************************************************
// forward declarations
//***************************************************************************

static fence_enum_t
hpcrun_check_fence(void* ip);



//***************************************************************************
// private functions
//***************************************************************************

static void
save_registers(hpcrun_unw_cursor_t* cursor, void *pc, void *bp, void *sp,
               void *ra)
{
  cursor->pc_unnorm = pc;
  cursor->bp        = bp;
  cursor->sp        = sp;
  cursor->ra        = ra;
}

static void
compute_normalized_ips(hpcrun_unw_cursor_t* cursor)
{
  void *func_start_pc =  (void*) cursor->unwr_info.interval.start;
  load_module_t* lm = cursor->unwr_info.lm;

  cursor->pc_norm = hpcrun_normalize_ip(cursor->pc_unnorm, lm);
  cursor->the_function = hpcrun_normalize_ip(func_start_pc, lm);
}



static bool
fence_stop(fence_enum_t fence)
{
  return (fence == FENCE_MAIN) || (fence == FENCE_THREAD);
}


static fence_enum_t
hpcrun_check_fence(void* ip)
{
  fence_enum_t rv = FENCE_NONE;
  if (monitor_unwind_process_bottom_frame(ip))
    rv = FENCE_MAIN;
  else if (monitor_unwind_thread_bottom_frame(ip))
    rv = FENCE_THREAD;

   if (ENABLED(FENCE_UNW) && rv != FENCE_NONE)
     TMSG(FENCE_UNW, "%s", fence_enum_name(rv));
   return rv;
}


static int
hpcrun_unw_get_unnorm_reg(hpcrun_unw_cursor_t* cursor, unw_reg_code_t reg_id,
                          void** reg_value)
{
  if (reg_id != UNW_REG_IP)
    hpcrun_terminate();
  *reg_value = cursor->pc_unnorm;

  return 0;
}


static int
hpcrun_unw_get_norm_reg(hpcrun_unw_cursor_t* cursor, unw_reg_code_t reg_id,
                        ip_normalized_t* reg_value)
{
  if (reg_id != UNW_REG_IP)
    hpcrun_terminate();
  *reg_value = cursor->pc_norm;

  return 0;
}


//***************************************************************************
// interface functions
//***************************************************************************

void
hpcrun_unw_init(void)
{
  static bool msg_sent = false;
  if (msg_sent == false) {
    TMSG(NU, "hpcrun_unw_init from ppc64_unwind.c" );
    msg_sent = true;
  }

  uw_recipe_map_init();
}

int
hpcrun_unw_get_ip_norm_reg(hpcrun_unw_cursor_t* c, ip_normalized_t* reg_value)
{
  return hpcrun_unw_get_norm_reg(c, UNW_REG_IP, reg_value);
}


int
hpcrun_unw_get_ip_unnorm_reg(hpcrun_unw_cursor_t* c, void** reg_value)
{
  return hpcrun_unw_get_unnorm_reg(c, UNW_REG_IP, reg_value);
}


//
// unimplemented for now
//  fix when trampoline support is added
//
void*
hpcrun_unw_get_ra_loc(hpcrun_unw_cursor_t* cursor)
{
  return cursor->ra_loc;
}

void *
getNxtPCFromReg(hpcrun_unw_cursor_t* cursor)
{
        unwind_interval *intvl = cursor->unwr_info.btuwi;
        if (intvl && UWI_RECIPE(intvl)->ra_ty == RATy_Reg) {
                if (UWI_RECIPE(intvl)->ra_arg == PPC_REG_LR) {
                        return (void*)(cursor->ctxt->uc_mcontext.regs->link);
                } else {
                        return (void*)(cursor->ctxt->uc_mcontext.regs->gpr[UWI_RECIPE(intvl)->ra_arg]);
                }
        }
        return 0;
}


void
hpcrun_unw_init_cursor(hpcrun_unw_cursor_t* cursor, void* context)
{
  ucontext_t* ctxt = (ucontext_t*)context;

  save_registers(cursor, ucontext_pc(ctxt), NULL, ucontext_sp(ctxt), NULL);
  cursor->pc_norm   = (ip_normalized_t) {0, 0};
  cursor->ra        = NULL;
  cursor->sp        = ucontext_sp(ctxt);
  cursor->bp        = NULL;
  cursor->flags     = UnwFlg_StackTop;
  cursor->ctxt      = ctxt;
  cursor->ra_loc    = NULL;

  bitree_uwi_t* intvl = NULL;

  bool found = uw_recipe_map_lookup(cursor->pc_unnorm, NATIVE_UNWINDER, &(cursor->unwr_info));
  if (found) {
        intvl = cursor->unwr_info.btuwi;
          if (intvl && UWI_RECIPE(intvl)->ra_ty == RATy_Reg) {
            if (UWI_RECIPE(intvl)->ra_arg == PPC_REG_LR) {
              cursor->ra = (void*)(ctxt->uc_mcontext.regs->link);
              cursor->ra_loc = &(ctxt->uc_mcontext.regs->link);
            }
            else {
              cursor->ra = (void*)(ctxt->uc_mcontext.regs->gpr[UWI_RECIPE(intvl)->ra_arg]);
              cursor->ra_loc = &(ctxt->uc_mcontext.regs->gpr[UWI_RECIPE(intvl)->ra_arg]);
            }
          }
  }
  else {
    EMSG("unw_init: cursor could NOT build an interval for initial pc = %p",
         cursor->pc_unnorm);
  }

  compute_normalized_ips(cursor);

  TMSG(UNW, "init: pc=%p, sp=%p, fp=%p",
       cursor->pc_unnorm, cursor->sp, cursor->bp);
  if (MYDBG) { ui_dump(intvl); }
}


// --FIXME--: add advanced fence processing and enclosing function to cursor here
//

step_state
hpcrun_unw_step(hpcrun_unw_cursor_t *cursor)
{
  // current frame
  void*  pc = cursor->pc_unnorm;
  void** sp = cursor->sp;
  void** fp = cursor->bp; // unused
  unwind_interval* intvl = (unwind_interval*)(cursor->unwr_info.btuwi);

  static bool msg_sent = false;
  if (msg_sent == false) {
    TMSG(NU, "hpcrun_unw_step from ppc64_unwind.c" );
    msg_sent = true;
  }

  bool isInteriorFrm = (cursor->flags != UnwFlg_StackTop);

  // next (parent) frame
  void*           nxt_pc = NULL;
  void** nxt_sp = NULL;
  void** nxt_fp = NULL; // unused
  void*  nxt_ra = NULL; // always NULL unless we go through a signal handler
  void*  ra_loc = NULL;
  unwind_interval* nxt_intvl = NULL;

  if (!intvl) {
    TMSG(UNW, "error: missing interval for pc=%p", pc);
    return STEP_ERROR;
  }


  cursor->fence = hpcrun_check_fence(cursor->pc_unnorm);

  //-----------------------------------------------------------
  // check if we have reached the end of our unwind, which is
  // demarcated with a fence.
  //-----------------------------------------------------------
  if (fence_stop(cursor->fence)) {
    TMSG(UNW,"unw_step: STEP_STOP, current pc in monitor fence pc=%p\n", cursor->pc_unnorm);
    return STEP_STOP;
  }

  if ((void*)sp >= monitor_stack_bottom()) {
    TMSG(FENCE_UNW, "stop: sp (%p) >= unw_stack_bottom", sp);
    cursor->fence = FENCE_MAIN;
    return STEP_ERROR;
  }

  //-----------------------------------------------------------
  // compute SP (stack pointer) for the caller's frame.  Do this first
  //   because we rely on the invariant that an interior frame contains
  //   a stack pointer and, above the stack pointer, a return address.
  //-----------------------------------------------------------
  if (UWI_RECIPE(intvl)->sp_ty == SPTy_Reg) {
    // SP already points to caller's stack frame
    nxt_sp = sp;
  }
  else if (UWI_RECIPE(intvl)->sp_ty == SPTy_SPRel) {
    // SP points to parent's SP
    nxt_sp = *sp;
  }
  else {
    // assert(0);
    EMSG("unwind failure computing SP at pc: %p, sp: %p", pc, sp);
    return STEP_ERROR;
  }


  //-----------------------------------------------------------
  // compute RA (return address) for the caller's frame
  //-----------------------------------------------------------
  if (UWI_RECIPE(intvl)->ra_ty == RATy_Reg) {
    nxt_pc = getNxtPCFromReg(cursor);
    if (nxt_pc == 0) {
       EMSG("bad register-based unwind at pc: %p, sp: %p", pc, sp);
       return STEP_ERROR;
    }
    if (nxt_pc > 0) nxt_pc -= 1;  // Move to caller
    ra_loc = cursor->ra_loc;
  } else if (UWI_RECIPE(intvl)->ra_ty == RATy_SPRel) {
    nxt_pc = getNxtPCFromSP(nxt_sp);
    if (nxt_pc > 0) nxt_pc -= 1;  // Move to caller
    ra_loc = (void*)getNxtPCLocFromSP(nxt_sp);
  }
  else {
    // assert(0);
    EMSG("unwind failure computing RA at pc: %p, sp: %p", pc, sp);
    return STEP_ERROR;
  }


  //-----------------------------------------------------------
  // compute unwind information for the caller's pc
  //-----------------------------------------------------------
  bool found = uw_recipe_map_lookup(nxt_pc, NATIVE_UNWINDER, &(cursor->unwr_info));
  if (found) {
        nxt_intvl = cursor->unwr_info.btuwi;
  }

  // if nxt_pc is invalid for some reason...
  if (!nxt_intvl) {
        TMSG(UNW, "warning: bad nxt pc=%p; sp=%p, fp=%p...", nxt_pc, sp, fp);

        //-------------------------------------------------------------------
        // If this is a leaf frame, assume the interval didn't correctly
        // track the return address.  Try one frame deeper.
        //-------------------------------------------------------------------
        void** try_sp = NULL;
        if (!isInteriorFrm) {
          try_sp = *nxt_sp;

          // Sanity check SP: Once in a while SP is clobbered.
          if (isPossibleParentSP(nxt_sp, try_sp)) {
                nxt_pc = getNxtPCFromSP(try_sp);
    ra_loc = (void*)getNxtPCLocFromSP(try_sp);
                bool found2 = uw_recipe_map_lookup(nxt_pc, NATIVE_UNWINDER, &(cursor->unwr_info));
                if (found2) {
                  nxt_intvl = cursor->unwr_info.btuwi;
                  if (nxt_pc > 0) nxt_pc -= 1; // Move to caller
                }
          }
        }

        if (!nxt_intvl) {
          TMSG(UNW, "error: skip-frame failed: nxt pc=%p, sp=%p; try sp=%p",
                  nxt_pc, nxt_sp, try_sp);
          return STEP_ERROR;
        }

        // INVARIANT: 'try_sp' is valid
        nxt_sp = try_sp;
        TMSG(UNW, "skip-frame: nxt pc=%p, sp=%p", nxt_pc, nxt_sp);
  }
  // INVARIANT: At this point, 'nxt_intvl' is valid


  // INVARIANT: Ensure we always make progress unwinding ...
  bool mayFrameSizeBe0 = (UWI_RECIPE(intvl)->sp_ty == SPTy_Reg);
  if (mayFrameSizeBe0) {
    if (cursor->pc_unnorm == nxt_pc && cursor->sp == nxt_sp) {
      // no forward progress with a register-based unwind
      TMSG(UNW, "error: loop! pc = nxt_pc = %p sp = nxt_sp = %p", nxt_pc, nxt_sp);
      return STEP_ERROR;
    }
  } else {
    if (!isPossibleParentSP(sp, nxt_sp)) {
      // no forward progress with a stack-based unwind
      TMSG(UNW, "error: loop! nxt_sp=%p, sp=%p", nxt_sp, sp);
      return STEP_ERROR;
    }
  }


  TMSG(UNW, "next: pc=%p, sp=%p, fp=%p", nxt_pc, nxt_sp, nxt_fp);
  if (MYDBG) { ui_dump(nxt_intvl); }

  save_registers(cursor, nxt_pc, nxt_fp, nxt_sp, nxt_ra);
  cursor->ra_loc    = ra_loc;
  cursor->flags     = UnwFlg_NULL;

  compute_normalized_ips(cursor);

  return STEP_OK;
}

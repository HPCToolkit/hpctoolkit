#include <stdio.h>
#include <stdbool.h>
#include "x86-addsub.h"
#include "x86-call.h"
#include "x86-decoder.h"
#include "x86-enter.h"
#include "x86-leave.h"
#include "x86-jump.h"
#include "x86-move.h"
#include "x86-push.h"
#include "x86-unwind-analysis.h"
#include "x86-return.h"
#include "x86-interval-arg.h"
#include "ui_tree.h"

//
// Cold code interval utilities
//


/* static */ void
cold_code_fixup(unwind_interval *current, unwind_interval *warm)
{
  ETMSG(COLD_CODE,"  --fixing up current intervals with the warm interval");
  int ra_offset = warm->sp_ra_pos;
  int bp_offset = warm->sp_bp_pos;
  if (ra_offset == 0) {
    ETMSG(COLD_CODE,"  --warm code calling routine has offset 0, so no action taken");
    return;
  }
  ETMSG(COLD_CODE,"  --updating sp_ra_pos with offset %d",ra_offset);
  for(unwind_interval *intv = current;intv;intv = (unwind_interval *)intv->common.prev) {
    intv->sp_ra_pos += ra_offset;
    intv->sp_bp_pos += bp_offset;
  }
}

// Confirm that the previous instruction is a conditional branch to beginning of
// cold call routine

/* static */ bool
confirm_cold_path_call(void *loc, interval_arg_t *iarg)
{
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  xed_error_enum_t xed_error;
  xed_decoded_inst_zero_set_mode(xptr, &x86_decoder_settings.xed_settings);
  xed_decoded_inst_zero_keep_mode(xptr);
  void *possible_call = loc - 6;
  void *routine       = iarg->beg;     
  xed_error = xed_decode(xptr, (uint8_t *)possible_call, 15);

  ETMSG(COLD_CODE,"  --trying to confirm a cold code 'call' from addr %p",possible_call);
  if (xed_error != XED_ERROR_NONE) {
    ETMSG(COLD_CODE,"  --addr %p has xed decode error when attempting confirm",possible_call);
    return false;
  }

  xed_iclass_enum_t xiclass = xed_decoded_inst_get_iclass(xptr);
  switch(xiclass) {
  case XED_ICLASS_JBE: 
  case XED_ICLASS_JL: 
  case XED_ICLASS_JLE: 
  case XED_ICLASS_JNB:
  case XED_ICLASS_JNBE: 
  case XED_ICLASS_JNL: 
  case XED_ICLASS_JNLE: 
  case XED_ICLASS_JNO:
  case XED_ICLASS_JNP:
  case XED_ICLASS_JNS:
  case XED_ICLASS_JNZ:
  case XED_ICLASS_JO:
  case XED_ICLASS_JP:
  case XED_ICLASS_JRCXZ:
  case XED_ICLASS_JS:
  case XED_ICLASS_JZ:
    ETMSG(COLD_CODE,"  --conditional branch confirmed @ %p",possible_call);
    void *the_call = x86_get_branch_target(possible_call,xptr);
    ETMSG(COLD_CODE,"  --comparing 'call' %p to actual routine %p",the_call,routine);
    return (the_call == routine);
    break;
  default:
    ETMSG(COLD_CODE,"  --No conditional branch @ %p, so NOT a cold call",possible_call);
    return false;
  }
  EMSG("confirm cold path call shouldn't get here!");
  return false;
}

// The cold code detector is called when unconditional jump is encountered

/* static */ bool
is_cold_code(void *ins, xed_decoded_inst_t *xptr, interval_arg_t *iarg)
{
  void *branch_target = x86_get_branch_target(ins,xptr);
  if (((ins + xed_decoded_inst_get_length(xptr)) == iarg->end) &&
      !(iarg->beg <= branch_target && branch_target <= iarg->end)){
    // this is a possible cold code routine
    ETMSG(COLD_CODE,"potential cold code jmp detected in routine starting @ %p",iarg->beg);
    void *beg, *end;
    if (fnbounds_enclosing_addr(branch_target, &beg, &end)){
      EEMSG("Wierd result! branch_target %p has no function bounds",branch_target);
      return false;
    }
    if (!(branch_target > beg)){
      ETMSG(COLD_CODE,"  --jump is a regular tail call, NOT a cold code return");
      return false;
    }
    iarg->return_addr = branch_target; // store the address of the branch, in case this turns out to be a
                                       // cold path routine.

    return confirm_cold_path_call(branch_target,iarg);
  }
  return false;
}

unwind_interval *process_inst(xed_decoded_inst_t *xptr, void **ins_ptr, void *end,
			      unwind_interval **current_ptr, unwind_interval *first,
			      bool *bp_just_pushed, 
			      highwatermark_t *highwatermark,
			      unwind_interval **canonical_interval, 
			      bool *bp_frames_found, char **rax_rbp_equivalent_at,
                              interval_arg_t *iarg)
{
  void *ins = *ins_ptr;
  unwind_interval *current = *current_ptr;
  xed_iclass_enum_t xiclass = xed_decoded_inst_get_iclass(xptr);
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  unwind_interval *next;
  int irdebug = 0;

  switch(xiclass) {

  case XED_ICLASS_JMP: 
  case XED_ICLASS_JMP_FAR:
    next = process_unconditional_branch(xptr, current, ins, end, 
					irdebug, first, highwatermark, 
					canonical_interval, *bp_frames_found);

    if (is_cold_code(ins, xptr, iarg)) {
      ETMSG(COLD_CODE,"  --cold code routine detected!");
      ETMSG(COLD_CODE,"fetching interval from location %p",iarg->return_addr);
      unwind_interval *ui = (unwind_interval *) csprof_addr_to_interval_unlocked(iarg->return_addr);
      ETMSG(COLD_CODE,"got unwind interval from csprof_addr_to_interval");
      if (ENABLED(COLD_CODE)) {
        dump_ui_stderr(ui);
      }
      // Fixup current intervals w.r.t. the warm code interval
      cold_code_fixup(current, ui);
    }

    break;

  case XED_ICLASS_JBE: 
  case XED_ICLASS_JL: 
  case XED_ICLASS_JLE: 
  case XED_ICLASS_JNB:
  case XED_ICLASS_JNBE: 
  case XED_ICLASS_JNL: 
  case XED_ICLASS_JNLE: 
  case XED_ICLASS_JNO:
  case XED_ICLASS_JNP:
  case XED_ICLASS_JNS:
  case XED_ICLASS_JNZ:
  case XED_ICLASS_JO:
  case XED_ICLASS_JP:
  case XED_ICLASS_JRCXZ:
  case XED_ICLASS_JS:
  case XED_ICLASS_JZ:
    next = process_conditional_branch(current, highwatermark);
    break;

  case XED_ICLASS_FNSTCW:
  case XED_ICLASS_STMXCSR:
    if (highwatermark->succ_inst_ptr == ins) { 
      //----------------------------------------------------------
      // recognize Pathscale idiom for routine prefix and ignore
      // any highwatermark setting that resulted from it.
      //
      // 20 December 2007 - John Mellor-Crummey
      //----------------------------------------------------------
      highwatermark_t empty_highwatermark = { NULL, NULL, HW_UNINITIALIZED };
      *highwatermark = empty_highwatermark;
    }
    next = current;
    break;

  case XED_ICLASS_LEA: 
    next = process_lea(ins, xptr, xi, current, highwatermark);
    break;

  case XED_ICLASS_MOV: 
    next = process_move(ins, xptr, xi, current, highwatermark, rax_rbp_equivalent_at);
    break;

  case XED_ICLASS_ENTER:
    next = process_enter(ins, xptr, xi, current, highwatermark);
    break;

  case XED_ICLASS_LEAVE:
    next = process_leave(ins, xptr, xi, current, highwatermark);
    break;

  case XED_ICLASS_CALL_FAR:
  case XED_ICLASS_CALL_NEAR:
    next = process_call(current, highwatermark);
    break;

  case XED_ICLASS_RET_FAR:
  case XED_ICLASS_RET_NEAR:
    next = process_return(xptr, current_ptr, ins_ptr, end, irdebug, first, 
			  highwatermark, canonical_interval, bp_frames_found);
    break;

  case XED_ICLASS_ADD:   
  case XED_ICLASS_SUB: 
    next = process_addsub(ins, xptr, xi, current, highwatermark, 
			  canonical_interval, bp_frames_found);
    break;

  case XED_ICLASS_PUSH:   
  case XED_ICLASS_PUSHF:  
  case XED_ICLASS_PUSHFD: 
  case XED_ICLASS_PUSHFQ: 
    next = process_push(ins, xptr, xi, current, highwatermark);
    break;

  case XED_ICLASS_POP:   
  case XED_ICLASS_POPF:  
  case XED_ICLASS_POPFD: 
  case XED_ICLASS_POPFQ: 
    next = process_pop(ins, xptr, xi, current, highwatermark);
    break;

  default:
    next = current;
    break;
  }

  return next;
}


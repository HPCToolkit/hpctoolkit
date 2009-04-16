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
#include "x86-interval-highwatermark.h"
#include "x86-return.h"
#include "x86-cold-path.h"
#include "x86-interval-arg.h"
#include "ui_tree.h"

unwind_interval *process_inst(xed_decoded_inst_t *xptr, interval_arg_t *iarg)
{
  xed_iclass_enum_t xiclass = xed_decoded_inst_get_iclass(xptr);
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  unwind_interval *next;
  int irdebug = 0;

  switch(xiclass) {

  case XED_ICLASS_JMP: 
  case XED_ICLASS_JMP_FAR:
    next = process_unconditional_branch(xptr, iarg->current, iarg->ins, iarg->end,
					irdebug, iarg->first, &(iarg->highwatermark),
					&(iarg->canonical_interval), iarg->bp_frames_found);

    if (hpcrun_is_cold_code(iarg->ins, xptr, iarg)) {
      TMSG(COLD_CODE,"  --cold code routine detected!");
      TMSG(COLD_CODE,"fetching interval from location %p",iarg->return_addr);
      unwind_interval *ui = (unwind_interval *) 
	csprof_addr_to_interval_locked(iarg->return_addr);
      TMSG(COLD_CODE,"got unwind interval from csprof_addr_to_interval");
      if (ENABLED(COLD_CODE)) {
        dump_ui_stderr(ui);
      }
      // Fixup current intervals w.r.t. the warm code interval
      hpcrun_cold_code_fixup(iarg->current, ui);
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
    next = process_conditional_branch(xptr, iarg->current, iarg->ins, iarg->end,
                                      iarg->first, &(iarg->highwatermark));

    break;

  case XED_ICLASS_FNSTCW:
  case XED_ICLASS_STMXCSR:
    if ((iarg->highwatermark).succ_inst_ptr == iarg->ins) { 
      //----------------------------------------------------------
      // recognize Pathscale idiom for routine prefix and ignore
      // any highwatermark setting that resulted from it.
      //
      // 20 December 2007 - John Mellor-Crummey
      //----------------------------------------------------------
      highwatermark_t empty_highwatermark = { NULL, NULL, HW_UNINITIALIZED };
      iarg->highwatermark = empty_highwatermark;
    }
    next = iarg->current;
    break;

  case XED_ICLASS_LEA: 
    next = process_lea(iarg->ins, xptr, xi, iarg->current, &(iarg->highwatermark));
    break;

  case XED_ICLASS_MOV: 
    next = process_move(iarg->ins, xptr, xi, iarg->current, &(iarg->highwatermark), &(iarg->rax_rbp_equivalent_at));
    break;

  case XED_ICLASS_ENTER:
    next = process_enter(iarg->ins, xptr, xi, iarg->current, &(iarg->highwatermark));
    break;

  case XED_ICLASS_LEAVE:
    next = process_leave(iarg->ins, xptr, xi, iarg->current, &(iarg->highwatermark));
    break;

  case XED_ICLASS_CALL_FAR:
  case XED_ICLASS_CALL_NEAR:
    next = process_call(iarg->current, &(iarg->highwatermark));
    break;

  case XED_ICLASS_RET_FAR:
  case XED_ICLASS_RET_NEAR:
    next = process_return(xptr, &(iarg->current), &(iarg->ins), iarg->end, irdebug, iarg->first,
			  &(iarg->highwatermark), &(iarg->canonical_interval), &(iarg->bp_frames_found));
    break;

  case XED_ICLASS_ADD:   
  case XED_ICLASS_SUB: 
    next = process_addsub(iarg->ins, xptr, xi, iarg->current, &(iarg->highwatermark),
			  &(iarg->canonical_interval), &(iarg->bp_frames_found));
    break;

  case XED_ICLASS_PUSH:
  case XED_ICLASS_PUSHF:  
  case XED_ICLASS_PUSHFD: 
  case XED_ICLASS_PUSHFQ: 
    next = process_push(iarg->ins, xptr, xi, iarg->current, &(iarg->highwatermark));
    break;

  case XED_ICLASS_POP:   
  case XED_ICLASS_POPF:  
  case XED_ICLASS_POPFD: 
  case XED_ICLASS_POPFQ: 
    next = process_pop(iarg->ins, xptr, xi, iarg->current, &(iarg->highwatermark));
    break;

  default:
    next = iarg->current;
    break;
  }

  return next;
}

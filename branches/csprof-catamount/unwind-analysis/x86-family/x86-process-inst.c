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


unwind_interval *process_inst(xed_decoded_inst_t *xptr, char *ins, char *end, 
			      unwind_interval *current, unwind_interval *first,
			      bool *bp_just_pushed, 
			      highwatermark_t *highwatermark,
			      unwind_interval **canonical_interval, 
			      bool *bp_frames_found)
{
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


  case XED_ICLASS_MOV: 
    next = process_move(ins, xptr, xi, current, highwatermark);
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
    next = process_return(xptr, current, ins, end, irdebug, first, 
			  highwatermark, canonical_interval, *bp_frames_found);
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


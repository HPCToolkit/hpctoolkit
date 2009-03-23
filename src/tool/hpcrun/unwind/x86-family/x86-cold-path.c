//**************************************************************************
// Build intervals for a cold path 'function'
//
// A cold path 'function' is a block of code that the hpcfnbounds detector 
// thinks is an independent function. The block of code, however, is not 
// treated as a function. Instead of being called, the code block is 
// conditionally branched to from a hot path 'parent' function. (it is not 
// 'called' very often, so we call it cold path code). Furthermore a cold 
// path 'function' does not 'return'; rather, it jumps back to the 
// instruction just after the conditional branch in the hot path
//
// These routines take care of detecting a cold path 'function', and 
// updating the intervals of the cold path code.
//
//**************************************************************************


//**************************************************************************
// system includes
//**************************************************************************

#include <stdbool.h>
#include <stdint.h>



//**************************************************************************
// local includes
//**************************************************************************

#include "fnbounds_interface.h"
#include "pmsg.h"
#include "ui_tree.h"
#include "x86-decoder.h"
#include "x86-unwind-analysis.h"
#include "x86-interval-arg.h"



//**************************************************************************
// forward declarations
//**************************************************************************

static bool confirm_cold_path_call(void *loc, interval_arg_t *iarg);



//**************************************************************************
// interface operations 
//**************************************************************************

void
hpcrun_cold_code_fixup(unwind_interval *current, unwind_interval *warm)
{
  TMSG(COLD_CODE,"  --fixing up current intervals with the warm interval");
  int ra_offset = warm->sp_ra_pos;
  int bp_offset = warm->sp_bp_pos;
  if (ra_offset == 0) {
    TMSG(COLD_CODE,"  --warm code calling routine has offset 0,"
	 " so no action taken");
    return;
  }
  TMSG(COLD_CODE,"  --updating sp_ra_pos with offset %d",ra_offset);
  for(unwind_interval *intv = current; intv; 
      intv = (unwind_interval *)intv->common.prev) {
    intv->sp_ra_pos += ra_offset;
    intv->sp_bp_pos += bp_offset;
  }
}

// The cold code detector is called when unconditional jump is encountered
bool
hpcrun_is_cold_code(void *ins, xed_decoded_inst_t *xptr, interval_arg_t *iarg)
{
  char *ins_end = ins + xed_decoded_inst_get_length(xptr);
  if (ins_end == iarg->end) {
    void *branch_target = x86_get_branch_target(ins,xptr);
    // branch target is outside bounds of current routine
    if (branch_target < iarg->beg || iarg->end <= branch_target) {
      // this is a possible cold code routine
      TMSG(COLD_CODE,"potential cold code jmp detected in routine starting @"
	   " %p (location in routine = %p)",iarg->beg,ins);
      void *beg, *end;
      if (fnbounds_enclosing_addr(branch_target, &beg, &end)){
	EMSG("Weird result! jmp @ %p branch_target %p has no function bounds",
	      ins, branch_target);
	return false;
      }
      if (branch_target == beg){
	TMSG(COLD_CODE,"  --jump is a regular tail call,"
	     " NOT a cold code return");
	return false;
      }
      // store the address of the branch, in case this turns out to be a
      // cold path routine.
      iarg->return_addr = branch_target; 

      return confirm_cold_path_call(branch_target,iarg);
    }
  }
  return false;
}



//**************************************************************************
// private operations 
//**************************************************************************

// Confirm that the previous instruction is a conditional branch to 
// the beginning of the cold call routine
static bool
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

  TMSG(COLD_CODE,"  --trying to confirm a cold code 'call' from addr %p",
       possible_call);
  if (xed_error != XED_ERROR_NONE) {
    TMSG(COLD_CODE,"  --addr %p has xed decode error when attempting confirm",
	 possible_call);
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
    TMSG(COLD_CODE,"  --conditional branch confirmed @ %p", possible_call);
    void *the_call = x86_get_branch_target(possible_call, xptr);
    TMSG(COLD_CODE,"  --comparing 'call' to %p to start of cold path %p", 
	 the_call, routine);
    return (the_call == routine);
    break;
  default:
    TMSG(COLD_CODE,"  --No conditional branch @ %p, so NOT a cold call",
	 possible_call);
    return false;
  }
  EMSG("confirm cold path call shouldn't get here!");
  return false;
}


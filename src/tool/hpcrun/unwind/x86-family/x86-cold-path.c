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
#include "ui_tree.h"
#include "x86-decoder.h"
#include "x86-unwind-analysis.h"
#include "x86-interval-arg.h"

#include <messages/messages.h>



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
hpcrun_is_cold_code(xed_decoded_inst_t *xptr, interval_arg_t *iarg)
{
  void *ins     = iarg->ins;
  char *ins_end = ins + xed_decoded_inst_get_length(xptr);
  if (ins_end == iarg->end) {
    void *branch_target = x86_get_branch_target(ins,xptr);
    // branch is indirect. this is not cold path code
    if (branch_target == NULL) return false;
    // branch target is outside bounds of current routine
    if (branch_target < iarg->beg || iarg->end <= branch_target) {
      // this is a possible cold code routine
      TMSG(COLD_CODE,"potential cold code jmp detected in routine starting @"
	   " %p (location in routine = %p)",iarg->beg,ins);
      void *beg, *end;
      if (! fnbounds_enclosing_addr(branch_target, &beg, &end, NULL)) {
	EMSG("Weird result! jmp @ %p branch_target %p has no function bounds",
	      ins, branch_target);
	return false;
      }
      if (branch_target == beg) {
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


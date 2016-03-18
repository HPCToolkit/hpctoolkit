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

#include "x86-canonical.h"
#include "x86-decoder.h"
#include "x86-interval-highwatermark.h"
#include "x86-interval-arg.h"

#include <lib/isa-lean/x86/instruction-set.h>

/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static bool plt_is_next(char *ins);


/******************************************************************************
 * interface operations
 *****************************************************************************/

unwind_interval *
process_return(xed_decoded_inst_t *xptr, bool irdebug, interval_arg_t *iarg)
{
  unwind_interval *next = iarg->current;

  if (iarg->current->ra_status == RA_SP_RELATIVE) {
    int offset = iarg->current->sp_ra_pos;
    if (offset != 0) {
      unwind_interval *u = iarg->current;
      for (;;) {
	// fix offset
	u->sp_ra_pos -= offset;
	u->sp_bp_pos -= offset;

	if (u->restored_canonical == 1) {
	  break;
	}
	u = (unwind_interval *) u->common.prev;
	if (! u) {
	  break;
	}
      }
    }
    if (iarg->current->bp_status == BP_HOSED) {
      // invariant: when we reach a return, if the BP was overwritten, it
      // should have been restored. this must be incorrect. let's reset
      // the bp status for all intervals leading up to this one since
      // the last canonical restore. 
      unwind_interval *u = iarg->current;
      for (;;) {
        if (u->bp_status != BP_HOSED) {
          break;
        }
        u->bp_status = BP_UNCHANGED;
	if (u->restored_canonical == 1) {
	  break;
	}
	u = (unwind_interval *) u->common.prev;
	if (! u) {
	  break;
	}
      }
    }
  }
  if (iarg->current->bp_status == BP_SAVED) {
     suspicious_interval(iarg->ins);
  }
  if (iarg->ins + xed_decoded_inst_get_length(xptr) < iarg->end) {
    //-------------------------------------------------------------------------
    // the return is not the last instruction in the interval; 
    // set up an interval for code after the return 
    //-------------------------------------------------------------------------
    if (plt_is_next(iarg->ins + xed_decoded_inst_get_length(xptr))) {
      //-------------------------------------------------------------------------
      // the code following the return is a program linkage table. each entry in 
      // the program linkage table should be invoked with the return address at 
      // top of the stack. this is exactly what the interval containing this
      // return instruction looks like. set the current interval as the 
      // "canonical interval" to be restored after then jump at the end of each
      // entry in the PLT. 
      //-------------------------------------------------------------------------
      iarg->canonical_interval = iarg->current;
    }
    else {
      reset_to_canonical_interval(xptr, &next, irdebug, iarg);
    }
  }
  return next;
}


/******************************************************************************
 * private operations
 *****************************************************************************/

static bool
plt_is_next(char *ins)
{
  
  // Assumes: 'ins' is pointing at the instruction from which
  // lookahead is to occur (i.e, the instruction prior to the first
  // lookahead).

  xed_state_t *xed_settings = &(x86_decoder_settings.xed_settings);

  xed_error_enum_t xed_err;
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  char *val_pushed = NULL;
  char *push_succ_addr = NULL;
  char *jmp_target = NULL;

  // skip optional padding if there appears to be any
  while ((((long) ins) & 0x11) && (*ins == 0x0)) ins++; 

  // -------------------------------------------------------
  // requirement 1: push of displacement relative to IP 
  // -------------------------------------------------------
  xed_decoded_inst_zero_set_mode(xptr, xed_settings);
  xed_err = xed_decode(xptr, (uint8_t*) ins, 15);
  if (xed_err != XED_ERROR_NONE) {
    return false;
  }

  if (iclass_eq(xptr, XED_ICLASS_PUSH)) {
    if (xed_decoded_inst_number_of_memory_operands(xptr) == 2) {
      const xed_inst_t* xi = xed_decoded_inst_inst(xptr);
      const xed_operand_t* op0 = xed_inst_operand(xi, 0);
      if ((xed_operand_name(op0) == XED_OPERAND_MEM0) && 
	    x86_isReg_IP(xed_decoded_inst_get_base_reg(xptr, 0))) {
	int64_t offset = xed_decoded_inst_get_memory_displacement(xptr, 0);
	push_succ_addr = ins + xed_decoded_inst_get_length(xptr);
	val_pushed = push_succ_addr + offset;
      }
    }
  }

  if (val_pushed == NULL) {
    // push of proper type not recognized 
    return false;
  }

  // -------------------------------------------------------
  // requirement 2: jump target affects stack
  // -------------------------------------------------------
  xed_decoded_inst_zero_set_mode(xptr, xed_settings);
  xed_err = xed_decode(xptr, (uint8_t*) push_succ_addr, 15);
  if (xed_err != XED_ERROR_NONE) {
    return false;
  }

  if (iclass_eq(xptr, XED_ICLASS_JMP) || 
      iclass_eq(xptr, XED_ICLASS_JMP_FAR)) {
    if (xed_decoded_inst_number_of_memory_operands(xptr) == 1) {

      const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
      const xed_operand_t *op0 =  xed_inst_operand(xi,0);
      if ((xed_operand_name(op0) == XED_OPERAND_MEM0) && 
	  x86_isReg_IP(xed_decoded_inst_get_base_reg(xptr, 0))) {
	long long offset = xed_decoded_inst_get_memory_displacement(xptr,0);
	jmp_target = push_succ_addr + xed_decoded_inst_get_length(xptr) + offset;
      }
    }
  }

  if (jmp_target == NULL) {
    // jump of proper type not recognized 
    return false;
  }

  //
  // FIXME: Does the 8 need to be sizeof(void*) ?????
  //
  if ((jmp_target - val_pushed) == 8){
    return true;
  }

  return false;
}


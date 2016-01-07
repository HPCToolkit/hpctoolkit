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

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "x86-interval-highwatermark.h"
#include "x86-decoder.h"
#include "x86-interval-arg.h"

#include <lib/isa-lean/x86/instruction-set.h>

//***************************************************************************

unwind_interval *
process_addsub(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg)
{
  highwatermark_t *hw_tmp = &(iarg->highwatermark);

  unwind_interval *next = iarg->current;
  const xed_operand_t* op0 = xed_inst_operand(xi,0);
  const xed_operand_t* op1 = xed_inst_operand(xi,1);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);

  if (op0_name == XED_OPERAND_REG0) {
    xed_reg_enum_t reg0 = xed_decoded_inst_get_reg(xptr, op0_name);
    if (x86_isReg_SP(reg0)) {
      //-----------------------------------------------------------------------
      // we are adjusting the stack pointer
      //-----------------------------------------------------------------------

      if (xed_operand_name(op1) == XED_OPERAND_IMM0) {
	int sign = (iclass_eq(xptr, XED_ICLASS_ADD)) ? -1 : 1;
	long immedv = sign * xed_decoded_inst_get_signed_immediate(xptr);
	ra_loc istatus = iarg->current->ra_status;
	if ((istatus == RA_STD_FRAME) && (immedv > 0) &&
	    (hw_tmp->state & HW_SP_DECREMENTED)) {
	  //-------------------------------------------------------------------
	  // if we are in a standard frame and we see a second subtract,
	  // it is time to convert interval to a BP frame to minimize
	  // the chance we get the wrong offset for the return address
	  // in a routine that manipulates SP frequently (as in
	  // leapfrog_mod_leapfrog_ in the SPEC CPU2006 benchmark
	  // 459.GemsFDTD, when compiled with PGI 7.0.3 with high levels
	  // of optimization).
	  //
	  // 9 December 2007 -- John Mellor-Crummey
	  //-------------------------------------------------------------------
	}
	next = new_ui(iarg->ins + xed_decoded_inst_get_length(xptr), 
		      istatus, iarg->current->sp_ra_pos + immedv, iarg->current->bp_ra_pos, 
		      iarg->current->bp_status, iarg->current->sp_bp_pos + immedv, 
		      iarg->current->bp_bp_pos, iarg->current);

	if (immedv > 0) {
	  if (HW_TEST_STATE(hw_tmp->state, 0, HW_SP_DECREMENTED)) {
	    //-----------------------------------------------------------------
	    // set the highwatermark and canonical interval upon seeing
	    // the FIRST subtract from SP; take no action on subsequent
	    // subtracts.
	    //
	    // test case: main in SPEC CPU2006 benchmark 470.lbm
	    // contains multiple subtracts from SP when compiled with
	    // PGI 7.0.3 with high levels of optimization. the first
	    // subtract from SP is to set up the frame; subsequent ones
	    // are to reserve space for arguments passed to functions.
	    //
	    // 9 December 2007 -- John Mellor-Crummey
	    //-----------------------------------------------------------------
	    hw_tmp->uwi = next;
	    hw_tmp->succ_inst_ptr = 
	      iarg->ins + xed_decoded_inst_get_length(xptr);
	    hw_tmp->state = 
	      HW_NEW_STATE(hw_tmp->state, HW_SP_DECREMENTED);
	    iarg->canonical_interval = next;
	  }
	}
      } else {
	if (iarg->current->ra_status != RA_BP_FRAME){
	  //-------------------------------------------------------------------
	  // no immediate in add/subtract from stack pointer; switch to
	  // BP_FRAME
	  //
	  // 9 December 2007 -- John Mellor-Crummey
	  //-------------------------------------------------------------------
	  next = new_ui(iarg->ins + xed_decoded_inst_get_length(xptr), RA_BP_FRAME, 
			iarg->current->sp_ra_pos, iarg->current->bp_ra_pos, 
			iarg->current->bp_status, iarg->current->sp_bp_pos, 
			iarg->current->bp_bp_pos, iarg->current);
	  iarg->bp_frames_found = true;
	}
      }
    }
  }
  return next;
}

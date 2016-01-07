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

#include "x86-interval-highwatermark.h"
#include "x86-decoder.h"
#include "x86-interval-arg.h"

#include <lib/isa-lean/x86/instruction-set.h>


unwind_interval *
process_move(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg)
{
  unwind_interval *next = iarg->current;
  highwatermark_t *hw_tmp = &(iarg->highwatermark);

  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  const xed_operand_t *op1 =  xed_inst_operand(xi, 1);

  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  xed_operand_enum_t   op1_name = xed_operand_name(op1);
    
  if ((op0_name == XED_OPERAND_MEM0) && (op1_name == XED_OPERAND_REG0)) { 
    //------------------------------------------------------------------------
    // storing a register to memory 
    //------------------------------------------------------------------------
    xed_reg_enum_t basereg = xed_decoded_inst_get_base_reg(xptr, 0);
    if (x86_isReg_SP(basereg)) {
      //----------------------------------------------------------------------
      // a memory move with SP as a base register
      //----------------------------------------------------------------------
      xed_reg_enum_t reg1 = xed_decoded_inst_get_reg(xptr, op1_name);
      if (x86_isReg_BP(reg1) ||  
	(x86_isReg_AX(reg1) && (iarg->rax_rbp_equivalent_at == iarg->ins))){
	//--------------------------------------------------------------------
	// register being stored is BP (or a copy in RAX)
	//--------------------------------------------------------------------
	if (iarg->current->bp_status == BP_UNCHANGED) {
	  //==================================================================
	  // instruction: save caller's BP into the stack  
	  // action:      create a new interval with 
	  //                (1) BP status reset to BP_SAVED
	  //                (2) BP position relative to the stack pointer set 
	  //                    to the offset from SP 
	  //==================================================================
	  next = new_ui(iarg->ins + xed_decoded_inst_get_length(xptr),
			iarg->current->ra_status, iarg->current->sp_ra_pos, 
			iarg->current->bp_ra_pos,
			BP_SAVED,
			xed_decoded_inst_get_memory_displacement(xptr, 0),
			iarg->current->bp_bp_pos,
			iarg->current);
	  hw_tmp->uwi = next;
	  hw_tmp->state = 
	    HW_NEW_STATE(hw_tmp->state, HW_BP_SAVED);
	}
      }
    }
  } else if ((op1_name == XED_OPERAND_MEM0) && (op0_name == XED_OPERAND_REG0)) { 
    //----------------------------------------------------------------------
    // loading a register from memory 
    //----------------------------------------------------------------------
    xed_reg_enum_t reg0 = xed_decoded_inst_get_reg(xptr, op0_name);
    if (x86_isReg_BP(reg0)) {
      //--------------------------------------------------------------------
      // register being loaded is BP
      //--------------------------------------------------------------------
      if (iarg->current->bp_status != BP_UNCHANGED) {
	int64_t offset = xed_decoded_inst_get_memory_displacement(xptr, 0);
	xed_reg_enum_t basereg = xed_decoded_inst_get_base_reg(xptr, 0); 
	if (x86_isReg_SP(basereg) && (offset == iarg->current->sp_bp_pos)) { 
	  //================================================================
	  // instruction: restore BP from its saved location in the stack  
	  // action:      create a new interval with BP status reset to 
	  //              BP_UNCHANGED
	  //================================================================
	  next = new_ui(iarg->ins + xed_decoded_inst_get_length(xptr),
			RA_SP_RELATIVE, iarg->current->sp_ra_pos, 
			iarg->current->bp_ra_pos, BP_UNCHANGED, iarg->current->sp_bp_pos, 
			iarg->current->bp_bp_pos, iarg->current);
	} else {
	  //================================================================
	  // instruction: BP is loaded from a memory address DIFFERENT from 
	  // its saved location in the stack
	  // action:      create a new interval with BP status reset to 
	  //              BP_HOSED
	  //================================================================
	  if (iarg->current->bp_status != BP_HOSED) {
	    next = new_ui(iarg->ins + xed_decoded_inst_get_length(xptr),
			  RA_SP_RELATIVE, iarg->current->sp_ra_pos, 
			  iarg->current->bp_ra_pos, BP_HOSED, iarg->current->sp_bp_pos,
			  iarg->current->bp_bp_pos, iarg->current);
	    if (HW_TEST_STATE(hw_tmp->state, HW_BP_SAVED, 
			      HW_BP_OVERWRITTEN) && 
		(hw_tmp->uwi->sp_ra_pos == next->sp_ra_pos)) {
	      hw_tmp->uwi = next;
	      hw_tmp->state = 
		HW_NEW_STATE(hw_tmp->state, HW_BP_OVERWRITTEN);
	    }
	  }
	}
      }
    } else if (x86_isReg_SP(reg0)) {
      //--------------------------------------------------------------------
      // register being loaded is SP
      //--------------------------------------------------------------------
      xed_reg_enum_t basereg = xed_decoded_inst_get_base_reg(xptr, 0); 
      if (x86_isReg_SP(basereg)) { 
	//================================================================
	// instruction: restore SP from a saved location in the stack  
	// action:      create a new interval with SP status reset to 
	//              BP_UNCHANGED
	//================================================================
	next = new_ui(iarg->ins + xed_decoded_inst_get_length(xptr),
	 	      RA_SP_RELATIVE, 0, 0,
		      iarg->current->bp_status, iarg->current->sp_bp_pos, 
		      iarg->current->bp_bp_pos, iarg->current);
      }
    }
  } else if ((op0_name == XED_OPERAND_REG0) && (op1_name == XED_OPERAND_REG1)){
    //----------------------------------------------------------------------
    // register-to-register move 
    //----------------------------------------------------------------------
    xed_reg_enum_t reg0 = xed_decoded_inst_get_reg(xptr, op0_name);
    xed_reg_enum_t reg1 = xed_decoded_inst_get_reg(xptr, op1_name);
    if (x86_isReg_BP(reg1) && x86_isReg_SP(reg0)) {
      //====================================================================
      // instruction: restore SP from BP
      // action:      begin a new SP_RELATIVE interval 
      //====================================================================
      next = new_ui(iarg->ins + xed_decoded_inst_get_length(xptr),
		    RA_SP_RELATIVE, iarg->current->bp_ra_pos, iarg->current->bp_ra_pos,
		    iarg->current->bp_status, iarg->current->bp_bp_pos, iarg->current->bp_bp_pos,
		    iarg->current);
    } else if (x86_isReg_BP(reg0) && x86_isReg_SP(reg1)) {
      //====================================================================
      // instruction: initialize BP with value of SP to set up a frame ptr
      // action:      begin a new SP_RELATIVE interval 
      //====================================================================
      next = new_ui(iarg->ins + xed_decoded_inst_get_length(xptr), 
		    RA_STD_FRAME,
		    iarg->current->sp_ra_pos, iarg->current->sp_ra_pos, BP_SAVED,
		    iarg->current->sp_bp_pos, iarg->current->sp_bp_pos, iarg->current); 
      if (HW_TEST_STATE(hw_tmp->state, HW_BP_SAVED, 
			HW_BP_OVERWRITTEN)) { 
	hw_tmp->uwi = next;
	hw_tmp->state = 
	  HW_NEW_STATE(hw_tmp->state, HW_BP_OVERWRITTEN);
      }
    } else if (x86_isReg_BP(reg1) && x86_isReg_AX(reg0)) {
      //====================================================================
      // instruction: copy BP to RAX
      //====================================================================
      iarg->rax_rbp_equivalent_at = iarg->ins + xed_decoded_inst_get_length(xptr);
    } else if (x86_isReg_BP(reg0)) {
      if (iarg->current->bp_status != BP_HOSED){
	//==================================================================
	// instruction: move some NON-special register to BP
	// state:       bp_status is NOT BP_HOSED
	// action:      begin a new RA_SP_RELATIVE,BP_HOSED interval
	//==================================================================
	next = new_ui(iarg->ins + xed_decoded_inst_get_length(xptr), 
		      RA_SP_RELATIVE,
		      iarg->current->sp_ra_pos, iarg->current->sp_ra_pos, BP_HOSED,
		      iarg->current->sp_bp_pos, iarg->current->sp_bp_pos, iarg->current);
	if (HW_TEST_STATE(hw_tmp->state, HW_BP_SAVED, 
			  HW_BP_OVERWRITTEN) && 
	    (hw_tmp->uwi->sp_ra_pos == next->sp_ra_pos)) {
	  hw_tmp->uwi = next;
	  hw_tmp->state = 
	    HW_NEW_STATE(hw_tmp->state, HW_BP_OVERWRITTEN);
	}
      }
    }
  }
  return next;
}

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

/******************************************************************************
 * include files
 *****************************************************************************/

#include "x86-interval-highwatermark.h"
#include "x86-decoder.h"
#include "x86-interval-arg.h"

#include <assert.h>

#include <lib/isa-lean/x86/instruction-set.h>

/******************************************************************************
 * interface operations 
 *****************************************************************************/

unwind_interval *
process_push(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg)
{
  unwind_interval *next;

  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  bp_loc bp_status = iarg->current->bp_status;
  int sp_bp_pos, size;

  switch(iclass(xptr)) {
  case XED_ICLASS_PUSH:   size = sizeof(void*); break;
  case XED_ICLASS_PUSHFQ: size = 8; break;
  case XED_ICLASS_PUSHFD: size = 4; break;
  case XED_ICLASS_PUSHF:  size = 2; break;
  default: assert(0);
  }

  sp_bp_pos = iarg->current->sp_bp_pos + size; 
  if (op0_name == XED_OPERAND_REG0) { 
    xed_reg_enum_t regname = xed_decoded_inst_get_reg(xptr, op0_name);
    if (x86_isReg_BP(regname) && bp_status == BP_UNCHANGED) {
      bp_status = BP_SAVED;
      sp_bp_pos = 0;
    }
  }

  next = new_ui(iarg->ins + xed_decoded_inst_get_length(xptr), iarg->current->ra_status, 
		iarg->current->sp_ra_pos + size, iarg->current->bp_ra_pos, bp_status,
		sp_bp_pos, iarg->current->bp_bp_pos, iarg->current);

  return next;
}


unwind_interval *
process_pop(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg)
{
  unwind_interval *next;

  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  bp_loc bp_status = iarg->current->bp_status;
  int size;

  switch(iclass(xptr)) {
  case XED_ICLASS_POP:   size = -((int)sizeof(void*));  break;  
  case XED_ICLASS_POPFQ: size = -8;  break;
  case XED_ICLASS_POPFD: size = -4;  break;
  case XED_ICLASS_POPF:  size = -2;  break;
  default: assert(0);
  }

  if (op0_name == XED_OPERAND_REG0) { 
    xed_reg_enum_t regname = xed_decoded_inst_get_reg(xptr, op0_name);
    if (x86_isReg_BP(regname)) bp_status = BP_UNCHANGED;
  }

  next = new_ui(iarg->ins + xed_decoded_inst_get_length(xptr), iarg->current->ra_status, 
		iarg->current->sp_ra_pos + size, iarg->current->bp_ra_pos, bp_status, 
		iarg->current->sp_bp_pos + size, iarg->current->bp_bp_pos, iarg->current);
  return next;
}

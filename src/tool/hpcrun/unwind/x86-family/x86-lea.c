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
#include "x86-lea.h"
#include "x86-interval-arg.h"

#include <lib/isa-lean/x86/instruction-set.h>

unwind_interval *
process_lea(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg)
{
  highwatermark_t *hw_tmp = &(iarg->highwatermark);
  unwind_interval *next = iarg->current;
  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);

  if ((op0_name == XED_OPERAND_REG0)) { 
    xed_reg_enum_t regname = xed_decoded_inst_get_reg(xptr, op0_name);
    if (x86_isReg_BP(regname)) {
      //=======================================================================
      // action: clobbering the base pointer; begin a new SP_RELATIVE interval 
      // note: we don't check that BP is BP_SAVED; we might have to
      //=======================================================================
      next = new_ui(iarg->ins + xed_decoded_inst_get_length(xptr),
		    RA_SP_RELATIVE, iarg->current->sp_ra_pos, iarg->current->bp_ra_pos,
		    BP_HOSED, iarg->current->sp_bp_pos, iarg->current->bp_bp_pos,
		    iarg->current);
      if (HW_TEST_STATE(hw_tmp->state, HW_BP_SAVED,
			HW_BP_OVERWRITTEN) &&
	  (hw_tmp->uwi->sp_ra_pos == next->sp_ra_pos)) {
	hw_tmp->uwi = next;
	hw_tmp->state = 
	  HW_NEW_STATE(hw_tmp->state, HW_BP_OVERWRITTEN);
      }
    }
  }
  return next;
}

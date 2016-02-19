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
#include "x86-interval-arg.h"

#include <messages/messages.h>

/******************************************************************************
 * interface operations 
 *****************************************************************************/

unwind_interval *
process_enter(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg)
{
  unsigned int i;
  unwind_interval *next;

  highwatermark_t *hw_tmp = &(iarg->highwatermark);

  long offset = 8;

  for(i = 0; i < xed_inst_noperands(xi) ; i++) {
    const xed_operand_t *op =  xed_inst_operand(xi,i);
    switch (xed_operand_name(op)) {
    case XED_OPERAND_IMM0SIGNED:
      offset += xed_decoded_inst_get_signed_immediate(xptr);
      break;
    default:
      break;
    }
  }
  TMSG(INTV,"new interval from ENTER");
  next = new_ui(iarg->ins + xed_decoded_inst_get_length(xptr),
		RA_STD_FRAME,
		iarg->current->sp_ra_pos + offset, 8, BP_SAVED,
		iarg->current->sp_bp_pos + offset - 8, 0, iarg->current);
  hw_tmp->uwi = next;
  hw_tmp->state = 
    HW_NEW_STATE(hw_tmp->state, HW_BP_SAVED | 
		 HW_SP_DECREMENTED | HW_BP_OVERWRITTEN);
  return next;
}


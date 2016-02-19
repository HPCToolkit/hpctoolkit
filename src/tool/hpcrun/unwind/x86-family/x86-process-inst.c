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

#include <stdio.h>
#include <stdbool.h>
#include "x86-addsub.h"
#include "x86-and.h"
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
    next = process_unconditional_branch(xptr, irdebug, iarg);
    if (hpcrun_is_cold_code(xptr, iarg)) {
      TMSG(COLD_CODE,"  --cold code routine detected!");
      TMSG(COLD_CODE,"fetching interval from location %p",iarg->return_addr);
      unwind_interval *ui = (unwind_interval *) 
	hpcrun_addr_to_interval_locked(iarg->return_addr);
      TMSG(COLD_CODE,"got unwind interval from hpcrun_addr_to_interval");
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
    next = process_conditional_branch(xptr, iarg);
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
    next = process_lea(xptr, xi, iarg);
    break;

  case XED_ICLASS_MOV:
    next = process_move(xptr, xi, iarg);
    break;

  case XED_ICLASS_ENTER:
    next = process_enter(xptr, xi, iarg);
    break;

  case XED_ICLASS_LEAVE:
    next = process_leave(xptr, xi, iarg);
    break;

  case XED_ICLASS_CALL_FAR:
  case XED_ICLASS_CALL_NEAR:
    next = process_call(xptr, xi, iarg);
    break;

  case XED_ICLASS_RET_FAR:
  case XED_ICLASS_RET_NEAR:
    next = process_return(xptr, irdebug, iarg);
    break;

  case XED_ICLASS_ADD:   
  case XED_ICLASS_SUB: 
    next = process_addsub(xptr, xi, iarg);
    break;

  case XED_ICLASS_PUSH:
  case XED_ICLASS_PUSHF:  
  case XED_ICLASS_PUSHFD: 
  case XED_ICLASS_PUSHFQ: 
    next = process_push(xptr, xi, iarg);
    break;

  case XED_ICLASS_POP:   
  case XED_ICLASS_POPF:  
  case XED_ICLASS_POPFD: 
  case XED_ICLASS_POPFQ: 
    next = process_pop(xptr, xi, iarg);
    break;

  case XED_ICLASS_AND:
    next = process_and(xptr, xi, iarg);
    break;

  default:
    next = iarg->current;
    break;
  }

  return next;
}

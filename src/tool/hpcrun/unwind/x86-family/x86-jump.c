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
 * includes
 *****************************************************************************/

#include <messages/messages.h>

#include "x86-build-intervals.h"
#include "x86-decoder.h"
#include "x86-process-inst.h"
#include "x86-canonical.h"
#include "x86-jump.h"
#include "x86-interval-highwatermark.h"
#include "x86-unwind-analysis.h"
#include "x86-interval-arg.h"


/******************************************************************************
 * interface operations
 *****************************************************************************/

unwind_interval *
process_unconditional_branch(xed_decoded_inst_t *xptr, bool irdebug, interval_arg_t *iarg)
{
  unwind_interval *next = iarg->current;

  if ((iarg->highwatermark).state == HW_UNINITIALIZED) {
    (iarg->highwatermark).uwi = iarg->current;
    (iarg->highwatermark).state = HW_INITIALIZED;
  }

  reset_to_canonical_interval(xptr, &next, irdebug, iarg);

  TMSG(TAIL_CALL,"checking for tail call via unconditional branch @ %p",iarg->ins);
  void *possible = x86_get_branch_target(iarg->ins, xptr);
  if (possible == NULL) {
    TMSG(TAIL_CALL,"indirect unconditional branch ==> possible tail call");
    next->has_tail_calls = true;
  }
  else if ((possible >= iarg->end) || (possible < iarg->first->common.start)) {
    TMSG(TAIL_CALL,"unconditional branch to address %p outside of current routine (%p to %p)",
         possible, iarg->first->common.start, iarg->end);
    next->has_tail_calls = true;
  }

  return next;
}


unwind_interval *
process_conditional_branch(xed_decoded_inst_t *xptr,
                           interval_arg_t *iarg)
{
  highwatermark_t *hw_tmp = &(iarg->highwatermark);
  if (hw_tmp->state == HW_UNINITIALIZED) {
    hw_tmp->uwi = iarg->current;
    hw_tmp->state = HW_INITIALIZED;
  }

  TMSG(TAIL_CALL,"checking for tail call via unconditional branch @ %p",iarg->ins);
  void *possible = x86_get_branch_target(iarg->ins, xptr);
  if (possible == NULL) {
    TMSG(TAIL_CALL,"indirect unconditional branch ==> possible tail call");
    iarg->current->has_tail_calls = true;
  }
  else if ((possible > iarg->end) || (possible < iarg->first->common.start)) {
    TMSG(TAIL_CALL,"unconditional branch to address %p outside of current routine (%p to %p)",
         possible, iarg->first->common.start, iarg->end);
    iarg->current->has_tail_calls = true;
  }

  return iarg->current;
}

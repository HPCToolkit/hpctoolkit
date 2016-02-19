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

/*
 * Troll the stack for potential valid return addresses

 * The algorithm is simple, given a stack pointer (address), look thru the
 * stack for a value that is an address in some function (and therefore is
 * a valid return address). If such a value is found, then the address (eg the
 * stack pointer) is returned.
 * 
 * Valid return address is determined by using the enclosing_function_bounds
 * routine.
 * 
 * NOTE: This is a secondary heuristic: when the normal binary analysis interval
 * builder does not yield a useful interval, the unwinder can use this stack trolling
 * method.
 */

#include <stdlib.h>
#include <inttypes.h>

#include <include/uint.h>

#include "stack_troll.h"
#include "fnbounds_interface.h"
#include "validate_return_addr.h"

#include <messages/messages.h>

static const int TROLL_LIMIT = 16;

troll_status
stack_troll(void **start_sp, uint *ra_pos, validate_addr_fn_t validate_addr, void *generic_arg)
{
  void **sp = start_sp;

  for (int i = 0; i < TROLL_LIMIT; i++) {
    switch (validate_addr(*sp, generic_arg)){
      case UNW_ADDR_CONFIRMED:
        TMSG(TROLL,"found a confirmed valid return address %p at sp = %p", \
	     *sp, sp);
        *ra_pos = (uintptr_t)sp - (uintptr_t)start_sp;
        return TROLL_VALID; // success

      case UNW_ADDR_PROBABLE_INDIRECT:
        TMSG(TROLL,"found a likely (from indirect call) valid return address %p at sp = %p", \
	     *sp, sp);
        *ra_pos = (uintptr_t)sp - (uintptr_t)start_sp;
        return TROLL_LIKELY; // success

      case UNW_ADDR_PROBABLE_TAIL:
        TMSG(TROLL,"found a likely (from tail call) valid return address %p at sp = %p", \
	     *sp, sp);
        *ra_pos = (uintptr_t)sp - (uintptr_t)start_sp;
        return TROLL_LIKELY; // success

      case UNW_ADDR_PROBABLE:
        TMSG(TROLL,"found a likely valid return address %p at sp = %p", \
	     *sp, sp);
        *ra_pos = (uintptr_t)sp - (uintptr_t)start_sp;
        return TROLL_LIKELY; // success

      case UNW_ADDR_CYCLE:
        TMSG(TROLL_CHK,"infinite loop detected with return address %p at sp = %p", \
	     *sp, sp);
        break;

      case UNW_ADDR_WRONG:
        TMSG(TROLL_CHK,"provably invalid return address %p at sp = %p", \
	     *sp, sp);
        break;

      default:
        EMSG("UNKNOWN return code from validate_addr in Trolling code %p at sp = %p",
             *sp, sp);
        break;
    }
    sp++;
  }
  
  TMSG(TROLL,"(sp=%p): failed using limit %d", start_sp, TROLL_LIMIT);
  *ra_pos = -1;
  return TROLL_INVALID; // error
}

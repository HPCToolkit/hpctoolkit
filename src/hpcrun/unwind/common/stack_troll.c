// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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

#define _GNU_SOURCE

#include <stdlib.h>
#include <inttypes.h>


#include "stack_troll.h"
#include "../../fnbounds/fnbounds_interface.h"
#include "validate_return_addr.h"

#include "../../messages/messages.h"

static const int TROLL_LIMIT = 16;

troll_status
stack_troll(void **start_sp, unsigned int *ra_pos, validate_addr_fn_t validate_addr, void *generic_arg)
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

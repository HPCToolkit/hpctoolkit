// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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

/* interface.h -- cross-platform interface for csprof */

#ifndef INTERFACE_H
#define INTERFACE_H

#include <setjmp.h>             /* jmp_buf */
#include <ucontext.h>           /* mcontext_t */

#include "state.h"
#include "backtrace.h"

typedef struct _ra_loc_s {
    enum { REGISTER, ADDRESS } type;
    union { int reg; void **address; } location;
} ra_loc_t;

struct lox {
    ra_loc_t current, stored;
};

/* retrieves the pc from a 'ucontext' */
void *context_pc(void *);

void csprof_remove_trampoline(csprof_state_t *, mcontext_t *);
void csprof_insert_trampoline(csprof_state_t *, struct lox *, mcontext_t *);

/* fill in 'lox' with information about where the return address for the
   active function in 'context' resides.  returns true if the trampoline
   should be inserted. */
int csprof_find_return_address_for_context(csprof_state_t *, struct lox *,
                                           mcontext_t *);
/* these two functions are necessary because some brilliant platforms
   (no names, please) implement mcontext_t, jmp_buf, and unwind contexts
   all as separate structures.  modifying jmp_bufs and unwind contexts to
   insert the trampoline should be substantially easier than doing things
   in the general case, so these are special functions rather than the
   general-purpose 'csprof_find_return_address_for_context' above. */
void csprof_twiddle_jmpbuf(csprof_state_t *, jmp_buf);
void csprof_twiddle_unwind_context(csprof_state_t *, void *);

/* fill in 'lox' with information about where the return address resides
   given the registers beginning at 'stackptr'.  returns true if the
   trampoline should be inserted.  to avoid unnecessary memory
   writes, this function only fills in the 'stored' member of 'lox'. */
int csprof_find_return_address_for_function(csprof_state_t *, struct lox *,
                                            void **);

/* replace the register numbered 'int' on the stack pointed to by 'stackptr'.
   we do this because the machine-independent code doesn't necessarily know
   how the saved registers are laid out from the assembly trampoline, but
   the machine-dependent code does.  returns the replaced value. */
void *csprof_replace_return_address_onstack(void **, int);

/* returns 'true' if 'context' represents an unsafe place to take samples
   and/or insert the trampoline. */
int csprof_context_is_unsafe(void *);

/* initializes 'epoch' with the list of loaded executable segments.
   'previous_epoch' points to, well, the previous epoch in case some of
   the storage there can be profitably reused */
void csprof_epoch_get_loaded_modules(csprof_epoch_t *, csprof_epoch_t *);

/* 'nlx' = 'non-local-exit'; this function handles the guts of when we
   do longjmps or similar operations (exception handling comes to mind).
   signals should be appropriately masked before entering this function.
   the structure of this function is very similar for different platforms,
   but contains some grotty bits which are hard to abstract away.

   since there are two ways in which an nlx can take place--exceptions
   and garden-variety setjmp/longjmp--we need to have two variants of
   the function for those platforms which use different structures for
   the different ways.

   the really picky would want three variants--one each for jmp_buf and
   sigjmp_buf, but most platforms implement the two structures as the
   same thing, so there's no reason to introduce unnecessary complexity. */
void csprof_nlx_to_jmp_buf(csprof_state_t *, jmp_buf);
void csprof_nlx_to_exception_context(csprof_state_t *, void *);

/* various functions inside csprof need to capture function pointers to
   internal libc functions.  the necessary function pointers are not
   consistent across platforms, however.  this function is provided to
   accomodate such wishes. */
void arch_libc_init();

#ifdef CSPROF_TRAMPOLINE_BACKEND
extern void *csprof_trampoline();

#define CSPROF_TRAMPOLINE_ADDRESS ((void *)&csprof_trampoline)
#endif

#endif

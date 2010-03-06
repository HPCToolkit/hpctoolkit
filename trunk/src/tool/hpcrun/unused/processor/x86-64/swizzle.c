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
// Copyright ((c)) 2002-2010, Rice University 
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

#include <sys/ucontext.h>

#include "interface.h"

extern void *__libc_start_main;

#ifdef CSPROF_TRAMPOLINE_BACKEND

int
csprof_find_return_address_for_context(csprof_state_t *state,
                                       struct lox *l, mcontext_t *ctx)
{
  /**** MWF: temp hack to see if works better on x86 ****/
#if 0
    /* the unwinding process has figured out the base pointer for us */
    void **rbp = (void **)state->extra_state;

    l->current.type = l->stored.type = ADDRESS;
    l->current.location.address = l->stored.location.address = rbp-1;
#else
    /* supremely easy on x86-64 */
    void **rbp = (void **)ctx->gregs[REG_RBP];

    l->current.type = l->stored.type = ADDRESS;
    l->current.location.address = l->stored.location.address = rbp+1;
#endif

    return 1;
}

int
csprof_find_return_address_for_function(csprof_state_t *state,
                                        struct lox *l, void **stackptr)
{
    void **rbp;

    l->current.type = l->stored.type = ADDRESS;

#if 1
    /* we can't rely on the stored-on-the-stack RBP to be accurate.  what
       we do know, however, is that we've already recorded the CFA for
       this frame during a previous unwind process (!).  so we poke around
       in our cached unwind stack to find what we need */
    /* FIXME: there may well be some issues with running off the stack
       here...? */
    csprof_frame_t *caller = state->bufstk;

    rbp = caller->sp - sizeof(void *);
#else
    /* the offset here is dependent upon how many registers are saved in
       tramp.s.  if you modify the prologue and epilogue of the trampoline
       there, you need to change this as well. */
    rbp = ((void **)*(stackptr + 13)) + 1;
#endif

    l->current.location.address = l->stored.location.address = rbp;

    /* we have to beware of swizzling the return from libc_start_main;
       we take the evil measure of guessing how long the procedure is
       going to be to figure out when the instruction pointer lies
       within libc_start_main.  this should be reasonably safe. */
    {
        void *libc_start_main_addr = (void **)&__libc_start_main;
        void *libc_start_main_end = VPTR_ADD(libc_start_main_addr, 240);
        void *ret = *rbp;

        return !((libc_start_main_addr <= ret) && (ret < libc_start_main_end));
    }
}

void
csprof_remove_trampoline(csprof_state_t *state, mcontext_t *context)
{
    if(csprof_swizzle_patch_is_register(state)) {
        DIE("x86-64 backend doesn't support register saves!", __FILE__, __LINE__);
    }
    else {
        void **addr = state->swizzle_patch;

	DBGMSG_PUB(1, "Removing trampoline at *(%p), restoring return address %p", addr, state->swizzle_return);
        *addr = state->swizzle_return;
    }
}

void
csprof_insert_trampoline(csprof_state_t *state, struct lox *l, mcontext_t *context)
{
    if(l->current.type == REGISTER) {
        DIE("x86-64 backend doesn't support register saves!", __FILE__, __LINE__);
    }
    else {
        void **addr = l->current.location.address;

	DBGMSG_PUB(1, "Inserting trampoline at *(%p), intercepting return to %p", addr, *addr);
	dump_backtraces(state, 0);

        state->swizzle_return = *addr;
        *addr = CSPROF_TRAMPOLINE_ADDRESS;
    }
}
#endif
